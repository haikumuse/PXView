/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2021 LUMERIIX
 * Copyright (C) 2024 Daniel Anselmi <danselmi@gmx.ch>
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

#ifndef LIBSIGROK_HARDWARE_BKPRECISION_1856D_PROTOCOL_H
#define LIBSIGROK_HARDWARE_BKPRECISION_1856D_PROTOCOL_H

/*
 * Rule 1: include replacement. The standard sigrok source used
 *   #include <libsigrok/libsigrok.h>
 *   #include "libsigrok-internal.h"
 * PXView's compat layer provides a single entry header instead.
 */
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "bkprecision-1856d"

#define SERIALCOMM "9600/8n1/dtr=1/rts=0"

#define BKPRECISION1856D_MSG_SIZE 15
#define BKPRECISION1856D_MSG_NUMBER_SIZE 10

enum {
	InputA = 0,
	InputC = 1,
};

/*
 * PXView's libsigrok.h does not define several standard sigrok config keys
 * that this frequency counter driver needs. Provide them here with unique
 * values that do not collide with PXView's existing SR_CONF_* keys (which
 * occupy the 10000-10006 device-type range, the 30000-30098 config range,
 * and the 50000-50007 acquisition range). These compat values live in a
 * reserved gap next to the DSO/sound compat keys in compat_config.h, and
 * match the approach used by other compat drivers (e.g. hp-3457a).
 *
 *   SR_CONF_FREQUENCY_COUNTER : driver-class identifier (drvopts[]), placed
 *                               next to SR_CONF_MULTIMETER=10002 /
 *                               SR_CONF_SOUNDLEVELMETER=10004 in the
 *                               device-type range.
 *   SR_CONF_GATE_TIME        : device option (devopts[]), placed in the
 *                               30160+ compat range alongside
 *                               SR_CONF_MEASURED_QUANTITY etc.
 */
#ifndef SR_CONF_FREQUENCY_COUNTER
#define SR_CONF_FREQUENCY_COUNTER 10005
#endif
#ifndef SR_CONF_GATE_TIME
#define SR_CONF_GATE_TIME 30163
#endif

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time. The SR_SW_LIMITS_H guard prevents redefinition
 * if this header is ever included twice in the same translation unit
 * (e.g. via another compat driver's protocol.h that follows the same
 * pattern).
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

struct dev_context {
	struct sr_sw_limits sw_limits;
	unsigned int sel_input;
	unsigned int curr_sel_input;
	unsigned int gate_time;

	char buffer[BKPRECISION1856D_MSG_SIZE];
	unsigned int buffer_level;
};

/*
 * Rule 14: PXView's sr_receive_data_callback_t is typed as
 *   int (*)(int fd, int revents, const struct sr_dev_inst *sdi)
 * (see libsigrok-internal.h:303). The original sigrok source used
 *   int (int fd, int revents, void *cb_data)
 * so the signature is adapted here: the opaque cb_data is replaced by a
 * typed const sdi, and the body no longer needs to dereference cb_data.
 */
SR_PRIV int bkprecision_1856d_receive_data(int fd, int revents,
		const struct sr_dev_inst *sdi);
SR_PRIV void bkprecision_1856d_init(const struct sr_dev_inst *sdi);
SR_PRIV void bkprecision_1856d_set_gate_time(struct dev_context *devc,
		int time);
SR_PRIV void bkprecision_1856d_select_input(struct dev_context *devc,
		int intput);

#endif
