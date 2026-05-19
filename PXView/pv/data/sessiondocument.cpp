/*
 * This file is part of the DSView project.
 *
 * Copyright (C) 2024 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "sessiondocument.h"
#include "../view/decodetrace.h"
#include <libsigrok.h>
#include "../deviceagent.h"
#include "../log.h"
#include <QDebug>

namespace pv {
namespace data {

class DecoderStack;
class DecoderModel;

SessionDocument::SessionDocument() :
    _dock_sample_rate(0),
    _dock_sample_limit(0),
    _dock_collect_mode(0),
    _dock_measure_fen_enabled(false),
    _samplerate(0),
    _samplelimits(0),
    _trigger_pos(0),
    _decoder_model(nullptr)
{
}

SessionDocument::~SessionDocument()
{
}

LogicSnapshot* SessionDocument::get_logic_snapshot()
{
    return get_active_logic();
}

AnalogSnapshot* SessionDocument::get_analog_snapshot()
{
    return get_active_analog();
}

DsoSnapshot* SessionDocument::get_dso_snapshot()
{
    return get_active_dso();
}

LogicSnapshot* SessionDocument::get_active_logic()
{
    return &_logic;
}

AnalogSnapshot* SessionDocument::get_active_analog()
{
    return &_analog;
}

DsoSnapshot* SessionDocument::get_active_dso()
{
    return &_dso;
}

void SessionDocument::copy_from_logic(LogicSnapshot *src)
{
    if (!src || src->empty())
        return;

    _logic.free_data();

    _logic._capacity = src->_capacity;
    _logic._channel_num = src->_channel_num;
    _logic._sample_count = src->_sample_count;
    _logic._total_sample_count = src->_total_sample_count;
    _logic._ring_sample_count = src->_ring_sample_count;
    _logic._unit_size = src->_unit_size;
    _logic._unit_bytes = src->_unit_bytes;
    _logic._unit_pitch = src->_unit_pitch;
    _logic._memory_failed = src->_memory_failed;
    _logic._last_ended = src->_last_ended;
    _logic._samplerate = src->_samplerate;
    _logic._ch_index = src->_ch_index;

    _logic._byte_fraction = src->_byte_fraction;
    _logic._ch_fraction = src->_ch_fraction;
    _logic._dest_ptr = NULL;
    memcpy(_logic._last_sample, src->_last_sample, sizeof(src->_last_sample));
    memcpy(_logic._last_calc_count, src->_last_calc_count, sizeof(src->_last_calc_count));
    _logic._is_loop = src->_is_loop;
    _logic._loop_offset = src->_loop_offset;
    _logic._able_free = src->_able_free;
    memcpy(_logic._cur_ref_block_indexs, src->_cur_ref_block_indexs, sizeof(src->_cur_ref_block_indexs));
    _logic._lst_free_block_index = src->_lst_free_block_index;

    for (size_t i = 0; i < src->_ch_data.size(); i++) {
        std::vector<LogicSnapshot::RootNode> new_channel;
        for (size_t j = 0; j < src->_ch_data[i].size(); j++) {
            const LogicSnapshot::RootNode &rn = src->_ch_data[i][j];
            LogicSnapshot::RootNode new_rn;
            new_rn.tog = rn.tog;
            new_rn.first = rn.first;
            new_rn.last = rn.last;
            for (unsigned int k = 0; k < LogicSnapshot::Scale; k++) {
                if (rn.lbp[k] != NULL) {
                    new_rn.lbp[k] = malloc(LogicSnapshot::LeafBlockSpace);
                    if (new_rn.lbp[k])
                        memcpy(new_rn.lbp[k], rn.lbp[k], LogicSnapshot::LeafBlockSpace);
                    else
                        _logic._memory_failed = true;
                } else {
                    new_rn.lbp[k] = NULL;
                }
            }
            new_channel.push_back(new_rn);
        }
        _logic._ch_data.push_back(std::move(new_channel));
    }
}

void SessionDocument::copy_from_analog(AnalogSnapshot *src)
{
    if (!src || src->empty())
        return;

    _analog.free_data();
    _analog.free_envelop();

    _analog._capacity = src->_capacity;
    _analog._channel_num = src->_channel_num;
    _analog._sample_count = src->_sample_count;
    _analog._total_sample_count = src->_total_sample_count;
    _analog._ring_sample_count = src->_ring_sample_count;
    _analog._unit_size = src->_unit_size;
    _analog._unit_bytes = src->_unit_bytes;
    _analog._unit_pitch = src->_unit_pitch;
    _analog._memory_failed = src->_memory_failed;
    _analog._last_ended = src->_last_ended;
    _analog._samplerate = src->_samplerate;
    _analog._ch_index = src->_ch_index;

    if (src->_data && src->_capacity > 0) {
        _analog._data = malloc(src->_capacity);
        if (_analog._data)
            memcpy(_analog._data, src->_data, src->_capacity);
        else
            _analog._memory_failed = true;
    }

    for (unsigned int i = 0; i < src->_channel_num; i++) {
        for (unsigned int level = 0; level < AnalogSnapshot::ScaleStepCount; level++) {
            const AnalogSnapshot::Envelope &src_env = src->_envelope_levels[i][level];
            AnalogSnapshot::Envelope &dst_env = _analog._envelope_levels[i][level];
            dst_env.length = src_env.length;
            dst_env.ring_length = src_env.ring_length;
            dst_env.count = src_env.count;
            dst_env.data_length = src_env.data_length;
            dst_env.samples = NULL;
            dst_env.max = NULL;
            dst_env.min = NULL;
            if (src_env.samples && src_env.count > 0) {
                dst_env.samples = (AnalogSnapshot::EnvelopeSample*)malloc(
                    src_env.count * sizeof(AnalogSnapshot::EnvelopeSample));
                if (dst_env.samples)
                    memcpy(dst_env.samples, src_env.samples,
                        src_env.count * sizeof(AnalogSnapshot::EnvelopeSample));
            }
        }
    }

    _analog._enabled_channel_indexs = src->_enabled_channel_indexs;
}

void SessionDocument::copy_from_dso(DsoSnapshot *src)
{
    if (!src || src->empty())
        return;

    _dso.free_data();
    _dso.free_envelop();

    _dso._capacity = src->_capacity;
    _dso._channel_num = src->_channel_num;
    _dso._sample_count = src->_sample_count;
    _dso._total_sample_count = src->_total_sample_count;
    _dso._ring_sample_count = src->_ring_sample_count;
    _dso._unit_size = src->_unit_size;
    _dso._unit_bytes = src->_unit_bytes;
    _dso._unit_pitch = src->_unit_pitch;
    _dso._memory_failed = src->_memory_failed;
    _dso._last_ended = src->_last_ended;
    _dso._samplerate = src->_samplerate;
    _dso._ch_index = src->_ch_index;

    for (size_t i = 0; i < src->_ch_data.size(); i++) {
        uint8_t *chan_buffer = (uint8_t*)malloc(src->_total_sample_count + 1);
        if (chan_buffer) {
            memcpy(chan_buffer, src->_ch_data[i], src->_total_sample_count + 1);
        } else {
            _dso._memory_failed = true;
        }
        _dso._ch_data.push_back(chan_buffer);
    }

    for (unsigned int i = 0; i < src->_channel_num; i++) {
        for (unsigned int level = 0; level < DsoSnapshot::ScaleStepCount; level++) {
            const DsoSnapshot::Envelope &src_env = src->_envelope_levels[i][level];
            DsoSnapshot::Envelope &dst_env = _dso._envelope_levels[i][level];
            dst_env.length = src_env.length;
            dst_env.data_length = src_env.data_length;
            dst_env.samples = NULL;
            if (src_env.samples && src_env.data_length > 0) {
                dst_env.samples = (DsoSnapshot::EnvelopeSample*)malloc(
                    src_env.data_length * sizeof(DsoSnapshot::EnvelopeSample));
                if (dst_env.samples)
                    memcpy(dst_env.samples, src_env.samples,
                        src_env.data_length * sizeof(DsoSnapshot::EnvelopeSample));
            }
        }
    }

    _dso._envelope_en = src->_envelope_en;
    _dso._envelope_done = src->_envelope_done;
    _dso._instant = src->_instant;
    _dso._threshold = src->_threshold;
    _dso._measure_voltage_factor1 = src->_measure_voltage_factor1;
    _dso._measure_voltage_factor2 = src->_measure_voltage_factor2;
    _dso._data_scale1 = src->_data_scale1;
    _dso._data_scale2 = src->_data_scale2;
    _dso._is_file = src->_is_file;
    _dso._ref_min = src->_ref_min;
    _dso._ref_max = src->_ref_max;
    _dso._data_out_off_range = src->_data_out_off_range;
}

void SessionDocument::set_samplerate(uint64_t rate)
{
    _samplerate = rate;
}

uint64_t SessionDocument::get_samplerate() const
{
    return _samplerate;
}

void SessionDocument::set_samplelimits(uint64_t limits)
{
    _samplelimits = limits;
}

uint64_t SessionDocument::get_samplelimits() const
{
    return _samplelimits;
}

void SessionDocument::set_trigger_pos(uint64_t pos)
{
    _trigger_pos = pos;
}

uint64_t SessionDocument::get_trigger_pos()
{
    return _trigger_pos;
}

double SessionDocument::get_sampletime() const
{
    if (_samplerate == 0)
        return 0;
    return _samplelimits * 1.0 / _samplerate;
}

bool SessionDocument::has_data()
{
    return !_logic.empty() || !_analog.empty() || !_dso.empty();
}

bool SessionDocument::empty()
{
    return !has_data();
}

void SessionDocument::clear()
{
    _logic.clear();
    _analog.clear();
    _dso.clear();
    _samplerate = 0;
    _samplelimits = 0;
    _trigger_pos = 0;
}

std::vector<DecoderStack*>& SessionDocument::get_decoder_stacks()
{
    return _decoder_stacks;
}

void SessionDocument::add_decoder_stack(DecoderStack *stack)
{
    if (stack)
        _decoder_stacks.push_back(stack);
}

void SessionDocument::remove_decoder_stack(DecoderStack *stack)
{
    auto it = std::find(_decoder_stacks.begin(), _decoder_stacks.end(), stack);
    if (it != _decoder_stacks.end())
        _decoder_stacks.erase(it);
}

DecoderModel* SessionDocument::get_decoder_model()
{
    return _decoder_model;
}

void SessionDocument::set_decoder_model(DecoderModel *model)
{
    _decoder_model = model;
}

std::vector<view::DecodeTrace*>& SessionDocument::get_decode_traces()
{
    return _decode_traces;
}

void SessionDocument::add_decode_trace(view::DecodeTrace *trace)
{
    if (trace) {
        _decode_traces.push_back(trace);
        if (trace->decoder()) {
            _decoder_stacks.push_back(trace->decoder());
        }
    }
}

void SessionDocument::remove_decode_trace(view::DecodeTrace *trace)
{
    auto it = std::find(_decode_traces.begin(), _decode_traces.end(), trace);
    if (it != _decode_traces.end()) {
        if (trace->decoder()) {
            auto sit = std::find(_decoder_stacks.begin(), _decoder_stacks.end(), trace->decoder());
            if (sit != _decoder_stacks.end())
                _decoder_stacks.erase(sit);
        }
        _decode_traces.erase(it);
    }
}

std::vector<view::Signal*>& SessionDocument::get_signals()
{
    return _signals;
}

std::vector<view::DecodeTrace*>& SessionDocument::get_decode_signals()
{
    return _decode_traces;
}

std::vector<view::SpectrumTrace*>& SessionDocument::get_spectrum_traces()
{
    return _spectrum_traces;
}

view::LissajousTrace* SessionDocument::get_lissajous_trace()
{
    return nullptr;
}

view::MathTrace* SessionDocument::get_math_trace()
{
    return nullptr;
}

uint64_t SessionDocument::cur_snap_samplerate()
{
    return _samplerate;
}

uint64_t SessionDocument::cur_samplelimits()
{
    return _samplelimits;
}

double SessionDocument::cur_sampletime()
{
    return _samplerate > 0 ? (_samplelimits * 1.0 / _samplerate) : 0.0;
}

double SessionDocument::cur_snap_sampletime()
{
    return _samplerate > 0 ? (_samplelimits * 1.0 / _samplerate) : 0.0;
}

data::Snapshot* SessionDocument::get_snapshot(int type)
{
    if (type == SR_CHANNEL_LOGIC)
        return get_active_logic();
    else if (type == SR_CHANNEL_ANALOG)
        return get_active_analog();
    else if (type == SR_CHANNEL_DSO)
        return get_active_dso();
    else
        return nullptr;
}

QJsonObject SessionDocument::signal_config_to_json() const
{
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
        ch_obj["vdiv"] = (qint64)ch.vdiv;
        ch_obj["coupling"] = ch.coupling;
        ch_obj["map_default"] = ch.map_default;
        ch_array.append(ch_obj);
    }
    obj["channels"] = ch_array;

    return obj;
}

void SessionDocument::signal_config_from_json(const QJsonObject &obj)
{
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
            cfg.vdiv = (uint64_t)ch_obj["vdiv"].toVariant().toULongLong();
            cfg.coupling = ch_obj["coupling"].toInt();
            cfg.map_default = ch_obj["map_default"].toBool();
            _signal_config.channels.push_back(cfg);
        }
    }

    _signal_config.is_valid = true;
    dsv_info("SessionDocument::save_signal_config() done, work_mode=%d ch_count=%d",
        _signal_config.work_mode, (int)_signal_config.channels.size());
}

void SessionDocument::save_signal_config(DeviceAgent *agent)
{
    if (!agent || !agent->have_instance()) {
        dsv_info("SessionDocument::save_signal_config() skip, agent=%p have_instance=%d",
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
        sr_channel *const probe = (sr_channel*)l->data;
        ChannelConfig cfg;
        cfg.index = (int)probe->index;
        cfg.enabled = probe->enabled;
        cfg.vdiv = 0;
        cfg.coupling = 0;
        cfg.map_default = true;

        if (mode == ANALOG || mode == DSO) {
            uint64_t vdiv;
            if (agent->get_config_uint64(SR_CONF_PROBE_VDIV, vdiv, probe, NULL))
                cfg.vdiv = vdiv;

            int coupling;
            if (agent->get_config_int16(SR_CONF_PROBE_COUPLING, coupling, probe, NULL))
                cfg.coupling = coupling;

            bool map_default = true;
            agent->get_config_bool(SR_CONF_PROBE_MAP_DEFAULT, map_default, probe, NULL);
            cfg.map_default = map_default;
        }

        _signal_config.channels.push_back(cfg);
    }

    _signal_config.is_valid = true;
}

void SessionDocument::apply_signal_config(DeviceAgent *agent)
{
    qDebug() << "SessionDocument::apply_signal_config() START is_valid=" << _signal_config.is_valid << "have_instance=" << (agent ? agent->have_instance() : 0);
    if (!agent || !agent->have_instance() || !_signal_config.is_valid) {
        dsv_info("SessionDocument::apply_signal_config() skip, agent=%p have_instance=%d is_valid=%d",
            agent, agent ? agent->have_instance() : 0, _signal_config.is_valid);
        return;
    }

    dsv_info("SessionDocument::apply_signal_config() work_mode=%d op_mode=%d ch_mode=%d",
        _signal_config.work_mode, _signal_config.operation_mode, _signal_config.channel_mode);

    int cur_mode = agent->get_work_mode();
    if (_signal_config.work_mode != cur_mode) {
        agent->set_config_int16(SR_CONF_DEVICE_MODE, _signal_config.work_mode);
    }

    agent->set_config_int16(SR_CONF_OPERATION_MODE, _signal_config.operation_mode);
    agent->set_config_int16(SR_CONF_CHANNEL_MODE, _signal_config.channel_mode);

    if (_signal_config.is_demo && !_signal_config.demo_operation_mode.isEmpty()) {
        agent->set_config_string(SR_CONF_PATTERN_MODE,
            _signal_config.demo_operation_mode.toLocal8Bit().data());
    }

    int mode = _signal_config.work_mode;
    int idx = 0;
    for (const GSList *l = agent->get_channels(); l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        if (idx < (int)_signal_config.channels.size()) {
            const ChannelConfig &cfg = _signal_config.channels[idx];
            agent->enable_probe(probe, cfg.enabled);

            if (mode == ANALOG || mode == DSO) {
                agent->set_config_uint64(SR_CONF_PROBE_VDIV, cfg.vdiv, probe, NULL);
                agent->set_config_int16(SR_CONF_PROBE_COUPLING, cfg.coupling, probe, NULL);
                agent->set_config_bool(SR_CONF_PROBE_MAP_DEFAULT, cfg.map_default, probe, NULL);
            }
        }
        idx++;
    }
}

void SessionDocument::apply_pending_config(DeviceAgent *agent)
{
    if (_pending_device_config.is_valid) {
        _signal_config = _pending_device_config;
        apply_signal_config(agent);
        _pending_device_config = SignalConfig();
    }
}

bool SessionDocument::has_signal_config() const
{
    return _signal_config.is_valid;
}

bool SessionDocument::has_pending_config() const
{
    return _pending_device_config.is_valid;
}

} // namespace data
} // namespace pv
