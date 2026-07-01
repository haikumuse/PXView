/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
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

/*
 * Local replacement for the DMM parser declarations that standard
 * libsigrok exposes in libsigrok-internal.h. PXView's libsigrok does
 * not ship these declarations (they were never part of its public or
 * internal API surface). Each serial-dmm parser file (bm25x.c, bm52x.c,
 * bm85x.c, bm86x.c, etc.) includes this header instead.
 *
 * NOTE: PXView's sr_datafeed_analog is the old flat layout (probes, mq,
 * unit, mqflags, data, num_samples, unit_bits, unit_pitch). There is no
 * sr_analog_init() helper and no sr_analog_meaning/encoding/spec sub-
 * struct pointers. Parser files converted from standard sigrok must use
 * the flat layout: assign analog->mq, analog->unit, analog->mqflags,
 * analog->data, etc. directly.
 */

#ifndef LIBSIGROK_HARDWARE_SERIAL_DMM_DMM_PARSERS_H
#define LIBSIGROK_HARDWARE_SERIAL_DMM_DMM_PARSERS_H

/*
 * Bring in PXView's libsigrok types (struct sr_datafeed_analog,
 * SR_PRIV, gboolean, struct sr_dev_inst, sr_receive_data_callback_t,
 * struct sr_serial_dev_inst, struct sr_channel_group, GVariant, etc.)
 * and the compat layer helpers (std_init, std_session_send_df_header,
 * serial_source_add, sr_dev_acquisition_stop, SR_CONF_* aliases, etc).
 */
#include "hardware/compat/compat.h"

/*--- dmm/es519xx.c ---------------------------------------------------------*/

/**
 * All 11-byte es519xx chips repeat each block twice for each conversion cycle
 * so always read 2 blocks at a time.
 */
#define ES519XX_11B_PACKET_SIZE (11 * 2)
#define ES519XX_14B_PACKET_SIZE 14

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

SR_PRIV gboolean sr_es519xx_2400_11b_packet_valid(const uint8_t *buf);
SR_PRIV int sr_es519xx_2400_11b_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);
SR_PRIV gboolean sr_es519xx_2400_11b_altfn_packet_valid(const uint8_t *buf);
SR_PRIV int sr_es519xx_2400_11b_altfn_parse(const uint8_t *buf,
		float *floatval, struct sr_datafeed_analog *analog, void *info);
SR_PRIV gboolean sr_es519xx_19200_11b_5digits_packet_valid(const uint8_t *buf);
SR_PRIV int sr_es519xx_19200_11b_5digits_parse(const uint8_t *buf,
		float *floatval, struct sr_datafeed_analog *analog, void *info);
SR_PRIV gboolean sr_es519xx_19200_11b_clamp_packet_valid(const uint8_t *buf);
SR_PRIV int sr_es519xx_19200_11b_clamp_parse(const uint8_t *buf,
		float *floatval, struct sr_datafeed_analog *analog, void *info);
SR_PRIV gboolean sr_es519xx_19200_11b_packet_valid(const uint8_t *buf);
SR_PRIV int sr_es519xx_19200_11b_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);
SR_PRIV gboolean sr_es519xx_19200_14b_packet_valid(const uint8_t *buf);
SR_PRIV int sr_es519xx_19200_14b_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);
SR_PRIV gboolean sr_es519xx_19200_14b_sel_lpf_packet_valid(const uint8_t *buf);
SR_PRIV int sr_es519xx_19200_14b_sel_lpf_parse(const uint8_t *buf,
		float *floatval, struct sr_datafeed_analog *analog, void *info);

/*--- dmm/fs9922.c ----------------------------------------------------------*/

#define FS9922_PACKET_SIZE 14

struct fs9922_info {
	gboolean is_auto, is_dc, is_ac, is_rel, is_hold, is_bpn, is_z1, is_z2;
	gboolean is_max, is_min, is_apo, is_bat, is_nano, is_z3, is_micro;
	gboolean is_milli, is_kilo, is_mega, is_beep, is_diode, is_percent;
	gboolean is_z4, is_volt, is_ampere, is_ohm, is_hfe, is_hertz, is_farad;
	gboolean is_celsius, is_fahrenheit;
	int bargraph_sign, bargraph_value;
};

SR_PRIV gboolean sr_fs9922_packet_valid(const uint8_t *buf);
SR_PRIV int sr_fs9922_parse(const uint8_t *buf, float *floatval,
			    struct sr_datafeed_analog *analog, void *info);
SR_PRIV void sr_fs9922_z1_diode(struct sr_datafeed_analog *analog, void *info);

/*--- dmm/fs9721.c ----------------------------------------------------------*/

#define FS9721_PACKET_SIZE 14

struct fs9721_info {
	gboolean is_ac, is_dc, is_auto, is_rs232, is_micro, is_nano, is_kilo;
	gboolean is_diode, is_milli, is_percent, is_mega, is_beep, is_farad;
	gboolean is_ohm, is_rel, is_hold, is_ampere, is_volt, is_hz, is_bat;
	gboolean is_c2c1_11, is_c2c1_10, is_c2c1_01, is_c2c1_00, is_sign;
};

SR_PRIV gboolean sr_fs9721_packet_valid(const uint8_t *buf);
SR_PRIV int sr_fs9721_parse(const uint8_t *buf, float *floatval,
			    struct sr_datafeed_analog *analog, void *info);
SR_PRIV void sr_fs9721_00_temp_c(struct sr_datafeed_analog *analog, void *info);
SR_PRIV void sr_fs9721_01_temp_c(struct sr_datafeed_analog *analog, void *info);
SR_PRIV void sr_fs9721_10_temp_c(struct sr_datafeed_analog *analog, void *info);
SR_PRIV void sr_fs9721_01_10_temp_f_c(struct sr_datafeed_analog *analog, void *info);
SR_PRIV void sr_fs9721_max_c_min(struct sr_datafeed_analog *analog, void *info);

/*--- dmm/mm38xr.c ---------------------------------------------------------*/

#define METERMAN_38XR_PACKET_SIZE 15

struct meterman_38xr_info { int dummy; };

SR_PRIV gboolean meterman_38xr_packet_valid(const uint8_t *buf);
SR_PRIV int meterman_38xr_parse(const uint8_t *buf, float *floatval,
	struct sr_datafeed_analog *analog, void *info);

/*--- dmm/ms2115b.c ---------------------------------------------------------*/

#define MS2115B_PACKET_SIZE 9

enum ms2115b_display {
	MS2115B_DISPLAY_MAIN,
	MS2115B_DISPLAY_SUB,
	MS2115B_DISPLAY_COUNT,
};

struct ms2115b_info {
	/* Selected channel. */
	size_t ch_idx;
	gboolean is_ac, is_dc, is_auto;
	gboolean is_diode, is_beep, is_farad;
	gboolean is_ohm, is_ampere, is_volt, is_hz;
	gboolean is_duty_cycle, is_percent;
};

extern SR_PRIV const char *ms2115b_channel_formats[];
SR_PRIV gboolean sr_ms2115b_packet_valid(const uint8_t *buf);
SR_PRIV int sr_ms2115b_parse(const uint8_t *buf, float *floatval,
	struct sr_datafeed_analog *analog, void *info);

/*--- dmm/ms8250d.c ---------------------------------------------------------*/

#define MS8250D_PACKET_SIZE 18

struct ms8250d_info {
	gboolean is_ac, is_dc, is_auto, is_rs232, is_micro, is_nano, is_kilo;
	gboolean is_diode, is_milli, is_percent, is_mega, is_beep, is_farad;
	gboolean is_ohm, is_rel, is_hold, is_ampere, is_volt, is_hz, is_bat;
	gboolean is_ncv, is_min, is_max, is_sign, is_autotimer;
};

SR_PRIV gboolean sr_ms8250d_packet_valid(const uint8_t *buf);
SR_PRIV int sr_ms8250d_parse(const uint8_t *buf, float *floatval,
			     struct sr_datafeed_analog *analog, void *info);

/*--- dmm/dtm0660.c ---------------------------------------------------------*/

#define DTM0660_PACKET_SIZE 15

struct dtm0660_info {
	gboolean is_ac, is_dc, is_auto, is_rs232, is_micro, is_nano, is_kilo;
	gboolean is_diode, is_milli, is_percent, is_mega, is_beep, is_farad;
	gboolean is_ohm, is_rel, is_hold, is_ampere, is_volt, is_hz, is_bat;
	gboolean is_degf, is_degc, is_c2c1_01, is_c2c1_00, is_apo, is_min;
	gboolean is_minmax, is_max, is_sign;
};

SR_PRIV gboolean sr_dtm0660_packet_valid(const uint8_t *buf);
SR_PRIV int sr_dtm0660_parse(const uint8_t *buf, float *floatval,
			struct sr_datafeed_analog *analog, void *info);

/*--- dmm/m2110.c -----------------------------------------------------------*/

#define BBCGM_M2110_PACKET_SIZE 9

/* Dummy info struct. The parser does not use it. */
struct m2110_info { int dummy; };

SR_PRIV gboolean sr_m2110_packet_valid(const uint8_t *buf);
SR_PRIV int sr_m2110_parse(const uint8_t *buf, float *floatval,
			     struct sr_datafeed_analog *analog, void *info);

/*--- dmm/metex14.c ---------------------------------------------------------*/

#define METEX14_PACKET_SIZE 14

struct metex14_info {
	size_t ch_idx;
	gboolean is_ac, is_dc, is_resistance, is_capacity, is_temperature;
	gboolean is_diode, is_frequency, is_ampere, is_volt, is_farad;
	gboolean is_hertz, is_ohm, is_celsius, is_fahrenheit, is_watt;
	gboolean is_pico, is_nano, is_micro, is_milli, is_kilo, is_mega;
	gboolean is_gain, is_decibel, is_power, is_decibel_mw, is_power_factor;
	gboolean is_hfe, is_unitless, is_logic, is_min, is_max, is_avg;
};

#ifdef HAVE_SERIAL_COMM
SR_PRIV int sr_metex14_packet_request(struct sr_serial_dev_inst *serial);
#endif
SR_PRIV gboolean sr_metex14_packet_valid(const uint8_t *buf);
SR_PRIV int sr_metex14_parse(const uint8_t *buf, float *floatval,
			     struct sr_datafeed_analog *analog, void *info);
SR_PRIV gboolean sr_metex14_4packets_valid(const uint8_t *buf);
SR_PRIV int sr_metex14_4packets_parse(const uint8_t *buf, float *floatval,
			     struct sr_datafeed_analog *analog, void *info);

/*--- dmm/rs9lcd.c ----------------------------------------------------------*/

#define RS9LCD_PACKET_SIZE 9

/* Dummy info struct. The parser does not use it. */
struct rs9lcd_info { int dummy; };

SR_PRIV gboolean sr_rs9lcd_packet_valid(const uint8_t *buf);
SR_PRIV int sr_rs9lcd_parse(const uint8_t *buf, float *floatval,
			    struct sr_datafeed_analog *analog, void *info);

/*--- dmm/qm1578.c -----------------------------------------------------------*/

#define DIGITECH_QM1578_PACKET_SIZE 15

/* Dummy info struct. The parser does not use it. */
struct qm1578_info { int dummy; };

SR_PRIV gboolean sr_digitech_qm1578_packet_valid(const uint8_t *buf);
SR_PRIV int sr_digitech_qm1578_parse(const uint8_t *buf, float *floatval,
			     struct sr_datafeed_analog *analog, void *info);

/*--- dmm/bm25x.c -----------------------------------------------------------*/

#define BRYMEN_BM25X_PACKET_SIZE 15

/* Dummy info struct. The parser does not use it. */
struct bm25x_info { int dummy; };

SR_PRIV gboolean sr_brymen_bm25x_packet_valid(const uint8_t *buf);
SR_PRIV int sr_brymen_bm25x_parse(const uint8_t *buf, float *floatval,
			     struct sr_datafeed_analog *analog, void *info);

/*--- dmm/bm52x.c -----------------------------------------------------------*/

#define BRYMEN_BM52X_PACKET_SIZE 24
#define BRYMEN_BM52X_DISPLAY_COUNT 2

struct brymen_bm52x_info { size_t ch_idx; };

#ifdef HAVE_SERIAL_COMM
SR_PRIV int sr_brymen_bm52x_packet_request(struct sr_serial_dev_inst *serial);
SR_PRIV int sr_brymen_bm82x_packet_request(struct sr_serial_dev_inst *serial);
#endif
SR_PRIV gboolean sr_brymen_bm52x_packet_valid(const uint8_t *buf);
SR_PRIV gboolean sr_brymen_bm82x_packet_valid(const uint8_t *buf);
/* BM520s and BM820s protocols are similar, the parse routine is shared. */
SR_PRIV int sr_brymen_bm52x_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);

struct brymen_bm52x_state;

SR_PRIV void *brymen_bm52x_state_init(void);
SR_PRIV void brymen_bm52x_state_free(void *state);
SR_PRIV int brymen_bm52x_config_get(void *state, uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg);
SR_PRIV int brymen_bm52x_config_set(void *state, uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg);
SR_PRIV int brymen_bm52x_config_list(void *state, uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg);
/*
 * PXView's sr_receive_data_callback_t passes the sdi directly as the
 * third argument (const struct sr_dev_inst *sdi) instead of void
 * *cb_data. The cb_data parameter was removed from acquire_start.
 */
SR_PRIV int brymen_bm52x_acquire_start(void *state,
	const struct sr_dev_inst *sdi,
	sr_receive_data_callback_t *cb);

/*--- dmm/bm85x.c -----------------------------------------------------------*/

#define BRYMEN_BM85x_PACKET_SIZE_MIN 4

struct brymen_bm85x_info { int dummy; };

#ifdef HAVE_SERIAL_COMM
SR_PRIV int brymen_bm85x_after_open(struct sr_serial_dev_inst *serial);
SR_PRIV int brymen_bm85x_packet_request(struct sr_serial_dev_inst *serial);
#endif
SR_PRIV gboolean brymen_bm85x_packet_valid(void *state,
	const uint8_t *buf, size_t len, size_t *pkt_len);
SR_PRIV int brymen_bm85x_parse(void *state, const uint8_t *buf, size_t len,
	double *floatval, struct sr_datafeed_analog *analog, void *info);

/*--- dmm/bm86x.c -----------------------------------------------------------*/

#define BRYMEN_BM86X_PACKET_SIZE 24
#define BRYMEN_BM86X_DISPLAY_COUNT 2

struct brymen_bm86x_info { size_t ch_idx; };

#ifdef HAVE_SERIAL_COMM
SR_PRIV int sr_brymen_bm86x_packet_request(struct sr_serial_dev_inst *serial);
#endif
SR_PRIV gboolean sr_brymen_bm86x_packet_valid(const uint8_t *buf);
SR_PRIV int sr_brymen_bm86x_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);

/*--- dmm/ut71x.c -----------------------------------------------------------*/

#define UT71X_PACKET_SIZE 11

struct ut71x_info {
	gboolean is_voltage, is_resistance, is_capacitance, is_temperature;
	gboolean is_celsius, is_fahrenheit, is_current, is_continuity;
	gboolean is_diode, is_frequency, is_duty_cycle, is_dc, is_ac;
	gboolean is_auto, is_manual, is_sign, is_power, is_loop_current;
};

SR_PRIV gboolean sr_ut71x_packet_valid(const uint8_t *buf);
SR_PRIV int sr_ut71x_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);

/*--- dmm/vc870.c -----------------------------------------------------------*/

#define VC870_PACKET_SIZE 23

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

SR_PRIV gboolean sr_vc870_packet_valid(const uint8_t *buf);
SR_PRIV int sr_vc870_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);

/*--- dmm/vc96.c ------------------------------------------------------------*/

#define VC96_PACKET_SIZE 13

struct vc96_info {
	size_t ch_idx;
	gboolean is_ac, is_dc, is_resistance, is_diode, is_ampere, is_volt;
	gboolean is_ohm, is_micro, is_milli, is_kilo, is_mega, is_hfe;
	gboolean is_unitless;
};

SR_PRIV gboolean sr_vc96_packet_valid(const uint8_t *buf);
SR_PRIV int sr_vc96_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);

/*--- dmm/asycii.c ----------------------------------------------------------*/

#define ASYCII_PACKET_SIZE 16

struct asycii_info {
	gboolean is_ac, is_dc, is_ac_and_dc;
	gboolean is_resistance, is_capacitance, is_diode, is_gain;
	gboolean is_frequency, is_duty_cycle, is_duty_pos, is_duty_neg;
	gboolean is_pulse_width, is_period_pos, is_period_neg;
	gboolean is_pulse_count, is_count_pos, is_count_neg;
	gboolean is_ampere, is_volt, is_volt_ampere, is_farad, is_ohm;
	gboolean is_hertz, is_percent, is_seconds, is_decibel;
	gboolean is_pico, is_nano, is_micro, is_milli, is_kilo, is_mega;
	gboolean is_unitless;
	gboolean is_peak_min, is_peak_max;
	gboolean is_invalid;
};

#ifdef HAVE_SERIAL_COMM
SR_PRIV int sr_asycii_packet_request(struct sr_serial_dev_inst *serial);
#endif
SR_PRIV gboolean sr_asycii_packet_valid(const uint8_t *buf);
SR_PRIV int sr_asycii_parse(const uint8_t *buf, float *floatval,
			    struct sr_datafeed_analog *analog, void *info);

/*--- dmm/eev121gw.c --------------------------------------------------------*/

#define EEV121GW_PACKET_SIZE 19

enum eev121gw_display {
	EEV121GW_DISPLAY_MAIN,
	EEV121GW_DISPLAY_SUB,
	EEV121GW_DISPLAY_BAR,
	EEV121GW_DISPLAY_COUNT,
};

struct eev121gw_info {
	/* Selected channel. */
	size_t ch_idx;
	/*
	 * Measured value, number and sign/overflow flags, scale factor
	 * and significant digits.
	 */
	uint32_t uint_value;
	gboolean is_ofl, is_neg;
	int factor, digits;
	/* Currently active mode (meter's function). */
	gboolean is_ac, is_dc, is_voltage, is_current, is_power, is_gain;
	gboolean is_resistance, is_capacitance, is_diode, is_temperature;
	gboolean is_continuity, is_frequency, is_period, is_duty_cycle;
	/* Quantities associated with mode/function. */
	gboolean is_ampere, is_volt, is_volt_ampere, is_dbm;
	gboolean is_ohm, is_farad, is_celsius, is_fahrenheit;
	gboolean is_hertz, is_seconds, is_percent, is_loop_current;
	gboolean is_unitless, is_logic;
	/* Other indicators. */
	gboolean is_min, is_max, is_avg, is_1ms_peak, is_rel, is_hold;
	gboolean is_low_pass, is_mem, is_bt, is_auto_range, is_test;
	gboolean is_auto_poweroff, is_low_batt;
};

extern SR_PRIV const char *eev121gw_channel_formats[];
SR_PRIV gboolean sr_eev121gw_packet_valid(const uint8_t *buf);
SR_PRIV int sr_eev121gw_3displays_parse(const uint8_t *buf, float *floatval,
		struct sr_datafeed_analog *analog, void *info);

#endif /* LIBSIGROK_HARDWARE_SERIAL_DMM_DMM_PARSERS_H */
