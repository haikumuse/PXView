/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2017 John Chajecki <subs@qcontinuum.plus.com>
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
#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include "protocol.h"

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_MULTIMETER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_SET,
};

/* Vendor, model, number of channels, poll period */
static const struct fluke_scpi_dmm_model supported_models[] = {
	{ "FLUKE", "45", 2, 0 },
};

/* Forward declaration - the PXView-compatible driver_info is defined at the
 * bottom of this file. Use extern (not static) so probe_device can reference
 * it before the actual definition. */
extern struct sr_dev_driver fluke_45_driver_info;

static struct sr_dev_inst *probe_device(struct sr_scpi_dev_inst *scpi)
{
	struct dev_context *devc;
	struct sr_dev_inst *sdi;
	struct sr_scpi_hw_info *hw_info;
	const struct scpi_command *cmdset = fluke_45_cmdset;
	unsigned int i;
	const struct fluke_scpi_dmm_model *model = NULL;
	gchar *channel_name;

	/* Get device IDN. */
	if (sr_scpi_get_hw_id(scpi, &hw_info) != SR_OK) {
		sr_scpi_hw_info_free(hw_info);
		sr_info("Couldn't get IDN response, retrying.");
		sr_scpi_close(scpi);
		sr_scpi_open(scpi);
		if (sr_scpi_get_hw_id(scpi, &hw_info) != SR_OK) {
			sr_scpi_hw_info_free(hw_info);
			sr_info("Couldn't get IDN response.");
			return NULL;
		}
	}

	/* Check IDN. */
	for (i = 0; i < ARRAY_SIZE(supported_models); i++) {
		if (!g_ascii_strcasecmp(hw_info->manufacturer,
					supported_models[i].vendor) &&
				!strcmp(hw_info->model, supported_models[i].model)) {
			model = &supported_models[i];
			break;
		}
	}
	if (!model) {
		sr_scpi_hw_info_free(hw_info);
		return NULL;
	}

	/* Set up device parameters. */
	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->vendor = g_strdup(model->vendor);
	sdi->model = g_strdup(model->model);
	sdi->version = g_strdup(hw_info->firmware_version);
	sdi->serial_num = g_strdup(hw_info->serial_number);
	sdi->conn = scpi;
	sdi->driver = &fluke_45_driver_info;
	sdi->inst_type = SR_INST_SCPI;
	sr_scpi_hw_info_free(hw_info);

	devc = g_malloc0(sizeof(struct dev_context));
	sr_sw_limits_init(&devc->limits);
	devc->num_channels = model->num_channels;
	devc->cmdset = cmdset;
	sdi->priv = devc;

	/* Create channels. */
	for (i = 0; i < devc->num_channels; i++) {
		channel_name = g_strdup_printf("P%d", i + 1);
		sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, channel_name);
	}

	return sdi;
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	GSList *devices;

	devices = sr_scpi_scan(di->priv, options, probe_device);

	return std_scan_complete_compat(di, devices);
}

static int dev_open(struct sr_dev_inst *sdi)
{
	struct sr_scpi_dev_inst *scpi;
	int ret;

	scpi = sdi->conn;

	if ((ret = sr_scpi_open(scpi)) < 0) {
		sr_err("Failed to open SCPI device: %s.", sr_strerror(ret));
		return SR_ERR;
	}

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	struct sr_scpi_dev_inst *scpi;

	scpi = sdi->conn;

	if (!scpi)
		return SR_ERR_BUG;

	return sr_scpi_close(scpi);
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	devc = sdi->priv;

	return sr_sw_limits_config_set(&devc->limits, key, data);
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return std_config_list(key, data, sdi, cg,
		scanopts, ARRAY_SIZE(scanopts),
		drvopts, ARRAY_SIZE(drvopts),
		devopts, ARRAY_SIZE(devopts));
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc = sdi->priv;

	(void)cg;

	return sr_sw_limits_config_get(&devc->limits, key, data);
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;
	int ret;

	scpi = sdi->conn;
	devc = sdi->priv;

	sr_sw_limits_acquisition_start(&devc->limits);
	std_session_send_df_header(sdi, NULL);

	if ((ret = sr_scpi_source_add(sdi->session, scpi, G_IO_IN, 10,
			fl45_scpi_receive_data, (void *)sdi)) != SR_OK)
		return ret;

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	struct sr_scpi_dev_inst *scpi;
	double d;

	scpi = sdi->conn;

	/*
	 * A requested value is certainly on the way. Retrieve it now,
	 * to avoid leaving the device in a state where it's not expecting
	 * commands.
	 */
	sr_scpi_get_double(scpi, NULL, &d);
	sr_scpi_source_remove(sdi->session, scpi);

	std_session_send_df_end(sdi, NULL);

	return SR_OK;
}

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *fluke_45_drv_ptr;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int fluke_45_compat_init(struct sr_context *sr_ctx)
{
	fluke_45_drv_ptr = &fluke_45_driver_info;
	return std_init(fluke_45_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int fluke_45_compat_cleanup(void)
{
	return std_cleanup(fluke_45_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *fluke_45_compat_scan(GSList *options)
{
	return scan(fluke_45_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int fluke_45_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int fluke_45_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int fluke_45_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int fluke_45_compat_acquisition_start(struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int fluke_45_compat_acquisition_stop(const struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop((struct sr_dev_inst *)sdi);
}

/* PXView-compatible driver info struct */
struct sr_dev_driver fluke_45_driver_info = {
	.name = "fluke-45",
	.longname = "Fluke 45",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = fluke_45_compat_init,
	.cleanup = fluke_45_compat_cleanup,
	.scan = fluke_45_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = fluke_45_compat_config_get,
	.config_set = fluke_45_compat_config_set,
	.config_list = fluke_45_compat_config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = fluke_45_compat_acquisition_start,
	.dev_acquisition_stop = fluke_45_compat_acquisition_stop,
	.priv = NULL,
};
