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

#ifndef LIBSIGROK_HARDWARE_RDTECH_TC_PROTOCOL_H
#define LIBSIGROK_HARDWARE_RDTECH_TC_PROTOCOL_H

#include "hardware/compat/compat.h"
#include <string.h>

#undef LOG_PREFIX
#define LOG_PREFIX "rdtech-tc"

/*
 * RDTech TC/UM drivers are energy meters. PXView does not define
 * SR_CONF_ENERGYMETER (the teleinfo compat driver defines it as 10007;
 * use the same value for consistency).
 */
#ifndef SR_CONF_ENERGYMETER
#define SR_CONF_ENERGYMETER 10007
#endif

/*
 * PXView's libsigrok.h does not define SR_MQ_ENERGY or SR_UNIT_WATT_HOUR.
 * The teleinfo compat driver defines them as 10015 and 10018 respectively;
 * reuse the same values for consistency. PXView's SR_MQ enum ends at
 * SR_MQ_RELATIVE_HUMIDITY (10014) so 10015 is free; PXView's SR_UNIT enum
 * ends at SR_UNIT_CONCENTRATION (10016) so 10018 is free.
 */
#ifndef SR_MQ_ENERGY
#define SR_MQ_ENERGY 10015
#endif
#ifndef SR_UNIT_WATT_HOUR
#define SR_UNIT_WATT_HOUR 10018
#endif

/*
 * Keep request and response buffers of sufficient size. The maximum
 * request text currently involved is "bgetva\r\n" which translates
 * to 9 bytes. The poll response (a measurement, the largest amount
 * of data that is currently received) is 192 bytes in length. Add
 * some slack for alignment, and for in-flight messages or adjacent
 * data during synchronization to the data stream.
 */
#define RDTECH_TC_MAXREQLEN 12
#define RDTECH_TC_RSPBUFSIZE 256

/* ===== Local sr_sw_limits (with limit_frames support) ===================
 *
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers. The rdtech-tc driver uses SR_CONF_LIMIT_FRAMES
 * (not LIMIT_SAMPLES), so the local implementation must include
 * limit_frames / frames_read fields.
 */
#ifndef RDTECH_TC_SR_SW_LIMITS_DEFINED
#define RDTECH_TC_SR_SW_LIMITS_DEFINED
struct sr_sw_limits {
	uint64_t limit_samples;
	uint64_t limit_msec;
	uint64_t limit_frames;
	int64_t starttime_ms;
	uint64_t samples_read;
	uint64_t frames_read;
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
	case SR_CONF_LIMIT_FRAMES:
		*data = g_variant_new_uint64(limits->limit_frames);
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
	case SR_CONF_LIMIT_FRAMES:
		limits->limit_frames = g_variant_get_uint64(data);
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
	limits->frames_read = 0;
}

static inline void sr_sw_limits_update_samples_read(struct sr_sw_limits *limits,
		uint64_t count)
{
	if (!limits)
		return;
	limits->samples_read += count;
}

static inline void sr_sw_limits_update_frames_read(struct sr_sw_limits *limits,
		uint64_t count)
{
	if (!limits)
		return;
	limits->frames_read += count;
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

	if (limits->limit_frames && limits->frames_read >= limits->limit_frames)
		return TRUE;

	return FALSE;
}
#endif /* RDTECH_TC_SR_SW_LIMITS_DEFINED */

/* ===== Local binary_value_spec replacement ==============================
 *
 * PXView does not provide struct binary_value_spec or bv_get_value_len.
 * Define them locally so the original channel descriptor tables and
 * protocol parsing logic compile with minimal changes.
 */
#ifndef RDTECH_TC_BVS_DEFINED
#define RDTECH_TC_BVS_DEFINED
enum rdtech_tc_binary_value_type {
	BVT_BE_UINT16,
	BVT_BE_UINT32,
	BVT_BE_INT16,
	BVT_LE_UINT16,
	BVT_LE_UINT32,
	BVT_LE_INT16,
	BVT_BE_FLOAT,
	BVT_LE_FLOAT,
};

struct binary_value_spec {
	size_t offset;
	enum rdtech_tc_binary_value_type type;
};

static inline int bv_get_value_len(float *value,
		const struct binary_value_spec *spec,
		const uint8_t *data, size_t dlen)
{
	if (!value || !spec || !data)
		return SR_ERR_ARG;

	switch (spec->type) {
	case BVT_BE_UINT16:
		if (spec->offset + 2 > dlen)
			return SR_ERR_DATA;
		*value = (float)read_u16be(&data[spec->offset]);
		break;
	case BVT_BE_UINT32:
		if (spec->offset + 4 > dlen)
			return SR_ERR_DATA;
		*value = (float)read_u32be(&data[spec->offset]);
		break;
	case BVT_LE_UINT16:
		if (spec->offset + 2 > dlen)
			return SR_ERR_DATA;
		*value = (float)read_u16le(&data[spec->offset]);
		break;
	case BVT_LE_UINT32:
		if (spec->offset + 4 > dlen)
			return SR_ERR_DATA;
		*value = (float)read_u32le(&data[spec->offset]);
		break;
	case BVT_BE_INT16:
		if (spec->offset + 2 > dlen)
			return SR_ERR_DATA;
		*value = (float)read_i16be(&data[spec->offset]);
		break;
	case BVT_LE_INT16:
		if (spec->offset + 2 > dlen)
			return SR_ERR_DATA;
		*value = (float)read_i16le(&data[spec->offset]);
		break;
	default:
		return SR_ERR;
	}

	return SR_OK;
}
#endif /* RDTECH_TC_BVS_DEFINED */

/* ===== Local read_u8 helper ============================================= */
#ifndef RDTECH_TC_READ_U8_DEFINED
#define RDTECH_TC_READ_U8_DEFINED
static inline uint8_t read_u8(const uint8_t *p)
{
	return p[0];
}
#endif

/* ===== Local sr_crc16 (PXView does not provide it) ====================== */
#ifndef SR_CRC16_DEFAULT_INIT
#define SR_CRC16_DEFAULT_INIT 0xFFFF
#endif
SR_PRIV uint16_t rdtech_tc_sr_crc16(uint16_t crc, const uint8_t *buf, size_t len);
#define sr_crc16(init, buf, len) rdtech_tc_sr_crc16((init), (buf), (len))

/* ===== Local sr_hexdump helpers (for debug spew) ======================== */
SR_PRIV GString *rdtech_tc_sr_hexdump_new(const uint8_t *buf, size_t len);
SR_PRIV void rdtech_tc_sr_hexdump_free(GString *gstr);
#define sr_hexdump_new(buf, len) rdtech_tc_sr_hexdump_new((buf), (len))
#define sr_hexdump_free(gstr) rdtech_tc_sr_hexdump_free(gstr)

/* ===== Local ser_name_is_bt (Bluetooth serial detection) =============== */
SR_PRIV gboolean rdtech_tc_ser_name_is_bt(struct sr_serial_dev_inst *serial);
#define ser_name_is_bt(serial) rdtech_tc_ser_name_is_bt(serial)

/* ===== Local std_session_send_df_frame_begin/end ======================== */
SR_PRIV int rdtech_tc_std_session_send_df_frame_begin(const struct sr_dev_inst *sdi);
SR_PRIV int rdtech_tc_std_session_send_df_frame_end(const struct sr_dev_inst *sdi);
#define std_session_send_df_frame_begin(sdi) rdtech_tc_std_session_send_df_frame_begin(sdi)
#define std_session_send_df_frame_end(sdi) rdtech_tc_std_session_send_df_frame_end(sdi)

/* ===== AES-256-ECB decryption (replaces nettle/aes.h) ==================
 *
 * The rdtech-tc protocol encrypts poll responses with AES-256-ECB using
 * a fixed key. PXView does not link against nettle, so a self-contained
 * AES-256 implementation is provided in protocol.c.
 */
struct aes256_ctx {
	uint8_t round_keys[240]; /* 15 round keys * 16 bytes */
};

SR_PRIV void rdtech_tc_aes256_set_decrypt_key(struct aes256_ctx *ctx,
		const uint8_t *key);
SR_PRIV void rdtech_tc_aes256_decrypt(struct aes256_ctx *ctx, size_t length,
		uint8_t *dst, const uint8_t *src);
#define aes256_set_decrypt_key(ctx, key) rdtech_tc_aes256_set_decrypt_key((ctx), (key))
#define aes256_decrypt(ctx, len, dst, src) rdtech_tc_aes256_decrypt((ctx), (len), (dst), (src))

/* ===== Local sr_rational (PXView does not define it) ==================== */
#ifndef RDTECH_TC_RATIONAL_DEFINED
#define RDTECH_TC_RATIONAL_DEFINED
struct sr_rational {
	uint64_t p;
	uint64_t q;
};
#endif

/* ===== Device structures =============================================== */

struct rdtech_dev_info {
	char *model_name;
	char *fw_ver;
	uint32_t serial_num;
};

struct rdtech_tc_channel_desc {
	const char *name;
	struct binary_value_spec spec;
	struct sr_rational scale;
	int digits;
	enum sr_mq mq;
	enum sr_unit unit;
};

struct dev_context {
	gboolean is_bluetooth;
	char req_text[RDTECH_TC_MAXREQLEN];
	struct rdtech_dev_info dev_info;
	const struct rdtech_tc_channel_desc *channels;
	size_t channel_count;
	struct sr_sw_limits limits;
	uint8_t buf[RDTECH_TC_RSPBUFSIZE];
	size_t rdlen;
	int64_t cmd_sent_at;
	size_t rx_after_tx;
};

SR_PRIV int rdtech_tc_probe(struct sr_serial_dev_inst *serial, struct dev_context *devc);
SR_PRIV int rdtech_tc_receive_data(int fd, int revents, const struct sr_dev_inst *sdi);
SR_PRIV int rdtech_tc_poll(const struct sr_dev_inst *sdi, gboolean force);

#endif
