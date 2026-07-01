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
 * Create a GVariant array of uint64 tuples ("a(tt)") from a uint64_t[][2]
 * array. Canonical libsigrok signature (libsigrok-internal.h:1942,
 * std.c:595). Used by SCPI oscilloscope drivers (rigol-ds, siglent-sds,
 * hameg-hmo, hantek-6xxx, kecheng-kc-330b, ...) to publish their
 * vdivs/timebases/sample-intervals tables.
 *
 * The previous PXView compat signature took a string array and produced
 * "as"; that was a non-canonical mis-naming. Drivers that need a string
 * array should call std_gvar_array_str() (declared below) instead.
 *
 * @param a Array of uint64_t pairs (a[n][2]).
 * @param n Number of pairs.
 *
 * @return GVariant containing the "a(tt)" tuple array.
 */
SR_PRIV GVariant *std_gvar_tuple_array(const uint64_t a[][2],
	unsigned int n);

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

/**
 * @name Standard sigrok log levels
 *
 * PXView's libsigrok.h does not define the SR_LOG_* enum (it uses xlog
 * internally instead). Standard sigrok drivers (kingst-la2016, uni-t-ut181a,
 * uni-t-ut32x, testo, cem-dt-885x, colead-slm, rdtech-tc, serial-dmm,
 * hantek-dso) use these constants for conditional debug output, typically
 * in the form `if (sr_log_loglevel_get() >= SR_LOG_SPEW) { ... }`.
 *
 * Defined here with #ifndef guards so that drivers or headers with their
 * own fallback definitions don't conflict. Canonical values from upstream
 * libsigrok.h:
 *   SR_LOG_NONE = 0, SR_LOG_ERR = 1, SR_LOG_WARN = 2,
 *   SR_LOG_INFO = 3, SR_LOG_DBG = 4, SR_LOG_SPEW = 5
 */
#ifndef SR_LOG_NONE
#define SR_LOG_NONE  0
#endif
#ifndef SR_LOG_ERR
#define SR_LOG_ERR   1
#endif
#ifndef SR_LOG_WARN
#define SR_LOG_WARN  2
#endif
#ifndef SR_LOG_INFO
#define SR_LOG_INFO  3
#endif
#ifndef SR_LOG_DBG
#define SR_LOG_DBG   4
#endif
#ifndef SR_LOG_SPEW
#define SR_LOG_SPEW  5
#endif

/**
 * Standard sigrok resource type constants.
 *
 * PXView's libsigrok (based on upstream 0.2.0) lacks the sr_resource API
 * (introduced upstream 2015-10). Standard sigrok drivers that upload
 * firmware (kingst-la2016, lecroy-logicstudio, sysclk-lwla) use
 * SR_RESOURCE_FIRMWARE together with the sr_resource_open/read/close
 * functions declared below.
 *
 * Guarded with #ifndef so that drivers with their own local definitions
 * (e.g. saleae-logic16's `#define SR_RESOURCE_FIRMWARE 1` in protocol.h)
 * don't get a redefinition warning.
 */
#ifndef SR_RESOURCE_FIRMWARE
#define SR_RESOURCE_FIRMWARE 1
#endif

/**
 * Standard sigrok resource descriptor.
 *
 * PXView's libsigrok does not define struct sr_resource. Drivers that use
 * the sr_resource_open/read/close API need this struct to hold the file
 * handle and size across the open/read/close sequence. The layout matches
 * upstream libsigrok.h exactly so that drivers using `res->size`,
 * `res->handle`, `res->type` compile unchanged.
 *
 * Guarded so that future drivers providing their own definition can
 * suppress this one by #defining SR_RESOURCE_STRUCT_DEFINED before
 * including compat.h.
 */
#ifndef SR_RESOURCE_STRUCT_DEFINED
#define SR_RESOURCE_STRUCT_DEFINED
struct sr_resource {
    /** Size of resource in bytes; set by sr_resource_open(). */
    uint64_t size;
    /** File handle or equivalent; set by sr_resource_open(). */
    void *handle;
    /** Resource type (SR_RESOURCE_FIRMWARE, ...). */
    int type;
};
#endif

/**
 * Standard sigrok's sr_resource API - open/read/close firmware resources.
 *
 * PXView's libsigrok does not provide these functions. Previously each
 * driver that needed firmware upload rolled its own local copy
 * (saleae-logic16 with a non-canonical `void *resource` signature, plus
 * asix-sigma and saleae-logic-pro with local `sr_resource_load` variants).
 * This centralized implementation uses the canonical upstream signatures
 * so that drivers (kingst-la2016, lecroy-logicstudio, sysclk-lwla) which
 * already call sr_resource_open/read/close with `struct sr_resource *`
 * link without modification.
 *
 * The implementation opens files from the DS_RES_PATH directory (matching
 * the local copies it replaces) using fopen/fread/fclose.
 *
 * Declared inside an #ifndef guard so that drivers which still carry their
 * own local declarations (saleae-logic16, OFF in the default build) can
 * suppress these by #defining COMPAT_SR_RESOURCE_DECLARED before including
 * compat.h. Task 13 will remove those local copies.
 */
#ifndef COMPAT_SR_RESOURCE_DECLARED
#define COMPAT_SR_RESOURCE_DECLARED
SR_PRIV int sr_resource_open(struct sr_context *sr_ctx,
        struct sr_resource *res, int type, const char *name);
SR_PRIV int sr_resource_close(struct sr_context *sr_ctx,
        struct sr_resource *res);
SR_PRIV gssize sr_resource_read(struct sr_context *sr_ctx,
        const struct sr_resource *res, void *buf, size_t count);
#endif

/**
 * Get the current libsigrok log level.
 *
 * PXView's libsigrok does not expose the log level (it stores it in a
 * static variable inside log.c). This stub returns SR_LOG_DBG (4) so that
 * conditional spew output (level 5) is suppressed while regular debug
 * output is visible, matching the behaviour drivers expect when no
 * verbose-logging flag has been set.
 *
 * @return The current log level (always SR_LOG_DBG).
 */
SR_PRIV int sr_log_loglevel_get(void);

/**
 * Standard sigrok's sr_hexdump_new/free - debug hex dump helpers.
 *
 * PXView's libsigrok does not provide these. Standard sigrok drivers
 * (kingst-la2016, uni-t-ut181a, uni-t-ut32x, testo, uni-t-dmm,
 * serial-dmm/bm52x, colead-slm, cem-dt-885x) use them to format raw byte
 * buffers for debug logging. This centralized implementation matches the
 * canonical upstream signature (GString-returning).
 *
 * The declarations are NOT inside a #ifndef guard because some drivers
 * (atorch, asix-omega-rtm-cli, microchip-pickit2, rdtech-tc) redirect
 * calls to their own helpers via `#define sr_hexdump_new(...)`. Those
 * #defines take effect in the driver translation unit AFTER compat.h is
 * included, so the centralized declaration is seen first (as a plain
 * function prototype) and the later #define only affects call sites.
 * The centralized definition in compat_helpers.c lives in its own
 * translation unit where the macro is not visible, so it compiles
 * normally; the resulting unused extern symbol is harmless.
 *
 * @param data Pointer to the byte sequence to print.
 * @param len Number of bytes to print.
 *
 * @return Newly allocated GString containing the hex dump, or NULL on
 *         error. Caller must free with sr_hexdump_free().
 */
SR_PRIV GString *sr_hexdump_new(const uint8_t *data, const size_t len);

/**
 * Free a hex dump GString created by sr_hexdump_new().
 *
 * @param s The GString to release (may be NULL).
 */
SR_PRIV void sr_hexdump_free(GString *s);

/*
 * Standard sigrok's sr_strerror - return human-readable error string.
 *
 * Canonical libsigrok (error.c:53) provides this as SR_API. PXView's
 * libsigrok does not expose it. Multiple standard drivers (fluke-45,
 * rigol-ds, siglent-sds, atten-pps3xxx, gwinstek-gds-800, motech-lps-30x,
 * scpi-dmm) log `sr_strerror(ret)` after a failed API call. This compact
 * implementation covers the SR_* codes that PXView defines plus the few
 * additional standard sigrok codes the compat layer emulates; unknown
 * codes return "unknown error" exactly like the upstream version.
 *
 * Declared with #ifndef guard so that future additions to PXView's
 * libsigrok.h (or driver-local fallbacks) don't conflict.
 */
#ifndef COMPAT_SR_STRERROR_DECLARED
#define COMPAT_SR_STRERROR_DECLARED
const char *sr_strerror(int error_code);
#endif

/*
 * Standard sigrok's std_gvar_array_str - build a GVariant "as" array
 * of strings. Canonical libsigrok (libsigrok-internal.h:1956, std.c:747).
 *
 * PXView's compat layer previously named this std_gvar_tuple_array and
 * gave it the wrong signature (string array). That collided with the
 * canonical std_gvar_tuple_array which takes uint64_t[][2] (see below).
 * This declaration provides the canonical string-array helper so that
 * drivers wanting a string array ("as") can call it by its standard
 * sigrok name; the driver-fixing task redirects the previously-misnamed
 * string callers (juntek-jds6600, arachnid-labs-re-load-pro,
 * siglent-sdl10x0) to this function.
 */
#ifndef COMPAT_STD_GVAR_ARRAY_STR_DECLARED
#define COMPAT_STD_GVAR_ARRAY_STR_DECLARED
SR_PRIV GVariant *std_gvar_array_str(const char *a[], unsigned int n);
#endif

/*
 * Standard sigrok's std_str_idx / std_u64_idx - lookup a GVariant value's
 * index in an array. Canonical libsigrok (libsigrok-internal.h:1960/1961,
 * std.c): both take (GVariant *data, const T a[], unsigned int n) and
 * return the matching index, or -1 on no match / invalid argument.
 *
 * PXView's compat layer previously declared these with a 5-argument
 * config-get signature (sdi/key/data/vals/count) that no driver uses.
 * All call sites in the tree (rigol-ds, siglent-sds, hantek-6xxx, plus
 * the local_ prefixed copies in many other drivers) use the 3-argument
 * canonical form. Grep across libsigrok/ confirmed: zero callers use
 * the 5-argument form. The signatures below match canonical sigrok
 * exactly so drivers compile unchanged.
 */
#ifndef COMPAT_STD_IDX_DECLARED
#define COMPAT_STD_IDX_DECLARED
SR_PRIV int std_str_idx(GVariant *data, const char *a[], unsigned int n);
SR_PRIV int std_u64_idx(GVariant *data, const uint64_t a[], unsigned int n);
#endif

/*
 * Standard sigrok's std_u64_tuple_idx / std_cg_idx.
 *
 * std_u64_tuple_idx (libsigrok-internal.h:1967, std.c:867): given a
 * GVariant "(tt)" tuple and an array of uint64_t pairs, return the
 * index of the matching pair, or -1.
 *
 * std_cg_idx (libsigrok-internal.h:1971, std.c:906): given a channel
 * group pointer and an array of channel group pointers, return the
 * index of the matching pointer, or -1. Used by SCPI oscilloscope
 * drivers (hameg-hmo, lecroy-xstream, rigol-ds, siglent-sds) to map
 * a cg back to its analog/digital channel-group index.
 *
 * Guarded so that future driver-local fallbacks don't conflict.
 */
#ifndef COMPAT_STD_TUPLE_CG_IDX_DECLARED
#define COMPAT_STD_TUPLE_CG_IDX_DECLARED
SR_PRIV int std_u64_tuple_idx(GVariant *data, const uint64_t a[][2],
	unsigned int n);
SR_PRIV int std_cg_idx(const struct sr_channel_group *cg,
	struct sr_channel_group *a[], unsigned int n);
#endif

/*
 * Standard sigrok's std_dev_clear_with_callback + std_dev_clear_callback.
 *
 * Canonical libsigrok (libsigrok-internal.h:1907/1924, std.c:405):
 *   typedef void (*std_dev_clear_callback)(void *priv);
 *   int std_dev_clear_with_callback(const struct sr_dev_driver *driver,
 *       std_dev_clear_callback clear_private);
 *
 * Iterates driver->context (PXView: driver->priv via compat_drv_context)
 * instance list; for each sdi calls dev_close() if active, then invokes
 * clear_private(sdi->priv) if non-NULL, frees sdi->priv, and frees the
 * sdi itself. PXView's compat layer previously only provided the 2-arg
 * std_dev_clear; several compat drivers (asix-sigma, hameg-hmo,
 * lecroy-xstream, juntek-jds6600, atorch, ikalogic-scanalogic2, ...)
 * call std_dev_clear_with_callback to release per-device resources
 * before the sdi is torn down.
 *
 * Note: the typedef's actual canonical signature is
 * `void (*)(void *priv)` (NOT the int-returning form mentioned in some
 * task notes). This matches upstream libsigrok-internal.h:1907 exactly.
 */
#ifndef COMPAT_STD_DEV_CLEAR_CALLBACK_DECLARED
#define COMPAT_STD_DEV_CLEAR_CALLBACK_DECLARED
typedef void (*std_dev_clear_callback)(void *priv);
#endif

#ifndef COMPAT_STD_DEV_CLEAR_WITH_CB_DECLARED
#define COMPAT_STD_DEV_CLEAR_WITH_CB_DECLARED
SR_PRIV int std_dev_clear_with_callback(const struct sr_dev_driver *driver,
	std_dev_clear_callback clear_private);
#endif

/*
 * Standard sigrok's ASCII numeric parsers - locale-independent.
 *
 * Canonical libsigrok (strutil.c): sr_atol/sr_atoi/sr_atof_ascii parse
 * base-10 numeric strings strictly (whole string must be a valid number)
 * and return SR_OK/SR_ERR. PXView's libsigrok does not provide them.
 * SCPI/DMM drivers (rigol-ds, siglent-sds, fluke-45, gwinstek-gds-800,
 * agilent-dmm, ...) use them to parse instrument responses without
 * being affected by the user's locale (g_ascii_strtod is C-locale).
 *
 * Implementation mirrors canonical strutil.c with PXView's SR_OK/SR_ERR
 * (positive) error codes.
 */
#ifndef COMPAT_SR_ATOI_DECLARED
#define COMPAT_SR_ATOI_DECLARED
SR_PRIV int sr_atol(const char *str, long *ret);
SR_PRIV int sr_atoi(const char *str, int *ret);
SR_PRIV int sr_atof_ascii(const char *str, float *ret);
#endif

#endif
