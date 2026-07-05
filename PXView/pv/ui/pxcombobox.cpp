#include "pxcombobox.h"
#include <QFontMetrics>
#include <QGuiApplication>
#include <QScreen>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QLibrary>
#include "../config/appconfig.h"
#include "../ui/dockfonts.h"
#include "../widgets/smoothscrollarea.h"

#ifdef WIN32
#include <windows.h>
#endif

PxComboPopup::PxComboPopup(QComboBox *combo, QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName("dsComboPopup");
    _combo = combo;

    int w = combo->width();
    int maxH = 400;

    QVBoxLayout *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(0, 0, 0, 0);
    mainLay->setSpacing(0);

    QWidget *listPanel = new QWidget(this);
    QVBoxLayout *listLay = new QVBoxLayout(listPanel);
    listLay->setContentsMargins(2, 2, 2, 2);
    listLay->setSpacing(0);
    listLay->setAlignment(Qt::AlignTop);

    QFont font = dock_font_content();
    int curIndex = combo->currentIndex();
    int itemH = 0;

    for (int i = 0; i < combo->count(); i++) {
        QPushButton *bt = new QPushButton(combo->itemText(i), listPanel);
        bt->setObjectName("flat");
        bt->setFont(font);
        bt->setMinimumWidth(w - 8);
        bt->setMaximumWidth(w - 8);

        if (i == curIndex) {
            bt->setProperty("current", true);
        }

        connect(bt, &QPushButton::clicked, this, &PxComboPopup::on_item_clicked);
        _itemButtons.push_back(bt);
        listLay->addWidget(bt);

        if (itemH == 0) {
            itemH = bt->sizeHint().height();
        }
    }

    int totalH = combo->count() * itemH + 8;
    if (totalH > maxH)
        totalH = maxH;
    if (totalH < itemH + 8)
        totalH = itemH + 8;

    pv::widgets::SmoothScrollArea *scroll = new pv::widgets::SmoothScrollArea(this);
    scroll->setWidget(listPanel);
    scroll->setObjectName("dock_search_combo_scroll");
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedSize(w, totalH);
    scroll->setLongTailAnimation(true);

    mainLay->addWidget(scroll);

    this->setFixedSize(w, totalH);

    QPoint gp = combo->mapToGlobal(QPoint(0, combo->height()));
    QScreen *screen = QGuiApplication::screenAt(gp);
    if (screen) {
        QRect screenGeom = screen->availableGeometry();
        if (gp.y() + totalH > screenGeom.bottom()) {
            gp.setY(combo->mapToGlobal(QPoint(0, 0)).y() - totalH);
        }
    }
    this->move(gp);

    if (curIndex >= 0 && curIndex < _itemButtons.size()) {
        QPushButton *curBt = _itemButtons[curIndex];
        scroll->ensureWidgetVisible(curBt);
    }
}

void PxComboPopup::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange) {
        if (!this->isActiveWindow()) {
            this->close();
            return;
        }
    }
    QDialog::changeEvent(event);
}

void PxComboPopup::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

#ifdef WIN32
    const DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33;
    const DWORD DWMWCP_DONOTROUND = 1;
    typedef HRESULT(WINAPI *tDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
    tDwmSetWindowAttribute pDwmSetWindowAttribute =
        tDwmSetWindowAttribute(QLibrary::resolve("dwmapi", "DwmSetWindowAttribute"));
    if (pDwmSetWindowAttribute) {
        HWND hwnd = (HWND)this->winId();
        DWORD preference = DWMWCP_DONOTROUND;
        pDwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
    }
#endif
}

void PxComboPopup::on_item_clicked()
{
    QPushButton *bt = qobject_cast<QPushButton *>(sender());
    if (!bt || !_combo)
        return;

    int index = _itemButtons.indexOf(bt);
    if (index >= 0) {
        _combo->setCurrentIndex(index);
    }

    this->close();
}

PxComboBox::PxComboBox(QWidget *parent)
    : QComboBox(parent)
{
    _bPopup = false;
    QComboBox::setSizeAdjustPolicy(QComboBox::AdjustToContents);
}

PxComboBox::~PxComboBox()
{
}

void PxComboBox::measureSize()
{
    int num = this->count();
    int maxWidth = 0;
    QFontMetrics fm = this->fontMetrics();

    for (int i = 0; i < num; i++) {
        QString text = this->itemText(i);
        QRect rc = fm.boundingRect(text);

        if (rc.width() > maxWidth) {
            maxWidth = rc.width();
        }
    }
}

void PxComboBox::showPopup()
{
    _bPopup = true;

    if (count() == 0) {
        return;
    }

    PxComboPopup *popup = new PxComboPopup(this, this);
    popup->show();
}

void PxComboBox::hidePopup()
{
    QComboBox::hidePopup();
    _bPopup = false;
}
