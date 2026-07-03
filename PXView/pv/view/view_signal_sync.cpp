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

// Phase J (modernize-view-layer-v3): signal-group / signal-rebuild /
// signals-changed layout behaviour extracted from the View God-class.
// ViewSignalSync is declared a friend of View so it can touch the private
// signal state (_own_signals / _signal_groups / _rebuild_in_progress /
// _session / _data_source / _document / _device_agent / _signalHeight /
// _signalHeightScale / _spanY / _trace_view_map / _time_viewport /
// _fft_viewport / _vsplitter / _viewport_list / _header / _data_source)
// directly. Cross-method calls that remain on View (e.g. get_traces,
// get_work_mode, normalize_layout, header_updated, update_scale_offset,
// data_updated, mark_derived_traces_dirty, set_data_document,
// viewport_update) go through _view->… so the public View API is unchanged.

#include "view_signal_sync.h"

#include <algorithm>
#include <cmath>
#include <limits.h>
#include <memory>
#include <set>
#include <vector>

#include <QDebug>
#include <QColor>
#include <QScrollBar>

#include "view.h"

#include "../config/appconfig.h"
#include "../data/decode/decoder.h"
#include "../data/decoderstack.h"
#include "../data/sessiondocument.h"
#include "../data/signalconfigstore.h"
#include "../data/signalmodel.h"
#include "../dsvdef.h"
#include "../log.h"
#include "../sigsession.h"

#include "analogsignal.h"
#include "decodetrace.h"
#include "dsosignal.h"
#include "header.h"
#include "lissajoustrace.h"
#include "logicsignal.h"
#include "mathtrace.h"
#include "signal.h"
#include "signalfactory.h"
#include "spectrumtrace.h"
#include "trace.h"
#include "viewport.h"
#include "viewstatus.h"
#include "../widgets/hoversplitter.h"

using namespace std;

namespace pv {
namespace view {

void ViewSignalSync::compute_signal_groups() {
  _view->_signal_groups.clear();

  if (_view->get_work_mode() != LOGIC) {
    return;
  }

  std::vector<Trace *> all_traces;
  _view->get_traces(ALL_VIEW, all_traces);

  std::vector<Trace *> decode_traces;
  std::vector<Trace *> logic_traces;

  for (auto t : all_traces) {
    if (t->get_type() == SR_CHANNEL_DECODER && t->enabled())
      decode_traces.push_back(t);
    else if (t->get_type() == SR_CHANNEL_LOGIC && t->enabled())
      logic_traces.push_back(t);
  }

  // 按 view_index 排序，确保分组顺序与布局顺序一致
  sort(decode_traces.begin(), decode_traces.end(), [](Trace *a, Trace *b) {
    return a->get_view_index() < b->get_view_index();
  });
  sort(logic_traces.begin(), logic_traces.end(), [](Trace *a, Trace *b) {
    return a->get_view_index() < b->get_view_index();
  });

  std::set<int> assigned_signals;
  int group_id = 0;

  // 第一阶段：收集每个解码通道绑定的逻辑通道索引集合
  struct DecodeBinding {
    DecodeTrace *trace;
    std::set<int> bound_logic_indices;
  };
  std::vector<DecodeBinding> decode_bindings;

  for (auto dt : decode_traces) {
    DecodeTrace *dtrace = dynamic_cast<DecodeTrace *>(dt);
    if (!dtrace)
      continue;

    DecodeBinding binding;
    binding.trace = dtrace;

    auto decoder_stack = dtrace->decoder();
    if (decoder_stack) {
      for (auto decoder : decoder_stack->stack()) {
        auto probe_list = decoder->binded_probe_list();
        for (auto probe : probe_list) {
          int binded_index = decoder->binded_probe_index(probe);
          binding.bound_logic_indices.insert(binded_index);
        }
      }
    }

    decode_bindings.push_back(binding);
  }

  // 第二阶段：将绑定到相同逻辑通道的解码通道合并到同一组
  std::vector<bool> grouped(decode_bindings.size(), false);

  for (size_t i = 0; i < decode_bindings.size(); i++) {
    if (grouped[i])
      continue;

    SignalGroup group;
    group.group_id = group_id++;
    group.traces.push_back(decode_bindings[i].trace);
    grouped[i] = true;

    // 收集该组的所有逻辑通道
    std::set<int> group_logic_indices = decode_bindings[i].bound_logic_indices;

    // 查找其他绑定到相同逻辑通道的解码通道并合并
    for (size_t j = i + 1; j < decode_bindings.size(); j++) {
      if (grouped[j])
        continue;

      // 检查是否有共同的逻辑通道绑定
      bool shares_logic = false;
      for (int logic_idx : decode_bindings[j].bound_logic_indices) {
        if (group_logic_indices.find(logic_idx) != group_logic_indices.end()) {
          shares_logic = true;
          break;
        }
      }

      if (shares_logic) {
        group.traces.push_back(decode_bindings[j].trace);
        grouped[j] = true;
        // 合并逻辑通道集合
        group_logic_indices.insert(
            decode_bindings[j].bound_logic_indices.begin(),
            decode_bindings[j].bound_logic_indices.end());
      }
    }

    // 将逻辑通道加入组（按原始顺序）
    for (auto lt : logic_traces) {
      int logic_index = lt->get_index();
      if (group_logic_indices.find(logic_index) != group_logic_indices.end() &&
          assigned_signals.find(logic_index) == assigned_signals.end()) {
        group.traces.push_back(lt);
        assigned_signals.insert(logic_index);
      }
    }

    _view->_signal_groups.push_back(group);
  }

  std::vector<Trace *> unassigned;
  for (auto lt : logic_traces) {
    if (assigned_signals.find(lt->get_index()) == assigned_signals.end()) {
      unassigned.push_back(lt);
    }
  }
  sort(unassigned.begin(), unassigned.end(), [](Trace *a, Trace *b) {
    return a->get_view_index() < b->get_view_index();
  });
  // 连续的未分配逻辑通道合并成一个组，不连续的单独成组
  if (!unassigned.empty()) {
    SignalGroup group;
    group.group_id = group_id++;
    group.traces.push_back(unassigned[0]);
    for (size_t i = 1; i < unassigned.size(); i++) {
      // 检查是否连续（view_index 相差1）
      if (unassigned[i]->get_view_index() ==
          unassigned[i - 1]->get_view_index() + 1) {
        group.traces.push_back(unassigned[i]);
      } else {
        // 不连续，创建新组
        _view->_signal_groups.push_back(group);
        group = SignalGroup();
        group.group_id = group_id++;
        group.traces.push_back(unassigned[i]);
      }
    }
    _view->_signal_groups.push_back(group);
  }

  for (auto &group : _view->_signal_groups) {
    sort(group.traces.begin(), group.traces.end(), [](Trace *a, Trace *b) {
      return a->get_v_offset() < b->get_v_offset();
    });
  }
}

void ViewSignalSync::signals_changed(const Trace *eventTrace) {
  _view->mark_derived_traces_dirty();
  double actualMargin = View::SignalMargin;
  int total_rows = 0;
  int label_size = 0;
  std::vector<Trace *> time_traces;
  std::vector<Trace *> fft_traces;
  std::vector<Trace *> traces;
  std::vector<Trace *> logic_traces;
  std::vector<Trace *> decoder_traces;

  (void)eventTrace;

  compute_signal_groups();

  if (_view->get_work_mode() == LOGIC && !_view->_signal_groups.empty()) {
    std::vector<size_t> group_order(_view->_signal_groups.size());
    for (size_t i = 0; i < _view->_signal_groups.size(); i++)
      group_order[i] = i;
    sort(group_order.begin(), group_order.end(), [this](size_t a, size_t b) {
      int minA = INT_MAX, minB = INT_MAX;
      for (auto gt : _view->_signal_groups[a].traces) {
        if (gt->get_view_index() >= 0)
          minA = min(minA, gt->get_view_index());
      }
      for (auto gt : _view->_signal_groups[b].traces) {
        if (gt->get_view_index() >= 0)
          minB = min(minB, gt->get_view_index());
      }
      return minA < minB;
    });

    int new_index = 0;
    for (size_t gi : group_order) {
      sort(_view->_signal_groups[gi].traces.begin(),
           _view->_signal_groups[gi].traces.end(),
           [](Trace *a, Trace *b) {
             return a->get_view_index() < b->get_view_index();
           });
      for (auto gt : _view->_signal_groups[gi].traces) {
        gt->set_view_index(new_index++);
      }
    }
  }

  _view->get_traces(ALL_VIEW, traces);

  for (auto t : traces) {
    if (_view->_trace_view_map[t->get_type()] == TIME_VIEW) {
      time_traces.push_back(t);
    } else if (_view->_trace_view_map[t->get_type()] == FFT_VIEW) {
      if (t->enabled())
        fft_traces.push_back(t);
    }

    if (t->get_type() == SR_CHANNEL_LOGIC)
      logic_traces.push_back(t);
    else if (t->get_type() == SR_CHANNEL_DECODER)
      decoder_traces.push_back(t);
  }

  if (!fft_traces.empty()) {
    if (!_view->_fft_viewport->isVisible()) {
      _view->_fft_viewport->setVisible(true);
      _view->_fft_viewport->clear_measure();
      _view->_viewport_list.push_back(_view->_fft_viewport);
      _view->_vsplitter->refresh();
    }

    for (auto t : fft_traces) {
      t->set_view(_view);
      t->set_viewport(_view->_fft_viewport);
      t->set_totalHeight(_view->_fft_viewport->height());
      t->set_v_offset(_view->_fft_viewport->geometry().bottom());
    }
  } else {
    _view->_fft_viewport->setVisible(false);
    _view->_vsplitter->refresh();

    // Find the _fft_viewport in the stack
    std::list<QWidget *>::iterator iter = _view->_viewport_list.begin();

    for (unsigned int i = 0; i < _view->_viewport_list.size(); i++, iter++) {
      if ((*iter) == _view->_fft_viewport)
        break;
    }

    // Delete the element
    if (iter != _view->_viewport_list.end())
      _view->_viewport_list.erase(iter);
  }

  if (!time_traces.empty() && _view->_time_viewport) {
    for (auto t : time_traces) {
      if (dynamic_cast<DsoSignal *>(t) || t->visible())
        total_rows += t->rows_size();
      if (t->rows_size() != 0)
        label_size++;
    }

    const double height =
        (_view->_time_viewport->height() - 2 * actualMargin * label_size) *
        1.0 / total_rows;

    if (_view->_device_agent->have_instance() == false) {
      assert(false);
      return;
    }

    int mode = _view->get_work_mode();

    if (mode == LOGIC) {
      _view->_signalHeight = _view->_signalHeightScale;
    } else if (_view->get_work_mode() == DSO) {
      // PXView's _viewbottom is hidden and overlaid on viewport,
      // so _header->height() is ~DsoStatusHeight larger than original DSView.
      // Subtract DsoStatusHeight to match the original signal height.
      _view->_signalHeight =
          (_view->_header->height() - View::DsoStatusHeight -
           _view->horizontalScrollBar()->height() -
           2 * actualMargin * label_size) *
          1.0 / total_rows;
    } else {
      _view->_signalHeight = (int)((height <= 0) ? 1 : height);
    }

    _view->_spanY = _view->_signalHeight + 2 * actualMargin;
    double next_v_offset = actualMargin;

    if (mode == LOGIC) {
      time_traces.clear();

      std::vector<Trace *> all_traces;

      for (auto t : logic_traces) {
        all_traces.push_back(t);
      }

      for (auto t : decoder_traces) {
        if (t->get_view_index() != -1)
          all_traces.push_back(t);
        else
          time_traces.push_back(t);
      }

      sort(all_traces.begin(), all_traces.end(), View::compare_trace_view_index);

      for (auto t : all_traces) {
        time_traces.push_back(t);
      }
    }

    int current_group_id = -1;

    for (auto t : time_traces) {
      t->set_view(_view);
      t->set_viewport(_view->_time_viewport);

      if (t->rows_size() == 0)
        continue;

      if (!dynamic_cast<DsoSignal *>(t) && !t->visible())
        continue;

      int trace_group_id = -1;
      for (auto &group : _view->_signal_groups) {
        for (auto gt : group.traces) {
          if (gt == t) {
            trace_group_id = group.group_id;
            break;
          }
        }
        if (trace_group_id != -1)
          break;
      }

      if (current_group_id != -1 && trace_group_id != current_group_id) {
        next_v_offset += View::GroupGap + 5;
      }
      current_group_id = trace_group_id;

      double traceHeight;
      if (t->get_own_height() > 0) {
        traceHeight = t->get_own_height();
      } else {
        traceHeight = _view->_signalHeight * t->rows_size();
      }
      t->set_totalHeight((int)traceHeight);
      t->set_v_offset(qRound(next_v_offset + 0.5 * traceHeight + actualMargin));
      next_v_offset += traceHeight + 2 * actualMargin;

      if (t->signal_type() == SR_CHANNEL_DSO) {
        auto sig = dynamic_cast<view::DsoSignal *>(t);
        // PXView's _viewbottom is hidden and overlaid on viewport,
        // so viewport height is ~DsoStatusHeight larger than original DSView.
        // Subtract DsoStatusHeight to match the original scale.
        const int scale_height =
            sig->get_view_rect().height() - View::DsoStatusHeight;
        sig->set_scale(scale_height > 0 ? scale_height
                                        : sig->get_view_rect().height());
      } else if (t->signal_type() == SR_CHANNEL_ANALOG) {
        auto sig = dynamic_cast<view::AnalogSignal *>(t);
        sig->set_scale(sig->get_totalHeight());
      }
    }
    _view->_time_viewport->clear_measure();
    _view->_data_source->update_dso_data_scale();
  }

  _view->normalize_layout();

  for (auto &group : _view->_signal_groups) {
    sort(group.traces.begin(), group.traces.end(), [](Trace *a, Trace *b) {
      return a->get_v_offset() < b->get_v_offset();
    });
  }

  _view->header_updated();
  _view->update_scale_offset();
  _view->data_updated();
}

void ViewSignalSync::rebuild_signals_from_config(
    const pv::data::SignalConfig &config) {
  // Re-entrancy guard: if a nested broadcast (e.g. DSV_MSG_DEVICE_OPTIONS_UPDATED
  // from within this function) triggers OnMessage → rebuild_signals() →
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
    signals_changed(NULL);
    return;
  }

  int view_index = 0;
  for (const auto &ch : config.channels) {
    // Create a temporary SignalModel for the channel configuration.
    // This SignalModel is not connected to a real device (no sr_channel),
    // but it allows the View layer to create Signal objects using the new
    // constructor signature.
    auto model = std::make_shared<data::SignalModel>();
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
      // Core-serialized field. In-memory visible state across rebuilds is
      // preserved by SignalFactory::save_ui_state/restore_ui_state. Persistence
      // will be handled by View-layer DockUiState (Task 17) writing the
      // uiLayout section of .pxc.

      // UI 布局状态从 ChannelConfig 恢复（单一持久化状态源）：
      // - view_index: 配置值 >= 0 时使用，否则按启用顺序派生（向后兼容）
      // - v_offset: 直接使用配置值
      // - own_height: 配置值 >= 0 时使用，否则 DSO/Analog 保持 -1（自动高度），
      //   Logic 不调用 set_own_height（由 Trace 构造函数处理主题默认）
      signal->set_v_offset(ch.v_offset);
      if (ch.own_height >= 0) {
        signal->set_own_height(ch.own_height);
      } else if (config.work_mode == DSO || config.work_mode == ANALOG) {
        signal->set_own_height(-1);
      }
      if (ch.view_index >= 0) {
        signal->set_view_index(ch.view_index);
      } else if (ch.enabled) {
        signal->set_view_index(view_index++);
      } else {
        signal->set_view_index(-1);
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

  signals_changed(NULL);
}

void ViewSignalSync::rebuild_signals() {
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
      rebuild_signals_from_config(config);
      SignalFactory::update_signals(_view->_own_signals, _view->_data_source,
                                    _view->_data_source,
                                    SignalFactory::Modified);
      // Only property changes, no layout needed - use incremental refresh
      signals_modified_refresh();
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
      s->set_enabled(s->model()->enabled());
      sig->set_visible(s->model()->enabled());
    }
  }

  // R9: restore persisted layout (view_index/v_offset/own_height) from
  // SessionDocument. Without this, rebuild_signals() (called from
  // DSV_MSG_DEVICE_OPTIONS_UPDATED handler after reload) creates Signals
  // with default heights, wiping the user's custom layout.
  // rebuild_signals_from_config() at line 2228 only runs when
  // _data_source == _document; this second path runs when _data_source is
  // _session (e.g., before/during capture), and must also restore layout.
  if (_view->_document && _view->_document->has_signal_config()) {
    const auto &cfg = _view->_document->get_signal_config();
    int view_index_seq = 0;
    for (auto *sig : _view->_own_signals) {
      auto it = std::find_if(cfg.channels.begin(), cfg.channels.end(),
                             [&](const data::ChannelConfig &ch) {
                               return ch.index == sig->get_index();
                             });
      if (it != cfg.channels.end()) {
        sig->set_v_offset(it->v_offset);
        if (it->own_height >= 0) {
          sig->set_own_height(it->own_height);
        } else if (cfg.work_mode == DSO || cfg.work_mode == ANALOG) {
          sig->set_own_height(-1);
        }
        if (it->view_index >= 0) {
          sig->set_view_index(it->view_index);
        } else if (it->enabled) {
          sig->set_view_index(view_index_seq++);
        } else {
          sig->set_view_index(-1);
        }
        pxv_info("View::rebuild_signals: restored channel %d from config: view_index=%d, v_offset=%d, own_height=%d",
                 sig->get_index(), sig->get_view_index(),
                 sig->get_v_offset(), sig->get_own_height());
      }
    }
  }

  if (_view->_document && _view->_document->has_data()) {
    _view->set_data_document(_view->_document);
  }

  signals_changed(NULL);
}

void ViewSignalSync::on_signals_changed() {
  // Incrementally update _own_signals to match the Core's SignalModel
  // list. Uses compute_change_event to detect the minimal update type,
  // avoiding full object recreation for minor changes.
  //
  // IMPORTANT: SignalModels ALWAYS live in SigSession (_data_source), never
  // in SessionDocument. SessionDocument::_signal_models is never populated
  // (it only stores data snapshots via _logic/_analog/_dso). Using the
  // snapshot-source accessor (the former effective_data_source(), now
  // document_snapshot_source()) here was a bug: when _document->has_data() is
  // true (after a capture), document_snapshot_source() returns _document, and
  // create_signals(_document) reads the empty _signal_models vector,
  // returning an empty list. AllReplaced then deletes all existing view
  // signals and creates 0 new ones, clearing the waveform tracks
  // (Header::paintEvent shows traces=1).
  //
  // This does NOT directly touch _own_decode_traces / _own_spectrum_traces
  // / _own_math_trace / _own_lissajous_trace. Those are derived traces
  // that wrap Core-owned Stack/Model objects and are synced lazily via
  // sync_derived_traces() based on the Stack pointer identity (not the
  // Signal list).

  if (!_view->_data_source)
    return;

  auto &models = _view->_data_source->get_signal_models();
  auto event = SignalFactory::compute_change_event(_view->_own_signals, models);

  SignalFactory::update_signals(_view->_own_signals, _view->_data_source,
                                _view->_data_source, event);

  // Dispatch to appropriate layout method based on event type.
  switch (event) {
  case SignalFactory::Added:
    // New signals added, layout needs adjustment
    signals_added_layout();
    break;
  case SignalFactory::Removed:
    signals_removed_layout();
    break;
  case SignalFactory::Modified:
    // Only property changes, no layout needed
    signals_modified_refresh();
    break;
  case SignalFactory::AllReplaced:
    // Full rebuild, need full layout
    signals_changed(NULL);
    break;
  }
}

void ViewSignalSync::signals_added_layout() {
  // Layout recalculation is O(N) but relatively cheap compared to object
  // recreation. SignalFactory::update_signals(Added) already added the new
  // Signal objects without recreating existing ones.
  signals_changed(NULL);
}

void ViewSignalSync::signals_removed_layout() {
  // Layout recalculation is O(N) but relatively cheap compared to object
  // recreation. SignalFactory::update_signals(Removed) already removed the
  // Signal objects without recreating existing ones.
  signals_changed(NULL);
}

void ViewSignalSync::signals_modified_refresh() {
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
  compute_signal_groups();

  _view->viewport_update();
  _view->header_updated();
}

// =============================================================================
// Phase J additional: static QColor definitions + refreshSignalColors
// (moved from view.cpp to reduce line count). These are View static member
// definitions — C++ allows them in any .cpp file.
// =============================================================================

QColor View::Red = AppConfig::Instance().GetThemeColor("@signal-red").isValid()
                       ? AppConfig::Instance().GetThemeColor("@signal-red")
                       : QColor(213, 15, 37, 255);
QColor View::Orange =
    AppConfig::Instance().GetThemeColor("@signal-orange").isValid()
        ? AppConfig::Instance().GetThemeColor("@signal-orange")
        : QColor(238, 178, 17, 255);
QColor View::Blue =
    AppConfig::Instance().GetThemeColor("@signal-blue").isValid()
        ? AppConfig::Instance().GetThemeColor("@signal-blue")
        : QColor(17, 133, 209, 255);
QColor View::Green =
    AppConfig::Instance().GetThemeColor("@signal-green").isValid()
        ? AppConfig::Instance().GetThemeColor("@signal-green")
        : QColor(0, 153, 37, 255);
QColor View::Purple =
    AppConfig::Instance().GetThemeColor("@signal-purple").isValid()
        ? AppConfig::Instance().GetThemeColor("@signal-purple")
        : QColor(109, 50, 156, 255);
QColor View::LightBlue =
    AppConfig::Instance().GetThemeColor("@signal-light-blue").isValid()
        ? AppConfig::Instance().GetThemeColor("@signal-light-blue")
        : QColor(17, 133, 209, 200);
QColor View::LightRed =
    AppConfig::Instance().GetThemeColor("@signal-light-red").isValid()
        ? AppConfig::Instance().GetThemeColor("@signal-light-red")
        : QColor(213, 15, 37, 200);

void View::refreshSignalColors() {
  Red = AppConfig::Instance().GetThemeColor("@signal-red").isValid()
            ? AppConfig::Instance().GetThemeColor("@signal-red")
            : QColor(213, 15, 37, 255);
  Orange = AppConfig::Instance().GetThemeColor("@signal-orange").isValid()
               ? AppConfig::Instance().GetThemeColor("@signal-orange")
               : QColor(238, 178, 17, 255);
  Blue = AppConfig::Instance().GetThemeColor("@signal-blue").isValid()
             ? AppConfig::Instance().GetThemeColor("@signal-blue")
             : QColor(17, 133, 209, 255);
  Green = AppConfig::Instance().GetThemeColor("@signal-green").isValid()
              ? AppConfig::Instance().GetThemeColor("@signal-green")
              : QColor(0, 153, 37, 255);
  Purple = AppConfig::Instance().GetThemeColor("@signal-purple").isValid()
               ? AppConfig::Instance().GetThemeColor("@signal-purple")
               : QColor(109, 50, 156, 255);
  LightBlue =
      AppConfig::Instance().GetThemeColor("@signal-light-blue").isValid()
          ? AppConfig::Instance().GetThemeColor("@signal-light-blue")
          : QColor(17, 133, 209, 200);
  LightRed = AppConfig::Instance().GetThemeColor("@signal-light-red").isValid()
                 ? AppConfig::Instance().GetThemeColor("@signal-light-red")
                 : QColor(213, 15, 37, 200);
}

// =============================================================================
// Phase J additional: trace access / layout / theme methods
// =============================================================================

void ViewSignalSync::get_traces(int type, std::vector<Trace *> &traces) {
  assert(_view->_session);

  auto &sigs = _view->_own_signals;
  auto &decode_sigs = _view->get_own_decode_traces();
  auto &spectrums = _view->get_own_spectrum_traces();

  for (auto t : sigs) {
    if (type == ALL_VIEW || _view->_trace_view_map[t->get_type()] == type)
      traces.push_back(t);
  }
  for (auto t : decode_sigs) {
    if (type == ALL_VIEW || _view->_trace_view_map[t->get_type()] == type)
      traces.push_back(t);
  }
  for (auto t : spectrums) {
    if (type == ALL_VIEW || _view->_trace_view_map[t->get_type()] == type)
      traces.push_back(t);
  }

  auto lissajous = _view->get_own_lissajous_trace();
  if (lissajous && lissajous->enabled() &&
      (type == ALL_VIEW ||
       _view->_trace_view_map[lissajous->get_type()] == type)) {
    traces.push_back(lissajous);
  }

  auto math = _view->get_own_math_trace();
  if (math && math->enabled() &&
      (type == ALL_VIEW || _view->_trace_view_map[math->get_type()] == type)) {
    traces.push_back(math);
  }

  sort(traces.begin(), traces.end(), compare_trace_v_offsets);
}

bool ViewSignalSync::compare_trace_v_offsets(const Trace *a, const Trace *b) {
  assert(a);
  assert(b);

  Trace *a1 = const_cast<Trace *>(a);
  Trace *b1 = const_cast<Trace *>(b);
  int v1 = 0;
  int v2 = 0;

  if (a1->get_type() != b1->get_type()) {
    v1 = a1->get_type();
    v2 = b1->get_type();
  } else if (a1->get_type() == SR_CHANNEL_DSO ||
             a1->get_type() == SR_CHANNEL_ANALOG) {
    v1 = a1->get_index();
    v2 = b1->get_index();
  } else {
    v1 = a1->get_v_offset();
    v2 = b1->get_v_offset();
  }
  return v1 < v2;
}

bool ViewSignalSync::compare_trace_view_index(const Trace *a, const Trace *b) {
  assert(a);
  assert(b);

  Trace *a1 = const_cast<Trace *>(a);
  Trace *b1 = const_cast<Trace *>(b);
  return a1->get_view_index() < b1->get_view_index();
}

bool ViewSignalSync::compare_trace_y(const Trace *a, const Trace *b) {
  assert(a);
  assert(b);

  Trace *a1 = const_cast<Trace *>(a);
  Trace *b1 = const_cast<Trace *>(b);
  return a1->get_v_offset() < b1->get_v_offset();
}

void ViewSignalSync::normalize_layout() {
  int v_min = INT_MAX;
  std::vector<Trace *> traces;
  _view->get_traces(ALL_VIEW, traces);

  for (auto t : traces) {
    v_min = min(t->get_v_offset(), v_min);
  }

  const int delta = -min(v_min, 0);

  for (auto t : traces) {
    t->set_v_offset(t->get_v_offset() + delta);
  }

  _view->_vOffset = 0;
  _view->verticalScrollBar()->setSliderPosition(0);
  _view->v_scroll_value_changed(0);
}

void ViewSignalSync::zoom_vertical(double steps) {
  int step = 10;
  int oldHeight = _view->_signalHeightScale;
  if (steps > 0)
    _view->_signalHeightScale += step;
  else
    _view->_signalHeightScale -= step;
  _view->_signalHeightScale =
      max(View::MinSignalHeight,
          min(_view->_signalHeightScale, View::MaxSignalHeight));
  if (_view->_signalHeightScale != oldHeight) {
    double scale = (double)_view->_signalHeightScale / oldHeight;
    std::vector<Trace *> traces;
    _view->get_traces(ALL_VIEW, traces);
    for (auto t : traces) {
      if (t->get_own_height() > 0) {
        t->set_own_height(
            max(View::MinSignalHeight, (int)(t->get_own_height() * scale)));
      }
    }
    _view->signals_changed(NULL);
    _view->update_scroll();
    _view->viewport_update();
  }
}

int ViewSignalSync::headerWidth() {
  if (_view->_header_collapsed) {
    int w = Trace::SquareWidth + 2 * Trace::Margin + 10;
    _view->setViewportMargins(w, View::RulerHeight, 0, 0);
    return w;
  }

  int headerWidth = _view->_header->get_nameEditWidth();

  std::vector<Trace *> traces;
  _view->get_traces(ALL_VIEW, traces);

  if (!traces.empty()) {
    for (auto t : traces) {
      int w = t->get_name_width() + t->get_leftWidth() + t->get_rightWidth();
      headerWidth = max(w, headerWidth);
    }
  }

  _view->setViewportMargins(headerWidth, View::RulerHeight, 0, 0);

  return headerWidth;
}

void ViewSignalSync::UpdateTheme() {
  View::refreshSignalColors();

  QString heightStr =
      AppConfig::Instance().GetThemeTokenValue("@logic-channel-height");
  bool ok;
  int h = heightStr.toInt(&ok);
  if (ok && h > 0) {
    _view->_signalHeightScale = h;
    _view->_signalHeight = h;

    std::vector<Trace *> traces;
    _view->get_traces(ALL_VIEW, traces);
    for (Trace *t : traces) {
      if (t && (t->get_type() == SR_CHANNEL_LOGIC ||
                t->get_type() == SR_CHANNEL_GROUP)) {
        t->set_totalHeight(h);
        t->set_own_height(h);
      }
    }
    _view->update_all_trace_postion();
  }

  _view->viewport_update();
}

QColor ViewSignalSync::get_group_card_color() {
  QColor c = AppConfig::Instance().GetThemeColor("@group-card-bg");
  if (c.isValid())
    return c;
  AppConfig &app = AppConfig::Instance();
  if (app.IsDarkStyle())
    return QColor(0x1a, 0x1a, 0x1a);
  else
    return QColor(0xfa, 0xfa, 0xfa);
}

bool ViewSignalSync::is_colored_card_mode() {
  QString val = AppConfig::Instance().GetThemeTokenValue("@group-card-colored");
  return val == "true";
}

QColor ViewSignalSync::get_group_card_color(int group_index) {
  if (is_colored_card_mode()) {
    const auto &groups = _view->get_signal_groups();
    if (group_index >= 0 && group_index < (int)groups.size()) {
      const auto &group = groups[group_index];
      if (!group.traces.empty()) {
        auto *trace = group.traces[0];
        return get_trace_card_color(trace);
      }
    }
  }
  return get_group_card_color();
}

QColor ViewSignalSync::get_trace_card_color(Trace *trace) {
  if (is_colored_card_mode() && trace) {
    auto index_list = trace->get_index_list();
    if (!index_list.empty()) {
      int idx = *index_list.begin() % 8;
      QString token = QString("@logic-channel-%1").arg(idx);
      QColor signalColor = AppConfig::Instance().GetThemeColor(token);
      if (!signalColor.isValid())
        signalColor = Trace::PROBE_COLORS[idx];
      QColor bgColor = signalColor;
      bgColor.setAlpha(8 * 255 / 100);
      return bgColor;
    }
  }
  return get_group_card_color();
}

} // namespace view
} // namespace pv
