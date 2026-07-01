/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2021 LUMERIIX
 * Copyright (C) 2024 Daniel Anselmi <danselmi@gmx.ch>
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
 * Rule 1: include replacement. The standard sigrok source used
 *   #include <config.h>
 * PXView's compat layer provides a single entry header instead.
 */
#include "hardware/compat/compat.h"
#include <string.h>
#include "protocol.h"

/*
 * Rule 7 / Rule 13: forward declaration. Must have external linkage (not
 * static) so that hwdriver.c's drivers_list[] entry
 * `&bkprecision_1856d_driver_info` can resolve at link time. The actual
 * definition is at the end of this file. The original sigrok source used
 * `static` here because it relied on SR_REGISTER_DEV_DRIVER section-based
 * registration, which PXView does not use.
 */
extern struct sr_dev_driver bkprecision_1856d_driver_info;

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
};

static const uint32_t drvopts[] = {
	SR_CONF_FREQUENCY_COUNTER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_GATE_TIME     | SR_CONF_SET | SR_CONF_GET | SR_CONF_LIST,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_SET,
	SR_CONF_DATA_SOURCE   | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

static struct sr_dev_driver *bkprecision_1856d_drv_ptr;

const uint64_t timebases[][2] = {
	/*miliseconds*/
	{ 10, 1000 },
	{ 100, 1000 },
	/* seconds */
	{ 1, 1 },
	{ 10, 1 },
};

static const char *data_sources[] = {
	"A", "C",
};

/*
 * Rule 11: local std_str_idx helper. PXView's compat layer provides
 * std_str_idx() as a config_get helper with signature
 *   (const struct sr_dev_inst *sdi, uint32_t key, GVariant **data,
 *    const char *const strs[], size_t count)
 * (see compat_helpers.h:349). The standard sigrok version this driver needs
 * takes (data, vals, count) and returns an index, which is what config_set
 * requires. Provide it locally as a static function (same pattern as
 * cem-dt-885x/api.c:36-47).
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

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct dev_context *devc;
	struct sr_dev_inst *sdi;
	GSList *l;
	struct sr_config *src;
	const char *conn, *serialcomm;

	/*
	 * PXView's libsigrok does not provide sr_serial_extract_options(), so
	 * manually walk the options list looking for SR_CONF_CONN and
	 * SR_CONF_SERIALCOMM (same pattern as colead-slm/api.c and
	 * hp-3457a/api.c:183-192).
	 */
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

	/*
	 * Rule 6: sr_dev_inst_new stays at 5-arg (PXView's compat layer
	 * supports the 5-arg form). However the original code built the sdi by
	 * hand via g_malloc0 + field assignment; we keep that pattern since
	 * this driver does not use sr_dev_inst_new() (and 5-arg form here would
	 * require passing inst_type, status, vendor, model, version, which does
	 * not match the original which only set status/vendor/model and set
	 * conn/inst_type/priv manually afterwards).
	 */
	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup("BK Precision");
	sdi->model = g_strdup("bk-1856d");
	devc = g_malloc0(sizeof(struct dev_context));
	sr_sw_limits_init(&(devc->sw_limits));
	sdi->conn = sr_serial_dev_inst_new(conn, serialcomm);
	sdi->inst_type = SR_INST_SERIAL;
	sdi->priv = devc;
	sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "P1");
	devc->sel_input = InputC;
	devc->curr_sel_input = InputC;
	devc->gate_time = 0;

	/*
	 * Rule 4: std_scan_complete -> std_scan_complete_compat.
	 * Rule 5: di->context is now di->priv (handled inside std_scan_complete_compat
	 * via driver->priv).
	 */
	return std_scan_complete_compat(di, g_slist_append(NULL, sdi));
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	if (!(devc = sdi->priv))
		return SR_ERR;

	switch (key) {
	case SR_CONF_GATE_TIME:
		*data = g_variant_new("(tt)",
				timebases[devc->gate_time][0],
				timebases[devc->gate_time][1]);
		break;
	case SR_CONF_LIMIT_SAMPLES:
		sr_sw_limits_config_get(&(devc->sw_limits), key, data);
		break;
	case SR_CONF_DATA_SOURCE:
		if (devc->sel_input == InputA)
			*data = g_variant_new_string(data_sources[0]);
		else
			*data = g_variant_new_string(data_sources[1]);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int idx;
	struct dev_context *devc;

	(void)cg;

	if (!(devc = sdi->priv))
		return SR_ERR;

	switch (key) {
	case SR_CONF_GATE_TIME:
		{
			uint64_t p, q;
			g_variant_get(data, "(tt)", &p, &q);
			if (p == 10 && q == 1000)
				bkprecision_1856d_set_gate_time(devc, 0);
			else if (p == 100 && q == 1000)
				bkprecision_1856d_set_gate_time(devc, 1);
			else if (p == 1 && q == 1)
				bkprecision_1856d_set_gate_time(devc, 2);
			else if (p == 10 && q == 1)
				bkprecision_1856d_set_gate_time(devc, 3);
			else
				return SR_ERR_NA;
		}
		break;
	case SR_CONF_LIMIT_SAMPLES:
		sr_sw_limits_config_set(&(devc->sw_limits), key, data);
		break;
	case SR_CONF_DATA_SOURCE:
		idx = local_std_str_idx(data, ARRAY_AND_SIZE(data_sources));
		if (idx < 0)
			return SR_ERR_ARG;
		bkprecision_1856d_select_input(devc, idx);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static GVariant *build_tuples(const uint64_t (*array)[][2], unsigned int n)
{
	unsigned int i;
	GVariant *rational[2];
	GVariantBuilder gvb;

	g_variant_builder_init(&gvb, G_VARIANT_TYPE_ARRAY);

	for (i = 0; i < n; i++) {
		rational[0] = g_variant_new_uint64((*array)[i][0]);
		rational[1] = g_variant_new_uint64((*array)[i][1]);
		g_variant_builder_add_value(&gvb, g_variant_new_tuple(rational, 2));
	}

	return g_variant_builder_end(&gvb);
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
	case SR_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
	case SR_CONF_GATE_TIME:
		*data = build_tuples(&timebases, ARRAY_SIZE(timebases));
		break;
	case SR_CONF_DATA_SOURCE:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(data_sources));
		break;
	default:
		return SR_ERR_NA;
	}
	return SR_OK;
}

/*
 * Rule 14: serial_source_add is the 5-arg PXView form
 *   (serial, events, timeout, cb, sdi)
 * The original sigrok call was
 *   serial_source_add(sdi->session, serial, G_IO_IN, 100, cb, (void *)sdi)
 * which had a leading session parameter and trailing void *cb_data. Both are
 * dropped here: PXView's sr_session_source_add has no session parameter, and
 * sr_receive_data_callback_t passes the sdi directly as the third arg.
 * Rule 2: std_session_send_df_header is the 2-arg form provided by PXView's
 * compat layer (sdi, prefix).
 */
static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct sr_serial_dev_inst *serial;

	std_session_send_df_header(sdi, LOG_PREFIX);

	serial = sdi->conn;
	serial_source_add(serial, G_IO_IN, 100,
			bkprecision_1856d_receive_data, sdi);

	bkprecision_1856d_init(sdi);

	return SR_OK;
}

/*
 * Local dev_acquisition_stop implementation.
 * PXView does not provide std_serial_dev_acquisition_stop, so we implement
 * the stop sequence locally: remove the serial source, close the serial
 * port, and send the DF_END packet (same pattern as colead-slm/api.c:121-133).
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

/* ===========================================================================
 * PXView compat wrapper layer (Rule 9: 8 compat wrapper functions).
 *
 * PXView's sr_dev_driver callbacks have different signatures from standard
 * sigrok's (int vs uint32_t key, extra ch parameter, cb_data in acquisition
 * start/stop). These thin wrappers adapt the standard sigrok callbacks above
 * to PXView's expected signatures.
 *
 * Rule 10: the original bkprecision-1856d driver does not provide a
 * dev_config_channel_set callback, so the ch parameter in config_set is
 * unused (SR_CONF_* keys are handled entirely by config_set() based on the
 * key and cg).
 * ==========================================================================*/

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int bkprecision_1856d_compat_init(struct sr_context *sr_ctx)
{
	bkprecision_1856d_drv_ptr = &bkprecision_1856d_driver_info;
	return std_init(bkprecision_1856d_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int bkprecision_1856d_compat_cleanup(void)
{
	/* Clear device instances before tearing down the driver context. */
	std_dev_clear(bkprecision_1856d_drv_ptr);
	return std_cleanup(bkprecision_1856d_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *bkprecision_1856d_compat_scan(GSList *options)
{
	return scan(bkprecision_1856d_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int bkprecision_1856d_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int bkprecision_1856d_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	/*
	 * Rule 10: the original bkprecision-1856d driver does not provide a
	 * config_channel_set callback, so the ch parameter is unused here.
	 * SR_CONF_* keys are handled entirely by config_set() based on the
	 * key and cg.
	 */
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int bkprecision_1856d_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int bkprecision_1856d_compat_acquisition_start(struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int bkprecision_1856d_compat_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop(sdi);
}

/*
 * Rule 8: PXView-compatible driver info struct. Fields completed with
 * driver_type=DRIVER_TYPE_HARDWARE, dev_mode_list=compat_dev_mode_list_default,
 * dev_destroy=compat_dev_destroy_default, dev_status_get=compat_dev_status_get_default,
 * priv=NULL. PXView's struct sr_dev_driver does not have dev_list/dev_clear
 * fields (they are not in libsigrok-internal.h:103-138), so they are omitted
 * here (matching hp-3457a and colead-slm).
 *
 * Rule 12: this driver does not internally call sr_config_set() so no
 * sr_config_set_compat() redirection is needed.
 */
struct sr_dev_driver bkprecision_1856d_driver_info = {
	.name = "bkprecision-1856d",
	.longname = "B&K Precision 1856D",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = bkprecision_1856d_compat_init,
	.cleanup = bkprecision_1856d_compat_cleanup,
	.scan = bkprecision_1856d_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = bkprecision_1856d_compat_config_get,
	.config_set = bkprecision_1856d_compat_config_set,
	.config_list = bkprecision_1856d_compat_config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = bkprecision_1856d_compat_acquisition_start,
	.dev_acquisition_stop = bkprecision_1856d_compat_acquisition_stop,
	.priv = NULL,
};
