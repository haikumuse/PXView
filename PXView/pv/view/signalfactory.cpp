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
#include <set>

#include <QColor>
#include <QString>

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
 * Apply properties (name, color, enabled, visible) from a SignalModel to
 * an already-constructed view::Signal.
 */
static void apply_model_properties(Signal *signal,
                                   std::shared_ptr<data::SignalModel> model) {
  if (!signal || !model)
    return;

  if (!model->name().empty())
    signal->set_name(QString::fromStdString(model->name()));

  if (!model->color().empty())
    signal->set_colour(QColor(QString::fromStdString(model->color())));

  signal->set_enabled(model->enabled());
  signal->set_visible(model->enabled());

  if (auto *logic_sig = dynamic_cast<LogicSignal *>(signal)) {
    logic_sig->set_trig(model->trig_type());
    // Establish live sync: subsequent SignalModel::set_trig_type() calls
    // will auto-update this LogicSignal's _trig via Qt signal/slot.
    // UniqueConnection prevents duplicate connections when
    // apply_model_properties is called again (e.g. via
    // update_signals(Modified)). Connection is auto-disconnected when either
    // object is destroyed.
    QObject::connect(model.get(), &data::SignalModel::trig_type_changed,
                     logic_sig, &LogicSignal::set_trig, Qt::UniqueConnection);
  }
}

Signal *SignalFactory::create_signal(std::shared_ptr<data::SignalModel> model,
                                     SigSession *session) {
  if (!model || !session)
    return nullptr;

  Signal *signal = nullptr;
  switch (model->type()) {
  case api::ChannelType::Logic:
    signal = new LogicSignal(get_logic_snapshot(session), model, session);
    break;
  case api::ChannelType::Analog:
    signal = new AnalogSignal(get_analog_snapshot(session), model, session);
    break;
  case api::ChannelType::Dso:
    signal = new DsoSignal(get_dso_snapshot(session), model, session);
    break;
  default:
    return nullptr;
  }

  apply_model_properties(signal, model);
  return signal;
}

std::vector<Signal *> SignalFactory::create_signals(data::DataSource *source,
                                                    SigSession *session) {
  std::vector<Signal *> result;
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

SignalFactory::SignalChangeEvent SignalFactory::compute_change_event(
    const std::vector<Signal *> &current_signals,
    const std::vector<std::shared_ptr<data::SignalModel>> &models) {
  // Empty current + non-empty models → first creation → AllReplaced
  if (current_signals.empty() && !models.empty())
    return AllReplaced;

  // Non-empty current + empty models → all removed → AllReplaced
  if (!current_signals.empty() && models.empty())
    return AllReplaced;

  // Both empty → no change, but Modified is safe fallback
  if (current_signals.empty() && models.empty())
    return Modified;

  // Build sets of channel indices
  std::set<int> current_indices;
  for (auto *sig : current_signals) {
    if (sig)
      current_indices.insert(sig->get_index());
  }

  std::set<int> model_indices;
  for (auto &model : models) {
    if (model)
      model_indices.insert(model->index());
  }

  // Check if index sets are identical → Modified (properties may have changed)
  if (current_indices == model_indices)
    return Modified;

  // Check if models is pure superset of current → Added
  // (all current indices exist in models, and models has extra indices)
  bool all_current_in_models = true;
  bool some_new_not_in_current = false;
  for (int idx : current_indices) {
    if (model_indices.find(idx) == model_indices.end()) {
      all_current_in_models = false;
      break;
    }
  }
  for (int idx : model_indices) {
    if (current_indices.find(idx) == current_indices.end()) {
      some_new_not_in_current = true;
      break;
    }
  }

  if (all_current_in_models && some_new_not_in_current)
    return Added;

  // Check if current is pure superset of models → Removed
  // (all model indices exist in current, and current has extra indices)
  bool all_models_in_current = true;
  bool some_current_not_in_models = false;
  for (int idx : model_indices) {
    if (current_indices.find(idx) == current_indices.end()) {
      all_models_in_current = false;
      break;
    }
  }
  for (int idx : current_indices) {
    if (model_indices.find(idx) == model_indices.end()) {
      some_current_not_in_models = true;
      break;
    }
  }

  if (all_models_in_current && some_current_not_in_models)
    return Removed;

  // Mixed: both additions and removals → conservative fallback to AllReplaced
  return AllReplaced;
}

void SignalFactory::update_signals(std::vector<Signal *> &current_signals,
                                   data::DataSource *source,
                                   SigSession *session,
                                   SignalChangeEvent event) {
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
    std::vector<Signal *> to_remove;
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

std::map<int, SignalFactory::SignalUiState>
SignalFactory::save_ui_state(const std::vector<Signal *> &sig_list) {
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

void SignalFactory::restore_ui_state(
    std::vector<Signal *> &sig_list,
    const std::map<int, SignalUiState> &saved_state) {
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

data::LogicSnapshot *SignalFactory::get_logic_snapshot(SigSession *session) {
  if (!session)
    return nullptr;
  return session->get_logic_snapshot();
}

data::AnalogSnapshot *SignalFactory::get_analog_snapshot(SigSession *session) {
  if (!session)
    return nullptr;
  return session->get_analog_snapshot();
}

data::DsoSnapshot *SignalFactory::get_dso_snapshot(SigSession *session) {
  if (!session)
    return nullptr;
  return session->get_dso_snapshot();
}

} // namespace view
} // namespace pv
