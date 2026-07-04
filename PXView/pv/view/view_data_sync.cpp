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

// Phase J (modernize-view-layer-v3): data-source / data-document / capture
// lifecycle data-sync behaviour extracted from the View God-class.
// ViewDataSync is declared a friend of View so it can touch the private
// data-sync state (_data_source / _document / _own_signals /
// _own_lissajous_trace / _time_viewport / _fft_viewport / _device_agent /
// _viewbottom / _data_updated_timer / _search_hit / _search_pos /
// _session) directly. Cross-method calls that remain on View (e.g.
// mark_derived_traces_dirty, rebuild_signals, document_snapshot_source,
// get_work_mode, limit_scale_offset, set_trig_cursor_posistion,
// set_search_pos, set_update, headerWidth, update_margins, update_scroll,
// update_scale_offset, viewport_update) go through _view->… so the public
// View API is unchanged.

#include "view_data_sync.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <QEvent>
#include <QMouseEvent>
#include <QObject>
#include <QResizeEvent>
#include <QtGlobal>

#include "view.h"

#include "../config/appconfig.h"
#include "../data/datasource.h"
#include "../data/sessiondocument.h"
#include "../data/signalmodel.h"
#include "../dialogs/calibration.h"
#include "../dsvdef.h"
#include "../sigsession.h"
#include "../toolbars/samplingbar.h"

#include "analogsignal.h"
#include "dsosignal.h"
#include "header.h"
#include "logicsignal.h"
#include "lissajoustrace.h"
#include "mathtrace.h"
#include "ruler.h"
#include "signal.h"
#include "signalfactory.h"
#include "viewport.h"
#include "viewstatus.h"

using namespace std;

namespace pv {
namespace view {

void ViewDataSync::set_data_source(pv::data::DataSource *source) {
  _view->_data_source = source;
  _view->mark_derived_traces_dirty();
  _view->rebuild_signals();

  if (_view->_time_viewport) {
    _view->_time_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
  }
  if (_view->_fft_viewport) {
    _view->_fft_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
  }
  _view->update();
}

void ViewDataSync::clear_signal_data() {
  for (auto sig : _view->_own_signals) {
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

  if (_view->_time_viewport) {
    _view->_time_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
  }
  if (_view->_fft_viewport) {
    _view->_fft_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
  }
  _view->update();
}

void ViewDataSync::set_signal_data_from_source(
    pv::data::DataSource *source) {
  for (auto sig : _view->_own_signals) {
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

  if (_view->_time_viewport) {
    _view->_time_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
  }
  if (_view->_fft_viewport) {
    _view->_fft_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
  }
  _view->update();
}

void ViewDataSync::set_data_document(pv::data::SessionDocument *doc) {
  // A2 fix: handle nullptr to detach the document pointer. Without this, the
  // early return left _document pointing at a soon-to-be-destroyed object,
  // causing use-after-free when the View received paint events before its
  // deleteLater() was processed.
  if (!doc) {
    _view->_document = nullptr;
    // Clear signal data pointers so paint events don't dereference freed data.
    for (auto sig : _view->_own_signals) {
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
    return;
  }

  _view->_document = doc;
  _view->mark_derived_traces_dirty();

  if (!doc->has_data())
    return;

  if (_view->_own_signals.empty()) {
    auto created_sigs =
        SignalFactory::create_signals(_view->_data_source, _view->_data_source);
    for (auto sig : created_sigs) {
      _view->_own_signals.push_back(sig);
    }
  }

  for (auto sig : _view->_own_signals) {
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

  if (_view->_time_viewport) {
    _view->_time_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
  }
  if (_view->_fft_viewport) {
    _view->_fft_viewport->update(UpdateEventType::UPDATE_EV_GENERIC);
  }
  _view->update();
}

void ViewDataSync::clone_signals_for_document(
    pv::data::SessionDocument *doc) {
  if (!doc)
    return;

  _view->_own_signals.clear();

  auto created_sigs =
      SignalFactory::create_signals(_view->_data_source, _view->_data_source);
  for (auto sig : created_sigs) {
    _view->_own_signals.push_back(sig);
  }

  set_data_document(doc);
}

pv::data::DataSource *ViewDataSync::document_snapshot_source() {
  if (_view->_document && _view->_document->has_data())
    return _view->_document;
  return _view->_data_source;
}

void ViewDataSync::frame_began() {
  _view->_search_hit = false;
  _view->_search_pos = 0;
  _view->set_search_pos(_view->_search_pos, _view->_search_hit);
}

void ViewDataSync::receive_end() {
  if (_view->get_work_mode() == LOGIC) {
    bool rle = false;
    uint64_t actual_samples;
    bool ret;

    ret = _view->_device_agent->get_config_bool(SR_CONF_RLE, rle);

    if (ret && rle) {
      ret = _view->_device_agent->get_config_uint64(SR_CONF_ACTUAL_SAMPLES,
                                                     actual_samples);
      if (ret) {
        if (actual_samples !=
            _view->document_snapshot_source()->cur_samplelimits()) {
          _view->_viewbottom->set_rle_depth(actual_samples);
        }
      }
    }
  }
  _view->_time_viewport->unshow_wait_trigger();

  _view->limit_scale_offset();
}

void ViewDataSync::receive_trigger(quint64 trig_pos1) {
  (void)trig_pos1;
  uint64_t trig_pos = _view->document_snapshot_source()->get_trigger_pos();
  _view->set_trig_cursor_posistion(trig_pos);
}

void ViewDataSync::data_updated() {
  // Deduplicate rapid calls: if called within 16ms of the last execution,
  // only mark viewports dirty without doing full update cycle
  if (_view->_data_updated_timer.isValid() &&
      _view->_data_updated_timer.elapsed() < 16) {
    _view->set_update(_view->_time_viewport, true);
    _view->set_update(_view->_fft_viewport, true);
    return;
  }

  // Refresh data pointers in render objects (does NOT rebuild them).
  // Signals hold raw snapshot pointers that may become stale when the
  // active data source swaps its backing snapshots (e.g. after a capture,
  // glitch filter, or document switch). Re-bind the latest snapshots from
  // the effective data source.
  auto *source = _view->document_snapshot_source();
  if (source) {
    for (auto sig : _view->_own_signals) {
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
    if (_view->_own_lissajous_trace) {
      _view->_own_lissajous_trace->set_data(source->get_dso_snapshot());
    }
    // Note: DecodeTrace / SpectrumTrace / MathTrace wrap Core-owned
    // Stack objects which manage their own snapshot pointers internally,
    // so no explicit refresh is needed here.
  }

  _view->setViewportMargins(_view->headerWidth(), View::RulerHeight, 0, 0);
  _view->update_margins();

  // Update the scroll bars
  _view->update_scroll();

  // update scale & offset
  _view->update_scale_offset();

  // Repaint the view
  _view->_time_viewport->unshow_wait_trigger();
  _view->set_update(_view->_time_viewport, true);
  _view->set_update(_view->_fft_viewport, true);
  _view->viewport_update();
  _view->_ruler->update();

  _view->_data_updated_timer.start();
}

void ViewDataSync::set_receive_len(uint64_t len) {
  if (_view->_time_viewport)
    _view->_time_viewport->set_receive_len(len);

  if (_view->_fft_viewport && _view->_device_agent->get_work_mode() == DSO)
    _view->_fft_viewport->set_receive_len(len);
}

// =============================================================================
// Phase J additional: coordinate conversion / capture / scale / geometry
// =============================================================================

double ViewDataSync::index2pixel(uint64_t index, bool has_hoff) {
  const uint64_t rateValue = _view->document_snapshot_source()->cur_snap_samplerate();
  const double scaleValue = _view->scale();
  const int64_t offsetValue = _view->offset();
  const double hoffValue = _view->trig_hoff();

  double pixels = 0;
  const double samples_per_pixel = rateValue * scaleValue;

  if (has_hoff) {
    pixels =
        index / samples_per_pixel - offsetValue + hoffValue / samples_per_pixel;
  } else {
    pixels = index / samples_per_pixel - offsetValue;
  }

  return pixels;
}

uint64_t ViewDataSync::pixel2index(double pixel) {
  const uint64_t rateValue = _view->document_snapshot_source()->cur_snap_samplerate();
  const double scaleValue = _view->scale();
  const int64_t offsetValue = _view->offset();
  const double hoffValue = _view->trig_hoff();

  const double samples_per_pixel = rateValue * scaleValue;
  const double index = (pixel + offsetValue) * samples_per_pixel - hoffValue;

  const uint64_t sampleIndex = (uint64_t)std::round(index);

  return sampleIndex;
}

void ViewDataSync::capture_init() {
  int width = _view->get_view_width();
  if (width == 0) {
    return;
  }

  int mode = _view->get_work_mode();

  if (mode == DSO)
    _view->show_trig_cursor(true);
  else if (!_view->_data_source->is_repeating())
    _view->show_trig_cursor(false);

  double sampletime = _view->document_snapshot_source()->cur_sampletime();
  if (sampletime > 0) {
    _view->_maxscale = sampletime / (width * View::MaxViewRate);

    if (mode == ANALOG) {
      _view->set_scale_offset(_view->_maxscale, 0);
    }
  }

  _view->status_clear();

  _view->_trig_hoff = 0;
}

void ViewDataSync::show_region(uint64_t start, uint64_t end, bool keep) {
  assert(start <= end);

  int width = _view->get_view_width();
  if (width == 0) {
    return;
  }

  if (keep) {
    _view->set_all_update(true);
    _view->update();
  } else if (_view->_data_source->get_map_zoom() == 0) {
    const double ideal_scale = (end - start) * 2.0 /
                               _view->document_snapshot_source()->cur_snap_samplerate() /
                               width;
    const double new_scale = max(min(ideal_scale, _view->_maxscale), _view->_minscale);
    const double new_off =
        (start + end) * 0.5 /
            (_view->document_snapshot_source()->cur_snap_samplerate() * new_scale) -
        (width / 2);
    _view->set_scale_offset(new_scale, new_off);
  } else {
    const double new_scale = _view->scale();
    const double new_off =
        (start + end) * 0.5 /
            (_view->document_snapshot_source()->cur_snap_samplerate() * new_scale) -
        (width / 2);
    _view->set_scale_offset(new_scale, new_off);
  }
}

void ViewDataSync::timebase_changed() {
  int width = _view->get_view_width();
  if (width == 0) {
    return;
  }

  if (_view->get_work_mode() != DSO) {
    return;
  }

  double scale = _view->scale();
  double hori_res = _view->_sampling_bar->get_hori_res();

  if (hori_res > 0) {
    scale = _view->_data_source->cur_view_time() / width;
  }

  _view->set_scale_offset(scale, _view->offset());
}

void ViewDataSync::mode_changed() {
  if (_view->_device_agent->is_virtual()) {
    uint64_t samplerate = _view->document_snapshot_source()->cur_snap_samplerate();
    if (samplerate > 0)
      _view->set_scale_offset(View::WellSamplesPerPixel * 1.0 / samplerate, _view->_offset);
  }
  _view->set_scale_offset(max(min(_view->_scale, _view->_maxscale), _view->_minscale), _view->_offset);
}

void ViewDataSync::auto_set_max_scale() {
  const double limitTime = _view->document_snapshot_source()->cur_sampletime();
  const int width = _view->get_view_width();

  if (width > 0) {
    _view->_maxscale = limitTime / (width * View::MaxViewRate);
    _view->set_scale(_view->_maxscale);
  }
}

int ViewDataSync::get_view_width() {
  int view_width = 0;
  if (_view->get_work_mode() == DSO) {
    for (auto s : _view->_own_signals) {
      view_width = max(view_width, s->get_view_rect().width());
    }
  } else {
    view_width = _view->_viewcenter->width();
  }

  if (view_width == 0) {
    view_width = 1;
  }

  return view_width;
}

int ViewDataSync::get_view_height() {
  int view_height = 0;
  if (_view->get_work_mode() == DSO) {
    for (auto s : _view->_own_signals) {
      view_height = max(view_height, s->get_view_rect().height());
    }
  } else {
    view_height = _view->_time_viewport ? _view->_time_viewport->height() : 0;
  }

  return view_height;
}

QRect ViewDataSync::get_view_rect() {
  if (_view->get_work_mode() == DSO) {
    const auto &sigs = _view->_own_signals;
    if (sigs.size() > 0) {
      return sigs[0]->get_view_rect();
    }
  }

  return _view->_viewcenter->rect();
}

int64_t ViewDataSync::get_logic_lst_data_offset() {
  int width = _view->get_view_width();
  assert(width > 0);

  return ceil((_view->_data_source->get_logic_data_view_time() / _view->_scale) -
              (width * View::MaxViewRate));
}

void ViewDataSync::scroll_to_logic_last_data_time() {
  _view->set_scale_offset(_view->scale(), get_logic_lst_data_offset() + 10);
}

void ViewDataSync::show_calibration() {
  if (_view->_cali != NULL) {
    _view->_cali->deleteLater();
    _view->_cali = NULL;
  }

  _view->_cali = new pv::dialogs::Calibration(_view->_session, _view);
  QObject::connect(_view->_cali, &pv::dialogs::Calibration::sig_closed, _view,
          &View::on_calibration_closed);
  _view->_cali->update_device_info();
  _view->_cali->show();
}

void ViewDataSync::on_calibration_closed() {
  if (_view->_cali != NULL) {
    _view->_cali->deleteLater();
    _view->_cali = NULL;
  }
}

void ViewDataSync::check_calibration() {
  if (_view->get_work_mode() == DSO) {
    bool cali = false;
    _view->_device_agent->get_config_bool(SR_CONF_CALI, cali);

    if (cali) {
      show_calibration();
    }
  }
}

void ViewDataSync::vDial_updated() {
  if (_view->_cali != NULL) {
    _view->_cali->update_device_info();
  }

  auto math_trace = _view->get_own_math_trace();
  if (math_trace && math_trace->enabled()) {
    math_trace->update_vDial();
  }
}

void ViewDataSync::dso_factor_updated() {
  auto math_trace = _view->get_own_math_trace();
  if (math_trace && math_trace->enabled()) {
    math_trace->update_vDial();
  }
}

QString ViewDataSync::get_index_delta(uint64_t start, uint64_t end) {
  if (start == end)
    return "0";

  uint64_t delta_sample = (start > end) ? start - end : end - start;
  return _view->_ruler->format_real_time(
      delta_sample, _view->document_snapshot_source()->cur_snap_samplerate());
}

// =============================================================================
// Phase J additional: Qt event handling bodies
// =============================================================================

bool ViewDataSync::eventFilter(QObject *object, QEvent *event) {
  if (_view->_destroying)
    return false;

  const QEvent::Type type = event->type();
  if (type == QEvent::MouseMove) {
    const QMouseEvent *const mouse_event = (QMouseEvent *)event;
    if (object == _view->_ruler || object == _view->_time_viewport ||
        object == _view->_fft_viewport) {
      double cur_periods = (mouse_event->position().toPoint().x() + _view->_offset) *
                           _view->_scale / _view->_ruler->get_min_period();
      int integer_x =
          round(cur_periods) * _view->_ruler->get_min_period() / _view->_scale - _view->_offset;
      double cur_deviate_x =
          qAbs(mouse_event->position().toPoint().x() - integer_x);
      if (_view->get_work_mode() == LOGIC && cur_deviate_x < 10)
        _view->_hover_point = QPoint(integer_x, mouse_event->position().toPoint().y());
      else
        _view->_hover_point = mouse_event->position().toPoint();
    } else if (object == _view->_header)
      _view->_hover_point = QPoint(0, (int)mouse_event->position().y());
    else
      _view->_hover_point = QPoint(-1, -1);

    _view->hover_point_changed();
  } else if (type == QEvent::Leave) {
    _view->_hover_point = QPoint(-1, -1);
    _view->hover_point_changed();
  }

  return false;
}

void ViewDataSync::resizeEvent(QResizeEvent *event) {
  (void)event;
  int width = _view->get_view_width();

  if (width == 0) {
    return;
  }

  bool widthChanged = (_view->_lastWidth != width);
  _view->_lastWidth = width;

  if (!widthChanged && _view->get_work_mode() != DSO) {
    _view->setViewportMargins(_view->headerWidth(), View::RulerHeight, 0, 0);
    _view->_header->header_resize();
    _view->update_scroll();
    _view->viewport_update();
    return;
  }

  _view->reconstruct();
  _view->setViewportMargins(_view->headerWidth(), View::RulerHeight, 0, 0);
  _view->update_margins();
  _view->update_scroll();
  _view->signals_changed(NULL);

  if (_view->get_work_mode() == DSO) {
    _view->set_scale_offset(_view->_data_source->cur_view_time() / width, _view->_offset);
  }

  if (_view->get_work_mode() != DSO) {
    _view->_maxscale =
        _view->document_snapshot_source()->cur_sampletime() / (width * View::MaxViewRate);
    if (_view->_scale > _view->_maxscale) {
      _view->set_scale_offset(_view->_maxscale, _view->_offset);
    }
  } else {
    _view->_maxscale = 1e9;
  }

  _view->_ruler->update();
  _view->_header->header_resize();
  _view->set_update(_view->_time_viewport, true);
  _view->set_update(_view->_fft_viewport, true);
  _view->resize();
  _view->schedule_visible_range_notify();
}

} // namespace view
} // namespace pv
