/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
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

#ifndef LIBSIGROK_HARDWARE_UNI_T_UT32X_PROTOCOL_H
#define LIBSIGROK_HARDWARE_UNI_T_UT32X_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <string.h>
#include "hardware/compat/compat.h"

#define LOG_PREFIX "uni-t-ut32x"

#define DEFAULT_DATA_SOURCE DATA_SOURCE_LIVE

#define PACKET_SIZE	19

enum ut32x_data_source {
	DATA_SOURCE_LIVE,
	DATA_SOURCE_MEMORY,
};

enum ut32x_cmd_code {
	CMD_GET_LIVE = 1,
	CMD_STOP = 2,
	CMD_GET_STORED = 7,
};

/*
 * PXView does not provide struct sr_sw_limits or the sr_sw_limits_*
 * helpers that standard sigrok offers. Define them locally as static
 * inline (with a unique guard) so this driver is self-contained and
 * cannot clash with copies living in other compat drivers at link time
 * (PXView's SR_PRIV macro is empty, so non-static definitions would
 * yield multiple-definition link errors). Both protocol.c and api.c
 * include this header, so each translation unit gets its own copy.
 * This driver only uses limit_samples / limit_msec.
 */
#ifndef UT32X_SR_SW_LIMITS_DEFINED
#define UT32X_SR_SW_LIMITS_DEFINED
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
#endif /* UT32X_SR_SW_LIMITS_DEFINED */

struct dev_context {
	struct sr_sw_limits limits;
	enum ut32x_data_source data_source;
	uint8_t packet[PACKET_SIZE];
	size_t packet_len;
};

/*
 * PXView's sr_receive_data_callback_t passes the sdi directly as the third
 * argument (const struct sr_dev_inst *sdi) instead of the void *cb_data
 * that standard sigrok uses.
 */
SR_PRIV int ut32x_handle_events(int fd, int revents, const struct sr_dev_inst *sdi);

#endif
