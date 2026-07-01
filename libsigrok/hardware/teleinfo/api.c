/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Aurelien Jacobs <aurel@gnuage.org>
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
	SR_CONF_ENERGYMETER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_SET,
};

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
	struct sr_dev_inst *sdi;
	GSList *devices = NULL, *l;
	const char *conn = NULL, *serialcomm = NULL;
	uint8_t buf[292];
	size_t len;
	struct sr_config *src;

	len = sizeof(buf);

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

	serial = sr_serial_dev_inst_new(conn, serialcomm);

	if (serial_open(serial, SERIAL_RDONLY) != SR_OK)
		return NULL;

	sr_info("Probing serial port %s.", conn);

	/* Let's get a bit of data and see if we can find a packet. */
	if (serial_stream_detect(serial, buf, &len, len,
			teleinfo_packet_valid, 3000, SERIAL_BAUDRATE) != SR_OK)
		goto scan_cleanup;

	sr_info("Found device on port %s.", conn);

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup("EDF");
	sdi->model = g_strdup("Teleinfo");
	devc = g_malloc0(sizeof(struct dev_context));
	devc->optarif = teleinfo_get_optarif(buf);
	sdi->inst_type = SR_INST_SERIAL;
	sdi->conn = serial;
	sdi->priv = devc;

	sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "P");

	if (devc->optarif == OPTARIF_BASE) {
		sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "BASE");
	} else if (devc->optarif == OPTARIF_HC) {
		sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "HP");
		sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "HC");
	} else if (devc->optarif == OPTARIF_EJP) {
		sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "HN");
		sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "HPM");
	} else if (devc->optarif == OPTARIF_BBR) {
		sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "HPJB");
		sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "HPJW");
		sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "HPJR");
		sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "HCJB");
		sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "HCJW");
		sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "HCJR");
	}

	sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "IINST");
	sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "PAPP");

	devices = g_slist_append(devices, sdi);

scan_cleanup:
	serial_close(serial);

	return std_scan_complete_compat(di, devices);
}

static int config_set(uint32_t key, GVariant *data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	devc = sdi->priv;

	return sr_sw_limits_config_set(&devc->sw_limits, key, data);
}

static int config_list(uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct sr_serial_dev_inst *serial = sdi->conn;
	struct dev_context *devc;

	devc = sdi->priv;

	sr_sw_limits_acquisition_start(&devc->sw_limits);

	std_session_send_df_header(sdi, LOG_PREFIX);

	serial_source_add(serial, G_IO_IN, 50,
			teleinfo_receive_data, sdi);

	return SR_OK;
}

/*
 * Local dev_acquisition_stop implementation.
 * PXView does not provide std_serial_dev_acquisition_stop, so we implement
 * the stop sequence locally: remove the serial source, close the serial
 * port, and send the DF_END packet.
 */
static int dev_acquisition_stop(const struct sr_dev_inst *sdi)
{
	struct sr_serial_dev_inst *serial;

	serial = sdi->conn;

	serial_source_remove(serial);
	serial_close(serial);

	std_session_send_df_end(sdi, LOG_PREFIX);

	return SR_OK;
}

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *teleinfo_drv_ptr;

/* Forward declaration - defined at end of file */
extern struct sr_dev_driver teleinfo_driver_info;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int teleinfo_compat_init(struct sr_context *sr_ctx)
{
	teleinfo_drv_ptr = &teleinfo_driver_info;
	return std_init(teleinfo_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int teleinfo_compat_cleanup(void)
{
	return std_cleanup(teleinfo_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *teleinfo_compat_scan(GSList *options)
{
	return scan(teleinfo_drv_ptr, options);
}

/*
 * Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg).
 * The original driver exposes no gettable options (config_get was NULL),
 * so report SR_ERR_NA for every key.
 */
static int teleinfo_compat_config_get(int id, GVariant **data,
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

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int teleinfo_compat_config_set(int id, GVariant *data,
		struct sr_dev_inst *sdi, struct sr_channel *ch,
		struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int teleinfo_compat_config_list(int info_id, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int teleinfo_compat_acquisition_start(struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int teleinfo_compat_acquisition_stop(const struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop(sdi);
}

/* PXView-compatible driver info struct */
struct sr_dev_driver teleinfo_driver_info = {
	.name = "teleinfo",
	.longname = "Teleinfo",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = teleinfo_compat_init,
	.cleanup = teleinfo_compat_cleanup,
	.scan = teleinfo_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = teleinfo_compat_config_get,
	.config_set = teleinfo_compat_config_set,
	.config_list = teleinfo_compat_config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = teleinfo_compat_acquisition_start,
	.dev_acquisition_stop = teleinfo_compat_acquisition_stop,
	.priv = NULL,
};
