/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2012 Alexandru Gagniuc <mr.nuke.me@gmail.com>
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

#ifndef LIBSIGROK_HARDWARE_SERIAL_DMM_PROTOCOL_H
#define LIBSIGROK_HARDWARE_SERIAL_DMM_PROTOCOL_H

#include "hardware/compat/compat.h"
#include <string.h>

#define LOG_PREFIX "serial-dmm"

/*
 * Standard sigrok packet-status codes used by packet_valid_len() callbacks.
 * PXView's libsigrok.h does not define them, so declare them locally for
 * the variable-length-packet path (only used by the brymen-bm85x variant
 * which is currently disabled because its parser has not been migrated).
 */
#ifndef SR_PACKET_NEED_RX
#define SR_PACKET_NEED_RX  0
#endif
#ifndef SR_PACKET_INVALID
#define SR_PACKET_INVALID  (-1)
#endif

/*
 * ===========================================================================
 * Local sr_sw_limits helpers.
 *
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_*() helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time.
 * ===========================================================================
 */
#ifndef SERIAL_DMM_SR_SW_LIMITS_DEFINED
#define SERIAL_DMM_SR_SW_LIMITS_DEFINED
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
#endif /* SERIAL_DMM_SR_SW_LIMITS_DEFINED */

/*
 * ===========================================================================
 * DMM chipset info structs and parser declarations.
 *
 * In upstream libsigrok these live in libsigrok-internal.h alongside the
 * dmm/<chipset>.c parser sources. PXView's libsigrok-internal.h does not
 * carry the dmm parser headers, so the subset referenced by this driver is
 * declared here. All declarations are guarded so they cannot clash with
 * another compat driver (e.g. uni-t-dmm) that happens to declare the same
 * types in its own protocol.h - those are separate translation units.
 * ===========================================================================
 */
#ifndef DMM_PARSER_ES519XX_DEFINED
#define DMM_PARSER_ES519XX_DEFINED
struct es519xx_info {
	gboolean is_judge, is_voltage, is_auto, is_micro, is_current;
	gboolean is_milli, is_resistance, is_continuity, is_diode;
	gboolean is_frequency, is_rpm, is_capacitance, is_duty_cycle;
	gboolean is_temperature, is_celsius, is_fahrenheit;
	gboolean is_adp0, is_adp1, is_adp2, is_adp3;
	gboolean is_sign, is_batt, is_ol, is_pmax, is_pmin, is_apo;
	gboolean is_dc, is_ac, is_vahz, is_min, is_max, is_rel, is_hold;
	gboolean is_digit4, is_ul, is_vasel, is_vbar, is_lpf1, is_lpf0, is_rmr;
	uint32_t baudrate;
	int packet_size;
	gboolean alt_functions, fivedigits, clampmeter, selectable_lpf;
	int digits;
};

#ifndef ES519XX_11B_PACKET_SIZE
#define ES519XX_11B_PACKET_SIZE (11 * 2)
#endif
#ifndef ES519XX_14B_PACKET_SIZE
#define ES519XX_14B_PACKET_SIZE 14
#endif

SR_PRIV gboolean sr_es519xx_2400_11b_packet_valid(const uint8_t *buf);
SR_PRIV int sr_es519xx_2400_11b_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);
SR_PRIV gboolean sr_es519xx_2400_11b_altfn_packet_valid(const uint8_t *buf);
SR_PRIV int sr_es519xx_2400_11b_altfn_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);
SR_PRIV gboolean sr_es519xx_19200_11b_5digits_packet_valid(const uint8_t *buf);
SR_PRIV int sr_es519xx_19200_11b_5digits_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);
SR_PRIV gboolean sr_es519xx_19200_11b_clamp_packet_valid(const uint8_t *buf);
SR_PRIV int sr_es519xx_19200_11b_clamp_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);
SR_PRIV gboolean sr_es519xx_19200_11b_packet_valid(const uint8_t *buf);
SR_PRIV int sr_es519xx_19200_11b_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);
SR_PRIV gboolean sr_es519xx_19200_14b_packet_valid(const uint8_t *buf);
SR_PRIV int sr_es519xx_19200_14b_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);
SR_PRIV gboolean sr_es519xx_19200_14b_sel_lpf_packet_valid(const uint8_t *buf);
SR_PRIV int sr_es519xx_19200_14b_sel_lpf_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);
#endif /* DMM_PARSER_ES519XX_DEFINED */

#ifndef DMM_PARSER_FS9721_DEFINED
#define DMM_PARSER_FS9721_DEFINED
struct fs9721_info {
	gboolean is_ac, is_dc, is_auto, is_rs232, is_micro, is_nano, is_kilo;
	gboolean is_diode, is_milli, is_percent, is_mega, is_beep, is_farad;
	gboolean is_ohm, is_rel, is_hold, is_ampere, is_volt, is_hz, is_bat;
	gboolean is_c2c1_11, is_c2c1_10, is_c2c1_01, is_c2c1_00, is_sign;
};

#ifndef FS9721_PACKET_SIZE
#define FS9721_PACKET_SIZE 14
#endif

SR_PRIV gboolean sr_fs9721_packet_valid(const uint8_t *buf);
SR_PRIV int sr_fs9721_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);
SR_PRIV void sr_fs9721_00_temp_c(struct sr_datafeed_analog *analog, void *info);
SR_PRIV void sr_fs9721_01_temp_c(struct sr_datafeed_analog *analog, void *info);
SR_PRIV void sr_fs9721_10_temp_c(struct sr_datafeed_analog *analog, void *info);
SR_PRIV void sr_fs9721_01_10_temp_f_c(struct sr_datafeed_analog *analog, void *info);
SR_PRIV void sr_fs9721_max_c_min(struct sr_datafeed_analog *analog, void *info);
#endif /* DMM_PARSER_FS9721_DEFINED */

#ifndef DMM_PARSER_FS9922_DEFINED
#define DMM_PARSER_FS9922_DEFINED
struct fs9922_info {
	gboolean is_auto, is_dc, is_ac, is_rel, is_hold, is_bpn, is_z1, is_z2;
	gboolean is_max, is_min, is_apo, is_bat, is_nano, is_z3, is_micro;
	gboolean is_milli, is_kilo, is_mega, is_beep, is_diode, is_percent;
	gboolean is_z4, is_volt, is_ampere, is_ohm, is_hfe, is_hertz, is_farad;
	gboolean is_celsius, is_fahrenheit;
	int bargraph_sign, bargraph_value;
};

#ifndef FS9922_PACKET_SIZE
#define FS9922_PACKET_SIZE 14
#endif

SR_PRIV gboolean sr_fs9922_packet_valid(const uint8_t *buf);
SR_PRIV int sr_fs9922_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);
SR_PRIV void sr_fs9922_z1_diode(struct sr_datafeed_analog *analog, void *info);
#endif /* DMM_PARSER_FS9922_DEFINED */

#ifndef DMM_PARSER_UT372_DEFINED
#define DMM_PARSER_UT372_DEFINED
struct ut372_info {
	int dummy;
};

#ifndef UT372_PACKET_SIZE
#define UT372_PACKET_SIZE 27
#endif

SR_PRIV gboolean sr_ut372_packet_valid(const uint8_t *buf);
SR_PRIV int sr_ut372_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);
#endif /* DMM_PARSER_UT372_DEFINED */

#ifndef DMM_PARSER_UT71X_DEFINED
#define DMM_PARSER_UT71X_DEFINED
struct ut71x_info {
	gboolean is_voltage, is_resistance, is_capacitance, is_temperature;
	gboolean is_celsius, is_fahrenheit, is_current, is_continuity;
	gboolean is_diode, is_frequency, is_duty_cycle, is_dc, is_ac;
	gboolean is_auto, is_manual, is_sign, is_power, is_loop_current;
};

#ifndef UT71X_PACKET_SIZE
#define UT71X_PACKET_SIZE 11
#endif

SR_PRIV gboolean sr_ut71x_packet_valid(const uint8_t *buf);
SR_PRIV int sr_ut71x_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);
#endif /* DMM_PARSER_UT71X_DEFINED */

#ifndef DMM_PARSER_VC870_DEFINED
#define DMM_PARSER_VC870_DEFINED
struct vc870_info {
	gboolean is_voltage, is_dc, is_ac, is_temperature, is_resistance;
	gboolean is_continuity, is_capacitance, is_diode, is_loop_current;
	gboolean is_current, is_micro, is_milli, is_power;
	gboolean is_power_factor_freq, is_power_apparent_power, is_v_a_rms_value;
	gboolean is_sign2, is_sign1, is_batt, is_ol1, is_max, is_min;
	gboolean is_maxmin, is_rel, is_ol2, is_open, is_manu, is_hold;
	gboolean is_light, is_usb, is_warning, is_auto_power, is_misplug_warn;
	gboolean is_lo, is_hi, is_open2;

	gboolean is_frequency, is_dual_display, is_auto;
};

#ifndef VC870_PACKET_SIZE
#define VC870_PACKET_SIZE 23
#endif

SR_PRIV gboolean sr_vc870_packet_valid(const uint8_t *buf);
SR_PRIV int sr_vc870_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);
#endif /* DMM_PARSER_VC870_DEFINED */

/*
 * ===========================================================================
 * Driver-specific data structures.
 * ===========================================================================
 */

/*
 * Variant descriptor for one serial-DMM model.
 *
 * Upstream serial-dmm embeds `struct sr_dev_driver di` as the first member
 * and registers one driver per variant via the DMM() / DMM_CONN() / DMM_LEN()
 * macros. PXView requires a non-static struct sr_dev_driver per variant with
 * the additional driver_type / dev_mode_list / dev_destroy / dev_status_get
 * fields, so the variant table here is plain data and the per-variant
 * driver_info structs are generated by the SERIAL_DMM_DRV() macro in api.c.
 */
struct dmm_info {
	/** Manufacturer/brand. */
	const char *vendor;
	/** Model. */
	const char *device;
	/** conn string. */
	const char *conn;
	/** serialcomm string. */
	const char *serialcomm;
	/** Packet size in bytes. */
	size_t packet_size;
	/**
	 * Request timeout [ms] before request is considered lost and a new
	 * one is sent. Used only if device needs polling.
	 */
	uint64_t req_timeout_ms;
	/**
	 * Delay between reception of packet and next request. Some DMMs
	 * need this. Used only if device needs polling.
	 */
	uint64_t req_delay_ms;
	/** Packet request function. */
	int (*packet_request)(struct sr_serial_dev_inst *);
	/** Number of channels / displays. */
	size_t channel_count;
	/** (Optional) printf formats for channel names. */
	const char **channel_formats;
	/** Packet validation function. */
	gboolean (*packet_valid)(const uint8_t *);
	/** Packet parsing function. */
	int (*packet_parse)(const uint8_t *, float *,
			    struct sr_datafeed_analog *, void *);
	/** */
	void (*dmm_details)(struct sr_datafeed_analog *, void *);
	/** Size of chipset info struct. */
	gsize info_size;
	/* Serial-dmm items "with state" and variable length packets. */
	void *dmm_state;
	void *(*dmm_state_init)(void);
	void (*dmm_state_free)(void *state);
	int (*after_open)(struct sr_serial_dev_inst *serial);
	int (*packet_valid_len)(void *state, const uint8_t *data, size_t dlen,
		size_t *pkt_len);
	int (*packet_parse_len)(void *state, const uint8_t *data, size_t dlen,
		double *val, struct sr_datafeed_analog *analog, void *info);
	int (*config_get)(void *state, uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg);
	int (*config_set)(void *state, uint32_t key, GVariant *data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg);
	int (*config_list)(void *state, uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg);
	/** Hook at acquisition start. Can re-route the receive routine. */
	int (*acquire_start)(void *state, const struct sr_dev_inst *sdi,
		sr_receive_data_callback_t *cb, void **cb_data);
};

#define DMM_BUFSIZE 256

struct dev_context {
	struct sr_sw_limits limits;

	uint8_t buf[DMM_BUFSIZE];
	size_t buflen;

	/**
	 * The timestamp [µs] to send the next request.
	 * Used only if device needs polling.
	 */
	uint64_t req_next_at;

	/** Active variant descriptor (set by scan() via the driver index). */
	const struct dmm_info *dmm;
};

/*
 * PXView's sr_receive_data_callback_t passes the sdi directly as the third
 * argument (const struct sr_dev_inst *sdi) instead of the void *cb_data
 * that standard sigrok uses.
 */
SR_PRIV int serial_dmm_receive_data(int fd, int revents,
		const struct sr_dev_inst *sdi);

#endif
