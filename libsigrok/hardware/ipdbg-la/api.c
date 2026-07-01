/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2016 Eva Kissling <eva.kissling@bluewin.ch>
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

static const uint32_t drvopts[] = {
	SR_CONF_LOGIC_ANALYZER,
};

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
};

static const uint32_t devopts[] = {
	SR_CONF_TRIGGER_MATCH | SR_CONF_LIST,
	SR_CONF_CAPTURE_RATIO | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET,
};

static const int32_t trigger_matches[] = {
	SR_TRIGGER_ZERO,
	SR_TRIGGER_ONE,
	SR_TRIGGER_RISING,
	SR_TRIGGER_FALLING,
	SR_TRIGGER_EDGE,
};

static void ipdbg_la_split_addr_port(const char *conn, char **addr,
	char **port)
{
	char **strs = g_strsplit(conn, "/", 3);

	*addr = g_strdup(strs[1]);
	*port = g_strdup(strs[2]);

	g_strfreev(strs);
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	GSList *devices;
	const char *conn;
	struct sr_config *src;
	GSList *l;

	(void)di;

	devices = NULL;
	conn = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case SR_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		}
	}

	if (!conn)
		return NULL;

	struct ipdbg_la_tcp *tcp = ipdbg_la_tcp_new();

	ipdbg_la_split_addr_port(conn, &tcp->address, &tcp->port);

	if (!tcp->address)
		return NULL;

	if (ipdbg_la_tcp_open(tcp) != SR_OK)
		return NULL;

	ipdbg_la_send_reset(tcp);
	ipdbg_la_send_reset(tcp);

	if (ipdbg_la_request_id(tcp) != SR_OK)
		return NULL;

	struct sr_dev_inst *sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup("ipdbg.org");
	sdi->model = g_strdup("IPDBG LA");
	sdi->version = g_strdup("v1.0");
	sdi->driver = di;

	struct dev_context *devc = ipdbg_la_dev_new();
	sdi->priv = devc;

	ipdbg_la_get_addrwidth_and_datawidth(tcp, devc);

	sr_dbg("addr_width = %d, data_width = %d\n", devc->addr_width,
		devc->data_width);
	sr_dbg("limit samples = %" PRIu64 "\n", devc->limit_samples_max);

	for (uint32_t i = 0; i < devc->data_width; i++) {
		char *name = g_strdup_printf("CH%d", i);
		sr_channel_new(sdi, i, SR_CHANNEL_LOGIC, TRUE, name);
		g_free(name);
	}

	sdi->inst_type = SR_INST_USER;
	sdi->conn = tcp;

	ipdbg_la_tcp_close(tcp);

	devices = g_slist_append(devices, sdi);

	return std_scan_complete_compat(di, devices);
}

static int dev_clear(const struct sr_dev_driver *di)
{
	/*
	 * std_dev_clear_with_callback is not available in the compat layer.
	 * Inline the per-instance tcp cleanup, then use std_dev_clear_compat
	 * to free everything. PXView stores driver private data in di->priv
	 * (a struct compat_drv_context *) instead of di->context.
	 */
	struct compat_drv_context *drvc;
	GSList *l;
	struct sr_dev_inst *sdi;

	if (!di || !di->priv)
		return SR_ERR_ARG;

	drvc = di->priv;
	for (l = drvc->instances; l; l = l->next) {
		sdi = l->data;
		struct ipdbg_la_tcp *tcp = sdi->conn;
		if (tcp) {
			ipdbg_la_tcp_close(tcp);
			ipdbg_la_tcp_free(tcp);
			g_free(tcp);
		}
		sdi->conn = NULL;
	}

	return std_dev_clear_compat(di);
}

static int dev_open(struct sr_dev_inst *sdi)
{
	struct ipdbg_la_tcp *tcp = sdi->conn;

	if (!tcp)
		return SR_ERR;

	if (ipdbg_la_tcp_open(tcp) != SR_OK)
		return SR_ERR;

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	/* Should be called before a new call to scan(). */
	struct ipdbg_la_tcp *tcp = sdi->conn;

	if (tcp)
		ipdbg_la_tcp_close(tcp);

	sdi->conn = NULL;

	return SR_OK;
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc = sdi->priv;

	(void)cg;

	switch (key) {
	case SR_CONF_CAPTURE_RATIO:
		*data = g_variant_new_uint64(devc->capture_ratio);
		break;
	case SR_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc = sdi->priv;

	(void)cg;

	switch (key) {
	case SR_CONF_CAPTURE_RATIO:
		devc->capture_ratio = g_variant_get_uint64(data);
		break;
	case SR_CONF_LIMIT_SAMPLES:
		{
			uint64_t limit_samples = g_variant_get_uint64(data);
			if (limit_samples <= devc->limit_samples_max)
				devc->limit_samples = limit_samples;
		}
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
		return std_config_list(key, data, sdi, cg, scanopts,
			ARRAY_SIZE(scanopts), drvopts, ARRAY_SIZE(drvopts),
			devopts, ARRAY_SIZE(devopts));
	case SR_CONF_TRIGGER_MATCH:
		*data = std_gvar_array_i32(ARRAY_AND_SIZE(trigger_matches));
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

/*
 * PXView's sr_receive_data_callback_t takes
 * (int fd, int revents, const struct sr_dev_inst *sdi)
 * but the original ipdbg_la_receive_data takes
 * (int fd, int revents, void *cb_data).
 * This wrapper adapts the PXView callback signature to the original function.
 */
static int ipdbg_la_receive_data_wrapper(int fd, int revents,
	const struct sr_dev_inst *sdi)
{
	return ipdbg_la_receive_data(fd, revents, (void *)sdi);
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct ipdbg_la_tcp *tcp = sdi->conn;
	struct dev_context *devc = sdi->priv;

	ipdbg_la_convert_trigger(sdi);
	ipdbg_la_send_trigger(devc, tcp);
	ipdbg_la_send_delay(devc, tcp);

	/* If the device stops sending for longer than it takes to send a byte,
	 * that means it's finished. But wait at least 100 ms to be safe.
	 *
	 * PXView compat: sr_session_source_add() takes
	 * (poll_object, events, timeout, cb, sdi) -- no session parameter.
	 */
	sr_session_source_add(tcp->socket, G_IO_IN, 100,
		ipdbg_la_receive_data_wrapper, sdi);

	ipdbg_la_send_start(tcp);

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	struct ipdbg_la_tcp *tcp = sdi->conn;
	struct dev_context *devc = sdi->priv;

	const size_t bufsize = 1024;
	uint8_t buffer[bufsize];

	if (devc->num_transfers > 0) {
		while (devc->num_transfers <
			(devc->limit_samples_max * devc->data_width_bytes)) {
			int recd = ipdbg_la_tcp_receive(tcp, buffer, bufsize);
			if (recd > 0)
				devc->num_transfers += recd;
		}
	}

	ipdbg_la_send_reset(tcp);
	ipdbg_la_abort_acquisition(sdi);

	return SR_OK;
}

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *ipdbg_la_drv_ptr;

/* Forward declaration - defined at end of file */
extern struct sr_dev_driver ipdbg_la_driver_info;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int ipdbg_la_compat_init(struct sr_context *sr_ctx)
{
	ipdbg_la_drv_ptr = &ipdbg_la_driver_info;
	return std_init(ipdbg_la_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int ipdbg_la_compat_cleanup(void)
{
	/* Call dev_clear to free per-device resources before cleanup */
	dev_clear(ipdbg_la_drv_ptr);
	return std_cleanup(ipdbg_la_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *ipdbg_la_compat_scan(GSList *options)
{
	return scan(ipdbg_la_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int ipdbg_la_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int ipdbg_la_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int ipdbg_la_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int ipdbg_la_compat_acquisition_start(struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int ipdbg_la_compat_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop((struct sr_dev_inst *)sdi);
}

/* PXView-compatible driver info struct */
struct sr_dev_driver ipdbg_la_driver_info = {
	.name = "ipdbg-la",
	.longname = "IPDBG LA",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = ipdbg_la_compat_init,
	.cleanup = ipdbg_la_compat_cleanup,
	.scan = ipdbg_la_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = ipdbg_la_compat_config_get,
	.config_set = ipdbg_la_compat_config_set,
	.config_list = ipdbg_la_compat_config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = ipdbg_la_compat_acquisition_start,
	.dev_acquisition_stop = ipdbg_la_compat_acquisition_stop,
	.priv = NULL,
};
