/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2023 Mathieu Pilato <pilato.mathieu@free.fr>
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

#ifndef LIBSIGROK_HARDWARE_ATORCH_PROTOCOL_H
#define LIBSIGROK_HARDWARE_ATORCH_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "atorch"

#define ATORCH_BUFSIZE	128

/*
 * PXView's libsigrok.h does not define several standard sigrok device-type
 * config keys that this driver advertises in drvopts[]. Provide them here
 * with values consistent with other compat drivers:
 *  - SR_CONF_ENERGYMETER  (teleinfo/rdtech-um/rdtech-tc use 10007)
 *  - SR_CONF_POWERMETER    (10008, adjacent gap)
 *  - SR_CONF_ELECTRONIC_LOAD (arachnid-labs-re-load-pro/itech-it8500/
 *                             maynuo-m97/siglent-sdl10x0 use 10009)
 */
#ifndef SR_CONF_ENERGYMETER
#define SR_CONF_ENERGYMETER 10007
#endif
#ifndef SR_CONF_POWERMETER
#define SR_CONF_POWERMETER 10008
#endif
#ifndef SR_CONF_ELECTRONIC_LOAD
#define SR_CONF_ELECTRONIC_LOAD 10009
#endif

/*
 * PXView's libsigrok.h does not define SR_MQ_ENERGY, SR_UNIT_WATT_HOUR or
 * SR_UNIT_AMPERE_HOUR. rdtech-um/rdtech-tc define SR_MQ_ENERGY=10015 and
 * SR_UNIT_WATT_HOUR=10018 (PXView's SR_MQ enum ends at SR_MQ_RELATIVE_
 * HUMIDITY=10014, SR_UNIT enum ends at SR_UNIT_CONCENTRATION=10016, so
 * 10015/10018/10019 are free). Reuse the same values for consistency.
 */
#ifndef SR_MQ_ENERGY
#define SR_MQ_ENERGY 10015
#endif
#ifndef SR_UNIT_WATT_HOUR
#define SR_UNIT_WATT_HOUR 10018
#endif
#ifndef SR_UNIT_AMPERE_HOUR
#define SR_UNIT_AMPERE_HOUR 10019
#endif

/* ===== Local sr_rational (PXView does not define it) ====================
 * Same definition as rdtech-um/rdtech-tc: uint64_t numerator/denominator.
 * The original sigrok driver used double literals (1e3) for q; PXView's
 * local struct uses uint64_t, so the channel tables below use integer
 * literals (1000) instead.
 */
#ifndef ATORCH_SR_RATIONAL_DEFINED
#define ATORCH_SR_RATIONAL_DEFINED
struct sr_rational {
	uint64_t p;
	uint64_t q;
};
#endif

/* ===== Local sr_sw_limits (with limit_frames support) ===================
 *
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers. The atorch driver uses SR_CONF_LIMIT_FRAMES
 * (not LIMIT_SAMPLES) as well as LIMIT_MSEC, so the local implementation
 * must include limit_frames / frames_read fields.
 *
 * Guarded with #ifndef SR_SW_LIMITS_H per the migration spec so that
 * re-inclusion within a single translation unit is a no-op.
 */
#ifndef SR_SW_LIMITS_H
#define SR_SW_LIMITS_H
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
	if (!limits)
		return;
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
#endif /* SR_SW_LIMITS_H */

/* ===== Local binary_value_spec replacement ==============================
 *
 * PXView does not provide struct binary_value_spec or bv_get_value.
 * Define them locally so the original channel descriptor tables and
 * protocol parsing logic compile with minimal changes. The atorch driver
 * uses BVT_BE_UINT16, BVT_BE_UINT24 and BVT_BE_UINT32 (the 24-bit variant
 * is atorch-specific and not present in rdtech-um/rdtech-tc).
 */
#ifndef ATORCH_BVS_DEFINED
#define ATORCH_BVS_DEFINED
enum atorch_binary_value_type {
	BVT_BE_UINT16,
	BVT_BE_UINT24,
	BVT_BE_UINT32,
};

struct binary_value_spec {
	size_t offset;
	enum atorch_binary_value_type type;
};

/*
 * Decode a binary value from the report payload into a float. Matches the
 * original sigrok bv_get_value() 3-arg signature (no dlen parameter): the
 * caller (parse_report_msg) always passes a full 36-byte report message,
 * and all channel descriptors reference offsets within that buffer, so no
 * bounds check is needed beyond a non-NULL guard.
 */
static inline int bv_get_value(float *value,
		const struct binary_value_spec *spec,
		const uint8_t *data)
{
	if (!value || !spec || !data)
		return SR_ERR_ARG;

	switch (spec->type) {
	case BVT_BE_UINT16:
		*value = (float)read_u16be(&data[spec->offset]);
		break;
	case BVT_BE_UINT24:
		*value = (float)((uint32_t)data[spec->offset] << 16 |
				(uint32_t)data[spec->offset + 1] << 8 |
				(uint32_t)data[spec->offset + 2]);
		break;
	case BVT_BE_UINT32:
		*value = (float)read_u32be(&data[spec->offset]);
		break;
	default:
		return SR_ERR;
	}

	return SR_OK;
}
#endif /* ATORCH_BVS_DEFINED */

/* ===== Local sr_hexdump helpers =========================================
 *
 * PXView's libsigrok does not provide sr_hexdump_new()/sr_hexdump_free().
 * The declarations and #define redirects live here; the implementations
 * live in protocol.c (same pattern as asix-omega-rtm-cli/microchip-
 * pickit2/rdtech-tc).
 */
#ifndef ATORCH_SR_HEXDUMP_DEFINED
#define ATORCH_SR_HEXDUMP_DEFINED
SR_PRIV GString *atorch_sr_hexdump_new(const uint8_t *buf, size_t len);
SR_PRIV void atorch_sr_hexdump_free(GString *gstr);
#define sr_hexdump_new(buf, len) atorch_sr_hexdump_new((buf), (len))
#define sr_hexdump_free(gstr) atorch_sr_hexdump_free(gstr)
#endif

/* ===== Local feed_queue_analog ==========================================
 *
 * PXView's libsigrok does not provide the feed_queue_analog_* family that
 * standard sigrok exposes via libsigrok-internal.h. The atorch driver uses
 * per-channel feed queues (one float sample per frame per channel), so a
 * local implementation is provided in protocol.c. flush() emits a single
 * SR_DF_ANALOG packet using PXView's flat struct sr_datafeed_analog
 * (analog.probes / analog.num_samples / analog.data / analog.mq /
 * analog.unit / analog.mqflags), applying the rational scale at emit time
 * (same approach as rdtech-um's send_channel_value).
 *
 * The struct layout follows the rdtech-um pattern (sdi + cap_samples +
 * count_samples + data buffer), extended with the channel pointer and the
 * mq/unit/mqflags/scale/digits metadata that the atorch channel descriptor
 * tables carry.
 */
/*
 * PXView's libsigrok.h defines SR_MQ_* / SR_UNIT_* / SR_MQFLAG_* as
 * anonymous enum values (not tagged enums), and struct sr_datafeed_analog
 * stores them as int / int / uint64_t. Mirror that here so this header
 * compiles without a local "enum sr_mq/sr_unit/sr_mqflag" tag declaration
 * (same approach as fluke-dmm/protocol.h).
 */
struct feed_queue_analog {
	const struct sr_dev_inst *sdi;
	struct sr_channel *channel;
	size_t unit_size;	/* sizeof(float) */
	size_t cap_samples;	/* max samples the buffer can hold */
	size_t count_samples;	/* samples currently buffered */
	uint8_t *data;		/* flat float buffer */

	int mq;
	int unit;
	uint64_t mqflags;
	struct sr_rational scale;
	int digits;
};

SR_PRIV struct feed_queue_analog *feed_queue_analog_alloc(
	const struct sr_dev_inst *sdi, size_t sample_count,
	int digits, struct sr_channel *ch);
SR_PRIV void feed_queue_analog_mq_unit(struct feed_queue_analog *q,
	int mq, uint64_t flags, int unit);
SR_PRIV void feed_queue_analog_scale_offset(struct feed_queue_analog *q,
	const struct sr_rational *scale, const struct sr_rational *offset);
SR_PRIV int feed_queue_analog_submit_one(struct feed_queue_analog *q,
	float value, size_t sample_count);
SR_PRIV int feed_queue_analog_flush(struct feed_queue_analog *q);
SR_PRIV void feed_queue_analog_free(struct feed_queue_analog *q);

/* ===== Device structures =============================================== */

/*
 * PXView's libsigrok.h defines SR_MQ_* / SR_UNIT_* / SR_MQFLAG_* as anonymous
 * enum values (no tagged enum sr_mq/sr_unit/sr_mqflag type). Store them as
 * int / int / uint64_t here, matching struct sr_datafeed_analog in
 * libsigrok.h and the fluke-dmm/protocol.h pattern. This lets the channel
 * descriptor tables below compile and assign SR_MQ_VOLTAGE etc. (anonymous
 * enum int values) to these fields without an explicit enum tag.
 */
struct atorch_channel_desc {
	const char *name;
	struct binary_value_spec spec;
	struct sr_rational scale;
	int digits;
	int mq;
	int unit;
	uint64_t flags;
};

struct atorch_device_profile {
	uint8_t device_type;
	const char *device_name;
	const struct atorch_channel_desc *channels;
	size_t channel_count;
};

enum atorch_msg_type {
	MSG_REPORT = 0x01,
	MSG_REPLY = 0x02,
	MSG_COMMAND = 0x11,
};

struct dev_context {
	const struct atorch_device_profile *profile;
	struct sr_sw_limits limits;
	struct feed_queue_analog **feeds;
	uint8_t buf[ATORCH_BUFSIZE];
	size_t wr_idx;
	size_t rd_idx;
};

/*
 * PXView's sr_receive_data_callback_t is typed as
 *   int (*)(int fd, int revents, const struct sr_dev_inst *sdi)
 * (see libsigrok-internal.h). The original sigrok source used
 *   int (int fd, int revents, void *cb_data)
 * so the signature is adapted here: the opaque cb_data is replaced by a
 * typed const sdi, and the body no longer needs to dereference cb_data.
 */
SR_PRIV int atorch_probe(struct sr_serial_dev_inst *serial,
	struct dev_context *devc);
SR_PRIV int atorch_receive_data_callback(int fd, int revents,
	const struct sr_dev_inst *sdi);

#endif
