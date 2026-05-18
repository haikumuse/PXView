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

#pragma once

#include <QObject>
#include <QWidget>
#include <QStringList>
#include <QListWidget>
#include <QStackedWidget>
#include <QMap>
#include <QSet>
#include <QLineEdit>

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QCheckBox;

class ShortcutKeyCapture : public QLineEdit
{
    Q_OBJECT

public:
    explicit ShortcutKeyCapture(QWidget *parent = nullptr);

    void setKeySequence(const QString &key);
    QString keySequence() const;

signals:
    void keySequenceChanged(const QString &newKey);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    QString m_keySeq;
    bool m_capturing;
};

namespace pv
{
namespace dialogs
{
    class ApplicationParamDlg
    { 
    public:
        ApplicationParamDlg();
        ~ApplicationParamDlg();

        bool ShowDlg(QWidget *parent);

    private:
        void bind_font_name_list(QComboBox *box, QString v);
        void bind_font_size_list(QComboBox *box, float size);

        QWidget* createDisplayPage();
        QWidget* createShortcutPage();
        QWidget* createStylePage();

        void saveDisplayOptions();
        void saveShortcutOptions();
        void saveStyleOptions();

        void onShortcutRowSelected(int row);
        void onShortcutKeyCaptured(int row, const QString &newKey);
        void onShortcutAccept();
        void onShortcutRestore();
        void onShortcutResetDefault();
        void onShortcutDelete();
        void onResetShortcuts();
        void checkShortcutClash();
        void updateShortcutButtons();
        void refreshShortcutList();
        void onStyleTokenChanged(int row);
        void onResetStyle();
        void onExportStyle();
        void onImportStyle();

        QString getShortcutKey(int actionId);
        void setShortcutKey(int actionId, const QString &keySeq);
        void refreshStyleTable();

    private:
        QStringList _font_name_list;

        QListWidget *_nav_list;
        QStackedWidget *_page_stack;

        QCheckBox *_ck_quickScroll;
        QCheckBox *_ck_trigInMid;
        QCheckBox *_ck_profileBar;
        QCheckBox *_ck_abortData;
        QCheckBox *_ck_autoScrollLatestData;
        QComboBox *_ftCbSize;

        QListWidget *_shortcut_list;
        int _shortcut_selected_row;
        QPushButton *_btn_accept;
        QPushButton *_btn_restore;
        QPushButton *_btn_reset_default;
        QPushButton *_btn_delete;
        QLabel *_clash_warning_label;

        QTableWidget *_style_table;
        QMap<QString, QString> _style_tokens;
        QMap<QString, QString> _default_style_tokens;

        QMap<int, QString> _shortcut_keys;
        QMap<int, QString> _shortcut_original_keys;
        QSet<int> _shortcut_clash_ids;
    };

}//
}//
