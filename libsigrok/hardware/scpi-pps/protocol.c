/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2019 Frank Stettner <frank-stettner@gmx.net>
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
#include <strings.h>
#include <stdarg.h>
#include "protocol.h"

/*
 * Local implementation of sr_next_enabled_channel() (PXView does not
 * provide it). Returns the next enabled channel after cur_channel, wrapping
 * around to the start of the list. If cur_channel is NULL the first enabled
 * channel is returned. If no other enabled channel exists (only cur_channel
 * itself is enabled, or none), cur_channel is returned.
 *
 * Exposed via SR_PRIV (declared in protocol.h) so api.c's
 * dev_acquisition_start() can call it. The function name is driver-specific
 * (scpi_pps_next_enabled_channel) so it cannot collide with the agilent-dmm
 * compat driver's sr_next_enabled_channel symbol at link time, even on
 * Windows where SR_PRIV is empty (default visibility).
 */
SR_PRIV struct sr_channel *scpi_pps_next_enabled_channel(
		const struct sr_dev_inst *sdi, struct sr_channel *cur_channel)
{
	GSList *l;
	struct sr_channel *next_channel;

	l = g_slist_find(sdi->channels, cur_channel);
	if (l)
		l = l->next;
	if (!l)
		l = sdi->channels;

	while (l) {
		next_channel = l->data;
		if (next_channel->enabled)
			return next_channel;
		l = l->next;
		if (!l)
			l = sdi->channels;
		if (l->data == cur_channel)
			break;
	}

	return cur_channel;
}

/*
 * PXView's sr_receive_data_callback_t passes the sdi directly as the third
 * argument (const struct sr_dev_inst *sdi) instead of the void *cb_data
 * that standard sigrok uses. Adapt scpi_pps_receive_data accordingly: take
 * sdi as a const pointer and drop the cb_data cast.
 */
SR_PRIV int scpi_pps_receive_data(int fd, int revents,
		const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	const struct scpi_pps *device;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	struct sr_channel *cur_acquisition_channel;
	int channel_group_cmd;
	const char *channel_group_name;
	struct pps_channel *pch;
	const struct channel_spec *ch_spec;
	int ret;
	float f;
	GVariant *gvdata;
	const GVariantType *gvtype;
	int cmd;

	(void)fd;
	(void)revents;

	if (!sdi)
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	if (!(device = devc->device))
		return TRUE;

	cur_acquisition_channel = devc->cur_acquisition_channel;
	if (!cur_acquisition_channel)
		return TRUE;

	pch = cur_acquisition_channel->priv;

	channel_group_cmd = 0;
	channel_group_name = NULL;
	if (g_slist_length(sdi->channel_groups) > 1) {
		channel_group_cmd = SCPI_CMD_SELECT_CHANNEL;
		channel_group_name = pch->hwname;
	}

	/*
	 * When the current channel is the first in the array, perform the device
	 * specific status update first.
	 */
	if (cur_acquisition_channel == scpi_pps_next_enabled_channel(sdi, NULL) &&
		device->update_status) {
		device->update_status(sdi);
	}

	if (pch->mq == SR_MQ_VOLTAGE) {
		gvtype = G_VARIANT_TYPE_DOUBLE;
		cmd = SCPI_CMD_GET_MEAS_VOLTAGE;
	} else if (pch->mq == SR_MQ_FREQUENCY) {
		gvtype = G_VARIANT_TYPE_DOUBLE;
		cmd = SCPI_CMD_GET_MEAS_FREQUENCY;
	} else if (pch->mq == SR_MQ_CURRENT) {
		gvtype = G_VARIANT_TYPE_DOUBLE;
		cmd = SCPI_CMD_GET_MEAS_CURRENT;
	} else if (pch->mq == SR_MQ_POWER) {
		gvtype = G_VARIANT_TYPE_DOUBLE;
		cmd = SCPI_CMD_GET_MEAS_POWER;
	} else {
		return SR_ERR;
	}

	ret = sr_scpi_cmd_resp(sdi, devc->device->commands,
		channel_group_cmd, channel_group_name, &gvdata, gvtype, cmd);

	if (ret != SR_OK)
		return ret;

	if (devc->channels) {
		/* Dynamically-probed devices. */
		ch_spec = &devc->channels[pch->hw_output_idx];
	} else {
		/* Statically-configured devices. */
		ch_spec = &devc->device->channels[pch->hw_output_idx];
	}
	packet.type = SR_DF_ANALOG;
	packet.payload = &analog;
	memset(&analog, 0, sizeof(analog));
	analog.probes = g_slist_append(NULL, cur_acquisition_channel);
	analog.num_samples = 1;
	analog.mq = pch->mq;
	analog.mqflags = pch->mqflags;
	if (pch->mq == SR_MQ_VOLTAGE) {
		analog.unit = SR_UNIT_VOLT;
		analog.digits = ch_spec->voltage[4];
		analog.spec_digits = ch_spec->voltage[3];
	} else if (pch->mq == SR_MQ_CURRENT) {
		analog.unit = SR_UNIT_AMPERE;
		analog.digits = ch_spec->current[4];
		analog.spec_digits = ch_spec->current[3];
	} else if (pch->mq == SR_MQ_POWER) {
		analog.unit = SR_UNIT_WATT;
		analog.digits = ch_spec->power[4];
		analog.spec_digits = ch_spec->power[3];
	} else if (pch->mq == SR_MQ_FREQUENCY) {
		analog.unit = SR_UNIT_HERTZ;
		analog.digits = ch_spec->frequency[4];
		analog.spec_digits = ch_spec->frequency[3];
	}
	f = (float)g_variant_get_double(gvdata);
	g_variant_unref(gvdata);
	analog.data = &f;
	sr_session_send(sdi, &packet);
	g_slist_free(analog.probes);

	/* Next channel. */
	if (g_slist_length(sdi->channels) > 1) {
		devc->cur_acquisition_channel =
			scpi_pps_next_enabled_channel(sdi, devc->cur_acquisition_channel);
	}

	if (devc->cur_acquisition_channel == scpi_pps_next_enabled_channel(sdi, NULL))
		/* First enabled channel, so each channel has been sampled */
		sr_sw_limits_update_samples_read(&devc->limits, 1);

	/* Stop if limits have been hit. */
	if (sr_sw_limits_check(&devc->limits))
		sr_dev_acquisition_stop(sdi);

	return TRUE;
}
