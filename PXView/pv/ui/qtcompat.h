#ifndef DSVIEW_QTCOMPAT_H
#define DSVIEW_QTCOMPAT_H

#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using NativeEventResult = qintptr;
#else
using NativeEventResult = long;
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define QT_COMPAT_POS(event) (event->position().toPoint())
#define QT_COMPAT_X(event) ((int)event->position().x())
#define QT_COMPAT_Y(event) ((int)event->position().y())
#define QT_COMPAT_GLOBAL_POS(event) (event->globalPosition().toPoint())
#else
#define QT_COMPAT_POS(event) (event->pos())
#define QT_COMPAT_X(event) (event->x())
#define QT_COMPAT_Y(event) (event->y())
#define QT_COMPAT_GLOBAL_POS(event) (event->globalPos())
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define QT_COMPAT_WHEEL_POS(event) ((int)event->position().x())
#define QT_COMPAT_WHEEL_DELTA(event) (event->angleDelta().y() != 0 ? event->angleDelta().y() : event->angleDelta().x())
#define QT_COMPAT_WHEEL_IS_VERTICAL(event) (event->angleDelta().y() != 0 && (event->angleDelta().x() == 0 || qAbs(event->angleDelta().y()) >= qAbs(event->angleDelta().x())))
#else
#define QT_COMPAT_WHEEL_POS(event) (event->x())
#define QT_COMPAT_WHEEL_DELTA(event) (event->delta())
#define QT_COMPAT_WHEEL_IS_VERTICAL(event) (event->orientation() == Qt::Vertical)
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define QT_COMPAT_FONT_WIDTH(fontMetrics, text) (fontMetrics.horizontalAdvance(text))
#else
#define QT_COMPAT_FONT_WIDTH(fontMetrics, text) (fontMetrics.width(text))
#endif

#endif
