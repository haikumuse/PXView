/*
 * This file is part of the DSView project.
 *
 * Copyright (C) 2025 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "slidingdrawer.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QGraphicsOpacityEffect>
#include <QApplication>
#include <QPropertyAnimation>
#include <QEasingCurve>

namespace pv {
namespace widgets {

static constexpr int DEFAULT_DRAWER_WIDTH = 350;
static constexpr int DEFAULT_ANIMATION_DURATION = 500;
static constexpr int TITLE_BAR_HEIGHT = 40;
static constexpr int MIN_DRAWER_WIDTH = 200;

// CSS cubic-bezier(0.4, 0, 0.2, 1) solver — matches Tailwind ease-in-out exactly
static qreal cssBezierEasing(qreal t)
{
    // cubic-bezier(0.4, 0, 0.2, 1)
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

    // Newton's method to solve sampleCurveX(s) = t
    double s = t;
    for (int i = 0; i < 8; i++) {
        double err = sampleCurveX(s) - t;
        if (qAbs(err) < 1e-7) break;
        double deriv = (3.0 * ax * s + 2.0 * bx) * s + cx;
        if (qAbs(deriv) < 1e-10) break;
        s -= err / deriv;
    }
    return qBound(0.0, sampleCurveY(s), 1.0);
}

static QEasingCurve makeTailwindCurve()
{
    QEasingCurve curve;
    curve.setCustomType(cssBezierEasing);
    return curve;
}

SlidingDrawer::SlidingDrawer(QWidget *parent)
    : QWidget(parent)
    , _slide_progress(0.0)
    , _drawer_width(DEFAULT_DRAWER_WIDTH)
    , _animation_duration(DEFAULT_ANIMATION_DURATION)
    , _backdrop_enabled(true)
    , _backdrop_color(0, 0, 0, 80)
    , _current_page(-1)
    , _is_open(false)
    , _is_animating(false)
    , _drag_active(false)
{
    // This widget covers the entire parent area as an overlay
    setVisible(false);
    setMouseTracking(true);

    // --- Backdrop ---
    _backdrop = new QWidget(this);
    _backdrop->setObjectName("sliding_drawer_backdrop");
    _backdrop->installEventFilter(this);

    _backdrop_opacity_effect = new QGraphicsOpacityEffect(_backdrop);
    _backdrop_opacity_effect->setOpacity(0.0);
    _backdrop->setGraphicsEffect(_backdrop_opacity_effect);

    // --- Panel (translates as a whole unit) ---
    _panel = new QWidget(this);
    _panel->setObjectName("sliding_drawer_panel");

    // --- Panel content (fixed width, always at 0,0 inside _panel) ---
    _panel_content = new QWidget(_panel);
    _panel_content->setObjectName("sliding_drawer_panel_content");
    _panel_content->setFixedWidth(_drawer_width);

    QVBoxLayout *content_layout = new QVBoxLayout(_panel_content);
    content_layout->setContentsMargins(0, 0, 0, 0);
    content_layout->setSpacing(0);

    // Title bar (no close button)
    _title_bar = new QWidget(_panel_content);
    _title_bar->setObjectName("sliding_drawer_titlebar");
    _title_bar->setFixedHeight(TITLE_BAR_HEIGHT);

    QHBoxLayout *title_layout = new QHBoxLayout(_title_bar);
    title_layout->setContentsMargins(12, 0, 12, 0);

    _title_label = new QLabel(_title_bar);
    _title_label->setObjectName("sliding_drawer_title");
    title_layout->addWidget(_title_label);
    title_layout->addStretch();

    content_layout->addWidget(_title_bar);

    // Stacked widget for content pages
    _stacked_widget = new QStackedWidget(_panel_content);
    _stacked_widget->setObjectName("sliding_drawer_stack");
    content_layout->addWidget(_stacked_widget, 1);

    // Install event filter on backdrop for click-to-close
    _backdrop->installEventFilter(this);

    // --- Open animation group ---
    _open_group = new QParallelAnimationGroup(this);

    QPropertyAnimation *open_slide = new QPropertyAnimation(this, "slideProgress");
    open_slide->setStartValue(0.0);
    open_slide->setEndValue(1.0);
    open_slide->setDuration(_animation_duration);
    open_slide->setEasingCurve(QEasingCurve::OutCubic);
    _open_group->addAnimation(open_slide);

    QPropertyAnimation *open_backdrop = new QPropertyAnimation(_backdrop_opacity_effect, "opacity");
    open_backdrop->setStartValue(0.0);
    open_backdrop->setEndValue(1.0);
    open_backdrop->setDuration(_animation_duration);
    open_backdrop->setEasingCurve(QEasingCurve::OutCubic);
    _open_group->addAnimation(open_backdrop);

    connect(_open_group, &QParallelAnimationGroup::finished, this, [this]() {
        _is_animating = false;
        _is_open = true;
        emit drawerOpened(_current_page);
    });

    // --- Close animation group ---
    _close_group = new QParallelAnimationGroup(this);

    QPropertyAnimation *close_slide = new QPropertyAnimation(this, "slideProgress");
    close_slide->setStartValue(1.0);
    close_slide->setEndValue(0.0);
    close_slide->setDuration(_animation_duration);
    close_slide->setEasingCurve(makeTailwindCurve());
    _close_group->addAnimation(close_slide);

    QPropertyAnimation *close_backdrop = new QPropertyAnimation(_backdrop_opacity_effect, "opacity");
    close_backdrop->setStartValue(1.0);
    close_backdrop->setEndValue(0.0);
    close_backdrop->setDuration(_animation_duration);
    close_backdrop->setEasingCurve(makeTailwindCurve());
    _close_group->addAnimation(close_backdrop);

    connect(_close_group, &QParallelAnimationGroup::finished, this, [this]() {
        _is_animating = false;
        _is_open = false;
        finishClose();
        emit drawerClosed();
    });
}

SlidingDrawer::~SlidingDrawer()
{
}

int SlidingDrawer::addPage(QWidget *content, const QString &title)
{
    if (!content)
        return -1;

    content->setParent(_stacked_widget);
    _stacked_widget->addWidget(content);

    PageInfo info;
    info.content = content;
    info.title = title;
    _pages.append(info);

    return _pages.size() - 1;
}

void SlidingDrawer::removePage(int index)
{
    if (index < 0 || index >= _pages.size())
        return;

    QWidget *content = _pages[index].content;
    _stacked_widget->removeWidget(content);
    content->setParent(nullptr);
    _pages.removeAt(index);

    if (_current_page == index) {
        close();
        _current_page = -1;
    } else if (_current_page > index) {
        _current_page--;
    }
}

QWidget* SlidingDrawer::page(int index) const
{
    if (index < 0 || index >= _pages.size())
        return nullptr;
    return _pages[index].content;
}

int SlidingDrawer::pageCount() const
{
    return _pages.size();
}

void SlidingDrawer::open(int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= _pages.size())
        return;

    // If already open with the same page, do nothing
    if (_is_open && _current_page == pageIndex && !_is_animating)
        return;

    // If already open with a DIFFERENT page, just switch content instantly (no slide animation)
    if (_is_open && !_is_animating) {
        _current_page = pageIndex;
        _stacked_widget->setCurrentIndex(pageIndex);
        _title_label->setText(_pages[pageIndex].title);
        _pages[pageIndex].content->show();
        return;
    }

    // If currently animating, stop it and resume from current progress
    if (_is_animating) {
        _open_group->stop();
        _close_group->stop();
        _is_animating = false;
    }

    _current_page = pageIndex;

    // Update stacked widget and title
    _stacked_widget->setCurrentIndex(pageIndex);
    _title_label->setText(_pages[pageIndex].title);
    _pages[pageIndex].content->show();

    // Show this widget and set initial state
    setVisible(true);
    raise();
    updatePanelGeometry();

    // Show backdrop if enabled
    if (_backdrop_enabled)
        _backdrop->setVisible(true);

    // Start open animation from current progress (smooth reverse on interrupt)
    qreal startProgress = _slide_progress;
    if (startProgress >= 1.0) startProgress = 0.0;

    QPropertyAnimation *open_slide = qobject_cast<QPropertyAnimation*>(_open_group->animationAt(0));
    QPropertyAnimation *open_backdrop = qobject_cast<QPropertyAnimation*>(_open_group->animationAt(1));
    if (open_slide) {
        open_slide->setStartValue(startProgress);
        open_slide->setEndValue(1.0);
    }
    if (open_backdrop) {
        open_backdrop->setStartValue(startProgress);
        open_backdrop->setEndValue(1.0);
    }

    _is_animating = true;
    _open_group->start();
}

void SlidingDrawer::close()
{
    if (!_is_open && !_is_animating)
        return;

    // If currently animating, stop it and resume from current progress
    if (_is_animating) {
        _open_group->stop();
        _close_group->stop();
    }

    // Start close animation from current progress (smooth reverse on interrupt)
    qreal startProgress = _slide_progress;
    if (startProgress <= 0.0) startProgress = 1.0;

    QPropertyAnimation *close_slide = qobject_cast<QPropertyAnimation*>(_close_group->animationAt(0));
    QPropertyAnimation *close_backdrop = qobject_cast<QPropertyAnimation*>(_close_group->animationAt(1));
    if (close_slide) {
        close_slide->setStartValue(startProgress);
        close_slide->setEndValue(0.0);
    }
    if (close_backdrop) {
        close_backdrop->setStartValue(startProgress);
        close_backdrop->setEndValue(0.0);
    }

    _is_animating = true;
    _close_group->start();
}

void SlidingDrawer::toggle(int pageIndex)
{
    // If animating: reverse direction (opening → close, closing → open)
    if (_is_animating) {
        if (_slide_progress > 0.01) {
            close();
        } else {
            open(pageIndex);
        }
        return;
    }

    if (_is_open && _current_page == pageIndex) {
        close();
    } else {
        open(pageIndex);
    }
}

bool SlidingDrawer::isOpen() const
{
    return _is_open;
}

bool SlidingDrawer::isAnimating() const
{
    return _is_animating;
}

void SlidingDrawer::setDrawerWidth(int width)
{
    _drawer_width = qMax(MIN_DRAWER_WIDTH, width);
    _panel_content->setFixedWidth(_drawer_width);
    updatePanelGeometry();
}

int SlidingDrawer::drawerWidth() const
{
    return _drawer_width;
}

void SlidingDrawer::setAnimationDuration(int ms)
{
    _animation_duration = qMax(50, ms);
    for (int i = 0; i < _open_group->animationCount(); i++) {
        QPropertyAnimation *anim = qobject_cast<QPropertyAnimation*>(_open_group->animationAt(i));
        if (anim) anim->setDuration(_animation_duration);
    }
    for (int i = 0; i < _close_group->animationCount(); i++) {
        QPropertyAnimation *anim = qobject_cast<QPropertyAnimation*>(_close_group->animationAt(i));
        if (anim) anim->setDuration(_animation_duration);
    }
}

int SlidingDrawer::animationDuration() const
{
    return _animation_duration;
}

void SlidingDrawer::setBackdropEnabled(bool enabled)
{
    _backdrop_enabled = enabled;
    if (_backdrop) {
        _backdrop->setVisible(enabled && (_is_open || _is_animating));
    }
}

bool SlidingDrawer::backdropEnabled() const
{
    return _backdrop_enabled;
}

void SlidingDrawer::setBackdropColor(const QColor &color)
{
    _backdrop_color = color;
    update();
}

QColor SlidingDrawer::backdropColor() const
{
    return _backdrop_color;
}

void SlidingDrawer::setPageTitle(int index, const QString &title)
{
    if (index < 0 || index >= _pages.size())
        return;
    _pages[index].title = title;
    if (_current_page == index)
        _title_label->setText(title);
}

// ---- Property animation support ----

qreal SlidingDrawer::slideProgress() const
{
    return _slide_progress;
}

void SlidingDrawer::setSlideProgress(qreal progress)
{
    progress = qBound(0.0, progress, 1.0);
    if (qFuzzyCompare(_slide_progress, progress))
        return;

    _slide_progress = progress;
    updatePanelGeometry();
}

void SlidingDrawer::updatePanelGeometry()
{
    if (!parentWidget())
        return;

    QWidget *p = parentWidget();
    int parent_w = p->width();
    int parent_h = p->height();

    // Resize this overlay to fill parent
    setGeometry(0, 0, parent_w, parent_h);

    // Backdrop covers entire area
    _backdrop->setGeometry(0, 0, parent_w, parent_h);

    // KEY: Panel translates as a whole unit (translate-x style)
    // _slide_progress 0.0 = panel is off-screen right
    // _slide_progress 1.0 = panel is fully visible, left edge at (parent_w - drawer_width)
    int panel_x = parent_w - qRound(_drawer_width * _slide_progress);

    // Panel always has full drawer_width — it just moves left/right
    _panel->setGeometry(panel_x, 0, _drawer_width, parent_h);

    // Content is always at (0,0) inside the panel — no shift, the whole thing moves together
    _panel_content->setGeometry(0, 0, _drawer_width, parent_h);
}

void SlidingDrawer::finishClose()
{
    if (_current_page >= 0 && _current_page < _pages.size()) {
        _pages[_current_page].content->hide();
    }
    setVisible(false);
    _current_page = -1;
}

// ---- Events ----

void SlidingDrawer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (!_backdrop_enabled || !_backdrop->isVisible())
        return;

    // Draw backdrop fill
    QPainter painter(_backdrop);
    painter.fillRect(_backdrop->rect(), _backdrop_color);
}

void SlidingDrawer::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    updatePanelGeometry();
}

bool SlidingDrawer::eventFilter(QObject *obj, QEvent *event)
{
    // --- Backdrop click to close ---
    if (obj == _backdrop && event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            close();
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void SlidingDrawer::mouseMoveEvent(QMouseEvent *event)
{
    if (!_is_open) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (_drag_active) {
        // Dragging left edge left = wider, right = narrower
        int dx = _drag_start_pos.x() - event->globalPos().x();
        int new_width = qMax(MIN_DRAWER_WIDTH, _drag_start_drawer_width + dx);
        setDrawerWidth(new_width);
    } else {
        // Update cursor based on position near the left edge of the panel
        int panel_left = _panel->geometry().left();
        int mouse_x = event->pos().x();
        int dist_to_edge = mouse_x - panel_left;

        if (dist_to_edge >= 0 && dist_to_edge < EDGE_GRIP_WIDTH) {
            setCursor(Qt::SplitHCursor);
        } else {
            unsetCursor();
        }
    }

    QWidget::mouseMoveEvent(event);
}

void SlidingDrawer::mousePressEvent(QMouseEvent *event)
{
    if (_is_open && event->button() == Qt::LeftButton) {
        int panel_left = _panel->geometry().left();
        int mouse_x = event->pos().x();
        int dist_to_edge = mouse_x - panel_left;

        if (dist_to_edge >= 0 && dist_to_edge < EDGE_GRIP_WIDTH) {
            _drag_active = true;
            _drag_start_pos = event->globalPos();
            _drag_start_drawer_width = _drawer_width;
            grabMouse();
            return;
        }
    }

    QWidget::mousePressEvent(event);
}

void SlidingDrawer::mouseReleaseEvent(QMouseEvent *event)
{
    if (_drag_active) {
        _drag_active = false;
        releaseMouse();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

} // namespace widgets
} // namespace pv
