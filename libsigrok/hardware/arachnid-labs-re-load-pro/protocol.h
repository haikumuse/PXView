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

#ifndef LIBSIGROK_HARDWARE_ARACHNID_LABS_RE_LOAD_PRO_PROTOCOL_H
#define LIBSIGROK_HARDWARE_ARACHNID_LABS_RE_LOAD_PRO_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <glib.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "arachnid-labs-re-load-pro"

#define SERIALCOMM "115200/8n1"

#define RELOADPRO_BUFSIZE 100

/*
 * PXView does not define several standard sigrok config keys that this
 * electronic load driver needs. Provide them here with unique values that
 * do not collide with PXView's existing SR_CONF_* keys (which occupy the
 * 10000-10006 device-type range, the 30000-30107 config range, and the
 * 50000-50007 acquisition range). These compat values live in reserved
 * gaps. The values mirror those used by the itech-it8500, korad-kaxxxxp,
 * atten-pps3xxx, gwinstek-psp and conrad-digi-35-cpu compat drivers so
 * all power-supply/electronic-load drivers agree when compiled together.
 */
#ifndef SR_CONF_ELECTRONIC_LOAD
#define SR_CONF_ELECTRONIC_LOAD 10009
#endif
#ifndef SR_CONF_ENABLED
#define SR_CONF_ENABLED 30200
#endif
#ifndef SR_CONF_VOLTAGE
#define SR_CONF_VOLTAGE 30220
#endif
#ifndef SR_CONF_CURRENT
#define SR_CONF_CURRENT 30222
#endif
#ifndef SR_CONF_CURRENT_LIMIT
#define SR_CONF_CURRENT_LIMIT 30223
#endif
#ifndef SR_CONF_OVER_TEMPERATURE_PROTECTION_ACTIVE
#define SR_CONF_OVER_TEMPERATURE_PROTECTION_ACTIVE 30224
#endif
#ifndef SR_CONF_REGULATION
#define SR_CONF_REGULATION 30225
#endif
#ifndef SR_CONF_OVER_CURRENT_PROTECTION_ENABLED
#define SR_CONF_OVER_CURRENT_PROTECTION_ENABLED 30226
#endif
#ifndef SR_CONF_OVER_VOLTAGE_PROTECTION_ENABLED
#define SR_CONF_OVER_VOLTAGE_PROTECTION_ENABLED 30227
#endif
#ifndef SR_CONF_OVER_TEMPERATURE_PROTECTION
#define SR_CONF_OVER_TEMPERATURE_PROTECTION 30253
#endif
#ifndef SR_CONF_UNDER_VOLTAGE_CONDITION
#define SR_CONF_UNDER_VOLTAGE_CONDITION 30254
#endif
#ifndef SR_CONF_UNDER_VOLTAGE_CONDITION_ACTIVE
#define SR_CONF_UNDER_VOLTAGE_CONDITION_ACTIVE 30255
#endif
#ifndef SR_CONF_UNDER_VOLTAGE_CONDITION_THRESHOLD
#define SR_CONF_UNDER_VOLTAGE_CONDITION_THRESHOLD 30256
#endif

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time. Same pattern as colead-slm, conrad-digi-35-cpu,
 * fluke-dmm and itech-it8500 compat drivers.
 */
#ifndef ARACHNID_LABS_RE_LOAD_PRO_SR_SW_LIMITS_DEFINED
#define ARACHNID_LABS_RE_LOAD_PRO_SR_SW_LIMITS_DEFINED
struct sr_sw_limits {
	uint64_t limit_samples;
	uint64_t limit_msec;
	int64_t starttime_ms;
	uint64_t samples_read;
};

static inline void sr_sw_limits_init(struct sr_sw_limits *limits)
{
	if (!limits)
		return;
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
#endif /* ARACHNID_LABS_RE_LOAD_PRO_SR_SW_LIMITS_DEFINED */

struct dev_context {
	struct sr_sw_limits limits;

	char buf[RELOADPRO_BUFSIZE];
	int buflen;

	float current_limit;
	float voltage;
	float current;
	gboolean otp_active;
	gboolean uvc_active;
	float uvc_threshold;

	gboolean acquisition_running;
	GMutex acquisition_mutex;

	GCond current_limit_cond;
	GCond voltage_cond;
	GCond uvc_threshold_cond;
};

/*
 * Local wrapper around PXView's 3-arg serial_timeout(). Standard sigrok's
 * serial_timeout() takes (serial, bytes) and derives the baudrate from the
 * serial port's internal state. PXView requires the baudrate explicitly, so
 * parse it from the serialcomm string (format like "115200/8n1"). Falls back
 * to a conservative default when parsing fails. Implemented in protocol.c.
 * Same pattern as atten-pps3xxx and colead-slm.
 */
SR_PRIV int reloadpro_serial_timeout(struct sr_serial_dev_inst *serial,
		int bytes);

SR_PRIV int reloadpro_set_current_limit(const struct sr_dev_inst *sdi,
		float current);
SR_PRIV int reloadpro_set_on_off(const struct sr_dev_inst *sdi, gboolean on);
SR_PRIV int reloadpro_set_under_voltage_threshold(const struct sr_dev_inst *sdi,
		float uvc_threshold);
SR_PRIV int reloadpro_get_current_limit(const struct sr_dev_inst *sdi,
		float *current_limit);
SR_PRIV int reloadpro_get_under_voltage_threshold(const struct sr_dev_inst *sdi,
		float *uvc_threshold);
SR_PRIV int reloadpro_get_voltage_current(const struct sr_dev_inst *sdi,
		float *voltage, float *current);

/*
 * PXView's sr_receive_data_callback_t passes the sdi directly as the
 * third argument (const struct sr_dev_inst *sdi) instead of the
 * void *cb_data that standard sigrok uses.
 */
SR_PRIV int reloadpro_receive_data(int fd, int revents,
		const struct sr_dev_inst *sdi);

#endif
