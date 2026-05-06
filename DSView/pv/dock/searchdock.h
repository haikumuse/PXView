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
#include <QLabel>
#include <QFrame>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>

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

namespace data {
    class LogicSnapshot;
}

namespace dock {

struct SearchData {
    int64_t start;
    int64_t end;
    SearchData(int64_t s, int64_t e) : start(s), end(e) {}
};

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
    void fill_table_batch();
    int64_t find_match_end(pv::data::LogicSnapshot *snapshot, int64_t start_pos);

    void UpdateLanguage() override;
    void UpdateTheme() override;
    void UpdateFont() override;

public slots:
    void on_pattern_changed();
    void on_device_updated();
    void on_result_clicked(int row, int col);

private:
    SigSession *_session;
    view::View *_view;
    std::map<uint16_t, QString> _pattern;

    widgets::SearchPatternInput *_pattern_input;
    QTableWidget *_result_table;
    QLabel *_legend_col1;
    QLabel *_legend_col2;
    QLabel *_legend_col3;
    int _logic_channel_count;
    std::vector<SearchData> _search_results;
    int _table_fill_index;
    QTimer *_table_fill_timer;
};

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_SEARCHDOCK_H
