/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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


#ifndef DSVIEW_PV_SEARCHDOCK_H
#define DSVIEW_PV_SEARCHDOCK_H

#include <QDockWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QFrame>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <vector>
#include <set>

#include "../widgets/searchpatterninput.h"
#include "../ui/dscombobox.h"
#include "../interface/icallbacks.h"
#include "../ui/uimanager.h"

namespace pv {

class SigSession;

namespace view {
    class View;
}

namespace dock {

class SearchDock : public QWidget, public IUiWindow
{
    Q_OBJECT

public:
    SearchDock(QWidget *parent, pv::view::View *view, SigSession *session);
    ~SearchDock();

    void set_view(view::View *view);

    void paintEvent(QPaintEvent *);

private:     
    void retranslateUi();
    void reStyle();
    void rebuild_pattern();
    void do_search();

    void UpdateLanguage() override;
    void UpdateTheme() override;
    void UpdateFont() override;

public slots:
    void on_pattern_changed();
    void on_device_updated();
    void on_search_clicked();
    void on_result_clicked(int row, int col);

private:
    SigSession *_session;
    view::View *_view;
    std::map<uint16_t, QString> _pattern;

    widgets::SearchPatternInput *_pattern_input;
    QHBoxLayout *_bit_range_layout;
    QPushButton *_search_button;
    QTableWidget *_result_table;
    QLabel *_legend_col1;
    QLabel *_legend_col2;
    QLabel *_legend_col3;
    int _logic_channel_count;
    std::vector<int64_t> _search_results;
};

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_SEARCHDOCK_H
