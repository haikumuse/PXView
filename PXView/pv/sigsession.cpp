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

#include <QDir>
#include <QString>
#include <assert.h>
#include <chrono>
#include <functional>
#include <map>
#include <stdexcept>
#include <sys/stat.h>

static QString get_default_disk_cache_path() {
  return QDir::tempPath() + "/PXView_cache";
}

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

std::vector<data::DecoderStack *> SigSession::_empty_decoder_stacks;

SigSession::SigSession() {
  _map_zoom = 0;
  _repeat_hold_prg = 0;
  _repeat_intvl = 1;
  _error = No_err;
  _is_instant = false;
  _is_working = false;
  _is_saving = false;
  _device_status = ST_INIT;
  _noData_cnt = 0;
  _data_lock = false;
  _data_updated = false;
  _clt_mode = COLLECT_SINGLE;
  _rt_refresh_time_id = 0;
  _rt_ck_refresh_time_id = 0;
  _view_data = NULL;
  _capture_data = NULL;
  _is_stream_mode = false;
  _is_action = false;
  _decoder_pannel = NULL;
  _active_document = nullptr;
  _is_triged = false;
  _trigger_preconfigured = false;
  _dso_status_valid = false;
  _glitch_filter_thread = nullptr;
  _glitch_filter_running = false;
  _signal_invert_thread = nullptr;
  _signal_invert_running = false;
  _copy_in_progress = false;
  _capture_owner_document = nullptr;

  _data_list.push_back(new SessionData());
  _data_list.push_back(new SessionData());
  _view_data = _data_list[0];
  _capture_data = _data_list[0];

  this->add_msg_listener(this);

  _decoder_model = new pv::data::DecoderModel(NULL);

  _lissajous_model = nullptr;
  _math_stack = nullptr;
  _bClose = false;
  _work_time_id = 0;
  _capture_times = 0;
  _confirm_store_time_id = 0;
  _repeat_wait_prog_step = 10;

  _device_agent.set_callback(this);

  _feed_timer.SetCallback(std::bind(&SigSession::feed_timeout, this));
  _repeat_timer.SetCallback(
      std::bind(&SigSession::repeat_capture_wait_timeout, this));
  _repeat_wait_prog_timer.SetCallback(
      std::bind(&SigSession::repeat_wait_prog_timeout, this));
  _refresh_rt_timer.SetCallback(
      std::bind(&SigSession::realtime_refresh_timeout, this));
  _trig_check_timer.SetCallback(
      std::bind(&SigSession::trig_check_timeout, this));
}

SigSession::SigSession(SigSession &o) { (void)o; }

SigSession::~SigSession() {
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

  ds_set_datafeed_callback_ex(data_feed_callback_ex, this);

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
  assert(!_callbacks.empty());

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
}

void SigSession::capture_init() {
  // update instant setting
  _device_agent.set_config_bool(SR_CONF_INSTANT, _is_instant);
  update_capture();

  set_cur_snap_samplerate(_device_agent.get_sample_rate());
  set_cur_samplelimits(_device_agent.get_sample_limit());

  _data_updated = false;
  _trigger_flag = false;
  _trigger_ch = 0;
  _hw_replied = false;
  _rt_refresh_time_id = 0;
  _rt_ck_refresh_time_id = 0;
  _noData_cnt = 0;

  data_unlock();

  // Init data container
  _capture_data->clear();
  _capture_data->get_logic()->set_disk_cache_config(_disk_cache_config);

  int mode = _device_agent.get_work_mode();
  if (mode == DSO) {
    for (auto m : _spectrum_stacks) {
      m->init();
    }

    if (_math_stack) {
      _math_stack->init();
    }
  }

  // In multi-tab architecture, SigSession::_signals do not have viewports.
  // We cannot call UI-dependent methods (like set_zero_ratio) on them here.
  // Hardware offset is already updated via View's own signal events when user changes it.

  // Start timer
  if (mode == DSO || mode == ANALOG)
    _feed_timer.Start(FeedInterval);
  else
    _feed_timer.Stop();
}

bool SigSession::start_capture(bool instant) {
  _is_action = true;
  int ret = action_start_capture(instant);
  _is_action = false;
  return ret;
}

bool SigSession::action_start_capture(bool instant) {
  assert(!_callbacks.empty());

  pxv_info("Start collect.");

  if (_is_working) {
    pxv_err("Error! Is working now.");
    return false;
  }

  if (_signal_models.empty()) {
    pxv_info("ERROR: channel list is empty, unable to capture data.");
    return false;
  }

  // Check that a device instance has been selected.
  if (_device_agent.have_instance() == false) {
    pxv_err("Error!No device selected");
    assert(false);
  }
  if (_device_status == ST_RUNNING || _device_agent.is_collecting()) {
    pxv_err("Error!Device is running.");
    return false;
  }

  clear_all_decode_task2();
  clear_decode_result();

  _capture_data->clear();
  _view_data->clear();
  _is_stream_mode = false;
  _capture_times = 0;
  _dso_packet_count = 0;
  _dso_status_valid = false;

  _capture_data = _view_data;
  set_cur_snap_samplerate(_device_agent.get_sample_rate());
  set_cur_samplelimits(_device_agent.get_sample_limit());

  set_session_time(QDateTime::currentDateTime());

  int mode = _device_agent.get_work_mode();
  if (mode == LOGIC) {
    if (is_repeat_mode() && _device_agent.is_hardware() &&
        _device_agent.is_stream_mode()) {
      set_repeat_intvl(0.1);
    }

    if (_device_agent.is_hardware()) {
      _is_stream_mode = _device_agent.is_stream_mode();
    } else if (_device_agent.is_demo() || _device_agent.is_file()) {
      _is_stream_mode = true;
    }

    if (is_loop_mode() && !_is_stream_mode) {
      set_collect_mode(COLLECT_SINGLE); // Reset the capture mode.
    }

    if (is_loop_mode() && _device_agent.is_demo()) {
      QString opt_mode = _device_agent.get_demo_operation_mode();
      if (opt_mode != "random") {
        set_collect_mode(COLLECT_SINGLE);
      }
    }

    if (_device_agent.is_hardware() || _device_agent.is_demo()) {
      bool bv = is_loop_mode() && _is_stream_mode;
      _device_agent.set_config_bool(SR_CONF_LOOP_MODE, bv);
    }
  }

  if (mode == DSO && _device_agent.is_hardware()) {
    uint32_t ref_max = 0;
    uint32_t ref_min = 0;
    _device_agent.get_config_uint32(SR_CONF_REF_MIN, ref_min);
    _device_agent.get_config_uint32(SR_CONF_REF_MAX, ref_max);
    _view_data->get_dso()->set_ref_range(ref_max, ref_min);
  }

  trigger_message(DSV_MSG_CAPTURE_STATE_CHANGED);

  bool disk_cache_enabled = false;
  _device_agent.get_config_bool(SR_CONF_DISK_CACHE_ENABLE, disk_cache_enabled);
  if (disk_cache_enabled) {
    QString cache_path;
    _device_agent.get_config_string(SR_CONF_DISK_CACHE_PATH, cache_path);
    if (cache_path.isEmpty()) {
      cache_path = get_default_disk_cache_path();
      _device_agent.set_config_string(SR_CONF_DISK_CACHE_PATH,
                                      cache_path.toUtf8().data());
    }
  }

  _disk_cache_config.enabled = false;

  pxv_info(
      "SigSession::start_capture: _is_stream_mode=%d, disk_cache_enabled=%d",
      _is_stream_mode, disk_cache_enabled);

  if (_is_stream_mode && disk_cache_enabled) {
    _disk_cache_config.enabled = true;

    QString cache_path;
    _device_agent.get_config_string(SR_CONF_DISK_CACHE_PATH, cache_path);
    if (cache_path.isEmpty()) {
      cache_path = get_default_disk_cache_path();
    }
    _disk_cache_config.cache_path = cache_path.toStdString();

    double disk_gb = 16;
    _device_agent.get_config_double(SR_CONF_STREAM_BUFF, disk_gb);
    _disk_cache_config.total_cache_depth_gb = (uint64_t)disk_gb;
    _disk_cache_config.memory_size_gb =
        0; // mmap mode: all data goes to disk file
    _disk_cache_config.calculate();

    uint64_t bytes_per_block = 2105376;
    _disk_cache_config.hot_window_blocks = _disk_cache_config.memory_size_gb *
                                           1024ULL * 1024 * 1024 /
                                           bytes_per_block;

    pxv_info("SigSession::start_capture: Configured disk cache: "
             "disk_gb=%f, path=%s",
             disk_gb, _disk_cache_config.cache_path.c_str());
  } else {
    pxv_info("SigSession::start_capture: Disk cache NOT configured.");
  }

  // update setting
  if (_device_agent.is_file())
    _is_instant = true;
  else
    _is_instant = instant;

  trigger_message(DSV_MSG_START_COLLECT_WORK_PREV);

  if (exec_capture()) {
    _work_time_id++;
    _is_working = true;
    _capture_owner_document = _active_document;
    trigger_message(DSV_MSG_START_COLLECT_WORK);

    // Start a timer, for able to refresh the view per (1000 / 30)ms.
    if (is_realtime_refresh()) {
      _refresh_rt_timer.Start(1000 / 30);
    }

    return true;
  }

  return false;
}

bool SigSession::exec_capture() {
  if (_device_agent.is_collecting()) {
    pxv_err("Error!Device is running.");
    return false;
  }

  // Wait for background copy_data_to_document to complete before
  // starting a new capture, to prevent source data from being cleared.
  if (_copy_in_progress) {
    pxv_info("Waiting for background copy_data_to_document to complete...");
    while (_copy_in_progress) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  if (_device_agent.have_enabled_channel() == false) {
    QString err_str(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_NO_ENABLED_CHANNEL),
                        "No channels enabled!"));
    MsgBox::Show(err_str);
    return false;
  }

  _capture_times++;
  _is_triged = false;

  int mode = _device_agent.get_work_mode();
  bool bAddDecoder = false;
  bool bSwapBuffer = false;

  if (mode == DSO || mode == ANALOG) {
    // reset measure of dso signal
    for (auto m : _signal_models) {
      if (m->type() == api::ChannelType::Dso) {
        // TODO: verify - view::DsoSignal::set_mValid(false) was a UI method.
        // Validity reset should be handled by the View layer.
      }
    }
  } else {
    if (is_single_mode()) {
      if (_is_stream_mode)
        bAddDecoder = true;
    } else if (is_repeat_mode()) {
      if (_is_stream_mode) {
        if (_capture_times == 1)
          bAddDecoder = true;
        else
          bSwapBuffer = true;
      } else {
        bSwapBuffer = true;
      }
    } else if (is_loop_mode()) {
    }
  }

  if (mode == LOGIC && _device_agent.is_hardware() &&
      _device_agent.get_hardware_operation_mode() == LO_OP_BUFFER) {
    _trig_check_timer.Start(200);
  }

  if (bAddDecoder) {
    clear_all_decode_task2();
    clear_decode_result();

    // CRITICAL: Release the active document's copy of the old mmap data.
    // copy_data_to_document() shares the mmap via shared_ptr. If we don't
    // clear the document's LogicSnapshot here, its shared_ptr reference
    // keeps the old multi-GB mmap alive while a new one is created,
    // causing memory to double on every capture.
    if (_active_document) {
      _active_document->get_active_logic()->clear();
    }
  }

  // Set the buffer to store the captured data
  if (bSwapBuffer) {
    int buf_index = -1;
    for (int i = 0; i < (int)_data_list.size(); i++) {
      if (_data_list[i] != _view_data) {
        buf_index = i;
        break;
      }
    }

    if (buf_index < 0) {
      _data_list.push_back(new SessionData());
      buf_index = (int)_data_list.size() - 1;
    }

    _capture_data = _data_list[buf_index];
    _capture_data->clear();

    set_cur_snap_samplerate(_device_agent.get_sample_rate());
    set_cur_samplelimits(_device_agent.get_sample_limit());
  }

  capture_init();

  // IMPORTANT: Ensure the session's logic signals point to the current capture
  // buffer. This is required because DecoderStack searches the session's signal
  // list to find the data source. Without this, decoders in stream mode would
  // bind to the old, cleared document snapshot and fail to show results.
  attach_data_to_signal(_capture_data);

  if (_device_agent.start() == false) {
    pxv_err("Start collect error!");
    return false;
  }

  if (mode == LOGIC) {
    for (auto de : decode_traces()) {
      if (bAddDecoder) {
        de->set_capture_end_flag(false);
        de->frame_ended();
        add_decode_task(de);
      }
    }
  }

  return true;
}

bool SigSession::stop_capture() {
  _is_action = true;
  int ret = action_stop_capture();
  _is_action = false;
  return ret;
}

bool SigSession::action_stop_capture() {
  if (!_is_working)
    return false;

  pxv_info("Stop collect.");

  if (_bClose) {
    _is_working = false;
    _repeat_timer.Stop();
    _repeat_wait_prog_timer.Stop();
    _refresh_rt_timer.Stop();
    exit_capture();
    return true;
  }

  bool wait_upload = false;
  if (is_single_mode() && _device_agent.get_work_mode() == LOGIC) {
    _device_agent.get_config_bool(SR_CONF_WAIT_UPLOAD, wait_upload);
  }

  if (!wait_upload) {
    _is_working = false;
    if (!_copy_in_progress) {
      _capture_owner_document = nullptr;
    }
    _repeat_timer.Stop();
    _repeat_wait_prog_timer.Stop();
    _refresh_rt_timer.Stop();

    if (_repeat_hold_prg != 0 && is_repeat_mode()) {
      _repeat_hold_prg = 0;
      repeat_hold(_repeat_hold_prg);
    }

    trigger_message(DSV_MSG_END_COLLECT_WORK_PREV);

    exit_capture();

    data_unlock();

    if (is_repeat_mode() && _device_status != ST_RUNNING) {
      trigger_message(DSV_MSG_END_COLLECT_WORK);
    }

    return true;
  } else {
    pxv_info("Data is uploading from device data buffer, waiting for stop.");
  }
  return false;
}

void SigSession::exit_capture() {
  _is_instant = false;

  _feed_timer.Stop();

  if (_device_agent.is_collecting())
    _device_agent.stop();
}

bool SigSession::get_capture_status(bool &triggered, int &progress) {
  uint64_t sample_limits = cur_samplelimits();
  sr_status status;

  if (_device_agent.get_device_status(status, true)) {
    triggered = status.trig_hit & 0x01;
    uint64_t captured_cnt = status.trig_hit >> 2;

    captured_cnt =
        ((uint64_t)status.captured_cnt0 +
         ((uint64_t)status.captured_cnt1 << 8) +
         ((uint64_t)status.captured_cnt2 << 16) +
         ((uint64_t)status.captured_cnt3 << 24) + (captured_cnt << 32));

    int mode = _device_agent.get_work_mode();

    if (mode == DSO)
      captured_cnt =
          captured_cnt * _signal_models.size() / get_ch_num(SR_CHANNEL_DSO);

    if (triggered)
      progress = (sample_limits - captured_cnt) * 100.0 / sample_limits;
    else
      progress = captured_cnt * 100.0 / sample_limits;

    if (progress == 100 && mode == LOGIC &&
        _capture_data->get_logic()->have_data() == false) {
      progress = 0;
    }

    return true;
  }
  return false;
}

std::vector<data::SignalModel *> &SigSession::get_signal_models() {
  return _signal_models;
}

void SigSession::check_update() {
  ds_lock_guard lock(_data_mutex);

  if (_device_agent.is_collecting() == false)
    return;

  if (_data_updated) {
    if (_device_agent.get_work_mode() != LOGIC)
      data_updated();

    _data_updated = false;
    _noData_cnt = 0;
    data_auto_unlock();
  } else {
    if (++_noData_cnt >= (WaitShowTime / FeedInterval))
      nodata_timeout();
  }
}

void SigSession::init_signals() {
  if (_device_agent.have_instance() == false) {
    assert(false);
  }

  std::vector<data::SignalModel *> models;
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
      auto *model = new data::SignalModel();
      model->set_index(probe->index);
      model->set_name(probe->name ? probe->name : "");
      model->set_type(ch_type);
      model->set_enabled(probe->enabled);

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
  std::vector<data::SignalModel *>().swap(_signal_models);
  _signal_models = models;
  make_channels_view_index();

  spectrum_rebuild();
  lissajous_disable();
  math_disable();

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

  std::vector<data::SignalModel *> models;
  int mode = _device_agent.get_work_mode();

  set_cur_snap_samplerate(_device_agent.get_sample_rate());
  set_cur_samplelimits(_device_agent.get_sample_limit());

  for (GSList *l = _device_agent.get_channels(); l; l = l->next) {
    sr_channel *probe = (sr_channel *)l->data;
    assert(probe);

    if (mode == LOGIC && probe->type != SR_CHANNEL_LOGIC) {
      continue;
    }

    if (mode == ANALOG && probe->type != SR_CHANNEL_ANALOG) {
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

    case SR_CHANNEL_ANALOG:
      if (probe->enabled) {
        should_create = true;
        ch_type = api::ChannelType::Analog;
      }
      break;
    }

    if (should_create) {
      // Try to preserve settings from the existing model with the same index
      data::SignalModel *old_model = nullptr;
      for (auto *m : _signal_models) {
        if (m->index() == (int)probe->index) {
          old_model = m;
          break;
        }
      }

      auto *model = new data::SignalModel();
      model->set_index(probe->index);
      model->set_name(probe->name ? probe->name : "");
      model->set_type(ch_type);
      model->set_enabled(probe->enabled);

      if (ch_type == api::ChannelType::Analog) {
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
    std::vector<data::SignalModel *>().swap(_signal_models);
    _signal_models = models;
    make_channels_view_index();
  } else if (mode == LOGIC || mode == ANALOG) {
    pxv_info("ERROR: Unable to create any channel.");
    clear_signals();
  }

  spectrum_rebuild();
}

void SigSession::refresh(int holdtime) {
  ds_lock_guard lock(_data_mutex);

  data_lock();
  _view_data->get_logic()->init();

  clear_all_decode_task2();
  clear_decode_result();

  _view_data->get_dso()->init();

  for (auto m : _spectrum_stacks) {
    m->init();
  }

  if (_math_stack)
    _math_stack->init();

  _view_data->get_analog()->init();

  _out_timer.TimeOut(holdtime, std::bind(&SigSession::feed_timeout, this));
  _data_updated = true;
}

void SigSession::data_auto_lock(int lock) { _data_auto_lock = lock; }

void SigSession::data_auto_unlock() {
  if (_data_auto_lock > 0)
    _data_auto_lock--;
  else if (_data_auto_lock < 0)
    _data_auto_lock = 0;
}

bool SigSession::get_data_auto_lock() { return _data_auto_lock != 0; }

void SigSession::feed_in_header(const sr_dev_inst *sdi) {
  (void)sdi;
  receive_header();
}

void SigSession::feed_in_meta(const sr_dev_inst *sdi,
                              const sr_datafeed_meta &meta) {
  (void)sdi;

  for (const GSList *l = meta.config; l; l = l->next) {
    const sr_config *const src = (const sr_config *)l->data;
    switch (src->key) {
    case SR_CONF_SAMPLERATE:
      /// @todo handle samplerate changes
      /// samplerate = (uint64_t *)src->value;
      break;
    }
  }
}

void SigSession::feed_in_trigger(const ds_trigger_pos &trigger_pos) {
  _hw_replied = true;

  if (_device_agent.get_work_mode() != DSO) {
    _trigger_flag = (trigger_pos.status & 0x01);
    if (_trigger_flag) {
      _capture_data->_trig_pos = trigger_pos.real_pos;

      // Update trig position for current view.
      if (_capture_data == _view_data) {
        receive_trigger(_capture_data->_trig_pos);
      }
    }
  } else {
    int probe_count = 0;
    int probe_en_count = 0;

    for (const GSList *l = _device_agent.get_channels(); l; l = l->next) {
      const sr_channel *const probe = (const sr_channel *)l->data;
      if (probe->type == SR_CHANNEL_DSO) {
        probe_count++;
        if (probe->enabled)
          probe_en_count++;
      }
    }

    _capture_data->_trig_pos =
        trigger_pos.real_pos * probe_count / probe_en_count;
    receive_trigger(_capture_data->_trig_pos);
  }
}

void SigSession::feed_in_logic(const sr_datafeed_logic &o) {
  if (_capture_data->get_logic()->memory_failed()) {
    pxv_err("Unexpected logic packet");
    return;
  }

  if (!_is_triged && o.length > 0) {
    _is_triged = true;
    _trig_time = QDateTime::currentDateTime();
  }

  if (_capture_data->get_logic()->last_ended()) {
    _capture_data->get_logic()->set_loop(is_loop_mode());

    bool bNotFree = !_running_tasks.empty() && _view_data == _capture_data;

    _capture_data->get_logic()->first_payload(
        o, _device_agent.get_sample_limit(), _device_agent.get_channels(),
        !bNotFree);

    // @todo Putting this here means that only listeners querying
    // for logic will be notified. Currently the only user of
    // frame_began is DecoderStack, but in future we need to signal
    // this after both analog and logic sweeps have begun.
    frame_began();
  } else {
    // Append to the existing data snapshot
    _capture_data->get_logic()->append_payload(o);
  }

  if (_capture_data->get_logic()->memory_failed()) {
    _error = Malloc_err;
    session_error();
    return;
  }

  set_receive_data_len(o.length * 8 / get_ch_num(SR_CHANNEL_LOGIC));

  _data_updated = true;
}

void SigSession::feed_in_dso(const sr_datafeed_dso &o) {
  if (_capture_data->get_dso()->memory_failed()) {
    pxv_err("Unexpected dso packet");
    return; // This dso packet was not expected.
  }

  if (_is_instant == false) {
    sr_status status;

    if (_device_agent.get_device_status(status, false)) {
      _dso_status_valid = true;
      _dso_status = status;
    }
  }

  _dso_packet_count++;

  if (!_is_triged && o.num_samples > 0) {
    _is_triged = true;
    _trig_time = QDateTime::currentDateTime();
    set_session_time(_trig_time);
  }

  if (_capture_data->get_dso()->last_ended()) {
    // In multi-tab architecture, SigSession::_signals do not have a viewport,
    // so we cannot and should not call get_view_rect() on them. 
    // The View's own cloned signals will handle their own rendering scales.

    // first payload
    _capture_data->get_dso()->first_payload(
        o, _device_agent.get_sample_limit(), _device_agent.get_channels(),
        _is_instant, _device_agent.is_file());
    frame_began();
  } else {
    // Append to the existing data snapshot
    _capture_data->get_dso()->append_payload(o);
  }

  if (o.num_samples != 0 && (!_is_instant || _dso_packet_count == 1)) {
    // update current sample rate
    set_cur_snap_samplerate(_device_agent.get_sample_rate());
  }

  if (_capture_data->get_dso()->memory_failed()) {
    _error = Malloc_err;
    session_error();
    return;
  }

  // calculate related spectrum results
  for (auto m : _spectrum_stacks) {
    // TODO: verify - view::SpectrumTrace::enabled() check was removed.
    // SpectrumStack has no enabled flag; calc_fft checks internal state.
    m->calc_fft();
  }

  // calculate related math results
  if (_math_stack) {
    // TODO: verify - MathStack::calc_math requires vDialfactor from view::MathTrace.
    // Need to determine how to obtain this value after de-view-ization.
    _math_stack->realloc(_device_agent.get_sample_limit());
    // _math_stack->calc_math(factor); // TODO: re-enable after MathStack de-view-ization
  }

  _trigger_flag = o.trig_flag;
  _trigger_ch = o.trig_ch;

  // Trigger update()
  set_receive_data_len(o.num_samples);

  if (!_is_instant)
    data_lock();

  _data_updated = true;
}

void SigSession::feed_in_analog(const sr_datafeed_analog &o) {
  if (_capture_data->get_analog()->memory_failed()) {
    pxv_err("Unexpected analog packet");
    return; // This analog packet was not expected.
  }

  if (_capture_data->get_analog()->last_ended()) {
    // In multi-tab architecture, SigSession::_signals do not have a viewport,
    // so we cannot and should not call UI rendering methods on them.

    // first payload
    _capture_data->get_analog()->first_payload(
        o, _device_agent.get_sample_limit(), _device_agent.get_channels());
    frame_began();
  } else {
    // Append to the existing data snapshot
    _capture_data->get_analog()->append_payload(o);
  }

  if (_capture_data->get_analog()->memory_failed()) {
    _error = Malloc_err;
    session_error();
    return;
  }

  set_receive_data_len(o.num_samples);
  _data_updated = true;
}

void SigSession::data_feed_in(const struct sr_dev_inst *sdi,
                              const struct sr_datafeed_packet *packet) {
  assert(sdi);
  assert(packet);

  ds_lock_guard lock(_data_mutex);

  if (_data_lock && packet->type != SR_DF_END)
    return;

  if (packet->type != SR_DF_END && packet->status != SR_PKT_OK) {
    _error = Pkt_data_err;
    session_error();
    return;
  }

  switch (packet->type) {
  case SR_DF_HEADER:
    feed_in_header(sdi);
    break;

  case SR_DF_META:
    assert(packet->payload);
    feed_in_meta(sdi, *(const sr_datafeed_meta *)packet->payload);
    break;

  case SR_DF_TRIGGER:
    assert(packet->payload);
    feed_in_trigger(*(const ds_trigger_pos *)packet->payload);
    break;

  case SR_DF_LOGIC:
    assert(packet->payload);
    feed_in_logic(*(const sr_datafeed_logic *)packet->payload);
    break;

  case SR_DF_DSO:
    assert(packet->payload);
    feed_in_dso(*(const sr_datafeed_dso *)packet->payload);
    break;

  case SR_DF_ANALOG:
    assert(packet->payload);
    feed_in_analog(*(const sr_datafeed_analog *)packet->payload);
    break;

  case SR_DF_OVERFLOW: {
    if (_error == No_err) {
      _error = Data_overflow;
      session_error();
    }
    break;
  }
  case SR_DF_END: {
    pxv_info("------------SR_DF_END packet.");

    _capture_data->get_logic()->capture_ended();
    _capture_data->get_dso()->capture_ended();
    _capture_data->get_analog()->capture_ended();

    if (packet->status != SR_PKT_OK) {
      _error = Pkt_data_err;
      session_error();
    } else {
      int mode = _device_agent.get_work_mode();

      // Post a message to start all decode tasks.
      if (mode == LOGIC) {
        trigger_message(DSV_MSG_REV_END_PACKET);
      } else {
        if (mode == DSO && _is_instant) {
          sr_status status;

          if (_device_agent.get_device_status(status, false)) {
            _dso_status_valid = true;
            _dso_status = status;
          }
        }

        frame_ended();
      }
    }

    break;
  }
  }
}

void SigSession::data_feed_callback_ex(const struct sr_dev_inst *sdi,
                                       const struct sr_datafeed_packet *packet,
                                       void *user_data) {
  assert(user_data);
  static_cast<SigSession *>(user_data)->data_feed_in(sdi, packet);
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

bool SigSession::add_decoder(
    srd_decoder *const dec, bool silent, DecoderStatus *dstatus,
    std::list<pv::data::decode::Decoder *> &sub_decoders,
    data::DecoderStack *&out_stack) {
  if (dec == NULL) {
    pxv_err("Decoder instance is null!");
    assert(false);
  }

  out_stack = NULL;

  try {
    bool ret = false;

    // Create the decoder
    std::map<const srd_channel *, int> probes;
    data::DecoderStack *decoder_stack =
        new data::DecoderStack(this, dec, dstatus);
    assert(decoder_stack);

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
        // DecoderStack has no set_name method; name management needs a new mechanism.
      }
    }

    sub_decoders.clear();

    // create_popup was a view::DecodeTrace UI method, removed during de-view-ization.
    // The View layer (Task 9/10) will handle decoder option popups.
    ret = true;

    if (ret) {
      if (_active_document) {
        _active_document->get_decoder_stacks().push_back(decoder_stack);
      } else {
        _empty_decoder_stacks.push_back(decoder_stack);
      }
      decoder_stack->set_owner_document(_active_document);

      // add decode task from ui
      if (!silent && have_view_data()) {
        add_decode_task(decoder_stack);
      }

      data_updated();

      out_stack = decoder_stack;
    } else {
      delete decoder_stack;
    }

    return ret;
  } catch (...) {
    pxv_err("Error!add_decoder() throws an exception.");
  }

  return false;
}

int SigSession::get_trace_index_by_key_handel(void *handel) {
  int dex = 0;

  for (auto stack : decode_traces()) {
    if (stack->get_key_handel() == handel) {
      return dex;
    }
    ++dex;
  }

  return -1;
}

void SigSession::remove_decoder(int index) {
  int size = (int)decode_traces().size();
  (void)size;
  assert(index < size);

  auto it = decode_traces().begin() + index;
  auto stack = (*it);
  decode_traces().erase(it);

  // decode_traces() returns _active_document->get_decoder_stacks() (or
  // _empty_decoder_stacks), so the erase above already removed it from the
  // document's list.

  // Stop the decode work and mark for deletion
  remove_decode_task(stack);
  stack->_delete_flag = true;

  // Check if the decode thread is still using this stack.
  // We must NOT join threads here as that can deadlock
  // (decode thread may need the main thread for Qt signals).
  bool thread_holds_stack = false;
  {
    std::lock_guard<std::mutex> lock(_running_tasks_mutex);
    for (auto *task : _running_tasks) {
      if (task == stack) {
        thread_holds_stack = true;
        break;
      }
    }
  }

  if (!thread_holds_stack) {
    // No thread is using this stack, safe to delete now
    delete stack;
    signals_changed();
  }
  // If thread still holds the stack, decode_single_task will
  // delete it via DESTROY_QT_LATER when it sees _delete_flag
}

void SigSession::remove_decoder_by_key_handel(void *handel) {
  int dex = get_trace_index_by_key_handel(handel);
  remove_decoder(dex);
}

void SigSession::rst_decoder(int index) {
  // TODO: Previously this called view::DecodeTrace::create_popup(false) to
  // re-open the decoder options dialog. After de-view-ization, SigSession
  // does not own view::DecodeTrace instances, so the popup must be triggered
  // by the View layer. The reset/clear/re-add decode task logic below runs
  // unconditionally; the View layer can hook into rst_decoder_by_key_handel
  // to handle the UI popup separately.
  auto stack = get_decoder_trace(index);

  if (stack) {
    remove_decode_task(stack); // remove old task
    stack->clear();
    add_decode_task(stack);
    data_updated();
  }
}

void SigSession::rst_decoder_by_key_handel(void *handel) {
  int dex = get_trace_index_by_key_handel(handel);
  rst_decoder(dex);
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
        auto spectrum_stack = new data::SpectrumStack(this, m->index());
        _spectrum_stacks.push_back(spectrum_stack);
      }
    }
  }

  if (!has_dso_signal) {
    RELEASE_ARRAY(_spectrum_stacks);
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

  DESTROY_OBJECT(_math_stack);

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
    _math_stack = new data::MathStack(this, ch1_index, ch2_index, type);
  }

  signals_changed();
}

void SigSession::math_disable() {
  if (_math_stack)
    _math_stack->init();
}

void SigSession::nodata_timeout() {
  int flag;
  _device_agent.get_config_byte(SR_CONF_TRIGGER_SOURCE, flag);
  if (flag != DSO_TRIGGER_AUTO) {
    show_wait_trigger();
  }
}

void SigSession::feed_timeout() {
  data_unlock();

  if (!_data_updated) {
    if (++_noData_cnt >= (WaitShowTime / FeedInterval))
      nodata_timeout();
  }
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

int SigSession::get_repeat_hold() {
  if (_is_working && is_repeat_mode())
    return _repeat_hold_prg;
  else
    return 0;
}

void SigSession::auto_end() {
  // TODO: view::DsoSignal::auto_end() was a UI rendering method that adjusted
  // the auto-set state and refreshed the trace. After de-view-ization, SigSession
  // does not own view::Signal instances. The View layer is responsible for
  // calling auto_end() on its own cloned DsoSignal objects when this event
  // occurs (e.g. by listening to a broadcast message or callback).
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

  for (auto p : _data_list) {
    p->clear();
  }
}

void SigSession::add_decode_task(data::DecoderStack *stack) {
  {
    std::lock_guard<std::mutex> lock(_running_tasks_mutex);
    _running_tasks.push_back(stack);
  }

  _decode_threads.push_back(
      std::thread(&SigSession::decode_single_task, this, stack));
}

void SigSession::remove_decode_task(data::DecoderStack *stack) {
  stack->stop_decode_work();
}

void SigSession::clear_all_decoder(bool bUpdateView) {
  if (decode_traces().empty())
    return;

  int dex = -1;
  clear_all_decode_task(dex);

  data::DecoderStack *runningStack = nullptr;
  if (dex != -1) {
    runningStack = decode_traces()[dex];
    runningStack->_delete_flag = true;
  }

  for (auto stack : decode_traces()) {
    if (stack != runningStack)
      delete stack;
  }
  decode_traces().clear();

  // decode_traces() returns _active_document->get_decoder_stacks() (or
  // _empty_decoder_stacks), so the clear above already removed them from
  // the document's list. No need to clear _active_document->get_decoder_stacks()
  // a second time.

  if (!_bClose && bUpdateView)
    signals_changed();
}

void SigSession::clear_all_documents_decoders() {
  int dex = -1;
  clear_all_decode_task(dex);

  data::DecoderStack *runningStack = nullptr;

  for (auto doc : _all_documents) {
    auto &stacks = doc->get_decoder_stacks();
    for (auto stack : stacks) {
      if (stack->IsRunning()) {
        runningStack = stack;
        stack->_delete_flag = true;
      }
    }
  }

  for (auto doc : _all_documents) {
    auto &stacks = doc->get_decoder_stacks();
    for (auto stack : stacks) {
      if (stack != runningStack)
        delete stack;
    }
    stacks.clear();
  }
}

void SigSession::clear_all_decode_task(int &runningDex) {
  {
    std::lock_guard<std::mutex> lock(_running_tasks_mutex);
    for (auto stack : _running_tasks) {
      if (stack)
        stack->stop_decode_work();
    }
  }

  runningDex = -1;
  for (auto doc : _all_documents) {
    int dex = 0;
    for (auto stack : doc->get_decoder_stacks()) {
      if (stack->IsRunning()) {
        stack->stop_decode_work();
        if (doc == _active_document)
          runningDex = dex;
      }
      dex++;
    }
  }

  for (auto &t : _decode_threads) {
    if (t.joinable())
      t.join();
  }
  _decode_threads.clear();

  {
    std::lock_guard<std::mutex> lock(_running_tasks_mutex);
    _running_tasks.clear();
  }
}

data::DecoderStack *SigSession::get_decoder_trace(int index) {
  if (index >= 0 && index < (int)decode_traces().size()) {
    return decode_traces()[index];
  }
  assert(false);
  return nullptr;
}

void SigSession::decode_single_task(data::DecoderStack *task) {
  pxv_info("------->decode thread start");

  if (!task->_delete_flag) {
    task->begin_decode_work();
  }

  if (task->_delete_flag) {
    pxv_info("destroy a decoder in task thread");

    DESTROY_QT_LATER(task);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (!_bClose) {
      signals_changed();
    }
  }

  {
    std::lock_guard<std::mutex> lock(_running_tasks_mutex);
    auto it = std::find(_running_tasks.begin(), _running_tasks.end(), task);
    if (it != _running_tasks.end())
      _running_tasks.erase(it);

    if (_running_tasks.empty()) {
      // Check if _view_data and its logic snapshot are valid before calling
      // decode_end
      if (_view_data != nullptr && _view_data->get_logic() != nullptr) {
        _view_data->get_logic()->decode_end();
      }
    }
  }

  pxv_info("------->decode thread end");
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
  if (_callbacks.empty()) {
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
      _is_instant = false;
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

void SigSession::add_msg_listener(IMessageListener *ln) {
  _msg_listeners.push_back(ln);
}

void SigSession::remove_callback(ISessionCallbackBase *callback) {
  auto it = std::find(_callbacks.begin(), _callbacks.end(), callback);
  if (it != _callbacks.end())
    _callbacks.erase(it);
}

void SigSession::broadcast_msg(int msg) {
  for (IMessageListener *cb : _msg_listeners) {
    cb->OnMessage(msg);
  }
}

void SigSession::set_collect_mode(DEVICE_COLLECT_MODE m) {
  assert(!_is_working);

  if (_clt_mode != m) {
    _clt_mode = m;
    _repeat_hold_prg = 0;
  }

  trigger_message(DSV_MSG_COLLECT_MODE_CHANGED);
}

void SigSession::repeat_capture_wait_timeout() {
  _repeat_timer.Stop();
  _repeat_wait_prog_timer.Stop();

  _repeat_hold_prg = 0;

  if (_is_working) {
    repeat_hold(_repeat_hold_prg);
    exec_capture();
  }
}

void SigSession::repeat_wait_prog_timeout() {
  _repeat_hold_prg -= _repeat_wait_prog_step;

  if (_repeat_hold_prg < 0)
    _repeat_hold_prg = 0;

  if (_is_working)
    repeat_hold(_repeat_hold_prg);
}

void SigSession::OnMessage(int msg) {
  switch (msg) {
  case DSV_MSG_DEVICE_OPTIONS_UPDATED:
    reload();
    break;

  case DSV_MSG_TRIG_NEXT_COLLECT: {
    if (_is_working && is_repeat_mode()) {
      if (_repeat_intvl > 0) {
        _repeat_hold_prg = 100;
        _repeat_timer.Start(_repeat_intvl * 1000);
        int intvl = _repeat_intvl * 1000 / 20;

        if (intvl >= 100) {
          _repeat_wait_prog_step = 5;
        } else if (_repeat_intvl >= 1) {
          intvl = _repeat_intvl * 1000 / 10;
          _repeat_wait_prog_step = 10;
        } else {
          intvl = _repeat_intvl * 1000 / 5;
          _repeat_wait_prog_step = 20;
        }

        _repeat_wait_prog_timer.Start(intvl);
      } else {
        _repeat_hold_prg = 0;
        exec_capture();
      }
    }
  } break;

  case DSV_MSG_REV_END_PACKET: {
    if (_device_agent.get_work_mode() == LOGIC) {
      bool bAddDecoder = false;
      bool bSwapBuffer = false;

      if (is_single_mode()) {
        if (!_is_stream_mode)
          bAddDecoder = true;
      } else if (is_repeat_mode()) {
        if (!_is_stream_mode) {
          bAddDecoder = true;
          bSwapBuffer = true;
        } else if (_capture_times > 1) {
          bAddDecoder = true;
          bSwapBuffer = true;
        }
      } else if (is_loop_mode()) {
        bAddDecoder = true;
      }

      if (is_repeat_mode()) {
        AppConfig &app = AppConfig::Instance();
        bool swapBackBufferAlways = app.appOptions.swapBackBufferAlways;
        if (!swapBackBufferAlways && !_is_working && _capture_times > 1) {
          bAddDecoder = false;
          bSwapBuffer = false;
          _capture_data->clear();
        }
      }

      if (bAddDecoder) {
        clear_all_decode_task2();
        clear_decode_result();
      }

      _trig_check_timer.Stop();

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

      if (bAddDecoder && _active_document) {
        // Move copy_data_to_document to a background thread
        // so the UI thread is not blocked by the deep copy.
        _copy_in_progress = true;
        auto doc = _active_document;

        std::thread([this, doc]() {
          copy_data_to_document(doc);
          _copy_in_progress = false;
          trigger_message(DSV_MSG_COPY_TO_DOC_DONE);
        }).detach();
      } else {
        // No active document (typical in headless mode). Skip the deep copy
        // to a SessionDocument and start the decoders directly. The decoders
        // read their snapshots from _view_data via get_signal_models(), so
        // they don't need a SessionDocument to be set up.
        _capture_owner_document = nullptr;
        for (auto stack : decode_traces()) {
          stack->set_capture_end_flag(true);
          stack->frame_ended();
          add_decode_task(stack);
        }
      }

      frame_ended();
    }
  } break;

  case DSV_MSG_COLLECT_END:
    break;

  case DSV_MSG_COPY_TO_DOC_DONE: {
    // Background copy_data_to_document has completed.
    // NOW we can safely start the decoders!
    _capture_owner_document = nullptr;
    for (auto stack : decode_traces()) {
      stack->set_capture_end_flag(true);
      stack->frame_ended();
      add_decode_task(stack);
    }
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
      clear_decode_result();
    }

    _is_stream_mode = false;
    if (mode == LOGIC) {
      if (_device_agent.is_hardware()) {
        _is_stream_mode = _device_agent.is_stream_mode();
      } else if (_device_agent.is_demo()) {
        _is_stream_mode = true;
      }
    }

    _capture_data->clear();
    _view_data->clear();
    _capture_data = _view_data;

    init_signals();

    set_cur_snap_samplerate(_device_agent.get_sample_rate());
    set_cur_samplelimits(_device_agent.get_sample_limit());

    pxv_info("Switch work mode to:%d", mode);

    broadcast_msg(DSV_MSG_DEVICE_MODE_CHANGED);

    return true;
  }
  return false;
}

bool SigSession::is_first_store_confirm() {
  if (_work_time_id != _confirm_store_time_id) {
    _confirm_store_time_id = _work_time_id;
    return true;
  }
  return false;
}

void SigSession::realtime_refresh_timeout() { _rt_refresh_time_id++; }

bool SigSession::have_new_realtime_refresh(bool keep) {
  if (_rt_ck_refresh_time_id != _rt_refresh_time_id) {
    if (!keep) {
      _rt_ck_refresh_time_id = _rt_refresh_time_id;
    }
    return true;
  }
  return false;
}

void SigSession::clear_decode_result() {
  for (auto stack : decode_traces()) {
    stack->init();
    stack->set_capture_end_flag(false);
  }
  trigger_message(DSV_MSG_CLEAR_DECODE_DATA);
}

void SigSession::clear_signals() {
  DESTROY_OBJECT(_math_stack);

  for (auto *p : _signal_models) {
    delete p;
  }
  _signal_models.clear();
}

data::SignalModel *SigSession::get_signal_by_index(int index) {
  for (auto *m : _signal_models) {
    if (m->index() == index)
      return m;
  }
  return nullptr;
}

bool SigSession::is_realtime_refresh() {
  if (is_loop_mode())
    return true;
  if (_is_stream_mode && is_single_mode())
    return true;
  if (_is_stream_mode && is_repeat_mode() && is_single_buffer())
    return true;
  return false;
}

void SigSession::on_load_config_end() {
  set_cur_snap_samplerate(_device_agent.get_sample_rate());
  set_cur_samplelimits(_device_agent.get_sample_limit());
}

void SigSession::clear_view_data() {
  _view_data->clear();
  data_updated();
}

void SigSession::set_trace_name(data::SignalModel *model, QString name) {
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

data::SignalModel *SigSession::get_channel_by_index(int orgIndex) {
  for (auto m : _signal_models) {
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

void SigSession::trig_check_timeout() {
  bool triged = false;
  int pro;

  if (_is_triged) {
    _trig_check_timer.Stop();
    return;
  }

  if (get_capture_status(triged, pro) && triged) {
    _trig_time = QDateTime::currentDateTime();
    _is_triged = true;
    _trig_check_timer.Stop();
  }
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
  if (_active_document == nullptr && doc != nullptr &&
      !_empty_decoder_stacks.empty()) {
    // Move any DecoderStack instances created before a SessionDocument was
    // bound (e.g. decoders added in headless mode without an active tab)
    // into the new document.
    auto &dst = doc->get_decoder_stacks();
    for (auto stack : _empty_decoder_stacks) {
      stack->set_owner_document(doc);
      dst.push_back(stack);
    }
    _empty_decoder_stacks.clear();
  }
  _active_document = doc;
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
  if (!data)
    return;

  // Update each SignalModel's snapshot pointer so consumers of SignalModel
  // can access the most recent snapshot data. The void* type is resolved
  // based on SignalModel::type().
  for (auto m : _signal_models) {
    switch (m->type()) {
    case api::ChannelType::Logic:
      m->set_snapshot(data->get_logic());
      break;
    case api::ChannelType::Analog:
      m->set_snapshot(data->get_analog());
      break;
    case api::ChannelType::Dso:
      m->set_snapshot(data->get_dso());
      break;
    }
  }
}

void SigSession::set_glitch_filter(
    const std::vector<uint32_t> &thresholds,
    const std::vector<GlitchFilterMode> &filter_modes) {
  if (_glitch_filter_running)
    return;

  if (_view_data->get_logic()->empty())
    return;

  bool has_filter = false;
  for (auto t : thresholds) {
    if (t > 0) {
      has_filter = true;
      break;
    }
  }
  if (!has_filter)
    return;

  _glitch_filter_running = true;
  trigger_message(DSV_MSG_GLITCH_FILTER_STARTED);

  if (_glitch_filter_thread) {
    _glitch_filter_thread->join();
    delete _glitch_filter_thread;
  }

  _glitch_filter_thread = new std::thread(&SigSession::glitch_filter_task, this,
                                          thresholds, filter_modes);
}

void SigSession::glitch_filter_task(
    const std::vector<uint32_t> thresholds,
    const std::vector<GlitchFilterMode> filter_modes) {
  if (!_view_data->_logic_backup) {
    _view_data->_logic_backup = new data::LogicSnapshot();
    _view_data->_logic_backup->copy_from(*(_view_data->get_logic()));
    if (_view_data->_logic_backup->memory_failed()) {
      delete _view_data->_logic_backup;
      _view_data->_logic_backup = nullptr;
      _glitch_filter_running = false;
      trigger_message(DSV_MSG_GLITCH_FILTER_COMPLETED);
      return;
    }
  } else {
    _view_data->get_logic()->copy_from(*_view_data->_logic_backup);
  }

  // If signal invert is active, apply invert before glitch filter
  if (_view_data->_signal_invert_active) {
    int ch_idx = 0;
    for (const GSList *l = _device_agent.get_channels(); l; l = l->next) {
      sr_channel *const probe = (sr_channel *)l->data;
      if (probe->type != SR_CHANNEL_LOGIC)
        continue;
      if (ch_idx < (int)_view_data->_signal_invert_channels.size() &&
          _view_data->_signal_invert_channels[ch_idx]) {
        _view_data->get_logic()->invert_channel(probe->index);
      }
      ch_idx++;
    }
  }

  _view_data->get_logic()->apply_glitch_filter_all(
      thresholds,
      [this](int progress) {
        (void)progress;
        trigger_message(DSV_MSG_GLITCH_FILTER_PROGRESS);
      },
      filter_modes);

  _view_data->_glitch_filter_active = true;
  _view_data->_glitch_filter_thresholds = thresholds;
  _view_data->_glitch_filter_modes = filter_modes;
  _glitch_filter_running = false;

  trigger_message(DSV_MSG_GLITCH_FILTER_COMPLETED);
  data_updated();
}

void SigSession::clear_glitch_filter() {
  if (_glitch_filter_running)
    return;

  if (!_view_data->_glitch_filter_active)
    return;

  if (_view_data->_logic_backup) {
    _view_data->get_logic()->copy_from(*_view_data->_logic_backup);
    delete _view_data->_logic_backup;
    _view_data->_logic_backup = nullptr;
  }

  _view_data->_glitch_filter_active = false;
  _view_data->_glitch_filter_thresholds.clear();
  _view_data->_glitch_filter_modes.clear();

  trigger_message(DSV_MSG_GLITCH_FILTER_CLEARED);
  data_updated();
}

bool SigSession::is_glitch_filter_active() {
  return _view_data->_glitch_filter_active;
}

void SigSession::set_signal_invert(const std::vector<bool> &channels) {
  if (_signal_invert_running)
    return;

  if (_view_data->get_logic()->empty())
    return;

  bool has_invert = false;
  for (auto ch : channels) {
    if (ch) {
      has_invert = true;
      break;
    }
  }
  if (!has_invert)
    return;

  _signal_invert_running = true;
  trigger_message(DSV_MSG_SIGNAL_INVERT_STARTED);

  if (_signal_invert_thread) {
    _signal_invert_thread->join();
    delete _signal_invert_thread;
  }

  _signal_invert_thread =
      new std::thread(&SigSession::signal_invert_task, this, channels);
}

void SigSession::signal_invert_task(const std::vector<bool> channels) {
  if (!_view_data->_logic_backup) {
    _view_data->_logic_backup = new data::LogicSnapshot();
    _view_data->_logic_backup->copy_from(*(_view_data->get_logic()));
    if (_view_data->_logic_backup->memory_failed()) {
      delete _view_data->_logic_backup;
      _view_data->_logic_backup = nullptr;
      _signal_invert_running = false;
      trigger_message(DSV_MSG_SIGNAL_INVERT_COMPLETED);
      return;
    }
  } else {
    _view_data->get_logic()->copy_from(*_view_data->_logic_backup);
  }

  // Apply invert on each enabled channel
  int ch_idx = 0;
  for (const GSList *l = _device_agent.get_channels(); l; l = l->next) {
    sr_channel *const probe = (sr_channel *)l->data;
    if (probe->type != SR_CHANNEL_LOGIC)
      continue;
    if (ch_idx < (int)channels.size() && channels[ch_idx]) {
      _view_data->get_logic()->invert_channel(probe->index);
    }
    ch_idx++;
  }

  // If glitch filter is active, re-apply on the inverted data
  if (_view_data->_glitch_filter_active) {
    _view_data->get_logic()->apply_glitch_filter_all(
        _view_data->_glitch_filter_thresholds, nullptr,
        _view_data->_glitch_filter_modes);
  }

  _view_data->_signal_invert_active = true;
  _view_data->_signal_invert_channels = channels;
  _signal_invert_running = false;

  trigger_message(DSV_MSG_SIGNAL_INVERT_COMPLETED);
  data_updated();
}

void SigSession::clear_signal_invert() {
  if (_signal_invert_running)
    return;

  if (!_view_data->_signal_invert_active)
    return;

  if (_view_data->_logic_backup) {
    _view_data->get_logic()->copy_from(*_view_data->_logic_backup);
    delete _view_data->_logic_backup;
    _view_data->_logic_backup = nullptr;
  }

  // If glitch filter is active, re-apply on the restored (non-inverted) data
  if (_view_data->_glitch_filter_active) {
    _view_data->get_logic()->apply_glitch_filter_all(
        _view_data->_glitch_filter_thresholds, nullptr,
        _view_data->_glitch_filter_modes);
  }

  _view_data->_signal_invert_active = false;
  _view_data->_signal_invert_channels.clear();

  trigger_message(DSV_MSG_SIGNAL_INVERT_CLEARED);
  data_updated();
}

bool SigSession::is_signal_invert_active() {
  return _view_data->_signal_invert_active;
}

void SigSession::restart_decoders() {
  if (decode_traces().empty())
    return;

  // Stop running decoders
  clear_all_decode_task2();
  clear_decode_result();

  // Copy current data to document for decoders
  if (_active_document) {
    copy_data_to_document(_active_document);
  }

  // Restart all decoders
  for (auto stack : decode_traces()) {
    stack->set_capture_end_flag(true);
    stack->frame_ended();
    add_decode_task(stack);
  }
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
