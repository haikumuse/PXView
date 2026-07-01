/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014 Janne Huttunen <jahuttun@gmail.com>
 * Copyright (C) 2019 Gerhard Sittig <gerhard.sittig@gmx.net>
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

/*
 * PXView port of the serial-lcr driver shell.
 *
 * Upstream registered two SR_REGISTER_DEV_DRIVER_LIST groups
 * (lcr_es51919_drivers, lcr_vc4080_drivers) totalling 6 device variants
 * (4 ES51919-based: DER EE DE-5000, MASTECH MS5308, PeakTech 2170,
 * UNI-T UT612; 2 VC4080-based: PeakTech 2165, Voltcraft 4080). PXView
 * registers a single `serial_lcr_driver_info`; the variant table below
 * is a static `lcr_info[]` array that scan() walks, probing each
 * chipset's packet format against the serial stream until one matches.
 */

#include "hardware/compat/compat.h"
#include <string.h>
#include "protocol.h"

/*
 * Forward declaration. Must have external linkage (not static) so that
 * hwdriver.c's drivers_list[] entry `&serial_lcr_driver_info` resolves
 * at link time. Matches the gwinstek-gpd / hp-3457a / colead-slm pattern.
 */
extern struct sr_dev_driver serial_lcr_driver_info;

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_LCRMETER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_FRAMES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_SET,
	SR_CONF_OUTPUT_FREQUENCY | SR_CONF_GET | SR_CONF_LIST,
	SR_CONF_EQUIV_CIRCUIT_MODEL | SR_CONF_GET | SR_CONF_LIST,
};

/*
 * Per-device-variant descriptor table. Replaces the upstream
 * SR_REGISTER_DEV_DRIVER_LIST + LCR_ES51919()/LCR_VC4080() macros.
 * Each entry carries the chipset-specific comm params, packet size and
 * parser hooks that scan()/config_*() dispatch through devc->lcr_info.
 */
static const struct lcr_info lcr_info[] = {
	/* ES51919-based devices (passive, no packet_request). */
	{
		.vendor = "DER EE", .model = "DE-5000",
		.comm = ES51919_COMM_PARAM,
		.channel_count = ES51919_CHANNEL_COUNT, .channel_formats = NULL,
		.packet_size = ES51919_PACKET_SIZE,
		.req_timeout_ms = 0, .packet_request = NULL,
		.packet_valid = es51919_packet_valid,
		.packet_parse = es51919_packet_parse,
		.config_get = NULL, .config_set = NULL,
		.config_list = es51919_config_list,
	},
	{
		.vendor = "MASTECH", .model = "MS5308",
		.comm = ES51919_COMM_PARAM,
		.channel_count = ES51919_CHANNEL_COUNT, .channel_formats = NULL,
		.packet_size = ES51919_PACKET_SIZE,
		.req_timeout_ms = 0, .packet_request = NULL,
		.packet_valid = es51919_packet_valid,
		.packet_parse = es51919_packet_parse,
		.config_get = NULL, .config_set = NULL,
		.config_list = es51919_config_list,
	},
	{
		.vendor = "PeakTech", .model = "2170",
		.comm = ES51919_COMM_PARAM,
		.channel_count = ES51919_CHANNEL_COUNT, .channel_formats = NULL,
		.packet_size = ES51919_PACKET_SIZE,
		.req_timeout_ms = 0, .packet_request = NULL,
		.packet_valid = es51919_packet_valid,
		.packet_parse = es51919_packet_parse,
		.config_get = NULL, .config_set = NULL,
		.config_list = es51919_config_list,
	},
	{
		.vendor = "UNI-T", .model = "UT612",
		.comm = ES51919_COMM_PARAM,
		.channel_count = ES51919_CHANNEL_COUNT, .channel_formats = NULL,
		.packet_size = ES51919_PACKET_SIZE,
		.req_timeout_ms = 0, .packet_request = NULL,
		.packet_valid = es51919_packet_valid,
		.packet_parse = es51919_packet_parse,
		.config_get = NULL, .config_set = NULL,
		.config_list = es51919_config_list,
	},
	/* VC4080-based devices (require periodic packet requests). */
	{
		.vendor = "PeakTech", .model = "2165",
		.comm = VC4080_COMM_PARAM,
		.channel_count = VC4080_CHANNEL_COUNT,
		.channel_formats = vc4080_channel_formats,
		.packet_size = VC4080_PACKET_SIZE,
		.req_timeout_ms = 500, .packet_request = vc4080_packet_request,
		.packet_valid = vc4080_packet_valid,
		.packet_parse = vc4080_packet_parse,
		.config_get = NULL, .config_set = NULL,
		.config_list = vc4080_config_list,
	},
	{
		.vendor = "Voltcraft", .model = "4080",
		.comm = VC4080_COMM_PARAM,
		.channel_count = VC4080_CHANNEL_COUNT,
		.channel_formats = vc4080_channel_formats,
		.packet_size = VC4080_PACKET_SIZE,
		.req_timeout_ms = 500, .packet_request = vc4080_packet_request,
		.packet_valid = vc4080_packet_valid,
		.packet_parse = vc4080_packet_parse,
		.config_get = NULL, .config_set = NULL,
		.config_list = vc4080_config_list,
	},
};

/*
 * Stream-detect callback hook: after the chipset's packet_valid check
 * passes, run the chipset's packet_parse against the detected packet so
 * the current output_freq / circuit_model get extracted before the
 * acquisition starts. Uses a file-scoped sdi pointer (same approach as
 * the upstream source).
 */
static struct sr_dev_inst *scan_packet_check_devinst;

static void scan_packet_check_setup(struct sr_dev_inst *sdi)
{
	scan_packet_check_devinst = sdi;
}

static gboolean scan_packet_check_func(const uint8_t *buf)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	const struct lcr_info *lcr;
	struct lcr_parse_info *info;

	sdi = scan_packet_check_devinst;
	if (!sdi)
		return FALSE;
	devc = sdi->priv;
	if (!devc)
		return FALSE;
	lcr = devc->lcr_info;
	if (!lcr)
		return FALSE;

	if (!lcr->packet_valid(buf))
		return FALSE;

	info = &devc->parse_info;
	memset(info, 0, sizeof(*info));
	if (lcr->packet_parse(buf, NULL, NULL, info) == SR_OK) {
		devc->output_freq = info->output_freq;
		if (info->circuit_model)
			devc->circuit_model = info->circuit_model;
	}

	return TRUE;
}

static int scan_lcr_port(const struct lcr_info *lcr,
	const char *conn, struct sr_serial_dev_inst *serial)
{
	size_t len;
	uint8_t buf[128];
	int ret;
	size_t dropped;

	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		return SR_ERR_IO;
	sr_info("Probing serial port %s.", conn);

	/*
	 * No supported device provides an "identify" command, and only the
	 * VC4080 family needs a packet request before it emits data. Check
	 * whether the stream's packets match the probed chipset's format.
	 */
	if (lcr->packet_request) {
		ret = lcr->packet_request(serial);
		if (ret < 0) {
			sr_err("Failed to request packet: %d.", ret);
			goto scan_port_cleanup;
		}
	}
	len = sizeof(buf);
	/*
	 * Rule: serial_stream_detect is 7-arg in PXView
	 * (serial, buf, &len, packet_size, is_valid, timeout_ms, baudrate).
	 * The upstream 8-arg call passed (..., NULL, NULL, 3000); the two
	 * NULLs collapse and 3000 becomes timeout_ms, baudrate is 0.
	 */
	ret = serial_stream_detect(serial, buf, &len,
		lcr->packet_size, lcr->packet_valid, 3000, 0);
	if (ret != SR_OK)
		goto scan_port_cleanup;

	dropped = len - lcr->packet_size;
	if (dropped > 2 * lcr->packet_size)
		sr_warn("Had to drop unexpected amounts of data.");

	sr_info("Found %s %s device on port %s.", lcr->vendor, lcr->model, conn);

scan_port_cleanup:
	if (ret != SR_OK)
		serial_close(serial);

	return ret;
}

static struct sr_dev_inst *create_lcr_sdi(const struct lcr_info *lcr,
	struct sr_serial_dev_inst *serial)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	size_t ch_idx;
	const char **ch_fmts;
	const char *fmt;
	char ch_name[8];

	sdi = g_malloc0(sizeof(*sdi));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup(lcr->vendor);
	sdi->model = g_strdup(lcr->model);
	sdi->inst_type = SR_INST_SERIAL;
	sdi->conn = serial;
	devc = g_malloc0(sizeof(*devc));
	sdi->priv = devc;
	devc->lcr_info = lcr;
	sr_sw_limits_init(&devc->limits);
	ch_fmts = lcr->channel_formats;
	for (ch_idx = 0; ch_idx < lcr->channel_count; ch_idx++) {
		fmt = (ch_fmts && ch_fmts[ch_idx]) ? ch_fmts[ch_idx] : "P%zu";
		snprintf(ch_name, sizeof(ch_name), fmt, ch_idx + 1);
		sr_channel_new(sdi, ch_idx, SR_CHANNEL_ANALOG, TRUE, ch_name);
	}

	return sdi;
}

static int read_lcr_port(struct sr_dev_inst *sdi,
	const struct lcr_info *lcr, struct sr_serial_dev_inst *serial)
{
	size_t len;
	uint8_t buf[128];
	int ret;

	serial_flush(serial);
	if (lcr->packet_request) {
		ret = lcr->packet_request(serial);
		if (ret < 0) {
			sr_err("Failed to request packet: %d.", ret);
			return ret;
		}
	}

	/*
	 * Receive a few more packets (and process them) so the current
	 * output frequency and circuit model get detected before the
	 * acquisition starts. The stream-detect phase only checked packet
	 * validity; this phase runs the scan_packet_check hook which also
	 * parses the packet.
	 */
	sr_info("Retrieving current acquisition parameters.");
	len = sizeof(buf);
	scan_packet_check_setup(sdi);
	ret = serial_stream_detect(serial, buf, &len,
		lcr->packet_size, scan_packet_check_func, 1500, 0);
	scan_packet_check_setup(NULL);

	return ret;
}

/*
 * scan() walks the lcr_info[] variant table. For each variant it opens
 * the serial port (using the user-provided serialcomm if given,
 * otherwise the chipset's default), runs stream detection with that
 * chipset's packet_valid, and returns the first match. Replaces the
 * upstream `lcr = (struct lcr_info *)di;` cast pattern.
 */
static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	const struct lcr_info *lcr;
	const struct sr_config *src;
	GSList *l;
	const char *conn, *user_serialcomm, *serialcomm;
	struct sr_serial_dev_inst *serial;
	int ret;
	struct sr_dev_inst *sdi;
	size_t i;

	(void)di;

	conn = NULL;
	user_serialcomm = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case SR_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		case SR_CONF_SERIALCOMM:
			user_serialcomm = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	if (!conn)
		return std_scan_complete_compat(di, NULL);

	for (i = 0; i < ARRAY_SIZE(lcr_info); i++) {
		lcr = &lcr_info[i];
		serialcomm = user_serialcomm ? user_serialcomm : lcr->comm;
		serial = sr_serial_dev_inst_new(conn, serialcomm);
		ret = scan_lcr_port(lcr, conn, serial);
		if (ret != SR_OK) {
			sr_serial_dev_inst_free(serial);
			continue;
		}
		sdi = create_lcr_sdi(lcr, serial);
		(void)read_lcr_port(sdi, lcr, serial);
		serial_close(serial);
		return std_scan_complete_compat(di, g_slist_append(NULL, sdi));
	}

	return std_scan_complete_compat(di, NULL);
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	const struct lcr_info *lcr;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;

	switch (key) {
	case SR_CONF_LIMIT_FRAMES:
	case SR_CONF_LIMIT_MSEC:
		return sr_sw_limits_config_get(&devc->limits, key, data);
	case SR_CONF_OUTPUT_FREQUENCY:
		*data = g_variant_new_double(devc->output_freq);
		return SR_OK;
	case SR_CONF_EQUIV_CIRCUIT_MODEL:
		if (!devc->circuit_model)
			return SR_ERR_NA;
		*data = g_variant_new_string(devc->circuit_model);
		return SR_OK;
	default:
		lcr = devc->lcr_info;
		if (!lcr || !lcr->config_get)
			return SR_ERR_NA;
		return lcr->config_get(key, data, sdi, cg);
	}
	/* UNREACH */
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	const struct lcr_info *lcr;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;

	switch (key) {
	case SR_CONF_LIMIT_FRAMES:
	case SR_CONF_LIMIT_MSEC:
		return sr_sw_limits_config_set(&devc->limits, key, data);
	default:
		lcr = devc->lcr_info;
		if (!lcr || !lcr->config_set)
			return SR_ERR_NA;
		return lcr->config_set(key, data, sdi, cg);
	}
	/* UNREACH */
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	const struct lcr_info *lcr;

	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
	case SR_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg,
			scanopts, drvopts, devopts);
	default:
		break;
	}

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;
	switch (key) {
	default:
		lcr = devc->lcr_info;
		if (!lcr || !lcr->config_list)
			return SR_ERR_NA;
		return lcr->config_list(key, data, sdi, cg);
	}
	/* UNREACH */
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;

	devc = sdi->priv;

	/*
	 * Clear values gathered during scan or a previous acquisition so
	 * this acquisition's data feed starts with meta packets before the
	 * first measurement values, and communicates subsequent changes.
	 */
	devc->output_freq = 0;
	devc->circuit_model = NULL;
	devc->buf_rxpos = 0;
	devc->req_next_at = 0;

	sr_sw_limits_acquisition_start(&devc->limits);
	std_session_send_df_header(sdi, LOG_PREFIX);

	/*
	 * Rule: PXView's serial_source_add is 5-arg
	 * (serial, events, timeout, cb, sdi) -- no leading session param.
	 */
	serial = sdi->conn;
	serial_source_add(serial, G_IO_IN, 50,
		lcr_receive_data, sdi);

	return SR_OK;
}

/* ===========================================================================
 * PXView compat wrapper layer
 *
 * PXView's sr_dev_driver callbacks differ from standard sigrok's:
 * init/cleanup lack the driver parameter, scan lacks the driver param,
 * config_get/set/list use int keys and carry an extra sr_channel param,
 * and dev_acquisition_start/stop carry an extra cb_data param. The
 * wrappers below adapt PXView's signatures to the standard-style
 * functions defined above. Matches the gwinstek-gpd driver layout.
 * ========================================================================== */

static struct sr_dev_driver *serial_lcr_drv_ptr;

static int serial_lcr_compat_init(struct sr_context *sr_ctx)
{
	serial_lcr_drv_ptr = &serial_lcr_driver_info;
	return std_init(serial_lcr_drv_ptr, sr_ctx);
}

static int serial_lcr_compat_cleanup(void)
{
	std_dev_clear(serial_lcr_drv_ptr);
	return std_cleanup(serial_lcr_drv_ptr);
}

static GSList *serial_lcr_compat_scan(GSList *options)
{
	return scan(serial_lcr_drv_ptr, options);
}

static int serial_lcr_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

static int serial_lcr_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

static int serial_lcr_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

static int serial_lcr_compat_acquisition_start(struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

static int serial_lcr_compat_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return serial_lcr_dev_acquisition_stop(sdi);
}

/* PXView-compatible driver info struct. */
struct sr_dev_driver serial_lcr_driver_info = {
	.name = "serial-lcr",
	.longname = "Serial LCR meters",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = serial_lcr_compat_init,
	.cleanup = serial_lcr_compat_cleanup,
	.scan = serial_lcr_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = serial_lcr_compat_config_get,
	.config_set = serial_lcr_compat_config_set,
	.config_list = serial_lcr_compat_config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = serial_lcr_compat_acquisition_start,
	.dev_acquisition_stop = serial_lcr_compat_acquisition_stop,
	.priv = NULL,
};
