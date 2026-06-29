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

#include "sessionsnapshot.h"
#include "analogsnapshot.h"
#include "decodermodel.h"
#include "dsosnapshot.h"
#include "logicsnapshot.h"
#include "snapshot.h"
#include "signalmodel.h"
#include "lissajousmodel.h"

#include <libsigrok.h>
#include <stdlib.h>
#include <string.h>

namespace pv {
namespace data {

SessionSnapshot::SessionSnapshot()
    : _samplerate(0), _samplelimits(0), _trig_pos(0), _decoder_model(NULL) {}

SessionSnapshot::~SessionSnapshot() {}

std::vector<SignalModel *> &SessionSnapshot::get_signal_models() {
  return _signal_models;
}

std::vector<DecoderStack *> &SessionSnapshot::get_decoder_stacks() {
  return _decoder_stacks;
}

std::vector<SpectrumStack *> &SessionSnapshot::get_spectrum_stacks() {
  return _spectrum_stacks;
}

MathStack *SessionSnapshot::get_math_stack() { return _math_stack; }

LissajousModel *SessionSnapshot::get_lissajous_model() {
  return _lissajous_model;
}

uint64_t SessionSnapshot::cur_snap_samplerate() {
  if (_samplerate == 0)
    return 1;
  return _samplerate;
}

uint64_t SessionSnapshot::cur_samplelimits() { return _samplelimits; }

double SessionSnapshot::cur_sampletime() {
  if (_samplerate == 0)
    return 0;
  return _samplelimits * 1.0 / _samplerate;
}

double SessionSnapshot::cur_snap_sampletime() {
  if (_samplerate == 0)
    return 0;
  return _samplelimits * 1.0 / _samplerate;
}

data::LogicSnapshot *SessionSnapshot::get_logic_snapshot() { return &_logic; }

data::AnalogSnapshot *SessionSnapshot::get_analog_snapshot() {
  return &_analog;
}

data::DsoSnapshot *SessionSnapshot::get_dso_snapshot() { return &_dso; }

data::Snapshot *SessionSnapshot::get_snapshot(int type) {
  if (type == SR_CHANNEL_LOGIC)
    return &_logic;
  else if (type == SR_CHANNEL_ANALOG)
    return &_analog;
  else if (type == SR_CHANNEL_DSO)
    return &_dso;
  else
    return NULL;
}

data::DecoderModel *SessionSnapshot::get_decoder_model() {
  return _decoder_model;
}

uint64_t SessionSnapshot::get_trigger_pos() { return _trig_pos; }

void SessionSnapshot::set_samplerate(uint64_t rate) {
  _samplerate = rate;
  if (rate > 0) {
    _logic.set_samplerate(rate);
    _analog.set_samplerate(rate);
    _dso.set_samplerate(rate);
  }
}

void SessionSnapshot::set_samplelimits(uint64_t limits) {
  _samplelimits = limits;
}

void SessionSnapshot::set_trigger_pos(uint64_t pos) { _trig_pos = pos; }

void SessionSnapshot::copy_from_logic(LogicSnapshot *src) {
  if (!src || src->empty())
    return;

  _logic.copy_from(*src);
}

void SessionSnapshot::copy_from_analog(AnalogSnapshot *src) {
  if (!src || src->empty())
    return;

  _analog.copy_from(*src);
}

void SessionSnapshot::copy_from_dso(DsoSnapshot *src) {
  if (!src || src->empty())
    return;

  _dso.copy_from(*src);
}

bool SessionSnapshot::load_from_file(const QString &file_name) {
  (void)file_name;
  return false;
}

} // namespace data
} // namespace pv
