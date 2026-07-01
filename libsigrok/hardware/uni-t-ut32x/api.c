/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
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

/*
 * Local helper for string index lookup.
 *
 * The compat layer's std_str_idx() has a different signature (it is a
 * config-get helper taking sdi/key/data). Standard sigrok's std_str_idx()
 * is a simple string-array lookup, so provide it locally to avoid the
 * name collision.
 */
static int local_std_str_idx(GVariant *data, const char *const strs[], size_t count)
{
	const char *str;
	size_t i;

	str = g_variant_get_string(data, NULL);
	for (i = 0; i < count; i++) {
		if (strcmp(str, strs[i]) == 0)
			return (int)i;
	}
	return -1;
}

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_THERMOMETER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_DATA_SOURCE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

/*
 * BEWARE! "T1-T2" looks like a range, and is probably not a good
 * channel name. Using it in sigrok-cli -C specs is troublesome. Use
 * "delta" instead? -- But OTOH channels are not selected by the
 * software. Instead received packets just reflect the one channel
 * that manually was selected by the user via the device's buttons.
 * So the name is not a blocker, and it matches the labels on the
 * device and in the manual. So we can get away with it.
 */
static const char *channel_names[] = {
	"T1", "T2", "T1-T2",
};

static const char *data_sources[] = {
	"Live", "Memory",
};

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	const char *conn, *serialcomm;
	struct sr_config *src;
	GSList *l, *devices;
	struct sr_serial_dev_inst *serial;
	int rc;
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	size_t i;

	/*
	 * Implementor's note: Do _not_ add a default conn value here,
	 * always expect users to specify the connection. Otherwise the
	 * UT32x driver's scan routine results in false positives, will
	 * match _any_ UT-D04 cable which uses the same USB HID chip.
	 */
	conn = NULL;
	serialcomm = "2400/8n1";
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

	devices = NULL;
	serial = sr_serial_dev_inst_new(conn, serialcomm);
	rc = serial_open(serial, SERIAL_RDWR);
	/* Cannot query/identify the device. Successful open shall suffice. */
	serial_close(serial);
	if (rc != SR_OK) {
		sr_serial_dev_inst_free(serial);
		return devices;
	}

	sdi = g_malloc0(sizeof(*sdi));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup("UNI-T");
	sdi->model = g_strdup("UT32x");
	sdi->inst_type = SR_INST_SERIAL;
	sdi->conn = serial;
	devc = g_malloc0(sizeof(*devc));
	sdi->priv = devc;
	sr_sw_limits_init(&devc->limits);
	devc->data_source = DEFAULT_DATA_SOURCE;
	for (i = 0; i < ARRAY_SIZE(channel_names); i++) {
		sr_channel_new(sdi, i, SR_CHANNEL_ANALOG, TRUE,
				channel_names[i]);
	}
	devices = g_slist_append(devices, sdi);

	return std_scan_complete_compat(di, devices);
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	devc = sdi->priv;
	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
	case SR_CONF_LIMIT_MSEC:
		return sr_sw_limits_config_get(&devc->limits, key, data);
	case SR_CONF_DATA_SOURCE:
		*data = g_variant_new_string(data_sources[devc->data_source]);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	int idx;

	(void)cg;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
	case SR_CONF_LIMIT_MSEC:
		return sr_sw_limits_config_set(&devc->limits, key, data);
	case SR_CONF_DATA_SOURCE:
		if ((idx = local_std_str_idx(data, ARRAY_AND_SIZE(data_sources))) < 0)
			return SR_ERR_ARG;
		devc->data_source = idx;
		break;
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
		return std_config_list(key, data, sdi, cg, scanopts, ARRAY_SIZE(scanopts), drvopts, ARRAY_SIZE(drvopts), devopts, ARRAY_SIZE(devopts));
	case SR_CONF_DATA_SOURCE:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(data_sources));
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
	uint8_t cmd;

	devc = sdi->priv;
	serial = sdi->conn;

	sr_sw_limits_acquisition_start(&devc->limits);
	devc->packet_len = 0;
	std_session_send_df_header(sdi, LOG_PREFIX);

	if (devc->data_source == DATA_SOURCE_LIVE)
		cmd = CMD_GET_LIVE;
	else
		cmd = CMD_GET_STORED;
	serial_write_blocking(serial, &cmd, sizeof(cmd), 0);

	serial_source_add(serial, G_IO_IN, 10,
			ut32x_handle_events, sdi);

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	/* Have the reception routine stop the acquisition. */
	sdi->status = SR_ST_STOPPING;

	return SR_OK;
}

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *uni_t_ut32x_drv_ptr;

/* Forward declaration - defined at end of file */
extern struct sr_dev_driver uni_t_ut32x_driver_info;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int uni_t_ut32x_compat_init(struct sr_context *sr_ctx)
{
	uni_t_ut32x_drv_ptr = &uni_t_ut32x_driver_info;
	return std_init(uni_t_ut32x_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int uni_t_ut32x_compat_cleanup(void)
{
	return std_cleanup(uni_t_ut32x_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *uni_t_ut32x_compat_scan(GSList *options)
{
	return scan(uni_t_ut32x_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int uni_t_ut32x_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int uni_t_ut32x_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int uni_t_ut32x_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int uni_t_ut32x_compat_acquisition_start(struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int uni_t_ut32x_compat_acquisition_stop(const struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop((struct sr_dev_inst *)sdi);
}

/* PXView-compatible driver info struct */
struct sr_dev_driver uni_t_ut32x_driver_info = {
	.name = "uni-t-ut32x",
	.longname = "UNI-T UT32x",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = uni_t_ut32x_compat_init,
	.cleanup = uni_t_ut32x_compat_cleanup,
	.scan = uni_t_ut32x_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = uni_t_ut32x_compat_config_get,
	.config_set = uni_t_ut32x_compat_config_set,
	.config_list = uni_t_ut32x_compat_config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = uni_t_ut32x_compat_acquisition_start,
	.dev_acquisition_stop = uni_t_ut32x_compat_acquisition_stop,
	.priv = NULL,
};
