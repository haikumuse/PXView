/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2015 Uwe Hermann <uwe@hermann-uwe.de>
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

/*
 * KERN scale driver (Serial).
 *
 * Ported from standard sigrok's src/hardware/kern-scale/api.c. The original
 * driver used a struct scale_info wrapper around sr_dev_driver plus a SCALE
 * macro + SR_REGISTER_DEV_DRIVER_LIST to register potentially multiple
 * variants. PXView's sr_dev_driver layout is fixed (extra driver_type /
 * dev_mode_list / dev_destroy / dev_status_get fields), so the wrapper-struct
 * approach cannot be used. Instead, the single registered variant
 * (kern-ew-6200-2nm) is hardcoded here and exposed as a single non-static
 * kern_ew_6200_2nm_driver_info struct.
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
	SR_CONF_SCALE,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_SET,
};

/*
 * KERN EW 6200-2NM variant parameters (hardcoded from the original SCALE
 * macro invocation). The packet size is 15 (or 14, depending on user
 * configuration); both variants are accepted transparently by
 * sr_kern_packet_valid().
 */
#define KERN_EW_6200_2NM_VENDOR      "KERN"
#define KERN_EW_6200_2NM_MODEL       "EW 6200-2NM"
#define KERN_EW_6200_2NM_SERIALCOMM  "1200/8n2"
#define KERN_EW_6200_2NM_PACKET_SIZE 15

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct sr_dev_inst *sdi;
	struct sr_config *src;
	GSList *l, *devices;
	const char *conn, *serialcomm;
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
	int ret;
	size_t len;
	uint8_t buf[128];

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
		serialcomm = KERN_EW_6200_2NM_SERIALCOMM;

	serial = sr_serial_dev_inst_new(conn, serialcomm);

	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		return NULL;

	sr_info("Probing serial port %s.", conn);

	devices = NULL;

	sr_spew("Set O1 mode (continuous values, stable and unstable ones).");
	if (serial_write_blocking(serial, "O1\r\n", 4, 0) < 0)
		goto scan_cleanup;
	/* Device replies with "A00\r\n" (OK) or "E01\r\n" (Error). Ignore. */

	/* Let's get a bit of data and see if we can find a packet. */
	len = sizeof(buf);
	ret = serial_stream_detect(serial, buf, &len,
			KERN_EW_6200_2NM_PACKET_SIZE, sr_kern_packet_valid,
			500, KERN_SCALE_BAUDRATE);
	if (ret != SR_OK)
		goto scan_cleanup;

	sr_info("Found device on port %s.", conn);

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup(KERN_EW_6200_2NM_VENDOR);
	sdi->model = g_strdup(KERN_EW_6200_2NM_MODEL);
	devc = g_malloc0(sizeof(struct dev_context));
	sr_sw_limits_init(&devc->limits);
	sdi->inst_type = SR_INST_SERIAL;
	sdi->conn = serial;
	sdi->priv = devc;
	sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "Mass");
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

	return sr_sw_limits_config_set(&devc->limits, key, data);
}

static int config_list(uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;

	devc = sdi->priv;
	serial = sdi->conn;

	sr_spew("Set O1 mode (continuous values, stable and unstable ones).");
	if (serial_write_blocking(serial, "O1\r\n", 4, 0) < 0)
		return SR_ERR;
	/* Device replies with "A00\r\n" (OK) or "E01\r\n" (Error). Ignore. */

	sr_sw_limits_acquisition_start(&devc->limits);
	std_session_send_df_header(sdi, LOG_PREFIX);

	serial_source_add(serial, G_IO_IN, 50,
			kern_scale_receive_data, sdi);

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
 * PXView compat wrappers + driver_info struct
 * ========================================================================= */

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *kern_ew_6200_2nm_drv_ptr;

/* Forward declaration - defined at end of file */
extern struct sr_dev_driver kern_ew_6200_2nm_driver_info;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int kern_ew_6200_2nm_compat_init(struct sr_context *sr_ctx)
{
	kern_ew_6200_2nm_drv_ptr = &kern_ew_6200_2nm_driver_info;
	return std_init(kern_ew_6200_2nm_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int kern_ew_6200_2nm_compat_cleanup(void)
{
	return std_cleanup(kern_ew_6200_2nm_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *kern_ew_6200_2nm_compat_scan(GSList *options)
{
	return scan(kern_ew_6200_2nm_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg)
 *
 * The original kern-scale driver has no config_get support (it sets
 * config_get to NULL). Return SR_ERR_NA for any key.
 */
static int kern_ew_6200_2nm_compat_config_get(int id, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel *ch,
		const struct sr_channel_group *cg)
{
	(void)id; (void)data; (void)sdi; (void)ch; (void)cg;
	/* Original driver has no config_get support. */
	return SR_ERR_NA;
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int kern_ew_6200_2nm_compat_config_set(int id, GVariant *data,
		struct sr_dev_inst *sdi, struct sr_channel *ch,
		struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int kern_ew_6200_2nm_compat_config_list(int info_id, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int kern_ew_6200_2nm_compat_acquisition_start(struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int kern_ew_6200_2nm_compat_acquisition_stop(const struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop(sdi);
}

/* PXView-compatible driver info struct.
 *
 * Variable name kern_ew_6200_2nm_driver_info derives from the driver's
 * .name field ("kern-ew-6200-2nm") with hyphens converted to underscores,
 * matching the convention used by center-309 -> center_309_driver_info and
 * voltcraft-k204 -> voltcraft_k204_driver_info. The hwdriver.c registration
 * references this exact name.
 */
struct sr_dev_driver kern_ew_6200_2nm_driver_info = {
	.name = "kern-ew-6200-2nm",
	.longname = "KERN EW 6200-2NM",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = kern_ew_6200_2nm_compat_init,
	.cleanup = kern_ew_6200_2nm_compat_cleanup,
	.scan = kern_ew_6200_2nm_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = kern_ew_6200_2nm_compat_config_get,
	.config_set = kern_ew_6200_2nm_compat_config_set,
	.config_list = kern_ew_6200_2nm_compat_config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = kern_ew_6200_2nm_compat_acquisition_start,
	.dev_acquisition_stop = kern_ew_6200_2nm_compat_acquisition_stop,
	.priv = NULL,
};
