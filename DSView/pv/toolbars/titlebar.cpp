/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
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

#include "titlebar.h"
#include <QStyle>
#include <QLabel> 
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QEvent>
#include <QMouseEvent> 
#include <QPainter>
#include <QStyleOption>
#include <assert.h>
#include <QTimer>
#include <QGuiApplication>
#include <QTabBar>

#include "../config/appconfig.h"
#include "../appcontrol.h"
#include "../dsvdef.h"
#include "../ui/fn.h"
#include "../log.h"

namespace pv {
namespace toolbars {

TitleBar::TitleBar(bool top, QWidget *parent, ITitleParent *titleParent, bool hasClose) :
    QWidget(parent)
{
   _minimizeButton = NULL;
   _maximizeButton = NULL;
   _closeButton = NULL;
   _moving = false;
   _is_draging = false;
   _parent = parent;
   _isTop = top;
   _hasClose = hasClose;
   _title = NULL;
   _is_native = false;
   _titleParent = titleParent;
   _is_done_moved = false;
   _is_able_drag = true;
   _tabBar = NULL;

    assert(parent);

    setObjectName("TitleBar");
    setContentsMargins(0,0,0,0);
    setFixedHeight(32);

    QHBoxLayout *lay1 = new QHBoxLayout(this);

    _title = new QLabel(this);
    lay1->addWidget(_title);

    if (_isTop) {
        _minimizeButton = new QToolButton(this);
        _minimizeButton->setObjectName("MinimizeButton");
        _minimizeButton->setFixedSize(46, 32);
        _minimizeButton->setIconSize(QSize(16, 16));
        _minimizeButton->setAutoRaise(true);
        _maximizeButton = new QToolButton(this);
        _maximizeButton->setObjectName("MaximizeButton");
        _maximizeButton->setFixedSize(46, 32);
        _maximizeButton->setIconSize(QSize(16, 16));
        _maximizeButton->setAutoRaise(true);

        lay1->addWidget(_minimizeButton);
        lay1->addWidget(_maximizeButton);

        connect(this, SIGNAL(normalShow()), parent, SLOT(showNormal()));
        connect(this, SIGNAL( maximizedShow()), parent, SLOT(showMaximized()));
        connect(_minimizeButton, SIGNAL( clicked()), parent, SLOT(showMinimized()));
        connect(_maximizeButton, SIGNAL( clicked()), this, SLOT(showMaxRestore()));
    }

    if (_isTop || _hasClose) {
        _closeButton= new QToolButton(this);
        _closeButton->setObjectName("CloseButton");
        _closeButton->setFixedSize(46, 32);
        _closeButton->setIconSize(QSize(16, 16));
        _closeButton->setAutoRaise(true);
        lay1->addWidget(_closeButton);
        connect(_closeButton, SIGNAL( clicked()), parent, SLOT(close()));
    }

    lay1->insertStretch(0, 500);
    lay1->insertStretch(1, 500);
    lay1->setContentsMargins(0,0,0,0);
    lay1->setSpacing(0);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    ADD_UI(this);
}

TitleBar::~TitleBar(){ 
    DESTROY_QT_OBJECT(_minimizeButton);
    DESTROY_QT_OBJECT(_maximizeButton);
    DESTROY_QT_OBJECT(_closeButton);

    REMOVE_UI(this);
}

void TitleBar::reStyle()
{
    QString iconPath = GetIconPath();

    if (_isTop) {
        _minimizeButton->setIcon(QIcon(iconPath+"/minimize.svg"));
        if (ParentIsMaxsized())
            _maximizeButton->setIcon(QIcon(iconPath+"/restore.svg"));
        else
            _maximizeButton->setIcon(QIcon(iconPath+"/maximize.svg"));
    }
    if (_isTop || _hasClose)
        _closeButton->setIcon(QIcon(iconPath+"/close.svg"));
}

bool TitleBar::ParentIsMaxsized()
{
    if (_titleParent != NULL){
        return _titleParent->ParentIsMaxsized();
    } 
    else{
        return parentWidget()->isMaximized();
    }
}

void TitleBar::paintEvent(QPaintEvent *event)
{ 
    QWidget::paintEvent(event);
}

void TitleBar::setTitle(QString title)
{
    if (!_is_native){
        _title->setText(title);
    }
    else if (_parent != NULL){
        _parent->setWindowTitle(title);
    }    
}
  
QString TitleBar::title()
{
    if (!_is_native){
        return _title->text();
    }
    else if (_parent != NULL){
        return _parent->windowTitle();
    }
    return "";
}

void TitleBar::showMaxRestore()
{
    QString iconPath = GetIconPath();
    if (ParentIsMaxsized()) {
        _maximizeButton->setIcon(QIcon(iconPath+"/maximize.svg"));
        normalShow();
    } else {
        _maximizeButton->setIcon(QIcon(iconPath+"/restore.svg"));
        maximizedShow();
    }   
}

void TitleBar::setRestoreButton(bool max)
{
    QString iconPath = GetIconPath();
    if (!max) {
        _maximizeButton->setIcon(QIcon(iconPath+"/maximize.svg"));
    } else {
        _maximizeButton->setIcon(QIcon(iconPath+"/restore.svg"));
    }
}

bool TitleBar::isOnTabBar(const QPoint &pos) const
{
    if (!_tabBar || !_tabBar->isVisible())
        return false;

    QWidget *child = childAt(pos);
    dsv_info("TitleBar::isOnTabBar pos=%d,%d child=%s tabBar=%p tabBarVis=%d tabBarGeom=%d,%d %dx%d",
        pos.x(), pos.y(),
        child ? child->objectName().toLocal8Bit().constData() : "null",
        _tabBar, _tabBar->isVisible(),
        _tabBar->x(), _tabBar->y(), _tabBar->width(), _tabBar->height());

    if (!child)
        return false;

    if (child == _tabBar)
        return true;

    QWidget *w = child;
    while (w) {
        dsv_info("  checking widget=%s (%p) vs tabBar=%p", w->metaObject()->className(), w, _tabBar);
        if (w == _tabBar)
            return true;
        w = w->parentWidget();
    }
    return false;
}
  
void TitleBar::mousePressEvent(QMouseEvent* event)
{
    dsv_info("TitleBar::mousePressEvent pos=%d,%d isOnTabBar=%d", event->pos().x(), event->pos().y(), isOnTabBar(event->pos()));

    if (isOnTabBar(event->pos())) {
        QWidget::mousePressEvent(event);
        return;
    }

    bool ableMove = !ParentIsMaxsized();

    if(event->button() == Qt::LeftButton && ableMove && _is_able_drag)
    {
        int x = event->pos().x();
        int y = event->pos().y();

        bool bTopWidow = AppControl::Instance()->GetTopWindow() == _parent;
        bool bClick = (x >= 6 && y >= 5 && x <= width() - 6);  //top window need resize hit check

        if (!bTopWidow || bClick ){
            _is_draging = true;

            _clickPos = event->globalPos();

            if (_titleParent != NULL){
                _oldPos = _titleParent->GetParentPos();
            }
            else{
                _oldPos = _parent->pos();
            }

            _is_done_moved = false;

            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{  
    if(_is_draging){ 

        int datX = 0;
        int datY = 0;

        datX = (event->globalPos().x() - _clickPos.x());
        datY = (event->globalPos().y() - _clickPos.y());

        int x = _oldPos.x() + datX;
        int y = _oldPos.y() + datY;

        if (!_moving){
            if (ABS_VAL(datX) >= 2 || ABS_VAL(datY) >= 2){
                _moving = true;
            }
            else{
                return;
            }
        }

        if (_titleParent != NULL){

            if (!_is_done_moved){
                _is_done_moved = true;
                _titleParent->MoveBegin();
            }

            _titleParent->MoveWindow(x, y);
        }
        else{

#ifdef _WIN32

        QRect screenRect = AppControl::Instance()->_screenRect;

        if (screenRect.width() > 0 && QGuiApplication::screens().size() > 1)
        {
            QRect rect = _parent->frameGeometry();

            if (x < screenRect.left()){
                x = screenRect.left();
            }
            if (x + _parent->frameGeometry().width() > screenRect.right())
            {
                x = screenRect.right() - _parent->frameGeometry().width();
            }
        }
#endif

            _parent->move(x, y);
        }
        
        event->accept();
        return;
    } 
    QWidget::mouseMoveEvent(event);
}

void TitleBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (_moving && _titleParent != NULL){
        _titleParent->MoveEnd();
    }
    _moving = false;
    _is_draging = false;
    QWidget::mouseReleaseEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{  
    QWidget::mouseDoubleClickEvent(event); 

    if (_isTop){ 

      QTimer::singleShot(200, this, [this](){
                showMaxRestore();
            });
    }
}

void TitleBar::UpdateLanguage()
{
    
}

void TitleBar::UpdateTheme()
{
    reStyle();
}

void TitleBar::UpdateFont()
{  
    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize+1);
    _title->setFont(font);
}

void TitleBar::EnableAbleDrag(bool bEnabled)
{
    _is_able_drag = bEnabled;
}

void TitleBar::setTabBar(QTabBar *tabBar)
{
    if (_tabBar) {
        _tabBar->setParent(nullptr);
    }
    _tabBar = tabBar;
    if (_tabBar) {
        _tabBar->setParent(this);
        QHBoxLayout *lay = qobject_cast<QHBoxLayout*>(layout());
        if (lay) {
            lay->insertWidget(0, _tabBar);
        }
        _tabBar->setShape(QTabBar::RoundedNorth);
        _tabBar->setDrawBase(false);
        _tabBar->setFixedHeight(32);
        _tabBar->setStyleSheet(
            "QTabBar {"
            "    background: transparent;"
            "}"
            "QTabBar::tab {"
            "    height: 28px;"
            "    padding: 2px 12px;"
            "    margin: 0px;"
            "    font-size: 12px;"
            "    color: #ffffff;"
            "    background: transparent;"
            "    border: none;"
            "}"
            "QTabBar::tab:selected {"
            "    background: rgba(255,255,255,0.25);"
            "    border-bottom: 2px solid #ffffff;"
            "}"
            "QTabBar::tab:hover:!selected {"
            "    background: rgba(255,255,255,0.12);"
            "}"
        );
    }
}

QTabBar* TitleBar::tabBar() const
{
    return _tabBar;
}

} // namespace toolbars
} // namespace pv
