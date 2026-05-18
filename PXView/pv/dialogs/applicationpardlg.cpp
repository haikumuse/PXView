/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
 * 
 * Copyright (C) 2021 DreamSourceLab <support@dreamsourcelab.com>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "applicationpardlg.h"
#include "dsdialog.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QString>
#include <QFontDatabase>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QColorDialog>
#include <QRegularExpression>
#include <QFile>
#include <QApplication>
#include <QKeyEvent>
#include <QScrollArea>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <vector>
#include <QGridLayout>
#include <algorithm>

#include "../config/appconfig.h"
#include "../config/shortcutdefs.h"
#include "../ui/langresource.h"
#include "../appcontrol.h"
#include "../sigsession.h"
#include "../ui/dscombobox.h"
#include "../log.h"

ShortcutKeyCapture::ShortcutKeyCapture(QWidget *parent)
    : QLineEdit(parent)
    , m_capturing(false)
{
    setReadOnly(true);
    setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

void ShortcutKeyCapture::setKeySequence(const QString &key)
{
    m_keySeq = key;
    setText(key.isEmpty() ? "" : key);
}

QString ShortcutKeyCapture::keySequence() const
{
    return m_keySeq;
}

void ShortcutKeyCapture::keyPressEvent(QKeyEvent *event)
{
    int key = event->key();
    Qt::KeyboardModifiers modifiers = event->modifiers();

    if (key == Qt::Key_Escape) {
        event->accept();
        clearFocus();
        return;
    }

    if (key == Qt::Key_Backspace || key == Qt::Key_Delete) {
        m_keySeq = "";
        setText("");
        emit keySequenceChanged("");
        event->accept();
        return;
    }

    if (key == Qt::Key_Control || key == Qt::Key_Shift ||
        key == Qt::Key_Alt || key == Qt::Key_Meta) {
        event->accept();
        return;
    }

    QString str;
    if (modifiers & Qt::ControlModifier)
        str += "Ctrl+";
    if (modifiers & Qt::ShiftModifier)
        str += "Shift+";
    if (modifiers & Qt::AltModifier && !str.isEmpty())
        str += "Alt+";

    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        str += QChar(key);
    } else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        str += QChar(key);
    } else if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        str += "F" + QString::number(key - Qt::Key_F1 + 1);
    } else {
        switch (key) {
        case Qt::Key_Space: str += "Space"; break;
        case Qt::Key_Tab: str += "Tab"; break;
        case Qt::Key_Backspace: str += "Backspace"; break;
        case Qt::Key_Return: str += "Return"; break;
        case Qt::Key_Enter: str += "Enter"; break;
        case Qt::Key_Insert: str += "Insert"; break;
        case Qt::Key_Delete: str += "Delete"; break;
        case Qt::Key_Home: str += "Home"; break;
        case Qt::Key_End: str += "End"; break;
        case Qt::Key_PageUp: str += "PgUp"; break;
        case Qt::Key_PageDown: str += "PgDown"; break;
        case Qt::Key_Up: str += "Up"; break;
        case Qt::Key_Down: str += "Down"; break;
        case Qt::Key_Left: str += "Left"; break;
        case Qt::Key_Right: str += "Right"; break;
        case Qt::Key_BracketLeft: str += "["; break;
        case Qt::Key_BracketRight: str += "]"; break;
        case Qt::Key_Pause: str += "Pause"; break;
        case Qt::Key_ScrollLock: str += "ScrollLock"; break;
        case Qt::Key_Minus: str += "-"; break;
        case Qt::Key_Equal: str += "="; break;
        case Qt::Key_Semicolon: str += ";"; break;
        case Qt::Key_Apostrophe: str += "'"; break;
        case Qt::Key_Comma: str += ","; break;
        case Qt::Key_Period: str += "."; break;
        case Qt::Key_Slash: str += "/"; break;
        case Qt::Key_Backslash: str += "\\"; break;
        case Qt::Key_QuoteLeft: str += "`"; break;
        default:
            event->ignore();
            return;
        }
    }

    m_keySeq = str;
    setText(str);
    emit keySequenceChanged(str);
    event->accept();
}

void ShortcutKeyCapture::focusOutEvent(QFocusEvent *event)
{
    QLineEdit::focusOutEvent(event);
}

namespace pv
{
namespace dialogs
{

ApplicationParamDlg::ApplicationParamDlg()
    : _nav_list(nullptr)
    , _page_stack(nullptr)
    , _ck_quickScroll(nullptr)
    , _ck_trigInMid(nullptr)
    , _ck_profileBar(nullptr)
    , _ck_abortData(nullptr)
    , _ck_autoScrollLatestData(nullptr)
    , _ftCbSize(nullptr)
    , _shortcut_list(nullptr)
    , _shortcut_selected_row(-1)
    , _btn_accept(nullptr)
    , _btn_restore(nullptr)
    , _btn_reset_default(nullptr)
    , _btn_delete(nullptr)
    , _clash_warning_label(nullptr)
    , _style_table(nullptr)
{
}

ApplicationParamDlg::~ApplicationParamDlg()
{
}

void ApplicationParamDlg::bind_font_name_list(QComboBox *box, QString v)
{   
    int selDex = -1;

    QString defName(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_DEFAULT_FONT), "Default"));
    box->addItem(defName);

    if (_font_name_list.size() == 0)
    {
        QFontDatabase fDataBase;
        _font_name_list = fDataBase.families();
    }
   
    for (QString family : _font_name_list) {
        if (family.indexOf("[") == -1)
        {
            box->addItem(family);

            if (selDex == -1 && family == v){
                selDex = box->count() - 1;
            }
        }
    }

    if (selDex == -1)
        selDex = 0;

    box->setCurrentIndex(selDex);
}

void ApplicationParamDlg::bind_font_size_list(QComboBox *box, float size)
{   
    int selDex = -1;

    float minSize = 0;
    float maxSize = 0;

    AppConfig::GetFontSizeRange(&minSize, &maxSize);

    for(int i=minSize; i<=maxSize; i++)
    {
        box->addItem(QString::number(i));
        if (i == size){
            selDex = box->count() - 1;
        }
    }
    if (selDex == -1)
        selDex = 2;
    box->setCurrentIndex(selDex);
}

QWidget* ApplicationParamDlg::createDisplayPage()
{
    AppConfig &app = AppConfig::Instance();

    QWidget *page = new QWidget();
    QVBoxLayout *lay = new QVBoxLayout();
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(8);
    lay->setAlignment(Qt::AlignTop);

    _ck_quickScroll = new QCheckBox();
    _ck_quickScroll->setChecked(app.appOptions.quickScroll);

    _ck_trigInMid = new QCheckBox();
    _ck_trigInMid->setChecked(app.appOptions.trigPosDisplayInMid);

    _ck_profileBar = new QCheckBox();
    _ck_profileBar->setChecked(app.appOptions.displayProfileInBar);

    _ck_abortData = new QCheckBox();
    _ck_abortData->setChecked(app.appOptions.swapBackBufferAlways);

    _ck_autoScrollLatestData = new QCheckBox();
    _ck_autoScrollLatestData->setChecked(app.appOptions.autoScrollLatestData);

    _ftCbSize = new DsComboBox();
    _ftCbSize->setFixedWidth(50);
    bind_font_size_list(_ftCbSize, app.appOptions.fontSize);

    QGroupBox *logicGroup = new QGroupBox(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_GROUP_LOGIC), "Logic"));
    QGridLayout *logicLay = new QGridLayout();
    logicLay->setContentsMargins(10, 15, 15, 10);
    logicLay->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    logicGroup->setLayout(logicLay);
    logicLay->addWidget(new QLabel(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_QUICK_SCROLL), "Quick scroll")), 0, 0, Qt::AlignLeft);
    logicLay->addWidget(_ck_quickScroll, 0, 1, Qt::AlignRight);
    logicLay->addWidget(new QLabel(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_USE_ABORT_DATA_REPEAT), "Used abort data")), 1, 0, Qt::AlignLeft);
    logicLay->addWidget(_ck_abortData, 1, 1, Qt::AlignRight);
    logicLay->addWidget(new QLabel(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_AUTO_SCROLL_LATEAST_DATA), "Auto scoll latest")), 2, 0, Qt::AlignLeft);
    logicLay->addWidget(_ck_autoScrollLatestData, 2, 1, Qt::AlignRight);
    lay->addWidget(logicGroup);

    QGroupBox *dsoGroup = new QGroupBox(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_GROUP_DSO), "Scope"));
    QGridLayout *dsoLay = new QGridLayout();
    dsoLay->setContentsMargins(10, 15, 15, 10);
    dsoLay->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    dsoGroup->setLayout(dsoLay);
    dsoLay->addWidget(new QLabel(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIG_DISPLAY_MIDDLE), "Tig pos in middle")), 0, 0, Qt::AlignLeft);
    dsoLay->addWidget(_ck_trigInMid, 0, 1, Qt::AlignRight);
    lay->addWidget(dsoGroup);

    QGroupBox *uiGroup = new QGroupBox(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_GROUP_UI), "UI"));
    QGridLayout *uiLay = new QGridLayout();
    uiLay->setContentsMargins(10, 15, 15, 10);
    uiLay->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    uiGroup->setLayout(uiLay);
    uiLay->addWidget(new QLabel(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_DISPLAY_PROFILE_IN_BAR), "Profile in bar")), 0, 0, Qt::AlignLeft);
    uiLay->addWidget(_ck_profileBar, 0, 1, Qt::AlignRight);
    uiLay->addWidget(new QLabel(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_FONT_SIZE), "Font size")), 1, 0, Qt::AlignLeft);
    uiLay->addWidget(_ftCbSize, 1, 1, Qt::AlignRight);
    lay->addWidget(uiGroup);

    lay->addStretch();
    page->setLayout(lay);
    return page;
}

QWidget* ApplicationParamDlg::createShortcutPage()
{
    int infoCount = 0;
    const ShortcutActionInfo *infos = GetShortcutActionInfos(&infoCount);

    _shortcut_keys.clear();
    _shortcut_original_keys.clear();
    _shortcut_clash_ids.clear();
    _shortcut_selected_row = -1;

    AppConfig &app = AppConfig::Instance();
    for (int i = 0; i < infoCount; i++) {
        QString keySeq(infos[i].keySequence);
        for (int j = 0; j < app.shortcutOptions.items.size(); j++) {
            if (app.shortcutOptions.items[j].actionId == infos[i].actionId) {
                keySeq = app.shortcutOptions.items[j].keySequence;
                break;
            }
        }
        _shortcut_keys[infos[i].actionId] = keySeq;
        _shortcut_original_keys[infos[i].actionId] = keySeq;
    }

    QWidget *page = new QWidget();
    QVBoxLayout *pageLay = new QVBoxLayout();
    pageLay->setContentsMargins(10, 10, 10, 10);
    pageLay->setSpacing(5);

    QHBoxLayout *headerLay = new QHBoxLayout();
    headerLay->setSpacing(5);
    QLabel *actionLabel = new QLabel(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SC_ACTION), "Action"));
    actionLabel->setFixedWidth(150);
    QLabel *shortcutLabel = new QLabel(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SC_SHORTCUT), "Shortcut"));
    headerLay->addWidget(actionLabel);
    headerLay->addWidget(shortcutLabel, 1);
    pageLay->addLayout(headerLay);

    QHBoxLayout *contentLay = new QHBoxLayout();
    contentLay->setSpacing(5);

    _shortcut_list = new QListWidget();
    _shortcut_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _shortcut_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    _shortcut_list->setSelectionMode(QAbstractItemView::SingleSelection);
    _shortcut_list->setFocusPolicy(Qt::NoFocus);

    for (int i = 0; i < infoCount; i++) {
        QString actionName = L_S(STR_PAGE_DLG, infos[i].displayName, infos[i].displayName);
        QString keyStr = _shortcut_keys[infos[i].actionId];

        QWidget *itemWidget = new QWidget();
        QHBoxLayout *itemLay = new QHBoxLayout(itemWidget);
        itemLay->setContentsMargins(5, 0, 5, 0);
        itemLay->setSpacing(0);

        QLabel *nameLabel = new QLabel(actionName);
        nameLabel->setFixedWidth(140);
        nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        QLabel *keyLabel = new QLabel(keyStr.isEmpty() ? "" : keyStr);
        keyLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        keyLabel->setObjectName("shortcut_key_label");

        QLabel *clashIcon = new QLabel();
        clashIcon->setFixedWidth(16);
        clashIcon->setTextFormat(Qt::PlainText);
        clashIcon->setObjectName("clash_icon");
        clashIcon->hide();

        itemLay->addWidget(nameLabel);
        itemLay->addWidget(keyLabel, 1);
        itemLay->addWidget(clashIcon);

        QListWidgetItem *listItem = new QListWidgetItem(_shortcut_list);
        listItem->setSizeHint(QSize(0, 24));
        _shortcut_list->addItem(listItem);
        _shortcut_list->setItemWidget(listItem, itemWidget);
    }

    QObject::connect(_shortcut_list, &QListWidget::currentRowChanged, [this](int row){
        onShortcutRowSelected(row);
    });

    contentLay->addWidget(_shortcut_list, 1);

    QVBoxLayout *btnLay = new QVBoxLayout();
    btnLay->setSpacing(4);
    btnLay->setContentsMargins(0, 0, 0, 0);

    _btn_accept = new QPushButton(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SC_ACCEPT), "Accept"));
    _btn_accept->setFixedWidth(82);
    _btn_accept->setEnabled(false);

    _btn_restore = new QPushButton(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SC_RESTORE), "Restore"));
    _btn_restore->setFixedWidth(82);
    _btn_restore->setEnabled(false);

    _btn_reset_default = new QPushButton(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SC_RESET_DEFAULT), "Reset Default"));
    _btn_reset_default->setFixedWidth(82);
    _btn_reset_default->setEnabled(false);

    _btn_delete = new QPushButton(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SC_DELETE_KEY), "Delete Key"));
    _btn_delete->setFixedWidth(82);
    _btn_delete->setEnabled(false);

    QFrame *sepLine = new QFrame();
    sepLine->setFrameShape(QFrame::HLine);
    sepLine->setFixedHeight(1);

    QPushButton *btnResetAll = new QPushButton(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SC_RESET_ALL), "Reset All"));
    btnResetAll->setFixedWidth(82);

    btnLay->addWidget(_btn_accept);
    btnLay->addWidget(_btn_restore);
    btnLay->addWidget(_btn_reset_default);
    btnLay->addWidget(_btn_delete);
    btnLay->addSpacing(6);
    btnLay->addWidget(sepLine);
    btnLay->addSpacing(6);
    btnLay->addWidget(btnResetAll);
    btnLay->addStretch();

    contentLay->addLayout(btnLay);
    pageLay->addLayout(contentLay, 1);

    _clash_warning_label = new QLabel();
    _clash_warning_label->setObjectName("clash_warning");
    _clash_warning_label->setFixedHeight(28);
    _clash_warning_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    _clash_warning_label->hide();
    pageLay->addWidget(_clash_warning_label);

    QObject::connect(_btn_accept, &QPushButton::clicked, _btn_accept, [this](){ onShortcutAccept(); });
    QObject::connect(_btn_restore, &QPushButton::clicked, _btn_restore, [this](){ onShortcutRestore(); });
    QObject::connect(_btn_reset_default, &QPushButton::clicked, _btn_reset_default, [this](){ onShortcutResetDefault(); });
    QObject::connect(_btn_delete, &QPushButton::clicked, _btn_delete, [this](){ onShortcutDelete(); });
    QObject::connect(btnResetAll, &QPushButton::clicked, btnResetAll, [this](){ onResetShortcuts(); });

    checkShortcutClash();

    page->setLayout(pageLay);
    return page;
}

QWidget* ApplicationParamDlg::createStylePage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *lay = new QVBoxLayout();
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(8);

    _default_style_tokens.clear();
    _style_tokens.clear();

    AppConfig &app = AppConfig::Instance();
    QString style = app.frameOptions.style;
    QString qssRes = ":/" + style + ".qss";
    QFile qss(qssRes);
    if (qss.open(QFile::ReadOnly | QFile::Text)) {
        QString qssContent = qss.readAll();
        qss.close();

        QRegularExpression tokenRe("@([\\w-]+):\\s*([^\\r\\n]+?)\\s*(?:\\*/|\\r|\\n)");
        QRegularExpressionMatchIterator it = tokenRe.globalMatch(qssContent);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QString tokenName = "@" + match.captured(1);
            QString tokenValue = match.captured(2).trimmed();
            _default_style_tokens[tokenName] = tokenValue;
        }
    }

    _style_tokens = _default_style_tokens;

    for (int i = 0; i < app.styleOptions.items.size(); i++) {
        _style_tokens[app.styleOptions.items[i].tokenName] = app.styleOptions.items[i].value;
    }

    QStringList tokenNames = _style_tokens.keys();
    std::sort(tokenNames.begin(), tokenNames.end());

    _style_table = new QTableWidget(tokenNames.size(), 3);
    _style_table->setHorizontalHeaderLabels(QStringList()
        << L_S(STR_PAGE_DLG, S_ID(IDS_DLG_STYLE_TOKEN), "Token")
        << L_S(STR_PAGE_DLG, S_ID(IDS_DLG_STYLE_PREVIEW), "Preview")
        << L_S(STR_PAGE_DLG, S_ID(IDS_DLG_STYLE_VALUE), "Value"));
    _style_table->horizontalHeader()->setStretchLastSection(false);
    _style_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    _style_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    _style_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    _style_table->setColumnWidth(1, 60);
    _style_table->setColumnWidth(2, 110);
    _style_table->verticalHeader()->setVisible(false);
    _style_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _style_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    for (int i = 0; i < tokenNames.size(); i++) {
        QString name = tokenNames[i];
        QString value = _style_tokens[name];

        QTableWidgetItem *nameItem = new QTableWidgetItem(name);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        _style_table->setItem(i, 0, nameItem);

        QWidget *previewWidget = new QWidget();
        QColor c(value);
        if (c.isValid()) {
            previewWidget->setStyleSheet(QString("background-color: %1; border: 1px solid #555;").arg(value));
        } else {
            previewWidget->setStyleSheet("background-color: transparent; border: 1px solid #555;");
        }
        previewWidget->setFixedSize(40, 20);
        QWidget *previewContainer = new QWidget();
        QHBoxLayout *previewLay = new QHBoxLayout(previewContainer);
        previewLay->setContentsMargins(2, 2, 2, 2);
        previewLay->setAlignment(Qt::AlignCenter);
        previewLay->addWidget(previewWidget);
        _style_table->setCellWidget(i, 1, previewContainer);

        QPushButton *colorBtn = new QPushButton(value);
        colorBtn->setFixedWidth(100);
        _style_table->setCellWidget(i, 2, colorBtn);

        QObject::connect(colorBtn, &QPushButton::clicked, colorBtn, [this, i](){ onStyleTokenChanged(i); });
    }

    lay->addWidget(_style_table);

    QPushButton *importBtn = new QPushButton(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_STYLE_IMPORT), "Import"));
    importBtn->setFixedWidth(80);
    QObject::connect(importBtn, &QPushButton::clicked, importBtn, [this](){ onImportStyle(); });

    QPushButton *exportBtn = new QPushButton(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_STYLE_EXPORT), "Export"));
    exportBtn->setFixedWidth(80);
    QObject::connect(exportBtn, &QPushButton::clicked, exportBtn, [this](){ onExportStyle(); });

    QPushButton *resetBtn = new QPushButton(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SC_RESET_ALL), "Reset All"));
    resetBtn->setFixedWidth(80);
    QObject::connect(resetBtn, &QPushButton::clicked, resetBtn, [this](){ onResetStyle(); });

    QHBoxLayout *btnLay = new QHBoxLayout();
    btnLay->addStretch();
    btnLay->addWidget(importBtn);
    btnLay->addWidget(exportBtn);
    btnLay->addWidget(resetBtn);
    lay->addLayout(btnLay);

    page->setLayout(lay);
    return page;
}

void ApplicationParamDlg::onShortcutRowSelected(int row)
{
    _shortcut_selected_row = row;

    int infoCount = 0;
    const ShortcutActionInfo *infos = GetShortcutActionInfos(&infoCount);

    for (int i = 0; i < infoCount; i++) {
        QListWidgetItem *item = _shortcut_list->item(i);
        QWidget *w = _shortcut_list->itemWidget(item);
        if (!w) continue;

        ShortcutKeyCapture *capture = w->findChild<ShortcutKeyCapture*>();
        if (capture) {
            capture->deleteLater();
            QLabel *keyLabel = new QLabel(_shortcut_keys[infos[i].actionId].isEmpty() ? "" : _shortcut_keys[infos[i].actionId]);
            keyLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            keyLabel->setObjectName("shortcut_key_label");
            QHBoxLayout *lay = qobject_cast<QHBoxLayout*>(w->layout());
            if (lay) {
                QLayoutItem *oldItem = lay->itemAt(1);
                if (oldItem && oldItem->widget()) {
                    lay->removeWidget(oldItem->widget());
                    oldItem->widget()->deleteLater();
                }
                lay->insertWidget(1, keyLabel, 1);
            }
        }
    }

    if (row >= 0 && row < infoCount) {
        QListWidgetItem *item = _shortcut_list->item(row);
        QWidget *w = _shortcut_list->itemWidget(item);
        if (w) {
            QHBoxLayout *lay = qobject_cast<QHBoxLayout*>(w->layout());
            if (lay) {
                QLayoutItem *oldItem = lay->itemAt(1);
                if (oldItem && oldItem->widget()) {
                    lay->removeWidget(oldItem->widget());
                    oldItem->widget()->deleteLater();
                }

                ShortcutKeyCapture *capture = new ShortcutKeyCapture();
                capture->setKeySequence(_shortcut_keys[infos[row].actionId]);
                capture->setObjectName("shortcut_key_capture");

                QObject::connect(capture, &ShortcutKeyCapture::keySequenceChanged, capture,
                    [this, row](const QString &newKey){
                        onShortcutKeyCaptured(row, newKey);
                    });

                lay->insertWidget(1, capture, 1);
                capture->setFocus();
            }
        }
    }

    updateShortcutButtons();
}

void ApplicationParamDlg::onShortcutKeyCaptured(int row, const QString &newKey)
{
    int infoCount = 0;
    const ShortcutActionInfo *infos = GetShortcutActionInfos(&infoCount);
    if (row < 0 || row >= infoCount)
        return;

    _shortcut_keys[infos[row].actionId] = newKey;
    checkShortcutClash();
    updateShortcutButtons();
}

void ApplicationParamDlg::onShortcutAccept()
{
    saveShortcutOptions();
}

void ApplicationParamDlg::onShortcutRestore()
{
    int infoCount = 0;
    const ShortcutActionInfo *infos = GetShortcutActionInfos(&infoCount);
    if (_shortcut_selected_row < 0 || _shortcut_selected_row >= infoCount)
        return;

    int actionId = infos[_shortcut_selected_row].actionId;
    _shortcut_keys[actionId] = _shortcut_original_keys[actionId];

    refreshShortcutList();
    onShortcutRowSelected(_shortcut_selected_row);
}

void ApplicationParamDlg::onShortcutResetDefault()
{
    int infoCount = 0;
    const ShortcutActionInfo *infos = GetShortcutActionInfos(&infoCount);
    if (_shortcut_selected_row < 0 || _shortcut_selected_row >= infoCount)
        return;

    int actionId = infos[_shortcut_selected_row].actionId;
    for (int i = 0; i < infoCount; i++) {
        if (infos[i].actionId == actionId) {
            _shortcut_keys[actionId] = QString(infos[i].keySequence);
            break;
        }
    }

    refreshShortcutList();
    onShortcutRowSelected(_shortcut_selected_row);
}

void ApplicationParamDlg::onShortcutDelete()
{
    int infoCount = 0;
    const ShortcutActionInfo *infos = GetShortcutActionInfos(&infoCount);
    if (_shortcut_selected_row < 0 || _shortcut_selected_row >= infoCount)
        return;

    int actionId = infos[_shortcut_selected_row].actionId;
    _shortcut_keys[actionId] = "";

    refreshShortcutList();
    onShortcutRowSelected(_shortcut_selected_row);
}

void ApplicationParamDlg::onResetShortcuts()
{
    int infoCount = 0;
    const ShortcutActionInfo *infos = GetShortcutActionInfos(&infoCount);

    _shortcut_keys.clear();
    for (int i = 0; i < infoCount; i++) {
        _shortcut_keys[infos[i].actionId] = QString(infos[i].keySequence);
    }

    _shortcut_selected_row = -1;
    refreshShortcutList();
    checkShortcutClash();
    updateShortcutButtons();
}

void ApplicationParamDlg::checkShortcutClash()
{
    _shortcut_clash_ids.clear();

    QMap<int, QString>::const_iterator it1, it2;
    for (it1 = _shortcut_keys.constBegin(); it1 != _shortcut_keys.constEnd(); ++it1) {
        if (it1.value().isEmpty()) continue;
        for (it2 = _shortcut_keys.constBegin(); it2 != _shortcut_keys.constEnd(); ++it2) {
            if (it1.key() != it2.key() && it1.value() == it2.value()) {
                _shortcut_clash_ids.insert(it1.key());
                _shortcut_clash_ids.insert(it2.key());
            }
        }
    }

    int infoCount = 0;
    const ShortcutActionInfo *infos = GetShortcutActionInfos(&infoCount);

    for (int i = 0; i < infoCount; i++) {
        QListWidgetItem *item = _shortcut_list->item(i);
        QWidget *w = _shortcut_list->itemWidget(item);
        if (!w) continue;

        QLabel *clashIcon = w->findChild<QLabel*>("clash_icon");
        if (clashIcon) {
            if (_shortcut_clash_ids.contains(infos[i].actionId)) {
                clashIcon->setText("!");
                clashIcon->setStyleSheet("color: #ff6600; font-weight: bold; font-size: 14px;");
                clashIcon->show();
            } else {
                clashIcon->hide();
            }
        }
    }

    if (!_shortcut_clash_ids.isEmpty()) {
        _clash_warning_label->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SC_CLASH_WARNING), "Clashing shortcuts will be ignored."));
        _clash_warning_label->setStyleSheet("color: #ff6600; padding-left: 5px;");
        _clash_warning_label->show();
    } else {
        _clash_warning_label->hide();
    }
}

void ApplicationParamDlg::updateShortcutButtons()
{
    int infoCount = 0;
    const ShortcutActionInfo *infos = GetShortcutActionInfos(&infoCount);

    bool hasSelection = _shortcut_selected_row >= 0 && _shortcut_selected_row < infoCount;
    bool isEditing = false;
    bool isModified = false;
    bool isDefault = false;
    bool hasKey = false;

    if (hasSelection) {
        int actionId = infos[_shortcut_selected_row].actionId;
        QString currentKey = _shortcut_keys[actionId];
        QString originalKey = _shortcut_original_keys[actionId];
        QString defaultKey;

        for (int i = 0; i < infoCount; i++) {
            if (infos[i].actionId == actionId) {
                defaultKey = QString(infos[i].keySequence);
                break;
            }
        }

        QListWidgetItem *item = _shortcut_list->item(_shortcut_selected_row);
        QWidget *w = _shortcut_list->itemWidget(item);
        if (w) {
            ShortcutKeyCapture *capture = w->findChild<ShortcutKeyCapture*>();
            isEditing = (capture != nullptr && capture->hasFocus());
        }

        isModified = (currentKey != originalKey);
        isDefault = (currentKey != defaultKey);
        hasKey = !currentKey.isEmpty();
    }

    _btn_accept->setEnabled(hasSelection && isEditing);
    _btn_restore->setEnabled(hasSelection && isModified);
    _btn_reset_default->setEnabled(hasSelection && isDefault);
    _btn_delete->setEnabled(hasSelection && hasKey);
}

void ApplicationParamDlg::refreshShortcutList()
{
    if (!_shortcut_list)
        return;

    int infoCount = 0;
    const ShortcutActionInfo *infos = GetShortcutActionInfos(&infoCount);

    for (int i = 0; i < infoCount; i++) {
        QListWidgetItem *item = _shortcut_list->item(i);
        QWidget *w = _shortcut_list->itemWidget(item);
        if (!w) continue;

        QLabel *keyLabel = w->findChild<QLabel*>("shortcut_key_label");
        ShortcutKeyCapture *capture = w->findChild<ShortcutKeyCapture*>();

        if (capture && i != _shortcut_selected_row) {
            QHBoxLayout *lay = qobject_cast<QHBoxLayout*>(w->layout());
            if (lay) {
                int idx = lay->indexOf(capture);
                if (idx >= 0) {
                    lay->removeWidget(capture);
                    capture->deleteLater();

                    QLabel *newLabel = new QLabel(_shortcut_keys[infos[i].actionId].isEmpty() ? "" : _shortcut_keys[infos[i].actionId]);
                    newLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                    newLabel->setObjectName("shortcut_key_label");
                    lay->insertWidget(idx, newLabel, 1);
                }
            }
        } else if (keyLabel) {
            keyLabel->setText(_shortcut_keys[infos[i].actionId].isEmpty() ? "" : _shortcut_keys[infos[i].actionId]);
        } else if (capture && i == _shortcut_selected_row) {
            capture->setKeySequence(_shortcut_keys[infos[i].actionId]);
        }
    }

    checkShortcutClash();
}

void ApplicationParamDlg::onStyleTokenChanged(int row)
{
    QStringList tokenNames = _style_tokens.keys();
    std::sort(tokenNames.begin(), tokenNames.end());

    if (row < 0 || row >= tokenNames.size())
        return;

    QString name = tokenNames[row];
    QString currentValue = _style_tokens[name];

    QColor initialColor(currentValue);
    if (!initialColor.isValid()) {
        initialColor = QColor(128, 128, 128);
    }

    QColor newColor = QColorDialog::getColor(initialColor, nullptr,
        L_S(STR_PAGE_DLG, S_ID(IDS_DLG_STYLE_SELECT_COLOR), "Select Color"));
    if (newColor.isValid()) {
        QString colorStr;
        if (newColor.alpha() < 255) {
            colorStr = QString("rgba(%1,%2,%3,%4)")
                .arg(newColor.red())
                .arg(newColor.green())
                .arg(newColor.blue())
                .arg(newColor.alpha());
        } else {
            colorStr = newColor.name();
        }
        _style_tokens[name] = colorStr;
        refreshStyleTable();
    }
}

void ApplicationParamDlg::onResetStyle()
{
    _style_tokens = _default_style_tokens;
    refreshStyleTable();
}

void ApplicationParamDlg::onExportStyle()
{
    QString filePath = QFileDialog::getSaveFileName(nullptr,
        L_S(STR_PAGE_DLG, S_ID(IDS_DLG_STYLE_EXPORT), "Export"),
        QString(),
        L_S(STR_PAGE_DLG, S_ID(IDS_DLG_STYLE_FILE_FILTER), "Style Files (*.pxstyle)"));
    if (filePath.isEmpty())
        return;

    if (!filePath.endsWith(".pxstyle"))
        filePath += ".pxstyle";

    QJsonObject root;
    root["version"] = 1;
    root["style"] = AppConfig::Instance().frameOptions.style;

    QJsonObject tokensObj;
    QStringList tokenNames = _style_tokens.keys();
    std::sort(tokenNames.begin(), tokenNames.end());
    for (const QString &name : tokenNames) {
        QString value = _style_tokens[name];
        QString defaultValue = _default_style_tokens.value(name, "");
        if (value != defaultValue) {
            tokensObj[name] = value;
        }
    }
    root["tokens"] = tokensObj;

    QJsonDocument doc(root);
    QFile file(filePath);
    if (file.open(QFile::WriteOnly | QFile::Text)) {
        file.write(doc.toJson());
        file.close();
    }
}

void ApplicationParamDlg::onImportStyle()
{
    QString filePath = QFileDialog::getOpenFileName(nullptr,
        L_S(STR_PAGE_DLG, S_ID(IDS_DLG_STYLE_IMPORT), "Import"),
        QString(),
        L_S(STR_PAGE_DLG, S_ID(IDS_DLG_STYLE_FILE_FILTER), "Style Files (*.pxstyle)"));
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QFile::ReadOnly | QFile::Text))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject())
        return;

    QJsonObject root = doc.object();
    if (root["version"].toInt() != 1)
        return;

    QJsonObject tokensObj = root["tokens"].toObject();

    _style_tokens = _default_style_tokens;

    for (auto it = tokensObj.constBegin(); it != tokensObj.constEnd(); ++it) {
        if (_style_tokens.contains(it.key())) {
            _style_tokens[it.key()] = it.value().toString();
        }
    }

    refreshStyleTable();
}

QString ApplicationParamDlg::getShortcutKey(int actionId)
{
    if (_shortcut_keys.contains(actionId))
        return _shortcut_keys[actionId];
    return QString();
}

void ApplicationParamDlg::setShortcutKey(int actionId, const QString &keySeq)
{
    _shortcut_keys[actionId] = keySeq;
}

void ApplicationParamDlg::refreshStyleTable()
{
    if (!_style_table)
        return;

    QStringList tokenNames = _style_tokens.keys();
    std::sort(tokenNames.begin(), tokenNames.end());

    _style_table->setRowCount(tokenNames.size());

    for (int i = 0; i < tokenNames.size(); i++) {
        QString name = tokenNames[i];
        QString value = _style_tokens[name];

        QTableWidgetItem *nameItem = new QTableWidgetItem(name);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        _style_table->setItem(i, 0, nameItem);

        QWidget *previewWidget = new QWidget();
        QColor c(value);
        if (c.isValid()) {
            previewWidget->setStyleSheet(QString("background-color: %1; border: 1px solid #555;").arg(value));
        } else {
            previewWidget->setStyleSheet("background-color: transparent; border: 1px solid #555;");
        }
        previewWidget->setFixedSize(40, 20);
        QWidget *previewContainer = new QWidget();
        QHBoxLayout *previewLay = new QHBoxLayout(previewContainer);
        previewLay->setContentsMargins(2, 2, 2, 2);
        previewLay->setAlignment(Qt::AlignCenter);
        previewLay->addWidget(previewWidget);
        _style_table->setCellWidget(i, 1, previewContainer);

        QPushButton *colorBtn = new QPushButton(value);
        colorBtn->setFixedWidth(100);
        _style_table->setCellWidget(i, 2, colorBtn);

        QObject::connect(colorBtn, &QPushButton::clicked, colorBtn, [this, i](){ onStyleTokenChanged(i); });
    }

    _style_table->resizeColumnsToContents();
    _style_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
}

void ApplicationParamDlg::saveDisplayOptions()
{
    AppConfig &app = AppConfig::Instance();
    bool bAppChanged = false;
    bool bFontChanged = false;
    float fSize = _ftCbSize->currentText().toFloat();

    if (app.appOptions.quickScroll != _ck_quickScroll->isChecked()){
        app.appOptions.quickScroll = _ck_quickScroll->isChecked();
        bAppChanged = true;
    }
    if (app.appOptions.trigPosDisplayInMid != _ck_trigInMid->isChecked()){
        app.appOptions.trigPosDisplayInMid = _ck_trigInMid->isChecked();
        bAppChanged = true;
    }
    if (app.appOptions.displayProfileInBar != _ck_profileBar->isChecked()){
        app.appOptions.displayProfileInBar = _ck_profileBar->isChecked();
        bAppChanged = true;
    }
    if (app.appOptions.swapBackBufferAlways != _ck_abortData->isChecked()){
        app.appOptions.swapBackBufferAlways = _ck_abortData->isChecked();
        bAppChanged = true;
    }
    if (app.appOptions.fontSize != fSize){
        app.appOptions.fontSize = fSize;
        bFontChanged = true;
    }
    if (app.appOptions.autoScrollLatestData != _ck_autoScrollLatestData->isChecked()){
        app.appOptions.autoScrollLatestData = _ck_autoScrollLatestData->isChecked();
        bAppChanged = true;
    }

    if (bAppChanged){
        app.SaveApp();
        AppControl::Instance()->GetSession()->broadcast_msg(DSV_MSG_APP_OPTIONS_CHANGED);
    }

    if (bFontChanged){
        if (!bAppChanged){
            app.SaveApp();
        }
        AppControl::Instance()->GetSession()->broadcast_msg(DSV_MSG_FONT_OPTIONS_CHANGED);
    }
}

void ApplicationParamDlg::saveShortcutOptions()
{
    AppConfig &app = AppConfig::Instance();

    QList<ShortcutItem> newItems;
    QMap<int, QString>::const_iterator it;
    for (it = _shortcut_keys.constBegin(); it != _shortcut_keys.constEnd(); ++it) {
        ShortcutItem item;
        item.actionId = it.key();
        item.keySequence = it.value();
        newItems.append(item);
    }

    app.shortcutOptions.items = newItems;
    app.SaveShortcuts();
    app.flushPendingSaves();
    AppControl::Instance()->GetSession()->broadcast_msg(DSV_MSG_SHORTCUT_CHANGED);

    _shortcut_original_keys = _shortcut_keys;
    updateShortcutButtons();
}

void ApplicationParamDlg::saveStyleOptions()
{
    AppConfig &app = AppConfig::Instance();
    bool bChanged = false;

    QList<StyleTokenItem> newItems;
    QStringList tokenNames = _style_tokens.keys();
    std::sort(tokenNames.begin(), tokenNames.end());

    for (int i = 0; i < tokenNames.size(); i++) {
        QString name = tokenNames[i];
        QString value = _style_tokens[name];
        QString defaultValue = _default_style_tokens.value(name, "");

        if (value != defaultValue) {
            StyleTokenItem item;
            item.tokenName = name;
            item.value = value;
            newItems.append(item);
        }
    }

    if (app.styleOptions.items.size() != newItems.size()) {
        bChanged = true;
    } else {
        for (int i = 0; i < newItems.size(); i++) {
            bool found = false;
            for (int j = 0; j < app.styleOptions.items.size(); j++) {
                if (app.styleOptions.items[j].tokenName == newItems[i].tokenName) {
                    if (app.styleOptions.items[j].value != newItems[i].value) {
                        bChanged = true;
                    }
                    found = true;
                    break;
                }
            }
            if (!found) {
                bChanged = true;
            }
            if (bChanged)
                break;
        }
    }

    if (bChanged) {
        app.styleOptions.items = newItems;

        QString style = app.frameOptions.style;
        QString qssRes = ":/" + style + ".qss";
        QFile qss(qssRes);
        if (qss.open(QFile::ReadOnly | QFile::Text)) {
            QString qssContent = qss.readAll();
            qss.close();

            QHash<QString, QString> tokens;
            QRegularExpression tokenRe("@([\\w-]+):\\s*([^\\r\\n]+?)\\s*(?:\\*/|\\r|\\n)");
            QRegularExpressionMatchIterator it = tokenRe.globalMatch(qssContent);
            while (it.hasNext()) {
                QRegularExpressionMatch match = it.next();
                QString tokenName = "@" + match.captured(1);
                QString tokenValue = match.captured(2).trimmed();
                tokens[tokenName] = tokenValue;
            }

            QMap<QString, QString>::const_iterator styleIt;
            for (styleIt = _style_tokens.constBegin(); styleIt != _style_tokens.constEnd(); ++styleIt) {
                tokens[styleIt.key()] = styleIt.value();
            }

            QList<QString> keys = tokens.keys();
            std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) {
                return a.length() > b.length();
            });

            for (const QString &key : keys) {
                qssContent.replace(key, tokens[key]);
            }

            app.SetThemeTokens(tokens);
            qApp->setStyleSheet(qssContent);
        }

        app.SaveStyle();
        AppControl::Instance()->GetSession()->broadcast_msg(DSV_MSG_STYLE_CHANGED);
    }
}

bool ApplicationParamDlg::ShowDlg(QWidget *parent)
{
    DSDialog dlg(parent, true, true);
    dlg.setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SETTINGS), "Settings"));
    dlg.setMinimumSize(520, 420);

    QHBoxLayout *mainLay = new QHBoxLayout();
    mainLay->setContentsMargins(0, 10, 0, 0);
    mainLay->setSpacing(0);

    _nav_list = new QListWidget();
    _nav_list->setFixedWidth(120);
    _nav_list->setObjectName("settingsNavList");
    _nav_list->addItem(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_NAV_DISPLAY), "Display"));
    _nav_list->addItem(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_NAV_SHORTCUTS), "Shortcuts"));
    _nav_list->addItem(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_NAV_STYLE), "Style"));
    _nav_list->setCurrentRow(0);
    _nav_list->setFocusPolicy(Qt::NoFocus);

    _page_stack = new QStackedWidget();
    _page_stack->addWidget(createDisplayPage());
    _page_stack->addWidget(createShortcutPage());
    _page_stack->addWidget(createStylePage());

    QObject::connect(_nav_list, &QListWidget::currentRowChanged, _page_stack, &QStackedWidget::setCurrentIndex);

    mainLay->addWidget(_nav_list);
    mainLay->addWidget(_page_stack, 1);

    dlg.layout()->addLayout(mainLay);
    dlg.exec();
    bool ret = dlg.IsClickYes();

    if (ret){
        saveDisplayOptions();
        saveShortcutOptions();
        saveStyleOptions();
    }

    return ret;
}

} //
}//
