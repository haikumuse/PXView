/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
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

#include "signalprocessingdock.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLayoutItem>
#include <QPushButton>

#include "../appcontrol.h"
#include "../config/appconfig.h"
#include "../data/sessiondocument.h"
#include "../deviceagent.h"
#include "../dsvdef.h"
#include "../interface/icallbacks.h"
#include "../sigsession.h"
#include "../tabcontext.h"
#include "../ui/dockfonts.h"
#include "../ui/dsspinbox.h"
#include "../ui/langresource.h"

using namespace std;

namespace pv {
namespace dock {

SignalProcessingDock::SignalProcessingDock(QWidget *parent, SigSession *session)
    : QWidget(parent), _session(session), _context(nullptr) {
  _device_agent = session->get_device();
  _container_panel = nullptr;
  _container_lay = nullptr;
  _invert_group = nullptr;
  _glitch_filter_group = nullptr;
  _no_logic_hint = nullptr;
  _apply_invert_btn = nullptr;
  _restore_invert_btn = nullptr;
  _invert_status_label = nullptr;
  _apply_filter_btn = nullptr;
  _restore_data_btn = nullptr;
  _filter_status_label = nullptr;

  this->setObjectName("dock_signal_processing_scroll");

  _container_panel = new QWidget();
  _container_lay = new QVBoxLayout();
  _container_lay->setDirection(QBoxLayout::TopToBottom);
  _container_lay->setAlignment(Qt::AlignTop);
  _container_lay->setContentsMargins(12, 10, 12, 10);
  _container_lay->setSpacing(10);
  _container_panel->setLayout(_container_lay);

  QVBoxLayout *mainLay = new QVBoxLayout(this);
  mainLay->setContentsMargins(0, 0, 0, 0);
  mainLay->setSpacing(0);
  mainLay->addWidget(_container_panel);
  mainLay->addStretch();

  if (_device_agent->have_instance()) {
    build_ui();
  }
}

SignalProcessingDock::~SignalProcessingDock() {}

void SignalProcessingDock::build_ui() {
  // Clear existing widgets
  QLayoutItem *item;
  while ((item = _container_lay->takeAt(0)) != nullptr) {
    if (item->widget()) {
      delete item->widget();
    }
    delete item;
  }

  _invert_group = nullptr;
  _glitch_filter_group = nullptr;
  _no_logic_hint = nullptr;
  _invert_checkBox_list.clear();
  _glitch_checkBox_list.clear();
  _glitch_spinbox_list.clear();
  _apply_invert_btn = nullptr;
  _restore_invert_btn = nullptr;
  _invert_status_label = nullptr;
  _apply_filter_btn = nullptr;
  _restore_data_btn = nullptr;
  _filter_status_label = nullptr;

  if (!_device_agent->have_instance())
    return;

  int mode = _device_agent->get_work_mode();
  if (mode != LOGIC) {
    QFont contentFont = dock_font_content();
    _no_logic_hint = new QWidget(_container_panel);
    QVBoxLayout *hint_lay = new QVBoxLayout(_no_logic_hint);
    hint_lay->setContentsMargins(0, 0, 0, 0);
    hint_lay->setSpacing(5);
    hint_lay->setAlignment(Qt::AlignTop);

    QLabel *hint_label =
        new QLabel("此功能仅在 Logic 模式下可用", _no_logic_hint);
    hint_label->setFont(contentFont);
    hint_lay->addWidget(hint_label);

    _container_lay->addWidget(_no_logic_hint);
    _container_lay->addStretch();
    return;
  }

  // Signal Invert section
  build_invert_panel();
  if (_invert_group) {
    _container_lay->addWidget(_invert_group);
  }

  // Separator
  QFrame *sep = new QFrame(_container_panel);
  sep->setObjectName("dock_section_separator");
  sep->setFrameShape(QFrame::HLine);
  _container_lay->addWidget(sep);

  // Glitch Filter section
  build_glitch_filter_panel();
  if (_glitch_filter_group) {
    _container_lay->addWidget(_glitch_filter_group);
  }

  _container_lay->addStretch();

  update_invert_state();
  update_glitch_filter_state();
}

void SignalProcessingDock::build_invert_panel() {
  if (_invert_group) {
    delete _invert_group;
    _invert_group = nullptr;
  }

  _invert_checkBox_list.clear();

  QFont sectionTitleFont = dock_font_section_title();
  QFont labelFont = dock_font_label();
  QFont contentFont = dock_font_content();

  _invert_group = new QWidget(_container_panel);
  _invert_group->setMinimumWidth(0);

  QLabel *invert_title = new QLabel("信号取反", _invert_group);
  invert_title->setObjectName("dock_section_title");
  invert_title->setFont(sectionTitleFont);

  QVBoxLayout *layout = new QVBoxLayout(_invert_group);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(5);
  layout->setAlignment(Qt::AlignTop);
  layout->addWidget(invert_title);

  QVBoxLayout *inner_layout = new QVBoxLayout();
  inner_layout->setContentsMargins(5, 2, 5, 5);
  inner_layout->setSpacing(8);
  inner_layout->setAlignment(Qt::AlignTop);
  layout->addLayout(inner_layout);

  // Channel checkboxes grid
  QWidget *ch_container = new QWidget(_invert_group);
  QGridLayout *ch_grid = new QGridLayout(ch_container);
  ch_grid->setContentsMargins(0, 2, 0, 2);
  ch_grid->setSpacing(0);
  ch_grid->setAlignment(Qt::AlignTop);

  int ch_idx = 0;
  int ch_row = 0;
  int ch_col = 0;
  int ch_columns = 4;

  for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
    sr_channel *const probe = (sr_channel *)l->data;
    if (probe->type != SR_CHANNEL_LOGIC)
      continue;

    QCheckBox *ch_check =
        new QCheckBox(QString("Ch%1").arg(probe->index), ch_container);
    ch_check->setObjectName("dock_content");
    ch_check->setFont(contentFont);
    ch_check->setEnabled(probe->enabled);
    _invert_checkBox_list.push_back(ch_check);

    ch_grid->addWidget(ch_check, ch_row, ch_col);

    ch_col++;
    if (ch_col == ch_columns) {
      ch_col = 0;
      ch_row++;
    }
    ch_idx++;
  }

  for (int c = 0; c < ch_columns; c++) {
    ch_grid->setColumnStretch(c, 1);
  }

  inner_layout->addWidget(ch_container);

  // Select All / Deselect All buttons
  QHBoxLayout *btn_layout = new QHBoxLayout();
  btn_layout->setSpacing(5);

  QPushButton *select_all_btn = new QPushButton("全选", _invert_group);
  QPushButton *deselect_all_btn = new QPushButton("取消全选", _invert_group);
  select_all_btn->setObjectName("dock_content");
  deselect_all_btn->setObjectName("dock_content");
  select_all_btn->setFont(contentFont);
  deselect_all_btn->setFont(contentFont);
  select_all_btn->setMaximumHeight(28);
  deselect_all_btn->setMaximumHeight(28);

  connect(select_all_btn, &QPushButton::clicked, this,
          &SignalProcessingDock::on_invert_select_all);
  connect(deselect_all_btn, &QPushButton::clicked, this,
          &SignalProcessingDock::on_invert_deselect_all);

  btn_layout->addWidget(select_all_btn);
  btn_layout->addWidget(deselect_all_btn);
  btn_layout->addStretch();
  inner_layout->addLayout(btn_layout);

  // Hint text
  QLabel *hint_label = new QLabel(
      "*勾选通道并点击应用后，信号电平将被翻转（0→1, 1→0）", _invert_group);
  hint_label->setFont(contentFont);
  inner_layout->addWidget(hint_label);

  // Apply / Restore buttons
  QHBoxLayout *action_layout = new QHBoxLayout();
  action_layout->setSpacing(5);

  _apply_invert_btn = new QPushButton("应用取反", _invert_group);
  _restore_invert_btn = new QPushButton("恢复原始数据", _invert_group);
  _apply_invert_btn->setObjectName("dock_content");
  _restore_invert_btn->setObjectName("dock_content");
  _apply_invert_btn->setFont(contentFont);
  _restore_invert_btn->setFont(contentFont);
  _apply_invert_btn->setMaximumHeight(28);
  _restore_invert_btn->setMaximumHeight(28);
  _apply_invert_btn->setMinimumWidth(75);
  _restore_invert_btn->setMinimumWidth(90);
  _restore_invert_btn->setEnabled(false);

  connect(_apply_invert_btn, &QPushButton::clicked, this,
          &SignalProcessingDock::on_apply_invert);
  connect(_restore_invert_btn, &QPushButton::clicked, this,
          &SignalProcessingDock::on_restore_original_data);

  action_layout->addWidget(_apply_invert_btn);
  action_layout->addWidget(_restore_invert_btn);
  action_layout->addStretch();
  inner_layout->addLayout(action_layout);

  // Status label
  _invert_status_label = new QLabel("", _invert_group);
  _invert_status_label->setFont(contentFont);
  inner_layout->addWidget(_invert_status_label);

  inner_layout->addStretch();
}

void SignalProcessingDock::build_glitch_filter_panel() {
  if (_glitch_filter_group) {
    delete _glitch_filter_group;
    _glitch_filter_group = nullptr;
  }

  _glitch_checkBox_list.clear();
  _glitch_spinbox_list.clear();

  if (_device_agent->get_work_mode() != LOGIC)
    return;

  QFont sectionTitleFont = dock_font_section_title();
  QFont labelFont = dock_font_label();
  QFont contentFont = dock_font_content();

  _glitch_filter_group = new QWidget(_container_panel);
  _glitch_filter_group->setMinimumWidth(230);

  QLabel *glitch_title = new QLabel("毛刺过滤", _glitch_filter_group);
  glitch_title->setObjectName("dock_section_title");
  glitch_title->setFont(sectionTitleFont);

  QVBoxLayout *layout = new QVBoxLayout(_glitch_filter_group);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(5);
  layout->setAlignment(Qt::AlignTop);
  layout->addWidget(glitch_title);

  QVBoxLayout *inner_layout = new QVBoxLayout();
  inner_layout->setContentsMargins(5, 2, 5, 5);
  inner_layout->setSpacing(8);
  inner_layout->setAlignment(Qt::AlignTop);
  layout->addLayout(inner_layout);

  QWidget *ch_container = new QWidget(_glitch_filter_group);
  QGridLayout *ch_grid = new QGridLayout(ch_container);
  ch_grid->setContentsMargins(2, 2, 2, 2);
  ch_grid->setSpacing(3);
  ch_grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);

  int ch_idx = 0;
  for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
    sr_channel *const probe = (sr_channel *)l->data;
    if (probe->type != SR_CHANNEL_LOGIC)
      continue;

    QCheckBox *ch_check =
        new QCheckBox(QString("Ch%1").arg(probe->index), ch_container);
    ch_check->setObjectName("dock_content");
    ch_check->setFont(contentFont);
    ch_check->setEnabled(probe->enabled);
    ch_check->setFixedWidth(55);
    _glitch_checkBox_list.push_back(ch_check);

    QLabel *le_label = new QLabel("≤", ch_container);
    le_label->setObjectName("dock_label");
    le_label->setFont(labelFont);

    pv::ui::DsSpinBox *spin = new pv::ui::DsSpinBox(ch_container);
    spin->setRange(1, 99999);
    spin->setValue(1);
    spin->setObjectName("dock_content");
    spin->setFont(contentFont);
    spin->setEnabled(false);
    spin->setMinimumWidth(65);
    spin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _glitch_spinbox_list.push_back(spin);

    QLabel *unit_label = new QLabel("采样周期", ch_container);
    unit_label->setObjectName("dock_label");
    unit_label->setFont(labelFont);

    ch_grid->addWidget(ch_check, ch_idx, 0);
    ch_grid->addWidget(le_label, ch_idx, 1);
    ch_grid->addWidget(spin, ch_idx, 2);
    ch_grid->addWidget(unit_label, ch_idx, 3);

    connect(ch_check, &QCheckBox::toggled,
            [spin](bool checked) { spin->setEnabled(checked); });

    ch_idx++;
  }

  for (int c = 0; c < 4; c++) {
    ch_grid->setColumnStretch(c, 0);
  }

  inner_layout->addWidget(ch_container);

  // Select All / Deselect All buttons
  QHBoxLayout *btn_layout = new QHBoxLayout();
  btn_layout->setSpacing(5);

  QPushButton *select_all_btn = new QPushButton("全选", _glitch_filter_group);
  QPushButton *deselect_all_btn =
      new QPushButton("取消全选", _glitch_filter_group);
  select_all_btn->setObjectName("dock_content");
  deselect_all_btn->setObjectName("dock_content");
  select_all_btn->setFont(contentFont);
  deselect_all_btn->setFont(contentFont);
  select_all_btn->setMaximumHeight(28);
  deselect_all_btn->setMaximumHeight(28);

  connect(select_all_btn, &QPushButton::clicked, this,
          &SignalProcessingDock::on_glitch_select_all);
  connect(deselect_all_btn, &QPushButton::clicked, this,
          &SignalProcessingDock::on_glitch_deselect_all);

  btn_layout->addWidget(select_all_btn);
  btn_layout->addWidget(deselect_all_btn);
  btn_layout->addStretch();
  inner_layout->addLayout(btn_layout);

  // Hint text
  QLabel *hint_label = new QLabel("*勾选通道并点击应用滤波后，小于设定宽度的脉冲将被滤除",
                                  _glitch_filter_group);
  hint_label->setFont(contentFont);
  inner_layout->addWidget(hint_label);

  // Apply / Restore buttons
  QHBoxLayout *action_layout = new QHBoxLayout();
  action_layout->setSpacing(5);

  _apply_filter_btn = new QPushButton("应用滤波", _glitch_filter_group);
  _restore_data_btn = new QPushButton("恢复原始数据", _glitch_filter_group);
  _apply_filter_btn->setObjectName("dock_content");
  _restore_data_btn->setObjectName("dock_content");
  _apply_filter_btn->setFont(contentFont);
  _restore_data_btn->setFont(contentFont);
  _apply_filter_btn->setMaximumHeight(28);
  _restore_data_btn->setMaximumHeight(28);
  _apply_filter_btn->setMinimumWidth(75);
  _restore_data_btn->setMinimumWidth(90);
  _restore_data_btn->setEnabled(false);

  connect(_apply_filter_btn, &QPushButton::clicked, this,
          &SignalProcessingDock::on_apply_glitch_filter);
  connect(_restore_data_btn, &QPushButton::clicked, this,
          &SignalProcessingDock::on_restore_glitch_data);

  action_layout->addWidget(_apply_filter_btn);
  action_layout->addWidget(_restore_data_btn);
  action_layout->addStretch();
  inner_layout->addLayout(action_layout);

  // Status label
  _filter_status_label = new QLabel("", _glitch_filter_group);
  _filter_status_label->setFont(contentFont);
  inner_layout->addWidget(_filter_status_label);

  inner_layout->addStretch();
}

void SignalProcessingDock::on_apply_invert() {
  std::vector<bool> channels;
  int ch_idx = 0;
  for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
    sr_channel *const probe = (sr_channel *)l->data;
    if (probe->type != SR_CHANNEL_LOGIC)
      continue;

    bool inverted = false;
    if (ch_idx < (int)_invert_checkBox_list.size() &&
        _invert_checkBox_list[ch_idx]->isChecked()) {
      inverted = true;
    }
    channels.push_back(inverted);
    ch_idx++;
  }

  _session->set_signal_invert(channels);
}

void SignalProcessingDock::on_restore_original_data() {
  _session->clear_signal_invert();
}

void SignalProcessingDock::on_invert_select_all() {
  for (size_t i = 0; i < _invert_checkBox_list.size(); i++) {
    int ch_idx = 0;
    for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
      sr_channel *const probe = (sr_channel *)l->data;
      if (probe->type != SR_CHANNEL_LOGIC)
        continue;
      if (ch_idx == (int)i && probe->enabled) {
        _invert_checkBox_list[i]->setChecked(true);
        break;
      }
      ch_idx++;
    }
  }
}

void SignalProcessingDock::on_invert_deselect_all() {
  for (auto cb : _invert_checkBox_list) {
    cb->setChecked(false);
  }
}

void SignalProcessingDock::on_apply_glitch_filter() {
  std::vector<uint32_t> thresholds;
  int ch_idx = 0;
  for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
    sr_channel *const probe = (sr_channel *)l->data;
    if (probe->type != SR_CHANNEL_LOGIC)
      continue;

    uint32_t threshold = 0;
    if (ch_idx < (int)_glitch_checkBox_list.size() &&
        _glitch_checkBox_list[ch_idx]->isChecked()) {
      threshold = _glitch_spinbox_list[ch_idx]->value();
    }
    thresholds.push_back(threshold);
    ch_idx++;
  }

  _session->set_glitch_filter(thresholds);
}

void SignalProcessingDock::on_restore_glitch_data() {
  _session->clear_glitch_filter();
}

void SignalProcessingDock::on_glitch_select_all() {
  for (size_t i = 0; i < _glitch_checkBox_list.size(); i++) {
    int ch_idx = 0;
    for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
      sr_channel *const probe = (sr_channel *)l->data;
      if (probe->type != SR_CHANNEL_LOGIC)
        continue;
      if (ch_idx == (int)i && probe->enabled) {
        _glitch_checkBox_list[i]->setChecked(true);
        break;
      }
      ch_idx++;
    }
  }
}

void SignalProcessingDock::on_glitch_deselect_all() {
  for (auto cb : _glitch_checkBox_list) {
    cb->setChecked(false);
  }
}

void SignalProcessingDock::update_invert_state() {
  bool is_active = _session->is_signal_invert_active();

  if (_invert_status_label) {
    _invert_status_label->setText(is_active ? "已取反" : "");
  }

  if (_restore_invert_btn) {
    _restore_invert_btn->setEnabled(is_active);
  }
}

void SignalProcessingDock::update_glitch_filter_state() {
  bool is_active = _session->is_glitch_filter_active();

  if (_filter_status_label) {
    _filter_status_label->setText(is_active ? "已滤波" : "");
  }

  if (_restore_data_btn) {
    _restore_data_btn->setEnabled(is_active);
  }
}

void SignalProcessingDock::rebuild_panels() { build_ui(); }

void SignalProcessingDock::update_view() { build_ui(); }

void SignalProcessingDock::auto_apply_settings() {
  // Called when new capture data arrives - re-apply current checkbox settings
  // Only apply if there are checked channels (same logic as on_apply_invert/on_apply_glitch_filter)
  if (!_session || !_device_agent || !_device_agent->have_instance())
    return;

  bool has_invert = false;
  std::vector<bool> invert_channels;
  int ch_idx = 0;
  for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
    sr_channel *const probe = (sr_channel *)l->data;
    if (probe->type != SR_CHANNEL_LOGIC)
      continue;
    bool inverted = false;
    if (ch_idx < (int)_invert_checkBox_list.size() &&
        _invert_checkBox_list[ch_idx]->isChecked()) {
      inverted = true;
      has_invert = true;
    }
    invert_channels.push_back(inverted);
    ch_idx++;
  }

  bool has_filter = false;
  std::vector<uint32_t> thresholds;
  ch_idx = 0;
  for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
    sr_channel *const probe = (sr_channel *)l->data;
    if (probe->type != SR_CHANNEL_LOGIC)
      continue;
    uint32_t threshold = 0;
    if (ch_idx < (int)_glitch_checkBox_list.size() &&
        _glitch_checkBox_list[ch_idx]->isChecked()) {
      threshold = _glitch_spinbox_list[ch_idx]->value();
      if (threshold > 0)
        has_filter = true;
    }
    thresholds.push_back(threshold);
    ch_idx++;
  }

  if (has_invert) {
    _session->set_signal_invert(invert_channels);
  }
  if (has_filter) {
    _session->set_glitch_filter(thresholds);
  }
}

void SignalProcessingDock::device_updated() {
  if (_device_agent->have_instance() == false) {
    QLayoutItem *item;
    while ((item = _container_lay->takeAt(0)) != nullptr) {
      if (item->widget()) {
        delete item->widget();
      }
      delete item;
    }
    _invert_group = nullptr;
    _glitch_filter_group = nullptr;
    _no_logic_hint = nullptr;
    _invert_checkBox_list.clear();
    _glitch_checkBox_list.clear();
    _glitch_spinbox_list.clear();
    return;
  }

  build_ui();
}

void SignalProcessingDock::update_widgets_status() {
  bool bEnable = !_session->is_working();

  for (int i = 0; i < _container_lay->count(); ++i) {
    QLayoutItem *item = _container_lay->itemAt(i);
    if (item->widget()) {
      item->widget()->setEnabled(bEnable);
    }
  }
}

void SignalProcessingDock::UpdateLanguage() { build_ui(); }

void SignalProcessingDock::UpdateTheme() { this->update(); }

void SignalProcessingDock::UpdateFont() {
  if (_container_panel == nullptr)
    return;

  QFont sectionTitleFont = dock_font_section_title();
  QFont labelFont = dock_font_label();
  QFont contentFont = dock_font_content();

  setUpdatesEnabled(false);

  auto section_titles =
      _container_panel->findChildren<QLabel *>("dock_section_title");
  for (auto lb : section_titles) {
    lb->setFont(sectionTitleFont);
  }

  auto labels = _container_panel->findChildren<QLabel *>("dock_label");
  for (auto lb : labels) {
    lb->setFont(labelFont);
  }

  auto content_widgets =
      _container_panel->findChildren<QWidget *>("dock_content");
  for (auto w : content_widgets) {
    w->setFont(contentFont);
  }

  auto check_boxes = _container_panel->findChildren<QCheckBox *>();
  for (auto cb : check_boxes) {
    cb->setFont(contentFont);
  }

  auto push_buttons = _container_panel->findChildren<QPushButton *>();
  for (auto pb : push_buttons) {
    pb->setFont(contentFont);
  }

  auto spin_boxes = _container_panel->findChildren<pv::ui::DsSpinBox *>();
  for (auto sb : spin_boxes) {
    sb->setFont(contentFont);
  }

  setUpdatesEnabled(true);
}

void SignalProcessingDock::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  update_invert_state();
  update_glitch_filter_state();
}

void SignalProcessingDock::hideEvent(QHideEvent *event) {
  QWidget::hideEvent(event);
}

void SignalProcessingDock::bind_context(TabContext *ctx) {
  _context = ctx;
  if (_device_agent && _device_agent->have_instance()) {
    auto &saved = ctx->document()->_dock_signal_processing_session;
    if (!saved.isEmpty()) {
      set_session(saved);
    } else {
      update_view();
    }
  } else {
    update_view();
  }
}

void SignalProcessingDock::unbind_context() {
  if (_context && _context->document() && _device_agent &&
      _device_agent->have_instance()) {
    _context->document()->_dock_signal_processing_session = get_session();
  }
  _context = nullptr;
}

QJsonObject SignalProcessingDock::get_session() {
  QJsonObject obj;

  // Invert settings
  QJsonArray invert_array;
  for (size_t i = 0; i < _invert_checkBox_list.size(); i++) {
    QJsonObject ch_obj;
    ch_obj["enable"] = _invert_checkBox_list[i]->isChecked();
    invert_array.append(ch_obj);
  }
  obj["signal_invert"] = invert_array;

  // Glitch filter settings
  QJsonArray glitch_array;
  for (size_t i = 0; i < _glitch_checkBox_list.size(); i++) {
    QJsonObject ch_obj;
    ch_obj["enable"] = _glitch_checkBox_list[i]->isChecked();
    ch_obj["num"] = _glitch_spinbox_list[i]->value();
    glitch_array.append(ch_obj);
  }
  obj["glitch_filter"] = glitch_array;

  return obj;
}

void SignalProcessingDock::set_session(QJsonObject &obj) {
  update_view();

  if (obj.contains("signal_invert")) {
    QJsonArray invert_array = obj["signal_invert"].toArray();
    for (int i = 0;
         i < invert_array.size() && i < (int)_invert_checkBox_list.size();
         i++) {
      QJsonObject ch_obj = invert_array[i].toObject();
      _invert_checkBox_list[i]->setChecked(ch_obj["enable"].toBool());
    }
  }

  if (obj.contains("glitch_filter")) {
    QJsonArray glitch_array = obj["glitch_filter"].toArray();
    for (int i = 0;
         i < glitch_array.size() && i < (int)_glitch_checkBox_list.size();
         i++) {
      QJsonObject ch_obj = glitch_array[i].toObject();
      _glitch_checkBox_list[i]->setChecked(ch_obj["enable"].toBool());
      _glitch_spinbox_list[i]->setValue(ch_obj["num"].toInt());
    }
  }
}

} // namespace dock
} // namespace pv
