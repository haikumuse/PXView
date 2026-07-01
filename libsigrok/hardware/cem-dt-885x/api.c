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

#define SERIALCOMM "9600/8n1"

/* 23ms is the longest interval between tokens. */
#define MAX_SCAN_TIME_US (25 * 1000)

/*
 * Local helper functions for std_str_idx / std_u64_tuple_idx.
 * The compat layer's versions of these helpers have a different signature
 * (they are config_get helpers with (sdi, key, data, ...) parameters).
 * The standard sigrok versions take (data, vals, count) and return an index,
 * which is what this driver needs in config_set.
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

static int local_std_u64_tuple_idx(GVariant *data, const uint64_t vals[][2], size_t count)
{
	uint64_t v0, v1;
	size_t i;

	g_variant_get(data, "(tt)", &v0, &v1);
	for (i = 0; i < count; i++) {
		if (vals[i][0] == v0 && vals[i][1] == v1)
			return (int)i;
	}
	return -1;
}

/*
 * Local helper to create a GVariant "a(tt)" array from an array of uint64
 * pairs. The compat layer's std_gvar_tuple_array() only handles string
 * arrays, so we provide this local version for the measurement ranges table.
 */
static GVariant *local_std_gvar_tuple_array_u64(const uint64_t vals[][2], size_t count)
{
	GVariantBuilder gvb;
	size_t i;

	g_variant_builder_init(&gvb, G_VARIANT_TYPE("a(tt)"));
	for (i = 0; i < count; i++)
		g_variant_builder_add(&gvb, "(tt)", vals[i][0], vals[i][1]);

	return g_variant_builder_end(&gvb);
}

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
};

static const uint32_t drvopts[] = {
	SR_CONF_SOUNDLEVELMETER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_SPL_WEIGHT_FREQ | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_SPL_WEIGHT_TIME | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_SPL_MEASUREMENT_RANGE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_DATALOG | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_HOLD_MAX | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_HOLD_MIN | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_POWER_OFF | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_DATA_SOURCE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

static const char *weight_freq[] = {
	"A",
	"C",
};

static const char *weight_time[] = {
	"F",
	"S",
};

static const uint64_t meas_ranges[][2] = {
	{ 30, 130 },
	{ 30, 80 },
	{ 50, 100 },
	{ 80, 130 },
};

static const char *data_sources[] = {
	"Live",
	"Memory",
};

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct dev_context *devc;
	struct sr_config *src;
	struct sr_serial_dev_inst *serial;
	struct sr_dev_inst *sdi;
	GSList *l, *devices;
	gint64 start;
	const char *conn;
	unsigned char c;

	/*
	 * PXView's libsigrok does not provide sr_serial_extract_options().
	 * Manually walk the options GSList to find SR_CONF_CONN (the only
	 * scanopt this driver declares). SERIALCOMM is hardcoded below.
	 */
	conn = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		if (src->key == SR_CONF_CONN)
			conn = g_variant_get_string(src->data, NULL);
	}
	if (!conn)
		return NULL;

	serial = sr_serial_dev_inst_new(conn, SERIALCOMM);

	if (serial_open(serial, SERIAL_RDONLY) != SR_OK)
		return NULL;

	devices = NULL;
	start = g_get_monotonic_time();
	while (g_get_monotonic_time() - start < MAX_SCAN_TIME_US) {
		if (serial_read_nonblocking(serial, &c, 1) == 1 && c == 0xa5) {
			/* Found one. */
			sdi = g_malloc0(sizeof(struct sr_dev_inst));
			sdi->status = SR_ST_INACTIVE;
			sdi->vendor = g_strdup("CEM");
			sdi->model = g_strdup("DT-885x");
			devc = g_malloc0(sizeof(struct dev_context));
			devc->cur_mqflags = 0;
			devc->recording = -1;
			devc->cur_meas_range = 0;
			devc->cur_data_source = DATA_SOURCE_LIVE;
			devc->enable_data_source_memory = FALSE;
			sdi->conn = sr_serial_dev_inst_new(conn, SERIALCOMM);
			sdi->inst_type = SR_INST_SERIAL;
			sdi->priv = devc;
			sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "SPL");
			devices = g_slist_append(devices, sdi);
			break;
		}
		/* It takes about 1ms for a byte to come in. */
		g_usleep(1000);
	}

	serial_close(serial);

	return std_scan_complete_compat(di, devices);
}

static int config_get(uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	uint64_t low, high;
	int tmp, ret;

	(void)cg;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;

	ret = SR_OK;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
		break;
	case SR_CONF_DATALOG:
		if ((ret = cem_dt_885x_recording_get(sdi, &tmp)) == SR_OK)
			*data = g_variant_new_boolean(tmp);
		break;
	case SR_CONF_SPL_WEIGHT_FREQ:
		tmp = cem_dt_885x_weight_freq_get(sdi);
		if (tmp == SR_MQFLAG_SPL_FREQ_WEIGHT_A)
			*data = g_variant_new_string("A");
		else if (tmp == SR_MQFLAG_SPL_FREQ_WEIGHT_C)
			*data = g_variant_new_string("C");
		else
			return SR_ERR;
		break;
	case SR_CONF_SPL_WEIGHT_TIME:
		tmp = cem_dt_885x_weight_time_get(sdi);
		if (tmp == SR_MQFLAG_SPL_TIME_WEIGHT_F)
			*data = g_variant_new_string("F");
		else if (tmp == SR_MQFLAG_SPL_TIME_WEIGHT_S)
			*data = g_variant_new_string("S");
		else
			return SR_ERR;
		break;
	case SR_CONF_HOLD_MAX:
		if ((ret = cem_dt_885x_holdmode_get(sdi, &tmp)) == SR_OK)
			*data = g_variant_new_boolean(tmp == SR_MQFLAG_MAX);
		break;
	case SR_CONF_HOLD_MIN:
		if ((ret = cem_dt_885x_holdmode_get(sdi, &tmp)) == SR_OK)
			*data = g_variant_new_boolean(tmp == SR_MQFLAG_MIN);
		break;
	case SR_CONF_SPL_MEASUREMENT_RANGE:
		if ((ret = cem_dt_885x_meas_range_get(sdi, &low, &high)) == SR_OK)
			*data = std_gvar_tuple_u64(low, high);
		break;
	case SR_CONF_POWER_OFF:
		*data = g_variant_new_boolean(FALSE);
		break;
	case SR_CONF_DATA_SOURCE:
		if (devc->cur_data_source == DATA_SOURCE_LIVE)
			*data = g_variant_new_string("Live");
		else
			*data = g_variant_new_string("Memory");
		break;
	default:
		return SR_ERR_NA;
	}

	return ret;
}

static int config_set(uint32_t key, GVariant *data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	int tmp, idx;

	(void)cg;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
		devc->limit_samples = g_variant_get_uint64(data);
		break;
	case SR_CONF_DATALOG:
		return cem_dt_885x_recording_set(sdi, g_variant_get_boolean(data));
	case SR_CONF_SPL_WEIGHT_FREQ:
		if ((idx = local_std_str_idx(data, ARRAY_AND_SIZE(weight_freq))) < 0)
			return SR_ERR_ARG;
		return cem_dt_885x_weight_freq_set(sdi, (weight_freq[idx][0] == 'A') ?
			SR_MQFLAG_SPL_FREQ_WEIGHT_A : SR_MQFLAG_SPL_FREQ_WEIGHT_C);
	case SR_CONF_SPL_WEIGHT_TIME:
		if ((idx = local_std_str_idx(data, ARRAY_AND_SIZE(weight_time))) < 0)
			return SR_ERR_ARG;
		return cem_dt_885x_weight_time_set(sdi, (weight_time[idx][0] == 'F') ?
			SR_MQFLAG_SPL_TIME_WEIGHT_F : SR_MQFLAG_SPL_TIME_WEIGHT_S);
	case SR_CONF_HOLD_MAX:
		tmp = g_variant_get_boolean(data) ? SR_MQFLAG_MAX : 0;
		return cem_dt_885x_holdmode_set(sdi, tmp);
	case SR_CONF_HOLD_MIN:
		tmp = g_variant_get_boolean(data) ? SR_MQFLAG_MIN : 0;
		return cem_dt_885x_holdmode_set(sdi, tmp);
	case SR_CONF_SPL_MEASUREMENT_RANGE:
		if ((idx = local_std_u64_tuple_idx(data, ARRAY_AND_SIZE(meas_ranges))) < 0)
			return SR_ERR_ARG;
		return cem_dt_885x_meas_range_set(sdi, meas_ranges[idx][0], meas_ranges[idx][1]);
	case SR_CONF_POWER_OFF:
		if (g_variant_get_boolean(data))
			return cem_dt_885x_power_off(sdi);
		break;
	case SR_CONF_DATA_SOURCE:
		if ((idx = local_std_str_idx(data, ARRAY_AND_SIZE(data_sources))) < 0)
			return SR_ERR_ARG;
		devc->cur_data_source = idx;
		devc->enable_data_source_memory = (idx == DATA_SOURCE_MEMORY);
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
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
	case SR_CONF_SPL_WEIGHT_FREQ:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(weight_freq));
		break;
	case SR_CONF_SPL_WEIGHT_TIME:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(weight_time));
		break;
	case SR_CONF_SPL_MEASUREMENT_RANGE:
		*data = local_std_gvar_tuple_array_u64(ARRAY_AND_SIZE(meas_ranges));
		break;
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

	devc = sdi->priv;
	devc->state = ST_INIT;
	devc->num_samples = 0;
	devc->buf_len = 0;

	std_session_send_df_header(sdi, LOG_PREFIX);

	serial = sdi->conn;
	/*
	 * PXView's serial_source_add() is 5-arg: it drops the session
	 * parameter that standard sigrok's version takes as its first arg.
	 * The callback signature is also different: the sdi is passed
	 * directly as the third argument instead of void *cb_data.
	 */
	serial_source_add(serial, G_IO_IN, 150,
			cem_dt_885x_receive_data, sdi);

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

/* ===========================================================================
 * PXView compat wrapper layer
 *
 * PXView's sr_dev_driver callbacks have different signatures from standard
 * sigrok's (int vs uint32_t key, extra ch parameter, cb_data in acquisition
 * start/stop, no dev_list/dev_clear fields). These thin wrappers adapt the
 * standard sigrok callbacks above to PXView's expected signatures.
 *
 * The original driver had no per-channel config_set logic (it used cg only
 * as a placeholder), so the ch parameter is dropped by the config_set
 * wrapper. config_get and config_list similarly ignore ch.
 * ==========================================================================*/

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *cem_dt_885x_drv_ptr;

/* Forward declaration - defined at end of file */
extern SR_PRIV struct sr_dev_driver cem_dt_885x_driver_info;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int cem_dt_885x_compat_init(struct sr_context *sr_ctx)
{
	cem_dt_885x_drv_ptr = &cem_dt_885x_driver_info;
	return std_init(cem_dt_885x_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int cem_dt_885x_compat_cleanup(void)
{
	return std_cleanup(cem_dt_885x_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *cem_dt_885x_compat_scan(GSList *options)
{
	return scan(cem_dt_885x_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg).
 * No per-channel logic in the original driver, so ch is dropped. */
static int cem_dt_885x_compat_config_get(int id, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel *ch,
		const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg).
 * No per-channel config_set logic in the original driver, so ch is dropped. */
static int cem_dt_885x_compat_config_set(int id, GVariant *data,
		struct sr_dev_inst *sdi, struct sr_channel *ch,
		struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int cem_dt_885x_compat_config_list(int info_id, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int cem_dt_885x_compat_acquisition_start(struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int cem_dt_885x_compat_acquisition_stop(const struct sr_dev_inst *sdi,
		void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop(sdi);
}

/* PXView-compatible driver info struct */
SR_PRIV struct sr_dev_driver cem_dt_885x_driver_info = {
	.name = "cem-dt-885x",
	.longname = "CEM DT-885x",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = cem_dt_885x_compat_init,
	.cleanup = cem_dt_885x_compat_cleanup,
	.scan = cem_dt_885x_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = cem_dt_885x_compat_config_get,
	.config_set = cem_dt_885x_compat_config_set,
	.config_list = cem_dt_885x_compat_config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = cem_dt_885x_compat_acquisition_start,
	.dev_acquisition_stop = cem_dt_885x_compat_acquisition_stop,
	.priv = NULL,
};
