/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Uwe Hermann <uwe@hermann-uwe.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "hardware/compat/compat.h"
#include <glib.h>
#include <string.h>
#include "protocol.h"

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_THERMOMETER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_SET,
};

static const char *channel_names[] = {
	"T1", "T2", "T3", "T4",
};

/* Forward declarations - PXView-compatible driver_info structs defined
 * at the bottom of this file. Referenced by center_devs[] below. */
extern struct sr_dev_driver center_309_driver_info;
extern struct sr_dev_driver voltcraft_k204_driver_info;

SR_PRIV const struct center_dev_info center_devs[] = {
	{
		"Center", "309", "9600/8n1", 4, 32000, 45,
		center_3xx_packet_valid,
		&center_309_driver_info, receive_data_CENTER_309,
	},
	{
		"Voltcraft", "K204", "9600/8n1", 4, 32000, 45,
		center_3xx_packet_valid,
		&voltcraft_k204_driver_info, receive_data_VOLTCRAFT_K204,
	},
};

static GSList *center_scan(const char *conn, const char *serialcomm, int idx)
{
	int i;
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;

	serial = sr_serial_dev_inst_new(conn, serialcomm);

	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		return NULL;

	sr_info("Found device on port %s.", conn);

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup(center_devs[idx].vendor);
	sdi->model = g_strdup(center_devs[idx].device);
	devc = g_malloc0(sizeof(struct dev_context));
	sr_sw_limits_init(&devc->sw_limits);
	sdi->inst_type = SR_INST_SERIAL;
	sdi->conn = serial;
	sdi->priv = devc;

	for (i = 0; i < center_devs[idx].num_channels; i++)
		sr_channel_new(sdi, i, SR_CHANNEL_ANALOG, TRUE, channel_names[i]);

	serial_close(serial);

	return g_slist_append(NULL, sdi);
}

static GSList *scan(struct sr_dev_driver *di, GSList *options, int idx)
{
	struct sr_config *src;
	GSList *l, *devices;
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

	if (serialcomm)
		devices = center_scan(conn, serialcomm, idx);
	else
		devices = center_scan(conn, center_devs[idx].conn, idx);

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

static int dev_acquisition_start(const struct sr_dev_inst *sdi, int idx)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;

	devc = sdi->priv;

	sr_sw_limits_acquisition_start(&devc->sw_limits);

	std_session_send_df_header(sdi, LOG_PREFIX);

	serial = sdi->conn;
	serial_source_add(serial, G_IO_IN, 500,
			center_devs[idx].receive_data, sdi);

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

/* =========================================================================
 * Driver: center_309
 * ========================================================================= */

static struct sr_dev_driver *center_309_drv_ptr;

static int center_309_compat_init(struct sr_context *sr_ctx)
{
	center_309_drv_ptr = &center_309_driver_info;
	return std_init(center_309_drv_ptr, sr_ctx);
}

static int center_309_compat_cleanup(void)
{
	return std_cleanup(center_309_drv_ptr);
}

static GSList *center_309_compat_scan(GSList *options)
{
	return scan(center_309_drv_ptr, options, CENTER_309);
}

static int center_309_compat_config_get(int id, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel *ch,
		const struct sr_channel_group *cg)
{
	(void)id; (void)data; (void)sdi; (void)ch; (void)cg;
	/* Original driver has no config_get support. */
	return SR_ERR_NA;
}

static int center_309_compat_config_set(int id, GVariant *data,
		struct sr_dev_inst *sdi, struct sr_channel *ch,
		struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

static int center_309_compat_config_list(int info_id, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

static int center_309_compat_acquisition_start(struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi, CENTER_309);
}

static int center_309_compat_acquisition_stop(const struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop(sdi);
}

struct sr_dev_driver center_309_driver_info = {
	.name = "center-309",
	.longname = "Center 309",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = center_309_compat_init,
	.cleanup = center_309_compat_cleanup,
	.scan = center_309_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = center_309_compat_config_get,
	.config_set = center_309_compat_config_set,
	.config_list = center_309_compat_config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = center_309_compat_acquisition_start,
	.dev_acquisition_stop = center_309_compat_acquisition_stop,
	.priv = NULL,
};

/* =========================================================================
 * Driver: voltcraft_k204
 * ========================================================================= */

static struct sr_dev_driver *voltcraft_k204_drv_ptr;

static int voltcraft_k204_compat_init(struct sr_context *sr_ctx)
{
	voltcraft_k204_drv_ptr = &voltcraft_k204_driver_info;
	return std_init(voltcraft_k204_drv_ptr, sr_ctx);
}

static int voltcraft_k204_compat_cleanup(void)
{
	return std_cleanup(voltcraft_k204_drv_ptr);
}

static GSList *voltcraft_k204_compat_scan(GSList *options)
{
	return scan(voltcraft_k204_drv_ptr, options, VOLTCRAFT_K204);
}

static int voltcraft_k204_compat_config_get(int id, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel *ch,
		const struct sr_channel_group *cg)
{
	(void)id; (void)data; (void)sdi; (void)ch; (void)cg;
	/* Original driver has no config_get support. */
	return SR_ERR_NA;
}

static int voltcraft_k204_compat_config_set(int id, GVariant *data,
		struct sr_dev_inst *sdi, struct sr_channel *ch,
		struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

static int voltcraft_k204_compat_config_list(int info_id, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

static int voltcraft_k204_compat_acquisition_start(struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi, VOLTCRAFT_K204);
}

static int voltcraft_k204_compat_acquisition_stop(const struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop(sdi);
}

struct sr_dev_driver voltcraft_k204_driver_info = {
	.name = "voltcraft-k204",
	.longname = "Voltcraft K204",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = voltcraft_k204_compat_init,
	.cleanup = voltcraft_k204_compat_cleanup,
	.scan = voltcraft_k204_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = voltcraft_k204_compat_config_get,
	.config_set = voltcraft_k204_compat_config_set,
	.config_list = voltcraft_k204_compat_config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = voltcraft_k204_compat_acquisition_start,
	.dev_acquisition_stop = voltcraft_k204_compat_acquisition_stop,
	.priv = NULL,
};
