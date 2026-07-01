/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2015 Aurelien Jacobs <aurel@gnuage.org>
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

#ifndef LIBSIGROK_HARDWARE_MAYNUO_M97_PROTOCOL_H
#define LIBSIGROK_HARDWARE_MAYNUO_M97_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <glib.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "maynuo-m97"

/*
 * PXView does not define several standard sigrok config keys that this
 * electronic load driver needs. Provide them here with unique values that
 * do not collide with PXView's existing SR_CONF_* keys (which occupy the
 * 10000-10006 device-type range, the 30000-30107 config range, and the
 * 50000-50007 acquisition range). These compat values live in reserved
 * gaps. The values mirror those used by the korad-kaxxxxp, atten-pps3xxx,
 * manson-hcs-3xxx, motech-lps-30x, rdtech-dps and itech-it8500 compat
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
#ifndef SR_CONF_OVER_TEMPERATURE_PROTECTION
#define SR_CONF_OVER_TEMPERATURE_PROTECTION 30253
#endif
/*
 * SR_CONF_MODBUSADDR is a scan option used to specify the Modbus slave
 * address during device scan. Assign a unique value in the reserved
 * compat range that does not collide with the keys above. Mirrors the
 * value used by the rdtech-dps compat driver.
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
#ifndef MAYNUO_M97_SR_SW_LIMITS_DEFINED
#define MAYNUO_M97_SR_SW_LIMITS_DEFINED
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
#endif /* MAYNUO_M97_SR_SW_LIMITS_DEFINED */

/*
 * Local big-endian float read/write helpers.
 *
 * Standard sigrok's libsigrok-internal.h provides RBFL()/WBFL() macros
 * that read/write a 32-bit IEEE-754 float stored in big-endian byte
 * order across two adjacent 16-bit Modbus registers. PXView's
 * libsigrok-internal.h does not provide these (it only has RB16/WB16
 * for 16-bit values), so implement them locally. Guard macro prevents
 * link-time clash with copies in other compat drivers.
 */
#ifndef MAYNUO_M97_FLTBE_HELPERS_DEFINED
#define MAYNUO_M97_FLTBE_HELPERS_DEFINED
static inline float maynuo_m97_read_fltbe(const uint8_t *p)
{
	union { uint32_t u32; float flt; } u;
	u.u32 = read_u32be(p);
	return u.flt;
}

static inline void maynuo_m97_write_fltbe(uint8_t *p, float x)
{
	union { uint32_t u; float f; } u;
	u.f = x;
	write_u32be(p, u.u);
}

#define RBFL(x) maynuo_m97_read_fltbe((const uint8_t *)(x))
#define WBFL(p, x) maynuo_m97_write_fltbe((uint8_t *)(p), (x))
#endif /* MAYNUO_M97_FLTBE_HELPERS_DEFINED */

/*
 * Local Modbus RTU support.
 *
 * PXView's libsigrok does not provide the sr_modbus_* API or
 * struct sr_modbus_dev_inst. The maynuo-m97 driver talks to its hardware
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
 *
 * The implementation mirrors the rdtech-dps compat driver's Modbus layer
 * but adds sr_modbus_read_coils() and sr_modbus_write_coil() which the
 * maynuo-m97 driver needs (it queries status bits via Modbus coils).
 */
#ifndef MAYNUO_M97_MODBUS_DEFINED
#define MAYNUO_M97_MODBUS_DEFINED

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
SR_PRIV int sr_modbus_read_coils(struct sr_modbus_dev_inst *modbus,
		int address, int nb_coils, uint8_t *coils);
SR_PRIV int sr_modbus_write_coil(struct sr_modbus_dev_inst *modbus,
		int address, int value);
SR_PRIV int sr_modbus_read_holding_registers(struct sr_modbus_dev_inst *modbus,
		int address, int nb_registers, uint16_t *registers);
SR_PRIV int sr_modbus_write_multiple_registers(struct sr_modbus_dev_inst *modbus,
		int address, int nb_registers, uint16_t *registers);
SR_PRIV GSList *sr_modbus_scan(struct sr_dev_driver *di, GSList *options,
		struct sr_dev_inst *(*probe_device)(struct sr_modbus_dev_inst *modbus));
#endif /* MAYNUO_M97_MODBUS_DEFINED */

/* std_session_send_df_frame_begin/end are provided by compat_helpers.c. */

struct maynuo_m97_model {
	unsigned int id;
	const char *name;
	unsigned int max_current;
	unsigned int max_voltage;
	unsigned int max_power;
};

struct dev_context {
	const struct maynuo_m97_model *model;
	struct sr_sw_limits limits;
	GMutex rw_mutex;
};

enum maynuo_m97_coil {
	PC1        = 0x0500,
	PC2        = 0X0501,
	TRIG       = 0x0502,
	REMOTE     = 0x0503,
	ISTATE     = 0x0510,
	TRACK      = 0x0511,
	MEMORY     = 0x0512,
	VOICEEN    = 0x0513,
	CONNECT    = 0x0514,
	ATEST      = 0x0515,
	ATESTUN    = 0x0516,
	ATESTPASS  = 0x0517,
	IOVER      = 0x0520,
	UOVER      = 0x0521,
	POVER      = 0x0522,
	HEAT       = 0x0523,
	REVERSE    = 0x0524,
	UNREG      = 0x0525,
	ERREP      = 0x0526,
	ERRCAL     = 0x0527,
};

enum maynuo_m97_register {
	CMD        = 0x0A00,
	IFIX       = 0x0A01,
	UFIX       = 0x0A03,
	PFIX       = 0x0A05,
	RFIX       = 0x0A07,
	TMCCS      = 0x0A09,
	TMCVS      = 0x0A0B,
	UCCONSET   = 0x0A0D,
	UCCOFFSET  = 0x0A0F,
	UCVONSET   = 0x0A11,
	UCVOFFSET  = 0x0A13,
	UCPONSET   = 0x0A15,
	UCPOFFSET  = 0x0A17,
	UCRONSET   = 0x0A19,
	UCROFFSET  = 0x0A1B,
	UCCCV      = 0x0A1D,
	UCRCV      = 0x0A1F,
	IA         = 0x0A21,
	IB         = 0x0A23,
	TMAWD      = 0x0A25,
	TMBWD      = 0x0A27,
	TMTRANSRIS = 0x0A29,
	TMTRANSFAL = 0x0A2B,
	MODETRAN   = 0x0A2D,
	UBATTEND   = 0x0A2E,
	BATT       = 0x0A30,
	SERLIST    = 0x0A32,
	SERATEST   = 0x0A33,
	IMAX       = 0x0A34,
	UMAX       = 0x0A36,
	PMAX       = 0x0A38,
	ILCAL      = 0x0A3A,
	IHCAL      = 0x0A3C,
	ULCAL      = 0x0A3E,
	UHCAL      = 0x0A40,
	TAGSCAL    = 0x0A42,
	U          = 0x0B00,
	I          = 0x0B02,
	SETMODE    = 0x0B04,
	INPUTMODE  = 0x0B05,
	MODEL      = 0x0B06,
	EDITION    = 0x0B07,
};

enum maynuo_m97_mode {
	CC            =  1,
	CV            =  2,
	CW            =  3,
	CR            =  4,
	CC_SOFT_START = 20,
	DYNAMIC       = 25,
	SHORT_CIRCUIT = 26,
	LIST          = 27,
	CC_L_AND_UL   = 30,
	CV_L_AND_UL   = 31,
	CW_L_AND_UL   = 32,
	CR_L_AND_UL   = 33,
	CC_TO_CV      = 34,
	CR_TO_CV      = 36,
	BATTERY_TEST  = 38,
	CV_SOFT_START = 39,
	SYSTEM_PARAM  = 41,
	INPUT_ON      = 42,
	INPUT_OFF     = 43,
};

SR_PRIV int maynuo_m97_get_bit(const struct sr_dev_inst *sdi,
		enum maynuo_m97_coil address, int *value);
SR_PRIV int maynuo_m97_set_bit(const struct sr_dev_inst *sdi,
		enum maynuo_m97_coil address, int value);
SR_PRIV int maynuo_m97_get_float(const struct sr_dev_inst *sdi,
		enum maynuo_m97_register address, float *value);
SR_PRIV int maynuo_m97_set_float(const struct sr_dev_inst *sdi,
		enum maynuo_m97_register address, float value);

SR_PRIV int maynuo_m97_get_mode(const struct sr_dev_inst *sdi,
		enum maynuo_m97_mode *mode);
SR_PRIV int maynuo_m97_set_mode(const struct sr_dev_inst *sdi,
		enum maynuo_m97_mode mode);
SR_PRIV int maynuo_m97_set_input(const struct sr_dev_inst *sdi, int enable);
SR_PRIV int maynuo_m97_get_model_version(struct sr_modbus_dev_inst *modbus,
		uint16_t *model, uint16_t *version);

SR_PRIV const char *maynuo_m97_mode_to_str(enum maynuo_m97_mode mode);

/*
 * Data reception callback. PXView's sr_receive_data_callback_t expects
 * a 3-argument signature with const sdi (matching itech-it8500, rdtech-dps,
 * manson-hcs-3xxx compat drivers). The original maynuo-m97 driver used
 * (int fd, int revents, void *cb_data) and unwrapped cb_data internally;
 * the PXView version receives sdi directly.
 */
SR_PRIV int maynuo_m97_receive_data(int fd, int revents,
		const struct sr_dev_inst *sdi);

#endif
