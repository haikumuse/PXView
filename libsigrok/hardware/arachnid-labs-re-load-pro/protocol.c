/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2015-2016 Uwe Hermann <uwe@hermann-uwe.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "hardware/compat/compat.h"
#include <errno.h>
#include <math.h>
#include <string.h>
#include <glib.h>
#include "protocol.h"

#define READ_TIMEOUT_MS 500

/*
 * Local wrapper around PXView's 3-arg serial_timeout(). Standard sigrok's
 * serial_timeout() takes (serial, bytes) and derives the baudrate from the
 * serial port's internal state. PXView requires the baudrate explicitly, so
 * parse it from the serialcomm string (format like "115200/8n1"). Falls back
 * to a conservative default when parsing fails. Same pattern as
 * atten-pps3xxx and colead-slm.
 */
SR_PRIV int reloadpro_serial_timeout(struct sr_serial_dev_inst *serial,
		int bytes)
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

/*
 * sr_session_send_meta() and std_session_send_df_frame_end() are provided by
 * the compat layer (compat_helpers.c, declared SR_PRIV in compat_helpers.h).
 * They were previously defined as static here, which conflicted with the
 * SR_PRIV (non-static) declarations in compat_helpers.h ("static declaration
 * follows non-static declaration"). The local copies have been removed and
 * the compat-layer implementations are used directly instead. This mirrors
 * the atorch and gwinstek-gpd compat drivers.
 */
static int send_cmd(const struct sr_dev_inst *sdi, const char *cmd,
		char *replybuf, int replybufsize)
{
	char *bufptr;
	int len, ret;
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;

	devc = sdi->priv;
	serial = sdi->conn;

	/* Send the command (blocking, with timeout). */
	if ((ret = serial_write_blocking(serial, cmd,
			strlen(cmd), reloadpro_serial_timeout(serial,
			strlen(cmd)))) < (int)strlen(cmd)) {
		sr_err("Unable to send command.");
		return SR_ERR;
	}

	if (!devc->acquisition_running) {
		/* Read the reply (blocking, with timeout). */
		memset(replybuf, 0, replybufsize);
		bufptr = replybuf;
		len = replybufsize;
		ret = serial_readline(serial, &bufptr, &len, READ_TIMEOUT_MS);

		/* If we got 0 characters (possibly one \r or \n), retry once. */
		if (len == 0) {
			len = replybufsize;
			ret = serial_readline(serial, &bufptr, &len, READ_TIMEOUT_MS);
		}

		if (g_str_has_prefix((const char *)&bufptr, "err ")) {
			sr_err("Device replied with an error: '%s'.", bufptr);
			return SR_ERR;
		}
	}

	return ret;
}

SR_PRIV int reloadpro_set_current_limit(const struct sr_dev_inst *sdi,
					float current_limit)
{
	struct dev_context *devc;
	int ret, ma;
	char buf[100];
	char *cmd;

	devc = sdi->priv;

	if (current_limit < 0 || current_limit > 6) {
		sr_err("The current limit must be 0-6 A (was %f A).", current_limit);
		return SR_ERR_ARG;
	}

	/* Hardware expects current limit in mA, integer (0..6000). */
	ma = (int)round(current_limit * 1000);

	cmd = g_strdup_printf("set %d\n", ma);
	g_mutex_lock(&devc->acquisition_mutex);
	ret = send_cmd(sdi, cmd, (char *)&buf, sizeof(buf));
	g_mutex_unlock(&devc->acquisition_mutex);
	g_free(cmd);

	if (ret < 0) {
		sr_err("Error sending current limit command: %d.", ret);
		return SR_ERR;
	}
	return SR_OK;
}

SR_PRIV int reloadpro_set_on_off(const struct sr_dev_inst *sdi, gboolean on)
{
	struct dev_context *devc;
	int ret;
	char buf[100];
	const char *cmd;

	devc = sdi->priv;

	cmd = (on) ? "on\n" : "off\n";
	g_mutex_lock(&devc->acquisition_mutex);
	ret = send_cmd(sdi, cmd, (char *)&buf, sizeof(buf));
	g_mutex_unlock(&devc->acquisition_mutex);

	if (ret < 0) {
		sr_err("Error sending on/off command: %d.", ret);
		return SR_ERR;
	}
	return SR_OK;
}

SR_PRIV int reloadpro_set_under_voltage_threshold(const struct sr_dev_inst *sdi,
					float voltage)
{
	struct dev_context *devc;
	int ret, mv;
	char buf[100];
	char *cmd;

	devc = sdi->priv;

	if (voltage < 0 || voltage > 60) {
		sr_err("The under voltage threshold must be 0-60 V (was %f V).",
			voltage);
		return SR_ERR_ARG;
	}

	/* Hardware expects voltage in mV, integer (0..60000). */
	mv = (int)round(voltage * 1000);

	sr_spew("Setting under voltage threshold to %f V (%d mV).", voltage, mv);

	cmd = g_strdup_printf("uvlo %d\n", mv);
	g_mutex_lock(&devc->acquisition_mutex);
	ret = send_cmd(sdi, cmd, (char *)&buf, sizeof(buf));
	g_mutex_unlock(&devc->acquisition_mutex);
	g_free(cmd);

	if (ret < 0) {
		sr_err("Error sending under voltage threshold command: %d.", ret);
		return SR_ERR;
	}
	return SR_OK;
}

SR_PRIV int reloadpro_get_current_limit(const struct sr_dev_inst *sdi,
					float *current_limit)
{
	struct dev_context *devc;
	int ret;
	char buf[100];
	gint64 end_time;

	devc = sdi->priv;

	g_mutex_lock(&devc->acquisition_mutex);
	if ((ret = send_cmd(sdi, "set\n", (char *)&buf, sizeof(buf))) < 0) {
		sr_err("Error sending current limit query: %d.", ret);
		return SR_ERR;
	}

	if (devc->acquisition_running) {
		end_time = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
		if (!g_cond_wait_until(&devc->current_limit_cond,
				&devc->acquisition_mutex, end_time)) {
			/* Timeout has passed. */
			g_mutex_unlock(&devc->acquisition_mutex);
			return SR_ERR;
		}
	} else {
		/* Hardware sends current limit in mA, integer (0..6000). */
		devc->current_limit = g_ascii_strtod(buf + 4, NULL) / 1000;
	}
	g_mutex_unlock(&devc->acquisition_mutex);

	if (current_limit)
		*current_limit = devc->current_limit;

	return SR_OK;
}

SR_PRIV int reloadpro_get_under_voltage_threshold(const struct sr_dev_inst *sdi,
					float *uvc_threshold)
{
	struct dev_context *devc;
	int ret;
	char buf[100];
	gint64 end_time;

	devc = sdi->priv;

	g_mutex_lock(&devc->acquisition_mutex);
	if ((ret = send_cmd(sdi, "uvlo\n", (char *)&buf, sizeof(buf))) < 0) {
		sr_err("Error sending under voltage threshold query: %d.", ret);
		return SR_ERR;
	}

	if (devc->acquisition_running) {
		end_time = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
		if (!g_cond_wait_until(&devc->uvc_threshold_cond,
				&devc->acquisition_mutex, end_time)) {
			/* Timeout has passed. */
			g_mutex_unlock(&devc->acquisition_mutex);
			return SR_ERR;
		}
	} else {
		/* Hardware sends voltage in mV, integer (0..60000). */
		devc->uvc_threshold = g_ascii_strtod(buf + 5, NULL) / 1000;
	}
	g_mutex_unlock(&devc->acquisition_mutex);

	if (uvc_threshold)
		*uvc_threshold = devc->uvc_threshold;

	return SR_OK;
}

SR_PRIV int reloadpro_get_voltage_current(const struct sr_dev_inst *sdi,
		float *voltage, float *current)
{
	struct dev_context *devc;
	int ret;
	char buf[100];
	char **tokens;
	gint64 end_time;

	devc = sdi->priv;

	g_mutex_lock(&devc->acquisition_mutex);
	if ((ret = send_cmd(sdi, "read\n", (char *)&buf, sizeof(buf))) < 0) {
		sr_err("Error sending voltage/current query: %d.", ret);
		return SR_ERR;
	}

	if (devc->acquisition_running) {
		end_time = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
		if (!g_cond_wait_until(&devc->voltage_cond,
				&devc->acquisition_mutex, end_time)) {
			/* Timeout has passed. */
			g_mutex_unlock(&devc->acquisition_mutex);
			return SR_ERR;
		}
	} else {
		/* Reply: "read <current> <voltage>". */
		tokens = g_strsplit((const char *)&buf, " ", 3);
		devc->voltage = g_ascii_strtod(tokens[2], NULL) / 1000;
		devc->current = g_ascii_strtod(tokens[1], NULL) / 1000;
		g_strfreev(tokens);
	}
	g_mutex_unlock(&devc->acquisition_mutex);

	if (voltage)
		*voltage = devc->voltage;
	if (current)
		*current = devc->current;

	return SR_OK;
}

static void handle_packet(const struct sr_dev_inst *sdi)
{
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	struct dev_context *devc;
	char **tokens;
	GSList *l;

	devc = sdi->priv;

	if (g_str_has_prefix((const char *)devc->buf, "overtemp")) {
		sr_warn("Overtemperature condition!");
		devc->otp_active = TRUE;
		sr_session_send_meta(sdi, SR_CONF_OVER_TEMPERATURE_PROTECTION_ACTIVE,
			g_variant_new_boolean(TRUE));
		return;
	}

	if (g_str_has_prefix((const char *)devc->buf, "undervolt")) {
		sr_warn("Undervoltage condition!");
		devc->uvc_active = TRUE;
		sr_session_send_meta(sdi, SR_CONF_UNDER_VOLTAGE_CONDITION_ACTIVE,
			g_variant_new_boolean(TRUE));
		return;
	}

	if (g_str_has_prefix((const char *)devc->buf, "err ")) {
		sr_err("Device replied with an error: '%s'.", devc->buf);
		return;
	}

	if (g_str_has_prefix((const char *)devc->buf, "set ")) {
		tokens = g_strsplit((const char *)devc->buf, " ", 2);
		devc->current_limit = g_ascii_strtod(tokens[1], NULL) / 1000;
		g_strfreev(tokens);
		g_cond_signal(&devc->current_limit_cond);
		sr_session_send_meta(sdi, SR_CONF_CURRENT_LIMIT,
			g_variant_new_double(devc->current_limit));
		return;
	}

	if (g_str_has_prefix((const char *)devc->buf, "uvlo ")) {
		tokens = g_strsplit((const char *)devc->buf, " ", 2);
		devc->uvc_threshold = g_ascii_strtod(tokens[1], NULL) / 1000;
		g_strfreev(tokens);
		g_cond_signal(&devc->uvc_threshold_cond);
		if (devc->uvc_threshold == .0) {
			sr_session_send_meta(sdi, SR_CONF_UNDER_VOLTAGE_CONDITION,
				g_variant_new_boolean(FALSE));
		} else {
			sr_session_send_meta(sdi, SR_CONF_UNDER_VOLTAGE_CONDITION,
				g_variant_new_boolean(TRUE));
			sr_session_send_meta(sdi,
				SR_CONF_UNDER_VOLTAGE_CONDITION_THRESHOLD,
				g_variant_new_double(devc->uvc_threshold));
		}
		return;
	}

	if (!g_str_has_prefix((const char *)devc->buf, "read ")) {
		sr_dbg("Unknown packet: '%s'.", devc->buf);
		return;
	}

	tokens = g_strsplit((const char *)devc->buf, " ", 3);
	devc->voltage = g_ascii_strtod(tokens[2], NULL) / 1000;
	devc->current = g_ascii_strtod(tokens[1], NULL) / 1000;
	g_strfreev(tokens);
	g_cond_signal(&devc->voltage_cond);

	/* Begin frame. Compat layer provides std_session_send_df_frame_begin. */
	std_session_send_df_frame_begin(sdi);

	/*
	 * PXView uses the flat struct sr_datafeed_analog (analog.probes /
	 * analog.mq / analog.unit / analog.mqflags / analog.data) instead of
	 * standard sigrok's encoding/meaning/spec sub-structs. Same pattern as
	 * the atorch compat driver's feed_queue_analog_flush().
	 */
	memset(&analog, 0, sizeof(analog));
	memset(&packet, 0, sizeof(packet));
	packet.type = SR_DF_ANALOG;
	packet.status = SR_PKT_OK;
	packet.payload = &analog;
	analog.num_samples = 1;
	analog.unit_bits = 32; /* float */
	analog.unit_pitch = 0;

	/* Voltage */
	l = g_slist_copy(sdi->channels);
	l = g_slist_remove_link(l, g_slist_nth(l, 1));
	analog.probes = l;
	analog.mq = SR_MQ_VOLTAGE;
	analog.mqflags = SR_MQFLAG_DC;
	analog.unit = SR_UNIT_VOLT;
	analog.data = &devc->voltage;
	sr_session_send(sdi, &packet);
	g_slist_free(l);

	/* Current */
	l = g_slist_copy(sdi->channels);
	l = g_slist_remove_link(l, g_slist_nth(l, 0));
	analog.probes = l;
	analog.mq = SR_MQ_CURRENT;
	analog.mqflags = SR_MQFLAG_DC;
	analog.unit = SR_UNIT_AMPERE;
	analog.data = &devc->current;
	sr_session_send(sdi, &packet);
	g_slist_free(l);

	/* End frame. Compat layer provides std_session_send_df_frame_end. */
	std_session_send_df_frame_end(sdi);

	sr_sw_limits_update_samples_read(&devc->limits, 1);
}

static void handle_new_data(const struct sr_dev_inst *sdi)
{
	int len;
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
	char *buf;

	devc = sdi->priv;
	serial = sdi->conn;

	len = RELOADPRO_BUFSIZE - devc->buflen;
	buf = devc->buf;
	g_mutex_lock(&devc->acquisition_mutex);
	if (serial_readline(serial, &buf, &len, 250) != SR_OK) {
		g_mutex_unlock(&devc->acquisition_mutex);
		return;
	}

	if (len == 0) {
		g_mutex_unlock(&devc->acquisition_mutex);
		return; /* No new bytes, nothing to do. */
	}
	if (len < 0) {
		sr_err("Serial port read error: %d.", len);
		g_mutex_unlock(&devc->acquisition_mutex);
		return;
	}
	devc->buflen += len;

	handle_packet(sdi);
	g_mutex_unlock(&devc->acquisition_mutex);
	memset(devc->buf, 0, RELOADPRO_BUFSIZE);
	devc->buflen = 0;
}

/*
 * PXView's sr_receive_data_callback_t passes the sdi directly as the
 * third argument (const struct sr_dev_inst *sdi) instead of the
 * void *cb_data that standard sigrok uses.
 */
SR_PRIV int reloadpro_receive_data(int fd, int revents,
		const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	(void)fd;

	if (!sdi)
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	if (revents != G_IO_IN)
		return TRUE;

	handle_new_data(sdi);

	if (sr_sw_limits_check(&devc->limits))
		sr_dev_acquisition_stop(sdi);

	return TRUE;
}
