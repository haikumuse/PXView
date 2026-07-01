/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2020 Timo Kokkonen <tjko@iki.fi>
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

/*
 * PXView does not provide the sr_sw_limits helpers. Implement them here
 * (matching standard sigrok semantics) so this self-contained driver
 * compiles and behaves correctly.
 */

SR_PRIV void sr_sw_limits_init(struct sr_sw_limits *limits)
{
	memset(limits, 0, sizeof(*limits));
}

SR_PRIV int sr_sw_limits_config_get(const struct sr_sw_limits *limits,
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

SR_PRIV int sr_sw_limits_config_set(struct sr_sw_limits *limits,
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

SR_PRIV void sr_sw_limits_acquisition_start(struct sr_sw_limits *limits)
{
	if (!limits)
		return;
	limits->starttime_ms = g_get_real_time() / 1000;
	limits->samples_read = 0;
}

SR_PRIV void sr_sw_limits_update_samples_read(struct sr_sw_limits *limits,
		uint64_t count)
{
	if (!limits)
		return;
	limits->samples_read += count;
}

SR_PRIV gboolean sr_sw_limits_check(const struct sr_sw_limits *limits)
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
 * Frame begin/end stubs for the compat layer. PXView has the
 * SR_DF_FRAME_BEGIN/SR_DF_FRAME_END packet types, so emit them directly.
 */
SR_PRIV int std_session_send_df_frame_begin(const struct sr_dev_inst *sdi)
{
	struct sr_datafeed_packet packet;

	packet.type = SR_DF_FRAME_BEGIN;
	packet.status = SR_PKT_OK;
	packet.payload = NULL;

	return ds_data_forward(sdi, &packet);
}

SR_PRIV int std_session_send_df_frame_end(const struct sr_dev_inst *sdi)
{
	struct sr_datafeed_packet packet;

	packet.type = SR_DF_FRAME_END;
	packet.status = SR_PKT_OK;
	packet.payload = NULL;

	return ds_data_forward(sdi, &packet);
}

SR_PRIV int rigol_dg_string_to_waveform(
		const struct channel_spec *ch, const char *s, enum waveform_type *wf)
{
	unsigned int i;

	for (i = 0; i < ch->num_waveforms; i++) {
		if (g_ascii_strncasecmp(s, ch->waveforms[i].scpi_name, strlen(ch->waveforms[i].scpi_name)) == 0 ||
				g_ascii_strncasecmp(s, ch->waveforms[i].user_name, strlen(ch->waveforms[i].user_name)) == 0) {
			*wf = ch->waveforms[i].waveform;
			return SR_OK;
		}
	}

	sr_warn("Unknown waveform: %s\n", s);
	return SR_ERR;
}

SR_PRIV const struct waveform_spec *rigol_dg_get_waveform_spec(
		const struct channel_spec *ch, enum waveform_type wf)
{
	const struct waveform_spec *spec;
	unsigned int i;

	spec = NULL;
	for (i = 0; i < ch->num_waveforms; i++) {
		if (ch->waveforms[i].waveform == wf) {
			spec = &ch->waveforms[i];
			break;
		}
	}

	return spec;
}

SR_PRIV int rigol_dg_get_double_param(const struct sr_dev_inst *sdi,
		const struct sr_channel_group *cg, int psg_cmd, double *value)
{
	struct dev_context *devc;
	struct sr_scpi_dev_inst *scpi;
	const char *command;
	GVariant *data;
	gchar *response, **params;
	const gchar *s;
	int ret;

	devc = sdi->priv;
	scpi = sdi->conn;
	data = NULL;
	params = NULL;
	response = NULL;
	ret = SR_ERR_NA;

	command = sr_scpi_cmd_get(devc->cmdset, psg_cmd);
	if (command && *command) {
		sr_scpi_get_opc(scpi);
		ret = sr_scpi_cmd_resp(sdi, devc->cmdset,
			PSG_CMD_SELECT_CHANNEL, cg->name, &data,
			G_VARIANT_TYPE_STRING, psg_cmd, cg->name);
		if (ret == SR_OK) {
			response = g_variant_dup_string(data, NULL);
			g_strstrip(response);
			s = sr_scpi_unquote_string(response);
			sr_spew("Double value is: '%s'", s);

			*value = g_ascii_strtod(s, NULL);
		}
	}

	if (data)
		g_variant_unref(data);
	g_free(response);
	g_strfreev(params);
	return ret;
}

SR_PRIV int rigol_dg_get_channel_state(const struct sr_dev_inst *sdi,
		const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	struct sr_scpi_dev_inst *scpi;
	struct sr_channel *ch;
	struct channel_status *ch_status;
	const char *command;
	GVariant *data;
	gchar *response, **params;
	const gchar *s;
	enum waveform_type wf;
	double freq, ampl, offset, phase;
	int ret;

	devc = sdi->priv;
	scpi = sdi->conn;
	data = NULL;
	params = NULL;
	response = NULL;
	ret = SR_ERR_NA;

	if (!sdi || !cg)
		return SR_ERR_BUG;

	ch = cg->channels->data;
	ch_status = &devc->ch_status[ch->index];

	command = sr_scpi_cmd_get(devc->cmdset, PSG_CMD_GET_SOURCE_NO_PARAM);
	if (command && *command) {
		sr_scpi_get_opc(scpi);
		ret = sr_scpi_cmd_resp(sdi, devc->cmdset,
			PSG_CMD_SELECT_CHANNEL, cg->name, &data,
			G_VARIANT_TYPE_STRING, PSG_CMD_GET_SOURCE_NO_PARAM, cg->name);
		if (ret != SR_OK)
			goto done;
		g_free(response);
		response = g_variant_dup_string(data, NULL);
		g_variant_unref(data);
		data = NULL;
		g_strstrip(response);
		s = sr_scpi_unquote_string(response);
		sr_spew("Channel state: '%s'", s);

		if ((ret = rigol_dg_string_to_waveform(
				&devc->device->channels[ch->index], s, &wf)) != SR_OK)
			goto done;

		ch_status->wf = wf;
		ch_status->wf_spec = rigol_dg_get_waveform_spec(
				&devc->device->channels[ch->index], wf);

		/* Ignore errors on read, keep default value */
		rigol_dg_get_double_param(sdi, cg, PSG_CMD_GET_FREQUENCY, &ch_status->freq);
		rigol_dg_get_double_param(sdi, cg, PSG_CMD_GET_AMPLITUDE, &ch_status->ampl);
		rigol_dg_get_double_param(sdi, cg, PSG_CMD_GET_OFFSET, &ch_status->offset);
		rigol_dg_get_double_param(sdi, cg, PSG_CMD_GET_PHASE, &ch_status->phase);
	}

	command = sr_scpi_cmd_get(devc->cmdset, PSG_CMD_GET_SOURCE);
	if (command && *command) {
		sr_scpi_get_opc(scpi);
		ret = sr_scpi_cmd_resp(sdi, devc->cmdset,
			PSG_CMD_SELECT_CHANNEL, cg->name, &data,
			G_VARIANT_TYPE_STRING, PSG_CMD_GET_SOURCE, cg->name);
		if (ret != SR_OK)
			goto done;
		g_free(response);
		response = g_variant_dup_string(data, NULL);
		g_variant_unref(data);
		data = NULL;
		g_strstrip(response);
		s = sr_scpi_unquote_string(response);
		sr_spew("Channel state: '%s'", s);
		params = g_strsplit(s, ",", 0);
		if (!params[0])
			goto done;

		/* First parameter is the waveform type */
		if (!(s = params[0]))
			goto done;
		if ((ret = rigol_dg_string_to_waveform(
				&devc->device->channels[ch->index], s, &wf)) != SR_OK)
			goto done;

		ch_status->wf = wf;
		ch_status->wf_spec = rigol_dg_get_waveform_spec(
				&devc->device->channels[ch->index], wf);

		/* Second parameter if the frequency (or "DEF" if not applicable) */
		if (!(s = params[1]))
			goto done;
		freq = g_ascii_strtod(s, NULL);
		ch_status->freq = freq;

		/* Third parameter if the amplitude (or "DEF" if not applicable) */
		if (!(s = params[2]))
			goto done;
		ampl = g_ascii_strtod(s, NULL);
		ch_status->ampl = ampl;

		/* Fourth parameter if the offset (or "DEF" if not applicable) */
		if (!(s = params[3]))
			goto done;
		offset = g_ascii_strtod(s, NULL);
		ch_status->offset = offset;

		/* Fifth parameter if the phase (or "DEF" if not applicable) */
		if (!(s = params[4]))
			goto done;
		phase = g_ascii_strtod(s, NULL);
		ch_status->phase = phase;

		ret = SR_OK;
	}

done:
	if (data)
		g_variant_unref(data);
	g_free(response);
	g_strfreev(params);
	return ret;
}

/*
 * Send a single analog sample for one channel.
 *
 * Adapted for PXView: PXView's sr_datafeed_analog uses a flat struct
 * (probes/num_samples/data/mq/unit/mqflags/unit_bits/unit_pitch) instead
 * of standard sigrok's encoding/meaning/spec sub-objects, and the packet
 * requires a status field. Fill the flat struct directly.
 */
static void rigol_dg_send_channel_value(const struct sr_dev_inst *sdi,
		struct sr_channel *ch, double value, enum sr_mq mq,
		enum sr_unit unit, int digits)
{
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	double val;

	(void)digits;

	val = value;
	memset(&analog, 0, sizeof(analog));
	analog.probes = g_slist_append(NULL, ch);
	analog.num_samples = 1;
	analog.data = &val;
	analog.unit_bits = 8 * sizeof(val); /* double */
	analog.unit_pitch = 0;
	analog.mq = mq;
	analog.unit = unit;
	analog.mqflags = 0;

	packet.type = SR_DF_ANALOG;
	packet.status = SR_PKT_OK;
	packet.payload = &analog;
	packet.bExportOriginalData = 0;

	ds_data_forward(sdi, &packet);
	g_slist_free(analog.probes);
}

SR_PRIV int rigol_dg_receive_data(int fd, int revents, void *cb_data)
{
	struct sr_dev_inst *sdi;
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;
	const char *cmd, *s;
	char *response, **params;
	double meas[5];
	GSList *l;
	int i, start_idx, ret;

	(void)fd;
	(void)revents;
	response = NULL;
	params = NULL;

	sdi = cb_data;
	if (!sdi)
		return TRUE;
	scpi = sdi->conn;
	devc = sdi->priv;
	if (!scpi || !devc)
		return TRUE;

	cmd = sr_scpi_cmd_get(devc->cmdset, PSG_CMD_COUNTER_MEASURE);
	if (!cmd || !*cmd)
		return TRUE;

	sr_scpi_get_opc(scpi);
	ret = sr_scpi_get_string(scpi, cmd, &response);
	if (ret != SR_OK) {
		sr_info("Error getting measurement from counter: %d", ret);
		sr_dev_acquisition_stop(sdi);
		return TRUE;
	}
	g_strstrip(response);

	/*
	 * Parse measurement string:
	 *  frequency, period, duty cycle, width+, width-
	 */
	params = g_strsplit(response, ",", 0);
	for (i = 0; i < 5; i++) {
		if (!(s = params[i]))
			goto done;
		meas[i] = g_ascii_strtod(s, NULL);
	}
	sr_spew("%s: freq=%.10E, period=%.10E, duty=%.10E, width+=%.10E,"
		"width-=%.10E", __func__,
		meas[0], meas[1], meas[2], meas[3], meas[4]);

	std_session_send_df_frame_begin(sdi);
	start_idx = devc->device->num_channels;

	/* Frequency */
	l = g_slist_nth(sdi->channels, start_idx++);
	rigol_dg_send_channel_value(sdi, l->data, meas[0], SR_MQ_FREQUENCY,
		SR_UNIT_HERTZ, 10);

	/* Period */
	l = g_slist_nth(sdi->channels, start_idx++);
	rigol_dg_send_channel_value(sdi, l->data, meas[1], SR_MQ_TIME,
		SR_UNIT_SECOND, 10);

	/* Duty Cycle */
	l = g_slist_nth(sdi->channels, start_idx++);
	rigol_dg_send_channel_value(sdi, l->data, meas[2], SR_MQ_DUTY_CYCLE,
		SR_UNIT_PERCENTAGE, 3);

	/* Pulse Width */
	l = g_slist_nth(sdi->channels, start_idx++);
	rigol_dg_send_channel_value(sdi, l->data, meas[3], SR_MQ_PULSE_WIDTH,
		SR_UNIT_SECOND, 10);

	std_session_send_df_frame_end(sdi);
	sr_sw_limits_update_samples_read(&devc->limits, 1);

	if (sr_sw_limits_check(&devc->limits))
		sr_dev_acquisition_stop(sdi);

done:
	g_free(response);
	g_strfreev(params);
	return TRUE;
}
