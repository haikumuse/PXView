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

#ifndef DSVIEW_PV_WIDGETS_SMOOTHSCROLLAREA_H
#define DSVIEW_PV_WIDGETS_SMOOTHSCROLLAREA_H

#include <QScrollArea>
#include <QPropertyAnimation>
#include <QTimer>

namespace pv {
namespace widgets {

class SmoothScrollArea : public QScrollArea
{
    Q_OBJECT

public:
    explicit SmoothScrollArea(QWidget *parent = nullptr);
    ~SmoothScrollArea();

    void setAnimationDuration(int ms);
    int animationDuration() const;

protected:
    void wheelEvent(QWheelEvent *event) override;
    bool viewportEvent(QEvent *event) override;

private:
    void animateScrollTo(int targetValue);

    QPropertyAnimation *_v_anim;
    QPropertyAnimation *_h_anim;
    int _anim_duration;
    qreal _v_anim_target;
    int _wheel_count;
    int _wheel_direction;
    QTimer _wheel_accel_timer;
};

} // namespace widgets
} // namespace pv

#endif // DSVIEW_PV_WIDGETS_SMOOTHSCROLLAREA_H
