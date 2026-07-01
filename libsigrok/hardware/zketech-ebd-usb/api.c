/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2018 Sven Bursch-Osewold <sb_git@bursch.com>
 * Copyright (C) 2019 King Kévin <kingkevin@cuvoodoo.info>
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
#include "protocol.h"

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_ELECTRONIC_LOAD,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_CURRENT_LIMIT | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_UNDER_VOLTAGE_CONDITION_THRESHOLD | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET,
};

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct dev_context *devc;
	GSList *l;
	struct sr_dev_inst *sdi;
	const char *conn, *serialcomm;
	struct sr_config *src;
	struct sr_serial_dev_inst *serial;
	uint8_t reply[MSG_MAX_LEN];

	conn = NULL;
	serialcomm = NULL;

	/*
	 * PXView's libsigrok does not provide sr_serial_extract_options().
	 * Manually walk the options GSList and parse SR_CONF_CONN and
	 * SR_CONF_SERIALCOMM (same pattern as conrad-digi-35-cpu and
	 * colead-slm compat drivers).
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
	if (!serialcomm)
		serialcomm = "9600/8e1";

	serial = sr_serial_dev_inst_new(conn, serialcomm);
	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		return NULL;

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup("ZKETECH");
	sdi->model = g_strdup("EBD-USB");
	sdi->inst_type = SR_INST_SERIAL;
	sdi->conn = serial;

	sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "V.VBUS");
	sr_channel_new(sdi, 1, SR_CHANNEL_ANALOG, TRUE, "I.VBUS");
	sr_channel_new(sdi, 2, SR_CHANNEL_ANALOG, TRUE, "V.D+");
	sr_channel_new(sdi, 3, SR_CHANNEL_ANALOG, TRUE, "V.D-");

	devc = g_malloc0(sizeof(struct dev_context));
	g_mutex_init(&devc->rw_mutex);
	devc->current_limit = 0;
	devc->uvc_threshold = 0;
	devc->running = FALSE;
	devc->load_activated = FALSE;
	sr_sw_limits_init(&devc->limits);
	sdi->priv = devc;

	/* Starting device. */
	ebd_init(serial, devc);
	int ret = ebd_read_message(serial, MSG_MAX_LEN, reply);
	if (ret < 0) {
		sr_warn("Could not receive message!");
		ret = SR_ERR;
	} else if (ret == 0) {
		sr_warn("No message received!");
		ret = SR_ERR;
	}
	ebd_stop(serial, devc);

	serial_close(serial);

	if (ret < 0)
		return NULL;

	return std_scan_complete_compat(di, g_slist_append(NULL, sdi));
}

static int dev_close(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	devc = (sdi) ? sdi->priv : NULL;
	if (devc)
		g_mutex_clear(&devc->rw_mutex);

	return std_serial_dev_close(sdi);
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret;
	struct dev_context *devc;
	float fvalue;

	(void)cg;

	if (!sdi || !data)
		return SR_ERR_ARG;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
	case SR_CONF_LIMIT_MSEC:
		return sr_sw_limits_config_get(&devc->limits, key, data);
	case SR_CONF_CURRENT_LIMIT:
		ret = ebd_get_current_limit(sdi, &fvalue);
		if (ret == SR_OK)
			*data = g_variant_new_double(fvalue);
		return ret;
	case SR_CONF_UNDER_VOLTAGE_CONDITION_THRESHOLD:
		ret = ebd_get_uvc_threshold(sdi, &fvalue);
		if (ret == SR_OK)
			*data = g_variant_new_double(fvalue);
		return ret;
	default:
		return SR_ERR_NA;
	}
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	double value;
	struct dev_context *devc;

	(void)data;
	(void)cg;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_LIMIT_MSEC:
	case SR_CONF_LIMIT_SAMPLES:
		return sr_sw_limits_config_set(&devc->limits, key, data);
	case SR_CONF_CURRENT_LIMIT:
		value = g_variant_get_double(data);
		if (value < 0.0 || value > 4.0)
			return SR_ERR_ARG;
		return ebd_set_current_limit(sdi, value);
	case SR_CONF_UNDER_VOLTAGE_CONDITION_THRESHOLD:
		value = g_variant_get_double(data);
		if (value < 0.0 || value > 21.0)
			return SR_ERR_ARG;
		return ebd_set_uvc_threshold(sdi, value);
	default:
		return SR_ERR_NA;
	}
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
	case SR_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts,
			devopts);
	case SR_CONF_CURRENT_LIMIT:
		*data = std_gvar_min_max_step(0.0, 4.0, 0.001);
		break;
	case SR_CONF_UNDER_VOLTAGE_CONDITION_THRESHOLD:
		*data = std_gvar_min_max_step(0.0, 21.0, 0.01);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;

	devc = sdi->priv;
	serial = sdi->conn;

	sr_sw_limits_acquisition_start(&devc->limits);
	std_session_send_df_header(sdi, LOG_PREFIX);

	ebd_init(serial, devc);

	/*
	 * PXView's serial_source_add is 5-arg (no session parameter) and
	 * the callback signature is int (*)(int, int, const struct sr_dev_inst *).
	 */
	serial_source_add(serial, G_IO_IN, 100, ebd_receive_data, sdi);

	return SR_OK;
}

/*
 * PXView does not provide std_serial_dev_acquisition_stop, so implement the
 * stop sequence locally: drive the load/toggle commands off, remove the
 * serial source, close the serial port, and send the DF_END packet. Same
 * pattern as the colead-slm compat driver.
 */
static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;

	if (!sdi)
		return SR_OK;

	devc = sdi->priv;
	if (devc) {
		if (devc->load_activated)
			ebd_loadtoggle(sdi->conn, devc);
		ebd_stop(sdi->conn, devc);
	}

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
 * sigrok's (int vs uint32_t key, extra ch parameter, cb_data in acquisition
 * start/stop, no dev_list/dev_clear fields). These thin wrappers adapt the
 * standard sigrok callbacks above to PXView's expected signatures.
 * ==========================================================================*/

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *zketech_ebd_usb_drv_ptr;

/* Forward declaration - defined at end of file */
extern SR_PRIV struct sr_dev_driver zketech_ebd_usb_driver_info;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int zketech_ebd_usb_compat_init(struct sr_context *sr_ctx)
{
	zketech_ebd_usb_drv_ptr = &zketech_ebd_usb_driver_info;
	return std_init(zketech_ebd_usb_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int zketech_ebd_usb_compat_cleanup(void)
{
	return std_cleanup(zketech_ebd_usb_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *zketech_ebd_usb_compat_scan(GSList *options)
{
	return scan(zketech_ebd_usb_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg).
 * No per-channel config_get logic in the original driver, so ch is dropped. */
static int zketech_ebd_usb_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg).
 * No per-channel config_set logic in the original driver, so ch is dropped. */
static int zketech_ebd_usb_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int zketech_ebd_usb_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int zketech_ebd_usb_compat_acquisition_start(struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int zketech_ebd_usb_compat_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop((struct sr_dev_inst *)sdi);
}

/* PXView-compatible driver info struct */
struct sr_dev_driver zketech_ebd_usb_driver_info = {
	.name = "zketech-ebd-usb",
	.longname = "ZKETECH EBD-USB",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = zketech_ebd_usb_compat_init,
	.cleanup = zketech_ebd_usb_compat_cleanup,
	.scan = zketech_ebd_usb_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = zketech_ebd_usb_compat_config_get,
	.config_set = zketech_ebd_usb_compat_config_set,
	.config_list = zketech_ebd_usb_compat_config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = zketech_ebd_usb_compat_acquisition_start,
	.dev_acquisition_stop = zketech_ebd_usb_compat_acquisition_stop,
	.priv = NULL,
};
