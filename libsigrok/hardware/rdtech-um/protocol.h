/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2018-2020 Andreas Sandberg <andreas@sandberg.pp.se>
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

#ifndef LIBSIGROK_HARDWARE_RDTECH_UM_PROTOCOL_H
#define LIBSIGROK_HARDWARE_RDTECH_UM_PROTOCOL_H

#include "hardware/compat/compat.h"
#include <string.h>

#undef LOG_PREFIX
#define LOG_PREFIX "rdtech-um"

/*
 * RDTech UM drivers are energy meters. PXView does not define
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

#define RDTECH_UM_BUFSIZE 256

/* ===== Local sr_sw_limits (with limit_frames support) ===================
 *
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers. The rdtech-um driver uses SR_CONF_LIMIT_FRAMES
 * (not LIMIT_SAMPLES), so the local implementation must include
 * limit_frames / frames_read fields.
 */
#ifndef RDTECH_UM_SR_SW_LIMITS_DEFINED
#define RDTECH_UM_SR_SW_LIMITS_DEFINED
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
#endif /* RDTECH_UM_SR_SW_LIMITS_DEFINED */

/* ===== Local binary_value_spec replacement ==============================
 *
 * PXView does not provide struct binary_value_spec or bv_get_value_len.
 * Define them locally so the original channel descriptor tables and
 * protocol parsing logic compile with minimal changes.
 */
#ifndef RDTECH_UM_BVS_DEFINED
#define RDTECH_UM_BVS_DEFINED
enum rdtech_um_binary_value_type {
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
	enum rdtech_um_binary_value_type type;
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
#endif /* RDTECH_UM_BVS_DEFINED */

/* ===== Local read_u8 helper ============================================= */
#ifndef RDTECH_UM_READ_U8_DEFINED
#define RDTECH_UM_READ_U8_DEFINED
static inline uint8_t read_u8(const uint8_t *p)
{
	return p[0];
}
#endif

/* ===== Local std_session_send_df_frame_begin/end ========================
 * PXView only provides std_session_send_df_header/end (with prefix arg),
 * not the frame variants. Same pattern as rdtech-tc, itech-it8500, etc.
 */
SR_PRIV int rdtech_um_std_session_send_df_frame_begin(const struct sr_dev_inst *sdi);
SR_PRIV int rdtech_um_std_session_send_df_frame_end(const struct sr_dev_inst *sdi);
#define std_session_send_df_frame_begin(sdi) rdtech_um_std_session_send_df_frame_begin(sdi)
#define std_session_send_df_frame_end(sdi) rdtech_um_std_session_send_df_frame_end(sdi)

/* ===== Local sr_rational (PXView does not define it) ==================== */
#ifndef RDTECH_UM_RATIONAL_DEFINED
#define RDTECH_UM_RATIONAL_DEFINED
struct sr_rational {
	uint64_t p;
	uint64_t q;
};
#endif

/* ===== Device structures =============================================== */

enum rdtech_um_model_id {
	RDTECH_UM24C = 0x0963,
	RDTECH_UM25C = 0x09c9,
	RDTECH_UM34C = 0x0d4c,
};

struct rdtech_um_channel_desc {
	const char *name;
	struct binary_value_spec spec;
	struct sr_rational scale;
	int digits;
	enum sr_mq mq;
	enum sr_unit unit;
};

struct rdtech_um_profile {
	const char *model_name;
	enum rdtech_um_model_id model_id;
	const struct rdtech_um_channel_desc *channels;
	size_t channel_count;
	gboolean (*csum_ok)(const uint8_t *buf, size_t len);
};

struct dev_context {
	const struct rdtech_um_profile *profile;
	struct sr_sw_limits limits;
	uint8_t buf[RDTECH_UM_BUFSIZE];
	size_t buflen;
	int64_t cmd_sent_at;
};

SR_PRIV const struct rdtech_um_profile *rdtech_um_probe(struct sr_serial_dev_inst *serial);
SR_PRIV int rdtech_um_receive_data(int fd, int revents, const struct sr_dev_inst *sdi);
SR_PRIV int rdtech_um_poll(const struct sr_dev_inst *sdi, gboolean force);

#endif
