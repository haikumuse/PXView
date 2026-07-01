/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
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
#include "protocol.h"

SR_PRIV const char *ols_channel_names[] = {
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
	"13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23",
	"24", "25", "26", "27", "28", "29", "30", "31",
};

SR_PRIV int send_shortcommand(struct sr_serial_dev_inst *serial, uint8_t command)
{
	char buf[1];
	buf[0] = command;
	if (serial_write_blocking(serial, buf, 1, serial_timeout(serial, 1)) != 1)
		return SR_ERR;
	if (serial_drain(serial) != SR_OK)
		return SR_ERR;
	return SR_OK;
}

SR_PRIV int send_longcommand(struct sr_serial_dev_inst *serial, uint8_t command, uint8_t *data)
{
	char buf[5];
	buf[0] = command;
	buf[1] = data[0];
	buf[2] = data[1];
	buf[3] = data[2];
	buf[4] = data[3];
	if (serial_write_blocking(serial, buf, 5, serial_timeout(serial, 1)) != 5)
		return SR_ERR;
	if (serial_drain(serial) != SR_OK)
		return SR_ERR;
	return SR_OK;
}

static int ols_send_longdata(struct sr_serial_dev_inst *serial, uint8_t command, uint32_t value)
{
	uint8_t data[4];
	WL32(data, value);
	return send_longcommand(serial, command, data);
}

SR_PRIV int ols_send_reset(struct sr_serial_dev_inst *serial)
{
	unsigned int i;
	for (i = 0; i < 5; i++) {
		if (send_shortcommand(serial, CMD_RESET) != SR_OK)
			return SR_ERR;
	}
	return SR_OK;
}

SR_PRIV uint32_t ols_channel_mask(const struct sr_dev_inst *sdi)
{
	uint32_t channel_mask = 0;
	for (const GSList *l = sdi->channels; l; l = l->next) {
		struct sr_channel *channel = l->data;
		if (channel->enabled)
			channel_mask |= 1 << channel->index;
	}
	return channel_mask;
}

SR_PRIV int ols_get_metadata(struct sr_dev_inst *sdi)
{
	struct sr_serial_dev_inst *serial = sdi->conn;
	struct dev_context *devc = sdi->priv;
	uint32_t tmp_int;
	uint8_t key, type;
	GString *devname, *version;
	guchar tmp_c;

	devname = g_string_new("");
	version = g_string_new("");

	key = 0xff;
	while (key) {
		if (serial_read_blocking(serial, &key, 1, 1000) != 1)
			break;
		if (key == METADATA_TOKEN_END)
			break;
		type = key >> 5;
		switch (type) {
		case 0:
			while (serial_read_blocking(serial, &tmp_c, 1, 1000) == 1 && tmp_c != '\0')
				g_string_append_c(devname, tmp_c);
			break;
		case 1:
			if (serial_read_blocking(serial, &tmp_int, 4, 1000) != 4)
				break;
			tmp_int = RB32(&tmp_int);
			switch (key) {
			case METADATA_TOKEN_NUM_PROBES_LONG:
				devc->max_channels = tmp_int;
				break;
			case METADATA_TOKEN_SAMPLE_MEMORY_BYTES:
				devc->max_samples = tmp_int;
				break;
			case METADATA_TOKEN_MAX_SAMPLE_RATE_HZ:
				devc->max_samplerate = tmp_int;
				break;
			}
			break;
		case 2:
			if (serial_read_blocking(serial, &tmp_c, 1, 1000) != 1)
				break;
			switch (key) {
			case METADATA_TOKEN_NUM_PROBES_SHORT:
				devc->max_channels = tmp_c;
				break;
			}
			break;
		}
	}

	sdi->model = g_string_free(devname, FALSE);
	sdi->version = g_string_free(version, FALSE);
	devc->unitsize = (devc->max_channels + 7) / 8;
	return SR_OK;
}

SR_PRIV int ols_set_samplerate(const struct sr_dev_inst *sdi, uint64_t samplerate)
{
	struct dev_context *devc = sdi->priv;
	if (devc->max_samplerate && samplerate > devc->max_samplerate)
		return SR_ERR_SAMPLERATE;

	if (samplerate > CLOCK_RATE) {
		devc->capture_flags |= CAPTURE_FLAG_DEMUX;
		devc->capture_flags &= ~CAPTURE_FLAG_NOISE_FILTER;
		devc->cur_samplerate_divider = (CLOCK_RATE * 2 / samplerate) - 1;
	} else {
		devc->capture_flags &= ~CAPTURE_FLAG_DEMUX;
		devc->capture_flags |= CAPTURE_FLAG_NOISE_FILTER;
		devc->cur_samplerate_divider = (CLOCK_RATE / samplerate) - 1;
	}
	devc->cur_samplerate = CLOCK_RATE / (devc->cur_samplerate_divider + 1);
	if (devc->capture_flags & CAPTURE_FLAG_DEMUX)
		devc->cur_samplerate *= 2;
	return SR_OK;
}

SR_PRIV void abort_acquisition(const struct sr_dev_inst *sdi)
{
	struct sr_serial_dev_inst *serial = sdi->conn;
	ols_send_reset(serial);
	serial_source_remove(serial);
	std_session_send_df_end(sdi, LOG_PREFIX);
}

SR_PRIV int ols_receive_data(int fd, int revents, void *cb_data)
{
	struct dev_context *devc;
	struct sr_dev_inst *sdi;
	struct sr_serial_dev_inst *serial;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_logic logic;
	uint32_t sample;
	int num_changroups, offset;
	unsigned int i;
	unsigned char byte;

	(void)fd;

	sdi = cb_data;
	serial = sdi->conn;
	devc = sdi->priv;

	if (devc->num_transfers == 0 && revents == 0)
		return TRUE;

	if (devc->num_transfers++ == 0) {
		devc->raw_sample_buf = g_try_malloc(devc->limit_samples * 4);
		if (!devc->raw_sample_buf)
			return FALSE;
		memset(devc->raw_sample_buf, 0, devc->limit_samples * 4);
	}

	num_changroups = 0;
	for (i = 0x20; i > 0x02; i >>= 1) {
		if ((devc->capture_flags & i) == 0)
			num_changroups++;
	}

	if (revents == G_IO_IN && devc->num_samples < devc->limit_samples) {
		if (serial_read_nonblocking(serial, &byte, 1) != 1)
			return FALSE;
		devc->cnt_bytes++;

		if (devc->num_samples >= devc->limit_samples)
			return TRUE;

		devc->sample[devc->num_bytes++] = byte;
		if (devc->num_bytes == num_changroups) {
			sample = devc->sample[0] | (devc->sample[1] << 8) |
				 (devc->sample[2] << 16) | (devc->sample[3] << 24);

			if (devc->capture_flags & CAPTURE_FLAG_RLE) {
				if (devc->sample[devc->num_bytes - 1] & 0x80) {
					sample &= ~(0x80 << (devc->num_bytes - 1) * 8);
					devc->rle_count = sample;
					devc->num_bytes = 0;
					return TRUE;
				}
			}
			devc->num_samples += devc->rle_count + 1;
			if (devc->num_samples > devc->limit_samples) {
				devc->rle_count -= devc->num_samples - devc->limit_samples;
				devc->num_samples = devc->limit_samples;
			}

			offset = (devc->limit_samples - devc->num_samples) * devc->unitsize;
			for (i = 0; i <= devc->rle_count; i++) {
				memcpy(devc->raw_sample_buf + offset + (i * devc->unitsize),
				       devc->sample, devc->unitsize);
			}
			memset(devc->sample, 0, 4);
			devc->num_bytes = 0;
			devc->rle_count = 0;
		}
	} else {
		packet.type = SR_DF_LOGIC;
		packet.payload = &logic;
		logic.length = devc->num_samples * devc->unitsize;
		logic.unitsize = devc->unitsize;
		logic.data = devc->raw_sample_buf;
		sr_session_send(sdi, &packet);

		g_free(devc->raw_sample_buf);
		serial_flush(serial);
		abort_acquisition(sdi);
	}
	return TRUE;
}

static int ols_set_trigger_stage(struct sr_serial_dev_inst *serial, int stage, int num_stages)
{
	uint8_t cmd, arg[4];

	cmd = CMD_SET_BASIC_TRIGGER_MASK0 + stage * 4;
	if (ols_send_longdata(serial, cmd, 0) != SR_OK)
		return SR_ERR;

	cmd = CMD_SET_BASIC_TRIGGER_VALUE0 + stage * 4;
	if (ols_send_longdata(serial, cmd, 0) != SR_OK)
		return SR_ERR;

	cmd = CMD_SET_BASIC_TRIGGER_CONFIG0 + stage * 4;
	arg[0] = arg[1] = arg[3] = 0x00;
	arg[2] = stage;
	if (stage == num_stages - 1)
		arg[3] |= TRIGGER_START;
	if (send_longcommand(serial, cmd, arg) != SR_OK)
		return SR_ERR;
	return SR_OK;
}

SR_PRIV int ols_prepare_acquisition(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc = sdi->priv;
	struct sr_serial_dev_inst *serial = sdi->conn;
	int num_changroups = 0;
	uint32_t channel_mask = ols_channel_mask(sdi);
	uint32_t readcount, delaycount;
	int i;

	for (i = 0; i < 4; i++) {
		if (channel_mask & (0xff << (i * 8)))
			num_changroups++;
	}

	uint32_t samplecount = MIN(devc->max_samples / num_changroups, devc->limit_samples);
	readcount = (samplecount + 3) / 4;

	ols_set_trigger_stage(serial, 0, 1);
	delaycount = readcount;

	if (ols_send_longdata(serial, CMD_SET_DIVIDER,
			      devc->cur_samplerate_divider & 0x00FFFFFF) != SR_OK)
		return SR_ERR;

	if (devc->max_samples > 256 * 1024) {
		if (ols_send_longdata(serial, CMD_CAPTURE_READCOUNT, readcount - 1) != SR_OK)
			return SR_ERR;
		if (ols_send_longdata(serial, CMD_CAPTURE_DELAYCOUNT, delaycount - 1) != SR_OK)
			return SR_ERR;
	} else {
		uint8_t arg[4];
		WL16(&arg[0], readcount - 1);
		WL16(&arg[2], delaycount - 1);
		if (send_longcommand(serial, CMD_CAPTURE_SIZE, arg) != SR_OK)
			return SR_ERR;
	}

	devc->capture_flags &= ~0x3c;
	devc->capture_flags |= (~(num_changroups > 0 ? 1 : 0) << 2) & 0x3c;

	if (ols_send_longdata(serial, CMD_SET_FLAGS, devc->capture_flags) != SR_OK)
		return SR_ERR;

	return SR_OK;
}