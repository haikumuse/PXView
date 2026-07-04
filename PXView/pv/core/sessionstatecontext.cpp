#include "sessionstatecontext.h"

#include "capturemanager.h"
#include "decodetaskmanager.h"
#include "documentregistry.h"
#include "eventbus.h"
#include "../sigsession.h"  // SessionData full definition
#include "../data/analogsnapshot.h"
#include "../data/decoderstack.h"
#include "../data/dsosnapshot.h"
#include "../data/logicsnapshot.h"
#include "../data/sessiondocument.h"
#include "../data/signalmodel.h"
#include "../data/spectrumstack.h"
#include "../interface/events.h"
#include "../interface/icallbacks.h"
#include "../log.h"

#include <assert.h>

namespace pv {
namespace core {

namespace {
// File-local empty stack returned by get_decoder_stacks() when no document is
// active. Mirrors the legacy SigSession::_empty_decoder_stacks static.
std::vector<std::shared_ptr<data::DecoderStack>> _empty_decoder_stacks;
} // namespace

SessionStateContext::SessionStateContext() {
  _sampling_mutex = std::make_unique<std::mutex>();
  _data_mutex = std::make_unique<std::mutex>();

  _data_list.push_back(new SessionData());
  _data_list.push_back(new SessionData());
  _view_data = _data_list[0];
  _capture_data = _data_list[0];
}

SessionStateContext::~SessionStateContext() {
  for (auto p : _data_list) {
    if (p) {
      p->clear();
      delete p;
    }
  }
  _data_list.clear();
}

// --- EventBus dispatch helpers (migrated from SigSession) -------------------

void SessionStateContext::data_updated() {
  _event_bus->dispatch_to<IDataCallback>(
      [](IDataCallback *cb) { cb->data_updated(); });
}

void SessionStateContext::set_receive_data_len(quint64 len) {
  _event_bus->dispatch_to<IDataCallback>(
      [len](IDataCallback *cb) { cb->receive_data_len(len); });
}

void SessionStateContext::receive_header() {
  _event_bus->dispatch_to<IDataCallback>(
      [](IDataCallback *cb) { cb->receive_header(); });
}

void SessionStateContext::cur_snap_samplerate_changed() {
  _event_bus->dispatch_to<IDataCallback>(
      [](IDataCallback *cb) { cb->cur_snap_samplerate_changed(); });
}

void SessionStateContext::frame_began() {
  _event_bus->dispatch_to<ICaptureCallback>(
      [](ICaptureCallback *cb) { cb->frame_began(); });
}

void SessionStateContext::frame_ended() {
  _event_bus->dispatch_to<ICaptureCallback>(
      [](ICaptureCallback *cb) { cb->frame_ended(); });
}

void SessionStateContext::update_capture() {
  _event_bus->dispatch_to<ICaptureCallback>(
      [](ICaptureCallback *cb) { cb->update_capture(); });
}

void SessionStateContext::repeat_hold(int percent) {
  _event_bus->dispatch_to<ICaptureCallback>(
      [percent](ICaptureCallback *cb) { cb->repeat_hold(percent); });
}

void SessionStateContext::receive_trigger(quint64 trigger_pos) {
  _event_bus->dispatch_to<ITriggerCallback>(
      [trigger_pos](ITriggerCallback *cb) {
        cb->receive_trigger(trigger_pos);
      });
}

void SessionStateContext::show_wait_trigger() {
  _event_bus->dispatch_to<ITriggerCallback>(
      [](ITriggerCallback *cb) { cb->show_wait_trigger(); });
}

void SessionStateContext::signals_changed() {
  _event_bus->dispatch_to<ISessionStateCallback>(
      [](ISessionStateCallback *cb) { cb->signals_changed(); });
  // 异步广播:避免在 on_event handler 中(如 on_event(DeviceOptionsUpdated)
  // → reload() → signals_changed())同步触发广播导致 _broadcast_depth>1 断言;
  // 同时保证 task thread 调用时的线程安全(Qt::QueuedConnection marshal 到主线程)。
  _event_bus->broadcast_async<interface::SignalsChanged>({});
}

void SessionStateContext::session_error() {
  _event_bus->dispatch_to<ISessionStateCallback>(
      [](ISessionStateCallback *cb) { cb->session_error(); });
}

void SessionStateContext::delay_prop_msg(QString strMsg) {
  _event_bus->dispatch_to<ISessionStateCallback>(
      [strMsg](ISessionStateCallback *cb) { cb->delay_prop_msg(strMsg); });
}

// --- Cross-manager helpers (migrated from SigSession) -----------------------

std::vector<std::shared_ptr<data::DecoderStack>> &
SessionStateContext::get_decoder_stacks(data::SessionDocument *doc) {
  data::SessionDocument *target =
      doc ? doc : _document_registry->get_active_document();
  return target ? target->get_decoder_stacks() : _empty_decoder_stacks;
}

std::vector<std::shared_ptr<data::DecoderStack>> &
SessionStateContext::decode_traces(data::SessionDocument *doc) {
  return get_decoder_stacks(doc);
}

std::shared_ptr<data::DecoderStack>
SessionStateContext::get_decoder_trace(int index, data::SessionDocument *doc) {
  auto &traces = decode_traces(doc);
  if (index >= 0 && index < (int)traces.size()) {
    return traces[index];
  }
  assert(false);
  return nullptr;
}

int SessionStateContext::get_trace_index_by_key_handel(void *handel,
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

void SessionStateContext::clear_all_decode_task2() {
  _decode_task_manager->clear_all_decode_task2();
}

void SessionStateContext::add_decode_task(
    std::shared_ptr<data::DecoderStack> stack) {
  _decode_task_manager->add_decode_task(stack);
}

void SessionStateContext::attach_data_to_signal(SessionData *data) {
  _decode_task_manager->attach_data_to_signal(data);
}

uint16_t SessionStateContext::get_ch_num(int type) {
  uint16_t num_channels = 0;
  uint16_t logic_ch_num = 0;
  uint16_t dso_ch_num = 0;
  uint16_t analog_ch_num = 0;

  if (_device_agent.have_instance()) {
    for (auto m : _signal_models) {
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

uint64_t SessionStateContext::cur_samplelimits() {
  return _capture_data->_cur_samplelimits;
}

uint64_t SessionStateContext::cur_snap_samplerate() {
  return _capture_data->_cur_snap_samplerate;
}

void SessionStateContext::set_cur_snap_samplerate(uint64_t samplerate) {
  assert(samplerate != 0);

  _capture_data->_cur_snap_samplerate = samplerate;
  _capture_data->get_logic()->set_samplerate(samplerate);
  _capture_data->get_analog()->set_samplerate(samplerate);
  _capture_data->get_dso()->set_samplerate(samplerate);

  int mode = _device_agent.get_work_mode();

  if (mode == DSO) {
    for (auto m : _signal_models) {
      if (m->type() == SR_CHANNEL_DSO) {
        _capture_data->get_dso()->set_measure_voltage_factor(
            (uint64_t)m->vfactor(), m->index());
        _capture_data->get_dso()->set_data_scale(m->vdiv(), m->index());
      }
    }
  }

  for (auto d : decode_traces()) {
    d->set_samplerate(samplerate);
  }

  if (_math_stack)
    _math_stack->set_samplerate(_device_agent.get_sample_rate());
  for (auto m : _spectrum_stacks) {
    m->set_samplerate(samplerate);
  }

  cur_snap_samplerate_changed();
}

void SessionStateContext::set_cur_samplelimits(uint64_t samplelimits) {
  assert(samplelimits != 0);
  _capture_data->_cur_samplelimits = samplelimits;
  _event_bus->dispatch_to<ICaptureCallback>(
      [](ICaptureCallback *cb) { cb->cur_samplelimits_changed(); });
}

void SessionStateContext::sync_trigger_to_libsigrok() {
  // Core→libsigrok 触发配置唯一同步点。
  uint16_t probes = 0;
  for (const auto &m : _signal_models) {
    if (m && m->type() == SR_CHANNEL_LOGIC)
      probes++;
  }

  const auto &cfg = _trigger_config;
  const int trig_pos = cfg.trigger_pos();

  if (cfg.mode() == data::TriggerConfig::Simple) {
    ds_trigger_reset();

    bool any_triggered = false;
    for (const auto &m : _signal_models) {
      if (!m || m->type() != SR_CHANNEL_LOGIC)
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

void SessionStateContext::clear_glitch_filter_state_for_capture() {
  // 新采集开始时调用:清除滤波激活状态和 backup,
  // 但保留 thresholds/modes(供 auto-apply 使用)。
  // 不恢复数据 — _view_data->get_logic() 已被 clear(),无数据可恢复。
  if (_view_data->_logic_backup) {
    delete _view_data->_logic_backup;
    _view_data->_logic_backup = nullptr;
  }
  if (_view_data->_glitch_filter_active) {
    _view_data->_glitch_filter_active = false;
    _event_bus->broadcast_async<interface::GlitchFilterCleared>({});
  }
}

} // namespace core
} // namespace pv
