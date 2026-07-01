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

#ifndef PXVIEW_PV_DATA_SIGNALMODEL_H
#define PXVIEW_PV_DATA_SIGNALMODEL_H

#include <string>

#include <libsigrok.h>
#include <QObject>
#include "pv/api/types.h"

namespace pv {

class SigSession;  // forward declaration — Core layer; full include in .cpp

namespace data {

class SignalModel : public QObject
{
    Q_OBJECT

public:
    enum LogicTrigType {
        NONTRIG = 0,
        POSTRIG,
        HIGTRIG,
        NEGTRIG,
        LOWTRIG,
        EDGTRIG
    };

public:
    SignalModel();
    ~SignalModel();

    // QObject disables copy; SignalModel is managed by pointer (new/delete).

    // ---- Channel identity ----
    inline int index() const { return _index; }
    void set_index(int index);

    inline const std::string &name() const { return _name; }
    void set_name(const std::string &name);

    // Returns api::ChannelType (Logic=0/Analog=1/Dso=2) for UI use. Do NOT
    // pass directly to libsigrok APIs — use sr_type() instead.
    inline api::ChannelType type() const { return _type; }
    void set_type(api::ChannelType type);

    /// Returns the libsigrok SR_CHANNEL_* value (LOGIC=10000/DSO=10001/
    /// ANALOG=10002) for this channel. Use this whenever a channel type is
    /// passed to libsigrok-style APIs (ds_*, get_snapshot, Trace base class,
    /// etc.) — type() returns the api::ChannelType enum which does NOT match
    /// the SR_CHANNEL_* constants and would cause NULL snapshots or wrong
    /// type dispatch.
    int sr_type() const;

    inline bool enabled() const { return _enabled; }
    void set_enabled(bool enabled);

    inline const std::string &color() const { return _color; }
    void set_color(const std::string &color);

    // ---- Probe configuration ----
    inline double vdiv() const { return _vdiv; }
    void set_vdiv(double vdiv);

    inline int coupling() const { return _coupling; }
    void set_coupling(int coupling);

    inline double vfactor() const { return _vfactor; }
    void set_vfactor(double vfactor);

    inline bool map_default() const { return _map_default; }
    void set_map_default(bool map_default);

    // ---- Trigger ----
    inline int trig_type() const { return _trig_type; }
    void set_trig_type(int trig_type);

    inline double trig_value() const { return _trig_value; }
    void set_trig_value(double v);

    bool commit_trig();

    // ---- DSO parameters ----
    inline double vertical_offset() const { return _vertical_offset; }
    void set_vertical_offset(double offset);

    inline double zero_offset() const { return _zero_offset; }
    void set_zero_offset(double offset);

    inline double hw_offset() const { return _hw_offset; }
    void set_hw_offset(double offset);

    // ---- Glitch filter ----
    inline bool glitch_filter_enabled() const { return _glitch_filter_enabled; }
    void set_glitch_filter_enabled(bool enabled);

    inline int glitch_filter_width() const { return _glitch_filter_width; }
    void set_glitch_filter_width(int width);

    // ---- Signal invert ----
    inline bool signal_invert_enabled() const { return _signal_invert_enabled; }
    void set_signal_invert_enabled(bool enabled);

    // ---- Snapshot association ----
    inline void *snapshot() const { return _snapshot; }
    void set_snapshot(void *snapshot);

    // ---- Device/session binding (Core layer only) ----
    // Injected by SigSession::init_signals() / reload() so the model can
    // write back to the underlying libsigrok sr_channel struct and the
    // DeviceAgent API. Both are weak references — SignalModel does NOT own
    // them.
    void set_session(SigSession *session) { _session = session; }
    void set_sr_channel(struct sr_channel *ch) { _sr_channel = ch; }

    /// Core-layer only. View layer MUST NOT call this — use property
    /// accessors (set_vdiv / set_coupling / ...) instead so the model can
    /// keep sr_channel and libsigrok in sync.
    inline struct sr_channel *sr_channel_handle() const { return _sr_channel; }

    /// Batch-sync all model fields back to the underlying sr_channel struct
    /// and to libsigrok via DeviceAgent. Used by SigSession::reload() to
    /// restore hardware config after SignalModel rebuild (e.g. when
    /// re-opening a session file). No-op in headless mode (no device).
    void commit_to_device();

signals:
    /// Emitted when visual properties (name, color, vdiv, etc.) change.
    void appearance_changed();

    /// Emitted when the enabled/disabled state changes.
    void visibility_changed();

    /// Emitted when set_trig_type() changes the trigger type.
    /// View layer LogicSignal connects to this to auto-sync its local _trig copy.
    void trig_type_changed(int trig_type);

private:
    int                 _index;
    std::string         _name;
    api::ChannelType    _type;
    bool                _enabled;
    std::string         _color;

    double              _vdiv;
    int                 _coupling;
    double              _vfactor;
    bool                _map_default;

    int                 _trig_type;
    double              _trig_value = 0.0;

    double              _vertical_offset;
    double              _zero_offset;
    double              _hw_offset;

    bool                _glitch_filter_enabled;
    int                 _glitch_filter_width;

    bool                _signal_invert_enabled;

    void               *_snapshot;

    // Weak references — see set_session() / set_sr_channel().
    SigSession         *_session = nullptr;
    struct sr_channel  *_sr_channel = nullptr;
};

} // namespace data
} // namespace pv

#endif // PXVIEW_PV_DATA_SIGNALMODEL_H
