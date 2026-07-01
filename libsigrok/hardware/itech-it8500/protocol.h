/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2020 Timo Kokkonen <tjko@iki.fi>
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

#ifndef LIBSIGROK_HARDWARE_ITECH_IT8500_PROTOCOL_H
#define LIBSIGROK_HARDWARE_ITECH_IT8500_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <glib.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "itech-it8500"

/*
 * PXView does not define several standard sigrok config keys that this
 * electronic load driver needs. Provide them here with unique values that
 * do not collide with PXView's existing SR_CONF_* keys (which occupy the
 * 10000-10006 device-type range, the 30000-30107 config range, and the
 * 50000-50007 acquisition range). These compat values live in reserved
 * gaps. The values mirror those used by the korad-kaxxxxp, atten-pps3xxx,
 * manson-hcs-3xxx, motech-lps-30x, gwinstek-psp and rdtech-dps compat
 * drivers so all power-supply/electronic-load drivers agree when compiled
 * together.
 */
#ifndef SR_CONF_ELECTRONIC_LOAD
#define SR_CONF_ELECTRONIC_LOAD 10009
#endif
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
/*
 * Additional electronic-load specific keys. Assign unique values in the
 * reserved compat range that do not collide with the keys above or with
 * PXView's keys or with other compat drivers' keys (e.g. MODBUSADDR=30240,
 * RANGE=30234, CHANNEL_CONFIG=30228).
 */
#ifndef SR_CONF_POWER
#define SR_CONF_POWER 30250
#endif
#ifndef SR_CONF_POWER_TARGET
#define SR_CONF_POWER_TARGET 30251
#endif
#ifndef SR_CONF_RESISTANCE_TARGET
#define SR_CONF_RESISTANCE_TARGET 30252
#endif
#ifndef SR_CONF_OVER_TEMPERATURE_PROTECTION
#define SR_CONF_OVER_TEMPERATURE_PROTECTION 30253
#endif
#ifndef SR_CONF_UNDER_VOLTAGE_CONDITION
#define SR_CONF_UNDER_VOLTAGE_CONDITION 30254
#endif
#ifndef SR_CONF_UNDER_VOLTAGE_CONDITION_ACTIVE
#define SR_CONF_UNDER_VOLTAGE_CONDITION_ACTIVE 30255
#endif
#ifndef SR_CONF_UNDER_VOLTAGE_CONDITION_THRESHOLD
#define SR_CONF_UNDER_VOLTAGE_CONDITION_THRESHOLD 30256
#endif

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time.
 */
#ifndef ITECH_IT8500_SR_SW_LIMITS_DEFINED
#define ITECH_IT8500_SR_SW_LIMITS_DEFINED
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
#endif /* ITECH_IT8500_SR_SW_LIMITS_DEFINED */

/*
 * Unit uses 26 byte binary packets for communications.
 * Packets have fixed format:
 *
 * Offset|Length|Description
 * ------|------|-------------------------------------
 *     0 |    1 | Preable (always set to 0xAA).
 *     1 |    1 | Unit Address (0-254, 255=broadcast).
 *     2 |    1 | Command number.
 *     3 |   22 | Variable data.
 *    25 |    1 | Parity code (checksum).
 */
#define IT8500_HEADER_LEN 3
#define IT8500_DATA_LEN 22
#define IT8500_PACKET_LEN (IT8500_HEADER_LEN + IT8500_DATA_LEN + 1)

/*
 * Data structure to track commands and reponses.
 */
struct itech_it8500_cmd_packet {
	uint8_t command; /* Command number. */
	uint8_t address; /* Unit address: 0..254 (255 = broadcast). */
	uint8_t data[IT8500_DATA_LEN]; /* Command/Response data. */
};

#define IT8500_PREAMBLE 0xaa

/*
 * Operating modes.
 * Note! These map directly to mode numbers used in CMD_SET_MODE
 * and CMD_GET_MODE commands, so values are manually defined below.
 */
enum itech_it8500_modes {
	CC = 0,
	CV = 1,
	CW = 2,
	CR = 3,
	IT8500_MODES, /* Total count, for internal use. */
};

enum itech_it8500_command {
	CMD_GET_LOAD_LIMITS = 0x01,
	CMD_SET_HW_OPP_VALUE = 0x02,
	CMD_GET_HW_OPP_VALUE = 0x03,
	CMD_SET_VON_MODE = 0x0e,
	CMD_GET_VON_MODE = 0x0f,
	CMD_SET_VON_VALUE = 0x10,
	CMD_GET_VON_VALUE = 0x11,
	CMD_RESPONSE = 0x12, /* Response to commands not returning any data. */
	CMD_SET_REMOTE_MODE = 0x20,
	CMD_LOAD_ON_OFF = 0x21,
	CMD_SET_MAX_VOLTAGE = 0x22,
	CMD_GET_MAX_VOLTAGE = 0x23,
	CMD_SET_MAX_CURRENT = 0x24,
	CMD_GET_MAX_CURRENT = 0x25,
	CMD_SET_MAX_POWER = 0x26,
	CMD_GET_MAX_POWER = 0x27,
	CMD_SET_MODE = 0x28,
	CMD_GET_MODE = 0x29,
	CMD_SET_CC_CURRENT = 0x2a,
	CMD_GET_CC_CURRENT = 0x2b,
	CMD_SET_CV_VOLTAGE = 0x2c,
	CMD_GET_CV_VOLTAGE = 0x2d,
	CMD_SET_CW_POWER = 0x2e,
	CMD_GET_CW_POWER = 0x2f,
	CMD_SET_CR_RESISTANCE = 0x30,
	CMD_GET_CR_RESISTANCE = 0x31,
	CMD_SET_BATTERY_MIN_VOLTAGE = 0x4e,
	CMD_GET_BATTERY_MIN_VOLTAGE = 0x4f,
	CMD_SET_LOAD_ON_TIMER = 0x50,
	CMD_GET_LOAD_ON_TIMER = 0x51,
	CMD_LOAD_ON_TIMER = 0x52,
	CMD_LOAD_ON_TIME_STATUS = 0x53,
	CMD_SET_ADDRESS = 0x54,
	CMD_LOCAL_CONTROL = 0x55,
	CMD_REMOTE_SENSING = 0x56,
	CMD_REMOTE_SENSING_STATUS = 0x57,
	CMD_SET_TRIGGER_SOURCE = 0x58,
	CMD_GET_TRIGGER_SOURCE = 0x59,
	CMD_TRIGGER = 0x5a,
	CMD_SAVE_SETTINGS = 0x5b,
	CMD_LOAD_SETTINGS = 0x5c,
	CMD_SET_FUNCTION = 0x5d,
	CMD_GET_FUNCTION = 0x5e,
	CMD_GET_STATE = 0x5f,
	CMD_GET_MODEL_INFO = 0x6a,
	CMD_GET_BARCODE_INFO = 0x6b,
	CMD_SET_OCP_VALUE = 0x80,
	CMD_GET_OCP_VALUE = 0x81,
	CMD_SET_OCP_DELAY = 0x82,
	CMD_GET_OCP_DELAY = 0x83,
	CMD_ENABLE_OCP = 0x84,
	CMD_DISABLE_OCP = 0x85,
	CMD_SET_OPP_VALUE = 0x86,
	CMD_GET_OPP_VALUE = 0x87,
	CMD_SET_OPP_DELAY = 0x88,
	CMD_GET_OPP_DELAY = 0x89,
};

/* Status packet status byte values. */
enum itech_it8500_status_code {
	STS_COMMAND_SUCCESSFUL = 0x80,
	STS_INVALID_CHECKSUM = 0x90,
	STS_INVALID_PARAMETER = 0xa0,
	STS_UNKNOWN_COMMAND = 0xb0,
	STS_INVALID_COMMAND = 0xc0,
};

/*
 * "Operation state" register flags.
 */
#define OS_CAL_FLAG   (1UL << 0)
#define OS_WTG_FLAG   (1UL << 1)
#define OS_REM_FLAG   (1UL << 2)
#define OS_OUT_FLAG   (1UL << 3)
#define OS_LOCAL_FLAG (1UL << 4)
#define OS_SENSE_FLAG (1UL << 5)
#define OS_LOT_FLAG   (1UL << 6)

/*
 * "Demand state" register flags.
 */
#define DS_RV_FLAG      (1UL << 0)
#define DS_OV_FLAG      (1UL << 1)
#define DS_OC_FLAG      (1UL << 2)
#define DS_OP_FLAG      (1UL << 3)
#define DS_OT_FLAG      (1UL << 4)
#define DS_SV_FLAG      (1UL << 5)
#define DS_CC_MODE_FLAG (1UL << 6)
#define DS_CV_MODE_FLAG (1UL << 7)
#define DS_CW_MODE_FLAG (1UL << 8)
#define DS_CR_MODE_FLAG (1UL << 9)

#define IT8500_MAX_MODEL_NAME_LEN 5

struct dev_context {
	char model[IT8500_MAX_MODEL_NAME_LEN + 1];
	uint8_t fw_ver_major;
	uint8_t fw_ver_minor;
	uint8_t address;
	double max_current;
	double min_voltage;
	double max_voltage;
	double max_power;
	double min_resistance;
	double max_resistance;
	size_t max_sample_rate_idx;

	double voltage;
	double current;
	double power;
	uint8_t operation_state;
	uint16_t demand_state;
	enum itech_it8500_modes mode;
	gboolean load_on;

	uint64_t sample_rate;
	struct sr_sw_limits limits;

	GMutex mutex;
};

/*
 * Local replacement for standard sigrok's sr_session_send_meta().
 * Sends a META packet with a single config key/value pair. Implemented
 * in protocol.c (same approach as the korad-kaxxxxp compat driver).
 */
SR_PRIV int sr_session_send_meta(const struct sr_dev_inst *sdi,
		uint32_t key, GVariant *data);

/*
 * Local replacements for standard sigrok's std_session_send_df_frame_begin/end().
 * PXView's libsigrok only provides std_session_send_df_header/end with a
 * prefix argument, not the frame variants. Implemented in protocol.c.
 */
SR_PRIV int std_session_send_df_frame_begin(const struct sr_dev_inst *sdi);
SR_PRIV int std_session_send_df_frame_end(const struct sr_dev_inst *sdi);

/*
 * Local wrapper around PXView's 3-arg serial_timeout(). Standard sigrok's
 * serial_timeout() takes (serial, bytes) and derives the baudrate from the
 * serial port's internal state. PXView requires the baudrate explicitly, so
 * parse it from the serialcomm string (format like "9600/8n1"). Falls back
 * to a conservative default when parsing fails. Implemented in protocol.c.
 */
SR_PRIV int itech_it8500_serial_timeout(struct sr_serial_dev_inst *serial,
		int bytes);

/*
 * Parse the baudrate from a serialcomm string (format like "9600/8n1").
 * Returns 0 if parsing fails. Used by scan() to compute the maximum safe
 * sample rate based on the serial connection speed.
 */
SR_PRIV uint64_t itech_it8500_parse_baudrate(struct sr_serial_dev_inst *serial);

SR_PRIV uint8_t itech_it8500_checksum(const uint8_t *packet);
SR_PRIV const char *itech_it8500_mode_to_string(enum itech_it8500_modes mode);
SR_PRIV int itech_it8500_string_to_mode(const char *modename,
		enum itech_it8500_modes *mode);
SR_PRIV int itech_it8500_send_cmd(struct sr_serial_dev_inst *serial,
		struct itech_it8500_cmd_packet *cmd,
		struct itech_it8500_cmd_packet **response);
SR_PRIV int itech_it8500_cmd(const struct sr_dev_inst *sdi,
		struct itech_it8500_cmd_packet *cmd,
		struct itech_it8500_cmd_packet **response);
SR_PRIV void itech_it8500_status_change(const struct sr_dev_inst *sdi,
		uint8_t old_op, uint8_t new_op,
		uint16_t old_de, uint16_t new_de,
		enum itech_it8500_modes old_m, enum itech_it8500_modes new_m);
SR_PRIV int itech_it8500_get_status(const struct sr_dev_inst *sdi);
SR_PRIV int itech_it8500_get_int(const struct sr_dev_inst *sdi,
		enum itech_it8500_command command, int *result);
SR_PRIV void itech_it8500_channel_send_value(const struct sr_dev_inst *sdi,
		struct sr_channel *ch, double value, enum sr_mq mq,
		enum sr_mqflag mqflags, enum sr_unit unit, int digits);
SR_PRIV int itech_it8500_receive_data(int fd, int revents,
		const struct sr_dev_inst *sdi);

#endif
