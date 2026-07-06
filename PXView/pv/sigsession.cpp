/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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

#include <libsigrokdecode.h>

#include "mainwindow.h"
#include "sigsession.h"

#include "core/filterprocessor.h"
#include "core/decodetaskmanager.h"
#include "core/datafeedparser.h"
#include "core/documentregistry.h"
#include "core/capturemanager.h"
#include "data/analogsnapshot.h"
#include "data/decode/decoder.h"
#include "data/decoderstack.h"
#include "data/disk_cache_config.h"
#include "data/dsosnapshot.h"
#include "data/lissajousmodel.h"
#include "data/logicsnapshot.h"
#include "data/mathstack.h"
#include "data/sessionsnapshot.h"
#include "data/signalmodel.h"
#include "data/spectrumstack.h"
#include "interface/events.h"

#include <QCoreApplication>
#include <QDir>
#include <QObject>
#include <QString>
#include <assert.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <functional>
#include <map>
#include <stdexcept>
#include <sys/stat.h>

#include "config/appconfig.h"
#include "data/decode/decoderstatus.h"
#include "dsvdef.h"
#include "log.h"
#include "ui/langresource.h"
#include "ui/msgbox.h"
#include "utility/path.h"

// Upstream libsigrok 0.6.0 is now the sole libsigrok (fork + bridge removed).
// All ds_* fork APIs are replaced by sr_* upstream APIs.

namespace pv {
SessionData::SessionData() {
  _cur_snap_samplerate = 0;
  _cur_samplelimits = 0;
  _trig_pos = 0;
  _logic_backup = nullptr;
  _glitch_filter_active = false;
  _glitch_filter_modes.clear();
  _signal_invert_active = false;
}

void SessionData::clear() {
  logic.clear();
  analog.clear();
  dso.clear();
  _trig_pos = 0;
  if (_logic_backup) {
    delete _logic_backup;
    _logic_backup = nullptr;
  }
  _glitch_filter_active = false;
  _glitch_filter_thresholds.clear();
  _glitch_filter_modes.clear();
  _signal_invert_active = false;
  _signal_invert_channels.clear();
}

// --- Dispatch helpers (forward to SessionStateContext) ---
// These were migrated to SessionStateContext in modernize-core-layer-radical
// phase 1. SigSession retains the methods for backward compat with View/API
// callers, but just forwards to _state.

void SigSession::data_updated() { _state->data_updated(); }
void SigSession::set_receive_data_len(quint64 len) { _state->set_receive_data_len(len); }
void SigSession::receive_header() { _state->receive_header(); }
void SigSession::cur_snap_samplerate_changed() { _state->cur_snap_samplerate_changed(); }
void SigSession::frame_began() { _state->frame_began(); }
void SigSession::frame_ended() { _state->frame_ended(); }
void SigSession::update_capture() { _state->update_capture(); }
void SigSession::repeat_hold(int percent) { _state->repeat_hold(percent); }
void SigSession::receive_trigger(quint64 trigger_pos) { _state->receive_trigger(trigger_pos); }
void SigSession::show_wait_trigger() { _state->show_wait_trigger(); }
void SigSession::signals_changed() { _state->signals_changed(); }
void SigSession::session_error() { _state->session_error(); }
void SigSession::delay_prop_msg(QString strMsg) { _state->delay_prop_msg(strMsg); }

// _empty_decoder_stacks static member removed: SessionStateContext now hosts
// its own file-static _empty_decoder_stacks (see sessionstatecontext.cpp) and
// exposes it via get_decoder_stacks(). SigSession::get_decoder_stacks forwards.

SigSession::SigSession() {
  _decoder_pannel = NULL;

  // SessionStateContext owns all shared mutable state (mutexes, signal models,
  // device agent, view/capture data, atomic flags, trigger config, etc.).
  // Its constructor initializes _sampling_mutex/_data_mutex (via make_unique),
  // _data_list (with 2 SessionData entries), _view_data/_capture_data (both
  // pointing to _data_list[0]), and all bool/atomic/numeric fields with their
  // default values.
  _state = std::make_unique<core::SessionStateContext>();

  // EventBus must be constructed before add_event_listener(this), since
  // add_event_listener forwards to _event_bus. All typed event dispatch goes
  // through broadcast<T>() / broadcast_sync<T>() / broadcast_async<T>().
  _event_bus = std::make_unique<core::EventBus>();
  _state->set_event_bus(_event_bus.get());
  // SigSession is now an IEventListener. It registers to receive the 5
  // Core-internal state-machine typed events whose logic lives in on_event
  // overrides (DeviceOptionsUpdated / TrigNextCollect / RevEndPacket /
  // CopyToDocDone / DeviceSpeedNotMatch).
  _event_bus->add_event_listener(this);

  // Managers are constructed after _event_bus (they hold a raw pointer to it)
  // and after _state (they hold a raw pointer to it). FilterProcessor accesses
  // _state->view_data(), which is already initialized by SessionStateContext's
  // constructor.
  _filter_processor = std::make_unique<core::FilterProcessor>(_event_bus.get(),
                                                              _state.get());
  _decode_task_manager = std::make_unique<core::DecodeTaskManager>(
      _event_bus.get(), _state.get());
  _data_feed_parser = std::make_unique<core::DataFeedParser>(_event_bus.get(),
                                                             _state.get());
  _document_registry = std::make_unique<core::DocumentRegistry>(
      _event_bus.get(), _state.get());
  // CaptureManager owns the capture lifecycle + DsTimer instances + the
  // _is_instant / _clt_mode / _data_lock / _repeat_intvl / _dso_packet_count
  // / _disk_cache_config state. Constructed after _document_registry because
  // action_start_capture calls _document_registry->acquire_capture_owner().
  _capture_manager = std::make_unique<core::CaptureManager>(_event_bus.get(),
                                                            _state.get());

  // Inject manager back-pointers into _state so cross-manager helpers
  // (decode_traces / attach_data_to_signal / sync_trigger_to_libsigrok /
  // clear_all_decode_task2 / etc.) can dispatch to the right manager.
  _state->set_capture_manager(_capture_manager.get());
  _state->set_decode_task_manager(_decode_task_manager.get());
  _state->set_data_feed_parser(_data_feed_parser.get());
  _state->set_document_registry(_document_registry.get());
  _state->set_filter_processor(_filter_processor.get());

  _state->device_agent().set_callback(this);
  // Wire the datafeed callback so DeviceAgent registers it with sr_session
  // when open_by_handle creates the session. The callback trampoline lives
  // on DataFeedParser (static method); user_data is the parser instance.
  _state->device_agent().set_datafeed_callback(
      &core::DataFeedParser::data_feed_callback_ex,
      _data_feed_parser.get());
}

SigSession::SigSession(SigSession &o) { (void)o; }

SigSession::~SigSession() {
  // A3 fix: ensure Close() has been called so background threads (decode/copy/
  // glitch_filter/signal_invert) are joined before we destroy _state.
  // Close() is idempotent (_bClose guard), so calling it here is safe even
  // if already called via uninit().
  Close();

  // Unregister as IEventListener before _event_bus is destroyed (unique_ptr
  // member, destroyed after the destructor body runs).
  if (_event_bus)
    _event_bus->remove_event_listener(this);

  // _state destructor clears _data_list entries. Managers (unique_ptrs) are
  // destroyed before _state due to reverse declaration order in sigsession.h,
  // so manager back-pointers in _state are already dangling-but-unused by the
  // time _state is destroyed.
}

// libsigrok log callback: forward sr_err/sr_warn/sr_info/sr_dbg into PXView's
// xlog system so driver-internal failures (e.g. fx2lafw_dev_open libusb errors,
// firmware version mismatch, interface claim failures) are visible in PXView.log.
// Without this, sr_err output goes to stderr and is invisible in a GUI app,
// leaving only "sr_dev_open failed" with no root cause.
static int sigrok_log_callback(void *cb_data, int loglevel,
                               const char *format, va_list args)
{
  (void)cb_data;
  char buf[1024];
  vsnprintf(buf, sizeof(buf), format, args);
  // Strip trailing newline added by sr_log_vprintf to keep xlog format clean.
  size_t n = strlen(buf);
  while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
    buf[--n] = 0;

  switch (loglevel) {
    case SR_LOG_ERR:
      pxv_err("sr: %s", buf);
      break;
    case SR_LOG_WARN:
      pxv_warn("sr: %s", buf);
      break;
    case SR_LOG_INFO:
      pxv_info("sr: %s", buf);
      break;
    case SR_LOG_DBG:
    case SR_LOG_SPEW:
      pxv_dbg("sr: %s", buf);
      break;
    default:
      break;
  }
  return 0;
}

bool SigSession::init() {
  // Upstream libsigrok 0.6.0 initialization (sole libsigrok after fork removal).
  // sr_init creates the sr_context which holds the libusb_context, driver list,
  // and resource hooks. The datafeed callback is registered per-session in
  // start_capture via sr_session_datafeed_callback_add.
  if (sr_init(&_sr_ctx) != SR_OK) {
    pxv_err("PXView run ERROR: libsigrok init failed.");
    return false;
  }

  // Forward libsigrok internal logs (sr_err/sr_warn/sr_info/sr_dbg) into
  // PXView's xlog so driver failures are observable in PXView.log. Without
  // this, GUI mode swallows sr_err output and only "sr_dev_open failed"
  // remains, hiding the root cause (libusb open/claim errors, fw version
  // mismatch, etc.). Set to SR_LOG_DBG so device-open failures include the
  // sr_dbg "Opening device instance" trace + driver sr_err details.
  sr_log_callback_set(sigrok_log_callback, nullptr);
  sr_log_loglevel_set(SR_LOG_DBG);

  // Diagnostic: log every firmware search path libsigrok will consult, so
  // "Failed to locate 'fx2lafw-cypress-fx2.fw'" can be cross-checked against
  // this list. PulseView finds the same file in <appdir>/share/sigrok-firmware,
  // so the question is whether g_get_system_data_dirs() returns that path.
  GSList *fw_paths = sr_resourcepaths_get(SR_RESOURCE_FIRMWARE);
  pxv_info("libsigrok firmware search paths:");
  for (GSList *p = fw_paths; p; p = p->next) {
    pxv_info("  -> %s", p->data ? (const char *)p->data : "(null)");
  }
  g_slist_free_full(fw_paths, g_free);

  pxv_info("libsigrok initialized (upstream 0.6.0, sole library)");
  return true;
}

void SigSession::uninit() {
  this->Close();

  // DeviceAgent owns sr_session; it is destroyed in release()/destructor.
  // Just tear down the sr_context here.
  if (_sr_ctx) {
    sr_exit(_sr_ctx);
    _sr_ctx = nullptr;
  }
}

bool SigSession::set_default_device() {
  assert(!_state->is_saving());

  if (_state->is_working()) {
    pxv_info("SigSession::set_default_device()，The current device is working, "
             "now to stop it.");
    pxv_info("SigSession::set_default_device(), stop capture");
    stop_capture();
  }

  // Use the device list to pick the last device (matches fork behavior:
  // the most recently scanned device becomes the default).
  int count = 0;
  int actived_index = -1;
  struct ds_device_base_info *array = get_device_list(count, actived_index);
  if (count < 1 || array == NULL) {
    pxv_err("Error! Device list is empty, can't set default device.");
    if (array)
      free(array);
    return false;
  }

  // Pick the last device (matches fork ds_get_device_list behavior where
  // the last entry is the most recently scanned/added device).
  struct ds_device_base_info *dev = (array + count - 1);
  ds_device_handle dev_handle = dev->handle;

  free(array);

  if (set_device(dev_handle)) {
    return true;
  }
  return false;
}

bool SigSession::set_device(ds_device_handle dev_handle) {
  assert(!_state->is_saving());
  assert(!_state->is_working());
  assert(_event_bus && _event_bus->has_callbacks());

  // modernize-core-layer-radical Task 11: pre-broadcast synchronously so
  // MainWindow can close modal dialogs / hide calibration / delete protocols
  // / reload the view BEFORE the old device is released below.
  // Caller (set_device) is on the main thread (user-initiated action).
  _event_bus->broadcast_sync<interface::CurrentDeviceChangePrev>({});
  // Release the old device.
  _state->device_agent().release();
  _state->set_device_status(ST_INIT);

  // Open the new device via DeviceAgent (handles sr_dev_open + channel setup).
  if (!_state->device_agent().open_by_handle(dev_handle, _sr_ctx)) {
    pxv_err("Switch device error!");
    // Broadcast DeviceOpenFailed so MainWindow can show a user-facing message
    // ("Failed to open device: <reason>") instead of leaving the UI blank.
    // The old device was already released above and the new one never opened,
    // so _dev_handle is NULL — without this event, the UI silently stays empty
    // and "_dev_handle is NULL" warnings flood the log.
    _event_bus->broadcast_async<interface::DeviceOpenFailed>({});
    return false;
  }

  _state->device_agent().update();
  set_collect_mode(COLLECT_SINGLE);

  if (_state->device_agent().is_file()) {
    std::string dev_name = pv::path::ToUnicodePath(_state->device_agent().name());
    pxv_info("Switch to file \"%s\" done.", dev_name.c_str());
  } else
    pxv_info("Switch to device \"%s\" done.",
             _state->device_agent().name().toUtf8().data());

  clear_all_documents_decoders();

  _state->view_data()->clear();
  _state->capture_data()->clear();
  _state->set_capture_data(_state->view_data());

  init_signals();

  set_cur_snap_samplerate(_state->device_agent().get_sample_rate());
  set_cur_samplelimits(_state->device_agent().get_sample_limit());

  // The current device changed.
  _event_bus->broadcast_async<interface::CurrentDeviceChanged>({});

  return true;
}

bool SigSession::set_file(QString name) {
  assert(!_state->is_saving());
  assert(!_state->is_working());

  std::string file_name = pv::path::ToUnicodePath(name);
  pxv_info("Load file: \"%s\"", file_name.c_str());

  // Use upstream sr_input API to load session files.
  const struct sr_input *in = nullptr;
  if (sr_input_scan_file(file_name.c_str(), &in) != SR_OK) {
    pxv_err("Load file error!");
    return false;
  }

  // Get the device instance from the input and register it via DeviceAgent.
  struct sr_dev_inst *sdi = sr_input_dev_inst_get(in);
  if (!sdi) {
    pxv_err("Load file error: no device instance!");
    return false;
  }

  // Register the file-loaded device with DeviceAgent.
  _state->device_agent().set_file_device(sdi, name);

  return set_default_device();
}

void SigSession::close_file(ds_device_handle dev_handle) {
  if (!dev_handle) {
    pxv_warn("%s", "SigSession::close_file: dev_handle is NULL");
    return;
  }

  if (dev_handle == _state->device_agent().handle() && _state->is_working()) {
    pxv_err("The virtual device is running, can't remove it.");
    return;
  }
  bool isCurrent = dev_handle == _state->device_agent().handle();

  // Remove the device from DeviceAgent's tracked list.
  _state->device_agent().remove_device(dev_handle);

  if (isCurrent)
    set_default_device();
}

bool SigSession::have_hardware_data() {
  if (_state->device_agent().have_instance() && _state->device_agent().is_hardware()) {
    Snapshot *data = get_signal_snapshot();
    return data->have_data();
  }
  return false;
}

struct ds_device_base_info *SigSession::get_device_list(int &out_count,
                                                        int &actived_index) {
  out_count = 0;
  actived_index = -1;

  // Scan all upstream drivers via sr_driver_list + sr_driver_scan.
  // Build a ds_device_base_info array compatible with the existing API.
  if (!_sr_ctx) {
    return nullptr;
  }

  struct sr_dev_driver **drivers = sr_driver_list(_sr_ctx);
  if (!drivers) {
    return nullptr;
  }

  // Collect all scanned devices into a temporary vector.
  std::vector<struct sr_dev_inst *> all_sdi;
  for (int i = 0; drivers[i]; i++) {
    struct sr_dev_driver *drv = drivers[i];
    if (!drv)
      continue;
    // Initialize driver on first use.
    if (sr_driver_init(_sr_ctx, drv) != SR_OK)
      continue;
    GSList *devs = sr_driver_scan(drv, nullptr);
    for (GSList *l = devs; l; l = l->next) {
      struct sr_dev_inst *sdi = (struct sr_dev_inst *)l->data;
      if (sdi)
        all_sdi.push_back(sdi);
    }
    // Note: sr_driver_scan returns a list owned by the driver; do not free.
  }

  // Also include any file-loaded devices tracked by DeviceAgent.
  auto &file_devs = _state->device_agent().file_devices();
  for (auto sdi : file_devs) {
    if (sdi)
      all_sdi.push_back(sdi);
  }

  if (all_sdi.empty()) {
    return nullptr;
  }

  // Allocate (count + 1) entries; last entry is a sentinel with handle=0.
  int count = (int)all_sdi.size();
  struct ds_device_base_info *array = (struct ds_device_base_info *)
      calloc(count + 1, sizeof(struct ds_device_base_info));
  if (!array) {
    return nullptr;
  }

  // Fill entries. Handle = index+1 (0 is reserved for NULL_HANDLE sentinel).
  for (int i = 0; i < count; i++) {
    struct ds_device_base_info *entry = &array[i];
    entry->handle = (ds_device_handle)(i + 1);

    // Build display name from vendor/model/conn fields.
    const char *vendor = sr_dev_inst_vendor_get(all_sdi[i]);
    const char *model = sr_dev_inst_model_get(all_sdi[i]);
    const char *conn = sr_dev_inst_connid_get(all_sdi[i]);

    char name_buf[150] = {0};
    if (vendor && model) {
      snprintf(name_buf, sizeof(name_buf), "%s %s", vendor, model);
    } else if (model) {
      snprintf(name_buf, sizeof(name_buf), "%s", model);
    } else if (conn) {
      snprintf(name_buf, sizeof(name_buf), "%s", conn);
    } else {
      snprintf(name_buf, sizeof(name_buf), "device-%d", i);
    }
    strncpy(entry->name, name_buf, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
  }

  // Sentinel.
  array[count].handle = 0;
  array[count].name[0] = '\0';

  out_count = count;
  // actived_index: track via DeviceAgent's current handle.
  ds_device_handle cur = _state->device_agent().handle();
  actived_index = (cur > 0 && cur <= (ds_device_handle)count) ? (int)(cur - 1) : -1;

  // Register the scanned SDI list with DeviceAgent so set_device() can
  // open the right device by handle (index+1).
  _state->device_agent().set_scanned_devices(all_sdi);

  return array;
}

uint64_t SigSession::cur_samplerate() {
  // samplerate for current viewport
  if (_state->device_agent().get_work_mode() == DSO)
    return _state->device_agent().get_sample_rate();
  else
    return cur_snap_samplerate();
}

uint64_t SigSession::cur_snap_samplerate() {
  // samplerate for current snapshot
  return _state->capture_data()->_cur_snap_samplerate;
}

uint64_t SigSession::cur_samplelimits() {
  return _state->capture_data()->_cur_samplelimits;
}

double SigSession::cur_sampletime() {
  return cur_samplelimits() * 1.0 / cur_samplerate();
}

double SigSession::cur_snap_sampletime() {
  return cur_samplelimits() * 1.0 / cur_snap_samplerate();
}

double SigSession::get_logic_data_view_time() {
  return _state->view_data()->get_logic()->get_ring_sample_count() * 1.0 /
         cur_snap_samplerate();
}

double SigSession::cur_view_time() {
  return _state->device_agent().get_time_base() * DS_CONF_DSO_HDIVS * 1.0 / SR_SEC(1);
}

void SigSession::set_cur_snap_samplerate(uint64_t samplerate) {
  if (samplerate == 0) {
    pxv_err("set_cur_snap_samplerate: samplerate=0, ignoring");
    return;
  }

  _state->capture_data()->_cur_snap_samplerate = samplerate;
  _state->capture_data()->get_logic()->set_samplerate(samplerate);
  _state->capture_data()->get_analog()->set_samplerate(samplerate);
  _state->capture_data()->get_dso()->set_samplerate(samplerate);

  int mode = _state->device_agent().get_work_mode();

  if (mode == DSO) {
    for (auto m : _state->signal_models()) {
      if (m->type() == SR_CHANNEL_DSO) {
        // TODO: verify - vfactor and vdiv replace view::DsoSignal getters.
        _state->capture_data()->get_dso()->set_measure_voltage_factor(
            (uint64_t)m->vfactor(), m->index());
        _state->capture_data()->get_dso()->set_data_scale(m->vdiv(), m->index());
      }
    }
  }

  // DecoderStack
  for (auto d : decode_traces()) {
    d->set_samplerate(samplerate);
  }

  // Math
  if (_state->math_stack())
    _state->math_stack()->set_samplerate(_state->device_agent().get_sample_rate());
  // SpectrumStack
  for (auto m : _state->spectrum_stacks()) {
    m->set_samplerate(samplerate);
  }

  cur_snap_samplerate_changed();
}

void SigSession::set_cur_samplelimits(uint64_t samplelimits) {
  if (samplelimits == 0) {
    pxv_err("set_cur_samplelimits: samplelimits=0, ignoring");
    return;
  }
  _state->capture_data()->_cur_samplelimits = samplelimits;
  // R1: symmetric to set_cur_snap_samplerate which fires
  // cur_snap_samplerate_changed(); notify capture listeners that the
  // sample limit changed.
  dispatch_to<ICaptureCallback>(
      [](ICaptureCallback *cb) { cb->cur_samplelimits_changed(); });
}

std::vector<std::shared_ptr<data::SignalModel>> &
SigSession::get_signal_models() {
  return _state->signal_models();
}

void SigSession::init_signals() {
  if (_state->device_agent().have_instance() == false) {
    pxv_err("init_signals: no device instance, aborting");
    return;
  }

  std::vector<std::shared_ptr<data::SignalModel>> models;
  unsigned int logic_probe_count = 0;
  unsigned int dso_probe_count = 0;
  unsigned int analog_probe_count = 0;

  _state->capture_data()->clear();
  _state->view_data()->clear();
  set_cur_snap_samplerate(_state->device_agent().get_sample_rate());
  set_cur_samplelimits(_state->device_agent().get_sample_limit());

  // Detect what data types we will receive
  if (_state->device_agent().have_instance()) {
    for (const GSList *l = _state->device_agent().get_channels(); l; l = l->next) {
      const sr_channel *const probe = (const sr_channel *)l->data;

      switch (probe->type) {
      case SR_CHANNEL_LOGIC:
        if (probe->enabled)
          logic_probe_count++;
        break;

      case SR_CHANNEL_DSO:
        dso_probe_count++;
        break;

      case SR_CHANNEL_ANALOG:
        if (probe->enabled)
          analog_probe_count++;
        break;
      }
    }
  }

  int mode = _state->device_agent().get_work_mode();
  int channel_count = g_slist_length((GSList *)_state->device_agent().get_channels());
  pxv_info("SigSession::init_signals() start. mode=%d, channel_count=%d", mode, channel_count);

  for (GSList *l = _state->device_agent().get_channels(); l; l = l->next) {
    sr_channel *probe = (sr_channel *)l->data;
    if (!probe) {
      pxv_warn("%s", "SigSession: probe is NULL in channel loop, skipping");
      continue;
    }
    assert(probe);

    // Allow LOGIC + ANALOG coexistence (PulseView native behavior).
    // DSL hardware requiring mutual exclusion is dropped in PXView.
    // DSO mode still filters (deprecated, hardware-specific).
    if (mode == DSO && probe->type != SR_CHANNEL_DSO) {
      pxv_info("init_signals probe skip: mode=DSO but probe->type=%d", probe->type);
      continue;
    }

    pxv_info("init_signals probe examine: index=%d name=%s type=%d enabled=%d",
             probe->index, probe->name ? probe->name : "null", probe->type, probe->enabled);

    bool should_create = false;
    int ch_type = SR_CHANNEL_LOGIC;

    switch (probe->type) {
    case SR_CHANNEL_LOGIC:
      if (probe->enabled) {
        should_create = true;
        ch_type = SR_CHANNEL_LOGIC;
      }
      break;

    case SR_CHANNEL_DSO:
      should_create = true;
      ch_type = SR_CHANNEL_DSO;
      break;

    case SR_CHANNEL_ANALOG:
      if (probe->enabled) {
        should_create = true;
        ch_type = SR_CHANNEL_ANALOG;
      }
      break;
    }

    if (should_create) {
      auto model = std::make_shared<data::SignalModel>();
      model->set_index(probe->index);
      model->set_name(probe->name ? probe->name : "");
      model->set_type(ch_type);
      model->set_enabled(probe->enabled);

      // Inject weak references so the model can write back to the
      // sr_channel struct and the DeviceAgent API. See
      // SignalModel::commit_to_device() and the enhanced setters.
      model->set_session(this);
      model->set_sr_channel(probe);

      // Read probe configuration for DSO/ANALOG channels.
      // Sources must match view::DsoSignal/AnalogSignal getters so that the
      // SignalModel mirrors what the View layer reports:
      //   - vfactor    <- SR_CONF_PROBE_FACTOR    (DsoSignal::get_factor)
      //   - hw_offset  <- SR_CONF_PROBE_HW_OFFSET (DsoSignal::get_hw_offset)
      //   - zero_offset<- SR_CONF_PROBE_OFFSET    (DsoSignal::load_settings)
      // vdiv / coupling were fork DSO keys (deleted); model defaults are used.
      // Use typed wrappers (DeviceAgent::get_probe_factor / get_probe_hw_offset
      // / get_probe_offset / get_probe_map_default) — they short-circuit on
      // non-DSL devices (is_dsl_device() guard) so we don't flood the log with
      // "Option 'probe_factor' not available" errors on demo/fx2lafw.
      if (ch_type == SR_CHANNEL_DSO ||
          ch_type == SR_CHANNEL_ANALOG) {
        uint64_t vfactor = 1;
        if (_state->device_agent().get_probe_factor(vfactor, probe))
          model->set_vfactor((double)vfactor);
        else
          model->set_vfactor(1.0);

        bool map_default = true;
        _state->device_agent().get_probe_map_default(map_default, probe);
        model->set_map_default(map_default);

        int hw_offset = 0;
        _state->device_agent().get_probe_hw_offset(hw_offset, probe);
        model->set_hw_offset(hw_offset);

        int zero_offset = 0;
        _state->device_agent().get_probe_offset(zero_offset, probe);
        model->set_zero_offset(zero_offset);
      }

      models.push_back(model);
    }
  }

  clear_signals();
  std::vector<std::shared_ptr<data::SignalModel>>().swap(_state->signal_models());
  _state->signal_models() = models;
  make_channels_view_index();

  spectrum_rebuild();
  lissajous_disable();
  math_disable();

  // Notify View layer to rebuild signals from the new SignalModels.
  // Without this, LogicSignals keep stale model pointers (old models were
  // deleted above) and never receive property-change notifications.
  signals_changed();

  if (_state->signal_models().empty()) {
    pxv_info("ERROR: Unable to create any channel. (models is empty)");
  } else {
    pxv_info("SigSession::init_signals() end. models.size()=%d", (int)_state->signal_models().size());
  }
}

void SigSession::reload() {
  if (_state->device_agent().have_instance() == false) {
    pxv_err("reload: no device instance, aborting");
    return;
  }

  if (_state->is_working())
    return;

  std::vector<std::shared_ptr<data::SignalModel>> models;
  int mode = _state->device_agent().get_work_mode();
  int channel_count = g_slist_length((GSList *)_state->device_agent().get_channels());
  pxv_info("SigSession::reload() start. mode=%d, channel_count=%d", mode, channel_count);

  set_cur_snap_samplerate(_state->device_agent().get_sample_rate());
  set_cur_samplelimits(_state->device_agent().get_sample_limit());

  for (GSList *l = _state->device_agent().get_channels(); l; l = l->next) {
    sr_channel *probe = (sr_channel *)l->data;
    if (!probe) {
      pxv_warn("%s", "SigSession: probe is NULL in channel loop, skipping");
      continue;
    }
    assert(probe);

    // Allow LOGIC + ANALOG coexistence (PulseView native behavior).
    // DSL hardware requiring mutual exclusion is dropped in PXView.
    // DSO mode still filters (deprecated, hardware-specific).
    if (mode == DSO && probe->type != SR_CHANNEL_DSO) {
      pxv_info("reload probe skip: mode=DSO but probe->type=%d", probe->type);
      continue;
    }

    pxv_info("reload probe examine: index=%d name=%s type=%d enabled=%d",
             probe->index, probe->name ? probe->name : "null", probe->type, probe->enabled);

    bool should_create = false;
    int ch_type = SR_CHANNEL_LOGIC;

    switch (probe->type) {
    case SR_CHANNEL_LOGIC:
      if (probe->enabled) {
        should_create = true;
        ch_type = SR_CHANNEL_LOGIC;
      }
      break;

    case SR_CHANNEL_DSO:
      should_create = true;
      ch_type = SR_CHANNEL_DSO;
      break;

    case SR_CHANNEL_ANALOG:
      if (probe->enabled) {
        should_create = true;
        ch_type = SR_CHANNEL_ANALOG;
      }
      break;
    }

    if (should_create) {
      // Try to preserve settings from the existing model with the same index
      std::shared_ptr<data::SignalModel> old_model = nullptr;
      for (auto &m : _state->signal_models()) {
        if (m->index() == (int)probe->index) {
          old_model = m;
          break;
        }
      }

      auto model = std::make_shared<data::SignalModel>();
      model->set_index(probe->index);
      model->set_name(probe->name ? probe->name : "");
      model->set_type(ch_type);
      model->set_enabled(probe->enabled);

      // Inject weak references (same as init_signals) so the rebuilt model
      // can write back to sr_channel / DeviceAgent and so
      // commit_to_device() works after reload.
      model->set_session(this);
      model->set_sr_channel(probe);

      if (ch_type == SR_CHANNEL_DSO ||
          ch_type == SR_CHANNEL_ANALOG) {
        // vdiv / coupling were fork DSO keys (deleted); model defaults are used.
        // Use typed wrappers (is_dsl_device() guard) — non-DSL devices skip
        // the queries entirely, avoiding "not available" log noise on demo.
        uint64_t vfactor = 1;
        if (_state->device_agent().get_probe_factor(vfactor, probe))
          model->set_vfactor((double)vfactor);
        else
          model->set_vfactor(1.0);

        bool map_default = true;
        _state->device_agent().get_probe_map_default(map_default, probe);
        model->set_map_default(map_default);

        int hw_offset = 0;
        _state->device_agent().get_probe_hw_offset(hw_offset, probe);
        model->set_hw_offset(hw_offset);

        int zero_offset = 0;
        _state->device_agent().get_probe_offset(zero_offset, probe);
        model->set_zero_offset(zero_offset);
      }

      if (old_model) {
        model->set_trig_type(old_model->trig_type());
        model->set_color(old_model->color());
      }

      models.push_back(model);
    }
  }

  if (!models.empty()) {
    pxv_info("SigSession::reload() end. clear signals, models.size()=%d", (int)models.size());
    clear_signals();
    std::vector<std::shared_ptr<data::SignalModel>>().swap(_state->signal_models());
    _state->signal_models() = models;
    make_channels_view_index();
  } else if (mode == LOGIC || mode == ANALOG || mode == DSO) {
    pxv_info("ERROR: Unable to create any channel in reload(). channels is empty or all skipped.");
    clear_signals();
  }

  spectrum_rebuild();

  // CRITICAL: reload() wholesale-replaces _state->signal_models() (new shared_ptr
  // objects, old ones destroyed). Without signals_changed(), the View's
  // DsoSignal/AnalogSignal keep stale _model pointers to the freed
  // SignalModels (0xfeeefeee), and any subsequent access UAFs. This is
  // symmetric with init_signals() which also ends with signals_changed().
  // Trigger path: load_config_from_json -> _session->reload() -> [here]
  // -> immediately after, load_config_from_json iterates
  // current_view()->get_own_signals() and calls DsoSignal::set_zero_ratio ->
  // _model->set_zero_offset. Without this notification the _model is dangling.
  // compute_change_event detects the pointer identity change and returns
  // AllReplaced, so View fully rebinds _model to the new SignalModels.
  // NOTE: reload() now handles DSO mode (case SR_CHANNEL_DSO), same as
  // init_signals(). Previously reload() skipped DSO channels entirely, making
  // it a no-op in DSO mode — so load_config_from_json's probe property
  // updates (vdiv/coupling/vfactor) were never reflected in SignalModel, and
  // the View kept stale _model pointers.
  signals_changed();
}

uint16_t SigSession::get_ch_num(int type) {
  uint16_t num_channels = 0;
  uint16_t logic_ch_num = 0;
  uint16_t dso_ch_num = 0;
  uint16_t analog_ch_num = 0;

  if (_state->device_agent().have_instance()) {
    for (auto m : _state->signal_models()) {
      if (!m->enabled())
        continue;

      if (m->type() == SR_CHANNEL_LOGIC)
        logic_ch_num++;
      else if (m->type() == SR_CHANNEL_DSO)
        dso_ch_num++;
      else if (m->type() == SR_CHANNEL_ANALOG)
        analog_ch_num++;
    }
  }

  switch (type) {
  case SR_CHANNEL_LOGIC:
    num_channels = logic_ch_num;
    break;
  case SR_CHANNEL_DSO:
    num_channels = dso_ch_num;
    break;
  case SR_CHANNEL_ANALOG:
    num_channels = analog_ch_num;
    break;
  default:
    num_channels = logic_ch_num + dso_ch_num + analog_ch_num;
    break;
  }

  return num_channels;
}

std::vector<std::shared_ptr<data::DecoderStack>> &
SigSession::get_decoder_stacks(data::SessionDocument *doc) {
  return _state->get_decoder_stacks(doc);
}

bool SigSession::add_decoder(
    srd_decoder *const dec, bool silent, DecoderStatus *dstatus,
    std::list<pv::data::decode::Decoder *> &sub_decoders,
    std::shared_ptr<data::DecoderStack> &out_stack,
    data::SessionDocument *doc) {
  (void)silent;
  if (dec == NULL) {
    pxv_err("Decoder instance is null!");
    return false;
  }

  data::SessionDocument *target = doc ? doc : _document_registry->get_active_document();

  out_stack = nullptr;

  try {
    bool ret = false;

    // Create the decoder
    std::map<const srd_channel *, int> probes;
    auto decoder_stack =
        std::make_shared<data::DecoderStack>(this, dec, dstatus);
    assert(decoder_stack);
    // Assign a unique handle id so the API/MCP layer can stably reference
    // this stack. A re-created stack (e.g. via a future add_decoder call)
    // always receives a fresh id, distinguishing it from reused stacks.
    decoder_stack->set_handle_id(_state->next_decoder_handle_id());

    // Make a list of all the probes
    std::vector<const srd_channel *> all_probes;

    for (const GSList *i = dec->channels; i; i = i->next) {
      all_probes.push_back((const srd_channel *)i->data);
    }

    for (const GSList *i = dec->opt_channels; i; i = i->next) {
      all_probes.push_back((const srd_channel *)i->data);
    }

    decoder_stack->stack().front()->set_probes(probes);

    // add sub decoder
    for (auto sub : sub_decoders) {
      decoder_stack->add_sub_decoder(sub);
    }

    if (sub_decoders.size() > 0) {
      auto lst_sub = sub_decoders.end();
      lst_sub--;
      QString sub_dec_name((*lst_sub)->decoder()->name);
      if (sub_dec_name != "") {
        // TODO: verify - decoder name was previously set on view::DecodeTrace.
        // DecoderStack has no set_name method; name management needs a new
        // mechanism.
      }
    }

    sub_decoders.clear();

    // The decoder options dialog (DecodeTrace::create_popup) is now shown
    // by the View layer (View::add_decoder) after Core returns the newly
    // created DecoderStack. Core never touches Qt Widgets, so it always
    // reports success here regardless of `silent`. The `silent` parameter
    // is kept for API compatibility but no longer triggers automatic
    // decode-task startup here (see the NOTE below).
    ret = true;

    if (ret) {
      if (target) {
        target->get_decoder_stacks().push_back(decoder_stack);
      }
      // When target is null (neither doc nor _active_document is bound, a
      // rare edge case now that MCP uses _api_document and UI uses
      // _active_document), the newly created DecoderStack is intentionally
      // NOT stored in any container — it is returned via out_stack and the
      // caller owns it. set_owner_document(nullptr) is safe (simple setter).
      // The legacy _empty_decoder_stacks staging path was removed together
      // with the set_active_document migration logic.
      decoder_stack->set_owner_document(target);

      // NOTE: Starting the decode task here is intentionally avoided.
      // Previously this called `add_decode_task(decoder_stack)` when
      // (!silent && have_view_data()). However, after de-view-ization the
      // decoder options dialog (DecodeTrace::create_popup) is shown by
      // the View layer AFTER this method returns, so the user has not yet
      // had a chance to configure channel mappings when we would start the
      // decode thread. The decode thread would then run with empty probes
      // and bail out with "required channels have not been specified".
      //
      // Callers are responsible for starting the decode task at the right
      // time:
      //   - UI path (View::add_decoder): after create_popup() returns true
      //     (user accepted the dialog and configured channels).
      //   - MCP path (SessionService::add_decoder): via
      //     QTimer::singleShot(0, ...) after do_add() returns to avoid
      //     Qt signal races during rebuild_decoder_pannel().
      //   - Capture pipeline: when CopyToDocDone fires and the
      //     stack was added before capture, frame_ended() + add_decode_task()
      //     is invoked by the message handler.
      data_updated();

      out_stack = decoder_stack;
    }

    return ret;
  } catch (...) {
    pxv_err("Error!add_decoder() throws an exception.");
  }

  return false;
}

int SigSession::get_trace_index_by_key_handel(void *handel,
                                              data::SessionDocument *doc) {
  int dex = 0;

  for (auto stack : decode_traces(doc)) {
    if (stack->get_key_handel() == handel) {
      return dex;
    }
    ++dex;
  }

  return -1;
}

void SigSession::remove_decoder(int index, data::SessionDocument *doc) {
  data::SessionDocument *target = doc ? doc : _document_registry->get_active_document();
  int size = (int)decode_traces(target).size();
  (void)size;
  assert(index < size);

  auto it = decode_traces(target).begin() + index;
  auto stack = (*it);
  decode_traces(target).erase(it);

  // decode_traces(target) returns target->get_decoder_stacks() (or
  // _empty_decoder_stacks), so the erase above already removed it from the
  // document's list.

  // Stop the decode work and mark for deletion
  remove_decode_task(stack);
  stack->_delete_flag = true;

  // Check if the decode thread is still using this stack.
  // We must NOT join threads here as that can deadlock
  // (decode thread may need the main thread for Qt signals).
  bool thread_holds_stack = _decode_task_manager->is_task_running(stack);

  if (!thread_holds_stack) {
    signals_changed();
  }
  // If thread still holds the stack, decode_single_task will
  // delete it via DESTROY_QT_LATER when it sees _delete_flag
}

void SigSession::remove_decoder_by_key_handel(void *handel,
                                              data::SessionDocument *doc) {
  data::SessionDocument *target = doc ? doc : _document_registry->get_active_document();
  int dex = get_trace_index_by_key_handel(handel, target);
  remove_decoder(dex, target);
}

void SigSession::spectrum_rebuild() {
  bool has_dso_signal = false;

  for (auto m : _state->signal_models()) {
    if (m->type() == SR_CHANNEL_DSO) {
      has_dso_signal = true;
      // check already have
      auto iter = _state->spectrum_stacks().begin();

      for (unsigned int i = 0; i < _state->spectrum_stacks().size(); i++, iter++) {
        if ((*iter)->get_index() == m->index())
          break;
      }

      // if not, rebuild
      if (iter == _state->spectrum_stacks().end()) {
        auto spectrum_stack =
            std::make_shared<data::SpectrumStack>(this, m->index());
        _state->spectrum_stacks().push_back(spectrum_stack);
      }
    }
  }

  if (!has_dso_signal) {
    _state->spectrum_stacks().clear();
  }

  signals_changed();
}

void SigSession::lissajous_rebuild(bool enable, int xindex, int yindex,
                                   double percent) {
  delete _state->lissajous_model();
  auto *m = new data::LissajousModel();
  m->set_enabled(enable);
  m->set_x_index(xindex);
  m->set_y_index(yindex);
  m->set_percent((int)percent);
  _state->set_lissajous_model(m);
  signals_changed();
}

void SigSession::lissajous_disable() {
  if (_state->lissajous_model())
    _state->lissajous_model()->set_enabled(false);
}

void SigSession::math_rebuild(bool enable, int ch1_index, int ch2_index,
                              data::MathStack::MathType type) {
  ds_lock_guard lock(_state->data_mutex());

  _state->set_math_stack(nullptr);

  // The MathStack constructor now accepts channel indices and resolves the
  // DSO parameters (vdiv / vfactor / hw_offset / snapshot) through
  // SignalModel — no view::DsoSignal dependency. The View layer is
  // responsible for creating the matching MathTrace later (see
  // View::sync_derived_traces).
  //
  // When the user disables math (enable=false), we destroy any existing
  // MathStack and do not create a new one. The View's sync_derived_traces
  // observes the null MathStack and tears down its MathTrace.
  if (enable) {
    _state->set_math_stack(
        std::make_shared<data::MathStack>(this, ch1_index, ch2_index, type));
  }

  signals_changed();
}

void SigSession::math_disable() {
  if (_state->math_stack())
    _state->math_stack()->init();
}

data::Snapshot *SigSession::get_snapshot(int type) {
  if (type == SR_CHANNEL_LOGIC)
    return _state->view_data()->get_logic();
  else if (type == SR_CHANNEL_ANALOG)
    return _state->view_data()->get_analog();
  else if (type == SR_CHANNEL_DSO)
    return _state->view_data()->get_dso();
  else
    return NULL;
}

void SigSession::clear_error() {
  _state->set_error_pattern(0);
  _state->set_error(No_err);
}

void SigSession::Open() {}

void SigSession::Close() {
  if (_state->bClose())
    return;

  _state->set_bClose(true);

  // Stop decode thread.
  clear_all_documents_decoders();

  pxv_info("SigSession::Close(), stop capture");
  stop_capture();

  // A3 fix: Stop glitch filter and signal invert background threads before
  // tearing down data. Set running flags false first so the task functions
  // know no new work should be accepted, then join the thread if still
  // joinable. Without this, a joinable std::thread would std::terminate on
  // destruction.
  _filter_processor->stop();

  // Join any in-flight background copy thread before tearing down data
  // (a joinable std::thread would otherwise std::terminate on destruction).
  join_copy_thread();

  for (auto p : _state->data_list()) {
    p->clear();
  }
}

void SigSession::clear_all_decoder(bool bUpdateView) {
  if (decode_traces().empty())
    return;

  int dex = -1;
  clear_all_decode_task(dex);

  if (dex != -1) {
    auto runningStack = decode_traces()[dex];
    runningStack->_delete_flag = true;
  }

  decode_traces().clear();

  // decode_traces() returns _active_document->get_decoder_stacks() (or
  // _empty_decoder_stacks), so the clear above already removed them from
  // the document's list. No need to clear
  // _active_document->get_decoder_stacks() a second time.

  if (!_state->bClose() && bUpdateView)
    signals_changed();
}

void SigSession::clear_all_documents_decoders() {
  int dex = -1;
  clear_all_decode_task(dex);

  _document_registry->clear_all_documents_decoders();
}

void SigSession::clear_all_decode_task(int &runningDex) {
  _decode_task_manager->clear_all_decode_task(runningDex);
}

void SigSession::clear_all_decode_task2() {
  _decode_task_manager->clear_all_decode_task2();
}

void SigSession::add_decode_task(std::shared_ptr<data::DecoderStack> stack) {
  _decode_task_manager->add_decode_task(stack);
}

std::shared_ptr<data::DecoderStack>
SigSession::get_decoder_trace(int index, data::SessionDocument *doc) {
  auto &traces = decode_traces(doc);
  if (index >= 0 && index < (int)traces.size()) {
    return traces[index];
  }
  pxv_err("get_decode_trace_by_index: index %d out of range (size=%d)", index, (int)traces.size());
  return nullptr;
}

Snapshot *SigSession::get_signal_snapshot() {
  int mode = _state->device_agent().get_work_mode();
  if (mode == ANALOG)
    return _state->view_data()->get_analog();
  else if (mode == DSO)
    return _state->view_data()->get_dso();
  else
    return _state->view_data()->get_logic();
}

// Note: device_lib_event_callback_ex / on_device_lib_event removed.
// Fork libsigrok's ds_set_event_callback_ex API is gone; upstream libsigrok
// uses sr_session_stopped_callback for session-end notification and the
// datafeed callback for packet events. Hotplug (DS_EV_NEW_DEVICE_ATTACH etc.)
// is not supported in this migration (would need libusb hotplug API directly).
// The CollectStart/CollectEnd/EndCollectWork events are now emitted by
// CaptureManager which owns the capture lifecycle.

// Note: add_event_listener / remove_event_listener / remove_callback /
// broadcast<T>() / broadcast_sync<T>() / broadcast_async<T>() /
// dispatch_to<Iface>() are now inline forwarders in sigsession.h that delegate
// to the EventBus owned by _event_bus. The _broadcast_depth guard and the
// _callbacks / _event_listeners vectors live inside EventBus.

// ============================================================================
// IEventListener overrides — Core-internal state-machine events.
// These 5 events were previously handled by the former OnMessage switch
// (now removed). The logic is copied verbatim. The self-emits inside
// on_event(RevEndPacket) now use broadcast_async<TypedEvent> (worker-thread
// safe via qApp queue).
// ============================================================================

void SigSession::on_event(const interface::DeviceOptionsUpdated &) {
  reload();
}

void SigSession::on_event(const interface::TrigNextCollect &) {
  if (_state->is_working() && is_repeat_mode()) {
    if (_capture_manager->get_repeat_intvl() > 0) {
      _capture_manager->set_repeat_hold_prg(100);
      _capture_manager->start_repeat_timer(_capture_manager->get_repeat_intvl() * 1000);
      int intvl = _capture_manager->get_repeat_intvl() * 1000 / 20;

      if (intvl >= 100) {
        _capture_manager->set_repeat_wait_prog_step(5);
      } else if (_capture_manager->get_repeat_intvl() >= 1) {
        intvl = _capture_manager->get_repeat_intvl() * 1000 / 10;
        _capture_manager->set_repeat_wait_prog_step(10);
      } else {
        intvl = _capture_manager->get_repeat_intvl() * 1000 / 5;
        _capture_manager->set_repeat_wait_prog_step(20);
      }

      _capture_manager->start_repeat_wait_prog_timer(intvl);
    } else {
      _capture_manager->set_repeat_hold_prg(0);
      _capture_manager->exec_capture();
    }
  }
}

void SigSession::on_event(const interface::RevEndPacket &) {
  pxv_info("SigSession::on_event(RevEndPacket): mode=%d stream=%d single=%d",
           _state->device_agent().get_work_mode(),
           _capture_manager->is_stream_mode(),
           _capture_manager->is_single_mode());
  if (_state->device_agent().get_work_mode() == LOGIC) {
    bool bAddDecoder = false;
    bool bSwapBuffer = false;

    if (is_single_mode()) {
      if (!_capture_manager->is_stream_mode())
        bAddDecoder = true;
    } else if (is_repeat_mode()) {
      if (!_capture_manager->is_stream_mode()) {
        bAddDecoder = true;
        bSwapBuffer = true;
      } else if (_capture_manager->capture_times() > 1) {
        bAddDecoder = true;
        bSwapBuffer = true;
      }
    } else if (is_loop_mode()) {
      bAddDecoder = true;
    }

    if (is_repeat_mode()) {
      AppConfig &app = AppConfig::Instance();
      bool swapBackBufferAlways = app.appOptions.swapBackBufferAlways;
      if (!swapBackBufferAlways && !_state->is_working() && _capture_manager->capture_times() > 1) {
        bAddDecoder = false;
        bSwapBuffer = false;
        _state->capture_data()->clear();
      }
    }

    if (bAddDecoder) {
      clear_all_decode_task2();
      _capture_manager->clear_decode_result();
    }

    _capture_manager->stop_trig_check_timer();

    // Switch the caputrued data buffer to view.
    if (bSwapBuffer) {
      if (_state->view_data() != _state->capture_data())
        _state->view_data()->clear();

      _state->set_view_data(_state->capture_data());
      attach_data_to_signal(_state->view_data());
      _state->set_session_time(_state->trig_time());

      receive_trigger(_state->view_data()->_trig_pos); // Update trig position.

      _event_bus->broadcast_async<interface::DataPoolChanged>({});
    }

    if (bAddDecoder && _document_registry->get_active_document()) {
      // Move copy_data_to_document to a background thread
      // so the UI thread is not blocked by the deep copy.
      // C4 fix: lock _capture_state_mutex to make the snapshot of
      // _copy_in_progress + _capture_owner_document atomic with respect
      // to CaptureOwnerGuard ctor/dtor and clear_capture_owner_document.
      data::SessionDocument *doc;
      {
        std::lock_guard<std::mutex> lock(_document_registry->capture_state_mutex());
        _document_registry->copy_in_progress() = true;
        doc = _document_registry->get_capture_owner_document()
                  ? _document_registry->get_capture_owner_document()
                  : _document_registry->get_active_document();
      }
      _event_bus->broadcast_async<interface::CopyInProgressChanged>(
          {is_copy_in_progress()});

      if (_document_registry->copy_thread().joinable()) {
        _document_registry->copy_thread().join(); // 等待上一个 copy 完成
      }
      _document_registry->copy_thread() = std::thread([this, doc]() {
        copy_data_to_document(doc);
        {
          std::lock_guard<std::mutex> lock(_document_registry->capture_state_mutex());
          _document_registry->copy_in_progress() = false;
        }
        _event_bus->broadcast_async<interface::CopyInProgressChanged>(
            {is_copy_in_progress()});
        _event_bus->broadcast_async<interface::CopyToDocDone>({nullptr});
      });
    } else {
      // No active document (typical in headless mode) OR stream mode (no
      // copy thread needed). Skip the deep copy to a SessionDocument and
      // start the decoders directly. The decoders read their snapshots
      // from _view_data via get_signal_models(), so they don't need a
      // SessionDocument to be set up.
      pxv_info("RevEndPacket ELSE branch: starting decoders + guard release (single=%d)",
               _capture_manager->is_single_mode());
      start_all_decode_tasks();

      // CRITICAL FIX: single 模式下，LOGIC 采集正常完成（无 copy 线程）
      // 时释放 CaptureOwnerGuard，使 _is_working=false，让 MCP
      // wait_capture_complete 能正确返回。原代码只 set_capture_owner_index
      // _locked(SIZE_MAX) 而不释放 guard，导致 _is_working 永远为 true。
      // repeat/loop 模式持续采集，由 stop_capture 释放。
      if (_capture_manager->is_single_mode()) {
        _capture_manager->data_unlock();
        _event_bus->broadcast_sync<interface::EndCollectWorkPrev>({});
        _document_registry->release_capture_owner();
        pxv_info("RevEndPacket: CaptureOwnerGuard released (single mode). is_working=%d",
                 _state->is_working());
        _event_bus->broadcast_async<interface::EndCollectWork>({});
      } else {
        // repeat/loop 模式：保留原逻辑（仅清 index 不释放 guard）
        std::lock_guard<std::mutex> lock(_document_registry->capture_state_mutex());
        _document_registry->set_capture_owner_index_locked(SIZE_MAX);
      }
    }

    // 采集完成后自动重新应用毛刺滤波(若用户启用了 auto-apply)
    if (_state->view_data()->_glitch_filter_auto_apply &&
        !_state->view_data()->_glitch_filter_thresholds.empty() &&
        _state->view_data()->get_logic() && !_state->view_data()->get_logic()->empty()) {
      _filter_processor->set_glitch_filter(
          _state->view_data()->_glitch_filter_thresholds,
          _state->view_data()->_glitch_filter_modes);
    }

    frame_ended();
  }
}

void SigSession::on_event(const interface::CopyToDocDone &) {
  // Background copy_data_to_document has completed. Start decoders.
  // NOTE: _capture_owner_document is NOT cleared here for repeat/loop mode —
  // it is managed by CaptureOwnerGuard for the whole capture session.
  // In repeat mode the owner persists across frames; the guard is reset
  // only on stop_capture or tab close (clear_capture_owner_document).
  start_all_decode_tasks();
  pxv_info("Background copy_data_to_document completed. Decoders started.");

  // CRITICAL FIX: single 模式下，LOGIC 采集经过 copy 线程完成后释放
  // CaptureOwnerGuard，使 _is_working=false，让 MCP wait_capture_complete
  // 能正确返回。repeat/loop 模式持续采集，由 stop_capture 释放。
  if (_capture_manager && _capture_manager->is_single_mode()) {
    _capture_manager->data_unlock();
    _event_bus->broadcast_sync<interface::EndCollectWorkPrev>({});
    _document_registry->release_capture_owner();
    pxv_info("CopyToDocDone: CaptureOwnerGuard released (single mode). is_working=%d",
             _state->is_working());
    _event_bus->broadcast_async<interface::EndCollectWork>({});
  }
}

void SigSession::on_event(const interface::DeviceSpeedNotMatch &) {
  QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_DEVICE_SPEED_TOO_LOW),
                     "Speed too low!"));
  delay_prop_msg(strMsg);
}

void SigSession::DeviceConfigChanged() {
  // broadcast_async<SampleCountUpdated> is queued on qApp via
  // Qt::QueuedConnection, so the previous _suppress_config_broadcast guard
  // (which prevented nested reload -> signals_changed -> View AllReplaced UAF
  // during JSON restore) is no longer needed: the caller's stack frame
  // completes before any listener processes the event.
  // Notify UI that device config changed (e.g. disk cache toggle),
  // so sampling duration can be recalculated from SR_CONF_HW_DEPTH
  _event_bus->broadcast_async<interface::SampleCountUpdated>(
      {(uint64_t)get_ring_sample_count()});
}

bool SigSession::switch_work_mode(int mode) {
  assert(!_state->is_working());
  int cur_mode = _state->device_agent().get_work_mode();

  if (cur_mode != mode) {
    set_collect_mode(COLLECT_SINGLE);

    // Only DSL/PXLogic devices implement SR_CONF_DEVICE_MODE.
    // demo/file/compat devices have no mode switch — get_work_mode()
    // always returns LOGIC for them, so this branch is unreachable for
    // those devices, but guard defensively to avoid "Option not
    // available" noise.
    if (_state->device_agent().is_dsl_device())
      _state->device_agent().set_config_int16(SR_CONF_DEVICE_MODE, mode);

    if (cur_mode == LOGIC) {
      clear_all_decode_task2();
      _capture_manager->clear_decode_result();
    }

    _capture_manager->set_is_stream_mode(false);
    if (mode == LOGIC) {
      if (_state->device_agent().is_hardware()) {
        _capture_manager->set_is_stream_mode(_state->device_agent().is_stream_mode());
      } else if (_state->device_agent().is_demo()) {
        _capture_manager->set_is_stream_mode(true);
      }
    }

    _state->capture_data()->clear();
    _state->view_data()->clear();
    _state->set_capture_data(_state->view_data());

    init_signals();

    set_cur_snap_samplerate(_state->device_agent().get_sample_rate());
    set_cur_samplelimits(_state->device_agent().get_sample_limit());

    pxv_info("Switch work mode to:%d", mode);

    // broadcast_async<DeviceModeChanged> is queued on qApp via
    // Qt::QueuedConnection, so View finishes its signals_changed rebuild
    // before handlers access view::Signal::_model. No separate _deferred
    // variant is needed.
    _event_bus->broadcast_async<interface::DeviceModeChanged>({});

    return true;
  }
  return false;
}

void SigSession::clear_signals() {
  _state->set_math_stack(nullptr);

  _state->signal_models().clear();
}

std::shared_ptr<data::SignalModel> SigSession::get_signal_by_index(int index) {
  for (auto &m : _state->signal_models()) {
    if (m->index() == index)
      return m;
  }
  return nullptr;
}

void SigSession::on_load_config_end() {
  set_cur_snap_samplerate(_state->device_agent().get_sample_rate());
  set_cur_samplelimits(_state->device_agent().get_sample_limit());
}

void SigSession::clear_view_data() {
  _state->view_data()->clear();
  data_updated();
}

void SigSession::set_trace_name(std::shared_ptr<data::SignalModel> model,
                                QString name) {
  if (!model) {
    pxv_warn("%s", "SigSession::set_trace_name: model is NULL");
    return;
  }
  assert(model);

  model->set_name(name.toStdString());

  // SignalModel covers Logic/Analog/Dso channel types. The decoder case is
  // handled separately via set_decoder_row_label().
  if (model->type() == SR_CHANNEL_LOGIC ||
      model->type() == SR_CHANNEL_ANALOG) {
    _state->device_agent().set_channel_name(model->index(), name.toUtf8());
  }
}

void SigSession::set_decoder_row_label(int index, QString label) {
  // TODO: Previously this delegated to set_trace_name() on a view::DecodeTrace,
  // which called view::Trace::set_name() and (for decoder traces) updated
  // DecoderPannel item name. DecoderStack has no set_name() method; a new
  // mechanism for naming decoders (e.g. storing the label on DecoderStack)
  // is required. The View layer/DecoderPannel should be updated to manage
  // decoder display names directly.
  (void)index;
  (void)label;
}

std::shared_ptr<data::SignalModel>
SigSession::get_channel_by_index(int orgIndex) {
  for (auto &m : _state->signal_models()) {
    if (m->index() == orgIndex) {
      return m;
    }
  }
  return nullptr;
}

void SigSession::make_channels_view_index(int start_dex) {
  // SignalModel is a pure data model and has no view_index property.
  // The View layer is responsible for tracking view index on its own
  // view::Signal objects (created from SignalModel via SignalFactory).
  // This method is now a no-op.
  (void)start_dex;
}

void SigSession::update_dso_data_scale() {
  int mode = _state->device_agent().get_work_mode();

  if (mode == DSO) {
    // TODO: view::DsoSignal::get_scale() returned a UI rendering scale
    // computed from view rect height, vdiv, vfactor and hw_offset. After
    // de-view-ization, SignalModel holds vdiv/vfactor/hw_offset but not the
    // view rect height, so the rendering scale cannot be computed here.
    // The View layer is responsible for calling DsoSnapshot::set_data_scale()
    // on its own cloned DsoSignal objects.
    (void)mode;
  }
}

int64_t SigSession::get_ring_sample_count() {
  int mode = _state->device_agent().get_work_mode();
  if (mode == LOGIC) {
    return _state->view_data()->get_logic()->get_ring_sample_count();
  } else if (mode == DSO) {
    return _state->view_data()->get_dso()->get_ring_sample_count();
  } else {
    return _state->view_data()->get_analog()->get_ring_sample_count();
  }
}

void SigSession::update_lang_text() {
  // TODO: view::SpectrumTrace::update_lang_text() was a UI rendering method
  // that refreshed localized text on spectrum trace widgets. After
  // de-view-ization, SigSession no longer owns view::SpectrumTrace instances.
  // The View layer is responsible for updating language text on its own
  // rendering objects.
}

bool SigSession::have_decoded_result() {
  for (auto stack : decode_traces()) {
    if (stack->get_result_count() > 0) {
      return true;
    }
  }

  return false;
}

void SigSession::apply_samplerate() { on_load_config_end(); }

data::LogicSnapshot *SigSession::get_logic_snapshot() {
  return _state->view_data()->get_logic();
}

data::AnalogSnapshot *SigSession::get_analog_snapshot() {
  return _state->view_data()->get_analog();
}

data::DsoSnapshot *SigSession::get_dso_snapshot() {
  return _state->view_data()->get_dso();
}

void SigSession::set_active_document(data::SessionDocument *doc) {
  _document_registry->set_active_document(doc);
}

void SigSession::clear_capture_owner_document(data::SessionDocument *doc) {
  _document_registry->clear_capture_owner_document(doc);
}

void SigSession::join_copy_thread() {
  _document_registry->join_copy_thread();
}

bool SigSession::is_copy_in_progress() const {
  return _document_registry->is_copy_in_progress();
}

data::SessionDocument *SigSession::get_capture_owner_document() const {
  return _document_registry->get_capture_owner_document();
}

data::SessionDocument *SigSession::get_active_document() {
  return _document_registry->get_active_document();
}

// phase 2: SigSession::register_document / unregister_document removed.
// Document ownership is now held by DocumentRegistry. Callers use
// _session->document_registry()->take_document(...) / release_document(...).

void SigSession::set_trigger_config(const data::TriggerConfig &cfg) {
  _state->set_trigger_config(cfg);
  _event_bus->broadcast_async<interface::TriggerConfigChanged>(
      {&_state->trigger_config()});
}

void SigSession::sync_trigger_to_libsigrok() {
  // Core→libsigrok 触发配置唯一同步点。在 start_capture 内部 sr_session_start 前调用。
  //
  // Fork libsigrok 删除后，ds_trigger_* API 不复存在。改用上游 sr_trigger_* API
  // 同步 simple trigger。Adv/Serial trigger 字段保留在 TriggerConfig 中但暂不下发
  // （UI 保留供 PXLogic 驱动未来扩展）。
  const auto &cfg = _state->trigger_config();

  // Always sync capture_ratio (trigger position as 0..100 percent) to the driver.
  // pxlogic's set_trigger() reads devc->capture_ratio at acquisition start to
  // compute trigger_pos_set; scilogic already supported SR_CONF_CAPTURE_RATIO.
  // Previously pxlogic read from pxlogic_trigger_cfg.trigger_pos stub that nobody
  // populated, leaving trigger_pos_set at zero — broken since the fork removal.
  const int trig_pos = cfg.trigger_pos();
  if (trig_pos >= 0 && trig_pos <= 100) {
    if (!_state->device_agent().set_config_uint64(SR_CONF_CAPTURE_RATIO,
                                                   (uint64_t)trig_pos)) {
      pxv_warn("sync_trigger_to_libsigrok: set SR_CONF_CAPTURE_RATIO=%d failed",
               trig_pos);
    } else {
      pxv_info("sync_trigger_to_libsigrok: capture_ratio=%d synced", trig_pos);
    }
  }

  // Only Simple trigger mode is synced to the driver. Adv/Serial trigger
  // configurations are retained in TriggerConfig for future PXLogic driver
  // extension but not currently synced (stub).
  if (cfg.mode() != data::TriggerConfig::Simple) {
    pxv_info("sync_trigger_to_libsigrok: Adv/Serial trigger not synced (stub)");
    return;
  }

  // Build an upstream sr_trigger from the SignalModel trig_type fields.
  // The trigger is attached to the session via sr_session_trigger_set().
  struct sr_trigger *trig = sr_trigger_new("pxview");
  if (!trig) {
    pxv_err("sync_trigger_to_libsigrok: sr_trigger_new failed");
    return;
  }

  struct sr_trigger_stage *stage = sr_trigger_stage_add(trig);
  if (!stage) {
    sr_trigger_free(trig);
    return;
  }

  bool any_triggered = false;
  for (const auto &m : _state->signal_models()) {
    if (!m || m->type() != SR_CHANNEL_LOGIC)
      continue;

    // Find the sr_channel for this SignalModel index.
    struct sr_channel *ch = nullptr;
    for (const GSList *l = _state->device_agent().get_channels(); l; l = l->next) {
      struct sr_channel *probe = (struct sr_channel *)l->data;
      if (probe && probe->index == m->index()) {
        ch = probe;
        break;
      }
    }
    if (!ch)
      continue;

    int match = 0;
    switch (m->trig_type()) {
    case data::SignalModel::POSTRIG:  match = SR_TRIGGER_RISING;  any_triggered = true; break;
    case data::SignalModel::NEGTRIG:  match = SR_TRIGGER_FALLING; any_triggered = true; break;
    case data::SignalModel::HIGTRIG:  match = SR_TRIGGER_ONE;     any_triggered = true; break;
    case data::SignalModel::LOWTRIG:  match = SR_TRIGGER_ZERO;    any_triggered = true; break;
    case data::SignalModel::EDGTRIG:  match = SR_TRIGGER_EDGE;    any_triggered = true; break;
    case data::SignalModel::NONTRIG:
    default: continue; // skip non-trigger channels
    }

    if (sr_trigger_match_add(stage, ch, match, 0.0f) != SR_OK) {
      pxv_warn("sync_trigger_to_libsigrok: sr_trigger_match_add failed for ch %d", m->index());
    }
  }

  if (any_triggered && _state->device_agent().sr_session()) {
    sr_session_trigger_set(_state->device_agent().sr_session(), trig);
    pxv_info("sync_trigger_to_libsigrok: simple trigger synced (%d matches)", stage->matches ? g_slist_length(stage->matches) : 0);
  } else {
    pxv_info("sync_trigger_to_libsigrok: no trigger matches, trigger disabled");
  }

  // sr_session_trigger_set copies the trigger; free our copy.
  sr_trigger_free(trig);
}

void SigSession::copy_data_to_document(data::SessionDocument *doc) {
  if (!doc || !_state->view_data() || !have_view_data())
    return;

  doc->set_samplerate(_state->view_data()->_cur_snap_samplerate);
  doc->set_samplelimits(_state->view_data()->_cur_samplelimits);
  doc->set_trigger_pos(_state->view_data()->_trig_pos);

  doc->copy_from_logic(_state->view_data()->get_logic());
  doc->copy_from_analog(_state->view_data()->get_analog());
  doc->copy_from_dso(_state->view_data()->get_dso());
}

void SigSession::attach_data_to_signal(SessionData *data) {
  _decode_task_manager->attach_data_to_signal(data);
}

// --- FilterProcessor forwarding wrappers ----------------------------------
void SigSession::set_glitch_filter(
    const std::vector<uint32_t> &thresholds,
    const std::vector<GlitchFilterMode> &filter_modes) {
  _filter_processor->set_glitch_filter(thresholds, filter_modes);
}
void SigSession::clear_glitch_filter() {
  _filter_processor->clear_glitch_filter();
}
bool SigSession::is_glitch_filter_active() {
  return _filter_processor->is_glitch_filter_active();
}

void SigSession::clear_glitch_filter_state_for_capture() {
  // 新采集开始时调用:清除滤波激活状态和 backup,
  // 但保留 thresholds/modes(供 auto-apply 使用)。
  // 不恢复数据 — _state->view_data()->get_logic() 已被 clear(),无数据可恢复。
  if (_state->view_data()->_logic_backup) {
    delete _state->view_data()->_logic_backup;
    _state->view_data()->_logic_backup = nullptr;
  }
  if (_state->view_data()->_glitch_filter_active) {
    _state->view_data()->_glitch_filter_active = false;
    _event_bus->broadcast_async<interface::GlitchFilterCleared>({});
  }
}
void SigSession::set_signal_invert(const std::vector<bool> &channels) {
  _filter_processor->set_signal_invert(channels);
}
void SigSession::clear_signal_invert() {
  _filter_processor->clear_signal_invert();
}
bool SigSession::is_signal_invert_active() {
  return _filter_processor->is_signal_invert_active();
}

void SigSession::restart_decoders() {
  if (decode_traces().empty())
    return;

  // Stop running decoders
  clear_all_decode_task2();
  _capture_manager->clear_decode_result();

  // Copy current data to document for decoders
  auto doc =
      _document_registry->get_capture_owner_document()
          ? _document_registry->get_capture_owner_document()
          : _document_registry->get_active_document();
  if (doc) {
    copy_data_to_document(doc);
  }

  // restart_decoders() reuses the existing DecoderStack instances in place
  // (it does NOT create new ones, so they keep their handle_id). Bump the
  // version on each stack so API/MCP consumers can invalidate any cached
  // results bound to a prior version.
  for (auto stack : decode_traces()) {
    if (stack)
      stack->bump_version();
  }

  start_all_decode_tasks();
}

void SigSession::start_all_decode_tasks() {
  _decode_task_manager->start_all_decode_tasks();
}

// --- DecodeTaskManager forwarding wrappers --------------------------------
void SigSession::rst_decoder(int index, data::SessionDocument *doc) {
  _decode_task_manager->rst_decoder(index, doc);
}

void SigSession::rst_decoder_by_key_handel(void *handel,
                                           data::SessionDocument *doc) {
  _decode_task_manager->rst_decoder_by_key_handel(handel, doc);
}

void SigSession::remove_decode_task(
    std::shared_ptr<data::DecoderStack> stack) {
  _decode_task_manager->remove_decode_task(stack);
}

size_t SigSession::get_disk_write_queue_depth() {
  if (_state->view_data()->get_logic()->is_disk_cache_active())
    return _state->view_data()->get_logic()->get_disk_write_queue_depth();
  return 0;
}

double SigSession::get_disk_write_speed_mbps() {
  if (_state->view_data()->get_logic()->is_disk_cache_active())
    return _state->view_data()->get_logic()->get_disk_write_speed_mbps();
  return 0.0;
}

bool SigSession::is_disk_write_disk_full() { return false; }

} // namespace pv
