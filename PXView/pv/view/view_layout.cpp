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

// Phase E (modernize-view-layer-v2): scale / offset / scroll / margin
// behaviour extracted from the View God-class. ViewLayout is declared a
// friend of View so it can touch the private _scale / _offset / _header /
// _ruler / _viewcenter / _time_viewport / _vOffset / _updating_scroll /
// _preScale / _preOffset / _minscale / _maxscale fields directly. All
// cross-method calls that remain on View (e.g. get_view_width,
// viewport_update, get_work_mode, document_snapshot_source) go through
// _view->… so the public View API is unchanged.

#include "view_layout.h"

#include <assert.h>
#include <cmath>
#include <algorithm>

#include <QDebug>
#include <QScrollBar>
#include "../log.h"

#include "view.h"
#include "viewport.h"
#include "devmode.h"
#include "header.h"
#include "ruler.h"
#include "../data/datasource.h"
#include "../sigsession.h"
#include "../toolbars/samplingbar.h"

using namespace std;

namespace pv {
namespace view {

void ViewLayout::set_scale_offset(double scale, int64_t offset) {
  // Use return address to identify the caller for debugging offset=-408 issue
  pxv_info("[DEBUG-SCALE] set_scale_offset ENTRY: scale=%.9g offset=%lld cur_scale=%.9g cur_offset=%lld maxscale=%.9g minscale=%.9g max_off=%lld min_off=%lld caller=%p",
           scale, (long long)offset, _view->_scale, (long long)_view->_offset,
           _view->_maxscale, _view->_minscale,
           (long long)get_max_offset(), (long long)get_min_offset(),
           __builtin_return_address(0));
  // Restore bidirectional clamping from Reference/DSView-master/DSView/pv/view/
  // view.cpp:355-356. Previously only `max(...)` (lower bound) was applied,
  // missing the upper bound clamp on both _scale and _offset. This caused
  // set_trig_cursor_posistion() to compute offset = (time/_scale) - (width/2)
  // when trig_pos was near the buffer end (typical for DSO acquisitions with
  // trigger delay) — offset exceeded get_max_offset(), the viewport scrolled
  // past the data end, and the right half of the screen showed no waveform
  // and no cursor. The original DSView clamps both bounds here, so do the
  // same. Note: in DSO mode update_scale_offset() sets _maxscale=1e9 (no
  // upper bound on scale), but get_max_offset() still constrains _offset
  // based on the current scale and total sampletime — that's the relevant
  // clamp for fixing the off-screen cursor regression.
  _view->_preScale = _view->_scale;
  _view->_preOffset = _view->_offset;

  _view->_scale = max(min(scale, _view->_maxscale), _view->_minscale);
  _view->_offset = floor(max(min(offset, _view->get_max_offset()),
                              _view->get_min_offset()));

  if (_view->_scale != _view->_preScale || _view->_offset != _view->_preOffset) {
    update_scroll();
    _view->_header->update();
    _view->_ruler->update();
    _view->viewport_update();
  }
  _view->schedule_visible_range_notify();
}

void ViewLayout::limit_scale_offset() {
  int width = _view->get_view_width();
  if (_view->get_work_mode() != DSO) {
    double sampletime = _view->document_snapshot_source()->cur_sampletime();
    uint64_t samplerate = _view->document_snapshot_source()->cur_snap_samplerate();
    if (sampletime > 0 && samplerate > 0 && width > 0) {
      _view->_maxscale = sampletime / (width * View::MaxViewRate);
      _view->_minscale = (1.0 / samplerate) / View::MaxPixelsPerSample;
    }
    _view->_scale = max(min(_view->_scale, _view->_maxscale), _view->_minscale);
  } else {
    // DSO mode: re-derive _scale from the (possibly changed) base_scale so
    // the user's _dso_zoom_factor is respected. limit_scale_offset() is
    // called from receive_end(), which fires after capture ends — without
    // updating scroll here the horizontal scrollbar range stays at its
    // pre-capture value (often 0), making the bar un-draggable even after
    // the user zoomed in.
    const double base_scale = _view->_data_source->cur_view_time() / width;
    if (base_scale > 0) {
      _view->_maxscale = 1e9;
      _view->_minscale = base_scale * 1e-6;
      _view->_scale = base_scale * _view->_dso_zoom_factor;
      _view->_scale = max(min(_view->_scale, _view->_maxscale), _view->_minscale);
      _view->_dso_zoom_factor = _view->_scale / base_scale;
    }
  }
  _view->_offset =
      max(min(_view->_offset, get_max_offset()), get_min_offset());
  update_scroll();
  _view->_ruler->update();
  _view->viewport_update();
  _view->schedule_visible_range_notify();
}

void ViewLayout::update_scale_offset() {
  int width = _view->get_view_width();
  if (width == 0) {
    return;
  }

  if (_view->get_work_mode() != DSO) {
    double sampletime = _view->document_snapshot_source()->cur_sampletime();
    uint64_t samplerate = _view->document_snapshot_source()->cur_snap_samplerate();
    if (sampletime > 0 && samplerate > 0) {
      _view->_maxscale = sampletime / (width * View::MaxViewRate);
      _view->_minscale = (1.0 / samplerate) / View::MaxPixelsPerSample;
    } else {
      _view->_maxscale = 1e9;
      _view->_minscale = 1e-15;
    }
    _view->_scale = max(_view->_scale, _view->_minscale);
  } else {
    // DSO mode: base_scale = fit one frame to viewport width. User zoom
    // is preserved across data frames via _dso_zoom_factor (zoom() only
    // mutates the factor; this re-derives _scale every frame). This lets
    // the user zoom in and pan horizontally like LOGIC mode, instead of
    // the original DSView design that stepped discrete timebase values.
    const double base_scale = _view->_data_source->cur_view_time() / width;
    if (base_scale > 0) {
      // Keep original _maxscale=1e9 (no zoom-out beyond fit-frame is
      // naturally prevented because get_max_offset() would go negative,
      // clamping offset to 0). _minscale caps how far in you can zoom.
      _view->_maxscale = 1e9;
      _view->_minscale = base_scale * 1e-6;
      _view->_scale = base_scale * _view->_dso_zoom_factor;
      _view->_scale = max(min(_view->_scale, _view->_maxscale), _view->_minscale);
      _view->_dso_zoom_factor = _view->_scale / base_scale;
    } else {
      // cur_view_time() not yet available (e.g. device not opened). Keep
      // wide defaults so we don't collapse _scale to 0/NaN and blank the view.
      _view->_maxscale = 1e9;
      _view->_minscale = 1e-15;
      _view->_scale = max(_view->_scale, _view->_minscale);
    }
  }

  // Restore upper bound clamp on _offset (Reference/DSView-master/DSView/
  // pv/view/view.cpp:660). Without `min(..., get_max_offset())` the offset
  // could remain past the data end after a scale change in DSO mode,
  // causing the same off-screen cursor / waveform symptom.
  _view->_offset = max(min(_view->_offset, get_max_offset()),
                        get_min_offset());

  _view->_preScale = _view->_scale;
  _view->_preOffset = _view->_offset;

  _view->_ruler->update();
  _view->viewport_update();
  _view->schedule_visible_range_notify();
}

void ViewLayout::set_scale(double scale) {
  if (scale < _view->_minscale)
    scale = _view->_minscale;
  if (scale > _view->_maxscale)
    scale = _view->_maxscale;

  if (_view->_scale != scale) {
    _view->_scale = scale;
    _view->_header->update();
    _view->_ruler->update();
    _view->viewport_update();
    update_scroll();
  }
  _view->schedule_visible_range_notify();
}

void ViewLayout::zoom(double steps) {
  int width = _view->get_view_width();
  if (width > 0) {
    zoom(steps, width / 2);
  }
}

bool ViewLayout::zoom(double steps, int offset) {
  int width = _view->get_view_width();
  pxv_info("[DEBUG-ZOOM] zoom ENTRY: steps=%.4f offset=%d width=%d mode=%d scale=%.9g zoom_factor=%.6f maxscale=%.9g minscale=%.9g",
           steps, offset, width, _view->get_work_mode(),
           _view->_scale, _view->_dso_zoom_factor,
           _view->_maxscale, _view->_minscale);
  if (width == 0) {
    pxv_info("[DEBUG-ZOOM] zoom ABORT: width=0");
    return false;
  }

  bool ret = true;
  _view->_preScale = _view->_scale;
  _view->_preOffset = _view->_offset;

  if (_view->get_work_mode() != DSO) {
    // LOGIC/ANALOG: direct continuous scale zoom
    _view->_scale *= std::pow(3.0 / 2.0, -steps);
    _view->_scale = max(min(_view->_scale, _view->_maxscale), _view->_minscale);
  } else {
    // DSO mode: user requested direct view scale zoom (matching LOGIC
    // behavior) instead of the original DSView design that stepped through
    // discrete timebase values via hori_knob(). The original design bound
    // vertical wheel scrolling to the horizontal timebase combobox, which
    // felt unnatural — DSO now zooms the view continuously just like LOGIC.
    // The instant-mode running guard is retained: don't zoom while an
    // instant capture is in progress.
    if (_view->_data_source->is_running_status() &&
        _view->_data_source->is_instant()) {
      return ret;
    }
    // Mutate the user zoom factor, not _scale directly. update_scale_offset()
    // (called every frame from data_updated()) re-derives _scale from
    // base_scale * _dso_zoom_factor, so without this indirection the user's
    // zoom would be wiped on the next frame and the horizontal scrollbar
    // would collapse back to range=0 (no panning possible).
    const double base_scale = _view->_data_source->cur_view_time() / width;
    if (base_scale <= 0) {
      return ret;  // can't zoom without a known view time
    }
    _view->_dso_zoom_factor *= std::pow(3.0 / 2.0, -steps);
    _view->_scale = base_scale * _view->_dso_zoom_factor;
    _view->_scale = max(min(_view->_scale, _view->_maxscale), _view->_minscale);
    _view->_dso_zoom_factor = _view->_scale / base_scale;
  }

  _view->_offset =
      floor((_view->_offset + offset) * (_view->_preScale / _view->_scale) - offset);
  _view->_offset =
      max(min(_view->_offset, get_max_offset()), get_min_offset());

  if (_view->_scale != _view->_preScale || _view->_offset != _view->_preOffset) {
    _view->_header->update();
    _view->_ruler->update();
    _view->viewport_update();
    update_scroll();
  }
  _view->schedule_visible_range_notify();

  return ret;
}

void ViewLayout::h_scroll_value_changed(int value) {
  // DEBUG: log BEFORE the guard so we can see if the signal fires at all
  pxv_info("[DEBUG-SCROLL] h_scroll_value_changed ENTRY: value=%d updating_scroll=%d max_off=%lld min_off=%lld scale=%.9g zoom_factor=%.6f offset=%lld",
           value, _view->_updating_scroll ? 1 : 0,
           (long long)get_max_offset(), (long long)get_min_offset(),
           _view->_scale, _view->_dso_zoom_factor, (long long)_view->_offset);

  if (_view->_updating_scroll)
    return;

  _view->_preOffset = _view->_offset;

  const int range = _view->horizontalScrollBar()->maximum();
  if (range < View::MaxScrollValue)
    _view->_offset = value;
  else {
    int64_t length = 0;
    int64_t offset = 0;
    get_scroll_layout(length, offset);
    _view->_offset = floor(value * 1.0 / View::MaxScrollValue * length);
  }

  _view->_offset =
      max(min(_view->_offset, get_max_offset()), get_min_offset());

  if (_view->_offset != _view->_preOffset) {
    _view->_ruler->update();
    _view->viewport_update();
  }
  _view->schedule_visible_range_notify();
}

void ViewLayout::get_scroll_layout(int64_t &length, int64_t &offset) {
  length = ceil(_view->document_snapshot_source()->cur_snap_sampletime() /
                _view->_scale);
  offset = _view->_offset;
}

void ViewLayout::update_scroll() {
  assert(_view->_viewcenter);

  int width = _view->get_view_width();
  if (width == 0) {
    return;
  }

  const QSize areaSize = QSize(width, _view->get_view_height());

  // Set the horizontal scroll bar
  int64_t length = 0;
  int64_t offset = 0;
  get_scroll_layout(length, offset);
  length = max(length - areaSize.width(), (int64_t)0);

  pxv_info("[DEBUG-SCROLL] update_scroll: mode=%d width=%d scale=%.9g sampletime=%.9g view_time=%.9g zoom_factor=%.6f length=%lld offset=%lld range=%lld max_off=%lld min_off=%lld",
           _view->get_work_mode(), width, _view->_scale,
           _view->document_snapshot_source()->cur_snap_sampletime(),
           _view->_data_source->cur_view_time(),
           _view->_dso_zoom_factor,
           (long long)length, (long long)offset, (long long)length,
           (long long)get_max_offset(), (long long)get_min_offset());

  _view->horizontalScrollBar()->setPageStep(areaSize.width());

  _view->_updating_scroll = true;

  // DEBUG: log scrollbar geometry & state to diagnose drag failure
  auto *hbar = _view->horizontalScrollBar();
  QRect hbarGeo = hbar->geometry();
  pxv_info("[DEBUG-SCROLL] hbar state: visible=%d enabled=%d geo=[x=%d y=%d w=%d h=%d] min=%d max=%d pagestep=%d sliderpos=%d value=%d",
           hbar->isVisible() ? 1 : 0, hbar->isEnabled() ? 1 : 0,
           hbarGeo.x(), hbarGeo.y(), hbarGeo.width(), hbarGeo.height(),
           hbar->minimum(), hbar->maximum(), hbar->pageStep(),
           hbar->sliderPosition(), hbar->value());

  if (length < View::MaxScrollValue) {
    _view->horizontalScrollBar()->setRange(0, length);
    _view->horizontalScrollBar()->setSliderPosition(offset);
  } else {
    _view->horizontalScrollBar()->setRange(0, View::MaxScrollValue);
    _view->horizontalScrollBar()->setSliderPosition(
        _view->_offset * 1.0 / length * View::MaxScrollValue);
  }

  _view->_updating_scroll = false;

  // Set the vertical scrollbar
  int totalContentHeight = 0;
  if (_view->_time_viewport)
    totalContentHeight = _view->_time_viewport->get_total_height();
  int vRange = max(0, totalContentHeight - areaSize.height());
  if (vRange > 0)
    _view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  else
    _view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  _view->verticalScrollBar()->setPageStep(areaSize.height());
  _view->verticalScrollBar()->setRange(0, vRange);
  _view->verticalScrollBar()->setSliderPosition(_view->_vOffset);
}

void ViewLayout::update_margins() {
  int width = _view->get_view_width();

  if (width > 0) {
    _view->_ruler->setGeometry(_view->_viewcenter->x(), 0,
                               _view->width() - _view->_viewcenter->x(),
                               _view->_viewcenter->y());
    _view->_header->setGeometry(0, _view->_viewcenter->y(),
                                _view->_viewcenter->x(),
                                _view->_viewcenter->height());
    _view->_devmode->setGeometry(0, 0, _view->_viewcenter->x(),
                                 _view->_viewcenter->y());
  }
}

int64_t ViewLayout::get_min_offset() {
  int width = _view->get_view_width();
  assert(width > 0);

  if (View::MaxViewRate > 1)
    return floor(width * (1 - View::MaxViewRate));
  else
    return 0;
}

int64_t ViewLayout::get_max_offset() {
  int width = _view->get_view_width();
  assert(width > 0);

  return ceil((_view->document_snapshot_source()->cur_snap_sampletime() /
               _view->_scale) -
              (width * View::MaxViewRate));
}

} // namespace view
} // namespace pv
