#include "debughelper.h"
#include "../config/appconfig.h"
#include "qtcompat.h"

#include <QApplication>
#include <QMouseEvent>
#include <QCursor>
#include <QScreen>
#include <QDebug>
#include <QPushButton>
#include <QToolButton>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QDockWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QMenu>

namespace pv {
namespace ui {

static DebugHelper *s_instance = nullptr;

static QString getWidgetTypeDesc(QWidget *widget)
{
    if (!widget) return "Unknown";

    if (qobject_cast<QPushButton*>(widget)) return "Button";
    if (qobject_cast<QToolButton*>(widget)) return "ToolButton";
    if (qobject_cast<QLineEdit*>(widget)) return "LineEdit";
    if (qobject_cast<QComboBox*>(widget)) return "ComboBox";
    if (qobject_cast<QCheckBox*>(widget)) return "CheckBox";
    if (qobject_cast<QRadioButton*>(widget)) return "RadioButton";
    if (qobject_cast<QSlider*>(widget)) return "Slider";
    if (qobject_cast<QSpinBox*>(widget)) return "SpinBox";
    if (qobject_cast<QDoubleSpinBox*>(widget)) return "DoubleSpinBox";
    if (qobject_cast<QTextEdit*>(widget)) return "TextEdit";
    if (qobject_cast<QPlainTextEdit*>(widget)) return "PlainTextEdit";
    if (qobject_cast<QTableWidget*>(widget)) return "TableWidget";
    if (qobject_cast<QTreeWidget*>(widget)) return "TreeWidget";
    if (qobject_cast<QListWidget*>(widget)) return "ListWidget";
    if (qobject_cast<QTabWidget*>(widget)) return "TabWidget";
    if (qobject_cast<QGroupBox*>(widget)) return "GroupBox";
    if (qobject_cast<QLabel*>(widget)) return "Label";
    if (qobject_cast<QMenu*>(widget)) return "Menu";
    if (qobject_cast<QDockWidget*>(widget)) return "DockWidget";

    return "Widget";
}

class DebugEventFilter : public QObject
{
public:
    DebugEventFilter(DebugHelper *helper, QObject *parent = nullptr)
        : QObject(parent), _helper(helper)
    {
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (!_helper->isEnabled())
            return false;

        QWidget *widget = qobject_cast<QWidget*>(obj);
        if (!widget)
            return false;

        QString sourceFile = widget->property("_debug_source_file").toString();

        if (event->type() == QEvent::Enter || event->type() == QEvent::HoverEnter)
        {
            if (_helper->currentWidget() == widget)
                return false;

            QString tooltipText = _helper->buildTooltipText(widget);
            QPoint pos = QCursor::pos();
            pos.setX(pos.x() + 15);
            pos.setY(pos.y() + 15);
            _helper->showTooltip(tooltipText, pos, widget);

            if (_helper->isLoggingEnabled()) {
                QString widgetName = widget->objectName();
                QString className = widget->metaObject()->className();
                QString typeDesc = getWidgetTypeDesc(widget);
                QString logMsg = QString("[DebugHelper] Hover: %1 (%2) - %3")
                    .arg(className)
                    .arg(typeDesc)
                    .arg(widgetName.isEmpty() ? "unnamed" : widgetName);
                _helper->logDebug(logMsg);
            }
        }
        else if (event->type() == QEvent::MouseMove)
        {
            QPoint pos = QCursor::pos();
            pos.setX(pos.x() + 15);
            pos.setY(pos.y() + 15);

            QWidget *actualWidget = QApplication::widgetAt(QCursor::pos());
            if (actualWidget) {
                if (_helper->currentWidget() != actualWidget) {
                    QString newText = _helper->buildTooltipText(actualWidget);
                    _helper->showTooltip(newText, pos, actualWidget);
                } else {
                    _helper->moveTooltip(pos);
                }
            }
        }
        else if (event->type() == QEvent::Leave || event->type() == QEvent::HoverLeave)
        {
            if (_helper->currentWidget() == widget) {
                QPoint globalPos = QCursor::pos();
                QPoint topLeft = widget->mapToGlobal(QPoint(0, 0));
                QRect widgetRect(topLeft.x(), topLeft.y(), widget->width(), widget->height());
                if (!widgetRect.contains(globalPos)) {
                    _helper->hideTooltip();
                }
            }
        }
        else if (event->type() == QEvent::MouseButtonPress)
        {
            if (_helper->isLoggingEnabled()) {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
                QString widgetName = widget->objectName();
                QString className = widget->metaObject()->className();
                QString typeDesc = getWidgetTypeDesc(widget);

                QString logMsg = QString("[DebugHelper] Click: %1 (%2) - %3, Button: %4, Pos: (%5,%6)")
                    .arg(className)
                    .arg(typeDesc)
                    .arg(widgetName.isEmpty() ? "unnamed" : widgetName)
                    .arg(mouseEvent->button())
                    .arg(QT_COMPAT_X(mouseEvent))
                    .arg(QT_COMPAT_Y(mouseEvent));
                _helper->logDebug(logMsg);
            }
        }

        return false;
    }

private:
    DebugHelper *_helper;
};

DebugHelper::DebugHelper(QObject *parent)
    : QObject(parent)
    , _enabled(true)
    , _loggingEnabled(true)
    , _tooltipLabel(nullptr)
    , _hideTimer(nullptr)
    , _currentWidget(nullptr)
{
    _tooltipLabel = new QLabel(nullptr, Qt::ToolTip | Qt::FramelessWindowHint);
    _tooltipLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    QString dbgBg = AppConfig::Instance().GetThemeTokenValue("@debug-tooltip-bg");
    QString dbgFg = AppConfig::Instance().GetThemeTokenValue("@debug-tooltip-fg");
    QString dbgBorder = AppConfig::Instance().GetThemeTokenValue("@debug-tooltip-border");
    if (dbgBg.isEmpty()) dbgBg = "#2d2d2d";
    if (dbgFg.isEmpty()) dbgFg = "#00ff00";
    if (dbgBorder.isEmpty()) dbgBorder = "#00ff00";
    _tooltipLabel->setStyleSheet(
        QStringLiteral("QLabel {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  padding: 8px;"
        "  font-family: monospace;"
        "  font-size: 11px;"
        "}"
    ).arg(dbgBg, dbgFg, dbgBorder));
    _tooltipLabel->setFrameStyle(QFrame::Panel | QFrame::Raised);

    _hideTimer = new QTimer(this);
    _hideTimer->setSingleShot(true);
    connect(_hideTimer, &QTimer::timeout, this, &DebugHelper::hideTooltip);
}

DebugHelper::~DebugHelper()
{
    if (_tooltipLabel)
    {
        _tooltipLabel->deleteLater();
        _tooltipLabel = nullptr;
    }
}

DebugHelper* DebugHelper::Instance()
{
    if (!s_instance)
    {
        s_instance = new DebugHelper();
    }
    return s_instance;
}

void DebugHelper::setEnabled(bool enabled)
{
    _enabled = enabled;
    if (!_enabled)
    {
        hideTooltip();
    }
}

bool DebugHelper::isEnabled() const
{
    return _enabled;
}

void DebugHelper::setLoggingEnabled(bool enabled)
{
    _loggingEnabled = enabled;
}

bool DebugHelper::isLoggingEnabled() const
{
    return _loggingEnabled;
}

void DebugHelper::logDebug(const QString &message)
{
    if (_loggingEnabled) {
        qDebug() << message;
    }
}

void DebugHelper::installOnWidget(QWidget *widget, const QString &sourceFile)
{
    if (!widget)
        return;

    widget->setProperty("_debug_source_file", sourceFile);

    widget->setAttribute(Qt::WA_Hover, true);
    widget->setMouseTracking(true);
    if (!widget->property("_debug_filter_installed").toBool()) {
        widget->installEventFilter(new DebugEventFilter(this, widget));
        widget->setProperty("_debug_filter_installed", true);
    }
}

void DebugHelper::installOnWidgetTree(QWidget *rootWidget, const QString &sourceFile)
{
    if (!rootWidget)
        return;

    installOnWidget(rootWidget, sourceFile);

    const QObjectList &children = rootWidget->children();
    for (QObject *child : children)
    {
        QWidget *childWidget = qobject_cast<QWidget*>(child);
        if (childWidget)
        {
            QString existingSource = childWidget->property("_debug_source_file").toString();
            if (!existingSource.isEmpty()) {
                continue;
            }
            installOnWidgetTree(childWidget, sourceFile);
        }
    }
}

void DebugHelper::showTooltip(const QString &text, const QPoint &pos, QWidget *widget)
{
    if (!_tooltipLabel || !_enabled)
        return;

    _currentWidget = widget;
    _tooltipLabel->setText(text);
    _tooltipLabel->adjustSize();

    QRect screenRect = QApplication::primaryScreen()->availableGeometry();
    QPoint tooltipPos = pos;

    if (tooltipPos.x() + _tooltipLabel->width() > screenRect.right())
    {
        tooltipPos.setX(screenRect.right() - _tooltipLabel->width() - 5);
    }
    if (tooltipPos.y() + _tooltipLabel->height() > screenRect.bottom())
    {
        tooltipPos.setY(pos.y() + 20);
    }
    if (tooltipPos.y() < screenRect.top())
    {
        tooltipPos.setY(screenRect.top() + 5);
    }

    _tooltipLabel->move(tooltipPos);
    _tooltipLabel->show();
    _tooltipLabel->raise();

    _hideTimer->start(5000);
}

void DebugHelper::hideTooltip()
{
    if (_tooltipLabel)
    {
        _tooltipLabel->hide();
    }
    _hideTimer->stop();
    _currentWidget = nullptr;
}

void DebugHelper::moveTooltip(const QPoint &pos)
{
    if (_tooltipLabel && _tooltipLabel->isVisible())
    {
        _tooltipLabel->move(pos);
    }
}

void DebugHelper::updateTooltip(const QString &text, const QPoint &pos)
{
    if (_tooltipLabel && _tooltipLabel->isVisible())
    {
        _tooltipLabel->setText(text);
        _tooltipLabel->adjustSize();
        _tooltipLabel->move(pos);
    }
}

QString DebugHelper::buildTooltipText(QWidget *widget)
{
    if (!widget)
        return QString();

    QString widgetName = widget->objectName();
    QString className = widget->metaObject()->className();
    QString typeDesc = getWidgetTypeDesc(widget);
    QString sourceFile = widget->property("_debug_source_file").toString();

    QString tooltipText;
    tooltipText += QString("=== Widget Info ===\n");
    tooltipText += QString("Class: %1\n").arg(className);
    tooltipText += QString("Type: %1\n").arg(typeDesc);
    if (!widgetName.isEmpty())
        tooltipText += QString("Name: %1\n").arg(widgetName);

    tooltipText += QString("\n=== Source Files ===\n");
    QWidget *current = widget;
    int level = 0;
    while (current && level < 10) {
        QString sf = current->property("_debug_source_file").toString();
        QString cls = current->metaObject()->className();
        if (!sf.isEmpty()) {
            tooltipText += QString("[%1] %2: %3\n").arg(level).arg(cls).arg(sf);
        } else {
            tooltipText += QString("[%1] %2: (no source)\n").arg(level).arg(cls);
        }
        current = current->parentWidget();
        level++;
    }

    tooltipText += QString("\n=== Active Source ===\n");
    if (!sourceFile.isEmpty()) {
        tooltipText += sourceFile;
    } else {
        tooltipText += "(inherited from parent)";
    }

    return tooltipText;
}

} // namespace ui
} // namespace pv
