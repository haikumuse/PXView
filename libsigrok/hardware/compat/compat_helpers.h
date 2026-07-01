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

#ifndef LIBSIGROK_COMPAT_HELPERS_H
#define LIBSIGROK_COMPAT_HELPERS_H

/**
 * @file
 *
 * Compat helper function declarations.
 *
 * These functions provide standard sigrok API equivalents that are
 * missing from PXView's libsigrok. Implementations are in compat_helpers.c.
 */

#include <glib.h>
#include <stdint.h>

/**
 * Add a USB file descriptor as a session data source.
 * Wraps PXView's sr_session_source_add() for USB device handles.
 *
 * @param ctx The sr_context.
 * @param timeout Timeout in ms.
 * @param cb Callback function for received data.
 * @param sdi The device instance.
 *
 * @return SR_OK on success, SR_ERR on failure.
 */
SR_PRIV int compat_usb_source_add(struct sr_context *ctx, int timeout,
    sr_receive_data_callback_t cb, const struct sr_dev_inst *sdi);

/**
 * Create a new channel group and add it to the device's channel_groups list.
 *
 * @param sdi The device instance.
 * @param name Name of the channel group.
 * @param priv Private data for driver use.
 *
 * @return Pointer to the new channel group, or NULL on error.
 */
SR_PRIV struct sr_channel_group *compat_sr_channel_group_new(
    struct sr_dev_inst *sdi, const char *name, void *priv);

/**
 * Create a new channel with sdi back-reference set.
 * This is the standard sigrok version of sr_channel_new that also
 * sets the sdi back-reference field.
 *
 * @param sdi The device instance that owns this channel.
 * @param index Channel index.
 * @param type Channel type (SR_CHANNEL_LOGIC, SR_CHANNEL_ANALOG_STD, etc.).
 * @param enabled Whether the channel is enabled.
 * @param name Channel name.
 *
 * @return Pointer to the new channel, or NULL on error.
 */
SR_PRIV struct sr_channel *compat_sr_channel_new(struct sr_dev_inst *sdi,
    int index, int type, gboolean enabled, const char *name);

/**
 * Standard sigrok's sr_config_get compat - uint32_t key, no ch parameter.
 * Calls PXView's sr_config_get with (int)key cast and ch=NULL.
 *
 * @param driver The device driver.
 * @param sdi The device instance.
 * @param cg The channel group (may be NULL).
 * @param key The config key (uint32_t in standard sigrok).
 * @param data Output GVariant.
 *
 * @return SR_OK on success, SR_ERR on failure.
 */
SR_PRIV int sr_config_get_compat(const struct sr_dev_driver *driver,
    const struct sr_dev_inst *sdi, const struct sr_channel_group *cg,
    uint32_t key, GVariant **data);

/**
 * Standard sigrok's sr_config_set compat - uint32_t key, no ch parameter.
 * Calls PXView's sr_config_set with (int)key cast and ch=NULL.
 *
 * @param sdi The device instance.
 * @param cg The channel group (may be NULL).
 * @param key The config key (uint32_t in standard sigrok).
 * @param data Input GVariant.
 *
 * @return SR_OK on success, SR_ERR on failure.
 */
SR_PRIV int sr_config_set_compat(const struct sr_dev_inst *sdi,
    const struct sr_channel_group *cg, uint32_t key, GVariant *data);

/**
 * Standard sigrok's sr_config_list compat - uint32_t key, no ch parameter.
 * Calls PXView's sr_config_list with (int)key cast and ch=NULL.
 *
 * @param driver The device driver.
 * @param sdi The device instance.
 * @param cg The channel group (may be NULL).
 * @param key The config key (uint32_t in standard sigrok).
 * @param data Output GVariant.
 *
 * @return SR_OK on success, SR_ERR on failure.
 */
SR_PRIV int sr_config_list_compat(const struct sr_dev_driver *driver,
    const struct sr_dev_inst *sdi, const struct sr_channel_group *cg,
    uint32_t key, GVariant **data);

/**
 * Standard sigrok's sr_dev_inst_new compat - creates a PXView sr_dev_inst
 * and also sets the compat fields (model, inst_type).
 *
 * @param inst_type Instance type (SR_INST_USB, SR_INST_SERIAL, etc.).
 * @param status Device status (SR_ST_NOT_FOUND, SR_ST_INITIALIZING, etc.).
 * @param vendor Vendor name.
 * @param model Model name.
 * @param version Version string.
 *
 * @return Pointer to the new device instance, or NULL on error.
 */
SR_PRIV struct sr_dev_inst *compat_sr_dev_inst_new(int inst_type, int status,
    const char *vendor, const char *model, const char *version);

/**
 * Standard sigrok's sr_usb_dev_inst_new compat - accepts hdl parameter.
 * PXView's version only takes bus and address; this version also sets
 * the devhdl field from the hdl parameter.
 *
 * @param bus USB bus number.
 * @param address USB device address.
 * @param hdl libusb device handle (may be NULL).
 *
 * @return Pointer to the new USB device instance, or NULL on error.
 */
SR_PRIV struct sr_usb_dev_inst *compat_sr_usb_dev_inst_new(uint8_t bus,
    uint8_t address, struct libusb_device_handle *hdl);

/**
 * Standard sigrok's std_init - creates drv_context and sets driver->priv.
 *
 * @param driver The device driver.
 * @param sr_ctx The sigrok context.
 *
 * @return SR_OK on success.
 */
SR_PRIV int std_init(struct sr_dev_driver *driver, struct sr_context *sr_ctx);

/**
 * Standard sigrok's std_cleanup - frees drv_context and clears instances.
 *
 * @param driver The device driver.
 *
 * @return SR_OK on success, SR_ERR_ARG on invalid arguments.
 */
SR_PRIV int std_cleanup(const struct sr_dev_driver *driver);

/**
 * Standard sigrok's std_dev_list - returns the driver's instance list.
 *
 * @param driver The device driver.
 *
 * @return GSList of device instances.
 */
SR_PRIV GSList *std_dev_list(const struct sr_dev_driver *driver);

/**
 * Standard sigrok's std_dev_clear - clears all device instances.
 *
 * @param driver The device driver.
 *
 * @return SR_OK on success.
 */
SR_PRIV int std_dev_clear(const struct sr_dev_driver *driver);

/**
 * Standard sigrok's std_session_send_df_end - send DF_END packet.
 *
 * @param sdi The device instance.
 * @param prefix Log message prefix.
 *
 * @return SR_OK on success.
 */
SR_PRIV int std_session_send_df_end(const struct sr_dev_inst *sdi,
    const char *prefix);

/**
 * Standard sigrok's std_session_send_df_frame_begin - send DF_FRAME_BEGIN packet.
 *
 * This is the single canonical implementation for compat drivers. Previously
 * each driver defined its own local copy (fx2lafw, gwinstek-gds-800, hameg-hmo,
 * hantek-dso, hung-chang-dso-2100), but since SR_PRIV is empty on Windows
 * those were global symbols that caused multiple-definition link errors when
 * two such drivers were enabled simultaneously. Consolidating here removes
 * that hazard; call sites are unchanged (same 1-arg signature).
 *
 * @param sdi The device instance.
 *
 * @return SR_OK on success.
 */
SR_PRIV int std_session_send_df_frame_begin(const struct sr_dev_inst *sdi);

/**
 * Standard sigrok's std_config_list - handle SR_CONF_DEVICE_OPTIONS etc.
 *
 * @param key The config key.
 * @param data Output GVariant.
 * @param sdi The device instance (may be NULL for scan options).
 * @param cg The channel group (may be NULL).
 * @param scanopts Array of scan option keys.
 * @param num_scanopts Number of scan options.
 * @param drvopts Array of driver option keys.
 * @param num_drvopts Number of driver options.
 * @param devopts Array of device option keys.
 * @param num_devopts Number of device options.
 *
 * @return SR_OK on success, SR_ERR_NA if key not handled.
 */
SR_PRIV int std_config_list(uint32_t key, GVariant **data,
    const struct sr_dev_inst *sdi, const struct sr_channel_group *cg,
    const uint32_t scanopts[], size_t num_scanopts,
    const uint32_t drvopts[], size_t num_drvopts,
    const uint32_t devopts[], size_t num_devopts);

/**
 * Create a GVariant array of samplerates (for SR_CONF_SAMPLERATE).
 *
 * @param samplerates Array of samplerate values.
 * @param count Number of elements.
 *
 * @return GVariant containing the samplerate array.
 */
SR_PRIV GVariant *std_gvar_samplerates(const uint64_t samplerates[],
    size_t count);

/**
 * Create a GVariant array of samplerate steps.
 *
 * @param steps Array of step values.
 * @param count Number of elements.
 *
 * @return GVariant containing the steps array.
 */
SR_PRIV GVariant *std_gvar_samplerate_steps(const uint64_t steps[],
    size_t count);

/**
 * Create a GVariant array of int32 values.
 *
 * @param vals Array of int32 values.
 * @param count Number of elements.
 *
 * @return GVariant containing the array.
 */
SR_PRIV GVariant *std_gvar_array_i32(const int32_t vals[], size_t count);

/**
 * Create a GVariant array of uint32 values.
 *
 * @param vals Array of uint32 values.
 * @param count Number of elements.
 *
 * @return GVariant containing the array.
 */
SR_PRIV GVariant *std_gvar_array_u32(const uint32_t vals[], size_t count);

/**
 * Create a GVariant array of uint64 values.
 *
 * @param vals Array of uint64 values.
 * @param count Number of elements.
 *
 * @return GVariant containing the array.
 */
SR_PRIV GVariant *std_gvar_array_u64(const uint64_t vals[], size_t count);

/**
 * Create a GVariant min/max/step tuple for ranged values.
 *
 * @param min Minimum value.
 * @param max Maximum value.
 * @param step Step value.
 *
 * @return GVariant containing (min, max, step) tuple.
 */
SR_PRIV GVariant *std_gvar_min_max_step(double min, double max, double step);

/**
 * Create a GVariant for samplerate range with steps.
 *
 * @param min Minimum samplerate.
 * @param max Maximum samplerate.
 * @param steps Array of step values.
 * @param count Number of steps.
 *
 * @return GVariant containing the range specification.
 */
SR_PRIV GVariant *std_gvar_min_max_steps_uint64(uint64_t min, uint64_t max,
    const uint64_t steps[], size_t count);

/**
 * Create a GVariant tuple array from string array.
 *
 * @param strs Array of string pointers.
 * @param count Number of elements.
 *
 * @return GVariant containing the tuple array.
 */
SR_PRIV GVariant *std_gvar_tuple_array(const char *strs[], size_t count);

/**
 * Config get helper for uint64 indexed values.
 *
 * @param sdi The device instance.
 * @param key The config key.
 * @param data Output GVariant.
 * @param vals Array of uint64 values.
 * @param count Number of values.
 *
 * @return SR_OK on success, SR_ERR_ARG on invalid arguments.
 */
SR_PRIV int std_u64_idx(const struct sr_dev_inst *sdi, uint32_t key,
    GVariant **data, const uint64_t vals[], size_t count);

/**
 * Config get helper for string indexed values.
 *
 * @param sdi The device instance.
 * @param key The config key.
 * @param data Output GVariant.
 * @param strs Array of string pointers.
 * @param count Number of values.
 *
 * @return SR_OK on success, SR_ERR_ARG on invalid arguments.
 */
SR_PRIV int std_str_idx(const struct sr_dev_inst *sdi, uint32_t key,
    GVariant **data, const char *const strs[], size_t count);

/**
 * Config get helper for boolean indexed values.
 *
 * @param sdi The device instance.
 * @param key The config key.
 * @param data Output GVariant.
 * @param vals Array of boolean values.
 * @param count Number of values.
 *
 * @return SR_OK on success, SR_ERR_ARG on invalid arguments.
 */
SR_PRIV int std_bool_idx(const struct sr_dev_inst *sdi, uint32_t key,
    GVariant **data, const gboolean vals[], size_t count);

/**
 * Parse probe names from a comma-separated string.
 * Stub implementation.
 *
 * @param str Comma-separated probe names.
 * @param defaults Default probe names.
 * @param ch_max Maximum number of channels.
 * @param channels Array of channel pointers to set names on.
 */
SR_PRIV void sr_parse_probe_names(const char *str, const char *defaults[],
    int ch_max, struct sr_channel **channels);

/**
 * Soft trigger logic - simplified implementation for compat drivers.
 * PXView does not have full soft trigger support, so this provides
 * basic edge detection functionality.
 *
 * Full definitions of sr_trigger / sr_trigger_stage / sr_trigger_match
 * are provided here (matching standard sigrok's layout) so that compat
 * drivers which inspect trigger stages (e.g. chronovu-la, hantek-4032l,
 * kingst-la2016) compile even though sr_session_trigger_get() is stubbed
 * to return NULL and the trigger path is dead at runtime.
 *
 * struct sr_channel is already fully defined via libsigrok-internal.h
 * (included before this header by compat.h).
 */
struct sr_trigger_match {
    struct sr_channel *channel;
    int match;
};

struct sr_trigger_stage {
    int stage;
    GSList *matches;
};

struct sr_trigger {
    char *name;
    GSList *stages;
};

struct soft_trigger_logic {
    const struct sr_dev_inst *sdi;
    struct sr_trigger *trigger;
    uint64_t pre_trigger_samples;
    int cur_stage;
};

SR_PRIV struct soft_trigger_logic *soft_trigger_logic_new(
    const struct sr_dev_inst *sdi, struct sr_trigger *trigger,
    uint64_t pre_trigger_samples);
SR_PRIV void soft_trigger_logic_free(struct soft_trigger_logic *stl);
SR_PRIV int soft_trigger_logic_check(struct soft_trigger_logic *stl,
    uint8_t *buf, size_t buflen, int *pre_trigger_samples);
SR_PRIV void soft_trigger_logic_reset(struct soft_trigger_logic *stl);

/**
 * sr_session_trigger_get stub - PXView does not have trigger support.
 */
struct sr_session; /* Forward declaration */
SR_PRIV struct sr_trigger *sr_session_trigger_get(const struct sr_session *session);

/**
 * Check if a USB device matches the given manufacturer and product strings.
 * Stub implementation.
 *
 * @param dev The libusb device.
 * @param manufacturer Expected manufacturer string.
 * @param product Expected product string.
 *
 * @return TRUE if matches, FALSE otherwise.
 */
SR_PRIV gboolean usb_match_manuf_prod(libusb_device *dev,
    const char *manufacturer, const char *product);

/**
 * Get the USB port path of a device.
 * Stub implementation.
 *
 * @param dev The libusb device.
 * @param path Output buffer for the path string.
 * @param path_len Length of the output buffer.
 *
 * @return SR_OK on success, SR_ERR on failure.
 */
SR_PRIV int usb_get_port_path(libusb_device *dev, char *path, int path_len);

/**
 * Create a GVariant tuple of two double values.
 * Like std_gvar_tuple_u64() but for the double type.
 *
 * @param first First value.
 * @param second Second value.
 *
 * @return GVariant containing the (first, second) tuple.
 */
SR_PRIV GVariant *std_gvar_tuple_double(double first, double second);

/**
 * Find the index of a GVariant double tuple in an array of double pairs.
 *
 * The input GVariant is expected to be a (dd) tuple. The function searches
 * the provided array of {low, high} pairs for a matching entry.
 *
 * @param data The GVariant (dd) tuple to search for.
 * @param vals Array of double pairs (vals[count][2]) to search in.
 * @param count Number of pairs in vals.
 *
 * @return Index of the matching pair, or -1 if not found / on error.
 */
SR_PRIV int std_double_tuple_idx(GVariant *data, const double (*vals)[2],
    int count);

/**
 * Create a GVariant array of double-pair thresholds ("a(dd)").
 *
 * @param thresholds Array of double pairs (thresholds[count][2]). May be
 *                   NULL, in which case an empty array is returned.
 * @param count Number of threshold pairs.
 *
 * @return GVariant containing the thresholds array.
 */
SR_PRIV GVariant *std_gvar_thresholds(const double (*thresholds)[2],
    int count);

/**
 * Create a GVariant array of double-pair thresholds combining a
 * min/max/step range and optional explicit threshold presets.
 *
 * When @a thresholds is non-NULL, the provided pairs are used directly
 * (interpreted as a flat array: low0, high0, low1, high1, ...).
 * When @a thresholds is NULL and @a step > 0, single-value pairs of the
 * form (v, v) are generated for v from @a min to @a max in steps of
 * @a step.
 *
 * @param min Minimum threshold value.
 * @param max Maximum threshold value.
 * @param step Step between generated threshold values.
 * @param thresholds Optional flat array of threshold pairs (may be NULL).
 * @param count Number of threshold pairs (0 if thresholds is NULL).
 *
 * @return GVariant containing the thresholds array ("a(dd)").
 */
SR_PRIV GVariant *std_gvar_min_max_step_thresholds(double min, double max,
    double step, const double *thresholds, int count);

/**
 * Standard sigrok's std_opts_config_list - handle SR_CONF_DEVICE_OPTIONS
 * (and SR_CONF_SCAN_OPTIONS) for a driver that keeps its option keys in a
 * GSList.
 *
 * @param opts GSList of config option keys (stored via GUINT_TO_POINTER).
 * @param id The config key being queried.
 * @param data Output GVariant.
 * @param sdi The device instance (may be NULL for scan options).
 * @param cg The channel group (may be NULL).
 *
 * @return SR_OK on success, SR_ERR_NA if the key is not handled.
 */
SR_PRIV int std_opts_config_list(GSList *opts, int id, GVariant **data,
    const struct sr_dev_inst *sdi, const struct sr_channel_group *cg);

/**
 * GDestroyNotify callback wrapper for sr_usb_dev_inst_free().
 * Suitable for passing to g_slist_free_full() and similar APIs without
 * an explicit (GDestroyNotify) cast at the call site.
 *
 * @param data The sr_usb_dev_inst pointer to free (passed as void *).
 */
SR_PRIV void sr_usb_dev_inst_free_cb(void *data);

/**
 * Standard sigrok's sr_usb_open compat - open the libusb device handle.
 * PXView's libsigrok does not provide sr_usb_open(); this shim wraps
 * libusb_open() against the struct sr_usb_dev_inst fields (usb_dev and
 * devhdl) populated at scan time.
 *
 * @param usb_ctx libusb context (unused; kept for API compatibility).
 * @param usb The USB device instance to open. usb->usb_dev must be set;
 *            on success usb->devhdl is populated.
 *
 * @return SR_OK on success, SR_ERR on failure.
 */
SR_PRIV int sr_usb_open(libusb_context *usb_ctx, struct sr_usb_dev_inst *usb);

/**
 * Standard sigrok's sr_usb_close compat - close the libusb device handle.
 * PXView's libsigrok does not provide sr_usb_close(); this shim wraps
 * libusb_close() and clears usb->devhdl. Callers are responsible for
 * releasing any claimed interfaces before calling this.
 *
 * @param usb The USB device instance whose handle should be closed.
 *
 * @return SR_OK on success, SR_ERR_ARG on invalid argument.
 */
SR_PRIV int sr_usb_close(struct sr_usb_dev_inst *usb);

#endif
