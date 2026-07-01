/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2015 Daniel Glöckner <daniel-gl@gmx.net>
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
#include <ieee1284.h>
#include <string.h>
#include "protocol.h"

/*
 * Local helper functions for std_str_idx / std_u64_idx / std_u64_tuple_idx /
 * std_u8_idx_s / std_gvar_tuple_array.
 *
 * The compat layer's versions of std_str_idx / std_u64_idx have a different
 * signature (they are config_get helpers with (sdi, key, data, ...) parameters),
 * and std_u64_tuple_idx / std_u8_idx_s / std_gvar_tuple_array (uint64 tuple
 * variant) are not provided at all. The standard sigrok versions take
 * (data, vals, count) and return an index, which is what this driver needs in
 * config_set and config_get.
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

static int local_std_u64_idx(GVariant *data, const uint64_t vals[], size_t count)
{
	uint64_t val;
	size_t i;

	val = g_variant_get_uint64(data);
	for (i = 0; i < count; i++) {
		if (vals[i] == val)
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

static int local_std_u8_idx_s(uint8_t val, const uint8_t vals[], size_t count)
{
	size_t i;

	for (i = 0; i < count; i++) {
		if (vals[i] == val)
			return (int)i;
	}
	return -1;
}

/*
 * Local helper to create a GVariant "a(tt)" array from an array of uint64
 * pairs. The compat layer's std_gvar_tuple_array() only handles string
 * arrays, so we provide this local version for the vdivs table.
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
	SR_CONF_OSCILLOSCOPE,
};

static const uint32_t devopts[] = {
	SR_CONF_CONN | SR_CONF_GET,
	SR_CONF_LIMIT_FRAMES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_SAMPLERATE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_TRIGGER_SOURCE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_TRIGGER_SLOPE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_BUFFERSIZE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

static const uint32_t devopts_cg[] = {
	SR_CONF_VDIV | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_COUPLING | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_PROBE_FACTOR | SR_CONF_GET | SR_CONF_SET,
};

static const uint64_t samplerates[] = {
	SR_MHZ(100), SR_MHZ(50),  SR_MHZ(25),   SR_MHZ(20),
	SR_MHZ(10),  SR_MHZ(5),   SR_KHZ(2500), SR_MHZ(2),
	SR_MHZ(1),   SR_KHZ(500), SR_KHZ(250),  SR_KHZ(200),
	SR_KHZ(100), SR_KHZ(50),  SR_KHZ(25),   SR_KHZ(20),
	SR_KHZ(10),  SR_KHZ(5),   SR_HZ(2500),  SR_KHZ(2),
	SR_KHZ(1),   SR_HZ(500),  SR_HZ(250),   SR_HZ(200),
	SR_HZ(100),  SR_HZ(50),   SR_HZ(25),    SR_HZ(20)
};

/* must be in sync with readout_steps[] in protocol.c */
static const uint64_t buffersizes[] = {
	2 * 500, 3 * 500, 4 * 500, 5 * 500,
	6 * 500, 7 * 500, 8 * 500, 9 * 500, 10 * 500,
	12 * 500 - 2, 14 * 500 - 2, 16 * 500 - 2,
	18 * 500 - 2, 20 * 500 - 2, 10240 - 2
};

static const uint64_t vdivs[][2] = {
	{ 10, 1000 },
	{ 20, 1000 },
	{ 50, 1000 },
	{ 100, 1000 },
	{ 200, 1000 },
	{ 500, 1000 },
	{ 1, 1 },
	{ 2, 1 },
	{ 5, 1 },
};

/* Bits 4 and 5 enable relays that add /10 filters to the chain
 * Bits 0 and 1 select an output from a resistor array */
static const uint8_t vdivs_map[] = {
	0x01, 0x02, 0x03, 0x21, 0x22, 0x23, 0x31, 0x32, 0x33
};


static const char *trigger_sources[] = {
	"A", "B", "EXT"
};

static const uint8_t trigger_sources_map[] = {
	0x00, 0x80, 0x40
};

static const char *trigger_slopes[] = {
	"f", "r"
};

static const char *coupling[] = {
	"DC", "AC", "GND"
};

static const uint8_t coupling_map[] = {
	0x00, 0x08, 0x04
};

static GSList *scan_port(GSList *devices, struct parport *port)
{
	struct sr_dev_inst *sdi;
	struct sr_channel *ch;
	struct sr_channel_group *cg;
	struct dev_context *devc;
	int i;

	if (ieee1284_open(port, 0, &i) != E1284_OK) {
		sr_err("Can't open parallel port %s", port->name);
		goto fail1;
	}

	if ((i & (CAP1284_RAW | CAP1284_BYTE)) != (CAP1284_RAW | CAP1284_BYTE)) {
		sr_err("Parallel port %s does not provide low-level bidirection access",
		       port->name);
		goto fail2;
	}

	if (ieee1284_claim(port) != E1284_OK) {
		sr_err("Parallel port %s already in use", port->name);
		goto fail2;
	}

	if (!hung_chang_dso_2100_check_id(port))
		goto fail3;

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup("Hung-Chang");
	sdi->model = g_strdup("DSO-2100");
	sdi->inst_type = 0; /* FIXME */
	sdi->conn = port;
	ieee1284_ref(port);

	for (i = 0; i < NUM_CHANNELS; i++) {
		cg = sr_channel_group_new(sdi, trigger_sources[i], NULL);
		ch = sr_channel_new(sdi, i, SR_CHANNEL_ANALOG, FALSE, trigger_sources[i]);
		cg->channels = g_slist_append(cg->channels, ch);
	}

	devc = g_malloc0(sizeof(struct dev_context));
	devc->enabled_channel = g_slist_append(NULL, NULL);
	devc->channel = 0;
	devc->rate = 0;
	devc->probe[0] = 10;
	devc->probe[1] = 10;
	devc->cctl[0] = 0x31; /* 1V/div, DC coupling, trigger on channel A*/
	devc->cctl[1] = 0x31; /* 1V/div, DC coupling, no tv sync trigger */
	devc->edge = 0;
	devc->tlevel = 0x80;
	devc->pos[0] = 0x80;
	devc->pos[1] = 0x80;
	devc->offset[0] = 0x80;
	devc->offset[1] = 0x80;
	devc->gain[0] = 0x80;
	devc->gain[1] = 0x80;
	devc->frame_limit = 0;
	devc->last_step = 0; /* buffersize = 1000 */
	sdi->priv = devc;

	devices = g_slist_append(devices, sdi);

fail3:
	ieee1284_release(port);
fail2:
	ieee1284_close(port);
fail1:
	return devices;
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct parport_list ports;
	struct sr_config *src;
	const char *conn = NULL;
	GSList *devices, *option;
	gboolean port_found;
	int i;


	for (option = options; option; option = option->next) {
		src = option->data;
		if (src->key == SR_CONF_CONN) {
			conn = g_variant_get_string(src->data, NULL);
			break;
		}
	}

	if (!conn)
		return NULL;

	if (ieee1284_find_ports(&ports, 0) != E1284_OK)
		return NULL;

	devices = NULL;
	port_found = FALSE;
	for (i = 0; i < ports.portc; i++)
		if (!strcmp(ports.portv[i]->name, conn)) {
			port_found = TRUE;
			devices = scan_port(devices, ports.portv[i]);
		}

	if (!port_found) {
		sr_err("Parallel port %s not found. Valid names are:", conn);
		for (i = 0; i < ports.portc; i++)
			sr_err("\t%s", ports.portv[i]->name);
	}

	ieee1284_free_ports(&ports);

	return std_scan_complete_compat(di, devices);
}

static void clear_helper(struct dev_context *devc)
{
	g_slist_free(devc->enabled_channel);
}

static int dev_clear(const struct sr_dev_driver *di)
{
	struct compat_drv_context *drvc;
	struct sr_dev_inst *sdi;
	GSList *l;

	if (!di || !di->priv)
		return SR_ERR_ARG;

	drvc = di->priv;
	for (l = drvc->instances; l; l = l->next) {
		sdi = l->data;
		ieee1284_unref(sdi->conn);
		if (sdi->priv)
			clear_helper(sdi->priv);
	}

	return std_dev_clear_compat(di);
}

static int dev_open(struct sr_dev_inst *sdi)
{
	struct dev_context *devc = sdi->priv;
	int i;

	if (ieee1284_open(sdi->conn, 0, &i) != E1284_OK)
		goto fail1;

	if (ieee1284_claim(sdi->conn) != E1284_OK)
		goto fail2;

	if (ieee1284_data_dir(sdi->conn, 1) != E1284_OK)
		goto fail3;

	if (hung_chang_dso_2100_move_to(sdi, 1))
		goto fail3;

	devc->samples = g_try_malloc(1000 * sizeof(*devc->samples));
	if (!devc->samples)
		goto fail3;

	return SR_OK;

fail3:
	hung_chang_dso_2100_reset_port(sdi->conn);
	ieee1284_release(sdi->conn);
fail2:
	ieee1284_close(sdi->conn);
fail1:
	return SR_ERR;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	struct dev_context *devc = sdi->priv;

	g_free(devc->samples);
	hung_chang_dso_2100_reset_port(sdi->conn);
	ieee1284_release(sdi->conn);
	ieee1284_close(sdi->conn);

	return SR_OK;
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc = sdi->priv;
	struct parport *port;
	int idx, ch = -1;

	if (cg) /* sr_config_get will validate cg using config_list */
		ch = ((struct sr_channel *)cg->channels->data)->index;

	switch (key) {
	case SR_CONF_CONN:
		port = sdi->conn;
		*data = g_variant_new_string(port->name);
		break;
	case SR_CONF_LIMIT_FRAMES:
		*data = g_variant_new_uint64(devc->frame_limit);
		break;
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(samplerates[devc->rate]);
		break;
	case SR_CONF_TRIGGER_SOURCE:
		if ((idx = local_std_u8_idx_s(devc->cctl[0] & 0xC0, ARRAY_AND_SIZE(trigger_sources_map))) < 0)
			return SR_ERR_BUG;
		*data = g_variant_new_string(trigger_sources[idx]);
		break;
	case SR_CONF_TRIGGER_SLOPE:
		if (devc->edge >= ARRAY_SIZE(trigger_slopes))
			return SR_ERR;
		*data = g_variant_new_string(trigger_slopes[devc->edge]);
		break;
	case SR_CONF_BUFFERSIZE:
		*data = g_variant_new_uint64(buffersizes[devc->last_step]);
		break;
	case SR_CONF_VDIV:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if ((idx = local_std_u8_idx_s(devc->cctl[ch] & 0x33, ARRAY_AND_SIZE(vdivs_map))) < 0)
			return SR_ERR_BUG;
		*data = g_variant_new("(tt)", vdivs[idx][0], vdivs[idx][1]);
		break;
	case SR_CONF_COUPLING:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if ((idx = local_std_u8_idx_s(devc->cctl[ch] & 0x0C, ARRAY_AND_SIZE(coupling_map))) < 0)
			return SR_ERR_BUG;
		*data = g_variant_new_string(coupling[idx]);
		break;
	case SR_CONF_PROBE_FACTOR:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		*data = g_variant_new_uint64(devc->probe[ch]);
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
	int idx, ch = -1;
	uint64_t u;

	/* Merged from old config_channel_set(): PXView's compat layer has no
	 * config_channel_set callback, so channel enable/disable must be handled
	 * here. devc->channel is a bitmask (one bit per channel); exclusivity
	 * enforces a single enabled channel, so devc->channel is a power of two
	 * and (devc->channel - 1) yields the channel index used in
	 * dev_acquisition_start(). */
	if (cg) {
		struct sr_channel *chan = cg->channels->data;
		if (chan->enabled) {
			uint8_t v = devc->channel | (1 << chan->index);
			if (v & (v - 1))
				return SR_ERR;
			devc->channel = v;
			devc->enabled_channel->data = chan;
		} else {
			devc->channel &= ~(1 << chan->index);
		}
	}

	if (cg) /* sr_config_set will validate cg using config_list */
		ch = ((struct sr_channel *)cg->channels->data)->index;

	switch (key) {
	case SR_CONF_LIMIT_FRAMES:
		devc->frame_limit = g_variant_get_uint64(data);
		break;
	case SR_CONF_SAMPLERATE:
		if ((idx = local_std_u64_idx(data, ARRAY_AND_SIZE(samplerates))) < 0)
			return SR_ERR_ARG;
		devc->rate = idx;
		break;
	case SR_CONF_TRIGGER_SOURCE:
		if ((idx = local_std_str_idx(data, ARRAY_AND_SIZE(trigger_sources))) < 0)
			return SR_ERR_ARG;
		devc->cctl[0] = (devc->cctl[0] & 0x3F) | trigger_sources_map[idx];
		break;
	case SR_CONF_TRIGGER_SLOPE:
		if ((idx = local_std_str_idx(data, ARRAY_AND_SIZE(trigger_slopes))) < 0)
			return SR_ERR_ARG;
		devc->edge = idx;
		break;
	case SR_CONF_BUFFERSIZE:
		if ((idx = local_std_u64_idx(data, ARRAY_AND_SIZE(buffersizes))) < 0)
			return SR_ERR_ARG;
		devc->last_step = idx;
		break;
	case SR_CONF_VDIV:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if (!g_variant_is_of_type(data, G_VARIANT_TYPE("(tt)")))
			return SR_ERR_ARG;
		if ((idx = local_std_u64_tuple_idx(data, ARRAY_AND_SIZE(vdivs))) < 0)
			return SR_ERR_ARG;
		devc->cctl[ch] = (devc->cctl[ch] & 0xCC) | vdivs_map[idx];
		break;
	case SR_CONF_COUPLING:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if ((idx = local_std_str_idx(data, ARRAY_AND_SIZE(coupling))) < 0)
			return SR_ERR_ARG;
		devc->cctl[ch] = (devc->cctl[ch] & 0xF3) | coupling_map[idx];
		break;
	case SR_CONF_PROBE_FACTOR:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		u = g_variant_get_uint64(data);
		if (!u)
			return SR_ERR_ARG;
		devc->probe[ch] = u;
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	GSList *l;

	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
	case SR_CONF_DEVICE_OPTIONS:
		break;
	case SR_CONF_SAMPLERATE:
	case SR_CONF_TRIGGER_SOURCE:
	case SR_CONF_TRIGGER_SLOPE:
	case SR_CONF_BUFFERSIZE:
		if (!sdi || cg)
			return SR_ERR_NA;
		break;
	case SR_CONF_VDIV:
	case SR_CONF_COUPLING:
		if (!sdi)
			return SR_ERR_NA;
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		l = g_slist_find(sdi->channel_groups, cg);
		if (!l)
			return SR_ERR_ARG;
		break;
	default:
		return SR_ERR_NA;
	}

	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
		return std_config_list(key, data, sdi, cg, scanopts, ARRAY_SIZE(scanopts), NULL, 0, NULL, 0);
	case SR_CONF_DEVICE_OPTIONS:
		if (!cg)
			return std_config_list(key, data, sdi, cg, NULL, 0, drvopts, ARRAY_SIZE(drvopts), devopts, ARRAY_SIZE(devopts));
		*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg));
		break;
	case SR_CONF_SAMPLERATE:
		*data = std_gvar_samplerates(ARRAY_AND_SIZE(samplerates));
		break;
	case SR_CONF_TRIGGER_SOURCE:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(trigger_sources));
		break;
	case SR_CONF_TRIGGER_SLOPE:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(trigger_slopes));
		break;
	case SR_CONF_BUFFERSIZE:
		*data = std_gvar_array_u64(ARRAY_AND_SIZE(buffersizes));
		break;
	case SR_CONF_VDIV:
		*data = local_std_gvar_tuple_array_u64(ARRAY_AND_SIZE(vdivs));
		break;
	case SR_CONF_COUPLING:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(coupling));
		break;
	}

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc = sdi->priv;
	int ret;

	if (devc->channel) {
		static const float res_array[] = {0.5, 1, 2, 5};
		static const uint8_t relays[] = {100, 10, 10, 1};
		devc->factor = devc->probe[devc->channel - 1] / 32.0;
		devc->factor *= res_array[devc->cctl[devc->channel - 1] & 0x03];
		devc->factor /= relays[(devc->cctl[devc->channel - 1] >> 4) & 0x03];
	}
	devc->frame = 0;
	devc->state_known = TRUE;
	devc->step = 0;
	devc->adc2 = FALSE;
	devc->retries = MAX_RETRIES;

	ret = hung_chang_dso_2100_move_to(sdi, 0x21);
	if (ret != SR_OK)
		return ret;

	std_session_send_df_header(sdi, LOG_PREFIX);

	sr_session_source_add(-1, 0, 8, hung_chang_dso_2100_poll, sdi);

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	std_session_send_df_end(sdi, LOG_PREFIX);
	sr_session_source_remove(-1);
	hung_chang_dso_2100_move_to(sdi, 1);

	return SR_OK;
}

/* Static driver pointer for wrapper functions */
static struct sr_dev_driver *hung_chang_dso_2100_drv_ptr;

/* Forward declaration - defined at end of file */
extern struct sr_dev_driver hung_chang_dso_2100_driver_info;

/* Wrapper: PXView init(sr_ctx) -> standard init(driver, sr_ctx) */
static int hung_chang_dso_2100_compat_init(struct sr_context *sr_ctx)
{
	hung_chang_dso_2100_drv_ptr = &hung_chang_dso_2100_driver_info;
	return std_init(hung_chang_dso_2100_drv_ptr, sr_ctx);
}

/* Wrapper: PXView cleanup(void) -> standard cleanup(driver) */
static int hung_chang_dso_2100_compat_cleanup(void)
{
	/* Call dev_clear to free per-device resources before cleanup */
	dev_clear(hung_chang_dso_2100_drv_ptr);
	return std_cleanup(hung_chang_dso_2100_drv_ptr);
}

/* Wrapper: PXView scan(options) -> standard scan(driver, options) */
static GSList *hung_chang_dso_2100_compat_scan(GSList *options)
{
	return scan(hung_chang_dso_2100_drv_ptr, options);
}

/* Wrapper: PXView config_get(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int hung_chang_dso_2100_compat_config_get(int id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel *ch,
	const struct sr_channel_group *cg)
{
	(void)ch;
	return config_get((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_set(id,data,sdi,ch,cg) -> standard(key,data,sdi,cg) */
static int hung_chang_dso_2100_compat_config_set(int id, GVariant *data,
	struct sr_dev_inst *sdi, struct sr_channel *ch,
	struct sr_channel_group *cg)
{
	(void)ch;
	return config_set((uint32_t)id, data, sdi, cg);
}

/* Wrapper: PXView config_list(info_id,data,sdi,cg) -> standard(key,data,sdi,cg) */
static int hung_chang_dso_2100_compat_config_list(int info_id, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	return config_list((uint32_t)info_id, data, sdi, cg);
}

/* Wrapper: PXView acquisition_start(sdi, cb_data) -> standard(sdi) */
static int hung_chang_dso_2100_compat_acquisition_start(struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_start(sdi);
}

/* Wrapper: PXView acquisition_stop(const sdi, cb_data) -> standard(sdi) */
static int hung_chang_dso_2100_compat_acquisition_stop(const struct sr_dev_inst *sdi,
	void *cb_data)
{
	(void)cb_data;
	return dev_acquisition_stop((struct sr_dev_inst *)sdi);
}

/* PXView-compatible driver info struct */
struct sr_dev_driver hung_chang_dso_2100_driver_info = {
	.name = "hung-chang-dso-2100",
	.longname = "Hung-Chang DSO-2100",
	.api_version = 1,
	.driver_type = DRIVER_TYPE_HARDWARE,
	.init = hung_chang_dso_2100_compat_init,
	.cleanup = hung_chang_dso_2100_compat_cleanup,
	.scan = hung_chang_dso_2100_compat_scan,
	.dev_mode_list = compat_dev_mode_list_default,
	.config_get = hung_chang_dso_2100_compat_config_get,
	.config_set = hung_chang_dso_2100_compat_config_set,
	.config_list = hung_chang_dso_2100_compat_config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_destroy = compat_dev_destroy_default,
	.dev_status_get = compat_dev_status_get_default,
	.dev_acquisition_start = hung_chang_dso_2100_compat_acquisition_start,
	.dev_acquisition_stop = hung_chang_dso_2100_compat_acquisition_stop,
	.priv = NULL,
};
