/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2015 Uwe Hermann <uwe@hermann-uwe.de>
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

#ifndef LIBSIGROK_HARDWARE_KERN_SCALE_PROTOCOL_H
#define LIBSIGROK_HARDWARE_KERN_SCALE_PROTOCOL_H

#include "hardware/compat/compat.h"
#include <string.h>

#define LOG_PREFIX "kern-scale"

/*
 * KERN EW 6200-2NM ships with a default 1200 baud link (8n2). The serial
 * parameters are user-configurable on the device, but PXView's
 * serial_timeout() helper takes an explicit baudrate argument (3-arg
 * signature) whereas standard sigrok's version derives it from the serial
 * handle (2-arg). Use the known default baudrate here.
 */
#define KERN_SCALE_BAUDRATE 1200

/*
 * Standard sigrok constants that PXView's libsigrok.h does not provide.
 * Define them locally (with #ifndef guards) so this driver is self-contained.
 *
 * SR_CONF_SCALE is a device-class config key (standard sigrok places it at
 * offset 14 within the device-class block that starts at SR_CONF_LOGIC_ANALYZER
 * = 10000). PXView only defines up to SR_CONF_HYGROMETER (10006), so 10014 is
 * safe and matches standard sigrok's value.
 *
 * SR_MQ_MASS follows SR_MQ_RELATIVE_HUMIDITY in standard sigrok's enum.
 *
 * The SR_UNIT_* mass units follow SR_UNIT_CONCENTRATION in standard sigrok's
 * enum. PXView stops at SR_UNIT_CONCENTRATION, so we continue from there.
 *
 * SR_MQFLAG_UNSTABLE is 0x100000 in standard sigrok (after the SPL flags).
 */
#ifndef SR_CONF_SCALE
#define SR_CONF_SCALE 10014
#endif

#ifndef SR_MQ_MASS
#define SR_MQ_MASS 10015
#endif

#ifndef SR_UNIT_GRAM
#define SR_UNIT_GRAM 10018
#endif
#ifndef SR_UNIT_CARAT
#define SR_UNIT_CARAT 10019
#endif
#ifndef SR_UNIT_OUNCE
#define SR_UNIT_OUNCE 10020
#endif
#ifndef SR_UNIT_POUND
#define SR_UNIT_POUND 10021
#endif
#ifndef SR_UNIT_TROY_OUNCE
#define SR_UNIT_TROY_OUNCE 10022
#endif
#ifndef SR_UNIT_PENNYWEIGHT
#define SR_UNIT_PENNYWEIGHT 10023
#endif
#ifndef SR_UNIT_GRAIN
#define SR_UNIT_GRAIN 10024
#endif
#ifndef SR_UNIT_TAEL
#define SR_UNIT_TAEL 10025
#endif
#ifndef SR_UNIT_MOMME
#define SR_UNIT_MOMME 10026
#endif
#ifndef SR_UNIT_TOLA
#define SR_UNIT_TOLA 10027
#endif
#ifndef SR_UNIT_PIECE
#define SR_UNIT_PIECE 10028
#endif

#ifndef SR_MQFLAG_UNSTABLE
#define SR_MQFLAG_UNSTABLE 0x100000
#endif

/*
 * KERN scale chipset info struct. Filled in by sr_kern_parse() and consumed
 * by handle_flags() to set the analog meaning's mq/unit/mqflags. Ported
 * verbatim from standard sigrok's libsigrok-internal.h.
 */
struct kern_info {
	gboolean is_gram, is_carat, is_ounce, is_pound, is_troy_ounce;
	gboolean is_pennyweight, is_grain, is_tael, is_momme, is_tola;
	gboolean is_percentage, is_piece, is_unstable, is_stable, is_error;
	int buflen;
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

#define SCALE_BUFSIZE 256

struct dev_context {
	struct sr_sw_limits limits;

	uint8_t buf[SCALE_BUFSIZE];
	int bufoffset;
	int buflen;
};

SR_PRIV gboolean sr_kern_packet_valid(const uint8_t *buf);
SR_PRIV int sr_kern_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);

/*
 * PXView's sr_receive_data_callback_t passes the sdi directly as the
 * third argument (const struct sr_dev_inst *sdi) instead of the
 * void *cb_data that standard sigrok uses.
 */
SR_PRIV int kern_scale_receive_data(int fd, int revents,
		const struct sr_dev_inst *sdi);

#endif
