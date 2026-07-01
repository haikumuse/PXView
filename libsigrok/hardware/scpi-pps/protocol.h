/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2017,2019 Frank Stettner <frank-stettner@gmx.net>
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

#ifndef LIBSIGROK_HARDWARE_SCPI_PPS_PROTOCOL_H
#define LIBSIGROK_HARDWARE_SCPI_PPS_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <glib.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "scpi-pps"

/*
 * Standard sigrok uses ALL_ZERO as the sentinel for scpi_command tables.
 * PXView's libsigrok does not define it; provide it here so profiles.c
 * keeps working unchanged.
 */
#ifndef ALL_ZERO
#define ALL_ZERO { 0 }
#endif

/*
 * PXView does not define several standard sigrok config keys that this
 * generic SCPI power supply driver needs. Provide them here with unique
 * values that do not collide with PXView's existing SR_CONF_* keys (which
 * occupy the 10000-10006 device-type range, the 30000-30107 config range,
 * and the 50000-50007 acquisition range). These compat values live in
 * reserved gaps. The values mirror those used by the siglent-sdl10x0,
 * itech-it8500, maynuo-m97, korad-kaxxxxp, atten-pps3xxx, manson-hcs-3xxx,
 * motech-lps-30x, gwinstek-psp and rdtech-dps compat drivers so all
 * power-supply/electronic-load drivers agree when compiled together.
 */
#ifndef SR_CONF_POWER_SUPPLY
#define SR_CONF_POWER_SUPPLY 10008
#endif
#ifndef SR_CONF_ENABLED
#define SR_CONF_ENABLED 30200
#endif
#ifndef SR_CONF_OUTPUT_FREQUENCY
#define SR_CONF_OUTPUT_FREQUENCY 30201
#endif
#ifndef SR_CONF_OUTPUT_FREQUENCY_TARGET
#define SR_CONF_OUTPUT_FREQUENCY_TARGET 30202
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
#ifndef SR_CONF_CHANNEL_CONFIG
#define SR_CONF_CHANNEL_CONFIG 30228
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
#ifndef SR_CONF_OVER_CURRENT_PROTECTION_DELAY
#define SR_CONF_OVER_CURRENT_PROTECTION_DELAY 30234
#endif
#ifndef SR_CONF_POWER
#define SR_CONF_POWER 30250
#endif
#ifndef SR_CONF_OVER_TEMPERATURE_PROTECTION
#define SR_CONF_OVER_TEMPERATURE_PROTECTION 30253
#endif

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time. Same pattern as siglent-sdl10x0/gwinstek-psp/etc.
 */
#ifndef SCPI_PPS_SR_SW_LIMITS_DEFINED
#define SCPI_PPS_SR_SW_LIMITS_DEFINED
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
#endif /* SCPI_PPS_SR_SW_LIMITS_DEFINED */

enum pps_scpi_cmds {
	SCPI_CMD_REMOTE = 1,
	SCPI_CMD_LOCAL,
	SCPI_CMD_BEEPER,
	SCPI_CMD_BEEPER_ENABLE,
	SCPI_CMD_BEEPER_DISABLE,
	SCPI_CMD_SELECT_CHANNEL,
	SCPI_CMD_GET_MEAS_VOLTAGE,
	SCPI_CMD_GET_MEAS_CURRENT,
	SCPI_CMD_GET_MEAS_POWER,
	SCPI_CMD_GET_MEAS_FREQUENCY,
	SCPI_CMD_GET_VOLTAGE_TARGET,
	SCPI_CMD_SET_VOLTAGE_TARGET,
	SCPI_CMD_GET_FREQUENCY_TARGET,
	SCPI_CMD_SET_FREQUENCY_TARGET,
	SCPI_CMD_GET_CURRENT_LIMIT,
	SCPI_CMD_SET_CURRENT_LIMIT,
	SCPI_CMD_GET_OUTPUT_ENABLED,
	SCPI_CMD_SET_OUTPUT_ENABLE,
	SCPI_CMD_SET_OUTPUT_DISABLE,
	SCPI_CMD_GET_OUTPUT_REGULATION,
	SCPI_CMD_GET_OVER_TEMPERATURE_PROTECTION,
	SCPI_CMD_SET_OVER_TEMPERATURE_PROTECTION_ENABLE,
	SCPI_CMD_SET_OVER_TEMPERATURE_PROTECTION_DISABLE,
	SCPI_CMD_GET_OVER_TEMPERATURE_PROTECTION_ACTIVE,
	SCPI_CMD_GET_OVER_VOLTAGE_PROTECTION_ENABLED,
	SCPI_CMD_SET_OVER_VOLTAGE_PROTECTION_ENABLE,
	SCPI_CMD_SET_OVER_VOLTAGE_PROTECTION_DISABLE,
	SCPI_CMD_GET_OVER_VOLTAGE_PROTECTION_ACTIVE,
	SCPI_CMD_GET_OVER_VOLTAGE_PROTECTION_THRESHOLD,
	SCPI_CMD_SET_OVER_VOLTAGE_PROTECTION_THRESHOLD,
	SCPI_CMD_GET_OVER_CURRENT_PROTECTION_ENABLED,
	SCPI_CMD_SET_OVER_CURRENT_PROTECTION_ENABLE,
	SCPI_CMD_SET_OVER_CURRENT_PROTECTION_DISABLE,
	SCPI_CMD_GET_OVER_CURRENT_PROTECTION_ACTIVE,
	SCPI_CMD_GET_OVER_CURRENT_PROTECTION_THRESHOLD,
	SCPI_CMD_SET_OVER_CURRENT_PROTECTION_THRESHOLD,
	SCPI_CMD_GET_OVER_CURRENT_PROTECTION_DELAY,
	SCPI_CMD_SET_OVER_CURRENT_PROTECTION_DELAY,
};

/* Defines the SCPI dialect */
enum pps_scpi_dialect {
	SCPI_DIALECT_UNKNOWN = 1,
	SCPI_DIALECT_HP_COMP,
	SCPI_DIALECT_HP_66XXB,
	SCPI_DIALECT_PHILIPS,
	SCPI_DIALECT_HMP,
	SCPI_DIALECT_KEYSIGHT_E36300A,
};

/*
 * These are bit values denoting features a device can have either globally,
 * in scpi_pps.features, or on a per-channel-group basis in
 * channel_group_spec.features.
 */
enum pps_features {
	PPS_OTP           = (1 << 0),
	PPS_OVP           = (1 << 1),
	PPS_OCP           = (1 << 2),
	PPS_INDEPENDENT   = (1 << 3),
	PPS_SERIES        = (1 << 4),
	PPS_PARALLEL      = (1 << 5),
};

struct scpi_pps {
	const char *vendor;
	const char *model;
	const enum pps_scpi_dialect dialect;
	uint64_t features;
	const uint32_t *devopts;
	unsigned int num_devopts;
	const uint32_t *devopts_cg;
	unsigned int num_devopts_cg;
	const struct channel_spec *channels;
	unsigned int num_channels;
	const struct channel_group_spec *channel_groups;
	unsigned int num_channel_groups;
	const struct scpi_command *commands;
	int (*probe_channels) (struct sr_dev_inst *sdi, struct sr_scpi_hw_info *hwinfo,
		struct channel_spec **channels, unsigned int *num_channels,
		struct channel_group_spec **channel_groups, unsigned int *num_channel_groups);
	int (*init_acquisition) (const struct sr_dev_inst *sdi);
	int (*update_status) (const struct sr_dev_inst *sdi);
};

struct channel_spec {
	const char *name;
	/* Min, max, programming resolution, spec digits, encoding digits. */
	double voltage[5];
	double current[5];
	double power[5];
	double frequency[5];
	double ovp[5];
	double ocp[5];
	double ocp_delay[5];
};

struct channel_group_spec {
	const char *name;
	uint64_t channel_index_mask;
	uint64_t features;
	/* The mqflags will only be applied to voltage and current channels! */
	enum sr_mqflag mqflags;
};

struct pps_channel {
	enum sr_mq mq;
	enum sr_mqflag mqflags;
	unsigned int hw_output_idx;
	const char *hwname;
	int digits;
};

struct pps_channel_instance {
	enum sr_mq mq;
	int command;
	const char *prefix;
};

struct pps_channel_group {
	uint64_t features;
};

enum acq_states {
	STATE_VOLTAGE,
	STATE_CURRENT,
	STATE_STOP,
};

struct dev_context {
	const struct scpi_pps *device;

	gboolean beeper_was_set;
	struct channel_spec *channels;
	struct channel_group_spec *channel_groups;

	struct sr_channel *cur_acquisition_channel;
	struct sr_sw_limits limits;
};

SR_PRIV extern unsigned int num_pps_profiles;
SR_PRIV extern const struct scpi_pps pps_profiles[];

SR_PRIV int select_channel(const struct sr_dev_inst *sdi, struct sr_channel *ch);

/*
 * Local replacement for standard sigrok's sr_next_enabled_channel(), which
 * PXView's libsigrok does not provide. Implemented in protocol.c and used
 * by both protocol.c (receive_data) and api.c (dev_acquisition_start).
 * Returns the next enabled channel after cur_channel, wrapping around to
 * the start of the list; if cur_channel is NULL the first enabled channel
 * is returned.
 */
SR_PRIV struct sr_channel *scpi_pps_next_enabled_channel(
		const struct sr_dev_inst *sdi, struct sr_channel *cur_channel);

/*
 * PXView's sr_receive_data_callback_t passes the sdi directly as the third
 * argument (const struct sr_dev_inst *sdi) instead of the void *cb_data
 * that standard sigrok uses. Adapt scpi_pps_receive_data accordingly.
 */
SR_PRIV int scpi_pps_receive_data(int fd, int revents,
		const struct sr_dev_inst *sdi);

/*
 * Local replacement for standard sigrok's sr_session_send_meta().
 * Sends a META packet with a single config key/value pair. Implemented
 * in protocol.c (same pattern as rdtech-dps/korad-kaxxxxp/itech-it8500).
 */
SR_PRIV int sr_session_send_meta(const struct sr_dev_inst *sdi,
		uint32_t key, GVariant *data);

/*
 * Local replacement for standard sigrok's sr_atoi(), which PXView's
 * libsigrok does not provide. Implemented in profiles.c (where it is
 * used by the HP/Keysight status register parsers).
 */
SR_PRIV int scpi_pps_sr_atoi(const char *str, int *ret);

#endif
