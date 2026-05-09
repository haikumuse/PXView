/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
 *
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
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

#ifndef DSVIEW_PV_TOOLBARS_TITLEBAR_H
#define DSVIEW_PV_TOOLBARS_TITLEBAR_H

#include <QWidget>
#include <QToolButton>
#include <QTabBar>
#include <QPropertyAnimation>
#include <QAction>
#include <QStackedWidget>

#include "../interface/icallbacks.h"
#include "../ui/uimanager.h"


class QHBoxLayout;
class QLabel;
class QVBoxLayout;

namespace pv {

class ITitleParent
{
public:
    virtual void MoveWindow(int x, int y)=0;
    virtual QPoint GetParentPos()=0;
    virtual bool ParentIsMaxsized()=0;
    virtual void MoveBegin()=0;
    virtual void MoveEnd()=0;
};

namespace toolbars {

class TitleBar : public QWidget, public IUiWindow
{
    Q_OBJECT
    // 使用 int 类型的 ribbonHeight 属性替代 qreal slideProgress，避免浮点精度丢失和布局抖动
    Q_PROPERTY(int ribbonHeight READ ribbonHeight WRITE setRibbonHeight)

public:
    TitleBar(bool top, QWidget *parent, ITitleParent *titleParent, bool hasClose);
    ~TitleBar();

    void setTitle(QString title);
    QString title();

    int addCategory(const QString &title);
    void addAction(int categoryIndex, QAction *action);
    void addSeparator(int categoryIndex);
    void retranslateUi(int categoryIndex, const QString &title);
    void expandRibbon();
    void hideRibbon();

    //IUiWindow
    void UpdateLanguage() override;
    void UpdateTheme() override;
    void UpdateFont() override;

    inline void set_native(){
        _is_native = true;
    }

    inline void update_font(){
        UpdateFont();
    }

    void EnableAbleDrag(bool bEnabled);

    // 用于动画的属性访问器
    int ribbonHeight() const;
    void setRibbonHeight(int h);

    // 查询动画状态，外部可以用此避免在动画期间做重计算
    inline bool isRibbonAnimating() const {
        return _ribbonAnimation && _ribbonAnimation->state() == QAbstractAnimation::Running;
    }

private:
    void reStyle();

    bool ParentIsMaxsized();
    bool isOnTabBar(const QPoint &pos) const;

signals:
    void normalShow();
    void maximizedShow();

public slots:
    void showMaxRestore();
    void setRestoreButton(bool max);
    inline bool IsMoving(){return _moving;}

private slots:
    void onTabClicked(int index);
    void onTabChanged(int index);

protected:
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);


    QToolButton *_minimizeButton;
    QToolButton *_maximizeButton;
    QToolButton *_closeButton;
    QLabel      *_title;
    QTabBar     *_tabBar;

    QWidget         *_ribbonPanel;
    QVBoxLayout     *_ribbonLayout;
    QStackedWidget  *_categoryStack;
    QList<QHBoxLayout*> _categoryLayouts;
    QPropertyAnimation *_ribbonAnimation;
    bool            _ribbonExpanded;
    int             _ribbonExpandedHeight;

    bool        _moving;
    bool        _is_draging;
    bool        _isTop;
    bool        _hasClose;
    QPoint      _clickPos;
    QPoint      _oldPos;
    QWidget     *_parent;
    bool        _is_native;
    ITitleParent    *_titleParent;
    bool        _is_done_moved;
    bool        _is_able_drag;
};

} // namespace toolbars
} // namespace pv

#endif // DSVIEW_PV_TOOLBARS_TITLEBAR_H
