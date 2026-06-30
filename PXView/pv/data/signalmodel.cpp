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

#include "signalmodel.h"

namespace pv {
namespace data {

SignalModel::SignalModel()
    : _index(0)
    , _type(api::ChannelType::Logic)
    , _enabled(false)
    , _vdiv(0.0)
    , _coupling(0)
    , _vfactor(1.0)
    , _map_default(true)
    , _trig_type(NONTRIG)
    , _trig_value(0.0)
    , _vertical_offset(0.0)
    , _zero_offset(0.0)
    , _hw_offset(0.0)
    , _glitch_filter_enabled(false)
    , _glitch_filter_width(0)
    , _signal_invert_enabled(false)
    , _snapshot(nullptr)
{
}

SignalModel::~SignalModel()
{
}

void SignalModel::set_index(int index) { _index = index; }
void SignalModel::set_name(const std::string &name) { _name = name; }
void SignalModel::set_type(api::ChannelType type) { _type = type; }
void SignalModel::set_enabled(bool enabled) { _enabled = enabled; }
void SignalModel::set_color(const std::string &color) { _color = color; }
void SignalModel::set_vdiv(double vdiv) { _vdiv = vdiv; }
void SignalModel::set_coupling(int coupling) { _coupling = coupling; }
void SignalModel::set_vfactor(double vfactor) { _vfactor = vfactor; }
void SignalModel::set_map_default(bool map_default) { _map_default = map_default; }
void SignalModel::set_trig_type(int trig_type) {
    if (_trig_type != trig_type) {
        _trig_type = trig_type;
        emit trig_type_changed(_trig_type);
    }
}
void SignalModel::set_trig_value(double v) { _trig_value = v; }

bool SignalModel::commit_trig()
{
    const uint16_t probe = static_cast<uint16_t>(_index);

    if (_trig_type == NONTRIG) {
        ds_trigger_probe_set(probe, 'X', 'X');
        return false;
    }

    ds_trigger_set_en(true);

    switch (_trig_type) {
    case POSTRIG:
        ds_trigger_probe_set(probe, 'R', 'X');
        break;
    case HIGTRIG:
        ds_trigger_probe_set(probe, '1', 'X');
        break;
    case NEGTRIG:
        ds_trigger_probe_set(probe, 'F', 'X');
        break;
    case LOWTRIG:
        ds_trigger_probe_set(probe, '0', 'X');
        break;
    case EDGTRIG:
        ds_trigger_probe_set(probe, 'C', 'X');
        break;
    default:
        ds_trigger_probe_set(probe, 'X', 'X');
        return false;
    }

    return true;
}

void SignalModel::set_vertical_offset(double offset) { _vertical_offset = offset; }
void SignalModel::set_zero_offset(double offset) { _zero_offset = offset; }
void SignalModel::set_hw_offset(double offset) { _hw_offset = offset; }
void SignalModel::set_glitch_filter_enabled(bool enabled) { _glitch_filter_enabled = enabled; }
void SignalModel::set_glitch_filter_width(int width) { _glitch_filter_width = width; }
void SignalModel::set_signal_invert_enabled(bool enabled) { _signal_invert_enabled = enabled; }
void SignalModel::set_snapshot(void *snapshot) { _snapshot = snapshot; }

} // namespace data
} // namespace pv
