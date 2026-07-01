/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014 Matthias Heidbrink <m-sigrok@heidbrink.biz>
 * Copyright (C) 2014 Bert Vermeulen <bert@biot.com> (code from atten-pps3xxx)
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

#ifndef LIBSIGROK_HARDWARE_MOTECH_LPS_30X_PROTOCOL_H
#define LIBSIGROK_HARDWARE_MOTECH_LPS_30X_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <glib.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "motech-lps-30x"

/*
 * PXView does not define several standard sigrok config keys that this
 * power supply driver needs. Provide them here with unique values that
 * do not collide with PXView's existing SR_CONF_* keys (which occupy the
 * 10000-10006 device-type range, the 30000-30107 config range, and the
 * 50000-50007 acquisition range). These compat values live in reserved
 * gaps. The values mirror those used by the korad-kaxxxxp, atten-pps3xxx,
 * gwinstek-psp and manson-hcs-3xxx compat drivers so all power-supply
 * drivers agree when compiled together.
 */
#ifndef SR_CONF_POWER_SUPPLY
#define SR_CONF_POWER_SUPPLY 10008
#endif
#ifndef SR_CONF_ENABLED
#define SR_CONF_ENABLED 30200
#endif
#ifndef SR_CONF_VOLTAGE
#define SR_CONF_VOLTAGE 30220
#endif
#ifndef SR_CONF_VOLTAGE_TARGET
#define SR_CONF_VOLTAGE_TARGET 30221
#endif
#ifndef SR_CONF_CURRENT
#define SR_CONF_CURRENT 30222
#endif
#ifndef SR_CONF_CURRENT_LIMIT
#define SR_CONF_CURRENT_LIMIT 30223
#endif
/*
 * SR_CONF_CHANNEL_CONFIG is specific to multi-channel power supplies.
 * PXView does not define it; assign a unique value in the reserved compat
 * range that does not collide with the keys above or with PXView's keys.
 * Matches the value used by atten-pps3xxx.
 */
#ifndef SR_CONF_CHANNEL_CONFIG
#define SR_CONF_CHANNEL_CONFIG 30228
#endif

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time.
 */
#ifndef MOTECH_LPS_30X_SR_SW_LIMITS_DEFINED
#define MOTECH_LPS_30X_SR_SW_LIMITS_DEFINED
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
#endif /* MOTECH_LPS_30X_SR_SW_LIMITS_DEFINED */

#define LINELEN_MAX 50	/**< Max. line length for requests */

#define REQ_TIMEOUT_MS 250 /**< Timeout [ms] for single request. */

#define MAX_CHANNELS 3

typedef enum {
	LPS_UNKNOWN = 0,/**< Unknown model (used during detection process) */
	LPS_301,	/**< Motech/Amrel LPS-301, 1 output */
	LPS_302,	/**< Motech/Amrel LPS-302, 1 output */
	LPS_303,	/**< Motech/Amrel LPS-303, 1 output */
	LPS_304,	/**< Motech/Amrel LPS-304, 3 outputs */
	LPS_305,	/**< Motech/Amrel LPS-305, 3 outputs */
} lps_modelid;

/** Channel specification */
struct channel_spec {
	/* Min, max, step. */
	gdouble voltage[3];
	gdouble current[3];
};

/** Model properties specification */
struct lps_modelspec {
	lps_modelid modelid;
	const char *modelstr;
	uint8_t num_channels;
	struct channel_spec channels[3];
};

/** Used to implement a little state machine to query all required values in a row. */
typedef enum {
	AQ_NONE,
	AQ_U1,
	AQ_I1,
	AQ_I2,
	AQ_U2,
	AQ_STATUS,
} acquisition_req;

/** Status of a single channel. */
struct channel_status {
	/* Channel information (struct channel_info*). data (struct) owned by sdi, just a reference to address a single channel. */
	GSList *info;
	/* Received from device. */
	gdouble output_voltage_last;
	gdouble output_current_last;
	gboolean output_enabled;	/**< Also used when set. */
	gboolean cc_mode;		/**< Constant current mode. If false, constant voltage mode. */
	/* Set by frontend. */
	gdouble output_voltage_max;
	gdouble output_current_max;
};

struct dev_context {
	const struct lps_modelspec *model;

	gboolean acq_running;		/**< Acquisition is running. */
	struct sr_sw_limits limits;
	acquisition_req acq_req;	/**< Current request. */
	uint8_t	acq_req_pending;	/**< Request pending. 0=none, 1=reply, 2=OK */

	struct channel_status channel_status[MAX_CHANNELS];
	guint8 tracking_mode;		/**< 0=off, 1=Tracking from CH1, 2=Tracking from CH2. */

	int64_t req_sent_at;    /**< Request sent. */
	gchar buf[LINELEN_MAX];	/**< Buffer for read callback */
	int buflen;		/**< Data len in buf */
};

/*
 * Local replacements for standard sigrok's sr_atoi() / sr_atod_ascii(),
 * which PXView's libsigrok does not provide. Parse ASCII numeric strings
 * in a locale-independent way and return SR_OK/SR_ERR. Implemented in
 * protocol.c so they can be shared by protocol.c (process_line) and
 * api.c (lps_query_status).
 */
SR_PRIV int lps_sr_atoi(const char *str, int *ret);
SR_PRIV int lps_sr_atod_ascii(const char *str, double *ret);

SR_PRIV int lps_process_status(struct sr_dev_inst *sdi, int stat);
SR_PRIV int lps_send_req(struct sr_serial_dev_inst *serial, const char *fmt, ...);

SR_PRIV int motech_lps_30x_receive_data(int fd, int revents,
		const struct sr_dev_inst *sdi);

#endif
