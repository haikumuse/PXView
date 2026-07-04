/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
 *
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

#include "signalrebuilder.h"

#include <QDebug>
#include <memory>
#include <vector>

#include "analogsignal.h"
#include "dsosignal.h"
#include "logicsignal.h"
#include "signal.h"
#include "signalfactory.h"
#include "view.h"

#include "../api/types.h"
#include "../data/datasource.h"
#include "../data/sessiondocument.h"
#include "../data/signalconfigstore.h"
#include "../data/signalmodel.h"
#include "../deviceagent.h"
#include "../dsvdef.h"
#include "../log.h"
#include "../sigsession.h"
#include "dock_ui_state.h"

namespace pv {
namespace view {

SignalRebuilder::SignalRebuilder(View *view) : _view(view) {}

SignalRebuilder::~SignalRebuilder() {}

void SignalRebuilder::rebuild_signals_from_config(
    const pv::data::SignalConfig &config) {
  // Re-entrancy guard: if a nested broadcast (e.g. DeviceOptionsUpdated
  // from within this function) triggers on_event → rebuild_signals() →
  // rebuild_signals_from_config() again, abort immediately to prevent
  // infinite recursion / stack overflow.
  if (_view->_rebuild_in_progress)
    return;
  _view->_rebuild_in_progress = true;
  struct RebuildGuard {
    bool &flag;
    RebuildGuard(bool &f) : flag(f) {}
    ~RebuildGuard() { flag = false; }
  } _rebuild_guard(_view->_rebuild_in_progress);

  qDebug() << "View::rebuild_signals_from_config() work_mode="
           << config.work_mode << "ch_count=" << config.channels.size()
           << "is_valid=" << config.is_valid;

  std::vector<Signal *> old_signals = _view->_own_signals;
  _view->_own_signals.clear();

  int channel_type;
  switch (config.work_mode) {
  case LOGIC:
    channel_type = SR_CHANNEL_LOGIC;
    break;
  case DSO:
    channel_type = SR_CHANNEL_DSO;
    break;
  case ANALOG:
    channel_type = SR_CHANNEL_ANALOG;
    break;
  default:
    for (auto sig : old_signals)
      delete sig;
    _view->signals_changed(NULL);
    return;
  }

  int view_index = 0;
  for (const auto &ch : config.channels) {
    // Create a temporary SignalModel for the channel configuration.
    // This SignalModel is not connected to a real device (no sr_channel),
    // but it allows the View layer to create Signal objects using the new
    // constructor signature.
    auto model = std::make_shared<pv::data::SignalModel>();
    model->set_index(ch.index);
    model->set_enabled(ch.enabled);
    model->set_name(std::to_string(ch.index));

    // Set channel type based on work mode
    switch (config.work_mode) {
    case LOGIC:
      model->set_type(SR_CHANNEL_LOGIC);
      break;
    case DSO:
      model->set_type(SR_CHANNEL_DSO);
      model->set_vdiv(ch.vdiv);
      model->set_coupling(ch.coupling);
      model->set_hw_offset(ch.hw_offset);
      model->set_vertical_offset(ch.offset);
      model->set_zero_offset(ch.zero_offset);
      break;
    case ANALOG:
      model->set_type(SR_CHANNEL_ANALOG);
      model->set_vdiv(ch.vdiv);
      model->set_coupling(ch.coupling);
      model->set_hw_offset(ch.hw_offset);
      model->set_vertical_offset(ch.offset);
      model->set_zero_offset(ch.zero_offset);
      break;
    }

    // Set session for the model (so it can call session methods if needed)
    model->set_session(_view->_session);

    Signal *old_signal = nullptr;
    for (auto os : old_signals) {
      if (os->get_index() == ch.index && os->signal_type() == channel_type) {
        old_signal = os;
        break;
      }
    }

    Signal *signal = nullptr;
    switch (config.work_mode) {
    case LOGIC:
      if (old_signal) {
        signal = new LogicSignal(static_cast<LogicSignal *>(old_signal),
                                 nullptr, model, _view->_data_source);
      } else {
        signal = new LogicSignal(nullptr, model, _view->_data_source);
      }
      break;
    case DSO:
      if (old_signal) {
        signal = new DsoSignal(static_cast<DsoSignal *>(old_signal), nullptr,
                               model, _view->_data_source);
      } else {
        signal = new DsoSignal(nullptr, model, _view->_data_source);
      }
      break;
    case ANALOG:
      if (old_signal) {
        signal = new AnalogSignal(static_cast<AnalogSignal *>(old_signal),
                                  nullptr, model, _view->_data_source);
      } else {
        signal = new AnalogSignal(nullptr, model, _view->_data_source);
      }
      break;
    }

    if (signal) {
      signal->set_enabled(ch.enabled);
      // Task 7 (purify-architecture-concepts): visible is no longer a
      // Core-serialized field. Default to visible; DockUiState restoration
      // below overrides when a persisted layout exists for the channel.
      signal->set_visible(true);

      // purify-architecture-concepts Task 15/17: UI 布局状态
      // (view_index/v_offset/own_height/visible) 从 View 层 DockUiState
      // 恢复，不再从 ChannelConfig 读取（Task 15 已删除该三字段）。
      // DockUiState 是 per-View 的内存状态，跨 capture reload 保留；
      // .pxc 持久化由独立的 "uiLayout" 段处理（Task 16）。
      auto layout_it = _view->_dock_ui_state.channel_layouts.find(ch.index);
      if (layout_it != _view->_dock_ui_state.channel_layouts.end()) {
        const auto &layout = layout_it->second;
        signal->set_v_offset(layout.v_offset);
        if (layout.own_height >= 0) {
          signal->set_own_height(layout.own_height);
        } else if (config.work_mode == DSO || config.work_mode == ANALOG) {
          signal->set_own_height(-1);
        }
        if (layout.view_index >= 0) {
          signal->set_view_index(layout.view_index);
        } else if (ch.enabled) {
          signal->set_view_index(view_index++);
        } else {
          signal->set_view_index(-1);
        }
        signal->set_visible(layout.visible);
      } else {
        // No persisted layout for this channel — derive defaults.
        if (ch.enabled) {
          signal->set_view_index(view_index++);
        } else {
          signal->set_view_index(-1);
        }
        if (config.work_mode == DSO || config.work_mode == ANALOG) {
          signal->set_own_height(-1);
        }
      }
      _view->_own_signals.push_back(signal);
    }
  }

  for (auto sig : old_signals)
    delete sig;

  if (_view->_document && _view->_document->has_data()) {
    for (auto sig : _view->_own_signals) {
      int type = sig->signal_type();
      switch (type) {
      case SR_CHANNEL_LOGIC: {
        view::LogicSignal *s = static_cast<view::LogicSignal *>(sig);
        s->set_data(_view->_document->get_active_logic());
        break;
      }
      case SR_CHANNEL_ANALOG: {
        view::AnalogSignal *s = static_cast<view::AnalogSignal *>(sig);
        s->set_data(_view->_document->get_active_analog());
        break;
      }
      case SR_CHANNEL_DSO: {
        view::DsoSignal *s = static_cast<view::DsoSignal *>(sig);
        s->set_data(_view->_document->get_active_dso());
        break;
      }
      }
    }
  }

  _view->signals_changed(NULL);
}

void SignalRebuilder::rebuild_signals() {
  _view->mark_derived_traces_dirty();

  if (_view->_data_source == _view->_document && _view->_document &&
      _view->_document->has_signal_config()) {
    const auto &config = _view->_document->get_signal_config();
    // 检查配置的通道数是否与设备当前的通道数匹配
    // 如果不匹配，说明通道模式已切换，需要从设备重新创建信号
    int device_ch_count = 0;
    for (const GSList *l = _view->_device_agent->get_channels(); l;
         l = l->next) {
      device_ch_count++;
    }
    if (config.channels.size() == (size_t)device_ch_count) {
      _view->rebuild_signals_from_config(config);
      SignalFactory::update_signals(_view->_own_signals, _view->_data_source,
                                    _view->_data_source, SignalFactory::Modified,
                                    &_view->_dock_ui_state);
      // Only property changes, no layout needed - use incremental refresh
      _view->signals_modified_refresh();
      return;
    }
  }

  if (!_view->_data_source)
    return;

  auto created_sigs =
      SignalFactory::create_signals(_view->_data_source, _view->_data_source);
  if (created_sigs.empty())
    return;

  for (auto sig : _view->_own_signals)
    delete sig;
  _view->_own_signals.clear();

  for (auto sig : created_sigs) {
    // create_signals 新建的信号已使用 Trace 构造函数的默认高度，
    // 无需在此二次重置。DSO/Analog 的自动高度由 set_data_document 路径处理。
    _view->_own_signals.push_back(sig);
  }

  for (auto sig : _view->_own_signals) {
    auto s = dynamic_cast<Signal *>(sig);
    if (s && s->model()) {
      // purify-architecture-concepts Task 8: enabled (hardware) and visible
      // (UI) are independent. Default to visible; DockUiState restoration
      // below overrides when a persisted layout exists for the channel.
      s->set_enabled(s->model()->enabled());
      s->set_visible(true);
    }
  }

  // R9: restore persisted UI layout (view_index/v_offset/own_height/visible)
  // from the View-layer DockUiState. (purify-architecture-concepts Task 17:
  // ChannelConfig no longer holds these fields; the old SessionDocument-based
  // restoration is replaced by DockUiState.channel_layouts.) DockUiState is
  // per-View (per-tab) and survives capture-triggered reload() in memory;
  // .pxc persistence is the dedicated "uiLayout" segment (Task 16).
  for (auto *sig : _view->_own_signals) {
    auto it = _view->_dock_ui_state.channel_layouts.find(sig->get_index());
    if (it == _view->_dock_ui_state.channel_layouts.end())
      continue;
    const auto &layout = it->second;
    sig->set_v_offset(layout.v_offset);
    if (layout.own_height >= 0) {
      sig->set_own_height(layout.own_height);
    } else if (_view->get_work_mode() == DSO ||
               _view->get_work_mode() == ANALOG) {
      sig->set_own_height(-1);
    }
    if (layout.view_index >= 0) {
      sig->set_view_index(layout.view_index);
    }
    sig->set_visible(layout.visible);
    pxv_info("View::rebuild_signals: restored channel %d from DockUiState: view_index=%d, v_offset=%d, own_height=%d, visible=%d",
             sig->get_index(), sig->get_view_index(),
             sig->get_v_offset(), sig->get_own_height(),
             (int)sig->visible());
  }

  if (_view->_document && _view->_document->has_data()) {
    _view->set_data_document(_view->_document);
  }

  _view->signals_changed(NULL);
}

void SignalRebuilder::signals_modified_refresh() {
  // Only property changes, no layout changes needed.
  // Just repaint the signals without calling signals_changed(NULL).
  // A1 fix: mark derived traces (Math/Spectrum/Lissajous/Decode) dirty so
  // sync_derived_traces() recreates them on the next paint cycle. Without this,
  // enabling Math/Spectrum/Lissajous via their option dialogs does not show the
  // trace until the user switches tabs (which triggers sync_derived_traces).
  _view->mark_derived_traces_dirty();

  // Rebuild _signal_groups before any paint. When a decoder is removed via
  // remove_decoder() or clear_all_decoders(), the DecodeTrace is deleted
  // directly (not through sync_derived_traces()), and on_signals_changed()
  // returns Modified (DecoderStacks aren't in _signal_models). Without this
  // rebuild, _signal_groups retains dangling Trace* pointers, causing SIGSEGV
  // in Header::paintEvent. compute_signal_groups() calls get_traces() which
  // calls sync_derived_traces() first (safe: _derived_traces_dirty was just
  // set above, so it runs once, then the flag is cleared).
  _view->compute_signal_groups();

  _view->viewport_update();
  _view->header_updated();
}

} // namespace view
} // namespace pv
