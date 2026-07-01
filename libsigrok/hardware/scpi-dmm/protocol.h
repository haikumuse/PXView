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

#ifndef LIBSIGROK_HARDWARE_SCPI_DMM_PROTOCOL_H
#define LIBSIGROK_HARDWARE_SCPI_DMM_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <glib.h>
#include "hardware/compat/compat.h"

#undef LOG_PREFIX
#define LOG_PREFIX "scpi-dmm"

/*
 * PXView's libsigrok.h does not define several standard sigrok config keys
 * that this DMM driver needs. Provide them here with unique values that do
 * not collide with PXView's existing SR_CONF_* keys (which occupy the
 * 10000-10006 device-type range, the 30000-30098 config range, and the
 * 50000-50007 acquisition range). These compat values live in a reserved
 * gap (30160+) next to the DSO/sound compat keys in compat_config.h, and
 * match the approach used by other compat drivers (e.g. hp-3457a,
 * hp-3478a).
 */
#ifndef SR_CONF_MEASURED_QUANTITY
#define SR_CONF_MEASURED_QUANTITY 30160
#endif
#ifndef SR_CONF_RANGE
#define SR_CONF_RANGE 30234
#endif

/*
 * PXView's libsigrok.h mqflags enum ends at SR_MQFLAG_SPL_PCT_OVER_ALARM
 * (0x10000) and does not include SR_MQFLAG_FOUR_WIRE, which this driver
 * uses for 4-wire resistance measurements. Define it here with the
 * standard sigrok value (0x200000), guarded so it does not clash if
 * PXView later adds it upstream.
 */
#ifndef SR_MQFLAG_FOUR_WIRE
#define SR_MQFLAG_FOUR_WIRE 0x200000
#endif

/*
 * PXView's libsigrok.h SR_MQ enum ends at SR_MQ_RELATIVE_HUMIDITY and does
 * not include SR_MQ_TIME (period measurement), which several scpi-dmm
 * models advertise. Define it locally (value chosen to not collide with
 * existing enum members, all of which are >= 10000 and < 10015 in
 * PXView's libsigrok.h).
 */
#ifndef SR_MQ_TIME
#define SR_MQ_TIME 10015
#endif

#define SCPI_DMM_MAX_CHANNELS	1

/* ALL_ZERO terminator for struct scpi_command arrays. */
#ifndef ALL_ZERO
#define ALL_ZERO { 0 }
#endif

enum scpi_dmm_cmdcode {
	DMM_CMD_SETUP_REMOTE,
	DMM_CMD_SETUP_FUNC,
	DMM_CMD_QUERY_FUNC,
	DMM_CMD_START_ACQ,
	DMM_CMD_STOP_ACQ,
	DMM_CMD_QUERY_VALUE,
	DMM_CMD_QUERY_PREC,
	DMM_CMD_SETUP_LOCAL,
	DMM_CMD_QUERY_RANGE_AUTO,
	DMM_CMD_QUERY_RANGE,
	DMM_CMD_SETUP_RANGE_AUTO,
	DMM_CMD_SETUP_RANGE,
};

struct mqopt_item {
	enum sr_mq mq;
	enum sr_mqflag mqflag;
	const char *scpi_func_setup;
	const char *scpi_func_query;
	int default_precision;
	uint32_t drv_flags;
};
#define NO_DFLT_PREC	-99
#define FLAGS_NONE	0
#define FLAG_NO_RANGE	(1 << 0)
#define FLAG_CONF_DELAY	(1 << 1)
#define FLAG_MEAS_DELAY	(1 << 2)

struct scpi_dmm_model {
	const char *vendor;
	const char *model;
	size_t num_channels;
	ssize_t digits;
	const struct scpi_command *cmdset;
	const struct mqopt_item *mqopts;
	size_t mqopt_size;
	int (*get_measurement)(const struct sr_dev_inst *sdi, size_t ch);
	const uint32_t *devopts;
	size_t devopts_size;
	unsigned int read_timeout_us; /* If zero, use default from src/scpi/scpi.c. */
	unsigned int conf_delay_us;
	unsigned int meas_delay_us;
	float infinity_limit; /* If zero, use default from protocol.c */
	gboolean check_opc;
	const char *(*get_range_text)(const struct sr_dev_inst *sdi);
	int (*set_range_from_text)(const struct sr_dev_inst *sdi,
		const char *range);
	GVariant *(*get_range_text_list)(const struct sr_dev_inst *sdi);
};

/*
 * PXView's libsigrok does not provide struct sr_sw_limits or the
 * sr_sw_limits_* helpers that standard sigrok's libsigrok-internal.h
 * exposes. Define them locally as static inline so this driver is
 * self-contained and cannot clash with copies living in other compat
 * drivers at link time (same pattern as hp-3478a/protocol.h,
 * motech-lps-30x/protocol.h).
 *
 * The SR_SW_LIMITS_H guard prevents redefinition if PXView ever adds
 * these types upstream, and is safe across drivers because each
 * driver's translation unit only includes its own protocol.h.
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
 * PXView's sr_datafeed_analog is the old flat layout (probes, num_samples,
 * unit_bits, unit_pitch, mq, unit, mqflags, data). There is no
 * sr_analog_init() helper and no encoding/meaning/spec sub-structs, so
 * the run_acq_info struct below omits the encoding/meaning/spec arrays
 * that the standard sigrok source used. The per-channel digits value
 * computed by the model's get_measurement() callback is informational
 * only (PXView's flat analog has no digits field); the calculation is
 * kept in protocol.c to preserve the original driver's precision logic
 * but the result is marked unused.
 */
struct dev_context {
	size_t num_channels;
	const struct scpi_command *cmdset;
	const struct scpi_dmm_model *model;
	struct sr_sw_limits limits;
	struct {
		enum sr_mq curr_mq;
		enum sr_mqflag curr_mqflag;
	} start_acq_mq;
	struct scpi_dmm_acq_info {
		float f_value;
		double d_value;
		struct sr_datafeed_packet packet;
		struct sr_datafeed_analog analog[SCPI_DMM_MAX_CHANNELS];
	} run_acq_info;
	gchar *precision;
	char range_text[32];
};

SR_PRIV void scpi_dmm_cmd_delay(struct sr_scpi_dev_inst *scpi);
SR_PRIV const struct mqopt_item *scpi_dmm_lookup_mq_number(
	const struct sr_dev_inst *sdi, enum sr_mq mq, enum sr_mqflag flag);
SR_PRIV const struct mqopt_item *scpi_dmm_lookup_mq_text(
	const struct sr_dev_inst *sdi, const char *text);
SR_PRIV int scpi_dmm_get_mq(const struct sr_dev_inst *sdi,
	enum sr_mq *mq, enum sr_mqflag *flag, char **rsp,
	const struct mqopt_item **mqitem);
SR_PRIV int scpi_dmm_set_mq(const struct sr_dev_inst *sdi,
	enum sr_mq mq, enum sr_mqflag flag);
SR_PRIV const char *scpi_dmm_get_range_text(const struct sr_dev_inst *sdi);
SR_PRIV int scpi_dmm_set_range_from_text(const struct sr_dev_inst *sdi,
	const char *range);
SR_PRIV GVariant *scpi_dmm_get_range_text_list(const struct sr_dev_inst *sdi);
SR_PRIV int scpi_dmm_get_meas_agilent(const struct sr_dev_inst *sdi, size_t ch);
SR_PRIV int scpi_dmm_get_meas_gwinstek(const struct sr_dev_inst *sdi, size_t ch);
/*
 * PXView's sr_receive_data_callback_t is typed as
 *   int (*)(int fd, int revents, const struct sr_dev_inst *sdi)
 * (see libsigrok-internal.h:303). The original sigrok source used
 *   int (int fd, int revents, void *cb_data)
 * so the signature is adapted here: the opaque cb_data is replaced by a
 * typed const sdi, and the body no longer needs to dereference cb_data.
 */
SR_PRIV int scpi_dmm_receive_data(int fd, int revents,
	const struct sr_dev_inst *sdi);

#endif
