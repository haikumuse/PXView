/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2021 Gerhard Sittig <gerhard.sittig@gmx.net>
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

#ifndef LIBSIGROK_HARDWARE_ASIX_OMEGA_RTM_CLI_PROTOCOL_H
#define LIBSIGROK_HARDWARE_ASIX_OMEGA_RTM_CLI_PROTOCOL_H

/* Rule 1: replace <config.h>/<libsigrok/libsigrok.h>/"libsigrok-internal.h"
 * with the PXView compat header. The compat header pulls in libsigrok's
 * internal types, the standard sigrok API aliases, and the local compat
 * helpers (config keys, byte-order macros, dev_inst/serial helpers, etc). */
#include "hardware/compat/compat.h"
#include <stdint.h>
#include <string.h>

#define LOG_PREFIX "asix-omega-rtm-cli"

#define RTMCLI_STDOUT_CHUNKSIZE (1024 * 1024)
#define FEED_QUEUE_DEPTH (256 * 1024)

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time.
 */
struct sr_sw_limits {
	uint64_t limit_samples;
	uint64_t limit_msec;
	int64_t starttime_ms;
	uint64_t samples_read;
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

/*
 * Local sr_sw_limits_get_remain() helper. Standard sigrok exposes this
 * from libsigrok-internal.h; PXView does not. The source driver uses it
 * in dev_acquisition_start() to query how many samples remain before the
 * user-specified limit is reached, so it can pre-compute the count to
 * enforce a stricter check while uncompressing the RLE stream. Only the
 * samples counter is consulted here (the source driver passes NULL for
 * frames/msecs/exceeded).
 */
static inline int sr_sw_limits_get_remain(const struct sr_sw_limits *limits,
		uint64_t *samples, uint64_t *frames, uint64_t *msecs,
		gboolean *exceeded)
{
	if (!limits)
		return SR_ERR_ARG;

	if (exceeded)
		*exceeded = FALSE;

	if (samples) {
		*samples = 0;
		if (limits->limit_samples) {
			if (limits->samples_read >= limits->limit_samples) {
				if (exceeded)
					*exceeded = TRUE;
			} else {
				*samples = limits->limit_samples - limits->samples_read;
			}
		}
	}

	if (frames)
		*frames = 0;

	if (msecs)
		*msecs = 0;

	return SR_OK;
}

/*
 * Local read_u16le_inc() helper. The compat layer's compat_config.h
 * provides read_u16le() (without _inc) and write_u16le(). The source
 * driver's RLE decompression logic walks the receive buffer with the
 * _inc variant, so define it locally as a static inline.
 */
static inline uint16_t read_u16le_inc(const uint8_t **ptr)
{
	uint16_t val;

	val = (uint16_t)(*ptr)[0];
	val |= ((uint16_t)(*ptr)[1]) << 8;
	*ptr += 2;
	return val;
}

/*
 * Forward declaration of the standard sigrok feed queue type.
 *
 * PXView's libsigrok does NOT provide the feed_queue_logic_* family of
 * functions (the only driver that uses them, kingst-la2016, is OFF by
 * default and would fail to link if enabled). This driver therefore
 * provides its own minimal local implementation in protocol.c, covering
 * only the four entry points that the RLE decompression path uses:
 * alloc/submit_one/flush/free. The struct is opaque to api.c (which only
 * holds a pointer in dev_context.samples.queue) so a forward declaration
 * is sufficient here. The local static functions live in protocol.c.
 */
struct feed_queue_logic;

/*
 * Local sr_hexdump helpers (simple space-separated hex for debug spew).
 *
 * PXView's libsigrok does not provide sr_hexdump_new()/sr_hexdump_free().
 * The source driver uses them for an sr_dbg() line in omega_rtm_cli_open()
 * to dump the spawned process' PID. Define local versions and redirect
 * the standard names to them via macros so the call site is unchanged.
 */
SR_PRIV GString *asix_omega_rtm_cli_sr_hexdump_new(const uint8_t *buf,
	size_t len);
SR_PRIV void asix_omega_rtm_cli_sr_hexdump_free(GString *gstr);
#define sr_hexdump_new(buf, len) \
	asix_omega_rtm_cli_sr_hexdump_new((buf), (len))
#define sr_hexdump_free(gstr) \
	asix_omega_rtm_cli_sr_hexdump_free(gstr)

struct dev_context {
	char **channel_names;
	struct sr_sw_limits limits;
	struct {
		gchar **argv;
		GSpawnFlags flags;
		gboolean running;
		GPid pid;
		gint fd_stdin_write;
		gint fd_stdout_read;
	} child;
	struct {
		uint8_t buff[RTMCLI_STDOUT_CHUNKSIZE];
		size_t fill;
	} rawdata;
	struct {
		struct feed_queue_logic *queue;
		uint8_t last_sample[sizeof(uint16_t)];
		uint64_t remain_count;
		gboolean check_count;
	} samples;
};

SR_PRIV int omega_rtm_cli_open(const struct sr_dev_inst *sdi);
SR_PRIV int omega_rtm_cli_close(const struct sr_dev_inst *sdi);

/*
 * Rule 14: PXView's sr_receive_data_callback_t passes the sdi directly
 * as the third argument (const struct sr_dev_inst *sdi) instead of the
 * void *cb_data that standard sigrok uses.
 */
SR_PRIV int omega_rtm_cli_receive_data(int fd, int revents,
		const struct sr_dev_inst *sdi);

#endif
