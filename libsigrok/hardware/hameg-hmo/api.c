/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 poljar (Damir Jelić) <poljarinho@gmail.com>
 * Copyright (C) 2018 Guido Trentalancia <guido@trentalancia.com>
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

extern struct sr_dev_driver hameg_hmo_driver_info;

static const char *manufacturers[] = {
	"HAMEG",
	"Rohde&Schwarz",
};

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_OSCILLOSCOPE,
	SR_CONF_LOGIC_ANALYZER,
};

enum {
	CG_INVALID = -1,
	CG_NONE,
	CG_ANALOG,
	CG_DIGITAL,
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
	sdi->driver = &hameg_hmo_driver_info;
	sdi->inst_type = SR_INST_SCPI;
	sdi->conn = scpi;

	sr_scpi_hw_info_free(hw_info);
	hw_info = NULL;

	devc = g_malloc0(sizeof(struct dev_context));
	sdi->priv = devc;

	if (hmo_init_device(sdi) != SR_OK)
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
	hmo_scope_state_free(devc->model_state);
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

	if (hmo_scope_state_get(sdi) != SR_OK)
		return SR_ERR;

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	return sr_scpi_close(sdi->conn);
}

static int check_channel_group(struct dev_context *devc,
                             const struct sr_channel_group *cg)
{
	const struct scope_config *model;

	model = devc->model_config;

	if (!cg)
		return CG_NONE;

	if (local_std_cg_idx(cg, devc->analog_groups, model->analog_channels) >= 0)
		return CG_ANALOG;

	if (local_std_cg_idx(cg, devc->digital_groups, model->digital_pods) >= 0)
		return CG_DIGITAL;

	sr_err("Invalid channel group specified.");

	return CG_INVALID;
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int cg_type, idx, i;
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
		break;
	case SR_CONF_TIMEBASE:
		*data = g_variant_new("(tt)", (*model->timebases)[state->timebase][0],
		                      (*model->timebases)[state->timebase][1]);
		break;
	case SR_CONF_NUM_VDIV:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if (cg_type != CG_ANALOG)
			return SR_ERR_NA;
		if (local_std_cg_idx(cg, devc->analog_groups, model->analog_channels) < 0)
			return SR_ERR_ARG;
		*data = g_variant_new_int32(model->num_ydivs);
		break;
	case SR_CONF_VDIV:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if (cg_type != CG_ANALOG)
			return SR_ERR_NA;
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
	case SR_CONF_TRIGGER_PATTERN:
		*data = g_variant_new_string(state->trigger_pattern);
		break;
	case SR_CONF_HIGH_RESOLUTION:
		*data = g_variant_new_boolean(state->high_resolution);
		break;
	case SR_CONF_PEAK_DETECTION:
		*data = g_variant_new_boolean(state->peak_detection);
		break;
	case SR_CONF_HORIZ_TRIGGERPOS:
		*data = g_variant_new_double(state->horiz_triggerpos);
		break;
	case SR_CONF_COUPLING:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if (cg_type != CG_ANALOG)
			return SR_ERR_NA;
		if ((idx = local_std_cg_idx(cg, devc->analog_groups, model->analog_channels)) < 0)
			return SR_ERR_ARG;
		*data = g_variant_new_string((*model->coupling_options)[state->analog_channels[idx].coupling]);
		break;
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(state->sample_rate);
		break;
	case SR_CONF_LOGIC_THRESHOLD:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if (cg_type != CG_DIGITAL)
			return SR_ERR_NA;
		if (!model)
			return SR_ERR_ARG;
		if ((idx = local_std_cg_idx(cg, devc->digital_groups, model->digital_pods)) < 0)
			return SR_ERR_ARG;
		*data = g_variant_new_string((*model->logic_threshold)[state->digital_pods[idx].threshold]);
		break;
	case SR_CONF_LOGIC_THRESHOLD_CUSTOM:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if (cg_type != CG_DIGITAL)
			return SR_ERR_NA;
		if (!model)
			return SR_ERR_ARG;
		if ((idx = local_std_cg_idx(cg, devc->digital_groups, model->digital_pods)) < 0)
			return SR_ERR_ARG;
		/* Check if the oscilloscope is currently in custom threshold mode. */
		for (i = 0; i < model->num_logic_threshold; i++) {
			if (!strcmp("USER2", (*model->logic_threshold)[i]))
				if (strcmp("USER2", (*model->logic_threshold)[state->digital_pods[idx].threshold]))
					return SR_ERR_NA;
			if (!strcmp("USER", (*model->logic_threshold)[i]))
				if (strcmp("USER", (*model->logic_threshold)[state->digital_pods[idx].threshold]))
					return SR_ERR_NA;
			if (!strcmp("MAN", (*model->logic_threshold)[i]))
				if (strcmp("MAN", (*model->logic_threshold)[state->digital_pods[idx].threshold]))
					return SR_ERR_NA;
		}
		*data = g_variant_new_double(state->digital_pods[idx].user_threshold);
		break;
	case SR_CONF_LIMIT_FRAMES:
		*data = g_variant_new_uint64(devc->frame_limit);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret, cg_type, idx, i, j;
	char command[MAX_COMMAND_SIZE], command2[MAX_COMMAND_SIZE];
	char float_str[30], *tmp_str;
	struct dev_context *devc;
	const struct scope_config *model;
	struct scope_state *state;
	double tmp_d, tmp_d2;
	gboolean update_sample_rate, tmp_bool;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;

	if ((cg_type = check_channel_group(devc, cg)) == CG_INVALID)
		return SR_ERR;

	model = devc->model_config;
	state = devc->model_state;
	update_sample_rate = FALSE;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
		devc->samples_limit = g_variant_get_uint64(data);
		ret = SR_OK;
		break;
	case SR_CONF_LIMIT_FRAMES:
		devc->frame_limit = g_variant_get_uint64(data);
		ret = SR_OK;
		break;
	case SR_CONF_VDIV:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if ((idx = local_std_u64_tuple_idx(data, *model->vdivs, model->num_vdivs)) < 0)
			return SR_ERR_ARG;
		if ((j = local_std_cg_idx(cg, devc->analog_groups, model->analog_channels)) < 0)
			return SR_ERR_ARG;
		g_ascii_formatd(float_str, sizeof(float_str), "%E",
			(float) (*model->vdivs)[idx][0] / (*model->vdivs)[idx][1]);
		g_snprintf(command, sizeof(command),
		           (*model->scpi_dialect)[SCPI_CMD_SET_VERTICAL_SCALE],
		           j + 1, float_str);
		if (sr_scpi_send(sdi->conn, command) != SR_OK ||
		    sr_scpi_get_opc(sdi->conn) != SR_OK)
			return SR_ERR;
		state->analog_channels[j].vdiv = idx;
		ret = SR_OK;
		break;
	case SR_CONF_TIMEBASE:
		if ((idx = local_std_u64_tuple_idx(data, *model->timebases, model->num_timebases)) < 0)
			return SR_ERR_ARG;
		g_ascii_formatd(float_str, sizeof(float_str), "%E",
			(float) (*model->timebases)[idx][0] / (*model->timebases)[idx][1]);
		g_snprintf(command, sizeof(command),
		           (*model->scpi_dialect)[SCPI_CMD_SET_TIMEBASE],
		           float_str);
		if (sr_scpi_send(sdi->conn, command) != SR_OK ||
		    sr_scpi_get_opc(sdi->conn) != SR_OK)
			return SR_ERR;
		state->timebase = idx;
		ret = SR_OK;
		update_sample_rate = TRUE;
		break;
	case SR_CONF_HORIZ_TRIGGERPOS:
		tmp_d = g_variant_get_double(data);
		if (tmp_d < 0.0 || tmp_d > 1.0)
			return SR_ERR;
		tmp_d2 = -(tmp_d - 0.5) *
			((double) (*model->timebases)[state->timebase][0] /
			(*model->timebases)[state->timebase][1])
		         * model->num_xdivs;
		g_ascii_formatd(float_str, sizeof(float_str), "%E", tmp_d2);
		g_snprintf(command, sizeof(command),
		           (*model->scpi_dialect)[SCPI_CMD_SET_HORIZ_TRIGGERPOS],
		           float_str);
		if (sr_scpi_send(sdi->conn, command) != SR_OK ||
		    sr_scpi_get_opc(sdi->conn) != SR_OK)
			return SR_ERR;
		state->horiz_triggerpos = tmp_d;
		ret = SR_OK;
		break;
	case SR_CONF_TRIGGER_SOURCE:
		if ((idx = local_std_str_idx(data, *model->trigger_sources, model->num_trigger_sources)) < 0)
			return SR_ERR_ARG;
		g_snprintf(command, sizeof(command),
		           (*model->scpi_dialect)[SCPI_CMD_SET_TRIGGER_SOURCE],
		           (*model->trigger_sources)[idx]);
		if (sr_scpi_send(sdi->conn, command) != SR_OK ||
		    sr_scpi_get_opc(sdi->conn) != SR_OK)
			return SR_ERR;
		state->trigger_source = idx;
		ret = SR_OK;
		break;
	case SR_CONF_TRIGGER_SLOPE:
		if ((idx = local_std_str_idx(data, *model->trigger_slopes, model->num_trigger_slopes)) < 0)
			return SR_ERR_ARG;
		g_snprintf(command, sizeof(command),
		           (*model->scpi_dialect)[SCPI_CMD_SET_TRIGGER_SLOPE],
		           (*model->trigger_slopes)[idx]);
		if (sr_scpi_send(sdi->conn, command) != SR_OK ||
		    sr_scpi_get_opc(sdi->conn) != SR_OK)
			return SR_ERR;
		state->trigger_slope = idx;
		ret = SR_OK;
		break;
	case SR_CONF_TRIGGER_PATTERN:
		tmp_str = (char *)g_variant_get_string(data, 0);
		idx = strlen(tmp_str);
		if (idx == 0 || idx > model->analog_channels + model->digital_channels)
			return SR_ERR_ARG;
		g_snprintf(command, sizeof(command),
		           (*model->scpi_dialect)[SCPI_CMD_SET_TRIGGER_PATTERN],
		           tmp_str);
		if (sr_scpi_send(sdi->conn, command) != SR_OK ||
		    sr_scpi_get_opc(sdi->conn) != SR_OK)
			return SR_ERR;
		strncpy(state->trigger_pattern,
		        tmp_str,
		        MAX_ANALOG_CHANNEL_COUNT + MAX_DIGITAL_CHANNEL_COUNT);
		ret = SR_OK;
		break;
	case SR_CONF_HIGH_RESOLUTION:
		tmp_bool = g_variant_get_boolean(data);
		g_snprintf(command, sizeof(command),
		           (*model->scpi_dialect)[SCPI_CMD_SET_HIGH_RESOLUTION],
		           tmp_bool ? "AUTO" : "OFF");
		if (sr_scpi_send(sdi->conn, command) != SR_OK ||
		    sr_scpi_get_opc(sdi->conn) != SR_OK)
			return SR_ERR;
		/* High Resolution mode automatically switches off Peak Detection. */
		if (tmp_bool) {
			g_snprintf(command, sizeof(command),
			           (*model->scpi_dialect)[SCPI_CMD_SET_PEAK_DETECTION],
			           "OFF");
			if (sr_scpi_send(sdi->conn, command) != SR_OK ||
			         sr_scpi_get_opc(sdi->conn) != SR_OK)
				return SR_ERR;
			state->peak_detection = FALSE;
		}
		state->high_resolution = tmp_bool;
		ret = SR_OK;
		break;
	case SR_CONF_PEAK_DETECTION:
		tmp_bool = g_variant_get_boolean(data);
		g_snprintf(command, sizeof(command),
		           (*model->scpi_dialect)[SCPI_CMD_SET_PEAK_DETECTION],
		           tmp_bool ? "AUTO" : "OFF");
		if (sr_scpi_send(sdi->conn, command) != SR_OK ||
		    sr_scpi_get_opc(sdi->conn) != SR_OK)
			return SR_ERR;
		/* Peak Detection automatically switches off High Resolution mode. */
		if (tmp_bool) {
			g_snprintf(command, sizeof(command),
			           (*model->scpi_dialect)[SCPI_CMD_SET_HIGH_RESOLUTION],
			           "OFF");
			if (sr_scpi_send(sdi->conn, command) != SR_OK ||
			         sr_scpi_get_opc(sdi->conn) != SR_OK)
				return SR_ERR;
			state->high_resolution = FALSE;
		}
		state->peak_detection = tmp_bool;
		ret = SR_OK;
		break;
	case SR_CONF_COUPLING:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if ((idx = local_std_str_idx(data, *model->coupling_options, model->num_coupling_options)) < 0)
			return SR_ERR_ARG;
		if ((j = local_std_cg_idx(cg, devc->analog_groups, model->analog_channels)) < 0)
			return SR_ERR_ARG;
		g_snprintf(command, sizeof(command),
		           (*model->scpi_dialect)[SCPI_CMD_SET_COUPLING],
		           j + 1, (*model->coupling_options)[idx]);
		if (sr_scpi_send(sdi->conn, command) != SR_OK ||
		    sr_scpi_get_opc(sdi->conn) != SR_OK)
			return SR_ERR;
		state->analog_channels[j].coupling = idx;
		ret = SR_OK;
		break;
	case SR_CONF_LOGIC_THRESHOLD:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if (cg_type != CG_DIGITAL)
			return SR_ERR_NA;
		if (!model)
			return SR_ERR_ARG;
		if ((idx = local_std_str_idx(data, *model->logic_threshold, model->num_logic_threshold)) < 0)
			return SR_ERR_ARG;
		if ((j = local_std_cg_idx(cg, devc->digital_groups, model->digital_pods)) < 0)
			return SR_ERR_ARG;
		/* Check if the threshold command is based on the POD or digital channel index. */
		if (model->logic_threshold_for_pod)
			i = j + 1;
		else
			i = j * DIGITAL_CHANNELS_PER_POD;
		g_snprintf(command, sizeof(command),
		           (*model->scpi_dialect)[SCPI_CMD_SET_DIG_POD_THRESHOLD],
		           i, (*model->logic_threshold)[idx]);
		if (sr_scpi_send(sdi->conn, command) != SR_OK ||
		    sr_scpi_get_opc(sdi->conn) != SR_OK)
			return SR_ERR;
		state->digital_pods[j].threshold = idx;
		ret = SR_OK;
		break;
	case SR_CONF_LOGIC_THRESHOLD_CUSTOM:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if (cg_type != CG_DIGITAL)
			return SR_ERR_NA;
		if (!model)
			return SR_ERR_ARG;
		if ((j = local_std_cg_idx(cg, devc->digital_groups, model->digital_pods)) < 0)
			return SR_ERR_ARG;
		tmp_d = g_variant_get_double(data);
		if (tmp_d < -2.0 || tmp_d > 8.0)
			return SR_ERR;
		g_ascii_formatd(float_str, sizeof(float_str), "%E", tmp_d);
		/* Check if the threshold command is based on the POD or digital channel index. */
		if (model->logic_threshold_for_pod)
			idx = j + 1;
		else
			idx = j * DIGITAL_CHANNELS_PER_POD;
		/* Try to support different dialects exhaustively. */
		for (i = 0; i < model->num_logic_threshold; i++) {
			if (!strcmp("USER2", (*model->logic_threshold)[i])) {
				g_snprintf(command, sizeof(command),
				           (*model->scpi_dialect)[SCPI_CMD_SET_DIG_POD_USER_THRESHOLD],
				           idx, 2, float_str); /* USER2 */
				g_snprintf(command2, sizeof(command2),
				           (*model->scpi_dialect)[SCPI_CMD_SET_DIG_POD_THRESHOLD],
				           idx, "USER2");
				break;
			}
			if (!strcmp("USER", (*model->logic_threshold)[i])) {
				g_snprintf(command, sizeof(command),
				           (*model->scpi_dialect)[SCPI_CMD_SET_DIG_POD_USER_THRESHOLD],
				           idx, float_str);
				g_snprintf(command2, sizeof(command2),
				           (*model->scpi_dialect)[SCPI_CMD_SET_DIG_POD_THRESHOLD],
				           idx, "USER");
				break;
			}
			if (!strcmp("MAN", (*model->logic_threshold)[i])) {
				g_snprintf(command, sizeof(command),
				           (*model->scpi_dialect)[SCPI_CMD_SET_DIG_POD_USER_THRESHOLD],
				           idx, float_str);
				g_snprintf(command2, sizeof(command2),
				           (*model->scpi_dialect)[SCPI_CMD_SET_DIG_POD_THRESHOLD],
				           idx, "MAN");
				break;
			}
		}
		if (sr_scpi_send(sdi->conn, command) != SR_OK ||
		    sr_scpi_get_opc(sdi->conn) != SR_OK)
			return SR_ERR;
		if (sr_scpi_send(sdi->conn, command2) != SR_OK ||
		    sr_scpi_get_opc(sdi->conn) != SR_OK)
			return SR_ERR;
		state->digital_pods[j].user_threshold = tmp_d;
		ret = SR_OK;
		break;
	default:
		ret = SR_ERR_NA;
		break;
	}

	if (ret == SR_OK && update_sample_rate)
		ret = hmo_update_sample_rate(sdi);

	return ret;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int cg_type = CG_NONE;
	struct dev_context *devc = NULL;
	const struct scope_config *model = NULL;

	if (sdi) {
		devc = sdi->priv;
		if ((cg_type = check_channel_group(devc, cg)) == CG_INVALID)
			return SR_ERR;

		model = devc->model_config;
	}

	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
		*data = std_gvar_array_u32(ARRAY_AND_SIZE(scanopts));
		break;
	case SR_CONF_DEVICE_OPTIONS:
		if (!cg) {
			if (model)
				*data = std_gvar_array_u32(*model->devopts, model->num_devopts);
			else
				*data = std_gvar_array_u32(ARRAY_AND_SIZE(drvopts));
		} else if (cg_type == CG_ANALOG) {
			*data = std_gvar_array_u32(*model->devopts_cg_analog, model->num_devopts_cg_analog);
		} else if (cg_type == CG_DIGITAL) {
			*data = std_gvar_array_u32(*model->devopts_cg_digital, model->num_devopts_cg_digital);
		} else {
			*data = std_gvar_array_u32(NULL, 0);
		}
		break;
	case SR_CONF_COUPLING:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
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
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if (!model)
			return SR_ERR_ARG;
		*data = local_std_gvar_tuple_array_u64(*model->vdivs, model->num_vdivs);
		break;
	case SR_CONF_LOGIC_THRESHOLD:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if (!model)
			return SR_ERR_ARG;
		*data = g_variant_new_strv(*model->logic_threshold, model->num_logic_threshold);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

SR_PRIV int hmo_request_data(const struct sr_dev_inst *sdi)
{
	char command[MAX_COMMAND_SIZE];
	struct sr_channel *ch;
	struct dev_context *devc;
	const struct scope_config *model;

	devc = sdi->priv;
	model = devc->model_config;

	ch = devc->current_channel->data;

	switch (ch->type) {
	case SR_CHANNEL_ANALOG:
		g_snprintf(command, sizeof(command),
		           (*model->scpi_dialect)[SCPI_CMD_GET_ANALOG_DATA],
#ifdef WORDS_BIGENDIAN
		           "MSBF",
#else
		           "LSBF",
#endif
		           ch->index + 1);
		break;
	case SR_CHANNEL_LOGIC:
		g_snprintf(command, sizeof(command),
		           (*model->scpi_dialect)[SCPI_CMD_GET_DIG_DATA],
		           ch->index / DIGITAL_CHANNELS_PER_POD + 1);
		break;
	default:
		sr_err("Invalid channel type.");
		break;
	}

	return sr_scpi_send(sdi->conn, command);
}

static int hmo_check_channels(GSList *channels)
{
	GSList *l;
	struct sr_channel *ch;
	gboolean enabled_chan[MAX_ANALOG_CHANNEL_COUNT];
	gboolean enabled_pod[MAX_DIGITAL_GROUP_COUNT];
	size_t idx;

	/* Preset "not enabled" for all channels / pods. */
	for (idx = 0; idx < ARRAY_SIZE(enabled_chan); idx++)
		enabled_chan[idx] = FALSE;
	for (idx = 0; idx < ARRAY_SIZE(enabled_pod); idx++)
		enabled_pod[idx] = FALSE;

	/*
	 * Determine which channels / pods are required for the caller's
	 * specified configuration.
	 */
	for (l = channels; l; l = l->next) {
		ch = l->data;
		switch (ch->type) {
		case SR_CHANNEL_ANALOG:
			idx = ch->index;
			if (idx < ARRAY_SIZE(enabled_chan))
				enabled_chan[idx] = TRUE;
			break;
		case SR_CHANNEL_LOGIC:
			idx = ch->index / DIGITAL_CHANNELS_PER_POD;
			if (idx < ARRAY_SIZE(enabled_pod))
				enabled_pod[idx] = TRUE;
			break;
		default:
			return SR_ERR;
		}
	}

	/*
	 * Check for resource conflicts. Some channels can be either
	 * analog or digital, but never both at the same time.
	 *
	 * Note that the constraints might depend on the specific model.
	 * These tests might need some adjustment when support for more
	 * models gets added to the driver.
	 */
	if (enabled_pod[0] && enabled_chan[2])
		return SR_ERR;
	if (enabled_pod[1] && enabled_chan[3])
		return SR_ERR;
	return SR_OK;
}

static int hmo_setup_channels(const struct sr_dev_inst *sdi)
{
	GSList *l;
	unsigned int i;
	gboolean *pod_enabled, setup_changed;
	char command[MAX_COMMAND_SIZE];
	struct scope_state *state;
	const struct scope_config *model;
	struct sr_channel *ch;
	struct dev_context *devc;
	struct sr_scpi_dev_inst *scpi;
	int ret;

	devc = sdi->priv;
	scpi = sdi->conn;
	state = devc->model_state;
	model = devc->model_config;
	setup_changed = FALSE;

	pod_enabled = g_try_malloc0(sizeof(gboolean) * model->digital_pods);

	for (l = sdi->channels; l; l = l->next) {
		ch = l->data;
		switch (ch->type) {
		case SR_CHANNEL_ANALOG:
			if (ch->enabled == state->analog_channels[ch->index].state)
				break;
			g_snprintf(command, sizeof(command),
			           (*model->scpi_dialect)[SCPI_CMD_SET_ANALOG_CHAN_STATE],
			           ch->index + 1, ch->enabled);

			if (sr_scpi_send(scpi, command) != SR_OK) {
				g_free(pod_enabled);
				return SR_ERR;
			}
			state->analog_channels[ch->index].state = ch->enabled;
			setup_changed = TRUE;
			break;
		case SR_CHANNEL_LOGIC:
			/*
			 * A digital POD needs to be enabled for every group of
			 * DIGITAL_CHANNELS_PER_POD channels.
			 */
			if (ch->enabled)
				pod_enabled[ch->index / DIGITAL_CHANNELS_PER_POD] = TRUE;

			if (ch->enabled == state->digital_channels[ch->index])
				break;
			g_snprintf(command, sizeof(command),
			           (*model->scpi_dialect)[SCPI_CMD_SET_DIG_CHAN_STATE],
			           ch->index, ch->enabled);

			if (sr_scpi_send(scpi, command) != SR_OK) {
				g_free(pod_enabled);
				return SR_ERR;
			}

			state->digital_channels[ch->index] = ch->enabled;
			setup_changed = TRUE;
			break;
		default:
			g_free(pod_enabled);
			return SR_ERR;
		}
	}

	ret = SR_OK;
	for (i = 0; i < model->digital_pods; i++) {
		if (state->digital_pods[i].state == pod_enabled[i])
			continue;
		g_snprintf(command, sizeof(command),
		           (*model->scpi_dialect)[SCPI_CMD_SET_DIG_POD_STATE],
		           i + 1, pod_enabled[i]);
		if (sr_scpi_send(scpi, command) != SR_OK) {
			ret = SR_ERR;
			break;
		}
		state->digital_pods[i].state = pod_enabled[i];
		setup_changed = TRUE;
	}
	g_free(pod_enabled);
	if (ret != SR_OK)
		return ret;

	if (setup_changed && hmo_update_sample_rate(sdi) != SR_OK)
		return SR_ERR;

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	GSList *l;
	gboolean digital_added[MAX_DIGITAL_GROUP_COUNT];
	size_t group, pod_count;
	struct sr_channel *ch;
	struct dev_context *devc;
	struct sr_scpi_dev_inst *scpi;
	int ret;

	scpi = sdi->conn;
	devc = sdi->priv;

	devc->num_samples = 0;
	devc->num_frames = 0;

	/* Preset empty results. */
	for (group = 0; group < ARRAY_SIZE(digital_added); group++)
		digital_added[group] = FALSE;
	g_slist_free(devc->enabled_channels);
	devc->enabled_channels = NULL;

	/*
	 * Contruct the list of enabled channels. Determine the highest
	 * number of digital pods involved in the acquisition.
	 */
	pod_count = 0;
	for (l = sdi->channels; l; l = l->next) {
		ch = l->data;
		if (!ch->enabled)
			continue;

		/* Only add a single digital channel per group (pod). */
		group = ch->index / DIGITAL_CHANNELS_PER_POD;
		if (ch->type != SR_CHANNEL_LOGIC || !digital_added[group]) {
			devc->enabled_channels = g_slist_append(
				        devc->enabled_channels, ch);
			if (ch->type == SR_CHANNEL_LOGIC) {
				digital_added[group] = TRUE;
				if (pod_count < group + 1)
					pod_count = group + 1;
			}
		}
	}
	if (!devc->enabled_channels)
		return SR_ERR;
	devc->pod_count = pod_count;
	devc->logic_data = NULL;

	/*
	 * Check constraints. Some channels can be either analog or
	 * digital, but not both at the same time.
	 */
	if (hmo_check_channels(devc->enabled_channels) != SR_OK) {
		sr_err("Invalid channel configuration specified!");
		ret = SR_ERR_NA;
		goto free_enabled;
	}

	/*
	 * Configure the analog and digital channels and the
	 * corresponding digital pods.
	 */
	if (hmo_setup_channels(sdi) != SR_OK) {
		sr_err("Failed to setup channel configuration!");
		ret = SR_ERR;
		goto free_enabled;
	}

	/*
	 * Start acquisition on the first enabled channel. The
	 * receive routine will continue driving the acquisition.
	 */
	sr_scpi_source_add(sdi->session, scpi, G_IO_IN, 50,
			hmo_receive_data, (void *)sdi);

	std_session_send_df_header(sdi, NULL);

	devc->current_channel = devc->enabled_channels;

	return hmo_request_data(sdi);

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

	devc->num_samples = 0;
	devc->num_frames = 0;
	g_slist_free(devc->enabled_channels);
	devc->enabled_channels = NULL;
	scpi = sdi->conn;
	sr_scpi_source_remove(sdi->session, scpi);

	return SR_OK;
}

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *hameg_hmo_drv_ptr;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int hameg_hmo_compat_init(struct sr_context *sr_ctx)
{
	hameg_hmo_drv_ptr = &hameg_hmo_driver_info;
	return std_init(hameg_hmo_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int hameg_hmo_compat_cleanup(void)
{
	/* Call dev_clear to free per-device resources before cleanup */
	dev_clear(hameg_hmo_drv_ptr);
	return std_cleanup(hameg_hmo_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *hameg_hmo_compat_scan(GSList *options)
{
	return scan(hameg_hmo_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int hameg_hmo_compat_config_get(int id, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel *ch,
		const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int hameg_hmo_compat_config_set(int id, GVariant *data,
		struct sr_dev_inst *sdi, struct sr_channel *ch,
		struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int hameg_hmo_compat_config_list(int info_id, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int hameg_hmo_compat_acquisition_start(struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int hameg_hmo_compat_acquisition_stop(const struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop((struct sr_dev_inst *)sdi);
}

/* PXView-compatible driver info struct */
struct sr_dev_driver hameg_hmo_driver_info = {
	.name = "hameg-hmo",
	.longname = "Hameg HMO",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = hameg_hmo_compat_init,
	.cleanup = hameg_hmo_compat_cleanup,
	.scan = hameg_hmo_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = hameg_hmo_compat_config_get,
	.config_set = hameg_hmo_compat_config_set,
	.config_list = hameg_hmo_compat_config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = hameg_hmo_compat_acquisition_start,
	.dev_acquisition_stop = hameg_hmo_compat_acquisition_stop,
	.priv = NULL,
};
