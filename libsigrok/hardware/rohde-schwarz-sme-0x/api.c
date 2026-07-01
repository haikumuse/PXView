/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2016 Vlad Ivanov <vlad.ivanov@lab-systems.ru>
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
 * hwdriver.c's drivers_list[] entry `&rohde_schwarz_sme_0x_driver_info`
 * can resolve at link time. The actual definition is at the end of this
 * file. (Matches the fx2lafw/rigol-dg pattern; the original sigrok source
 * used `static` here because it relied on SR_REGISTER_DEV_DRIVER
 * section-based registration, which PXView does not use.)
 */
extern struct sr_dev_driver rohde_schwarz_sme_0x_driver_info;

static const char *manufacturer = "Rohde&Schwarz";

static const struct rs_device_model device_models[] = {
	{
		.model_str = "SME02",
		.freq_max = SR_GHZ(1.5),
		.freq_min = SR_KHZ(5),
		.power_max = 16,
		.power_min = -144,
	},
	{
		.model_str = "SME03E",
		.freq_max = SR_GHZ(2.2),
		.freq_min = SR_KHZ(5),
		.power_max = 16,
		.power_min = -144,
	},
	{
		.model_str = "SME03A",
		.freq_max = SR_GHZ(3),
		.freq_min = SR_KHZ(5),
		.power_max = 16,
		.power_min = -144,
	},
	{
		.model_str = "SME03",
		.freq_max = SR_GHZ(3),
		.freq_min = SR_KHZ(5),
		.power_max = 16,
		.power_min = -144,
	},
	{
		.model_str = "SME06",
		.freq_max = SR_GHZ(1.5),
		.freq_min = SR_KHZ(5),
		.power_max = 16,
		.power_min = -144,
	}
};

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_SIGNAL_GENERATOR,
};

static const uint32_t devopts[] = {
	SR_CONF_OUTPUT_FREQUENCY | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_AMPLITUDE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

static int rs_init_device(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	uint8_t model_found;

	devc = sdi->priv;
	model_found = 0;

	for (size_t i = 0; i < ARRAY_SIZE(device_models); i++) {
		if (!strcmp(device_models[i].model_str, sdi->model)) {
			model_found = 1;
			devc->model_config = &device_models[i];
			break;
		}
	}

	if (!model_found) {
		sr_dbg("Device %s %s is not supported by this driver.",
			manufacturer, sdi->model);
		return SR_ERR_NA;
	}

	return SR_OK;
}

static struct sr_dev_inst *probe_device(struct sr_scpi_dev_inst *scpi)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_scpi_hw_info *hw_info;

	sdi = NULL;
	devc = NULL;
	hw_info = NULL;

	rs_sme0x_mode_remote(scpi);

	if (sr_scpi_get_hw_id(scpi, &hw_info) != SR_OK)
		goto fail;

	if (strcmp(hw_info->manufacturer, manufacturer) != 0)
		goto fail;

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->vendor = g_strdup(hw_info->manufacturer);
	sdi->model = g_strdup(hw_info->model);
	sdi->version = g_strdup(hw_info->firmware_version);
	sdi->serial_num = g_strdup(hw_info->serial_number);
	sdi->driver = &rohde_schwarz_sme_0x_driver_info;
	sdi->inst_type = SR_INST_SCPI;
	sdi->conn = scpi;

	sr_scpi_hw_info_free(hw_info);
	hw_info = NULL;

	devc = g_malloc0(sizeof(struct dev_context));
	sdi->priv = devc;
	/*
	 * From this point, sdi owns devc. Set the local pointer to NULL so
	 * the fail path's g_free(devc) is a no-op. PXView's sr_dev_inst_free()
	 * frees sdi->priv (unlike standard sigrok's), so without this a
	 * rs_init_device() failure would double-free devc.
	 */
	devc = NULL;

	if (rs_init_device(sdi) != SR_OK)
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
	/*
	 * PXView stores the driver private data (struct drv_context *) in
	 * di->priv. The original sigrok used di->context. sr_scpi_scan()
	 * expects a struct drv_context *, so cast di->priv accordingly.
	 */
	return sr_scpi_scan((struct drv_context *)di->priv, options, probe_device);
}

static int dev_open(struct sr_dev_inst *sdi)
{
	return sr_scpi_open(sdi->conn);
}

static int dev_close(struct sr_dev_inst *sdi)
{
	return sr_scpi_close(sdi->conn);
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	double value_f;

	(void) cg;

	switch (key) {
	case SR_CONF_OUTPUT_FREQUENCY:
		rs_sme0x_get_freq(sdi, &value_f);
		*data = g_variant_new_double(value_f);
		break;
	case SR_CONF_AMPLITUDE:
		rs_sme0x_get_power(sdi, &value_f);
		*data = g_variant_new_double(value_f);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	double value_f;

	(void)cg;

	if (!sdi)
		return SR_ERR_ARG;

	switch (key) {
	case SR_CONF_OUTPUT_FREQUENCY:
		value_f = g_variant_get_double(data);
		rs_sme0x_set_freq(sdi, value_f);
		break;
	case SR_CONF_AMPLITUDE:
		value_f = g_variant_get_double(data);
		rs_sme0x_set_power(sdi, value_f);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	/*
	 * Use std_config_list() directly (PXView compat layer).
	 * The original STD_CONFIG_LIST macro is mapped to a direct call
	 * per PXView migration conventions.
	 */
	return std_config_list(key, data, sdi, cg,
		scanopts, ARRAY_SIZE(scanopts),
		drvopts, ARRAY_SIZE(drvopts),
		devopts, ARRAY_SIZE(devopts));
}

/* ===========================================================================
 * PXView compat wrapper layer
 *
 * PXView's sr_dev_driver callbacks have different signatures from standard
 * sigrok's (int vs uint32_t key, extra ch parameter, cb_data in acquisition
 * start/stop, no dev_list/dev_clear fields). These thin wrappers adapt the
 * standard sigrok callbacks above to PXView's expected signatures.
 *
 * Note: This is a signal generator driver. The original sigrok source used
 * std_dummy_dev_acquisition_start (no-op returning SR_OK) and
 * std_serial_dev_acquisition_stop for the acquisition callbacks. Neither
 * exists in the PXView compat layer, so the wrappers below are no-ops that
 * simply return SR_OK, matching the original semantics.
 * ==========================================================================*/

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *rohde_schwarz_sme_0x_drv_ptr;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int rohde_schwarz_sme_0x_compat_init(struct sr_context *sr_ctx)
{
	rohde_schwarz_sme_0x_drv_ptr = &rohde_schwarz_sme_0x_driver_info;
	return std_init(rohde_schwarz_sme_0x_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int rohde_schwarz_sme_0x_compat_cleanup(void)
{
	/* Clear device instances before tearing down the driver context. */
	std_dev_clear(rohde_schwarz_sme_0x_drv_ptr);
	return std_cleanup(rohde_schwarz_sme_0x_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *rohde_schwarz_sme_0x_compat_scan(GSList *options)
{
	return scan(rohde_schwarz_sme_0x_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int rohde_schwarz_sme_0x_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int rohde_schwarz_sme_0x_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int rohde_schwarz_sme_0x_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/*
 * Wrapper: PXView acquisition_start(sdi, cb_data).
 * Original used std_dummy_dev_acquisition_start (no-op). This driver is a
 * signal generator and does not perform data acquisition, so this is a no-op.
 */
static int rohde_schwarz_sme_0x_compat_acquisition_start(struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)sdi;
	(void)cb_data;
	return SR_OK;
}

/*
 * Wrapper: PXView acquisition_stop(const sdi, cb_data).
 * Original used std_serial_dev_acquisition_stop. This driver is a signal
 * generator and does not perform data acquisition, so this is a no-op.
 */
static int rohde_schwarz_sme_0x_compat_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)sdi;
	(void)cb_data;
	return SR_OK;
}

/* PXView-compatible driver info struct */
struct sr_dev_driver rohde_schwarz_sme_0x_driver_info = {
	.name = "rohde-schwarz-sme-0x",
	.longname = "Rohde&Schwarz SME-0x",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = rohde_schwarz_sme_0x_compat_init,
	.cleanup = rohde_schwarz_sme_0x_compat_cleanup,
	.scan = rohde_schwarz_sme_0x_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = rohde_schwarz_sme_0x_compat_config_get,
	.config_set = rohde_schwarz_sme_0x_compat_config_set,
	.config_list = rohde_schwarz_sme_0x_compat_config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = rohde_schwarz_sme_0x_compat_acquisition_start,
	.dev_acquisition_stop = rohde_schwarz_sme_0x_compat_acquisition_stop,
	.priv = NULL,
};
