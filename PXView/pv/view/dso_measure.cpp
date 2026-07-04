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

#include "dso_measure.h"

#include <QApplication>
#include <functional>
#include <math.h>
// Retained: sr_status value-type dependency (lines ~482-553). Removal requires
// introducing a Core DsoMeasureStatus mirror struct and changing
// DataSource::get_dso_status() signature — deferred to a separate spec.
#include <libsigrok/libsigrok.h>

#include "../data/dsosnapshot.h"
#include "../data/signalmodel.h"
#include "../dsvdef.h"
#include "../log.h"
#include "../sigsession.h"
#include "dsosignal.h"
#include "view.h"

using namespace std;

namespace pv {
namespace view {

DsoMeasure::DsoMeasure(DsoSignal *signal) : _signal(signal) {}

DsoMeasure::~DsoMeasure() {}

QString DsoMeasure::get_measure(int type) {
  const QString mNone = "--";
  QString mString;

  if (!_signal->_data || _signal->_data->empty()) {
    return mNone;
  }

  if (_signal->_mValid) {
    const int hw_offset = _signal->get_hw_offset();

    switch (type) {
    case DSO_MS_AMPT:
      if (_signal->_level_valid)
        mString = get_voltage(_signal->_high - _signal->_low, 2);
      else
        mString = mNone;
      break;
    case DSO_MS_VHIG:
      if (_signal->_level_valid)
        mString = get_voltage(hw_offset - _signal->_low, 2);
      else
        mString = mNone;
      break;
    case DSO_MS_VLOW:
      if (_signal->_level_valid)
        mString = get_voltage(hw_offset - _signal->_high, 2);
      else
        mString = mNone;
      break;
    case DSO_MS_VP2P:
      mString = get_voltage(_signal->_max - _signal->_min, 2);
      break;
    case DSO_MS_VMAX:
      mString = get_voltage(hw_offset - _signal->_min, 2);
      break;
    case DSO_MS_VMIN:
      mString = get_voltage(hw_offset - _signal->_max, 2);
      break;
    case DSO_MS_PERD:
      mString = get_time(_signal->_period);
      break;
    case DSO_MS_FREQ:
      if (_signal->_period == 0)
        mString = mNone;
      else if (abs(_signal->_period) > 1000000)
        mString = QString::number(1000000000 / _signal->_period, 'f', 2) + "Hz";
      else if (abs(_signal->_period) > 1000)
        mString = QString::number(1000000 / _signal->_period, 'f', 2) + "kHz";
      else
        mString = QString::number(1000 / _signal->_period, 'f', 2) + "MHz";
      break;
    case DSO_MS_VRMS:
      mString = get_voltage(_signal->_rms, 2);
      break;
    case DSO_MS_VMEA:
      mString = get_voltage(_signal->_mean, 2);
      break;
    case DSO_MS_NOVR:
      if (_signal->_level_valid && (_signal->_high - _signal->_low != 0))
        mString =
            QString::number((_signal->_max - _signal->_high) * 100.0 / (_signal->_high - _signal->_low), 'f', 2) +
            "%";
      else
        mString = mNone;
      break;
    case DSO_MS_POVR:
      if (_signal->_level_valid && (_signal->_high - _signal->_low != 0))
        mString =
            QString::number((_signal->_low - _signal->_min) * 100.0 / (_signal->_high - _signal->_low), 'f', 2) +
            "%";
      else
        mString = mNone;
      break;
    case DSO_MS_PDUT:
      if (_signal->_level_valid && _signal->_period != 0)
        mString = QString::number(_signal->_high_time / _signal->_period * 100, 'f', 2) + "%";
      else
        mString = mNone;
      break;
    case DSO_MS_NDUT:
      if (_signal->_level_valid && _signal->_period != 0)
        mString =
            QString::number(100 - _signal->_high_time / _signal->_period * 100, 'f', 2) + "%";
      else
        mString = mNone;
      break;
    case DSO_MS_PWDT:
      if (_signal->_level_valid)
        mString = get_time(_signal->_high_time);
      else
        mString = mNone;
      break;
    case DSO_MS_NWDT:
      if (_signal->_level_valid)
        mString = get_time(_signal->_period - _signal->_high_time);
      else
        mString = mNone;
      break;
    case DSO_MS_RISE:
      if (_signal->_level_valid)
        mString = get_time(_signal->_rise_time);
      else
        mString = mNone;
      break;
    case DSO_MS_FALL:
      if (_signal->_level_valid)
        mString = get_time(_signal->_fall_time);
      else
        mString = mNone;
      break;
    case DSO_MS_BRST:
      if (_signal->_level_valid)
        mString = get_time(_signal->_burst_time);
      else
        mString = mNone;
      break;
    case DSO_MS_PCNT:
      if (_signal->_level_valid)
        mString =
            (_signal->_pcount > 1000000
                 ? QString::number((double)_signal->_pcount / 1000000.0, 'f', 6) + "M"
             : _signal->_pcount > 1000
                 ? QString::number((double)_signal->_pcount / 1000.0, 'f', 3) + "K"
                 : QString::number((double)_signal->_pcount, 'f', 0));
      else
        mString = mNone;
      break;
    default:
      mString = "Error";
      break;
    }
  } else {
    mString = mNone;
  }
  return mString;
}

bool DsoMeasure::measure(const QPointF &p) {
  _signal->_hover_en = false;

  if (!_signal->enabled() || !_signal->show())
    return false;

  if (_signal->_data_source->is_stopped_status() == false)
    return false;

  const QRectF window = _signal->get_view_rect();
  if (!window.contains(p))
    return false;

  if (!_signal->_data || _signal->_data->empty())
    return false;

  _signal->_hover_index = _signal->_view->pixel2index(p.x());
  if (_signal->_hover_index >= _signal->_data->get_sample_count())
    return false;

  int chan_index = _signal->get_index();
  if (_signal->_data->has_data(chan_index) == false) {
    pxv_err("channel %d have no data.", chan_index);
    return false;
  }

  _signal->_hover_point = get_point(_signal->_hover_index, _signal->_hover_value);
  _signal->_hover_en = true;
  return true;
}

bool DsoMeasure::get_hover(uint64_t &index, QPointF &p, double &value) {
  if (_signal->_hover_en) {
    index = _signal->_hover_index;
    p = _signal->_hover_point;
    value = _signal->_hover_value;
    return true;
  }
  return false;
}

QPointF DsoMeasure::get_point(uint64_t index, float &value) {
  QPointF pt = QPointF(-1, -1);

  if (!_signal->enabled() || !_signal->_data)
    return pt;

  if (_signal->_data->empty())
    return pt;

  if (index >= _signal->_data->get_sample_count())
    return pt;

  value = *_signal->_data->get_samples(index, index, _signal->get_index());
  const float top = _signal->get_view_rect().top();
  const float bottom = _signal->get_view_rect().bottom();
  const int hw_offset = _signal->get_hw_offset();
  const float x = _signal->_view->index2pixel(index);
  const float y =
      min(max(top, _signal->get_zero_vpos() + (value - hw_offset) * _signal->_scale), bottom);
  pt = QPointF(x, y);

  return pt;
}

double DsoMeasure::get_voltage(uint64_t index) {
  if (!_signal->enabled() || !_signal->_data)
    return 1;

  if (_signal->_data->empty())
    return 1;

  if (index >= _signal->_data->get_sample_count())
    return 1;

  const double value = *_signal->_data->get_samples(index, index, _signal->get_index());
  const int hw_offset = _signal->get_hw_offset();
  uint64_t k = _signal->_data->get_measure_voltage_factor(_signal->get_index());
  float data_scale = _signal->_data->get_data_scale(_signal->get_index());

  return (hw_offset - value) * data_scale * k * _signal->_vDial->get_factor() *
         DS_CONF_DSO_VDIVS / _signal->get_view_rect().height();
}

QString DsoMeasure::get_voltage(double v, int p, bool scaled) {
  if (_signal->_vDial == NULL) {
    assert(false);
    return QString("--");
  }

  if (_signal->get_view_rect().height() == 0) {
    assert(false);
    return QString("--");
  }

  if (!_signal->_data)
    return QString("--");

  uint64_t k = _signal->_data->get_measure_voltage_factor(_signal->get_index());
  float data_scale = _signal->_data->get_data_scale(_signal->get_index());

  if (scaled)
    v = v * k * _signal->_vDial->get_factor() * DS_CONF_DSO_VDIVS /
        _signal->get_view_rect().height();
  else
    v = v * data_scale * k * _signal->_vDial->get_factor() * DS_CONF_DSO_VDIVS /
        _signal->get_view_rect().height();

  return abs(v) >= 1000 ? QString::number(v / 1000.0, 'f', p) + "V"
                        : QString::number(v, 'f', p) + "mV";
}

QString DsoMeasure::get_time(double t) {
  QString str =
      (abs(t) > 1000000000 ? QString::number(t / 1000000000, 'f', 2) + "S"
       : abs(t) > 1000000  ? QString::number(t / 1000000, 'f', 2) + "mS"
       : abs(t) > 1000     ? QString::number(t / 1000, 'f', 2) + "uS"
                           : QString::number(t, 'f', 2) + "nS");
  return str;
}

void DsoMeasure::auto_set() {
  if (_signal->_data_source->is_stopped_status()) {
    if (_signal->_autoV)
      autoV_end();
    if (_signal->_autoH)
      autoH_end();
  } else {
    if (_signal->_autoH && _signal->_autoV && _signal->get_zero_ratio() != 0.5) {
      _signal->set_zero_ratio(0.5);
    }
    if (_signal->_mValid && !_signal->_data_source->get_data_auto_lock()) {
      if (_signal->_autoH) {
        bool roll = false;
        _signal->_data_source->device()->is_roll_mode(roll);

        const double hori_res = _signal->_view->get_hori_res();
        if (_signal->_level_valid &&
            ((!roll && _signal->_pcount < 3) || _signal->_period > 4 * hori_res)) {
          _signal->_view->zoom(-1);
        } else if (_signal->_level_valid && _signal->_pcount > 6 && _signal->_period < 1.5 * hori_res) {
          _signal->_view->zoom(1);
        } else if (_signal->_level_valid) {
          autoH_end();
        }
      }
      if (_signal->_autoV) {
        const bool over_flag = _signal->_max == 0xff || _signal->_min == 0x0;
        const bool out_flag = _signal->_max >= 0xE0 || _signal->_min <= 0x20;
        const bool under_flag = _signal->_max <= 0xA0 && _signal->_min >= 0x60;
        if (over_flag) {
          if (!_signal->_autoV_over)
            _signal->_auto_cnt = 0;
          _signal->_autoV_over = true;
          _signal->go_vDialNext(false);
        } else if (out_flag) {
          _signal->go_vDialNext(false);
        } else if (!_signal->_autoV_over && under_flag) {
          _signal->go_vDialPre(false);
        } else if (!_signal->_autoH) {
          autoV_end();
        }

        if (_signal->_autoV_over && under_flag) {
          if (_signal->_auto_cnt++ > 16)
            _signal->_autoV_over = false;
        } else {
          _signal->_auto_cnt = 0;
        }

        if (_signal->_level_valid) {
          _signal->_trig_value = (_signal->_min + _signal->_max) / 2;
          _signal->set_trig_vpos(_signal->ratio2pos(_signal->get_trig_vrate()));
        }
      }
      if (_signal->_autoH || _signal->_autoV)
        _signal->_data_source->data_auto_lock(DsoSignal::AutoLock);
    }
  }
}

void DsoMeasure::autoV_end() {
  _signal->_autoV = false;
  _signal->_autoV_over = false;
  _signal->_view->auto_trig(_signal->get_index());
  _signal->_trig_value = (_signal->_min + _signal->_max) / 2;
  _signal->set_trig_vpos(_signal->ratio2pos(_signal->get_trig_vrate()));
  _signal->_view->set_update(_signal->_viewport, true);
  _signal->_view->update();
}

void DsoMeasure::autoH_end() {
  _signal->_autoH = false;
  _signal->_view->set_update(_signal->_viewport, true);
  _signal->_view->update();
}

void DsoMeasure::auto_end() {
  if (_signal->_autoV)
    autoV_end();
  if (_signal->_autoH)
    autoH_end();
}

void DsoMeasure::auto_start() {
  if (_signal->_autoV || _signal->_autoH)
    return;

  if (_signal->_data_source->is_running_status()) {
    _signal->_data_source->data_auto_lock(DsoSignal::AutoLock);
    _signal->_autoV = true;
    _signal->_autoH = true;
    _signal->_view->auto_trig(_signal->get_index());
    _signal->_end_timer.TimeOut(DsoSignal::AutoTime, std::bind(&DsoMeasure::call_auto_end,
                                           this)); // start a timeout
  }
}

void DsoMeasure::call_auto_end() { _signal->_data_source->auto_end(); }

void DsoMeasure::paint_hover_measure(QPainter &p, QColor fore, QColor back) {
  const int hw_offset = _signal->get_hw_offset();
  // Hover measure
  if (_signal->_hover_en && _signal->_hover_point != QPointF(-1, -1)) {
    QString hover_str = _signal->get_voltage(hw_offset - _signal->_hover_value, 2);
    const int hover_width =
        p.boundingRect(0, 0, INT_MAX, INT_MAX, Qt::AlignLeft | Qt::AlignTop,
                       hover_str)
            .width() +
        10;
    const int hover_height =
        p.boundingRect(0, 0, INT_MAX, INT_MAX, Qt::AlignLeft | Qt::AlignTop,
                       hover_str)
            .height();
    QRectF hover_rect(_signal->_hover_point.x(), _signal->_hover_point.y() - hover_height / 2,
                      hover_width, hover_height);
    if (hover_rect.right() > _signal->get_view_rect().right())
      hover_rect.moveRight(_signal->_hover_point.x());
    if (hover_rect.top() < _signal->get_view_rect().top())
      hover_rect.moveTop(_signal->_hover_point.y());
    if (hover_rect.bottom() > _signal->get_view_rect().bottom())
      hover_rect.moveBottom(_signal->_hover_point.y());

    p.setPen(fore);
    p.setBrush(back);
    p.drawRect(_signal->_hover_point.x() - 1, _signal->_hover_point.y() - 1, _signal->HoverPointSize,
               _signal->HoverPointSize);
    p.drawText(hover_rect, Qt::AlignCenter | Qt::AlignTop | Qt::TextDontClip,
               hover_str);
  }

  auto &cursor_list = _signal->_view->get_cursorList();
  auto i = cursor_list.begin();

  while (i != cursor_list.end()) {
    float pt_value;

    int chan_index = (*i)->index();
    if (!_signal->_data || _signal->_data->has_data(chan_index) == false) {
      i++;
      continue;
    }

    const QPointF pt = _signal->get_point(chan_index, pt_value);
    if (pt == QPointF(-1, -1)) {
      i++;
      continue;
    }

    QString pt_str = _signal->get_voltage(hw_offset - pt_value, 2);
    const int pt_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                                        Qt::AlignLeft | Qt::AlignTop, pt_str)
                             .width() +
                         10;
    const int pt_height = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                                         Qt::AlignLeft | Qt::AlignTop, pt_str)
                              .height();
    QRectF pt_rect(pt.x(), pt.y() - pt_height / 2, pt_width, pt_height);
    if (pt_rect.right() > _signal->get_view_rect().right())
      pt_rect.moveRight(pt.x());
    if (pt_rect.top() < _signal->get_view_rect().top())
      pt_rect.moveTop(pt.y());
    if (pt_rect.bottom() > _signal->get_view_rect().bottom())
      pt_rect.moveBottom(pt.y());

    p.drawRect(pt.x() - 1, pt.y() - 1, 2, 2);
    p.drawLine(pt.x() - 2, pt.y() - 2, pt.x() + 2, pt.y() + 2);
    p.drawLine(pt.x() + 2, pt.y() - 2, pt.x() - 2, pt.y() + 2);
    p.drawText(pt_rect, Qt::AlignCenter | Qt::AlignTop | Qt::TextDontClip,
               pt_str);

    i++;
  }
}

void DsoMeasure::update_measure_status(int index, int hw_offset, uint16_t enabled_channels,
                                       double samplerate) {
  sr_status status;

  if (!_signal->_data_source->dso_status_is_valid())
    return;

  _signal->_mValid = true;
  status = _signal->_data_source->get_dso_status();

  if (!status.measure_valid)
    return;

  _signal->_min = (index == 0) ? status.ch0_min : status.ch1_min;
  _signal->_max = (index == 0) ? status.ch0_max : status.ch1_max;

  _signal->_level_valid =
      (index == 0) ? status.ch0_level_valid : status.ch1_level_valid;
  _signal->_low = (index == 0) ? status.ch0_low_level : status.ch1_low_level;
  _signal->_high = (index == 0) ? status.ch0_high_level : status.ch1_high_level;

  const uint32_t count =
      (index == 0) ? status.ch0_cyc_cnt : status.ch1_cyc_cnt;
  const bool plevel =
      (index == 0) ? status.ch0_plevel : status.ch1_plevel;
  const bool startXORend = (index == 0) ? (status.ch0_cyc_llen == 0)
                                        : (status.ch1_cyc_llen == 0);
  uint16_t total_channels =
      _signal->_data_source->device()->get_channel_count();

  if (total_channels == 1 && _signal->_data->is_file()) {
    total_channels++;
  }

  const double tfactor =
      (total_channels / enabled_channels) * 1000000000ULL * 1.0 / samplerate;

  double samples =
      (index == 0) ? status.ch0_cyc_tlen : status.ch1_cyc_tlen;
  _signal->_period = ((count == 0) ? 0 : samples / count) * tfactor;

  samples = (index == 0) ? status.ch0_cyc_flen : status.ch1_cyc_flen;
  _signal->_rise_time =
      ((count == 0)
           ? 0
           : samples / ((plevel && startXORend) ? count : count + 1)) *
      tfactor;
  samples = (index == 0) ? status.ch0_cyc_rlen : status.ch1_cyc_rlen;
  _signal->_fall_time =
      ((count == 0)
           ? 0
           : samples / ((!plevel && startXORend) ? count : count + 1)) *
      tfactor;

  samples = (index == 0)
                ? (status.ch0_plevel
                       ? status.ch0_cyc_plen - status.ch0_cyc_llen
                       : status.ch0_cyc_tlen - status.ch0_cyc_plen +
                             status.ch0_cyc_llen)
                : (status.ch1_plevel
                       ? status.ch1_cyc_plen - status.ch1_cyc_llen
                       : status.ch1_cyc_tlen - status.ch1_cyc_plen +
                             status.ch1_cyc_llen);
  _signal->_high_time = ((count == 0) ? 0 : samples / count) * tfactor;

  samples = (index == 0) ? status.ch0_cyc_tlen + status.ch0_cyc_llen
                         : status.ch1_cyc_flen + status.ch1_cyc_llen;
  _signal->_burst_time = samples * tfactor;

  _signal->_pcount = count + (plevel & !startXORend);
  _signal->_rms = (index == 0) ? status.ch0_acc_square : status.ch1_acc_square;
  _signal->_rms = sqrt(_signal->_rms / _signal->_data->get_sample_count());
  _signal->_mean = (index == 0) ? status.ch0_acc_mean : status.ch1_acc_mean;
  _signal->_mean = hw_offset - _signal->_mean / _signal->_data->get_sample_count();
}

} // namespace view
} // namespace pv
