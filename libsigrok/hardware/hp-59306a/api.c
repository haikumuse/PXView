/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2021 Frank Stettner <frank-stettner@gmx.net>
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

#include "protocol.h"

/*
 * Forward declaration. Must have external linkage (not static) so that
 * hwdriver.c's drivers_list[] entry `&hp_59306a_driver_info` can resolve
 * at link time. The actual definition is at the end of this file.
 * (Matches the fx2lafw/rohde-schwarz-sme-0x pattern; the original sigrok
 * source used `static` here because it relied on SR_REGISTER_DEV_DRIVER
 * section-based registration, which PXView does not use.)
 */
extern struct sr_dev_driver hp_59306a_driver_info;

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
};

static const uint32_t drvopts[] = {
	SR_CONF_MULTIPLEXER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONN | SR_CONF_GET,
	SR_CONF_ENABLED | SR_CONF_SET,
};

static const uint32_t devopts_cg[] = {
	SR_CONF_ENABLED | SR_CONF_SET,
};

static struct sr_dev_inst *probe_device(struct sr_scpi_dev_inst *scpi)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct channel_group_context *cgc;
	size_t idx, nr;
	struct sr_channel_group *cg;
	char cg_name[24];

	/*
	 * The device cannot get identified by means of SCPI queries.
	 * Neither shall non-SCPI requests get emitted before reliable
	 * identification of the device. Assume that we only get here
	 * when user specs led us to believe it's safe to communicate
	 * to the expected kind of device.
	 */

	sdi = g_malloc0(sizeof(*sdi));
	sdi->vendor = g_strdup("Hewlett-Packard");
	sdi->model = g_strdup("59306A");
	sdi->conn = scpi;
	sdi->driver = &hp_59306a_driver_info;
	sdi->inst_type = SR_INST_SCPI;
	if (sr_scpi_connection_id(scpi, &sdi->connection_id) != SR_OK) {
		g_free(sdi->connection_id);
		sdi->connection_id = NULL;
	}

	devc = g_malloc0(sizeof(*devc));
	sdi->priv = devc;

	devc->channel_count = 6;
	for (idx = 0; idx < devc->channel_count; idx++) {
		nr = idx + 1;
		snprintf(cg_name, sizeof(cg_name), "R%zu", nr);
		cgc = g_malloc0(sizeof(*cgc));
		cgc->number = nr;
		cg = sr_channel_group_new(sdi, cg_name, cgc);
		(void)cg;
	}

	return sdi;
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	GSList *devices;
	const char *conn;
	GSList *l;
	struct sr_config *src;

	/*
	 * Only scan for a device when conn= was specified.
	 *
	 * PXView's libsigrok does not provide sr_serial_extract_options(),
	 * so manually walk the options list looking for SR_CONF_CONN (same
	 * pattern as scpi-pps/api.c and motech-lps-30x/api.c).
	 */
	conn = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		if (src->key == SR_CONF_CONN) {
			conn = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	if (!conn)
		return NULL;

	/*
	 * PXView stores the driver private data (struct drv_context *) in
	 * di->priv. The original sigrok used di->context. sr_scpi_scan()
	 * expects a struct drv_context *, so cast di->priv accordingly.
	 */
	devices = sr_scpi_scan((struct drv_context *)di->priv, options,
			probe_device);

	return std_scan_complete_compat(di, devices);
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
	(void)cg;

	if (!sdi || !data)
		return SR_ERR_ARG;

	switch (key) {
	case SR_CONF_CONN:
		*data = g_variant_new_string(sdi->connection_id);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	gboolean on;

	if (!cg) {
		switch (key) {
		case SR_CONF_ENABLED:
			/* Enable/disable all channels at the same time. */
			on = g_variant_get_boolean(data);
			return hp_59306a_switch_cg(sdi, cg, on);
		default:
			return SR_ERR_NA;
		}
	} else {
		switch (key) {
		case SR_CONF_ENABLED:
			on = g_variant_get_boolean(data);
			return hp_59306a_switch_cg(sdi, cg, on);
		default:
			return SR_ERR_NA;
		}
	}

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	if (!cg) {
		switch (key) {
		case SR_CONF_SCAN_OPTIONS:
		case SR_CONF_DEVICE_OPTIONS:
			return STD_CONFIG_LIST(key, data, sdi, cg,
				scanopts, drvopts, devopts);
		default:
			return SR_ERR_NA;
		}
	} else {
		switch (key) {
		case SR_CONF_DEVICE_OPTIONS:
			*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg));
			break;
		default:
			return SR_ERR_NA;
		}
	}

	return SR_OK;
}

/* ===========================================================================
 * PXView compat wrapper layer
 *
 * PXView's sr_dev_driver callbacks have different signatures from standard
 * sigrok's (int vs uint32_t key, extra ch parameter, cb_data in acquisition
 * start/stop, no dev_list/dev_clear fields). These thin wrappers adapt the
 * standard sigrok callbacks above to PXView's expected signatures.
 *
 * Note: This is a relay multiplexer driver. The original sigrok source used
 * std_dummy_dev_acquisition_start (no-op returning SR_OK) and
 * std_dummy_dev_acquisition_stop for the acquisition callbacks. Neither
 * exists in the PXView compat layer, so the wrappers below are no-ops that
 * simply return SR_OK, matching the original semantics.
 * ==========================================================================*/

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *hp_59306a_drv_ptr;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int hp_59306a_compat_init(struct sr_context *sr_ctx)
{
	hp_59306a_drv_ptr = &hp_59306a_driver_info;
	return std_init(hp_59306a_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int hp_59306a_compat_cleanup(void)
{
	/* Clear device instances before tearing down the driver context. */
	std_dev_clear(hp_59306a_drv_ptr);
	return std_cleanup(hp_59306a_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *hp_59306a_compat_scan(GSList *options)
{
	return scan(hp_59306a_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int hp_59306a_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int hp_59306a_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	/*
	 * The original hp-59306a driver does not provide a config_channel_set
	 * callback, so the ch parameter is unused here. SR_CONF_ENABLED is
	 * handled entirely by config_set() based on whether cg is NULL.
	 */
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int hp_59306a_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/*
 * Wrapper: PXView acquisition_start(sdi, cb_data).
 * Original used std_dummy_dev_acquisition_start (no-op). This driver is a
 * relay multiplexer and does not perform data acquisition, so this is a no-op.
 */
static int hp_59306a_compat_acquisition_start(struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)sdi;
	(void)cb_data;
	return SR_OK;
}

/*
 * Wrapper: PXView acquisition_stop(const sdi, cb_data).
 * Original used std_dummy_dev_acquisition_stop. This driver is a relay
 * multiplexer and does not perform data acquisition, so this is a no-op.
 */
static int hp_59306a_compat_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)sdi;
	(void)cb_data;
	return SR_OK;
}

/* PXView-compatible driver info struct */
struct sr_dev_driver hp_59306a_driver_info = {
	.name = "hp-59306a",
	.longname = "HP 59306A",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = hp_59306a_compat_init,
	.cleanup = hp_59306a_compat_cleanup,
	.scan = hp_59306a_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = hp_59306a_compat_config_get,
	.config_set = hp_59306a_compat_config_set,
	.config_list = hp_59306a_compat_config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = hp_59306a_compat_acquisition_start,
	.dev_acquisition_stop = hp_59306a_compat_acquisition_stop,
	.priv = NULL,
};
