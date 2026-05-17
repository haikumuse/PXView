/*
 * This file is part of the DSView project.
 * Copyright (C) 2025 DreamSourceLab <support@dreamsourcelab.com>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "sidebarbutton.h"
#include "../config/appconfig.h"
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include "../ui/qtcompat.h"

SideBarButton::SideBarButton(QWidget *parent)
    : QWidget(parent), _isCheckable(false), _isChecked(false),
      _isRunning(false), _isHovered(false), _isPressed(false), _isEnabled(true),
      _itemType(DockItem), _drawerPageIndex(-1) {
  setFixedSize(45, 50);
  setCursor(Qt::PointingHandCursor);
}

void SideBarButton::setIconName(const QString &name) {
  _iconName = name;
  update();
}

void SideBarButton::setAlternateIconName(const QString &name) {
  _alternateIconName = name;
  update();
}

void SideBarButton::setText(const QString &text) {
  _text = stripShortcut(text);
  update();
}

void SideBarButton::setCheckable(bool checkable) { _isCheckable = checkable; }

void SideBarButton::setChecked(bool checked) {
  _isChecked = checked;
  update();
}

void SideBarButton::setRunning(bool running) {
  _isRunning = running;
  update();
}

void SideBarButton::setItemType(ItemType type) { _itemType = type; }

void SideBarButton::setDrawerPageIndex(int page) { _drawerPageIndex = page; }

void SideBarButton::setEnabled(bool enabled) {
  _isEnabled = enabled;
  QWidget::setEnabled(enabled);
  update();
}

void SideBarButton::setVisible(bool visible) { QWidget::setVisible(visible); }

QSize SideBarButton::sizeHint() const { return QSize(45, 50); }

QRectF SideBarButton::indicatorRect() const {
  return QRectF(width() - 3, 10, 3, 20);
}

void SideBarButton::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                         QPainter::SmoothPixmapTransform);
  painter.setPen(Qt::NoPen);

  drawBackground(&painter);
  drawIcon(&painter);
  drawText(&painter);
}

void SideBarButton::drawBackground(QPainter *painter) {
  if (!painter)
    return;

  bool isDark = AppConfig::Instance().frameOptions.style == "dark" ||
                AppConfig::Instance().frameOptions.style == "";
  int c = isDark ? 255 : 0;

  if (_isChecked) {
    // Selected: very subtle background with soft inner glow feel
    painter->setBrush(QColor(c, c, c, _isHovered ? 8 : 12));
    painter->drawRoundedRect(rect(), 5, 5);
  } else if (_isHovered && _isEnabled) {
    painter->setBrush(QColor(c, c, c, 10));
    painter->drawRoundedRect(rect(), 5, 5);
  }
}

void SideBarButton::drawIcon(QPainter *painter) {
  if (!painter)
    return;

  QString effectiveIconName = (_isRunning && !_alternateIconName.isEmpty())
                                  ? _alternateIconName
                                  : _iconName;
  if (effectiveIconName.isEmpty())
    return;

  if (_isPressed) {
    painter->setOpacity(0.7);
  }
  if (!_isEnabled) {
    painter->setOpacity(0.4);
  }

  QIcon icon(GetIconPath() + "/" + effectiveIconName);
  int iconSize = 24;
  int iconX = (width() - iconSize) / 2;
  QRectF iconRect(iconX, 6, iconSize, iconSize);

  if (_isChecked) {
    // Selected: tint icon with soft accent color
    QPixmap pix = icon.pixmap(iconSize, iconSize);
    if (!pix.isNull()) {
      QPainter p(&pix);
      p.setCompositionMode(QPainter::CompositionMode_SourceIn);
      p.fillRect(pix.rect(), QColor("#5B8DEF"));
      p.end();
      painter->drawPixmap(iconRect.topLeft(), pix);
    }
  } else {
    icon.paint(painter, iconRect.toRect());
  }

  painter->setOpacity(1.0);
}

void SideBarButton::drawText(QPainter *painter) {
  if (!painter)
    return;
  if (_text.isEmpty())
    return;

  bool isDark = AppConfig::Instance().frameOptions.style == "dark" ||
                AppConfig::Instance().frameOptions.style == "";

  if (_isChecked) {
    painter->setPen(isDark ? QColor("#E0E0E0") : QColor("#1A1A1A"));
  } else {
    painter->setPen(QColor("#A0A0B0"));
  }

  if (!_isEnabled) {
    painter->setOpacity(0.4);
  }

  QFont f = font();
  f.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
  painter->setFont(f);
  painter->drawText(QRect(0, 32, width(), 14), Qt::AlignCenter, _text);

  painter->setOpacity(1.0);
}

QString SideBarButton::stripShortcut(const QString &text) {
  QRegularExpression re(QLatin1String("\\s*\\([^)]*\\)\\s*$"));
  QString result = text;
  result.remove(re);
  result.remove(QLatin1Char('&'));
  return result.trimmed();
}

void SideBarButton::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    _isPressed = true;
    update();
  }
  QWidget::mousePressEvent(event);
}

void SideBarButton::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    _isPressed = false;
    if (_isEnabled && rect().contains(QT_COMPAT_POS(event))) {
      emit clicked();
    }
    update();
  }
  QWidget::mouseReleaseEvent(event);
}

void SideBarButton::click() { emit clicked(); }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void SideBarButton::enterEvent(QEnterEvent *event) {
#else
void SideBarButton::enterEvent(QEvent *event) {
#endif
  _isHovered = true;
  update();
  QWidget::enterEvent(event);
}

void SideBarButton::leaveEvent(QEvent *event) {
  _isHovered = false;
  _isPressed = false;
  update();
  QWidget::leaveEvent(event);
}
