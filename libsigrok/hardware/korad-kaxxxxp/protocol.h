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

#ifndef LIBSIGROK_HARDWARE_KORAD_KAXXXXP_PROTOCOL_H
#define LIBSIGROK_HARDWARE_KORAD_KAXXXXP_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <glib.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "korad-kaxxxxp"

#define KAXXXXP_POLL_INTERVAL_MS 80

/*
 * PXView does not define several standard sigrok config keys that this
 * power supply driver needs. Provide them here with unique values that
 * do not collide with PXView's existing SR_CONF_* keys (which occupy the
 * 10000-10006 device-type range, the 30000-30107 config range, and the
 * 50000-50007 acquisition range). These compat values live in reserved
 * gaps. The values mirror those used by the gwinstek-psp and rigol-dg
 * compat drivers so all power-supply drivers agree when compiled together.
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
#ifndef SR_CONF_REGULATION
#define SR_CONF_REGULATION 30225
#endif
#ifndef SR_CONF_OVER_CURRENT_PROTECTION_ENABLED
#define SR_CONF_OVER_CURRENT_PROTECTION_ENABLED 30226
#endif
#ifndef SR_CONF_OVER_VOLTAGE_PROTECTION_ENABLED
#define SR_CONF_OVER_VOLTAGE_PROTECTION_ENABLED 30227
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
#ifndef KORAD_KAXXXXP_SR_SW_LIMITS_DEFINED
#define KORAD_KAXXXXP_SR_SW_LIMITS_DEFINED
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
#endif /* KORAD_KAXXXXP_SR_SW_LIMITS_DEFINED */

enum korad_quirks_flag {
	KORAD_QUIRK_NONE = 0,
	KORAD_QUIRK_LABPS_OVP_EN = 1UL << 0,
	KORAD_QUIRK_ID_NO_VENDOR = 1UL << 1,
	KORAD_QUIRK_ID_TRAILING = 1UL << 2,
	KORAD_QUIRK_ID_OPT_VERSION = 1UL << 3,
	KORAD_QUIRK_SLOW_PROCESSING = 1UL << 4,
	KORAD_QUIRK_ALL = (1UL << 5) - 1,
};

/* Information on single model */
struct korad_kaxxxxp_model {
	const char *vendor; /**< Vendor name */
	const char *name; /**< Model name */
	const char *id; /**< Model ID, as delivered by interface */
	int channels; /**< Number of channels */
	const double *voltage; /**< References: Min, max, step */
	const double *current; /**< References: Min, max, step */
	enum korad_quirks_flag quirks;
};

/* Reply targets */
enum {
	KAXXXXP_CURRENT,
	KAXXXXP_CURRENT_LIMIT,
	KAXXXXP_VOLTAGE,
	KAXXXXP_VOLTAGE_TARGET,
	KAXXXXP_STATUS,
	KAXXXXP_OUTPUT,
	KAXXXXP_BEEP,
	KAXXXXP_OCP,
	KAXXXXP_OVP,
	KAXXXXP_SAVE,
	KAXXXXP_RECALL,
};

struct dev_context {
	const struct korad_kaxxxxp_model *model; /**< Model information. */

	struct sr_sw_limits limits;
	int64_t next_req_time;
	GMutex rw_mutex;

	float current;          /**< Last current value [A] read from device. */
	float current_limit;    /**< Output current set. */
	float voltage;          /**< Last voltage value [V] read from device. */
	float voltage_target;   /**< Output voltage set. */
	gboolean cc_mode[2];    /**< Device is in CC mode (otherwise CV). */

	gboolean output_enabled; /**< Is the output enabled? */
	gboolean beep_enabled;   /**< Enable beeper. */
	gboolean ocp_enabled;    /**< Output current protection enabled. */
	gboolean ovp_enabled;    /**< Output voltage protection enabled. */

	gboolean cc_mode_1_changed;      /**< CC mode of channel 1 has changed. */
	gboolean cc_mode_2_changed;      /**< CC mode of channel 2 has changed. */
	gboolean output_enabled_changed; /**< Output enabled state has changed. */
	gboolean ocp_enabled_changed;    /**< OCP enabled state has changed. */
	gboolean ovp_enabled_changed;    /**< OVP enabled state has changed. */

	int acquisition_target;  /**< What reply to expect. */
	int program;             /**< Program to store or recall. */

	float set_current_limit;     /**< New output current to set. */
	float set_voltage_target;    /**< New output voltage to set. */
	gboolean set_output_enabled; /**< New output enabled to set. */
	gboolean set_beep_enabled;   /**< New enable beeper to set. */
	gboolean set_ocp_enabled;    /**< New OCP enabled to set. */
	gboolean set_ovp_enabled;    /**< New OVP enabled to set. */
};

SR_PRIV int korad_kaxxxxp_send_cmd(struct sr_serial_dev_inst *serial,
		const char *cmd);
SR_PRIV int korad_kaxxxxp_read_chars(struct sr_serial_dev_inst *serial,
		size_t count, char *buf);
SR_PRIV int korad_kaxxxxp_set_value(struct sr_serial_dev_inst *serial,
		int target, struct dev_context *devc);
SR_PRIV int korad_kaxxxxp_get_value(struct sr_serial_dev_inst *serial,
		int target, struct dev_context *devc);
SR_PRIV int korad_kaxxxxp_get_all_values(struct sr_serial_dev_inst *serial,
		struct dev_context *devc);
SR_PRIV int korad_kaxxxxp_receive_data(int fd, int revents,
		const struct sr_dev_inst *sdi);

#endif
