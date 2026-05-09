/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
 * ... (License header omitted for brevity) ...
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

    // --- 优化的 Ribbon Panel ---
    _ribbonPanel = new QWidget(this);
    _ribbonPanel->setObjectName("RibbonPanel");
    _ribbonPanel->setContentsMargins(0,0,0,0);
    // VITAL: 初始强制固定高度为0，彻底锁死布局
    _ribbonPanel->setFixedHeight(0);

    _ribbonLayout = new QVBoxLayout(_ribbonPanel);
    _ribbonLayout->setContentsMargins(0,0,0,0);
    _ribbonLayout->setSpacing(0);

    _categoryStack = new QStackedWidget(_ribbonPanel);
    _categoryStack->setContentsMargins(0,0,0,0);
    // VITAL: 锁死内部堆栈高度，使其在父容器缩放时永远不重算布局
    _categoryStack->setFixedHeight(_ribbonExpandedHeight);
    
    // 移除 .hide()。让它一直处于显示状态，只是被 _ribbonPanel 的 0 高度"裁切"掉了。
    // 这样在展开时不会产生重建渲染树的卡顿。
    _ribbonLayout->addWidget(_categoryStack, 0, Qt::AlignTop);

    mainLayout->addWidget(_ribbonPanel);

    // --- 优化的动画引擎 ---
    // 直接操作 ribbonHeight 属性 (对应 int setRibbonHeight)
    _ribbonAnimation = new QPropertyAnimation(this, "ribbonHeight");
    
    // UI反馈建议：350ms 太长会有粘滞感，250ms 更符合现代流畅 UI 标准
    _ribbonAnimation->setDuration(250);
    
    // QEasingCurve::OutCubic: 像抽屉一样，开头快，结尾柔和，非常丝滑
    _ribbonAnimation->setEasingCurve(QEasingCurve::OutCubic);

    connect(_tabBar, &QTabBar::tabBarClicked, this, &TitleBar::onTabClicked);
    connect(_tabBar, &QTabBar::currentChanged, this, &TitleBar::onTabChanged);

    // TitleBar 高度固定为 titleRow(32) + ribbonPanel(动态)，使用 Fixed 避免布局抖动
    setFixedHeight(32); // 初始只有标题行高度

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
    line->setObjectName("RibbonSeparator");
    line->setFixedWidth(1);

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
    
    // 从当前高度动态开始，防止反向点击时跳跃
    _ribbonAnimation->setStartValue(_ribbonPanel->height());
    _ribbonAnimation->setEndValue(_ribbonExpandedHeight);
    _ribbonAnimation->setEasingCurve(QEasingCurve::OutCubic); // 展开时用 OutCubic
    _ribbonAnimation->start();
    _ribbonExpanded = true;
}

void TitleBar::hideRibbon()
{
    if (_ribbonAnimation->state() == QAbstractAnimation::Running) {
        _ribbonAnimation->stop();
    }
    _ribbonAnimation->setStartValue(_ribbonPanel->height());
    _ribbonAnimation->setEndValue(0);
    // 收起时可以用 InOutCubic 或者 OutCubic
    _ribbonAnimation->setEasingCurve(QEasingCurve::OutCubic);
    _ribbonAnimation->start();
    _ribbonExpanded = false;
}

void TitleBar::onTabClicked(int index)
{

    if (_ribbonExpanded && index == _tabBar->currentIndex()) {
        hideRibbon();
    } else {
        expandRibbon();
    }
}

void TitleBar::onTabChanged(int index)
{
    if (index >= 0 && index < _categoryStack->count()) {
        _categoryStack->setCurrentIndex(index);
    }
}

// 核心优化：直接固定高度，一次性解决布局计算，杜绝抽搐
int TitleBar::ribbonHeight() const
{
    return _ribbonPanel->height();
}

void TitleBar::setRibbonHeight(int h)
{
    // setFixedHeight 内部只触发一次 Layout Request，远胜于设置 Min/Max
    _ribbonPanel->setFixedHeight(h);
    
    // 同步更新 TitleBar 自身的固定高度，避免布局系统反复计算
    // titleRow 高度为 32，加上 ribbonPanel 的动态高度
    int totalHeight = 32 + h;
    setFixedHeight(totalHeight);
}

// 核心优化 2：替换掉极度耗性能的 childAt(pos)
bool TitleBar::isOnTabBar(const QPoint &pos) const
{
    // 直接用坐标几何判断，避免 O(N) 复杂度的控件树遍历
    if (_tabBar->rect().contains(_tabBar->mapFrom(this, pos))) {
        return true;
    }
    if (_ribbonExpanded && _ribbonPanel->rect().contains(_ribbonPanel->mapFrom(this, pos))) {
        return true;
    }
    return false;
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

void TitleBar::mousePressEvent(QMouseEvent* event)
{
    // 使用优化后的判断方法
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
