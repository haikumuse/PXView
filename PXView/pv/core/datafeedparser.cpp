#include "datafeedparser.h"

#include "capturemanager.h"
#include "decodetaskmanager.h"
#include "documentregistry.h"
#include "eventbus.h"
#include "filterprocessor.h"
#include "sessionstatecontext.h"
#include "../sigsession.h"  // SessionData full definition
#include "../data/analogsnapshot.h"
#include "../data/dsosnapshot.h"
#include "../data/logicsnapshot.h"
#include "../data/mathstack.h"
#include "../data/spectrumstack.h"
#include "../log.h"

#include <QDateTime>
#include <assert.h>
#include <chrono>

namespace pv {
namespace core {

DataFeedParser::DataFeedParser(EventBus *bus, SessionStateContext *state)
    : _event_bus(bus), _state(state) {}

DataFeedParser::~DataFeedParser() {}

void DataFeedParser::feed_in_header(const sr_dev_inst *sdi) {
  (void)sdi;
  _state->receive_header();
}

void DataFeedParser::feed_in_meta(const sr_dev_inst *sdi,
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

void DataFeedParser::feed_in_trigger() {
  // Upstream SR_DF_TRIGGER has NO payload (fork ds_trigger_pos removed).
  // Query the real trigger sample position from the driver via the
  // PXView-local SR_CONF_TRIGGER_POS key. PXLogic exposes it (returns
  // devc->trigger_pos_set); other devices return 0 (start-of-capture
  // fallback, matching prior behavior). device_agent() returns a
  // reference (never null); the no-device case is handled inside
  // get_config (returns false → get_trigger_pos returns 0).
  _state->set_hw_replied(true);

  if (_state->device_agent().get_work_mode() != DSO) {
    _state->set_trigger_flag(true);

    // Read the trigger position reported by the driver.
    _state->capture_data()->_trig_pos = _state->device_agent().get_trigger_pos();

    // Update trig position for current view.
    if (_state->capture_data() == _state->view_data()) {
      _state->receive_trigger(_state->capture_data()->_trig_pos);
    }
  }
}

void DataFeedParser::feed_in_logic(const sr_datafeed_logic &o) {
  if (_state->capture_data()->get_logic()->memory_failed()) {
    pxv_err("Unexpected logic packet");
    return;
  }

  if (!_state->is_triged() && o.length > 0) {
    _state->set_is_triged(true);
    _state->set_trig_time(QDateTime::currentDateTime());
  }

  if (_state->capture_data()->get_logic()->last_ended()) {
    _state->capture_data()->get_logic()->set_loop(
        _state->capture_manager()->is_loop_mode());

    bool bNotFree = _state->decode_task_manager()->has_running_tasks() &&
                    _state->view_data() == _state->capture_data();

    _state->capture_data()->get_logic()->first_payload(
        o, _state->device_agent().get_ring_sample_count(),
        _state->device_agent().get_channels(), !bNotFree);

    // @todo Putting this here means that only listeners querying
    // for logic will be notified. Currently the only user of
    // frame_began is DecoderStack, but in future we need to signal
    // this after both analog and logic sweeps have begun.
    _state->frame_began();
  } else {
    // Append to the existing data snapshot
    _state->capture_data()->get_logic()->append_payload(o);
  }

  if (_state->capture_data()->get_logic()->memory_failed()) {
    _state->set_error(SessionStateContext::Malloc_err);
    _state->session_error();
    return;
  }

  // DSO/ANALOG 模式下可能收到 logic packet（demo 驱动始终发送 logic 数据），
  // 但 get_ch_num(SR_CHANNEL_LOGIC) 返回 0 会导致除零异常。
  const int logic_ch_num = _state->get_ch_num(SR_CHANNEL_LOGIC);
  if (logic_ch_num > 0) {
    _state->set_receive_data_len(o.length * 8 / logic_ch_num);
  } else {
    // 无 logic 通道时，按字节长度记录接收数据量
    _state->set_receive_data_len(o.length);
  }

  _state->capture_manager()->set_data_updated(true);

  // modernize-core-layer-radical Task 13: emit DataUpdated typed event.
  // feed_in_logic runs on the libsigrok data-feed thread; use broadcast_async
  // to queue on_event(DataUpdated) onto qApp's event loop, so MainWindow's
  // handler runs on the main thread (safe to touch QWidget).
  _event_bus->broadcast_async<interface::DataUpdated>({});
}

void DataFeedParser::feed_in_analog(const sr_datafeed_analog &o) {
  if (_state->capture_data()->get_analog()->memory_failed()) {
    pxv_err("Unexpected analog packet");
    return; // This analog packet was not expected.
  }

  if (_state->capture_data()->get_analog()->last_ended()) {
    // In multi-tab architecture, SigSession::_signals do not have viewports,
    // so we cannot and should not call UI rendering methods on them.

    // first payload
    _state->capture_data()->get_analog()->first_payload(
        o, _state->device_agent().get_ring_sample_count(),
        _state->device_agent().get_channels());
    _state->frame_began();
  } else {
    // Append to the existing data snapshot
    _state->capture_data()->get_analog()->append_payload(o);
  }

  if (_state->capture_data()->get_analog()->memory_failed()) {
    _state->set_error(SessionStateContext::Malloc_err);
    _state->session_error();
    return;
  }

  _state->set_receive_data_len(o.num_samples);
  _state->capture_manager()->set_data_updated(true);

  // modernize-core-layer-radical Task 13: emit DataUpdated (async, worker thread).
  _event_bus->broadcast_async<interface::DataUpdated>({});
}

void DataFeedParser::feed_in_dso(const sr_datafeed_dso &o) {
  pxv_info("[DEBUG-DSO] feed_in_dso: num_samples=%llu en_ch_num=%d sample_bits=%d trig_flag=%d",
           (unsigned long long)o.num_samples, o.en_ch_num, o.sample_bits, o.trig_flag);
  if (_state->capture_data()->get_dso()->memory_failed()) {
    pxv_err("Unexpected dso packet");
    return;
  }

  if (!_state->is_triged() && o.num_samples > 0) {
    _state->set_is_triged(true);
    _state->set_trig_time(QDateTime::currentDateTime());
  }

  if (_state->capture_data()->get_dso()->last_ended()) {
    // first payload
    _state->capture_data()->get_dso()->first_payload(
        o, _state->device_agent().get_ring_sample_count(),
        _state->device_agent().get_channels(),
        _state->capture_manager()->is_instant(),
        false /* isFile */);
    _state->frame_began();
  } else {
    // Append to the existing data snapshot
    _state->capture_data()->get_dso()->append_payload(o);
  }

  if (_state->capture_data()->get_dso()->memory_failed()) {
    _state->set_error(SessionStateContext::Malloc_err);
    _state->session_error();
    return;
  }

  _state->set_receive_data_len(o.num_samples);
  _state->capture_manager()->set_data_updated(true);

  // modernize-core-layer-radical Task 13: emit DataUpdated (async, worker thread).
  _event_bus->broadcast_async<interface::DataUpdated>({});
}

void DataFeedParser::data_feed_in(const struct sr_dev_inst *sdi,
                                  const struct sr_datafeed_packet *packet) {
  if (!sdi) {
    pxv_warn("%s", "SigSession::data_feed_in: sdi is NULL");
    return;
  }
  if (!packet) {
    pxv_warn("%s", "SigSession::data_feed_in: packet is NULL");
    return;
  }
  assert(sdi);
  assert(packet);

  static int _df_count = 0;
  _df_count++;

  auto _df_t0 = std::chrono::steady_clock::now();
  auto _lock_t0 = std::chrono::steady_clock::now();
  ds_lock_guard lock(_state->data_mutex());
  auto _lock_t1 = std::chrono::steady_clock::now();
  auto _lock_ms = std::chrono::duration_cast<std::chrono::milliseconds>(_lock_t1 - _lock_t0).count();

  if (_state->capture_manager()->is_data_lock() && packet->type != SR_DF_END)
    return;

  // Upstream sr_datafeed_packet has no `status` field (fork-only).
  // Error checking is now done via SR_DF_END handling and session stopped
  // callback.

  switch (packet->type) {
  case SR_DF_HEADER:
    feed_in_header(sdi);
    break;

  case SR_DF_META:
    assert(packet->payload);
    feed_in_meta(sdi, *(const sr_datafeed_meta *)packet->payload);
    break;

  case SR_DF_TRIGGER:
    // Upstream SR_DF_TRIGGER has NO payload.
    feed_in_trigger();
    break;

  case SR_DF_LOGIC:
    assert(packet->payload);
    feed_in_logic(*(const sr_datafeed_logic *)packet->payload);
    break;

  case SR_DF_ANALOG:
    assert(packet->payload);
    feed_in_analog(*(const sr_datafeed_analog *)packet->payload);
    break;

  case SR_DF_DSO:
    assert(packet->payload);
    feed_in_dso(*(const sr_datafeed_dso *)packet->payload);
    break;

  case SR_DF_END: {
    pxv_info("------------SR_DF_END packet.");

    _state->capture_data()->get_logic()->capture_ended();
    _state->capture_data()->get_dso()->capture_ended();
    _state->capture_data()->get_analog()->capture_ended();

    // CRITICAL FIX: fork 迁移遗漏 — 采集正常结束时设置 device_status =
    // ST_STOPPED。旧 fork libsigrok 在 ds_stop_collect 内部会通过 sr_status
    // 结构体设置 device_status；上游 libsigrok 0.6 无此机制，导致
    // _device_status 永远停在 ST_RUNNING/ST_INIT，is_stopped_status() 恒为
    // false，viewport_painter.cpp 的 doPaint 无法进入 paintSignals 分支。
    _state->set_device_status(ST_STOPPED);

    int mode = _state->device_agent().get_work_mode();

    // Post a message to start all decode tasks.
    if (mode == LOGIC) {
      _event_bus->broadcast_async<interface::RevEndPacket>({});
    } else {
      _state->frame_ended();

      // 非 LOGIC 模式（DSO/ANALOG/MSO）采集正常完成时，启动解码器。
      // CaptureOwnerGuard 的释放 + EndCollectWork 广播由 SessionStopped 事件
      // 统一处理（在 DeviceAgent worker 线程的 sr_session_run() 返回后触发），
      // 而不再在 SR_DF_END 时提前释放。SR_DF_END 触发时 libsigrok 的 main
      // loop 可能仍在运行，提前释放 guard 会让第二次 sr_session_start() 与
      // 上一次 session 的停止发生竞争。
      // repeat/loop 模式下也由 SessionStopped 处理，但 SessionStopped 只在
      // _is_working 为 true 时才释放 guard —— repeat 模式每帧的 guard 释放
      // 由 TrigNextCollect / stop_capture 路径负责。
      _state->decode_task_manager()->start_all_decode_tasks();

      // 架构修复：MSO 模式包含 LOGIC 通道，采集完成后若启用 auto-apply
      // 且有保存的 thresholds，则自动重新应用毛刺滤波。
      // LOGIC 模式走 RevEndPacket 路径已在 on_event(RevEndPacket) 中处理；
      // MSO 模式走本 else 分支，原代码遗漏了 auto-apply。
      if (mode == MSO &&
          _state->view_data()->_glitch_filter_auto_apply &&
          !_state->view_data()->_glitch_filter_thresholds.empty() &&
          _state->view_data()->get_logic() &&
          !_state->view_data()->get_logic()->empty() &&
          _state->filter_processor()) {
        _state->filter_processor()->set_glitch_filter(
            _state->view_data()->_glitch_filter_thresholds,
            _state->view_data()->_glitch_filter_modes);
      }
    }

    break;
  }
  }

  auto _df_t1 = std::chrono::steady_clock::now();
  auto _df_ms = std::chrono::duration_cast<std::chrono::milliseconds>(_df_t1 - _df_t0).count();
  if (_df_ms > 5 || _df_count <= 20) {
    pxv_warn("data_feed_in[%d]: type=%d, lock_wait=%lldms, total=%lldms",
             _df_count, (int)packet->type, (long long)_lock_ms, (long long)_df_ms);
  }
}

void DataFeedParser::data_feed_callback_ex(const struct sr_dev_inst *sdi,
                                           const struct sr_datafeed_packet *packet,
                                           void *user_data) {
  if (!user_data) {
    pxv_warn("%s", "SigSession::data_feed_callback_ex: user_data is NULL");
    return;
  }
  assert(user_data);
  static_cast<DataFeedParser *>(user_data)->data_feed_in(sdi, packet);
}

} // namespace core
} // namespace pv
