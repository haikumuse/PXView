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

#ifndef DSVIEW_PV_DATA_SESSIONDOCUMENT_H
#define DSVIEW_PV_DATA_SESSIONDOCUMENT_H

#include <stdint.h>
#include <vector>
#include "datasource.h"
#include "logicsnapshot.h"
#include "analogsnapshot.h"
#include "dsosnapshot.h"

namespace pv {
namespace view { class Signal; }
namespace view { class DecodeTrace; }
namespace view { class SpectrumTrace; }
namespace view { class LissajousTrace; }
namespace view { class MathTrace; }

namespace data {

class DecoderStack;
class DecoderModel;

class SessionDocument : public DataSource
{
public:
    SessionDocument();
    ~SessionDocument();

    LogicSnapshot* get_logic_snapshot() override;
    AnalogSnapshot* get_analog_snapshot() override;
    DsoSnapshot* get_dso_snapshot() override;

    LogicSnapshot* get_active_logic();
    AnalogSnapshot* get_active_analog();
    DsoSnapshot* get_active_dso();
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

    std::vector<DecoderStack*>& get_decoder_stacks();
    void add_decoder_stack(DecoderStack *stack);
    void remove_decoder_stack(DecoderStack *stack);
    DecoderModel* get_decoder_model() override;
    void set_decoder_model(DecoderModel *model);

    std::vector<view::DecodeTrace*>& get_decode_traces();
    void add_decode_trace(view::DecodeTrace *trace);
    void remove_decode_trace(view::DecodeTrace *trace);

    std::vector<view::Signal*>& get_signals() override;
    std::vector<view::DecodeTrace*>& get_decode_signals() override;
    std::vector<view::SpectrumTrace*>& get_spectrum_traces() override;
    view::LissajousTrace* get_lissajous_trace() override;
    view::MathTrace* get_math_trace() override;
    uint64_t cur_snap_samplerate() override;
    uint64_t cur_samplelimits() override;
    double cur_sampletime() override;
    double cur_snap_sampletime() override;
    data::Snapshot* get_snapshot(int type) override;

private:
    LogicSnapshot   _logic;
    AnalogSnapshot  _analog;
    DsoSnapshot     _dso;
    uint64_t        _samplerate;
    uint64_t        _samplelimits;
    uint64_t        _trigger_pos;
    std::vector<DecoderStack*> _decoder_stacks;
    DecoderModel    *_decoder_model;
    std::vector<view::DecodeTrace*> _decode_traces;
    std::vector<view::Signal*> _signals;
    std::vector<view::SpectrumTrace*> _spectrum_traces;
};

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_SESSIONDOCUMENT_H
