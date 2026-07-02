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
#include "data/decodermodel.h"
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

// --- Dispatch helpers (moved from inline in sigsession.h) ---

void SigSession::data_updated() {
  dispatch_to<IDataCallback>([](IDataCallback *cb) { cb->data_updated(); });
}

void SigSession::set_receive_data_len(quint64 len) {
  dispatch_to<IDataCallback>(
      [len](IDataCallback *cb) { cb->receive_data_len(len); });
}

void SigSession::receive_header() {
  dispatch_to<IDataCallback>([](IDataCallback *cb) { cb->receive_header(); });
}

void SigSession::cur_snap_samplerate_changed() {
  dispatch_to<IDataCallback>(
      [](IDataCallback *cb) { cb->cur_snap_samplerate_changed(); });
}

void SigSession::frame_began() {
  dispatch_to<ICaptureCallback>([](ICaptureCallback *cb) { cb->frame_began(); });
}

void SigSession::frame_ended() {
  dispatch_to<ICaptureCallback>([](ICaptureCallback *cb) { cb->frame_ended(); });
}

void SigSession::update_capture() {
  dispatch_to<ICaptureCallback>(
      [](ICaptureCallback *cb) { cb->update_capture(); });
}

void SigSession::repeat_hold(int percent) {
  dispatch_to<ICaptureCallback>(
      [percent](ICaptureCallback *cb) { cb->repeat_hold(percent); });
}

void SigSession::receive_trigger(quint64 trigger_pos) {
  dispatch_to<ITriggerCallback>(
      [trigger_pos](ITriggerCallback *cb) { cb->receive_trigger(trigger_pos); });
}

void SigSession::show_wait_trigger() {
  dispatch_to<ITriggerCallback>(
      [](ITriggerCallback *cb) { cb->show_wait_trigger(); });
}

void SigSession::trigger_message(int msg) {
  dispatch_to<ITriggerCallback>(
      [msg](ITriggerCallback *cb) { cb->trigger_message(msg); });
  broadcast_msg(msg);
}

void SigSession::signals_changed() {
  dispatch_to<ISessionStateCallback>(
      [](ISessionStateCallback *cb) { cb->signals_changed(); });
  broadcast<interface::SignalsChanged>({});
}

void SigSession::session_error() {
  dispatch_to<ISessionStateCallback>(
      [](ISessionStateCallback *cb) { cb->session_error(); });
}

void SigSession::delay_prop_msg(QString strMsg) {
  dispatch_to<ISessionStateCallback>(
      [strMsg](ISessionStateCallback *cb) { cb->delay_prop_msg(strMsg); });
}

std::vector<std::shared_ptr<data::DecoderStack>>
    SigSession::_empty_decoder_stacks;

SigSession::SigSession() {
  _map_zoom = 0;
  _error = No_err;
  _is_working = false;
  _is_saving = false;
  _device_status = ST_INIT;
  _view_data = NULL;
  _capture_data = NULL;
  _decoder_pannel = NULL;
  _is_triged = false;
  _dso_status_valid = false;

  _data_list.push_back(new SessionData());
  _data_list.push_back(new SessionData());
  _view_data = _data_list[0];
  _capture_data = _data_list[0];

  // EventBus must be constructed before add_msg_listener(this), since
  // add_msg_listener forwards to _event_bus. All broadcast_msg /
  // trigger_message calls are ASYNC (queued on qApp via QueuedConnection).
  _event_bus = std::make_unique<core::EventBus>();
  this->add_msg_listener(this);

  // Managers are constructed after _event_bus (they hold a raw pointer to it)
  // and after _view_data/_capture_data are initialized (FilterProcessor
  // accesses _view_data).
  _filter_processor = std::make_unique<core::FilterProcessor>(_event_bus.get(),
                                                              this);
  _decode_task_manager = std::make_unique<core::DecodeTaskManager>(
      _event_bus.get(), this);
  _data_feed_parser = std::make_unique<core::DataFeedParser>(_event_bus.get(),
                                                             this);
  _document_registry = std::make_unique<core::DocumentRegistry>(
      _event_bus.get(), this);
  // CaptureManager owns the capture lifecycle + DsTimer instances + the
  // _is_instant / _clt_mode / _data_lock / _repeat_intvl / _dso_packet_count
  // / _disk_cache_config state. Constructed after _document_registry because
  // action_start_capture calls _document_registry->acquire_capture_owner().
  _capture_manager = std::make_unique<core::CaptureManager>(_event_bus.get(),
                                                            this);

  _decoder_model = new pv::data::DecoderModel(NULL);

  _lissajous_model = nullptr;
  _math_stack = nullptr;
  _bClose = false;

  _device_agent.set_callback(this);
}

SigSession::SigSession(SigSession &o) { (void)o; }

SigSession::~SigSession() {
  // A3 fix: ensure Close() has been called so background threads (decode/copy/
  // glitch_filter/signal_invert) are joined before we destroy _data_list.
  // Close() is idempotent (_bClose guard), so calling it here is safe even
  // if already called via uninit().
  Close();

  for (auto p : _data_list) {
    p->clear();
    delete p;
  }
  _data_list.clear();
}

bool SigSession::init() {
  ds_log_set_context(pxv_log_context());

  // Register callbacks using the _ex API to pass `this` as user data,
  // so the static trampolines can dispatch to the instance method without
  // relying on a static singleton pointer.
  ds_set_event_callback_ex(device_lib_event_callback_ex, this);

  ds_set_datafeed_callback_ex(core::DataFeedParser::data_feed_callback_ex,
                              _data_feed_parser.get());

  // firmware resource directory
  QString resdir = GetFirmwareDir();
  std::string res_path = pv::path::ToUnicodePath(resdir);
  ds_set_firmware_resource_dir(res_path.c_str());

  if (ds_lib_init() != SR_OK) {
    pxv_err("PXView run ERROR: collect lib init failed.");
    return false;
  }

  return true;
}

void SigSession::uninit() {
  this->Close();

  ds_lib_exit();
}

bool SigSession::set_default_device() {
  assert(!_is_saving);

  if (_is_working) {
    pxv_info("SigSession::set_default_device()，The current device is working, "
             "now to stop it.");
    pxv_info("SigSession::set_default_device(), stop capture");
    stop_capture();
  }

  struct ds_device_base_info *array = NULL;
  int count = 0;

  pxv_info("Set default device.");

  if (ds_get_device_list(&array, &count) != SR_OK) {
    pxv_err("Get device list error!");
    return false;
  }
  if (count < 1 || array == NULL) {
    pxv_err("Error! Device list is empty, can't set default device.");
    return false;
  }

  struct ds_device_base_info *dev = (array + count - 1);
  ds_device_handle dev_handle = dev->handle;

  free(array);

  if (set_device(dev_handle)) {
    return true;
  }
  return false;
}

bool SigSession::set_device(ds_device_handle dev_handle) {
  assert(!_is_saving);
  assert(!_is_working);
  assert(_event_bus && _event_bus->has_callbacks());

  ds_device_handle old_dev = _device_agent.handle();

  trigger_message(DSV_MSG_CURRENT_DEVICE_CHANGE_PREV);
  // Release the old device.
  _device_agent.release();
  _device_status = ST_INIT;

  if (ds_active_device(dev_handle) != SR_OK) {
    pxv_err("Switch device error!");
    return false;
  }

  _device_agent.update();
  set_collect_mode(COLLECT_SINGLE);

  if (_device_agent.is_file()) {
    std::string dev_name = pv::path::ToUnicodePath(_device_agent.name());
    pxv_info("Switch to file \"%s\" done.", dev_name.c_str());
  } else
    pxv_info("Switch to device \"%s\" done.",
             _device_agent.name().toUtf8().data());

  clear_all_documents_decoders();

  _view_data->clear();
  _capture_data->clear();
  _capture_data = _view_data;

  init_signals();

  set_cur_snap_samplerate(_device_agent.get_sample_rate());
  set_cur_samplelimits(_device_agent.get_sample_limit());

  // The current device changed.
  // trigger_message is now ALWAYS async (queued on qApp via
  // Qt::QueuedConnection): init_signals() above rebuilt Core SignalModels, and
  // the ITriggerCallback dispatch + broadcast_msg will run AFTER the View
  // signals_changed rebuild so handlers (on_device_changed ->
  // load_device_config -> DsoSignal::set_zero_ratio) see valid
  // view::Signal::_model pointers. No separate _deferred variant is needed.
  trigger_message(DSV_MSG_CURRENT_DEVICE_CHANGED);

  if (ds_get_last_error() == SR_ERR_DEVICE_FIRMWARE_VERSION_LOW) {
    QString strMsg = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_TO_RECONNECT_FOR_FIRMWARE),
                         "Please reconnect the device!");
    delay_prop_msg(strMsg);
    return false;
  }

  if (ds_get_last_error() == SR_ERR_FIRMWARE_NOT_EXIST) {
    QString strMsg = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_FIRMWARE_NOT_EXIST),
                         "Firmware not exist!");
    delay_prop_msg(strMsg);
    return false;
  }

  if (ds_get_last_error() == SR_ERR_DEVICE_USB_IO_ERROR) {
    QString strMsg =
        L_S(STR_PAGE_MSG, S_ID(IDS_MSG_DEVICE_USB_IO_ERROR), "USB io error!");
    delay_prop_msg(strMsg);
    return false;
  }

  if (ds_get_last_error() == SR_ERR_DEVICE_IS_EXCLUSIVE) {
    QString strMsg = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_DEVICE_BUSY_SWITCH_FAILED),
                         "Device is busy!");
    if (old_dev != NULL_HANDLE)
      MsgBox::Show(strMsg);
    else
      delay_prop_msg(strMsg);
    return false;
  }

  return true;
}

bool SigSession::set_file(QString name) {
  assert(!_is_saving);
  assert(!_is_working);

  std::string file_name = pv::path::ToUnicodePath(name);
  pxv_info("Load file: \"%s\"", file_name.c_str());

  std::string file_str = name.toUtf8().toStdString();

  if (ds_device_from_file(file_str.c_str()) != SR_OK) {
    pxv_err("Load file error!");
    return false;
  }

  return set_default_device();
}

void SigSession::close_file(ds_device_handle dev_handle) {
  if (!dev_handle) {
    pxv_warn("%s", "SigSession::close_file: dev_handle is NULL");
    return;
  }
  assert(dev_handle);

  if (dev_handle == _device_agent.handle() && _is_working) {
    pxv_err("The virtual device is running, can't remove it.");
    return;
  }
  bool isCurrent = dev_handle == _device_agent.handle();

  if (ds_remove_device(dev_handle) != SR_OK) {
    pxv_err("Remove virtual deivice error!");
  }

  if (isCurrent)
    set_default_device();
}

bool SigSession::have_hardware_data() {
  if (_device_agent.have_instance() && _device_agent.is_hardware()) {
    Snapshot *data = get_signal_snapshot();
    return data->have_data();
  }
  return false;
}

struct ds_device_base_info *SigSession::get_device_list(int &out_count,
                                                        int &actived_index) {
  out_count = 0;
  actived_index = -1;
  struct ds_device_base_info *array = NULL;

  if (ds_get_device_list(&array, &out_count) == SR_OK) {
    actived_index = ds_get_actived_device_index();
    return array;
  }
  return NULL;
}

uint64_t SigSession::cur_samplerate() {
  // samplerate for current viewport
  if (_device_agent.get_work_mode() == DSO)
    return _device_agent.get_sample_rate();
  else
    return cur_snap_samplerate();
}

uint64_t SigSession::cur_snap_samplerate() {
  // samplerate for current snapshot
  return _capture_data->_cur_snap_samplerate;
}

uint64_t SigSession::cur_samplelimits() {
  return _capture_data->_cur_samplelimits;
}

double SigSession::cur_sampletime() {
  return cur_samplelimits() * 1.0 / cur_samplerate();
}

double SigSession::cur_snap_sampletime() {
  return cur_samplelimits() * 1.0 / cur_snap_samplerate();
}

double SigSession::get_logic_data_view_time() {
  return _view_data->get_logic()->get_ring_sample_count() * 1.0 /
         cur_snap_samplerate();
}

double SigSession::cur_view_time() {
  return _device_agent.get_time_base() * DS_CONF_DSO_HDIVS * 1.0 / SR_SEC(1);
}

void SigSession::set_cur_snap_samplerate(uint64_t samplerate) {
  assert(samplerate != 0);

  _capture_data->_cur_snap_samplerate = samplerate;
  _capture_data->get_logic()->set_samplerate(samplerate);
  _capture_data->get_analog()->set_samplerate(samplerate);
  _capture_data->get_dso()->set_samplerate(samplerate);

  int mode = _device_agent.get_work_mode();

  if (mode == DSO) {
    for (auto m : _signal_models) {
      if (m->type() == api::ChannelType::Dso) {
        // TODO: verify - vfactor and vdiv replace view::DsoSignal getters.
        _capture_data->get_dso()->set_measure_voltage_factor(
            (uint64_t)m->vfactor(), m->index());
        _capture_data->get_dso()->set_data_scale(m->vdiv(), m->index());
      }
    }
  }

  // DecoderStack
  for (auto d : decode_traces()) {
    d->set_samplerate(samplerate);
  }

  // Math
  if (_math_stack)
    _math_stack->set_samplerate(_device_agent.get_sample_rate());
  // SpectrumStack
  for (auto m : _spectrum_stacks) {
    m->set_samplerate(samplerate);
  }

  cur_snap_samplerate_changed();
}

void SigSession::set_cur_samplelimits(uint64_t samplelimits) {
  assert(samplelimits != 0);
  _capture_data->_cur_samplelimits = samplelimits;
  // R1: symmetric to set_cur_snap_samplerate which fires
  // cur_snap_samplerate_changed(); notify capture listeners that the
  // sample limit changed.
  dispatch_to<ICaptureCallback>(
      [](ICaptureCallback *cb) { cb->cur_samplelimits_changed(); });
}

std::vector<std::shared_ptr<data::SignalModel>> &
SigSession::get_signal_models() {
  return _signal_models;
}

void SigSession::init_signals() {
  if (_device_agent.have_instance() == false) {
    assert(false);
  }

  std::vector<std::shared_ptr<data::SignalModel>> models;
  unsigned int logic_probe_count = 0;
  unsigned int dso_probe_count = 0;
  unsigned int analog_probe_count = 0;

  _capture_data->clear();
  _view_data->clear();
  set_cur_snap_samplerate(_device_agent.get_sample_rate());
  set_cur_samplelimits(_device_agent.get_sample_limit());

  // Detect what data types we will receive
  if (_device_agent.have_instance()) {
    for (const GSList *l = _device_agent.get_channels(); l; l = l->next) {
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

  int mode = _device_agent.get_work_mode();

  for (GSList *l = _device_agent.get_channels(); l; l = l->next) {
    sr_channel *probe = (sr_channel *)l->data;
    if (!probe) {
      pxv_warn("%s", "SigSession: probe is NULL in channel loop, skipping");
      continue;
    }
    assert(probe);

    if (mode == LOGIC && probe->type != SR_CHANNEL_LOGIC) {
      continue;
    }

    if (mode == ANALOG && probe->type != SR_CHANNEL_ANALOG) {
      continue;
    }

    if (mode == DSO && probe->type != SR_CHANNEL_DSO) {
      continue;
    }

    bool should_create = false;
    api::ChannelType ch_type = api::ChannelType::Logic;

    switch (probe->type) {
    case SR_CHANNEL_LOGIC:
      if (probe->enabled) {
        should_create = true;
        ch_type = api::ChannelType::Logic;
      }
      break;

    case SR_CHANNEL_DSO:
      should_create = true;
      ch_type = api::ChannelType::Dso;
      break;

    case SR_CHANNEL_ANALOG:
      if (probe->enabled) {
        should_create = true;
        ch_type = api::ChannelType::Analog;
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
      //   - vdiv       <- SR_CONF_PROBE_VDIV      (DsoSignal::get_vDialValue)
      //   - vfactor    <- SR_CONF_PROBE_FACTOR    (DsoSignal::get_factor)
      //   - hw_offset  <- SR_CONF_PROBE_HW_OFFSET (DsoSignal::get_hw_offset)
      //   - zero_offset<- SR_CONF_PROBE_OFFSET    (DsoSignal::load_settings)
      if (ch_type == api::ChannelType::Dso ||
          ch_type == api::ChannelType::Analog) {
        uint64_t vdiv = 0;
        if (_device_agent.get_config_uint64(SR_CONF_PROBE_VDIV, vdiv, probe,
                                            NULL))
          model->set_vdiv((double)vdiv);

        uint64_t vfactor = 1;
        if (_device_agent.get_config_uint64(SR_CONF_PROBE_FACTOR, vfactor,
                                            probe, NULL))
          model->set_vfactor((double)vfactor);
        else
          model->set_vfactor(1.0);

        int coupling = 0;
        if (_device_agent.get_config_int16(SR_CONF_PROBE_COUPLING, coupling,
                                           probe, NULL))
          model->set_coupling(coupling);

        bool map_default = true;
        _device_agent.get_config_bool(SR_CONF_PROBE_MAP_DEFAULT, map_default,
                                      probe, NULL);
        model->set_map_default(map_default);

        // hw_offset: prefer live SR_CONF_PROBE_HW_OFFSET (matches
        // DsoSignal::get_hw_offset), fall back to the cached channel value.
        int hw_offset = probe ? probe->hw_offset : 0;
        _device_agent.get_config_uint16(SR_CONF_PROBE_HW_OFFSET, hw_offset,
                                        probe, NULL);
        model->set_hw_offset(hw_offset);

        // zero_offset: prefer live SR_CONF_PROBE_OFFSET (matches
        // DsoSignal::load_settings), fall back to the cached channel value.
        int zero_offset = probe ? probe->zero_offset : 0;
        _device_agent.get_config_uint16(SR_CONF_PROBE_OFFSET, zero_offset,
                                        probe, NULL);
        model->set_zero_offset(zero_offset);
      }

      models.push_back(model);
    }
  }

  clear_signals();
  std::vector<std::shared_ptr<data::SignalModel>>().swap(_signal_models);
  _signal_models = models;
  make_channels_view_index();

  spectrum_rebuild();
  lissajous_disable();
  math_disable();

  // Notify View layer to rebuild signals from the new SignalModels.
  // Without this, LogicSignals keep stale model pointers (old models were
  // deleted above) and never receive property-change notifications.
  signals_changed();

  if (_signal_models.empty()) {
    pxv_info("ERROR: Unable to create any channel.");
  }
}

void SigSession::reload() {
  if (_device_agent.have_instance() == false) {
    assert(false);
  }

  if (_is_working)
    return;

  std::vector<std::shared_ptr<data::SignalModel>> models;
  int mode = _device_agent.get_work_mode();

  set_cur_snap_samplerate(_device_agent.get_sample_rate());
  set_cur_samplelimits(_device_agent.get_sample_limit());

  for (GSList *l = _device_agent.get_channels(); l; l = l->next) {
    sr_channel *probe = (sr_channel *)l->data;
    if (!probe) {
      pxv_warn("%s", "SigSession: probe is NULL in channel loop, skipping");
      continue;
    }
    assert(probe);

    if (mode == LOGIC && probe->type != SR_CHANNEL_LOGIC) {
      continue;
    }

    if (mode == ANALOG && probe->type != SR_CHANNEL_ANALOG) {
      continue;
    }

    if (mode == DSO && probe->type != SR_CHANNEL_DSO) {
      continue;
    }

    bool should_create = false;
    api::ChannelType ch_type = api::ChannelType::Logic;

    switch (probe->type) {
    case SR_CHANNEL_LOGIC:
      if (probe->enabled) {
        should_create = true;
        ch_type = api::ChannelType::Logic;
      }
      break;

    case SR_CHANNEL_DSO:
      should_create = true;
      ch_type = api::ChannelType::Dso;
      break;

    case SR_CHANNEL_ANALOG:
      if (probe->enabled) {
        should_create = true;
        ch_type = api::ChannelType::Analog;
      }
      break;
    }

    if (should_create) {
      // Try to preserve settings from the existing model with the same index
      std::shared_ptr<data::SignalModel> old_model = nullptr;
      for (auto &m : _signal_models) {
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

      if (ch_type == api::ChannelType::Dso ||
          ch_type == api::ChannelType::Analog) {
        uint64_t vdiv = 0;
        if (_device_agent.get_config_uint64(SR_CONF_PROBE_VDIV, vdiv, probe,
                                            NULL))
          model->set_vdiv((double)vdiv);

        uint64_t vfactor = 1;
        if (_device_agent.get_config_uint64(SR_CONF_PROBE_FACTOR, vfactor,
                                            probe, NULL))
          model->set_vfactor((double)vfactor);
        else
          model->set_vfactor(1.0);

        int coupling = 0;
        if (_device_agent.get_config_int16(SR_CONF_PROBE_COUPLING, coupling,
                                           probe, NULL))
          model->set_coupling(coupling);

        bool map_default = true;
        _device_agent.get_config_bool(SR_CONF_PROBE_MAP_DEFAULT, map_default,
                                      probe, NULL);
        model->set_map_default(map_default);

        int hw_offset = probe ? probe->hw_offset : 0;
        _device_agent.get_config_uint16(SR_CONF_PROBE_HW_OFFSET, hw_offset,
                                        probe, NULL);
        model->set_hw_offset(hw_offset);

        int zero_offset = probe ? probe->zero_offset : 0;
        _device_agent.get_config_uint16(SR_CONF_PROBE_OFFSET, zero_offset,
                                        probe, NULL);
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
    pxv_info("SigSession::reload(), clear signals");
    clear_signals();
    std::vector<std::shared_ptr<data::SignalModel>>().swap(_signal_models);
    _signal_models = models;
    make_channels_view_index();
  } else if (mode == LOGIC || mode == ANALOG || mode == DSO) {
    pxv_info("ERROR: Unable to create any channel.");
    clear_signals();
  }

  spectrum_rebuild();

  // CRITICAL: reload() wholesale-replaces _signal_models (new shared_ptr
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

  if (_device_agent.have_instance()) {
    for (auto m : _signal_models) {
      if (!m->enabled())
        continue;

      if (m->type() == api::ChannelType::Logic)
        logic_ch_num++;
      else if (m->type() == api::ChannelType::Dso)
        dso_ch_num++;
      else if (m->type() == api::ChannelType::Analog)
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
  data::SessionDocument *target = doc ? doc : _document_registry->get_active_document();
  return target ? target->get_decoder_stacks() : _empty_decoder_stacks;
}

bool SigSession::add_decoder(
    srd_decoder *const dec, bool silent, DecoderStatus *dstatus,
    std::list<pv::data::decode::Decoder *> &sub_decoders,
    std::shared_ptr<data::DecoderStack> &out_stack,
    data::SessionDocument *doc) {
  if (dec == NULL) {
    pxv_err("Decoder instance is null!");
    assert(false);
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
    decoder_stack->set_handle_id(_next_decoder_handle_id.fetch_add(1));

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
      //   - Capture pipeline: when DSV_MSG_COPY_TO_DOC_DONE fires and the
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

  for (auto m : _signal_models) {
    if (m->type() == api::ChannelType::Dso) {
      has_dso_signal = true;
      // check already have
      auto iter = _spectrum_stacks.begin();

      for (unsigned int i = 0; i < _spectrum_stacks.size(); i++, iter++) {
        if ((*iter)->get_index() == m->index())
          break;
      }

      // if not, rebuild
      if (iter == _spectrum_stacks.end()) {
        auto spectrum_stack =
            std::make_shared<data::SpectrumStack>(this, m->index());
        _spectrum_stacks.push_back(spectrum_stack);
      }
    }
  }

  if (!has_dso_signal) {
    _spectrum_stacks.clear();
  }

  signals_changed();
}

void SigSession::lissajous_rebuild(bool enable, int xindex, int yindex,
                                   double percent) {
  DESTROY_OBJECT(_lissajous_model);
  _lissajous_model = new data::LissajousModel();
  _lissajous_model->set_enabled(enable);
  _lissajous_model->set_x_index(xindex);
  _lissajous_model->set_y_index(yindex);
  _lissajous_model->set_percent((int)percent);
  signals_changed();
}

void SigSession::lissajous_disable() {
  if (_lissajous_model)
    _lissajous_model->set_enabled(false);
}

void SigSession::math_rebuild(bool enable, int ch1_index, int ch2_index,
                              data::MathStack::MathType type) {
  ds_lock_guard lock(_data_mutex);

  _math_stack.reset();

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
    _math_stack =
        std::make_shared<data::MathStack>(this, ch1_index, ch2_index, type);
  }

  signals_changed();
}

void SigSession::math_disable() {
  if (_math_stack)
    _math_stack->init();
}

data::Snapshot *SigSession::get_snapshot(int type) {
  if (type == SR_CHANNEL_LOGIC)
    return _view_data->get_logic();
  else if (type == SR_CHANNEL_ANALOG)
    return _view_data->get_analog();
  else if (type == SR_CHANNEL_DSO)
    return _view_data->get_dso();
  else
    return NULL;
}

void SigSession::clear_error() {
  _error_pattern = 0;
  _error = No_err;
}

void SigSession::Open() {}

void SigSession::Close() {
  if (_bClose)
    return;

  _bClose = true;

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

  for (auto p : _data_list) {
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

  if (!_bClose && bUpdateView)
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
  assert(false);
  return nullptr;
}

Snapshot *SigSession::get_signal_snapshot() {
  int mode = _device_agent.get_work_mode();
  if (mode == ANALOG)
    return _view_data->get_analog();
  else if (mode == DSO)
    return _view_data->get_dso();
  else
    return _view_data->get_logic();
}

void SigSession::device_lib_event_callback_ex(int event, void *user_data) {
  if (user_data == NULL) {
    pxv_err("Error!Event callback user_data is null.");
    return;
  }
  static_cast<SigSession *>(user_data)->on_device_lib_event(event);
}

void SigSession::on_device_lib_event(int event) {
  if (!_event_bus || !_event_bus->has_callbacks()) {
    pxv_detail("The callback list is empty, so the device event was ignored.");
    return;
  }

  switch (event) {
  case DS_EV_DEVICE_RUNNING:
    _device_status = ST_RUNNING;
    set_receive_data_len(0);
    break;

  case DS_EV_DEVICE_STOPPED:
    _device_status = ST_STOPPED;
    // Confirm that SR_DF_END was received
    if (!_capture_data->get_logic()->last_ended() ||
        !_capture_data->get_dso()->last_ended() ||
        !_capture_data->get_analog()->last_ended()) {
      pxv_err("Error!The data is not completed.");
      assert(false);
    }
    break;

  case DS_EV_COLLECT_TASK_START:
    trigger_message(DSV_MSG_COLLECT_START);
    break;

  case DS_EV_COLLECT_TASK_END:
  case DS_EV_COLLECT_TASK_END_BY_ERROR:
  case DS_EV_COLLECT_TASK_END_BY_DETACHED: {
    trigger_message(DSV_MSG_COLLECT_END);

    if (_capture_data->get_logic()->last_ended() == false)
      pxv_err("The collected data is error!");

    if (_capture_data->get_dso()->last_ended() == false)
      pxv_err("The collected data is error!");

    if (_capture_data->get_analog()->last_ended() == false)
      pxv_err("The collected data is error!");

    // trig next collect
    if (is_repeat_mode() && _is_working && event == DS_EV_COLLECT_TASK_END) {
      trigger_message(DSV_MSG_TRIG_NEXT_COLLECT);
    } else {
      _is_working = false;
      _capture_manager->_is_instant = false;
      trigger_message(DSV_MSG_END_COLLECT_WORK);
    }
  } break;

  case DS_EV_NEW_DEVICE_ATTACH:
    trigger_message(DSV_MSG_NEW_USB_DEVICE);
    break;

  case DS_EV_CURRENT_DEVICE_DETACH: {
    if (_is_working) {
      pxv_info("SigSession::on_device_lib_event,DS_EV_CURRENT_DEVICE_DETACH, "
               "stop capture");
      stop_capture();
    }

    trigger_message(DSV_MSG_CURRENT_DEVICE_DETACHED);
  } break;

  case DS_EV_INACTIVE_DEVICE_DETACH:
    trigger_message(DSV_MSG_DEVICE_LIST_UPDATED); // Update list only.
    break;

  case DS_EV_DEVICE_SPEED_NOT_MATCH:
    trigger_message(DS_EV_DEVICE_SPEED_NOT_MATCH);
    break;

  default:
    pxv_err("Error!Unknown device event.");
    break;
  }
}

// Note: add_msg_listener / add_event_listener / remove_event_listener /
// remove_callback / broadcast_msg / trigger_message / broadcast<T>() /
// dispatch_to<Iface>() are now inline forwarders in sigsession.h that delegate
// to the EventBus owned by _event_bus. The _broadcast_depth guard and the
// _callbacks / _msg_listeners / _event_listeners vectors live inside EventBus.
// broadcast_msg_deferred and trigger_message_deferred have been removed:
// broadcast_msg and trigger_message are now ALWAYS async (queued on qApp via
// Qt::QueuedConnection), so the explicit _deferred variants are redundant.

void SigSession::OnMessage(int msg, int param) {
  // --- Typed event translation (compat layer) ------------------------------
  // Translate every notification-style DSV_MSG_* into the matching typed event
  // and dispatch it to IEventListener registrants via broadcast<T>(). This runs
  // BEFORE the legacy switch below so both listener kinds (IMessageListener via
  // the switch, IEventListener via broadcast) are notified in parallel.
  //
  // The translation table covers all 38 user-facing notification codes; only
  // the 5 "prev" pre/post ordering codes (REV_END_PACKET, START_COLLECT_WORK_PREV,
  // END_COLLECT_WORK_PREV, CURRENT_DEVICE_CHANGE_PREV, STORE_CONF_PREV) are
  // skipped — they drive SigSession's own state machine in the legacy switch
  // below and have no semantic meaning for typed listeners. There is no
  // feedback-loop risk: broadcast<T>() only invokes IEventListener::on_event
  // overrides (never OnMessage itself), and the thread-local re-entrancy guard
  // short-circuits any nested broadcast.
  //
  // Known compat-layer limitations (resolved when call sites migrate to direct
  // broadcast<T>() in Task 5):
  //   * CaptureOwnerChanged.old_owner / ActiveDocumentChanged.old_doc are
  //     nullptr — the legacy (int,int) call sites mutate state before
  //     broadcasting and cannot recover the previous value.
  //   * CopyToDocDone.doc is nullptr — the legacy call site does not pass the
  //     doc pointer through (int param).
  //   * DeviceModeChanged.mode / CollectModeChanged.mode carry `param`, which
  //     is 0 at the current broadcast sites; consumers needing the actual mode
  //     should query the session. Direct broadcast sites will pass the real
  //     value.
  // The broadcast<T>() re-entrancy guard ensures that even if a typed listener
  // synchronously re-emits an event, the nested dispatch is short-circuited.
  switch (msg) {
  case DSV_MSG_CAPTURE_STATE_CHANGED:
    broadcast<interface::CaptureStateChanged>({is_working(), _device_status});
    break;
  case DSV_MSG_CAPTURE_OWNER_CHANGED:
    broadcast<interface::CaptureOwnerChanged>({nullptr, _document_registry->get_capture_owner_document()});
    break;
  case DSV_MSG_TRIGGER_CONFIG_CHANGED:
    broadcast<interface::TriggerConfigChanged>({&_trigger_config});
    break;
  case DSV_MSG_SAMPLE_COUNT_UPDATED:
    broadcast<interface::SampleCountUpdated>(
        {(uint64_t)get_ring_sample_count()});
    break;
  case DSV_MSG_DEVICE_OPTIONS_UPDATED:
    broadcast<interface::DeviceOptionsUpdated>({});
    break;
  case DSV_MSG_ACTIVE_DOCUMENT_CHANGED:
    broadcast<interface::ActiveDocumentChanged>({nullptr, _document_registry->get_active_document()});
    break;
  case DSV_MSG_COPY_TO_DOC_DONE:
    broadcast<interface::CopyToDocDone>({nullptr});
    break;
  case DSV_MSG_DEVICE_MODE_CHANGED:
    broadcast<interface::DeviceModeChanged>({param});
    break;
  case DSV_MSG_COLLECT_MODE_CHANGED:
    broadcast<interface::CollectModeChanged>({param});
    break;
  case DSV_MSG_DEVICE_LIST_UPDATED:
    broadcast<interface::DeviceListUpdated>({});
    break;
  case DSV_MSG_CURRENT_DEVICE_CHANGED:
    broadcast<interface::CurrentDeviceChanged>({});
    break;
  case DSV_MSG_NEW_USB_DEVICE:
    broadcast<interface::UsbDeviceArrived>({});
    break;
  case DSV_MSG_CURRENT_DEVICE_DETACHED:
    broadcast<interface::DeviceDetached>({});
    break;
  case DSV_MSG_SAVE_COMPLETE:
    broadcast<interface::SaveComplete>({});
    break;
  case DSV_MSG_START_COLLECT_WORK:
    broadcast<interface::StartCollectWork>({});
    break;
  case DSV_MSG_COLLECT_START:
    broadcast<interface::CollectStart>({});
    break;
  case DSV_MSG_COLLECT_END:
    broadcast<interface::CollectEnd>({});
    break;
  case DSV_MSG_END_COLLECT_WORK:
    broadcast<interface::EndCollectWork>({});
    break;
  case DSV_MSG_END_DEVICE_OPTIONS:
    broadcast<interface::EndDeviceOptions>({});
    break;
  case DSV_MSG_DEVICE_DURATION_UPDATED:
    broadcast<interface::SampleRateChanged>({});
    break;
  case DSV_MSG_DEVICE_CONFIG_UPDATED:
    broadcast<interface::DeviceConfigUpdated>({});
    break;
  case DSV_MSG_DEMO_OPERATION_MODE_CHNAGED:
    broadcast<interface::DemoModeChanged>({});
    break;
  case DSV_MSG_DATA_POOL_CHANGED:
    broadcast<interface::DataPoolChanged>({});
    break;
  case DSV_MSG_SIMPLE_TRIGGER_CHANGED:
    broadcast<interface::SimpleTriggerChanged>({});
    break;
  case DSV_MSG_GLITCH_FILTER_STARTED:
    broadcast<interface::GlitchFilterStarted>({});
    break;
  case DSV_MSG_GLITCH_FILTER_PROGRESS:
    broadcast<interface::GlitchFilterProgress>({param});
    break;
  case DSV_MSG_GLITCH_FILTER_COMPLETED:
    broadcast<interface::GlitchFilterCompleted>({});
    break;
  case DSV_MSG_GLITCH_FILTER_CLEARED:
    broadcast<interface::GlitchFilterCleared>({});
    break;
  case DSV_MSG_SIGNAL_INVERT_STARTED:
    broadcast<interface::SignalInvertStarted>({});
    break;
  case DSV_MSG_SIGNAL_INVERT_COMPLETED:
    broadcast<interface::SignalInvertCompleted>({});
    break;
  case DSV_MSG_SIGNAL_INVERT_CLEARED:
    broadcast<interface::SignalInvertCleared>({});
    break;
  case DSV_MSG_COPY_IN_PROGRESS_CHANGED:
    broadcast<interface::CopyInProgressChanged>({is_copy_in_progress()});
    break;
  case DSV_MSG_TRIG_NEXT_COLLECT:
    broadcast<interface::TrigNextCollect>({});
    break;
  case DSV_MSG_CLEAR_DECODE_DATA:
    broadcast<interface::ClearDecodeData>({});
    break;
  case DSV_MSG_APP_OPTIONS_CHANGED:
    broadcast<interface::AppOptionsChanged>({});
    break;
  case DSV_MSG_FONT_OPTIONS_CHANGED:
    broadcast<interface::FontOptionsChanged>({});
    break;
  case DSV_MSG_SHORTCUT_CHANGED:
    broadcast<interface::ShortcutChanged>({});
    break;
  case DSV_MSG_STYLE_CHANGED:
    broadcast<interface::StyleChanged>({});
    break;
  default:
    // Core-internal state-machine messages (or messages with no typed
    // equivalent): no translation, fall through to the legacy switch only.
    // The 5 "prev" notification pairs (REV_END_PACKET, START_COLLECT_WORK_PREV,
    // END_COLLECT_WORK_PREV, CURRENT_DEVICE_CHANGE_PREV, STORE_CONF_PREV) are
    // intentionally not typed — they drive pre/post ordering of state-machine
    // transitions and have no semantic meaning for typed listeners.
    break;
  }

  // --- Legacy state-machine handling ---------------------------------------
  // Original switch retained verbatim for backward compatibility. Handles
  // Core-internal state transitions (reload, repeat-collect scheduling,
  // capture-end packet processing, copy-to-doc completion, etc.) that the
  // typed event bus deliberately does NOT replicate.
  switch (msg) {
  case DSV_MSG_DEVICE_OPTIONS_UPDATED:
    reload();
    break;

  case DSV_MSG_TRIG_NEXT_COLLECT: {
    if (_is_working && is_repeat_mode()) {
      if (_capture_manager->_repeat_intvl > 0) {
        _capture_manager->_repeat_hold_prg = 100;
        _capture_manager->_repeat_timer.Start(_capture_manager->_repeat_intvl * 1000);
        int intvl = _capture_manager->_repeat_intvl * 1000 / 20;

        if (intvl >= 100) {
          _capture_manager->_repeat_wait_prog_step = 5;
        } else if (_capture_manager->_repeat_intvl >= 1) {
          intvl = _capture_manager->_repeat_intvl * 1000 / 10;
          _capture_manager->_repeat_wait_prog_step = 10;
        } else {
          intvl = _capture_manager->_repeat_intvl * 1000 / 5;
          _capture_manager->_repeat_wait_prog_step = 20;
        }

        _capture_manager->_repeat_wait_prog_timer.Start(intvl);
      } else {
        _capture_manager->_repeat_hold_prg = 0;
        _capture_manager->exec_capture();
      }
    }
  } break;

  case DSV_MSG_REV_END_PACKET: {
    if (_device_agent.get_work_mode() == LOGIC) {
      bool bAddDecoder = false;
      bool bSwapBuffer = false;

      if (is_single_mode()) {
        if (!_capture_manager->_is_stream_mode)
          bAddDecoder = true;
      } else if (is_repeat_mode()) {
        if (!_capture_manager->_is_stream_mode) {
          bAddDecoder = true;
          bSwapBuffer = true;
        } else if (_capture_manager->_capture_times > 1) {
          bAddDecoder = true;
          bSwapBuffer = true;
        }
      } else if (is_loop_mode()) {
        bAddDecoder = true;
      }

      if (is_repeat_mode()) {
        AppConfig &app = AppConfig::Instance();
        bool swapBackBufferAlways = app.appOptions.swapBackBufferAlways;
        if (!swapBackBufferAlways && !_is_working && _capture_manager->_capture_times > 1) {
          bAddDecoder = false;
          bSwapBuffer = false;
          _capture_data->clear();
        }
      }

      if (bAddDecoder) {
        clear_all_decode_task2();
        _capture_manager->clear_decode_result();
      }

      _capture_manager->_trig_check_timer.Stop();

      // Switch the caputrued data buffer to view.
      if (bSwapBuffer) {
        if (_view_data != _capture_data)
          _view_data->clear();

        _view_data = _capture_data;
        attach_data_to_signal(_view_data);
        set_session_time(_trig_time);

        receive_trigger(_view_data->_trig_pos); // Update trig position.

        trigger_message(DSV_MSG_DATA_POOL_CHANGED);
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
        trigger_message(DSV_MSG_COPY_IN_PROGRESS_CHANGED);

        if (_document_registry->copy_thread().joinable()) {
          _document_registry->copy_thread().join(); // 等待上一个 copy 完成
        }
        _document_registry->copy_thread() = std::thread([this, doc]() {
          copy_data_to_document(doc);
          {
            std::lock_guard<std::mutex> lock(_document_registry->capture_state_mutex());
            _document_registry->copy_in_progress() = false;
          }
          trigger_message(DSV_MSG_COPY_IN_PROGRESS_CHANGED);
          trigger_message(DSV_MSG_COPY_TO_DOC_DONE);
        });
      } else {
        // No active document (typical in headless mode). Skip the deep copy
        // to a SessionDocument and start the decoders directly. The decoders
        // read their snapshots from _view_data via get_signal_models(), so
        // they don't need a SessionDocument to be set up.
        // C4 fix: lock the mutex to clear _capture_owner_document atomically
        // with respect to CaptureOwnerGuard lifecycle.
        {
          std::lock_guard<std::mutex> lock(_document_registry->capture_state_mutex());
          _document_registry->capture_owner_document() = nullptr;
        }
        start_all_decode_tasks();
      }

      frame_ended();
    }
  } break;

  case DSV_MSG_COLLECT_END:
    break;

  case DSV_MSG_COPY_TO_DOC_DONE: {
    // Background copy_data_to_document has completed. Start decoders.
    // NOTE: _capture_owner_document is NOT cleared here — it is now managed
    // by CaptureOwnerGuard for the whole capture session. In repeat mode the
    // owner persists across frames; the guard is reset only on stop_capture
    // or tab close (clear_capture_owner_document).
    start_all_decode_tasks();
    pxv_info("Background copy_data_to_document completed. Decoders started.");
  } break;

  case DS_EV_DEVICE_SPEED_NOT_MATCH: {
    QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_DEVICE_SPEED_TOO_LOW),
                       "Speed too low!"));
    delay_prop_msg(strMsg);
  } break;
  }
}

void SigSession::DeviceConfigChanged() {
  // broadcast_msg is now ASYNC (queued on qApp via Qt::QueuedConnection), so
  // the previous _suppress_config_broadcast guard (which prevented nested
  // reload -> signals_changed -> View AllReplaced UAF during JSON restore) is
  // no longer needed: the caller's stack frame completes before any listener
  // processes the message.
  // Notify UI that device config changed (e.g. disk cache toggle),
  // so sampling duration can be recalculated from SR_CONF_HW_DEPTH
  broadcast_msg(DSV_MSG_SAMPLE_COUNT_UPDATED);
}

bool SigSession::switch_work_mode(int mode) {
  assert(!_is_working);
  int cur_mode = _device_agent.get_work_mode();

  if (cur_mode != mode) {
    set_collect_mode(COLLECT_SINGLE);

    _device_agent.set_config_int16(SR_CONF_DEVICE_MODE, mode);

    if (cur_mode == LOGIC) {
      clear_all_decode_task2();
      _capture_manager->clear_decode_result();
    }

    _capture_manager->_is_stream_mode = false;
    if (mode == LOGIC) {
      if (_device_agent.is_hardware()) {
        _capture_manager->_is_stream_mode = _device_agent.is_stream_mode();
      } else if (_device_agent.is_demo()) {
        _capture_manager->_is_stream_mode = true;
      }
    }

    _capture_data->clear();
    _view_data->clear();
    _capture_data = _view_data;

    init_signals();

    set_cur_snap_samplerate(_device_agent.get_sample_rate());
    set_cur_samplelimits(_device_agent.get_sample_limit());

    pxv_info("Switch work mode to:%d", mode);

    // broadcast_msg is now ALWAYS async (queued on qApp via
    // Qt::QueuedConnection), so View finishes its signals_changed rebuild
    // before handlers access view::Signal::_model. No separate _deferred
    // variant is needed.
    broadcast_msg(DSV_MSG_DEVICE_MODE_CHANGED);

    return true;
  }
  return false;
}

void SigSession::clear_signals() {
  _math_stack.reset();

  _signal_models.clear();
}

std::shared_ptr<data::SignalModel> SigSession::get_signal_by_index(int index) {
  for (auto &m : _signal_models) {
    if (m->index() == index)
      return m;
  }
  return nullptr;
}

void SigSession::on_load_config_end() {
  set_cur_snap_samplerate(_device_agent.get_sample_rate());
  set_cur_samplelimits(_device_agent.get_sample_limit());
}

void SigSession::clear_view_data() {
  _view_data->clear();
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
  if (model->type() == api::ChannelType::Logic ||
      model->type() == api::ChannelType::Analog) {
    _device_agent.set_channel_name(model->index(), name.toUtf8());
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
  for (auto &m : _signal_models) {
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
  int mode = _device_agent.get_work_mode();

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
  int mode = _device_agent.get_work_mode();
  if (mode == LOGIC) {
    return _view_data->get_logic()->get_ring_sample_count();
  } else if (mode == DSO) {
    return _view_data->get_dso()->get_ring_sample_count();
  } else {
    return _view_data->get_analog()->get_ring_sample_count();
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
  return _view_data->get_logic();
}

data::AnalogSnapshot *SigSession::get_analog_snapshot() {
  return _view_data->get_analog();
}

data::DsoSnapshot *SigSession::get_dso_snapshot() {
  return _view_data->get_dso();
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

void SigSession::register_document(data::SessionDocument *doc) {
  _document_registry->register_document(doc);
}

void SigSession::unregister_document(data::SessionDocument *doc) {
  _document_registry->unregister_document(doc);
}

void SigSession::set_trigger_config(const data::TriggerConfig &cfg) {
  _trigger_config = cfg;
  broadcast_msg(DSV_MSG_TRIGGER_CONFIG_CHANGED);
}

void SigSession::sync_trigger_to_libsigrok() {
  // Core→libsigrok 触发配置唯一同步点。在 start_capture 内部 ds_start_collect 前调用。
  // 消除 TriggerDock::commit_trigger / SessionService::start_capture /
  // TriggerDock::try_commit_trigger 各自调 ds_trigger_* 导致的互相覆盖。
  //
  // probes 参数 = Logic 类型 SignalModel 数量（参考 triggerdock.cpp 的 _cur_ch_num
  // 与 session_service.cpp 的处理）。计算一次保存为局部变量。
  uint16_t probes = 0;
  for (const auto &m : _signal_models) {
    if (m && m->type() == api::ChannelType::Logic)
      probes++;
  }

  const auto &cfg = _trigger_config;
  const int trig_pos = cfg.trigger_pos();

  if (cfg.mode() == data::TriggerConfig::Simple) {
    // Simple 模式：遍历 SignalModel.trig_type() 映射到 ds_trigger_probe_set。
    ds_trigger_reset();

    bool any_triggered = false;
    for (const auto &m : _signal_models) {
      if (!m || m->type() != api::ChannelType::Logic)
        continue;
      const uint16_t probe = static_cast<uint16_t>(m->index());
      char c0 = 'X';
      switch (m->trig_type()) {
      case data::SignalModel::POSTRIG: c0 = 'R'; any_triggered = true; break;
      case data::SignalModel::NEGTRIG: c0 = 'F'; any_triggered = true; break;
      case data::SignalModel::HIGTRIG: c0 = '1'; any_triggered = true; break;
      case data::SignalModel::LOWTRIG: c0 = '0'; any_triggered = true; break;
      case data::SignalModel::EDGTRIG: c0 = 'C'; any_triggered = true; break;
      case data::SignalModel::NONTRIG:
      default: c0 = 'X'; break;
      }
      ds_trigger_probe_set(probe, c0, 'X');
    }

    if (any_triggered) {
      ds_trigger_set_en(1);
      ds_trigger_set_mode(SIMPLE_TRIGGER);
    } else {
      ds_trigger_set_en(0);
    }
    ds_trigger_set_pos(trig_pos);
  } else if (cfg.mode() == data::TriggerConfig::Adv) {
    // Adv 模式：从 _trigger_config.stages() 一次性同步到 ds_trigger_*。
    ds_trigger_reset();
    ds_trigger_set_en(true);
    ds_trigger_set_mode(ADV_TRIGGER);
    ds_trigger_set_pos(trig_pos);

    const int stage_count = cfg.stage_count();
    if (stage_count > 0) {
      ds_trigger_set_stage(stage_count - 1);
      const auto &stages = cfg.stages();
      for (int i = 0; i < stage_count && i < (int)stages.size(); ++i) {
        const auto &st = stages[i];
        QByteArray v0 = st.value0.toLocal8Bit();
        QByteArray v1 = st.value1.toLocal8Bit();
        ds_trigger_stage_set_value(i, probes, v0.data(), v1.data());
        ds_trigger_stage_set_logic(i, probes, st.logic);
        ds_trigger_stage_set_inv(i, probes, st.inv0, st.inv1);
        ds_trigger_stage_set_count(i, probes, st.count0, st.count1);
      }
    }
  } else if (cfg.mode() == data::TriggerConfig::Serial) {
    // Serial 模式：与 Adv 相同的 stage 同步，仅 mode 不同。
    // stages[1].count0 应为 1，stages[3].count0 应为 serial_bits-1
    // （这些在 commit_trigger 时已写入 TriggerConfig，sync 时直接用 stage.count0/count1）
    ds_trigger_reset();
    ds_trigger_set_en(true);
    ds_trigger_set_mode(SERIAL_TRIGGER);
    ds_trigger_set_pos(trig_pos);

    const int stage_count = cfg.stage_count();
    if (stage_count > 0) {
      ds_trigger_set_stage(stage_count - 1);
      const auto &stages = cfg.stages();
      for (int i = 0; i < stage_count && i < (int)stages.size(); ++i) {
        const auto &st = stages[i];
        QByteArray v0 = st.value0.toLocal8Bit();
        QByteArray v1 = st.value1.toLocal8Bit();
        ds_trigger_stage_set_value(i, probes, v0.data(), v1.data());
        ds_trigger_stage_set_logic(i, probes, st.logic);
        ds_trigger_stage_set_inv(i, probes, st.inv0, st.inv1);
        ds_trigger_stage_set_count(i, probes, st.count0, st.count1);
      }
    }
  }
}

void SigSession::copy_data_to_document(data::SessionDocument *doc) {
  if (!doc || !_view_data || !have_view_data())
    return;

  doc->set_samplerate(_view_data->_cur_snap_samplerate);
  doc->set_samplelimits(_view_data->_cur_samplelimits);
  doc->set_trigger_pos(_view_data->_trig_pos);

  doc->copy_from_logic(_view_data->get_logic());
  doc->copy_from_analog(_view_data->get_analog());
  doc->copy_from_dso(_view_data->get_dso());
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
  if (_view_data->get_logic()->is_disk_cache_active())
    return _view_data->get_logic()->get_disk_write_queue_depth();
  return 0;
}

double SigSession::get_disk_write_speed_mbps() {
  if (_view_data->get_logic()->is_disk_cache_active())
    return _view_data->get_logic()->get_disk_write_speed_mbps();
  return 0.0;
}

bool SigSession::is_disk_write_disk_full() { return false; }

} // namespace pv
