/*
 * This file is part of the PXView project.
 *
 * Copyright (C) 2025 DreamSourceLab <support@dreamsourcelab.com>
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
 */

#include "smoothscrollarea.h"

#include <QWheelEvent>
#include <QScrollBar>
#include <QEvent>

namespace pv {
namespace widgets {

SmoothScrollArea::SmoothScrollArea(QWidget *parent)
    : QScrollArea(parent),
      _v_anim(nullptr),
      _h_anim(nullptr),
      _anim_duration(250),
      _v_anim_target(0),
      _wheel_count(0),
      _wheel_direction(0)
{
    _v_anim = new QPropertyAnimation(verticalScrollBar(), "value");
    _v_anim->setEasingCurve(QEasingCurve::OutCubic);
    _v_anim->setDuration(_anim_duration);

    _h_anim = new QPropertyAnimation(horizontalScrollBar(), "value");
    _h_anim->setEasingCurve(QEasingCurve::OutCubic);
    _h_anim->setDuration(_anim_duration);

    _wheel_accel_timer.setInterval(100);
    _wheel_accel_timer.setSingleShot(true);
    connect(&_wheel_accel_timer, &QTimer::timeout, this, [this](){
        _wheel_count = 0;
        _wheel_direction = 0;
    });
}

SmoothScrollArea::~SmoothScrollArea()
{
    if (_v_anim) {
        _v_anim->stop();
        delete _v_anim;
    }
    if (_h_anim) {
        _h_anim->stop();
        delete _h_anim;
    }
}

void SmoothScrollArea::setAnimationDuration(int ms)
{
    _anim_duration = ms;
    _v_anim->setDuration(ms);
    _h_anim->setDuration(ms);
}

int SmoothScrollArea::animationDuration() const
{
    return _anim_duration;
}

void SmoothScrollArea::animateScrollTo(int targetValue)
{
    QScrollBar *vbar = verticalScrollBar();

    targetValue = qBound(vbar->minimum(), targetValue, vbar->maximum());
    if (targetValue == vbar->value() && _v_anim->state() != QAbstractAnimation::Running)
        return;

    if (_v_anim->state() == QAbstractAnimation::Running) {
        _v_anim_target += (targetValue - vbar->value());
        _v_anim_target = qBound((qreal)vbar->minimum(), _v_anim_target, (qreal)vbar->maximum());
    } else {
        _v_anim_target = targetValue;
    }

    _v_anim->stop();
    _v_anim->setStartValue(vbar->value());
    _v_anim->setEndValue(_v_anim_target);

    int duration = _anim_duration;
    if (_wheel_count > 6)
        duration = 5000;
    else if (_wheel_count > 3)
        duration = 800;
    _v_anim->setDuration(duration);

    _v_anim->start();
}

bool SmoothScrollArea::viewportEvent(QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
        int delta = wheelEvent->angleDelta().y();
        if (delta == 0)
            return QScrollArea::viewportEvent(event);

        QScrollBar *vbar = verticalScrollBar();
        if (vbar->minimum() >= vbar->maximum())
            return QScrollArea::viewportEvent(event);

        int direction = (delta > 0) ? -1 : 1;

        if (_wheel_accel_timer.isActive() && _wheel_direction == direction) {
            _wheel_count++;
        } else {
            _wheel_count = 1;
            _wheel_direction = direction;
        }

        int step = qMax(vbar->singleStep() * 3, 60);
        if (_wheel_count > 3)
            step *= 2;
        if (_wheel_count > 6)
            step *= 3;

        int target = vbar->value() + direction * step;

        animateScrollTo(target);
        _wheel_accel_timer.start();
        event->accept();
        return true;
    }

    return QScrollArea::viewportEvent(event);
}

void SmoothScrollArea::wheelEvent(QWheelEvent *event)
{
    event->accept();

    int delta = event->angleDelta().y();
    if (delta == 0)
        return;

    QScrollBar *vbar = verticalScrollBar();
    if (vbar->minimum() >= vbar->maximum()) {
        QScrollArea::wheelEvent(event);
        return;
    }

    int direction = (delta > 0) ? -1 : 1;

    if (_wheel_accel_timer.isActive() && _wheel_direction == direction) {
        _wheel_count++;
    } else {
        _wheel_count = 1;
        _wheel_direction = direction;
    }

    int step = qMax(vbar->singleStep() * 3, 60);
    if (_wheel_count > 3)
        step *= 2;
    if (_wheel_count > 6)
        step *= 3;

    int target = vbar->value() + direction * step;

    animateScrollTo(target);
    _wheel_accel_timer.start();
}

} // namespace widgets
} // namespace pv
