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
 * helpers that standard sigrok offers. Define them locally with a unique
 * guard so this driver compiles standalone without clashing with other
 * compat drivers' own definitions.
 */
#ifndef UT32X_SR_SW_LIMITS_DEFINED
#define UT32X_SR_SW_LIMITS_DEFINED
struct sr_sw_limits {
	uint64_t limit_samples;
	uint64_t limit_msec;
	int64_t starttime_ms;
	uint64_t samples_read;
};

SR_PRIV void sr_sw_limits_init(struct sr_sw_limits *limits);
SR_PRIV int sr_sw_limits_config_get(const struct sr_sw_limits *limits,
		uint32_t key, GVariant **data);
SR_PRIV int sr_sw_limits_config_set(struct sr_sw_limits *limits,
		uint32_t key, GVariant *data);
SR_PRIV void sr_sw_limits_acquisition_start(struct sr_sw_limits *limits);
SR_PRIV void sr_sw_limits_update_samples_read(struct sr_sw_limits *limits,
		uint64_t count);
SR_PRIV gboolean sr_sw_limits_check(const struct sr_sw_limits *limits);
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
