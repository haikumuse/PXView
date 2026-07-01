/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2017-2021 Frank Stettner <frank-stettner@gmx.net>
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

#ifndef LIBSIGROK_HARDWARE_HP_3478A_PROTOCOL_H
#define LIBSIGROK_HARDWARE_HP_3478A_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <glib.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "hp-3478a"

/*
 * PXView's libsigrok.h does not define several standard sigrok config keys
 * that this DMM driver needs. Provide them here with unique values that do
 * not collide with PXView's existing SR_CONF_* keys (which occupy the
 * 10000-10006 device-type range, the 30000-30098 config range, and the
 * 50000-50007 acquisition range). These compat values live in a reserved
 * gap (30160+) next to the DSO/sound compat keys in compat_config.h, and
 * match the approach used by other compat drivers (e.g. hp-3457a,
 * rdtech-dps).
 */
#ifndef SR_CONF_MEASURED_QUANTITY
#define SR_CONF_MEASURED_QUANTITY 30160
#endif
#ifndef SR_CONF_DIGITS
#define SR_CONF_DIGITS 30162
#endif
#ifndef SR_CONF_RANGE
#define SR_CONF_RANGE 30234
#endif

/*
 * PXView's libsigrok.h mqflags enum ends at SR_MQFLAG_SPL_PCT_OVER_ALARM
 * (0x10000) and does not include SR_MQFLAG_FOUR_WIRE, which this driver
 * uses for 4-wire resistance measurements. Define it here with the standard
 * sigrok value (0x200000), guarded so it does not clash if PXView later
 * adds it upstream.
 */
#ifndef SR_MQFLAG_FOUR_WIRE
#define SR_MQFLAG_FOUR_WIRE 0x200000
#endif

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time (same pattern as fluke-dmm/protocol.h).
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
 * PXView's compat SCPI layer (compat_scpi.c/h) does not provide
 * sr_scpi_gpib_spoll() because PXView has no GPIB transport. Provide a local
 * stub that signals "not available" (SR_ERR_NA) so the receive callback can
 * fall back to direct reading, matching the approach used by other PXView
 * SCPI DMM drivers (e.g. hp-3457a) which read directly without spoll.
 */
#ifndef sr_scpi_gpib_spoll
static inline int sr_scpi_gpib_spoll(struct sr_scpi_dev_inst *scpi, char *spoll)
{
	(void)scpi;
	if (spoll)
		*spoll = 0;
	return SR_ERR_NA;
}
#endif

#define SB1_FUNCTION_BLOCK	0b11100000
#define SB1_RANGE_BLOCK		0b00011100
#define SB1_DIGITS_BLOCK	0b00000011

/** Status Byte 1 (Function) */
enum sb1_function {
	FUNCTION_VDC			= 0b00100000,
	FUNCTION_VAC			= 0b01000000,
	FUNCTION_2WR			= 0b01100000,
	FUNCTION_4WR			= 0b10000000,
	FUNCTION_ADC			= 0b10100000,
	FUNCTION_AAC			= 0b11000000,
	FUNCTION_EXR			= 0b11100000,
};

/** Status Byte 1 (Range V DC) */
enum sb1_range_vdc {
	RANGE_VDC_30MV			= 0b00000100,
	RANGE_VDC_300MV			= 0b00001000,
	RANGE_VDC_3V			= 0b00001100,
	RANGE_VDC_30V			= 0b00010000,
	RANGE_VDC_300V			= 0b00010100,
};

/** Status Byte 1 (Range V AC) */
enum sb1_range_vac {
	RANGE_VAC_300MV			= 0b00000100,
	RANGE_VAC_3V			= 0b00001000,
	RANGE_VAC_30V			= 0b00001100,
	RANGE_VAC_300V			= 0b00010000,
};

/** Status Byte 1 (Range A) */
enum sb1_range_a {
	RANGE_A_300MA			= 0b00000100,
	RANGE_A_3A			= 0b00001000,
};

/** Status Byte 1 (Range Ohm) */
enum sb1_range_ohm {
	RANGE_OHM_30R			= 0b00000100,
	RANGE_OHM_300R			= 0b00001000,
	RANGE_OHM_3KR			= 0b00001100,
	RANGE_OHM_30KR			= 0b00010000,
	RANGE_OHM_300KR			= 0b00010100,
	RANGE_OHM_3MR			= 0b00011000,
	RANGE_OHM_30MR			= 0b00011100,
};

/** Status Byte 1 (Digits) */
enum sb1_digits {
	DIGITS_5_5			= 0b00000001,
	DIGITS_4_5			= 0b00000010,
	DIGITS_3_5			= 0b00000011,
};

/** Status Byte 2 */
enum sb2_status {
	STATUS_INT_TRIGGER		= (1 << 0),
	STATUS_AUTO_RANGE		= (1 << 1),
	STATUS_AUTO_ZERO		= (1 << 2),
	STATUS_50HZ			= (1 << 3),
	STATUS_FRONT_TERMINAL		= (1 << 4),
	STATUS_CAL_RAM			= (1 << 5),
	STATUS_EXT_TRIGGER		= (1 << 6),
};

/** Status Byte 3 (Serial Poll Mask) */
enum sb3_srq {
	SRQ_BUS_AVAIL			= (1 << 0),
	SRQ_SYNTAX_ERR			= (1 << 2),
	SRQ_HARDWARE_ERR		= (1 << 3),
	SRQ_KEYBORD			= (1 << 4),
	SRQ_CAL_FAILED			= (1 << 5),
	SRQ_POWER_ON			= (1 << 7),
};

/** Status Byte 4 (Error) */
enum sb4_error {
	ERROR_SELF_TEST			= (1 << 0),
	ERROR_RAM_SELF_TEST		= (1 << 1),
	ERROR_ROM_SELF_TEST		= (1 << 2),
	ERROR_AD_SLOPE			= (1 << 3),
	ERROR_AD_SELF_TEST		= (1 << 4),
	ERROR_AD_LINK			= (1 << 5),
};

/** Channel connector (front terminals or rear terminals. */
enum terminal_connector {
	TERMINAL_FRONT,
	TERMINAL_REAR,
};

/** Available triggers */
enum trigger_state {
	TRIGGER_UNDEFINED,
	TRIGGER_EXTERNAL,
	TRIGGER_INTERNAL,
};

/** Available line frequencies */
enum line_freq {
	LINE_50HZ,
	LINE_60HZ,
};

struct dev_context {
	struct sr_sw_limits limits;

	double measurement;
	enum sr_mq measurement_mq;
	/**
	 * The measurement mq flag can contain none or one of the
	 * following flags: AC, DC, or 4-wire.
	 */
	enum sr_mqflag measurement_mq_flag;
	/**
	 * The acquisition mq flags can contain multiple flags,
	 * for example autoranging, RMS, etc.
	 */
	enum sr_mqflag acquisition_mq_flags;
	enum sr_unit measurement_unit;
	int range_exp;
	/**
	 * The total number of digits. Rounded up from the resoultion of
	 * the device, so a 5.5 resolution would be 6 digits.
	 */
	uint8_t digits;
	/**
	 * The digits used for encoding.digits and spec.spec_digits in
	 * the analog payload.
	 */
	uint8_t sr_digits;

	enum terminal_connector terminal;
	enum trigger_state trigger;
	enum line_freq line;
	gboolean auto_zero;
	gboolean calibration;
};

struct channel_context {
	int index;
	enum terminal_connector location;
};

SR_PRIV int hp_3478a_set_mq(const struct sr_dev_inst *sdi, enum sr_mq mq,
				enum sr_mqflag mq_flags);
SR_PRIV int hp_3478a_set_range(const struct sr_dev_inst *sdi, int range_exp);
SR_PRIV int hp_3478a_set_digits(const struct sr_dev_inst *sdi, uint8_t digits);
SR_PRIV int hp_3478a_get_status_bytes(const struct sr_dev_inst *sdi);
/*
 * PXView's sr_receive_data_callback_t is typed as
 *   int (*)(int fd, int revents, const struct sr_dev_inst *sdi)
 * (see libsigrok-internal.h:303). The original sigrok source used
 *   int (int fd, int revents, void *cb_data)
 * so the signature is adapted here: the opaque cb_data is replaced by a
 * typed const sdi, and the body no longer needs to dereference cb_data.
 */
SR_PRIV int hp_3478a_receive_data(int fd, int revents, const struct sr_dev_inst *sdi);

#endif
