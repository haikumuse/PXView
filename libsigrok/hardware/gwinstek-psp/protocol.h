/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2023 ettom <36895504+ettom@users.noreply.github.com>
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

#ifndef LIBSIGROK_HARDWARE_GWINSTEK_PSP_PROTOCOL_H
#define LIBSIGROK_HARDWARE_GWINSTEK_PSP_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "gwinstek-psp"

#define GWINSTEK_PSP_PROCESSING_TIME_MS 50
#define GWINSTEK_PSP_STATUS_POLL_TIME_MS 245 /**< 'L' query response time. */

/*
 * PXView does not define several standard sigrok config keys that this
 * power supply driver needs. Provide them here with unique values that
 * do not collide with PXView's existing SR_CONF_* keys (which occupy the
 * 10000-10006 device-type range, the 30000-30107 config range, and the
 * 50000-50007 acquisition range). These compat values live in reserved
 * gaps. SR_CONF_ENABLED mirrors the value used by the rigol-dg compat
 * driver so both drivers agree when compiled together.
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
#ifndef SR_CONF_OVER_TEMPERATURE_PROTECTION_ACTIVE
#define SR_CONF_OVER_TEMPERATURE_PROTECTION_ACTIVE 30224
#endif

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time.
 */
#ifndef GWINSTEK_PSP_SR_SW_LIMITS_DEFINED
#define GWINSTEK_PSP_SR_SW_LIMITS_DEFINED
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
#endif /* GWINSTEK_PSP_SR_SW_LIMITS_DEFINED */

/* Information on single model */
struct gwinstek_psp_model {
	const char *vendor;    /**< Vendor name */
	const char *name;      /**< Model name */
	const double *voltage; /**< References: Min, max, step */
	const double *current; /**< References: Min, max, step */
};

struct dev_context {
	const struct gwinstek_psp_model *model; /**< Model information. */

	struct sr_sw_limits limits;
	int64_t next_req_time;
	int64_t last_status_query_time;
	GMutex rw_mutex;

	float power;            /**< Last power value [W] read from device. */
	float current;          /**< Last current value [A] read from device. */
	float current_limit;    /**< Output current set. */
	float voltage;          /**< Last voltage value [V] read from device. */
	float voltage_or_0;     /**< Same, but 0 if output is off. */
	int voltage_limit;      /**< Output voltage limit. */

	/*< Output voltage target. The device has no means to query this
	 * directly. It's equal to the voltage if the output is disabled
	 * (detectable) or the device is in CV mode (undetectable).*/
	float voltage_target;
	int64_t voltage_target_updated; /**< When device last reported a voltage target. */

	float set_voltage_target;           /**< The last set output voltage target. */
	int64_t set_voltage_target_updated; /**< When the voltage target was last set. */

	gboolean output_enabled; /**< Is the output enabled? */
	gboolean otp_active;     /**< Is the overtemperature protection active? */

	int msg_terminator_len; /** < 2 or 3, depending on the URPSP1/2 setting */
};

SR_PRIV int gwinstek_psp_send_cmd(struct sr_serial_dev_inst *serial,
    struct dev_context *devc, const char* cmd, gboolean lock);
SR_PRIV int gwinstek_psp_check_terminator(struct sr_serial_dev_inst *serial,
    struct dev_context *devc);
SR_PRIV int gwinstek_psp_get_initial_voltage_target(struct dev_context *devc);
SR_PRIV int gwinstek_psp_get_all_values(struct sr_serial_dev_inst *serial,
	struct dev_context *devc);
SR_PRIV int gwinstek_psp_receive_data(int fd, int revents,
	const struct sr_dev_inst *sdi);

#endif
