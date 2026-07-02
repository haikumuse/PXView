/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2023 Gerhard Sittig <gerhard.sittig@gmx.net>
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

#define VENDOR_TEXT	"Devantech"

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
};

static const uint32_t drvopts[] = {
	SR_CONF_MULTIPLEXER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONN | SR_CONF_GET,
	SR_CONF_ENABLED | SR_CONF_SET, /* Enable/disable all relays at once. */
};

static const uint32_t devopts_cg_do[] = {
	SR_CONF_ENABLED | SR_CONF_GET | SR_CONF_SET,
};

static const uint32_t devopts_cg_di[] = {
	SR_CONF_ENABLED | SR_CONF_GET,
};

static const uint32_t devopts_cg_ai[] = {
	SR_CONF_VOLTAGE | SR_CONF_GET,
};

/* List of supported devices. Sorted by model ID. */
static const struct devantech_eth008_model models[] = {
	{ 18, "ETH002",    2,  0,  0, 0, 1, 0, 0, },
	{ 19, "ETH008",    8,  0,  0, 0, 1, 0, 0, },
	{ 20, "ETH484",   16,  8,  4, 0, 2, 2, 0x00f0, },
	{ 21, "ETH8020",  20,  8,  8, 0, 3, 4, 0, },
	{ 22, "WIFI484",  16,  8,  4, 0, 2, 2, 0x00f0, },
	{ 24, "WIFI8020", 20,  8,  8, 0, 3, 4, 0, },
	{ 26, "WIFI002",   2,  0,  0, 0, 1, 0, 0, },
	{ 28, "WIFI008",   8,  0,  0, 0, 1, 0, 0, },
	{ 52, "ETH1610",  10, 16, 16, 0, 2, 2, 0, },
};

static const struct devantech_eth008_model *find_model(uint8_t code)
{
	size_t idx;
	const struct devantech_eth008_model *check;

	for (idx = 0; idx < ARRAY_SIZE(models); idx++) {
		check = &models[idx];
		if (check->code != code)
			continue;
		return check;
	}

	return NULL;
}

/* Forward declaration - defined at end of file. */
extern struct sr_dev_driver devantech_eth008_driver_info;

/*
 * Parse a conn= spec into a TCP device instance. Accepts both
 * "tcp:host:port" and "host:port" forms (the "tcp:" prefix is optional).
 * The last ':' separates the host from the port so that IPv6-style
 * addresses like "[::1]:17494" work as well.
 */
static struct sr_tcp_dev_inst *tcp_dev_from_conn(const char *conn)
{
	const char *p, *colon;
	char *host, *port;
	struct sr_tcp_dev_inst *tcp;

	if (!conn || !*conn)
		return NULL;

	p = conn;
	if (strncmp(p, "tcp:", 4) == 0)
		p += 4;

	colon = strrchr(p, ':');
	if (!colon || colon == p)
		return NULL;

	host = g_strndup(p, (size_t)(colon - p));
	port = g_strdup(colon + 1);
	tcp = sr_tcp_dev_inst_new(host, port);
	g_free(host);
	g_free(port);
	return tcp;
}

static struct sr_dev_inst *probe_device_conn(const char *conn)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_tcp_dev_inst *tcp;
	uint8_t code, hwver, fwver;
	const struct devantech_eth008_model *model;
	gboolean has_serno_cmd;
	char snr_txt[16];
	struct channel_group_context *cgc;
	size_t ch_idx, nr, do_idx, di_idx, ai_idx;
	struct sr_channel_group *cg;
	char cg_name[24];
	int ret;

	sdi = g_malloc0(sizeof(*sdi));
	devc = g_malloc0(sizeof(*devc));
	sdi->priv = devc;
	tcp = tcp_dev_from_conn(conn);
	sdi->conn = tcp;
	if (!tcp)
		goto probe_fail;
	ret = sr_tcp_connect(tcp);
	if (ret != SR_OK)
		goto probe_fail;

	ret = devantech_eth008_get_model(tcp, &code, &hwver, &fwver);
	if (ret != SR_OK)
		goto probe_fail;
	model = find_model(code);
	if (!model) {
		sr_err("Unknown model ID 0x%02x (HW %u, FW %u).",
			code, hwver, fwver);
		goto probe_fail;
	}
	devc->model_code = code;
	devc->hardware_version = hwver;
	devc->firmware_version = fwver;
	devc->model = model;
	sdi->vendor = g_strdup(VENDOR_TEXT);
	sdi->model = g_strdup(model->name);
	sdi->version = g_strdup_printf("HW%u FW%u", hwver, fwver);
	sdi->connection_id = g_strdup(conn);
	sdi->driver = &devantech_eth008_driver_info;
	sdi->inst_type = SR_INST_SERIAL;

	has_serno_cmd = TRUE;
	if (model->min_serno_fw && fwver < model->min_serno_fw)
		has_serno_cmd = FALSE;
	if (has_serno_cmd) {
		snr_txt[0] = '\0';
		ret = devantech_eth008_get_serno(tcp,
			snr_txt, sizeof(snr_txt));
		if (ret != SR_OK)
			goto probe_fail;
		sdi->serial_num = g_strdup(snr_txt);
	}

	ch_idx = 0;
	devc->mask_do = (1UL << devc->model->ch_count_do) - 1;
	devc->mask_do &= ~devc->model->mask_do_missing;
	for (do_idx = 0; do_idx < devc->model->ch_count_do; do_idx++) {
		nr = do_idx + 1;
		if (devc->model->mask_do_missing & (1UL << do_idx))
			continue;
		snprintf(cg_name, sizeof(cg_name), "DO%zu", nr);
		cgc = g_malloc0(sizeof(*cgc));
		cg = sr_channel_group_new(sdi, cg_name, cgc);
		cgc->index = do_idx;
		cgc->number = nr;
		cgc->ch_type = DV_CH_DIGITAL_OUTPUT;
		(void)cg;
		ch_idx++;
	}
	for (di_idx = 0; di_idx < devc->model->ch_count_di; di_idx++) {
		nr = di_idx + 1;
		snprintf(cg_name, sizeof(cg_name), "DI%zu", nr);
		cgc = g_malloc0(sizeof(*cgc));
		cg = sr_channel_group_new(sdi, cg_name, cgc);
		cgc->index = di_idx;
		cgc->number = nr;
		cgc->ch_type = DV_CH_DIGITAL_INPUT;
		(void)cg;
		ch_idx++;
	}
	for (ai_idx = 0; ai_idx < devc->model->ch_count_ai; ai_idx++) {
		nr = ai_idx + 1;
		snprintf(cg_name, sizeof(cg_name), "AI%zu", nr);
		cgc = g_malloc0(sizeof(*cgc));
		cg = sr_channel_group_new(sdi, cg_name, cgc);
		cgc->index = ai_idx;
		cgc->number = nr;
		cgc->ch_type = DV_CH_ANALOG_INPUT;
		(void)cg;
		ch_idx++;
	}
	if (1) {
		/* Create an analog channel for the supply voltage. */
		snprintf(cg_name, sizeof(cg_name), "Vsupply");
		cgc = g_malloc0(sizeof(*cgc));
		cg = sr_channel_group_new(sdi, cg_name, cgc);
		cgc->index = 0;
		cgc->number = 0;
		cgc->ch_type = DV_CH_SUPPLY_VOLTAGE;
		(void)cg;
		ch_idx++;
	}

	return sdi;

probe_fail:
	if (tcp) {
		sr_tcp_disconnect(tcp);
		sr_tcp_dev_inst_free(tcp);
		/*
		 * sr_dev_inst_free() below calls safe_free(sdi->conn);
		 * null it here to avoid a double free since we already
		 * freed the tcp struct above.
		 */
		sdi->conn = NULL;
	}
	if (devc) {
		g_free(devc);
	}
	if (sdi) {
		sdi->priv = NULL;
		sr_dev_inst_free(sdi);
		sdi = NULL;
	}
	return sdi;
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct drv_context *drvc;
	const char *conn;
	GSList *devices, *l;
	struct sr_dev_inst *sdi;
	struct sr_config *src;

	drvc = di->priv;
	drvc->instances = NULL;

	/*
	 * A conn= spec is required for the TCP attached device. PXView's
	 * libsigrok does not provide sr_serial_extract_options(), so
	 * manually traverse the options GSList for SR_CONF_CONN. Same
	 * pattern as arachnid-labs-re-load-pro and other compat drivers.
	 */
	conn = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		if (src->key == SR_CONF_CONN) {
			conn = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	if (!conn || !*conn)
		return NULL;

	devices = NULL;
	sdi = probe_device_conn(conn);
	if (sdi)
		devices = g_slist_append(devices, sdi);

	return std_scan_complete_compat(di, devices);
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct channel_group_context *cgc;
	gboolean on;
	uint16_t vin;
	double vsupply;
	int ret;

	if (!cg) {
		switch (key) {
		case SR_CONF_CONN:
			if (!sdi->connection_id)
				return SR_ERR_NA;
			*data = g_variant_new_string(sdi->connection_id);
			return SR_OK;
		default:
			return SR_ERR_NA;
		}
	}

	cgc = cg->priv;
	if (!cgc)
		return SR_ERR_NA;
	switch (key) {
	case SR_CONF_ENABLED:
		if (cgc->ch_type == DV_CH_DIGITAL_OUTPUT) {
			ret = devantech_eth008_query_do(sdi, cg, &on);
			if (ret != SR_OK)
				return ret;
			*data = g_variant_new_boolean(on);
			return SR_OK;
		}
		if (cgc->ch_type == DV_CH_DIGITAL_INPUT) {
			ret = devantech_eth008_query_di(sdi, cg, &on);
			if (ret != SR_OK)
				return ret;
			*data = g_variant_new_boolean(on);
			return SR_OK;
		}
		return SR_ERR_NA;
	case SR_CONF_VOLTAGE:
		if (cgc->ch_type == DV_CH_ANALOG_INPUT) {
			ret = devantech_eth008_query_ai(sdi, cg, &vin);
			if (ret != SR_OK)
				return ret;
			*data = g_variant_new_uint32(vin);
			return SR_OK;
		}
		if (cgc->ch_type == DV_CH_SUPPLY_VOLTAGE) {
			ret = devantech_eth008_query_supply(sdi, cg, &vin);
			if (ret != SR_OK)
				return ret;
			vsupply = vin;
			vsupply /= 1000.;
			*data = g_variant_new_double(vsupply);
			return SR_OK;
		}
		return SR_ERR_NA;
	default:
		return SR_ERR_NA;
	}
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct channel_group_context *cgc;
	gboolean on;

	if (!cg) {
		switch (key) {
		case SR_CONF_ENABLED:
			/* Enable/disable all channels at the same time. */
			on = g_variant_get_boolean(data);
			return devantech_eth008_setup_do(sdi, cg, on);
		default:
			return SR_ERR_NA;
		}
	}

	cgc = cg->priv;
	if (!cgc)
		return SR_ERR_NA;
	switch (key) {
	case SR_CONF_ENABLED:
		if (cgc->ch_type != DV_CH_DIGITAL_OUTPUT)
			return SR_ERR_NA;
		on = g_variant_get_boolean(data);
		return devantech_eth008_setup_do(sdi, cg, on);
	default:
		return SR_ERR_NA;
	}
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct channel_group_context *cgc;

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

	cgc = cg->priv;
	if (!cgc)
		return SR_ERR_NA;
	switch (key) {
	case SR_CONF_DEVICE_OPTIONS:
		if (cgc->ch_type == DV_CH_DIGITAL_OUTPUT) {
			*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg_do));
			return SR_OK;
		}
		if (cgc->ch_type == DV_CH_DIGITAL_INPUT) {
			*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg_di));
			return SR_OK;
		}
		if (cgc->ch_type == DV_CH_ANALOG_INPUT) {
			*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg_ai));
			return SR_OK;
		}
		if (cgc->ch_type == DV_CH_SUPPLY_VOLTAGE) {
			*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg_ai));
			return SR_OK;
		}
		return SR_ERR_NA;
	default:
		return SR_ERR_NA;
	}
}

/*
 * TCP dev_open / dev_close. The original driver used std_serial_dev_open /
 * std_serial_dev_close which operate on a sr_serial_dev_inst. Since this
 * migration moved the transport to raw TCP (sr_tcp_dev_inst), provide
 * custom open/close that call sr_tcp_connect / sr_tcp_disconnect.
 */
static int dev_open(struct sr_dev_inst *sdi)
{
	struct sr_tcp_dev_inst *tcp;

	if (!sdi || !sdi->conn)
		return SR_ERR_ARG;
	tcp = sdi->conn;
	return sr_tcp_connect(tcp);
}

static int dev_close(struct sr_dev_inst *sdi)
{
	struct sr_tcp_dev_inst *tcp;

	if (!sdi || !sdi->conn)
		return SR_ERR_ARG;
	tcp = sdi->conn;
	return sr_tcp_disconnect(tcp);
}

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *devantech_eth008_drv_ptr;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int devantech_eth008_compat_init(struct sr_context *sr_ctx)
{
	devantech_eth008_drv_ptr = &devantech_eth008_driver_info;
	return std_init(devantech_eth008_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int devantech_eth008_compat_cleanup(void)
{
	return std_cleanup(devantech_eth008_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *devantech_eth008_compat_scan(GSList *options)
{
	return scan(devantech_eth008_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int devantech_eth008_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int devantech_eth008_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int devantech_eth008_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/*
 * Wrapper: PXView acquisition_start(sdi, cb_data).
 * The original driver used std_dummy_dev_acquisition_start (no-op). This is
 * a relay/IO controller that does not perform data acquisition, so this is
 * a no-op returning SR_OK. PXView's compat layer does not provide
 * std_dummy_dev_acquisition_start, hence the local no-op.
 */
static int devantech_eth008_compat_acquisition_start(struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)sdi;
	(void)cb_data;
	return SR_OK;
}

/*
 * Wrapper: PXView acquisition_stop(const sdi, cb_data).
 * The original driver used std_dummy_dev_acquisition_stop. No-op for the
 * same reason as acquisition_start above.
 */
static int devantech_eth008_compat_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)sdi;
	(void)cb_data;
	return SR_OK;
}

/* PXView-compatible driver info struct */
struct sr_dev_driver devantech_eth008_driver_info = {
	.name = "devantech-eth008",
	.longname = "Devantech ETH008",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = devantech_eth008_compat_init,
	.cleanup = devantech_eth008_compat_cleanup,
	.scan = devantech_eth008_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = devantech_eth008_compat_config_get,
	.config_set = devantech_eth008_compat_config_set,
	.config_list = devantech_eth008_compat_config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = devantech_eth008_compat_acquisition_start,
	.dev_acquisition_stop = devantech_eth008_compat_acquisition_stop,
	.priv = NULL,
};
