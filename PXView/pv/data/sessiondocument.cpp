/*
 * This file is part of the PXView project.
 *
 * Copyright (C) 2024 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "sessiondocument.h"
#include "../log.h"
#include "../sigsession.h"
#include "lissajousmodel.h"
#include "mathstack.h"
#include "signalmodel.h"
#include "spectrumstack.h"
#include <QDebug>
#include <libsigrok.h>

namespace pv {
namespace data {

class DecoderStack;
class DecoderModel;

SessionDocument::SessionDocument(SigSession *session)
    : _samplerate(0), _samplelimits(0), _trigger_pos(0),
      _decoder_model(nullptr),
      _signal_config_store(std::make_unique<SignalConfigStore>(session)) {}

SessionDocument::~SessionDocument() {}

LogicSnapshot *SessionDocument::get_logic_snapshot() {
  return get_active_logic();
}

AnalogSnapshot *SessionDocument::get_analog_snapshot() {
  return get_active_analog();
}

DsoSnapshot *SessionDocument::get_dso_snapshot() { return get_active_dso(); }

LogicSnapshot *SessionDocument::get_active_logic() { return &_logic; }

AnalogSnapshot *SessionDocument::get_active_analog() { return &_analog; }

DsoSnapshot *SessionDocument::get_active_dso() { return &_dso; }

void SessionDocument::copy_from_logic(LogicSnapshot *src) {
  if (!src || src->empty())
    return;

  _logic.copy_from(*src);
}

void SessionDocument::copy_from_analog(AnalogSnapshot *src) {
  if (!src || src->empty())
    return;

  _analog.copy_from(*src);
}

void SessionDocument::copy_from_dso(DsoSnapshot *src) {
  if (!src || src->empty())
    return;

  _dso.copy_from(*src);
}

void SessionDocument::set_samplerate(uint64_t rate) { _samplerate = rate; }

uint64_t SessionDocument::get_samplerate() const { return _samplerate; }

void SessionDocument::set_samplelimits(uint64_t limits) {
  _samplelimits = limits;
}

uint64_t SessionDocument::get_samplelimits() const { return _samplelimits; }

void SessionDocument::set_trigger_pos(uint64_t pos) { _trigger_pos = pos; }

uint64_t SessionDocument::get_trigger_pos() { return _trigger_pos; }

double SessionDocument::get_sampletime() const {
  if (_samplerate == 0)
    return 0;
  return _samplelimits * 1.0 / _samplerate;
}

bool SessionDocument::has_data() {
  return !_logic.empty() || !_analog.empty() || !_dso.empty();
}

bool SessionDocument::empty() { return !has_data(); }

void SessionDocument::clear() {
  _logic.clear();
  _analog.clear();
  _dso.clear();
  _samplerate = 0;
  _samplelimits = 0;
  _trigger_pos = 0;

  for (auto m : _signal_models) {
  }
  _signal_models.clear();

  for (auto s : _spectrum_stacks) {
  }
  _spectrum_stacks.clear();

  if (_math_stack) {
    _math_stack.reset();
    _math_stack = nullptr;
  }

  if (_lissajous_model) {
    delete _lissajous_model;
    _lissajous_model = nullptr;
  }
}

std::vector<std::shared_ptr<DecoderStack>> &
SessionDocument::get_decoder_stacks(SessionDocument *doc) {
  (void)doc; // A SessionDocument always returns its own stacks.
  return _decoder_stacks;
}

void SessionDocument::add_decoder_stack(std::shared_ptr<DecoderStack> stack) {
  if (stack)
    _decoder_stacks.push_back(stack);
}

void SessionDocument::remove_decoder_stack(std::shared_ptr<DecoderStack> stack) {
  auto it = std::find(_decoder_stacks.begin(), _decoder_stacks.end(), stack);
  if (it != _decoder_stacks.end())
    _decoder_stacks.erase(it);
}

DecoderModel *SessionDocument::get_decoder_model() { return _decoder_model; }

void SessionDocument::set_decoder_model(DecoderModel *model) {
  _decoder_model = model;
}

std::vector<std::shared_ptr<SignalModel>> &SessionDocument::get_signal_models() {
  return _signal_models;
}

std::vector<std::shared_ptr<SpectrumStack>> &SessionDocument::get_spectrum_stacks() {
  return _spectrum_stacks;
}

std::shared_ptr<MathStack> SessionDocument::get_math_stack() { return _math_stack; }

LissajousModel *SessionDocument::get_lissajous_model() {
  return _lissajous_model;
}

uint64_t SessionDocument::cur_snap_samplerate() { return _samplerate; }

uint64_t SessionDocument::cur_samplelimits() { return _samplelimits; }

double SessionDocument::cur_sampletime() {
  return _samplerate > 0 ? (_samplelimits * 1.0 / _samplerate) : 0.0;
}

double SessionDocument::cur_snap_sampletime() {
  return _samplerate > 0 ? (_samplelimits * 1.0 / _samplerate) : 0.0;
}

data::Snapshot *SessionDocument::get_snapshot(int type) {
  if (type == SR_CHANNEL_LOGIC)
    return get_active_logic();
  else if (type == SR_CHANNEL_ANALOG)
    return get_active_analog();
  else if (type == SR_CHANNEL_DSO)
    return get_active_dso();
  else
    return nullptr;
}

// Wrap SignalConfigStore's serialization and merge in triggerConfig from
// _trigger_config to keep .pxc format unchanged (trigger_config remains a
// SessionDocument-owned field).
QJsonObject SessionDocument::signal_config_to_json() const {
  QJsonObject obj = _signal_config_store->signal_config_to_json();
  obj["triggerConfig"] = _trigger_config.to_json();
  return obj;
}

void SessionDocument::signal_config_from_json(const QJsonObject &obj) {
  _signal_config_store->signal_config_from_json(obj);
  if (obj.contains("triggerConfig")) {
    _trigger_config.from_json(obj["triggerConfig"].toObject());
  }
}

void SessionDocument::set_trigger_config(const data::TriggerConfig &cfg) {
  _trigger_config = cfg;
}

} // namespace data
} // namespace pv
