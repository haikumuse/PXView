/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2018 Bastian Schmitz <bastian.schmitz@udo.edu>
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
#include <string.h>
#include "protocol.h"

SR_PRIV int gpd_send_cmd(struct sr_serial_dev_inst *serial, const char *cmd, ...)
{
	int ret;
	char cmdbuf[50];
	char *cmd_esc;
	va_list args;

	va_start(args, cmd);
	vsnprintf(cmdbuf, sizeof(cmdbuf), cmd, args);
	va_end(args);

	cmd_esc = g_strescape(cmdbuf, NULL);
	sr_dbg("Sending '%s'.", cmd_esc);
	g_free(cmd_esc);

	ret = serial_write_blocking(serial, cmdbuf, strlen(cmdbuf),
				    serial_timeout(serial, strlen(cmdbuf)));
	if (ret < 0) {
		sr_err("Error sending command: %d.", ret);
		return ret;
	}

	return ret;
}

SR_PRIV int gpd_receive_reply(struct sr_serial_dev_inst *serial, char *buf,
				int buflen)
{
	int l_recv = 0, bufpos = 0, retc, l_startpos = 0, lines = 1;
	gint64 start, remaining;
	const int timeout_ms = 250;

	if (!serial || !buf || (buflen <= 0))
		return SR_ERR_ARG;

	start = g_get_monotonic_time();
	remaining = timeout_ms;

	while ((l_recv < lines) && (bufpos < (buflen + 1))) {
		retc = serial_read_blocking(serial, &buf[bufpos], 1, remaining);
		if (retc != 1)
			return SR_ERR;

		if (bufpos == 0 && buf[bufpos] == '\r')
			continue;
		if (bufpos == 0 && buf[bufpos] == '\n')
			continue;

		if (buf[bufpos] == '\n' || buf[bufpos] == '\r') {
			buf[bufpos] = '\0';
			sr_dbg("Received line '%s'.", &buf[l_startpos]);
			buf[bufpos] = '\n';
			l_startpos = bufpos + 1;
			l_recv++;
		}
		bufpos++;

		/* Reduce timeout by time elapsed. */
		remaining = timeout_ms - ((g_get_monotonic_time() - start) / 1000);
		if (remaining <= 0)
			return SR_ERR; /* Timeout. */
	}

	buf[bufpos] = '\0';

	if (l_recv == lines)
		return SR_OK;
	else
		return SR_ERR;
}

/*
 * PXView note: the signature changed from (int, int, void *cb_data) to
 * (int, int, const struct sr_dev_inst *sdi) to match
 * sr_receive_data_callback_t (libsigrok-internal.h). The body drops the
 * former cb_data dereference and uses sdi directly.
 *
 * PXView's sr_datafeed_analog is the old flat layout (probes, mq, unit,
 * mqflags, data, unit_bits, ...). There is no sr_analog_init() helper and
 * no encoding/meaning/spec sub-structs, so the fields are initialized
 * directly (matching the hp-3457a / rdtech-um / fluke-dmm compat driver
 * pattern). The probes GSList is built fresh per packet and freed after
 * sending, mirroring the meaning.channels handling in the original.
 */
SR_PRIV int gpd_receive_data(int fd, int revents, const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	struct sr_channel *ch;
	unsigned int i;
	char reply[50];
	char *reply_esc;

	(void)fd;

	if (!sdi)
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	serial = sdi->conn;

	if (revents == G_IO_IN) {
		if (!devc->reply_pending) {
			sr_err("No reply pending.");
			gpd_receive_reply(serial, reply, sizeof(reply));
			reply_esc = g_strescape(reply, NULL);
			sr_err("Unexpected data '%s'.", reply_esc);
			g_free(reply_esc);
		} else {
			for (i = 0; i < devc->model->num_channels; i++) {
				packet.type = SR_DF_ANALOG;
				packet.payload = &analog;

				reply[0] = '\0';
				gpd_receive_reply(serial, reply, sizeof(reply));
				if (sscanf(reply, "%f", &devc->config[i].output_current_last) != 1) {
					sr_err("Invalid reply to IOUT1?: '%s'.",
						reply);
					return TRUE;
				}

				/* Send the current value forward. */
				memset(&analog, 0, sizeof(analog));
				analog.num_samples = 1;
				ch = g_slist_nth_data(sdi->channels, i);
				analog.probes = g_slist_append(NULL, ch);
				analog.mq = SR_MQ_CURRENT;
				analog.unit = SR_UNIT_AMPERE;
				analog.mqflags = 0;
				analog.data = &devc->config[i].output_current_last;
				analog.unit_bits = 32; /* sizeof(float) */
				analog.unit_pitch = 0;
				sr_session_send(sdi, &packet);
				g_slist_free(analog.probes);

				reply[0] = '\0';
				gpd_receive_reply(serial, reply, sizeof(reply));
				if (sscanf(reply, "%f", &devc->config[i].output_voltage_last) != 1) {
					sr_err("Invalid reply to VOUT1?: '%s'.",
						reply);
					return TRUE;
				}

				/* Send the voltage value forward. */
				memset(&analog, 0, sizeof(analog));
				analog.num_samples = 1;
				ch = g_slist_nth_data(sdi->channels, i);
				analog.probes = g_slist_append(NULL, ch);
				analog.mq = SR_MQ_VOLTAGE;
				analog.unit = SR_UNIT_VOLT;
				analog.mqflags = SR_MQFLAG_DC;
				analog.data = &devc->config[i].output_voltage_last;
				analog.unit_bits = 32; /* sizeof(float) */
				analog.unit_pitch = 0;
				sr_session_send(sdi, &packet);
				g_slist_free(analog.probes);
			}

			devc->reply_pending = FALSE;
			sr_sw_limits_update_samples_read(&devc->limits, 1);
		}
	} else {
		if (!devc->reply_pending) {
			for (i = 0; i < devc->model->num_channels; i++)
				gpd_send_cmd(serial, "IOUT%d?\nVOUT%d?\n",
					i + 1, i + 1);
			devc->req_sent_at = g_get_monotonic_time();
			devc->reply_pending = TRUE;
		}
	}

	if (sr_sw_limits_check(&devc->limits)) {
		sr_dev_acquisition_stop(sdi);
		return TRUE;
	}

	return TRUE;
}
