/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Matthias Heidbrink <m-sigrok@heidbrink.biz>
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

#ifndef LIBSIGROK_HARDWARE_NORMA_DMM_PROTOCOL_H
#define LIBSIGROK_HARDWARE_NORMA_DMM_PROTOCOL_H

#include "hardware/compat/compat.h"
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <glib.h>

#define LOG_PREFIX "norma-dmm"

/*
 * ALL_ZERO sentinel for array terminators (standard sigrok idiom, not
 * provided by PXView's libsigrok-internal.h).
 */
#ifndef ALL_ZERO
#define ALL_ZERO { 0 }
#endif

#define NMADMM_BUFSIZE 256

#define NMADMM_TIMEOUT_MS 2000 /**< Request timeout. */

/*
 * Serial baudrate for Norma DM9x0 / Siemens B102x DMMs. Used by the
 * serial_timeout() calls (PXView's compat layer takes a 3-arg form:
 * serial_timeout(serial, baudrate, bytes)).
 */
#define NMADMM_BAUDRATE 4800

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time (same pattern as fluke-dmm).
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

/** Norma DMM request types (used ones only, the DMMs support about 50). */
enum {
	NMADMM_REQ_IDN = 0,	/**< Request identity */
	NMADMM_REQ_STATUS,	/**< Request device status (value + ...) */
};

/** Defines requests used to communicate with device. */
struct nmadmm_req {
	int req_type;		/**< Request type. */
	const char *req_str;	/**< Request string. */
};

/** Strings for requests. */
extern const struct nmadmm_req nmadmm_requests[];

struct dev_context {
	int type;		/**< DM9x0, e.g. 5 = DM950 */

	struct sr_sw_limits limits;

	int last_req;			/**< Last request. */
	int64_t req_sent_at;		/**< Request sent. */
	gboolean last_req_pending;	/**< Last request not answered yet. */
	int lowbatt;			/**< Low battery. 1=low, 2=critical. */

	uint8_t buf[NMADMM_BUFSIZE];	/**< Buffer for read callback */
	int buflen;			/**< Data len in buf */
};

/*
 * PXView's sr_receive_data_callback_t passes the sdi directly as the third
 * argument (const struct sr_dev_inst *sdi) instead of the void *cb_data
 * that standard sigrok uses.
 */
SR_PRIV int norma_dmm_receive_data(int fd, int revents, const struct sr_dev_inst *sdi);
SR_PRIV int xgittoint(char xgit);

#endif
