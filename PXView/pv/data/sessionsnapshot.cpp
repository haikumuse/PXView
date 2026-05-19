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
#include "../view/signal.h"
#include "analogsnapshot.h"
#include "decodermodel.h"
#include "dsosnapshot.h"
#include "leaf_block_pool.h"
#include "logicsnapshot.h"
#include "snapshot.h"

#include <libsigrok.h>
#include <stdlib.h>
#include <string.h>

namespace pv {
namespace data {

SessionSnapshot::SessionSnapshot()
    : _samplerate(0), _samplelimits(0), _trig_pos(0), _lissajous_trace(NULL),
      _math_trace(NULL), _decoder_model(NULL) {}

SessionSnapshot::~SessionSnapshot() {
  for (auto sig : _signals) {
    // Only delete copied signals (Logic/Analog), not referenced ones (DSO)
    int type = sig->signal_type();
    if (type == SR_CHANNEL_LOGIC || type == SR_CHANNEL_ANALOG) {
      delete sig;
    }
  }
  _signals.clear();
}

std::vector<view::Signal *> &SessionSnapshot::get_signals() { return _signals; }

std::vector<view::DecodeTrace *> &SessionSnapshot::get_decode_signals() {
  return _decode_traces;
}

std::vector<view::SpectrumTrace *> &SessionSnapshot::get_spectrum_traces() {
  return _spectrum_traces;
}

view::LissajousTrace *SessionSnapshot::get_lissajous_trace() {
  return _lissajous_trace;
}

view::MathTrace *SessionSnapshot::get_math_trace() { return _math_trace; }

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
  memcpy(_logic._last_calc_count, src->_last_calc_count,
         sizeof(src->_last_calc_count));
  _logic._is_loop = src->_is_loop;
  _logic._loop_offset = src->_loop_offset;
  _logic._able_free = src->_able_free;
  memcpy(_logic._cur_ref_block_indexs, src->_cur_ref_block_indexs,
         sizeof(src->_cur_ref_block_indexs));
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
          new_rn.lbp[k] =
              LeafBlockPool::instance().acquire(LogicSnapshot::LeafBlockSpace);
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

void SessionSnapshot::copy_from_analog(AnalogSnapshot *src) {
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
    for (unsigned int level = 0; level < AnalogSnapshot::ScaleStepCount;
         level++) {
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
        dst_env.samples = (AnalogSnapshot::EnvelopeSample *)malloc(
            src_env.count * sizeof(AnalogSnapshot::EnvelopeSample));
        if (dst_env.samples)
          memcpy(dst_env.samples, src_env.samples,
                 src_env.count * sizeof(AnalogSnapshot::EnvelopeSample));
      }
    }
  }

  _analog._enabled_channel_indexs = src->_enabled_channel_indexs;
}

void SessionSnapshot::copy_from_dso(DsoSnapshot *src) {
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
    uint8_t *chan_buffer = (uint8_t *)malloc(src->_total_sample_count + 1);
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
        dst_env.samples = (DsoSnapshot::EnvelopeSample *)malloc(
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

bool SessionSnapshot::load_from_file(const QString &file_name) {
  (void)file_name;
  return false;
}

} // namespace data
} // namespace pv
