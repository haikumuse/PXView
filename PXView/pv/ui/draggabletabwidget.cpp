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
 * This program is distributed in the hope that will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "draggabletabwidget.h"
#include "draggabletabbar.h"
#include "../submainframe.h"

#include <QPushButton>
#include <QHBoxLayout>
#include <QStyle>
#include <QLineEdit>
#include <QTimer>
#include <QResizeEvent>

namespace pv {
namespace ui {

DraggableTabWidget::DraggableTabWidget(QWidget *parent)
    : QTabWidget(parent)
{
    _draggable_tab_bar = new DraggableTabBar(this);
    setTabBar(_draggable_tab_bar);

    setTabsClosable(true);
    setMovable(true);

    connect(_draggable_tab_bar, &DraggableTabBar::detachTab,
            this, &DraggableTabWidget::onDetachTab);
    connect(this, &QTabWidget::tabCloseRequested,
            this, &DraggableTabWidget::onTabCloseRequested);
    connect(_draggable_tab_bar, &DraggableTabBar::tabRenameRequested,
            this, &DraggableTabWidget::onTabRenameRequested);
    connect(_draggable_tab_bar, &DraggableTabBar::tabRenameRequested,
            this, &DraggableTabWidget::tabRenameRequested);
    connect(_draggable_tab_bar, &DraggableTabBar::tabCloseOthersRequested,
            this, &DraggableTabWidget::tabCloseOthersRequested);
    connect(_draggable_tab_bar, &DraggableTabBar::tabCloseRightRequested,
            this, &DraggableTabWidget::tabCloseRightRequested);

    _add_button = new QPushButton(QStringLiteral("+"), this);
    _add_button->setFixedSize(QSize(24, 24));
    _add_button->setToolTip(tr("New Tab"));
    _add_button->setCursor(Qt::PointingHandCursor);
    _add_button->setStyleSheet(
        QStringLiteral("QPushButton {"
            "border: none;"
            "font-size: 20px;"
            "font-weight: normal;"
            "padding: 0px;"
            "color: #aaa;"
            "background: transparent;"
            "}"
            "QPushButton:hover {"
            "color: #fff;"
            "}"));
    _add_button->raise();
    _add_button->show();

    connect(_add_button, &QPushButton::clicked,
            this, &DraggableTabWidget::newTabRequested);

    QTimer::singleShot(0, this, [this]() { update_add_button_position(); });
}

int DraggableTabWidget::addTab(QWidget *page, const QString &label)
{
    int result = QTabWidget::addTab(page, label);
    QTimer::singleShot(0, this, [this]() { update_add_button_position(); });
    return result;
}

int DraggableTabWidget::addTab(QWidget *page, const QIcon &icon, const QString &label)
{
    int result = QTabWidget::addTab(page, icon, label);
    QTimer::singleShot(0, this, [this]() { update_add_button_position(); });
    return result;
}

void DraggableTabWidget::update_add_button_position()
{
    if (count() == 0) {
        _add_button->move(4, 2);
        return;
    }

    QRect last_tab_rect = _draggable_tab_bar->tabRect(count() - 1);
    QPoint tab_bar_pos = _draggable_tab_bar->mapToParent(QPoint(0, 0));
    int x = tab_bar_pos.x() + last_tab_rect.right() + 4;
    int y = tab_bar_pos.y() + (_draggable_tab_bar->height() - _add_button->height()) / 2;
    _add_button->move(x, y);
    _add_button->raise();
}

void DraggableTabWidget::tabInserted(int index)
{
    QTabWidget::tabInserted(index);
    QTimer::singleShot(0, this, [this]() { update_add_button_position(); });
}

void DraggableTabWidget::tabRemoved(int index)
{
    QTabWidget::tabRemoved(index);
    QTimer::singleShot(0, this, [this]() { update_add_button_position(); });
}

void DraggableTabWidget::resizeEvent(QResizeEvent *event)
{
    QTabWidget::resizeEvent(event);
    update_add_button_position();
}

void DraggableTabWidget::onDetachTab(int index, const QPoint &dropPos)
{
    if (index < 0 || index >= count())
        return;

    QWidget *widget = this->widget(index);
    QString title = tabText(index);

    removeTab(index);

    SubMainFrame *floating_window = new SubMainFrame(widget, title);
    connect(floating_window, &SubMainFrame::windowClosed,
            this, &DraggableTabWidget::onDetachedWindowClosed);

    _detached_windows.append(QPointer<SubMainFrame>(floating_window));

    QPoint window_pos = dropPos - QPoint(100, 20);
    floating_window->showAt(window_pos, QSize(800, 600));

    emit tabDetached(index, widget, title);
}

void DraggableTabWidget::onTabCloseRequested(int index)
{
    if (index < 0 || index >= count())
        return;

    emit tabCloseRequested(index);
}

void DraggableTabWidget::onTabRenameRequested(int index)
{
    if (index < 0 || index >= count())
        return;

    QRect tab_rect = _draggable_tab_bar->tabRect(index);
    QLineEdit *editor = new QLineEdit(_draggable_tab_bar);
    editor->setGeometry(tab_rect.adjusted(2, 2, -2, -2));
    editor->setText(tabText(index));
    editor->selectAll();
    editor->setFocus();

    connect(editor, &QLineEdit::returnPressed, this, [this, editor, index]() {
        QString new_title = editor->text();
        setTabText(index, new_title);
        emit tabRenamed(index, new_title);
        editor->setParent(nullptr);
        editor->deleteLater();
    });

    connect(editor, &QLineEdit::editingFinished, this, [this, editor, index]() {
        if (editor->parent() != nullptr) {
            QString new_title = editor->text();
            setTabText(index, new_title);
            emit tabRenamed(index, new_title);
            editor->setParent(nullptr);
            editor->deleteLater();
        }
    });

    editor->show();
}

void DraggableTabWidget::onDetachedWindowClosed(QWidget *content, const QString &title)
{
    int idx = addTab(content, title);
    emit tabAttached(content, title);
    setCurrentIndex(idx);

    SubMainFrame *window = qobject_cast<SubMainFrame*>(sender());
    if (window) {
        _detached_windows.removeOne(QPointer<SubMainFrame>(window));
        window->deleteLater();
    }
}

void DraggableTabWidget::closeAllDetachedWindows()
{
    for (auto &ptr : _detached_windows) {
        if (ptr) {
            ptr->close();
        }
    }
    _detached_windows.clear();
}

} // namespace ui
} // namespace pv
