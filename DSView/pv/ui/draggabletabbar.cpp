/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
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

#include "draggabletabbar.h"

#include <QMouseEvent>
#include <QApplication>
#include <QMenu>
#include <QContextMenuEvent>

namespace pv {
namespace ui {

DraggableTabBar::DraggableTabBar(QWidget *parent)
    : QTabBar(parent),
      _drag_started(false),
      _drag_index(-1)
{
}

void DraggableTabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        _drag_index = tabAt(event->pos());
        _drag_start_pos = event->pos();
        _drag_started = false;
    }
    QTabBar::mousePressEvent(event);
}

void DraggableTabBar::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton && _drag_index >= 0) {
        QPoint delta = event->pos() - _drag_start_pos;
        int distance = delta.manhattanLength();

        if (!_drag_started && distance > _drag_threshold) {
            _drag_started = true;
        }

        if (_drag_started) {
            QPoint global_pos = mapToGlobal(event->pos());
            QRect bar_rect = rect();

            if (count() > 0) {
                bar_rect.adjust(0, 0, 0, tabRect(count() - 1).bottom() - bar_rect.bottom());
            }

            if (!bar_rect.contains(event->pos())) {
                emit detachTab(_drag_index, global_pos);
                _drag_started = false;
                _drag_index = -1;
                return;
            }
        }
    }
    QTabBar::mouseMoveEvent(event);
}

void DraggableTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    _drag_started = false;
    _drag_index = -1;
    QTabBar::mouseReleaseEvent(event);
}

void DraggableTabBar::contextMenuEvent(QContextMenuEvent *event)
{
    int index = tabAt(event->pos());
    if (index < 0) {
        QTabBar::contextMenuEvent(event);
        return;
    }

    QMenu menu(this);
    QAction *rename_action = menu.addAction(tr("Rename"));
    menu.addSeparator();
    QAction *close_action = menu.addAction(tr("Close"));
    QAction *close_others_action = menu.addAction(tr("Close Others"));
    QAction *close_right_action = menu.addAction(tr("Close All to the Right"));

    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == rename_action) {
        emit tabRenameRequested(index);
    } else if (chosen == close_action) {
        emit tabCloseRequested(index);
    } else if (chosen == close_others_action) {
        emit tabCloseOthersRequested(index);
    } else if (chosen == close_right_action) {
        emit tabCloseRightRequested(index);
    }
}

void DraggableTabBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    int index = tabAt(event->pos());
    if (index >= 0) {
        emit tabRenameRequested(index);
    }
    QTabBar::mouseDoubleClickEvent(event);
}

} // namespace ui
} // namespace pv
