/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2020 Andreas Sandberg <andreas@sandberg.pp.se>
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

#include "hardware/compat/compat.h"
#include <stdlib.h>
#include <string.h>
#include "protocol.h"

#define PROBE_TO_MS	1000
#define WRITE_TO_MS	1
#define POLL_PERIOD_MS	100

/*
 * Response data (raw sample data) consists of three adjacent chunks
 * of 64 bytes each. These chunks start with their magic string, and
 * end in a 32bit checksum field. Measurement values are scattered
 * across these 192 bytes total size. All multi-byte integer values
 * are represented in little endian format. Typical size is 32 bits.
 */

#define MAGIC_PAC1	0x70616331	/* 'pac1' */
#define MAGIC_PAC2	0x70616332	/* 'pac2' */
#define MAGIC_PAC3	0x70616333	/* 'pac3' */

#define PAC_LEN 64
#define PAC_CRC_POS (PAC_LEN - sizeof(uint32_t))

/* Offset to PAC block from start of poll data */
#define OFF_PAC1 (0 * PAC_LEN)
#define OFF_PAC2 (1 * PAC_LEN)
#define OFF_PAC3 (2 * PAC_LEN)
#define TC_POLL_LEN (3 * PAC_LEN)
#if TC_POLL_LEN > RDTECH_TC_RSPBUFSIZE
#  error "response length exceeds receive buffer space"
#endif

#define OFF_MODEL 4
#define LEN_MODEL 4

#define OFF_FW_VER 8
#define LEN_FW_VER 4

#define OFF_SERIAL 12

static const uint8_t aes_key[] = {
	0x58, 0x21, 0xfa, 0x56, 0x01, 0xb2, 0xf0, 0x26,
	0x87, 0xff, 0x12, 0x04, 0x62, 0x2a, 0x4f, 0xb0,
	0x86, 0xf4, 0x02, 0x60, 0x81, 0x6f, 0x9a, 0x0b,
	0xa7, 0xf1, 0x06, 0x61, 0x9a, 0xb8, 0x72, 0x88,
};

static const struct rdtech_tc_channel_desc rdtech_tc_channels[] = {
	{ "V",  {   0 + 48, BVT_LE_UINT32, }, { 100, 1000000, }, 4, SR_MQ_VOLTAGE, SR_UNIT_VOLT },
	{ "I",  {   0 + 52, BVT_LE_UINT32, }, {  10, 1000000, }, 5, SR_MQ_CURRENT, SR_UNIT_AMPERE },
	{ "D+", {  64 + 32, BVT_LE_UINT32, }, {  10, 1000, }, 2, SR_MQ_VOLTAGE, SR_UNIT_VOLT },
	{ "D-", {  64 + 36, BVT_LE_UINT32, }, {  10, 1000, }, 2, SR_MQ_VOLTAGE, SR_UNIT_VOLT },
	{ "E0", {  64 + 12, BVT_LE_UINT32, }, {   1, 1000, }, 3, SR_MQ_ENERGY, SR_UNIT_WATT_HOUR },
	{ "E1", {  64 + 20, BVT_LE_UINT32, }, {   1, 1000, }, 3, SR_MQ_ENERGY, SR_UNIT_WATT_HOUR },
};

/* ===========================================================================
 * Local AES-256-ECB decryption implementation.
 *
 * PXView does not link against nettle. The rdtech-tc protocol encrypts poll
 * responses with AES-256-ECB using a fixed key, so a self-contained
 * AES-256 implementation is provided here based on FIPS-197.
 * =========================================================================== */

static const uint8_t aes_sbox[256] = {
	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
	0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
	0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
	0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
	0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
	0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
	0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
	0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
	0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
	0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
	0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

static const uint8_t aes_inv_sbox[256] = {
	0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
	0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
	0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
	0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
	0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
	0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
	0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
	0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
	0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
	0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
	0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
	0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
	0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
	0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
	0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
	0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d,
};

/* Round constants for AES-256 key expansion (7 needed: indices 1..7). */
static const uint8_t aes_rcon[] = {
	0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40,
};

/* GF(2^8) multiplication using the AES irreducible polynomial x^8+x^4+x^3+x+1. */
static uint8_t rdtech_tc_gmul(uint8_t a, uint8_t b)
{
	uint8_t p = 0;
	int i;

	for (i = 0; i < 8; i++) {
		if (b & 1)
			p ^= a;
		uint8_t hi = a & 0x80;
		a <<= 1;
		if (hi)
			a ^= 0x1b;
		b >>= 1;
	}
	return p;
}

/* AES-256 key expansion: produces 60 words (240 bytes) of round key. */
static void rdtech_tc_key_expansion(const uint8_t *key, uint8_t *rk)
{
	int i;

	/* First 8 words (32 bytes) come directly from the key. */
	memcpy(rk, key, 32);

	for (i = 8; i < 60; i++) {
		uint8_t t0, t1, t2, t3;

		t0 = rk[(i - 1) * 4 + 0];
		t1 = rk[(i - 1) * 4 + 1];
		t2 = rk[(i - 1) * 4 + 2];
		t3 = rk[(i - 1) * 4 + 3];

		if (i % 8 == 0) {
			/* RotWord then SubWord then XOR Rcon. */
			uint8_t tmp = t0;
			t0 = aes_sbox[t1] ^ aes_rcon[i / 8];
			t1 = aes_sbox[t2];
			t2 = aes_sbox[t3];
			t3 = aes_sbox[tmp];
		} else if (i % 8 == 4) {
			/* SubWord only. */
			t0 = aes_sbox[t0];
			t1 = aes_sbox[t1];
			t2 = aes_sbox[t2];
			t3 = aes_sbox[t3];
		}

		rk[i * 4 + 0] = rk[(i - 8) * 4 + 0] ^ t0;
		rk[i * 4 + 1] = rk[(i - 8) * 4 + 1] ^ t1;
		rk[i * 4 + 2] = rk[(i - 8) * 4 + 2] ^ t2;
		rk[i * 4 + 3] = rk[(i - 8) * 4 + 3] ^ t3;
	}
}

static void rdtech_tc_inv_sub_bytes(uint8_t *s)
{
	int i;

	for (i = 0; i < 16; i++)
		s[i] = aes_inv_sbox[s[i]];
}

static void rdtech_tc_inv_shift_rows(uint8_t *s)
{
	uint8_t t;

	/* State is column-major: s[r + 4*c]. Row r, column c. */
	/* Row 1: shift right by 1. */
	t = s[1];
	s[1]  = s[5];
	s[5]  = s[9];
	s[9]  = s[13];
	s[13] = t;

	/* Row 2: shift right by 2 (swap pairs). */
	t = s[2];  s[2]  = s[10]; s[10] = t;
	t = s[6];  s[6]  = s[14]; s[14] = t;

	/* Row 3: shift right by 3 (== shift left by 1). */
	t = s[15];
	s[15] = s[11];
	s[11] = s[7];
	s[7]  = s[3];
	s[3]  = t;
}

static void rdtech_tc_inv_mix_columns(uint8_t *s)
{
	int c;

	for (c = 0; c < 4; c++) {
		uint8_t *col = &s[4 * c];
		uint8_t a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];

		col[0] = rdtech_tc_gmul(a0, 0x0e) ^ rdtech_tc_gmul(a1, 0x0b) ^
		         rdtech_tc_gmul(a2, 0x0d) ^ rdtech_tc_gmul(a3, 0x09);
		col[1] = rdtech_tc_gmul(a0, 0x09) ^ rdtech_tc_gmul(a1, 0x0e) ^
		         rdtech_tc_gmul(a2, 0x0b) ^ rdtech_tc_gmul(a3, 0x0d);
		col[2] = rdtech_tc_gmul(a0, 0x0d) ^ rdtech_tc_gmul(a1, 0x09) ^
		         rdtech_tc_gmul(a2, 0x0e) ^ rdtech_tc_gmul(a3, 0x0b);
		col[3] = rdtech_tc_gmul(a0, 0x0b) ^ rdtech_tc_gmul(a1, 0x0d) ^
		         rdtech_tc_gmul(a2, 0x09) ^ rdtech_tc_gmul(a3, 0x0e);
	}
}

static void rdtech_tc_add_round_key(uint8_t *s, const uint8_t *rk)
{
	int i;

	for (i = 0; i < 16; i++)
		s[i] ^= rk[i];
}

SR_PRIV void rdtech_tc_aes256_set_decrypt_key(struct aes256_ctx *ctx,
		const uint8_t *key)
{
	rdtech_tc_key_expansion(key, ctx->round_keys);
}

SR_PRIV void rdtech_tc_aes256_decrypt(struct aes256_ctx *ctx, size_t length,
		uint8_t *dst, const uint8_t *src)
{
	const uint8_t *rk = ctx->round_keys;

	while (length >= 16) {
		uint8_t state[16];
		int round;

		memcpy(state, src, 16);

		/* Initial AddRoundKey with the last round key. */
		rdtech_tc_add_round_key(state, &rk[14 * 16]);

		/* Rounds Nr-1 down to 1. */
		for (round = 13; round >= 1; round--) {
			rdtech_tc_inv_shift_rows(state);
			rdtech_tc_inv_sub_bytes(state);
			rdtech_tc_add_round_key(state, &rk[round * 16]);
			rdtech_tc_inv_mix_columns(state);
		}

		/* Final round (no InvMixColumns). */
		rdtech_tc_inv_shift_rows(state);
		rdtech_tc_inv_sub_bytes(state);
		rdtech_tc_add_round_key(state, &rk[0]);

		memcpy(dst, state, 16);
		src += 16;
		dst += 16;
		length -= 16;
	}
}

/* ===========================================================================
 * Local sr_crc16 (CRC-16/CCITT-FALSE, poly 0x1021, init 0xFFFF).
 * =========================================================================== */
SR_PRIV uint16_t rdtech_tc_sr_crc16(uint16_t crc, const uint8_t *buf, size_t len)
{
	size_t i;
	int j;

	for (i = 0; i < len; i++) {
		crc ^= (uint16_t)buf[i] << 8;
		for (j = 0; j < 8; j++) {
			if (crc & 0x8000)
				crc = (crc << 1) ^ 0x1021;
			else
				crc <<= 1;
		}
	}
	return crc;
}

/* ===========================================================================
 * Local sr_hexdump helpers (simple space-separated hex for debug spew).
 * =========================================================================== */
SR_PRIV GString *rdtech_tc_sr_hexdump_new(const uint8_t *buf, size_t len)
{
	GString *gstr;
	size_t i;

	gstr = g_string_sized_new(len * 3 + 1);
	for (i = 0; i < len; i++) {
		if (i > 0)
			g_string_append_c(gstr, ' ');
		g_string_append_printf(gstr, "%02x", buf[i]);
	}
	return gstr;
}

SR_PRIV void rdtech_tc_sr_hexdump_free(GString *gstr)
{
	if (gstr)
		g_string_free(gstr, TRUE);
}

/* ===========================================================================
 * Local ser_name_is_bt stub.
 * PXView does not have Bluetooth serial transport support. Return FALSE so
 * the driver uses the CDC poll command ("getva") instead of BLE ("bgetva").
 * =========================================================================== */
SR_PRIV gboolean rdtech_tc_ser_name_is_bt(struct sr_serial_dev_inst *serial)
{
	(void)serial;
	return FALSE;
}

/* ===========================================================================
 * Local std_session_send_df_frame_begin/end.
 * PXView only provides std_session_send_df_header/end (with prefix arg),
 * not the frame variants. Same pattern as itech-it8500, maynuo-m97.
 * =========================================================================== */
SR_PRIV int rdtech_tc_std_session_send_df_frame_begin(const struct sr_dev_inst *sdi)
{
	struct sr_datafeed_packet packet;

	memset(&packet, 0, sizeof(packet));
	packet.type = SR_DF_FRAME_BEGIN;
	packet.status = SR_PKT_OK;
	packet.payload = NULL;
	return ds_data_forward(sdi, &packet);
}

SR_PRIV int rdtech_tc_std_session_send_df_frame_end(const struct sr_dev_inst *sdi)
{
	struct sr_datafeed_packet packet;

	memset(&packet, 0, sizeof(packet));
	packet.type = SR_DF_FRAME_END;
	packet.status = SR_PKT_OK;
	packet.payload = NULL;
	return ds_data_forward(sdi, &packet);
}

/* ===========================================================================
 * Protocol parsing logic (preserved from original driver).
 * =========================================================================== */

static gboolean check_pac_crc(uint8_t *data)
{
	uint16_t crc_calc;
	uint32_t crc_recv;

	crc_calc = sr_crc16(SR_CRC16_DEFAULT_INIT, data, PAC_CRC_POS);
	crc_recv = read_u32le(&data[PAC_CRC_POS]);
	if (crc_calc != crc_recv) {
		sr_spew("CRC error. Calculated: %04" PRIx16 ", expected: %04" PRIx32,
			crc_calc, crc_recv);
		return FALSE;
	}

	return TRUE;
}

static int process_poll_pkt(struct dev_context *devc, uint8_t *dst)
{
	struct aes256_ctx ctx;
	gboolean ok;

	aes256_set_decrypt_key(&ctx, aes_key);
	aes256_decrypt(&ctx, TC_POLL_LEN, dst, devc->buf);

	ok = TRUE;
	ok &= read_u32be(&dst[OFF_PAC1]) == MAGIC_PAC1;
	ok &= read_u32be(&dst[OFF_PAC2]) == MAGIC_PAC2;
	ok &= read_u32be(&dst[OFF_PAC3]) == MAGIC_PAC3;
	if (!ok) {
		sr_err("Invalid poll response packet (magic values).");
		return SR_ERR_DATA;
	}

	ok &= check_pac_crc(&dst[OFF_PAC1]);
	ok &= check_pac_crc(&dst[OFF_PAC2]);
	ok &= check_pac_crc(&dst[OFF_PAC3]);
	if (!ok) {
		sr_err("Invalid poll response packet (checksum).");
		return SR_ERR_DATA;
	}

	if (sr_log_loglevel_get() >= SR_LOG_SPEW) {
		static const size_t chunk_max = 32;

		const uint8_t *rdptr;
		size_t rdlen, chunk_addr, chunk_len;
		GString *txt;

		sr_spew("check passed on decrypted receive data");
		rdptr = dst;
		rdlen = TC_POLL_LEN;
		chunk_addr = 0;
		while (rdlen) {
			chunk_len = rdlen;
			if (chunk_len > chunk_max)
				chunk_len = chunk_max;
			txt = sr_hexdump_new(rdptr, chunk_len);
			sr_spew("%04zx  %s", chunk_addr, txt->str);
			sr_hexdump_free(txt);
			chunk_addr += chunk_len;
			rdptr += chunk_len;
			rdlen -= chunk_len;
		}
	}

	return SR_OK;
}

SR_PRIV int rdtech_tc_probe(struct sr_serial_dev_inst *serial, struct dev_context *devc)
{
	static const char *poll_cmd_cdc = "getva";
	static const char *poll_cmd_ble = "bgetva\r\n";

	int len;
	uint8_t poll_pkt[TC_POLL_LEN];

	/* Construct the request text. Which differs across transports. */
	devc->is_bluetooth = ser_name_is_bt(serial);
	snprintf(devc->req_text, sizeof(devc->req_text), "%s",
		devc->is_bluetooth ? poll_cmd_ble : poll_cmd_cdc);
	sr_dbg("is bluetooth %d -> poll request '%s'.",
		devc->is_bluetooth, devc->req_text);

	/* Transmit the request. */
	len = serial_write_blocking(serial,
		devc->req_text, strlen(devc->req_text), WRITE_TO_MS);
	if (len < 0) {
		sr_err("Failed to send probe request.");
		return SR_ERR;
	}

	/* Receive a response. */
	len = serial_read_blocking(serial, devc->buf, TC_POLL_LEN, PROBE_TO_MS);
	if (len != TC_POLL_LEN) {
		sr_err("Failed to read probe response.");
		return SR_ERR;
	}

	if (process_poll_pkt(devc, poll_pkt) != SR_OK) {
		sr_err("Unrecognized TC device!");
		return SR_ERR;
	}

	devc->channels = rdtech_tc_channels;
	devc->channel_count = ARRAY_SIZE(rdtech_tc_channels);
	devc->dev_info.model_name = g_strndup((const char *)&poll_pkt[OFF_MODEL], LEN_MODEL);
	devc->dev_info.fw_ver = g_strndup((const char *)&poll_pkt[OFF_FW_VER], LEN_FW_VER);
	devc->dev_info.serial_num = read_u32le(&poll_pkt[OFF_SERIAL]);

	return SR_OK;
}

SR_PRIV int rdtech_tc_poll(const struct sr_dev_inst *sdi, gboolean force)
{
	struct dev_context *devc;
	int64_t now, elapsed;
	struct sr_serial_dev_inst *serial;
	int len;

	/*
	 * Don't send the request while receive data is being accumulated.
	 * Defer request transmission when a previous request has not yet
	 * seen any response data at all (more probable to happen shortly
	 * after connecting to the peripheral).
	 */
	devc = sdi->priv;
	if (!force) {
		if (devc->rdlen)
			return SR_OK;
		if (!devc->rx_after_tx)
			return SR_OK;
	}

	/*
	 * Send the request when the transmit interval was reached. Or
	 * when the caller forced the transmission.
	 */
	now = g_get_monotonic_time() / 1000;
	elapsed = now - devc->cmd_sent_at;
	if (!force && elapsed < POLL_PERIOD_MS)
		return SR_OK;

	/*
	 * Transmit another measurement request. Only advance the
	 * interval after successful transmission.
	 */
	serial = sdi->conn;
	len = serial_write_blocking(serial,
		devc->req_text, strlen(devc->req_text), WRITE_TO_MS);
	if (len < 0) {
		sr_err("Unable to send poll request.");
		return SR_ERR;
	}
	devc->cmd_sent_at = now;
	devc->rx_after_tx = 0;

	return SR_OK;
}

/*
 * Send a single analog sample for one channel. Uses PXView's native
 * sr_datafeed_analog struct directly (no sr_analog_init/meaning/encoding/spec
 * which PXView does not provide). The scale rational is applied manually.
 */
static int send_channel_value(const struct sr_dev_inst *sdi,
		const struct rdtech_tc_channel_desc *pch, float raw_value,
		size_t ch_idx)
{
	struct sr_datafeed_analog analog;
	struct sr_datafeed_packet packet;
	struct sr_channel *ch;
	float value;

	/* Apply the rational scale: value = raw * p / q. */
	value = raw_value * (float)pch->scale.p / (float)pch->scale.q;

	ch = g_slist_nth_data(sdi->channels, ch_idx);
	if (!ch)
		return SR_ERR_BUG;

	memset(&analog, 0, sizeof(analog));
	analog.probes = g_slist_append(NULL, ch);
	analog.num_samples = 1;
	analog.data = &value;
	analog.mq = pch->mq;
	analog.unit = pch->unit;
	analog.mqflags = 0;
	analog.unit_bits = 32;  /* float */
	analog.unit_pitch = 0;

	memset(&packet, 0, sizeof(packet));
	packet.type = SR_DF_ANALOG;
	packet.status = SR_PKT_OK;
	packet.payload = &analog;
	sr_session_send(sdi, &packet);

	g_slist_free(analog.probes);
	return SR_OK;
}

static int handle_poll_data(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	uint8_t poll_pkt[TC_POLL_LEN];
	size_t ch_idx;
	const struct rdtech_tc_channel_desc *pch;
	int ret;
	float v;

	devc = sdi->priv;
	sr_spew("Received poll packet (len: %zu).", devc->rdlen);
	if (devc->rdlen < TC_POLL_LEN) {
		sr_err("Insufficient poll packet length: %zu", devc->rdlen);
		return SR_ERR_DATA;
	}

	if (process_poll_pkt(devc, poll_pkt) != SR_OK) {
		sr_err("Failed to process poll packet.");
		return SR_ERR_DATA;
	}

	ret = SR_OK;
	std_session_send_df_frame_begin(sdi);
	for (ch_idx = 0; ch_idx < devc->channel_count; ch_idx++) {
		pch = &devc->channels[ch_idx];
		ret = bv_get_value_len(&v, &pch->spec, poll_pkt, TC_POLL_LEN);
		if (ret != SR_OK)
			break;
		ret = send_channel_value(sdi, pch, v, ch_idx);
		if (ret != SR_OK)
			break;
	}
	std_session_send_df_frame_end(sdi);

	sr_sw_limits_update_frames_read(&devc->limits, 1);
	if (sr_sw_limits_check(&devc->limits))
		sr_dev_acquisition_stop(sdi);

	return ret;
}

static int recv_poll_data(struct sr_dev_inst *sdi, struct sr_serial_dev_inst *serial)
{
	struct dev_context *devc;
	size_t space;
	int len;
	int ret;

	/* Receive data became available. Drain the transport layer. */
	devc = sdi->priv;
	while (devc->rdlen < TC_POLL_LEN) {
		space = sizeof(devc->buf) - devc->rdlen;
		len = serial_read_nonblocking(serial,
			&devc->buf[devc->rdlen], space);
		if (len < 0)
			return SR_ERR_IO;
		if (len == 0)
			return SR_OK;
		devc->rdlen += len;
		devc->rx_after_tx += len;
	}

	/*
	 * TODO Want to (re-)synchronize to the packet stream? The
	 * 'pac1' string literal would be a perfect match for that.
	 */

	/* Process packets when their reception has completed. */
	while (devc->rdlen >= TC_POLL_LEN) {
		ret = handle_poll_data(sdi);
		if (ret != SR_OK)
			return ret;
		devc->rdlen -= TC_POLL_LEN;
		if (devc->rdlen)
			memmove(devc->buf, &devc->buf[TC_POLL_LEN], devc->rdlen);
	}

	return SR_OK;
}

SR_PRIV int rdtech_tc_receive_data(int fd, int revents, const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
	int ret;

	(void)fd;

	if (!sdi || !(devc = sdi->priv))
		return TRUE;

	/* Handle availability of receive data. */
	serial = sdi->conn;
	if (revents == G_IO_IN) {
		ret = recv_poll_data(sdi, serial);
		if (ret != SR_OK)
			sr_dev_acquisition_stop(sdi);
	}

	/* Check configured acquisition limits. */
	if (sr_sw_limits_check(&devc->limits)) {
		sr_dev_acquisition_stop(sdi);
		return TRUE;
	}

	/* Periodically retransmit measurement requests. */
	(void)rdtech_tc_poll(sdi, FALSE);

	return TRUE;
}
