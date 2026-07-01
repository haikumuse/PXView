/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014 abraxa (Soeren Apel) <soeren@apelpie.net>
 * Based on the Hameg HMO driver by poljar (Damir Jelić) <poljarinho@gmail.com>
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
#include <string.h>
#include "protocol.h"

/*
 * SR_ERR_CHANNEL_GROUP is not defined in PXView's libsigrok.
 * Map it to SR_ERR_ARG (the closest equivalent).
 */
#ifndef SR_ERR_CHANNEL_GROUP
#define SR_ERR_CHANNEL_GROUP SR_ERR_ARG
#endif

/* Forward declaration - the actual struct is defined at the bottom of this file. */
extern struct sr_dev_driver yokogawa_dlm_driver_info;

static const char *MANUFACTURER_ID = "YOKOGAWA";

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
};

static const uint32_t drvopts[] = {
	SR_CONF_LOGIC_ANALYZER,
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

static const uint32_t devopts_cg_digital[] = {
	/* EMPTY */
};

enum {
	CG_INVALID = -1,
	CG_NONE,
	CG_ANALOG,
	CG_DIGITAL,
};

/*
 * Local helper functions for std_str_idx / std_u64_tuple_idx / std_cg_idx.
 * PXView's compat layer versions of these helpers have a different signature
 * (they are config_get helpers with (sdi, key, data, ...) parameters).
 * The standard sigrok versions take (data, vals, count) and return an index,
 * which is what this driver needs in config_set.
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

static int local_std_u64_tuple_idx(GVariant *data, const uint64_t vals[][2],
		size_t count)
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
		struct sr_channel_group **groups, unsigned int count)
{
	unsigned int i;

	if (!cg)
		return -1;

	for (i = 0; i < count; i++) {
		if (cg == groups[i])
			return (int)i;
	}
	return -1;
}

/*
 * Local helper to create a GVariant "a(tt)" array from an array of uint64
 * pairs. PXView's compat layer std_gvar_tuple_array() only handles string
 * arrays, so we provide this local version for the timebases/vdivs tables.
 */
static GVariant *local_std_gvar_tuple_array_u64(const uint64_t vals[][2],
		size_t count)
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
	char *model_name;
	int model_index;

	sdi = NULL;
	devc = NULL;
	hw_info = NULL;

	if (sr_scpi_get_hw_id(scpi, &hw_info) != SR_OK) {
		sr_info("Couldn't get IDN response.");
		goto fail;
	}

	if (strcmp(hw_info->manufacturer, MANUFACTURER_ID) != 0)
		goto fail;

	if (dlm_model_get(hw_info->model, &model_name, &model_index) != SR_OK)
		goto fail;

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->vendor = g_strdup("Yokogawa");
	sdi->model = g_strdup(model_name);
	sdi->version = g_strdup(hw_info->firmware_version);

	sdi->serial_num = g_strdup(hw_info->serial_number);

	sr_scpi_hw_info_free(hw_info);
	hw_info = NULL;

	devc = g_malloc0(sizeof(struct dev_context));

	sdi->driver = &yokogawa_dlm_driver_info;
	sdi->priv = devc;
	sdi->inst_type = SR_INST_SCPI;
	sdi->conn = scpi;

	if (dlm_device_init(sdi, model_index) != SR_OK)
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

	/*
	 * PXView's sr_dev_driver has no ->context field; the drv_context is
	 * stored in ->priv (set by std_init). sr_scpi_scan() expects a
	 * struct drv_context *, so cast di->priv accordingly.
	 */
	devices = sr_scpi_scan((struct drv_context *)di->priv, options,
			probe_device);

	/* Register discovered devices with the driver's instance list. */
	return std_scan_complete_compat(di, devices);
}

static void clear_helper(struct dev_context *devc)
{
	dlm_scope_state_destroy(devc->model_state);
	g_free(devc->analog_groups);
	g_free(devc->digital_groups);
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

	if (dlm_scope_state_query(sdi) != SR_OK)
		return SR_ERR;

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	return sr_scpi_close(sdi->conn);
}

/**
 * Check which category a given channel group belongs to.
 *
 * @param devc Our internal device context.
 * @param cg The channel group to check.
 *
 * @retval CG_NONE cg is NULL
 * @retval CG_ANALOG cg is an analog group
 * @retval CG_DIGITAL cg is a digital group
 * @retval CG_INVALID cg is something else
 */
static int check_channel_group(struct dev_context *devc,
			const struct sr_channel_group *cg)
{
	const struct scope_config *model;

	if (!devc)
		return CG_INVALID;
	model = devc->model_config;

	if (!cg)
		return CG_NONE;

	if (local_std_cg_idx(cg, devc->analog_groups, model->analog_channels) >= 0)
		return CG_ANALOG;

	if (local_std_cg_idx(cg, devc->digital_groups, model->pods) >= 0)
		return CG_DIGITAL;

	sr_err("Invalid channel group specified.");

	return CG_INVALID;
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret, cg_type, idx;
	struct dev_context *devc;
	const struct scope_config *model;
	struct scope_state *state;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;

	if ((cg_type = check_channel_group(devc, cg)) == CG_INVALID)
		return SR_ERR;

	model = devc->model_config;
	state = devc->model_state;

	switch (key) {
	case SR_CONF_NUM_HDIV:
		*data = g_variant_new_int32(model->num_xdivs);
		ret = SR_OK;
		break;
	case SR_CONF_TIMEBASE:
		*data = g_variant_new("(tt)",
				dlm_timebases[state->timebase][0],
				dlm_timebases[state->timebase][1]);
		ret = SR_OK;
		break;
	case SR_CONF_NUM_VDIV:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if (cg_type != CG_ANALOG)
			return SR_ERR_NA;
		*data = g_variant_new_int32(model->num_ydivs);
		ret = SR_OK;
		break;
	case SR_CONF_VDIV:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if (cg_type != CG_ANALOG)
			return SR_ERR_NA;
		if ((idx = local_std_cg_idx(cg, devc->analog_groups, model->analog_channels)) < 0)
			return SR_ERR_ARG;
		*data = g_variant_new("(tt)",
				dlm_vdivs[state->analog_states[idx].vdiv][0],
				dlm_vdivs[state->analog_states[idx].vdiv][1]);
		ret = SR_OK;
		break;
	case SR_CONF_TRIGGER_SOURCE:
		*data = g_variant_new_string((*model->trigger_sources)[state->trigger_source]);
		ret = SR_OK;
		break;
	case SR_CONF_TRIGGER_SLOPE:
		*data = g_variant_new_string(dlm_trigger_slopes[state->trigger_slope]);
		ret = SR_OK;
		break;
	case SR_CONF_HORIZ_TRIGGERPOS:
		*data = g_variant_new_double(state->horiz_triggerpos);
		ret = SR_OK;
		break;
	case SR_CONF_COUPLING:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if (cg_type != CG_ANALOG)
			return SR_ERR_NA;
		if ((idx = local_std_cg_idx(cg, devc->analog_groups, model->analog_channels)) < 0)
			return SR_ERR_ARG;
		*data = g_variant_new_string((*model->coupling_options)[state->analog_states[idx].coupling]);
		ret = SR_OK;
		break;
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(state->sample_rate);
		ret = SR_OK;
		break;
	default:
		ret = SR_ERR_NA;
	}

	return ret;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret, cg_type, idx, j;
	char float_str[30];
	struct dev_context *devc;
	const struct scope_config *model;
	struct scope_state *state;
	double tmp_d;
	gboolean update_sample_rate;

	if (!sdi || !(devc = sdi->priv))
		return SR_ERR_ARG;

	if ((cg_type = check_channel_group(devc, cg)) == CG_INVALID)
		return SR_ERR;

	model = devc->model_config;
	state = devc->model_state;
	update_sample_rate = FALSE;

	switch (key) {
	case SR_CONF_LIMIT_FRAMES:
		devc->frame_limit = g_variant_get_uint64(data);
		ret = SR_OK;
		break;
	case SR_CONF_TRIGGER_SOURCE:
		if ((idx = local_std_str_idx(data, *model->trigger_sources, model->num_trigger_sources)) < 0)
			return SR_ERR_ARG;
		state->trigger_source = idx;
		/* TODO: A and B trigger support possible? */
		ret = dlm_trigger_source_set(sdi->conn, (*model->trigger_sources)[idx]);
		break;
	case SR_CONF_VDIV:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if ((idx = local_std_u64_tuple_idx(data, ARRAY_AND_SIZE(dlm_vdivs))) < 0)
			return SR_ERR_ARG;
		if ((j = local_std_cg_idx(cg, devc->analog_groups, model->analog_channels)) < 0)
			return SR_ERR_ARG;
		state->analog_states[j].vdiv = idx;
		g_ascii_formatd(float_str, sizeof(float_str),
				"%E", (float) dlm_vdivs[idx][0] / dlm_vdivs[idx][1]);
		if (dlm_analog_chan_vdiv_set(sdi->conn, j + 1, float_str) != SR_OK ||
				sr_scpi_get_opc(sdi->conn) != SR_OK)
			return SR_ERR;
		ret = SR_OK;
		break;
	case SR_CONF_TIMEBASE:
		if ((idx = local_std_u64_tuple_idx(data, ARRAY_AND_SIZE(dlm_timebases))) < 0)
			return SR_ERR_ARG;
		state->timebase = idx;
		g_ascii_formatd(float_str, sizeof(float_str),
				"%E", (float) dlm_timebases[idx][0] / dlm_timebases[idx][1]);
		ret = dlm_timebase_set(sdi->conn, float_str);
		update_sample_rate = TRUE;
		break;
	case SR_CONF_HORIZ_TRIGGERPOS:
		tmp_d = g_variant_get_double(data);

		/* TODO: Check if the calculation makes sense for the DLM. */
		if (tmp_d < 0.0 || tmp_d > 1.0)
			return SR_ERR;

		state->horiz_triggerpos = tmp_d;
		tmp_d = -(tmp_d - 0.5) *
				((double) dlm_timebases[state->timebase][0] /
				dlm_timebases[state->timebase][1])
				* model->num_xdivs;

		g_ascii_formatd(float_str, sizeof(float_str), "%E", tmp_d);
		ret = dlm_horiz_trigger_pos_set(sdi->conn, float_str);
		break;
	case SR_CONF_TRIGGER_SLOPE:
		if ((idx = local_std_str_idx(data, ARRAY_AND_SIZE(dlm_trigger_slopes))) < 0)
			return SR_ERR_ARG;
		/* Note: See dlm_trigger_slopes[] in protocol.c. */
		state->trigger_slope = idx;
		ret = dlm_trigger_slope_set(sdi->conn, state->trigger_slope);
		break;
	case SR_CONF_COUPLING:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if ((idx = local_std_str_idx(data, *model->coupling_options, model->num_coupling_options)) < 0)
			return SR_ERR_ARG;
		if ((j = local_std_cg_idx(cg, devc->analog_groups, model->analog_channels)) < 0)
			return SR_ERR_ARG;
		state->analog_states[j].coupling = idx;
		if (dlm_analog_chan_coupl_set(sdi->conn, j + 1, (*model->coupling_options)[idx]) != SR_OK ||
				sr_scpi_get_opc(sdi->conn) != SR_OK)
			return SR_ERR;
		ret = SR_OK;
		break;
	default:
		ret = SR_ERR_NA;
		break;
	}

	if (ret == SR_OK)
		ret = sr_scpi_get_opc(sdi->conn);

	if (ret == SR_OK && update_sample_rate)
		ret = dlm_sample_rate_query(sdi);

	return ret;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int cg_type = CG_NONE;
	struct dev_context *devc;
	const struct scope_config *model;

	devc = (sdi) ? sdi->priv : NULL;
	model = (devc) ? devc->model_config : NULL;

	if (!cg) {
		switch (key) {
		case SR_CONF_SCAN_OPTIONS:
		case SR_CONF_DEVICE_OPTIONS:
			/*
			 * PXView's std_config_list() takes explicit count
			 * arguments instead of the STD_CONFIG_LIST macro.
			 */
			return std_config_list(key, data, sdi, cg,
					scanopts, ARRAY_SIZE(scanopts),
					drvopts, ARRAY_SIZE(drvopts),
					devopts, ARRAY_SIZE(devopts));
		case SR_CONF_TIMEBASE:
			*data = local_std_gvar_tuple_array_u64(ARRAY_AND_SIZE(dlm_timebases));
			return SR_OK;
		case SR_CONF_TRIGGER_SOURCE:
			if (!model)
				return SR_ERR_ARG;
			*data = g_variant_new_strv(*model->trigger_sources, model->num_trigger_sources);
			return SR_OK;
		case SR_CONF_TRIGGER_SLOPE:
			*data = g_variant_new_strv(ARRAY_AND_SIZE(dlm_trigger_slopes));
			return SR_OK;
		case SR_CONF_NUM_HDIV:
			if (!model)
				return SR_ERR_ARG;
			*data = g_variant_new_uint32(model->num_xdivs);
			return SR_OK;
		default:
			return SR_ERR_NA;
		}
	}

	if ((cg_type = check_channel_group(devc, cg)) == CG_INVALID)
		return SR_ERR;

	switch (key) {
	case SR_CONF_DEVICE_OPTIONS:
		if (cg_type == CG_ANALOG) {
			*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg_analog));
			break;
		}
		if (cg_type == CG_DIGITAL) {
			if (!ARRAY_SIZE(devopts_cg_digital)) {
				*data = std_gvar_array_u32(NULL, 0);
				break;
			}
			*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg_digital));
			break;
		}
		*data = std_gvar_array_u32(NULL, 0);
		break;
	case SR_CONF_COUPLING:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		*data = g_variant_new_strv(*model->coupling_options, model->num_coupling_options);
		break;
	case SR_CONF_VDIV:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		*data = local_std_gvar_tuple_array_u64(ARRAY_AND_SIZE(dlm_vdivs));
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int dlm_check_channels(GSList *channels)
{
	GSList *l;
	struct sr_channel *ch;
	gboolean enabled_pod1, enabled_chan4;

	enabled_pod1 = enabled_chan4 = FALSE;

	/* Note: On the DLM2000, CH4 and Logic are shared. */
	/* TODO Handle non-DLM2000 models. */
	for (l = channels; l; l = l->next) {
		ch = l->data;
		switch (ch->type) {
		case SR_CHANNEL_ANALOG:
			if (ch->index == 3)
				enabled_chan4 = TRUE;
			break;
		case SR_CHANNEL_LOGIC:
			enabled_pod1 = TRUE;
			break;
		default:
			return SR_ERR;
		}
	}

	if (enabled_pod1 && enabled_chan4)
		return SR_ERR;

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	GSList *l;
	gboolean digital_added;
	struct sr_channel *ch;
	struct dev_context *devc;
	struct sr_scpi_dev_inst *scpi;

	scpi = sdi->conn;
	devc = sdi->priv;
	digital_added = FALSE;

	g_slist_free(devc->enabled_channels);
	devc->enabled_channels = NULL;

	for (l = sdi->channels; l; l = l->next) {
		ch = l->data;
		if (!ch->enabled)
			continue;
		/* Only add a single digital channel. */
		if (ch->type != SR_CHANNEL_LOGIC || !digital_added) {
			devc->enabled_channels = g_slist_append(
				devc->enabled_channels, ch);
			if (ch->type == SR_CHANNEL_LOGIC)
				digital_added = TRUE;
		}
	}

	if (!devc->enabled_channels)
		return SR_ERR;

	if (dlm_check_channels(devc->enabled_channels) != SR_OK) {
		sr_err("Invalid channel configuration specified!");
		return SR_ERR_NA;
	}

	/* Request data for the first enabled channel. */
	devc->current_channel = devc->enabled_channels;
	dlm_channel_data_request(sdi);

	/*
	 * PXView's sr_receive_data_callback_t passes the sdi directly as the
	 * third argument (const struct sr_dev_inst *) instead of void *.
	 * dlm_data_receive() is declared with that signature in protocol.h,
	 * so no cast is needed here.
	 */
	sr_scpi_source_add(sdi->session, scpi, G_IO_IN, 5,
			dlm_data_receive, (void *)sdi);

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	/*
	 * PXView's std_session_send_df_end takes a (sdi, prefix) signature;
	 * pass NULL for the prefix to match the standard sigrok behavior of
	 * std_session_send_df_end(sdi).
	 */
	std_session_send_df_end(sdi, NULL);

	devc = sdi->priv;

	devc->num_frames = 0;
	g_slist_free(devc->enabled_channels);
	devc->enabled_channels = NULL;

	sr_scpi_source_remove(sdi->session, sdi->conn);

	return SR_OK;
}

/* ------------------------------------------------------------------------ */
/* PXView-compatible wrapper functions and driver_info                       */
/* ------------------------------------------------------------------------ */

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *yokogawa_dlm_drv_ptr;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int yokogawa_dlm_compat_init(struct sr_context *sr_ctx)
{
	yokogawa_dlm_drv_ptr = &yokogawa_dlm_driver_info;
	return std_init(yokogawa_dlm_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int yokogawa_dlm_compat_cleanup(void)
{
	/* Call dev_clear to free per-device resources before cleanup. */
	dev_clear(yokogawa_dlm_drv_ptr);
	return std_cleanup(yokogawa_dlm_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *yokogawa_dlm_compat_scan(GSList *options)
{
	return scan(yokogawa_dlm_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int yokogawa_dlm_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int yokogawa_dlm_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	int ret;

	if (ch) {
		/* Channel state change: propagate to hardware (Rule 10
		 * config_channel_set merge). The original upstream
		 * config_channel_set() delegates to dlm_channel_state_set()
		 * in protocol.c; PXView's sr_dev_driver has no
		 * config_channel_set callback field, so wire it through the
		 * compat config_set(ch) path instead. */
		ret = dlm_channel_state_set(sdi, ch->index, ch->enabled);
		if (ret != SR_OK)
			return ret;
	}

	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int yokogawa_dlm_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int yokogawa_dlm_compat_acquisition_start(struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int yokogawa_dlm_compat_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop((struct sr_dev_inst *)sdi);
}

/* PXView-compatible driver info struct.
 *
 * Note: PXView's sr_dev_driver has no config_channel_set callback field,
 * so the original config_channel_set() logic (which delegates to
 * dlm_channel_state_set() in protocol.c) is wired up through
 * yokogawa_dlm_compat_config_set()'s ch != NULL branch instead.
 */
struct sr_dev_driver yokogawa_dlm_driver_info = {
	.name = "yokogawa-dlm",
	.longname = "Yokogawa DL/DLM",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = yokogawa_dlm_compat_init,
	.cleanup = yokogawa_dlm_compat_cleanup,
	.scan = yokogawa_dlm_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = yokogawa_dlm_compat_config_get,
	.config_set = yokogawa_dlm_compat_config_set,
	.config_list = yokogawa_dlm_compat_config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = yokogawa_dlm_compat_acquisition_start,
	.dev_acquisition_stop = yokogawa_dlm_compat_acquisition_stop,
	.priv = NULL,
};
