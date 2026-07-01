/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2018 Sven Bursch-Osewold <sb_git@bursch.com>
 * Copyright (C) 2019 King Kévin <kingkevin@cuvoodoo.info>
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

#ifndef LIBSIGROK_HARDWARE_ZKETECH_EBD_USB_PROTOCOL_H
#define LIBSIGROK_HARDWARE_ZKETECH_EBD_USB_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <glib.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "zketech-ebd-usb"

#define MSG_MAX_LEN 19
#define MSG_FRAME_BEGIN 0xfa
#define MSG_FRAME_END 0xf8

/*
 * PXView does not define several standard sigrok config keys that this
 * electronic-load driver needs. Provide them here with unique values that
 * do not collide with PXView's existing SR_CONF_* keys. The values mirror
 * those used by the itech-it8500, siglent-sdl10x0, maynuo-m97 and the
 * korad-kaxxxxp / atten-pps3xxx / manson-hcs-3xxx / motech-lps-30x /
 * gwinstek-psp / rdtech-dps / scpi-pps compat drivers, so all power-supply
 * and electronic-load drivers agree when compiled together.
 */
#ifndef SR_CONF_ELECTRONIC_LOAD
#define SR_CONF_ELECTRONIC_LOAD 10009
#endif
#ifndef SR_CONF_CURRENT_LIMIT
#define SR_CONF_CURRENT_LIMIT 30223
#endif
#ifndef SR_CONF_UNDER_VOLTAGE_CONDITION_THRESHOLD
#define SR_CONF_UNDER_VOLTAGE_CONDITION_THRESHOLD 30256
#endif

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time.
 */
#ifndef ZKETECH_EBD_USB_SR_SW_LIMITS_DEFINED
#define ZKETECH_EBD_USB_SR_SW_LIMITS_DEFINED
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
#endif /* ZKETECH_EBD_USB_SR_SW_LIMITS_DEFINED */

struct dev_context {
	struct sr_sw_limits limits;
	GMutex rw_mutex;
	float current_limit;
	float uvc_threshold;
	gboolean running;
	gboolean load_activated;
};

/* Communication via serial. */
SR_PRIV int ebd_read_message(struct sr_serial_dev_inst *serial, size_t length,
	uint8_t *buf);

/* Commands. */
SR_PRIV int ebd_init(struct sr_serial_dev_inst *serial,
	struct dev_context *devc);
SR_PRIV int ebd_loadstart(struct sr_serial_dev_inst *serial,
	struct dev_context *devc);
SR_PRIV int ebd_receive_data(int fd, int revents,
	const struct sr_dev_inst *sdi);
SR_PRIV int ebd_stop(struct sr_serial_dev_inst *serial,
	struct dev_context *devc);
SR_PRIV int ebd_loadtoggle(struct sr_serial_dev_inst *serial,
	struct dev_context *devc);

/* Configuration. */
SR_PRIV int ebd_get_current_limit(const struct sr_dev_inst *sdi, float *current);
SR_PRIV int ebd_set_current_limit(const struct sr_dev_inst *sdi, float current);
SR_PRIV int ebd_get_uvc_threshold(const struct sr_dev_inst *sdi, float *voltage);
SR_PRIV int ebd_set_uvc_threshold(const struct sr_dev_inst *sdi, float voltage);
SR_PRIV gboolean ebd_current_is0(struct dev_context *devc);

#endif
