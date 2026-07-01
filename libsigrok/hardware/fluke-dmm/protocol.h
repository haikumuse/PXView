/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
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

#ifndef LIBSIGROK_HARDWARE_FLUKE_DMM_PROTOCOL_H
#define LIBSIGROK_HARDWARE_FLUKE_DMM_PROTOCOL_H

#include "hardware/compat/compat.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define LOG_PREFIX "fluke-dmm"

#define FLUKEDMM_BUFSIZE 256

/* Always USB-serial, 1ms is plenty. */
#define SERIAL_WRITE_TIMEOUT_MS 1

/* Supported models */
enum {
	FLUKE_87 = 1,
	FLUKE_89,
	FLUKE_187,
	FLUKE_189,
	FLUKE_190,
	FLUKE_287,
	FLUKE_289,
};

/* Supported device profiles */
struct flukedmm_profile {
	int model;
	const char *modelname;
	/* How often to poll, in ms. */
	int poll_period;
	/* If no response received, how long to wait before retrying. */
	int timeout;
};

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time.
 */
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

/*
 * PXView's libsigrok does not provide sr_atof_ascii_digits() either. This
 * local static inline parses an ASCII numeric string in a locale-independent
 * way (via g_ascii_strtod), stores the resulting float in *ret, and (when
 * @digits is non-NULL) writes the count of fractional digits found in the
 * input string to *digits. Returns SR_OK on success, SR_ERR on parse
 * failure, SR_ERR_ARG on invalid arguments.
 */
static inline int local_sr_atof_ascii_digits(const char *str, float *ret,
		int *digits)
{
	char *e;
	double tmp;
	const char *dot, *p;
	int count = 0;

	if (!str || !ret)
		return SR_ERR_ARG;

	errno = 0;
	tmp = g_ascii_strtod(str, &e);
	if (e == str || errno != 0)
		return SR_ERR;
	*ret = (float)tmp;

	/* Count digits after the decimal point (exponent suffix ignored). */
	dot = strchr(str, '.');
	if (dot) {
		p = dot + 1;
		while (*p >= '0' && *p <= '9') {
			count++;
			p++;
		}
	}
	if (digits)
		*digits = count;

	return SR_OK;
}

struct dev_context {
	const struct flukedmm_profile *profile;
	struct sr_sw_limits limits;

	char buf[FLUKEDMM_BUFSIZE];
	int buflen;
	int64_t cmd_sent_at;
	int expect_response;
	int meas_type;
	int is_relative;
	/*
	 * PXView's libsigrok.h declares the SR_MQ_*, SR_UNIT_*, SR_MQFLAG_*
	 * constants via anonymous enums and exposes the matching fields on
	 * struct sr_datafeed_analog as int/int/uint64_t. Mirror that layout
	 * here so assignments from these constants stay type-correct without
	 * needing a local "enum sr_mq/sr_unit/sr_mqflag" tag declaration.
	 */
	int mq;
	int unit;
	uint64_t mqflags;
};

SR_PRIV void fluke_handle_qm_18x(const struct sr_dev_inst *sdi, char **tokens);
SR_PRIV void fluke_handle_qm_190(const struct sr_dev_inst *sdi, char **tokens);
SR_PRIV void fluke_handle_qm_28x(const struct sr_dev_inst *sdi, char **tokens);

SR_PRIV int fluke_receive_data(int fd, int revents, const struct sr_dev_inst *sdi);

#endif
