/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2018 Gerhard Sittig <gerhard.sittig@gmx.net>
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

#ifndef LIBSIGROK_HARDWARE_MICROCHIP_PICKIT2_PROTOCOL_H
#define LIBSIGROK_HARDWARE_MICROCHIP_PICKIT2_PROTOCOL_H

#include <glib.h>
#include <libusb.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/compat/compat.h"

#define LOG_PREFIX "microchip-pickit2"

#define PICKIT2_CHANNEL_COUNT	3
#define PICKIT2_SAMPLE_COUNT	1024
#define PICKIT2_SAMPLE_RAWLEN	(4 * 128)

enum pickit_state {
	STATE_IDLE,
	STATE_CONF,
	STATE_WAIT,
	STATE_DATA,
};

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time.
 */
#ifndef PICKIT2_SR_SW_LIMITS_DEFINED
#define PICKIT2_SR_SW_LIMITS_DEFINED
struct sr_sw_limits {
	uint64_t limit_samples;
	uint64_t limit_msec;
	int64_t starttime_ms;
	uint64_t samples_read;
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
#endif /* PICKIT2_SR_SW_LIMITS_DEFINED */

/*
 * Local sr_hexdump helpers (simple space-separated hex for debug spew).
 * PXView's libsigrok does not provide sr_hexdump_new()/sr_hexdump_free().
 */
#ifndef PICKIT2_SR_HEXDUMP_DEFINED
#define PICKIT2_SR_HEXDUMP_DEFINED
SR_PRIV GString *pickit2_sr_hexdump_new(const uint8_t *buf, size_t len);
SR_PRIV void pickit2_sr_hexdump_free(GString *gstr);
#define sr_hexdump_new(buf, len) pickit2_sr_hexdump_new((buf), (len))
#define sr_hexdump_free(gstr) pickit2_sr_hexdump_free(gstr)
#endif /* PICKIT2_SR_HEXDUMP_DEFINED */

struct dev_context {
	char **channel_names;
	enum pickit_state state;
	const uint64_t *samplerates;
	size_t num_samplerates;
	size_t curr_samplerate_idx;
	const uint64_t *captureratios;
	size_t num_captureratios;
	size_t curr_captureratio_idx;
	struct sr_sw_limits sw_limits;
	gboolean detached_kernel_driver;
	int32_t triggers[PICKIT2_CHANNEL_COUNT];	/**@< see @ref SR_TRIGGER_ZERO et al */
	size_t trigpos;
	uint8_t samples_raw[PICKIT2_SAMPLE_RAWLEN];
	uint8_t samples_conv[PICKIT2_SAMPLE_COUNT];
};

SR_PRIV int microchip_pickit2_setup_trigger(const struct sr_dev_inst *sdi);

/*
 * PXView's sr_receive_data_callback_t passes the sdi directly as the
 * third argument (const struct sr_dev_inst *sdi) instead of the
 * void *cb_data that standard sigrok uses.
 */
SR_PRIV int microchip_pickit2_receive_data(int fd, int revents,
		const struct sr_dev_inst *sdi);

#endif
