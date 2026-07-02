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

#include "signalconfigstore.h"
#include "../deviceagent.h"
#include "../log.h"
#include "../sigsession.h"
#include "signalmodel.h"
#include <QDebug>
#include <libsigrok.h>

namespace pv {
namespace data {

SignalConfigStore::SignalConfigStore(SigSession *session) : _session(session) {}

SignalConfigStore::~SignalConfigStore() {}

QJsonObject SignalConfigStore::signal_config_to_json() const {
  QJsonObject obj;
  obj["work_mode"] = _signal_config.work_mode;
  obj["operation_mode"] = _signal_config.operation_mode;
  obj["channel_mode"] = _signal_config.channel_mode;
  obj["is_demo"] = _signal_config.is_demo;
  obj["demo_operation_mode"] = _signal_config.demo_operation_mode;

  QJsonArray ch_array;
  for (const auto &ch : _signal_config.channels) {
    QJsonObject ch_obj;
    ch_obj["index"] = ch.index;
    ch_obj["enabled"] = ch.enabled;
    ch_obj["visible"] = ch.visible;
    ch_obj["vdiv"] = (qint64)ch.vdiv;
    ch_obj["coupling"] = ch.coupling;
    ch_obj["map_default"] = ch.map_default;
    ch_obj["hw_offset"] = ch.hw_offset;
    ch_obj["offset"] = ch.offset;
    ch_obj["zero_offset"] = ch.zero_offset;
    ch_obj["trig_type"] = ch.trig_type;
    ch_obj["view_index"] = ch.view_index;
    ch_obj["v_offset"] = ch.v_offset;
    ch_obj["own_height"] = ch.own_height;
    ch_array.append(ch_obj);
  }
  obj["channels"] = ch_array;

  return obj;
}

void SignalConfigStore::signal_config_from_json(const QJsonObject &obj) {
  _signal_config.work_mode = obj["work_mode"].toInt();
  _signal_config.operation_mode = obj["operation_mode"].toInt();
  _signal_config.channel_mode = obj["channel_mode"].toInt();
  _signal_config.is_demo = obj["is_demo"].toBool();
  _signal_config.demo_operation_mode = obj["demo_operation_mode"].toString();

  _signal_config.channels.clear();
  if (obj.contains("channels")) {
    QJsonArray ch_array = obj["channels"].toArray();
    for (const auto &ch_val : ch_array) {
      QJsonObject ch_obj = ch_val.toObject();
      ChannelConfig cfg;
      cfg.index = ch_obj["index"].toInt();
      cfg.enabled = ch_obj["enabled"].toBool();
      cfg.visible =
          ch_obj.contains("visible") ? ch_obj["visible"].toBool() : cfg.enabled;
      cfg.vdiv = (uint64_t)ch_obj["vdiv"].toVariant().toULongLong();
      cfg.coupling = ch_obj["coupling"].toInt();
      cfg.map_default = ch_obj["map_default"].toBool();
      cfg.hw_offset = (uint16_t)ch_obj["hw_offset"].toInt();
      cfg.offset = (uint16_t)ch_obj["offset"].toInt();
      cfg.zero_offset = (uint16_t)ch_obj["zero_offset"].toInt();
      cfg.trig_type =
          ch_obj.contains("trig_type") ? ch_obj["trig_type"].toInt() : 0;
      cfg.view_index =
          ch_obj.contains("view_index") ? ch_obj["view_index"].toInt() : -1;
      cfg.v_offset =
          ch_obj.contains("v_offset") ? ch_obj["v_offset"].toInt() : 0;
      cfg.own_height =
          ch_obj.contains("own_height") ? ch_obj["own_height"].toInt() : -1;
      _signal_config.channels.push_back(cfg);
    }
  }

  _signal_config.is_valid = true;
  pxv_info(
      "SignalConfigStore::signal_config_from_json() done, work_mode=%d ch_count=%d",
      _signal_config.work_mode, (int)_signal_config.channels.size());
}

void SignalConfigStore::save_signal_config(
    const std::map<int, bool> &channel_visibility,
    const std::vector<std::shared_ptr<SignalModel>> &signal_models,
    const std::map<int, ChannelLayoutState> &channel_layout) {
  DeviceAgent *agent = _session ? _session->get_device() : nullptr;
  if (!agent || !agent->have_instance()) {
    pxv_info(
        "SignalConfigStore::save_signal_config() skip, agent=%p have_instance=%d",
        agent, agent ? agent->have_instance() : 0);
    return;
  }

  _signal_config.work_mode = agent->get_work_mode();

  int opt_mode;
  if (agent->get_config_int16(SR_CONF_OPERATION_MODE, opt_mode))
    _signal_config.operation_mode = opt_mode;

  int ch_mode;
  if (agent->get_config_int16(SR_CONF_CHANNEL_MODE, ch_mode))
    _signal_config.channel_mode = ch_mode;

  _signal_config.is_demo = agent->is_demo();

  if (_signal_config.is_demo)
    _signal_config.demo_operation_mode = agent->get_demo_operation_mode();

  _signal_config.channels.clear();
  int mode = _signal_config.work_mode;
  for (const GSList *l = agent->get_channels(); l; l = l->next) {
    sr_channel *const probe = (sr_channel *)l->data;
    ChannelConfig cfg;
    cfg.index = (int)probe->index;
    cfg.enabled = probe->enabled;
    // Preserve per-channel visibility from the View layer.
    // Fall back to probe->enabled for channels not in the map
    // (e.g. DSO channels where enabled == visible).
    auto vis_it = channel_visibility.find(cfg.index);
    cfg.visible =
        (vis_it != channel_visibility.end()) ? vis_it->second : probe->enabled;
    cfg.vdiv = 0;
    cfg.coupling = 0;
    cfg.map_default = true;

    if (mode == ANALOG || mode == DSO) {
      uint64_t vdiv;
      if (agent->get_config_uint64(SR_CONF_PROBE_VDIV, vdiv, probe, NULL))
        cfg.vdiv = vdiv;

      int coupling;
      if (agent->get_config_int16(SR_CONF_PROBE_COUPLING, coupling, probe,
                                  NULL))
        cfg.coupling = coupling;

      bool map_default = true;
      agent->get_config_bool(SR_CONF_PROBE_MAP_DEFAULT, map_default, probe,
                             NULL);
      cfg.map_default = map_default;

      cfg.hw_offset = probe->hw_offset;
      cfg.offset = probe->offset;
      cfg.zero_offset = probe->zero_offset;
    }

    // R2: 保存 Logic 通道触发类型 (trig_type 存于 SignalModel，不在 sr_channel
    // 中)
    if (mode == LOGIC) {
      for (auto m : signal_models) {
        if (m && m->index() == cfg.index) {
          cfg.trig_type = m->trig_type();
          break;
        }
      }
    }

    // UI 布局状态：从 channel_layout 按 index 匹配写入；map 中无此 index
    // 时保持默认值
    auto layout_it = channel_layout.find(cfg.index);
    if (layout_it != channel_layout.end()) {
      cfg.view_index = layout_it->second.view_index;
      cfg.v_offset = layout_it->second.v_offset;
      cfg.own_height = layout_it->second.own_height;
      pxv_info("SignalConfigStore::save_signal_config: channel %d layout saved: view_index=%d, v_offset=%d, own_height=%d",
               cfg.index, cfg.view_index, cfg.v_offset, cfg.own_height);
    } else {
      pxv_info("SignalConfigStore::save_signal_config: channel %d NOT in channel_layout map, keeping defaults (view_index=-1, v_offset=0, own_height=-1)",
               cfg.index);
    }

    _signal_config.channels.push_back(cfg);
  }

  pxv_info("SignalConfigStore::save_signal_config() done, work_mode=%d ch_count=%d, channel_layout param size=%d",
           mode, (int)_signal_config.channels.size(), (int)channel_layout.size());
  _signal_config.is_valid = true;
}

void SignalConfigStore::apply_signal_config() {
  DeviceAgent *agent = _session ? _session->get_device() : nullptr;
  qDebug() << "SignalConfigStore::apply_signal_config() START is_valid="
           << _signal_config.is_valid
           << "have_instance=" << (agent ? agent->have_instance() : 0);
  if (!agent || !agent->have_instance() || !_signal_config.is_valid) {
    pxv_info("SignalConfigStore::apply_signal_config() skip, agent=%p "
             "have_instance=%d is_valid=%d",
             agent, agent ? agent->have_instance() : 0,
             _signal_config.is_valid);
    return;
  }

  pxv_info("SignalConfigStore::apply_signal_config() work_mode=%d op_mode=%d "
           "ch_mode=%d",
           _signal_config.work_mode, _signal_config.operation_mode,
           _signal_config.channel_mode);

  int cur_mode = agent->get_work_mode();
  if (_signal_config.work_mode != cur_mode) {
    agent->set_config_int16(SR_CONF_DEVICE_MODE, _signal_config.work_mode);
  }

  agent->set_config_int16(SR_CONF_OPERATION_MODE,
                          _signal_config.operation_mode);
  agent->set_config_int16(SR_CONF_CHANNEL_MODE, _signal_config.channel_mode);

  if (_signal_config.is_demo && !_signal_config.demo_operation_mode.isEmpty()) {
    agent->set_config_string(
        SR_CONF_PATTERN_MODE,
        _signal_config.demo_operation_mode.toLocal8Bit().data());
  }

  int mode = _signal_config.work_mode;
  int idx = 0;
  for (const GSList *l = agent->get_channels(); l; l = l->next) {
    sr_channel *const probe = (sr_channel *)l->data;
    if (idx < (int)_signal_config.channels.size()) {
      const ChannelConfig &cfg = _signal_config.channels[idx];
      agent->enable_probe(probe, cfg.enabled);

      if (mode == ANALOG || mode == DSO) {
        agent->set_config_uint64(SR_CONF_PROBE_VDIV, cfg.vdiv, probe, NULL);
        agent->set_config_int16(SR_CONF_PROBE_COUPLING, cfg.coupling, probe,
                                NULL);
        agent->set_config_bool(SR_CONF_PROBE_MAP_DEFAULT, cfg.map_default,
                               probe, NULL);
        probe->hw_offset = cfg.hw_offset;
        probe->offset = cfg.offset;
        probe->zero_offset = cfg.zero_offset;
      }
    }
    idx++;
  }
}

void SignalConfigStore::apply_pending_config() {
  if (_pending_device_config.is_valid) {
    _signal_config = _pending_device_config;
    apply_signal_config();
    _pending_device_config = SignalConfig();
  }
}

bool SignalConfigStore::has_signal_config() const {
  return _signal_config.is_valid;
}

bool SignalConfigStore::has_pending_config() const {
  return _pending_device_config.is_valid;
}

} // namespace data
} // namespace pv
