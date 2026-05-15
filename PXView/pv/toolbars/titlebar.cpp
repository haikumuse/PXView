/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
 * ... (License header omitted for brevity) ...
 */

#include "titlebar.h"
#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleOption>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <assert.h>

#include "../appcontrol.h"
#include "../config/appconfig.h"
#include "../dsvdef.h"
#include "../log.h"
#include "../ui/fn.h"

namespace pv {
namespace toolbars {

static qreal cssBezierEasing(qreal t) {
  static const double x1 = 0.4, y1 = 0.0;
  static const double x2 = 0.2, y2 = 1.0;

  double cx = 3.0 * x1;
  double bx = 3.0 * (x2 - x1) - cx;
  double ax = 1.0 - cx - bx;

  double cy = 3.0 * y1;
  double by = 3.0 * (y2 - y1) - cy;
  double ay = 1.0 - cy - by;

  auto sampleCurveX = [&](double s) { return ((ax * s + bx) * s + cx) * s; };
  auto sampleCurveY = [&](double s) { return ((ay * s + by) * s + cy) * s; };

  double s = t;
  for (int i = 0; i < 8; i++) {
    double err = sampleCurveX(s) - t;
    if (qAbs(err) < 1e-7)
      break;
    double deriv = (3.0 * ax * s + 2.0 * bx) * s + cx;
    if (qAbs(deriv) < 1e-10)
      break;
    s -= err / deriv;
  }
  return qBound(0.0, sampleCurveY(s), 1.0);
}

static QEasingCurve makeTailwindCurve() {
  QEasingCurve curve;
  curve.setCustomType(cssBezierEasing);
  return curve;
}

TitleBar::TitleBar(bool top, QWidget *parent, ITitleParent *titleParent,
                   bool hasClose)
    : QMenuBar(parent) {
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
  _ribbonContainer = NULL;
  _ribbonPanel = NULL;
  _ribbonLayout = NULL;
  _categoryStack = NULL;
  _ribbonAnimation = NULL;
  _ribbonExpanded = false;
  _ribbonExpandedHeight = 65;
  _slideOffset = 65;
  _pinButton = NULL;
  _ribbonPinned = true;

  assert(parent);

  setObjectName("TitleBar");
  setContentsMargins(0, 0, 0, 0);

  QWidget *titleRow = new QWidget(this);
  titleRow->setObjectName("TitleRow");
  titleRow->setFixedHeight(32);
  titleRow->setContentsMargins(0, 0, 0, 0);

  QHBoxLayout *titleRowLayout = new QHBoxLayout(titleRow);
  titleRowLayout->setContentsMargins(0, 0, 0, 0);
  titleRowLayout->setSpacing(0);

  _tabBar = new QTabBar(titleRow);
  _tabBar->setDrawBase(false);
  _tabBar->setFixedHeight(32);
  _tabBar->setShape(QTabBar::RoundedNorth);
  _tabBar->setMinimumWidth(100);
  titleRowLayout->addWidget(_tabBar);

  titleRowLayout->addStretch(500);

  _title = new QLabel(titleRow);
  _title->setAlignment(Qt::AlignCenter);
  titleRowLayout->addWidget(_title);

  titleRowLayout->addStretch(500);

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

    titleRowLayout->addWidget(_minimizeButton);
    titleRowLayout->addWidget(_maximizeButton);

    connect(this, SIGNAL(normalShow()), parent, SLOT(showNormal()));
    connect(this, SIGNAL(maximizedShow()), parent, SLOT(showMaximized()));
    connect(_minimizeButton, SIGNAL(clicked()), parent, SLOT(showMinimized()));
    connect(_maximizeButton, SIGNAL(clicked()), this, SLOT(showMaxRestore()));
  }

  if (_isTop || _hasClose) {
    _closeButton = new QToolButton(titleRow);
    _closeButton->setObjectName("CloseButton");
    _closeButton->setFixedSize(46, 32);
    _closeButton->setIconSize(QSize(16, 16));
    _closeButton->setAutoRaise(true);
    titleRowLayout->addWidget(_closeButton);
    connect(_closeButton, SIGNAL(clicked()), parent, SLOT(close()));
  }

  titleRow->setParent(this);
  titleRow->move(0, 0);
  titleRow->show();

  setFixedHeight(32);

  // --- 平移滑动式 Ribbon ---
  // _ribbonContainer: 裁切容器，固定高度，作为 _parent 的子控件浮在主界面上方
  // 开启 WA_ClipChildren 让超出容器边界的面板部分被裁切，实现真正的平移滑动效果
  _ribbonContainer = new QWidget(parent);
  _ribbonContainer->setObjectName("RibbonContainer");
  _ribbonContainer->setContentsMargins(0, 0, 0, 0);
  _ribbonContainer->setFixedHeight(_ribbonExpandedHeight);
  _ribbonContainer->setAutoFillBackground(false);
  _ribbonContainer->setAttribute(Qt::WA_TranslucentBackground);
  _ribbonContainer->hide();

  // _ribbonPanel: 实际内容面板，在容器内做 Y 轴平移
  // 初始位置在容器上方(y=-65)，完全被裁切不可见
  _ribbonPanel = new QWidget(this);
  _ribbonPanel->setObjectName("RibbonPanel");
  _ribbonPanel->setContentsMargins(0, 0, 0, 0);
  _ribbonPanel->setFixedSize(1, _ribbonExpandedHeight);
  _ribbonPanel->move(0, 32);
  _ribbonPanel->hide();

  _ribbonLayout = new QVBoxLayout(_ribbonPanel);
  _ribbonLayout->setContentsMargins(0, 0, 0, 0);
  _ribbonLayout->setSpacing(0);

  _categoryStack = new QStackedWidget(_ribbonPanel);
  _categoryStack->setContentsMargins(0, 0, 0, 0);
  _categoryStack->setFixedHeight(_ribbonExpandedHeight);
  _categoryStack->setMinimumWidth(800);
  _ribbonLayout->addWidget(_categoryStack, 0, Qt::AlignTop);

  _pinButton = new QToolButton(_ribbonPanel);
  _pinButton->setObjectName("RibbonPinButton");
  _pinButton->setFixedSize(20, 20);
  _pinButton->setIconSize(QSize(14, 14));
  _pinButton->setAutoRaise(true);
  _pinButton->setCheckable(true);
  _pinButton->setChecked(true);
  _pinButton->setToolTip(tr("Unpin Ribbon"));
  _pinButton->setIcon(QIcon(GetIconPath() + "/unpin.svg"));
  connect(_pinButton, &QToolButton::toggled, this, &TitleBar::onPinToggled);

  // TitleBar 高度固定 32px，不随 Ribbon 变化
  setFixedHeight(32);

  // --- 动画引擎：Y轴平移 ---
  // slideOffset 从 _ribbonExpandedHeight(面板在容器上方,隐藏) 到
  // 0(面板完全可见)
  _ribbonAnimation = new QPropertyAnimation(this, "slideOffset");
  _ribbonAnimation->setDuration(250);
  _ribbonAnimation->setEasingCurve(QEasingCurve::OutCubic);

  connect(_ribbonAnimation, &QPropertyAnimation::finished, this, [this]() {
    QWidget *tlw = window();
    if (_ribbonExpanded && _ribbonPinned) {
      if (tlw)
        tlw->setUpdatesEnabled(false);
      _ribbonPanel->setParent(this);
      _ribbonPanel->move(0, 32);
      _ribbonPanel->setFixedWidth(width());
      _ribbonPanel->show();
      setFixedHeight(32 + _ribbonExpandedHeight);
      _ribbonContainer->hide();
      positionPinButton();
      if (tlw)
        tlw->setUpdatesEnabled(true);
    } else if (!_ribbonExpanded) {
      if (tlw)
        tlw->setUpdatesEnabled(false);
      _ribbonContainer->hide();
      if (_ribbonPinned) {
        setFixedHeight(32);
      }
      if (tlw)
        tlw->setUpdatesEnabled(true);
    }
  });

  connect(_tabBar, &QTabBar::tabBarClicked, this, &TitleBar::onTabClicked);
  connect(_tabBar, &QTabBar::currentChanged, this, &TitleBar::onTabChanged);

  ADD_UI(this);
}

TitleBar::~TitleBar() {
  DESTROY_QT_OBJECT(_minimizeButton);
  DESTROY_QT_OBJECT(_maximizeButton);
  DESTROY_QT_OBJECT(_closeButton);

  REMOVE_UI(this);
}

int TitleBar::addCategory(const QString &title) {
  int index = _tabBar->addTab(title);

  QWidget *categoryWidget = new QWidget(_categoryStack);
  categoryWidget->setContentsMargins(0, 0, 0, 0);
  QHBoxLayout *categoryLayout = new QHBoxLayout(categoryWidget);
  categoryLayout->setContentsMargins(4, 4, 4, 4);
  categoryLayout->setSpacing(6);
  categoryLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));

  _categoryStack->addWidget(categoryWidget);
  _categoryLayouts.append(categoryLayout);

  dsv_info("TitleBar::addCategory index=%d title='%s'", index,
           title.toUtf8().constData());

  return index;
}

void TitleBar::addAction(int categoryIndex, QAction *action) {
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
    dsv_info("TitleBar::addAction '%s' icon was NULL, set default",
             action->text().toUtf8().constData());
  }

  btn->setDefaultAction(action);

  int insertPos = layout->count() - 1;
  if (insertPos < 0)
    insertPos = 0;
  layout->insertWidget(insertPos, btn);

  dsv_info("TitleBar::addAction categoryIndex=%d action='%s'", categoryIndex,
           action->text().toUtf8().constData());
}

void TitleBar::addSeparator(int categoryIndex) {
  if (categoryIndex < 0 || categoryIndex >= _categoryLayouts.size()) {
    dsv_info("TitleBar::addSeparator invalid categoryIndex=%d", categoryIndex);
    return;
  }

  QHBoxLayout *layout = _categoryLayouts[categoryIndex];

  QWidget *line = new QWidget();
  line->setObjectName("RibbonSeparator");
  line->setFixedWidth(1);

  int insertPos = layout->count() - 1;
  if (insertPos < 0)
    insertPos = 0;
  layout->insertWidget(insertPos, line);
}

void TitleBar::retranslateUi(int categoryIndex, const QString &title) {
  if (categoryIndex >= 0 && categoryIndex < _tabBar->count()) {
    _tabBar->setTabText(categoryIndex, title);
  }
}

void TitleBar::expandRibbon() {
  if (_ribbonAnimation->state() == QAbstractAnimation::Running) {
    _ribbonAnimation->stop();
  }

  positionRibbonContainer();
  _ribbonContainer->show();
  _ribbonContainer->raise();

  _ribbonPanel->show();

  _ribbonAnimation->setStartValue(_slideOffset);
  _ribbonAnimation->setEndValue(0);
  _ribbonAnimation->setEasingCurve(QEasingCurve::OutCubic);
  _ribbonAnimation->start();
  _ribbonExpanded = true;

  qApp->installEventFilter(this);
}

void TitleBar::hideRibbon() {
  if (_ribbonAnimation->state() == QAbstractAnimation::Running) {
    _ribbonAnimation->stop();
  }

  _ribbonAnimation->setStartValue(_slideOffset);
  _ribbonAnimation->setEndValue(_ribbonExpandedHeight);
  _ribbonAnimation->setEasingCurve(makeTailwindCurve());
  _ribbonAnimation->start();
  _ribbonExpanded = false;

  qApp->removeEventFilter(this);
}

void TitleBar::expandRibbonPinned() {
  dsv_info("expandRibbonPinned");

  if (_ribbonAnimation->state() == QAbstractAnimation::Running)
    _ribbonAnimation->stop();

  if (_ribbonPanel->parentWidget() != _ribbonContainer) {
    _ribbonPanel->setParent(_ribbonContainer);
  }

  _ribbonPanel->setFixedSize(_parent->width(), _ribbonExpandedHeight);
  _ribbonPanel->move(0, -_ribbonExpandedHeight);
  _ribbonPanel->show();

  positionRibbonContainer();
  _ribbonContainer->show();
  _ribbonContainer->raise();

  _ribbonAnimation->setStartValue(_ribbonExpandedHeight);
  _ribbonAnimation->setEndValue(0);
  _ribbonAnimation->setEasingCurve(QEasingCurve::OutCubic);
  _ribbonAnimation->start();

  _ribbonExpanded = true;
}

void TitleBar::hideRibbonPinned() {
  dsv_info("hideRibbonPinned");

  if (_ribbonAnimation->state() == QAbstractAnimation::Running)
    _ribbonAnimation->stop();

  setFixedHeight(32);

  if (_ribbonPanel->parentWidget() != _ribbonContainer) {
    _ribbonPanel->setParent(_ribbonContainer);
  }

  _ribbonPanel->setFixedSize(_parent->width(), _ribbonExpandedHeight);
  _ribbonPanel->move(0, 0);
  _ribbonPanel->show();

  positionRibbonContainer();
  _ribbonContainer->show();
  _ribbonContainer->raise();

  _ribbonAnimation->setStartValue(0);
  _ribbonAnimation->setEndValue(_ribbonExpandedHeight);
  _ribbonAnimation->setEasingCurve(makeTailwindCurve());
  _ribbonAnimation->start();

  _ribbonExpanded = false;
}

void TitleBar::positionRibbonContainer() {
  // 容器永远锚定在标题栏那一行（32px）的下方，而不是跟着动态的 height() 跑
  int y = mapTo(_parent, QPoint(0, 32)).y();
  _ribbonContainer->move(0, y);
  _ribbonContainer->setFixedWidth(_parent->width());
}

void TitleBar::onTabClicked(int index) {
  if (_ribbonExpanded && index == _tabBar->currentIndex()) {
    _ribbonPinned ? hideRibbonPinned() : hideRibbon();
  } else if (!_ribbonExpanded) {
    _ribbonPinned ? expandRibbonPinned() : expandRibbon();
  }
}

void TitleBar::onTabChanged(int index) {
  if (index >= 0 && index < _categoryStack->count()) {
    _categoryStack->setCurrentIndex(index);
  }
}

int TitleBar::slideOffset() const { return _slideOffset; }

void TitleBar::setSlideOffset(int offset) {
  _slideOffset = offset;
  _ribbonPanel->move(0, -offset);
  if (_ribbonPanel->width() != _ribbonContainer->width()) {
    _ribbonPanel->setFixedWidth(_ribbonContainer->width());
  }
  positionPinButton();
}

bool TitleBar::isOnTabBar(const QPoint &pos) const {
  if (_tabBar->rect().contains(_tabBar->mapFrom(this, pos))) {
    return true;
  }
  if (_ribbonExpanded) {
    if (_ribbonPinned && _ribbonPanel->parent() == this) {
      QPoint posInPanel = _ribbonPanel->mapFrom(this, pos);
      if (_ribbonPanel->rect().contains(posInPanel)) {
        return true;
      }
    } else if (_ribbonContainer->isVisible()) {
      QPoint posInContainer = _ribbonContainer->mapFrom(this, pos);
      if (_ribbonContainer->rect().contains(posInContainer)) {
        return true;
      }
    }
  }
  return false;
}

void TitleBar::reStyle() {
  QString iconPath = GetIconPath();

  if (_isTop) {
    _minimizeButton->setIcon(QIcon(iconPath + "/minimize.svg"));
    if (ParentIsMaxsized())
      _maximizeButton->setIcon(QIcon(iconPath + "/restore.svg"));
    else
      _maximizeButton->setIcon(QIcon(iconPath + "/maximize.svg"));
  }
  if (_isTop || _hasClose)
    _closeButton->setIcon(QIcon(iconPath + "/close.svg"));

  if (_pinButton) {
    if (_ribbonPinned)
      _pinButton->setIcon(QIcon(iconPath + "/unpin.svg"));
    else
      _pinButton->setIcon(QIcon(iconPath + "/pin.svg"));
  }
}

bool TitleBar::ParentIsMaxsized() {
  if (_titleParent != NULL) {
    return _titleParent->ParentIsMaxsized();
  } else {
    return parentWidget()->isMaximized();
  }
}

void TitleBar::paintEvent(QPaintEvent *event) { QMenuBar::paintEvent(event); }

void TitleBar::resizeEvent(QResizeEvent *event) {
  dsv_info("TitleBar::resizeEvent old=%dx%d new=%dx%d minH=%d maxH=%d",
           event->oldSize().width(), event->oldSize().height(),
           event->size().width(), event->size().height(), minimumHeight(),
           maximumHeight());
  QMenuBar::resizeEvent(event);

  QWidget *titleRow = findChild<QWidget *>("TitleRow");
  if (titleRow) {
    titleRow->setFixedWidth(width());
  }

  if (_ribbonContainer->isVisible()) {
    positionRibbonContainer();
  }
  if (_ribbonPinned && _ribbonPanel->parentWidget() == this &&
      _ribbonPanel->isVisible()) {
    _ribbonPanel->setFixedWidth(width());
    _ribbonPanel->move(0, 32);
    positionPinButton();
  }
}

void TitleBar::setTitle(QString title) {
  if (!_is_native) {
    _title->setText(title);
  } else if (_parent != NULL) {
    _parent->setWindowTitle(title);
  }
}

QString TitleBar::title() {
  if (!_is_native) {
    return _title->text();
  } else if (_parent != NULL) {
    return _parent->windowTitle();
  }
  return "";
}

void TitleBar::showMaxRestore() {
  QString iconPath = GetIconPath();
  if (ParentIsMaxsized()) {
    _maximizeButton->setIcon(QIcon(iconPath + "/maximize.svg"));
    normalShow();
  } else {
    _maximizeButton->setIcon(QIcon(iconPath + "/restore.svg"));
    maximizedShow();
  }
}

void TitleBar::setRestoreButton(bool max) {
  QString iconPath = GetIconPath();
  if (!max) {
    _maximizeButton->setIcon(QIcon(iconPath + "/maximize.svg"));
  } else {
    _maximizeButton->setIcon(QIcon(iconPath + "/restore.svg"));
  }
}

bool TitleBar::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::MouseButtonPress) {
    QMouseEvent *me = static_cast<QMouseEvent *>(event);
    QPoint globalPos = me->globalPos();

    bool onTitleBar = rect().contains(mapFromGlobal(globalPos));
    bool onRibbon = _ribbonContainer->isVisible() &&
                    _ribbonContainer->rect().contains(
                        _ribbonContainer->mapFromGlobal(globalPos));

    if (!onTitleBar && !onRibbon) {
      if (!_ribbonPinned) {
        hideRibbon();
      }
      return true;
    }
  }
  return QMenuBar::eventFilter(watched, event);
}

void TitleBar::mousePressEvent(QMouseEvent *event) {
  if (isOnTabBar(event->pos())) {
    QMenuBar::mousePressEvent(event);
    return;
  }

  bool ableMove = !ParentIsMaxsized();

  if (event->button() == Qt::LeftButton && ableMove && _is_able_drag) {
    int x = event->pos().x();
    int y = event->pos().y();

    bool bTopWidow = AppControl::Instance()->GetTopWindow() == _parent;
    bool bClick = (x >= 6 && y >= 5 && x <= width() - 6);

    if (!bTopWidow || bClick) {
      _is_draging = true;

      _clickPos = event->globalPos();

      if (_titleParent != NULL) {
        _oldPos = _titleParent->GetParentPos();
      } else {
        _oldPos = _parent->pos();
      }

      _is_done_moved = false;

      event->accept();
      return;
    }
  }
  QMenuBar::mousePressEvent(event);
}

void TitleBar::mouseMoveEvent(QMouseEvent *event) {
  if (_is_draging) {

    int datX = 0;
    int datY = 0;

    datX = (event->globalPos().x() - _clickPos.x());
    datY = (event->globalPos().y() - _clickPos.y());

    int x = _oldPos.x() + datX;
    int y = _oldPos.y() + datY;

    if (!_moving) {
      if (ABS_VAL(datX) >= 2 || ABS_VAL(datY) >= 2) {
        _moving = true;
      } else {
        return;
      }
    }

    if (_titleParent != NULL) {

      if (!_is_done_moved) {
        _is_done_moved = true;
        _titleParent->MoveBegin();
      }

      _titleParent->MoveWindow(x, y);
    } else {

#ifdef _WIN32

      QRect screenRect = AppControl::Instance()->_screenRect;

      if (screenRect.width() > 0 && QGuiApplication::screens().size() > 1) {
        QRect rect = _parent->frameGeometry();

        if (x < screenRect.left()) {
          x = screenRect.left();
        }
        if (x + _parent->frameGeometry().width() > screenRect.right()) {
          x = screenRect.right() - _parent->frameGeometry().width();
        }
      }
#endif

      _parent->move(x, y);
    }

    event->accept();
    return;
  }
  QMenuBar::mouseMoveEvent(event);
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event) {
  if (_moving && _titleParent != NULL) {
    _titleParent->MoveEnd();
  }
  _moving = false;
  _is_draging = false;
  QMenuBar::mouseReleaseEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event) {
  QMenuBar::mouseDoubleClickEvent(event);

  if (_isTop) {

    QTimer::singleShot(200, this, [this]() { showMaxRestore(); });
  }
}

void TitleBar::UpdateLanguage() {}

void TitleBar::UpdateTheme() { reStyle(); }

void TitleBar::UpdateFont() {
  QFont font = this->font();
  font.setPointSizeF(AppConfig::Instance().appOptions.fontSize + 1);
  _title->setFont(font);
}

void TitleBar::onPinToggled(bool checked) {
  dsv_info("onPinToggled: checked=%d", checked);
  _ribbonPinned = checked;
  QString iconPath = GetIconPath();

  if (checked) {
    _pinButton->setIcon(QIcon(iconPath + "/unpin.svg"));
    _pinButton->setToolTip(tr("Unpin Ribbon"));
    qApp->removeEventFilter(this);

    if (_ribbonExpanded) {
      if (_ribbonAnimation->state() == QAbstractAnimation::Running)
        _ribbonAnimation->stop();
      _ribbonContainer->hide();
      _ribbonPanel->setParent(this);
      _ribbonPanel->move(0, 32);
      _ribbonPanel->setFixedWidth(width());
      _ribbonPanel->show();
      setFixedHeight(32 + _ribbonExpandedHeight);
      positionPinButton();
    } else {
      _ribbonPanel->setParent(this);
      _ribbonPanel->move(0, 32);
      _ribbonPanel->setFixedWidth(width());
      _ribbonPanel->hide();
    }
  } else {
    _pinButton->setIcon(QIcon(iconPath + "/pin.svg"));
    _pinButton->setToolTip(tr("Pin Ribbon"));

    if (_ribbonExpanded) {
      qApp->installEventFilter(this);
      if (_ribbonAnimation->state() == QAbstractAnimation::Running)
        _ribbonAnimation->stop();
      setFixedHeight(32);
      _ribbonPanel->setParent(_ribbonContainer);
      _slideOffset = 0;
      _ribbonPanel->move(0, 0);
      _ribbonPanel->setFixedWidth(_ribbonContainer->width());
      _ribbonPanel->show();
      positionRibbonContainer();
      _ribbonContainer->show();
      _ribbonContainer->raise();
    } else {
      _ribbonPanel->setParent(_ribbonContainer);
      _ribbonPanel->move(0, -_ribbonExpandedHeight);
      _ribbonPanel->setFixedWidth(_ribbonContainer->width());
      setFixedHeight(32);
    }
  }
}

void TitleBar::positionPinButton() {
  if (!_pinButton)
    return;
  int pinX = _ribbonPanel->width() - _pinButton->width() - 6;
  int pinY = _ribbonPanel->height() - _pinButton->height() - 4;
  _pinButton->move(pinX, pinY);
}

void TitleBar::EnableAbleDrag(bool bEnabled) { _is_able_drag = bEnabled; }

} // namespace toolbars
} // namespace pv
