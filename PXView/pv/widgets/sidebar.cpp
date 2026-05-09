/*
 * This file is part of the DSView project.
 *
 * Copyright (C) 2025 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "sidebar.h"

#include <QFrame>
#include <QIcon>
#include "../ui/langresource.h"
#include "../config/appconfig.h"

namespace pv {
namespace widgets {

SideBar::SideBar(QWidget *parent) :
    QWidget(parent),
    _next_index(0)
{
    setObjectName("sidebar");

    _layout = new QVBoxLayout(this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(2);
    _layout->addStretch();

    ADD_UI(this);
}

SideBar::~SideBar()
{
    REMOVE_UI(this);
}

int SideBar::addItem(const QString &iconName, const char *textId, const QString &defaultText,
                     ItemType type, int drawerPageIndex)
{
    XToolButton *btn = new XToolButton(this);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setIconSize(QSize(24, 24));

    if (type == DockItem)
        btn->setCheckable(true);

    btn->setIcon(QIcon(GetIconPath() + "/" + iconName));
    btn->setText(L_S(STR_PAGE_TOOLBAR, textId, defaultText.toUtf8().constData()));

    _layout->insertWidget(_layout->count() - 1, btn);

    connect(btn, &XToolButton::clicked, this, &SideBar::onButtonClicked);

    ItemInfo info;
    info.index = _next_index;
    info.type = type;
    info.iconName = iconName;
    info.textId = textId;
    info.defaultText = defaultText;
    info.button = btn;
    info.drawerPageIndex = drawerPageIndex;
    _items.append(info);

    return _next_index++;
}

void SideBar::addSeparator()
{
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    _layout->insertWidget(_layout->count() - 1, line);
}

void SideBar::onButtonClicked()
{
    XToolButton *btn = qobject_cast<XToolButton*>(sender());
    if (!btn)
        return;

    for (int i = 0; i < _items.size(); i++) {
        if (_items[i].button == btn) {
            if (_items[i].type == DockItem) {
                for (int j = 0; j < _items.size(); j++) {
                    if (j != i && _items[j].type == DockItem)
                        _items[j].button->setChecked(false);
                }
                emit dockItemClicked(_items[i].index);
            } else {
                emit actionItemClicked(_items[i].index);
            }
            break;
        }
    }
}

void SideBar::setItemVisible(int index, bool visible)
{
    for (auto &item : _items) {
        if (item.index == index) {
            item.button->setVisible(visible);
            break;
        }
    }
}

void SideBar::setItemEnabled(int index, bool enabled)
{
    for (auto &item : _items) {
        if (item.index == index) {
            item.button->setEnabled(enabled);
            break;
        }
    }
}

void SideBar::setItemChecked(int index, bool checked)
{
    for (auto &item : _items) {
        if (item.index == index) {
            item.button->setChecked(checked);
            break;
        }
    }
}

void SideBar::clearAllChecked()
{
    for (auto &item : _items) {
        if (item.type == DockItem)
            item.button->setChecked(false);
    }
}

int SideBar::itemCount() const
{
    return _items.size();
}

const SideBar::ItemInfo* SideBar::getItem(int index) const
{
    for (int i = 0; i < _items.size(); i++) {
        if (_items[i].index == index)
            return &_items[i];
    }
    return nullptr;
}

void SideBar::UpdateLanguage()
{
    for (auto &item : _items) {
        item.button->setText(L_S(STR_PAGE_TOOLBAR, item.textId, item.defaultText.toUtf8().constData()));
    }
}

void SideBar::UpdateTheme()
{
    for (auto &item : _items) {
        item.button->setIcon(QIcon(GetIconPath() + "/" + item.iconName));
    }
}

void SideBar::UpdateFont()
{
    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
    for (auto &item : _items) {
        item.button->setFont(font);
    }
}

} // namespace widgets
} // namespace pv
