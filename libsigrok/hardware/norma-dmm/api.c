/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Matthias Heidbrink <m-sigrok@heidbrink.biz>
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
	SR_CONF_MULTIMETER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_SET,
};

#define BUF_MAX 50

#define SERIALCOMM "4800/8n1/dtr=1/rts=0/flow=1"

/*
 * Forward declarations of the two PXView-compatible driver_info structs.
 * The get_brandstr()/get_typestr() helpers compare the incoming driver
 * pointer against these addresses to pick the right brand/model strings.
 * Both drivers share the same scan/config/acquisition callbacks; only the
 * .name/.longname differ.
 */
extern struct sr_dev_driver norma_dmm_driver_info;
extern struct sr_dev_driver siemens_b102x_driver_info;

static const char *get_brandstr(struct sr_dev_driver *drv)
{
	return (drv == &norma_dmm_driver_info) ? "Norma" : "Siemens";
}

static const char *get_typestr(int type, struct sr_dev_driver *drv)
{
	static const char *nameref[5][2] = {
		{"DM910", "B1024"},
		{"DM920", "B1025"},
		{"DM930", "B1026"},
		{"DM940", "B1027"},
		{"DM950", "B1028"},
	};

	if ((type < 1) || (type > 5))
		return "Unknown type!";

	return nameref[type - 1][(drv == &siemens_b102x_driver_info)];
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_config *src;
	struct sr_serial_dev_inst *serial;
	GSList *l, *devices;
	int len, cnt, auxtype;
	const char *conn, *serialcomm;
	char *buf;
	char req[10];

	devices = NULL;
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
		serialcomm = SERIALCOMM;

	serial = sr_serial_dev_inst_new(conn, serialcomm);

	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		return NULL;

	buf = g_malloc(BUF_MAX);

	snprintf(req, sizeof(req), "%s\r\n",
		 nmadmm_requests[NMADMM_REQ_IDN].req_str);
	g_usleep(150 * 1000); /* Wait a little to allow serial port to settle. */
	for (cnt = 0; cnt < 7; cnt++) {
		/*
		 * PXView's compat serial_timeout() takes 3 args:
		 * serial_timeout(serial, baudrate, bytes). The original
		 * standard sigrok used the 2-arg form which read the
		 * baudrate from the serial port's own config.
		 */
		if (serial_write_blocking(serial, req, strlen(req),
				serial_timeout(serial, NMADMM_BAUDRATE, strlen(req))) < 0) {
			sr_err("Unable to send identification request.");
			g_free(buf);
			return NULL;
		}
		len = BUF_MAX;
		serial_readline(serial, &buf, &len, NMADMM_TIMEOUT_MS);
		if (!len)
			continue;
		buf[BUF_MAX - 1] = '\0';

		/* Match ID string, e.g. "1834 065 V1.06,IF V1.02" (DM950). */
		if (g_regex_match_simple("^1834 [^,]*,IF V*", (char *)buf, 0, 0)) {
			auxtype = xgittoint(buf[7]);
			sr_spew("%s %s DMM %s detected!", get_brandstr(di), get_typestr(auxtype, di), buf + 9);

			sdi = g_malloc0(sizeof(struct sr_dev_inst));
			sdi->status = SR_ST_INACTIVE;
			sdi->vendor = g_strdup(get_brandstr(di));
			sdi->model = g_strdup(get_typestr(auxtype, di));
			sdi->version = g_strdup(buf + 9);
			sdi->inst_type = SR_INST_SERIAL;
			devc = g_malloc0(sizeof(struct dev_context));
			sr_sw_limits_init(&devc->limits);
			devc->type = auxtype;
			sdi->conn = serial;
			sdi->priv = devc;
			sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "P1");
			devices = g_slist_append(devices, sdi);
			break;
		}

		/*
		 * The interface of the DM9x0 contains a cap that needs to
		 * charge for up to 10s before the interface works, if not
		 * powered externally. Therefore wait a little to improve
		 * chances.
		 */
		if (cnt == 3) {
			sr_info("Waiting 5s to allow interface to settle.");
			g_usleep(5 * 1000 * 1000);
		}
	}

	g_free(buf);

	serial_close(serial);
	if (!devices)
		sr_serial_dev_inst_free(serial);

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

	sr_sw_limits_acquisition_start(&devc->limits);
	std_session_send_df_header(sdi, LOG_PREFIX);

	serial = sdi->conn;
	/*
	 * PXView's compat serial_source_add() takes 5 args without the
	 * session parameter (it fishes the session out internally):
	 * serial_source_add(serial, events, timeout, cb, sdi).
	 * The callback receives the sdi directly (no void* cast needed).
	 */
	serial_source_add(serial, G_IO_IN, 100,
			norma_dmm_receive_data, sdi);

	return SR_OK;
}

/*
 * Local dev_acquisition_stop implementation.
 * PXView does not provide std_serial_dev_acquisition_stop, so we implement
 * the stop sequence locally: remove the serial source, close the serial
 * port, and send the DF_END packet (same pattern as agilent-dmm/fluke-dmm).
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

/* =====================================================================
 * Compat wrappers for norma_dmm_driver_info
 * ===================================================================== */

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *norma_dmm_drv_ptr;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int norma_dmm_compat_init(struct sr_context *sr_ctx)
{
	norma_dmm_drv_ptr = &norma_dmm_driver_info;
	return std_init(norma_dmm_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int norma_dmm_compat_cleanup(void)
{
	return std_cleanup(norma_dmm_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *norma_dmm_compat_scan(GSList *options)
{
	return scan(norma_dmm_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int norma_dmm_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	(void)sdi;
	(void)cg;
	(void)data;
	(void)id;
	/* No config_get implementation in original driver */
	return SR_ERR_NA;
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int norma_dmm_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int norma_dmm_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int norma_dmm_compat_acquisition_start(struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int norma_dmm_compat_acquisition_stop(const struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop(sdi);
}

/* PXView-compatible driver info struct: Norma DM9x0 */
struct sr_dev_driver norma_dmm_driver_info = {
	.name = "norma-dmm",
	.longname = "Norma DM9x0 DMMs",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = norma_dmm_compat_init,
	.cleanup = norma_dmm_compat_cleanup,
	.scan = norma_dmm_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = norma_dmm_compat_config_get,
	.config_set = norma_dmm_compat_config_set,
	.config_list = norma_dmm_compat_config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = norma_dmm_compat_acquisition_start,
	.dev_acquisition_stop = norma_dmm_compat_acquisition_stop,
	.priv = NULL,
};

/* =====================================================================
 * Compat wrappers for siemens_b102x_driver_info
 * (shares scan/config/acquisition with norma_dmm; only branding differs)
 * ===================================================================== */

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *siemens_b102x_drv_ptr;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int siemens_b102x_compat_init(struct sr_context *sr_ctx)
{
	siemens_b102x_drv_ptr = &siemens_b102x_driver_info;
	return std_init(siemens_b102x_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int siemens_b102x_compat_cleanup(void)
{
	return std_cleanup(siemens_b102x_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *siemens_b102x_compat_scan(GSList *options)
{
	return scan(siemens_b102x_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int siemens_b102x_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	(void)sdi;
	(void)cg;
	(void)data;
	(void)id;
	/* No config_get implementation in original driver */
	return SR_ERR_NA;
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int siemens_b102x_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int siemens_b102x_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int siemens_b102x_compat_acquisition_start(struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int siemens_b102x_compat_acquisition_stop(const struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop(sdi);
}

/* PXView-compatible driver info struct: Siemens B102x */
struct sr_dev_driver siemens_b102x_driver_info = {
	.name = "siemens-b102x",
	.longname = "Siemens B102x DMMs",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = siemens_b102x_compat_init,
	.cleanup = siemens_b102x_compat_cleanup,
	.scan = siemens_b102x_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = siemens_b102x_compat_config_get,
	.config_set = siemens_b102x_compat_config_set,
	.config_list = siemens_b102x_compat_config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = siemens_b102x_compat_acquisition_start,
	.dev_acquisition_stop = siemens_b102x_compat_acquisition_stop,
	.priv = NULL,
};
