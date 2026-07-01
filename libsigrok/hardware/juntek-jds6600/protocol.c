/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2023 Gerhard Sittig <gerhard.sittig@gmx.net>
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
 * Juntek JDS6600 is a DDS signal generator.
 * Often rebranded, goes by different names, among them Joy-IT JDS6600.
 *
 * This driver was built using Kristoff Bonne's knowledge as seen in his
 * MIT licensed Python code for JDS6600 control. For details see the
 * https://github.com/on1arf/jds6600_python repository.
 *
 * Supported features:
 * - Model detection, which determines the upper output frequency limit
 *   (15..60MHz models exist).
 * - Assumes exactly two channels. Other models were not seen out there.
 * - Per channel configuration of: Waveform, output frequency, amplitude,
 *   offset, duty cycle.
 * - Phase between channels is a global property and affects multiple
 *   channels at the same time (their relation to each other).
 *
 * Communication is over a USB CDC virtual COM port (115200/8n1). The driver
 * sends text-format requests (":rNN=0." / ":wNN=value.") and parses text
 * responses (":rNN=value." / ":ok"). It is a pure control device -- no
 * sample data is streamed back, so acquisition start/stop are local no-ops.
 *
 * PXView migration notes:
 * - Includes replaced by "hardware/compat/compat.h" (Rule 1).
 * - serial_write_blocking / serial_read_blocking are provided by
 *   compat_serial.c and called directly (driver-specific point 6).
 * - sr_hexdump_new / sr_hexdump_free are provided by compat_helpers.c.
 * - sr_text_trim_spaces / sr_text_next_word / sr_atoul_base / sr_atod are
 *   NOT provided by PXView's libsigrok; local implementations are provided
 *   below (same pattern as motech-lps-30x local sr_atoi / sr_atod_ascii).
 * - std_dummy_dev_acquisition_start / stop are local no-ops since PXView's
 *   compat layer does not provide them (Rule 14 / point 1).
 * - clear_helper + jds6600_dev_clear follow the atorch local pattern since
 *   PXView does not provide std_dev_clear_with_callback (point 2).
 */

#include "hardware/compat/compat.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "protocol.h"

#define WITH_SERIAL_RAW_DUMP	0 /* Includes EOL and non-printables. */
#define WITH_ARBWAVE_DOWNLOAD	0 /* Development HACK */

/*
 * The firmware's maximum response length. Seen when an arbitrary
 * waveform gets retrieved. Carries 2048 samples in the 0..4095 range.
 * Plus some decoration around that data.
 *   :b01=4095,4095,...,4095,<CRLF>
 */
#define MAX_RSP_LENGTH	(8 + 2048 * 5)

/*
 * Times are in milliseconds.
 * - Delay after transmission was an option during initial development.
 *   Has become obsolete. Support remains because it doesn't harm.
 * - Delay after flash is essential when writing multiple waveforms to
 *   the device. Not letting more idle time pass after successful write
 *   and reception of the "ok" response, and before the next write, will
 *   result in corrupted waveform storage in the device. The next wave
 *   that is written waveform will start with several hundred samples
 *   of all-one bits.
 * - Timeout per receive attempt at the physical layer can be short.
 *   Experience suggests that 2ms are a good value. Reception ends when
 *   the response termination was seen. Or when no receive data became
 *   available within that per-attemt timeout, and no higher level total
 *   timeout was specified. Allow some slack for USB FS frame intervals.
 * - Timeout for identify attempts at the logical level can be short.
 *   Captures of the microcontroller communication suggest that firmware
 *   responds immediately (within 2ms). So 10ms per identify attempt
 *   are plenty for successful communication, yet quick enough to not
 *   stall on missing peripherals.
 * - Timeout for waveform upload/download needs to be huge. Textual
 *   presentation of 2k samples with 12 significant bits (0..4095 range)
 *   combined with 115200bps UART communication result in a 1s maximum
 *   transfer time per waveform. So 1.2s is a good value.
 */
#define DELAY_AFTER_SEND	0
#define DELAY_AFTER_FLASH	100
#define TIMEOUT_READ_CHUNK	2
#define TIMEOUT_IDENTIFY	10
#define TIMEOUT_WAVEFORM	1200

/* Instruction codes. Read/write parameters/waveforms. */
#define INSN_WRITE_PARA	'w'
#define INSN_READ_PARA	'r'
#define INSN_WRITE_WAVE	'a'
#define INSN_READ_WAVE	'b'

/* Indices for "register access". */
enum param_index {
	IDX_DEVICE_TYPE = 0,
	IDX_SERIAL_NUMBER = 1,
	IDX_CHANNELS_ENABLE = 20,
	IDX_WAVEFORM_CH1 = 21,
	IDX_WAVEFORM_CH2 = 22,
	IDX_FREQUENCY_CH1 = 23,
	IDX_FREQUENCY_CH2 = 24,
	IDX_AMPLITUDE_CH1 = 25,
	IDX_AMPLITUDE_CH2 = 26,
	IDX_OFFSET_CH1 = 27,
	IDX_OFFSET_CH2 = 28,
	IDX_DUTYCYCLE_CH1 = 29,
	IDX_DUTYCYCLE_CH2 = 30,
	IDX_PHASE_CHANNELS = 31,
	IDX_ACTION = 32,
	IDX_MODE = 33,
	IDX_INPUT_COUPLING = 36,
	IDX_MEASURE_GATE = 37,
	IDX_MEASURE_MODE = 38,
	IDX_COUNTER_RESET = 39,
	IDX_SWEEP_STARTFREQ = 40,
	IDX_SWEEP_ENDFREQ = 41,
	IDX_SWEEP_TIME = 42,
	IDX_SWEEP_DIRECTION = 43,
	IDX_SWEEP_MODE = 44,
	IDX_PULSE_WIDTH = 45,
	IDX_PULSE_PERIOD = 46,
	IDX_PULSE_OFFSET = 47,
	IDX_PULSE_AMPLITUDE = 48,
	IDX_BURST_COUNT = 49,
	IDX_BURST_MODE = 50,
	IDX_SYSTEM_SOUND = 51,
	IDX_SYSTEM_BRIGHTNESS = 52,
	IDX_SYSTEM_LANGUAGE = 53,
	IDX_SYSTEM_SYNC = 54, /* "Tracking" channels? */
	IDX_SYSTEM_ARBMAX = 55,
	IDX_PROFILE_SAVE = 70,
	IDX_PROFILE_LOAD = 71,
	IDX_PROFILE_CLEAR = 72,
	IDX_COUNTER_VALUE = 80,
	IDX_MEAS_VALUE_FREQLOW = 81,
	IDX_MEAS_VALUE_FREQHI = 82,
	IDX_MEAS_VALUE_WIDTHHI = 83,
	IDX_MEAS_VALUE_WIDTHLOW = 84,
	IDX_MEAS_VALUE_PERIOD = 85,
	IDX_MEAS_VALUE_DUTYCYCLE = 86,
	IDX_MEAS_VALUE_U1 = 87,
	IDX_MEAS_VALUE_U2 = 88,
	IDX_MEAS_VALUE_U3 = 89,
};

/* Firmware's codes for waveform selection. */
enum waveform_index_t {
	/* 17 pre-defined waveforms. */
	WAVE_SINE = 0,
	WAVE_SQUARE = 1,
	WAVE_PULSE = 2,
	WAVE_TRIANGLE = 3,
	WAVE_PARTIAL_SINE = 4,
	WAVE_CMOS = 5,
	WAVE_DC = 6,
	WAVE_HALF_WAVE = 7,
	WAVE_FULL_WAVE = 8,
	WAVE_POS_LADDER = 9,
	WAVE_NEG_LADDER = 10,
	WAVE_NOISE = 11,
	WAVE_EXP_RISE = 12,
	WAVE_EXP_DECAY = 13,
	WAVE_MULTI_TONE = 14,
	WAVE_SINC = 15,
	WAVE_LORENZ = 16,
	WAVES_COUNT_BUILTIN,
	/* Up to 60 arbitrary waveforms. */
	WAVES_ARB_BASE = 100,
	WAVE_ARB01 = WAVES_ARB_BASE +  1,
	/* ... */
	WAVE_ARB60 = WAVES_ARB_BASE + 60,
	WAVES_PAST_LAST_ARB,
};
#define WAVES_COUNT_ARBITRARY	(WAVES_PAST_LAST_ARB - WAVE_ARB01)

static const char *waveform_names[] = {
	[WAVE_SINE] = "sine",
	[WAVE_SQUARE] = "square",
	[WAVE_PULSE] = "pulse",
	[WAVE_TRIANGLE] = "triangle",
	[WAVE_PARTIAL_SINE] = "partial-sine",
	[WAVE_CMOS] = "cmos",
	[WAVE_DC] = "dc",
	[WAVE_HALF_WAVE] = "half-wave",
	[WAVE_FULL_WAVE] = "full-wave",
	[WAVE_POS_LADDER] = "pos-ladder",
	[WAVE_NEG_LADDER] = "neg-ladder",
	[WAVE_NOISE] = "noise",
	[WAVE_EXP_RISE] = "exp-rise",
	[WAVE_EXP_DECAY] = "exp-decay",
	[WAVE_MULTI_TONE] = "multi-tone",
	[WAVE_SINC] = "sinc",
	[WAVE_LORENZ] = "lorenz",
};
#define WAVEFORM_ARB_NAME_FMT	"arb-%02zu"

/*
 * ===========================================================================
 * Local text/numeric parsing helpers.
 *
 * PXView's libsigrok does not provide sr_text_trim_spaces(),
 * sr_text_next_word(), sr_atoul_base(), or sr_atod(). These are required by
 * the JDS6600 protocol parser to interpret text responses. Local static
 * implementations are provided here, matching the standard sigrok semantics.
 * Same pattern as motech-lps-30x's local sr_atoi / sr_atod_ascii helpers.
 * ===========================================================================
 */

/*
 * Trim surrounding whitespace from a NUL-terminated string. Returns a
 * pointer into the buffer (in-place modification: shifts left, NUL-trims
 * trailing spaces). Matches standard sigrok's sr_text_trim_spaces().
 */
static char *sr_text_trim_spaces(char *s)
{
	char *start, *end;

	if (!s)
		return NULL;

	/* Skip leading whitespace. */
	start = s;
	while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
		start++;

	if (!*start) {
		/* All whitespace. */
		s[0] = '\0';
		return s;
	}

	/* Trim trailing whitespace. */
	end = start + strlen(start);
	while (end > start) {
		end--;
		if (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')
			*end = '\0';
		else
			break;
	}

	/* Shift to the beginning of the original buffer. */
	if (start != s)
		memmove(s, start, strlen(start) + 1);

	return s;
}

/*
 * Extract the next whitespace-separated word from *p. Advances *p past the
 * word (and any trailing whitespace). Returns a pointer to a NUL-terminated
 * copy of the word (caller must g_free()), or NULL when no more words remain.
 * Matches standard sigrok's sr_text_next_word().
 */
static char *sr_text_next_word(const char *s, char **endp)
{
	const char *start, *next;
	char *word;
	size_t len;

	if (!s || !endp)
		return NULL;

	*endp = (char *)s;

	/* Skip leading whitespace. */
	start = s;
	while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
		start++;

	if (!*start) {
		*endp = (char *)start;
		return NULL;
	}

	/* Find end of the word. */
	next = start;
	while (*next && *next != ' ' && *next != '\t' &&
	       *next != '\r' && *next != '\n')
		next++;

	len = (size_t)(next - start);
	word = g_malloc(len + 1);
	memcpy(word, start, len);
	word[len] = '\0';

	/* Skip trailing whitespace so the next call sees the next word. */
	while (*next == ' ' || *next == '\t' || *next == '\r' || *next == '\n')
		next++;
	*endp = (char *)next;

	return word;
}

/*
 * Parse an unsigned long from a string with an explicit base. Optionally
 * returns the end pointer. Returns SR_OK on success, SR_ERR on parse
 * failure (no digits consumed). Matches standard sigrok's sr_atoul_base().
 */
static int sr_atoul_base(const char *str, unsigned long *val, char **end, int base)
{
	char *endptr;
	unsigned long result;

	if (!str)
		return SR_ERR_ARG;

	errno = 0;
	result = strtoul(str, &endptr, base);
	if (endptr == str)
		return SR_ERR;
	if (errno != 0)
		return SR_ERR;

	if (val)
		*val = result;
	if (end)
		*end = endptr;
	return SR_OK;
}

/*
 * Parse a double from a string in a locale-independent way. Returns SR_OK
 * on success, SR_ERR on parse failure. Matches standard sigrok's sr_atod().
 * Uses g_ascii_strtod() so the decimal separator is always '.' regardless
 * of the current locale.
 */
static int sr_atod(const char *str, double *val)
{
	char *endptr;
	double result;

	if (!str || !val)
		return SR_ERR_ARG;

	errno = 0;
	result = g_ascii_strtod(str, &endptr);
	if (endptr == str)
		return SR_ERR;
	if (errno != 0)
		return SR_ERR;

	*val = result;
	return SR_OK;
}

/*
 * ===========================================================================
 * Serial raw dump helper (debug only).
 * ===========================================================================
 */

static void log_raw_bytes(const char *caption, GString *buff)
{
	GString *text;

	if (!WITH_SERIAL_RAW_DUMP)
		return;
	if (sr_log_loglevel_get() < SR_LOG_SPEW)
		return;

	if (!caption)
		caption = "";
	text = sr_hexdump_new((const uint8_t *)buff->str, buff->len);
	sr_spew("%s%s", caption, text->str);
	sr_hexdump_free(text);
}

/*
 * ===========================================================================
 * Serial text-line send / receive.
 * ===========================================================================
 */

/*
 * Writes a text line to the serial port. Normalizes end-of-line
 * including trailing period.
 *
 * Accepts:
 *   ":r01=0.<CR><LF>"
 *   ":r01=0."
 *   ":r01=0<LF>"
 *   ":r01=0"
 * Normalizes to:
 *   ":r01=0.<CR><LF>"
 */
static int serial_send_textline(const struct sr_dev_inst *sdi,
	GString *s, unsigned int delay_ms)
{
	struct sr_serial_dev_inst *conn;
	const char *rdptr;
	size_t padlen, rdlen, wrlen;
	int ret;

	if (!sdi)
		return SR_ERR_ARG;
	conn = sdi->conn;
	if (!conn)
		return SR_ERR_ARG;
	if (!s)
		return SR_ERR_ARG;

	/*
	 * Trim surrounding whitespace. Normalize to canonical format.
	 * Make sure there is enough room for the period and CR/LF
	 * (and NUL termination). Use a glib API that's easy to adjust
	 * the padded length of. Performance is not a priority here.
	 */
	padlen = 4;
	while (padlen--)
		g_string_append_c(s, '\0');
	rdptr = sr_text_trim_spaces(s->str);
	rdlen = strlen(rdptr);
	if (rdlen && rdptr[rdlen - 1] == '.')
		rdlen--;
	g_string_set_size(s, rdlen);
	g_string_append_c(s, '.');
	sr_spew("serial TX text: --> %s", rdptr);
	g_string_append_c(s, '\r');
	g_string_append_c(s, '\n');
	rdlen = strlen(rdptr);
	log_raw_bytes("serial TX bytes: --> ", s);

	/* Handle chunked writes, check for transmission errors. */
	while (rdlen) {
		ret = serial_write_blocking(conn, rdptr, rdlen, 0);
		if (ret < 0)
			return SR_ERR_IO;
		wrlen = (size_t)ret;
		if (wrlen > rdlen)
			wrlen = rdlen;
		rdptr += wrlen;
		rdlen -= wrlen;
	}

	if (delay_ms)
		g_usleep(delay_ms * 1000);

	return SR_OK;
}

/*
 * Reads a text line from the serial port. Assumes that only a single
 * response text line is in flight (does not handle the case of more
 * receive data following after the first EOL). Transparently deals
 * with trailing period and end-of-line, so callers need not bother.
 *
 * Checks plausibility when the caller specifies conditions to check.
 * Optionally returns references (and lengths) to the response's RHS.
 * That's fine because data resides in a caller provided buffer.
 */
static int serial_recv_textline(const struct sr_dev_inst *sdi,
	GString *s, unsigned int delay_ms, unsigned int timeout_ms,
	gboolean *is_ok, char wants_insn, size_t wants_index,
	char **rhs_start, size_t *rhs_length)
{
	struct sr_serial_dev_inst *ser;
	char *rdptr;
	size_t rdlen, got;
	int ret;
	guint64 now_us, deadline_us;
	gboolean has_timedout;
	char *eol_pos, *endptr;
	char got_insn;
	unsigned long got_index;

	if (is_ok)
		*is_ok = FALSE;
	if (rhs_start)
		*rhs_start = NULL;
	if (rhs_length)
		*rhs_length = 0;

	if (!sdi)
		return SR_ERR_ARG;
	ser = sdi->conn;
	if (!ser)
		return SR_ERR_ARG;
	if (!s)
		return SR_ERR_ARG;

	g_string_set_size(s, MAX_RSP_LENGTH);
	g_string_truncate(s, 0);

	/* Arrange for overall receive timeout when caller specified. */
	now_us = deadline_us = 0;
	if (timeout_ms) {
		now_us = g_get_monotonic_time();
		deadline_us = now_us;
		deadline_us += timeout_ms * 1000;
	}

	rdptr = s->str;
	rdlen = s->allocated_len - 1 - s->len;
	while (rdlen) {
		/* Get another chunk of receive data. Check for EOL. */
		ret = serial_read_blocking(ser, rdptr, rdlen, delay_ms);
		if (ret < 0)
			return SR_ERR_IO;
		got = (size_t)ret;
		if (got > rdlen)
			got = rdlen;
		rdptr[got] = '\0';
		eol_pos = strchr(rdptr, '\n');
		rdptr += got;
		rdlen -= got;
		g_string_set_size(s, s->len + got);
		/* Check timeout expiration upon empty reception. */
		has_timedout = FALSE;
		if (timeout_ms && !got) {
			now_us = g_get_monotonic_time();
			if (now_us >= deadline_us)
				has_timedout = TRUE;
		}
		if (!eol_pos) {
			if (has_timedout)
				break;
			continue;
		}
		log_raw_bytes("serial RX bytes: <-- ", s);

		/* Normalize the received text line. */
		*eol_pos++ = '\0';
		rdptr = s->str;
		(void)sr_text_trim_spaces(rdptr);
		rdlen = strlen(rdptr);
		sr_spew("serial RX text: <-- %s", rdptr);
		if (rdlen && rdptr[rdlen - 1] == '.')
			rdptr[--rdlen] = '\0';

		/* Check conditions as requested by the caller. */
		if (is_ok || wants_insn || rhs_start) {
			if (*rdptr != ':') {
				sr_dbg("serial read, colon missing");
				return SR_ERR_DATA;
			}
			rdptr++;
			rdlen--;
		}
		/*
		 * The check for 'ok' is terminal. Does not combine with
		 * responses which carry payload data on their RHS.
		 */
		if (is_ok) {
			*is_ok = strcmp(rdptr, "ok") == 0;
			sr_dbg("serial read, 'ok' check %d", *is_ok);
			return *is_ok ? SR_OK : SR_ERR_DATA;
		}
		/*
		 * Conditional strict checks for caller's expected fields.
		 * Unconditional weaker checks for general structure.
		 */
		if (wants_insn && *rdptr != wants_insn) {
			sr_dbg("serial read, unexpected insn");
			return SR_ERR_DATA;
		}
		got_insn = *rdptr++;
		switch (got_insn) {
		case INSN_WRITE_PARA:
		case INSN_READ_PARA:
		case INSN_WRITE_WAVE:
		case INSN_READ_WAVE:
			/* EMPTY */
			break;
		default:
			sr_dbg("serial read, unknown insn %c", got_insn);
			return SR_ERR_DATA;
		}
		endptr = NULL;
		ret = sr_atoul_base(rdptr, &got_index, &endptr, 10);
		if (ret != SR_OK || !endptr)
			return SR_ERR_DATA;
		if (wants_index && got_index != wants_index) {
			sr_dbg("serial read, unexpected index %lu", got_index);
			return SR_ERR_DATA;
		}
		rdptr = endptr;
		if (rhs_start || rhs_length) {
			if (*rdptr != '=') {
				sr_dbg("serial read, equals sign missing");
				return SR_ERR_DATA;
			}
		}
		if (*rdptr)
			rdptr++;

		/* Response is considered plausible here. */
		if (rhs_start)
			*rhs_start = rdptr;
		if (rhs_length)
			*rhs_length = strlen(rdptr);
		return SR_OK;
	}
	log_raw_bytes("serial RX bytes: <-- ", s);
	sr_dbg("serial read, unterminated response, discarded");

	return SR_ERR_DATA;
}

/* Formatting helpers for request construction. */

static void append_insn_read_para(GString *s, char insn, size_t idx)
{
	g_string_append_printf(s, ":%c%02zu=0", insn, idx & 0xff);
}

static void append_insn_write_para_va(GString *s, char insn, size_t idx,
	const char *fmt, va_list args) ATTR_FMT_PRINTF(4, 0);
static void append_insn_write_para_va(GString *s, char insn, size_t idx,
	const char *fmt, va_list args)
{
	g_string_append_printf(s, ":%c%02zu=", insn, idx & 0xff);
	g_string_append_vprintf(s, fmt, args);
}

static void append_insn_write_para_dots(GString *s, char insn, size_t idx,
	const char *fmt, ...) ATTR_FMT_PRINTF(4, 5);
static void append_insn_write_para_dots(GString *s, char insn, size_t idx,
	const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	append_insn_write_para_va(s, insn, idx, fmt, args);
	va_end(args);
}

/*
 * Turn comma separators into whitespace. Simplifies the interpretation
 * of multi-value response payloads. Also replaces any trailing period
 * in case callers kept one in the receive buffer.
 */
static void replace_separators(char *s)
{

	while (s && *s) {
		if (s[0] == ',') {
			*s++ = ' ';
			continue;
		}
		if (s[0] == '.' && s[1] == '\0') {
			*s++ = ' ';
			continue;
		}
		s++;
	}
}

/*
 * Convenience to interpret responses' values. Also concentrates the
 * involved magic and simplifies diagnostics. It's essential to apply
 * implicit multipliers, and to properly combine multiple fields into
 * the resulting parameter's value (think scaling and offsetting).
 */

static const double scales_freq[] = {
	1, 1, 1, 1e-3, 1e-6,
};

static int parse_freq_text(char *s, double *value)
{
	char *word;
	int ret;
	double dvalue;
	unsigned long scale;

	replace_separators(s);

	/* First word is a mantissa, in centi-Hertz. :-O */
	word = sr_text_next_word(s, &s);
	if (!word)
		return SR_ERR_DATA;
	ret = sr_atod(word, &dvalue);
	g_free(word);
	if (ret != SR_OK)
		return ret;

	/* Next word is an encoded scaling factor. */
	word = sr_text_next_word(s, &s);
	if (!word)
		return SR_ERR_DATA;
	ret = sr_atoul_base(word, &scale, NULL, 10);
	g_free(word);
	if (ret != SR_OK)
		return ret;
	sr_spew("parse freq, mant %f, scale %lu", dvalue, scale);
	if (scale >= ARRAY_SIZE(scales_freq))
		return SR_ERR_DATA;

	/* Do scale the mantissa's value. */
	dvalue /= 100.0;
	dvalue /= scales_freq[scale];
	sr_spew("parse freq, value %f", dvalue);

	if (value)
		*value = dvalue;
	return SR_OK;
}

static int parse_volt_text(char *s, double *value)
{
	int ret;
	double dvalue;

	/* Single value, in units of mV. */
	ret = sr_atod(s, &dvalue);
	if (ret != SR_OK)
		return ret;
	sr_spew("parse volt, mant %f", dvalue);
	dvalue /= 1000.0;
	sr_spew("parse volt, value %f", dvalue);

	if (value)
		*value = dvalue;
	return SR_OK;
}

static int parse_bias_text(char *s, double *value)
{
	int ret;
	double dvalue;

	/*
	 * Single value, in units of 10mV with a 10V offset. Capped to
	 * the +9.99V..-9.99V range. The Joy-IT PDF is a little weird
	 * suggesting that ":w27=9999." translates to 9.99 volts.
	 */
	ret = sr_atod(s, &dvalue);
	if (ret != SR_OK)
		return ret;
	sr_spew("parse bias, mant %f", dvalue);
	dvalue /= 100.0;
	dvalue -= 10.0;
	if (dvalue >= 9.99)
		dvalue = 9.99;
	if (dvalue <= -9.99)
		dvalue = -9.99;
	sr_spew("parse bias, value %f", dvalue);

	if (value)
		*value = dvalue;
	return SR_OK;
}

static int parse_duty_text(char *s, double *value)
{
	int ret;
	double dvalue;

	/*
	 * Single value, in units of 0.1% (permille).
	 * Scale to the 0.0..1.0 range.
	 */
	ret = sr_atod(s, &dvalue);
	if (ret != SR_OK)
		return ret;
	sr_spew("parse duty, mant %f", dvalue);
	dvalue /= 1000.0;
	sr_spew("parse duty, value %f", dvalue);

	if (value)
		*value = dvalue;
	return SR_OK;
}

static int parse_phase_text(char *s, double *value)
{
	int ret;
	double dvalue;

	/* Single value, in units of deci-degrees. */
	ret = sr_atod(s, &dvalue);
	if (ret != SR_OK)
		return ret;
	sr_spew("parse phase, mant %f", dvalue);
	dvalue /= 10.0;
	sr_spew("parse phase, value %f", dvalue);

	if (value)
		*value = dvalue;
	return SR_OK;
}

/*
 * Convenience to generate request presentations. Also concentrates the
 * involved magic and simplifies diagnostics. It's essential to apply
 * implicit multipliers, and to properly create all request fields that
 * communicate a value to the device's firmware (think scale and offset).
 */

static void write_freq_text(GString *s, double freq)
{
	unsigned long scale_idx;
	const char *text_pos;

	sr_spew("write freq, value %f", freq);
	text_pos = &s->str[s->len];

	/*
	 * First word is mantissa in centi-Hertz. Second word is a
	 * scaling factor code. Keep scaling simple, always scale
	 * by a factor of 1.0.
	 */
	scale_idx = 0;
	freq *= scales_freq[scale_idx];
	freq *= 100.0;

	g_string_append_printf(s, "%.0f,%lu", freq, scale_idx);
	sr_spew("write freq, text %s", text_pos);
}

static void write_volt_text(GString *s, double volt)
{
	const char *text_pos;

	sr_spew("write volt, value %f", volt);
	text_pos = &s->str[s->len];

	/*
	 * Single value in units of 1mV.
	 * Limit input values to the 0..+20 range. This writer is only
	 * used by the amplitude setter.
	 */
	if (volt > 20.0)
		volt = 20.0;
	if (volt < 0.0)
		volt = 0.0;
	volt *= 1000.0;
	g_string_append_printf(s, "%.0f", volt);
	sr_spew("write volt, text %s", text_pos);
}

static void write_bias_text(GString *s, double volt)
{
	const char *text_pos;

	sr_spew("write bias, value %f", volt);
	text_pos = &s->str[s->len];

	/*
	 * Single value in units of 10mV with a 10V offset. Capped to
	 * the +9.99..-9.99 range.
	 */
	if (volt > 9.99)
		volt = 9.99;
	if (volt < -9.99)
		volt = -9.99;
	volt += 10.0;
	volt *= 100.0;

	g_string_append_printf(s, "%.0f", volt);
	sr_spew("write bias, text %s", text_pos);
}

static void write_duty_text(GString *s, double duty)
{
	const char *text_pos;

	sr_spew("write duty, value %f", duty);
	text_pos = &s->str[s->len];

	/*
	 * Single value in units of 0.1% (permille). Capped to the
	 * 0.0..1.0 range.
	 */
	if (duty < 0.0)
		duty = 0.0;
	if (duty > 1.0)
		duty = 1.0;
	duty *= 1000.0;

	g_string_append_printf(s, "%.0f", duty);
	sr_spew("write duty, text %s", text_pos);
}

static void write_phase_text(GString *s, double phase)
{
	const char *text_pos;

	sr_spew("write phase, value %f", phase);
	text_pos = &s->str[s->len];

	/*
	 * Single value in units of deci-degrees.
	 * Kept to the 0..360 range by means of a modulo operation.
	 */
	phase = fmod(phase, 360.0);
	phase *= 10.0;

	g_string_append_printf(s, "%.0f", phase);
	sr_spew("write phase, text %s", text_pos);
}

/*
 * Convenience communication wrapper. Re-uses a buffer in devc, which
 * simplifies resource handling in error paths. Sends a parameter-less
 * read-request. Then receives a response which can carry values.
 */
static int quick_send_read_then_recv(const struct sr_dev_inst *sdi,
	char insn, size_t idx,
	unsigned int read_timeout_ms,
	char **rhs_start, size_t *rhs_length)
{
	struct dev_context *devc;
	GString *s;
	int ret;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;
	if (!devc->quick_req)
		devc->quick_req = g_string_sized_new(MAX_RSP_LENGTH);
	s = devc->quick_req;

	g_string_truncate(s, 0);
	append_insn_read_para(s, insn, idx);
	ret = serial_send_textline(sdi, s, DELAY_AFTER_SEND);
	if (ret != SR_OK)
		return ret;

	ret = serial_recv_textline(sdi, s,
		TIMEOUT_READ_CHUNK, read_timeout_ms,
		NULL, insn, idx, rhs_start, rhs_length);
	if (ret != SR_OK)
		return ret;

	return SR_OK;
}

/*
 * Convenience communication wrapper, re-uses a buffer in devc. Sends a
 * write-request with parameters. Then receives an "ok" style response.
 * Had to put the request details after the response related parameters
 * because of the va_list API.
 */
static int quick_send_write_then_recv_ok(const struct sr_dev_inst *sdi,
	unsigned int read_timeout_ms, gboolean *is_ok,
	char insn, size_t idx, const char *fmt, ...) ATTR_FMT_PRINTF(6, 7);
static int quick_send_write_then_recv_ok(const struct sr_dev_inst *sdi,
	unsigned int read_timeout_ms, gboolean *is_ok,
	char insn, size_t idx, const char *fmt, ...)
{
	struct dev_context *devc;
	GString *s;
	va_list args;
	int ret;
	gboolean ok;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;
	if (!devc->quick_req)
		devc->quick_req = g_string_sized_new(MAX_RSP_LENGTH);
	s = devc->quick_req;

	g_string_truncate(s, 0);
	va_start(args, fmt);
	append_insn_write_para_va(s, insn, idx, fmt, args);
	va_end(args);
	ret = serial_send_textline(sdi, s, DELAY_AFTER_SEND);
	if (ret != SR_OK)
		return ret;

	ret = serial_recv_textline(sdi, s,
		TIMEOUT_READ_CHUNK, read_timeout_ms,
		&ok, '\0', 0, NULL, NULL);
	if (is_ok)
		*is_ok = ok;
	if (ret != SR_OK)
		return ret;

	return SR_OK;
}

/*
 * ===========================================================================
 * High level getters/setters for device properties.
 * To be used by the api.c config get/set infrastructure.
 * ===========================================================================
 */

SR_PRIV int jds6600_get_chans_enable(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	int ret;
	char *rdptr, *word, *endptr;
	struct devc_dev *device;
	struct devc_chan *chans;
	size_t idx;
	unsigned long on;

	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;

	/* Transmit the request, receive the response. */
	ret = quick_send_read_then_recv(sdi,
		INSN_READ_PARA, IDX_CHANNELS_ENABLE,
		0, &rdptr, NULL);
	if (ret != SR_OK)
		return ret;
	sr_dbg("get enabled, response text: %s", rdptr);

	/* Interpret the response (multiple values, boolean). */
	replace_separators(rdptr);
	device = &devc->device;
	chans = devc->channel_config;
	for (idx = 0; idx < device->channel_count_gen; idx++) {
		word = sr_text_next_word(rdptr, &rdptr);
		if (!word || !*word)
			return SR_ERR_DATA;
		endptr = NULL;
		ret = sr_atoul_base(word, &on, &endptr, 10);
		g_free(word);
		if (ret != SR_OK || !endptr || *endptr)
			return SR_ERR_DATA;
		chans[idx].enabled = on;
	}

	return SR_OK;
}

SR_PRIV int jds6600_get_waveform(const struct sr_dev_inst *sdi, size_t ch_idx)
{
	struct dev_context *devc;
	int ret;
	char *rdptr, *endptr;
	struct devc_wave *waves;
	struct devc_chan *chan;
	unsigned long code;
	size_t idx;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;
	waves = &devc->waveforms;
	if (ch_idx >= ARRAY_SIZE(devc->channel_config))
		return SR_ERR_ARG;
	chan = &devc->channel_config[ch_idx];

	/* Transmit the request, receive the response. */
	ret = quick_send_read_then_recv(sdi,
		INSN_READ_PARA, IDX_WAVEFORM_CH1 + ch_idx,
		0, &rdptr, NULL);
	if (ret != SR_OK)
		return ret;
	sr_dbg("get waveform, response text: %s", rdptr);

	/*
	 * Interpret the response (integer value, waveform code).
	 * Lookup the firmware's code for that waveform in the
	 * list of user perceivable names for waveforms.
	 */
	endptr = NULL;
	ret = sr_atoul_base(rdptr, &code, &endptr, 10);
	if (ret != SR_OK)
		return SR_ERR_DATA;
	for (idx = 0; idx < waves->names_count; idx++) {
		if (code != waves->fw_codes[idx])
			continue;
		chan->waveform_code = code;
		chan->waveform_index = idx;
		sr_dbg("get waveform, code %lu, idx %zu, name %s",
			code, idx, waves->names[idx]);
		return SR_OK;
	}

	return SR_ERR_DATA;
}

SR_PRIV int jds6600_get_frequency(const struct sr_dev_inst *sdi, size_t ch_idx)
{
	struct dev_context *devc;
	struct devc_chan *chan;
	int ret;
	char *rdptr;
	double freq;

	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;
	if (ch_idx >= ARRAY_SIZE(devc->channel_config))
		return SR_ERR_ARG;
	chan = &devc->channel_config[ch_idx];

	/* Transmit the request, receive the response. */
	ret = quick_send_read_then_recv(sdi,
		INSN_READ_PARA, IDX_FREQUENCY_CH1 + ch_idx,
		0, &rdptr, NULL);
	if (ret != SR_OK)
		return ret;
	sr_dbg("get frequency, response text: %s", rdptr);

	/* Interpret the response (value and scale, frequency). */
	ret = parse_freq_text(rdptr, &freq);
	if (ret != SR_OK)
		return SR_ERR_DATA;
	sr_dbg("get frequency, value %f", freq);
	chan->output_frequency = freq;
	return SR_OK;
}

SR_PRIV int jds6600_get_amplitude(const struct sr_dev_inst *sdi, size_t ch_idx)
{
	struct dev_context *devc;
	struct devc_chan *chan;
	int ret;
	char *rdptr;
	double amp;

	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;
	if (ch_idx >= ARRAY_SIZE(devc->channel_config))
		return SR_ERR_ARG;
	chan = &devc->channel_config[ch_idx];

	/* Transmit the request, receive the response. */
	ret = quick_send_read_then_recv(sdi,
		INSN_READ_PARA, IDX_AMPLITUDE_CH1 + ch_idx,
		0, &rdptr, NULL);
	if (ret != SR_OK)
		return ret;
	sr_dbg("get amplitude, response text: %s", rdptr);

	/* Interpret the response (single value, a voltage). */
	ret = parse_volt_text(rdptr, &amp);
	if (ret != SR_OK)
		return SR_ERR_DATA;
	sr_dbg("get amplitude, value %f", amp);
	chan->amplitude = amp;
	return SR_OK;
}

SR_PRIV int jds6600_get_offset(const struct sr_dev_inst *sdi, size_t ch_idx)
{
	struct dev_context *devc;
	struct devc_chan *chan;
	int ret;
	char *rdptr;
	double off;

	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;
	if (ch_idx >= ARRAY_SIZE(devc->channel_config))
		return SR_ERR_ARG;
	chan = &devc->channel_config[ch_idx];

	/* Transmit the request, receive the response. */
	ret = quick_send_read_then_recv(sdi,
		INSN_READ_PARA, IDX_OFFSET_CH1 + ch_idx,
		0, &rdptr, NULL);
	if (ret != SR_OK)
		return ret;
	sr_dbg("get offset, response text: %s", rdptr);

	/* Interpret the response (single value, an offset). */
	ret = parse_bias_text(rdptr, &off);
	if (ret != SR_OK)
		return SR_ERR_DATA;
	sr_dbg("get offset, value %f", off);
	chan->offset = off;
	return SR_OK;
}

SR_PRIV int jds6600_get_dutycycle(const struct sr_dev_inst *sdi, size_t ch_idx)
{
	struct dev_context *devc;
	struct devc_chan *chan;
	int ret;
	char *rdptr;
	double duty;

	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;
	if (ch_idx >= ARRAY_SIZE(devc->channel_config))
		return SR_ERR_ARG;
	chan = &devc->channel_config[ch_idx];

	/* Transmit the request, receive the response. */
	ret = quick_send_read_then_recv(sdi,
		INSN_READ_PARA, IDX_DUTYCYCLE_CH1 + ch_idx,
		0, &rdptr, NULL);
	if (ret != SR_OK)
		return ret;
	sr_dbg("get duty cycle, response text: %s", rdptr);

	/* Interpret the response (single value, a percentage). */
	ret = parse_duty_text(rdptr, &duty);
	if (ret != SR_OK)
		return SR_ERR_DATA;
	sr_dbg("get duty cycle, value %f", duty);
	chan->dutycycle = duty;
	return SR_OK;
}

SR_PRIV int jds6600_get_phase_chans(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	int ret;
	char *rdptr;
	double phase;

	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;

	/* Transmit the request, receive the response. */
	ret = quick_send_read_then_recv(sdi,
		INSN_READ_PARA, IDX_PHASE_CHANNELS,
		0, &rdptr, NULL);
	if (ret != SR_OK)
		return ret;
	sr_dbg("get phase, response text: %s", rdptr);

	/* Interpret the response (single value, an angle). */
	ret = parse_phase_text(rdptr, &phase);
	if (ret != SR_OK)
		return SR_ERR_DATA;
	sr_dbg("get phase, value %f", phase);
	devc->channels_phase = phase;
	return SR_OK;
}

SR_PRIV int jds6600_set_chans_enable(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct devc_chan *chans;
	GString *en_text;
	size_t idx;
	int ret;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;

	/* Transmit the request, receive an "ok" style response. */
	chans = devc->channel_config;
	en_text = g_string_sized_new(20);
	for (idx = 0; idx < devc->device.channel_count_gen; idx++) {
		if (en_text->len)
			g_string_append_c(en_text, ',');
		g_string_append_c(en_text, chans[idx].enabled ? '1' : '0');
	}
	sr_dbg("set enabled, request text: %s", en_text->str);
	ret = quick_send_write_then_recv_ok(sdi, 0, NULL,
		INSN_WRITE_PARA, IDX_CHANNELS_ENABLE, "%s", en_text->str);
	g_string_free(en_text, TRUE);
	if (ret != SR_OK)
		return ret;

	return SR_OK;
}

SR_PRIV int jds6600_set_waveform(const struct sr_dev_inst *sdi, size_t ch_idx)
{
	struct dev_context *devc;
	struct devc_chan *chan;
	int ret;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;
	if (ch_idx >= devc->device.channel_count_gen)
		return SR_ERR_ARG;
	chan = &devc->channel_config[ch_idx];

	/* Transmit the request, receive an "ok" style response. */
	ret = quick_send_write_then_recv_ok(sdi, 0, NULL,
		INSN_WRITE_PARA, IDX_WAVEFORM_CH1 + ch_idx,
		"%" PRIu32, chan->waveform_code);
	if (ret != SR_OK)
		return ret;

	return SR_OK;
}

SR_PRIV int jds6600_set_frequency(const struct sr_dev_inst *sdi, size_t ch_idx)
{
	struct dev_context *devc;
	struct devc_chan *chan;
	double freq;
	GString *freq_text;
	int ret;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;
	if (ch_idx >= devc->device.channel_count_gen)
		return SR_ERR_ARG;
	chan = &devc->channel_config[ch_idx];

	/* Limit input values to the range supported by the model. */
	freq = chan->output_frequency;
	if (freq < 0.01)
		freq = 0.01;
	if (freq > devc->device.max_output_frequency)
		freq = devc->device.max_output_frequency;

	/* Transmit the request, receive an "ok" style response. */
	freq_text = g_string_sized_new(32);
	write_freq_text(freq_text, freq);
	ret = quick_send_write_then_recv_ok(sdi, 0, NULL,
		INSN_WRITE_PARA, IDX_FREQUENCY_CH1 + ch_idx,
		"%s", freq_text->str);
	g_string_free(freq_text, TRUE);
	if (ret != SR_OK)
		return ret;

	return SR_OK;
}

SR_PRIV int jds6600_set_amplitude(const struct sr_dev_inst *sdi, size_t ch_idx)
{
	struct dev_context *devc;
	struct devc_chan *chan;
	GString *volt_text;
	int ret;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;
	if (ch_idx >= devc->device.channel_count_gen)
		return SR_ERR_ARG;
	chan = &devc->channel_config[ch_idx];

	/* Transmit the request, receive an "ok" style response. */
	volt_text = g_string_sized_new(32);
	write_volt_text(volt_text, chan->amplitude);
	ret = quick_send_write_then_recv_ok(sdi, 0, NULL,
		INSN_WRITE_PARA, IDX_AMPLITUDE_CH1 + ch_idx,
		"%s", volt_text->str);
	g_string_free(volt_text, TRUE);
	if (ret != SR_OK)
		return ret;

	return SR_OK;
}

SR_PRIV int jds6600_set_offset(const struct sr_dev_inst *sdi, size_t ch_idx)
{
	struct dev_context *devc;
	struct devc_chan *chan;
	GString *volt_text;
	int ret;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;
	if (ch_idx >= devc->device.channel_count_gen)
		return SR_ERR_ARG;
	chan = &devc->channel_config[ch_idx];

	/* Transmit the request, receive an "ok" style response. */
	volt_text = g_string_sized_new(32);
	write_bias_text(volt_text, chan->offset);
	ret = quick_send_write_then_recv_ok(sdi, 0, NULL,
		INSN_WRITE_PARA, IDX_OFFSET_CH1 + ch_idx,
		"%s", volt_text->str);
	g_string_free(volt_text, TRUE);
	if (ret != SR_OK)
		return ret;

	return SR_OK;
}

SR_PRIV int jds6600_set_dutycycle(const struct sr_dev_inst *sdi, size_t ch_idx)
{
	struct dev_context *devc;
	struct devc_chan *chan;
	GString *duty_text;
	int ret;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;
	if (ch_idx >= devc->device.channel_count_gen)
		return SR_ERR_ARG;
	chan = &devc->channel_config[ch_idx];

	/* Transmit the request, receive an "ok" style response. */
	duty_text = g_string_sized_new(32);
	write_duty_text(duty_text, chan->dutycycle);
	ret = quick_send_write_then_recv_ok(sdi, 0, NULL,
		INSN_WRITE_PARA, IDX_DUTYCYCLE_CH1 + ch_idx,
		"%s", duty_text->str);
	g_string_free(duty_text, TRUE);
	if (ret != SR_OK)
		return ret;

	return SR_OK;
}

SR_PRIV int jds6600_set_phase_chans(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	GString *phase_text;
	int ret;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;

	/* Transmit the request, receive an "ok" style response. */
	phase_text = g_string_sized_new(32);
	write_phase_text(phase_text, devc->channels_phase);
	ret = quick_send_write_then_recv_ok(sdi, 0, NULL,
		INSN_WRITE_PARA, IDX_PHASE_CHANNELS,
		"%s", phase_text->str);
	g_string_free(phase_text, TRUE);
	if (ret != SR_OK)
		return ret;

	return SR_OK;
}

/*
 * ===========================================================================
 * High level helpers for the scan/probe phase. Identify the attached
 * device and synchronize to its current state and its capabilities.
 * ===========================================================================
 */

SR_PRIV int jds6600_identify(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	int ret;
	char *rdptr, *endptr;
	unsigned long devtype;

	(void)append_insn_write_para_dots;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;

	/* Transmit "read device type" request, receive the response. */
	ret = quick_send_read_then_recv(sdi,
		INSN_READ_PARA, IDX_DEVICE_TYPE,
		TIMEOUT_IDENTIFY, &rdptr, NULL);
	if (ret != SR_OK)
		return ret;
	sr_dbg("identify, device type '%s'", rdptr);

	/* Interpret the response (integer value, max freq). */
	endptr = NULL;
	ret = sr_atoul_base(rdptr, &devtype, &endptr, 10);
	if (ret != SR_OK || !endptr)
		return SR_ERR_DATA;
	devc->device.device_type = devtype;

	/* Transmit "read serial number" request. receive response. */
	ret = quick_send_read_then_recv(sdi,
		INSN_READ_PARA, IDX_SERIAL_NUMBER,
		0, &rdptr, NULL);
	if (ret != SR_OK)
		return ret;
	sr_dbg("identify, serial number '%s'", rdptr);

	/* Keep the response (in string format, some serial number). */
	devc->device.serial_number = g_strdup(rdptr);

	return SR_OK;
}

SR_PRIV int jds6600_setup_devc(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	size_t alloc_count, assign_idx, idx;
	struct devc_dev *device;
	struct devc_wave *waves;
	enum waveform_index_t code;
	char *name;
	int ret;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;

	/*
	 * Derive maximum output frequency from detected device type.
	 * Open coded generator channel count.
	 */
	device = &devc->device;
	if (!device->device_type)
		return SR_ERR_DATA;
	device->max_output_frequency = device->device_type;
	device->max_output_frequency *= SR_MHZ(1);
	device->channel_count_gen = MAX_GEN_CHANNELS;

	/* Construct the list of waveform names and their codes. */
	waves = &devc->waveforms;
	waves->builtin_count = WAVES_COUNT_BUILTIN;
	waves->arbitrary_count = WAVES_COUNT_ARBITRARY;
	alloc_count = waves->builtin_count;
	alloc_count += waves->arbitrary_count;
	waves->names_count = alloc_count;
	waves->fw_codes = g_malloc0(alloc_count * sizeof(waves->fw_codes[0]));
	alloc_count++;
	waves->names = g_malloc0(alloc_count * sizeof(waves->names[0]));
	if (!waves->names || !waves->fw_codes) {
		g_free(waves->names);
		g_free(waves->fw_codes);
		return SR_ERR_MALLOC;
	}
	assign_idx = 0;
	for (idx = 0; idx < waves->builtin_count; idx++) {
		code = idx;
		name = g_strdup(waveform_names[idx]);
		waves->fw_codes[assign_idx] = code;
		waves->names[assign_idx] = name;
		assign_idx++;
	}
	for (idx = 0; idx < waves->arbitrary_count; idx++) {
		code = WAVE_ARB01 + idx;
		name = g_strdup_printf(WAVEFORM_ARB_NAME_FMT, idx + 1);
		waves->fw_codes[assign_idx] = code;
		waves->names[assign_idx] = name;
		assign_idx++;
	}
	waves->names[assign_idx] = NULL;

	/*
	 * Populate internal channel configuration details from the
	 * device's current state. Emit a series of queries which
	 * update internal knowledge.
	 *
	 * Implementation detail: Channel count is low, all parameters
	 * are simple scalars. Communication cycles are few, while we
	 * still are in the scan/probe phase and successfully verified
	 * the device to respond. Disconnects and other exceptional
	 * conditions are extremely unlikely. Not checking every getter
	 * call's return value is acceptable here.
	 */
	ret = SR_OK;
	ret |= jds6600_get_chans_enable(sdi);
	for (idx = 0; idx < device->channel_count_gen; idx++) {
		ret |= jds6600_get_waveform(sdi, idx);
		ret |= jds6600_get_frequency(sdi, idx);
		ret |= jds6600_get_amplitude(sdi, idx);
		ret |= jds6600_get_offset(sdi, idx);
		ret |= jds6600_get_dutycycle(sdi, idx);
		if (ret != SR_OK)
			break;
	}
	ret |= jds6600_get_phase_chans(sdi);
	if (ret != SR_OK)
		return SR_ERR_DATA;

	return SR_OK;
}

/*
 * ===========================================================================
 * Local std_dummy_dev_acquisition_start / stop no-ops (Rule 14 / point 1).
 *
 * The JDS6600 is a signal generator that does not stream sample data.
 * Acquisition start/stop therefore are no-ops that return SR_OK without
 * registering any data source or sending DF_HEADER/DF_END packets. Matches
 * the conrad-digi-35-cpu and hp-59306a compat pattern.
 * ===========================================================================
 */

SR_PRIV int std_dummy_dev_acquisition_start(struct sr_dev_inst *sdi)
{
	(void)sdi;
	return SR_OK;
}

SR_PRIV int std_dummy_dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	(void)sdi;
	return SR_OK;
}

/*
 * ===========================================================================
 * Local dev_clear with callback (driver-specific point 2).
 *
 * PXView's compat layer does not provide std_dev_clear_with_callback().
 * This implementation iterates the driver's instance list, calls
 * clear_helper on each devc to release the waveform names/codes/quick_req
 * buffers and the serial_number string, then delegates to
 * std_dev_clear_compat() to free the sdi list. Matches the atorch pattern.
 * ===========================================================================
 */

static void clear_helper(struct dev_context *devc)
{
	struct devc_wave *waves;

	if (!devc)
		return;

	g_free(devc->device.serial_number);
	waves = &devc->waveforms;
	while (waves->names_count)
		g_free((char *)waves->names[--waves->names_count]);
	g_free(waves->names);
	g_free(waves->fw_codes);
	if (devc->quick_req)
		g_string_free(devc->quick_req, TRUE);
}

SR_PRIV int jds6600_dev_clear(const struct sr_dev_driver *di)
{
	struct compat_drv_context *drvc;
	GSList *l;
	struct sr_dev_inst *sdi;

	if (!di || !di->priv)
		return SR_ERR_ARG;

	drvc = di->priv;
	for (l = drvc->instances; l; l = l->next) {
		sdi = l->data;
		if (sdi && sdi->priv)
			clear_helper(sdi->priv);
	}

	return std_dev_clear_compat(di);
}
