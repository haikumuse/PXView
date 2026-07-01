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

#include "hardware/compat/compat.h"
#include <string.h>
#include "protocol.h"

/* Duration of scan. */
#define ATORCH_PROBE_TIMEOUT_MS	10000

/*
 * Message layout:
 * 2 magic header bytes
 * 1 message type byte
 * N payload bytes, determined by message type
 */

/* Position of message type byte in a message. */
#define HEADER_MSGTYPE_IDX	2
#define PAYLOAD_START_IDX	3

/* Length of each message type. */
#define MSGLEN_REPORT	(4 + 32)
#define MSGLEN_REPLY	(4 + 4)
#define MSGLEN_COMMAND	(4 + 6)

/* Minimal length of a valid message. */
#define MSGLEN_MIN	4

static const uint8_t header_magic[] = {
	0xff, 0x55,
};

/*
 * Channel descriptor tables.
 *
 * The original sigrok driver used double literals (1e3) for the
 * sr_rational.q field, but PXView's local sr_rational uses uint64_t
 * fields. Use integer literals (1000) instead to avoid double-to-uint64_t
 * conversion warnings. Same approach as rdtech-um/protocol.c.
 */
static const struct atorch_channel_desc atorch_dc_power_meter_channels[] = {
	{ "V", { 4, BVT_BE_UINT24, }, { 100, 1000, }, 1, SR_MQ_VOLTAGE, SR_UNIT_VOLT, SR_MQFLAG_DC, },
	{ "I", { 7, BVT_BE_UINT24, }, { 1, 1000, }, 3, SR_MQ_CURRENT, SR_UNIT_AMPERE, SR_MQFLAG_DC, },
	{ "C", { 10, BVT_BE_UINT24, }, { 10, 1000, }, 2, SR_MQ_ENERGY, SR_UNIT_AMPERE_HOUR, 0, },
	{ "E", { 13, BVT_BE_UINT32, }, { 10, 1, }, -2, SR_MQ_ENERGY, SR_UNIT_WATT_HOUR, 0, },
	{ "T", { 24, BVT_BE_UINT16, }, { 1, 1, }, 0, SR_MQ_TEMPERATURE, SR_UNIT_CELSIUS, 0, },
};

static const struct atorch_channel_desc atorch_usb_power_meter_channels[] = {
	{ "V", { 4, BVT_BE_UINT24, }, { 10, 1000, }, 2, SR_MQ_VOLTAGE, SR_UNIT_VOLT, SR_MQFLAG_DC, },
	{ "I", { 7, BVT_BE_UINT24, }, { 10, 1000, }, 2, SR_MQ_CURRENT, SR_UNIT_AMPERE, SR_MQFLAG_DC, },
	{ "C", { 10, BVT_BE_UINT24, }, { 1, 1000, }, 3, SR_MQ_ENERGY, SR_UNIT_AMPERE_HOUR, 0, },
	{ "E", { 13, BVT_BE_UINT32, }, { 10, 1000, }, 2, SR_MQ_ENERGY, SR_UNIT_WATT_HOUR, 0, },
	{ "D-", { 17, BVT_BE_UINT16, }, { 10, 1000, }, 2, SR_MQ_VOLTAGE, SR_UNIT_VOLT, SR_MQFLAG_DC, },
	{ "D+", { 19, BVT_BE_UINT16, }, { 10, 1000, }, 2, SR_MQ_VOLTAGE, SR_UNIT_VOLT, SR_MQFLAG_DC, },
	{ "T", { 21, BVT_BE_UINT16, }, { 1, 1, }, 0, SR_MQ_TEMPERATURE, SR_UNIT_CELSIUS, 0, },
};

static const struct atorch_device_profile atorch_profiles[] = {
	{ 0x02, "DC Meter", ARRAY_AND_SIZE(atorch_dc_power_meter_channels), },
	{ 0x03, "USB Meter", ARRAY_AND_SIZE(atorch_usb_power_meter_channels), },
};

/* ===========================================================================
 * Local sr_hexdump helpers (simple space-separated hex for debug spew).
 *
 * PXView's libsigrok does not provide sr_hexdump_new()/sr_hexdump_free().
 * The declarations and #define redirects live in protocol.h; the
 * implementations here match the asix-omega-rtm-cli / microchip-pickit2 /
 * rdtech-tc pattern.
 * =========================================================================== */
SR_PRIV GString *atorch_sr_hexdump_new(const uint8_t *buf, size_t len)
{
	GString *gstr;
	size_t i;

	if (!buf || !len)
		return g_string_new("");

	gstr = g_string_sized_new(len * 3 + 1);
	for (i = 0; i < len; i++) {
		if (i > 0)
			g_string_append_c(gstr, ' ');
		g_string_append_printf(gstr, "%02x", buf[i]);
	}
	return gstr;
}

SR_PRIV void atorch_sr_hexdump_free(GString *gstr)
{
	if (gstr)
		g_string_free(gstr, TRUE);
}

/* ===========================================================================
 * Local std_session_send_df_frame_end.
 *
 * PXView's compat layer provides std_session_send_df_frame_begin() as a
 * single canonical implementation (compat_helpers.c) but does NOT provide
 * the frame_end variant. Define it locally as a file-scoped static, same
 * pattern as arachnid-labs-re-load-pro/protocol.c and rdtech-um/protocol.c.
 * =========================================================================== */
static int std_session_send_df_frame_end(const struct sr_dev_inst *sdi)
{
	struct sr_datafeed_packet packet;

	memset(&packet, 0, sizeof(packet));
	packet.type = SR_DF_FRAME_END;
	packet.status = SR_PKT_OK;
	packet.payload = NULL;
	return ds_data_forward(sdi, &packet);
}

/* ===========================================================================
 * Local feed_queue_analog implementation.
 *
 * PXView's libsigrok does NOT provide the feed_queue_analog_* family that
 * standard sigrok exposes via libsigrok-internal.h. This is a minimal local
 * implementation covering the entry points the atorch driver uses:
 *
 *   - alloc(sdi, sample_count, digits, ch)
 *   - mq_unit(q, mq, flags, unit)
 *   - scale_offset(q, scale, offset)
 *   - submit_one(q, value, sample_count)
 *   - flush(q)
 *   - free(q)
 *
 * The implementation maintains a flat float buffer of (sample_count *
 * sizeof(float)) bytes. submit_one() appends the given sample(s), auto-
 * flushing when the buffer fills up. flush() applies the rational scale
 * (value = raw * p / q) to every buffered sample, then emits a single
 * SR_DF_ANALOG packet via sr_session_send() (which the compat layer maps
 * to PXView's ds_data_forward()) using PXView's flat
 * struct sr_datafeed_analog (analog.probes / analog.num_samples /
 * analog.data / analog.mq / analog.unit / analog.mqflags).
 *
 * This mirrors the rdtech-um send_channel_value() flat-analog approach but
 * keeps the feed_queue_analog API surface so the original atorch source's
 * create_channels_feed_queues() and parse_report_msg() need no rewrite.
 * =========================================================================== */
struct feed_queue_analog *feed_queue_analog_alloc(
		const struct sr_dev_inst *sdi, size_t sample_count,
		int digits, struct sr_channel *ch)
{
	struct feed_queue_analog *q;

	if (!sdi || !sample_count || !ch)
		return NULL;

	q = g_malloc0(sizeof(*q));
	q->sdi = sdi;
	q->channel = ch;
	q->unit_size = sizeof(float);
	q->cap_samples = sample_count;
	q->count_samples = 0;
	q->data = g_malloc0(sample_count * q->unit_size);
	q->digits = digits;
	/* mq / unit / mqflags / scale default to zeroed by g_malloc0. */
	return q;
}

/*
 * Parameter types are int / uint64_t / int (not enum sr_mq/sr_mqflag/
 * sr_unit) because PXView's libsigrok.h defines those constants as
 * anonymous enum values. See the note on struct feed_queue_analog and
 * atorch_channel_desc in protocol.h.
 */
void feed_queue_analog_mq_unit(struct feed_queue_analog *q,
		int mq, uint64_t flags, int unit)
{
	if (!q)
		return;
	q->mq = mq;
	q->mqflags = flags;
	q->unit = unit;
}

void feed_queue_analog_scale_offset(struct feed_queue_analog *q,
		const struct sr_rational *scale, const struct sr_rational *offset)
{
	if (!q || !scale)
		return;
	q->scale = *scale;
	/* offset is unused by the atorch driver (always passes NULL). */
	(void)offset;
}

int feed_queue_analog_flush(struct feed_queue_analog *q)
{
	struct sr_datafeed_analog analog;
	struct sr_datafeed_packet packet;
	float *fdata;
	size_t i;

	if (!q)
		return SR_ERR_ARG;
	if (q->count_samples == 0)
		return SR_OK;

	/*
	 * Apply the rational scale to every buffered sample in place:
	 * value = raw * p / q. Guard against q == 0 to avoid division by
	 * zero (the atorch channel tables always use a non-zero q).
	 */
	if (q->scale.q != 0 && (q->scale.p != 1 || q->scale.q != 1)) {
		fdata = (float *)q->data;
		for (i = 0; i < q->count_samples; i++)
			fdata[i] = fdata[i] * (float)q->scale.p / (float)q->scale.q;
	}

	memset(&analog, 0, sizeof(analog));
	analog.probes = g_slist_append(NULL, q->channel);
	analog.num_samples = q->count_samples;
	analog.data = q->data;
	analog.mq = q->mq;
	analog.unit = q->unit;
	analog.mqflags = q->mqflags;
	analog.unit_bits = 32;	/* float */
	analog.unit_pitch = 0;

	memset(&packet, 0, sizeof(packet));
	packet.type = SR_DF_ANALOG;
	packet.status = SR_PKT_OK;
	packet.payload = &analog;
	sr_session_send(q->sdi, &packet);

	g_slist_free(analog.probes);
	q->count_samples = 0;

	return SR_OK;
}

int feed_queue_analog_submit_one(struct feed_queue_analog *q,
		float value, size_t sample_count)
{
	float *dst;

	if (!q || !sample_count)
		return SR_ERR_ARG;

	/* Auto-flush when the new submission would overflow the buffer. */
	if (q->count_samples + sample_count > q->cap_samples)
		feed_queue_analog_flush(q);

	dst = (float *)q->data + q->count_samples;
	*dst = value;
	q->count_samples += sample_count;

	/* Auto-flush when the buffer is full (atorch uses cap_samples=1). */
	if (q->count_samples >= q->cap_samples)
		feed_queue_analog_flush(q);

	return SR_OK;
}

void feed_queue_analog_free(struct feed_queue_analog *q)
{
	if (!q)
		return;
	/* Flush any trailing pending sample before tearing down. */
	feed_queue_analog_flush(q);
	g_free(q->data);
	g_free(q);
}

/* ===========================================================================
 * Original atorch protocol parsing logic (unchanged apart from the const
 * sdi callback signature and the now-extern feed_queue_analog_submit_one).
 * =========================================================================== */

static size_t get_length_for_msg_type(uint8_t msg_type)
{
	switch (msg_type) {
	case MSG_REPORT:
		return MSGLEN_REPORT;
	case MSG_REPLY:
		return MSGLEN_REPLY;
	case MSG_COMMAND:
		return MSGLEN_COMMAND;
	default:
		return 0;
	}
}

/*
 * PXView's libsigrok does not provide sr_log_loglevel_get() or SR_LOG_DBG.
 * Always emit the hexdump + sr_dbg() call; sr_dbg() is itself a no-op when
 * debug logging is disabled at the xlog level, so the cost is negligible.
 */
static void log_atorch_msg(const uint8_t *buf, size_t len)
{
	GString *text;

	text = sr_hexdump_new(buf, len);
	sr_dbg("Atorch msg: %s", text->str);
	sr_hexdump_free(text);
}

static const uint8_t *locate_next_valid_msg(struct dev_context *devc)
{
	uint8_t *valid_msg_ptr;
	size_t valid_msg_len;
	uint8_t *msg_ptr;

	/* Enough byte to make a message? */
	while (devc->rd_idx + MSGLEN_MIN <= devc->wr_idx) {
		/* Look for header magic. */
		msg_ptr = devc->buf + devc->rd_idx;
		if (memcmp(msg_ptr, header_magic, sizeof(header_magic)) != 0) {
			devc->rd_idx += 1;
			continue;
		}

		/* Determine msg type and length. */
		valid_msg_len = get_length_for_msg_type(msg_ptr[HEADER_MSGTYPE_IDX]);
		if (!valid_msg_len) {
			devc->rd_idx += 2;
			continue;
		}

		/* Do we have the complete message? */
		if (devc->rd_idx + valid_msg_len <= devc->wr_idx) {
			valid_msg_ptr = msg_ptr;
			devc->rd_idx += valid_msg_len;
			log_atorch_msg(valid_msg_ptr, valid_msg_len);
			return valid_msg_ptr;
		}

		return NULL;
	}
	return NULL;
}

static const uint8_t *receive_msg(struct sr_serial_dev_inst *serial,
	struct dev_context *devc)
{
	size_t len;
	const uint8_t *valid_msg_ptr;

	while (1) {
		/* Remove bytes already processed. */
		if (devc->rd_idx > 0) {
			len = devc->wr_idx - devc->rd_idx;
			memmove(devc->buf, devc->buf + devc->rd_idx, len);
			devc->wr_idx -= devc->rd_idx;
			devc->rd_idx = 0;
		}

		/* Read more bytes to process. */
		len = ATORCH_BUFSIZE - devc->wr_idx;
		len = serial_read_nonblocking(serial, devc->buf + devc->wr_idx, len);
		if (len <= 0)
			return NULL;
		devc->wr_idx += len;

		/* Locate next start of message. */
		valid_msg_ptr = locate_next_valid_msg(devc);
		if (valid_msg_ptr)
			return valid_msg_ptr;
	}
}

static const struct atorch_device_profile *find_profile_for_device_type(uint8_t dev_type)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(atorch_profiles); i++) {
		if (atorch_profiles[i].device_type == dev_type)
			return &atorch_profiles[i];
	}
	return NULL;
}

static void parse_report_msg(struct sr_dev_inst *sdi, const uint8_t *report_ptr)
{
	struct dev_context *devc;
	float val;
	size_t i;

	devc = sdi->priv;

	/* Compat layer's single canonical std_session_send_df_frame_begin(). */
	std_session_send_df_frame_begin(sdi);

	for (i = 0; i < devc->profile->channel_count; i++) {
		bv_get_value(&val, &devc->profile->channels[i].spec, report_ptr);
		feed_queue_analog_submit_one(devc->feeds[i], val, 1);
	}

	/* Local std_session_send_df_frame_end() defined above. */
	std_session_send_df_frame_end(sdi);

	sr_sw_limits_update_frames_read(&devc->limits, 1);
	if (sr_sw_limits_check(&devc->limits))
		sr_dev_acquisition_stop(sdi);
}

SR_PRIV int atorch_probe(struct sr_serial_dev_inst *serial, struct dev_context *devc)
{
	int64_t deadline_us;
	const struct atorch_device_profile *p;
	const uint8_t *msg_ptr;

	devc->wr_idx = 0;
	devc->rd_idx = 0;

	deadline_us = g_get_monotonic_time();
	deadline_us += ATORCH_PROBE_TIMEOUT_MS * 1000;
	while (g_get_monotonic_time() <= deadline_us) {
		msg_ptr = receive_msg(serial, devc);
		if (msg_ptr && msg_ptr[HEADER_MSGTYPE_IDX] == MSG_REPORT) {
			p = find_profile_for_device_type(msg_ptr[PAYLOAD_START_IDX]);
			if (p) {
				devc->profile = p;
				return SR_OK;
			}
			sr_err("Unrecognized device type (0x%.4" PRIx8 ").",
			       devc->buf[PAYLOAD_START_IDX]);
			return SR_ERR;
		}
		g_usleep(100 * 1000);
	}
	return SR_ERR;
}

/*
 * PXView note: the signature changed from (int, int, void *cb_data) to
 * (int, int, const struct sr_dev_inst *sdi) to match
 * sr_receive_data_callback_t (libsigrok-internal.h). The body drops the
 * former cb_data dereference and uses sdi directly. sr_dev_acquisition_stop()
 * is provided as a static inline by compat.h taking a const sdi.
 */
SR_PRIV int atorch_receive_data_callback(int fd, int revents,
		const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	const uint8_t *msg_ptr;

	(void)fd;

	if (!sdi || !(devc = sdi->priv))
		return TRUE;

	if (revents & G_IO_IN) {
		while ((msg_ptr = receive_msg(sdi->conn, devc))) {
			if (msg_ptr[HEADER_MSGTYPE_IDX] == MSG_REPORT)
				parse_report_msg((struct sr_dev_inst *)sdi, msg_ptr);
		}
	}

	return TRUE;
}
