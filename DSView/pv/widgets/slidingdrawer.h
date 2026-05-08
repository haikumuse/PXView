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

#ifndef DSVIEW_PV_WIDGETS_SLIDINGDRAWER_H
#define DSVIEW_PV_WIDGETS_SLIDINGDRAWER_H

#include <QWidget>
#include <QParallelAnimationGroup>
#include <QStackedWidget>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>

class QLabel;
class QGraphicsOpacityEffect;

namespace pv {
namespace widgets {

/**
 * A sliding drawer panel that translates in from the right side of the parent widget,
 * overlaying the content. Similar to Tailwind UI drawer pattern.
 *
 * The panel slides as a whole unit (translate-x), NOT clip-reveal.
 * Supports drag-to-resize via the left edge of the panel.
 *
 * Usage:
 *   SlidingDrawer *drawer = new SlidingDrawer(parent);
 *   drawer->addPage(myWidget, "Page Title");
 *   drawer->open(0);  // open page index 0
 *   drawer->close();
 */
class SlidingDrawer : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal slideProgress READ slideProgress WRITE setSlideProgress)

public:
    explicit SlidingDrawer(QWidget *parent = nullptr);
    ~SlidingDrawer();

    int addPage(QWidget *content, const QString &title = QString());
    void removePage(int index);
    QWidget* page(int index) const;
    int pageCount() const;

    void open(int pageIndex);
    void close();
    void toggle(int pageIndex);

    bool isOpen() const;
    bool isAnimating() const;

    void setDrawerWidth(int width);
    int drawerWidth() const;

    void setAnimationDuration(int ms);
    int animationDuration() const;

    void setBackdropEnabled(bool enabled);
    bool backdropEnabled() const;

    void setBackdropColor(const QColor &color);
    QColor backdropColor() const;

    void setPageTitle(int index, const QString &title);

signals:
    void drawerOpened(int pageIndex);
    void drawerClosed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    qreal slideProgress() const;
    void setSlideProgress(qreal progress);
    void updatePanelGeometry();
    void finishClose();

    struct PageInfo {
        QWidget *content;
        QString title;
    };

    QVector<PageInfo> _pages;
    QStackedWidget *_stacked_widget;

    // Panel: always full drawer_width, translates left/right
    QWidget *_panel;
    QWidget *_panel_content;
    QWidget *_title_bar;
    QLabel *_title_label;

    // Backdrop (semi-transparent overlay)
    QWidget *_backdrop;
    QGraphicsOpacityEffect *_backdrop_opacity_effect;

    // Animation
    QParallelAnimationGroup *_open_group;
    QParallelAnimationGroup *_close_group;

    qreal _slide_progress;      // 0.0 = closed (off-screen right), 1.0 = open (fully visible)
    int _drawer_width;
    int _animation_duration;
    bool _backdrop_enabled;
    QColor _backdrop_color;
    int _current_page;
    bool _is_open;
    bool _is_animating;

    // Drag state (edge resize)
    bool _drag_active;
    int _drag_start_drawer_width;
    QPoint _drag_start_pos;

    static constexpr int EDGE_GRIP_WIDTH = 6; // pixels from left edge for resize grip
};

} // namespace widgets
} // namespace pv

#endif // DSVIEW_PV_WIDGETS_SLIDINGDRAWER_H
