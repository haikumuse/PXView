#include "datafeedparser.h"

#include "capturemanager.h"
#include "decodetaskmanager.h"
#include "eventbus.h"
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

void DataFeedParser::feed_in_trigger(const ds_trigger_pos &trigger_pos) {
  _state->set_hw_replied(true);

  if (_state->device_agent().get_work_mode() != DSO) {
    _state->set_trigger_flag((trigger_pos.status & 0x01) != 0);
    if (_state->trigger_flag()) {
      _state->capture_data()->_trig_pos = trigger_pos.real_pos;

      // Update trig position for current view.
      if (_state->capture_data() == _state->view_data()) {
        _state->receive_trigger(_state->capture_data()->_trig_pos);
      }
    }
  } else {
    int probe_count = 0;
    int probe_en_count = 0;

    for (const GSList *l = _state->device_agent().get_channels(); l;
         l = l->next) {
      const sr_channel *const probe = (const sr_channel *)l->data;
      if (probe->type == SR_CHANNEL_DSO) {
        probe_count++;
        if (probe->enabled)
          probe_en_count++;
      }
    }

    _state->capture_data()->_trig_pos =
        trigger_pos.real_pos * probe_count / probe_en_count;
    _state->receive_trigger(_state->capture_data()->_trig_pos);
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

void DataFeedParser::feed_in_dso(const sr_datafeed_dso &o) {
  if (_state->capture_data()->get_dso()->memory_failed()) {
    pxv_err("Unexpected dso packet");
    return; // This dso packet was not expected.
  }

  if (_state->capture_manager()->is_instant() == false) {
    sr_status status;

    if (_state->device_agent().get_device_status(status, false)) {
      _state->set_dso_status_valid(true);
      _state->set_dso_status(status);
    }
  }

  _state->capture_manager()->inc_dso_packet_count();

  if (!_state->is_triged() && o.num_samples > 0) {
    _state->set_is_triged(true);
    _state->set_trig_time(QDateTime::currentDateTime());
    _state->set_session_time(_state->trig_time());
  }

  if (_state->capture_data()->get_dso()->last_ended()) {
    // In multi-tab architecture, SigSession::_signals do not have a viewport,
    // so we cannot and should not call get_view_rect() on them.
    // The View's own cloned signals will handle their own rendering scales.

    // first payload
    _state->capture_data()->get_dso()->first_payload(
        o, _state->device_agent().get_sample_limit(),
        _state->device_agent().get_channels(), _state->capture_manager()->is_instant(),
        _state->device_agent().is_file());
    _state->frame_began();
  } else {
    // Append to the existing data snapshot
    _state->capture_data()->get_dso()->append_payload(o);
  }

  if (o.num_samples != 0 && (!_state->capture_manager()->is_instant() ||
                             _state->capture_manager()->dso_packet_count() == 1)) {
    // update current sample rate
    _state->set_cur_snap_samplerate(_state->device_agent().get_sample_rate());
  }

  if (_state->capture_data()->get_dso()->memory_failed()) {
    _state->set_error(SessionStateContext::Malloc_err);
    _state->session_error();
    return;
  }

  // calculate related spectrum results
  for (auto m : _state->spectrum_stacks()) {
    // TODO: verify - view::SpectrumTrace::enabled() check was removed.
    // SpectrumStack has no enabled flag; calc_fft checks internal state.
    m->calc_fft();
  }

  // calculate related math results
  if (_state->math_stack()) {
    // TODO: verify - MathStack::calc_math requires vDialfactor from
    // view::MathTrace. Need to determine how to obtain this value after
    // de-view-ization.
    _state->math_stack()->realloc(_state->device_agent().get_sample_limit());
    // _state->math_stack()->calc_math(factor); // TODO: re-enable after
    // MathStack de-view-ization
  }

  _state->set_trigger_flag(o.trig_flag);
  _state->set_trigger_ch(o.trig_ch);

  // Trigger update()
  _state->set_receive_data_len(o.num_samples);

  if (!_state->capture_manager()->is_instant())
    _state->capture_manager()->data_lock();

  _state->capture_manager()->set_data_updated(true);

  // modernize-core-layer-radical Task 13: emit DataUpdated (async, worker thread).
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

  if (packet->type != SR_DF_END && packet->status != SR_PKT_OK) {
    _state->set_error(SessionStateContext::Pkt_data_err);
    _state->session_error();
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
    if (_state->error() == SessionStateContext::No_err) {
      _state->set_error(SessionStateContext::Data_overflow);
      _state->session_error();
    }
    break;
  }
  case SR_DF_END: {
    pxv_info("------------SR_DF_END packet.");

    _state->capture_data()->get_logic()->capture_ended();
    _state->capture_data()->get_dso()->capture_ended();
    _state->capture_data()->get_analog()->capture_ended();

    if (packet->status != SR_PKT_OK) {
      _state->set_error(SessionStateContext::Pkt_data_err);
      _state->session_error();
    } else {
      int mode = _state->device_agent().get_work_mode();

      // Post a message to start all decode tasks.
      if (mode == LOGIC) {
        _event_bus->broadcast_async<interface::RevEndPacket>({});
      } else {
        if (mode == DSO && _state->capture_manager()->is_instant()) {
          sr_status status;

          if (_state->device_agent().get_device_status(status, false)) {
            _state->set_dso_status_valid(true);
            _state->set_dso_status(status);
          }
        }

        _state->frame_ended();
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
