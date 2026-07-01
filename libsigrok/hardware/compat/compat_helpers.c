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

SR_PRIV GVariant *std_gvar_tuple_array(const char *strs[], size_t count)
{
    GVariantBuilder gvb;
    size_t i;

    g_variant_builder_init(&gvb, G_VARIANT_TYPE("as"));
    for (i = 0; i < count; i++)
        g_variant_builder_add(&gvb, "s", strs[i]);

    return g_variant_builder_end(&gvb);
}

/*--- Config get helpers ---------------------------------------------------*/

SR_PRIV int std_u64_idx(const struct sr_dev_inst *sdi, uint32_t key,
    GVariant **data, const uint64_t vals[], size_t count)
{
    (void)sdi;
    (void)key;

    if (!data || !vals || count == 0)
        return SR_ERR_ARG;

    *data = g_variant_new_uint64(vals[0]);
    return SR_OK;
}

SR_PRIV int std_str_idx(const struct sr_dev_inst *sdi, uint32_t key,
    GVariant **data, const char *const strs[], size_t count)
{
    (void)sdi;
    (void)key;

    if (!data || !strs || count == 0)
        return SR_ERR_ARG;

    *data = g_variant_new_string(strs[0]);
    return SR_OK;
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
