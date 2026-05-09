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

#ifndef DSVIEW_PV_WIDGETS_SIDEBAR_H
#define DSVIEW_PV_WIDGETS_SIDEBAR_H

#include <QToolBar>
#include <QVBoxLayout>
#include <QList>
#include "../ui/xtoolbutton.h"
#include "../ui/uimanager.h"

class QFrame;

namespace pv {
namespace widgets {

class SideBar : public QToolBar, public IUiWindow
{
    Q_OBJECT

public:
    enum ItemType { DockItem, ActionItem };

    struct ItemInfo {
        int index;
        ItemType type;
        QString iconName;
        const char *textId;
        QString defaultText;
        XToolButton *button;
        int drawerPageIndex;
    };

    explicit SideBar(QWidget *parent = nullptr);
    ~SideBar();

    int addItem(const QString &iconName, const char *textId, const QString &defaultText,
                ItemType type = DockItem, int drawerPageIndex = -1);
    void addSeparator();

    void setItemVisible(int index, bool visible);
    void setItemEnabled(int index, bool enabled);
    void setItemChecked(int index, bool checked);
    void clearAllChecked();

    int itemCount() const;
    const ItemInfo* getItem(int index) const;

signals:
    void dockItemClicked(int index);
    void actionItemClicked(int index);

private:
    void onButtonClicked();

    void UpdateLanguage() override;
    void UpdateTheme() override;
    void UpdateFont() override;

    QVBoxLayout *_layout;
    QWidget *_container;
    QList<ItemInfo> _items;
    int _next_index;
};

} // namespace widgets
} // namespace pv

#endif
