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

#ifndef PXVIEW_PV_VIEW_IVIEW_DELEGATES_H
#define PXVIEW_PV_VIEW_IVIEW_DELEGATES_H

#include <cstdint>
#include <vector>

class QColor;
class QString;

namespace pv {
namespace view {

/**
 * @brief Read-only interface to ViewLayout state.
 *
 * Phase 8 (Testability): extracted so that ViewCursors, ViewSignalSync,
 * ViewDerivedTraces, ViewDataSync etc. can depend on this abstract
 * interface instead of the concrete ViewLayout class. In unit tests,
 * a mock implementation can be substituted.
 *
 * Currently ViewLayout already provides these methods; the interface
 * simply formalizes the contract. Future refactoring can have
 * ViewLayout implement IViewLayout, and delegates accept IViewLayout*
 * instead of View* (eliminating the friend relationship).
 */
class IViewLayout {
public:
  virtual ~IViewLayout() = default;

  // -- Scale / offset state (read) --
  virtual double scale() const = 0;
  virtual int64_t offset() const = 0;
  virtual double maxscale() const = 0;
  virtual double minscale() const = 0;

  // -- Signal height state (read) --
  virtual int spanY() const = 0;
  virtual int signalHeight() const = 0;
  virtual int signalHeightScale() const = 0;

  // -- Scale / offset mutation --
  virtual void set_scale_offset(double scale, int64_t offset) = 0;
};

/**
 * @brief Read-only interface to ViewCursors state.
 *
 * Phase 8 (Testability): allows other delegates and rendering code
 * to query cursor state without depending on the concrete ViewCursors
 * class.
 */
class IViewCursors {
public:
  virtual ~IViewCursors() = default;

  virtual bool cursors_shown() const = 0;
  virtual bool trig_cursor_shown() const = 0;
  virtual bool search_cursor_shown() const = 0;
  virtual bool xcursors_shown() const = 0;
};

/**
 * @brief Read-only interface to ViewSignalSync state.
 *
 * Phase 8 (Testability): allows rendering code and tests to query
 * the signal list without depending on the concrete ViewSignalSync
 * class.
 */
class IViewSignalStore {
public:
  virtual ~IViewSignalStore() = default;

  virtual size_t signal_count() const = 0;
  virtual bool rebuild_in_progress() const = 0;
};

} // namespace view
} // namespace pv

#endif // PXVIEW_PV_VIEW_IVIEW_DELEGATES_H
