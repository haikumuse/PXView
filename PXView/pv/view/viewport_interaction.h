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

#ifndef PXVIEW_PV_VIEW_VIEWPORT_INTERACTION_H
#define PXVIEW_PV_VIEW_VIEWPORT_INTERACTION_H

#include <QNativeGestureEvent>
#include <QPoint>

#include "edge_nav_button.h"

class QEvent;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

namespace pv {
namespace view {

class LogicSignal;
class Viewport;

// Interaction delegate extracted from Viewport (Phase F2).
// Holds a non-owning back-pointer to its Viewport and accesses the
// Viewport's private state through friend access. All Qt mouse/wheel/
// key/gesture event handling and the three mode-specific release
// handlers live here; Viewport keeps only thin Qt override forwarders.
class ViewportInteraction {
public:
  explicit ViewportInteraction(Viewport *viewport);
  ~ViewportInteraction();

  void mousePressEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
  void mouseDoubleClickEvent(QMouseEvent *event);
  void wheelEvent(QWheelEvent *event);
  void keyPressEvent(QKeyEvent *event);
  bool gestureEvent(QNativeGestureEvent *event);
  void leaveEvent(QEvent *event);

  void update_edge_nav_buttons();
  void navigate_to_edge(EdgeNavButton::Direction dir);

private:
  void onLogicMouseRelease(QMouseEvent *event);
  void onDsoMouseRelease(QMouseEvent *event);
  void onAnalogMouseRelease(QMouseEvent *event);
  LogicSignal *get_hovered_logic_signal(const QPoint &pos);

  Viewport *_viewport;
};

} // namespace view
} // namespace pv

#endif // PXVIEW_PV_VIEW_VIEWPORT_INTERACTION_H
