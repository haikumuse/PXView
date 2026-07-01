/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Aurelien Jacobs <aurel@gnuage.org>
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

#ifndef LIBSIGROK_HARDWARE_TELEINFO_PROTOCOL_H
#define LIBSIGROK_HARDWARE_TELEINFO_PROTOCOL_H

#include "hardware/compat/compat.h"
#include <string.h>

#define LOG_PREFIX "teleinfo"

/*
 * PXView's libsigrok.h does not define several measurement-quantity,
 * unit, and device-class config constants that standard sigrok provides
 * and that the Teleinfo driver needs. Define them locally with #ifndef
 * guards so this driver is self-contained and so they do not clash if
 * PXView ever introduces them upstream.
 *
 * Values are chosen to sit just past the end of the corresponding
 * PXView enum range (SR_MQ_* ends at SR_MQ_RELATIVE_HUMIDITY=10014,
 * SR_UNIT_* ends at SR_UNIT_CONCENTRATION=10016, SR_CONF drvopts end
 * at SR_CONF_HYGROMETER=10006).
 */
#ifndef SR_MQ_ENERGY
#define SR_MQ_ENERGY            10015
#endif
#ifndef SR_UNIT_VOLT_AMPERE
#define SR_UNIT_VOLT_AMPERE     10017
#endif
#ifndef SR_UNIT_WATT_HOUR
#define SR_UNIT_WATT_HOUR       10018
#endif
#ifndef SR_CONF_ENERGYMETER
#define SR_CONF_ENERGYMETER     10007
#endif

/* Default serial communication parameters for the French Teleinfo link. */
#define SERIALCOMM "1200/7e1"
#define SERIAL_BAUDRATE 1200

enum optarif {
	OPTARIF_NONE,
	OPTARIF_BASE,
	OPTARIF_HC,
	OPTARIF_EJP,
	OPTARIF_BBR,
};

#define TELEINFO_BUF_SIZE 256

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

struct dev_context {
	struct sr_sw_limits sw_limits;
	enum optarif optarif; /**< The device mode (which measures are reported) */
	uint8_t buf[TELEINFO_BUF_SIZE];
	int buf_len;
};

SR_PRIV gboolean teleinfo_packet_valid(const uint8_t *buf);
SR_PRIV int teleinfo_get_optarif(const uint8_t *buf);

/*
 * PXView's sr_receive_data_callback_t passes the sdi directly as the
 * third argument (const struct sr_dev_inst *sdi) instead of the
 * void *cb_data that standard sigrok uses.
 */
SR_PRIV int teleinfo_receive_data(int fd, int revents,
		const struct sr_dev_inst *sdi);

#endif
