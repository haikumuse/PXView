/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2023 Mathieu Pilato <pilato.mathieu@free.fr>
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

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_ENERGYMETER,
	SR_CONF_POWERMETER,
	SR_CONF_ELECTRONIC_LOAD,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_MSEC | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_FRAMES | SR_CONF_GET | SR_CONF_SET,
};

static int create_channels_feed_queues(struct sr_dev_inst *sdi,
	struct dev_context *devc)
{
	size_t i;
	struct sr_channel *sr_ch;
	const struct atorch_channel_desc *at_ch;
	struct feed_queue_analog *feed;
	const struct atorch_device_profile *p;

	p = devc->profile;
	devc->feeds = g_malloc0(p->channel_count * sizeof(devc->feeds[0]));
	for (i = 0; i < p->channel_count; i++) {
		at_ch = &p->channels[i];
		sr_ch = sr_channel_new(sdi, i, SR_CHANNEL_ANALOG, TRUE, at_ch->name);
		feed = feed_queue_analog_alloc(sdi, 1, at_ch->digits, sr_ch);
		feed_queue_analog_mq_unit(feed, at_ch->mq, at_ch->flags, at_ch->unit);
		feed_queue_analog_scale_offset(feed, &at_ch->scale, NULL);
		devc->feeds[i] = feed;
	}

	return SR_OK;
}

static GSList *atorch_scan(struct sr_dev_driver *di,
	const char *conn, const char *serialcomm)
{
	struct sr_serial_dev_inst *serial;
	GSList *devices;
	struct dev_context *devc;
	struct sr_dev_inst *sdi;

	serial = sr_serial_dev_inst_new(conn, serialcomm);
	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		goto err_out;

	devc = g_malloc0(sizeof(*devc));

	if (atorch_probe(serial, devc) != SR_OK) {
		sr_err("Failed to find a supported Atorch device.");
		goto err_out_serial;
	}

	sr_sw_limits_init(&devc->limits);

	sdi = g_malloc0(sizeof(*sdi));
	sdi->priv = devc;
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup("Atorch");
	sdi->model = g_strdup(devc->profile->device_name);
	sdi->version = NULL;
	sdi->inst_type = SR_INST_SERIAL;
	sdi->conn = serial;

	create_channels_feed_queues(sdi, devc);

	serial_close(serial);

	devices = g_slist_append(NULL, sdi);
	return std_scan_complete_compat(di, devices);

err_out_serial:
	g_free(devc);
	serial_close(serial);
err_out:
	sr_serial_dev_inst_free(serial);

	return NULL;
}

/*
 * PXView's libsigrok does not provide sr_serial_extract_options(), so
 * manually walk the options GSList looking for SR_CONF_CONN and
 * SR_CONF_SERIALCOMM (same pattern as colead-slm/api.c, hp-3457a/api.c,
 * hp-59306a/api.c and scpi-pps/api.c).
 */
static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	const char *conn, *serialcomm;
	GSList *l;
	struct sr_config *src;

	conn = NULL;
	serialcomm = "9600/8n1";

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

	if (!conn || !*conn)
		return NULL;

	return atorch_scan(di, conn, serialcomm);
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	if (!sdi || !data)
		return SR_ERR_ARG;

	devc = sdi->priv;
	if (!devc)
		return SR_ERR_ARG;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
	case SR_CONF_LIMIT_FRAMES:
	case SR_CONF_LIMIT_MSEC:
		return sr_sw_limits_config_get(&devc->limits, key, data);
	default:
		return SR_ERR_NA;
	}
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

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
	case SR_CONF_LIMIT_FRAMES:
	case SR_CONF_LIMIT_MSEC:
		return sr_sw_limits_config_set(&devc->limits, key, data);
	default:
		return SR_ERR_NA;
	}
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct sr_serial_dev_inst *serial;
	struct dev_context *devc;

	serial = sdi->conn;
	devc = sdi->priv;

	sr_sw_limits_acquisition_start(&devc->limits);
	/* Rule 2: std_session_send_df_header(sdi) -> 2-arg form. */
	std_session_send_df_header(sdi, LOG_PREFIX);

	/*
	 * Rule 14: PXView's serial_source_add is 5-arg (no session first
	 * parameter) and the callback is typed
	 *   int (*)(int fd, int revents, const struct sr_dev_inst *sdi)
	 * so pass sdi directly (no (void *) cast).
	 */
	serial_source_add(serial, G_IO_IN, 100,
		atorch_receive_data_callback, sdi);

	return SR_OK;
}

/*
 * PXView does not provide std_serial_dev_acquisition_stop. Implement the
 * stop sequence inline: remove the serial source, close the port, and send
 * the DF_END packet. Same pattern as appa-55ii, rdtech-um, colead-slm, etc.
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

static void clear_helper(struct dev_context *devc)
{
	size_t idx;

	if (!devc)
		return;

	if (devc->feeds && devc->profile) {
		for (idx = 0; idx < devc->profile->channel_count; idx++)
			feed_queue_analog_free(devc->feeds[idx]);
		g_free(devc->feeds);
	}
}

/*
 * dev_clear: standard sigrok's std_dev_clear_with_callback() is not
 * available in the PXView compat layer. Inline the logic: iterate the
 * driver's instance list, call clear_helper on each devc to release the
 * feed queues, then use std_dev_clear_compat to free everything. Same
 * pattern as itech-it8500/api.c and chronovu-la/api.c.
 */
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
		if (sdi && sdi->priv)
			clear_helper(sdi->priv);
	}

	return std_dev_clear_compat(di);
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

static struct sr_dev_driver *atorch_drv_ptr;

/* Forward declaration - defined at end of file (rule 7). */
extern struct sr_dev_driver atorch_driver_info;

static int atorch_compat_init(struct sr_context *sr_ctx)
{
	atorch_drv_ptr = &atorch_driver_info;
	return std_init(atorch_drv_ptr, sr_ctx);
}

static int atorch_compat_cleanup(void)
{
	return std_cleanup(atorch_drv_ptr);
}

static GSList *atorch_compat_scan(GSList *options)
{
	return scan(atorch_drv_ptr, options);
}

static int atorch_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

static int atorch_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	/*
	 * Rule 10: config_channel_set is merged into config_set. The extra
	 * ch parameter from PXView's signature is dropped (standard sigrok
	 * config_set has no per-channel variant).
	 */
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

static int atorch_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

static int atorch_compat_acquisition_start(struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

static int atorch_compat_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop(sdi);
}

/* Rule 8: driver_info with compat fields filled in. */
struct sr_dev_driver atorch_driver_info = {
	.name = "atorch",
	.longname = "atorch meters and loads",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = atorch_compat_init,
	.cleanup = atorch_compat_cleanup,
	.scan = atorch_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = atorch_compat_config_get,
	.config_set = atorch_compat_config_set,
	.config_list = atorch_compat_config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = atorch_compat_acquisition_start,
	.dev_acquisition_stop = atorch_compat_acquisition_stop,
	.priv = NULL,
};
