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

#include <QScrollBar>

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
  _view->_preScale = _view->_scale;
  _view->_preOffset = _view->_offset;

  _view->_scale = max(scale, _view->_minscale);
  _view->_offset = floor(max(offset, _view->get_min_offset()));

  if (_view->_scale != _view->_preScale || _view->_offset != _view->_preOffset) {
    update_scroll();
    _view->_header->update();
    _view->_ruler->update();
    _view->viewport_update();
  }
  _view->schedule_visible_range_notify();
}

void ViewLayout::limit_scale_offset() {
  if (_view->get_work_mode() != DSO) {
    int width = _view->get_view_width();
    double sampletime = _view->document_snapshot_source()->cur_sampletime();
    uint64_t samplerate = _view->document_snapshot_source()->cur_snap_samplerate();
    if (sampletime > 0 && samplerate > 0 && width > 0) {
      _view->_maxscale = sampletime / (width * View::MaxViewRate);
      _view->_minscale = (1.0 / samplerate) / View::MaxPixelsPerSample;
    }
    _view->_scale = max(min(_view->_scale, _view->_maxscale), _view->_minscale);
    _view->_offset =
        max(min(_view->_offset, get_max_offset()), get_min_offset());
    update_scroll();
    _view->_ruler->update();
    _view->viewport_update();
  }
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
    _view->_scale = _view->_data_source->cur_view_time() / width;
    _view->_maxscale = 1e9;
    _view->_minscale = 1e-15;
    _view->_scale = max(_view->_scale, _view->_minscale);
  }

  _view->_offset = max(_view->_offset, get_min_offset());

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
  if (width == 0) {
    return false;
  }

  bool ret = true;
  _view->_preScale = _view->_scale;
  _view->_preOffset = _view->_offset;

  pxv_info("[DEBUG-DSO] zoom: steps=%.3f offset=%d width=%d work_mode=%d scale=%.9g",
           steps, offset, width, _view->get_work_mode(), _view->_scale);

  if (_view->get_work_mode() != DSO) {
    _view->_scale *= std::pow(3.0 / 2.0, -steps);
    _view->_scale = max(min(_view->_scale, _view->_maxscale), _view->_minscale);
  } else {
    if (_view->_data_source->is_running_status() &&
        _view->_data_source->is_instant()) {
      pxv_info("[DEBUG-DSO] zoom: skipped (running+instant)");
      return ret;
    }

    double hori_res = -1;
    if (steps > 0.5)
      hori_res = _view->_sampling_bar->hori_knob(-1);
    else if (steps < -0.5)
      hori_res = _view->_sampling_bar->hori_knob(1);

    pxv_info("[DEBUG-DSO] zoom: hori_res=%.9g cur_view_time=%.9g",
             hori_res, _view->_data_source->cur_view_time());

    if (hori_res > 0) {
      const double scale = _view->_data_source->cur_view_time() / width;
      _view->_scale = max(min(scale, _view->_maxscale), _view->_minscale);
      pxv_info("[DEBUG-DSO] zoom: new scale=%.9g", _view->_scale);
    } else {
      ret = false;
    }
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

  _view->horizontalScrollBar()->setPageStep(areaSize.width() / 2);

  _view->_updating_scroll = true;

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
