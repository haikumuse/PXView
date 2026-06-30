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

#include "signalfactory.h"

#include <algorithm>

#include <QColor>
#include <QString>

#include <libsigrok.h>

#include "analogsignal.h"
#include "dsosignal.h"
#include "logicsignal.h"
#include "signal.h"

#include "../api/types.h"
#include "../data/datasource.h"
#include "../data/signalmodel.h"
#include "../deviceagent.h"
#include "../sigsession.h"

namespace pv {
namespace view {

/**
 * Locate the sr_channel from the device's channel list whose index matches
 * the SignalModel's index. Returns nullptr if the device is unavailable
 * (e.g. headless mode) or no matching channel is found.
 */
static sr_channel* find_probe_by_index(SigSession *session, int index)
{
    if (!session)
        return nullptr;

    DeviceAgent *device = session->get_device();
    if (!device || !device->have_instance())
        return nullptr;

    GSList *channels = device->get_channels();
    for (GSList *l = channels; l; l = l->next) {
        sr_channel *probe = static_cast<sr_channel*>(l->data);
        if (probe && probe->index == index)
            return probe;
    }
    return nullptr;
}

/**
 * Apply properties (name, color, enabled, visible) from a SignalModel to
 * an already-constructed view::Signal.
 */
static void apply_model_properties(Signal *signal, std::shared_ptr<data::SignalModel> model)
{
    if (!signal || !model)
        return;

    if (!model->name().empty())
        signal->set_name(QString::fromStdString(model->name()));

    if (!model->color().empty())
        signal->set_colour(QColor(QString::fromStdString(model->color())));

    signal->set_enabled(model->enabled());
    signal->set_visible(model->enabled());

    if (auto *logic_sig = dynamic_cast<LogicSignal*>(signal)) {
        logic_sig->set_trig(model->trig_type());
        // Establish live sync: subsequent SignalModel::set_trig_type() calls
        // will auto-update this LogicSignal's _trig via Qt signal/slot.
        // UniqueConnection prevents duplicate connections when apply_model_properties
        // is called again (e.g. via update_signals(Modified)).
        // Connection is auto-disconnected when either object is destroyed.
        QObject::connect(model.get(), &data::SignalModel::trig_type_changed,
                         logic_sig, &LogicSignal::set_trig,
                         Qt::UniqueConnection);
    }
}

Signal* SignalFactory::create_signal(std::shared_ptr<data::SignalModel> model, SigSession *session)
{
    if (!model || !session)
        return nullptr;

    // view::Signal construction requires an sr_channel* (probe).
    // Match the probe by index against the SignalModel's index.
    sr_channel *probe = find_probe_by_index(session, model->index());
    if (!probe)
        return nullptr;

    Signal *signal = nullptr;
    switch (model->type()) {
    case api::ChannelType::Logic:
        signal = new LogicSignal(get_logic_snapshot(session), probe);
        break;
    case api::ChannelType::Analog:
        signal = new AnalogSignal(get_analog_snapshot(session), probe);
        break;
    case api::ChannelType::Dso:
        signal = new DsoSignal(get_dso_snapshot(session), probe);
        break;
    default:
        return nullptr;
    }

    apply_model_properties(signal, model);
    return signal;
}

std::vector<Signal*> SignalFactory::create_signals(data::DataSource *source, SigSession *session)
{
    std::vector<Signal*> result;
    if (!source || !session)
        return result;

    auto &models = source->get_signal_models();
    result.reserve(models.size());
    for (auto model : models) {
        Signal *s = create_signal(model, session);
        if (s)
            result.push_back(s);
    }
    return result;
}

void SignalFactory::update_signals(std::vector<Signal*> &current_signals,
                                   data::DataSource *source,
                                   SigSession *session,
                                   SignalChangeEvent event)
{
    if (!source || !session) {
        if (event == AllReplaced) {
            for (auto *s : current_signals)
                delete s;
            current_signals.clear();
        }
        return;
    }

    auto &models = source->get_signal_models();

    switch (event) {
    case AllReplaced: {
        // Save UI state from existing signals, recreate all, restore state.
        std::map<int, SignalUiState> saved_state = save_ui_state(current_signals);

        for (auto *s : current_signals)
            delete s;
        current_signals.clear();

        current_signals = create_signals(source, session);

        restore_ui_state(current_signals, saved_state);

        break;
    }

    case Added: {
        // Create signals for models that have no matching signal yet.
        for (auto model : models) {
            bool exists = false;
            for (auto *s : current_signals) {
                if (s->get_index() == model->index()) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                Signal *new_sig = create_signal(model, session);
                if (new_sig)
                    current_signals.push_back(new_sig);
            }
        }
        break;
    }

    case Removed: {
        // Delete signals whose index no longer exists in the model list.
        std::vector<Signal*> to_remove;
        for (auto *s : current_signals) {
            bool found = false;
            for (auto model : models) {
                if (model->index() == s->get_index()) {
                    found = true;
                    break;
                }
            }
            if (!found)
                to_remove.push_back(s);
        }
        for (auto *s : to_remove) {
            auto it = std::find(current_signals.begin(), current_signals.end(), s);
            if (it != current_signals.end()) {
                current_signals.erase(it);
                delete s;
            }
        }
        break;
    }

    case Modified: {
        // Refresh properties of existing signals from the models.
        for (auto *s : current_signals) {
            for (auto model : models) {
                if (model->index() != s->get_index())
                    continue;
                apply_model_properties(s, model);
                break;
            }
        }
        break;
    }
    }
}

std::map<int, SignalFactory::SignalUiState> SignalFactory::save_ui_state(
        const std::vector<Signal*> &sig_list)
{
    std::map<int, SignalUiState> state;
    for (auto *s : sig_list) {
        if (!s)
            continue;
        SignalUiState ui;
        ui.channel_index = s->get_index();
        ui.selected = s->selected();
        ui.visible = s->visible();
        ui.view_index = s->get_view_index();
        ui.v_offset = s->get_v_offset();
        ui.own_height = s->get_own_height();
        state[ui.channel_index] = ui;
    }
    return state;
}

void SignalFactory::restore_ui_state(std::vector<Signal*> &sig_list,
                                     const std::map<int, SignalUiState> &saved_state)
{
    for (auto *s : sig_list) {
        if (!s)
            continue;
        auto it = saved_state.find(s->get_index());
        if (it == saved_state.end())
            continue;
        const SignalUiState &ui = it->second;
        s->select(ui.selected);
        s->set_visible(ui.visible);
        s->set_view_index(ui.view_index);
        s->set_v_offset(ui.v_offset);
        s->set_own_height(ui.own_height);
    }
}

data::LogicSnapshot* SignalFactory::get_logic_snapshot(SigSession *session)
{
    if (!session)
        return nullptr;
    return session->get_logic_snapshot();
}

data::AnalogSnapshot* SignalFactory::get_analog_snapshot(SigSession *session)
{
    if (!session)
        return nullptr;
    return session->get_analog_snapshot();
}

data::DsoSnapshot* SignalFactory::get_dso_snapshot(SigSession *session)
{
    if (!session)
        return nullptr;
    return session->get_dso_snapshot();
}

} // namespace view
} // namespace pv
