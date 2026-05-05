#ifndef DSVIEW_PV_UI_DEBUGHELPER_H
#define DSVIEW_PV_UI_DEBUGHELPER_H

#include <QObject>
#include <QLabel>
#include <QWidget>
#include <QEvent>

namespace pv {
namespace ui {

class DebugHelper : public QObject
{
    Q_OBJECT

public:
    explicit DebugHelper(QObject *parent = nullptr);
    ~DebugHelper();
    void install();
    void uninstall();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void updateInfo(const QPoint &globalPos);
    void clearHighlight();
    QString buildParentChain(QWidget *widget);

    QLabel *_infoLabel;
    QWidget *_lastWidget;
    QString _lastWidgetStyle;
    bool _updating;
};

}
}

#endif
