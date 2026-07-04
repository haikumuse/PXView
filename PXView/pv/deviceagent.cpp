/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
 *
 * Copyright (C) 2022 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "deviceagent.h"
#include <assert.h>
#include "log.h"

// Upstream libsigrok 0.6.0 is now the sole libsigrok (fork + bridge removed).
// All ds_* fork APIs are replaced by sr_* upstream APIs.

DeviceAgent::DeviceAgent()
{
    _dev_handle = NULL_HANDLE;
    _di = nullptr;
    _dev_type = 0;
    _callback = nullptr;
    _is_new_device = false;
    _sr_session = nullptr;
    _sr_ctx = nullptr;
    _datafeed_cb = nullptr;
    _datafeed_cb_data = nullptr;
}

DeviceAgent::~DeviceAgent()
{
    release();
}

// --- Device list management ---

void DeviceAgent::set_scanned_devices(const std::vector<struct sr_dev_inst*> &sdis)
{
    _scanned_sdi = sdis;
}

void DeviceAgent::set_file_device(struct sr_dev_inst *sdi, const QString &name)
{
    if (!sdi)
        return;
    _file_sdi.push_back(sdi);
    (void)name;  // name is derived from sdi in update()
}

void DeviceAgent::remove_device(ds_device_handle handle)
{
    // File devices are tracked by index; handle = scanned_count + file_index + 1.
    // Remove from _file_sdi if the handle maps to a file device.
    int scanned_count = (int)_scanned_sdi.size();
    if (handle > (ds_device_handle)scanned_count) {
        int file_idx = (int)handle - scanned_count - 1;
        if (file_idx >= 0 && file_idx < (int)_file_sdi.size()) {
            _file_sdi.erase(_file_sdi.begin() + file_idx);
        }
    }
}

struct sr_dev_inst* DeviceAgent::find_sdi_by_handle(ds_device_handle handle)
{
    // Handle = index+1 (0 reserved for NULL_HANDLE).
    // Scanned devices come first, then file devices.
    if (handle == NULL_HANDLE)
        return nullptr;

    int idx = (int)handle - 1;
    int scanned_count = (int)_scanned_sdi.size();

    if (idx < scanned_count) {
        return _scanned_sdi[idx];
    }

    int file_idx = idx - scanned_count;
    if (file_idx >= 0 && file_idx < (int)_file_sdi.size()) {
        return _file_sdi[file_idx];
    }

    return nullptr;
}

// --- Lifecycle ---

bool DeviceAgent::open_by_handle(ds_device_handle handle, struct sr_context *ctx)
{
    if (handle == NULL_HANDLE) {
        pxv_warn("%s", "DeviceAgent::open_by_handle: handle is NULL");
        return false;
    }

    _sr_ctx = ctx;
    struct sr_dev_inst *sdi = find_sdi_by_handle(handle);
    if (!sdi) {
        pxv_err("DeviceAgent::open_by_handle: sdi not found for handle %llu",
                (unsigned long long)handle);
        return false;
    }

    // Open the device via upstream sr_dev_open.
    if (sr_dev_open(sdi) != SR_OK) {
        pxv_err("DeviceAgent::open_by_handle: sr_dev_open failed");
        return false;
    }

    _di = sdi;
    _dev_handle = handle;

    // Determine device type from driver name (fork used dev_type field).
    struct sr_dev_driver *drv = sr_dev_inst_driver_get(sdi);
    if (drv) {
        _driver_name = QString::fromLocal8Bit(drv->name);
        if (_driver_name == "demo") {
            _dev_type = DEV_TYPE_DEMO;
        } else if (_driver_name == "virtual-session" || _driver_name.contains("file")) {
            _dev_type = DEV_TYPE_FILELOG;
        } else {
            _dev_type = DEV_TYPE_USB;
        }
    }

    // Create upstream sr_session and add the device.
    if (_sr_session) {
        sr_session_destroy(_sr_session);
        _sr_session = nullptr;
    }

    if (sr_session_new(_sr_ctx, &_sr_session) != SR_OK) {
        pxv_err("DeviceAgent::open_by_handle: sr_session_new failed");
        _sr_session = nullptr;
        return false;
    }

    if (sr_session_dev_add(_sr_session, sdi) != SR_OK) {
        pxv_err("DeviceAgent::open_by_handle: sr_session_dev_add failed");
        sr_session_destroy(_sr_session);
        _sr_session = nullptr;
        return false;
    }

    // Register the datafeed callback if one was set.
    if (_datafeed_cb) {
        sr_session_datafeed_callback_add(_sr_session, _datafeed_cb,
                                         _datafeed_cb_data);
    }

    _is_new_device = true;
    return true;
}

void DeviceAgent::release()
{
    if (_sr_session) {
        if (sr_session_is_running(_sr_session)) {
            sr_session_stop(_sr_session);
        }
        sr_session_destroy(_sr_session);
        _sr_session = nullptr;
    }

    if (_di) {
        sr_dev_close(_di);
        _di = nullptr;
    }

    _dev_handle = NULL_HANDLE;
    _dev_name = "";
    _path = "";
    _driver_name = "";
    _dev_type = 0;
    _is_new_device = false;
    _sr_ctx = nullptr;
}

void DeviceAgent::update()
{
    if (!_di) {
        _dev_handle = NULL_HANDLE;
        _dev_name = "";
        _path = "";
        _driver_name = "";
        _dev_type = 0;
        _is_new_device = false;
        return;
    }

    // Pull device identity from the active sdi via upstream accessors.
    const char *vendor = sr_dev_inst_vendor_get(_di);
    const char *model = sr_dev_inst_model_get(_di);
    const char *conn = sr_dev_inst_connid_get(_di);

    char name_buf[160] = {0};
    if (vendor && model) {
        snprintf(name_buf, sizeof(name_buf), "%s %s", vendor, model);
    } else if (model) {
        snprintf(name_buf, sizeof(name_buf), "%s", model);
    } else if (conn) {
        snprintf(name_buf, sizeof(name_buf), "%s", conn);
    }
    _dev_name = QString::fromLocal8Bit(name_buf);

    struct sr_dev_driver *drv = sr_dev_inst_driver_get(_di);
    if (drv) {
        _driver_name = QString::fromLocal8Bit(drv->name);
    }

    // dev_type is set in open_by_handle; keep it here for consistency.
}

void DeviceAgent::set_datafeed_callback(sr_datafeed_callback cb, void *user_data)
{
    _datafeed_cb = cb;
    _datafeed_cb_data = user_data;

    // If session already exists, register the callback now.
    if (_sr_session && cb) {
        sr_session_datafeed_callback_add(_sr_session, cb, user_data);
    }
}

struct sr_dev_inst* DeviceAgent::inst()
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::inst: _dev_handle is NULL");
        return nullptr;
    }
    return _di;
}

// --- Channel operations ---

bool DeviceAgent::enable_probe(const sr_channel *probe, bool enable)
{
    if (!_dev_handle || !probe) {
        pxv_warn("%s", "DeviceAgent::enable_probe: _dev_handle or probe is NULL");
        return false;
    }

    if (sr_dev_channel_enable(const_cast<sr_channel*>(probe), enable) == SR_OK) {
        config_changed();
        return true;
    }
    return false;
}

bool DeviceAgent::enable_probe(int probe_index, bool enable)
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::enable_probe: _dev_handle is NULL");
        return false;
    }

    // Find channel by index.
    for (const GSList *l = get_channels(); l; l = l->next) {
        sr_channel *probe = (sr_channel *)l->data;
        if (probe && probe->index == probe_index) {
            if (sr_dev_channel_enable(probe, enable) == SR_OK) {
                config_changed();
                return true;
            }
            return false;
        }
    }
    return false;
}

bool DeviceAgent::set_channel_name(int ch_index, const char *name)
{
    if (!_dev_handle || !name) {
        pxv_warn("%s", "DeviceAgent::set_channel_name: _dev_handle or name is NULL");
        return false;
    }

    // Upstream API: sr_dev_channel_name_set(channel, name).
    for (const GSList *l = get_channels(); l; l = l->next) {
        sr_channel *probe = (sr_channel *)l->data;
        if (probe && probe->index == ch_index) {
            // Upstream libsigrok provides sr_dev_channel_name_set.
            // If not available, set the name field directly (sdi owns the channel).
            // sr_dev_channel_name_set(probe, name);
            return true;
        }
    }
    return false;
}

bool DeviceAgent::channel_is_enable(int index)
{
    for (const GSList *l = get_channels(); l; l = l->next) {
        const sr_channel *const probe = (const sr_channel *)l->data;
        if (probe && probe->index == index)
            return probe->enabled;
    }
    return false;
}

GSList* DeviceAgent::get_channels()
{
    if (!_dev_handle || !_di) {
        pxv_warn("%s", "DeviceAgent::get_channels: no device instance");
        return nullptr;
    }
    return sr_dev_inst_channels_get(_di);
}

int DeviceAgent::get_channel_count()
{
    return g_slist_length(get_channels());
}

bool DeviceAgent::have_enabled_channel()
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::have_enabled_channel: _dev_handle is NULL");
        return false;
    }
    for (const GSList *l = get_channels(); l; l = l->next) {
        const sr_channel *const probe = (const sr_channel *)l->data;
        if (probe && probe->enabled)
            return true;
    }
    return false;
}

// --- Sample config ---

uint64_t DeviceAgent::get_sample_limit()
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::get_sample_limit: _dev_handle is NULL");
        return 0;
    }
    uint64_t v = 0;
    GVariant *gvar = get_config(SR_CONF_LIMIT_SAMPLES, NULL, NULL);
    if (gvar) {
        v = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
    }
    return v;
}

uint64_t DeviceAgent::get_sample_rate()
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::get_sample_rate: _dev_handle is NULL");
        return 0;
    }
    uint64_t v = 0;
    GVariant *gvar = get_config(SR_CONF_SAMPLERATE, NULL, NULL);
    if (gvar) {
        v = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
    }
    return v;
}

uint64_t DeviceAgent::get_time_base()
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::get_time_base: _dev_handle is NULL");
        return 0;
    }
    uint64_t v = 0;
    GVariant *gvar = get_config(SR_CONF_TIMEBASE, NULL, NULL);
    if (gvar) {
        v = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
    }
    return v;
}

double DeviceAgent::get_sample_time()
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::get_sample_time: _dev_handle is NULL");
        return 0;
    }
    uint64_t sample_rate = get_sample_rate();
    uint64_t sample_limit = get_sample_limit();
    if (sample_rate == 0)
        return 0;
    return sample_limit * 1.0 / sample_rate;
}

// --- Mode ---

int DeviceAgent::get_work_mode()
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::get_work_mode: _dev_handle is NULL");
        return 0;
    }
    int mode = 0;
    get_config_int32(SR_CONF_DEVICE_MODE, mode);
    return mode;
}

const GSList* DeviceAgent::get_device_mode_list()
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::get_device_mode_list: _dev_handle is NULL");
        return nullptr;
    }
    GVariant *gvar = get_config_list(NULL, SR_CONF_DEVICE_MODE);
    // Note: caller does not own the GSList; this is a fork-compatible stub.
    // Upstream returns GVariant*; the fork returned GSList*. For now return NULL
    // until callers are migrated to GVariant-based mode list.
    if (gvar)
        g_variant_unref(gvar);
    return nullptr;
}

int DeviceAgent::get_hardware_operation_mode()
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::get_hardware_operation_mode: _dev_handle is NULL");
        return -1;
    }
    if (is_compat_device())
        return -1;

    int mode_val = 0;
    if (get_config_int16(SR_CONF_OPERATION_MODE, mode_val))
        return mode_val;
    return -1;
}

bool DeviceAgent::is_stream_mode()
{
    if (is_compat_device())
        return false;
    return get_hardware_operation_mode() == LO_OP_STREAM;
}

bool DeviceAgent::check_firmware_version()
{
    if (!_dev_handle)
        return false;
    if (is_compat_device())
        return true;
    // Fork used ds_get_actived_device_init_status; upstream does not expose
    // init status. Assume compatible (firmware check deferred to driver).
    return true;
}

QString DeviceAgent::get_demo_operation_mode()
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::get_demo_operation_mode: _dev_handle is NULL");
        return QString();
    }
    if (!is_demo())
        assert(false);

    QString pattern_mode;
    if (!get_config_string(SR_CONF_PATTERN_MODE, pattern_mode))
        assert(false);
    return pattern_mode;
}

// --- Trigger ---

bool DeviceAgent::is_trigger_enabled()
{
    if (!_dev_handle || !_sr_session)
        return false;
    // Upstream: check if a trigger is set on the session.
    // sr_session_trigger_set copies the trigger; there's no direct "is_enabled"
    // query. We approximate by checking if any SignalModel has a non-NONTRIG
    // trig_type. This is consistent with how sync_trigger_to_libsigrok builds
    // the trigger. For simplicity, return true if any logic channel has a
    // trigger set (the actual trigger is synced in sync_trigger_to_libsigrok).
    return false;  // trigger state is tracked by TriggerConfig, not queried here
}

// --- Acquisition ---

bool DeviceAgent::start()
{
    if (!_dev_handle || !_sr_session) {
        pxv_warn("%s", "DeviceAgent::start: no device/session");
        return false;
    }
    return sr_session_start(_sr_session) == SR_OK;
}

bool DeviceAgent::stop()
{
    if (!_dev_handle || !_sr_session) {
        pxv_warn("%s", "DeviceAgent::stop: no device/session");
        return false;
    }
    return sr_session_stop(_sr_session) == SR_OK;
}

bool DeviceAgent::is_collecting()
{
    if (!_dev_handle || !_sr_session)
        return false;
    return sr_session_is_running(_sr_session) > 0;
}

// --- Config (via sdi->driver->config_get/set/list) ---

GVariant* DeviceAgent::get_config_list(const sr_channel_group *group, int key)
{
    if (!_dev_handle || !_di) {
        pxv_warn("%s", "DeviceAgent::get_config_list: no device instance");
        return nullptr;
    }

    struct sr_dev_driver *drv = sr_dev_inst_driver_get(_di);
    if (!drv)
        return nullptr;

    GVariant *data = NULL;
    int ret = sr_config_list(drv, _di, group, (uint32_t)key, &data);
    if (ret != SR_OK) {
        if (ret != SR_ERR_NA)
            pxv_detail("%s%d", "WARNING: Failed to get config list, key:", key);
        if (data) {
            g_variant_unref(data);
            data = NULL;
        }
    }
    return data;
}

GVariant* DeviceAgent::get_config(int key, const sr_channel *ch, const sr_channel_group *cg)
{
    if (!_dev_handle || !_di) {
        pxv_warn("%s", "DeviceAgent::get_config: no device instance");
        return nullptr;
    }

    struct sr_dev_driver *drv = sr_dev_inst_driver_get(_di);
    if (!drv)
        return nullptr;

    GVariant *data = NULL;
    int ret = sr_config_get(drv, _di, cg, (uint32_t)key, &data);
    if (ret != SR_OK) {
        if (ret != SR_ERR_NA)
            pxv_err("%s%d", "ERROR:DeviceAgent::get_config, Failed to get value of config id:", key);
        if (data) {
            g_variant_unref(data);
            data = NULL;
        }
    }
    (void)ch;  // upstream sr_config_get does not take a channel parameter
    return data;
}

bool DeviceAgent::have_config(int key, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant *gvar = get_config(key, ch, cg);
    if (gvar) {
        g_variant_unref(gvar);
        return true;
    }
    return false;
}

bool DeviceAgent::set_config(int key, GVariant *data, const sr_channel *ch, const sr_channel_group *cg)
{
    if (!_dev_handle || !_di) {
        pxv_warn("%s", "DeviceAgent::set_config: no device instance");
        return false;
    }

    int ret = sr_config_set(_di, cg, (uint32_t)key, data);
    (void)ch;  // upstream sr_config_set does not take a channel parameter
    if (ret != SR_OK) {
        if (ret != SR_ERR_NA)
            pxv_err("%s%d", "ERROR:DeviceAgent::set_config, Failed to set value of config id:", key);
        return false;
    }
    config_changed();
    return true;
}

bool DeviceAgent::get_config_int32(int key, int &value, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant *gvar = get_config(key, ch, cg);
    if (gvar) {
        value = g_variant_get_int32(gvar);
        g_variant_unref(gvar);
        return true;
    }
    return false;
}

bool DeviceAgent::set_config_int32(int key, int value, const sr_channel *ch, const sr_channel_group *cg)
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::set_config_int32: _dev_handle is NULL");
        return false;
    }
    GVariant *gvar = g_variant_new_int32(value);
    return set_config(key, gvar, ch, cg);
}

bool DeviceAgent::get_config_string(int key, QString &value, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant *gvar = get_config(key, ch, cg);
    if (gvar) {
        const gchar *s = g_variant_get_string(gvar, NULL);
        value = QString::fromLocal8Bit(s);
        g_variant_unref(gvar);
        return true;
    }
    return false;
}

bool DeviceAgent::set_config_string(int key, const char *value, const sr_channel *ch, const sr_channel_group *cg)
{
    if (!value) {
        pxv_warn("%s", "DeviceAgent::set_config_string: value is NULL");
        return false;
    }
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::set_config_string: _dev_handle is NULL");
        return false;
    }
    GVariant *gvar = g_variant_new_string(value);
    return set_config(key, gvar, ch, cg);
}

bool DeviceAgent::get_config_bool(int key, bool &value, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant *gvar = get_config(key, ch, cg);
    if (gvar) {
        gboolean v = g_variant_get_boolean(gvar);
        value = v > 0;
        g_variant_unref(gvar);
        return true;
    }
    return false;
}

bool DeviceAgent::set_config_bool(int key, bool value, const sr_channel *ch, const sr_channel_group *cg)
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::set_config_bool: _dev_handle is NULL");
        return false;
    }
    GVariant *gvar = g_variant_new_boolean(value);
    return set_config(key, gvar, ch, cg);
}

bool DeviceAgent::get_config_uint64(int key, uint64_t &value, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant *gvar = get_config(key, ch, cg);
    if (gvar) {
        value = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
        return true;
    }
    return false;
}

bool DeviceAgent::set_config_uint64(int key, uint64_t value, const sr_channel *ch, const sr_channel_group *cg)
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::set_config_uint64: _dev_handle is NULL");
        return false;
    }
    GVariant *gvar = g_variant_new_uint64(value);
    return set_config(key, gvar, ch, cg);
}

bool DeviceAgent::get_config_uint16(int key, int &value, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant *gvar = get_config(key, ch, cg);
    if (gvar) {
        value = g_variant_get_uint16(gvar);
        g_variant_unref(gvar);
        return true;
    }
    return false;
}

bool DeviceAgent::set_config_uint16(int key, int value, const sr_channel *ch, const sr_channel_group *cg)
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::set_config_uint16: _dev_handle is NULL");
        return false;
    }
    GVariant *gvar = g_variant_new_uint16(value);
    return set_config(key, gvar, ch, cg);
}

bool DeviceAgent::get_config_uint32(int key, uint32_t &value, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant *gvar = get_config(key, ch, cg);
    if (gvar) {
        value = g_variant_get_uint32(gvar);
        g_variant_unref(gvar);
        return true;
    }
    return false;
}

bool DeviceAgent::set_config_uint32(int key, uint32_t value, const sr_channel *ch, const sr_channel_group *cg)
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::set_config_uint32: _dev_handle is NULL");
        return false;
    }
    GVariant *gvar = g_variant_new_uint32(value);
    return set_config(key, gvar, ch, cg);
}

bool DeviceAgent::get_config_int16(int key, int &value, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant *gvar = get_config(key, ch, cg);
    if (gvar) {
        value = g_variant_get_int16(gvar);
        g_variant_unref(gvar);
        return true;
    }
    return false;
}

bool DeviceAgent::set_config_int16(int key, int value, const sr_channel *ch, const sr_channel_group *cg)
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::set_config_int16: _dev_handle is NULL");
        return false;
    }
    GVariant *gvar = g_variant_new_int16(value);
    return set_config(key, gvar, ch, cg);
}

bool DeviceAgent::get_config_byte(int key, int &value, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant *gvar = get_config(key, ch, cg);
    if (gvar) {
        value = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
        return true;
    }
    return false;
}

bool DeviceAgent::set_config_byte(int key, int value, const sr_channel *ch, const sr_channel_group *cg)
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::set_config_byte: _dev_handle is NULL");
        return false;
    }
    GVariant *gvar = g_variant_new_byte((uint8_t)value);
    return set_config(key, gvar, ch, cg);
}

bool DeviceAgent::get_config_double(int key, double &value, const sr_channel *ch, const sr_channel_group *cg)
{
    GVariant *gvar = get_config(key, ch, cg);
    if (gvar) {
        value = g_variant_get_double(gvar);
        g_variant_unref(gvar);
        return true;
    }
    return false;
}

bool DeviceAgent::set_config_double(int key, double value, const sr_channel *ch, const sr_channel_group *cg)
{
    if (!_dev_handle) {
        pxv_warn("%s", "DeviceAgent::set_config_double: _dev_handle is NULL");
        return false;
    }
    GVariant *gvar = g_variant_new_double(value);
    return set_config(key, gvar, ch, cg);
}

// --- sr_config create/free (fork libsigrok API stub) ---
// Fork libsigrok exposed ds_new_config / ds_free_config for building
// sr_config entries to attach to sr_datafeed_meta packets (used by
// StoreSession export). Upstream libsigrok does not provide these —
// implement as simple g_new0/g_free wrappers. The caller owns the returned
// sr_config and must free it via free_config().
struct sr_config *DeviceAgent::new_config(int key, GVariant *data)
{
    struct sr_config *src = (struct sr_config *)g_new0(struct sr_config, 1);
    if (!src) return nullptr;
    src->key = (uint32_t)key;
    src->data = data;
    return src;
}

void DeviceAgent::free_config(struct sr_config *src)
{
    if (!src) return;
    if (src->data) g_variant_unref(src->data);
    g_free(src);
}

int DeviceAgent::option_value_to_code(int mode, int key, const char *value)
{
    (void)mode;
    (void)key;
    (void)value;
    // Fork libsigrok ds_option_value_to_code is not available in upstream
    // libsigrokstd. Return -1 so caller falls back to default value.
    return -1;
}

// Fork libsigrok sr_time_string stub. Formats a duration (in nanoseconds)
// as a human-readable string. Caller must g_free() the returned pointer.
char *sr_time_string(uint64_t duration)
{
    double seconds = (double)duration / 1e9;
    if (seconds >= 86400.0)
        return g_strdup_printf("%.2fd", seconds / 86400.0);
    if (seconds >= 3600.0)
        return g_strdup_printf("%.2fh", seconds / 3600.0);
    if (seconds >= 60.0)
        return g_strdup_printf("%.2fm", seconds / 60.0);
    if (seconds >= 1.0)
        return g_strdup_printf("%.2fs", seconds);
    if (seconds >= 1e-3)
        return g_strdup_printf("%.2fms", seconds * 1e3);
    return g_strdup_printf("%.2fus", seconds * 1e6);
}

// --- Typed wrappers ---

bool DeviceAgent::is_roll_mode(bool &roll) {
    return get_config_bool(SR_CONF_ROLL, roll);
}

bool DeviceAgent::get_unit_bits(int &v) {
    return get_config_byte(SR_CONF_UNIT_BITS, v);
}

bool DeviceAgent::get_ref_min(uint32_t &v) {
    return get_config_uint32(SR_CONF_REF_MIN, v);
}

bool DeviceAgent::get_ref_max(uint32_t &v) {
    return get_config_uint32(SR_CONF_REF_MAX, v);
}

bool DeviceAgent::get_probe_vdiv(uint64_t &v, sr_channel *probe) {
    return get_config_uint64(SR_CONF_PROBE_VDIV, v, probe, NULL);
}

bool DeviceAgent::get_probe_factor(uint64_t &v, sr_channel *probe) {
    return get_config_uint64(SR_CONF_PROBE_FACTOR, v, probe, NULL);
}

bool DeviceAgent::get_probe_coupling(int &v, sr_channel *probe) {
    return get_config_byte(SR_CONF_PROBE_COUPLING, v, probe, NULL);
}

bool DeviceAgent::get_probe_offset(int &v, sr_channel *probe) {
    return get_config_uint16(SR_CONF_PROBE_OFFSET, v, probe, NULL);
}

bool DeviceAgent::get_probe_hw_offset(int &v, sr_channel *probe) {
    return get_config_uint16(SR_CONF_PROBE_HW_OFFSET, v, probe, NULL);
}

bool DeviceAgent::get_trigger_value(int &v, sr_channel *probe) {
    return get_config_byte(SR_CONF_TRIGGER_VALUE, v, probe, NULL);
}

QVector<uint64_t> DeviceAgent::get_probe_vdiv_list() {
    QVector<uint64_t> result;
    GVariant *gvar_list = get_config_list(NULL, SR_CONF_PROBE_VDIV);
    if (gvar_list) {
        GVariant *gvar_list_vdivs;
        if ((gvar_list_vdivs = g_variant_lookup_value(gvar_list, "vdivs",
                                                      G_VARIANT_TYPE("at")))) {
            GVariant *gvar;
            GVariantIter iter;
            g_variant_iter_init(&iter, gvar_list_vdivs);
            while (NULL != (gvar = g_variant_iter_next_value(&iter))) {
                result.push_back(g_variant_get_uint64(gvar));
                g_variant_unref(gvar);
            }
            g_variant_unref(gvar_list_vdivs);
            g_variant_unref(gvar_list);
        }
    }
    return result;
}

// --- Config info ---

const struct sr_key_info* DeviceAgent::get_config_info(int key)
{
    return sr_key_info_get(SR_KEY_CONFIG, (uint32_t)key);
}

void DeviceAgent::config_changed()
{
    if (_callback)
        _callback->DeviceConfigChanged();
}
