/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2018 James Churchill <pelrun@gmail.com>
 * Copyright (C) 2019 Frank Stettner <frank-stettner@gmx.net>
 * Copyright (C) 2021 Gerhard Sittig <gerhard.sittig@gmx.net>
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

#ifndef LIBSIGROK_HARDWARE_RDTECH_DPS_PROTOCOL_H
#define LIBSIGROK_HARDWARE_RDTECH_DPS_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <glib.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "rdtech-dps"

/*
 * PXView does not define several standard sigrok config keys that this
 * power supply driver needs. Provide them here with unique values that
 * do not collide with PXView's existing SR_CONF_* keys (which occupy the
 * 10000-10006 device-type range, the 30000-30107 config range, and the
 * 50000-50007 acquisition range). These compat values live in reserved
 * gaps. The values mirror those used by the korad-kaxxxxp, atten-pps3xxx,
 * manson-hcs-3xxx and motech-lps-30x compat drivers so all power-supply
 * drivers agree when compiled together.
 */
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
#ifndef SR_CONF_RANGE
#define SR_CONF_RANGE 30234
#endif
/*
 * SR_CONF_MODBUSADDR is a scan option used to specify the Modbus slave
 * address during device scan. Assign a unique value in the reserved
 * compat range that does not collide with the keys above.
 */
#ifndef SR_CONF_MODBUSADDR
#define SR_CONF_MODBUSADDR 30240
#endif

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time.
 */
#ifndef RDTECH_DPS_SR_SW_LIMITS_DEFINED
#define RDTECH_DPS_SR_SW_LIMITS_DEFINED
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
#endif /* RDTECH_DPS_SR_SW_LIMITS_DEFINED */

/*
 * Local big-endian read-with-auto-increment helpers. PXView's compat
 * layer provides write_u16be() and read_u16be() (in compat_config.h) but
 * not the _inc variants that the rdtech-dps protocol parser uses to walk
 * through received register banks. Define them here as static inline.
 */
#ifndef RDTECH_DPS_BE_INC_HELPERS_DEFINED
#define RDTECH_DPS_BE_INC_HELPERS_DEFINED
static inline uint16_t rdtech_dps_read_u16be_inc(const uint8_t **pp)
{
	uint16_t v = read_u16be(*pp);
	*pp += 2;
	return v;
}

static inline uint32_t rdtech_dps_read_u32be_inc(const uint8_t **pp)
{
	uint32_t v = read_u32be(*pp);
	*pp += 4;
	return v;
}
/* Aliases so the original protocol.c body compiles unchanged. */
#define read_u16be_inc(pp) rdtech_dps_read_u16be_inc(pp)
#define read_u32be_inc(pp) rdtech_dps_read_u32be_inc(pp)
#endif /* RDTECH_DPS_BE_INC_HELPERS_DEFINED */

/*
 * Local Modbus RTU support.
 *
 * PXView's libsigrok does not provide the sr_modbus_* API or
 * struct sr_modbus_dev_inst. The rdtech-dps driver talks to its hardware
 * over Modbus RTU on a serial port, so we implement a small self-contained
 * Modbus serial-RTU layer here, modelled on standard sigrok's modbus.c and
 * modbus_serial_rtu.c. The struct layout and the public function names
 * mirror standard sigrok so the original protocol logic compiles with
 * minimal changes.
 *
 * Differences from standard sigrok:
 *  - sr_modbus_source_add/remove do NOT take a session parameter (PXView's
 *    serial_source_add/remove are session-less too).
 *  - sr_modbus_scan takes a struct sr_dev_driver * (uses di->priv as the
 *    drv_context) instead of a bare struct drv_context *.
 */
#ifndef RDTECH_DPS_MODBUS_DEFINED
#define RDTECH_DPS_MODBUS_DEFINED

/* Modbus serial RTU private state. */
struct modbus_serial_rtu {
	struct sr_serial_dev_inst *serial;
	uint8_t slave_addr;
	uint16_t crc;
};

/* Modbus device instance (mirrors standard sigrok's struct sr_modbus_dev_inst). */
struct sr_modbus_dev_inst {
	const char *name;
	const char *prefix;
	size_t priv_size;
	GSList *(*scan)(int modbusaddr);
	int (*dev_inst_new)(void *priv, const char *resource, char **params,
			const char *serialcomm, int modbusaddr);
	int (*open)(void *priv);
	int (*source_add)(void *priv, int events, int timeout,
			sr_receive_data_callback_t cb, const struct sr_dev_inst *sdi);
	int (*source_remove)(void *priv);
	int (*send)(void *priv, const uint8_t *buffer, int buffer_size);
	int (*read_begin)(void *priv, uint8_t *function_code);
	int (*read_data)(void *priv, uint8_t *buf, int maxlen);
	int (*read_end)(void *priv);
	int (*close)(void *priv);
	void (*free)(void *priv);
	void *priv;
	int read_timeout_ms;
};

SR_PRIV struct sr_modbus_dev_inst *modbus_dev_inst_new(const char *resource,
		const char *serialcomm, int modbusaddr);
SR_PRIV int sr_modbus_open(struct sr_modbus_dev_inst *modbus);
SR_PRIV int sr_modbus_close(struct sr_modbus_dev_inst *modbus);
SR_PRIV void sr_modbus_free(struct sr_modbus_dev_inst *modbus);
SR_PRIV int sr_modbus_source_add(struct sr_modbus_dev_inst *modbus,
		int events, int timeout, sr_receive_data_callback_t cb,
		const struct sr_dev_inst *sdi);
SR_PRIV int sr_modbus_source_remove(struct sr_modbus_dev_inst *modbus);
SR_PRIV int sr_modbus_read_holding_registers(struct sr_modbus_dev_inst *modbus,
		int address, int nb_registers, uint16_t *registers);
SR_PRIV int sr_modbus_write_multiple_registers(struct sr_modbus_dev_inst *modbus,
		int address, int nb_registers, uint16_t *registers);
SR_PRIV GSList *sr_modbus_scan(struct sr_dev_driver *di, GSList *options,
		struct sr_dev_inst *(*probe_device)(struct sr_modbus_dev_inst *modbus));
#endif /* RDTECH_DPS_MODBUS_DEFINED */

/*
 * Local replacement for standard sigrok's sr_session_send_meta().
 * Sends a META packet with a single config key/value pair. Implemented
 * in protocol.c (same pattern as korad-kaxxxxp).
 */
SR_PRIV int sr_session_send_meta(const struct sr_dev_inst *sdi,
		uint32_t key, GVariant *data);

enum rdtech_dps_model_type {
	MODEL_NONE,
	MODEL_DPS,
	MODEL_RD,
};

struct rdtech_dps_range {
	const char *range_str;
	unsigned int max_current;
	unsigned int max_voltage;
	unsigned int max_power;
	unsigned int current_digits;
	unsigned int voltage_digits;
};

struct rdtech_dps_model {
	enum rdtech_dps_model_type model_type;
	unsigned int id;
	const char *name;
	const struct rdtech_dps_range *ranges;
	size_t n_ranges;
};

struct dev_context {
	const struct rdtech_dps_model *model;
	double current_multiplier;
	double voltage_multiplier;
	struct sr_sw_limits limits;
	GMutex rw_mutex;
	gboolean curr_ovp_state;
	gboolean curr_ocp_state;
	gboolean curr_cc_state;
	gboolean curr_out_state;
	size_t curr_range;
	gboolean acquisition_started;
};

/* Container to get and set parameter values. */
struct rdtech_dps_state {
	enum rdtech_dps_state_mask {
		STATE_LOCK = 1 << 0,
		STATE_OUTPUT_ENABLED = 1 << 1,
		STATE_REGULATION_CC = 1 << 2,
		STATE_PROTECT_OVP = 1 << 3,
		STATE_PROTECT_OCP = 1 << 4,
		STATE_PROTECT_ENABLED = 1 << 5,
		STATE_VOLTAGE_TARGET = 1 << 6,
		STATE_CURRENT_LIMIT = 1 << 7,
		STATE_OVP_THRESHOLD = 1 << 8,
		STATE_OCP_THRESHOLD = 1 << 9,
		STATE_VOLTAGE = 1 << 10,
		STATE_CURRENT = 1 << 11,
		STATE_POWER = 1 << 12,
		STATE_RANGE = 1 << 13,
	} mask;
	gboolean lock;
	gboolean output_enabled, regulation_cc;
	gboolean protect_ovp, protect_ocp, protect_enabled;
	float voltage_target, current_limit;
	float ovp_threshold, ocp_threshold;
	float voltage, current, power;
	size_t range;
};

enum rdtech_dps_state_context {
	ST_CTX_NONE,
	ST_CTX_CONFIG,
	ST_CTX_PRE_ACQ,
	ST_CTX_IN_ACQ,
};

SR_PRIV int rdtech_dps_get_state(const struct sr_dev_inst *sdi,
	struct rdtech_dps_state *state, enum rdtech_dps_state_context reason);
SR_PRIV int rdtech_dps_set_state(const struct sr_dev_inst *sdi,
	struct rdtech_dps_state *state);

SR_PRIV int rdtech_dps_get_model_version(struct sr_modbus_dev_inst *modbus,
	enum rdtech_dps_model_type model_type,
	uint16_t *model, uint16_t *version, uint32_t *serno);
SR_PRIV void rdtech_dps_update_multipliers(const struct sr_dev_inst *sdi);
SR_PRIV int rdtech_dps_update_range(const struct sr_dev_inst *sdi);
SR_PRIV int rdtech_dps_seed_receive(const struct sr_dev_inst *sdi);
SR_PRIV int rdtech_dps_receive_data(int fd, int revents, const struct sr_dev_inst *sdi);

#endif
