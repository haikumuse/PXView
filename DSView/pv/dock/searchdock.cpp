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

namespace pv {
namespace dock {

using namespace pv::view;

SearchDock::SearchDock(QWidget *parent, View *view, SigSession *session) :
    QWidget(parent),
    _session(session),
    _view(view),
    _pattern_input(nullptr),
    _bit_range_layout(nullptr),
    _search_button(nullptr),
    _result_table(nullptr),
    _legend_col1(nullptr),
    _legend_col2(nullptr),
    _legend_col3(nullptr),
    _logic_channel_count(0)
{
    _bit_range_layout = new QHBoxLayout();
    _bit_range_layout->setSpacing(0);
    _bit_range_layout->setContentsMargins(0, 0, 0, 0);

    _pattern_input = new widgets::SearchPatternInput(this);
    connect(_pattern_input, SIGNAL(pattern_changed()), this, SLOT(on_pattern_changed()));

    _search_button = new QPushButton(this);
    connect(_search_button, SIGNAL(clicked()), this, SLOT(on_search_clicked()));

    QHBoxLayout *input_row = new QHBoxLayout();
    input_row->addWidget(_pattern_input);
    input_row->addWidget(_search_button);
    input_row->addStretch(1);

    _result_table = new QTableWidget(this);
    _result_table->setColumnCount(3);
    _result_table->setHorizontalHeaderLabels(
        QStringList() << "#" << QString(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_COL_START), "Start"))
                      << QString(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_COL_LENGTH), "Length")));
    _result_table->horizontalHeader()->setStretchLastSection(true);
    _result_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    _result_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _result_table->setSelectionMode(QAbstractItemView::SingleSelection);
    _result_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _result_table->verticalHeader()->setVisible(false);
    _result_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    connect(_result_table, SIGNAL(cellClicked(int, int)),
            this, SLOT(on_result_clicked(int, int)));

    QFrame *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);

    _legend_col1 = new QLabel(this);
    _legend_col2 = new QLabel(this);
    _legend_col3 = new QLabel(this);

    QHBoxLayout *legend_layout = new QHBoxLayout();
    legend_layout->setSpacing(16);
    legend_layout->addWidget(_legend_col1);
    legend_layout->addWidget(_legend_col2);
    legend_layout->addWidget(_legend_col3);
    legend_layout->addStretch(1);

    QVBoxLayout *main_layout = new QVBoxLayout();
    main_layout->addLayout(_bit_range_layout);
    main_layout->addLayout(input_row);
    main_layout->addWidget(separator);
    main_layout->addLayout(legend_layout);
    main_layout->addWidget(_result_table, 1);

    setLayout(main_layout);

    connect(_session->device_event_object(), SIGNAL(device_updated()),
            this, SLOT(on_device_updated()));

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

void SearchDock::rebuild_pattern()
{
    int count = 0;
    for (auto s : _session->get_signals()) {
        if (s->signal_type() == SR_CHANNEL_LOGIC)
            count++;
    }

    _logic_channel_count = count;

    while (_bit_range_layout->count() > 0) {
        QLayoutItem *item = _bit_range_layout->takeAt(0);
        delete item->widget();
        delete item;
    }

    int num_groups = (count + 7) / 8;
    for (int g = num_groups - 1; g >= 0; g--) {
        int high = g * 8 + 7;
        if (high >= count)
            high = count - 1;
        int low = g * 8;

        if (high == low) {
            QLabel *label = new QLabel(QString::number(high), this);
            _bit_range_layout->addWidget(label);
        } else {
            QLabel *label = new QLabel(
                QString::number(high) + "---" + QString::number(low), this);
            _bit_range_layout->addWidget(label);
        }

        if (g > 0) {
            QSpacerItem *spacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
            _bit_range_layout->addItem(spacer);
        }
    }

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
}

void SearchDock::on_device_updated()
{
    rebuild_pattern();
}

void SearchDock::on_search_clicked()
{
    do_search();
}

void SearchDock::do_search()
{
    const auto snapshot = _session->get_snapshot(SR_CHANNEL_LOGIC);
    if (!snapshot) return;
    const auto logic_snapshot = dynamic_cast<data::LogicSnapshot*>(snapshot);
    if (!logic_snapshot || logic_snapshot->empty()) {
        QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_NO_SAMPLE_DATA), "No Sample data!"));
        MsgBox::Show(strMsg);
        return;
    }

    _pattern = _pattern_input->get_pattern();
    _search_results.clear();

    const int64_t end = logic_snapshot->get_sample_count() - 1;
    int64_t pos = 0;
    const int max_results = 10000;

    while (pos <= end && (int)_search_results.size() < max_results) {
        bool ret = logic_snapshot->pattern_search(0, end, pos, _pattern, true);
        if (!ret)
            break;
        _search_results.push_back(pos);
        pos++;
    }

    _result_table->setRowCount(_search_results.size());
    for (int i = 0; i < (int)_search_results.size(); i++) {
        _result_table->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        _result_table->setItem(i, 1, new QTableWidgetItem(QString::number(_search_results[i])));
        _result_table->setItem(i, 2, new QTableWidgetItem("1"));
    }
}

void SearchDock::on_result_clicked(int row, int col)
{
    (void)col;
    if (row >= 0 && row < (int)_search_results.size()) {
        _view->set_search_pos(_search_results[row], true);
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
    QString iconPath = GetIconPath();
    _search_button->setIcon(QIcon(iconPath+"/search.svg"));
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
