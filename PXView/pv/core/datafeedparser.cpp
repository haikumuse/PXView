#include "datafeedparser.h"

#include "capturemanager.h"
#include "decodetaskmanager.h"
#include "documentregistry.h"
#include "eventbus.h"
#include "sessionstatecontext.h"
#include "../sigsession.h"  // SessionData full definition
#include "../data/analogsnapshot.h"
#include "../data/logicsnapshot.h"
#include "../data/mathstack.h"
#include "../data/spectrumstack.h"
#include "../log.h"

#include <QDateTime>
#include <assert.h>

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
  // We can no longer read the trigger sample position from the packet.
  // Set hw_replied + trigger_flag; the trigger position remains 0 (start
  // of capture) unless the driver provides it via another mechanism.
  _state->set_hw_replied(true);

  if (_state->device_agent().get_work_mode() != DSO) {
    _state->set_trigger_flag(true);
    _state->capture_data()->_trig_pos = 0;

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
        o, _state->device_agent().get_sample_limit(),
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

  _state->set_receive_data_len(o.length * 8 /
                                _state->get_ch_num(SR_CHANNEL_LOGIC));

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
        o, _state->device_agent().get_sample_limit(),
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

  ds_lock_guard lock(_state->data_mutex());

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

      // CRITICAL FIX: 非 LOGIC 模式（DSO/ANALOG）采集正常完成时，启动解码器
      // 并在 single 模式下释放 CaptureOwnerGuard，使 _is_working=false，让 MCP
      // wait_capture_complete 能正确返回。LOGIC 模式由 RevEndPacket →
      // CopyToDocDone 路径处理（需要等 copy 线程完成）。repeat/loop 模式
      // 持续采集，由用户点击 stop_capture 释放。
      // 注意: MainWindow::on_frame_ended 不再做 copy+decode，所以必须在此处理。
      _state->decode_task_manager()->start_all_decode_tasks();

      if (_state->capture_manager()->is_single_mode()) {
        _state->capture_manager()->data_unlock();
        _event_bus->broadcast_sync<interface::EndCollectWorkPrev>({});
        _state->document_registry()->release_capture_owner();
        pxv_info("SR_DF_END non-LOGIC: CaptureOwnerGuard released (single mode).");
        _event_bus->broadcast_async<interface::EndCollectWork>({});
      }
    }

    break;
  }
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
