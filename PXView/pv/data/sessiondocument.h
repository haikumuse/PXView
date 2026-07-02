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

#ifndef PXVIEW_PV_DATA_SESSIONDOCUMENT_H
#define PXVIEW_PV_DATA_SESSIONDOCUMENT_H

#include "analogsnapshot.h"
#include "datasource.h"
#include "dsosnapshot.h"
#include "lissajousmodel.h"
#include "logicsnapshot.h"
#include "signalconfigstore.h"
#include "signalmodel.h"
#include "triggerconfig.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <map>
#include <memory>
#include <stdint.h>
#include <vector>

namespace pv {

class SigSession;
class TabContext;

namespace data {

class DecoderStack;
class SpectrumStack;
class MathStack;
class DecoderModel;

// SessionDocument is now a pure data container. Signal/pending config and
// DeviceAgent interaction have been extracted to SignalConfigStore
// (accessed via signal_config_store()). trigger_config remains here as a
// SessionDocument-owned field. The SigSession* is injected so SignalConfigStore
// can reach DeviceAgent via _session->get_device() without SessionDocument
// itself depending on DeviceAgent.
class SessionDocument : public DataSource {
public:
  explicit SessionDocument(SigSession *session);
  ~SessionDocument();

  LogicSnapshot *get_logic_snapshot() override;
  AnalogSnapshot *get_analog_snapshot() override;
  DsoSnapshot *get_dso_snapshot() override;

  LogicSnapshot *get_active_logic();
  AnalogSnapshot *get_active_analog();
  DsoSnapshot *get_active_dso();
  void copy_from_logic(LogicSnapshot *src);
  void copy_from_analog(AnalogSnapshot *src);
  void copy_from_dso(DsoSnapshot *src);

  void set_samplerate(uint64_t rate);
  uint64_t get_samplerate() const;

  void set_samplelimits(uint64_t limits);
  uint64_t get_samplelimits() const;

  void set_trigger_pos(uint64_t pos);
  uint64_t get_trigger_pos() override;

  double get_sampletime() const;

  bool has_data();
  bool empty();

  void clear();

  std::vector<std::shared_ptr<DecoderStack>> &
  get_decoder_stacks(SessionDocument *doc = nullptr) override;
  void add_decoder_stack(std::shared_ptr<DecoderStack> stack);
  void remove_decoder_stack(std::shared_ptr<DecoderStack> stack);
  DecoderModel *get_decoder_model() override;
  void set_decoder_model(DecoderModel *model);

  std::vector<std::shared_ptr<SignalModel>> &get_signal_models() override;
  std::vector<std::shared_ptr<SpectrumStack>> &get_spectrum_stacks() override;
  std::shared_ptr<MathStack> get_math_stack() override;
  LissajousModel *get_lissajous_model() override;
  uint64_t cur_snap_samplerate() override;
  uint64_t cur_samplelimits() override;
  double cur_sampletime() override;
  double cur_snap_sampletime() override;
  data::Snapshot *get_snapshot(int type) override;

  // --- Signal config forwarding (delegated to SignalConfigStore) ---
  // signal_config_to_json/from_json wrap the store's version and merge in
  // triggerConfig from _trigger_config to keep .pxc format unchanged.
  QJsonObject signal_config_to_json() const;
  void signal_config_from_json(const QJsonObject &obj);
  void save_signal_config(
      const std::map<int, bool> &channel_visibility = {},
      const std::vector<std::shared_ptr<SignalModel>> &signal_models = {},
      const std::map<int, ChannelLayoutState> &channel_layout = {}) {
    _signal_config_store->save_signal_config(channel_visibility, signal_models,
                                             channel_layout);
  }
  void apply_signal_config() { _signal_config_store->apply_signal_config(); }
  void apply_pending_config() {
    _signal_config_store->apply_pending_config();
  }
  bool has_signal_config() const {
    return _signal_config_store->has_signal_config();
  }
  bool has_pending_config() const {
    return _signal_config_store->has_pending_config();
  }
  const SignalConfig &get_signal_config() const {
    return _signal_config_store->get_signal_config();
  }
  // For TabContext to restore trig_type after reload (replaces friend access).
  const std::vector<ChannelConfig> &get_channels() const {
    return _signal_config_store->get_channels();
  }
  // For TabContext to save pending config (replaces friend access).
  void set_pending_config(const SignalConfig &cfg) {
    _signal_config_store->set_pending_config(cfg);
  }
  SignalConfigStore *signal_config_store() {
    return _signal_config_store.get();
  }

  inline const data::TriggerConfig &trigger_config() const {
    return _trigger_config;
  }
  inline data::TriggerConfig &trigger_config() { return _trigger_config; }
  void set_trigger_config(const data::TriggerConfig &cfg);

private:
  LogicSnapshot _logic;
  AnalogSnapshot _analog;
  DsoSnapshot _dso;
  uint64_t _samplerate;
  uint64_t _samplelimits;
  uint64_t _trigger_pos;
  std::vector<std::shared_ptr<DecoderStack>> _decoder_stacks;
  DecoderModel *_decoder_model;
  std::vector<std::shared_ptr<SignalModel>> _signal_models;
  std::vector<std::shared_ptr<SpectrumStack>> _spectrum_stacks;
  std::shared_ptr<MathStack> _math_stack = nullptr;
  LissajousModel *_lissajous_model = nullptr;
  std::unique_ptr<SignalConfigStore> _signal_config_store;
  data::TriggerConfig _trigger_config;
};

} // namespace data
} // namespace pv

#endif // PXVIEW_PV_DATA_SESSIONDOCUMENT_H
