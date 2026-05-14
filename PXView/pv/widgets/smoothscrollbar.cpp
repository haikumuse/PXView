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

#include "smoothscrollbar.h"

#include <QWheelEvent>

namespace pv {
namespace widgets {

SmoothScrollBar::SmoothScrollBar(Qt::Orientation orientation, QWidget *parent)
    : QScrollBar(orientation, parent),
      _anim(nullptr),
      _anim_duration(300),
      _slider_dragging(false),
      _wheel_direction(0),
      _wheel_count(0),
      _anim_target(0)
{
    _anim = new QPropertyAnimation(this, "value");
    _anim->setEasingCurve(QEasingCurve::OutCubic);
    _anim->setDuration(_anim_duration);

    _wheel_accel_timer.setInterval(100);
    _wheel_accel_timer.setSingleShot(true);
    connect(&_wheel_accel_timer, &QTimer::timeout,
            this, &SmoothScrollBar::onAccelerationTimeout);

    connect(this, &QAbstractSlider::sliderPressed, this, [this](){ _slider_dragging = true; });
    connect(this, &QAbstractSlider::sliderReleased, this, [this](){ _slider_dragging = false; });
}

SmoothScrollBar::~SmoothScrollBar()
{
    if (_anim) {
        _anim->stop();
        delete _anim;
    }
}

void SmoothScrollBar::smoothSetValue(int value)
{
    if (_slider_dragging) {
        QScrollBar::setValue(value);
        return;
    }

    value = qBound(minimum(), value, maximum());
    if (value == this->value() && _anim->state() != QAbstractAnimation::Running)
        return;

    if (_anim->state() == QAbstractAnimation::Running) {
        _anim_target += (value - this->value());
        _anim_target = qBound((qreal)minimum(), _anim_target, (qreal)maximum());
    } else {
        _anim_target = value;
    }

    _anim->stop();
    _anim->setStartValue(this->value());
    _anim->setEndValue(_anim_target);
    _anim->start();
}

void SmoothScrollBar::immediateSetValue(int value)
{
    _anim->stop();
    QScrollBar::setValue(value);
}

void SmoothScrollBar::setAnimationDuration(int ms)
{
    _anim_duration = ms;
    _anim->setDuration(ms);
}

int SmoothScrollBar::animationDuration() const
{
    return _anim_duration;
}

void SmoothScrollBar::wheelEvent(QWheelEvent *event)
{
    event->accept();

    int delta = event->angleDelta().y();
    if (delta == 0)
        return;

    int direction = (delta > 0) ? -1 : 1;

    if (_wheel_accel_timer.isActive() && _wheel_direction == direction) {
        _wheel_count++;
    } else {
        _wheel_count = 1;
        _wheel_direction = direction;
    }

    int step = singleStep() * 3;
    int duration = _anim_duration;

    if (_wheel_count > 3) {
        step *= 2;
        duration = 800;
    }
    if (_wheel_count > 6) {
        step *= 3;
        duration = 5000;
    }

    int target = value() + direction * step;
    target = qBound(minimum(), target, maximum());

    if (_anim->state() == QAbstractAnimation::Running) {
        _anim_target += direction * step;
        _anim_target = qBound((qreal)minimum(), _anim_target, (qreal)maximum());
    } else {
        _anim_target = target;
    }

    _anim->stop();
    _anim->setDuration(duration);
    _anim->setStartValue(this->value());
    _anim->setEndValue(_anim_target);
    _anim->start();

    _wheel_accel_timer.start();
}

void SmoothScrollBar::onAccelerationTimeout()
{
    _wheel_count = 0;
    _wheel_direction = 0;
}

} // namespace widgets
} // namespace pv
