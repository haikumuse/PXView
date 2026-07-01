/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2017-2021 Frank Stettner <frank-stettner@gmx.net>
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
 * Forward declaration. Must have external linkage (not static) so that
 * hwdriver.c's drivers_list[] entry `&hp_3478a_driver_info` can resolve
 * at link time. The actual definition is at the end of this file.
 * (Matches the hp-3457a/rohde-schwarz-sme-0x pattern; the original sigrok
 * source used `static` here because it relied on SR_REGISTER_DEV_DRIVER
 * section-based registration, which PXView does not use.)
 */
extern struct sr_dev_driver hp_3478a_driver_info;

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
};

static const uint32_t drvopts[] = {
	SR_CONF_MULTIMETER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_MEASURED_QUANTITY | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_RANGE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_DIGITS | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

static const struct {
	enum sr_mq mq;
	enum sr_mqflag mqflag;
} mqopts[] = {
	{SR_MQ_VOLTAGE, SR_MQFLAG_DC},
	{SR_MQ_VOLTAGE, SR_MQFLAG_AC},
	{SR_MQ_CURRENT, SR_MQFLAG_DC},
	{SR_MQ_CURRENT, SR_MQFLAG_AC},
	{SR_MQ_RESISTANCE, 0},
	{SR_MQ_RESISTANCE, SR_MQFLAG_FOUR_WIRE},
};

static const struct {
	enum sr_mq mq;
	enum sr_mqflag mqflag;
	int range_exp;
	const char *range_str;
} rangeopts[] = {
	/* -99 is a dummy exponent for auto ranging. */
	{SR_MQ_VOLTAGE, SR_MQFLAG_DC,             -99,   "Auto"},
	{SR_MQ_VOLTAGE, SR_MQFLAG_DC,              -2,   "30mV"},
	{SR_MQ_VOLTAGE, SR_MQFLAG_DC,              -1,   "300mV"},
	{SR_MQ_VOLTAGE, SR_MQFLAG_DC,               0,   "3V"},
	{SR_MQ_VOLTAGE, SR_MQFLAG_DC,               1,   "30V"},
	{SR_MQ_VOLTAGE, SR_MQFLAG_DC,               2,   "300V"},
	/* -99 is a dummy exponent for auto ranging. */
	{SR_MQ_VOLTAGE, SR_MQFLAG_AC,             -99,   "Auto"},
	{SR_MQ_VOLTAGE, SR_MQFLAG_AC,              -1,   "300mV"},
	{SR_MQ_VOLTAGE, SR_MQFLAG_AC,               0,   "3V"},
	{SR_MQ_VOLTAGE, SR_MQFLAG_AC,               1,   "30V"},
	{SR_MQ_VOLTAGE, SR_MQFLAG_AC,               2,   "300V"},
	/* -99 is a dummy exponent for auto ranging. */
	{SR_MQ_CURRENT, SR_MQFLAG_DC,             -99,   "Auto"},
	{SR_MQ_CURRENT, SR_MQFLAG_DC,              -1,   "300mA"},
	{SR_MQ_CURRENT, SR_MQFLAG_DC,               0,   "3A"},
	/* -99 is a dummy exponent for auto ranging. */
	{SR_MQ_CURRENT, SR_MQFLAG_AC,             -99,   "Auto"},
	{SR_MQ_CURRENT, SR_MQFLAG_AC,              -1,   "300mA"},
	{SR_MQ_CURRENT, SR_MQFLAG_AC,               0,   "3A"},
	/* -99 is a dummy exponent for auto ranging. */
	{SR_MQ_RESISTANCE, 0,                     -99,   "Auto"},
	{SR_MQ_RESISTANCE, 0,                       1,   "30R"},
	{SR_MQ_RESISTANCE, 0,                       2,   "300R"},
	{SR_MQ_RESISTANCE, 0,                       3,   "3kR"},
	{SR_MQ_RESISTANCE, 0,                       4,   "30kR"},
	{SR_MQ_RESISTANCE, 0,                       5,   "300kR"},
	{SR_MQ_RESISTANCE, 0,                       6,   "3MR"},
	{SR_MQ_RESISTANCE, 0,                       7,   "30MR"},
	/* -99 is a dummy exponent for auto ranging. */
	{SR_MQ_RESISTANCE, SR_MQFLAG_FOUR_WIRE,   -99,   "Auto"},
	{SR_MQ_RESISTANCE, SR_MQFLAG_FOUR_WIRE,     1,   "30R"},
	{SR_MQ_RESISTANCE, SR_MQFLAG_FOUR_WIRE,     2,   "300R"},
	{SR_MQ_RESISTANCE, SR_MQFLAG_FOUR_WIRE,     3,   "3kR"},
	{SR_MQ_RESISTANCE, SR_MQFLAG_FOUR_WIRE,     4,   "30kR"},
	{SR_MQ_RESISTANCE, SR_MQFLAG_FOUR_WIRE,     5,   "300kR"},
	{SR_MQ_RESISTANCE, SR_MQFLAG_FOUR_WIRE,     6,   "3MR"},
	{SR_MQ_RESISTANCE, SR_MQFLAG_FOUR_WIRE,     7,   "30MR"},
};

/** Available digits as strings. */
static const char *digits[] = {
	"3.5", "4.5", "5.5",
};

/** Mapping between devc->digits and digits string. */
static const char *digits_map[] = {
	"", "", "", "", "3.5", "4.5", "5.5",
};

static int create_front_channel(struct sr_dev_inst *sdi, int chan_idx)
{
	struct sr_channel *channel;
	struct channel_context *chanc;

	chanc = g_malloc(sizeof(*chanc));
	chanc->location = TERMINAL_FRONT;

	channel = sr_channel_new(sdi, chan_idx++, SR_CHANNEL_ANALOG, TRUE, "P1");
	channel->priv = chanc;

	return chan_idx;
}

static struct sr_dev_inst *probe_device(struct sr_scpi_dev_inst *scpi)
{
	int ret;
	struct sr_dev_inst *sdi;
	struct dev_context *devc;

	/*
	 * The device cannot get identified by means of SCPI queries.
	 * Neither shall non-SCPI requests get emitted before reliable
	 * identification of the device. Assume that we only get here
	 * when user specs led us to believe it's safe to communicate
	 * to the expected kind of device.
	 */

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->vendor = g_strdup("Hewlett-Packard");
	sdi->model = g_strdup("3478A");
	sdi->conn = scpi;
	sdi->driver = &hp_3478a_driver_info;
	sdi->inst_type = SR_INST_SCPI;

	devc = g_malloc0(sizeof(struct dev_context));
	sr_sw_limits_init(&devc->limits);
	sdi->priv = devc;

	/* Get actual status (function, digits, ...). */
	ret = hp_3478a_get_status_bytes(sdi);
	if (ret != SR_OK)
		return NULL;

	create_front_channel(sdi, 0);

	return sdi;
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	GSList *devices;
	const char *conn;
	GSList *l;
	struct sr_config *src;

	/*
	 * Only scan for a device when conn= was specified.
	 *
	 * PXView's libsigrok does not provide sr_serial_extract_options(),
	 * so manually walk the options list looking for SR_CONF_CONN (same
	 * pattern as scpi-pps/api.c, motech-lps-30x/api.c and hp-3457a/api.c).
	 */
	conn = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		if (src->key == SR_CONF_CONN) {
			conn = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	if (!conn)
		return NULL;

	/*
	 * PXView stores the driver private data (struct drv_context *) in
	 * di->priv. The original sigrok used di->context. sr_scpi_scan()
	 * expects a struct drv_context *, so cast di->priv accordingly.
	 */
	devices = sr_scpi_scan((struct drv_context *)di->priv, options,
			probe_device);

	return std_scan_complete_compat(di, devices);
}

static int dev_open(struct sr_dev_inst *sdi)
{
	return sr_scpi_open(sdi->conn);
}

static int dev_close(struct sr_dev_inst *sdi)
{
	return sr_scpi_close(sdi->conn);
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	int ret;
	GVariant *arr[2];
	unsigned int i;
	const char *range_str;

	(void)cg;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
	case SR_CONF_LIMIT_MSEC:
		return sr_sw_limits_config_get(&devc->limits, key, data);
	case SR_CONF_MEASURED_QUANTITY:
		ret = hp_3478a_get_status_bytes(sdi);
		if (ret != SR_OK)
			return ret;
		arr[0] = g_variant_new_uint32(devc->measurement_mq);
		arr[1] = g_variant_new_uint64(devc->measurement_mq_flag);
		*data = g_variant_new_tuple(arr, 2);
		break;
	case SR_CONF_RANGE:
		ret = hp_3478a_get_status_bytes(sdi);
		if (ret != SR_OK)
			return ret;
		range_str = "Auto";
		for (i = 0; i < ARRAY_SIZE(rangeopts); i++) {
			if (rangeopts[i].mq == devc->measurement_mq &&
					rangeopts[i].mqflag == devc->measurement_mq_flag &&
					rangeopts[i].range_exp == devc->range_exp) {
				range_str = rangeopts[i].range_str;
				break;
			}
		}
		*data = g_variant_new_string(range_str);
		break;
	case SR_CONF_DIGITS:
		ret = hp_3478a_get_status_bytes(sdi);
		if (ret != SR_OK)
			return ret;
		*data = g_variant_new_string(digits_map[devc->digits]);
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
	enum sr_mq mq;
	enum sr_mqflag mq_flags;
	GVariant *tuple_child;
	unsigned int i;
	const char *range_str;
	const char *digits_str;

	(void)cg;

	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
	case SR_CONF_LIMIT_MSEC:
		return sr_sw_limits_config_set(&devc->limits, key, data);
	case SR_CONF_MEASURED_QUANTITY:
		tuple_child = g_variant_get_child_value(data, 0);
		mq = g_variant_get_uint32(tuple_child);
		g_variant_unref(tuple_child);
		tuple_child = g_variant_get_child_value(data, 1);
		mq_flags = g_variant_get_uint64(tuple_child);
		g_variant_unref(tuple_child);
		return hp_3478a_set_mq(sdi, mq, mq_flags);
	case SR_CONF_RANGE:
		range_str = g_variant_get_string(data, NULL);
		for (i = 0; i < ARRAY_SIZE(rangeopts); i++) {
			if (rangeopts[i].mq == devc->measurement_mq &&
					rangeopts[i].mqflag == devc->measurement_mq_flag &&
					g_strcmp0(rangeopts[i].range_str, range_str) == 0) {
				return hp_3478a_set_range(sdi, rangeopts[i].range_exp);
			}
		}
		return SR_ERR_NA;
	case SR_CONF_DIGITS:
		digits_str = g_variant_get_string(data, NULL);
		for (i = 0; i < ARRAY_SIZE(rangeopts); i++) {
			if (g_strcmp0(digits_map[i], digits_str) == 0)
				return hp_3478a_set_digits(sdi, i);
		}
		return SR_ERR_NA;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	int ret;
	unsigned int i;
	GVariant *gvar, *arr[2];
	GVariantBuilder gvb;

	/* Only handle standard keys when no device instance is given. */
	if (!sdi)
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
	case SR_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
	case SR_CONF_MEASURED_QUANTITY:
		/*
		 * TODO: move to std.c as
		 *       SR_PRIV GVariant *std_gvar_measured_quantities()
		 */
		g_variant_builder_init(&gvb, G_VARIANT_TYPE_ARRAY);
		for (i = 0; i < ARRAY_SIZE(mqopts); i++) {
			arr[0] = g_variant_new_uint32(mqopts[i].mq);
			arr[1] = g_variant_new_uint64(mqopts[i].mqflag);
			gvar = g_variant_new_tuple(arr, 2);
			g_variant_builder_add_value(&gvb, gvar);
		}
		*data = g_variant_builder_end(&gvb);
		break;
	case SR_CONF_RANGE:
		ret = hp_3478a_get_status_bytes(sdi);
		if (ret != SR_OK)
			return ret;
		g_variant_builder_init(&gvb, G_VARIANT_TYPE_ARRAY);
		for (i = 0; i < ARRAY_SIZE(rangeopts); i++) {
			if (rangeopts[i].mq == devc->measurement_mq &&
					rangeopts[i].mqflag == devc->measurement_mq_flag) {
				g_variant_builder_add(&gvb, "s", rangeopts[i].range_str);
			}
		}
		*data = g_variant_builder_end(&gvb);
		break;
	case SR_CONF_DIGITS:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(digits));
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

/*
 * PXView note: sr_scpi_source_add() is provided by compat_scpi.c and keeps
 * the (struct sr_session *) first parameter (it is NOT sr_session_source_add,
 * which would lose the session parameter per the 5-arg rule). The callback
 * hp_3478a_receive_data now has PXView's const-sdi signature, so the
 * (void *)sdi cast matches the void *cb_data parameter of sr_scpi_source_add.
 */
static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;

	scpi = sdi->conn;
	devc = sdi->priv;

	sr_sw_limits_acquisition_start(&devc->limits);
	std_session_send_df_header(sdi, LOG_PREFIX);

	/*
	 * NOTE: For faster readings, there are some things one can do:
	 *     - Turn off the display: sr_scpi_send(scpi, "D3SIGROK").
	 *     - Set the line frequency to 60Hz via switch (back of the unit).
	 *     - Set to 3.5 digits measurement.
	 */

	/* Set to internal trigger. */
	sr_scpi_send(scpi, "T1");
	/* Get device status. */
	hp_3478a_get_status_bytes(sdi);

	return sr_scpi_source_add(sdi->session, scpi, G_IO_IN, 100,
			hp_3478a_receive_data, (void *)sdi);
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	struct sr_scpi_dev_inst *scpi;

	scpi = sdi->conn;

	sr_scpi_source_remove(sdi->session, scpi);
	std_session_send_df_end(sdi, LOG_PREFIX);

	/* Set to internal trigger. */
	sr_scpi_send(scpi, "T1");
	/* Turn on display. */
	sr_scpi_send(scpi, "D1");

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
 * Note: The original hp-3478a driver does not provide a config_channel_set
 * callback, so the ch parameter in config_set is unused (SR_CONF_* keys are
 * handled entirely by config_set() based on the key and cg).
 * ==========================================================================*/

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *hp_3478a_drv_ptr;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int hp_3478a_compat_init(struct sr_context *sr_ctx)
{
	hp_3478a_drv_ptr = &hp_3478a_driver_info;
	return std_init(hp_3478a_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int hp_3478a_compat_cleanup(void)
{
	/* Clear device instances before tearing down the driver context. */
	std_dev_clear(hp_3478a_drv_ptr);
	return std_cleanup(hp_3478a_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *hp_3478a_compat_scan(GSList *options)
{
	return scan(hp_3478a_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int hp_3478a_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int hp_3478a_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	/*
	 * The original hp-3478a driver does not provide a config_channel_set
	 * callback, so the ch parameter is unused here. SR_CONF_* keys are
	 * handled entirely by config_set() based on the key and cg.
	 */
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int hp_3478a_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int hp_3478a_compat_acquisition_start(struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int hp_3478a_compat_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop((struct sr_dev_inst *)sdi);
}

/* PXView-compatible driver info struct */
struct sr_dev_driver hp_3478a_driver_info = {
	.name = "hp-3478a",
	.longname = "HP 3478A",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = hp_3478a_compat_init,
	.cleanup = hp_3478a_compat_cleanup,
	.scan = hp_3478a_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = hp_3478a_compat_config_get,
	.config_set = hp_3478a_compat_config_set,
	.config_list = hp_3478a_compat_config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = hp_3478a_compat_acquisition_start,
	.dev_acquisition_stop = hp_3478a_compat_acquisition_stop,
	.priv = NULL,
};
