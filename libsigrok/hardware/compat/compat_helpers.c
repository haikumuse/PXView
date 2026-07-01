/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2025 Compat Layer Authors
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

#include "compat.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

/**
 * @file
 *
 * Compat helper function implementations.
 *
 * These functions provide standard sigrok API equivalents that are
 * missing from PXView's libsigrok.
 */

/*--- USB source add compat ------------------------------------------------*/

SR_PRIV int compat_usb_source_add(struct sr_context *ctx, int timeout,
    sr_receive_data_callback_t cb, const struct sr_dev_inst *sdi)
{
    /* In standard sigrok, usb_source_add() adds a libusb fd to the
     * session event loop. PXView's sr_session_source_add() does the
     * same thing but takes different parameters.
     * For now, we use a timer-based source (-1 fd) as a fallback.
     * Real USB drivers should use sr_session_source_add() directly
     * with the USB file descriptor. */
    (void)ctx;
    return sr_session_source_add(-1, 0, timeout, cb, sdi);
}

/*--- Channel group compat -------------------------------------------------*/

SR_PRIV struct sr_channel_group *compat_sr_channel_group_new(
    struct sr_dev_inst *sdi, const char *name, void *priv)
{
    struct sr_channel_group *cg;

    cg = g_malloc0(sizeof(struct sr_channel_group));
    cg->name = name;
    cg->channels = NULL;
    cg->priv = priv;

    if (sdi)
        sdi->channel_groups = g_slist_append(sdi->channel_groups, cg);

    return cg;
}

/*--- Channel creation compat ----------------------------------------------*/

SR_PRIV struct sr_channel *compat_sr_channel_new(struct sr_dev_inst *sdi,
    int index, int type, gboolean enabled, const char *name)
{
    struct sr_channel *ch;

    ch = g_malloc0(sizeof(struct sr_channel));
    ch->index = index;
    ch->type = type;
    ch->enabled = enabled;
    ch->name = g_strdup(name);
    ch->sdi = sdi;
    ch->priv = NULL;
    ch->trigger = NULL;

    if (sdi)
        sdi->channels = g_slist_append(sdi->channels, ch);

    return ch;
}

/*--- sr_config_get/set/list compat ----------------------------------------*/

SR_PRIV int sr_config_get_compat(const struct sr_dev_driver *driver,
    const struct sr_dev_inst *sdi, const struct sr_channel_group *cg,
    uint32_t key, GVariant **data)
{
    return sr_config_get(driver, sdi, NULL, cg, (int)key, data);
}

SR_PRIV int sr_config_set_compat(const struct sr_dev_inst *sdi,
    const struct sr_channel_group *cg, uint32_t key, GVariant *data)
{
    return sr_config_set((struct sr_dev_inst *)sdi, NULL,
        (struct sr_channel_group *)cg, (int)key, data);
}

SR_PRIV int sr_config_list_compat(const struct sr_dev_driver *driver,
    const struct sr_dev_inst *sdi, const struct sr_channel_group *cg,
    uint32_t key, GVariant **data)
{
    return sr_config_list(driver, sdi, cg, (int)key, data);
}

/*--- sr_dev_inst_new compat -----------------------------------------------*/

SR_PRIV struct sr_dev_inst *compat_sr_dev_inst_new(int inst_type, int status,
    const char *vendor, const char *model, const char *version)
{
    struct sr_dev_inst *sdi;

    /* PXView's sr_dev_inst_new takes mode instead of inst_type.
     * We pass 0 as mode since standard sigrok drivers don't use it.
     * Use parentheses to prevent macro expansion from compat.h. */
    sdi = (sr_dev_inst_new)(0, status, vendor, model, version);
    if (!sdi)
        return NULL;

    /* Set compat fields that standard sigrok expects */
    sdi->inst_type = inst_type;
    if (model)
        sdi->model = g_strdup(model);

    return sdi;
}

/*--- sr_usb_dev_inst_new compat -------------------------------------------*/

SR_PRIV struct sr_usb_dev_inst *compat_sr_usb_dev_inst_new(uint8_t bus,
    uint8_t address, struct libusb_device_handle *hdl)
{
    struct sr_usb_dev_inst *udi;

    /* Use parentheses to prevent macro expansion from compat.h */
    udi = (sr_usb_dev_inst_new)(bus, address);
    if (!udi)
        return NULL;

    /* Standard sigrok passes the handle directly; PXView sets it later */
    udi->devhdl = hdl;

    return udi;
}

/*--- Standard sigrok std_init ---------------------------------------------*/

SR_PRIV int std_init(struct sr_dev_driver *driver, struct sr_context *sr_ctx)
{
    struct compat_drv_context *drvc;

    drvc = g_malloc0(sizeof(struct compat_drv_context));
    drvc->sr_ctx = sr_ctx;
    drvc->instances = NULL;
    driver->priv = drvc;

    return SR_OK;
}

/*--- Standard sigrok std_cleanup ------------------------------------------*/

SR_PRIV int std_cleanup(const struct sr_dev_driver *driver)
{
    struct compat_drv_context *drvc;

    if (!driver || !driver->priv)
        return SR_ERR_ARG;

    drvc = driver->priv;
    g_slist_free_full(drvc->instances, (GDestroyNotify)sr_dev_inst_free);
    drvc->instances = NULL;
    g_free(drvc);

    /* Cast away const to set priv to NULL */
    ((struct sr_dev_driver *)driver)->priv = NULL;

    return SR_OK;
}

/*--- Standard sigrok std_dev_list -----------------------------------------*/

SR_PRIV GSList *std_dev_list(const struct sr_dev_driver *driver)
{
    struct compat_drv_context *drvc;

    if (!driver || !driver->priv)
        return NULL;

    drvc = driver->priv;
    return drvc->instances;
}

/*--- Standard sigrok std_dev_clear ----------------------------------------*/

SR_PRIV int std_dev_clear(const struct sr_dev_driver *driver)
{
    struct compat_drv_context *drvc;

    if (!driver || !driver->priv)
        return SR_ERR_ARG;

    drvc = driver->priv;
    g_slist_free_full(drvc->instances, (GDestroyNotify)sr_dev_inst_free);
    drvc->instances = NULL;

    return SR_OK;
}

/*--- Standard sigrok std_session_send_df_header ---------------------------*/
/* Note: std_session_send_df_header is provided by libsigrok/std.c (same
 * 2-arg signature). Do not redefine here to avoid multiple-definition
 * link errors. */

/*--- Standard sigrok std_session_send_df_end ------------------------------*/

SR_PRIV int std_session_send_df_end(const struct sr_dev_inst *sdi,
    const char *prefix)
{
    struct sr_datafeed_packet packet;
    (void)prefix;

    packet.type = SR_DF_END;
    packet.status = SR_PKT_OK;
    packet.payload = NULL;

    return ds_data_forward(sdi, &packet);
}

/*--- Standard sigrok std_session_send_df_frame_begin ----------------------*/

SR_PRIV int std_session_send_df_frame_begin(const struct sr_dev_inst *sdi)
{
    struct sr_datafeed_packet packet;

    packet.type = SR_DF_FRAME_BEGIN;
    packet.status = SR_PKT_OK;
    packet.payload = NULL;

    return ds_data_forward(sdi, &packet);
}

/*--- Standard sigrok std_session_send_df_frame_end ------------------------*/

SR_PRIV int std_session_send_df_frame_end(const struct sr_dev_inst *sdi)
{
    struct sr_datafeed_packet packet;

    packet.type = SR_DF_FRAME_END;
    packet.status = SR_PKT_OK;
    packet.payload = NULL;

    return ds_data_forward(sdi, &packet);
}

/*--- Standard sigrok sr_session_send_meta ---------------------------------*/

SR_PRIV int sr_session_send_meta(const struct sr_dev_inst *sdi,
    uint32_t key, GVariant *data)
{
    struct sr_datafeed_packet packet;
    struct sr_datafeed_meta meta;
    struct sr_config *src;

    if (!sdi || !data)
        return SR_ERR_ARG;

    src = sr_config_new((int)key, data);
    if (!src)
        return SR_ERR;

    memset(&packet, 0, sizeof(packet));
    packet.type = SR_DF_META;
    packet.status = SR_PKT_OK;
    packet.payload = &meta;
    meta.config = g_slist_append(NULL, src);

    ds_data_forward(sdi, &packet);

    sr_config_free(src);
    g_slist_free(meta.config);

    return SR_OK;
}

/*--- Standard sigrok std_config_list --------------------------------------*/

SR_PRIV int std_config_list(uint32_t key, GVariant **data,
    const struct sr_dev_inst *sdi, const struct sr_channel_group *cg,
    const uint32_t scanopts[], size_t num_scanopts,
    const uint32_t drvopts[], size_t num_drvopts,
    const uint32_t devopts[], size_t num_devopts)
{
    (void)sdi;
    (void)cg;

    /*
     * Treat NULL option arrays as empty. This supports the NO_OPTS
     * macro (which is NULL) passed through STD_CONFIG_LIST: ARRAY_SIZE()
     * on NULL yields a bogus count, so we must clamp it to 0 here before
     * the array is accessed by std_gvar_array_u32().
     */
    if (!scanopts)
        num_scanopts = 0;
    if (!drvopts)
        num_drvopts = 0;
    if (!devopts)
        num_devopts = 0;

    switch (key) {
    case SR_CONF_SCAN_OPTIONS:
        *data = std_gvar_array_u32(scanopts, num_scanopts);
        break;
    case SR_CONF_DEVICE_OPTIONS:
        if (sdi)
            *data = std_gvar_array_u32(devopts, num_devopts);
        else
            *data = std_gvar_array_u32(drvopts, num_drvopts);
        break;
    default:
        return SR_ERR_NA;
    }

    return SR_OK;
}

/*--- GVariant helper functions --------------------------------------------*/

SR_PRIV GVariant *std_gvar_samplerates(const uint64_t samplerates[],
    size_t count)
{
    GVariant *gvar;
    GVariantBuilder gvb;
    size_t i;

    g_variant_builder_init(&gvb, G_VARIANT_TYPE("a{sv}"));
    for (i = 0; i < count; i++) {
        g_variant_builder_add(&gvb, "{sv}", "samplerate",
            g_variant_new_uint64(samplerates[i]));
    }
    gvar = g_variant_builder_end(&gvb);

    return gvar;
}

SR_PRIV GVariant *std_gvar_samplerate_steps(const uint64_t steps[],
    size_t count)
{
    return std_gvar_array_u64(steps, count);
}

SR_PRIV GVariant *std_gvar_array_i32(const int32_t vals[], size_t count)
{
    GVariantBuilder gvb;
    size_t i;

    g_variant_builder_init(&gvb, G_VARIANT_TYPE("ai"));
    for (i = 0; i < count; i++)
        g_variant_builder_add(&gvb, "i", vals[i]);

    return g_variant_builder_end(&gvb);
}

SR_PRIV GVariant *std_gvar_array_u32(const uint32_t vals[], size_t count)
{
    GVariantBuilder gvb;
    size_t i;

    g_variant_builder_init(&gvb, G_VARIANT_TYPE("au"));
    for (i = 0; i < count; i++)
        g_variant_builder_add(&gvb, "u", vals[i]);

    return g_variant_builder_end(&gvb);
}

SR_PRIV GVariant *std_gvar_array_u64(const uint64_t vals[], size_t count)
{
    GVariantBuilder gvb;
    size_t i;

    g_variant_builder_init(&gvb, G_VARIANT_TYPE("at"));
    for (i = 0; i < count; i++)
        g_variant_builder_add(&gvb, "t", vals[i]);

    return g_variant_builder_end(&gvb);
}

SR_PRIV GVariant *std_gvar_min_max_step(double min, double max, double step)
{
    GVariant *range[3];

    range[0] = g_variant_new_double(min);
    range[1] = g_variant_new_double(max);
    range[2] = g_variant_new_double(step);

    return g_variant_new_tuple(range, 3);
}

SR_PRIV GVariant *std_gvar_min_max_steps_uint64(uint64_t min, uint64_t max,
    const uint64_t steps[], size_t count)
{
    GVariant *range[3];

    range[0] = g_variant_new_uint64(min);
    range[1] = g_variant_new_uint64(max);
    range[2] = std_gvar_array_u64(steps, count);

    return g_variant_new_tuple(range, 3);
}

SR_PRIV GVariant *std_gvar_tuple_array(const uint64_t a[][2],
	unsigned int n)
{
	GVariant *rational[2];
	GVariantBuilder gvb;
	unsigned int i;

	/* Canonical libsigrok implementation (std.c:595): builds "a(tt)"
	 * from a uint64_t[][2] array. Used by SCPI oscilloscope drivers
	 * (rigol-ds, siglent-sds, hameg-hmo, hantek-6xxx, ...) for their
	 * vdivs/timebases tables. */
	g_variant_builder_init(&gvb, G_VARIANT_TYPE("a(tt)"));

	for (i = 0; i < n; i++) {
		rational[0] = g_variant_new_uint64(a[i][0]);
		rational[1] = g_variant_new_uint64(a[i][1]);
		g_variant_builder_add_value(&gvb,
			g_variant_new_tuple(rational, 2));
	}

	return g_variant_builder_end(&gvb);
}

/*--- Config get helpers (3-arg canonical form) ---------------------------*/

SR_PRIV int std_u64_idx(GVariant *data, const uint64_t a[], unsigned int n)
{
	uint64_t v;
	unsigned int i;

	/* Canonical libsigrok (std.c): look up a GVariant uint64 in an
	 * array of uint64 values, return the matching index or -1. */
	if (!data || !a || n == 0)
		return -1;

	v = g_variant_get_uint64(data);
	for (i = 0; i < n; i++)
		if (a[i] == v)
			return (int)i;

	return -1;
}

SR_PRIV int std_str_idx(GVariant *data, const char *a[], unsigned int n)
{
	const char *s;
	unsigned int i;

	/* Canonical libsigrok (std.c): look up a GVariant string in an
	 * array of strings, return the matching index or -1. */
	if (!data || !a || n == 0)
		return -1;

	s = g_variant_get_string(data, NULL);
	if (!s)
		return -1;

	for (i = 0; i < n; i++)
		if (a[i] && g_strcmp0(s, a[i]) == 0)
			return (int)i;

	return -1;
}

SR_PRIV int std_bool_idx(const struct sr_dev_inst *sdi, uint32_t key,
    GVariant **data, const gboolean vals[], size_t count)
{
    (void)sdi;
    (void)key;

    if (!data || !vals || count == 0)
        return SR_ERR_ARG;

    *data = g_variant_new_boolean(vals[0]);
    return SR_OK;
}

/*--- Soft trigger logic ---------------------------------------------------*/

SR_PRIV struct soft_trigger_logic *soft_trigger_logic_new(
    const struct sr_dev_inst *sdi, struct sr_trigger *trigger,
    uint64_t pre_trigger_samples)
{
    struct soft_trigger_logic *stl;

    stl = g_malloc0(sizeof(struct soft_trigger_logic));
    if (!stl)
        return NULL;

    stl->sdi = sdi;
    stl->trigger = trigger;
    stl->pre_trigger_samples = pre_trigger_samples;
    stl->cur_stage = 0;

    return stl;
}

SR_PRIV void soft_trigger_logic_free(struct soft_trigger_logic *stl)
{
    if (!stl)
        return;

    g_free(stl);
}

/**
 * Check if trigger conditions are met in the given logic data buffer.
 *
 * This is a simplified implementation that provides basic trigger matching.
 * Full trigger logic requires parsing the trigger structure (sr_trigger),
 * which has stages, matches, and channel associations.
 *
 * For now, this implementation:
 * - Returns -1 if trigger condition not met (no trigger)
 * - Returns 0 if trigger fires immediately at buffer start
 * - Returns positive offset if trigger fires mid-buffer
 *
 * @param stl Soft trigger logic state
 * @param buf Logic sample buffer (one sample per bit, packed)
 * @param buflen Buffer length in bytes
 * @param pre_trigger_samples Output: number of pre-trigger samples captured
 *
 * @return Trigger offset in samples, or -1 if not triggered
 */
SR_PRIV int soft_trigger_logic_check(struct soft_trigger_logic *stl,
    uint8_t *buf, size_t buflen, int *pre_trigger_samples)
{
    size_t i;
    uint8_t prev_sample, cur_sample;
    size_t sample_count;

    if (!stl || !buf || buflen == 0)
        return -1;

    /* Set pre_trigger_samples output if provided */
    if (pre_trigger_samples)
        *pre_trigger_samples = (int)stl->pre_trigger_samples;

    /* If no trigger configured, fire immediately */
    if (!stl->trigger) {
        return 0;
    }

    /* Simplified trigger matching:
     * For basic edge trigger detection, we scan for transitions.
     * This is a placeholder implementation - full trigger logic
     * would need to parse stl->trigger structure for:
     * - Trigger stages
     * - Match conditions (rising/falling/edge/level)
     * - Channel assignments
     * - Trigger counts/delays
     */

    /* Basic edge detection on channel 0 (bit 0 of each sample) */
    prev_sample = buf[0] & 0x01;
    sample_count = buflen * 8;  /* Assuming 8 samples per byte */

    for (i = 1; i < buflen; i++) {
        cur_sample = buf[i] & 0x01;

        /* Detect rising or falling edge */
        if (cur_sample != prev_sample) {
            /* Trigger fired at this sample position */
            /* Return offset in samples (not bytes) */
            int trigger_offset = (int)(i * 8);

            /* Limit pre-trigger samples to actual buffer content */
            if (pre_trigger_samples) {
                int max_pre = trigger_offset;
                if ((int)stl->pre_trigger_samples > max_pre)
                    *pre_trigger_samples = max_pre;
                else
                    *pre_trigger_samples = (int)stl->pre_trigger_samples;
            }

            return trigger_offset;
        }

        prev_sample = cur_sample;
    }

    /* No trigger condition met in this buffer */
    return -1;
}

/**
 * Reset soft trigger logic state (for multi-stage triggers).
 * Call this when starting a new trigger sequence or after a trigger fires.
 */
SR_PRIV void soft_trigger_logic_reset(struct soft_trigger_logic *stl)
{
    if (!stl)
        return;

    stl->cur_stage = 0;
}

/*--- sr_session_trigger_get stub ------------------------------------------*/

SR_PRIV struct sr_trigger *sr_session_trigger_get(const struct sr_session *session)
{
    (void)session;
    /* Stub: no trigger support in PXView compat layer */
    return NULL;
}

/*--- Stub implementations -------------------------------------------------*/

SR_PRIV void sr_parse_probe_names(const char *str, const char *defaults[],
    int ch_max, struct sr_channel **channels)
{
    (void)str;
    (void)defaults;
    (void)ch_max;
    (void)channels;
    /* Stub - not yet implemented */
}

SR_PRIV gboolean usb_match_manuf_prod(libusb_device *dev,
    const char *manufacturer, const char *product)
{
    (void)dev;
    (void)manufacturer;
    (void)product;
    /* Stub - always returns FALSE */
    return FALSE;
}

SR_PRIV int usb_get_port_path(libusb_device *dev, char *path, int path_len)
{
    (void)dev;
    (void)path;
    (void)path_len;
    /* Stub - returns error */
    return SR_ERR_NA;
}

/*--- std_gvar_tuple_double ------------------------------------------------*/

SR_PRIV GVariant *std_gvar_tuple_double(double first, double second)
{
    GVariant *vals[2];

    vals[0] = g_variant_new_double(first);
    vals[1] = g_variant_new_double(second);

    return g_variant_new_tuple(vals, 2);
}

/*--- std_double_tuple_idx -------------------------------------------------*/

SR_PRIV int std_double_tuple_idx(GVariant *data, const double (*vals)[2],
    int count)
{
    double low, high;
    int i;

    if (!data || !vals || count <= 0)
        return -1;

    g_variant_get(data, "(dd)", &low, &high);

    for (i = 0; i < count; i++) {
        if (vals[i][0] == low && vals[i][1] == high)
            return i;
    }

    return -1;
}

/*--- std_gvar_thresholds --------------------------------------------------*/

SR_PRIV GVariant *std_gvar_thresholds(const double (*thresholds)[2],
    int count)
{
    GVariantBuilder gvb;
    GVariant *gvar;
    int i;

    g_variant_builder_init(&gvb, G_VARIANT_TYPE("a(dd)"));
    if (thresholds) {
        for (i = 0; i < count; i++) {
            g_variant_builder_add(&gvb, "(dd)",
                thresholds[i][0], thresholds[i][1]);
        }
    }
    gvar = g_variant_builder_end(&gvb);

    return gvar;
}

/*--- std_gvar_min_max_step_thresholds -------------------------------------*/

SR_PRIV GVariant *std_gvar_min_max_step_thresholds(double min, double max,
    double step, const double *thresholds, int count)
{
    GVariantBuilder gvb;
    GVariant *gvar;
    int i;

    g_variant_builder_init(&gvb, G_VARIANT_TYPE("a(dd)"));
    if (thresholds) {
        /* Use provided threshold pairs (flat array: low0, high0, ...). */
        for (i = 0; i < count; i++) {
            g_variant_builder_add(&gvb, "(dd)",
                thresholds[i * 2], thresholds[i * 2 + 1]);
        }
    } else if (step > 0.0) {
        /*
         * Generate single-value pairs (v, v) from min to max in steps of
         * step. The half-step margin guards against floating-point drift
         * dropping the final entry.
         */
        double v;
        for (v = min; v <= max + step * 0.5; v += step) {
            double t = (v > max) ? max : v;
            g_variant_builder_add(&gvb, "(dd)", t, t);
        }
    }
    gvar = g_variant_builder_end(&gvb);

    return gvar;
}

/*--- std_opts_config_list -------------------------------------------------*/

SR_PRIV int std_opts_config_list(GSList *opts, int id, GVariant **data,
    const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
    (void)sdi;
    (void)cg;

    switch (id) {
    case SR_CONF_DEVICE_OPTIONS:
    case SR_CONF_SCAN_OPTIONS:
        if (data) {
            GVariantBuilder gvb;
            GSList *l;

            g_variant_builder_init(&gvb, G_VARIANT_TYPE("au"));
            for (l = opts; l; l = l->next)
                g_variant_builder_add(&gvb, "u", GPOINTER_TO_UINT(l->data));
            *data = g_variant_builder_end(&gvb);
        }
        break;
    default:
        return SR_ERR_NA;
    }

    return SR_OK;
}

/*--- sr_usb_dev_inst_free_cb ----------------------------------------------*/

SR_PRIV void sr_usb_dev_inst_free_cb(void *data)
{
	sr_usb_dev_inst_free((struct sr_usb_dev_inst *)data);
}

/*--- sr_usb_open / sr_usb_close compat ------------------------------------*/

SR_PRIV int sr_usb_open(libusb_context *usb_ctx, struct sr_usb_dev_inst *usb)
{
	int ret;

	(void)usb_ctx;

	if (!usb || !usb->usb_dev)
		return SR_ERR_ARG;

	/* Already open. */
	if (usb->devhdl)
		return SR_OK;

	ret = libusb_open(usb->usb_dev, &usb->devhdl);
	if (ret != 0) {
		sr_err("Failed to open USB device (%d.%d): %s.",
			usb->bus, usb->address, libusb_error_name(ret));
		usb->devhdl = NULL;
		return SR_ERR;
	}

	return SR_OK;
}

SR_PRIV int sr_usb_close(struct sr_usb_dev_inst *usb)
{
	if (!usb)
		return SR_ERR_ARG;

	if (usb->devhdl) {
		/*
		 * Callers (e.g. scpi_usbtmc_libusb_close) are expected to
		 * release any claimed interfaces before reaching here, since
		 * the interface number is driver-private and not stored in
		 * struct sr_usb_dev_inst. We only release the handle.
		 */
		libusb_close(usb->devhdl);
		usb->devhdl = NULL;
	}

	return SR_OK;
}

/*--- sr_resource_open / read / close compat --------------------------------*/
/*
 * PXView's libsigrok (based on upstream 0.2.0) lacks the sr_resource API
 * (introduced upstream 2015-10). Standard sigrok drivers that upload
 * firmware (kingst-la2016, lecroy-logicstudio, sysclk-lwla) call
 * sr_resource_open/read/close with `struct sr_resource *` and need these
 * functions to link.
 *
 * The implementation mirrors the local copies that previously existed in
 * saleae-logic16/protocol.c and asix-sigma/protocol.c: it opens files from
 * the DS_RES_PATH directory using fopen/fread/fclose. DS_RES_PATH is an
 * `extern char[500]` declared in libsigrok-internal.h and defined in
 * lib_main.c; it is visible here via compat.h -> libsigrok-internal.h.
 */

SR_PRIV int sr_resource_open(struct sr_context *sr_ctx,
        struct sr_resource *res, int type, const char *name)
{
	char path[512];
	FILE *fp;
	long file_size;

	(void)sr_ctx;
	(void)type;

	if (!res || !name)
		return SR_ERR_ARG;

	res->size = 0;
	res->handle = NULL;
	res->type = type;

	if (DS_RES_PATH[0] != '\0')
		snprintf(path, sizeof(path), "%s/%s", DS_RES_PATH, name);
	else
		snprintf(path, sizeof(path), "%s", name);

	fp = fopen(path, "rb");
	if (!fp) {
		sr_err("Failed to open resource '%s'.", path);
		return SR_ERR;
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		sr_err("Failed to seek resource '%s'.", path);
		fclose(fp);
		return SR_ERR;
	}
	file_size = ftell(fp);
	if (file_size < 0) {
		sr_err("Failed to tell size of resource '%s'.", path);
		fclose(fp);
		return SR_ERR;
	}
	rewind(fp);

	res->size = (uint64_t)file_size;
	res->handle = fp;

	return SR_OK;
}

SR_PRIV gssize sr_resource_read(struct sr_context *sr_ctx,
        const struct sr_resource *res, void *buf, size_t count)
{
	FILE *fp;
	size_t n_read;

	(void)sr_ctx;

	if (!res || !buf)
		return SR_ERR_ARG_NEG;

	fp = (FILE *)res->handle;
	if (!fp)
		return SR_ERR_ARG_NEG;

	n_read = fread(buf, 1, count, fp);
	if (n_read == 0 && ferror(fp))
		return SR_ERR_NEG;

	return (gssize)n_read;
}

SR_PRIV int sr_resource_close(struct sr_context *sr_ctx,
        struct sr_resource *res)
{
	FILE *fp;

	(void)sr_ctx;

	if (!res)
		return SR_ERR_ARG;

	fp = (FILE *)res->handle;
	if (fp) {
		fclose(fp);
		res->handle = NULL;
	}
	res->size = 0;

	return SR_OK;
}

/*--- sr_log_loglevel_get compat --------------------------------------------*/
/*
 * PXView's libsigrok does not expose the log level (it stores it in a static
 * variable inside log.c). Return SR_LOG_DBG (4) so that conditional spew
 * output (level 5) is suppressed while regular debug output is visible.
 * Drivers that gate debug dumps on `sr_log_loglevel_get() >= SR_LOG_SPEW`
 * will skip the dump, matching the default-no-verbose-logging behaviour.
 */

SR_PRIV int sr_log_loglevel_get(void)
{
	return SR_LOG_DBG;
}

/*--- sr_hexdump_new / free compat ------------------------------------------*/
/*
 * PXView's libsigrok does not provide these debug helpers. The canonical
 * upstream implementation (strutil.c) returns a GString; drivers assign the
 * result to `GString *` variables and feed it to sr_dbg/sr_spew. This
 * matches that signature exactly so all driver call sites compile and link
 * without modification.
 */

SR_PRIV GString *sr_hexdump_new(const uint8_t *data, const size_t len)
{
	GString *s;
	size_t i;
	size_t cap;

	if (!data && len)
		return NULL;

	/* "XX " per byte, plus a space every 8 and a newline every 16. */
	cap = 3 * len + len / 8 + len / 16 + 1;
	s = g_string_sized_new(cap);
	if (!s)
		return NULL;

	for (i = 0; i < len; i++) {
		if (i)
			g_string_append_c(s, ' ');
		if (i && (i % 8) == 0)
			g_string_append_c(s, ' ');
		if (i && (i % 16) == 0)
			g_string_append_c(s, ' ');
		g_string_append_printf(s, "%02x", data[i]);
	}

	return s;
}

SR_PRIV void sr_hexdump_free(GString *s)
{
	if (s)
		g_string_free(s, TRUE);
}

/*--- std_gvar_array_str compat ---------------------------------------------*/
/*
 * Canonical libsigrok (std.c:747): build a GVariant "as" (array of strings)
 * from a const char * array. Previously PXView's compat layer mis-named this
 * std_gvar_tuple_array and gave it a string-array signature; that collided
 * with the canonical uint64_t[][2] std_gvar_tuple_array. Drivers that need
 * a string array should call this function by its canonical name.
 */

SR_PRIV GVariant *std_gvar_array_str(const char *a[], unsigned int n)
{
	GVariantBuilder gvb;
	unsigned int i;

	g_variant_builder_init(&gvb, G_VARIANT_TYPE("as"));
	for (i = 0; i < n; i++)
		g_variant_builder_add(&gvb, "s", a[i]);

	return g_variant_builder_end(&gvb);
}

/*--- std_u64_tuple_idx / std_cg_idx compat ---------------------------------*/
/*
 * Canonical libsigrok (std.c:867): given a GVariant "(tt)" tuple and an
 * array of uint64_t pairs, return the index of the matching pair or -1.
 * Used by SCPI oscilloscope drivers (rigol-ds, siglent-sds, hameg-hmo,
 * hantek-6xxx, kecheng-kc-330b) to validate config_set values against
 * the driver's vdivs/timebases/sample-intervals tables.
 */

SR_PRIV int std_u64_tuple_idx(GVariant *data, const uint64_t a[][2],
	unsigned int n)
{
	uint64_t low, high;
	unsigned int i;

	if (!data || !a || n == 0)
		return -1;

	g_variant_get(data, "(tt)", &low, &high);

	for (i = 0; i < n; i++)
		if (a[i][0] == low && a[i][1] == high)
			return (int)i;

	return -1;
}

/*
 * Canonical libsigrok (std.c:906): given a channel group pointer and an
 * array of channel group pointers, return the index of the matching
 * pointer or -1. Used by SCPI oscilloscope drivers (hameg-hmo,
 * lecroy-xstream, rigol-ds, siglent-sds) to map a cg back to its
 * analog/digital channel-group index.
 */

SR_PRIV int std_cg_idx(const struct sr_channel_group *cg,
	struct sr_channel_group *a[], unsigned int n)
{
	unsigned int i;

	if (!cg || !a || n == 0)
		return -1;

	for (i = 0; i < n; i++)
		if (cg == a[i])
			return (int)i;

	return -1;
}

/*--- std_dev_clear_with_callback compat ------------------------------------*/
/*
 * Canonical libsigrok (std.c:405): iterate the driver's instance list; for
 * each sdi call dev_close() if it is SR_ST_ACTIVE, then invoke the callback
 * on sdi->priv (so the driver can release per-device resources), then free
 * sdi->priv and the sdi itself.
 *
 * PXView mapping:
 *   - driver->context (canonical) -> driver->priv (PXView) which holds a
 *     struct compat_drv_context * with an ->instances GSList.
 *   - sdi->conn / sdi->inst_type / sdi->status / sdi->priv all exist on
 *     PXView's sr_dev_inst (libsigrok-internal.h:141), so the canonical
 *     cleanup logic translates directly. PXView does not have
 *     sr_serial_dev_inst_free / sr_usb_dev_inst_free / sr_scpi_free /
 *     sr_modbus_free as free-standing symbols available here, and the
 *     existing std_dev_clear() in this file does not free sdi->conn
 *     either; to stay consistent with the surrounding compat code we
 *     skip the conn-type-specific free and only call the driver's
 *     dev_close() callback plus the clear_private hook. The callback
 *     itself is responsible for any per-device teardown it needs.
 */
SR_PRIV int std_dev_clear_with_callback(const struct sr_dev_driver *driver,
	std_dev_clear_callback clear_private)
{
	struct compat_drv_context *drvc;
	struct sr_dev_inst *sdi;
	GSList *l;
	int ret;

	if (!driver) {
		sr_err("%s: Invalid argument.", __func__);
		return SR_ERR_ARG;
	}

	if (!driver->priv)
		return SR_OK;

	drvc = driver->priv;

	ret = SR_OK;
	for (l = drvc->instances; l; l = l->next) {
		if (!(sdi = l->data)) {
			sr_err("%s: Invalid device instance.", __func__);
			ret = SR_ERR_BUG;
			continue;
		}

		/* Close the device if it is currently active. Cast away const
		 * because dev_close takes a non-const sdi (PXView matches
		 * canonical sigrok here). */
		if (driver->dev_close && sdi->status == SR_ST_ACTIVE)
			driver->dev_close(sdi);

		/* Let the driver release per-device private resources. */
		if (clear_private)
			clear_private(sdi->priv);

		/* Free the device context (devc) and then the sdi itself. */
		g_free(sdi->priv);
		sr_dev_inst_free(sdi);
	}

	g_slist_free(drvc->instances);
	drvc->instances = NULL;

	return ret;
}

/*--- sr_strerror compat ---------------------------------------------------*/
/*
 * Canonical libsigrok (error.c:53): return a human-readable error string
 * for the given error code. PXView's libsigrok does not expose this; many
 * standard drivers (fluke-45, rigol-ds, siglent-sds, atten-pps3xxx,
 * gwinstek-gds-800, motech-lps-30x, scpi-dmm) log sr_strerror(ret) on
 * failure. This implementation covers the SR_* codes PXView defines
 * (positive values 0-12) plus the additional standard sigrok codes the
 * compat layer emulates; unknown codes return "unknown error" exactly
 * like the upstream version.
 *
 * Note: PXView uses positive error codes (SR_ERR=1, SR_ERR_ARG=3, ...),
 * so the switch below uses those positive values, not the canonical
 * negative ones. Drivers that compare against the named SR_* macros
 * work either way because the macros expand to the same positive values
 * in PXView.
 */
const char *sr_strerror(int error_code)
{
	switch (error_code) {
	case SR_OK:
		return "no error";
	case SR_ERR:
		return "generic/unspecified error";
	case SR_ERR_MALLOC:
		return "memory allocation error";
	case SR_ERR_ARG:
		return "invalid argument";
	case SR_ERR_BUG:
		return "internal error";
	case SR_ERR_SAMPLERATE:
		return "invalid samplerate";
	case SR_ERR_NA:
		return "not applicable";
	case SR_ERR_DEVICE_CLOSED:
		return "device closed but should be open";
	case SR_ERR_CALL_STATUS:
		return "function call status error";
	case SR_ERR_IO:
		return "input/output error";
	case SR_ERR_DATA:
		return "data is invalid";
	default:
		return "unknown error";
	}
}

/*--- sr_atol / sr_atoi / sr_atof_ascii compat -----------------------------*/
/*
 * Canonical libsigrok (strutil.c:73/216/372): strict, locale-independent
 * ASCII numeric parsers. PXView's libsigrok does not provide them; SCPI/DMM
 * drivers (rigol-ds, siglent-sds, fluke-45, gwinstek-gds-800, agilent-dmm,
 * ...) use them to parse instrument responses without being affected by
 * the user's locale. Implementation mirrors canonical strutil.c with
 * PXView's positive SR_OK/SR_ERR codes.
 */

SR_PRIV int sr_atol(const char *str, long *ret)
{
	long tmp;
	char *endptr = NULL;

	if (!str || !ret)
		return SR_ERR_ARG;

	errno = 0;
	tmp = strtol(str, &endptr, 10);

	while (endptr && isspace((unsigned char)*endptr))
		endptr++;

	if (!endptr || *endptr || errno) {
		if (!errno)
			errno = EINVAL;
		return SR_ERR;
	}

	*ret = tmp;
	return SR_OK;
}

SR_PRIV int sr_atoi(const char *str, int *ret)
{
	long tmp;

	if (!str || !ret)
		return SR_ERR_ARG;

	if (sr_atol(str, &tmp) != SR_OK)
		return SR_ERR;

	if ((int)tmp != tmp) {
		errno = ERANGE;
		return SR_ERR;
	}

	*ret = (int)tmp;
	return SR_OK;
}

SR_PRIV int sr_atof_ascii(const char *str, float *ret)
{
	double tmp;
	char *endptr = NULL;

	if (!str || !ret)
		return SR_ERR_ARG;

	errno = 0;
	tmp = g_ascii_strtod(str, &endptr);

	while (endptr && isspace((unsigned char)*endptr))
		endptr++;

	if (!endptr || *endptr || errno) {
		if (!errno)
			errno = EINVAL;
		return SR_ERR;
	}

	*ret = (float)tmp;
	return SR_OK;
}
