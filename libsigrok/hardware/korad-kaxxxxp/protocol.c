/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2015 Hannu Vuolasaho <vuokkosetae@gmail.com>
 * Copyright (C) 2018-2019 Frank Stettner <frank-stettner@gmx.net>
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
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include "protocol.h"

#define DEVICE_PROCESSING_TIME_MS 80
#define EXTRA_PROCESSING_TIME_MS  450

/*
 * Local replacement for standard sigrok's sr_atof_ascii(), which PXView's
 * libsigrok does not provide. Parses an ASCII numeric string in a
 * locale-independent way and stores the result in a float.
 */
static int korad_sr_atof_ascii(const char *str, float *ret)
{
	char *e;
	double tmp;

	errno = 0;
	tmp = g_ascii_strtod(str, &e);
	if (e == str || errno != 0)
		return SR_ERR;
	*ret = (float)tmp;
	return SR_OK;
}

/*
 * Local wrapper around PXView's 3-arg serial_timeout(). Standard sigrok's
 * serial_timeout() takes (serial, bytes) and derives the baudrate from the
 * serial port's internal state. PXView requires the baudrate explicitly, so
 * parse it from the serialcomm string (format like "9600/8n1"). Falls back
 * to a conservative default when parsing fails.
 */
static int korad_serial_timeout(struct sr_serial_dev_inst *serial, int bytes)
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

SR_PRIV int korad_kaxxxxp_send_cmd(struct sr_serial_dev_inst *serial,
	const char *cmd)
{
	int ret;

	sr_dbg("Sending '%s'.", cmd);
	if ((ret = serial_write_blocking(serial, cmd, strlen(cmd), 0)) < 0) {
		sr_err("Error sending command: %d.", ret);
		return ret;
	}

	return ret;
}

/**
 * Read a variable length non-terminated string (caller specified maximum size).
 *
 * @param[in] serial The serial port to read from.
 * @param[in] count The maximum amount of data to read.
 * @param[out] buf The buffer to read data into. Must be larger than @a count.
 *
 * @return The amount of received data, or negative in case of error.
 *     See @ref SR_ERR and other error codes.
 *
 * @internal
 *
 * The protocol has no concept of request/response termination. The only
 * terminating conditions are either the caller's expected maxmimum byte
 * count, or a period of time without receive data. It's essential to
 * accept a longer initial period of time before the first receive data
 * is seen. The supported devices can be very slow to respond.
 *
 * The protocol is text based. That's why the 'count' parameter specifies
 * the expected number of text characters, and does not include the NUL
 * termination which is not part of the wire protocol but gets added by
 * the receive routine. The caller provided buffer is expected to have
 * enough space for the text data and the NUL termination.
 *
 * Implementation detail: It's assumed that once receive data was seen,
 * remaining response data will follow at wire speed. No further delays
 * are expected beyond bitrate expectations. All normal commands in the
 * acquisition phase are of fixed length which is known to the caller.
 * Identification during device scan needs to deal with variable length
 * data. Quick termination after reception is important there, as is the
 * larger initial timeout period before receive data is seen.
 */
SR_PRIV int korad_kaxxxxp_read_chars(struct sr_serial_dev_inst *serial,
	size_t count, char *buf)
{
	int timeout_first, timeout_later, timeout;
	size_t retries_first, retries_later, retries;
	size_t received;
	int ret;

	/* Clear the buffer early, to simplify the receive code path. */
	memset(buf, 0, count + 1);

	/*
	 * This calculation is aiming for backwards compatibility with
	 * an earlier implementation. An initial timeout is used which
	 * depends on the expected response byte count, and a maximum
	 * iteration count is used for read attempts.
	 *
	 * After initial receive data was seen, a shorter timeout is
	 * used which corresponds to a few bytes at wire speed. Idle
	 * periods without receive data longer than this threshold are
	 * taken as the end of the response.
	 */
	timeout_first = korad_serial_timeout(serial, (int)count);
	retries_first = 100;
	timeout_later = korad_serial_timeout(serial, 3);
	retries_later = 1;

	sr_spew("want %zu bytes, timeout/retry: init %d/%zu, later %d/%zu.",
		count, timeout_first, retries_first,
		timeout_later, retries_later);

	/*
	 * Run a sequence of read attempts. Try with the larger timeout
	 * and a high retry count until the first receive data became
	 * available. Then continue with a short timeout and small retry
	 * count.
	 */
	received = 0;
	timeout = timeout_first;
	retries = retries_first;
	while (received < count && retries--) {
		ret = serial_read_blocking(serial,
			&buf[received], count - received, timeout);
		if (ret < 0) {
			sr_err("Error %d reading %zu bytes from device.",
			       ret, count);
			return ret;
		}
		if (ret == 0 && !received)
			continue;
		if (ret == 0 && received) {
			sr_spew("receive timed out, want %zu, received %zu.",
				count, received);
			break;
		}
		received += ret;
		timeout = timeout_later;
		retries = retries_later;
	}
	/* TODO Escape non-printables? Seen those with status queries. */
	sr_dbg("got %zu bytes, received: '%s'.", received, buf);

	return (int)received;
}

static void give_device_time_to_process(struct dev_context *devc)
{
	int64_t sleeping_time;

	if (!devc->next_req_time)
		return;

	sleeping_time = devc->next_req_time - g_get_monotonic_time();
	if (sleeping_time > 0) {
		g_usleep(sleeping_time);
		sr_spew("Sleeping for processing %" PRIi64 " usec", sleeping_time);
	}
}

static int64_t next_req_time(struct dev_context *devc,
	gboolean is_set, int target)
{
	gboolean is_slow_device, is_long_command;
	int64_t processing_time_us;

	is_slow_device = devc->model->quirks & KORAD_QUIRK_SLOW_PROCESSING;
	is_long_command = is_set;
	is_long_command |= target == KAXXXXP_STATUS;

	processing_time_us = DEVICE_PROCESSING_TIME_MS;
	if (is_slow_device && is_long_command)
		processing_time_us += EXTRA_PROCESSING_TIME_MS;
	processing_time_us *= 1000;

	return g_get_monotonic_time() + processing_time_us;
}

SR_PRIV int korad_kaxxxxp_set_value(struct sr_serial_dev_inst *serial,
	int target, struct dev_context *devc)
{
	char msg[20];
	int ret;

	g_mutex_lock(&devc->rw_mutex);
	give_device_time_to_process(devc);

	msg[0] = '\0';
	ret = SR_OK;
	switch (target) {
	case KAXXXXP_CURRENT:
	case KAXXXXP_VOLTAGE:
	case KAXXXXP_STATUS:
		sr_err("Can't set measured value %d.", target);
		ret = SR_ERR;
		break;
	case KAXXXXP_CURRENT_LIMIT:
		snprintf(msg, sizeof(msg),
			"ISET1:%05.3f", devc->set_current_limit);
		break;
	case KAXXXXP_VOLTAGE_TARGET:
		snprintf(msg, sizeof(msg),
			"VSET1:%05.2f", devc->set_voltage_target);
		break;
	case KAXXXXP_OUTPUT:
		snprintf(msg, sizeof(msg),
			"OUT%1d", (devc->set_output_enabled) ? 1 : 0);
		/* Set value back to recognize changes */
		devc->output_enabled = devc->set_output_enabled;
		break;
	case KAXXXXP_BEEP:
		snprintf(msg, sizeof(msg),
			"BEEP%1d", (devc->set_beep_enabled) ? 1 : 0);
		break;
	case KAXXXXP_OCP:
		snprintf(msg, sizeof(msg),
			"OCP%1d", (devc->set_ocp_enabled) ? 1 : 0);
		/* Set value back to recognize changes */
		devc->ocp_enabled = devc->set_ocp_enabled;
		break;
	case KAXXXXP_OVP:
		snprintf(msg, sizeof(msg),
			"OVP%1d", (devc->set_ovp_enabled) ? 1 : 0);
		/* Set value back to recognize changes */
		devc->ovp_enabled = devc->set_ovp_enabled;
		break;
	case KAXXXXP_SAVE:
		if (devc->program < 1 || devc->program > 5) {
			sr_err("Program %d is not in the supported 1-5 range.",
			       devc->program);
			ret = SR_ERR;
			break;
		}
		snprintf(msg, sizeof(msg),
			"SAV%1d", devc->program);
		break;
	case KAXXXXP_RECALL:
		if (devc->program < 1 || devc->program > 5) {
			sr_err("Program %d is not in the supported 1-5 range.",
			       devc->program);
			ret = SR_ERR;
			break;
		}
		snprintf(msg, sizeof(msg),
			"RCL%1d", devc->program);
		break;
	default:
		sr_err("Don't know how to set target %d.", target);
		ret = SR_ERR;
		break;
	}

	if (ret == SR_OK && msg[0]) {
		ret = korad_kaxxxxp_send_cmd(serial, msg);
		devc->next_req_time = next_req_time(devc, TRUE, target);
	}

	g_mutex_unlock(&devc->rw_mutex);

	return ret;
}

SR_PRIV int korad_kaxxxxp_get_value(struct sr_serial_dev_inst *serial,
	int target, struct dev_context *devc)
{
	int ret, count;
	char reply[6];
	float *value;
	char status_byte;
	gboolean needs_ovp_quirk;
	gboolean prev_status;

	g_mutex_lock(&devc->rw_mutex);
	give_device_time_to_process(devc);

	value = NULL;
	count = 5;

	switch (target) {
	case KAXXXXP_CURRENT:
		/* Read current from device. */
		ret = korad_kaxxxxp_send_cmd(serial, "IOUT1?");
		value = &(devc->current);
		break;
	case KAXXXXP_CURRENT_LIMIT:
		/* Read set current from device. */
		ret = korad_kaxxxxp_send_cmd(serial, "ISET1?");
		value = &(devc->current_limit);
		break;
	case KAXXXXP_VOLTAGE:
		/* Read voltage from device. */
		ret = korad_kaxxxxp_send_cmd(serial, "VOUT1?");
		value = &(devc->voltage);
		break;
	case KAXXXXP_VOLTAGE_TARGET:
		/* Read set voltage from device. */
		ret = korad_kaxxxxp_send_cmd(serial, "VSET1?");
		value = &(devc->voltage_target);
		break;
	case KAXXXXP_STATUS:
	case KAXXXXP_OUTPUT:
	case KAXXXXP_OCP:
	case KAXXXXP_OVP:
		/* Read status from device. */
		ret = korad_kaxxxxp_send_cmd(serial, "STATUS?");
		count = 1;
		break;
	default:
		sr_err("Don't know how to query %d.", target);
		ret = SR_ERR;
	}
	if (ret < 0) {
		g_mutex_unlock(&devc->rw_mutex);
		return ret;
	}

	devc->next_req_time = next_req_time(devc, FALSE, target);

	if ((ret = korad_kaxxxxp_read_chars(serial, count, reply)) < 0) {
		g_mutex_unlock(&devc->rw_mutex);
		return ret;
	}

	if (value) {
		korad_sr_atof_ascii((const char *)&reply, value);
		sr_dbg("value: %f", *value);
	} else {
		/* We have status reply. */
		status_byte = reply[0];

		/* Constant current channel one. */
		prev_status = devc->cc_mode[0];
		devc->cc_mode[0] = !(status_byte & (1 << 0));
		devc->cc_mode_1_changed = devc->cc_mode[0] != prev_status;
		/* Constant current channel two. */
		prev_status = devc->cc_mode[1];
		devc->cc_mode[1] = !(status_byte & (1 << 1));
		devc->cc_mode_2_changed = devc->cc_mode[1] != prev_status;

		/*
		 * Tracking:
		 * status_byte & ((1 << 2) | (1 << 3))
		 * 00 independent 01 series 11 parallel
		 */
		devc->beep_enabled = status_byte & (1 << 4);

		/* OCP enabled. */
		prev_status = devc->ocp_enabled;
		devc->ocp_enabled = status_byte & (1 << 5);
		devc->ocp_enabled_changed = devc->ocp_enabled != prev_status;

		/* Output status. */
		prev_status = devc->output_enabled;
		devc->output_enabled = status_byte & (1 << 6);
		devc->output_enabled_changed = devc->output_enabled != prev_status;

		/* OVP enabled, special handling for Velleman LABPS3005 quirk. */
		needs_ovp_quirk = devc->model->quirks & KORAD_QUIRK_LABPS_OVP_EN;
		if (!needs_ovp_quirk || devc->output_enabled) {
			prev_status = devc->ovp_enabled;
			devc->ovp_enabled = status_byte & (1 << 7);
			devc->ovp_enabled_changed = devc->ovp_enabled != prev_status;
		}

		sr_dbg("Status: 0x%02x", status_byte);
		sr_spew("Status: CH1: constant %s CH2: constant %s. "
			"Tracking would be %s and %s. Output is %s. "
			"OCP is %s, OVP is %s. Device is %s.",
			(status_byte & (1 << 0)) ? "voltage" : "current",
			(status_byte & (1 << 1)) ? "voltage" : "current",
			(status_byte & (1 << 2)) ? "parallel" : "series",
			(status_byte & (1 << 3)) ? "tracking" : "independent",
			(status_byte & (1 << 6)) ? "enabled" : "disabled",
			(status_byte & (1 << 5)) ? "enabled" : "disabled",
			(status_byte & (1 << 7)) ? "enabled" : "disabled",
			(status_byte & (1 << 4)) ? "beeping" : "silent");
	}

	/* Read the sixth byte from ISET? BUG workaround. */
	if (target == KAXXXXP_CURRENT_LIMIT)
		serial_read_blocking(serial, &status_byte, 1, 10);

	g_mutex_unlock(&devc->rw_mutex);

	return ret;
}

SR_PRIV int korad_kaxxxxp_get_all_values(struct sr_serial_dev_inst *serial,
	struct dev_context *devc)
{
	int ret, target;

	for (target = KAXXXXP_CURRENT;
			target <= KAXXXXP_STATUS; target++) {
		if ((ret = korad_kaxxxxp_get_value(serial, target, devc)) < 0)
			return ret;
	}

	return ret;
}

static void next_measurement(struct dev_context *devc)
{
	switch (devc->acquisition_target) {
	case KAXXXXP_CURRENT:
		devc->acquisition_target = KAXXXXP_VOLTAGE;
		break;
	case KAXXXXP_VOLTAGE:
		devc->acquisition_target = KAXXXXP_STATUS;
		break;
	case KAXXXXP_STATUS:
		devc->acquisition_target = KAXXXXP_CURRENT;
		break;
	default:
		devc->acquisition_target = KAXXXXP_CURRENT;
		sr_err("Invalid target for next acquisition.");
	}
}

SR_PRIV int korad_kaxxxxp_receive_data(int fd, int revents,
	const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	GSList *l;

	(void)fd;
	(void)revents;

	if (!sdi)
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	serial = sdi->conn;

	/* Get the value. */
	korad_kaxxxxp_get_value(serial, devc->acquisition_target, devc);

	memset(&analog, 0, sizeof(analog));

	/* Send the value forward. */
	packet.type = SR_DF_ANALOG;
	packet.payload = &analog;
	analog.num_samples = 1;
	l = g_slist_copy(sdi->channels);
	if (devc->acquisition_target == KAXXXXP_CURRENT) {
		l = g_slist_remove_link(l, g_slist_nth(l, 0));
		analog.probes = l;
		analog.mq = SR_MQ_CURRENT;
		analog.unit = SR_UNIT_AMPERE;
		analog.mqflags = SR_MQFLAG_DC;
		analog.digits = 3;
		analog.spec_digits = 3;
		analog.data = &devc->current;
		sr_session_send(sdi, &packet);
		g_slist_free(l);
	} else if (devc->acquisition_target == KAXXXXP_VOLTAGE) {
		l = g_slist_remove_link(l, g_slist_nth(l, 1));
		analog.probes = l;
		analog.mq = SR_MQ_VOLTAGE;
		analog.unit = SR_UNIT_VOLT;
		analog.mqflags = SR_MQFLAG_DC;
		analog.digits = 2;
		analog.spec_digits = 2;
		analog.data = &devc->voltage;
		sr_session_send(sdi, &packet);
		g_slist_free(l);
		sr_sw_limits_update_samples_read(&devc->limits, 1);
	} else if (devc->acquisition_target == KAXXXXP_STATUS) {
		if (devc->cc_mode_1_changed) {
			sr_session_send_meta(sdi, SR_CONF_REGULATION,
				g_variant_new_string((devc->cc_mode[0]) ? "CC" : "CV"));
			devc->cc_mode_1_changed = FALSE;
		}
		if (devc->cc_mode_2_changed) {
			sr_session_send_meta(sdi, SR_CONF_REGULATION,
				g_variant_new_string((devc->cc_mode[1]) ? "CC" : "CV"));
			devc->cc_mode_2_changed = FALSE;
		}
		if (devc->output_enabled_changed) {
			sr_session_send_meta(sdi, SR_CONF_ENABLED,
				g_variant_new_boolean(devc->output_enabled));
			devc->output_enabled_changed = FALSE;
		}
		if (devc->ocp_enabled_changed) {
			sr_session_send_meta(sdi, SR_CONF_OVER_CURRENT_PROTECTION_ENABLED,
				g_variant_new_boolean(devc->ocp_enabled));
			devc->ocp_enabled_changed = FALSE;
		}
		if (devc->ovp_enabled_changed) {
			sr_session_send_meta(sdi, SR_CONF_OVER_VOLTAGE_PROTECTION_ENABLED,
				g_variant_new_boolean(devc->ovp_enabled));
			devc->ovp_enabled_changed = FALSE;
		}
	}
	next_measurement(devc);

	if (sr_sw_limits_check(&devc->limits))
		sr_dev_acquisition_stop(sdi);

	return TRUE;
}
