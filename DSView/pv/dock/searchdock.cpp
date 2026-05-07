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

#include "searchdock.h"
#include "../sigsession.h"
#include "../view/view.h"
#include "../view/logicsignal.h"
#include "../data/snapshot.h"
#include "../data/logicsnapshot.h"

#include <QObject>
#include <QPainter>
#include <stdint.h>
#include "../config/appconfig.h"
#include "../ui/langresource.h"
#include "../ui/msgbox.h"
#include "../appcontrol.h"
#include "../ui/fn.h"
#include "../tabcontext.h"

namespace pv {
namespace dock {

using namespace pv::view;

static const int kBatchSize = 200;

SearchDock::SearchDock(QWidget *parent, View *view, SigSession *session) :
    QWidget(parent),
    _session(session),
    _view(view),
    _pattern_input(nullptr),
    _result_table(nullptr),
    _legend_col1(nullptr),
    _legend_col2(nullptr),
    _legend_col3(nullptr),
    _logic_channel_count(0),
    _table_fill_index(0),
    _table_fill_timer(nullptr)
{
    _pattern_input = new widgets::SearchPatternInput(this);
    connect(_pattern_input, SIGNAL(pattern_changed()), this, SLOT(on_pattern_changed()));

    QHBoxLayout *input_layout = new QHBoxLayout();
    input_layout->addStretch(1);
    input_layout->addWidget(_pattern_input);
    input_layout->addStretch(1);

    _result_table = new QTableWidget(this);
    _result_table->setColumnCount(3);
    _result_table->setHorizontalHeaderLabels(
        QStringList() << "#"
                      << QString(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_COL_START), "Start"))
                      << QString(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_COL_LENGTH), "Length")));
    _result_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _result_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    _result_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    _result_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _result_table->setSelectionMode(QAbstractItemView::SingleSelection);
    _result_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _result_table->verticalHeader()->setVisible(false);
    _result_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    _result_table->setShowGrid(true);
    _result_table->setGridStyle(Qt::DotLine);
    _result_table->setFrameShape(QFrame::NoFrame);
    _result_table->setMinimumWidth(0);
    _result_table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    _result_table->setStyleSheet(
        "QTableWidget { border: none; gridline-color: #d0d0d0; }"
        "QTableWidget::item { padding: 2px; }"
        "QHeaderView::section { background: transparent; border: none; "
        "border-bottom: 1px solid #d0d0d0; font-weight: normal; padding: 2px; }");
    connect(_result_table, SIGNAL(cellClicked(int, int)),
            this, SLOT(on_result_clicked(int, int)));

    _legend_col1 = new QLabel(this);
    _legend_col1->setWordWrap(true);
    _legend_col2 = new QLabel(this);
    _legend_col2->setWordWrap(true);
    _legend_col3 = new QLabel(this);
    _legend_col3->setWordWrap(true);

    QVBoxLayout *legend_layout = new QVBoxLayout();
    legend_layout->setSpacing(4);
    legend_layout->addWidget(_legend_col1);
    legend_layout->addWidget(_legend_col2);
    legend_layout->addWidget(_legend_col3);

    QVBoxLayout *main_layout = new QVBoxLayout();
    main_layout->addLayout(input_layout);
    main_layout->addLayout(legend_layout);
    main_layout->addWidget(_result_table, 1);

    setLayout(main_layout);

    connect(_session->device_event_object(), SIGNAL(device_updated()),
            this, SLOT(on_device_updated()));

    _table_fill_timer = new QTimer(this);
    _table_fill_timer->setSingleShot(false);
    connect(_table_fill_timer, SIGNAL(timeout()), this, SLOT(fill_table_batch()));

    rebuild_pattern();

    ADD_UI(this);
}

SearchDock::~SearchDock()
{
    REMOVE_UI(this);
}

void SearchDock::set_view(view::View *view)
{
    _view = view;
}

void SearchDock::bind_context(TabContext *ctx)
{
    assert(ctx);
    _session = ctx->session();
    _view = ctx->view();
    _search_results.clear();
    _result_table->setRowCount(0);
    rebuild_pattern();
}

void SearchDock::unbind_context()
{
    _search_results.clear();
    _result_table->setRowCount(0);
}

void SearchDock::rebuild_pattern()
{
    int count = 0;
    for (auto s : _session->get_signals()) {
        if (s->signal_type() == SR_CHANNEL_LOGIC)
            count++;
    }

    _logic_channel_count = count;

    _pattern_input->set_channel_count(count);
    _pattern_input->set_pattern(_pattern);

    std::set<uint16_t> active_indices;
    for (auto s : _session->get_signals()) {
        if (s->signal_type() == SR_CHANNEL_LOGIC)
            active_indices.insert(s->get_index());
    }
    for (auto it = _pattern.begin(); it != _pattern.end(); ) {
        if (active_indices.find(it->first) == active_indices.end())
            it = _pattern.erase(it);
        else
            ++it;
    }
}

void SearchDock::on_pattern_changed()
{
    _pattern = _pattern_input->get_pattern();
    _view->set_search_pos(_view->get_search_pos(), false);
    do_search();
}

void SearchDock::on_device_updated()
{
    rebuild_pattern();
}

int64_t SearchDock::find_match_end(data::LogicSnapshot *snapshot, int64_t start_pos)
{
    const int64_t end = snapshot->get_sample_count() - 1;
    bool has_edge = false;
    for (auto &it : _pattern) {
        QChar ch = it.second.at(0).toUpper();
        if (ch == 'R' || ch == 'F' || ch == 'C') {
            has_edge = true;
            break;
        }
    }

    if (has_edge)
        return start_pos;

    int64_t pos = start_pos + 1;
    while (pos <= end) {
        bool match = true;
        for (auto &it : _pattern) {
            QChar ch = it.second.at(0).toUpper();
            int sig_index = it.first;
            if (ch == '0') {
                if (snapshot->get_sample(pos, sig_index)) {
                    match = false;
                    break;
                }
            } else if (ch == '1') {
                if (!snapshot->get_sample(pos, sig_index)) {
                    match = false;
                    break;
                }
            }
        }
        if (!match)
            break;
        pos++;
    }
    return pos - 1;
}

void SearchDock::do_search()
{
    const auto snapshot = _session->get_snapshot(SR_CHANNEL_LOGIC);
    if (!snapshot) return;
    const auto logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot);
    if (!logic_snapshot || logic_snapshot->empty()) {
        _search_results.clear();
        _result_table->setRowCount(0);
        return;
    }

    _pattern = _pattern_input->get_pattern();
    _search_results.clear();

    const int64_t end = logic_snapshot->get_sample_count() - 1;
    int64_t pos = 0;
    const int max_results = 100000;

    while (pos <= end && (int)_search_results.size() < max_results) {
        bool ret = logic_snapshot->pattern_search(0, end, pos, _pattern, true);
        if (!ret)
            break;
        int64_t match_end = find_match_end(logic_snapshot, pos);
        _search_results.push_back(SearchData(pos, match_end));
        pos = match_end + 1;
    }

    _result_table->setRowCount(_search_results.size());
    _table_fill_index = 0;
    _table_fill_timer->start(0);
}

void SearchDock::fill_table_batch()
{
    int end = qMin(_table_fill_index + kBatchSize, (int)_search_results.size());
    for (int i = _table_fill_index; i < end; i++) {
        _result_table->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        _result_table->setItem(i, 1, new QTableWidgetItem(QString::number(_search_results[i].start)));
        _result_table->setItem(i, 2, new QTableWidgetItem(QString::number(_search_results[i].end - _search_results[i].start + 1)));
    }
    _table_fill_index = end;
    if (_table_fill_index >= (int)_search_results.size()) {
        _table_fill_timer->stop();
    }
}

void SearchDock::on_result_clicked(int row, int col)
{
    (void)col;
    if (row >= 0 && row < (int)_search_results.size()) {
        _view->set_search_pos(_search_results[row].start, true);
    }
}

void SearchDock::retranslateUi()
{
    _legend_col1->setText(
        QString(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_LABEL_X), "X: Don't care")) + "\n" +
        QString(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_LABEL_R), "R: Rising edge")));
    _legend_col2->setText(
        QString(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_LABEL_0), "0: Low level")) + "\n" +
        QString(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_LABEL_F), "F: Falling edge")));
    _legend_col3->setText(
        QString(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_LABEL_1), "1: High level")) + "\n" +
        QString(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_LABEL_C), "C: Rising/Falling edge")));

    if (_result_table) {
        QStringList headers;
        headers << "#"
                << QString(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_COL_START), "Start"))
                << QString(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_COL_LENGTH), "Length"));
        _result_table->setHorizontalHeaderLabels(headers);
    }
}

void SearchDock::reStyle()
{
}

void SearchDock::paintEvent(QPaintEvent *)
{
}

void SearchDock::UpdateLanguage()
{
    retranslateUi();
}

void SearchDock::UpdateTheme()
{
    reStyle();
}

void SearchDock::UpdateFont()
{
    QFont font("Source Code Pro");
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
    _pattern_input->setFont(font);
    _pattern_input->update();
}

} // namespace dock
} // namespace pv
