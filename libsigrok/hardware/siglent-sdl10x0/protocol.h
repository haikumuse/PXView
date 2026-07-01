/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2024 Timo Boettcher <timo@timoboettcher.name>
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

#ifndef LIBSIGROK_HARDWARE_SIGLENT_SDL10X0_PROTOCOL_H
#define LIBSIGROK_HARDWARE_SIGLENT_SDL10X0_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <glib.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "siglent-sdl10x0"

/*
 * PXView does not define several standard sigrok config keys that this
 * electronic load driver needs. Provide them here with unique values that
 * do not collide with PXView's existing SR_CONF_* keys (which occupy the
 * 10000-10006 device-type range, the 30000-30107 config range, and the
 * 50000-50007 acquisition range). These compat values live in reserved
 * gaps. The values mirror those used by the itech-it8500, maynuo-m97,
 * korad-kaxxxxp, atten-pps3xxx, manson-hcs-3xxx, motech-lps-30x,
 * gwinstek-psp and rdtech-dps compat drivers so all power-supply/
 * electronic-load drivers agree when compiled together.
 */
#ifndef SR_CONF_ELECTRONIC_LOAD
#define SR_CONF_ELECTRONIC_LOAD 10009
#endif
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
#ifndef SR_CONF_REGULATION
#define SR_CONF_REGULATION 30225
#endif
#ifndef SR_CONF_OVER_CURRENT_PROTECTION_ENABLED
#define SR_CONF_OVER_CURRENT_PROTECTION_ENABLED 30226
#endif
#ifndef SR_CONF_OVER_VOLTAGE_PROTECTION_ENABLED
#define SR_CONF_OVER_VOLTAGE_PROTECTION_ENABLED 30227
#endif
#ifndef SR_CONF_OVER_VOLTAGE_PROTECTION_ACTIVE
#define SR_CONF_OVER_VOLTAGE_PROTECTION_ACTIVE 30230
#endif
#ifndef SR_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD
#define SR_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD 30231
#endif
#ifndef SR_CONF_OVER_CURRENT_PROTECTION_ACTIVE
#define SR_CONF_OVER_CURRENT_PROTECTION_ACTIVE 30232
#endif
#ifndef SR_CONF_OVER_CURRENT_PROTECTION_THRESHOLD
#define SR_CONF_OVER_CURRENT_PROTECTION_THRESHOLD 30233
#endif
#ifndef SR_CONF_POWER
#define SR_CONF_POWER 30250
#endif
#ifndef SR_CONF_POWER_TARGET
#define SR_CONF_POWER_TARGET 30251
#endif
#ifndef SR_CONF_RESISTANCE_TARGET
#define SR_CONF_RESISTANCE_TARGET 30252
#endif
#ifndef SR_CONF_OVER_TEMPERATURE_PROTECTION
#define SR_CONF_OVER_TEMPERATURE_PROTECTION 30253
#endif
/*
 * Additional electronic-load specific keys used by siglent-sdl10x0 that are
 * not yet defined by the other compat drivers. Assign unique values that
 * continue the reserved compat range above without colliding.
 */
#ifndef SR_CONF_RESISTANCE
#define SR_CONF_RESISTANCE 30257
#endif
#ifndef SR_CONF_OVER_POWER_PROTECTION_ENABLED
#define SR_CONF_OVER_POWER_PROTECTION_ENABLED 30258
#endif
#ifndef SR_CONF_OVER_POWER_PROTECTION_ACTIVE
#define SR_CONF_OVER_POWER_PROTECTION_ACTIVE 30259
#endif
#ifndef SR_CONF_OVER_POWER_PROTECTION_THRESHOLD
#define SR_CONF_OVER_POWER_PROTECTION_THRESHOLD 30260
#endif

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time.
 */
#ifndef SIGLENT_SDL10X0_SR_SW_LIMITS_DEFINED
#define SIGLENT_SDL10X0_SR_SW_LIMITS_DEFINED
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
#endif /* SIGLENT_SDL10X0_SR_SW_LIMITS_DEFINED */

/*
 * Operating modes.
 */
enum siglent_sdl10x0_modes {
	CC = 0,
	CV = 1,
	CP = 2,
	CR = 3,
	LED = 4,
	SDL10x0_MODES, /* Total count, for internal use. */
};

/*
 * Possible states in an acquisition.
 */
enum acquisition_state {
	ACQ_REQUESTED_VOLTAGE,
	ACQ_REQUESTED_CURRENT,
	ACQ_REQUESTED_POWER,
	ACQ_REQUESTED_RESISTANCE,
};

struct dev_context {
	struct sr_sw_limits limits;
	enum acquisition_state acq_state;
	float voltage;
	float current;
	double maxpower;
};

SR_PRIV const char *siglent_sdl10x0_mode_to_string(enum siglent_sdl10x0_modes mode);
SR_PRIV const char *siglent_sdl10x0_mode_to_longstring(enum siglent_sdl10x0_modes mode);
SR_PRIV int siglent_sdl10x0_string_to_mode(const char *modename, enum siglent_sdl10x0_modes *mode);

SR_PRIV void siglent_sdl10x0_send_value(const struct sr_dev_inst *sdi, float value, enum sr_mq mq, enum sr_mqflag mqflags, enum sr_unit unit, int digits);

SR_PRIV int siglent_sdl10x0_receive_data(struct sr_dev_inst *sdi);

/*
 * PXView's sr_receive_data_callback_t passes the sdi directly as the third
 * argument (const struct sr_dev_inst *sdi) instead of the void *cb_data
 * that standard sigrok uses. Adapt handle_events accordingly.
 */
SR_PRIV int siglent_sdl10x0_handle_events(int fd, int revents,
		const struct sr_dev_inst *sdi);

/* std_session_send_df_frame_begin/end are provided by compat_helpers.c. */

#endif
