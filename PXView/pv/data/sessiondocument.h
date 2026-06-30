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
#include "signalmodel.h"
#include "triggerconfig.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <map>
#include <stdint.h>
#include <vector>

class DeviceAgent;

namespace pv {
class TabContext;

namespace data {

class DecoderStack;
class SpectrumStack;
class MathStack;
class DecoderModel;

struct ChannelLayoutState {
  int view_index;
  int v_offset;
  int own_height;
  ChannelLayoutState() : view_index(-1), v_offset(0), own_height(-1) {}
};

struct ChannelConfig {
  int index;
  bool enabled;
  bool visible;
  uint64_t vdiv;
  int coupling;
  bool map_default;
  uint16_t hw_offset;
  uint16_t offset;
  uint16_t zero_offset;
  int trig_type;  // R2: Logic 通道触发类型 (SignalModel::LogicTrigType)，仅
                  // LOGIC 模式有意义
  int view_index; // UI 布局：通道在视图中的顺序，-1
                  // 表示未设置（按启用顺序派生）
  int v_offset;   // UI 布局：垂直偏移
  int own_height; // UI 布局：轨道高度，-1 表示自动高度

  ChannelConfig()
      : index(0), enabled(false), visible(true), vdiv(0), coupling(0),
        map_default(true), hw_offset(0), offset(0), zero_offset(0),
        trig_type(0), view_index(-1), v_offset(0), own_height(-1) {}
};

struct SignalConfig {
  int work_mode;
  int operation_mode;
  int channel_mode;
  bool is_demo;
  QString demo_operation_mode;
  std::vector<ChannelConfig> channels;
  bool is_valid;

  SignalConfig()
      : work_mode(0), operation_mode(0), channel_mode(0), is_demo(false),
        is_valid(false) {}
};

class SessionDocument : public DataSource {
public:
  SessionDocument();
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

  QJsonObject signal_config_to_json() const;
  void signal_config_from_json(const QJsonObject &obj);
  void save_signal_config(
      DeviceAgent *agent, const std::map<int, bool> &channel_visibility = {},
      const std::vector<std::shared_ptr<SignalModel>> &signal_models = {},
      const std::map<int, ChannelLayoutState> &channel_layout = {});
  void apply_signal_config(DeviceAgent *agent);
  void apply_pending_config(DeviceAgent *agent);
  bool has_signal_config() const;
  bool has_pending_config() const;
  const SignalConfig &get_signal_config() const { return _signal_config; }

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
  SignalConfig _signal_config;
  SignalConfig _pending_device_config;
  data::TriggerConfig _trigger_config;

  friend class pv::TabContext;
};

} // namespace data
} // namespace pv

#endif // PXVIEW_PV_DATA_SESSIONDOCUMENT_H
