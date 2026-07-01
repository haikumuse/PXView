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
 * PXView port of the libsigrok "serial-lcr" driver.
 *
 * The upstream driver is split across src/hardware/serial-lcr/ (the
 * device driver shell: api.c/protocol.c/protocol.h) and src/lcr/
 * (the chipset-specific packet parsers es51919.c and vc4080.c, plus
 * the shared struct lcr_parse_info and the ES51919_ and VC4080_ macro families
 * that live in libsigrok-internal.h). PXView does not ship any of
 * those, so the chipset parsers are inlined into protocol.c below
 * and the missing macros/types are defined here.
 *
 * PXView does not use SR_REGISTER_DEV_DRIVER_LIST. The original
 * registered two driver lists (lcr_es51919_drivers with 4 device
 * variants, lcr_vc4080_drivers with 2 device variants). PXView
 * registers a single `serial_lcr_driver_info`; scan() walks a
 * static `lcr_info[]` table (one entry per device variant) and
 * probes each chipset against the serial port until one matches.
 */

#ifndef LIBSIGROK_HARDWARE_SERIAL_LCR_PROTOCOL_H
#define LIBSIGROK_HARDWARE_SERIAL_LCR_PROTOCOL_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "serial-lcr"

/*
 * PXView's libsigrok.h does not define the LCR-meter device class or
 * the LCR-specific config keys. Provide them locally with values that
 * do not clash with the upstream anonymous-enum ranges. Other compat
 * drivers (rohde-schwarz-sme-0x, rigol-dg, scpi-pps) already define
 * SR_CONF_OUTPUT_FREQUENCY locally as 30201 -- reuse the same value.
 */
#ifndef SR_CONF_LCRMETER
#define SR_CONF_LCRMETER                10010
#endif
#ifndef SR_CONF_OUTPUT_FREQUENCY
#define SR_CONF_OUTPUT_FREQUENCY        30201
#endif
#ifndef SR_CONF_EQUIV_CIRCUIT_MODEL
#define SR_CONF_EQUIV_CIRCUIT_MODEL     30203
#endif

/* ===== Local sr_sw_limits (with limit_frames support) ===================
 *
 * PXView does not provide struct sr_sw_limits or the sr_sw_limits_*
 * helpers. The serial-lcr driver uses SR_CONF_LIMIT_FRAMES as well as
 * SR_CONF_LIMIT_MSEC, so the local implementation needs the
 * limit_frames / frames_read fields (same layout as the atorch driver).
 * Guarded so re-inclusion within a single translation unit is a no-op.
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

/*
 * Shared parse context for the LCR chipset parsers. Kept verbatim from
 * the upstream libsigrok-internal.h definition. The mq/unit/mqflags are
 * NOT stored here -- they live on the per-packet sr_datafeed_analog.
 */
struct lcr_parse_info {
	size_t ch_idx;
	uint64_t output_freq;
	const char *circuit_model;
};

/* ===== ES51919 chipset constants ======================================= */
#define ES51919_PACKET_SIZE	17
#define ES51919_CHANNEL_COUNT	2
#define ES51919_COMM_PARAM	"9600/8n1/rts=1/dtr=1"

/* ===== VC4080 chipset constants ======================================== */
#define VC4080_PACKET_SIZE	39
#define VC4080_COMM_PARAM	"1200/8n1"

/*
 * VC4080 display indices. The D/Q auxiliary channels are compiled out
 * (VC4080_WITH_DQ_CHANS=0) to match the upstream default, which only
 * exposes the primary and secondary displays.
 */
#define VC4080_WITH_DQ_CHANS	0

enum {
	VC4080_DISPLAY_PRIMARY,
	VC4080_DISPLAY_SECONDARY,
#if VC4080_WITH_DQ_CHANS
	VC4080_DISPLAY_D_VALUE,
	VC4080_DISPLAY_Q_VALUE,
#endif
	VC4080_CHANNEL_COUNT,
};

/*
 * Per-device-variant descriptor. In upstream libsigrok the first field
 * was `struct sr_dev_driver di` (because each variant was its own
 * registered driver and scan() recovered the descriptor from `di` via
 * a cast). PXView registers a single driver, so the descriptor is now
 * a plain data table entry referenced from scan() and stored on
 * dev_context.
 */
struct lcr_info {
	const char *vendor;
	const char *model;
	const char *comm;
	size_t channel_count;
	const char **channel_formats;
	size_t packet_size;
	int64_t req_timeout_ms;
	int (*packet_request)(struct sr_serial_dev_inst *serial);
	gboolean (*packet_valid)(const uint8_t *pkt);
	int (*packet_parse)(const uint8_t *pkt, float *value,
		struct sr_datafeed_analog *analog, void *info);
	int (*config_get)(uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi,
		const struct sr_channel_group *cg);
	int (*config_set)(uint32_t key, GVariant *data,
		const struct sr_dev_inst *sdi,
		const struct sr_channel_group *cg);
	int (*config_list)(uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi,
		const struct sr_channel_group *cg);
};

#define LCR_BUFSIZE	128

struct dev_context {
	const struct lcr_info *lcr_info;
	struct sr_sw_limits limits;
	uint8_t buf[LCR_BUFSIZE];
	size_t buf_rxpos;
	struct lcr_parse_info parse_info;
	uint64_t output_freq;
	const char *circuit_model;
	int64_t req_next_at;
};

/*
 * PXView's sr_receive_data_callback_t is
 *   int (*)(int fd, int revents, const struct sr_dev_inst *sdi)
 * (see libsigrok-internal.h). The original sigrok source used
 *   int (int fd, int revents, void *cb_data)
 * so the signature is adapted: cb_data is replaced by a typed const sdi
 * and the body no longer dereferences cb_data.
 */
SR_PRIV int lcr_receive_data(int fd, int revents, const struct sr_dev_inst *sdi);

/* Local acquisition stop (PXView lacks std_serial_dev_acquisition_stop). */
SR_PRIV int serial_lcr_dev_acquisition_stop(const struct sr_dev_inst *sdi);

/* Chipset parser entry points (implemented in protocol.c). */
SR_PRIV gboolean es51919_packet_valid(const uint8_t *pkt);
SR_PRIV int es51919_packet_parse(const uint8_t *pkt, float *val,
		struct sr_datafeed_analog *analog, void *info);
SR_PRIV int es51919_config_list(uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg);

SR_PRIV int vc4080_packet_request(struct sr_serial_dev_inst *serial);
SR_PRIV gboolean vc4080_packet_valid(const uint8_t *pkt);
SR_PRIV int vc4080_packet_parse(const uint8_t *pkt, float *val,
		struct sr_datafeed_analog *analog, void *info);
SR_PRIV int vc4080_config_list(uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg);

extern SR_PRIV const char *vc4080_channel_formats[VC4080_CHANNEL_COUNT];

#endif
