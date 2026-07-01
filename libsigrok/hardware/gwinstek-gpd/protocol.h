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

#ifndef LIBSIGROK_HARDWARE_GWINSTEK_GPD_PROTOCOL_H
#define LIBSIGROK_HARDWARE_GWINSTEK_GPD_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "gwinstek-gpd"

/*
 * PXView's libsigrok.h does not define several standard sigrok config keys
 * that this power-supply driver needs. Provide them here with unique values
 * in the reserved compat range, guarded so they do not clash if PXView later
 * adds them upstream. Values match the convention used by other compat
 * power-supply drivers (atten-pps3xxx, gwinstek-psp, motech-lps-30x, ...).
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
#ifndef SR_CONF_CHANNEL_CONFIG
#define SR_CONF_CHANNEL_CONFIG 30228
#endif

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time. The guard macro prevents double-definition if
 * another header in the same translation unit already pulled in a copy.
 */
#ifndef SR_SW_LIMITS_H
#define SR_SW_LIMITS_H

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

#endif /* SR_SW_LIMITS_H */

enum {
	GPD_2303S,
	GPD_3303S,
};

/* Maximum number of output channels handled by this driver. */
#define MAX_CHANNELS 2

#define CHANMODE_INDEPENDENT (1 << 0)
#define CHANMODE_SERIES      (1 << 1)
#define CHANMODE_PARALLEL    (1 << 2)

struct channel_spec {
	/* Min, max, step. */
	gdouble voltage[3];
	gdouble current[3];
};

struct gpd_model {
	int modelid;
	const char *name;
	int channel_modes;
	unsigned int num_channels;
	struct channel_spec channels[MAX_CHANNELS];
};

struct per_channel_config {
	/* Received from device. */
	float output_voltage_last;
	float output_current_last;
	/* Set by frontend. */
	float output_voltage_max;
	float output_current_max;
};

struct dev_context {
	/* Received from device. */
	gboolean output_enabled;
	int64_t req_sent_at;
	gboolean reply_pending;

	struct sr_sw_limits limits;
	int channel_mode;
	struct per_channel_config *config;
	const struct gpd_model *model;
};

SR_PRIV int gpd_send_cmd(struct sr_serial_dev_inst *serial, const char *cmd, ...);
SR_PRIV int gpd_receive_reply(struct sr_serial_dev_inst *serial, char *buf, int buflen);

/*
 * PXView's sr_receive_data_callback_t is typed as
 *   int (*)(int fd, int revents, const struct sr_dev_inst *sdi)
 * (see libsigrok-internal.h). The original sigrok source used
 *   int (int fd, int revents, void *cb_data)
 * so the signature is adapted here: the opaque cb_data is replaced by a
 * typed const sdi, and the body no longer needs to dereference cb_data.
 */
SR_PRIV int gpd_receive_data(int fd, int revents, const struct sr_dev_inst *sdi);

#endif
