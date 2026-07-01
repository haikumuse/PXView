/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2018 Bastian Schmitz <bastian.schmitz@udo.edu>
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
 * Forward declaration. Must have external linkage (not static) so that
 * hwdriver.c's drivers_list[] entry `&gwinstek_gpd_driver_info` can resolve
 * at link time. The actual definition is at the end of this file.
 * (Matches the hp-3457a / colead-slm / rdtech-um pattern; the original
 * sigrok source used `static` here because it relied on
 * SR_REGISTER_DEV_DRIVER section-based registration, which PXView does
 * not use.)
 */
extern struct sr_dev_driver gwinstek_gpd_driver_info;

#define IDN_RETRIES 3 /* at least 2 */

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_POWER_SUPPLY,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_CHANNEL_CONFIG | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_ENABLED | SR_CONF_GET | SR_CONF_SET,
};

static const uint32_t devopts_cg[] = {
	SR_CONF_VOLTAGE | SR_CONF_GET,
	SR_CONF_VOLTAGE_TARGET | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_CURRENT | SR_CONF_GET,
	SR_CONF_CURRENT_LIMIT | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

static const char *channel_modes[] = {
	"Independent",
};

static const char *gpd_serialcomms[] = {
	"9600/8n1",
	"57600/8n1",
	"115200/8n1"
};

static const struct gpd_model models[] = {
	{ GPD_2303S, "GPD-2303S",
		CHANMODE_INDEPENDENT,
		2,
		{
			/* Channel 1 */
			{ { 0, 30, 0.001 }, { 0, 3, 0.001 } },
			/* Channel 2 */
			{ { 0, 30, 0.001 }, { 0, 3, 0.001 } },
		},
	},
	{ GPD_3303S, "GPD-3303S",
		CHANMODE_INDEPENDENT,
		2,
		{
			/* Channel 1 */
			{ { 0, 32, 0.001 }, { 0, 3.2, 0.001 } },
			/* Channel 2 */
			{ { 0, 32, 0.001 }, { 0, 3.2, 0.001 } },
		},
	},
};

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	const char *conn, *serialcomm, **serialcomms;
	const struct gpd_model *model;
	const struct sr_config *src;
	struct sr_channel *ch;
	struct sr_channel_group *cg;
	GSList *l;
	struct sr_serial_dev_inst *serial;
	struct sr_dev_inst *sdi;
	char reply[100];
	unsigned int i, b, serialcomms_count;
	struct dev_context *devc;
	char channel[10];
	GRegex *regex;
	GMatchInfo *match_info;
	unsigned int cc_cv_ch1, cc_cv_ch2, track1, track2, beep, baud1, baud2;

	serial = NULL;
	match_info = NULL;
	regex = NULL;
	conn = NULL;
	serialcomm = NULL;

	/*
	 * PXView's libsigrok does not provide sr_serial_extract_options(), so
	 * manually walk the options list looking for SR_CONF_CONN and
	 * SR_CONF_SERIALCOMM (same pattern as colead-slm, hp-59306a, ...).
	 */
	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case SR_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		case SR_CONF_SERIALCOMM:
			serialcomm = g_variant_get_string(src->data, NULL);
			break;
		}
	}

	if (!conn)
		return NULL;
	if (serialcomm) {
		serialcomms = &serialcomm;
		serialcomms_count = 1;
	} else {
		serialcomms = gpd_serialcomms;
		serialcomms_count = sizeof(gpd_serialcomms) / sizeof(gpd_serialcomms[0]);
	}

	for( b = 0; b < serialcomms_count; b++) {
		serialcomm = serialcomms[b];
		sr_info("Probing serial port %s @ %s", conn, serialcomm);
		serial = sr_serial_dev_inst_new(conn, serialcomm);
		if (serial_open(serial, SERIAL_RDWR) != SR_OK)
			continue;

		/*
		 * Problem: we need to clear the GPD receive buffer before we
		 * can expect it to process commands correctly.
		 *
		 * Do not just send a newline, since that may cause it to
		 * execute a currently buffered command.
		 *
		 * Solution: Send identification request a few times.
		 * The first should corrupt any previous buffered command if present
		 * and respond with "Invalid Character." or respond directly with
		 * an identification string.
		 */
		for (i = 0; i < IDN_RETRIES; i++) {
			/* Request the GPD to identify itself */
			gpd_send_cmd(serial, "*IDN?\n");
			if (gpd_receive_reply(serial, reply, sizeof(reply)) == SR_OK) {
				if (0 == strncmp(reply, "GW INSTEK", 9)) {
					break;
				}
			}
		}
		if (i == IDN_RETRIES) {
			sr_err("Device did not reply to identification request.");
			serial_flush(serial);
			goto error;
		}

		/*
		 * Returned identification string is for example:
		 * "GW INSTEK,GPD-2303S,SN:ER915277,V2.10"
		 */
		regex = g_regex_new("GW INSTEK,(.+),SN:(.+),(V.+)", 0, 0, NULL);
		if (!g_regex_match(regex, reply, 0, &match_info)) {
			sr_err("Unsupported model '%s'.", reply);
			goto error;
		}

		model = NULL;
		for (i = 0; i < ARRAY_SIZE(models); i++) {
			if (!strcmp(g_match_info_fetch(match_info, 1), models[i].name)) {
				model = &models[i];
				break;
			}
		}
		if (!model) {
			sr_err("Unsupported model '%s'.", reply);
			goto error;
		}

		sr_info("Detected model '%s'.", model->name);

		sdi = g_malloc0(sizeof(struct sr_dev_inst));
		sdi->status = SR_ST_INACTIVE;
		sdi->vendor = g_strdup("GW Instek");
		sdi->model = g_strdup(model->name);
		sdi->inst_type = SR_INST_SERIAL;
		sdi->conn = serial;

		for (i = 0; i < model->num_channels; i++) {
			snprintf(channel, sizeof(channel), "CH%d", i + 1);
			ch = sr_channel_new(sdi, i, SR_CHANNEL_ANALOG, TRUE, channel);
			cg = sr_channel_group_new(sdi, channel, NULL);
			cg->channels = g_slist_append(NULL, ch);
		}

		devc = g_malloc0(sizeof(struct dev_context));
		sr_sw_limits_init(&devc->limits);
		devc->model = model;
		devc->config = g_malloc0(sizeof(struct per_channel_config)
					 * model->num_channels);
		sdi->priv = devc;

		serial_flush(serial);
		gpd_send_cmd(serial, "STATUS?\n");
		gpd_receive_reply(serial, reply, sizeof(reply));

		if (sscanf(reply, "%1u%1u%1u%1u%1u%1u%1u%1u", &cc_cv_ch1,
				&cc_cv_ch2, &track1, &track2, &beep,
				&devc->output_enabled, &baud1, &baud2) != 8) {
			/* old firmware (< 2.00?) responds with different format */
			if (sscanf(reply, "%1u %1u %1u %1u %1u X %1u X", &cc_cv_ch1,
			       &cc_cv_ch2, &track1, &track2, &beep,
			       &devc->output_enabled) != 6) {
				sr_err("Invalid reply to STATUS: '%s'.", reply);
				goto error;
			}
			/* ignore remaining two lines of status message */
			gpd_receive_reply(serial, reply, sizeof(reply));
			gpd_receive_reply(serial, reply, sizeof(reply));
		}

		for (i = 0; i < model->num_channels; i++) {
			gpd_send_cmd(serial, "ISET%d?\n", i + 1);
			gpd_receive_reply(serial, reply, sizeof(reply));
			if (sscanf(reply, "%f", &devc->config[i].output_current_max) != 1) {
				sr_err("Invalid reply to ISETn?: '%s'.", reply);
				goto error;
			}

			gpd_send_cmd(serial, "VSET%d?\n", i + 1);
			gpd_receive_reply(serial, reply, sizeof(reply));
			if (sscanf(reply, "%f", &devc->config[i].output_voltage_max) != 1) {
				sr_err("Invalid reply to VSETn?: '%s'.", reply);
				goto error;
			}
			gpd_send_cmd(serial, "IOUT%d?\n", i + 1);
			gpd_receive_reply(serial, reply, sizeof(reply));
			if (sscanf(reply, "%f", &devc->config[i].output_current_last) != 1) {
				sr_err("Invalid reply to IOUTn?: '%s'.", reply);
				goto error;
			}
			gpd_send_cmd(serial, "VOUT%d?\n", i + 1);
			gpd_receive_reply(serial, reply, sizeof(reply));
			if (sscanf(reply, "%f", &devc->config[i].output_voltage_last) != 1) {
				sr_err("Invalid reply to VOUTn?: '%s'.", reply);
				goto error;
			}
		}

		return std_scan_complete_compat(di, g_slist_append(NULL, sdi));

	error:
		if (match_info)
			g_match_info_free(match_info);
		if (regex)
			g_regex_unref(regex);
		if (serial)
			serial_close(serial);
	}

	return NULL;
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int channel;
	const struct dev_context *devc;
	const struct sr_channel *ch;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;

	if (!cg) {
		switch (key) {
		case SR_CONF_LIMIT_SAMPLES:
		case SR_CONF_LIMIT_MSEC:
			return sr_sw_limits_config_get(&devc->limits, key, data);
		case SR_CONF_CHANNEL_CONFIG:
			*data = g_variant_new_string(
				channel_modes[devc->channel_mode]);
			break;
		case SR_CONF_ENABLED:
			*data = g_variant_new_boolean(devc->output_enabled);
			break;
		default:
			return SR_ERR_NA;
		}
	} else {
		ch = cg->channels->data;
		channel = ch->index;
		switch (key) {
		case SR_CONF_VOLTAGE:
			*data = g_variant_new_double(
				devc->config[channel].output_voltage_last);
			break;
		case SR_CONF_VOLTAGE_TARGET:
			*data = g_variant_new_double(
				devc->config[channel].output_voltage_max);
			break;
		case SR_CONF_CURRENT:
			*data = g_variant_new_double(
				devc->config[channel].output_current_last);
			break;
		case SR_CONF_CURRENT_LIMIT:
			*data = g_variant_new_double(
				devc->config[channel].output_current_max);
			break;
		default:
			return SR_ERR_NA;
		}
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret, channel;
	const struct sr_channel *ch;
	double dval;
	gboolean bval;
	struct dev_context *devc;

	devc = sdi->priv;

	ret = SR_OK;

	switch (key) {
	case SR_CONF_LIMIT_MSEC:
	case SR_CONF_LIMIT_SAMPLES:
		return sr_sw_limits_config_set(&devc->limits, key, data);
	case SR_CONF_ENABLED:
		bval = g_variant_get_boolean(data);
		gpd_send_cmd(sdi->conn, "OUT%c\n", bval ? '1' : '0');
		devc->output_enabled = bval;
		break;
	case SR_CONF_VOLTAGE_TARGET:
		ch = cg->channels->data;
		channel = ch->index;
		dval = g_variant_get_double(data);
		if (dval < devc->model->channels[channel].voltage[0]
		    || dval > devc->model->channels[channel].voltage[1])
			return SR_ERR_ARG;
		gpd_send_cmd(sdi->conn, "VSET%d:%05.3lf\n", channel + 1, dval);
		devc->config[channel].output_voltage_max = dval;
		break;
	case SR_CONF_CURRENT_LIMIT:
		ch = cg->channels->data;
		channel = ch->index;
		dval = g_variant_get_double(data);
		if (dval < devc->model->channels[channel].current[0]
		    || dval > devc->model->channels[channel].current[1])
			return SR_ERR_ARG;
		gpd_send_cmd(sdi->conn, "ISET%d:%05.3lf\n", channel + 1, dval);
		devc->config[channel].output_current_max = dval;
		break;
	default:
		ret = SR_ERR_NA;
		break;
	}

	return ret;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	const struct dev_context *devc;
	const struct sr_channel *ch;
	int channel;

	devc = (sdi) ? sdi->priv : NULL;

	if (!cg) {
		switch (key) {
		case SR_CONF_SCAN_OPTIONS:
		case SR_CONF_DEVICE_OPTIONS:
			return STD_CONFIG_LIST(key, data, sdi, cg, scanopts,
					       drvopts, devopts);
		case SR_CONF_CHANNEL_CONFIG:
			*data = g_variant_new_strv(ARRAY_AND_SIZE(channel_modes));
			break;
		default:
			return SR_ERR_NA;
		}
	} else {
		ch = cg->channels->data;
		channel = ch->index;

		switch (key) {
		case SR_CONF_DEVICE_OPTIONS:
			*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg));
			break;
		case SR_CONF_VOLTAGE_TARGET:
			/*
			 * PXView's compat layer provides std_gvar_min_max_step()
			 * (three separate args), not the std_gvar_min_max_step_array()
			 * helper the original used. Unpack the channel_spec triple
			 * manually. Same pattern as atten-pps3xxx, gwinstek-psp.
			 */
			*data = std_gvar_min_max_step(
				devc->model->channels[channel].voltage[0],
				devc->model->channels[channel].voltage[1],
				devc->model->channels[channel].voltage[2]);
			break;
		case SR_CONF_CURRENT_LIMIT:
			*data = std_gvar_min_max_step(
				devc->model->channels[channel].current[0],
				devc->model->channels[channel].current[1],
				devc->model->channels[channel].current[2]);
			break;
		default:
			return SR_ERR_NA;
		}
	}

	return SR_OK;
}

/*
 * PXView note: serial_source_add() is provided by compat_serial.c and uses
 * the 5-arg signature (no leading session parameter), matching the rule
 * that PXView session-source helpers take no session first arg. The
 * callback gpd_receive_data now has PXView's const-sdi signature.
 * std_session_send_df_header() is the 2-arg compat form (sdi, prefix);
 * the original sigrok call used a 1-arg form.
 */
static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;

	devc = sdi->priv;

	sr_sw_limits_acquisition_start(&devc->limits);
	std_session_send_df_header(sdi, LOG_PREFIX);

	devc->reply_pending = FALSE;
	devc->req_sent_at = 0;
	serial = sdi->conn;
	serial_source_add(serial, G_IO_IN, 100,
			  gpd_receive_data, sdi);

	return SR_OK;
}

/*
 * PXView does not provide std_serial_dev_acquisition_stop. Implement the
 * stop sequence inline: remove the serial source, close the port, and send
 * the DF_END packet. Same pattern as colead-slm, rdtech-um, appa-55ii.
 */
static int dev_acquisition_stop(const struct sr_dev_inst *sdi)
{
	struct sr_serial_dev_inst *serial;

	if (!sdi)
		return SR_ERR_ARG;

	serial = sdi->conn;

	serial_source_remove(serial);
	serial_close(serial);

	std_session_send_df_end(sdi, LOG_PREFIX);

	return SR_OK;
}

/* ===========================================================================
 * PXView compat wrapper layer
 *
 * PXView's sr_dev_driver callbacks have different signatures from standard
 * sigrok's: init/cleanup/scan lack the driver parameter, config_get/set/list
 * use int keys and carry an extra sr_channel parameter, and
 * dev_acquisition_start/stop carry an extra cb_data parameter. The wrappers
 * below adapt the PXView signatures to the standard sigrok-style functions
 * defined above.
 *
 * Note: The original gwinstek-gpd driver does not provide a
 * config_channel_set callback, so the ch parameter in config_set is unused
 * (SR_CONF_* keys are handled entirely by config_set() based on the key
 * and cg).
 * ========================================================================== */

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *gwinstek_gpd_drv_ptr;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int gwinstek_gpd_compat_init(struct sr_context *sr_ctx)
{
	gwinstek_gpd_drv_ptr = &gwinstek_gpd_driver_info;
	return std_init(gwinstek_gpd_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int gwinstek_gpd_compat_cleanup(void)
{
	/* Clear device instances before tearing down the driver context. */
	std_dev_clear(gwinstek_gpd_drv_ptr);
	return std_cleanup(gwinstek_gpd_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *gwinstek_gpd_compat_scan(GSList *options)
{
	return scan(gwinstek_gpd_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int gwinstek_gpd_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int gwinstek_gpd_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	/*
	 * The original gwinstek-gpd driver does not provide a
	 * config_channel_set callback, so the ch parameter is unused here.
	 * SR_CONF_* keys are handled entirely by config_set() based on the
	 * key and cg.
	 */
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int gwinstek_gpd_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int gwinstek_gpd_compat_acquisition_start(struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int gwinstek_gpd_compat_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop(sdi);
}

/* PXView-compatible driver info struct */
struct sr_dev_driver gwinstek_gpd_driver_info = {
	.name = "gwinstek-gpd",
	.longname = "GW Instek GPD",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = gwinstek_gpd_compat_init,
	.cleanup = gwinstek_gpd_compat_cleanup,
	.scan = gwinstek_gpd_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = gwinstek_gpd_compat_config_get,
	.config_set = gwinstek_gpd_compat_config_set,
	.config_list = gwinstek_gpd_compat_config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = gwinstek_gpd_compat_acquisition_start,
	.dev_acquisition_stop = gwinstek_gpd_compat_acquisition_stop,
	.priv = NULL,
};
