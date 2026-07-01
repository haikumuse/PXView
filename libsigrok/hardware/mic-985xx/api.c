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

static const uint32_t drvopts_temp[] = {
	SR_CONF_THERMOMETER,
};

static const uint32_t drvopts_temp_hum[] = {
	SR_CONF_THERMOMETER,
	SR_CONF_HYGROMETER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_SET,
};

/* Forward declarations - defined at bottom of file via MIC_DRV macro. */
struct sr_dev_driver mic_98581_driver_info;
struct sr_dev_driver mic_98583_driver_info;

SR_PRIV const struct mic_dev_info mic_devs[] = {
	{
		"MIC", "98581", "38400/8n2", 32000, TRUE, FALSE, 6,
		packet_valid_temp,
		&mic_98581_driver_info, receive_data_MIC_98581,
	},
	{
		"MIC", "98583", "38400/8n2", 32000, TRUE, TRUE, 10,
		packet_valid_temp_hum,
		&mic_98583_driver_info, receive_data_MIC_98583,
	},
};

static GSList *mic_scan(const char *conn, const char *serialcomm, int idx)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;

	serial = sr_serial_dev_inst_new(conn, serialcomm);

	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		return NULL;

	/* TODO: Query device type. */
	/* ret = mic_cmd_get_device_info(serial); */

	sr_info("Found device on port %s.", conn);

	/* TODO: Fill in version from protocol response. */
	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup(mic_devs[idx].vendor);
	sdi->model = g_strdup(mic_devs[idx].device);
	devc = g_malloc0(sizeof(struct dev_context));
	sr_sw_limits_init(&devc->limits);
	sdi->inst_type = SR_INST_SERIAL;
	sdi->conn = serial;
	sdi->priv = devc;

	sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "Temperature");

	if (mic_devs[idx].has_humidity)
		sr_channel_new(sdi, 1, SR_CHANNEL_ANALOG, TRUE, "Humidity");

	serial_close(serial);

	return std_scan_complete_compat(mic_devs[idx].di,
			g_slist_append(NULL, sdi));
}

static GSList *scan(GSList *options, int idx)
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
		devices = mic_scan(conn, serialcomm, idx);
	else
		devices = mic_scan(conn, mic_devs[idx].conn, idx);

	return devices;
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
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg, int idx)
{
	/*
	 * We can't use the ternary operator here! The result would contain
	 * sizeof((cond) ? A : B) where A/B are arrays of different type/size.
	 * The ternary operator always returns the "common" type of A and B,
	 * which would be a pointer instead of either the A or B arrays.
	 * Thus, sizeof() would yield the size of a pointer, not the size
	 * of either the A or B array, which is not what we want.
	 */
	if (mic_devs[idx].has_humidity)
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts_temp_hum, devopts);
	else
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts_temp, devopts);
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi, int idx)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;

	devc = sdi->priv;

	sr_sw_limits_acquisition_start(&devc->limits);
	std_session_send_df_header(sdi, LOG_PREFIX);

	serial = sdi->conn;
	serial_source_add(serial, G_IO_IN, 100,
			mic_devs[idx].receive_data, sdi);

	return SR_OK;
}

/*
 * Local dev_acquisition_stop implementation.
 * PXView does not provide std_serial_dev_acquisition_stop, so we implement
 * the stop sequence locally: remove the serial source, close the serial
 * port, and send the DF_END packet.
 */
static int mic_dev_acquisition_stop(const struct sr_dev_inst *sdi)
{
	struct sr_serial_dev_inst *serial;

	serial = sdi->conn;

	serial_source_remove(serial);
	serial_close(serial);

	std_session_send_df_end(sdi, LOG_PREFIX);

	return SR_OK;
}

/*
 * Macro that generates PXView-compatible wrapper functions and the
 * driver_info struct for one MIC 985xx variant.
 *
 * ID        - C identifier for the driver (e.g. mic_98581)
 * ID_UPPER  - enum value for the device index (e.g. MIC_98581)
 * NAME      - short driver name string
 * LONGNAME  - long driver name string
 */
#define MIC_DRV(ID, ID_UPPER, NAME, LONGNAME) \
\
static int ID##_compat_init(struct sr_context *sr_ctx) \
{ \
	return std_init(&ID##_driver_info, sr_ctx); \
} \
\
static int ID##_compat_cleanup(void) \
{ \
	return std_cleanup(&ID##_driver_info); \
} \
\
static GSList *ID##_compat_scan(GSList *options) \
{ \
	return scan(options, ID_UPPER); \
} \
\
static int ID##_compat_config_get(int id, GVariant **data, \
		const struct sr_dev_inst *sdi, const struct sr_channel *ch, \
		const struct sr_channel_group *cg) \
{ \
	(void)id; (void)data; (void)sdi; (void)ch; (void)cg; \
	return SR_ERR_NA; \
} \
\
static int ID##_compat_config_set(int id, GVariant *data, \
		struct sr_dev_inst *sdi, struct sr_channel *ch, \
		struct sr_channel_group *cg) \
{ \
	(void)ch; \
	return config_set((uint32_t)id, data, sdi, cg); \
} \
\
static int ID##_compat_config_list(int info_id, GVariant **data, \
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg) \
{ \
	return config_list((uint32_t)info_id, data, sdi, cg, ID_UPPER); \
} \
\
static int ID##_compat_acquisition_start(struct sr_dev_inst *sdi, \
		void *cb_data) \
{ \
	(void)cb_data; \
	return dev_acquisition_start(sdi, ID_UPPER); \
} \
\
static int ID##_compat_acquisition_stop(const struct sr_dev_inst *sdi, \
		void *cb_data) \
{ \
	(void)cb_data; \
	return mic_dev_acquisition_stop(sdi); \
} \
\
struct sr_dev_driver ID##_driver_info = { \
	.name = NAME, \
	.longname = LONGNAME, \
	.api_version = 1, \
	.driver_type = DRIVER_TYPE_HARDWARE, \
	.init = ID##_compat_init, \
	.cleanup = ID##_compat_cleanup, \
	.scan = ID##_compat_scan, \
	.dev_mode_list = compat_dev_mode_list_default, \
	.config_get = ID##_compat_config_get, \
	.config_set = ID##_compat_config_set, \
	.config_list = ID##_compat_config_list, \
	.dev_open = std_serial_dev_open, \
	.dev_close = std_serial_dev_close, \
	.dev_destroy = compat_dev_destroy_default, \
	.dev_status_get = compat_dev_status_get_default, \
	.dev_acquisition_start = ID##_compat_acquisition_start, \
	.dev_acquisition_stop = ID##_compat_acquisition_stop, \
	.priv = NULL, \
};

MIC_DRV(mic_98581, MIC_98581, "mic-98581", "MIC 98581")
MIC_DRV(mic_98583, MIC_98583, "mic-98583", "MIC 98583")
