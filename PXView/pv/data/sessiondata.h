#ifndef PXVIEW_PV_DATA_SESSIONDATA_H
#define PXVIEW_PV_DATA_SESSIONDATA_H

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

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "analogsnapshot.h"
#include "dsosnapshot.h"
#include "logicsnapshot.h"
#include "../pxvdef.h" // GlitchFilterMode

namespace pv {

/**
 * SessionData — per-capture-frame snapshot bundle (logic + analog + dso)
 * plus the per-frame glitch-filter / signal-invert state.
 *
 * Extracted from sigsession.h (Task 19 / Phase A) so CaptureManager can
 * hold `SessionData*` members without including sigsession.h (which would
 * create a circular dependency: SigSession owns CaptureManager via
 * unique_ptr, CaptureManager references SessionData).
 *
 * Public members are intentional: the managers (CaptureManager /
 * DataFeedParser / FilterProcessor) read/write the snapshot pointers and
 * the filter state directly. This is a plain data struct, not an
 * encapsulated type.
 */
class SessionData {
public:
  SessionData();
  // Raw pointer getters (backward compat — returns nullptr if shared_ptr is null)
  data::LogicSnapshot *get_logic() {
    // Self-contained: ensure the snapshot carries its samplerate so the renderer
    // can read _data->samplerate() without any external pass-through. This covers
    // the path where rendering reads directly from SessionData (no SessionDocument).
    if (_logic && _cur_snap_samplerate > 0)
      _logic->set_samplerate((double)_cur_snap_samplerate);
    return _logic.get();
  }
  data::AnalogSnapshot *get_analog() {
    // Same samplerate injection as get_logic(): clear() creates a fresh
    // AnalogSnapshot with _samplerate=0. Without this, AnalogSignal::paint_mid
    // reads _data->samplerate() == 0 → samples_per_pixel == 0 → flat-line
    // waveform (all pixels show the same sample).
    if (_analog && _cur_snap_samplerate > 0)
      _analog->set_samplerate((double)_cur_snap_samplerate);
    return _analog.get();
  }
  data::DsoSnapshot *get_dso() {
    // Same samplerate injection as get_logic()/get_analog().
    if (_dso && _cur_snap_samplerate > 0)
      _dso->set_samplerate((double)_cur_snap_samplerate);
    return _dso.get();
  }

  // Shared_ptr getters — for zero-copy ownership sharing with SessionDocument.
  // The caller (copy_data_to_document) copies the shared_ptr, incrementing the
  // ref count. When SessionData::clear() resets its shared_ptr, the underlying
  // snapshot stays alive because SessionDocument still holds a reference.
  std::shared_ptr<data::LogicSnapshot> logic_shared() { return _logic; }
  std::shared_ptr<data::AnalogSnapshot> analog_shared() { return _analog; }
  std::shared_ptr<data::DsoSnapshot> dso_shared() { return _dso; }

  void clear();

  uint64_t _cur_snap_samplerate, _cur_samplelimits, _trig_pos;
  // Track B3: _logic_backup owned via unique_ptr (was raw pointer)
  std::unique_ptr<data::LogicSnapshot> _logic_backup;
  bool _glitch_filter_active, _signal_invert_active;
  bool _glitch_filter_auto_apply = false;  // 采集后自动重新应用滤波
  bool _show_glitch_filter_overlay = true; // 显示波形轨道红色滤波提示叠加层
  // 架构修复：用 channel_index 作 key（消除 View/Core 位置序号错位）
  std::map<int, uint32_t> _glitch_filter_thresholds;
  std::map<int, GlitchFilterMode> _glitch_filter_modes;
  std::vector<bool> _signal_invert_channels;

private:
  // shared_ptr enables zero-copy ownership sharing with SessionDocument.
  // clear() resets these to fresh instances; if SessionDocument also holds
  // a shared_ptr to the old snapshot, the old data stays alive (ref count > 0).
  std::shared_ptr<data::LogicSnapshot> _logic;
  std::shared_ptr<data::AnalogSnapshot> _analog;
  std::shared_ptr<data::DsoSnapshot> _dso;
};

} // namespace pv

#endif // PXVIEW_PV_DATA_SESSIONDATA_H
