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

#ifndef PXVIEW_PV_VIEW_DSO_HARDWARE_CONFIG_H
#define PXVIEW_PV_VIEW_DSO_HARDWARE_CONFIG_H

#include <QString>
#include <cstdint>

struct sr_channel;

namespace pv {
namespace view {

class DsoSignal;

/**
 * DsoHardwareConfig — delegate for DsoSignal hardware configuration.
 *
 * Extracted from DsoSignal (Phase G1 of modernize-view-layer-v2). Owns the
 * method bodies for vDial / coupling / factor / zero-offset / commit /
 * load-settings / ratio helpers. Holds a non-owning pointer back to the
 * parent DsoSignal and accesses its private state via friendship.
 *
 * The DsoSignal public API is preserved: DsoSignal keeps thin facade methods
 * that forward to this delegate.
 */
class DsoHardwareConfig
{
public:
    explicit DsoHardwareConfig(DsoSignal *signal);
    ~DsoHardwareConfig();

    // -- channel enable --
    void set_enable(bool enable);

    // -- vDial (vertical division) --
    void set_vDialActive(bool active);
    bool go_vDialPre(bool manul);
    bool go_vDialNext(bool manul);
    uint64_t get_vDialValue();
    uint16_t get_vDialSel();

    /**
     * Builds the _vDial dial from the device SR_CONF_PROBE_VDIV list.
     * Extracted from the DsoSignal constructors so the GVariant boilerplate
     * is not duplicated. If src is non-null, the sel/factor are copied from
     * src (clone-constructor path).
     */
    void init_vDial(DsoSignal *src = nullptr);

    // -- coupling --
    void set_acCoupling(uint8_t coupling);

    // -- factor (probe attenuation x1/x10/x100) --
    void set_factor(uint64_t factor);
    uint64_t get_factor();

    // -- zero offset (vertical position) --
    int get_zero_vpos();
    double get_zero_ratio();
    int get_hw_offset();
    void set_zero_vpos(int pos);
    void set_zero_ratio(double ratio);

    // -- ratio/value/pos conversion helpers (depend on _ref_min/_ref_max) --
    int ratio2value(double ratio);
    int ratio2pos(double ratio);
    double value2ratio(int value);
    double pos2ratio(int pos);

    // -- hardware settings load / commit --
    bool load_settings();
    int commit_settings();

private:
    DsoSignal *_signal;
};

} // namespace view
} // namespace pv

#endif // PXVIEW_PV_VIEW_DSO_HARDWARE_CONFIG_H
