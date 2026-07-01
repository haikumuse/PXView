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

#include <assert.h>
#include <cmath>
#include <limits.h>
#include <memory>
#include <string.h>

#include <QEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QtGlobal>
#include <algorithm>

#include "../api/types.h"
#include "../data/decode/decoder.h"
#include "../data/decode/decoderstatus.h"
#include "../data/decoderstack.h"
#include "../data/signalmodel.h"
#include "analogsignal.h"
#include "decodetrace.h"
#include "devmode.h"
#include "dsosignal.h"
#include "groupsignal.h"
#include "header.h"
#include "lissajoustrace.h"
#include "logicsignal.h"
#include "mathtrace.h"
#include "ruler.h"
#include "signal.h"
#include "signalfactory.h"
#include "spectrumtrace.h"
#include "trace.h"
#include "view.h"
#include "viewport.h"

#include "../appcontrol.h"
#include "../config/appconfig.h"
#include "../data/logicsnapshot.h"
#include "../data/sessiondocument.h"
#include "../data/spectrumstack.h"
#include "../dialogs/calibration.h"
#include "../dialogs/lissajousoptions.h"
#include "../dsvdef.h"
#include "../log.h"
#include "../sigsession.h"
#include "../widgets/hoversplitter.h"

using namespace std;

namespace pv {
namespace view {

const int View::LabelMarginWidth = 70;
const int View::RulerHeight = 50;

const int View::MaxScrollValue = INT_MAX / 2;
const int View::MaxHeightUnit = 24;
const int View::MinSignalHeight = 10;
const int View::MaxSignalHeight = 500;

// const int View::SignalHeight = 30;s
const int View::SignalMargin = 7;
const int View::SignalSnapGridSize = 10;

const QColor View::CursorAreaColour(220, 231, 243);
const QSizeF View::LabelPadding(4, 4);
const QString View::Unknown_Str = "########";

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

View::View(SigSession *session, pv::toolbars::SamplingBar *sampling_bar,
           QWidget *parent)
    : QScrollArea(parent), _sampling_bar(sampling_bar), _scale(10),
      _preScale(1e-6), _maxscale(1e9), _minscale(1e-15), _offset(0),
      _preOffset(0), _vOffset(0), _signalHeightScale(MaxHeightUnit),
      _lastWidth(-1), _updating_scroll(false), _trig_hoff(0),
      _show_cursors(false), _search_hit(false), _show_xcursors(false),
      _hover_point(-1, -1), _dso_auto(true), _show_lissajous(false),
      _back_ready(false) {
  _trig_cursor = NULL;
  _search_cursor = NULL;
  _cali = NULL;

  _session = session;
  _data_source = session;
  _document = nullptr;
  _device_agent = session->get_device();

  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  setStyleSheet(
      QString("QScrollBar:vertical { margin-top: %1px; }").arg(RulerHeight));

  connect(horizontalScrollBar(), &QScrollBar::valueChanged, this,
          &View::h_scroll_value_changed);
  connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
          &View::v_scroll_value_changed);

  // trace viewport map
  _trace_view_map[SR_CHANNEL_LOGIC] = TIME_VIEW;
  _trace_view_map[SR_CHANNEL_GROUP] = TIME_VIEW;
  _trace_view_map[SR_CHANNEL_DECODER] = TIME_VIEW;
  _trace_view_map[SR_CHANNEL_ANALOG] = TIME_VIEW;
  _trace_view_map[SR_CHANNEL_DSO] = TIME_VIEW;
  _trace_view_map[SR_CHANNEL_FFT] = FFT_VIEW;
  _trace_view_map[SR_CHANNEL_LISSAJOUS] = TIME_VIEW;
  _trace_view_map[SR_CHANNEL_MATH] = TIME_VIEW;

  _active_viewport = NULL;
  _header_collapsed = false;
  _ruler = new Ruler(*this);
  _header = new Header(*this);
  _devmode = new DevMode(this, session);

  setViewportMargins(headerWidth(), RulerHeight, 0, 0);

  // windows splitter
  _time_viewport = new Viewport(*this, TIME_VIEW);
  _time_viewport->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  _time_viewport->setMinimumHeight(100);

  _fft_viewport = new Viewport(*this, FFT_VIEW);
  _fft_viewport->setVisible(false);
  _fft_viewport->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  _fft_viewport->setMinimumHeight(100);

  _vsplitter = new pv::widgets::HoverSplitter(this);
  _vsplitter->setOrientation(Qt::Vertical);
  _vsplitter->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

  _viewport_list.push_back(_time_viewport);
  _vsplitter->addWidget(_time_viewport);
  _vsplitter->setCollapsible(0, false);
  _vsplitter->setStretchFactor(0, 2);
  _viewport_list.push_back(_fft_viewport);
  _vsplitter->addWidget(_fft_viewport);
  _vsplitter->setCollapsible(1, false);
  _vsplitter->setStretchFactor(1, 1);

  _viewcenter = new QWidget(this);
  _viewcenter->setContentsMargins(0, 0, 0, 0);
  QGridLayout *layout = new QGridLayout(_viewcenter);
  layout->setSpacing(0);
  layout->setContentsMargins(0, 0, 0, 0);
  _viewcenter->setLayout(layout);
  layout->addWidget(_vsplitter, 0, 0);
  _viewbottom = new ViewStatus(_session, *this);
  _viewbottom->setFixedHeight(StatusHeight);
  _viewbottom->hide();
  _viewbottom->setParent(_time_viewport);
  _viewbottom->setGeometry(0, _time_viewport->height() - StatusHeight,
                           _time_viewport->width(), StatusHeight);

#ifdef Q_OS_DARWIN
  QWidget *lineSpan = new QWidget(this);
  lineSpan->setFixedHeight(10);
  layout->addWidget(lineSpan, 2, 0);
#endif

  setViewport(_viewcenter);

  _time_viewport->installEventFilter(this);
  _fft_viewport->installEventFilter(this);
  _ruler->installEventFilter(this);
  _header->installEventFilter(this);
  _devmode->installEventFilter(this);

  // tr
  _viewcenter->setObjectName("ViewArea_center");
  _ruler->setObjectName("ViewArea_ruler");
  _header->setObjectName("ViewArea_header");

  QColor fore(QWidget::palette().color(QWidget::foregroundRole()));
  fore.setAlpha(View::BackAlpha);

  _show_trig_cursor = false;
  _trig_cursor = new Cursor(*this, -1, 0);
  _trig_cursor->set_colour(View::LightRed);
  _show_search_cursor = false;
  _search_pos = 0;
  _search_cursor = new Cursor(*this, -1, _search_pos);
  _search_cursor->set_colour(fore);

  connect(_time_viewport, &Viewport::measure_updated, this,
          &View::on_measure_updated);
  connect(_time_viewport, &Viewport::prgRate, this, &View::prgRate);
  connect(_fft_viewport, &Viewport::measure_updated, this,
          &View::on_measure_updated);

  connect(_vsplitter, &QSplitter::splitterMoved, this, &View::splitterMoved);

  connect(_header, &Header::traces_moved, this, &View::on_traces_moved);
  connect(_header, &Header::header_updated, this, &View::header_updated);
  connect(_devmode, &DevMode::header_collapse_changed, this,
          &View::on_header_collapse_changed);

  ADD_UI(this);
}

View::~View() {
  _destroying = true;

  // Disconnect signals and remove event filters before child destruction
  // to prevent callbacks on partially-destroyed View
  disconnect(_header, nullptr, this, nullptr);
  disconnect(_devmode, nullptr, this, nullptr);
  _header->removeEventFilter(this);
  _ruler->removeEventFilter(this);
  _devmode->removeEventFilter(this);
  _time_viewport->removeEventFilter(this);
  _fft_viewport->removeEventFilter(this);

  for (auto sig : _own_signals)
    delete sig;
  _own_signals.clear();

  // Clean up View-owned wrapper traces (these wrap Core layer Stack/Model
  // objects and are owned by the View, not by the data source).
  for (auto dt : _own_decode_traces)
    delete dt;
  _own_decode_traces.clear();

  for (auto st : _own_spectrum_traces)
    delete st;
  _own_spectrum_traces.clear();

  if (_own_math_trace) {
    delete _own_math_trace;
    _own_math_trace = nullptr;
  }

  if (_own_lissajous_trace) {
    delete _own_lissajous_trace;
    _own_lissajous_trace = nullptr;
  }

  DESTROY_OBJECT(_trig_cursor);
  DESTROY_OBJECT(_search_cursor);
  REMOVE_UI(this);
}

void View::set_data_source(pv::data::DataSource *source) {
  _data_source = source;
  mark_derived_traces_dirty();
  rebuild_signals();

  if (_time_viewport) {
    _time_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
  }
  if (_fft_viewport) {
    _fft_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
  }
  update();
}

void View::clear_signal_data() {
  for (auto sig : _own_signals) {
    int type = sig->signal_type();
    switch (type) {
    case SR_CHANNEL_LOGIC: {
      view::LogicSignal *s = static_cast<view::LogicSignal *>(sig);
      s->set_data(nullptr);
      break;
    }
    case SR_CHANNEL_ANALOG: {
      view::AnalogSignal *s = static_cast<view::AnalogSignal *>(sig);
      s->set_data(nullptr);
      break;
    }
    case SR_CHANNEL_DSO: {
      view::DsoSignal *s = static_cast<view::DsoSignal *>(sig);
      s->set_data(nullptr);
      break;
    }
    }
  }

  if (_time_viewport) {
    _time_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
  }
  if (_fft_viewport) {
    _fft_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
  }
  update();
}

void View::set_signal_data_from_source(pv::data::DataSource *source) {
  for (auto sig : _own_signals) {
    int type = sig->signal_type();
    switch (type) {
    case SR_CHANNEL_LOGIC: {
      view::LogicSignal *s = static_cast<view::LogicSignal *>(sig);
      s->set_data(source->get_logic_snapshot());
      break;
    }
    case SR_CHANNEL_ANALOG: {
      view::AnalogSignal *s = static_cast<view::AnalogSignal *>(sig);
      s->set_data(source->get_analog_snapshot());
      break;
    }
    case SR_CHANNEL_DSO: {
      view::DsoSignal *s = static_cast<view::DsoSignal *>(sig);
      s->set_data(source->get_dso_snapshot());
      break;
    }
    }
  }

  if (_time_viewport) {
    _time_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
  }
  if (_fft_viewport) {
    _fft_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
  }
  update();
}

void View::set_data_document(pv::data::SessionDocument *doc) {
  if (!doc)
    return;

  _document = doc;
  mark_derived_traces_dirty();

  if (!doc->has_data())
    return;

  if (_own_signals.empty()) {
    auto created_sigs = SignalFactory::create_signals(_data_source, _session);
    for (auto sig : created_sigs) {
      _own_signals.push_back(sig);
    }
  }

  for (auto sig : _own_signals) {
    int type = sig->signal_type();
    switch (type) {
    case SR_CHANNEL_LOGIC: {
      view::LogicSignal *s = static_cast<view::LogicSignal *>(sig);
      s->set_data(doc->get_active_logic());
      break;
    }
    case SR_CHANNEL_ANALOG: {
      view::AnalogSignal *s = static_cast<view::AnalogSignal *>(sig);
      s->set_data(doc->get_active_analog());
      break;
    }
    case SR_CHANNEL_DSO: {
      view::DsoSignal *s = static_cast<view::DsoSignal *>(sig);
      s->set_data(doc->get_active_dso());
      break;
    }
    }
  }

  if (_time_viewport) {
    _time_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
  }
  if (_fft_viewport) {
    _fft_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
  }
  update();
}

void View::clone_signals_for_document(pv::data::SessionDocument *doc) {
  if (!doc)
    return;

  _own_signals.clear();

  auto created_sigs = SignalFactory::create_signals(_data_source, _session);
  for (auto sig : created_sigs) {
    _own_signals.push_back(sig);
  }

  set_data_document(doc);
}

data::DataSource *View::document_snapshot_source() {
  if (_document && _document->has_data())
    return _document;
  return _data_source;
}

void View::show_wait_trigger() { _time_viewport->show_wait_trigger(); }

void View::set_device() { _devmode->set_device(); }

void View::capture_init() {
  int width = get_view_width();
  if (width == 0) {
    return;
  }

  int mode = get_work_mode();

  if (mode == DSO)
    show_trig_cursor(true);
  else if (!_session->is_repeating())
    show_trig_cursor(false);

  double sampletime = document_snapshot_source()->cur_sampletime();
  if (sampletime > 0) {
    _maxscale = sampletime / (width * MaxViewRate);

    if (mode == ANALOG) {
      set_scale_offset(_maxscale, 0);
    }
  }

  status_clear();

  _trig_hoff = 0;
}

void View::zoom(double steps) {
  int width = get_view_width();
  if (width > 0) {
    zoom(steps, width / 2);
  }
}

void View::set_update(Viewport *viewport, bool need_update) {
  viewport->set_need_update(need_update);
}

void View::set_all_update(bool need_update) {
  _time_viewport->set_need_update(need_update);
  _fft_viewport->set_need_update(need_update);
}

double View::get_hori_res() { return _sampling_bar->get_hori_res(); }

void View::update_hori_res() {
  if (get_work_mode() == DSO) {
    _sampling_bar->hori_knob(0);
  }
}

bool View::zoom(double steps, int offset) {
  int width = get_view_width();
  if (width == 0) {
    return false;
  }

  bool ret = true;
  _preScale = _scale;
  _preOffset = _offset;

  if (get_work_mode() != DSO) {
    _scale *= std::pow(3.0 / 2.0, -steps);
    _scale = max(min(_scale, _maxscale), _minscale);
  } else {
    if (_session->is_running_status() && _session->is_instant()) {
      return ret;
    }

    double hori_res = -1;
    if (steps > 0.5)
      hori_res = _sampling_bar->hori_knob(-1);
    else if (steps < -0.5)
      hori_res = _sampling_bar->hori_knob(1);

    if (hori_res > 0) {
      const double scale = _session->cur_view_time() / width;
      _scale = max(min(scale, _maxscale), _minscale);
    } else {
      ret = false;
    }
  }

  _offset = floor((_offset + offset) * (_preScale / _scale) - offset);
  _offset = max(min(_offset, get_max_offset()), get_min_offset());

  if (_scale != _preScale || _offset != _preOffset) {
    _header->update();
    _ruler->update();
    viewport_update();
    update_scroll();
  }

  return ret;
}

void View::zoom_vertical(double steps) {
  int step = 10;
  int oldHeight = _signalHeightScale;
  if (steps > 0)
    _signalHeightScale += step;
  else
    _signalHeightScale -= step;
  _signalHeightScale =
      max(MinSignalHeight, min(_signalHeightScale, MaxSignalHeight));
  if (_signalHeightScale != oldHeight) {
    double scale = (double)_signalHeightScale / oldHeight;
    std::vector<Trace *> traces;
    get_traces(ALL_VIEW, traces);
    for (auto t : traces) {
      if (t->get_own_height() > 0) {
        t->set_own_height(
            max(MinSignalHeight, (int)(t->get_own_height() * scale)));
      }
    }
    signals_changed(NULL);
    update_scroll();
    viewport_update();
  }
}

void View::compute_signal_groups() {
  _signal_groups.clear();

  if (get_work_mode() != LOGIC) {
    return;
  }

  std::vector<Trace *> all_traces;
  get_traces(ALL_VIEW, all_traces);

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

    _signal_groups.push_back(group);
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
        _signal_groups.push_back(group);
        group = SignalGroup();
        group.group_id = group_id++;
        group.traces.push_back(unassigned[i]);
      }
    }
    _signal_groups.push_back(group);
  }

  for (auto &group : _signal_groups) {
    sort(group.traces.begin(), group.traces.end(), [](Trace *a, Trace *b) {
      return a->get_v_offset() < b->get_v_offset();
    });
  }
}

QColor View::get_group_card_color() {
  QColor c = AppConfig::Instance().GetThemeColor("@group-card-bg");
  if (c.isValid())
    return c;
  AppConfig &app = AppConfig::Instance();
  if (app.IsDarkStyle())
    return QColor(0x1a, 0x1a, 0x1a);
  else
    return QColor(0xfa, 0xfa, 0xfa);
}

bool View::is_colored_card_mode() {
  QString val = AppConfig::Instance().GetThemeTokenValue("@group-card-colored");
  return val == "true";
}

QColor View::get_group_card_color(int group_index) {
  if (is_colored_card_mode()) {
    const auto &groups = get_signal_groups();
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

QColor View::get_trace_card_color(Trace *trace) {
  if (is_colored_card_mode() && trace) {
    auto index_list = trace->get_index_list();
    if (!index_list.empty()) {
      int idx = *index_list.begin() % 8;
      QString token = QString("@logic-channel-%1").arg(idx);
      QColor signalColor = AppConfig::Instance().GetThemeColor(token);
      if (!signalColor.isValid())
        signalColor = Trace::PROBE_COLORS[idx];
      // PulseView style: signal color + 8% alpha
      QColor bgColor = signalColor;
      bgColor.setAlpha(8 * 255 / 100);
      return bgColor;
    }
  }
  return get_group_card_color();
}

void View::timebase_changed() {
  int width = get_view_width();
  if (width == 0) {
    return;
  }

  if (get_work_mode() != DSO) {
    return;
  }

  double scale = this->scale();
  double hori_res = _sampling_bar->get_hori_res();

  if (hori_res > 0) {
    scale = _session->cur_view_time() / width;
  }

  set_scale_offset(scale, this->offset());
}

void View::set_scale_offset(double scale, int64_t offset) {
  _preScale = _scale;
  _preOffset = _offset;

  _scale = max(scale, _minscale);
  _offset = floor(max(offset, get_min_offset()));

  if (_scale != _preScale || _offset != _preOffset) {
    update_scroll();
    _header->update();
    _ruler->update();
    viewport_update();
  }
}

void View::limit_scale_offset() {
  if (get_work_mode() != DSO) {
    int width = get_view_width();
    double sampletime = document_snapshot_source()->cur_sampletime();
    uint64_t samplerate = document_snapshot_source()->cur_snap_samplerate();
    if (sampletime > 0 && samplerate > 0 && width > 0) {
      _maxscale = sampletime / (width * MaxViewRate);
      _minscale = (1.0 / samplerate) / MaxPixelsPerSample;
    }
    _scale = max(min(_scale, _maxscale), _minscale);
    _offset = max(min(_offset, get_max_offset()), get_min_offset());
    update_scroll();
    _ruler->update();
    viewport_update();
  }
}

void View::set_preScale_preOffset() { set_scale_offset(_preScale, _preOffset); }

void View::get_traces(int type, std::vector<Trace *> &traces) {
  assert(_session);

  auto &sigs = _own_signals;

  auto &decode_sigs = get_own_decode_traces();

  auto &spectrums = get_own_spectrum_traces();

  for (auto t : sigs) {
    if (type == ALL_VIEW || _trace_view_map[t->get_type()] == type)
      traces.push_back(t);
  }

  for (auto t : decode_sigs) {
    if (type == ALL_VIEW || _trace_view_map[t->get_type()] == type)
      traces.push_back(t);
  }

  for (auto t : spectrums) {
    if (type == ALL_VIEW || _trace_view_map[t->get_type()] == type)
      traces.push_back(t);
  }

  auto lissajous = get_own_lissajous_trace();
  if (lissajous && lissajous->enabled() &&
      (type == ALL_VIEW || _trace_view_map[lissajous->get_type()] == type)) {
    traces.push_back(lissajous);
  }

  auto math = get_own_math_trace();
  if (math && math->enabled() &&
      (type == ALL_VIEW || _trace_view_map[math->get_type()] == type)) {
    traces.push_back(math);
  }

  sort(traces.begin(), traces.end(), compare_trace_v_offsets);
}

bool View::compare_trace_v_offsets(const Trace *a, const Trace *b) {
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

bool View::compare_trace_view_index(const Trace *a, const Trace *b) {
  assert(a);
  assert(b);

  Trace *a1 = const_cast<Trace *>(a);
  Trace *b1 = const_cast<Trace *>(b);
  return a1->get_view_index() < b1->get_view_index();
}

bool View::compare_trace_y(const Trace *a, const Trace *b) {
  assert(a);
  assert(b);

  Trace *a1 = const_cast<Trace *>(a);
  Trace *b1 = const_cast<Trace *>(b);
  return a1->get_v_offset() < b1->get_v_offset();
}

void View::show_cursors(bool show) {
  _show_cursors = show;
  _ruler->update();
  viewport_update();
}

void View::show_trig_cursor(bool show) {
  _show_trig_cursor = show;
  _ruler->update();
  viewport_update();
}

void View::show_search_cursor(bool show) {
  _show_search_cursor = show;
  _ruler->update();
  viewport_update();
}

void View::status_clear() {
  _time_viewport->clear_dso_xm();
  _time_viewport->clear_measure();
  _viewbottom->clear();
}

void View::repeat_unshow() { _viewbottom->repeat_unshow(); }

void View::frame_began() {
  _search_hit = false;
  _search_pos = 0;
  set_search_pos(_search_pos, _search_hit);
}

void View::receive_end() {
  if (get_work_mode() == LOGIC) {
    bool rle = false;
    uint64_t actual_samples;
    bool ret;

    ret = _device_agent->get_config_bool(SR_CONF_RLE, rle);

    if (ret && rle) {
      ret = _device_agent->get_config_uint64(SR_CONF_ACTUAL_SAMPLES,
                                             actual_samples);
      if (ret) {
        if (actual_samples != document_snapshot_source()->cur_samplelimits()) {
          _viewbottom->set_rle_depth(actual_samples);
        }
      }
    }
  }
  _time_viewport->unshow_wait_trigger();

  limit_scale_offset();
}

void View::receive_trigger(quint64 trig_pos1) {
  (void)trig_pos1;
  uint64_t trig_pos = document_snapshot_source()->get_trigger_pos();
  set_trig_cursor_posistion(trig_pos);
}

void View::set_trig_cursor_posistion(uint64_t trig_pos) {
  const double time =
      trig_pos * 1.0 / document_snapshot_source()->cur_snap_samplerate();
  _trig_cursor->set_index(trig_pos);

  int width = get_view_width();
  assert(width > 0);

  if (ds_trigger_get_en() || _device_agent->is_virtual() ||
      get_work_mode() == DSO) {
    _show_trig_cursor = true;

    AppConfig &app = AppConfig::Instance();
    if (app.appOptions.trigPosDisplayInMid) {
      set_scale_offset(_scale, (time / _scale) - (width / 2));
    }
  }

  _ruler->update();
  viewport_update();
}

void View::set_trig_pos(int percent) {
  uint64_t index = document_snapshot_source()->cur_samplelimits() * percent / 100;

  if (_session->have_view_data() == false || _session->is_working()) {
    set_trig_cursor_posistion(index);
  }
}

void View::set_search_pos(uint64_t search_pos, bool hit) {
  QColor fore(QWidget::palette().color(QWidget::foregroundRole()));
  fore.setAlpha(View::BackAlpha);

  const double time =
      search_pos * 1.0 / document_snapshot_source()->cur_snap_samplerate();
  _search_pos = search_pos;
  _search_hit = hit;
  _search_cursor->set_index(search_pos);
  _search_cursor->set_colour(hit ? View::Blue : fore);

  int width = get_view_width();
  assert(width);

  if (hit) {
    set_scale_offset(_scale, (time / _scale) - (width / 2));
    _ruler->update();
    viewport_update();
  }
}

void View::normalize_layout() {
  int v_min = INT_MAX;
  std::vector<Trace *> traces;
  get_traces(ALL_VIEW, traces);

  for (auto t : traces) {
    v_min = min(t->get_v_offset(), v_min);
  }

  const int delta = -min(v_min, 0);

  for (auto t : traces) {
    t->set_v_offset(t->get_v_offset() + delta);
  }

  _vOffset = 0;
  verticalScrollBar()->setSliderPosition(0);
  v_scroll_value_changed(0);
}

void View::get_scroll_layout(int64_t &length, int64_t &offset) {
  length = ceil(document_snapshot_source()->cur_snap_sampletime() / _scale);
  offset = _offset;
}

void View::update_scroll() {
  assert(_viewcenter);

  int width = get_view_width();
  if (width == 0) {
    return;
  }

  const QSize areaSize = QSize(width, get_view_height());

  // Set the horizontal scroll bar
  int64_t length = 0;
  int64_t offset = 0;
  get_scroll_layout(length, offset);
  length = max(length - areaSize.width(), (int64_t)0);

  horizontalScrollBar()->setPageStep(areaSize.width() / 2);

  _updating_scroll = true;

  if (length < MaxScrollValue) {
    horizontalScrollBar()->setRange(0, length);
    horizontalScrollBar()->setSliderPosition(offset);
  } else {
    horizontalScrollBar()->setRange(0, MaxScrollValue);
    horizontalScrollBar()->setSliderPosition(_offset * 1.0 / length *
                                             MaxScrollValue);
  }

  _updating_scroll = false;

  // Set the vertical scrollbar
  int totalContentHeight = 0;
  if (_time_viewport)
    totalContentHeight = _time_viewport->get_total_height();
  int vRange = max(0, totalContentHeight - areaSize.height());
  if (vRange > 0)
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  else
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  verticalScrollBar()->setPageStep(areaSize.height());
  verticalScrollBar()->setRange(0, vRange);
  verticalScrollBar()->setSliderPosition(_vOffset);
}

void View::update_scale_offset() {
  int width = get_view_width();
  if (width == 0) {
    return;
  }

  if (get_work_mode() != DSO) {
    double sampletime = document_snapshot_source()->cur_sampletime();
    uint64_t samplerate = document_snapshot_source()->cur_snap_samplerate();
    if (sampletime > 0 && samplerate > 0) {
      _maxscale = sampletime / (width * MaxViewRate);
      _minscale = (1.0 / samplerate) / MaxPixelsPerSample;
    } else {
      _maxscale = 1e9;
      _minscale = 1e-15;
    }
    _scale = max(_scale, _minscale);
  } else {
    _scale = _session->cur_view_time() / width;
    _maxscale = 1e9;
    _minscale = 1e-15;
    _scale = max(_scale, _minscale);
  }

  _offset = max(_offset, get_min_offset());

  _preScale = _scale;
  _preOffset = _offset;

  _ruler->update();
  viewport_update();
}

void View::mode_changed() {
  if (_device_agent->is_virtual()) {
    uint64_t samplerate = document_snapshot_source()->cur_snap_samplerate();
    if (samplerate > 0)
      _scale = WellSamplesPerPixel * 1.0 / samplerate;
  }
  _scale = max(min(_scale, _maxscale), _minscale);
}

void View::signals_changed(const Trace *eventTrace) {
  mark_derived_traces_dirty();
  double actualMargin = SignalMargin;
  int total_rows = 0;
  int label_size = 0;
  std::vector<Trace *> time_traces;
  std::vector<Trace *> fft_traces;
  std::vector<Trace *> traces;
  std::vector<Trace *> logic_traces;
  std::vector<Trace *> decoder_traces;

  (void)eventTrace;

  compute_signal_groups();

  if (get_work_mode() == LOGIC && !_signal_groups.empty()) {
    std::vector<size_t> group_order(_signal_groups.size());
    for (size_t i = 0; i < _signal_groups.size(); i++)
      group_order[i] = i;
    sort(group_order.begin(), group_order.end(), [this](size_t a, size_t b) {
      int minA = INT_MAX, minB = INT_MAX;
      for (auto gt : _signal_groups[a].traces) {
        if (gt->get_view_index() >= 0)
          minA = min(minA, gt->get_view_index());
      }
      for (auto gt : _signal_groups[b].traces) {
        if (gt->get_view_index() >= 0)
          minB = min(minB, gt->get_view_index());
      }
      return minA < minB;
    });

    int new_index = 0;
    for (size_t gi : group_order) {
      sort(_signal_groups[gi].traces.begin(), _signal_groups[gi].traces.end(),
           [](Trace *a, Trace *b) {
             return a->get_view_index() < b->get_view_index();
           });
      for (auto gt : _signal_groups[gi].traces) {
        gt->set_view_index(new_index++);
      }
    }
  }

  get_traces(ALL_VIEW, traces);

  for (auto t : traces) {
    if (_trace_view_map[t->get_type()] == TIME_VIEW) {
      time_traces.push_back(t);
    } else if (_trace_view_map[t->get_type()] == FFT_VIEW) {
      if (t->enabled())
        fft_traces.push_back(t);
    }

    if (t->get_type() == SR_CHANNEL_LOGIC)
      logic_traces.push_back(t);
    else if (t->get_type() == SR_CHANNEL_DECODER)
      decoder_traces.push_back(t);
  }

  if (!fft_traces.empty()) {
    if (!_fft_viewport->isVisible()) {
      _fft_viewport->setVisible(true);
      _fft_viewport->clear_measure();
      _viewport_list.push_back(_fft_viewport);
      _vsplitter->refresh();
    }

    for (auto t : fft_traces) {
      t->set_view(this);
      t->set_viewport(_fft_viewport);
      t->set_totalHeight(_fft_viewport->height());
      t->set_v_offset(_fft_viewport->geometry().bottom());
    }
  } else {
    _fft_viewport->setVisible(false);
    _vsplitter->refresh();

    // Find the _fft_viewport in the stack
    std::list<QWidget *>::iterator iter = _viewport_list.begin();

    for (unsigned int i = 0; i < _viewport_list.size(); i++, iter++) {
      if ((*iter) == _fft_viewport)
        break;
    }

    // Delete the element
    if (iter != _viewport_list.end())
      _viewport_list.erase(iter);
  }

  if (!time_traces.empty() && _time_viewport) {
    for (auto t : time_traces) {
      if (dynamic_cast<DsoSignal *>(t) || t->visible())
        total_rows += t->rows_size();
      if (t->rows_size() != 0)
        label_size++;
    }

    const double height =
        (_time_viewport->height() - 2 * actualMargin * label_size) * 1.0 /
        total_rows;

    if (_device_agent->have_instance() == false) {
      assert(false);
    }

    int mode = get_work_mode();

    if (mode == LOGIC) {
      _signalHeight = _signalHeightScale;
    } else if (get_work_mode() == DSO) {
      // PXView's _viewbottom is hidden and overlaid on viewport,
      // so _header->height() is ~DsoStatusHeight larger than original DSView.
      // Subtract DsoStatusHeight to match the original signal height.
      _signalHeight =
          (_header->height() - DsoStatusHeight -
           horizontalScrollBar()->height() - 2 * actualMargin * label_size) *
          1.0 / total_rows;
    } else {
      _signalHeight = (int)((height <= 0) ? 1 : height);
    }

    _spanY = _signalHeight + 2 * actualMargin;
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

      sort(all_traces.begin(), all_traces.end(), compare_trace_view_index);

      for (auto t : all_traces) {
        time_traces.push_back(t);
      }
    }

    int current_group_id = -1;

    for (auto t : time_traces) {
      t->set_view(this);
      t->set_viewport(_time_viewport);

      if (t->rows_size() == 0)
        continue;

      if (!dynamic_cast<DsoSignal *>(t) && !t->visible())
        continue;

      int trace_group_id = -1;
      for (auto &group : _signal_groups) {
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
        next_v_offset += GroupGap + 5;
      }
      current_group_id = trace_group_id;

      double traceHeight;
      if (t->get_own_height() > 0) {
        traceHeight = t->get_own_height();
      } else {
        traceHeight = _signalHeight * t->rows_size();
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
            sig->get_view_rect().height() - DsoStatusHeight;
        sig->set_scale(scale_height > 0 ? scale_height
                                        : sig->get_view_rect().height());
      } else if (t->signal_type() == SR_CHANNEL_ANALOG) {
        auto sig = dynamic_cast<view::AnalogSignal *>(t);
        sig->set_scale(sig->get_totalHeight());
      }
    }
    _time_viewport->clear_measure();
    _session->update_dso_data_scale();
  }

  normalize_layout();

  for (auto &group : _signal_groups) {
    sort(group.traces.begin(), group.traces.end(), [](Trace *a, Trace *b) {
      return a->get_v_offset() < b->get_v_offset();
    });
  }

  header_updated();
  update_scale_offset();
  data_updated();
}

bool View::eventFilter(QObject *object, QEvent *event) {
  if (_destroying)
    return QObject::eventFilter(object, event);

  const QEvent::Type type = event->type();
  if (type == QEvent::MouseMove) {
    const QMouseEvent *const mouse_event = (QMouseEvent *)event;
    if (object == _ruler || object == _time_viewport ||
        object == _fft_viewport) {
      //_hover_point = QPoint(mouse_event->x(), 0);
      double cur_periods = (mouse_event->position().toPoint().x() + _offset) *
                           _scale / _ruler->get_min_period();
      int integer_x =
          round(cur_periods) * _ruler->get_min_period() / _scale - _offset;
      double cur_deviate_x =
          qAbs(mouse_event->position().toPoint().x() - integer_x);
      if (get_work_mode() == LOGIC && cur_deviate_x < 10)
        _hover_point = QPoint(integer_x, mouse_event->position().toPoint().y());
      else
        _hover_point = mouse_event->position().toPoint();
    } else if (object == _header)
      _hover_point = QPoint(0, (int)mouse_event->position().y());
    else
      _hover_point = QPoint(-1, -1);

    hover_point_changed();
  } else if (type == QEvent::Leave) {
    _hover_point = QPoint(-1, -1);
    hover_point_changed();
  }

  return QObject::eventFilter(object, event);
}

bool View::viewportEvent(QEvent *e) {
  switch (e->type()) {
  case QEvent::Paint:
  case QEvent::MouseButtonPress:
  case QEvent::MouseButtonRelease:
  case QEvent::MouseButtonDblClick:
  case QEvent::MouseMove:
  case QEvent::Wheel:
  case QEvent::Gesture:
    return false;

  default:
    return QAbstractScrollArea::viewportEvent(e);
  }
}

int View::headerWidth() {
  if (_header_collapsed) {
    int w = Trace::SquareWidth + 2 * Trace::Margin + 10;
    setViewportMargins(w, RulerHeight, 0, 0);
    return w;
  }

  int headerWidth = _header->get_nameEditWidth();

  std::vector<Trace *> traces;
  get_traces(ALL_VIEW, traces);

  if (!traces.empty()) {
    for (auto t : traces) {
      int w = t->get_name_width() + t->get_leftWidth() + t->get_rightWidth();
      headerWidth = max(w, headerWidth);
    }
  }

  setViewportMargins(headerWidth, RulerHeight, 0, 0);

  return headerWidth;
}

void View::paintEvent(QPaintEvent *event) { QScrollArea::paintEvent(event); }

void View::scrollContentsBy(int dx, int dy) {
  (void)dx;
  (void)dy;
}

void View::resizeEvent(QResizeEvent *event) {
  (void)event;
  int width = get_view_width();

  if (width == 0) {
    return;
  }

  // 优化：如果只是高度变化（如 TitleBar Ribbon 展开/折叠），且宽度不变，
  // 则跳过大部分重计算，因为 viewport 只是被平移，内容没有变化
  bool widthChanged = (_lastWidth != width);
  _lastWidth = width;

  if (!widthChanged && get_work_mode() != DSO) {
    setViewportMargins(headerWidth(), RulerHeight, 0, 0);
    _header->header_resize();
    update_scroll();
    viewport_update();
    return;
  }

  reconstruct();
  setViewportMargins(headerWidth(), RulerHeight, 0, 0);
  update_margins();
  update_scroll();
  signals_changed(NULL);

  if (get_work_mode() == DSO) {
    _scale = _session->cur_view_time() / width;
  }

  if (get_work_mode() != DSO) {
    _maxscale =
        document_snapshot_source()->cur_sampletime() / (width * MaxViewRate);
    if (_scale > _maxscale) {
      _scale = _maxscale;
    }
  } else {
    _maxscale = 1e9;
  }

  _ruler->update();
  _header->header_resize();
  set_update(_time_viewport, true);
  set_update(_fft_viewport, true);
  resize();
}

void View::h_scroll_value_changed(int value) {
  if (_updating_scroll)
    return;

  _preOffset = _offset;

  const int range = horizontalScrollBar()->maximum();
  if (range < MaxScrollValue)
    _offset = value;
  else {
    int64_t length = 0;
    int64_t offset = 0;
    get_scroll_layout(length, offset);
    _offset = floor(value * 1.0 / MaxScrollValue * length);
  }

  _offset = max(min(_offset, get_max_offset()), get_min_offset());

  if (_offset != _preOffset) {
    _ruler->update();
    viewport_update();
  }
}

void View::v_scroll_value_changed(int value) {
  _vOffset = value;
  _header->update();
  viewport_update();
}

void View::data_updated() {
  // Deduplicate rapid calls: if called within 16ms of the last execution,
  // only mark viewports dirty without doing full update cycle
  if (_data_updated_timer.isValid() && _data_updated_timer.elapsed() < 16) {
    set_update(_time_viewport, true);
    set_update(_fft_viewport, true);
    return;
  }

  // Refresh data pointers in render objects (does NOT rebuild them).
  // Signals hold raw snapshot pointers that may become stale when the
  // active data source swaps its backing snapshots (e.g. after a capture,
  // glitch filter, or document switch). Re-bind the latest snapshots from
  // the effective data source.
  auto *source = document_snapshot_source();
  if (source) {
    for (auto sig : _own_signals) {
      int type = sig->signal_type();
      switch (type) {
      case SR_CHANNEL_LOGIC: {
        view::LogicSignal *s = static_cast<view::LogicSignal *>(sig);
        s->set_data(source->get_logic_snapshot());
        break;
      }
      case SR_CHANNEL_ANALOG: {
        view::AnalogSignal *s = static_cast<view::AnalogSignal *>(sig);
        s->set_data(source->get_analog_snapshot());
        break;
      }
      case SR_CHANNEL_DSO: {
        view::DsoSignal *s = static_cast<view::DsoSignal *>(sig);
        s->set_data(source->get_dso_snapshot());
        break;
      }
      }
    }

    // LissajousTrace holds a DsoSnapshot pointer directly.
    if (_own_lissajous_trace) {
      _own_lissajous_trace->set_data(source->get_dso_snapshot());
    }
    // Note: DecodeTrace / SpectrumTrace / MathTrace wrap Core-owned
    // Stack objects which manage their own snapshot pointers internally,
    // so no explicit refresh is needed here.
  }

  setViewportMargins(headerWidth(), RulerHeight, 0, 0);
  update_margins();

  // Update the scroll bars
  update_scroll();

  // update scale & offset
  update_scale_offset();

  // Repaint the view
  _time_viewport->unshow_wait_trigger();
  set_update(_time_viewport, true);
  set_update(_fft_viewport, true);
  viewport_update();
  _ruler->update();

  _data_updated_timer.start();
}

void View::update_margins() {
  int width = get_view_width();

  if (width > 0) {
    _ruler->setGeometry(_viewcenter->x(), 0, this->width() - _viewcenter->x(),
                        _viewcenter->y());
    _header->setGeometry(0, _viewcenter->y(), _viewcenter->x(),
                         _viewcenter->height());
    _devmode->setGeometry(0, 0, _viewcenter->x(), _viewcenter->y());
  }
}

void View::header_updated() {
  headerWidth();
  update_margins();

  // Update the scroll bars
  update_scroll();

  viewport_update();
  _header->update();
}

void View::on_header_collapse_changed(bool collapsed) {
  _header_collapsed = collapsed;
  headerWidth();
  update_margins();
  update_scroll();
  viewport_update();
  _header->update();
  _ruler->update();
}

void View::marker_time_changed() {
  _ruler->update();
  viewport_update();
}

void View::on_traces_moved() {
  update_scroll();
  set_update(_time_viewport, true);
  viewport_update();
}

void View::make_cursors_order() {
  int dex = 1;

  for (auto cursor : get_cursorList()) {
    cursor->set_order(dex++);
  }

  dex = 1;
  for (auto cursor : get_xcursorList()) {
    cursor->set_order(dex++);
  }
}

void View::add_cursor(QColor color, uint64_t sampleIndex) {
  (void)color;
  Cursor *newCursor = new Cursor(*this, -1, sampleIndex);
  get_cursorList().push_back(newCursor);
  make_cursors_order();
  cursor_update();
}

void View::add_cursor(uint64_t sampleIndex) {
  static int lastOrder = 1;
  Cursor *newCursor = new Cursor(*this, lastOrder++, sampleIndex);
  get_cursorList().push_back(newCursor);
  make_cursors_order();
  cursor_update();
}

void View::del_cursor(Cursor *cursor) {
  assert(cursor);

  get_cursorList().remove(cursor);
  delete cursor;
  make_cursors_order();

  cursor_update();
}

void View::clear_cursors() {
  auto &lst = get_cursorList();
  for (auto c : lst) {
    delete c;
  }

  lst.clear();
}

void View::add_xcursor(double value0, double value1) {
  static int lastXCursorOrder = 1;
  XCursor *newXCursor = new XCursor(*this, lastXCursorOrder++, value0, value1);
  _xcursorList.push_back(newXCursor);
  make_cursors_order();
  xcursor_update();
}

void View::del_xcursor(XCursor *xcursor) {
  assert(xcursor);

  _xcursorList.remove(xcursor);
  delete xcursor;
  make_cursors_order();
  xcursor_update();
}

void View::set_cursor_middle(int index) {
  auto &lst = get_cursorList();
  int size = lst.size();
  (void)size;
  assert(index < size);

  int width = get_view_width();
  // if (width > 0);

  auto i = lst.begin();

  while (index-- != 0) {
    i++;
  }

  set_scale_offset(
      _scale, (*i)->index() / (document_snapshot_source()->cur_snap_samplerate() *
                               _scale) -
                  (width / 2));
}

void View::on_measure_updated() {
  _active_viewport = dynamic_cast<Viewport *>(sender());
  measure_updated();
}

QString View::get_measure(QString option) {
  if (_active_viewport) {
    return _active_viewport->get_measure(option);
  }
  return Unknown_Str;
}

QString View::get_cm_time(int index) {
  uint64_t sampleIndex = get_cursor_samples(index);
  uint64_t sampleRate = document_snapshot_source()->cur_snap_samplerate();
  return _ruler->format_real_time(sampleIndex, sampleRate);
}

QString View::get_cm_delta(int index1, int index2) {
  if (index1 == index2)
    return "0";

  uint64_t samples1 = get_cursor_samples(index1);
  uint64_t samples2 = get_cursor_samples(index2);
  uint64_t delta_sample =
      (samples1 > samples2) ? samples1 - samples2 : samples2 - samples1;
  return _ruler->format_real_time(
      delta_sample, document_snapshot_source()->cur_snap_samplerate());
}

QString View::get_index_delta(uint64_t start, uint64_t end) {
  if (start == end)
    return "0";

  uint64_t delta_sample = (start > end) ? start - end : end - start;
  return _ruler->format_real_time(
      delta_sample, document_snapshot_source()->cur_snap_samplerate());
}

uint64_t View::get_cursor_samples(int index) {
  auto &lst = get_cursorList();
  assert(index < (int)lst.size());

  uint64_t ret = 0;
  int curIndex = 0;
  for (list<Cursor *>::iterator i = lst.begin(); i != lst.end(); i++) {
    if (index == curIndex) {
      ret = (*i)->index();
    }
    curIndex++;
  }
  return ret;
}

void View::set_measure_en(int enable) {
  _time_viewport->set_measure_en(enable);
  _fft_viewport->set_measure_en(enable);
}

void View::on_state_changed(bool stop) {
  if (stop) {
    _time_viewport->stop_trigger_timer();
    _fft_viewport->stop_trigger_timer();
  }
  update_scale_offset();
}

QRect View::get_view_rect() {
  if (get_work_mode() == DSO) {
    const auto &sigs = _own_signals;
    if (sigs.size() > 0) {
      return sigs[0]->get_view_rect();
    }
  }

  return _viewcenter->rect();
}

int View::get_work_mode() const {
  if (_document && _document->has_signal_config()) {
    return _document->get_signal_config().work_mode;
  }
  return _device_agent->get_work_mode();
}

int View::get_view_width() {
  int view_width = 0;
  if (get_work_mode() == DSO) {
    for (auto s : _own_signals) {
      view_width = max(view_width, s->get_view_rect().width());
    }
  } else {
    view_width = _viewcenter->width();
  }

  if (view_width == 0) {
    view_width = 1;
  }

  return view_width;
}

int View::get_view_height() {
  int view_height = 0;
  if (get_work_mode() == DSO) {
    for (auto s : _own_signals) {
      view_height = max(view_height, s->get_view_rect().height());
    }
  } else {
    view_height = _time_viewport ? _time_viewport->height() : 0;
  }

  return view_height;
}

int64_t View::get_min_offset() {
  int width = get_view_width();
  assert(width > 0);

  if (MaxViewRate > 1)
    return floor(width * (1 - MaxViewRate));
  else
    return 0;
}

int64_t View::get_max_offset() {
  int width = get_view_width();
  assert(width > 0);

  return ceil((document_snapshot_source()->cur_snap_sampletime() / _scale) -
              (width * MaxViewRate));
}

int64_t View::get_logic_lst_data_offset() {
  int width = get_view_width();
  assert(width > 0);

  return ceil((_session->get_logic_data_view_time() / _scale) -
              (width * MaxViewRate));
}

void View::scroll_to_logic_last_data_time() {
  set_scale_offset(scale(), get_logic_lst_data_offset() + 10);
}

// -- calibration dialog
void View::show_calibration() {
  if (_cali != NULL) {
    _cali->deleteLater();
    _cali = NULL;
  }

  _cali = new pv::dialogs::Calibration(_session, this);
  connect(_cali, &pv::dialogs::Calibration::sig_closed, this,
          &View::on_calibration_closed);
  _cali->update_device_info();
  _cali->show();
}

void View::on_calibration_closed() {
  if (_cali != NULL) {
    _cali->deleteLater();
    _cali = NULL;
  }
}

void View::hide_calibration() { on_calibration_closed(); }

void View::vDial_updated() {
  if (_cali != NULL) {
    _cali->update_device_info();
  }

  auto math_trace = get_own_math_trace();
  if (math_trace && math_trace->enabled()) {
    math_trace->update_vDial();
  }
}

void View::dso_factor_updated() {
  auto math_trace = get_own_math_trace();
  if (math_trace && math_trace->enabled()) {
    math_trace->update_vDial();
  }
}

// -- lissajous figure
void View::show_lissajous(bool show) {
  _show_lissajous = show;
  signals_changed(NULL);
}

void View::show_region(uint64_t start, uint64_t end, bool keep) {
  assert(start <= end);

  int width = get_view_width();
  if (width == 0) {
    return;
  }

  if (keep) {
    set_all_update(true);
    update();
  } else if (_session->get_map_zoom() == 0) {
    const double ideal_scale = (end - start) * 2.0 /
                               document_snapshot_source()->cur_snap_samplerate() /
                               width;
    const double new_scale = max(min(ideal_scale, _maxscale), _minscale);
    const double new_off =
        (start + end) * 0.5 /
            (document_snapshot_source()->cur_snap_samplerate() * new_scale) -
        (width / 2);
    set_scale_offset(new_scale, new_off);
  } else {
    const double new_scale = scale();
    const double new_off =
        (start + end) * 0.5 /
            (document_snapshot_source()->cur_snap_samplerate() * new_scale) -
        (width / 2);
    set_scale_offset(new_scale, new_off);
  }
}

void View::viewport_update() {
  // Mark decode pixmap dirty so it will be rebuilt on next paint.
  // This is needed because decode data can change independently of
  // view parameters (e.g. new decode data arriving).
  if (_time_viewport)
    _time_viewport->set_decode_dirty();

  _viewcenter->update();
  for (QWidget *viewport : _viewport_list)
    viewport->update();
}

void View::splitterMoved(int pos, int index) {
  (void)pos;
  (void)index;
  signals_changed(NULL);
}

void View::reload() {
  clear();

  /*
   * if headerwidth not change, viewport height will not be updated
   * lead to a wrong signal height
   */
  reconstruct();
}

void View::clear() {
  show_trig_cursor(false);

  if (get_work_mode() != DSO) {
    show_xcursors(false);
  } else {
    if (!get_xcursorList().empty())
      show_xcursors(true);
  }
}

void View::reconstruct() {
  if (get_work_mode() == DSO)
    _viewbottom->setFixedHeight(DsoStatusHeight);
  else
    _viewbottom->setFixedHeight(StatusHeight);
  _viewbottom->setGeometry(0, _time_viewport->height() - _viewbottom->height(),
                           _time_viewport->width(), _viewbottom->height());
  _viewbottom->reload();
}

void View::repeat_show() { _viewbottom->update(); }

void View::show_captured_progress(bool triggered, int progress) {
  _viewbottom->set_capture_status(triggered, progress);
  _viewbottom->update();
}

bool View::get_dso_trig_moved() { return _time_viewport->get_dso_trig_moved(); }

double View::index2pixel(uint64_t index, bool has_hoff) {
  const uint64_t rateValue = document_snapshot_source()->cur_snap_samplerate();
  const double scaleValue = scale();
  const int64_t offsetValue = offset();
  const double hoffValue = trig_hoff();

  double pixels = 0;

  const double samples_per_pixel = rateValue * scaleValue;

  if (has_hoff) {
    pixels =
        index / samples_per_pixel - offsetValue + hoffValue / samples_per_pixel;
  } else {
    pixels = index / samples_per_pixel - offsetValue;
  }

  /*
  const double samples_per_pixel = _data_source->cur_snap_samplerate() *
  scale(); double pixels; if (has_hoff) pixels = index/samples_per_pixel -
  offset() + trig_hoff()/samples_per_pixel; else pixels =
  index/samples_per_pixel - offset();
      */

  return pixels;
}

uint64_t View::pixel2index(double pixel) {
  const uint64_t rateValue = document_snapshot_source()->cur_snap_samplerate();
  const double scaleValue = scale();
  const int64_t offsetValue = offset();
  const double hoffValue = trig_hoff();

  const double samples_per_pixel = rateValue * scaleValue;
  const double index = (pixel + offsetValue) * samples_per_pixel - hoffValue;

  const uint64_t sampleIndex = (uint64_t)std::round(index);

  return sampleIndex;

  // const double samples_per_pixel = session().cur_snap_samplerate() * scale();
  // uint64_t index = (pixel + offset()) * samples_per_pixel - trig_hoff();
}

void View::set_receive_len(uint64_t len) {
  if (_time_viewport)
    _time_viewport->set_receive_len(len);

  if (_fft_viewport && _session->get_device()->get_work_mode() == DSO)
    _fft_viewport->set_receive_len(len);
}

int View::get_cursor_index_by_key(uint64_t key) {
  auto &lst = get_cursorList();

  int dex = 0;
  for (auto c : lst) {
    if (c->get_key() == key) {
      return dex;
    }
    ++dex;
  }
  return -1;
}

void View::rebuild_signals_from_config(const data::SignalConfig &config) {
  // Re-entrancy guard: if a nested broadcast (e.g. DSV_MSG_DEVICE_OPTIONS_UPDATED
  // from within this function) triggers OnMessage → rebuild_signals() →
  // rebuild_signals_from_config() again, abort immediately to prevent
  // infinite recursion / stack overflow.
  if (_rebuild_in_progress)
    return;
  _rebuild_in_progress = true;
  struct RebuildGuard {
    bool &flag;
    RebuildGuard(bool &f) : flag(f) {}
    ~RebuildGuard() { flag = false; }
  } _rebuild_guard(_rebuild_in_progress);

  qDebug() << "View::rebuild_signals_from_config() work_mode="
           << config.work_mode << "ch_count=" << config.channels.size()
           << "is_valid=" << config.is_valid;

  std::vector<Signal *> old_signals = _own_signals;
  _own_signals.clear();

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
      model->set_type(api::ChannelType::Logic);
      break;
    case DSO:
      model->set_type(api::ChannelType::Dso);
      model->set_vdiv(ch.vdiv);
      model->set_coupling(ch.coupling);
      model->set_hw_offset(ch.hw_offset);
      model->set_vertical_offset(ch.offset);
      model->set_zero_offset(ch.zero_offset);
      break;
    case ANALOG:
      model->set_type(api::ChannelType::Analog);
      model->set_vdiv(ch.vdiv);
      model->set_coupling(ch.coupling);
      model->set_hw_offset(ch.hw_offset);
      model->set_vertical_offset(ch.offset);
      model->set_zero_offset(ch.zero_offset);
      break;
    }

    // Set session for the model (so it can call session methods if needed)
    model->set_session(_session);

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
                                 nullptr, model, _session);
      } else {
        signal = new LogicSignal(nullptr, model, _session);
      }
      break;
    case DSO:
      if (old_signal) {
        signal = new DsoSignal(static_cast<DsoSignal *>(old_signal), nullptr,
                               model, _session);
      } else {
        signal = new DsoSignal(nullptr, model, _session);
      }
      break;
    case ANALOG:
      if (old_signal) {
        signal = new AnalogSignal(static_cast<AnalogSignal *>(old_signal),
                                  nullptr, model, _session);
      } else {
        signal = new AnalogSignal(nullptr, model, _session);
      }
      break;
    }

    if (signal) {
      signal->set_enabled(ch.enabled);
      // Use persisted visibility from SignalConfig (saved at deactivate time).
      // For newly created channels not yet in the config, default visible
      // to enabled.
      signal->set_visible(ch.visible);

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
      _own_signals.push_back(signal);
    }
  }

  for (auto sig : old_signals)
    delete sig;

  if (_document && _document->has_data()) {
    for (auto sig : _own_signals) {
      int type = sig->signal_type();
      switch (type) {
      case SR_CHANNEL_LOGIC: {
        view::LogicSignal *s = static_cast<view::LogicSignal *>(sig);
        s->set_data(_document->get_active_logic());
        break;
      }
      case SR_CHANNEL_ANALOG: {
        view::AnalogSignal *s = static_cast<view::AnalogSignal *>(sig);
        s->set_data(_document->get_active_analog());
        break;
      }
      case SR_CHANNEL_DSO: {
        view::DsoSignal *s = static_cast<view::DsoSignal *>(sig);
        s->set_data(_document->get_active_dso());
        break;
      }
      }
    }
  }

  signals_changed(NULL);
}

void View::rebuild_signals() {
  mark_derived_traces_dirty();

  if (_data_source == _document && _document &&
      _document->has_signal_config()) {
    const auto &config = _document->get_signal_config();
    // 检查配置的通道数是否与设备当前的通道数匹配
    // 如果不匹配，说明通道模式已切换，需要从设备重新创建信号
    int device_ch_count = 0;
    for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
      device_ch_count++;
    }
    if (config.channels.size() == (size_t)device_ch_count) {
      rebuild_signals_from_config(config);
      SignalFactory::update_signals(_own_signals, _session, _session,
                                    SignalFactory::Modified);
      // Only property changes, no layout needed - use incremental refresh
      signals_modified_refresh();
      return;
    }
  }

  if (!_data_source)
    return;

  auto created_sigs = SignalFactory::create_signals(_data_source, _session);
  if (created_sigs.empty())
    return;

  for (auto sig : _own_signals)
    delete sig;
  _own_signals.clear();

  for (auto sig : created_sigs) {
    // create_signals 新建的信号已使用 Trace 构造函数的默认高度，
    // 无需在此二次重置。DSO/Analog 的自动高度由 set_data_document 路径处理。
    _own_signals.push_back(sig);
  }

  for (auto sig : _own_signals) {
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
  if (_document && _document->has_signal_config()) {
    const auto &cfg = _document->get_signal_config();
    int view_index_seq = 0;
    for (auto *sig : _own_signals) {
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

  if (_document && _document->has_data()) {
    set_data_document(_document);
  }

  signals_changed(NULL);
}

bool View::add_decoder(srd_decoder *const dec, bool silent,
                       DecoderStatus *dstatus,
                       std::list<pv::data::decode::Decoder *> &sub_decoders,
                       std::shared_ptr<pv::data::DecoderStack> &out_stack) {
  if (!_session)
    return false;

  out_stack = nullptr;

  // 1. Core layer creates the DecoderStack and adds it to the active
  //    document's stack list. Core owns the DecoderStack.
  if (!_session->add_decoder(dec, silent, dstatus, sub_decoders, out_stack))
    return false;

  if (!out_stack)
    return false;

  // 2. View directly creates its DecodeTrace wrapper for the new
  //    DecoderStack. We do NOT rely on the signals_changed event callback
  //    (which would only lazily sync via sync_derived_traces()) because the
  //    caller may need to interact with the DecodeTrace immediately after
  //    this method returns (e.g. to attach protocol layer items).
  //    The index used here is the position in the View's own list, matching
  //    the pattern in sync_derived_traces().
  int decode_index = (int)_own_decode_traces.size();
  auto *trace = new DecodeTrace(_session, out_stack, decode_index);
  // Set initial view_index: place after all signal tracks (same pattern
  // as sync_derived_traces). Without this, view_index defaults to -1 and
  // causes incorrect layout ordering in LOGIC mode.
  trace->set_view_index((int)_own_signals.size() + decode_index);
  // CRITICAL: set_view(this) must be called BEFORE create_popup() because
  // the dialog accesses _trace->get_view() to get session and signal_models.
  trace->set_view(this);

  // 3. If silent is false, show the decoder options dialog so the user can
  //    configure channel mappings, decode range, etc. This was previously
  //    done inside SigSession::add_decoder() via DecodeTrace::create_popup();
  //    it was moved here during de-view-ization because Core must not depend
  //    on Qt Widgets. If the user cancels (or fails to set required probes),
  //    roll back: delete the DecodeTrace and ask Core to delete the
  //    DecoderStack so we don't leave an unconfigured decoder around.
  pxv_info("View: before create_popup(true), silent=%d", silent);
  if (!silent) {
    bool settings_changed = trace->create_popup(true);
    pxv_info("View: create_popup returned %d", settings_changed);
    if (!settings_changed) {
      delete trace;
      void *key = out_stack->get_key_handel();
      _session->remove_decoder_by_key_handel(key);
      out_stack = nullptr;
      pxv_info("View: rollback complete, returning false");
      return false;
    }
  }

  _own_decode_traces.push_back(trace);

  // 4. Mark derived traces NOT dirty since we just synced the DecodeTrace
  //    list manually. This prevents sync_derived_traces() from recreating
  //    the DecodeTrace we just added.
  _derived_traces_dirty = false;

  // 5. Broadcast first so that SigSession::OnMessage can run reload() before
  //    we start the decode task. The broadcast is synchronous (direct function
  //    call, not Qt queued signal), so reload() will clear SignalModel snapshots
  //    immediately. We then call add_decode_task() which internally calls
  //    attach_data_to_signal() to restore the snapshot pointers. If we called
  //    add_decode_task() before broadcast_msg(), the snapshot would be set and
  //    then cleared by reload(), causing "没有设置需要解码哪些通道的数据".
  //    This broadcast also notifies MCP/WebSocket clients (SessionService maps
  //    DSV_MSG_DEVICE_OPTIONS_UPDATED to DeviceConfigChanged).
  _session->broadcast_msg(DSV_MSG_DEVICE_OPTIONS_UPDATED);

  // 6. Now start the decode task after reload() has completed. The public
  //    start_all_decode_tasks() funnel calls attach_data_to_signal() before
  //    starting tasks, restoring snapshot pointers. If there is no view data
  //    yet (decoder added before capture), the capture pipeline will start
  //    the decode for us via DSV_MSG_COPY_TO_DOC_DONE → frame_ended() +
  //    start_all_decode_tasks().
  if (!silent && _session->have_view_data()) {
    _session->start_all_decode_tasks();
  }

  // 7. Refresh layout. signals_changed(NULL) calls mark_derived_traces_dirty()
  //    at the top, but since the DecodeTrace list is already in sync, the
  //    subsequent sync_derived_traces() will be a no-op for decoders.
  signals_changed(NULL);

  return true;
}

bool View::rst_decoder_by_key_handel(void *handel) {
  if (!_session || !handel)
    return false;

  // Find the View-owned DecodeTrace that wraps this DecoderStack.
  auto find_trace = [&]() -> DecodeTrace * {
    for (auto *trace : _own_decode_traces) {
      if (trace && trace->decoder() &&
          trace->decoder()->get_key_handel() == handel)
        return trace;
    }
    return nullptr;
  };

  DecodeTrace *target = find_trace();

  // Fall back to lazy sync if not found (the list might be dirty).
  if (!target) {
    sync_derived_traces();
    target = find_trace();
  }

  if (!target)
    return false;

  // Re-open the options dialog. If the user cancels (no settings change),
  // do NOT reset the decoder — keep the existing configuration. This
  // restores the pre-de-view-ization behavior where SigSession bailed out
  // of rst_decoder() when create_popup() returned false.
  bool settings_changed = target->create_popup(false);
  if (!settings_changed)
    return false;

  // Forward to Core to clear the existing decode task and re-add it.
  _session->rst_decoder_by_key_handel(handel);
  return true;
}

void View::remove_decoder(DecodeTrace *trace) {
  if (!trace)
    return;

  auto it =
      std::find(_own_decode_traces.begin(), _own_decode_traces.end(), trace);
  if (it == _own_decode_traces.end())
    return;

  auto stack = trace->decoder();
  void *key_handel = stack ? stack->get_key_handel() : nullptr;

  // 1. View deletes its DecodeTrace (View-owned). The DecodeTrace
  //    destructor does NOT delete the DecoderStack (Core owns it).
  _own_decode_traces.erase(it);
  delete trace;

  // 2. Notify Core layer to delete the corresponding DecoderStack.
  //    Core's remove_decoder_by_key_handel() will remove the stack from
  //    its list and delete it (immediately if no decode thread holds it,
  //    otherwise asynchronously via _delete_flag). Core fires
  //    signals_changed() callback which triggers View::signals_changed(NULL)
  //    — at that point sync_derived_traces() will find no DecodeTrace to
  //    remove (we already deleted it), so no double-free occurs.
  if (key_handel) {
    _session->remove_decoder_by_key_handel(key_handel);
  }

  // 3. Broadcast so the API layer can push a ServiceEvent to remote clients.
  //    View cannot call SessionService::broadcast_event directly (View does
  //    not depend on the API layer), so we forward via
  //    SigSession::broadcast_msg. There is no dedicated
  //    DSV_MSG_DECODER_REMOVED; DSV_MSG_DEVICE_OPTIONS_UPDATED is mapped by
  //    SessionService to DeviceConfigChanged, which triggers state
  //    synchronization. (The MCP remove_analyzer path already broadcasts
  //    DecoderRemoved directly; this covers the GUI-triggered path.)
  _session->broadcast_msg(DSV_MSG_DEVICE_OPTIONS_UPDATED);
}

void View::remove_decoder(int index) {
  if (index < 0 || index >= (int)_own_decode_traces.size())
    return;
  remove_decoder(_own_decode_traces[index]);
}

void View::remove_decoder_by_key_handel(void *key_handel) {
  if (!_session || !key_handel)
    return;

  // Find the View-owned DecodeTrace that wraps the DecoderStack with this
  // key_handel. This mirrors the pattern in rst_decoder_by_key_handel().
  auto find_trace = [&]() -> DecodeTrace * {
    for (auto *trace : _own_decode_traces) {
      if (trace && trace->decoder() &&
          trace->decoder()->get_key_handel() == key_handel)
        return trace;
    }
    return nullptr;
  };

  DecodeTrace *target = find_trace();

  // Fall back to lazy sync if not found (the list might be dirty).
  if (!target) {
    sync_derived_traces();
    target = find_trace();
  }

  if (!target)
    return;

  remove_decoder(target);
}

void View::clear_all_decoders() {
  if (!_session)
    return;

  // 1. Delete all View-owned DecodeTrace objects first.
  for (auto *trace : _own_decode_traces) {
    delete trace;
  }
  _own_decode_traces.clear();

  // 2. Notify Core to clear all DecoderStacks.
  //    Core's clear_all_decoder() will fire signals_changed() callback
  //    which triggers View::signals_changed(), but since we already
  //    deleted all DecodeTrace, the subsequent sync_derived_traces()
  //    will be a no-op.
  _session->clear_all_decoder(true);

  // 3. Broadcast so the API layer can push a ServiceEvent to remote clients.
  _session->broadcast_msg(DSV_MSG_DEVICE_OPTIONS_UPDATED);
}

void View::on_signals_changed() {
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

  if (!_data_source)
    return;

  auto &models = _data_source->get_signal_models();
  auto event = SignalFactory::compute_change_event(_own_signals, models);

  SignalFactory::update_signals(_own_signals, _data_source, _session, event);

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

void View::signals_added_layout() {
  // Layout recalculation is O(N) but relatively cheap compared to object
  // recreation. SignalFactory::update_signals(Added) already added the new
  // Signal objects without recreating existing ones.
  signals_changed(NULL);
}

void View::signals_removed_layout() {
  // Layout recalculation is O(N) but relatively cheap compared to object
  // recreation. SignalFactory::update_signals(Removed) already removed the
  // Signal objects without recreating existing ones.
  signals_changed(NULL);
}

void View::signals_modified_refresh() {
  // Only property changes, no layout changes needed.
  // Just repaint the signals without calling signals_changed(NULL).
  viewport_update();
  header_updated();
}

void View::check_calibration() {
  if (get_work_mode() == DSO) {
    bool cali = false;
    _device_agent->get_config_bool(SR_CONF_CALI, cali);

    if (cali) {
      show_calibration();
    }
  }
}

void View::set_scale(double scale) {
  if (scale < _minscale)
    scale = _minscale;
  if (scale > _maxscale)
    scale = _maxscale;

  if (_scale != scale) {
    _scale = scale;
    _header->update();
    _ruler->update();
    viewport_update();
    update_scroll();
  }
}

void View::auto_set_max_scale() {
  const double limitTime = document_snapshot_source()->cur_sampletime();
  const int width = get_view_width();

  if (width > 0) {
    _maxscale = limitTime / (width * MaxViewRate);
    set_scale(_maxscale);
  }
}

int View::get_body_width() {
  if (_time_viewport != NULL)
    return _time_viewport->width();
  return 0;
}

int View::get_body_height() {
  if (_time_viewport != NULL)
    return _time_viewport->height();
  return 0;
}

void View::update_view_port() {
  if (_time_viewport)
    _time_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
}

void View::update_font() { headerWidth(); }

void View::check_measure() {
  _time_viewport->measure();
  _time_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
}

std::list<Cursor *> &View::get_cursorList() {
  if (_session->get_device()->get_work_mode() == LOGIC) {
    return _logic_cursors;
  } else {
    return _dso_cursors;
  }
}

bool View::header_is_draging() { return _header->mouse_is_down(); }

Cursor *View::get_cursor_by_index(int index) {
  int dex = 0;
  auto &cursors = get_cursorList();

  for (auto c : cursors) {
    if (dex == index) {
      return c;
    }
    dex++;
  }
  return NULL;
}

void View::UpdateLanguage() {}

void View::UpdateTheme() {
  refreshSignalColors();

  QString heightStr =
      AppConfig::Instance().GetThemeTokenValue("@logic-channel-height");
  bool ok;
  int h = heightStr.toInt(&ok);
  if (ok && h > 0) {
    _signalHeightScale = h;
    _signalHeight = h;

    std::vector<Trace *> traces;
    get_traces(ALL_VIEW, traces);
    for (Trace *t : traces) {
      if (t && (t->get_type() == SR_CHANNEL_LOGIC ||
                t->get_type() == SR_CHANNEL_GROUP)) {
        t->set_totalHeight(h);
        t->set_own_height(h);
      }
    }
    update_all_trace_postion();
  }

  viewport_update();
}

void View::UpdateFont() { update_font(); }

bool View::view_is_ready() {
  int w = get_view_width();
  return w > 0;
}

void View::mark_derived_traces_dirty() { _derived_traces_dirty = true; }

void View::sync_derived_traces() {
  if (!_derived_traces_dirty)
    return;

  _derived_traces_dirty = false;

  auto *source = document_snapshot_source();
  if (!source)
    return;

  // ---- Sync DecodeTrace list from DecoderStack list ----
  auto &decoder_stacks = source->get_decoder_stacks();

  // Remove DecodeTrace whose DecoderStack no longer exists.
  for (auto it = _own_decode_traces.begin(); it != _own_decode_traces.end();) {
    DecodeTrace *dt = *it;
    auto target = dt->decoder().get();
    auto it_stack = std::find_if(
        decoder_stacks.begin(), decoder_stacks.end(),
        [target](const std::shared_ptr<pv::data::DecoderStack> &s) {
          return s.get() == target;
        });
    if (it_stack == decoder_stacks.end()) {
      delete dt;
      it = _own_decode_traces.erase(it);
    } else {
      ++it;
    }
  }

  // Add DecodeTrace for new DecoderStacks that have no wrapper yet.
  int decode_index = 0;
  for (auto stack : decoder_stacks) {
    bool exists = false;
    for (auto *dt : _own_decode_traces) {
      if (dt->decoder().get() == stack.get()) {
        exists = true;
        break;
      }
    }
    if (!exists) {
      auto *dt = new DecodeTrace(_session, stack, decode_index);
      // Sync _visible from Decoder::_shown so that a decoder the user
      // previously hid stays hidden after tab switch (sync_derived_traces
      // is called lazily when the DecodeTrace list is accessed).
      if (!stack->stack().empty() && !stack->stack().front()->shown()) {
        dt->set_visible(false);
      }
      // Set initial view_index: place after all signal tracks.
      // Without this, view_index defaults to -1 (trace.h:81) and
      // causes incorrect layout ordering in LOGIC mode signals_changed.
      dt->set_view_index((int)_own_signals.size() + decode_index);
      _own_decode_traces.push_back(dt);
    }
    decode_index++;
  }

  // ---- Sync SpectrumTrace list from SpectrumStack list ----
  auto &spectrum_stacks = source->get_spectrum_stacks();

  // Remove SpectrumTrace whose SpectrumStack no longer exists.
  for (auto it = _own_spectrum_traces.begin();
       it != _own_spectrum_traces.end();) {
    SpectrumTrace *st = *it;
    auto target = st->get_spectrum_stack().get();
    auto it_stack = std::find_if(
        spectrum_stacks.begin(), spectrum_stacks.end(),
        [target](const std::shared_ptr<pv::data::SpectrumStack> &s) {
          return s.get() == target;
        });
    if (it_stack == spectrum_stacks.end()) {
      delete st;
      it = _own_spectrum_traces.erase(it);
    } else {
      ++it;
    }
  }

  // Add SpectrumTrace for new SpectrumStacks that have no wrapper yet.
  for (auto stack : spectrum_stacks) {
    bool exists = false;
    for (auto *st : _own_spectrum_traces) {
      if (st->get_spectrum_stack().get() == stack.get()) {
        exists = true;
        break;
      }
    }
    if (!exists) {
      auto *st = new SpectrumTrace(_session, stack, stack->get_index());
      _own_spectrum_traces.push_back(st);
    }
  }

  // ---- Sync MathTrace from MathStack ----
  auto math_stack = source->get_math_stack();
  if (math_stack) {
    if (!_own_math_trace ||
        _own_math_trace->get_math_stack().get() != math_stack.get()) {
      // Tear down any stale MathTrace bound to a previous MathStack.
      if (_own_math_trace) {
        delete _own_math_trace;
        _own_math_trace = nullptr;
      }

      // MathStack now exposes the source channel indices. Look up the
      // matching DsoSignal instances in _own_signals so the MathTrace can
      // read DSO-specific UI state (ref_min/ref_max, view rect, etc.).
      DsoSignal *dso1 = nullptr;
      DsoSignal *dso2 = nullptr;
      const int idx1 = math_stack->ch1_index();
      const int idx2 = math_stack->ch2_index();
      for (auto *sig : _own_signals) {
        if (!sig || sig->signal_type() != SR_CHANNEL_DSO)
          continue;
        if (sig->get_index() == idx1)
          dso1 = dynamic_cast<DsoSignal *>(sig);
        if (sig->get_index() == idx2)
          dso2 = dynamic_cast<DsoSignal *>(sig);
        if (dso1 && dso2)
          break;
      }

      if (dso1 && dso2) {
        _own_math_trace = new MathTrace(true, math_stack, dso1, dso2);
      } else {
        pxv_warn("View::sync_derived_traces: DsoSignal not found for "
                 "math src1=%d or src2=%d — MathTrace creation skipped.",
                 idx1, idx2);
      }
    }
  } else {
    if (_own_math_trace) {
      delete _own_math_trace;
      _own_math_trace = nullptr;
    }
  }

  // ---- Sync LissajousTrace from LissajousModel ----
  auto *lissajous_model = source->get_lissajous_model();
  if (lissajous_model && lissajous_model->enabled()) {
    if (!_own_lissajous_trace) {
      auto *snapshot = source->get_dso_snapshot();
      _own_lissajous_trace = new LissajousTrace(
          lissajous_model->enabled(), snapshot, lissajous_model->x_index(),
          lissajous_model->y_index(), lissajous_model->percent());
    }
  } else {
    if (_own_lissajous_trace) {
      delete _own_lissajous_trace;
      _own_lissajous_trace = nullptr;
    }
  }
}

} // namespace view
} // namespace pv
