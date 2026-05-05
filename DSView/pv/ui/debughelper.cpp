#include "debughelper.h"
#include "../log.h"
#include <QApplication>
#include <QMouseEvent>
#include <QScreen>

namespace pv {
namespace ui {

DebugHelper::DebugHelper(QObject *parent) :
    QObject(parent),
    _lastWidget(nullptr),
    _updating(false)
{
    _infoLabel = new QLabel();
    _infoLabel->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    _infoLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    _infoLabel->setAttribute(Qt::WA_ShowWithoutActivating);
    _infoLabel->setStyleSheet(
        "background: rgba(30,30,30,220);"
        "color: #E0E0E0;"
        "padding: 8px;"
        "border-radius: 6px;"
        "font-family: Consolas, Courier New, monospace;"
        "font-size: 12px;"
        "border: 1px solid rgba(255,255,255,30);");
    _infoLabel->setWordWrap(false);
    _infoLabel->setFixedWidth(420);

    dsv_info("Debug Helper: enabled (compile-time)");
}

DebugHelper::~DebugHelper()
{
    uninstall();
}

void DebugHelper::install()
{
    qApp->installEventFilter(this);
}

void DebugHelper::uninstall()
{
    qApp->removeEventFilter(this);
    clearHighlight();
    _infoLabel->hide();
}

void DebugHelper::clearHighlight()
{
    if (_lastWidget) {
        _lastWidget->setStyleSheet(_lastWidgetStyle);
        _lastWidget = nullptr;
        _lastWidgetStyle = QString();
    }
}

bool DebugHelper::eventFilter(QObject *watched, QEvent *event)
{
    if (_updating)
        return false;

    if (event->type() == QEvent::MouseMove) {
        updateInfo(static_cast<QMouseEvent*>(event)->globalPos());
        return false;
    }

    if (event->type() == QEvent::Leave && watched->isWidgetType()) {
        clearHighlight();
        _infoLabel->hide();
        return false;
    }

    return false;
}

void DebugHelper::updateInfo(const QPoint &globalPos)
{
    _updating = true;

    _infoLabel->hide();

    QWidget *widget = QApplication::widgetAt(globalPos);

    if (!widget) {
        clearHighlight();
        _updating = false;
        return;
    }

    if (widget == _lastWidget) {
        QPoint infoPos(globalPos.x() + 15, globalPos.y() + 15);
        QScreen *screen = QGuiApplication::screenAt(globalPos);
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        if (screen) {
            QRect screenGeom = screen->availableGeometry();
            if (infoPos.x() + _infoLabel->width() > screenGeom.right())
                infoPos.setX(globalPos.x() - _infoLabel->width() - 15);
            if (infoPos.y() + _infoLabel->height() > screenGeom.bottom())
                infoPos.setY(globalPos.y() - _infoLabel->height() - 15);
        }
        _infoLabel->move(infoPos);
        _infoLabel->show();
        _updating = false;
        return;
    }

    clearHighlight();

    _lastWidget = widget;
    _lastWidgetStyle = widget->styleSheet();
    widget->setStyleSheet(_lastWidgetStyle + "; border: 2px solid rgba(255,0,0,180);");

    QString objectName = widget->objectName().isEmpty()
        ? QString("&lt;unnamed&gt;") : widget->objectName();
    QString parentChain = buildParentChain(widget);

    QString text;
    text += QString("<b>Class:</b> %1<br>").arg(widget->metaObject()->className());
    text += QString("<b>objectName:</b> %1<br>").arg(objectName);
    text += QString("<b>Parent Chain:</b> %1<br>").arg(parentChain);
    text += QString("<b>Geometry:</b> x=%1, y=%2, w=%3, h=%4<br>")
        .arg(widget->x()).arg(widget->y()).arg(widget->width()).arg(widget->height());
    text += QString("<b>Visible:</b> %1").arg(widget->isVisible() ? "true" : "false");

    _infoLabel->setText(text);
    _infoLabel->adjustSize();

    QPoint infoPos(globalPos.x() + 15, globalPos.y() + 15);
    QScreen *screen = QGuiApplication::screenAt(globalPos);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeom = screen->availableGeometry();
        if (infoPos.x() + _infoLabel->width() > screenGeom.right())
            infoPos.setX(globalPos.x() - _infoLabel->width() - 15);
        if (infoPos.y() + _infoLabel->height() > screenGeom.bottom())
            infoPos.setY(globalPos.y() - _infoLabel->height() - 15);
    }
    _infoLabel->move(infoPos);
    _infoLabel->show();

    _updating = false;
}

QString DebugHelper::buildParentChain(QWidget *widget)
{
    QStringList parts;
    QWidget *w = widget;
    while (w) {
        QString name = w->objectName().isEmpty()
            ? QString("&lt;unnamed&gt;") : w->objectName();
        parts.prepend(QString("%1(%2)").arg(w->metaObject()->className()).arg(name));
        w = w->parentWidget();
    }
    return parts.join(" -> ");
}

}
}
