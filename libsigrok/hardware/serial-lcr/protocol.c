/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014 Janne Huttunen <jahuttun@gmail.com>
 * Copyright (C) 2019 Gerhard Sittig <gerhard.sittig@gmx.net>
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
 * PXView port of the serial-lcr protocol layer.
 *
 * This file inlines the upstream src/lcr/es51919.c and src/lcr/vc4080.c
 * chipset parsers (PXView does not ship them) and rewrites the
 * data-feed handling to use PXView's flat `struct sr_datafeed_analog`
 * layout. The original sigrok source used sr_analog_init() plus the
 * encoding/meaning/spec sub-structs; PXView has neither, so the analog
 * fields (mq/unit/mqflags/probes/data) are written directly. The
 * digits/spec_digits metadata that lived on encoding/spec is dropped
 * (not representable in the flat layout); the measurement value and
 * its mq/unit/mqflags are still communicated correctly.
 */

#include "hardware/compat/compat.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "protocol.h"

/* ===== Local sr_session_send_meta ======================================
 *
 * PXView's compat layer does not provide sr_session_send_meta(). Sends a
 * META datafeed packet carrying a single config key/value pair. Same
 * pattern as gwinstek-psp / arachnid-labs-re-load-pro. Has external
 * linkage (SR_PRIV) because protocol.h declares it and handle_frame_start
 * calls it; static would still be fine but the header decl uses SR_PRIV.
 */
SR_PRIV int sr_session_send_meta(const struct sr_dev_inst *sdi,
		uint32_t key, GVariant *data)
{
	struct sr_datafeed_packet packet;
	struct sr_datafeed_meta meta;
	struct sr_config *src;

	if (!sdi || !data)
		return SR_ERR_ARG;

	src = sr_config_new((int)key, data);
	if (!src)
		return SR_ERR;

	memset(&packet, 0, sizeof(packet));
	packet.type = SR_DF_META;
	packet.status = SR_PKT_OK;
	packet.payload = &meta;
	meta.config = g_slist_append(NULL, src);

	ds_data_forward(sdi, &packet);

	sr_config_free(src);
	g_slist_free(meta.config);

	return SR_OK;
}

/* ===== Local std_session_send_df_frame_end =============================
 *
 * PXView only provides std_session_send_df_frame_begin() (1-arg, in
 * compat_helpers.c). The frame_end variant is missing, so provide it
 * locally (same approach as atorch / arachnid-labs-re-load-pro).
 */
static int std_session_send_df_frame_end(const struct sr_dev_inst *sdi)
{
	struct sr_datafeed_packet packet;

	memset(&packet, 0, sizeof(packet));
	packet.type = SR_DF_FRAME_END;
	packet.status = SR_PKT_OK;
	packet.payload = NULL;

	return ds_data_forward(sdi, &packet);
}

/* ===== Local sr_atof_ascii (used by the VC4080 parser) =================
 *
 * PXView does not provide sr_atof_ascii(). Locale-independent ASCII
 * float parse, same pattern as agilent-dmm / korad-kaxxxxp / scpi-pps.
 */
static int serial_lcr_sr_atof_ascii(const char *str, float *ret)
{
	char *endptr;
	double val;

	if (!str || !ret)
		return SR_ERR_ARG;

	errno = 0;
	val = g_ascii_strtod(str, &endptr);
	if (errno != 0 || endptr == str)
		return SR_ERR;

	*ret = (float)val;
	return SR_OK;
}

/* =========================================================================
 * Cyrustek ES51919 LCR chipset parser (inlined from src/lcr/es51919.c)
 * =========================================================================
 */

static const double es51919_frequencies[] = {
	SR_HZ(0), SR_HZ(100), SR_HZ(120),
	SR_KHZ(1), SR_KHZ(10), SR_KHZ(100),
};

static const size_t es51919_freq_code_map[] = {
	1, 2, 3, 4, 5, 0,
};

static uint64_t es51919_get_frequency(size_t code)
{
	uint64_t freq;

	if (code >= ARRAY_SIZE(es51919_freq_code_map)) {
		sr_err("Unknown output frequency code %zu.", code);
		return es51919_frequencies[0];
	}

	code = es51919_freq_code_map[code];
	freq = es51919_frequencies[code];

	return freq;
}

enum { ES51919_MODEL_NONE, ES51919_MODEL_PAR, ES51919_MODEL_SER, ES51919_MODEL_AUTO, };

static const char *const es51919_circuit_models[] = {
	"NONE", "PARALLEL", "SERIES", "AUTO",
};

static const char *es51919_get_equiv_model(size_t code)
{
	if (code >= ARRAY_SIZE(es51919_circuit_models)) {
		sr_err("Unknown equivalent circuit model code %zu.", code);
		return "NONE";
	}

	return es51919_circuit_models[code];
}

static const uint8_t *es51919_pkt_to_buf(const uint8_t *pkt, int is_secondary)
{
	return is_secondary ? pkt + 10 : pkt + 5;
}

static int es51919_parse_mq(const uint8_t *pkt, int is_secondary, int is_parallel)
{
	const uint8_t *buf;

	buf = es51919_pkt_to_buf(pkt, is_secondary);

	switch (is_secondary << 8 | buf[0]) {
	case 0x001:
		return is_parallel ?
			SR_MQ_PARALLEL_INDUCTANCE : SR_MQ_SERIES_INDUCTANCE;
	case 0x002:
		return is_parallel ?
			SR_MQ_PARALLEL_CAPACITANCE : SR_MQ_SERIES_CAPACITANCE;
	case 0x003:
	case 0x103:
		return is_parallel ?
			SR_MQ_PARALLEL_RESISTANCE : SR_MQ_SERIES_RESISTANCE;
	case 0x004:
		return SR_MQ_RESISTANCE;
	case 0x100:
		return SR_MQ_DIFFERENCE;
	case 0x101:
		return SR_MQ_DISSIPATION_FACTOR;
	case 0x102:
		return SR_MQ_QUALITY_FACTOR;
	case 0x104:
		return SR_MQ_PHASE_ANGLE;
	}

	sr_err("Unknown quantity 0x%03x.", is_secondary << 8 | buf[0]);

	return 0;
}

static float es51919_parse_value(const uint8_t *buf, int *digits)
{
	static const int exponents[] = {0, -1, -2, -3, -4, -5, -6, -7};

	int exponent;
	int16_t val;
	float fval;

	exponent = exponents[buf[3] & 7];
	*digits = -exponent;
	val = (buf[1] << 8) | buf[2];
	fval = (float)val;
	fval *= powf(10, exponent);

	return fval;
}

/*
 * Adapted to PXView's flat sr_datafeed_analog: writes analog->mq /
 * analog->unit / analog->mqflags directly. The encoding->digits and
 * spec->spec_digits assignments from the original are dropped (no
 * corresponding flat fields). The value scale/precision still flows
 * through *floatval.
 */
static void es51919_parse_measurement(const uint8_t *pkt, float *floatval,
	struct sr_datafeed_analog *analog, int is_secondary)
{
	static const struct {
		int unit;
		int exponent;
	} units[] = {
		{ SR_UNIT_UNITLESS,   0 }, /* no unit */
		{ SR_UNIT_OHM,        0 }, /* Ohm */
		{ SR_UNIT_OHM,        3 }, /* kOhm */
		{ SR_UNIT_OHM,        6 }, /* MOhm */
		{ -1,                 0 }, /* ??? */
		{ SR_UNIT_HENRY,     -6 }, /* uH */
		{ SR_UNIT_HENRY,     -3 }, /* mH */
		{ SR_UNIT_HENRY,      0 }, /* H */
		{ SR_UNIT_HENRY,      3 }, /* kH */
		{ SR_UNIT_FARAD,    -12 }, /* pF */
		{ SR_UNIT_FARAD,     -9 }, /* nF */
		{ SR_UNIT_FARAD,     -6 }, /* uF */
		{ SR_UNIT_FARAD,     -3 }, /* mF */
		{ SR_UNIT_PERCENTAGE, 0 }, /* % */
		{ SR_UNIT_DEGREE,     0 }, /* degree */
	};

	const uint8_t *buf;
	int digits, exponent;
	int state;

	buf = es51919_pkt_to_buf(pkt, is_secondary);

	analog->mq = 0;
	analog->mqflags = 0;

	state = buf[4] & 0xf;

	if (state != 0 && state != 3)
		return;

	if (pkt[2] & 0x18) {
		/* Calibration and Sorting modes not supported. */
		return;
	}

	if (!is_secondary) {
		if (pkt[2] & 0x01)
			analog->mqflags |= SR_MQFLAG_HOLD;
		if (pkt[2] & 0x02)
			analog->mqflags |= SR_MQFLAG_REFERENCE;
	} else {
		if (pkt[2] & 0x04)
			analog->mqflags |= SR_MQFLAG_RELATIVE;
	}

	if ((analog->mq = es51919_parse_mq(pkt, is_secondary, pkt[2] & 0x80)) == 0)
		return;

	if ((buf[3] >> 3) >= ARRAY_SIZE(units)) {
		sr_err("Unknown unit %u.", buf[3] >> 3);
		analog->mq = 0;
		return;
	}

	analog->unit = units[buf[3] >> 3].unit;

	exponent = units[buf[3] >> 3].exponent;
	*floatval = es51919_parse_value(buf, &digits);
	*floatval *= (state == 0) ? powf(10, exponent) : INFINITY;
}

static uint64_t es51919_parse_freq(const uint8_t *pkt)
{
	return es51919_get_frequency(pkt[3] >> 5);
}

static const char *es51919_parse_model(const uint8_t *pkt)
{
	size_t code;

	if (pkt[2] & 0x40)
		code = ES51919_MODEL_AUTO;
	else if (es51919_parse_mq(pkt, 0, 0) == SR_MQ_RESISTANCE)
		code = ES51919_MODEL_NONE;
	else
		code = (pkt[2] & 0x80) ? ES51919_MODEL_PAR : ES51919_MODEL_SER;

	return es51919_get_equiv_model(code);
}

SR_PRIV gboolean es51919_packet_valid(const uint8_t *pkt)
{
	/* Check for fixed 0x00 0x0d prefix. */
	if (pkt[0] != 0x00 || pkt[1] != 0x0d)
		return FALSE;

	/* Check for fixed 0x0d 0x0a suffix. */
	if (pkt[15] != 0x0d || pkt[16] != 0x0a)
		return FALSE;

	/* Packet appears to be valid. */
	return TRUE;
}

SR_PRIV int es51919_packet_parse(const uint8_t *pkt, float *val,
		struct sr_datafeed_analog *analog, void *info)
{
	struct lcr_parse_info *parse_info;

	parse_info = info;
	if (!parse_info->ch_idx) {
		parse_info->output_freq = es51919_parse_freq(pkt);
		parse_info->circuit_model = es51919_parse_model(pkt);
	}
	if (val && analog)
		es51919_parse_measurement(pkt, val, analog, parse_info->ch_idx == 1);

	return SR_OK;
}

SR_PRIV int es51919_config_list(uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	(void)sdi;
	(void)cg;

	switch (key) {
	case SR_CONF_OUTPUT_FREQUENCY:
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_DOUBLE,
			ARRAY_AND_SIZE(es51919_frequencies), sizeof(es51919_frequencies[0]));
		return SR_OK;
	case SR_CONF_EQUIV_CIRCUIT_MODEL:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(es51919_circuit_models));
		return SR_OK;
	default:
		return SR_ERR_NA;
	}
	/* UNREACH */
}

/* =========================================================================
 * Voltcraft 4080 LCR chipset parser (inlined from src/lcr/vc4080.c)
 * =========================================================================
 */

static const double vc4080_frequencies[] = {
	SR_HZ(120), SR_KHZ(1),
};

static uint64_t vc4080_get_frequency(char code)
{
	switch (code) {
	case 'A': return SR_KHZ(1);
	case 'B': return SR_HZ(120);
	default: return 0;
	}
}

enum vc4080_equiv_model { VC4080_MODEL_PAR, VC4080_MODEL_SER, VC4080_MODEL_NONE, };

static const char *const vc4080_circuit_models[] = {
	"PARALLEL", "SERIES", "NONE",
};

static enum vc4080_equiv_model vc4080_get_equiv_model(char lcr_code, char model_code)
{
	switch (lcr_code) {
	case 'L': /* EMPTY */ break;
	case 'C': /* EMPTY */ break;
	case 'R': return VC4080_MODEL_NONE;
	default: return VC4080_MODEL_NONE;
	}
	switch (model_code) {
	case 'P': return VC4080_MODEL_PAR;
	case 'S': return VC4080_MODEL_SER;
	default: return VC4080_MODEL_NONE;
	}
}

static const char *vc4080_get_equiv_model_text(enum vc4080_equiv_model model)
{
	return vc4080_circuit_models[model];
}

static uint64_t vc4080_parse_freq(const uint8_t *pkt)
{
	return vc4080_get_frequency(pkt[2]);
}

static const char *vc4080_parse_model(const uint8_t *pkt)
{
	return vc4080_get_equiv_model_text(vc4080_get_equiv_model(pkt[0], pkt[3]));
}

static float vc4080_parse_number(const uint8_t *digits, size_t length)
{
	char value_text[8];
	float number;
	int ret;

	memcpy(value_text, digits, length);
	value_text[length] = '\0';
	ret = serial_lcr_sr_atof_ascii(value_text, &number);

	return (ret == SR_OK) ? number : 0;
}

enum vc4080_lcr_kind { VC4080_LCR_NONE, VC4080_LCR_IS_L, VC4080_LCR_IS_C, VC4080_LCR_IS_R, };
enum vc4080_dqr_kind { VC4080_DQR_NONE, VC4080_DQR_IS_D, VC4080_DQR_IS_Q, VC4080_DQR_IS_R, };

static int vc4080_get_main_scale_rs(int *digits, int *rs,
	uint8_t range, enum vc4080_lcr_kind lcr, uint64_t freq)
{
	static const int dig_r[] = { -3, -2, -1, +0, +1, +2, +3, };
	static const int dig_l_1k[] = { -7, -6, -5, -4, -3, -2, -1, };
	static const int dig_l_120[] = { -6, -5, -4, -3, -2, -1, 0, };
	static const int dig_c_1k[] = { -13, -12, -11, -10, -9, -8, -7, };
	static const int dig_c_120[] = { -12, -11, -10, -9, -8, -7, -6, };
	static const int rs_r_l[] = {
		100, 100, 100, 1000, 10000, 100000, 100000,
	};
	static const int rs_c[] = {
		100000, 100000, 10000, 1000, 100, 100, 100,
	};

	const int *digits_table, *rs_table;

	if (range > 6)
		return SR_ERR_DATA;

	if (lcr == VC4080_LCR_IS_R) {
		digits_table = dig_r;
		rs_table = rs_r_l;
	} else if (lcr == VC4080_LCR_IS_L && freq == SR_KHZ(1)) {
		digits_table = dig_l_1k;
		rs_table = rs_r_l;
	} else if (lcr == VC4080_LCR_IS_L && freq == SR_HZ(120)) {
		digits_table = dig_l_120;
		rs_table = rs_r_l;
	} else if (lcr == VC4080_LCR_IS_C && freq == SR_KHZ(1)) {
		digits_table = dig_c_1k;
		rs_table = rs_c;
	} else if (lcr == VC4080_LCR_IS_C && freq == SR_HZ(120)) {
		digits_table = dig_c_120;
		rs_table = rs_c;
	} else {
		return SR_ERR_DATA;
	}

	if (digits)
		*digits = digits_table[range];
	if (rs)
		*rs = rs_table[range];

	return SR_OK;
}

static int vc4080_get_sec_scale(int *digits, uint8_t range, enum vc4080_dqr_kind dqr, int rs)
{
	static const int dig_d_q[] = { 0, -1, -2, -3, -4, 0, };
	static const int dig_r_100[] = { 0, -2, -1, +0, +1, 0, };
	static const int dig_r_1k_10k[] = { 0, -2, -1, +0, +1, +2, };
	static const int dig_r_100k[] = { 0, 0, -1, +0, +1, +2, };

	const int *digits_table;

	if (range < 1 || range > 5)
		return SR_ERR_DATA;

	if (dqr == VC4080_DQR_IS_D || dqr == VC4080_DQR_IS_Q) {
		if (range > 4)
			return SR_ERR_DATA;
		digits_table = dig_d_q;
	} else if (dqr == VC4080_DQR_IS_R && rs == 100) {
		if (range > 4)
			return SR_ERR_DATA;
		digits_table = dig_r_100;
	} else if (dqr == VC4080_DQR_IS_R && (rs == 1000 || rs == 10000)) {
		digits_table = dig_r_1k_10k;
	} else if (dqr == VC4080_DQR_IS_R && rs == 100000) {
		if (range < 2)
			return SR_ERR_DATA;
		digits_table = dig_r_100k;
	} else {
		return SR_ERR_DATA;
	}

	if (digits)
		*digits = digits_table[range];

	return SR_OK;
}

/*
 * Adapted to PXView's flat sr_datafeed_analog. The original assigned to
 * analog->meaning->{mq,mqflags,unit} and analog->encoding->digits /
 * analog->spec->spec_digits; the flat layout stores mq/unit/mqflags
 * directly and has no digits field, so the digits/spec_digits writes
 * are dropped. The value scaling still applies through *floatval.
 */
static void vc4080_parse_measurement(const uint8_t *pkt, float *floatval,
	struct sr_datafeed_analog *analog, size_t disp_idx)
{
	enum vc4080_lcr_kind lcr;
	enum vc4080_dqr_kind dqr;
	uint64_t freq;
	enum vc4080_equiv_model model;
	gboolean is_auto, main_ranging, main_ol, sec_ol, d_ol, q_ol;
	float main_value, sec_value, d_value, q_value;
	char main_range, sec_range, d_range, q_range;
	gboolean is_hold, is_relative, has_adapter, is_lowbatt;
	enum minmax_kind {
		MINMAX_MAX, MINMAX_MIN, MINMAX_SPAN,
		MINMAX_AVG, MINMAX_CURR, MINMAX_NONE,
	} minmax;
	gboolean is_parallel;
	int mq, mqflags, unit;
	float value;
	int digits, exponent;
	gboolean ol, invalid;
	int ret, rs, main_digits, sec_digits, d_digits, q_digits;
	int main_invalid, sec_invalid, d_invalid, q_invalid;

	/* Prepare void return values for error paths. */
	analog->mq = 0;
	analog->mqflags = 0;
	if (disp_idx >= VC4080_CHANNEL_COUNT)
		return;

	switch (pkt[0]) {
	case 'L': lcr = VC4080_LCR_IS_L; break;
	case 'R': lcr = VC4080_LCR_IS_R; break;
	case 'C': lcr = VC4080_LCR_IS_C; break;
	default: return;
	}
	switch (pkt[1]) {
	case 'D': dqr = VC4080_DQR_IS_D; break;
	case 'Q': dqr = VC4080_DQR_IS_Q; break;
	case 'R': dqr = VC4080_DQR_IS_R; break;
	case '_': dqr = VC4080_DQR_NONE; break;
	default: return;
	}
	freq = vc4080_get_frequency(pkt[2]);
	model = vc4080_get_equiv_model(pkt[0], pkt[3]);
	is_auto = pkt[4] == 'A';
	main_ranging = pkt[5] == '8';
	if (main_ranging)
		return;
	main_ol = pkt[5] == '9';
	main_value = vc4080_parse_number(&pkt[5], 5);
	main_range = pkt[10];
	if (main_range < '0' || main_range > '6')
		main_range = '9';
	main_range -= '0';
	sec_ol = 0 && pkt[11] == '9';
	sec_value = vc4080_parse_number(&pkt[11], 4);
	sec_range = pkt[15];
	if (sec_range < '0' || sec_range > '6')
		sec_range = '9';
	sec_range -= '0';
	d_ol = pkt[17] == '9';
	d_value = vc4080_parse_number(&pkt[17], 4);
	d_range = pkt[21];
	if (d_range < '0' || d_range > '6')
		d_range = '9';
	d_range -= '0';
	q_ol = pkt[22] == '9';
	q_value = vc4080_parse_number(&pkt[22], 4);
	q_range = pkt[26];
	if (q_range < '0' || q_range > '6')
		q_range = '9';
	d_range -= '0';
	switch (pkt[27]) {
	case 'S': return;
	case '_': /* EMPTY */ break;
	default: return;
	}
	is_hold = pkt[29] == 'H';
	switch (pkt[30]) {
	case 'R': minmax = MINMAX_CURR; break;
	case 'M': minmax = MINMAX_MAX; break;
	case 'I': minmax = MINMAX_MIN; break;
	case 'X': minmax = MINMAX_SPAN; break;
	case 'A': minmax = MINMAX_AVG; break;
	case '_': minmax = MINMAX_NONE; break;
	default: return;
	}
	if (minmax == MINMAX_SPAN)
		return;
	if (minmax == MINMAX_CURR)
		minmax = MINMAX_NONE;
	switch (pkt[31]) {
	case 'R': is_relative = TRUE; break;
	case 'S': return;
	case '_': is_relative = FALSE; break;
	default: return;
	}
	if (pkt[32] != '_')
		return;
	if (pkt[33] != '_')
		return;
	has_adapter = pkt[35] == 'A';
	is_lowbatt = pkt[36] == 'B';

	rs = main_digits = sec_digits = d_digits = q_digits = 0;
	main_invalid = sec_invalid = d_invalid = q_invalid = 0;
	ret = vc4080_get_main_scale_rs(&main_digits, &rs, main_range, lcr, freq);
	if (ret != SR_OK)
		main_invalid = 1;
	ret = vc4080_get_sec_scale(&sec_digits, sec_range, dqr, rs);
	if (ret != SR_OK)
		sec_invalid = 1;
	ret = vc4080_get_sec_scale(&d_digits, d_range, dqr, rs);
	if (ret != SR_OK)
		d_invalid = 1;
	ret = vc4080_get_sec_scale(&q_digits, q_range, dqr, rs);
	if (ret != SR_OK)
		q_invalid = 1;

	is_parallel = model == VC4080_MODEL_PAR;
	mq = 0;
	mqflags = 0;
	unit = 0;
	switch (disp_idx) {
	case VC4080_DISPLAY_PRIMARY:
		invalid = main_invalid;
		if (invalid)
			break;
		if (lcr == VC4080_LCR_IS_L) {
			mq = is_parallel
				? SR_MQ_PARALLEL_INDUCTANCE
				: SR_MQ_SERIES_INDUCTANCE;
			unit = SR_UNIT_HENRY;
		} else if (lcr == VC4080_LCR_IS_C) {
			mq = is_parallel
				? SR_MQ_PARALLEL_CAPACITANCE
				: SR_MQ_SERIES_CAPACITANCE;
			unit = SR_UNIT_FARAD;
		} else if (lcr == VC4080_LCR_IS_R) {
			mq = is_parallel
				? SR_MQ_PARALLEL_RESISTANCE
				: SR_MQ_SERIES_RESISTANCE;
			unit = SR_UNIT_OHM;
		}
		value = main_value;
		ol = main_ol;
		digits = 0;
		exponent = main_digits;
		break;
	case VC4080_DISPLAY_SECONDARY:
		invalid = sec_invalid;
		if (invalid)
			break;
		if (dqr == VC4080_DQR_IS_D) {
			mq = SR_MQ_DISSIPATION_FACTOR;
			unit = SR_UNIT_UNITLESS;
		} else if (dqr == VC4080_DQR_IS_Q) {
			mq = SR_MQ_QUALITY_FACTOR;
			unit = SR_UNIT_UNITLESS;
		} else if (dqr == VC4080_DQR_IS_R) {
			mq = SR_MQ_RESISTANCE;
			unit = SR_UNIT_OHM;
		}
		value = sec_value;
		ol = sec_ol;
		digits = 0;
		exponent = sec_digits;
		break;
#if VC4080_WITH_DQ_CHANS
	case VC4080_DISPLAY_D_VALUE:
		invalid = d_invalid;
		if (invalid)
			break;
		mq = SR_MQ_DISSIPATION_FACTOR;
		unit = SR_UNIT_UNITLESS;
		value = d_value;
		ol = d_ol;
		digits = 4;
		exponent = d_digits;
		break;
	case VC4080_DISPLAY_Q_VALUE:
		invalid = q_invalid;
		if (invalid)
			break;
		mq = SR_MQ_QUALITY_FACTOR;
		unit = SR_UNIT_UNITLESS;
		value = q_value;
		ol = q_ol;
		digits = 4;
		exponent = q_digits;
		break;
#else
	(void)d_invalid;
	(void)d_value;
	(void)d_ol;
	(void)d_digits;
	(void)q_invalid;
	(void)q_value;
	(void)q_ol;
	(void)q_digits;
#endif
	default:
		return;
	}
	if (invalid)
		return;
	if (is_auto)
		mqflags |= SR_MQFLAG_AUTORANGE;
	if (is_hold)
		mqflags |= SR_MQFLAG_HOLD;
	if (is_relative)
		mqflags |= SR_MQFLAG_RELATIVE;
	if (has_adapter)
		mqflags |= SR_MQFLAG_FOUR_WIRE;
	switch (minmax) {
	case MINMAX_MAX:
		mqflags |= SR_MQFLAG_MAX;
		break;
	case MINMAX_MIN:
		mqflags |= SR_MQFLAG_MIN;
		break;
	case MINMAX_SPAN:
		mqflags |= SR_MQFLAG_MAX | SR_MQFLAG_RELATIVE;
		break;
	case MINMAX_AVG:
		mqflags |= SR_MQFLAG_AVG;
		break;
	case MINMAX_CURR:
	case MINMAX_NONE:
	default:
		/* EMPTY */
		break;
	}

	/* "Commit" the resulting value. */
	if (ol) {
		value = INFINITY;
	} else {
		value *= powf(10, exponent);
		digits -= exponent;
	}
	*floatval = value;
	analog->mq = mq;
	analog->mqflags = (uint64_t)mqflags;
	analog->unit = unit;

	/* Low battery is rather severe, the measurement could be invalid. */
	if (is_lowbatt)
		sr_warn("Low battery.");
}

/*
 * Workaround for cables' improper(?) parity handling. Strips the high
 * (parity) bit off every byte. Idempotent, so it works during stream
 * detect as well as in the acquisition loop.
 */
static void vc4080_strip_parity_bit(uint8_t *p, size_t l)
{
	while (l--)
		*p++ &= ~0x80;
}

SR_PRIV const char *vc4080_channel_formats[VC4080_CHANNEL_COUNT] = {
	"P1", "P2",
#if VC4080_WITH_DQ_CHANS
	"D", "Q",
#endif
};

SR_PRIV int vc4080_packet_request(struct sr_serial_dev_inst *serial)
{
	static const char *command = "N";

	serial_write_blocking(serial, command, strlen(command), 0);

	return SR_OK;
}

SR_PRIV gboolean vc4080_packet_valid(const uint8_t *pkt)
{
	vc4080_strip_parity_bit((uint8_t *)pkt, VC4080_PACKET_SIZE);

	if (pkt[37] != '\r' || pkt[38] != '\n')
		return FALSE;

	return TRUE;
}

SR_PRIV int vc4080_packet_parse(const uint8_t *pkt, float *val,
		struct sr_datafeed_analog *analog, void *info)
{
	struct lcr_parse_info *parse_info;

	vc4080_strip_parity_bit((uint8_t *)pkt, VC4080_PACKET_SIZE);

	parse_info = info;
	if (!parse_info->ch_idx) {
		parse_info->output_freq = vc4080_parse_freq(pkt);
		parse_info->circuit_model = vc4080_parse_model(pkt);
	}
	if (val && analog)
		vc4080_parse_measurement(pkt, val, analog, parse_info->ch_idx);

	return SR_OK;
}

SR_PRIV int vc4080_config_list(uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	(void)sdi;
	(void)cg;

	switch (key) {
	case SR_CONF_OUTPUT_FREQUENCY:
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_DOUBLE,
			ARRAY_AND_SIZE(vc4080_frequencies), sizeof(vc4080_frequencies[0]));
		return SR_OK;
	case SR_CONF_EQUIV_CIRCUIT_MODEL:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(vc4080_circuit_models));
		return SR_OK;
	default:
		return SR_ERR_NA;
	}
}

/* =========================================================================
 * serial-lcr data feed handling
 * =========================================================================
 */

static void send_frame_start(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct lcr_parse_info *info;
	uint64_t freq;
	const char *model;

	devc = sdi->priv;
	info = &devc->parse_info;

	/* Communicate changes of frequency or model before data values. */
	freq = info->output_freq;
	if (freq != devc->output_freq) {
		devc->output_freq = freq;
		sr_session_send_meta(sdi, SR_CONF_OUTPUT_FREQUENCY,
			g_variant_new_double(freq));
	}
	model = info->circuit_model;
	if (model && model != devc->circuit_model) {
		devc->circuit_model = model;
		sr_session_send_meta(sdi, SR_CONF_EQUIV_CIRCUIT_MODEL,
			g_variant_new_string(model));
	}

	/* Data is about to get sent. Start a new frame. */
	std_session_send_df_frame_begin(sdi);
}

/*
 * Adapted to PXView's flat sr_datafeed_analog. The original used
 * sr_analog_init() + encoding/meaning/spec and assigned
 * analog.meaning->channels per channel; here we use analog.probes and
 * the direct mq/unit/mqflags fields. unit_bits is set to 32 (sizeof float)
 * matching the gwinstek-gpd / hp-3457a compat pattern.
 */
static int handle_packet(struct sr_dev_inst *sdi, const uint8_t *pkt)
{
	struct dev_context *devc;
	struct lcr_parse_info *info;
	const struct lcr_info *lcr;
	size_t ch_idx;
	int rc;
	float value;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	gboolean frame;
	struct sr_channel *channel;

	devc = sdi->priv;
	info = &devc->parse_info;
	lcr = devc->lcr_info;

	memset(&analog, 0, sizeof(analog));
	analog.num_samples = 1;
	analog.data = &value;
	analog.unit_bits = 32; /* sizeof(float) */

	frame = FALSE;
	for (ch_idx = 0; ch_idx < lcr->channel_count; ch_idx++) {
		channel = g_slist_nth_data(sdi->channels, ch_idx);
		analog.probes = g_slist_append(NULL, channel);
		info->ch_idx = ch_idx;
		rc = lcr->packet_parse(pkt, &value, &analog, info);
		if (sdi->session && rc == SR_OK && analog.mq && channel && channel->enabled) {
			if (!frame) {
				send_frame_start(sdi);
				frame = TRUE;
			}
			packet.type = SR_DF_ANALOG;
			packet.payload = &analog;
			sr_session_send(sdi, &packet);
		}
		g_slist_free(analog.probes);
		analog.probes = NULL;
	}
	if (frame) {
		std_session_send_df_frame_end(sdi);
		sr_sw_limits_update_frames_read(&devc->limits, 1);
	}

	return SR_OK;
}

static int handle_new_data(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
	ssize_t rdsize;
	const struct lcr_info *lcr;
	uint8_t *pkt;
	size_t copy_len;

	devc = sdi->priv;
	serial = sdi->conn;

	/* Read another chunk of data into the buffer. */
	rdsize = sizeof(devc->buf) - devc->buf_rxpos;
	rdsize = serial_read_nonblocking(serial, &devc->buf[devc->buf_rxpos], rdsize);
	if (rdsize < 0)
		return SR_ERR_IO;
	devc->buf_rxpos += rdsize;

	/*
	 * Process as many packets as the buffer might contain. Assume
	 * that the stream is synchronized in the typical case. Re-sync
	 * in case of mismatch (skip individual bytes until data matches
	 * the expected packet layout again).
	 */
	lcr = devc->lcr_info;
	while (devc->buf_rxpos >= lcr->packet_size) {
		pkt = &devc->buf[0];
		if (!lcr->packet_valid(pkt)) {
			copy_len = devc->buf_rxpos - 1;
			memmove(&devc->buf[0], &devc->buf[1], copy_len);
			devc->buf_rxpos--;
			continue;
		}
		(void)handle_packet(sdi, pkt);
		copy_len = devc->buf_rxpos - lcr->packet_size;
		memmove(&devc->buf[0], &devc->buf[lcr->packet_size], copy_len);
		devc->buf_rxpos -= lcr->packet_size;
	}

	return SR_OK;
}

static int handle_timeout(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	const struct lcr_info *lcr;
	struct sr_serial_dev_inst *serial;
	int64_t now;
	int ret;

	devc = sdi->priv;
	lcr = devc->lcr_info;

	if (!lcr->packet_request)
		return SR_OK;

	now = g_get_monotonic_time();
	if (devc->req_next_at && now < devc->req_next_at)
		return SR_OK;

	serial = sdi->conn;
	ret = lcr->packet_request(serial);
	if (ret < 0) {
		sr_err("Failed to request packet: %d.", ret);
		return ret;
	}

	if (lcr->req_timeout_ms)
		devc->req_next_at = now + lcr->req_timeout_ms * 1000;

	return SR_OK;
}

SR_PRIV int lcr_receive_data(int fd, int revents, const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	int ret;

	(void)fd;

	if (!sdi)
		return TRUE;
	if (!(devc = sdi->priv))
		return TRUE;

	if (revents == G_IO_IN)
		ret = handle_new_data(sdi);
	else
		ret = handle_timeout(sdi);
	if (sr_sw_limits_check(&devc->limits))
		sr_dev_acquisition_stop(sdi);
	if (ret != SR_OK)
		return FALSE;

	return TRUE;
}

/*
 * Local acquisition stop. PXView does not provide
 * std_serial_dev_acquisition_stop. Remove the serial source, close the
 * port, send the DF_END packet. Same pattern as colead-slm / gwinstek-gpd.
 * PXView's serial_source_remove() is 1-arg (serial only).
 */
SR_PRIV int serial_lcr_dev_acquisition_stop(const struct sr_dev_inst *sdi)
{
	struct sr_serial_dev_inst *serial;

	if (!sdi)
		return SR_ERR_ARG;

	serial = sdi->conn;
	if (serial) {
		serial_source_remove(serial);
		serial_close(serial);
	}

	return std_session_send_df_end(sdi, LOG_PREFIX);
}
