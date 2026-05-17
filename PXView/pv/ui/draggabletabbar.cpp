/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
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
#include <QPainter>
#include <QPaintEvent>
#include <QStyleOption>
#include <QStylePainter>
#include "qtcompat.h"

namespace pv {
namespace ui {

DraggableTabBar::DraggableTabBar(QWidget *parent)
    : QTabBar(parent),
      _drag_started(false),
      _drag_index(-1),
      _drag_preview(nullptr),
      _drag_outside(false)
{
    setDrawBase(false);
}

DraggableTabBar::~DraggableTabBar()
{
    destroy_drag_preview();
}

void DraggableTabBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QStylePainter p(this);

    for (int i = 0; i < count(); ++i) {
        if (!tabRect(i).isValid())
            continue;
        QStyleOptionTab opt;
        initStyleOption(&opt, i);
        p.drawControl(QStyle::CE_TabBarTab, opt);
    }
}

void DraggableTabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        _drag_index = tabAt(QT_COMPAT_POS(event));
        _drag_start_pos = QT_COMPAT_POS(event);
        _drag_started = false;

        if (_drag_index >= 0) {
            QRect tr = tabRect(_drag_index);
            _drag_offset = QT_COMPAT_POS(event) - tr.topLeft();
        }
    }
    QTabBar::mousePressEvent(event);
}

void DraggableTabBar::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton && _drag_index >= 0) {
        QPoint delta = QT_COMPAT_POS(event) - _drag_start_pos;
        int distance = delta.manhattanLength();

        if (!_drag_started && distance > _drag_threshold) {
            _drag_started = true;
        }

        if (_drag_started) {
            QPoint global_pos = mapToGlobal(QT_COMPAT_POS(event));
            QRect bar_rect = rect();

            if (count() > 0) {
                bar_rect.adjust(0, 0, 0, tabRect(count() - 1).bottom() - bar_rect.bottom());
            }

            if (!bar_rect.contains(QT_COMPAT_POS(event))) {
                if (!_drag_outside) {
                    _drag_outside = true;
                    create_drag_preview(_drag_index);
                }
                update_drag_preview_pos(global_pos);
                return;
            } else if (_drag_outside) {
                _drag_outside = false;
                destroy_drag_preview();
            }
        }
    }
    QTabBar::mouseMoveEvent(event);
}

void DraggableTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (_drag_outside && _drag_index >= 0) {
        QPoint global_pos = mapToGlobal(QT_COMPAT_POS(event));
        destroy_drag_preview();
        emit detachTab(_drag_index, global_pos);
    } else {
        destroy_drag_preview();
    }
    _drag_started = false;
    _drag_index = -1;
    _drag_outside = false;
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
    int index = tabAt(QT_COMPAT_POS(event));
    if (index >= 0) {
        emit tabRenameRequested(index);
    }
    QTabBar::mouseDoubleClickEvent(event);
}

void DraggableTabBar::create_drag_preview(int index)
{
    destroy_drag_preview();

    if (index < 0 || index >= count())
        return;

    QRect tab_rect = this->tabRect(index);
    QPixmap pixmap(tab_rect.size() * devicePixelRatioF());
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QStyleOptionTab opt;
    opt.initFrom(this);
    opt.rect = QRect(0, 0, tab_rect.width(), tab_rect.height());
    opt.text = tabText(index);
    opt.icon = tabIcon(index);
    opt.state = QStyle::State_Selected | QStyle::State_Enabled | QStyle::State_Active;
    opt.iconSize = iconSize();

    style()->drawControl(QStyle::CE_TabBarTab, &opt, &painter, this);

    _drag_preview = new QLabel(nullptr,
        static_cast<Qt::WindowFlags>(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint));
    _drag_preview->setAttribute(Qt::WA_ShowWithoutActivating);
    _drag_preview->setAttribute(Qt::WA_TranslucentBackground);
    _drag_preview->setWindowOpacity(0.8);
    _drag_preview->setPixmap(pixmap);
    _drag_preview->setFixedSize(pixmap.size() / devicePixelRatioF());
    _drag_preview->show();
}

void DraggableTabBar::update_drag_preview_pos(const QPoint &global_pos)
{
    if (_drag_preview) {
        _drag_preview->move(global_pos - _drag_offset);
    }
}

void DraggableTabBar::destroy_drag_preview()
{
    if (_drag_preview) {
        _drag_preview->close();
        _drag_preview->deleteLater();
        _drag_preview = nullptr;
    }
}

} // namespace ui
} // namespace pv
