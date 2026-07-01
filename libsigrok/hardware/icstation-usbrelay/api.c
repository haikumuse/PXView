/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2021-2023 Frank Stettner <frank-stettner@gmx.net>
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

#define SERIALCOMM "9600/8n1"

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_MULTIPLEXER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONN | SR_CONF_GET,
	SR_CONF_ENABLED | SR_CONF_SET, /* Enable/disable all relays at once. */
};

static const uint32_t devopts_cg[] = {
	SR_CONF_ENABLED | SR_CONF_GET | SR_CONF_SET,
};

static const struct ics_usbrelay_profile supported_ics_usbrelay[] = {
	{ ICSE012A, 0xAB, "ICSE012A", 4 },
	{ ICSE013A, 0xAD, "ICSE013A", 2 },
	{ ICSE014A, 0xAC, "ICSE014A", 8 },
};

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
	GSList *devices, *l;
	struct sr_config *src;
	size_t i, ch_idx;
	const char *conn, *serialcomm;
	int ret;
	uint8_t device_id;
	const struct ics_usbrelay_profile *profile;
	struct sr_channel_group *cg;
	struct channel_group_context *cgc;

	devices = NULL;

	/*
	 * PXView's libsigrok does not provide sr_serial_extract_options(),
	 * so manually walk the options GSList looking for SR_CONF_CONN and
	 * SR_CONF_SERIALCOMM (same pattern as conrad-digi-35-cpu, juntek-jds6600,
	 * atorch, hp-59306a). Only scan for a device when conn= was specified.
	 */
	conn = NULL;
	serialcomm = SERIALCOMM;
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

	serial = sr_serial_dev_inst_new(conn, serialcomm);
	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		return NULL;

	/* Get device model. */
	ret = icstation_usbrelay_identify(serial, &device_id);
	if (ret != SR_OK) {
		sr_err("Cannot retrieve identification details.");
		serial_close(serial);
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(supported_ics_usbrelay); i++) {
		profile = &supported_ics_usbrelay[i];
		if (device_id != profile->id)
			continue;
		sdi = g_malloc0(sizeof(*sdi));
		sdi->status = SR_ST_INACTIVE;
		sdi->vendor = g_strdup("ICStation");
		sdi->model = g_strdup(profile->modelname);
		sdi->inst_type = SR_INST_SERIAL;
		sdi->conn = serial;
		sdi->connection_id = g_strdup(conn);

		devc = g_malloc0(sizeof(*devc));
		sdi->priv = devc;
		devc->relay_count = profile->nb_channels;
		devc->relay_mask = (1U << devc->relay_count) - 1;
		/* Assume that all relays are off at the start. */
		devc->relay_state = 0;
		for (ch_idx = 0; ch_idx < devc->relay_count; ch_idx++) {
			cg = g_malloc0(sizeof(*cg));
			cg->name = g_strdup_printf("R%zu", ch_idx + 1);
			cgc = g_malloc0(sizeof(*cgc));
			cg->priv = cgc;
			cgc->index = ch_idx;
			sdi->channel_groups = g_slist_append(sdi->channel_groups, cg);
		}

		devices = g_slist_append(devices, sdi);
		break;
	}

	serial_close(serial);
	if (!devices) {
		sr_serial_dev_inst_free(serial);
		sr_warn("Unknown device identification 0x%02hhx.", device_id);
	}

	return std_scan_complete_compat(di, devices);
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	struct channel_group_context *cgc;
	uint8_t mask;
	gboolean on;

	if (!sdi || !data)
		return SR_ERR_ARG;
	devc = sdi->priv;

	if (!cg) {
		switch (key) {
		case SR_CONF_CONN:
			*data = g_variant_new_string(sdi->connection_id);
			break;
		default:
			return SR_ERR_NA;
		}
	}

	switch (key) {
	case SR_CONF_ENABLED:
		cgc = cg->priv;
		mask = 1U << cgc->index;
		on = devc->relay_state & mask;
		*data = g_variant_new_boolean(on);
		return SR_OK;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	gboolean on;

	if (!cg) {
		switch (key) {
		case SR_CONF_ENABLED:
			/* Enable/disable all channels at the same time. */
			on = g_variant_get_boolean(data);
			return icstation_usbrelay_switch_cg(sdi, cg, on);
		default:
			return SR_ERR_NA;
		}
	} else {
		switch (key) {
		case SR_CONF_ENABLED:
			on = g_variant_get_boolean(data);
			return icstation_usbrelay_switch_cg(sdi, cg, on);
		default:
			return SR_ERR_NA;
		}
	}

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	if (!cg) {
		switch (key) {
		case SR_CONF_SCAN_OPTIONS:
		case SR_CONF_DEVICE_OPTIONS:
			return STD_CONFIG_LIST(key, data, sdi, cg,
				scanopts, drvopts, devopts);
		default:
			return SR_ERR_NA;
		}
	}

	switch (key) {
	case SR_CONF_DEVICE_OPTIONS:
		*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg));
		return SR_OK;
	default:
		return SR_ERR_NA;
	}
}

static int dev_open(struct sr_dev_inst *sdi)
{
	struct sr_serial_dev_inst *serial;
	int ret;

	if (!sdi)
		return SR_ERR_ARG;
	serial = sdi->conn;
	if (!serial)
		return SR_ERR_ARG;

	ret = std_serial_dev_open(sdi);
	if (ret != SR_OK)
		return ret;

	/* Start command mode. */
	ret = icstation_usbrelay_start(sdi);
	if (ret != SR_OK) {
		sr_err("Cannot initiate command mode.");
		serial_close(serial);
		return SR_ERR_IO;
	}

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
 * Note: This driver is a relay/multiplexer device. The original sigrok source
 * used std_dummy_dev_acquisition_start / std_dummy_dev_acquisition_stop
 * (no-ops returning SR_OK) for the acquisition callbacks. Neither exists in
 * the PXView compat layer, so the wrappers below are no-ops that simply
 * return SR_OK, matching the original semantics.
 * ==========================================================================*/

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *icstation_usbrelay_drv_ptr;

/* Forward declaration - defined at end of file */
extern struct sr_dev_driver icstation_usbrelay_driver_info;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int icstation_usbrelay_compat_init(struct sr_context *sr_ctx)
{
	icstation_usbrelay_drv_ptr = &icstation_usbrelay_driver_info;
	return std_init(icstation_usbrelay_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int icstation_usbrelay_compat_cleanup(void)
{
	return std_cleanup(icstation_usbrelay_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *icstation_usbrelay_compat_scan(GSList *options)
{
	return scan(icstation_usbrelay_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int icstation_usbrelay_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int icstation_usbrelay_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int icstation_usbrelay_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/*
 * Wrapper: PXView acquisition_start(sdi, cb_data).
 * Original used std_dummy_dev_acquisition_start (no-op). This driver is a
 * relay actuator and does not perform data acquisition, so this is a no-op.
 */
static int icstation_usbrelay_compat_acquisition_start(struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)sdi;
	(void)cb_data;
	return SR_OK;
}

/*
 * Wrapper: PXView acquisition_stop(const sdi, cb_data).
 * Original used std_dummy_dev_acquisition_stop (no-op). This driver is a
 * relay actuator and does not perform data acquisition, so this is a no-op.
 */
static int icstation_usbrelay_compat_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)sdi;
	(void)cb_data;
	return SR_OK;
}

/* PXView-compatible driver info struct */
struct sr_dev_driver icstation_usbrelay_driver_info = {
	.name = "icstation-usbrelay",
	.longname = "ICStation USBRelay",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = icstation_usbrelay_compat_init,
	.cleanup = icstation_usbrelay_compat_cleanup,
	.scan = icstation_usbrelay_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = icstation_usbrelay_compat_config_get,
	.config_set = icstation_usbrelay_compat_config_set,
	.config_list = icstation_usbrelay_compat_config_list,
	.dev_open = dev_open,
	.dev_close = std_serial_dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = icstation_usbrelay_compat_acquisition_start,
	.dev_acquisition_stop = icstation_usbrelay_compat_acquisition_stop,
	.priv = NULL,
};
