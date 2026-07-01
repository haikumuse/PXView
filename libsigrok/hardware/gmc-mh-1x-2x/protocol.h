/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013, 2014 Matthias Heidbrink <m-sigrok@heidbrink.biz>
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

#ifndef LIBSIGROK_HARDWARE_GMC_MH_1X_2X_PROTOCOL_H
#define LIBSIGROK_HARDWARE_GMC_MH_1X_2X_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "gmc-mh-1x-2x"

#define GMC_BUFSIZE 266
#define GMC_REPLY_SIZE 14

/** Message ID bits 4, 5 */
#define MSGID_MASK  0x30 /**< Mask to get message ID bits */
#define MSGID_INF   0x00 /**< Start of message with device info */
#define MSGID_D10   0x10 /**< Start of data message, non-displayed intermediate */
#define MSGID_DTA   0x20 /**< Start of data message, displayed, averaged */
#define MSGID_DATA  0x30 /**< Data byte in message */

#define MSGC_MASK   0x0f /**< Mask to get message byte contents in send mode */

#define MSGSRC_MASK 0xc0 /**< Mask to get bits related to message source */

#define bc(x) (x & MSGC_MASK) /**< Get contents from a byte */

#define MASK_6BITS  0x3f /**< Mask lower six bits. */

/**
 * Internal multimeter model codes. In opposite to the multimeter models from
 * protocol (see decode_model()), these codes allow working with ranges.
 */
enum model {
	METRAHIT_NONE		= 0,  /**< Value for uninitialized variable */
	METRAHIT_12S		= 12,
	METRAHIT_13S14A		= 13,
	METRAHIT_14S		= 14,
	METRAHIT_15S		= 15,
	METRAHIT_16S		= 16,
	METRAHIT_16I		= 17, /**< Metrahit 16I, L */
	METRAHIT_16T		= 18, /**< Metrahit 16T, U, KMM2002 */
	METRAHIT_16X = METRAHIT_16T,  /**< All Metrahit 16 */
	/* A Metrahit 17 exists, but seems not to have an IR interface. */
	METRAHIT_18S		= 19,
	METRAHIT_2X		= 20, /**< For model type comparisons */
	METRAHIT_22SM		= METRAHIT_2X + 1,	/**< Send mode */
	METRAHIT_22S		= METRAHIT_22SM + 1,	/**< Bidi mode */
	METRAHIT_22M		= METRAHIT_22S + 1,	/**< Bidi mode */
	METRAHIT_23S		= METRAHIT_22M + 1,
	METRAHIT_24S		= METRAHIT_23S + 1,
	METRAHIT_25S		= METRAHIT_24S + 1,
	METRAHIT_26SM		= METRAHIT_25S + 1,	/**< Send mode */
	METRAHIT_26S		= METRAHIT_26SM + 1,	/**< Bidi mode */
	METRAHIT_26M		= METRAHIT_26S + 1,	/**< Bidi mode */
	/* The Metrahit 27x and 28Cx have a totally different protocol */
	METRAHIT_28S		= METRAHIT_26M + 1,
	METRAHIT_29S		= METRAHIT_28S + 1,
};

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time. The guard macro prevents double-definition if
 * another header in the same translation unit already pulled in a copy.
 */
#ifndef SR_SW_LIMITS_H
#define SR_SW_LIMITS_H

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

#endif /* SR_SW_LIMITS_H */

/*
 * PXView's libsigrok.h defines SR_MQ_* / SR_UNIT_* / SR_MQFLAG_* as
 * anonymous enum values (no tagged enum sr_mq/sr_unit/sr_mqflag type),
 * and struct sr_datafeed_analog stores them as int / int / uint64_t.
 * Mirror that layout here so assignments from these constants stay
 * type-correct without needing a local "enum sr_mq/sr_unit/sr_mqflag"
 * tag declaration (same approach as fluke-dmm/protocol.h,
 * atorch/protocol.h).
 */
struct dev_context {
	enum model model;	/**< Model code. */

	struct sr_sw_limits limits;

	gboolean settings_ok;	/**< Settings msg received yet. */
	int msg_type;       /**< Message type (MSGID_INF, ...). */
	int msg_len;        /**< Message length (valid when msg, curr. type known).*/
	int mq;             /**< Measured quantity (SR_MQ_* as int). */
	int unit;           /**< Measured unit (SR_UNIT_* as int). */
	uint64_t mqflags;   /**< Measured quantity flags (SR_MQFLAG_* as uint64_t). */
	float value;		/**< Measured value */
	int8_t scale;       /**< Scale for value. */
	int8_t scale1000;   /**< Additional scale factor 1000x. */
	int addr;           /**< Device address (1..15). */
	int cmd_idx;        /**< Parameter "Idx" (Index) of current command, if required. */
	int cmd_seq;        /**< Command sequence. Used to query status every n messages. */
	gboolean autorng;   /**< Auto range enabled. */
	float ubatt;        /**< Battery voltage. */
	uint8_t fw_ver_maj; /**< Firmware version major. */
	uint8_t fw_ver_min; /**< Firmware version minor. */
	int64_t req_sent_at;    /**< Request sent. */
	gboolean response_pending; /**< Request sent, response is pending. */

	uint8_t buf[GMC_BUFSIZE];	/**< Buffer for read callback */
	int buflen;			/**< Data len in buf */
};

/*
 * PXView's sr_receive_data_callback_t is typed as
 *   int (*)(int fd, int revents, const struct sr_dev_inst *sdi)
 * (see libsigrok-internal.h). The original sigrok source used
 *   int (int fd, int revents, void *cb_data)
 * so the signature is adapted here: the opaque cb_data is replaced by a
 * typed const sdi, and the body no longer needs to dereference cb_data.
 */
SR_PRIV int config_set(uint32_t key, GVariant *data, const struct sr_dev_inst *sdi,
		const struct sr_channel_group *cg);
SR_PRIV int gmc_decode_model_bd(uint8_t mcode);
SR_PRIV int gmc_decode_model_sm(uint8_t mcode);
SR_PRIV int gmc_mh_1x_2x_receive_data(int fd, int revents, const struct sr_dev_inst *sdi);
SR_PRIV int gmc_mh_2x_receive_data(int fd, int revents, const struct sr_dev_inst *sdi);
SR_PRIV const char *gmc_model_str(enum model mcode);
SR_PRIV int process_msg14(const struct sr_dev_inst *sdi);
SR_PRIV int req_meas14(const struct sr_dev_inst *sdi);
SR_PRIV int req_stat14(const struct sr_dev_inst *sdi, gboolean power_on);
SR_PRIV int gmc_mh_1x_2x_dev_acquisition_stop(struct sr_dev_inst *sdi);

#endif
