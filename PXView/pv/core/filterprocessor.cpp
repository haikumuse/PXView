#include "filterprocessor.h"

#include "eventbus.h"
#include "../sigsession.h"
#include "../data/logicsnapshot.h"
#include "../log.h"

#include <libsigrok.h>

namespace pv {
namespace core {

FilterProcessor::FilterProcessor(EventBus *bus, SigSession *session)
    : _event_bus(bus), _session(session),
      _glitch_filter_running(false),
      _signal_invert_running(false) {}

FilterProcessor::~FilterProcessor() { stop(); }

void FilterProcessor::stop() {
  // A3 fix: Stop glitch filter and signal invert background threads before
  // tearing down data. Set running flags false first so the task functions
  // know no new work should be accepted, then join the thread if still
  // joinable. Without this, a joinable std::thread would std::terminate on
  // destruction.
  //
  // modernize-core-layer-final Task 5: threads are now held by unique_ptr.
  // No manual delete — reset() releases the thread object after join().
  _glitch_filter_running = false;
  if (_glitch_filter_thread) {
    if (_glitch_filter_thread->joinable())
      _glitch_filter_thread->join();
    _glitch_filter_thread.reset();
  }
  _signal_invert_running = false;
  if (_signal_invert_thread) {
    if (_signal_invert_thread->joinable())
      _signal_invert_thread->join();
    _signal_invert_thread.reset();
  }
}

void FilterProcessor::set_glitch_filter(
    const std::vector<uint32_t> &thresholds,
    const std::vector<GlitchFilterMode> &filter_modes) {
  if (_glitch_filter_running)
    return;

  if (_session->_view_data->get_logic()->empty())
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
  _event_bus->trigger_message(DSV_MSG_GLITCH_FILTER_STARTED);

  if (_glitch_filter_thread) {
    _glitch_filter_thread->join();
    _glitch_filter_thread.reset();
  }

  _glitch_filter_thread = std::make_unique<std::thread>(
      &FilterProcessor::glitch_filter_task, this, thresholds, filter_modes);
}

void FilterProcessor::glitch_filter_task(
    const std::vector<uint32_t> thresholds,
    const std::vector<GlitchFilterMode> filter_modes) {
  if (!_session->_view_data->_logic_backup) {
    _session->_view_data->_logic_backup = new data::LogicSnapshot();
    _session->_view_data->_logic_backup->copy_from(
        *(_session->_view_data->get_logic()));
    if (_session->_view_data->_logic_backup->memory_failed()) {
      delete _session->_view_data->_logic_backup;
      _session->_view_data->_logic_backup = nullptr;
      _glitch_filter_running = false;
      _event_bus->trigger_message(DSV_MSG_GLITCH_FILTER_COMPLETED);
      return;
    }
  } else {
    _session->_view_data->get_logic()->copy_from(
        *_session->_view_data->_logic_backup);
  }

  // 重新滤波前清空持久化区间（apply_glitch_filter 会重新累积，避免残留）
  if (_session->_view_data->get_logic()) {
    _session->_view_data->get_logic()->clear_filtered_ranges();
  }

  // If signal invert is active, apply invert before glitch filter
  if (_session->_view_data->_signal_invert_active) {
    int ch_idx = 0;
    for (const GSList *l = _session->_device_agent.get_channels(); l;
         l = l->next) {
      sr_channel *const probe = (sr_channel *)l->data;
      if (probe->type != SR_CHANNEL_LOGIC)
        continue;
      if (ch_idx < (int)_session->_view_data->_signal_invert_channels.size() &&
          _session->_view_data->_signal_invert_channels[ch_idx]) {
        _session->_view_data->get_logic()->invert_channel(probe->index);
      }
      ch_idx++;
    }
  }

  _session->_view_data->get_logic()->apply_glitch_filter_all(
      thresholds,
      [this](int progress) {
        _event_bus->trigger_message(DSV_MSG_GLITCH_FILTER_PROGRESS, progress);
      },
      filter_modes);

  _session->_view_data->_glitch_filter_active = true;
  _session->_view_data->_glitch_filter_thresholds = thresholds;
  _session->_view_data->_glitch_filter_modes = filter_modes;
  _glitch_filter_running = false;

  _event_bus->trigger_message(DSV_MSG_GLITCH_FILTER_COMPLETED);
  _session->data_updated();
}

void FilterProcessor::clear_glitch_filter() {
  if (_glitch_filter_running)
    return;

  if (!_session->_view_data->_glitch_filter_active)
    return;

  if (_session->_view_data->_logic_backup) {
    _session->_view_data->get_logic()->copy_from(
        *_session->_view_data->_logic_backup);
    delete _session->_view_data->_logic_backup;
    _session->_view_data->_logic_backup = nullptr;
  }

  // 清除滤波后清空持久化区间，恢复原始数据无 overlay
  if (_session->_view_data->get_logic()) {
    _session->_view_data->get_logic()->clear_filtered_ranges();
  }

  _session->_view_data->_glitch_filter_active = false;
  _session->_view_data->_glitch_filter_thresholds.clear();
  _session->_view_data->_glitch_filter_modes.clear();

  _event_bus->trigger_message(DSV_MSG_GLITCH_FILTER_CLEARED);
  _session->data_updated();
}

bool FilterProcessor::is_glitch_filter_active() {
  return _session->_view_data->_glitch_filter_active;
}

void FilterProcessor::set_signal_invert(const std::vector<bool> &channels) {
  if (_signal_invert_running)
    return;

  if (_session->_view_data->get_logic()->empty())
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
  _event_bus->trigger_message(DSV_MSG_SIGNAL_INVERT_STARTED);

  if (_signal_invert_thread) {
    _signal_invert_thread->join();
    _signal_invert_thread.reset();
  }

  _signal_invert_thread =
      std::make_unique<std::thread>(
          &FilterProcessor::signal_invert_task, this, channels);
}

void FilterProcessor::signal_invert_task(const std::vector<bool> channels) {
  if (!_session->_view_data->_logic_backup) {
    _session->_view_data->_logic_backup = new data::LogicSnapshot();
    _session->_view_data->_logic_backup->copy_from(
        *(_session->_view_data->get_logic()));
    if (_session->_view_data->_logic_backup->memory_failed()) {
      delete _session->_view_data->_logic_backup;
      _session->_view_data->_logic_backup = nullptr;
      _signal_invert_running = false;
      _event_bus->trigger_message(DSV_MSG_SIGNAL_INVERT_COMPLETED);
      return;
    }
  } else {
    _session->_view_data->get_logic()->copy_from(
        *_session->_view_data->_logic_backup);
  }

  // Apply invert on each enabled channel
  int ch_idx = 0;
  for (const GSList *l = _session->_device_agent.get_channels(); l; l = l->next) {
    sr_channel *const probe = (sr_channel *)l->data;
    if (probe->type != SR_CHANNEL_LOGIC)
      continue;
    if (ch_idx < (int)channels.size() && channels[ch_idx]) {
      _session->_view_data->get_logic()->invert_channel(probe->index);
    }
    ch_idx++;
  }

  // If glitch filter is active, re-apply on the inverted data
  if (_session->_view_data->_glitch_filter_active) {
    _session->_view_data->get_logic()->apply_glitch_filter_all(
        _session->_view_data->_glitch_filter_thresholds, nullptr,
        _session->_view_data->_glitch_filter_modes);
  }

  _session->_view_data->_signal_invert_active = true;
  _session->_view_data->_signal_invert_channels = channels;
  _signal_invert_running = false;

  _event_bus->trigger_message(DSV_MSG_SIGNAL_INVERT_COMPLETED);
  _session->data_updated();
}

void FilterProcessor::clear_signal_invert() {
  if (_signal_invert_running)
    return;

  if (!_session->_view_data->_signal_invert_active)
    return;

  if (_session->_view_data->_logic_backup) {
    _session->_view_data->get_logic()->copy_from(
        *_session->_view_data->_logic_backup);
    delete _session->_view_data->_logic_backup;
    _session->_view_data->_logic_backup = nullptr;
  }

  // If glitch filter is active, re-apply on the restored (non-inverted) data
  if (_session->_view_data->_glitch_filter_active) {
    _session->_view_data->get_logic()->apply_glitch_filter_all(
        _session->_view_data->_glitch_filter_thresholds, nullptr,
        _session->_view_data->_glitch_filter_modes);
  }

  _session->_view_data->_signal_invert_active = false;
  _session->_view_data->_signal_invert_channels.clear();

  _event_bus->trigger_message(DSV_MSG_SIGNAL_INVERT_CLEARED);
  _session->data_updated();
}

bool FilterProcessor::is_signal_invert_active() {
  return _session->_view_data->_signal_invert_active;
}

} // namespace core
} // namespace pv
