/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2020 Andreas Sandberg <andreas@sandberg.pp.se>
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
#include "protocol.h"

#define RDTECH_TC_SERIALCOMM "115200/8n1"

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_ENERGYMETER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_FRAMES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_GET | SR_CONF_SET,
};

/*
 * Probe the serial port for an RDTech TC device. On success, returns a
 * populated sr_dev_inst. The caller owns the returned device.
 *
 * Adapted from the original sigrok driver: the feed_queue_analog setup is
 * removed (PXView does not provide feed_queue_analog). Channels are created
 * with sr_channel_new; the per-channel scale/mq/unit metadata is kept in
 * devc->channels (the rdtech_tc_channel_desc table) and applied at sample
 * emission time in send_channel_value() (see protocol.c).
 */
static GSList *rdtech_tc_scan(struct sr_dev_driver *di,
		const char *conn, const char *serialcomm)
{
	struct sr_serial_dev_inst *serial;
	GSList *devices = NULL;
	struct dev_context *devc;
	struct sr_dev_inst *sdi;
	size_t i;
	const struct rdtech_tc_channel_desc *pch;

	serial = sr_serial_dev_inst_new(conn, serialcomm);
	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		goto err_out;

	devc = g_malloc0(sizeof(*devc));
	sr_sw_limits_init(&devc->limits);

	if (rdtech_tc_probe(serial, devc) != SR_OK) {
		sr_err("Failed to find a supported RDTech TC device.");
		goto err_out_serial;
	}

	sdi = g_malloc0(sizeof(*sdi));
	sdi->priv = devc;
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup("RDTech");
	sdi->model = g_strdup(devc->dev_info.model_name);
	sdi->version = g_strdup(devc->dev_info.fw_ver);
	sdi->serial_num = g_strdup_printf("%08" PRIu32, devc->dev_info.serial_num);
	sdi->inst_type = SR_INST_SERIAL;
	sdi->conn = serial;

	for (i = 0; i < devc->channel_count; i++) {
		pch = &devc->channels[i];
		sr_channel_new(sdi, i, SR_CHANNEL_ANALOG, TRUE, pch->name);
	}

	devices = g_slist_append(NULL, sdi);
	serial_close(serial);

	return std_scan_complete_compat(di, devices);

err_out_serial:
	g_free(devc);
	serial_close(serial);
err_out:
	sr_serial_dev_inst_free(serial);

	return NULL;
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	const char *conn;
	const char *serialcomm;
	GSList *l;
	struct sr_config *src;

	conn = NULL;
	serialcomm = RDTECH_TC_SERIALCOMM;
	/*
	 * PXView's libsigrok does not provide sr_serial_extract_options(), so
	 * manually walk the options GSList looking for SR_CONF_CONN and
	 * SR_CONF_SERIALCOMM.
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

	return rdtech_tc_scan(di, conn, serialcomm);
}

static int config_get(uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;

	return sr_sw_limits_config_get(&devc->limits, key, data);
}

static int config_set(uint32_t key, GVariant *data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;

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

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;

	sr_sw_limits_acquisition_start(&devc->limits);
	std_session_send_df_header(sdi, LOG_PREFIX);

	serial = sdi->conn;
	serial_source_add(serial, G_IO_IN, 50,
			rdtech_tc_receive_data, sdi);

	return rdtech_tc_poll(sdi, TRUE);
}

/*
 * PXView does not provide std_serial_dev_acquisition_stop. Implement the
 * stop sequence inline: remove the serial source, close the port, and send
 * the DF_END packet. Same pattern as appa-55ii, mastech-ms6514, etc.
 */
static int dev_acquisition_stop(const struct sr_dev_inst *sdi)
{
	struct sr_serial_dev_inst *serial;

	if (!sdi)
		return SR_ERR_ARG;

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
 * sigrok's: init/cleanup/scan lack the driver parameter, config_get/set/list
 * use int keys and carry an extra sr_channel parameter, and
 * dev_acquisition_start/stop carry an extra cb_data parameter. The wrappers
 * below adapt the PXView signatures to the standard sigrok-style functions
 * defined above.
 * =========================================================================== */

static struct sr_dev_driver *rdtech_tc_drv_ptr;

/* Forward declaration - defined at end of file */
extern struct sr_dev_driver rdtech_tc_driver_info;

static int rdtech_tc_compat_init(struct sr_context *sr_ctx)
{
	rdtech_tc_drv_ptr = &rdtech_tc_driver_info;
	return std_init(rdtech_tc_drv_ptr, sr_ctx);
}

static int rdtech_tc_compat_cleanup(void)
{
	return std_cleanup(rdtech_tc_drv_ptr);
}

static GSList *rdtech_tc_compat_scan(GSList *options)
{
	return scan(rdtech_tc_drv_ptr, options);
}

static int rdtech_tc_compat_config_get(int id, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel *ch,
		const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

static int rdtech_tc_compat_config_set(int id, GVariant *data,
		struct sr_dev_inst *sdi, struct sr_channel *ch,
		struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

static int rdtech_tc_compat_config_list(int info_id, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

static int rdtech_tc_compat_acquisition_start(struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

static int rdtech_tc_compat_acquisition_stop(const struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop(sdi);
}

struct sr_dev_driver rdtech_tc_driver_info = {
	.name = "rdtech-tc",
	.longname = "RDTech TC66C USB power meter",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = rdtech_tc_compat_init,
	.cleanup = rdtech_tc_compat_cleanup,
	.scan = rdtech_tc_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = rdtech_tc_compat_config_get,
	.config_set = rdtech_tc_compat_config_set,
	.config_list = rdtech_tc_compat_config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = rdtech_tc_compat_acquisition_start,
	.dev_acquisition_stop = rdtech_tc_compat_acquisition_stop,
	.priv = NULL,
};
