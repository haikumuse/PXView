/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2012 Alexandru Gagniuc <mr.nuke.me@gmail.com>
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

#ifndef LIBSIGROK_HARDWARE_SERIAL_DMM_PROTOCOL_H
#define LIBSIGROK_HARDWARE_SERIAL_DMM_PROTOCOL_H

#include "hardware/compat/compat.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define LOG_PREFIX "serial-dmm"

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time.
 */
#ifndef SERIAL_DMM_SR_SW_LIMITS_DEFINED
#define SERIAL_DMM_SR_SW_LIMITS_DEFINED
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
#endif /* SERIAL_DMM_SR_SW_LIMITS_DEFINED */

/*
 * Standard sigrok's packet validation result codes. PXView's libsigrok
 * does not define these (it uses a simpler serial_stream_detect API).
 * They are used by the length-based packet validation path in
 * protocol.c's handle_new_data() for DMMs that provide packet_valid_len.
 */
#ifndef SR_PACKET_NEED_RX
#define SR_PACKET_NEED_RX   0
#endif
#ifndef SR_PACKET_VALID
#define SR_PACKET_VALID     1
#endif
#ifndef SR_PACKET_INVALID
#define SR_PACKET_INVALID   2
#endif

struct dmm_info {
	/** libsigrok driver info struct. */
	struct sr_dev_driver di;
	/** Manufacturer/brand. */
	const char *vendor;
	/** Model. */
	const char *device;
	/** conn string. */
	const char *conn;
	/** serialcomm string. */
	const char *serialcomm;
	/** Packet size in bytes. */
	size_t packet_size;
	/**
	 * Request timeout [ms] before request is considered lost and a new
	 * one is sent. Used only if device needs polling.
	 */
	uint64_t req_timeout_ms;
	/**
	 * Delay between reception of packet and next request. Some DMMs
	 * need this. Used only if device needs polling.
	 */
	uint64_t req_delay_ms;
	/** Packet request function. */
	int (*packet_request)(struct sr_serial_dev_inst *);
	/** Number of channels / displays. */
	size_t channel_count;
	/** (Optional) printf formats for channel names. */
	const char **channel_formats;
	/** Packet validation function. */
	gboolean (*packet_valid)(const uint8_t *);
	/** Packet parsing function. */
	int (*packet_parse)(const uint8_t *, float *,
			    struct sr_datafeed_analog *, void *);
	/** */
	void (*dmm_details)(struct sr_datafeed_analog *, void *);
	/** Size of chipset info struct. */
	gsize info_size;
	/* Serial-dmm items "with state" and variable length packets. */
	void *dmm_state;
	void *(*dmm_state_init)(void);
	void (*dmm_state_free)(void *state);
	int (*after_open)(struct sr_serial_dev_inst *serial);
	int (*packet_valid_len)(void *state, const uint8_t *data, size_t dlen,
		size_t *pkt_len);
	int (*packet_parse_len)(void *state, const uint8_t *data, size_t dlen,
		double *val, struct sr_datafeed_analog *analog, void *info);
	int (*config_get)(void *state, uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg);
	int (*config_set)(void *state, uint32_t key, GVariant *data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg);
	int (*config_list)(void *state, uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg);
	/**
	 * Hook at acquisition start. Can re-route the receive routine.
	 *
	 * NOTE: PXView's sr_receive_data_callback_t passes the sdi directly
	 * as the third argument (const struct sr_dev_inst *sdi) instead of
	 * the void *cb_data that standard sigrok uses. The cb_data parameter
	 * was therefore removed from this callback signature.
	 */
	int (*acquire_start)(void *state, const struct sr_dev_inst *sdi,
		sr_receive_data_callback_t *cb);
};

#define DMM_BUFSIZE 256

struct dev_context {
	struct sr_sw_limits limits;

	/**
	 * Pointer to the DMM chipset info entry that this device instance
	 * was probed with. Stored here so that config_get/set/list and the
	 * receive callback can retrieve the per-chipset function pointers
	 * without casting sdi->driver (which points to the standalone
	 * serial_dmm_driver_info struct in PXView's compat layer).
	 */
	struct dmm_info *dmm;

	uint8_t buf[DMM_BUFSIZE];
	size_t buflen;

	/**
	 * The timestamp [us] to send the next request.
	 * Used only if device needs polling.
	 */
	uint64_t req_next_at;
};

SR_PRIV int req_packet(struct sr_dev_inst *sdi);
/*
 * PXView's sr_receive_data_callback_t passes the sdi directly as the
 * third argument (const struct sr_dev_inst *sdi) instead of the
 * void *cb_data that standard sigrok uses.
 */
SR_PRIV int receive_data(int fd, int revents, const struct sr_dev_inst *sdi);

#endif
