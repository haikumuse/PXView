/*
 * This file is part of the PXView project.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "toast.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QTimer>
#include <QPropertyAnimation>
#include <QApplication>
#include <QPainter>
#include <QScreen>
#include <QIcon>
#include "../config/appconfig.h"

namespace pv {
namespace ui {

Toast::Toast(QWidget *parent, const QString &text, Level level)
    : QWidget(parent)
{
    // Qt::ToolTip allows it to be a top-level window that doesn't steal focus
    // Qt::FramelessWindowHint removes OS window decorations
    setWindowFlags(Qt::FramelessWindowHint | Qt::ToolTip | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_ShowWithoutActivating);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 15, 20, 15);
    layout->setSpacing(10);

    // Icon
    QLabel *iconLabel = new QLabel(this);
    if (level == Warning) {
        QIcon warnIcon(":/icons/status-warning.svg");
        if (!warnIcon.isNull()) {
            iconLabel->setPixmap(warnIcon.pixmap(24, 24));
        } else {
            iconLabel->setText("⚠️");
        }
    } else if (level == Error) {
        iconLabel->setText("❌");
    } else {
        iconLabel->setText("ℹ️");
    }
    
    QLabel *textLabel = new QLabel(text, this);
    textLabel->setStyleSheet("color: white; font-size: 14px;");

    layout->addWidget(iconLabel);
    layout->addWidget(textLabel);

    _timer = new QTimer(this);
    _timer->setSingleShot(true);
    connect(_timer, &QTimer::timeout, this, &Toast::closeAnimation);
}

void Toast::show(QWidget *parent, const QString &text, Level level)
{
    QWidget *w = parent;
    if (!w) {
        w = QApplication::activeWindow();
    }
    
    Toast *toast = new Toast(w, text, level);
    toast->adjustSize();
    
    // Position at bottom-right of the active window or screen
    if (w) {
        // Find top-level window
        QWidget *topLevel = w->window();
        if (!topLevel) topLevel = w;
        
        QPoint p = topLevel->mapToGlobal(QPoint(topLevel->width() - toast->width() - 30, 
                                                topLevel->height() - toast->height() - 30));
        toast->move(p);
    } else {
        QScreen *screen = QApplication::primaryScreen();
        if (screen) {
            QRect r = screen->geometry();
            toast->move(r.width() - toast->width() - 30, r.height() - toast->height() - 60);
        }
    }

    toast->showAnimation();
}

void Toast::showAnimation()
{
    QWidget::show();
    setWindowOpacity(0.0);
    QPropertyAnimation *anim = new QPropertyAnimation(this, "windowOpacity");
    anim->setDuration(300);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    _timer->start(3500); // Wait 3.5 seconds before starting fade out
}

void Toast::closeAnimation()
{
    QPropertyAnimation *anim = new QPropertyAnimation(this, "windowOpacity");
    anim->setDuration(300);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    connect(anim, &QPropertyAnimation::finished, this, &QWidget::close);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void Toast::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    
    // Get theme-aware color or fallback to dark gray
    QColor bgColor(30, 30, 30, 230);
    if (AppConfig::Instance().IsDarkStyle()) {
        bgColor = QColor(45, 45, 45, 240);
    } else {
        bgColor = QColor(30, 30, 30, 230); // Use dark bg even on light theme for contrast
    }

    p.setBrush(bgColor);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect(), 8, 8);
}

} // namespace ui
} // namespace pv
