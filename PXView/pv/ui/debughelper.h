#ifndef DSVIEW_PV_UI_DEBUGHELPER_H
#define DSVIEW_PV_UI_DEBUGHELPER_H

#include <QObject>
#include <QWidget>
#include <QString>
#include <QPoint>
#include <QTimer>
#include <QLabel>

namespace pv {
namespace ui {

class DebugHelper : public QObject
{
    Q_OBJECT

public:
    static DebugHelper* Instance();

    void setEnabled(bool enabled);
    bool isEnabled() const;

    void setLoggingEnabled(bool enabled);
    bool isLoggingEnabled() const;

    void logDebug(const QString &message);

    void installOnWidget(QWidget *widget, const QString &sourceFile);
    void installOnWidgetTree(QWidget *rootWidget, const QString &sourceFile);

    void showTooltip(const QString &text, const QPoint &pos, QWidget *widget = nullptr);
    void hideTooltip();
    void moveTooltip(const QPoint &pos);
    void updateTooltip(const QString &text, const QPoint &pos);

    QString buildTooltipText(QWidget *widget);

    QWidget* currentWidget() const { return _currentWidget; }

private:
    explicit DebugHelper(QObject *parent = nullptr);
    ~DebugHelper();
    DebugHelper(const DebugHelper&) = delete;
    DebugHelper& operator=(const DebugHelper&) = delete;

    bool _enabled;
    bool _loggingEnabled;
    QLabel *_tooltipLabel;
    QTimer *_hideTimer;
    QWidget *_currentWidget;
};

#ifdef ENABLE_DEBUG_HELPER
    #define DEBUG_INSTALL(widget) \
        pv::ui::DebugHelper::Instance()->installOnWidget(widget, QString(__FILE__))
    #define DEBUG_INSTALL_TREE(widget) \
        pv::ui::DebugHelper::Instance()->installOnWidgetTree(widget, QString(__FILE__))
    #define DEBUG_LOG(msg) \
        pv::ui::DebugHelper::Instance()->logDebug(msg)
#else
    #define DEBUG_INSTALL(widget) ((void)0)
    #define DEBUG_INSTALL_TREE(widget) ((void)0)
    #define DEBUG_LOG(msg) ((void)0)
#endif

} // namespace ui
} // namespace pv

#endif // DSVIEW_PV_UI_DEBUGHELPER_H
