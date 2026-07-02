/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2019 Katherine J. Temkin <k@ktemkin.com>
 * Copyright (C) 2019 Mikaela Szekely <qyriad@gmail.com>
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

#ifndef LIBSIGROK_HARDWARE_GREATFET_PROTOCOL_H
#define LIBSIGROK_HARDWARE_GREATFET_PROTOCOL_H

#include <glib.h>
#include <libusb.h>
#include <stdint.h>
#include <string.h>

#include "hardware/compat/compat.h"

/*
 * usb_source_remove compat - PXView's libsigrok does not provide the
 * standard sigrok usb_source_remove() helper. The arguments (session,
 * ctx) are discarded; sr_session_source_remove(-1) removes the timer
 * source that compat_usb_source_add() registered with fd=-1. See
 * sysclk-lwla/protocol.h and ikalogic-scanalogic2/protocol.h for the
 * same pattern.
 */
#ifndef usb_source_remove
#define usb_source_remove(session, ctx) sr_session_source_remove(-1)
#endif

#undef LOG_PREFIX
#define LOG_PREFIX "greatfet"

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time.
 */
#ifndef GREATFET_SR_SW_LIMITS_DEFINED
#define GREATFET_SR_SW_LIMITS_DEFINED
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
 * from libsigrok-internal.h; PXView does not. The greatfet driver uses
 * it in greatfet_process_receive_data() to query how many samples
 * remain before the user-specified limit is reached.
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
#endif /* GREATFET_SR_SW_LIMITS_DEFINED */

/*
 * Local sr_next_power_of_two() helper. Standard sigrok exposes this
 * from libsigrok-internal.h; PXView does not. The greatfet driver uses
 * it in greatfet_calc_capture_chans() to round up the channel count to
 * the next power of two (1/2/4/8).
 */
#ifndef GREATFET_SR_NEXT_POWER_OF_TWO_DEFINED
#define GREATFET_SR_NEXT_POWER_OF_TWO_DEFINED
static inline int sr_next_power_of_two(uint64_t value, uint64_t *prev,
		uint64_t *next)
{
	uint64_t p, pp;

	if (!next && !prev)
		return SR_ERR_ARG;

	p = 1;
	pp = 0;
	while (p < value) {
		pp = p;
		p <<= 1;
	}

	if (prev)
		*prev = pp;
	if (next)
		*next = p;

	return SR_OK;
}
#endif /* GREATFET_SR_NEXT_POWER_OF_TWO_DEFINED */

/*
 * Forward declaration of the standard sigrok feed queue type.
 *
 * PXView's libsigrok does NOT provide the feed_queue_logic_* family of
 * functions. This driver provides its own minimal local implementation
 * in protocol.c, covering the four entry points that the acquisition
 * path uses: alloc/submit_many/flush/free. The struct is opaque to
 * api.c (which only holds a pointer in devc->acquisition.feed_queue)
 * so a forward declaration is sufficient here.
 */
struct feed_queue_logic;

/* SR_PRIV entry points used by api.c (defined in protocol.c). */
SR_PRIV struct feed_queue_logic *feed_queue_logic_alloc(
		const struct sr_dev_inst *sdi,
		size_t sample_count, size_t unit_size);
SR_PRIV void feed_queue_logic_free(struct feed_queue_logic *q);

/*
 * Local sr_parse_probe_names / sr_free_probe_names helpers.
 *
 * PXView's compat layer declares sr_parse_probe_names() with a
 * different (incompatible) signature and a stub body. The greatfet
 * driver uses the canonical sigrok signature which returns a char **
 * and takes (str, defaults, max_count, default_count, *count). Redirect
 * the call to a local function via macro so the call site in api.c is
 * unchanged. The local implementation lives in protocol.c.
 */
#ifndef GREATFET_SR_PARSE_PROBE_NAMES_DEFINED
#define GREATFET_SR_PARSE_PROBE_NAMES_DEFINED
SR_PRIV char **greatfet_sr_parse_probe_names(const char *str,
		const char *const *defaults, size_t max_count,
		size_t default_count, size_t *count);
SR_PRIV void greatfet_sr_free_probe_names(char **names);
#define sr_parse_probe_names(str, defaults, max_count, default_count, count) \
		greatfet_sr_parse_probe_names((str), (defaults), (max_count), \
		(default_count), (count))
#define sr_free_probe_names(names) greatfet_sr_free_probe_names(names)
#endif /* GREATFET_SR_PARSE_PROBE_NAMES_DEFINED */

struct dev_context {
	struct sr_dev_inst *sdi;
	GString *usb_comm_buffer;
	char *firmware_version;
	char *serial_number;
	size_t channel_count;
	char **channel_names;
	size_t feed_unit_size;
	struct sr_sw_limits sw_limits;
	uint64_t samplerate;
	struct dev_acquisition_t {
		uint64_t bandwidth_threshold;
		size_t wire_unit_size;
		struct feed_queue_logic *feed_queue;
		size_t capture_channels;
		gboolean use_upper_pins;
		size_t channel_shift;
		size_t points_per_byte;
		uint64_t capture_samplerate;
		size_t firmware_bufsize;
		uint8_t samples_endpoint;
		uint8_t control_interface;
		uint8_t samples_interface;
		enum {
			ACQ_IDLE,
			ACQ_PREPARE,
			ACQ_RECEIVE,
			ACQ_SHUTDOWN,
		} acquisition_state;
		gboolean frame_begin_sent;
		gboolean control_interface_claimed;
		gboolean samples_interface_claimed;
		gboolean start_req_sent;
	} acquisition;
	struct dev_transfers_t {
		size_t transfer_bufsize;
		size_t transfers_count;
		uint8_t *transfer_buffer;
		struct libusb_transfer **transfers;
		size_t active_transfers;
		size_t capture_bufsize;
	} transfers;
};

SR_PRIV int greatfet_get_serial_number(const struct sr_dev_inst *sdi);
SR_PRIV int greatfet_get_version_number(const struct sr_dev_inst *sdi);

SR_PRIV int greatfet_setup_acquisition(const struct sr_dev_inst *sdi);
SR_PRIV int greatfet_start_acquisition(const struct sr_dev_inst *sdi);
SR_PRIV void greatfet_abort_acquisition(const struct sr_dev_inst *sdi);
SR_PRIV int greatfet_stop_acquisition(const struct sr_dev_inst *sdi);
SR_PRIV void greatfet_release_resources(const struct sr_dev_inst *sdi);

/*
 * PXView's sr_receive_data_callback_t passes the sdi directly as the
 * third argument (const struct sr_dev_inst *sdi) instead of the
 * void *cb_data that standard sigrok uses.
 */
SR_PRIV int greatfet_receive_data(int fd, int revents,
		const struct sr_dev_inst *sdi);

#endif
