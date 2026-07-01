/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2015 Aurelien Jacobs <aurel@gnuage.org>
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

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
	SR_CONF_MODBUSADDR,
};

static const uint32_t drvopts[] = {
	SR_CONF_ELECTRONIC_LOAD,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_GET | SR_CONF_SET,
};

static const uint32_t devopts_cg[] = {
	SR_CONF_ENABLED | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_REGULATION | SR_CONF_GET,
	SR_CONF_VOLTAGE | SR_CONF_GET,
	SR_CONF_VOLTAGE_TARGET | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_CURRENT | SR_CONF_GET,
	SR_CONF_CURRENT_LIMIT | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_OVER_VOLTAGE_PROTECTION_ENABLED | SR_CONF_GET,
	SR_CONF_OVER_VOLTAGE_PROTECTION_ACTIVE | SR_CONF_GET,
	SR_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_OVER_CURRENT_PROTECTION_ENABLED | SR_CONF_GET,
	SR_CONF_OVER_CURRENT_PROTECTION_ACTIVE | SR_CONF_GET,
	SR_CONF_OVER_CURRENT_PROTECTION_THRESHOLD | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_OVER_TEMPERATURE_PROTECTION | SR_CONF_GET,
	SR_CONF_OVER_TEMPERATURE_PROTECTION_ACTIVE | SR_CONF_GET,
};

/*
 * The IDs in this list are only guessed and needs to be verified
 * against some real hardware. If at least a few of them matches,
 * it will probably be safe to enable the others.
 */
static const struct maynuo_m97_model supported_models[] = {
//	{  53, "M9711"     ,   30, 150,    150 },
//	{  54, "M9712"     ,   30, 150,    300 },
//	{  55, "M9712C"    ,   60, 150,    300 },
//	{  56, "M9713"     ,  120, 150,    600 },
//	{  57, "M9712B"    ,   15, 500,    300 },
//	{  58, "M9713B"    ,   30, 500,    600 },
//	{  59, "M9714"     ,  240, 150,   1200 },
//	{  60, "M9714B"    ,   60, 500,   1200 },
//	{  61, "M9715"     ,  240, 150,   1800 },
//	{  62, "M9715B"    ,  120, 500,   1800 },
//	{  63, "M9716"     ,  240, 150,   2400 },
//	{  64, "M9716B"    ,  120, 500,   2400 },
//	{  65, "M9717C"    ,  480, 150,   3600 },
//	{  66, "M9717"     ,  240, 150,   3600 },
//	{  67, "M9717B"    ,  120, 500,   3600 },
//	{  68, "M9718"     ,  240, 150,   6000 },
//	{  69, "M9718B"    ,  120, 500,   6000 },
//	{  70, "M9718D"    ,  240, 500,   6000 },
//	{  71, "M9836"     ,  500, 150,  20000 },
//	{  72, "M9836B"    ,  240, 500,  20000 },
//	{  73, "M9838B"    ,  240, 500,  50000 },
//	{  74, "M9839B"    ,  240, 500, 100000 },
//	{  75, "M9840B"    ,  500, 500, 200000 },
//	{  76, "M9840"     , 1500, 150, 200000 },
//	{  77, "M9712B30"  ,   30, 500,    300 },
//	{  78, "M9718E"    ,  120, 600,   6000 },
//	{  79, "M9718F"    ,  480, 150,   6000 },
//	{  80, "M9716E"    ,  480, 150,   3000 },
	{  28, "M9710"     ,   30, 150,    150 },
//	{  82, "M9834"     ,  500, 150,  10000 },
//	{  83, "M9835"     ,  500, 150,  15000 },
//	{  84, "M9835B"    ,  240, 500,  15000 },
//	{  85, "M9837"     ,  500, 150,  35000 },
//	{  86, "M9837B"    ,  240, 500,  35000 },
//	{  87, "M9838"     ,  500, 150,  50000 },
//	{  88, "M9839"     ,  500, 150, 100000 },
//	{  89, "M9835C"    , 1000, 150,  15000 }, /* ?? */
//	{  90, "M9836C"    , 1000, 150,  20000 }, /* ?? */
//	{  91, "M9718F-300",  480, 300,   6000 }, /* ?? */
//	{  92, "M9836F"    , 1000, 150,  20000 }, /* ?? */
//	{  93, "M9836E"    ,  240, 600,  20000 }, /* ?? */
//	{  94, "M9717D"    ,  240, 500,   3600 }, /* ?? */
//	{  95, "M9836B-720",  240, 720,  20000 }, /* ?? */
//	{  96, "M9834H"    ,  500, 150,  10000 }, /* ?? */
//	{  97, "M9836H"    ,  500, 150,  20000 }, /* ?? */
//	{  98, "M9718F-500",  480, 500,   6000 }, /* ?? */
//	{  99, "M9834B"    ,  240, 500,  10000 }, /* ?? */
//	{ 100, "M9811"     ,   30, 150,    200 },
	{ 101, "M9812"     ,   30, 150,    300 },
//	{ 102, "M9812B"    ,   15, 500,    300 },
};

/* Forward declaration - PXView-compatible driver_info struct defined
 * at the bottom of this file. */
extern struct sr_dev_driver maynuo_m97_driver_info;

static struct sr_dev_inst *probe_device(struct sr_modbus_dev_inst *modbus)
{
	const struct maynuo_m97_model *model = NULL;
	struct dev_context *devc;
	struct sr_dev_inst *sdi;
	struct sr_channel_group *cg;
	struct sr_channel *ch;
	uint16_t id, version;
	unsigned int i;

	int ret = maynuo_m97_get_model_version(modbus, &id, &version);
	if (ret != SR_OK)
		return NULL;
	for (i = 0; i < ARRAY_SIZE(supported_models); i++)
		if (id == supported_models[i].id) {
			model = &supported_models[i];
			break;
		}
	if (model == NULL) {
		sr_err("Unknown model: %d.", id);
		return NULL;
	}

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup("Maynuo");
	sdi->model = g_strdup(model->name);
	sdi->version = g_strdup_printf("v%d.%d", version / 10, version % 10);
	sdi->conn = modbus;
	sdi->driver = &maynuo_m97_driver_info;
	sdi->inst_type = SR_INST_MODBUS;

	cg = sr_channel_group_new(sdi, "1", NULL);

	ch = sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "V1");
	cg->channels = g_slist_append(cg->channels, ch);

	ch = sr_channel_new(sdi, 1, SR_CHANNEL_ANALOG, TRUE, "I1");
	cg->channels = g_slist_append(cg->channels, ch);

	devc = g_malloc0(sizeof(struct dev_context));
	devc->model = model;
	sr_sw_limits_init(&devc->limits);
	g_mutex_init(&devc->rw_mutex);

	sdi->priv = devc;

	return sdi;
}

static int config_compare(gconstpointer a, gconstpointer b)
{
	const struct sr_config *ac = a, *bc = b;
	return ac->key != bc->key;
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct sr_config default_serialcomm = {
		.key = SR_CONF_SERIALCOMM,
		.data = g_variant_new_string("9600/8n1"),
	};
	struct sr_config default_modbusaddr = {
		.key = SR_CONF_MODBUSADDR,
		.data = g_variant_new_uint64(1),
	};
	GSList *opts = options, *devices;

	if (!g_slist_find_custom(options, &default_serialcomm, config_compare))
		opts = g_slist_prepend(opts, &default_serialcomm);
	if (!g_slist_find_custom(options, &default_modbusaddr, config_compare))
		opts = g_slist_prepend(opts, &default_modbusaddr);

	/*
	 * Standard sigrok called sr_modbus_scan(di->context, opts, probe_device).
	 * PXView uses di->priv for the drv_context instead. The local
	 * sr_modbus_scan() takes the sr_dev_driver pointer (matching the
	 * rdtech-dps compat driver's convention) and ignores di->priv for
	 * resource discovery (it scans the user-provided conn option).
	 */
	devices = sr_modbus_scan(di, opts, probe_device);

	while (opts != options)
		opts = g_slist_delete_link(opts, opts);
	g_variant_unref(default_serialcomm.data);
	g_variant_unref(default_modbusaddr.data);

	return std_scan_complete_compat(di, devices);
}

/*
 * dev_open / dev_close: the maynuo-m97 driver talks over Modbus RTU
 * (sdi->conn is a struct sr_modbus_dev_inst), so the standard
 * std_serial_dev_open/close helpers cannot be used here. We keep the
 * original open/close logic that calls sr_modbus_open/close and toggles
 * the PC1 coil (remote control enable).
 */
static int dev_open(struct sr_dev_inst *sdi)
{
	struct sr_modbus_dev_inst *modbus = sdi->conn;

	if (sr_modbus_open(modbus) < 0)
		return SR_ERR;

	maynuo_m97_set_bit(sdi, PC1, 1);

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	struct sr_modbus_dev_inst *modbus;

	modbus = sdi->conn;

	if (!modbus)
		return SR_ERR_BUG;

	maynuo_m97_set_bit(sdi, PC1, 0);

	return sr_modbus_close(modbus);
}

static int config_get(uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	enum maynuo_m97_mode mode;
	int ret, ivalue;
	float fvalue;

	(void)cg;

	devc = sdi->priv;

	ret = SR_OK;
	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
	case SR_CONF_LIMIT_MSEC:
		ret = sr_sw_limits_config_get(&devc->limits, key, data);
		break;
	case SR_CONF_ENABLED:
		if ((ret = maynuo_m97_get_bit(sdi, ISTATE, &ivalue)) == SR_OK)
			*data = g_variant_new_boolean(ivalue);
		break;
	case SR_CONF_REGULATION:
		if ((ret = maynuo_m97_get_bit(sdi, UNREG, &ivalue)) != SR_OK)
			break;
		if (ivalue)
			*data = g_variant_new_string("UR");
		else if ((ret = maynuo_m97_get_mode(sdi, &mode)) == SR_OK)
			*data = g_variant_new_string(maynuo_m97_mode_to_str(mode));
		break;
	case SR_CONF_VOLTAGE:
		if ((ret = maynuo_m97_get_float(sdi, U, &fvalue)) == SR_OK)
			*data = g_variant_new_double(fvalue);
		break;
	case SR_CONF_VOLTAGE_TARGET:
		if ((ret = maynuo_m97_get_float(sdi, UFIX, &fvalue)) == SR_OK)
			*data = g_variant_new_double(fvalue);
		break;
	case SR_CONF_CURRENT:
		if ((ret = maynuo_m97_get_float(sdi, I, &fvalue)) == SR_OK)
			*data = g_variant_new_double(fvalue);
		break;
	case SR_CONF_CURRENT_LIMIT:
		if ((ret = maynuo_m97_get_float(sdi, IFIX, &fvalue)) == SR_OK)
			*data = g_variant_new_double(fvalue);
		break;
	case SR_CONF_OVER_VOLTAGE_PROTECTION_ENABLED:
		*data = g_variant_new_boolean(TRUE);
		break;
	case SR_CONF_OVER_VOLTAGE_PROTECTION_ACTIVE:
		if ((ret = maynuo_m97_get_bit(sdi, UOVER, &ivalue)) == SR_OK)
			*data = g_variant_new_boolean(ivalue);
		break;
	case SR_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD:
		if ((ret = maynuo_m97_get_float(sdi, UMAX, &fvalue)) == SR_OK)
			*data = g_variant_new_double(fvalue);
		break;
	case SR_CONF_OVER_CURRENT_PROTECTION_ENABLED:
		*data = g_variant_new_boolean(TRUE);
		break;
	case SR_CONF_OVER_CURRENT_PROTECTION_ACTIVE:
		if ((ret = maynuo_m97_get_bit(sdi, IOVER, &ivalue)) == SR_OK)
			*data = g_variant_new_boolean(ivalue);
		break;
	case SR_CONF_OVER_CURRENT_PROTECTION_THRESHOLD:
		if ((ret = maynuo_m97_get_float(sdi, IMAX, &fvalue)) == SR_OK)
			*data = g_variant_new_double(fvalue);
		break;
	case SR_CONF_OVER_TEMPERATURE_PROTECTION:
		*data = g_variant_new_boolean(TRUE);
		break;
	case SR_CONF_OVER_TEMPERATURE_PROTECTION_ACTIVE:
		if ((ret = maynuo_m97_get_bit(sdi, HEAT, &ivalue)) == SR_OK)
			*data = g_variant_new_boolean(ivalue);
		break;
	default:
		return SR_ERR_NA;
	}

	return ret;
}

static int config_set(uint32_t key, GVariant *data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
	case SR_CONF_LIMIT_MSEC:
		return sr_sw_limits_config_set(&devc->limits, key, data);
	case SR_CONF_ENABLED:
		return maynuo_m97_set_input(sdi, g_variant_get_boolean(data));
	case SR_CONF_VOLTAGE_TARGET:
		return maynuo_m97_set_float(sdi, UFIX, g_variant_get_double(data));
	case SR_CONF_CURRENT_LIMIT:
		return maynuo_m97_set_float(sdi, IFIX, g_variant_get_double(data));
	case SR_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD:
		return maynuo_m97_set_float(sdi, UMAX, g_variant_get_double(data));
	case SR_CONF_OVER_CURRENT_PROTECTION_THRESHOLD:
		return maynuo_m97_set_float(sdi, IMAX, g_variant_get_double(data));
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	devc = (sdi) ? sdi->priv : NULL;

	if (!cg) {
		return std_config_list(key, data, sdi, cg, scanopts,
				ARRAY_SIZE(scanopts), drvopts, ARRAY_SIZE(drvopts),
				devopts, ARRAY_SIZE(devopts));
	} else {
		switch (key) {
		case SR_CONF_DEVICE_OPTIONS:
			*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg));
			break;
		case SR_CONF_VOLTAGE_TARGET:
			if (!devc || !devc->model)
				return SR_ERR_ARG;
			*data = std_gvar_min_max_step(0.0,
					(double)devc->model->max_voltage, 0.001);
			break;
		case SR_CONF_CURRENT_LIMIT:
			if (!devc || !devc->model)
				return SR_ERR_ARG;
			*data = std_gvar_min_max_step(0.0,
					(double)devc->model->max_current, 0.0001);
			break;
		default:
			return SR_ERR_NA;
		}
	}

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_modbus_dev_inst *modbus;
	int ret;

	modbus = sdi->conn;
	devc = sdi->priv;

	/*
	 * Standard sigrok called sr_modbus_source_add(sdi->session, modbus,
	 * G_IO_IN, 10, maynuo_m97_receive_data, (void *)sdi) with 6 args.
	 * PXView's local sr_modbus_source_add() drops the session parameter
	 * (5 args), matching the rdtech-dps compat driver's convention.
	 * The receive callback receives sdi directly (no cb_data unwrap).
	 */
	if ((ret = sr_modbus_source_add(modbus, G_IO_IN, 10,
			maynuo_m97_receive_data, sdi)) != SR_OK)
		return ret;

	sr_sw_limits_acquisition_start(&devc->limits);
	std_session_send_df_header(sdi, LOG_PREFIX);

	return SR_OK;
}

/*
 * Local dev_acquisition_stop implementation.
 * Standard sigrok's std_serial_dev_acquisition_stop() is not available in
 * PXView, and the original maynuo-m97 driver had its own stop logic anyway
 * (send DF_END, remove the modbus source). Adapted to PXView's session-less
 * sr_modbus_source_remove() and 2-arg std_session_send_df_end().
 */
static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	struct sr_modbus_dev_inst *modbus;

	std_session_send_df_end(sdi, LOG_PREFIX);

	modbus = sdi->conn;
	sr_modbus_source_remove(modbus);

	return SR_OK;
}

/*
 * dev_clear: standard sigrok's std_dev_clear_with_callback() is not available
 * in the PXView compat layer. Inline the logic: iterate instances, clear the
 * per-device mutex, then use std_dev_clear_compat to free everything. Same
 * pattern as the itech-it8500 compat driver.
 */
static void dev_clear_callback(void *priv)
{
	struct dev_context *devc;

	if (!priv)
		return;

	devc = priv;
	g_mutex_clear(&devc->rw_mutex);
}

static int dev_clear(const struct sr_dev_driver *di)
{
	struct compat_drv_context *drvc;
	GSList *l;
	struct sr_dev_inst *sdi;

	if (!di || !di->priv)
		return SR_ERR_ARG;

	drvc = di->priv;
	for (l = drvc->instances; l; l = l->next) {
		sdi = l->data;
		if (sdi->priv)
			dev_clear_callback(sdi->priv);
	}

	return std_dev_clear_compat(di);
}

/* ===========================================================================
 * PXView compat wrapper layer
 * =========================================================================== */

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *maynuo_m97_drv_ptr;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int maynuo_m97_compat_init(struct sr_context *sr_ctx)
{
	maynuo_m97_drv_ptr = &maynuo_m97_driver_info;
	return std_init(maynuo_m97_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int maynuo_m97_compat_cleanup(void)
{
	return std_cleanup(maynuo_m97_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *maynuo_m97_compat_scan(GSList *options)
{
	return scan(maynuo_m97_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int maynuo_m97_compat_config_get(int id, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel *ch,
		const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int maynuo_m97_compat_config_set(int id, GVariant *data,
		struct sr_dev_inst *sdi, struct sr_channel *ch,
		struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int maynuo_m97_compat_config_list(int info_id, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int maynuo_m97_compat_acquisition_start(struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int maynuo_m97_compat_acquisition_stop(const struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop((struct sr_dev_inst *)sdi);
}

/* PXView-compatible driver info struct.
 *
 * The original maynuo-m97 driver used SR_REGISTER_DEV_DRIVER() which is a
 * standard sigrok macro that auto-registers via constructor attribute. PXView
 * does not support constructor attributes, so the driver is registered
 * manually via drivers_list[] in hwdriver.c. The variable name
 * (maynuo_m97_driver_info) must match the name referenced in hwdriver.c.
 */
struct sr_dev_driver maynuo_m97_driver_info = {
	.name = "maynuo-m97",
	.longname = "maynuo M97/M98 series",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = maynuo_m97_compat_init,
	.cleanup = maynuo_m97_compat_cleanup,
	.scan = maynuo_m97_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = maynuo_m97_compat_config_get,
	.config_set = maynuo_m97_compat_config_set,
	.config_list = maynuo_m97_compat_config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = maynuo_m97_compat_acquisition_start,
	.dev_acquisition_stop = maynuo_m97_compat_acquisition_stop,
	.priv = NULL,
};
