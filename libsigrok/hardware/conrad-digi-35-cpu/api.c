/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014 Matthias Heidbrink <m-sigrok@heidbrink.biz>
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

#define SERIALCOMM "9600/8n1"

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_POWER_SUPPLY,
};

static const uint32_t devopts[] = {
	SR_CONF_VOLTAGE_TARGET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_CURRENT_LIMIT | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_OVER_CURRENT_PROTECTION_ENABLED | SR_CONF_SET,
};

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct dev_context *devc;
	struct sr_dev_inst *sdi;
	struct sr_config *src;
	struct sr_serial_dev_inst *serial;
	GSList *l;
	const char *conn, *serialcomm;

	conn = serialcomm = NULL;

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
	if (!serialcomm)
		serialcomm = SERIALCOMM;

	/*
	 * We cannot scan for this device because it is write-only.
	 * So just check that the port parameters are valid and assume that
	 * the device is there.
	 */

	serial = sr_serial_dev_inst_new(conn, serialcomm);

	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		return NULL;

	serial_close(serial);

	sr_spew("Conrad DIGI 35 CPU assumed at %s.", conn);

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup("Conrad");
	sdi->model = g_strdup("DIGI 35 CPU");
	devc = g_malloc0(sizeof(struct dev_context));
	sr_sw_limits_init(&devc->limits);
	sdi->inst_type = SR_INST_SERIAL;
	sdi->conn = serial;
	sdi->priv = devc;

	return std_scan_complete_compat(di, g_slist_append(NULL, sdi));
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	double dblval;

	(void)cg;

	switch (key) {
	case SR_CONF_VOLTAGE_TARGET:
		dblval = g_variant_get_double(data);
		if ((dblval < 0.0) || (dblval > 35.0)) {
			sr_err("Voltage out of range (0 - 35.0)!");
			return SR_ERR_ARG;
		}
		return send_msg1(sdi, 'V', (int) (dblval * 10 + 0.5));
	case SR_CONF_CURRENT_LIMIT:
		dblval = g_variant_get_double(data);
		if ((dblval < 0.00) || (dblval > 2.55)) {
			sr_err("Current out of range (0 - 2.55)!");
			return SR_ERR_ARG;
		}
		return send_msg1(sdi, 'C', (int) (dblval * 100 + 0.5));
	case SR_CONF_OVER_CURRENT_PROTECTION_ENABLED:
		if (g_variant_get_boolean(data))
			return send_msg1(sdi, 'V', 900);
		else /* Constant current mode */
			return send_msg1(sdi, 'V', 901);
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
	case SR_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
	case SR_CONF_VOLTAGE_TARGET:
		*data = std_gvar_min_max_step(0.0, 35.0, 0.1);
		break;
	case SR_CONF_CURRENT_LIMIT:
		*data = std_gvar_min_max_step(0.0, 2.55, 0.01);
		break;
	default:
		return SR_ERR_NA;
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
 * Note: The original driver used std_dummy_dev_acquisition_start/_stop
 * (no-ops returning SR_OK) since this is a write-only power supply that
 * does not stream sample data. Neither helper exists in PXView's compat
 * layer, so the wrappers below are no-ops that match the original semantics.
 * The original driver also had config_get = NULL, so the compat_config_get
 * wrapper returns SR_ERR_NA to preserve that behaviour.
 * ==========================================================================*/

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *conrad_digi_35_cpu_drv_ptr;

/* Forward declaration - defined at end of file */
extern struct sr_dev_driver conrad_digi_35_cpu_driver_info;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int conrad_digi_35_cpu_compat_init(struct sr_context *sr_ctx)
{
	conrad_digi_35_cpu_drv_ptr = &conrad_digi_35_cpu_driver_info;
	return std_init(conrad_digi_35_cpu_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int conrad_digi_35_cpu_compat_cleanup(void)
{
	return std_cleanup(conrad_digi_35_cpu_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *conrad_digi_35_cpu_compat_scan(GSList *options)
{
	return scan(conrad_digi_35_cpu_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg).
 * Original driver had config_get = NULL, so this returns SR_ERR_NA. */
static int conrad_digi_35_cpu_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)id;
	(void)data;
	(void)sdi;
	(void)ch;
	(void)cg;
	return SR_ERR_NA;
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg).
 * No per-channel config_set logic in the original driver, so ch is dropped. */
static int conrad_digi_35_cpu_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int conrad_digi_35_cpu_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/*
 * Wrapper: PXView acquisition_start(sdi, cb_data).
 * Original used std_dummy_dev_acquisition_start (no-op). This driver is a
 * write-only power supply and does not perform data acquisition, so this is
 * a no-op that returns SR_OK.
 */
static int conrad_digi_35_cpu_compat_acquisition_start(struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)sdi;
	(void)cb_data;
	return SR_OK;
}

/*
 * Wrapper: PXView acquisition_stop(const sdi, cb_data).
 * Original used std_dummy_dev_acquisition_stop (no-op). This driver is a
 * write-only power supply and does not perform data acquisition, so this is
 * a no-op that returns SR_OK.
 */
static int conrad_digi_35_cpu_compat_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)sdi;
	(void)cb_data;
	return SR_OK;
}

/* PXView-compatible driver info struct */
struct sr_dev_driver conrad_digi_35_cpu_driver_info = {
	.name = "conrad-digi-35-cpu",
	.longname = "Conrad DIGI 35 CPU",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = conrad_digi_35_cpu_compat_init,
	.cleanup = conrad_digi_35_cpu_compat_cleanup,
	.scan = conrad_digi_35_cpu_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = conrad_digi_35_cpu_compat_config_get,
	.config_set = conrad_digi_35_cpu_compat_config_set,
	.config_list = conrad_digi_35_cpu_compat_config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = conrad_digi_35_cpu_compat_acquisition_start,
	.dev_acquisition_stop = conrad_digi_35_cpu_compat_acquisition_stop,
	.priv = NULL,
};
