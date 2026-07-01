/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2014 Matthias Heidbrink <m-sigrok@heidbrink.biz>
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

#ifndef LIBSIGROK_HARDWARE_MANSON_HCS_3XXX_PROTOCOL_H
#define LIBSIGROK_HARDWARE_MANSON_HCS_3XXX_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <glib.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "manson-hcs-3xxx"

/*
 * PXView does not define several standard sigrok config keys that this
 * power supply driver needs. Provide them here with unique values that
 * do not collide with PXView's existing SR_CONF_* keys (which occupy the
 * 10000-10006 device-type range, the 30000-30107 config range, and the
 * 50000-50007 acquisition range). These compat values live in reserved
 * gaps. The values mirror those used by the korad-kaxxxxp, atten-pps3xxx
 * and gwinstek-psp compat drivers so all power-supply drivers agree when
 * compiled together.
 */
#ifndef SR_CONF_POWER_SUPPLY
#define SR_CONF_POWER_SUPPLY 10008
#endif
#ifndef SR_CONF_ENABLED
#define SR_CONF_ENABLED 30200
#endif
#ifndef SR_CONF_VOLTAGE
#define SR_CONF_VOLTAGE 30220
#endif
#ifndef SR_CONF_VOLTAGE_TARGET
#define SR_CONF_VOLTAGE_TARGET 30221
#endif
#ifndef SR_CONF_CURRENT
#define SR_CONF_CURRENT 30222
#endif
#ifndef SR_CONF_CURRENT_LIMIT
#define SR_CONF_CURRENT_LIMIT 30223
#endif

/* ALL_ZERO sentinel for the models[] array terminator. */
#ifndef ALL_ZERO
#define ALL_ZERO { 0 }
#endif

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time.
 */
#ifndef MANSON_HCS_3XXX_SR_SW_LIMITS_DEFINED
#define MANSON_HCS_3XXX_SR_SW_LIMITS_DEFINED
struct sr_sw_limits {
	uint64_t limit_samples;
	uint64_t limit_msec;
	int64_t starttime_ms;
	uint64_t samples_read;
};

static inline void sr_sw_limits_init(struct sr_sw_limits *limits)
{
	memset(limits, 0, sizeof(*limits));
}

static inline int sr_sw_limits_config_get(const struct sr_sw_limits *limits,
		uint32_t key, GVariant **data)
{
	if (!limits || !data)
		return SR_ERR_ARG;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(limits->limit_samples);
		break;
	case SR_CONF_LIMIT_MSEC:
		*data = g_variant_new_uint64(limits->limit_msec);
		break;
	default:
		return SR_ERR;
	}

	return SR_OK;
}

static inline int sr_sw_limits_config_set(struct sr_sw_limits *limits,
		uint32_t key, GVariant *data)
{
	if (!limits || !data)
		return SR_ERR_ARG;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
		limits->limit_samples = g_variant_get_uint64(data);
		break;
	case SR_CONF_LIMIT_MSEC:
		limits->limit_msec = g_variant_get_uint64(data);
		break;
	default:
		return SR_ERR;
	}

	return SR_OK;
}

static inline void sr_sw_limits_acquisition_start(struct sr_sw_limits *limits)
{
	if (!limits)
		return;
	limits->starttime_ms = g_get_real_time() / 1000;
	limits->samples_read = 0;
}

static inline void sr_sw_limits_update_samples_read(struct sr_sw_limits *limits,
		uint64_t count)
{
	if (!limits)
		return;
	limits->samples_read += count;
}

static inline gboolean sr_sw_limits_check(const struct sr_sw_limits *limits)
{
	uint64_t elapsed_ms;

	if (!limits)
		return FALSE;

	if (limits->limit_msec) {
		elapsed_ms = (uint64_t)(g_get_real_time() / 1000) -
				(uint64_t)limits->starttime_ms;
		if (elapsed_ms >= limits->limit_msec)
			return TRUE;
	}

	if (limits->limit_samples && limits->samples_read >= limits->limit_samples)
		return TRUE;

	return FALSE;
}
#endif /* MANSON_HCS_3XXX_SR_SW_LIMITS_DEFINED */

enum {
	MANSON_HCS_3100,
	MANSON_HCS_3102,
	MANSON_HCS_3104,
	MANSON_HCS_3150,
	MANSON_HCS_3200,
	MANSON_HCS_3202,
	MANSON_HCS_3204,
	MANSON_HCS_3300,
	MANSON_HCS_3302,
	MANSON_HCS_3304,
	MANSON_HCS_3400,
	MANSON_HCS_3402,
	MANSON_HCS_3404,
	MANSON_HCS_3600,
	MANSON_HCS_3602,
	MANSON_HCS_3604,
};

/** Information on a single model. */
struct hcs_model {
	int model_id;      /**< Model info */
	const char *name;  /**< Model name */
	const char *id;    /**< Model ID, like delivered by interface */
	double voltage[3]; /**< Min, max, step */
	double current[3]; /**< Min, max, step */
};

struct dev_context {
	const struct hcs_model *model; /**< Model information. */

	struct sr_sw_limits limits;
	int64_t req_sent_at;
	gboolean reply_pending;

	float current;		/**< Last current value [A] read from device. */
	float current_max;	/**< Output current set. */
	float current_max_device;/**< Device-provided maximum output current. */
	float voltage;		/**< Last voltage value [V] read from device. */
	float voltage_max;	/**< Output voltage set. */
	float voltage_max_device;/**< Device-provided maximum output voltage. */
	gboolean cc_mode;	/**< Device is in constant current mode (otherwise constant voltage). */

	gboolean output_enabled; /**< Is the output enabled? */

	char buf[50];
	int buflen;
};

/*
 * Local wrapper around PXView's 3-arg serial_timeout(). Standard sigrok's
 * serial_timeout() takes (serial, bytes) and derives the baudrate from the
 * serial port's internal state. PXView requires the baudrate explicitly, so
 * parse it from the serialcomm string (format like "9600/8n1"). Falls back
 * to a conservative default when parsing fails. Implemented in protocol.c.
 */
SR_PRIV int hcs_serial_timeout(struct sr_serial_dev_inst *serial, int bytes);

SR_PRIV int hcs_parse_volt_curr_mode(struct sr_dev_inst *sdi, char **tokens);
SR_PRIV int hcs_read_reply(struct sr_serial_dev_inst *serial, int lines, char *buf, int buflen);
SR_PRIV int hcs_send_cmd(struct sr_serial_dev_inst *serial, const char *cmd, ...);
SR_PRIV int hcs_receive_data(int fd, int revents, const struct sr_dev_inst *sdi);

#endif
