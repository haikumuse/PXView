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

#include "dso_hardware_config.h"

#include <QApplication>
#include <QCoreApplication>
#include <QTimer>
#include <libsigrok.h>

#include "../data/signalmodel.h"
#include "../dsvdef.h"
#include "../log.h"
#include "../sigsession.h"
#include "dsosignal.h"
#include "view.h"

using namespace std;

namespace pv {
namespace view {

DsoHardwareConfig::DsoHardwareConfig(DsoSignal *signal) : _signal(signal) {}

DsoHardwareConfig::~DsoHardwareConfig() {}

void DsoHardwareConfig::set_enable(bool enable) {
  sr_channel *probe = _signal->_model ? _signal->_model->sr_channel_handle() : nullptr;
  if (!probe)
    return;

  if (_signal->_data_source->device()->is_hardware_logic() && _signal->get_index() == 0) {
    return;
  }

  _signal->_en_lock = true;
  bool cur_enable = _signal->_model->enabled();
  if (cur_enable == enable) {
    _signal->_en_lock = false;
    return;
  }

  bool running = false;

  if (_signal->_data_source->is_running_status()) {
    running = true;
    _signal->_data_source->stop_capture();
  }

  while (_signal->_data_source->is_running_status())
    QCoreApplication::processEvents();

  set_vDialActive(false);
  _signal->_model->set_probe_enabled(enable, probe);

  _signal->_view->update_hori_res();

  if (running) {
    _signal->_data_source->stop_capture();
    _signal->_data_source->start_capture(false);
  }

  _signal->_view->set_update(_signal->_viewport, true);
  _signal->_view->update();
  _signal->_en_lock = false;
}

void DsoHardwareConfig::set_vDialActive(bool active) {
  if (_signal->enabled())
    _signal->_vDialActive = active;
}

bool DsoHardwareConfig::go_vDialPre(bool manul) {
  sr_channel *probe = _signal->_model ? _signal->_model->sr_channel_handle() : nullptr;

  if (_signal->_autoV && manul)
    _signal->autoV_end();

  if (_signal->enabled() && !_signal->_vDial->isMin()) {
    if (_signal->_data_source->is_running_status())
      _signal->_data_source->refresh(DsoSignal::RefreshShort);

    const double pre_vdiv = _signal->_vDial->get_value();
    _signal->_vDial->set_sel(_signal->_vDial->get_sel() - 1);

    if (_signal->_data_source->is_stopped_status()) {
      _signal->set_stop_scale(_signal->_stop_scale * (pre_vdiv / _signal->_vDial->get_value()));
      _signal->set_scale(_signal->get_view_rect().height());
    }
    if (probe)
      _signal->_model->set_probe_offset((uint16_t)_signal->_zero_offset, probe);

    _signal->_view->vDial_updated();
    _signal->_view->set_update(_signal->_viewport, true);
    _signal->_view->update();
    // Task 7.2: 写回 Core SignalModel + 广播（用户交互入口：mouse_press /
    // 键盘快捷键）。
    if (_signal->_model) {
      _signal->_model->set_vdiv((double)_signal->_vDial->get_value());
    }
    return true;
  } else {
    if (_signal->_autoV && !_signal->_autoV_over)
      _signal->autoV_end();
    return false;
  }
}

bool DsoHardwareConfig::go_vDialNext(bool manul) {
  sr_channel *probe = _signal->_model ? _signal->_model->sr_channel_handle() : nullptr;

  if (_signal->_autoV && manul)
    _signal->autoV_end();

  if (_signal->enabled() && !_signal->_vDial->isMax()) {
    if (_signal->_data_source->is_running_status())
      _signal->_data_source->refresh(DsoSignal::RefreshShort);

    const double pre_vdiv = _signal->_vDial->get_value();
    _signal->_vDial->set_sel(_signal->_vDial->get_sel() + 1);

    if (_signal->_data_source->is_stopped_status()) {
      _signal->set_stop_scale(_signal->_stop_scale * (pre_vdiv / _signal->_vDial->get_value()));
      _signal->set_scale(_signal->get_view_rect().height());
    }
    if (probe)
      _signal->_model->set_probe_offset((uint16_t)_signal->_zero_offset, probe);

    _signal->_view->vDial_updated();
    _signal->_view->set_update(_signal->_viewport, true);
    _signal->_view->update();
    // Task 7.2: 写回 Core SignalModel + 广播（用户交互入口：mouse_press /
    // 键盘快捷键）。
    if (_signal->_model) {
      _signal->_model->set_vdiv((double)_signal->_vDial->get_value());
    }
    return true;
  } else {
    if (_signal->_autoV && !_signal->_autoV_over)
      _signal->autoV_end();
    return false;
  }
}

void DsoHardwareConfig::init_vDial(DsoSignal *src) {
  QVector<uint64_t> vValue;
  QVector<QString> vUnit;

  for (uint64_t i = 0; i < DsoSignal::vDialUnitCount; i++) {
    vUnit.append(DsoSignal::vDialUnit[i]);
  }

  _signal->_vDial = NULL;

  GVariant *gvar_list, *gvar_list_vdivs;
  gvar_list = _signal->_data_source->device()->get_config_list(NULL, SR_CONF_PROBE_VDIV);

  if (gvar_list != NULL) {
    assert(gvar_list);
    if ((gvar_list_vdivs = g_variant_lookup_value(gvar_list, "vdivs",
                                                  G_VARIANT_TYPE("at")))) {
      GVariant *gvar;
      GVariantIter iter;
      g_variant_iter_init(&iter, gvar_list_vdivs);

      while (NULL != (gvar = g_variant_iter_next_value(&iter))) {
        vValue.push_back(g_variant_get_uint64(gvar));
        g_variant_unref(gvar);
      }

      g_variant_unref(gvar_list_vdivs);
      g_variant_unref(gvar_list);
    }
  }
  _signal->_vDial = new dslDial(vValue.count(), DsoSignal::vDialValueStep, vValue, vUnit, false);

  if (src) {
    _signal->_vDial->set_sel(src->_vDial->get_sel());
    _signal->_vDial->set_factor(src->_vDial->get_factor());
  }
}

bool DsoHardwareConfig::load_settings() {
  sr_channel *probe = _signal->_model ? _signal->_model->sr_channel_handle() : nullptr;
  int v;
  uint32_t ui32;
  bool ret;

  // dso channel bits
  ret = _signal->_data_source->device()->get_config_byte(SR_CONF_UNIT_BITS, v);
  if (ret) {
    _signal->_bits = (uint8_t)v;
  } else {
    _signal->_bits = DsoSignal::DefaultBits;
    pxv_warn(
        "%s%d",
        "Warning: config_get SR_CONF_UNIT_BITS failed, set to %d(default).",
        DsoSignal::DefaultBits);

    if (_signal->_data_source->device()->is_hardware())
      return false;
  }

  ret = _signal->_data_source->device()->get_config_uint32(SR_CONF_REF_MIN, ui32);
  if (ret)
    _signal->_ref_min = (double)ui32;
  else
    _signal->_ref_min = 1;

  ret = _signal->_data_source->device()->get_config_uint32(SR_CONF_REF_MAX, ui32);
  if (ret)
    _signal->_ref_max = (double)ui32;
  else
    _signal->_ref_max = ((1 << _signal->_bits) - 1);

  // -- vdiv
  uint64_t vdiv;
  uint64_t vfactor;
  if (probe) {
    ret = _signal->_data_source->device()->get_config_uint64(SR_CONF_PROBE_VDIV, vdiv,
                                                   probe, NULL);
    if (!ret) {
      pxv_err("ERROR: config_get SR_CONF_PROBE_VDIV failed.");
      return false;
    }

    ret = _signal->_data_source->device()->get_config_uint64(SR_CONF_PROBE_FACTOR,
                                                   vfactor, probe, NULL);
    if (!ret) {
      pxv_err("ERROR: config_get SR_CONF_PROBE_FACTOR failed.");
      return false;
    }
  } else {
    vdiv = _signal->_model ? _signal->_model->vdiv() : 0;
    vfactor = _signal->_model ? _signal->_model->vfactor() : 1;
  }

  _signal->_vDial->set_value(vdiv);
  _signal->_vDial->set_factor(vfactor);

  // -- coupling
  if (probe) {
    ret = _signal->_data_source->device()->get_config_byte(SR_CONF_PROBE_COUPLING, v,
                                                 probe, NULL);
    if (ret) {
      _signal->_acCoupling = uint8_t(v);
    } else {
      pxv_err("ERROR: config_get SR_CONF_PROBE_COUPLING failed.");
      return false;
    }
  } else {
    _signal->_acCoupling = _signal->_model ? _signal->_model->coupling() : 0;
  }

  // -- vpos
  if (probe) {
    ret = _signal->_data_source->device()->get_config_uint16(SR_CONF_PROBE_OFFSET,
                                                   _signal->_zero_offset, probe, NULL);
    if (!ret) {
      pxv_err("ERROR: config_get SR_CONF_PROBE_OFFSET failed.");
      return false;
    }
  } else {
    _signal->_zero_offset = _signal->_model ? (int)_signal->_model->vertical_offset() : 0;
  }

  // -- trig_value
  if (probe) {
    ret = _signal->_data_source->device()->get_config_byte(SR_CONF_TRIGGER_VALUE,
                                                 _signal->_trig_value, probe, NULL);
    if (ret) {
      _signal->_trig_delta = _signal->get_trig_vrate() - get_zero_ratio();
    } else {
      pxv_err("ERROR: config_get SR_CONF_TRIGGER_VALUE failed.");

      if (_signal->_data_source->device()->is_hardware())
        return false;
    }
  } else {
    _signal->_trig_value = _signal->_model ? _signal->_model->trig_value() : 0;
    _signal->_trig_delta = _signal->get_trig_vrate() - get_zero_ratio();
  }

  if (_signal->_view) {
    _signal->_view->set_update(_signal->_viewport, true);
    _signal->_view->update();
  }
  return true;
}

int DsoHardwareConfig::commit_settings() {
  sr_channel *probe = _signal->_model ? _signal->_model->sr_channel_handle() : nullptr;
  if (!probe)
    return 0;

  // -- enable
  _signal->_model->set_probe_enabled(_signal->enabled(), probe);

  // -- vdiv
  _signal->_model->set_vdiv((double)_signal->_vDial->get_value());
  _signal->_model->set_probe_factor(_signal->_vDial->get_factor(), probe);

  // -- coupling
  _signal->_model->set_coupling((int)_signal->_acCoupling);

  // -- offset
  _signal->_model->set_probe_offset((uint16_t)_signal->_zero_offset, probe);

  // -- trig_value
  _signal->_model->set_trigger_value((double)_signal->_trig_value, probe);

  return 1;
}

uint64_t DsoHardwareConfig::get_vDialValue() { return _signal->_vDial->get_value(); }

uint16_t DsoHardwareConfig::get_vDialSel() { return _signal->_vDial->get_sel(); }

void DsoHardwareConfig::set_acCoupling(uint8_t coupling) {
  // Same nested-broadcast guard as set_zero_ratio.
  auto model = _signal->_model;

  if (_signal->enabled()) {
    _signal->_acCoupling = coupling;
    // Task 7.2: 写回 Core SignalModel + 广播（用户交互入口：mouse_press AC/DC
    // 切换）。
    if (model) {
      model->set_coupling((int)coupling);
    }
  }
}

int DsoHardwareConfig::ratio2value(double ratio) {
  return ratio * (_signal->_ref_max - _signal->_ref_min) + _signal->_ref_min;
}

int DsoHardwareConfig::ratio2pos(double ratio) {
  return ratio * _signal->get_view_rect().height() + _signal->get_view_rect().top();
}

double DsoHardwareConfig::value2ratio(int value) {
  return max(0.0, (value - _signal->_ref_min) / (_signal->_ref_max - _signal->_ref_min));
}

double DsoHardwareConfig::pos2ratio(int pos) {
  return min(max(pos - _signal->get_view_rect().top(), 0), _signal->get_view_rect().height()) *
         1.0 / _signal->get_view_rect().height();
}

int DsoHardwareConfig::get_zero_vpos() { return ratio2pos(get_zero_ratio()); }

double DsoHardwareConfig::get_zero_ratio() { return value2ratio(_signal->_zero_offset); }

int DsoHardwareConfig::get_hw_offset() {
  sr_channel *probe = _signal->_model ? _signal->_model->sr_channel_handle() : nullptr;
  if (_signal->_data_source->is_running_status()) {
    int hw_offset = _signal->_cached_hw_offset;
    if (probe && _signal->_data_source->device()->get_config_uint16(
                     SR_CONF_PROBE_HW_OFFSET, hw_offset, probe, NULL)) {
      _signal->_cached_hw_offset = hw_offset;
    }
  }
  return _signal->_cached_hw_offset;
}

void DsoHardwareConfig::set_zero_vpos(int pos) {
  if (_signal->enabled()) {
    set_zero_ratio(pos2ratio(pos));
    _signal->set_trig_ratio(_signal->_trig_delta + get_zero_ratio(), false);
  }
}

void DsoHardwareConfig::set_zero_ratio(double ratio) {
  // CRITICAL: Copy _model to a local shared_ptr BEFORE calling set_config_*.
  // set_config_uint16 -> config_changed -> broadcast_async<SampleCountUpdated>
  // is SYNCHRONOUS and can trigger nested reload -> signals_changed -> View
  // AllReplaced rebuild, which DELETES this DsoSignal (and its _model member).
  // After set_config returns, _model may be dangling. The local copy keeps the
  // SignalModel alive even if `this` is deleted mid-method.
  auto model = _signal->_model;
  _signal->_zero_offset = ratio2value(ratio);
  // Task 7.2: 写回 Core SignalModel。不广播：本方法亦被 mainwindow JSON
  // 恢复路径 (mainwindow.cpp restore_session) 调用，广播会触发 rebuild 循环。
  if (model) {
    model->set_zero_offset((double)_signal->_zero_offset);
  }
}

void DsoHardwareConfig::set_factor(uint64_t factor) {
  // Same nested-broadcast guard as set_zero_ratio.
  auto model = _signal->_model;
  sr_channel *probe = model ? model->sr_channel_handle() : nullptr;

  if (_signal->enabled()) {
    uint64_t prefactor = 0;
    bool ret;

    if (probe) {
      ret = _signal->_data_source->device()->get_config_uint64(SR_CONF_PROBE_FACTOR,
                                                     prefactor, probe, NULL);
      if (!ret) {
        pxv_err("ERROR: config_get SR_CONF_PROBE_FACTOR failed.");
        return;
      }
    } else {
      prefactor = model ? model->vfactor() : 1;
    }

    if (prefactor != factor) {
      _signal->_vDial->set_factor(factor);
      _signal->_view->set_update(_signal->_viewport, true);
      _signal->_view->update();
      // Task 7.2: 写回 Core SignalModel + 广播（用户交互入口：mouse_press
      // X1/X10/X100）。
      if (model) {
        model->set_vfactor((double)factor);
      }
    }
  }
}

uint64_t DsoHardwareConfig::get_factor() {
  sr_channel *probe = _signal->_model ? _signal->_model->sr_channel_handle() : nullptr;
  uint64_t factor;

  if (probe) {
    bool ret = _signal->_data_source->device()->get_config_uint64(SR_CONF_PROBE_FACTOR,
                                                        factor, probe, NULL);
    if (ret) {
      return factor;
    } else {
      pxv_err("ERROR: config_get SR_CONF_PROBE_FACTOR failed.");
      return 1;
    }
  } else {
    return _signal->_model ? _signal->_model->vfactor() : 1;
  }
}

} // namespace view
} // namespace pv
