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

#ifndef PXVIEW_PV_VIEW_SIGNALREBUILDER_H
#define PXVIEW_PV_VIEW_SIGNALREBUILDER_H

namespace pv {

namespace data {
struct SignalConfig;
} // namespace data

namespace view {

class View;

/**
 * SignalRebuilder — extracts signal rebuild logic from View
 * (purify-architecture-concepts Task 22).
 *
 * Owns the rebuild_signals / rebuild_signals_from_config /
 * signals_modified_refresh methods. View holds a unique_ptr<SignalRebuilder>
 * and forwards calls. Declared as a friend of View so it can touch private
 * members (same access pattern as the original inline methods).
 *
 * The methods are pure mechanical migrations of the original View methods:
 * all `xxx` (implicit this->xxx View member access) became `_view->xxx`.
 * No functional logic changed.
 */
class SignalRebuilder
{
public:
  explicit SignalRebuilder(View *view);
  ~SignalRebuilder();

  void rebuild_signals();
  void rebuild_signals_from_config(const pv::data::SignalConfig &config);
  void signals_modified_refresh();

private:
  View *_view;
};

} // namespace view
} // namespace pv

#endif // PXVIEW_PV_VIEW_SIGNALREBUILDER_H
