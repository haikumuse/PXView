/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014 Bert Vermeulen <bert@biot.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "hardware/compat/compat.h"
#include <errno.h>
#include <string.h>
#include "protocol.h"

/*
 * Local wrapper around PXView's 3-arg serial_timeout(). Standard sigrok's
 * serial_timeout() takes (serial, bytes) and derives the baudrate from the
 * serial port's internal state. PXView requires the baudrate explicitly, so
 * parse it from the serialcomm string (format like "9600/8n2"). Falls back
 * to a conservative default when parsing fails.
 */
SR_PRIV int atten_serial_timeout(struct sr_serial_dev_inst *serial, int bytes)
{
	uint64_t baudrate = 0;
	const char *sc;

	if (serial && serial->serialcomm && serial->serialcomm[0]) {
		sc = serial->serialcomm;
		errno = 0;
		baudrate = (uint64_t)g_ascii_strtoull(sc, NULL, 10);
		if (errno != 0 || baudrate == 0)
			baudrate = 0;
	}

	return serial_timeout(serial, baudrate, bytes);
}

static void dump_packet(const char *msg, uint8_t *packet)
{
	int i;
	char str[128];

	str[0] = 0;
	for (i = 0; i < PACKET_SIZE; i++)
		sprintf(str + strlen(str), "%.2x ", packet[i]);
	sr_dbg("%s: %s", msg, str);

}

static void handle_packet(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	struct sr_analog_encoding encoding;
	struct sr_analog_meaning meaning;
	struct sr_analog_spec spec;
	float value, data[MAX_CHANNELS];
	int offset, i;

	devc = sdi->priv;
	dump_packet("received", devc->packet);

	/* Note: digits/spec_digits will be overridden later. */
	sr_analog_init(&analog, &encoding, &meaning, &spec, 0);

	packet.type = SR_DF_ANALOG;
	packet.payload = &analog;
	analog.meaning->channels = sdi->channels;
	analog.num_samples = 1;

	analog.meaning->mq = SR_MQ_VOLTAGE;
	analog.meaning->unit = SR_UNIT_VOLT;
	analog.meaning->mqflags = SR_MQFLAG_DC;
	analog.encoding->digits = 2;
	analog.spec->spec_digits = 2;
	analog.data = data;
	for (i = 0; i < devc->model->num_channels; i++) {
		offset = 2 + i * 4;
		value = ((devc->packet[offset] << 8) + devc->packet[offset + 1]) / 100.0;
		((float *)analog.data)[i] = value;
		devc->config[i].output_voltage_last = value;
	}
	sr_session_send(sdi, &packet);

	analog.meaning->mq = SR_MQ_CURRENT;
	analog.meaning->unit = SR_UNIT_AMPERE;
	analog.meaning->mqflags = 0;
	analog.encoding->digits = 3;
	analog.spec->spec_digits = 3;
	analog.data = data;
	for (i = 0; i < devc->model->num_channels; i++) {
		offset = 4 + i * 4;
		value = ((devc->packet[offset] << 8) + devc->packet[offset + 1]) / 1000.0;
		((float *)analog.data)[i] = value;
		devc->config[i].output_current_last = value;
	}
	sr_session_send(sdi, &packet);

	for (i = 0; i < devc->model->num_channels; i++)
		devc->config[i].output_enabled = (devc->packet[15] & (1 << i)) ? TRUE : FALSE;

	devc->over_current_protection = devc->packet[18] ? TRUE : FALSE;
	if (devc->packet[19] < 3)
		devc->channel_mode = devc->packet[19];

}

SR_PRIV void send_packet(const struct sr_dev_inst *sdi, uint8_t *packet)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;

	devc = sdi->priv;
	serial = sdi->conn;
	if (serial_write_blocking(serial, packet, PACKET_SIZE, devc->delay_ms) < PACKET_SIZE)
		sr_dbg("Failed to send packet.");
	dump_packet("sent", packet);
}

SR_PRIV void send_config(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	uint8_t packet[PACKET_SIZE];
	int value, offset, i;

	devc = sdi->priv;
	memset(packet, 0, PACKET_SIZE);
	packet[0] = 0xaa;
	packet[1] = 0x20;
	packet[14] = 0x01;
	packet[16] = 0x01;
	for (i = 0; i < devc->model->num_channels; i++) {
		offset = 2 + i * 4;
		value = devc->config[i].output_voltage_max * 100;
		packet[offset] = (value >> 8) & 0xff;
		packet[offset + 1] = value & 0xff;
		value = devc->config[i].output_current_max * 1000;
		packet[offset + 2] = (value >> 8) & 0xff;
		packet[offset + 3] = value & 0xff;
		if (devc->config[i].output_enabled_set)
			packet[15] |= 1 << i;
	}
	packet[18] = devc->over_current_protection_set ? 1 : 0;
	packet[19] = devc->channel_mode_set;
	/* Checksum. */
	value = 0;
	for (i = 0; i < PACKET_SIZE - 1; i++)
		value += packet[i];
	packet[i] = value & 0xff;
	send_packet(sdi, packet);
	devc->config_dirty = FALSE;

}

/*
 * PXView's sr_receive_data_callback_t passes the sdi directly as the third
 * argument (const struct sr_dev_inst *sdi) instead of the void *cb_data
 * that standard sigrok uses. Adapt accordingly.
 */
SR_PRIV int atten_pps3xxx_receive_data(int fd, int revents,
		const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
	unsigned char c;

	(void)fd;

	if (!sdi)
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	serial = sdi->conn;
	if (revents == G_IO_IN) {
		if (serial_read_nonblocking(serial, &c, 1) < 0)
			return TRUE;
		devc->packet[devc->packet_size++] = c;
		if (devc->packet_size == PACKET_SIZE) {
			handle_packet(sdi);
			devc->packet_size = 0;
			if (devc->acquisition_running)
				send_config(sdi);
			else {
				serial_source_remove(serial);
				std_session_send_df_end(sdi, LOG_PREFIX);
			}
		}
	}

	return TRUE;
}
