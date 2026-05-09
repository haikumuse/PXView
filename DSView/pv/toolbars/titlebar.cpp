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
#include <QResizeEvent>
#include <QPainter>
#include <QStyleOption>
#include <assert.h>
#include <QTimer>
#include <QGuiApplication>
#include <QTabBar>
#include <QStackedWidget>
#include <QToolButton>
#include <QPropertyAnimation>
#include <QAction>
#include <QSpacerItem>

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
   _ribbonPanel = NULL;
   _ribbonLayout = NULL;
   _categoryStack = NULL;
   _ribbonAnimation = NULL;
   _ribbonExpanded = false;
   _ribbonExpandedHeight = 65;
   _slideProgress = 0.0;

    assert(parent);

    setObjectName("TitleBar");
    setContentsMargins(0,0,0,0);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);
    mainLayout->setSpacing(0);

    QWidget *titleRow = new QWidget(this);
    titleRow->setObjectName("TitleRow");
    titleRow->setFixedHeight(32);
    titleRow->setContentsMargins(0,0,0,0);

    QHBoxLayout *lay1 = new QHBoxLayout(titleRow);
    lay1->setContentsMargins(0,0,0,0);
    lay1->setSpacing(0);

    _tabBar = new QTabBar(titleRow);
    _tabBar->setDrawBase(false);
    _tabBar->setFixedHeight(32);
    _tabBar->setShape(QTabBar::RoundedNorth);
    _tabBar->setMinimumWidth(100);
    lay1->addWidget(_tabBar);

    lay1->addStretch(500);

    _title = new QLabel(titleRow);
    _title->setAlignment(Qt::AlignCenter);
    lay1->addWidget(_title);

    lay1->addStretch(500);

    if (_isTop) {
        _minimizeButton = new QToolButton(titleRow);
        _minimizeButton->setObjectName("MinimizeButton");
        _minimizeButton->setFixedSize(46, 32);
        _minimizeButton->setIconSize(QSize(16, 16));
        _minimizeButton->setAutoRaise(true);
        _maximizeButton = new QToolButton(titleRow);
        _maximizeButton->setObjectName("MaximizeButton");
        _maximizeButton->setFixedSize(46, 32);
        _maximizeButton->setIconSize(QSize(16, 16));
        _maximizeButton->setAutoRaise(true);

        lay1->addWidget(_minimizeButton);
        lay1->addWidget(_maximizeButton);

        connect(this, SIGNAL(normalShow()), parent, SLOT(showNormal()));
        connect(this, SIGNAL(maximizedShow()), parent, SLOT(showMaximized()));
        connect(_minimizeButton, SIGNAL(clicked()), parent, SLOT(showMinimized()));
        connect(_maximizeButton, SIGNAL(clicked()), this, SLOT(showMaxRestore()));
    }

    if (_isTop || _hasClose) {
        _closeButton= new QToolButton(titleRow);
        _closeButton->setObjectName("CloseButton");
        _closeButton->setFixedSize(46, 32);
        _closeButton->setIconSize(QSize(16, 16));
        _closeButton->setAutoRaise(true);
        lay1->addWidget(_closeButton);
        connect(_closeButton, SIGNAL(clicked()), parent, SLOT(close()));
    }

    mainLayout->addWidget(titleRow);

    _ribbonPanel = new QWidget(this);
    _ribbonPanel->setObjectName("RibbonPanel");
    _ribbonPanel->setContentsMargins(0,0,0,0);
    _ribbonPanel->setFixedHeight(0);

    _ribbonLayout = new QVBoxLayout(_ribbonPanel);
    _ribbonLayout->setContentsMargins(0,0,0,0);
    _ribbonLayout->setSpacing(0);

    _categoryStack = new QStackedWidget(_ribbonPanel);
    _categoryStack->setContentsMargins(0,0,0,0);
    _categoryStack->hide();
    _ribbonLayout->addWidget(_categoryStack);

    mainLayout->addWidget(_ribbonPanel);

    _ribbonAnimation = new QPropertyAnimation(this, "slideProgress");
    _ribbonAnimation->setDuration(150);
    _ribbonAnimation->setEasingCurve(QEasingCurve::OutCubic);

    connect(_ribbonAnimation, &QPropertyAnimation::finished, this, [this](){
        if (_slideProgress <= 0.01) {
            _categoryStack->hide();
        }
    });

    connect(_tabBar, &QTabBar::tabBarClicked, this, &TitleBar::onTabClicked);
    connect(_tabBar, &QTabBar::currentChanged, this, &TitleBar::onTabChanged);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    ADD_UI(this);
}

TitleBar::~TitleBar(){
    DESTROY_QT_OBJECT(_minimizeButton);
    DESTROY_QT_OBJECT(_maximizeButton);
    DESTROY_QT_OBJECT(_closeButton);

    REMOVE_UI(this);
}

int TitleBar::addCategory(const QString &title)
{
    int index = _tabBar->addTab(title);

    QWidget *categoryWidget = new QWidget(_categoryStack);
    categoryWidget->setContentsMargins(0,0,0,0);
    QHBoxLayout *categoryLayout = new QHBoxLayout(categoryWidget);
    categoryLayout->setContentsMargins(4,4,4,4);
    categoryLayout->setSpacing(6);
    categoryLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));

    _categoryStack->addWidget(categoryWidget);
    _categoryLayouts.append(categoryLayout);

    dsv_info("TitleBar::addCategory index=%d title='%s'", index, title.toUtf8().constData());

    return index;
}

void TitleBar::addAction(int categoryIndex, QAction *action)
{
    if (categoryIndex < 0 || categoryIndex >= _categoryLayouts.size()) {
        dsv_info("TitleBar::addAction invalid categoryIndex=%d", categoryIndex);
        return;
    }

    QHBoxLayout *layout = _categoryLayouts[categoryIndex];

    QToolButton *btn = new QToolButton;
    btn->setIconSize(QSize(32, 32));
    btn->setAutoRaise(true);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    if (action->menu()) {
        btn->setPopupMode(QToolButton::MenuButtonPopup);
    }

    btn->setCheckable(action->isCheckable());
    btn->setChecked(action->isChecked());

    if (action->icon().isNull()) {
        static QIcon defaultIcon(":/icons/light/gear.svg");
        action->setIcon(defaultIcon);
        dsv_info("TitleBar::addAction '%s' icon was NULL, set default", action->text().toUtf8().constData());
    }

    btn->setDefaultAction(action);

    int insertPos = layout->count() - 1;
    if (insertPos < 0) insertPos = 0;
    layout->insertWidget(insertPos, btn);

    dsv_info("TitleBar::addAction categoryIndex=%d action='%s'", categoryIndex, action->text().toUtf8().constData());
}

void TitleBar::addSeparator(int categoryIndex)
{
    if (categoryIndex < 0 || categoryIndex >= _categoryLayouts.size()) {
        dsv_info("TitleBar::addSeparator invalid categoryIndex=%d", categoryIndex);
        return;
    }

    QHBoxLayout *layout = _categoryLayouts[categoryIndex];

    QWidget *line = new QWidget();
    line->setFixedWidth(1);
    line->setStyleSheet("background:#555555");

    int insertPos = layout->count() - 1;
    if (insertPos < 0) insertPos = 0;
    layout->insertWidget(insertPos, line);
}

void TitleBar::retranslateUi(int categoryIndex, const QString &title)
{
    if (categoryIndex >= 0 && categoryIndex < _tabBar->count()) {
        _tabBar->setTabText(categoryIndex, title);
    }
}

void TitleBar::expandRibbon()
{
    if (_ribbonAnimation->state() == QAbstractAnimation::Running) {
        _ribbonAnimation->stop();
    }
    _categoryStack->show();
    _categoryStack->updateGeometry();
    _ribbonAnimation->setStartValue(_slideProgress);
    _ribbonAnimation->setEndValue(1.0);
    _ribbonAnimation->start();
    _ribbonExpanded = true;

    dsv_info("TitleBar::expandRibbon expandedHeight=%d", _ribbonExpandedHeight);
}

void TitleBar::hideRibbon()
{
    if (_ribbonAnimation->state() == QAbstractAnimation::Running) {
        _ribbonAnimation->stop();
    }
    _ribbonAnimation->setStartValue(_slideProgress);
    _ribbonAnimation->setEndValue(0.0);
    _ribbonAnimation->start();
    _ribbonExpanded = false;

    dsv_info("TitleBar::hideRibbon");
}

void TitleBar::onTabClicked(int index)
{
    dsv_info("TitleBar::onTabClicked index=%d expanded=%d currentIndex=%d",
        index, _ribbonExpanded, _tabBar->currentIndex());

    if (_ribbonExpanded && index == _tabBar->currentIndex()) {
        hideRibbon();
    } else {
        expandRibbon();
    }
}

void TitleBar::onTabChanged(int index)
{
    dsv_info("TitleBar::onTabChanged index=%d", index);

    if (index >= 0 && index < _categoryStack->count()) {
        _categoryStack->setCurrentIndex(index);
    }

    if (!_ribbonExpanded) {
        expandRibbon();
    }
}

qreal TitleBar::slideProgress() const
{
    return _slideProgress;
}

void TitleBar::setSlideProgress(qreal progress)
{
    progress = qBound(0.0, progress, 1.0);
    if (qFuzzyCompare(_slideProgress, progress))
        return;
    _slideProgress = progress;
    updateRibbonGeometry();
}

void TitleBar::updateRibbonGeometry()
{
    int visibleH = qRound(_ribbonExpandedHeight * _slideProgress);
    _ribbonPanel->setFixedHeight(visibleH);

    if (layout()) {
        layout()->activate();
    }
}

void TitleBar::reStyle()
{
    QString iconPath = GetIconPath();

    this->setStyleSheet(
        "QWidget#TitleBar {"
        "    background: #202020;"
        "}"
        "QWidget#TitleRow {"
        "    background: #202020;"
        "}"
        "QTabBar {"
        "    background: #202020;"
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
        "    background: transparent;"
        "    border-bottom: 2px solid #ffffff;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "    background: rgba(255,255,255,0.08);"
        "}"
        "QTabBar::tab:selected:hover {"
        "    background: transparent;"
        "}"
        "QWidget#RibbonPanel {"
        "    background: #2d2d2d;"
        "}"
        "QToolButton {"
        "    background: transparent;"
        "    color: #cccccc;"
        "    border: none;"
        "    padding: 2px;"
        "}"
        "QToolButton:hover {"
        "    background: rgba(255,255,255,0.08);"
        "}"
        "QToolButton:pressed {"
        "    background: rgba(255,255,255,0.15);"
        "}"
    );

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
    QWidget *child = childAt(pos);
    if (!child)
        return false;

    if (child == _tabBar || child == _ribbonPanel)
        return true;

    QWidget *w = child;
    while (w) {
        if (w == _tabBar || w == _ribbonPanel)
            return true;
        w = w->parentWidget();
    }
    return false;
}

void TitleBar::mousePressEvent(QMouseEvent* event)
{
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
        bool bClick = (x >= 6 && y >= 5 && x <= width() - 6);

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

} // namespace toolbars
} // namespace pv
