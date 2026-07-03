/*
 * srstd_pxview_glue.h — Glue functions bridging PXView to upstream libsigrokstd.
 *
 * === Purpose ===
 * PXView's libsigrok.h and upstream libsigrokstd's libsigrok.h (consumed via
 * srstd_rename.h) cannot be included in the same translation unit — the
 * srstd_rename.h macros rename sr_* struct tags / functions / typedefs, which
 * would corrupt PXView's own sr_* references. This header declares void*
 * wrapper functions whose implementations live in srstd_pxview_glue.c
 * (compiled WITH -include srstd_rename.h inside the libsigrokstd target).
 * PXView C++ code calls these wrappers without ever including srstd.h.
 *
 * Three groups of wrappers:
 *
 * 1. Library init/exit (called by SigSession::init/uninit):
 *      srstd_pxview_init_shared  — wraps srstd_init_shared (libusb_context
 *                                  sharing + slogic driver registration)
 *      srstd_pxview_exit         — wraps srstd_exit with libusb_ctx=NULL
 *                                  safety (prevents releasing the shared
 *                                  PXView libusb_context)
 *
 * 2. Device config/acquisition dispatch (called by DeviceAgent for
 *    LIB_SRSTD devices — Task 7/8):
 *      srstd_glue_dev_config_get / _set / _list
 *      srstd_glue_acquisition_start / _stop
 *    Task 8 wires these to real srstd_config_get/set /
 *    srstd_dev_acquisition_start/stop calls operating on the active sdi
 *    tracked by the glue layer.
 *
 * 3. Device scan + datafeed callback (Task 8 Part 3):
 *      srstd_glue_scan_devices          — scan upstream drivers
 *      srstd_glue_get_scanned_device_name — get name by index
 *      srstd_glue_open_scanned_device   — open + set active + create session
 *      srstd_glue_close_active_device   — close + destroy session
 *      srstd_glue_get_active_device_name — for DeviceAgent::update()
 *      srstd_glue_set_datafeed_callback — register PXView datafeed callback
 *    Plus handle macros to distinguish srstd device handles from PXView's.
 *
 * === Why void* for all struct pointers ===
 * The C token "struct sr_channel" resolves to PXView's struct inside
 * DeviceAgent.cpp (compiled without srstd_rename.h) but to upstream's
 * struct srstd_channel inside srstd_pxview_glue.c (compiled with
 * -include srstd_rename.h). Declaring parameters as void* sidesteps this
 * token-resolution mismatch so the same extern "C" prototype compiles
 * cleanly in both translation units.
 *
 * === Include safety ===
 * This header only pulls in <glib.h> (for GVariant). It does NOT include
 * any libsigrok.h, so it is safe to include from PXView C++ translation
 * units that already include PXView's libsigrok.h.
 */

#ifndef SRSTD_PXVIEW_GLUE_H_
#define SRSTD_PXVIEW_GLUE_H_

#include <glib.h>      /* GVariant */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SR_ERR_NA = 6 in BOTH PXView's libsigrok.h and upstream libsigrokstd's
 * libsigrok.h (enum values are not renamed by srstd_rename.h — only struct
 * tags, function names, and typedefs are). Redefined here as a macro so
 * this header is self-contained without pulling in either libsigrok.h.
 */
#define SRSTD_GLUE_ERR_NA 6

/* ===== srstd device handle tagging (Task 8 Part 3 device-scan merge) =====
 *
 * PXView device handles are `unsigned long long` (ds_device_handle). To
 * distinguish srstd devices from PXView devices in the merged device list,
 * srstd handles set the highest bit (0x8000000000000000). PXView handles
 * are either small indices or user-space pointers (high 16 bits zero on
 * Windows x64), so the high bit is always clear for PXView handles.
 *
 * Usage in SigSession::set_default_device:
 *   srstd_glue_scan_devices() returns count N
 *   for i in 0..N: array[i].handle = SRSTD_MAKE_HANDLE(i)
 *   srstd_glue_get_scanned_device_name(i, array[i].name, sizeof(name))
 *
 * Usage in SigSession::set_device:
 *   if (SRSTD_IS_HANDLE(dev_handle)) {
 *       int idx = SRSTD_HANDLE_INDEX(dev_handle);
 *       srstd_glue_open_scanned_device(idx);
 *   } else {
 *       ds_active_device(dev_handle);
 *   }
 */
#define SRSTD_HANDLE_BIT        0x8000000000000000ULL
#define SRSTD_MAKE_HANDLE(idx)  (SRSTD_HANDLE_BIT | (unsigned long long)(idx))
#define SRSTD_IS_HANDLE(h)      (((unsigned long long)(h) & SRSTD_HANDLE_BIT) != 0)
#define SRSTD_HANDLE_INDEX(h)   ((int)((unsigned long long)(h) & ~SRSTD_HANDLE_BIT))

/* ===== Library init/exit wrappers =====
 *
 * srstd_pxview_init_shared — create an upstream srstd_context that shares
 * PXView's already-created libusb_context, and register the slogic driver.
 *
 * Parameters:
 *   ctx_out          - output: receives the new srstd_context* (opaque)
 *   shared_usb_ctx   - PXView's existing libusb_context* (opaque)
 *
 * Returns SR_OK (0) on success, negative SR_ERR_* on failure.
 */
int  srstd_pxview_init_shared(void **ctx_out, void *shared_usb_ctx);

/*
 * srstd_pxview_exit — tear down an upstream srstd_context created by
 * srstd_pxview_init_shared. Internally sets libusb_ctx=NULL before calling
 * srstd_exit so the shared PXView libusb_context is NOT released.
 */
void srstd_pxview_exit(void *ctx);

/* ===== Driver-name lookup (Task 5) =====
 *
 * srstd_glue_get_driver_names — get all registered srstd driver names.
 *
 * Returns a NULL-terminated array of C strings (const char*), each being
 * a driver's .name field (e.g. "sipeed-slogic-analyzer", "fx2lafw",
 * "saleae-logic16"). The array is owned by the glue layer and valid
 * until srstd_pxview_exit. Caller must NOT free.
 *
 * Returns NULL if glue layer not initialized (srstd_pxview_init_shared
 * not called or failed).
 *
 * @param count_out  optional output: number of driver names (excluding
 *                   NULL terminator). Pass NULL if not needed.
 * @return NULL-terminated array of const char*, or NULL if not initialized.
 */
const char** srstd_glue_get_driver_names(int* count_out);

/* ===== Device config/acquisition dispatch wrappers =====
 *
 * These mirror PXView's ds_get_actived_device_config / ds_set_actived_device_config
 * / ds_get_actived_device_config_list / ds_start_collect / ds_stop_collect but
 * operate on an explicit upstream sdi instead of PXView's "actived device"
 * global. DeviceAgent calls them when device_lib() == LIB_SRSTD.
 *
 * Parameters:
 *   sdi  - upstream srstd_dev_inst* (opaque; NULL until Task 8 wires scan/open)
 *   ch   - upstream srstd_channel* (opaque; pass-through from DeviceAgent,
 *          currently NULL since PXView channels are not yet bridged to srstd)
 *   cg   - upstream srstd_channel_group* (opaque; same caveat)
 *   key  - SR_CONF_* config key (same numeric values in both libsigrok forks
 *          for the keys DeviceAgent uses)
 *   data - GVariant* (get/list: output, caller unrefs; set: input, caller
 *          retains ownership)
 *
 * Returns SR_OK (0) on success, SR_ERR_* (<0) on failure. Stubs return
 * SRSTD_GLUE_ERR_NA. Task 8/9 replaces them with real srstd_config_get/set
 * / srstd_dev_acquisition_* calls.
 */
int srstd_glue_dev_config_get(void *sdi, void *ch, void *cg,
                              int key, GVariant **data);
int srstd_glue_dev_config_set(void *sdi, void *ch, void *cg,
                              int key, GVariant *data);
int srstd_glue_dev_config_list(void *sdi, void *cg,
                               int key, GVariant **data);
int srstd_glue_acquisition_start(void *sdi);
int srstd_glue_acquisition_stop(void *sdi);

/* ===== Device scan + datafeed callback (Task 8 Part 3) =====
 *
 * These functions wire the upstream libsigrokstd session/datafeed pipeline
 * to PXView's DataFeedParser. The glue layer tracks active sdi + session
 * internally (mirroring PXView's "actived device" pattern), so DeviceAgent
 * calls srstd_glue_* with NULL sdi and the glue layer uses its internal
 * active sdi.
 *
 * Lifecycle:
 *   1. srstd_pxview_init_shared OK → g_ctx stored
 *   2. srstd_glue_set_datafeed_callback — register PXView callback (once)
 *   3. srstd_glue_scan_devices — scan upstream drivers, populate sdi list
 *   4. srstd_glue_get_scanned_device_name — fill merged device list
 *   5. srstd_glue_open_scanned_device — open + create session + register
 *      upstream datafeed callback (wrapper converts packets to PXView format
 *      via srstd_packet_to_pxview, then calls PXView callback)
 *   6. srstd_glue_acquisition_start/stop — start/stop upstream session
 *   7. srstd_glue_close_active_device — close + destroy session
 *   8. srstd_glue_get_active_device_name — DeviceAgent::update() probes this
 *      first; returns SR_ERR when no srstd device is active so DeviceAgent
 *      falls back to ds_get_actived_device_info.
 */

/*
 * srstd_glue_datafeed_cb_t — PXView datafeed callback type.
 *
 * ABI-compatible with PXView's ds_datafeed_callback_ex_t (both take three
 * pointer-sized args). Uses const void* to avoid pulling libsigrok.h into
 * this header. PXView registers &DataFeedParser::data_feed_callback_ex via
 * a reinterpret_cast.
 */
typedef void (*srstd_glue_datafeed_cb_t)(const void *sdi,
                                          const void *packet,
                                          void *user_data);

/*
 * srstd_glue_set_datafeed_callback — register PXView's datafeed callback.
 * The glue layer wraps it in an upstream sr_datafeed_callback that converts
 * upstream packets to PXView format via srstd_packet_to_pxview() before
 * forwarding. Call once during SigSession::init (after
 * srstd_pxview_init_shared succeeds). Pass cb=NULL to unregister.
 */
void srstd_glue_set_datafeed_callback(srstd_glue_datafeed_cb_t cb,
                                       void *user_data);

/*
 * srstd_glue_scan_devices — scan all registered upstream drivers
 * (currently sipeed-slogic-analyzer). Stores found sdi list internally.
 * Returns device count (0 if no devices or srstd not initialized).
 * Safe to call multiple times (frees previous scan results first).
 */
int srstd_glue_scan_devices(void);

/*
 * srstd_glue_get_scanned_device_name — fill buf with the human-readable
 * device name (e.g., "Sipeed SLogic"). Returns SR_OK on success.
 */
int srstd_glue_get_scanned_device_name(int index, char *buf, int buf_size);

/*
 * srstd_glue_open_scanned_device — open the device at the given scan index,
 * set it as active, create an upstream session (if not yet created), add
 * the device to the session, and register the upstream datafeed callback
 * wrapper. Closes any previously active srstd device first.
 * Returns SR_OK on success.
 */
int srstd_glue_open_scanned_device(int index);

/*
 * srstd_glue_close_active_device — close the active srstd device, stop
 * acquisition if running, destroy the upstream session. Safe to call when
 * no srstd device is active (no-op).
 */
int srstd_glue_close_active_device(void);

/*
 * srstd_glue_get_active_device_name — fill name/driver buffers for the
 * active srstd device. Returns SR_OK if a srstd device is active, SR_ERR
 * otherwise (so DeviceAgent::update falls back to ds_get_actived_device_info).
 */
int srstd_glue_get_active_device_name(char *name_buf, int name_buf_size,
                                       char *driver_buf, int driver_buf_size);

/*
 * srstd_glue_get_active_handle — returns the tagged handle of the active
 * srstd device (SRSTD_MAKE_HANDLE(index)), or 0 if none active.
 */
unsigned long long srstd_glue_get_active_handle(void);

/*
 * srstd_glue_get_active_sdi_shadow — returns the PXView sr_dev_inst* shadow
 * for the active srstd device (or NULL if none active). The shadow is
 * allocated by srstd_bridge_pxview_sdi_alloc and filled by srstd_sdi_to_pxview.
 * DeviceAgent::update() sets _di to this so inst() returns a valid pointer,
 * and DeviceAgent::get_channels() reads channels from it via
 * srstd_glue_get_active_channels().
 */
void *srstd_glue_get_active_sdi_shadow(void);

/*
 * srstd_glue_get_active_channels — returns the channels GSList* from the
 * active srstd device's shadow sdi. Returns NULL if no srstd device active.
 * The channels are PXView sr_channel structs converted from upstream by
 * srstd_sdi_to_pxview(). DeviceAgent::get_channels() calls this for
 * LIB_SRSTD devices instead of ds_get_actived_device_channels().
 */
GSList *srstd_glue_get_active_channels(void);

/*
 * srstd_glue_is_collecting — returns 1 if the srstd session thread is
 * running (acquisition in progress), 0 otherwise. DeviceAgent::is_collecting()
 * calls this for LIB_SRSTD devices instead of ds_is_collecting().
 */
int srstd_glue_is_collecting(void);

/* ===== Trigger synchronization (Task 9) =====
 *
 * These functions wire PXView's TriggerConfig to the upstream session's
 * software trigger (soft_trigger_logic). Called by
 * SigSession::sync_trigger_to_libsigrok() when device_lib() == LIB_SRSTD.
 *
 * Flow:
 *   1. srstd_glue_trigger_create        — allocate upstream sr_trigger
 *   2. pxview_trigger_to_srstd           — fill stages/matches (bridge.c)
 *   3. srstd_glue_trigger_fix_channels   — replace tagged channel pointers
 *      with real upstream sr_channel* from the active sdi's channel list
 *   4. srstd_glue_session_trigger_set    — set trigger on the upstream
 *      session (takes ownership of the trigger; frees it on session close)
 *
 * Trigger lifecycle:
 *   srstd_glue_session_trigger_set takes ownership of the trigger pointer.
 *   The glue layer frees the previous trigger (if any) when a new one is
 *   set, and frees the active trigger when the session is closed
 *   (srstd_glue_close_active_device) or the library exits (srstd_pxview_exit).
 *   The caller must NOT call srstd_glue_trigger_free after a successful
 *   srstd_glue_session_trigger_set — the glue layer owns it. If
 *   srstd_glue_session_trigger_set fails (e.g., no active session), it
 *   frees the trigger internally so the caller still doesn't need to.
 *
 * === Channel pointer fixup ===
 *
 * pxview_trigger_to_srstd (Task 4) creates matches with match->channel set
 * to a tagged pointer encoding the channel index: (void*)(intptr_t)(index+1).
 * This is because 'X' tokens in the value string are skipped (no match
 * created), so match position in the list ≠ channel index. fix_channels
 * decodes the tag, looks up the real upstream sr_channel* from
 * g_active_sdi->channels by index, and replaces the tagged pointer with
 * the real pointer. After fix_channels, all match->channel pointers are
 * either real upstream sr_channel* (with correct index/enabled fields for
 * soft_trigger_logic_check) or NULL (if the channel index was out of range).
 */

/*
 * srstd_glue_trigger_create — allocate an upstream sr_trigger (zero-init,
 * empty stages list). Returns NULL on allocation failure.
 */
void *srstd_glue_trigger_create(void);

/*
 * srstd_glue_trigger_free — free an upstream sr_trigger and all its
 * stages/matches (calls upstream sr_trigger_free, renamed to
 * srstd_trigger_free). Safe to call with NULL (no-op).
 */
void srstd_glue_trigger_free(void *trigger);

/*
 * srstd_glue_trigger_fix_channels — replace tagged channel pointers in
 * trigger->stages->matches with real upstream sr_channel* from the active
 * sdi's channel list. Must be called AFTER pxview_trigger_to_srstd and
 * BEFORE srstd_glue_session_trigger_set. No-op if no active sdi.
 */
void srstd_glue_trigger_fix_channels(void *trigger);

/*
 * srstd_glue_session_trigger_set — set the trigger on the upstream session.
 * Takes ownership of trigger (caller must NOT free it). Frees the previous
 * trigger if one was set. Returns SR_OK (0) on success, SR_ERR if no
 * active session (trigger is freed internally in that case).
 */
int srstd_glue_session_trigger_set(void *trigger);

#ifdef __cplusplus
}
#endif

#endif /* SRSTD_PXVIEW_GLUE_H_ */
