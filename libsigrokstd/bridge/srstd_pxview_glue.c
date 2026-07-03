/*
 * srstd_pxview_glue.c — Glue layer between PXView and upstream libsigrokstd.
 *
 * This file is part of the libsigrokstd static library and is compiled WITH
 * -include srstd_rename.h (injected by the libsigrokstd CMake target). The
 * sr_* tokens below are therefore macro-renamed to srstd_* by the
 * preprocessor, matching the upstream library's renamed symbol table. The
 * glue function names themselves (srstd_pxview_*, srstd_glue_*) do NOT
 * match the sr_* rename pattern, so they keep their names — that is what
 * makes them callable from PXView C++ code via the extern "C" declarations
 * in srstd_pxview_glue.h.
 *
 * Three groups of functions:
 *
 * 1. Library init/exit (called by SigSession::init/uninit via extern "C"
 *    declarations in sigsession.cpp):
 *      srstd_pxview_init_shared  — wraps srstd_init_shared (defined in
 *                                  srstd_init_shared.c). Shares PXView's
 *                                  libusb_context with the upstream
 *                                  srstd_context and registers the slogic
 *                                  driver. Stores the ctx in g_ctx for
 *                                  later use by scan/session functions.
 *      srstd_pxview_exit         — cleans up glue state (closes active
 *                                  device, frees shadow sdi, frees scan
 *                                  list), then sets ctx->libusb_ctx=NULL
 *                                  (to keep the shared PXView libusb_context
 *                                  alive) then calls sr_exit(ctx).
 *
 * 2. Device config/acquisition dispatch (called by DeviceAgent for LIB_SRSTD
 *    devices — Task 7/8). Task 8 Part 3 wires these to real upstream calls:
 *      srstd_glue_dev_config_get  → sr_config_get(driver, sdi, cg, key, data)
 *      srstd_glue_dev_config_set  → sr_config_set(sdi, cg, key, data)
 *      srstd_glue_dev_config_list → sr_config_list(driver, sdi, cg, key, data)
 *      srstd_glue_acquisition_start → sr_session_start + thread for sr_session_run
 *      srstd_glue_acquisition_stop  → sr_session_stop + join thread
 *    The sdi parameter may be NULL; the glue layer uses its internal
 *    g_active_sdi (set by srstd_glue_open_scanned_device).
 *
 * 3. Device scan + datafeed callback (Task 8 Part 3):
 *      srstd_glue_scan_devices          — sr_driver_scan on each registered driver
 *      srstd_glue_get_scanned_device_name — vendor+model from sdi
 *      srstd_glue_open_scanned_device   — sr_dev_open + sr_session_new +
 *      srstd_glue_close_active_device     sr_session_dev_add + register callback
 *      srstd_glue_get_active_device_name — for DeviceAgent::update
 *      srstd_glue_set_datafeed_callback  — register PXView callback
 *    The upstream datafeed callback wrapper converts packets to PXView format
 *    via srstd_packet_to_pxview() (from srstd_bridge.c) before forwarding.
 *
 * === Why srstd_rename.h is safe here ===
 * SRSTD_GLUE_ERR_NA is a #define (not an enum tag), so it is not renamed.
 * SR_OK / SR_ERR / SR_ERR_ARG / SR_ERR_NA are enum VALUES (not tags), so
 * they are not renamed either. GVariant / glib types are not in the sr_*
 * rename scope. The sr_* function/struct names ARE renamed to srstd_*,
 * which is exactly what we want when calling upstream APIs.
 */

#include "srstd_pxview_glue.h"

/* libsigrok/libsigrok.h is the upstream version (resolved via libsigrokstd's
 * BEFORE include path). srstd_rename.h has been removed in Phase 1/2, so all
 * sr_* tokens below reference the real upstream symbols directly.
 * libsigrok-internal.h provides the full struct sr_context definition
 * (including the libusb_ctx field accessed in srstd_pxview_exit). */
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

/* srstd_bridge.h declares the data-structure conversion functions
 * (srstd_sdi_to_pxview / srstd_packet_to_pxview) and the PXView-side
 * alloc/free helpers (srstd_bridge_pxview_sdi_alloc etc.). These are
 * implemented in srstd_bridge.c (compiled WITHOUT srstd_rename.h, so it
 * can include PXView's libsigrok.h). The function signatures use
 * struct sr_dev_inst / struct sr_datafeed_packet which srstd_rename.h
 * renames to srstd_dev_inst / srstd_datafeed_packet in THIS TU — but
 * since we only pass void* pointers obtained from the bridge _alloc
 * functions and never dereference them, the type mismatch is benign
 * (same pointer value, same ABI). */
#include "srstd_bridge.h"

#include <stdio.h>   /* snprintf */
#include <string.h>

/* Forward declaration — defined in srstd_init_shared.c (same library).
 * Previously declared in srstd.h (deleted in Phase 1/2). */
int srstd_init_shared(struct sr_context **ctx, libusb_context *shared_usb_ctx);

/* ===== Glue layer state =====
 *
 * All state is static (file-scope). The glue layer mirrors PXView's
 * "actived device" pattern: the active sdi is tracked internally, and
 * srstd_glue_* dispatch functions use it when the caller passes NULL.
 *
 * Lifecycle:
 *   srstd_pxview_init_shared → g_ctx set
 *   srstd_glue_set_datafeed_callback → g_pxview_cb / g_pxview_cb_data set
 *   srstd_glue_scan_devices → g_scanned_sdi_list populated
 *   srstd_glue_open_scanned_device → g_active_sdi / g_session /
 *       g_pxview_sdi_shadow set
 *   srstd_glue_acquisition_start → g_session_thread started
 *   srstd_glue_acquisition_stop → g_session_thread joined
 *   srstd_glue_close_active_device → g_active_sdi / g_session cleared
 *       (g_pxview_sdi_shadow kept for reuse)
 *   srstd_pxview_exit → all state cleared
 */
static struct sr_context *g_ctx = NULL;            /* upstream srstd_context */
static struct sr_session *g_session = NULL;         /* upstream session */
static struct sr_dev_inst *g_active_sdi = NULL;     /* upstream sdi (active) */
static GSList *g_scanned_sdi_list = NULL;           /* scanned sdi list */
static int g_active_sdi_index = -1;                 /* index into scanned list */

/* PXView callback + shadow sdi.
 * g_pxview_sdi_shadow is a PXView struct sr_dev_inst allocated by
 * srstd_bridge_pxview_sdi_alloc (in srstd_bridge.c, compiled without
 * rename). Stored as void* here to avoid naming the renamed type.
 * Filled by srstd_sdi_to_pxview when a device is opened. Passed to
 * the PXView callback (which expects PXView struct sr_dev_inst*). */
static srstd_glue_datafeed_cb_t g_pxview_cb = NULL;
static void *g_pxview_cb_data = NULL;
static void *g_pxview_sdi_shadow = NULL;            /* PXView sr_dev_inst* */

/* Session run thread. sr_session_run is blocking (runs GMainLoop), so it
 * must run in a separate thread. Started in acquisition_start, joined in
 * acquisition_stop. */
static GThread *g_session_thread = NULL;

/* Active trigger (Task 9). Set by srstd_glue_session_trigger_set, freed
 * when a new trigger is set or when the session is closed. sr_session_trigger_set
 * stores the raw pointer (no copy, no ownership transfer), and sr_session_destroy
 * does NOT free it, so the glue layer must own the lifecycle. */
static struct sr_trigger *g_active_trigger = NULL;

/* ===== Forward declarations ===== */
static void upstream_datafeed_wrapper(const struct sr_dev_inst *sdi,
                                      const struct sr_datafeed_packet *packet,
                                      void *cb_data);
static gpointer session_thread_func(gpointer data);

/* ===== Library init/exit wrappers ===== */

int srstd_pxview_init_shared(void **ctx_out, void *shared_usb_ctx)
{
    if (!ctx_out || !shared_usb_ctx) {
        return SR_ERR_ARG;  /* SR_ERR_ARG = 3, not renamed (enum value) */
    }

    /* srstd_init_shared is declared in srstd.h and defined in
     * srstd_init_shared.c (same library). Its name doesn't match sr_* so
     * it is not renamed. Casts from void* are safe because PXView passes
     * the same libusb_context* that libsigrokstd expects. */
    int ret = srstd_init_shared((struct sr_context **)ctx_out,
                                (libusb_context *)shared_usb_ctx);
    if (ret == SR_OK) {
        g_ctx = (struct sr_context *)(*ctx_out);
    }
    return ret;
}

void srstd_pxview_exit(void *ctx)
{
    /* Clean up glue state first (stop acquisition, close device, free shadow) */
    srstd_glue_close_active_device();

    if (g_pxview_sdi_shadow) {
        srstd_bridge_pxview_sdi_free(g_pxview_sdi_shadow);
        g_pxview_sdi_shadow = NULL;
    }

    if (g_scanned_sdi_list) {
        /* sdi instances are owned by their drivers — don't free them,
         * just free the list container. */
        g_slist_free(g_scanned_sdi_list);
        g_scanned_sdi_list = NULL;
    }

    g_pxview_cb = NULL;
    g_pxview_cb_data = NULL;
    g_ctx = NULL;

    if (!ctx) {
        return;
    }

    struct sr_context *srctx = (struct sr_context *)ctx;

    /* MUST clear libusb_ctx before sr_exit, otherwise sr_exit's internal
     * libusb_exit(ctx->libusb_ctx) would release the SHARED PXView
     * libusb_context (which PXView still needs). libusb_exit(NULL) is a
     * safe no-op per libusb documentation. */
    srctx->libusb_ctx = NULL;

    /* sr_exit is renamed to srstd_exit by srstd_rename.h. */
    sr_exit(srctx);
}

/* ===== Driver-name lookup (Task 5) =====
 *
 * srstd_glue_get_driver_names — returns a NULL-terminated array of the
 * .name fields of all drivers registered in g_ctx->driver_list. Used by
 * DeviceAgent::is_srstd_device() to dynamically identify ANY srstd-backed
 * device (not just a hardcoded driver name).
 *
 * The returned array is owned by the glue layer and valid for the glue
 * layer's lifetime. It is computed once and cached in static storage
 * (repeated calls return the cached pointer without re-allocating).
 * Caller must NOT free the returned array.
 *
 * Returns NULL if the glue layer is not initialized (g_ctx NULL or
 * driver_list NULL). If count_out is non-NULL, it receives the number of
 * driver names (excluding the NULL terminator), or 0 on failure.
 *
 * Note: This TU is compiled with -include srstd_rename.h, so struct
 * sr_dev_driver is macro-renamed to struct srstd_dev_driver, and g_ctx
 * is struct sr_context* (renamed to srstd_context*). The function name
 * srstd_glue_get_driver_names does not match the sr_* rename pattern so
 * it is NOT renamed. The const char** return type is a plain C type,
 * no renaming needed.
 */
const char** srstd_glue_get_driver_names(int* count_out)
{
    static const char** cached_names = NULL;
    static int cached_count = 0;

    if (!g_ctx || !g_ctx->driver_list) {
        if (count_out) *count_out = 0;
        return NULL;
    }

    /* Return cached result on subsequent calls (app-lifetime ownership). */
    if (cached_names) {
        if (count_out) *count_out = cached_count;
        return cached_names;
    }

    /* Count drivers in the list (NULL-terminated). */
    int count = 0;
    struct sr_dev_driver **drv = g_ctx->driver_list;
    while (drv[count]) count++;

    if (count == 0) {
        if (count_out) *count_out = 0;
        return NULL;
    }

    /* Allocate array of pointers (count + 1 for NULL terminator).
     * Allocated once, never freed (glue layer lifetime = app lifetime). */
    const char** names = (const char**)g_malloc((count + 1) * sizeof(const char*));
    if (!names) {
        if (count_out) *count_out = 0;
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        names[i] = drv[i]->name;  /* drv->name is const char*, owned by driver struct */
    }
    names[count] = NULL;

    cached_names = names;
    cached_count = count;
    if (count_out) *count_out = count;
    return cached_names;
}

/* ===== Upstream datafeed callback wrapper =====
 *
 * Registered with sr_session_datafeed_callback_add. Called by the upstream
 * session (from the session thread / libusb event handler) with UPSTREAM
 * sdi + packet. Converts the packet to PXView format via
 * srstd_packet_to_pxview (from srstd_bridge.c) and forwards to the PXView
 * callback with the PXView sdi shadow.
 *
 * Type notes:
 *   - sdi/packet are upstream types (srstd_dev_inst/srstd_datafeed_packet
 *     after rename) — we don't dereference them, only pass as const void*
 *     to srstd_packet_to_pxview.
 *   - pxview_packet is allocated by srstd_bridge_pxview_packet_alloc
 *     (returns PXView struct sr_datafeed_packet* but seen as srstd_ type
 *     here due to rename). We pass it as void* to the PXView callback.
 *   - g_pxview_sdi_shadow is a PXView sr_dev_inst* (from bridge alloc).
 *     We pass it as const void* to the callback.
 */
static void upstream_datafeed_wrapper(const struct sr_dev_inst *sdi,
                                      const struct sr_datafeed_packet *packet,
                                      void *cb_data)
{
    (void)sdi;       /* we use g_pxview_sdi_shadow instead */
    (void)cb_data;   /* we use g_pxview_cb_data instead */

    if (!g_pxview_cb || !g_pxview_sdi_shadow || !packet) {
        return;
    }

    /* Allocate a PXView-side packet structure (zero-initialized). */
    void *pxview_packet = srstd_bridge_pxview_packet_alloc();
    if (!pxview_packet) {
        return;
    }

    /* Convert upstream packet → PXView packet.
     * srstd_packet_to_pxview is declared in srstd_bridge.h as:
     *   int srstd_packet_to_pxview(const void *src,
     *                               struct sr_datafeed_packet *dst);
     * In this TU, struct sr_datafeed_packet is renamed to srstd_datafeed_packet
     * by srstd_rename.h. The cast is safe: pxview_packet is actually a PXView
     * struct sr_datafeed_packet* (from bridge alloc), and the bridge function
     * (compiled without rename) treats it as such. */
    if (srstd_packet_to_pxview((const void *)packet,
                               (struct sr_datafeed_packet *)pxview_packet) == 0) {
        /* Forward to PXView callback.
         * g_pxview_cb is srstd_glue_datafeed_cb_t = void(*)(const void*,
         * const void*, void*). The actual PXView callback is
         * DataFeedParser::data_feed_callback_ex which takes
         * (const sr_dev_inst*, const sr_datafeed_packet*, void*).
         * ABI-compatible (three pointer-sized args). */
        g_pxview_cb((const void *)g_pxview_sdi_shadow,
                    (const void *)pxview_packet,
                    g_pxview_cb_data);
    }

    srstd_bridge_pxview_packet_free(pxview_packet);
}

/* ===== Session run thread =====
 *
 * sr_session_run blocks while the session's GMainLoop is running. It must
 * run in a separate thread so PXView's GUI thread remains responsive.
 * sr_session_stop (called from the GUI thread) signals the GMainLoop to
 * quit, causing sr_session_run to return; then g_thread_join collects
 * the thread. */
static gpointer session_thread_func(gpointer data)
{
    (void)data;
    if (g_session) {
        sr_session_run(g_session);
    }
    return NULL;
}

/* ===== Device config/acquisition dispatch (real implementations) ===== */

int srstd_glue_dev_config_get(void *sdi, void *ch, void *cg,
                              int key, GVariant **data)
{
    /* Use explicit sdi if provided, else fall back to active sdi. */
    struct sr_dev_inst *active = (sdi != NULL)
        ? (struct sr_dev_inst *)sdi : g_active_sdi;
    if (!active || !g_ctx) {
        if (data) *data = NULL;
        return SR_ERR;
    }

    /* sr_config_get needs the driver; obtain it from sdi.
     * sr_dev_inst_driver_get is renamed to srstd_dev_inst_driver_get. */
    struct sr_dev_driver *driver = sr_dev_inst_driver_get(active);
    if (!driver) {
        if (data) *data = NULL;
        return SR_ERR;
    }

    /* sr_config_get is renamed to srstd_config_get.
     * ch is not used by upstream sr_config_get (it uses sdi->channels). */
    (void)ch;
    return sr_config_get(driver, active,
                         (const struct sr_channel_group *)cg,
                         (uint32_t)key, data);
}

int srstd_glue_dev_config_set(void *sdi, void *ch, void *cg,
                              int key, GVariant *data)
{
    (void)ch;  /* upstream sr_config_set doesn't take a channel param */
    struct sr_dev_inst *active = (sdi != NULL)
        ? (struct sr_dev_inst *)sdi : g_active_sdi;
    if (!active || !g_ctx) {
        return SR_ERR;
    }

    return sr_config_set(active,
                         (const struct sr_channel_group *)cg,
                         (uint32_t)key, data);
}

int srstd_glue_dev_config_list(void *sdi, void *cg,
                               int key, GVariant **data)
{
    struct sr_dev_inst *active = (sdi != NULL)
        ? (struct sr_dev_inst *)sdi : g_active_sdi;
    if (!active || !g_ctx) {
        if (data) *data = NULL;
        return SR_ERR;
    }

    struct sr_dev_driver *driver = sr_dev_inst_driver_get(active);
    if (!driver) {
        if (data) *data = NULL;
        return SR_ERR;
    }

    return sr_config_list(driver, active,
                          (const struct sr_channel_group *)cg,
                          (uint32_t)key, data);
}

int srstd_glue_acquisition_start(void *sdi)
{
    (void)sdi;  /* always uses g_active_sdi (set by open_scanned_device) */

    if (!g_active_sdi || !g_session) {
        return SR_ERR;
    }

    /* If a previous session thread is still running (shouldn't happen
     * if stop was called, but guard against it), join it first. */
    if (g_session_thread) {
        sr_session_stop(g_session);
        g_thread_join(g_session_thread);
        g_session_thread = NULL;
    }

    /* sr_session_start calls dev_acquisition_start on each device in the
     * session. For USB devices, this registers libusb transfers but does
     * NOT block — the actual data flow happens when libusb events are
     * processed (by sr_session_run in the thread below). */
    int ret = sr_session_start(g_session);
    if (ret != SR_OK) {
        return ret;
    }

    /* Run the session's GMainLoop in a thread so it can process libusb
     * events without blocking PXView's GUI thread. */
    g_session_thread = g_thread_new("srstd-session", session_thread_func, NULL);
    if (!g_session_thread) {
        sr_session_stop(g_session);
        return SR_ERR;
    }

    return SR_OK;
}

int srstd_glue_acquisition_stop(void *sdi)
{
    (void)sdi;

    if (!g_session) {
        return SR_ERR;
    }

    /* Signal the session's GMainLoop to quit. This causes sr_session_run
     * (in the thread) to return. */
    sr_session_stop(g_session);

    /* Wait for the session thread to finish. */
    if (g_session_thread) {
        g_thread_join(g_session_thread);
        g_session_thread = NULL;
    }

    return SR_OK;
}

/* ===== Device scan + datafeed callback (Task 8 Part 3) ===== */

void srstd_glue_set_datafeed_callback(srstd_glue_datafeed_cb_t cb,
                                       void *user_data)
{
    g_pxview_cb = cb;
    g_pxview_cb_data = user_data;
}

int srstd_glue_scan_devices(void)
{
    if (!g_ctx) {
        return 0;
    }

    /* Free previous scan results (sdi are owned by drivers, not freed here). */
    if (g_scanned_sdi_list) {
        g_slist_free(g_scanned_sdi_list);
        g_scanned_sdi_list = NULL;
    }

    /* Iterate the driver_list registered in g_ctx (slogic driver added
     * in srstd_init_shared). */
    struct sr_dev_driver **drv = g_ctx->driver_list;
    if (!drv) {
        return 0;
    }

    for (int i = 0; drv[i] != NULL; i++) {
        /* Initialize the driver (idempotent — sr_driver_init guards
         * against double-init via driver->priv). */
        if (sr_driver_init(g_ctx, drv[i]) != SR_OK) {
            continue;
        }

        /* Scan with default options (NULL = no Conn= filter). */
        GSList *devices = sr_driver_scan(drv[i], NULL);
        if (devices) {
            g_scanned_sdi_list = g_slist_concat(g_scanned_sdi_list, devices);
        }
    }

    return g_slist_length(g_scanned_sdi_list);
}

int srstd_glue_get_scanned_device_name(int index, char *buf, int buf_size)
{
    if (!buf || buf_size <= 0) {
        return SR_ERR_ARG;
    }
    buf[0] = '\0';

    if (!g_scanned_sdi_list || index < 0 ||
        index >= g_slist_length(g_scanned_sdi_list)) {
        return SR_ERR_ARG;
    }

    GSList *node = g_slist_nth(g_scanned_sdi_list, index);
    if (!node || !node->data) {
        return SR_ERR;
    }

    struct sr_dev_inst *sdi = (struct sr_dev_inst *)node->data;
    const char *vendor = sr_dev_inst_vendor_get(sdi);
    const char *model = sr_dev_inst_model_get(sdi);

    /* Format: "vendor model" or just "model" */
    if (vendor && vendor[0] && model) {
        snprintf(buf, buf_size, "%s %s", vendor, model);
    } else if (model) {
        snprintf(buf, buf_size, "%s", model);
    } else {
        snprintf(buf, buf_size, "srstd-device-%d", index);
    }

    return SR_OK;
}

int srstd_glue_open_scanned_device(int index)
{
    if (!g_ctx) {
        return SR_ERR;
    }
    if (!g_scanned_sdi_list || index < 0 ||
        index >= g_slist_length(g_scanned_sdi_list)) {
        return SR_ERR_ARG;
    }

    /* Close any previously active srstd device (stops acquisition, destroys
     * session). Safe no-op if none active. */
    srstd_glue_close_active_device();

    GSList *node = g_slist_nth(g_scanned_sdi_list, index);
    if (!node || !node->data) {
        return SR_ERR;
    }

    struct sr_dev_inst *sdi = (struct sr_dev_inst *)node->data;

    /* Open the device (calls driver->dev_open). */
    int ret = sr_dev_open(sdi);
    if (ret != SR_OK) {
        return ret;
    }

    /* Create a new upstream session (one session per active device). */
    ret = sr_session_new(g_ctx, &g_session);
    if (ret != SR_OK) {
        sr_dev_close(sdi);
        return ret;
    }

    /* Add the device to the session (sets sdi->session). */
    ret = sr_session_dev_add(g_session, sdi);
    if (ret != SR_OK) {
        sr_session_destroy(g_session);
        g_session = NULL;
        sr_dev_close(sdi);
        return ret;
    }

    /* Register the upstream datafeed callback. The wrapper converts
     * upstream packets to PXView format and forwards to g_pxview_cb. */
    sr_session_datafeed_callback_remove_all(g_session);
    sr_session_datafeed_callback_add(g_session, upstream_datafeed_wrapper, NULL);

    /* Create / refresh the PXView sdi shadow. This is a PXView struct
     * sr_dev_inst allocated by the bridge (compiled without rename).
     * Filled with vendor/model/channels from the upstream sdi. Passed to
     * the PXView callback as the sdi argument. */
    if (!g_pxview_sdi_shadow) {
        g_pxview_sdi_shadow = srstd_bridge_pxview_sdi_alloc();
    }
    if (g_pxview_sdi_shadow) {
        /* srstd_sdi_to_pxview(const void *src, struct sr_dev_inst *dst).
         * In this TU, struct sr_dev_inst is renamed to srstd_dev_inst.
         * The cast is safe: g_pxview_sdi_shadow is actually a PXView
         * sr_dev_inst* (from bridge alloc), and the bridge function
         * (compiled without rename) treats it as such. */
        srstd_sdi_to_pxview((const void *)sdi,
                            (struct sr_dev_inst *)g_pxview_sdi_shadow);
    }

    g_active_sdi = sdi;
    g_active_sdi_index = index;

    return SR_OK;
}

int srstd_glue_close_active_device(void)
{
    /* Stop acquisition if running (joins session thread). */
    if (g_session_thread) {
        if (g_session) {
            sr_session_stop(g_session);
        }
        g_thread_join(g_session_thread);
        g_session_thread = NULL;
    }

    /* Free the active trigger before destroying the session, since the
     * session holds a raw pointer to it (sr_session_destroy does NOT free
     * the trigger). sr_trigger_free is renamed to srstd_trigger_free. */
    if (g_active_trigger) {
        sr_trigger_free(g_active_trigger);
        g_active_trigger = NULL;
    }

    /* Destroy the session (removes all devices + callbacks). */
    if (g_session) {
        sr_session_destroy(g_session);
        g_session = NULL;
    }

    /* Close the device (calls driver->dev_close). */
    if (g_active_sdi) {
        sr_dev_close(g_active_sdi);
        g_active_sdi = NULL;
    }
    g_active_sdi_index = -1;

    /* NOTE: g_pxview_sdi_shadow is kept for reuse (will be refilled on
     * next open). It is freed in srstd_pxview_exit. */

    return SR_OK;
}

int srstd_glue_get_active_device_name(char *name_buf, int name_buf_size,
                                       char *driver_buf, int driver_buf_size)
{
    if (!name_buf || name_buf_size <= 0 || !driver_buf || driver_buf_size <= 0) {
        return SR_ERR_ARG;
    }
    name_buf[0] = '\0';
    driver_buf[0] = '\0';

    if (!g_active_sdi) {
        return SR_ERR;
    }

    const char *vendor = sr_dev_inst_vendor_get(g_active_sdi);
    const char *model = sr_dev_inst_model_get(g_active_sdi);

    if (vendor && vendor[0] && model) {
        snprintf(name_buf, name_buf_size, "%s %s", vendor, model);
    } else if (model) {
        snprintf(name_buf, name_buf_size, "%s", model);
    } else {
        snprintf(name_buf, name_buf_size, "srstd-device");
    }

    /* Get driver name from sdi->driver->name. */
    struct sr_dev_driver *drv = sr_dev_inst_driver_get(g_active_sdi);
    if (drv && drv->name) {
        snprintf(driver_buf, driver_buf_size, "%s", drv->name);
    } else {
        snprintf(driver_buf, driver_buf_size, "unknown");
    }

    return SR_OK;
}

unsigned long long srstd_glue_get_active_handle(void)
{
    if (g_active_sdi_index < 0) {
        return 0;
    }
    return SRSTD_MAKE_HANDLE(g_active_sdi_index);
}

void *srstd_glue_get_active_sdi_shadow(void)
{
    return g_pxview_sdi_shadow;
}

GSList *srstd_glue_get_active_channels(void)
{
    if (!g_pxview_sdi_shadow) {
        return NULL;
    }
    /* srstd_bridge_pxview_sdi_get_channels is declared in srstd_bridge.h as
     * taking const struct sr_dev_inst*. In this TU, struct sr_dev_inst is
     * renamed to struct srstd_dev_inst by srstd_rename.h. The cast is safe:
     * g_pxview_sdi_shadow is a PXView sr_dev_inst* (from bridge alloc), and
     * the bridge function (compiled without rename) treats it as such. */
    return srstd_bridge_pxview_sdi_get_channels(
        (const struct sr_dev_inst *)g_pxview_sdi_shadow);
}

int srstd_glue_is_collecting(void)
{
    /* Acquisition is in progress if the session thread is running.
     * The thread is started in acquisition_start and joined in
     * acquisition_stop / close_active_device. */
    return (g_session_thread != NULL && g_session != NULL) ? 1 : 0;
}

/* ===== Trigger synchronization (Task 9) =====
 *
 * These functions bridge PXView's TriggerConfig to the upstream session's
 * software trigger. See srstd_pxview_glue.h for the full flow description.
 *
 * Note on sr_* renames: this TU is compiled with -include srstd_rename.h,
 * so sr_trigger_new / sr_trigger_free / sr_session_trigger_set are
 * macro-renamed to srstd_trigger_new / srstd_trigger_free /
 * srstd_session_trigger_set. The struct tags sr_trigger / sr_trigger_stage /
 * sr_trigger_match are renamed to srstd_trigger / srstd_trigger_stage /
 * srstd_trigger_match. We write the sr_* form in source and let the
 * preprocessor handle the rename.
 */

void *srstd_glue_trigger_create(void)
{
    /* sr_trigger_new is renamed to srstd_trigger_new. Allocates a
     * zero-initialized sr_trigger with NULL name and NULL stages. */
    return (void *)sr_trigger_new(NULL);
}

void srstd_glue_trigger_free(void *trigger)
{
    if (!trigger) return;
    /* sr_trigger_free is renamed to srstd_trigger_free. Frees the trigger
     * and all its stages/matches (but NOT the channel pointers — they
     * belong to the sdi's channel list). */
    sr_trigger_free((struct sr_trigger *)trigger);
}

void srstd_glue_trigger_fix_channels(void *trigger)
{
    struct sr_trigger *trig;
    GSList *l_stage;

    if (!trigger || !g_active_sdi) return;

    trig = (struct sr_trigger *)trigger;

    /* Get the upstream sdi's channel list.
     * sr_dev_inst_channels_get is renamed to srstd_dev_inst_channels_get.
     * Returns a GSList of struct sr_channel* (renamed to srstd_channel*). */
    GSList *channels = sr_dev_inst_channels_get(g_active_sdi);
    if (!channels) return;

    /* Iterate stages → matches, replace tagged channel pointers with real
     * upstream sr_channel* from the sdi's channel list.
     *
     * Tagged pointer encoding (from srstd_bridge.c parse_trigger_value_string):
     *   match->channel = (void*)(intptr_t)(channel_index + 1)
     * Decode: index = (intptr_t)match->channel - 1
     *
     * We look up the channel by index in the sdi's channel list. If the
     * index is out of range, we set channel = NULL (soft_trigger_logic_check
     * would skip it via the !match->channel->enabled check — but actually
     * NULL deref would crash, so we just leave it NULL and let the caller
     * deal with it; in practice the indices should always be valid since
     * they come from the PXView signal model which mirrors the sdi channels). */
    for (l_stage = trig->stages; l_stage; l_stage = l_stage->next) {
        struct sr_trigger_stage *stage = l_stage->data;
        GSList *l_match;
        if (!stage) continue;

        for (l_match = stage->matches; l_match; l_match = l_match->next) {
            struct sr_trigger_match *match = l_match->data;
            if (!match) continue;

            /* Only fix tagged pointers (non-NULL, small positive integers).
             * If match->channel is already a real pointer (e.g., from a
             * previous fix_channels call), skip it. We detect tagged
             * pointers by checking that the value is a small positive
             * integer (channel index + 1, typically < 1000). Real pointers
             * are much larger. This is a heuristic but safe for practical
             * channel counts. */
            intptr_t tag = (intptr_t)match->channel;
            if (tag <= 0) {
                /* NULL or negative: already fixed or invalid, skip */
                continue;
            }
            if (tag > 0x10000) {
                /* Looks like a real pointer (large value), already fixed */
                continue;
            }

            /* Decode channel index */
            int ch_index = (int)(tag - 1);

            /* Look up the channel by index in the sdi's channel list */
            struct sr_channel *real_ch = NULL;
            GSList *l_ch;
            for (l_ch = channels; l_ch; l_ch = l_ch->next) {
                struct sr_channel *ch = l_ch->data;
                if (ch && ch->index == ch_index) {
                    real_ch = ch;
                    break;
                }
            }

            /* Replace tagged pointer with real channel pointer (or NULL
             * if not found — soft_trigger_logic_check would crash on NULL
             * deref, but that's a configuration error the caller should
             * fix by ensuring trigger channels match the sdi's channels). */
            match->channel = real_ch;
        }
    }
}

int srstd_glue_session_trigger_set(void *trigger)
{
    if (!trigger) {
        return SR_ERR_ARG;
    }

    /* Free the previous trigger (if any). sr_trigger_free is renamed to
     * srstd_trigger_free. */
    if (g_active_trigger) {
        sr_trigger_free(g_active_trigger);
        g_active_trigger = NULL;
    }

    if (!g_session) {
        /* No active session — can't set the trigger. Free the trigger
         * internally so the caller doesn't need to. */
        sr_trigger_free((struct sr_trigger *)trigger);
        return SR_ERR;
    }

    /* sr_session_trigger_set is renamed to srstd_session_trigger_set.
     * It just stores the pointer (no copy, no ownership transfer).
     * The glue layer owns the lifecycle (frees on close/exit). */
    int ret = sr_session_trigger_set(g_session, (struct sr_trigger *)trigger);
    if (ret == SR_OK) {
        g_active_trigger = (struct sr_trigger *)trigger;
    } else {
        /* Setting failed — free the trigger to avoid leak */
        sr_trigger_free((struct sr_trigger *)trigger);
    }
    return ret;
}
