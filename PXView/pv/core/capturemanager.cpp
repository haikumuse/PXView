#include "capturemanager.h"

#include "eventbus.h"
#include "documentregistry.h"
#include "../sigsession.h"
#include "../data/decoderstack.h"
#include "../data/sessiondocument.h"
#include "../data/signalmodel.h"
#include "../data/spectrumstack.h"
#include "../data/mathstack.h"
#include "../data/lissajousmodel.h"
#include "../log.h"
#include "../ui/langresource.h"
#include "../ui/msgbox.h"
#include "../utility/path.h"
#include "../config/appconfig.h"

#include <QDateTime>
#include <QDir>
#include <QString>
#include <algorithm>
#include <assert.h>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <thread>

#include <libsigrok.h>

namespace pv {
namespace core {

// File-local helper duplicated from sigsession.cpp (used by action_start_capture
// to compute the default disk-cache path when the user hasn't set one).
// Kept file-local to avoid leaking the symbol into the public API.
static QString get_default_disk_cache_path() {
  return QDir::tempPath() + "/PXView_cache";
}

CaptureManager::CaptureManager(EventBus *bus, SigSession *session)
    : _event_bus(bus), _session(session), _noData_cnt(0), _data_lock(false),
      _data_updated(false), _data_auto_lock(0), _repeat_intvl(1),
      _repeat_hold_prg(0), _repeat_wait_prog_step(10), _is_instant(false),
      _work_time_id(0), _capture_times(0), _confirm_store_time_id(0),
      _rt_refresh_time_id(0), _rt_ck_refresh_time_id(0),
      _clt_mode(COLLECT_SINGLE), _is_stream_mode(false), _is_action(false),
      _dso_packet_count(0) {
  _feed_timer.SetCallback(std::bind(&CaptureManager::feed_timeout, this));
  _repeat_timer.SetCallback(
      std::bind(&CaptureManager::repeat_capture_wait_timeout, this));
  _repeat_wait_prog_timer.SetCallback(
      std::bind(&CaptureManager::repeat_wait_prog_timeout, this));
  _refresh_rt_timer.SetCallback(
      std::bind(&CaptureManager::realtime_refresh_timeout, this));
  _trig_check_timer.SetCallback(
      std::bind(&CaptureManager::trig_check_timeout, this));
}

CaptureManager::~CaptureManager() = default;

void CaptureManager::capture_init() {
  // update instant setting
  _session->_device_agent.set_config_bool(SR_CONF_INSTANT, _is_instant);
  _session->update_capture();

  _session->set_cur_snap_samplerate(_session->_device_agent.get_sample_rate());
  _session->set_cur_samplelimits(_session->_device_agent.get_sample_limit());

  _data_updated = false;
  _session->_trigger_flag = false;
  _session->_trigger_ch = 0;
  _session->_hw_replied = false;
  _rt_refresh_time_id = 0;
  _rt_ck_refresh_time_id = 0;
  _noData_cnt = 0;

  data_unlock();

  // Init data container
  _session->_capture_data->clear();
  _session->_capture_data->get_logic()->set_disk_cache_config(_disk_cache_config);

  int mode = _session->_device_agent.get_work_mode();
  if (mode == DSO) {
    for (auto m : _session->_spectrum_stacks) {
      m->init();
    }

    if (_session->_math_stack) {
      _session->_math_stack->init();
    }
  }

  // In multi-tab architecture, SigSession::_signals do not have viewports.
  // We cannot call UI-dependent methods (like set_zero_ratio) on them here.
  // Hardware offset is already updated via View's own signal events when user
  // changes it.

  // Start timer
  if (mode == DSO || mode == ANALOG)
    _feed_timer.Start(SigSession::FeedInterval);
  else
    _feed_timer.Stop();
}

bool CaptureManager::start_capture(bool instant, data::SessionDocument *owner) {
  _is_action = true;
  int ret = action_start_capture(instant, owner);
  _is_action = false;
  return ret;
}

bool CaptureManager::action_start_capture(bool instant,
                                          data::SessionDocument *owner) {
  assert(_event_bus && _event_bus->has_callbacks());

  pxv_info("Start collect.");

  if (_session->_is_working) {
    pxv_err("Error! Is working now.");
    return false;
  }

  if (_session->_signal_models.empty()) {
    pxv_info("ERROR: channel list is empty, unable to capture data.");
    return false;
  }

  // Check that a device instance has been selected.
  if (_session->_device_agent.have_instance() == false) {
    pxv_err("Error!No device selected");
    assert(false);
  }
  if (_session->_device_status == ST_RUNNING ||
      _session->_device_agent.is_collecting()) {
    pxv_err("Error!Device is running.");
    return false;
  }

  _session->clear_all_decode_task2();
  clear_decode_result();

  _session->_capture_data->clear();
  _session->_view_data->clear();
  // 清除毛刺滤波状态(backup 悬垂、active 标志过期),保留 thresholds/modes
  // 供 auto-apply 使用
  _session->clear_glitch_filter_state_for_capture();
  _is_stream_mode = false;
  _capture_times = 0;
  _dso_packet_count = 0;
  _session->_dso_status_valid = false;

  _session->_capture_data = _session->_view_data;
  _session->set_cur_snap_samplerate(_session->_device_agent.get_sample_rate());
  _session->set_cur_samplelimits(_session->_device_agent.get_sample_limit());

  _session->set_session_time(QDateTime::currentDateTime());

  int mode = _session->_device_agent.get_work_mode();
  if (mode == LOGIC) {
    if (is_repeat_mode() && _session->_device_agent.is_hardware() &&
        _session->_device_agent.is_stream_mode()) {
      set_repeat_intvl(0.1);
    }

    if (_session->_device_agent.is_hardware()) {
      _is_stream_mode = _session->_device_agent.is_stream_mode();
    } else if (_session->_device_agent.is_demo() ||
               _session->_device_agent.is_file()) {
      _is_stream_mode = true;
    }

    if (is_loop_mode() && !_is_stream_mode) {
      set_collect_mode(COLLECT_SINGLE); // Reset the capture mode.
    }

    if (is_loop_mode() && _session->_device_agent.is_demo()) {
      QString opt_mode = _session->_device_agent.get_demo_operation_mode();
      if (opt_mode != "random") {
        set_collect_mode(COLLECT_SINGLE);
      }
    }

    if (_session->_device_agent.is_hardware() ||
        _session->_device_agent.is_demo()) {
      bool bv = is_loop_mode() && _is_stream_mode;
      _session->_device_agent.set_config_bool(SR_CONF_LOOP_MODE, bv);
    }
  }

  if (mode == DSO && _session->_device_agent.is_hardware()) {
    uint32_t ref_max = 0;
    uint32_t ref_min = 0;
    _session->_device_agent.get_config_uint32(SR_CONF_REF_MIN, ref_min);
    _session->_device_agent.get_config_uint32(SR_CONF_REF_MAX, ref_max);
    _session->_view_data->get_dso()->set_ref_range(ref_max, ref_min);
  }

  _session->trigger_message(DSV_MSG_CAPTURE_STATE_CHANGED);

  bool disk_cache_enabled = false;
  _session->_device_agent.get_config_bool(SR_CONF_DISK_CACHE_ENABLE,
                                          disk_cache_enabled);
  if (disk_cache_enabled) {
    QString cache_path;
    _session->_device_agent.get_config_string(SR_CONF_DISK_CACHE_PATH,
                                               cache_path);
    if (cache_path.isEmpty()) {
      cache_path = get_default_disk_cache_path();
      _session->_device_agent.set_config_string(SR_CONF_DISK_CACHE_PATH,
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
    _session->_device_agent.get_config_string(SR_CONF_DISK_CACHE_PATH,
                                               cache_path);
    if (cache_path.isEmpty()) {
      cache_path = get_default_disk_cache_path();
    }
    _disk_cache_config.cache_path = cache_path.toStdString();

    double disk_gb = 16;
    _session->_device_agent.get_config_double(SR_CONF_STREAM_BUFF, disk_gb);
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
  if (_session->_device_agent.is_file())
    _is_instant = true;
  else
    _is_instant = instant;

  _session->trigger_message(DSV_MSG_START_COLLECT_WORK_PREV);

  if (exec_capture()) {
    _work_time_id++;
    // CaptureOwnerGuard manages _is_working + _capture_owner_document +
    // CaptureOwnerChanged broadcast as a single RAII unit. Replaces the
    // manual _is_working=true / _capture_owner_document=... / broadcast_msg.
    _session->_document_registry->acquire_capture_owner(
        owner ? owner : _session->_document_registry->get_active_document());
    _session->trigger_message(DSV_MSG_START_COLLECT_WORK);

    // Start a timer, for able to refresh the view per (1000 / 30)ms.
    if (is_realtime_refresh()) {
      _refresh_rt_timer.Start(1000 / 30);
    }

    return true;
  }

  return false;
}

bool CaptureManager::exec_capture() {
  if (_session->_device_agent.is_collecting()) {
    pxv_err("Error!Device is running.");
    return false;
  }

  // Wait for background copy_data_to_document to complete before
  // starting a new capture, to prevent source data from being cleared.
  if (_session->_document_registry->is_copy_in_progress()) {
    pxv_info("Waiting for background copy_data_to_document to complete...");
    while (_session->_document_registry->is_copy_in_progress()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  if (_session->_device_agent.have_enabled_channel() == false) {
    QString err_str(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_NO_ENABLED_CHANNEL),
                        "No channels enabled!"));
    MsgBox::Show(err_str);
    return false;
  }

  _capture_times++;
  _session->_is_triged = false;

  int mode = _session->_device_agent.get_work_mode();
  bool bAddDecoder = false;
  bool bSwapBuffer = false;

  if (mode == DSO || mode == ANALOG) {
    // reset measure of dso signal
    for (auto m : _session->_signal_models) {
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

  if (mode == LOGIC && _session->_device_agent.is_hardware() &&
      _session->_device_agent.get_hardware_operation_mode() == LO_OP_BUFFER) {
    _trig_check_timer.Start(200);
  }

  if (bAddDecoder) {
    _session->clear_all_decode_task2();
    clear_decode_result();

    // CRITICAL: Release the active document's copy of the old mmap data.
    // copy_data_to_document() shares the mmap via shared_ptr. If we don't
    // clear the document's LogicSnapshot here, its shared_ptr reference
    // keeps the old multi-GB mmap alive while a new one is created,
    // causing memory to double on every capture.
    if (_session->_document_registry->get_active_document()) {
      _session->_document_registry->get_active_document()
          ->get_active_logic()
          ->clear();
    }
  }

  // Set the buffer to store the captured data
  if (bSwapBuffer) {
    int buf_index = -1;
    for (int i = 0; i < (int)_session->_data_list.size(); i++) {
      if (_session->_data_list[i] != _session->_view_data) {
        buf_index = i;
        break;
      }
    }

    if (buf_index < 0) {
      _session->_data_list.push_back(new SessionData());
      buf_index = (int)_session->_data_list.size() - 1;
    }

    _session->_capture_data = _session->_data_list[buf_index];
    _session->_capture_data->clear();
    _session->set_cur_snap_samplerate(_session->_device_agent.get_sample_rate());
    _session->set_cur_samplelimits(_session->_device_agent.get_sample_limit());
  }

  capture_init();

  // IMPORTANT: Ensure the session's logic signals point to the current capture
  // buffer. This is required because DecoderStack searches the session's signal
  // list to find the data source. Without this, decoders in stream mode would
  // bind to the old, cleared document snapshot and fail to show results.
  _session->attach_data_to_signal(_session->_capture_data);

  // Core→libsigrok 触发配置唯一同步点。在 ds_start_collect 前一次性同步，
  // 消除 TriggerDock/SessionService 各自调 ds_trigger_* 导致的互相覆盖。
  _session->sync_trigger_to_libsigrok();

  if (_session->_device_agent.start() == false) {
    pxv_err("Start collect error!");
    return false;
  }

  if (mode == LOGIC) {
    for (auto de : _session->decode_traces()) {
      if (bAddDecoder) {
        de->set_capture_end_flag(false);
        de->frame_ended();
        _session->add_decode_task(de);
      }
    }
  }

  return true;
}

bool CaptureManager::stop_capture() {
  _is_action = true;
  int ret = action_stop_capture();
  _is_action = false;
  return ret;
}

bool CaptureManager::action_stop_capture() {
  if (!_session->_is_working)
    return false;

  pxv_info("Stop collect.");

  if (_session->_bClose) {
    _session->_is_working = false;
    _repeat_timer.Stop();
    _repeat_wait_prog_timer.Stop();
    _refresh_rt_timer.Stop();
    exit_capture();
    // Task 4: RAII cleanup — join copy thread + clear owner + broadcast.
    _session->_document_registry->release_capture_owner();
    return true;
  }

  bool wait_upload = false;
  if (is_single_mode() &&
      _session->_device_agent.get_work_mode() == LOGIC) {
    _session->_device_agent.get_config_bool(SR_CONF_WAIT_UPLOAD, wait_upload);
  }

  if (!wait_upload) {
    _session->_is_working = false;
    _repeat_timer.Stop();
    _repeat_wait_prog_timer.Stop();
    _refresh_rt_timer.Stop();

    if (_repeat_hold_prg != 0 && is_repeat_mode()) {
      _repeat_hold_prg = 0;
      _session->repeat_hold(_repeat_hold_prg);
    }

    _session->trigger_message(DSV_MSG_END_COLLECT_WORK_PREV);

    exit_capture();

    data_unlock();

    if (is_repeat_mode() && _session->_device_status != ST_RUNNING) {
      _session->trigger_message(DSV_MSG_END_COLLECT_WORK);
    }

    // Task 4: RAII cleanup — join copy thread + clear owner + _is_working=false
    // (redundant here, set above) + CaptureOwnerChanged broadcast. Replaces the
    // old manual `_capture_owner_document = nullptr` (gated on !_copy_in_progress)
    // — the guard always joins the copy thread first, which is safer.
    _session->_document_registry->release_capture_owner();
    return true;
  } else {
    pxv_info("Data is uploading from device data buffer, waiting for stop.");
  }
  return false;
}

void CaptureManager::exit_capture() {
  _is_instant = false;

  _feed_timer.Stop();

  if (_session->_device_agent.is_collecting())
    _session->_device_agent.stop();
}

bool CaptureManager::get_capture_status(bool &triggered, int &progress) {
  uint64_t sample_limits = _session->cur_samplelimits();
  sr_status status;

  if (_session->_device_agent.get_device_status(status, true)) {
    triggered = status.trig_hit & 0x01;
    uint64_t captured_cnt = status.trig_hit >> 2;

    captured_cnt =
        ((uint64_t)status.captured_cnt0 +
         ((uint64_t)status.captured_cnt1 << 8) +
         ((uint64_t)status.captured_cnt2 << 16) +
         ((uint64_t)status.captured_cnt3 << 24) + (captured_cnt << 32));

    int mode = _session->_device_agent.get_work_mode();

    if (mode == DSO)
      captured_cnt =
          captured_cnt * _session->_signal_models.size() /
          _session->get_ch_num(SR_CHANNEL_DSO);

    if (triggered)
      progress = (sample_limits - captured_cnt) * 100.0 / sample_limits;
    else
      progress = captured_cnt * 100.0 / sample_limits;

    if (progress == 100 && mode == LOGIC &&
        _session->_capture_data->get_logic()->have_data() == false) {
      progress = 0;
    }

    return true;
  }
  return false;
}

void CaptureManager::check_update() {
  ds_lock_guard lock(_session->_data_mutex);

  if (_session->_device_agent.is_collecting() == false)
    return;

  if (_data_updated) {
    if (_session->_device_agent.get_work_mode() != LOGIC)
      _session->data_updated();

    _data_updated = false;
    _noData_cnt = 0;
    data_auto_unlock();
  } else {
    if (++_noData_cnt >= (SigSession::WaitShowTime / SigSession::FeedInterval))
      nodata_timeout();
  }
}

void CaptureManager::nodata_timeout() {
  int flag;
  _session->_device_agent.get_config_byte(SR_CONF_TRIGGER_SOURCE, flag);
  if (flag != DSO_TRIGGER_AUTO) {
    _session->show_wait_trigger();
  }
}

void CaptureManager::feed_timeout() {
  data_unlock();

  if (!_data_updated) {
    if (++_noData_cnt >= (SigSession::WaitShowTime / SigSession::FeedInterval))
      nodata_timeout();
  }
}

int CaptureManager::get_repeat_hold() {
  if (_session->_is_working && is_repeat_mode())
    return _repeat_hold_prg;
  else
    return 0;
}

void CaptureManager::auto_end() {
  // TODO: view::DsoSignal::auto_end() was a UI rendering method that adjusted
  // the auto-set state and refreshed the trace. After de-view-ization,
  // SigSession does not own view::Signal instances. The View layer is
  // responsible for calling auto_end() on its own cloned DsoSignal objects when
  // this event occurs (e.g. by listening to a broadcast message or callback).
}

void CaptureManager::set_collect_mode(DEVICE_COLLECT_MODE m) {
  assert(!_session->_is_working);

  if (_clt_mode != m) {
    _clt_mode = m;
    _repeat_hold_prg = 0;
  }

  _session->trigger_message(DSV_MSG_COLLECT_MODE_CHANGED);
}

void CaptureManager::repeat_capture_wait_timeout() {
  _repeat_timer.Stop();
  _repeat_wait_prog_timer.Stop();

  _repeat_hold_prg = 0;

  if (_session->_is_working) {
    _session->repeat_hold(_repeat_hold_prg);
    exec_capture();
  }
}

void CaptureManager::repeat_wait_prog_timeout() {
  _repeat_hold_prg -= _repeat_wait_prog_step;

  if (_repeat_hold_prg < 0)
    _repeat_hold_prg = 0;

  if (_session->_is_working)
    _session->repeat_hold(_repeat_hold_prg);
}

void CaptureManager::realtime_refresh_timeout() { _rt_refresh_time_id++; }

bool CaptureManager::have_new_realtime_refresh(bool keep) {
  if (_rt_ck_refresh_time_id != _rt_refresh_time_id) {
    if (!keep) {
      _rt_ck_refresh_time_id = _rt_refresh_time_id;
    }
    return true;
  }
  return false;
}

void CaptureManager::clear_decode_result() {
  for (auto stack : _session->decode_traces()) {
    stack->init();
    stack->set_capture_end_flag(false);
  }
  _session->trigger_message(DSV_MSG_CLEAR_DECODE_DATA);
}

bool CaptureManager::is_first_store_confirm() {
  if (_work_time_id != _confirm_store_time_id) {
    _confirm_store_time_id = _work_time_id;
    return true;
  }
  return false;
}

void CaptureManager::trig_check_timeout() {
  bool triged = false;
  int pro;

  if (_session->_is_triged) {
    _trig_check_timer.Stop();
    return;
  }

  if (get_capture_status(triged, pro) && triged) {
    _session->_trig_time = QDateTime::currentDateTime();
    _session->_is_triged = true;
    _trig_check_timer.Stop();
  }
}

void CaptureManager::refresh(int holdtime) {
  ds_lock_guard lock(_session->_data_mutex);

  data_lock();
  _session->_view_data->get_logic()->init();

  _session->clear_all_decode_task2();
  clear_decode_result();

  _session->_view_data->get_dso()->init();

  for (auto m : _session->_spectrum_stacks) {
    m->init();
  }

  if (_session->_math_stack)
    _session->_math_stack->init();

  _session->_view_data->get_analog()->init();

  _out_timer.TimeOut(holdtime,
                     std::bind(&CaptureManager::feed_timeout, this));
  _data_updated = true;
}

void CaptureManager::data_auto_lock(int lock) { _data_auto_lock = lock; }

void CaptureManager::data_auto_unlock() {
  if (_data_auto_lock > 0)
    _data_auto_lock--;
  else if (_data_auto_lock < 0)
    _data_auto_lock = 0;
}

bool CaptureManager::get_data_auto_lock() { return _data_auto_lock != 0; }

bool CaptureManager::is_realtime_refresh() {
  if (is_loop_mode())
    return true;
  if (_is_stream_mode && is_single_mode())
    return true;
  if (_is_stream_mode && is_repeat_mode() && _session->is_single_buffer())
    return true;
  return false;
}

bool CaptureManager::is_repeating() {
  return _clt_mode == COLLECT_REPEAT && !_is_instant;
}

bool CaptureManager::is_single_mode() { return _clt_mode == COLLECT_SINGLE; }

bool CaptureManager::is_repeat_mode() { return _clt_mode == COLLECT_REPEAT; }

bool CaptureManager::is_loop_mode() { return _clt_mode == COLLECT_LOOP; }

int CaptureManager::get_collect_mode() { return (int)_clt_mode; }

} // namespace core
} // namespace pv
