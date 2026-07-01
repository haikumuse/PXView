/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2017 Sven Schnelle <svens@stackframe.org>
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
#include <stdlib.h>
#include "protocol.h"

/*
 * SR_CONF_ENABLED is not defined by PXView's libsigrok nor by the compat
 * layer. Provide a local fallback so the SR_CONF_ENABLED case in
 * config_get() compiles. It is not advertised in any devopts[] array, so
 * the value is never queried through the standard config path.
 */
#ifndef SR_CONF_ENABLED
#define SR_CONF_ENABLED 30152
#endif

extern struct sr_dev_driver lecroy_xstream_driver_info;

static const char *manufacturers[] = {
	"LECROY",
};

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
};

static const uint32_t drvopts[] = {
	SR_CONF_OSCILLOSCOPE,
};

static const uint32_t devopts[] = {
	SR_CONF_LIMIT_FRAMES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_SAMPLERATE | SR_CONF_GET,
	SR_CONF_TIMEBASE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_NUM_HDIV | SR_CONF_GET,
	SR_CONF_HORIZ_TRIGGERPOS | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_TRIGGER_SOURCE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_TRIGGER_SLOPE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

static const uint32_t devopts_cg_analog[] = {
	SR_CONF_NUM_VDIV | SR_CONF_GET,
	SR_CONF_VDIV | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_COUPLING | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

/*
 * Local helpers for string/tuple/channel-group index lookups.
 *
 * The compat layer's std_str_idx() has a different signature (it is a
 * config-get helper taking sdi/key/data). Standard sigrok's std_str_idx(),
 * std_str_idx_s(), std_u64_tuple_idx() and std_cg_idx() are simple lookups,
 * so provide them locally to avoid the name collision. std_gvar_tuple_array()
 * in the compat layer only handles string arrays, so a local u64-pair variant
 * is provided for the timebases/vdivs tables.
 */
static int local_std_str_idx(GVariant *data, const char *const strs[], size_t count)
{
	const char *str;
	size_t i;

	str = g_variant_get_string(data, NULL);
	for (i = 0; i < count; i++) {
		if (strcmp(str, strs[i]) == 0)
			return (int)i;
	}
	return -1;
}

static int local_std_str_idx_s(const char *needle, const char *const strs[], size_t count)
{
	size_t i;

	for (i = 0; i < count; i++) {
		if (strcmp(needle, strs[i]) == 0)
			return (int)i;
	}
	return -1;
}

static int local_std_u64_tuple_idx(GVariant *data, const uint64_t vals[][2], size_t count)
{
	uint64_t v0, v1;
	size_t i;

	g_variant_get(data, "(tt)", &v0, &v1);
	for (i = 0; i < count; i++) {
		if (vals[i][0] == v0 && vals[i][1] == v1)
			return (int)i;
	}
	return -1;
}

static int local_std_cg_idx(const struct sr_channel_group *cg,
		struct sr_channel_group **groups, unsigned int num_groups)
{
	unsigned int i;

	if (!cg)
		return -1;

	for (i = 0; i < num_groups; i++) {
		if (groups[i] == cg)
			return (int)i;
	}
	return -1;
}

static GVariant *local_std_gvar_tuple_array_u64(const uint64_t vals[][2], size_t count)
{
	GVariantBuilder gvb;
	size_t i;

	g_variant_builder_init(&gvb, G_VARIANT_TYPE("a(tt)"));
	for (i = 0; i < count; i++)
		g_variant_builder_add(&gvb, "(tt)", vals[i][0], vals[i][1]);

	return g_variant_builder_end(&gvb);
}

static struct sr_dev_inst *probe_device(struct sr_scpi_dev_inst *scpi)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_scpi_hw_info *hw_info;

	sdi = NULL;
	devc = NULL;
	hw_info = NULL;

	if (sr_scpi_get_hw_id(scpi, &hw_info) != SR_OK) {
		sr_info("Couldn't get IDN response.");
		goto fail;
	}

	if (local_std_str_idx_s(hw_info->manufacturer, ARRAY_AND_SIZE(manufacturers)) < 0)
		goto fail;

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->vendor = g_strdup(hw_info->manufacturer);
	sdi->model = g_strdup(hw_info->model);
	sdi->version = g_strdup(hw_info->firmware_version);
	sdi->serial_num = g_strdup(hw_info->serial_number);
	sdi->driver = &lecroy_xstream_driver_info;
	sdi->inst_type = SR_INST_SCPI;
	sdi->conn = scpi;

	sr_scpi_hw_info_free(hw_info);
	hw_info = NULL;

	devc = g_malloc0(sizeof(struct dev_context));
	sdi->priv = devc;

	if (lecroy_xstream_init_device(sdi) != SR_OK)
		goto fail;

	return sdi;

fail:
	sr_scpi_hw_info_free(hw_info);
	sr_dev_inst_free(sdi);
	g_free(devc);

	return NULL;
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	GSList *devices;

	devices = sr_scpi_scan((struct drv_context *)di->priv, options,
			probe_device);

	return std_scan_complete_compat(di, devices);
}

static void clear_helper(struct dev_context *devc)
{
	lecroy_xstream_state_free(devc->model_state);
	g_free(devc->analog_groups);
}

static int dev_clear(const struct sr_dev_driver *di)
{
	/*
	 * std_dev_clear_with_callback is not available in the compat layer.
	 * Inline the clear_helper logic: iterate instances, call clear_helper
	 * on each devc, then use std_dev_clear_compat to free everything.
	 */
	struct compat_drv_context *drvc;
	GSList *l;
	struct sr_dev_inst *sdi;

	if (!di || !di->priv)
		return SR_ERR_ARG;

	drvc = di->priv;
	for (l = drvc->instances; l; l = l->next) {
		sdi = l->data;
		if (sdi->priv)
			clear_helper(sdi->priv);
	}

	return std_dev_clear_compat(di);
}

static int dev_open(struct sr_dev_inst *sdi)
{
	if (sr_scpi_open(sdi->conn) != SR_OK)
		return SR_ERR;

	if (lecroy_xstream_state_get(sdi) != SR_OK)
		return SR_ERR;

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	return sr_scpi_close(sdi->conn);
}

static int config_get(uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int idx;
	struct dev_context *devc;
	const struct scope_config *model;
	struct scope_state *state;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;

	model = devc->model_config;
	state = devc->model_state;
	*data = NULL;

	switch (key) {
	case SR_CONF_NUM_HDIV:
		*data = g_variant_new_int32(model->num_xdivs);
		break;
	case SR_CONF_TIMEBASE:
		*data = g_variant_new("(tt)",
				(*model->timebases)[state->timebase][0],
				(*model->timebases)[state->timebase][1]);
		break;
	case SR_CONF_NUM_VDIV:
		if (local_std_cg_idx(cg, devc->analog_groups, model->analog_channels) < 0)
			return SR_ERR_ARG;
		*data = g_variant_new_int32(model->num_ydivs);
		break;
	case SR_CONF_VDIV:
		if ((idx = local_std_cg_idx(cg, devc->analog_groups, model->analog_channels)) < 0)
			return SR_ERR_ARG;
		*data = g_variant_new("(tt)",
				(*model->vdivs)[state->analog_channels[idx].vdiv][0],
				(*model->vdivs)[state->analog_channels[idx].vdiv][1]);
		break;
	case SR_CONF_TRIGGER_SOURCE:
		*data = g_variant_new_string((*model->trigger_sources)[state->trigger_source]);
		break;
	case SR_CONF_TRIGGER_SLOPE:
		*data = g_variant_new_string((*model->trigger_slopes)[state->trigger_slope]);
		break;
	case SR_CONF_HORIZ_TRIGGERPOS:
		*data = g_variant_new_double(state->horiz_triggerpos);
		break;
	case SR_CONF_COUPLING:
		if ((idx = local_std_cg_idx(cg, devc->analog_groups, model->analog_channels)) < 0)
			return SR_ERR_ARG;
		*data = g_variant_new_string((*model->coupling_options)[state->analog_channels[idx].coupling]);
		break;
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(state->sample_rate);
		break;
	case SR_CONF_ENABLED:
		*data = g_variant_new_boolean(FALSE);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret, idx, j;
	char command[MAX_COMMAND_SIZE];
	struct dev_context *devc;
	const struct scope_config *model;
	struct scope_state *state;
	double tmp_d;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;

	model = devc->model_config;
	state = devc->model_state;

	switch (key) {
	case SR_CONF_LIMIT_FRAMES:
		devc->frame_limit = g_variant_get_uint64(data);
		ret = SR_OK;
		break;
	case SR_CONF_TRIGGER_SOURCE:
		if ((idx = local_std_str_idx(data, *model->trigger_sources, model->num_trigger_sources)) < 0)
			return SR_ERR_ARG;
		state->trigger_source = idx;
		g_snprintf(command, sizeof(command),
				"TRIG_SELECT EDGE,SR,%s", (*model->trigger_sources)[idx]);
		ret = sr_scpi_send(sdi->conn, command);
		break;
	case SR_CONF_VDIV:
		if ((idx = local_std_u64_tuple_idx(data, *model->vdivs, model->num_vdivs)) < 0)
			return SR_ERR_ARG;
		if ((j = local_std_cg_idx(cg, devc->analog_groups, model->analog_channels)) < 0)
			return SR_ERR_ARG;
		state->analog_channels[j].vdiv = idx;
		g_snprintf(command, sizeof(command),
				"C%d:VDIV %E", j + 1, (float) (*model->vdivs)[idx][0] / (*model->vdivs)[idx][1]);
		if (sr_scpi_send(sdi->conn, command) != SR_OK || sr_scpi_get_opc(sdi->conn) != SR_OK)
			return SR_ERR;
		ret = SR_OK;
		break;
	case SR_CONF_TIMEBASE:
		if ((idx = local_std_u64_tuple_idx(data, *model->timebases, model->num_timebases)) < 0)
			return SR_ERR_ARG;
		state->timebase = idx;
		g_snprintf(command, sizeof(command),
				"TIME_DIV %E", (float) (*model->timebases)[idx][0] / (*model->timebases)[idx][1]);
		ret = sr_scpi_send(sdi->conn, command);
		break;
	case SR_CONF_HORIZ_TRIGGERPOS:
		tmp_d = g_variant_get_double(data);

		if (tmp_d < 0.0 || tmp_d > 1.0)
			return SR_ERR;

		state->horiz_triggerpos = tmp_d;
		tmp_d = -(tmp_d - 0.5) *
				((double)(*model->timebases)[state->timebase][0] /
				(*model->timebases)[state->timebase][1])
				* model->num_xdivs;

		g_snprintf(command, sizeof(command), "TRIG POS %e S", tmp_d);

		ret = sr_scpi_send(sdi->conn, command);
		break;
	case SR_CONF_TRIGGER_SLOPE:
		if ((idx = local_std_str_idx(data, *model->trigger_slopes, model->num_trigger_slopes)) < 0)
			return SR_ERR_ARG;
		state->trigger_slope = idx;
		g_snprintf(command, sizeof(command),
				"%s:TRIG_SLOPE %s", (*model->trigger_sources)[state->trigger_source],
				(*model->trigger_slopes)[idx]);
		ret = sr_scpi_send(sdi->conn, command);
		break;
	case SR_CONF_COUPLING:
		if ((idx = local_std_str_idx(data, *model->coupling_options, model->num_coupling_options)) < 0)
			return SR_ERR_ARG;
		if ((j = local_std_cg_idx(cg, devc->analog_groups, model->analog_channels)) < 0)
			return SR_ERR_ARG;
		state->analog_channels[j].coupling = idx;
		g_snprintf(command, sizeof(command), "C%d:COUPLING %s",
				j + 1, (*model->coupling_options)[idx]);
		if (sr_scpi_send(sdi->conn, command) != SR_OK || sr_scpi_get_opc(sdi->conn) != SR_OK)
			return SR_ERR;
		ret = SR_OK;
		break;
	default:
		ret = SR_ERR_NA;
		break;
	}

	if (ret == SR_OK)
		ret = sr_scpi_get_opc(sdi->conn);

	return ret;
}

static int config_list(uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	const struct scope_config *model;

	devc = (sdi) ? sdi->priv : NULL;
	model = (devc) ? devc->model_config : NULL;

	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
		return std_config_list(key, data, sdi, cg,
				scanopts, ARRAY_SIZE(scanopts),
				NULL, 0,
				NULL, 0);
	case SR_CONF_DEVICE_OPTIONS:
		if (!cg)
			return std_config_list(key, data, sdi, cg,
					NULL, 0,
					drvopts, ARRAY_SIZE(drvopts),
					devopts, ARRAY_SIZE(devopts));
		*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg_analog));
		break;
	case SR_CONF_COUPLING:
		if (!model)
			return SR_ERR_ARG;
		*data = g_variant_new_strv(*model->coupling_options, model->num_coupling_options);
		break;
	case SR_CONF_TRIGGER_SOURCE:
		if (!model)
			return SR_ERR_ARG;
		*data = g_variant_new_strv(*model->trigger_sources, model->num_trigger_sources);
		break;
	case SR_CONF_TRIGGER_SLOPE:
		if (!model)
			return SR_ERR_ARG;
		*data = g_variant_new_strv(*model->trigger_slopes, model->num_trigger_slopes);
		break;
	case SR_CONF_TIMEBASE:
		if (!model)
			return SR_ERR_ARG;
		*data = local_std_gvar_tuple_array_u64(*model->timebases, model->num_timebases);
		break;
	case SR_CONF_VDIV:
		if (!model)
			return SR_ERR_ARG;
		*data = local_std_gvar_tuple_array_u64(*model->vdivs, model->num_vdivs);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

SR_PRIV int lecroy_xstream_request_data(const struct sr_dev_inst *sdi)
{
	char command[MAX_COMMAND_SIZE];
	struct sr_channel *ch;
	struct dev_context *devc;

	devc = sdi->priv;

	/*
	 * We may be left with an invalid current_channel if acquisition was
	 * already stopped but we are processing the last pending events.
	 */
	if (!devc->current_channel)
		return SR_ERR_NA;

	ch = devc->current_channel->data;

	if (ch->type != SR_CHANNEL_ANALOG)
		return SR_ERR;

	g_snprintf(command, sizeof(command), "C%d:WAVEFORM?", ch->index + 1);
	return sr_scpi_send(sdi->conn, command);
}

static int setup_channels(const struct sr_dev_inst *sdi)
{
	GSList *l;
	char command[MAX_COMMAND_SIZE];
	struct scope_state *state;
	struct sr_channel *ch;
	struct dev_context *devc;
	struct sr_scpi_dev_inst *scpi;

	devc = sdi->priv;
	scpi = sdi->conn;
	state = devc->model_state;

	for (l = sdi->channels; l; l = l->next) {
		ch = l->data;
		switch (ch->type) {
		case SR_CHANNEL_ANALOG:
			if (ch->enabled == state->analog_channels[ch->index].state)
				break;
			g_snprintf(command, sizeof(command), "C%d:TRACE %s",
					ch->index + 1, ch->enabled ? "ON" : "OFF");

			if (sr_scpi_send(scpi, command) != SR_OK)
				return SR_ERR;

			state->analog_channels[ch->index].state = ch->enabled;
			break;
		default:
			return SR_ERR;
		}
	}

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	GSList *l;
	struct sr_channel *ch;
	struct dev_context *devc;
	struct scope_state *state;
	int ret;
	struct sr_scpi_dev_inst *scpi;

	devc = sdi->priv;
	scpi = sdi->conn;

	/* Preset empty results. */
	g_slist_free(devc->enabled_channels);
	devc->enabled_channels = NULL;
	state = devc->model_state;
	state->sample_rate = 0;

	/* Contruct the list of enabled channels. */
	for (l = sdi->channels; l; l = l->next) {
		ch = l->data;
		if (!ch->enabled)
			continue;

		devc->enabled_channels = g_slist_append(devc->enabled_channels, ch);
	}

	if (!devc->enabled_channels)
		return SR_ERR;

	/* Configure the analog channels. */
	if (setup_channels(sdi) != SR_OK) {
		sr_err("Failed to setup channel configuration!");
		ret = SR_ERR;
		goto free_enabled;
	}

	/*
	 * Start acquisition on the first enabled channel. The
	 * receive routine will continue driving the acquisition.
	 */
	sr_scpi_source_add(sdi->session, scpi, G_IO_IN, 50,
			lecroy_xstream_receive_data, (void *)sdi);

	std_session_send_df_header(sdi, NULL);

	devc->current_channel = devc->enabled_channels;

	return lecroy_xstream_request_data(sdi);

free_enabled:
	g_slist_free(devc->enabled_channels);
	devc->enabled_channels = NULL;

	return ret;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_scpi_dev_inst *scpi;

	std_session_send_df_end(sdi, NULL);

	devc = sdi->priv;

	devc->num_frames = 0;
	g_slist_free(devc->enabled_channels);
	devc->enabled_channels = NULL;
	scpi = sdi->conn;
	sr_scpi_source_remove(sdi->session, scpi);

	return SR_OK;
}

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *lecroy_xstream_drv_ptr;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int lecroy_xstream_compat_init(struct sr_context *sr_ctx)
{
	lecroy_xstream_drv_ptr = &lecroy_xstream_driver_info;
	return std_init(lecroy_xstream_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int lecroy_xstream_compat_cleanup(void)
{
	/* Call dev_clear to free per-device resources before cleanup */
	dev_clear(lecroy_xstream_drv_ptr);
	return std_cleanup(lecroy_xstream_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *lecroy_xstream_compat_scan(GSList *options)
{
	return scan(lecroy_xstream_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int lecroy_xstream_compat_config_get(int id, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel *ch,
		const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int lecroy_xstream_compat_config_set(int id, GVariant *data,
		struct sr_dev_inst *sdi, struct sr_channel *ch,
		struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int lecroy_xstream_compat_config_list(int info_id, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int lecroy_xstream_compat_acquisition_start(struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int lecroy_xstream_compat_acquisition_stop(const struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop((struct sr_dev_inst *)sdi);
}

/* PXView-compatible driver info struct */
struct sr_dev_driver lecroy_xstream_driver_info = {
	.name = "lecroy-xstream",
	.longname = "LeCroy X-Stream",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = lecroy_xstream_compat_init,
	.cleanup = lecroy_xstream_compat_cleanup,
	.scan = lecroy_xstream_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = lecroy_xstream_compat_config_get,
	.config_set = lecroy_xstream_compat_config_set,
	.config_list = lecroy_xstream_compat_config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = lecroy_xstream_compat_acquisition_start,
	.dev_acquisition_stop = lecroy_xstream_compat_acquisition_stop,
	.priv = NULL,
};
