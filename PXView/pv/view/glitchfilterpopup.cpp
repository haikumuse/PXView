/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include "glitchfilterpopup.h"

#include <algorithm>

#include "../config/appconfig.h"
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScreen>
#include <QGuiApplication>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QWidget>

#include "logicsignal.h"
#include "pulsehistogramwidget.h"
#include "signal.h"
#include "view.h"
#include "../data/logicsnapshot.h"
#include "../data/pulse_analyzer.h"
#include "../data/signalmodel.h"
#include "../sigsession.h"

namespace pv {
namespace view {

GlitchFilterPopup::GlitchFilterPopup(View& view, QWidget* parent)
    : QWidget(parent),
      _view(view)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(420);

    // 显式设置字体:避免继承 app 全局字体(CJK 字体在小字号下模糊)。
    // QGraphicsDropShadowEffect 会让 widget 先渲染到 pixmap 再投影,导致
    // 文字栅格化模糊,因此不使用 graphics effect,改用 paintEvent 画边框。
    QFont popupFont("Segoe UI", 9);
    popupFont.setStyleStrategy(QFont::PreferAntialias);
    setFont(popupFont);

    build_ui();
    apply_qss();
}

uint32_t GlitchFilterPopup::current_threshold() const
{
    return (uint32_t)_threshold_slider->value();
}

GlitchFilterMode GlitchFilterPopup::current_mode() const
{
    return (GlitchFilterMode)_mode_combo->currentIndex();
}

void GlitchFilterPopup::build_ui()
{
    // 整体布局:无外边距,由 header frame + body 提供内边距
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ===== 1. 头部 QFrame (背景 #2d323d,底部边框,顶部圆角 8px,高 44px) =====
    {
        auto* header = new QFrame(this);
        header->setObjectName(QStringLiteral("header"));
        header->setFixedHeight(44);
        auto* hLay = new QHBoxLayout(header);
        hLay->setContentsMargins(16, 8, 12, 8);
        _title_label = new QLabel(QStringLiteral("毛刺滤波"), header);
        _title_label->setObjectName(QStringLiteral("title"));
        _close_btn = new QPushButton(QStringLiteral("×"), header);
        _close_btn->setObjectName(QStringLiteral("close"));
        _close_btn->setFixedSize(28, 28);
        _close_btn->setCursor(Qt::PointingHandCursor);
        _close_btn->setFocusPolicy(Qt::NoFocus);
        hLay->addWidget(_title_label);
        hLay->addStretch();
        hLay->addWidget(_close_btn);
        root->addWidget(header);
        connect(_close_btn, &QPushButton::clicked, this, &GlitchFilterPopup::on_cancel_clicked);
    }

    // ===== 2. body =====
    auto* body = new QWidget(this);
    auto* bLay = new QVBoxLayout(body);
    bLay->setContentsMargins(16, 16, 16, 16);
    bLay->setSpacing(10);

    // 2a. "脉冲宽度分布" section title
    {
        auto* section = new QLabel(QStringLiteral("脉冲宽度分布"), body);
        section->setObjectName(QStringLiteral("sectionTitle"));
        bLay->addWidget(section);
    }

    // 2b. 直方图
    {
        _histogram = new PulseHistogramWidget(body);
        _histogram->setMinimumHeight(160);
        _histogram->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        bLay->addWidget(_histogram);
    }

    // 2c. 统计行 (QFrame#statsBox 包裹,背景 #1a1d24,边框 #2a2f38,圆角 4px)
    {
        auto* statsBox = new QFrame(body);
        statsBox->setObjectName(QStringLiteral("statsBox"));
        auto* sLay = new QHBoxLayout(statsBox);
        sLay->setContentsMargins(12, 10, 12, 10);
        auto* leftStat = new QLabel(QStringLiteral("将滤除: "), statsBox);
        leftStat->setObjectName(QStringLiteral("statsLabel"));
        _filter_count_lbl = new QLabel(QStringLiteral("0"), statsBox);
        _filter_count_lbl->setObjectName(QStringLiteral("filterCount"));
        auto* leftUnit = new QLabel(QStringLiteral(" 个脉冲"), statsBox);
        leftUnit->setObjectName(QStringLiteral("statsLabel"));
        auto* rightStat = new QLabel(QStringLiteral("剩余有效脉冲: "), statsBox);
        rightStat->setObjectName(QStringLiteral("statsLabel"));
        _remain_count_lbl = new QLabel(QStringLiteral("0"), statsBox);
        _remain_count_lbl->setObjectName(QStringLiteral("remainCount"));
        sLay->addWidget(leftStat);
        sLay->addWidget(_filter_count_lbl);
        sLay->addWidget(leftUnit);
        sLay->addStretch();
        sLay->addWidget(rightStat);
        sLay->addWidget(_remain_count_lbl);
        bLay->addWidget(statsBox);
    }

    // 2d. 类型行
    {
        auto* modeRow = new QHBoxLayout();
        modeRow->setSpacing(12);
        auto* modeLbl = new QLabel(QStringLiteral("类型"), body);
        modeLbl->setObjectName(QStringLiteral("controlLabel"));
        modeLbl->setFixedWidth(50);
        _mode_combo = new QComboBox(body);
        _mode_combo->addItem(QStringLiteral("Both (双向毛刺)"));
        _mode_combo->addItem(QStringLiteral("High (仅高电平上的低毛刺)"));
        _mode_combo->addItem(QStringLiteral("Low (仅低电平上的高毛刺)"));
        modeRow->addWidget(modeLbl);
        modeRow->addWidget(_mode_combo, 1);
        bLay->addLayout(modeRow);
        connect(_mode_combo, SIGNAL(currentIndexChanged(int)),
                this, SLOT(on_mode_changed(int)));
    }

    // 2e. 阈值行
    {
        auto* thrRow = new QHBoxLayout();
        thrRow->setSpacing(12);
        auto* thrLbl = new QLabel(QStringLiteral("阈值"), body);
        thrLbl->setObjectName(QStringLiteral("controlLabel"));
        thrLbl->setFixedWidth(50);
        _threshold_slider = new QSlider(Qt::Horizontal, body);
        _threshold_slider->setMinimum(1);
        _threshold_slider->setMaximum(30);
        _threshold_slider->setValue(_recommended_threshold);
        _threshold_value_lbl = new QLabel(QString::number(_recommended_threshold), body);
        _threshold_value_lbl->setObjectName(QStringLiteral("thresholdValue"));
        _threshold_value_lbl->setAlignment(Qt::AlignCenter);
        _threshold_value_lbl->setMinimumWidth(50);
        auto* thrUnit = new QLabel(QStringLiteral("cycles"), body);
        thrUnit->setObjectName(QStringLiteral("thresholdUnit"));
        thrRow->addWidget(thrLbl);
        thrRow->addWidget(_threshold_slider, 1);
        thrRow->addWidget(_threshold_value_lbl);
        thrRow->addWidget(thrUnit);
        bLay->addLayout(thrRow);
        connect(_threshold_slider, &QSlider::valueChanged,
                this, &GlitchFilterPopup::on_slider_moved);
    }

    // 2e2. 上限行(用户自定义统计范围上限)
    {
        auto* maxRow = new QHBoxLayout();
        maxRow->setSpacing(12);
        auto* maxLbl = new QLabel(QStringLiteral("上限"), body);
        maxLbl->setObjectName(QStringLiteral("controlLabel"));
        maxLbl->setFixedWidth(50);
        _max_spinbox = new QSpinBox(body);
        _max_spinbox->setRange(10, 500);
        _max_spinbox->setValue(30);
        _max_spinbox->setFixedWidth(70);
        _max_spinbox->setSuffix(QStringLiteral(" cyc"));
        auto* maxHint = new QLabel(QStringLiteral("(统计范围,超出不计入)"), body);
        maxHint->setObjectName(QStringLiteral("thresholdUnit"));
        maxRow->addWidget(maxLbl);
        maxRow->addWidget(_max_spinbox);
        maxRow->addWidget(maxHint);
        maxRow->addStretch();
        bLay->addLayout(maxRow);
        connect(_max_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &GlitchFilterPopup::on_max_changed);
    }

    // 2f. 分隔线
    {
        auto* div = new QFrame(body);
        div->setObjectName(QStringLiteral("divider"));
        div->setFrameShape(QFrame::NoFrame);
        bLay->addSpacing(2);
        bLay->addWidget(div);
        bLay->addSpacing(2);
    }

    // 2g2. 自动应用行
    {
        auto* row = new QHBoxLayout();
        row->setSpacing(12);
        _auto_apply_chk = new QCheckBox(
            QStringLiteral("采集后自动应用滤波"), body);
        _auto_apply_chk->setObjectName(QStringLiteral("autoApply"));
        row->addStretch();
        row->addWidget(_auto_apply_chk);
        bLay->addLayout(row);
        connect(_auto_apply_chk, &QCheckBox::toggled,
                this, &GlitchFilterPopup::on_auto_apply_toggled);
    }

    // 2h. 底部按钮行
    {
        auto* btnRow = new QHBoxLayout();
        btnRow->setSpacing(8);
        _apply_one_btn = new QPushButton(QStringLiteral("应用到本通道"), body);
        _apply_all_btn = new QPushButton(QStringLiteral("应用到所有逻辑通道 →"), body);
        _apply_all_btn->setObjectName(QStringLiteral("primary"));
        _cancel_btn = new QPushButton(QStringLiteral("取消"), body);
        btnRow->addWidget(_apply_one_btn);
        btnRow->addWidget(_apply_all_btn);
        btnRow->addStretch();
        btnRow->addWidget(_cancel_btn);
        bLay->addLayout(btnRow);
        connect(_apply_one_btn, &QPushButton::clicked,
                this, &GlitchFilterPopup::on_apply_one_clicked);
        connect(_apply_all_btn, &QPushButton::clicked,
                this, &GlitchFilterPopup::on_apply_all_clicked);
        connect(_cancel_btn, &QPushButton::clicked,
                this, &GlitchFilterPopup::on_cancel_clicked);
    }

    root->addWidget(body);
}

void GlitchFilterPopup::apply_qss()
{
    // 所有规则以 GlitchFilterPopup 为前缀,确保不被 app 全局 QSS 覆盖。
    // 背景等基础色用主题 token,跟随主题切换;强调色(阈值/滤波计数)保留固定色。
    auto token = [](const char* name) {
        return AppConfig::Instance().GetThemeTokenValue(name);
    };
    const QString bgOverlay = token("@bg-overlay");     // 弹窗主背景
    const QString bgBase = token("@bg-base");           // 输入框/统计盒背景
    const QString fgBase = token("@fg-base");           // 主文字
    const QString fgMuted = token("@fg-muted");         // 次要文字
    const QString border = token("@border-strong");     // 边框
    const QString accent = token("@accent");            // 主按钮/强调
    const QString hover = token("@toolbtn-hover");      // hover 态

    // token 缺失(主题未加载)时回退到固定深色,保证可读性
    const QString Q_BG = bgOverlay.isEmpty() ? QStringLiteral("#252932") : bgOverlay;
    const QString Q_INPUTBG = bgBase.isEmpty() ? QStringLiteral("#1a1d24") : bgBase;
    const QString Q_FG = fgBase.isEmpty() ? QStringLiteral("#e0e0e0") : fgBase;
    const QString Q_MUTED = fgMuted.isEmpty() ? QStringLiteral("#9e9e9e") : fgMuted;
    const QString Q_BORDER = border.isEmpty() ? QStringLiteral("#3a3f4b") : border;
    const QString Q_ACCENT = accent.isEmpty() ? QStringLiteral("#1976d2") : accent;
    const QString Q_HOVER = hover.isEmpty() ? QStringLiteral("#3a3f4b") : hover;

    setStyleSheet(QStringLiteral(R"(
        GlitchFilterPopup {
            background: %1;
            border: 1px solid %2;
            border-radius: 8px;
        }
        GlitchFilterPopup QLabel { color: %3; }
        GlitchFilterPopup QLabel#sectionTitle { color: %4; font-size: 11px; }
        GlitchFilterPopup QLabel#statsLabel   { color: %3; font-size: 12px; }
        GlitchFilterPopup QLabel#controlLabel { color: %4; font-size: 12px; }
        GlitchFilterPopup QLabel#thresholdValue { color: #42a5f5; font-family: Consolas, monospace; font-size: 13px; }
        GlitchFilterPopup QLabel#thresholdUnit  { color: %4; font-size: 11px; }
        GlitchFilterPopup QLabel#filterCount { color: #ff5252; font-weight: 600; font-size: 14px; }
        GlitchFilterPopup QLabel#remainCount { color: #81c784; font-weight: 600; font-size: 14px; }
        GlitchFilterPopup QLabel#title       { color: %3; font-weight: 600; font-size: 13px; }
        GlitchFilterPopup QPushButton {
            background: %5; border: 1px solid %2; color: %3;
            padding: 6px 14px; border-radius: 4px; font-size: 12px;
        }
        GlitchFilterPopup QPushButton:hover   { background: %6; border-color: %2; }
        GlitchFilterPopup QPushButton:pressed { background: %5; }
        GlitchFilterPopup QPushButton#primary { background: %7; border-color: %7; color: #ffffff; }
        GlitchFilterPopup QPushButton#primary:hover { background: %7; border-color: %7; }
        GlitchFilterPopup QPushButton#close { background: transparent; border: none; color: %4; font-size: 18px; padding: 0 4px; }
        GlitchFilterPopup QPushButton#close:hover { color: %3; }
        GlitchFilterPopup QComboBox {
            background: %8; border: 1px solid %2; color: %3;
            padding: 5px 8px; border-radius: 4px; font-size: 12px;
        }
        GlitchFilterPopup QComboBox::drop-down { border: none; width: 18px; }
        GlitchFilterPopup QComboBox QAbstractItemView {
            background: %8; border: 1px solid %2; color: %3;
            selection-background-color: %6;
        }
        GlitchFilterPopup QSpinBox {
            background: %8; border: 1px solid %2; color: #42a5f5;
            padding: 3px 6px; border-radius: 4px; font-size: 12px;
        }
        GlitchFilterPopup QSpinBox::up-button, GlitchFilterPopup QSpinBox::down-button {
            background: %5; border: none; width: 16px;
        }
        GlitchFilterPopup QSpinBox::up-button:hover, GlitchFilterPopup QSpinBox::down-button:hover {
            background: %6;
        }
        GlitchFilterPopup QSpinBox::up-arrow { image: none; border-left: 4px solid transparent; border-right: 4px solid transparent; border-bottom: 5px solid %4; width: 0; height: 0; }
        GlitchFilterPopup QSpinBox::down-arrow { image: none; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 5px solid %4; width: 0; height: 0; }
        GlitchFilterPopup QSlider::groove:horizontal { height: 4px; background: %2; border-radius: 2px; }
        GlitchFilterPopup QSlider::handle:horizontal {
            background: #42a5f5; width: 14px; height: 14px; margin: -6px 0;
            border-radius: 7px; border: 2px solid #ffffff;
        }
        GlitchFilterPopup QSlider::handle:horizontal:hover { background: #64b5f6; }
        GlitchFilterPopup QFrame#header {
            background: %5; border-bottom: 1px solid %2;
            border-top-left-radius: 8px; border-top-right-radius: 8px;
        }
        GlitchFilterPopup QFrame#statsBox { background: %8; border: 1px solid %2; border-radius: 4px; }
        GlitchFilterPopup QFrame#divider { background: %2; max-height: 1px; min-height: 1px; }
        GlitchFilterPopup QCheckBox { color: %3; font-size: 12px; spacing: 6px; }
        GlitchFilterPopup QCheckBox::indicator {
            width: 14px; height: 14px;
            border: 1px solid %2; border-radius: 3px;
            background: %8;
        }
        GlitchFilterPopup QCheckBox::indicator:hover { border-color: #42a5f5; }
        GlitchFilterPopup QCheckBox::indicator:checked {
            background: %7; border-color: %7;
            image: none;
        }
    )").arg(Q_BG, Q_BORDER, Q_FG, Q_MUTED, Q_BG, Q_HOVER, Q_ACCENT, Q_INPUTBG));
}

void GlitchFilterPopup::refresh_from_signal()
{
    if (_target_sigs.empty()) {
        return;
    }

    // 批量模式:合并所有子通道的 pulses(sum counts 由 build_histogram 完成)
    // 单通道模式:_target_sigs 仅含 _target_sig,行为等同原实现
    _cached_pulses.clear();
    for (auto* sig : _target_sigs) {
        if (!sig) continue;
        auto* snap = sig->data();
        if (!snap) continue;
        auto model = sig->model();
        if (!model) continue;
        int sig_index = model->index();
        auto pulses = pv::data::PulseAnalyzer::find_pulses(snap, sig_index);
        _cached_pulses.insert(_cached_pulses.end(), pulses.begin(), pulses.end());
    }

    if (_cached_pulses.empty()) {
        _cached_hist.width_counts.clear();
        _cached_hist.max_width = 0;
        _recommended_threshold = 3;
        return;
    }

    rebuild_histogram();
}

void GlitchFilterPopup::rebuild_histogram()
{
    const uint32_t cap = _max_spinbox ? (uint32_t)_max_spinbox->value() : 30;
    _cached_hist = pv::data::PulseAnalyzer::build_histogram(_cached_pulses, cap);
    _recommended_threshold = pv::data::PulseAnalyzer::recommend_threshold(_cached_hist);

    // 滑块上限 = cap(spinbox 值),柱子数 = cap,统一以 cap 为准。
    // 初始 cap=30,调整 spinbox 后滑块上限跟随。
    // blockSignals 防止 setRange 触发 valueChanged → on_slider_moved 连锁更新
    const int upper = (int)cap;
    if (_threshold_slider) {
        _threshold_slider->blockSignals(true);
        _threshold_slider->setRange(1, upper);
        // setRange 会自动 clamp 当前值,同步显示
        _threshold_slider->blockSignals(false);
        _threshold_value_lbl->setText(QString::number(_threshold_slider->value()));
    }
}

void GlitchFilterPopup::refresh()
{
    if (!_target_sig) {
        return;
    }

    // 不重新扫描 LogicSnapshot — 滤波后 snapshot 中短脉冲已被滤除,
    // 重新扫描会得到错误的分布。始终使用 open_for_signal 时缓存的原始脉冲。
    // 仅更新 UI 控件状态(阈值线、统计数字)。
    if (_histogram) {
        _histogram->setData(_cached_hist);
        _histogram->setThresholds(_recommended_threshold, current_threshold());
        _histogram->setFilterThreshold(current_threshold());
    }
    update_stats();
}

void GlitchFilterPopup::on_filter_completed()
{
    // popup 不可见或无目标信号时不做无谓刷新
    if (!isVisible() || !_target_sig) {
        return;
    }
    refresh();
}

void GlitchFilterPopup::on_filter_cleared()
{
    if (!isVisible() || !_target_sig) {
        return;
    }
    refresh();
}

void GlitchFilterPopup::open_for_signal(LogicSignal* sig, const QPoint& anchor_pos)
{
    if (!sig) {
        return;
    }
    _target_sig = sig;
    _target_sigs = {sig};
    _is_batch_mode = false;

    refresh_from_signal();

    // 设置标题:使用 Trace::get_name() 作为通道显示名
    QString display_name = sig->get_name();
    if (display_name.isEmpty()) {
        display_name = QStringLiteral("通道");
    }
    _title_label->setText(display_name + QStringLiteral(" 毛刺滤波"));

    // 直方图数据 + 阈值线
    _histogram->setData(_cached_hist);
    _histogram->setThresholds(_recommended_threshold, _recommended_threshold);
    _histogram->setFilterThreshold(_recommended_threshold);

    // 参数恢复:优先从 Core SessionData 中已应用的 threshold/mode 恢复
    // (FilterProcessor 写入 _glitch_filter_thresholds/_modes,与 SignalModel
    //  的 glitch_filter_width 不一定同步)。仅当滤波处于激活态且向量长度
    // 足够时才取该通道的值;否则回退到推荐阈值 + BOTH 模式。
    uint32_t initial_threshold = _recommended_threshold;
    GlitchFilterMode initial_mode = GLITCH_FILTER_BOTH;
    auto &sess = _view.session();
    if (sess.is_glitch_filter_active()) {
        // 构建 logic_sigs(与 view_glitch_filter.cpp 一致的索引顺序)
        std::vector<LogicSignal*> logic_sigs;
        for (auto s : _view.get_own_signals()) {
            if (s && s->signal_type() == SR_CHANNEL_LOGIC)
                logic_sigs.push_back(static_cast<LogicSignal*>(s));
        }
        auto it = std::find(logic_sigs.begin(), logic_sigs.end(), sig);
        if (it != logic_sigs.end()) {
            int idx = (int)std::distance(logic_sigs.begin(), it);
            const auto &th = sess.glitch_filter_thresholds();
            const auto &md = sess.glitch_filter_modes();
            if (idx < (int)th.size() && th[idx] > 0) {
                // clamp 到当前滑块范围 [1, cap]
                int cap = _max_spinbox ? _max_spinbox->value() : 30;
                int v = (int)th[idx];
                if (v < 1) v = 1;
                if (v > cap) v = cap;
                initial_threshold = (uint32_t)v;
            }
            if (idx < (int)md.size()) {
                initial_mode = md[idx];
            }
        }
    } else if (sig->model()) {
        // 兼容旧路径:滤波未激活时若 SignalModel 有遗留 width,沿用之
        if (sig->model()->glitch_filter_enabled() && sig->model()->glitch_filter_width() > 0) {
            int w = sig->model()->glitch_filter_width();
            if (w >= 1 && w <= (int)_cached_hist.max_width) {
                initial_threshold = (uint32_t)w;
            }
        }
    }

    // 同步控件状态(注意:setValue 会触发 valueChanged -> on_slider_moved,
    // 但此时 _cached_pulses 已就绪,统计可正确计算)
    _threshold_slider->blockSignals(true);
    _threshold_slider->setValue((int)initial_threshold);
    _threshold_slider->blockSignals(false);
    _threshold_value_lbl->setText(QString::number(initial_threshold));

    _mode_combo->blockSignals(true);
    _mode_combo->setCurrentIndex((int)initial_mode);
    _mode_combo->blockSignals(false);


    if (_auto_apply_chk) {
        _auto_apply_chk->blockSignals(true);
        _auto_apply_chk->setChecked(_view.session().glitch_filter_auto_apply());
        _auto_apply_chk->blockSignals(false);
    }

    // 初次着色与统计
    _histogram->setFilterThreshold(initial_threshold);
    _histogram->setThresholds(_recommended_threshold, initial_threshold);
    update_stats();

    show_and_position(anchor_pos);

    // 发出首次预览
    emit preview_changed(_target_sig, initial_threshold, current_mode());
}

void GlitchFilterPopup::open_for_batch(const std::vector<LogicSignal*>& sigs, const QPoint& anchor_pos)
{
    if (sigs.empty()) {
        return;
    }
    _target_sigs = sigs;
    _target_sig = sigs[0];  // 主信号(用于 refresh/close 等检查)
    _is_batch_mode = true;

    refresh_from_signal();

    // 批量模式标题:汇总子通道名
    QString title;
    if (sigs.size() == 1) {
        title = sigs[0]->get_name();
    } else {
        // 取前 3 个通道名 + "+N" 后缀
        QStringList names;
        for (size_t i = 0; i < sigs.size() && i < 3; ++i) {
            names << sigs[i]->get_name();
        }
        title = names.join(QStringLiteral(", "));
        if (sigs.size() > 3) {
            title += QStringLiteral(" +%1").arg(sigs.size() - 3);
        }
    }
    if (title.isEmpty()) title = QStringLiteral("批量");
    _title_label->setText(title + QStringLiteral(" 批量毛刺滤波"));

    // 直方图数据 + 阈值线
    _histogram->setData(_cached_hist);
    _histogram->setThresholds(_recommended_threshold, _recommended_threshold);
    _histogram->setFilterThreshold(_recommended_threshold);

    // 批量模式参数恢复:若所有子通道在 SessionData 中已应用的 threshold/mode
    // 一致,则恢复该共同值;否则回退到推荐阈值 + BOTH 模式(避免误用某通道值)。
    uint32_t initial_threshold = _recommended_threshold;
    GlitchFilterMode initial_mode = GLITCH_FILTER_BOTH;
    auto &sess = _view.session();
    if (sess.is_glitch_filter_active()) {
        std::vector<LogicSignal*> logic_sigs;
        for (auto s : _view.get_own_signals()) {
            if (s && s->signal_type() == SR_CHANNEL_LOGIC)
                logic_sigs.push_back(static_cast<LogicSignal*>(s));
        }
        const auto &th = sess.glitch_filter_thresholds();
        const auto &md = sess.glitch_filter_modes();
        int cap = _max_spinbox ? _max_spinbox->value() : 30;

        bool first = true;
        bool consistent = true;
        uint32_t common_th = 0;
        GlitchFilterMode common_md = GLITCH_FILTER_BOTH;
        for (auto *s : sigs) {
            auto it = std::find(logic_sigs.begin(), logic_sigs.end(), s);
            if (it == logic_sigs.end()) continue;
            int idx = (int)std::distance(logic_sigs.begin(), it);
            uint32_t t = (idx < (int)th.size()) ? th[idx] : 0;
            GlitchFilterMode m = (idx < (int)md.size()) ? md[idx] : GLITCH_FILTER_BOTH;
            if (first) {
                first = false;
                // clamp 到滑块范围
                int v = (int)t;
                if (v < 1) v = 1;
                if (v > cap) v = cap;
                common_th = (uint32_t)v;
                common_md = m;
            } else {
                int v = (int)t;
                if (v < 1) v = 1;
                if (v > cap) v = cap;
                if ((uint32_t)v != common_th || m != common_md) {
                    consistent = false;
                    break;
                }
            }
        }
        if (!first && consistent) {
            initial_threshold = common_th;
            initial_mode = common_md;
        }
    }

    _threshold_slider->blockSignals(true);
    _threshold_slider->setValue((int)initial_threshold);
    _threshold_slider->blockSignals(false);
    _threshold_value_lbl->setText(QString::number(initial_threshold));

    _mode_combo->blockSignals(true);
    _mode_combo->setCurrentIndex((int)initial_mode);
    _mode_combo->blockSignals(false);

    if (_auto_apply_chk) {
        _auto_apply_chk->blockSignals(true);
        _auto_apply_chk->setChecked(_view.session().glitch_filter_auto_apply());
        _auto_apply_chk->blockSignals(false);
    }

    _histogram->setFilterThreshold(initial_threshold);
    _histogram->setThresholds(_recommended_threshold, initial_threshold);
    update_stats();

    // 批量模式:隐藏"应用到所有逻辑通道"按钮(语义不符),
    // "应用本通道"按钮改名为"应用到子通道"
    if (_apply_all_btn) {
        _apply_all_btn->setVisible(false);
    }
    if (_apply_one_btn) {
        _apply_one_btn->setText(QStringLiteral("应用到子通道"));
    }

    show_and_position(anchor_pos);

    emit preview_batch_changed(_target_sigs, initial_threshold, current_mode());
}

void GlitchFilterPopup::show_and_position(const QPoint& anchor_pos)
{
    // 弹出位置(参考实现:屏幕边缘检测,避免超出)
    QPoint pos = anchor_pos;
    adjustSize();
    QScreen* screen = QGuiApplication::screenAt(pos);
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect geom = screen->availableGeometry();
        if (pos.y() + height() > geom.bottom() - 8) {
            pos.setY(geom.bottom() - height() - 8);
        }
        if (pos.x() + width() > geom.right() - 8) {
            // 右侧放不下,尝试放到锚点左侧
            pos.setX(pos.x() - width() - 16);
        }
    }
    move(pos);

    // 淡入动画 200ms(增强体验;对应参考实现 FilterPopup::openForChannel)
    setWindowOpacity(0.0);
    show();
    raise();
    activateWindow();
    auto* fade = new QPropertyAnimation(this, "windowOpacity", this);
    fade->setDuration(200);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}

void GlitchFilterPopup::on_slider_moved(int value)
{
    _threshold_value_lbl->setText(QString::number(value));
    _histogram->setFilterThreshold((uint32_t)value);
    _histogram->setThresholds(_recommended_threshold, (uint32_t)value);
    update_stats();
    if (_is_batch_mode) {
        emit preview_batch_changed(_target_sigs, (uint32_t)value, current_mode());
    } else if (_target_sig) {
        emit preview_changed(_target_sig, (uint32_t)value, current_mode());
    }
}

void GlitchFilterPopup::on_mode_changed(int /*index*/)
{
    update_histogram_coloring();
    update_stats();
    if (_is_batch_mode) {
        emit preview_batch_changed(_target_sigs, current_threshold(), current_mode());
    } else if (_target_sig) {
        emit preview_changed(_target_sig, current_threshold(), current_mode());
    }
}

void GlitchFilterPopup::on_apply_one_clicked()
{
    if (_is_batch_mode) {
        if (_target_sigs.empty()) return;
        emit apply_batch_requested(_target_sigs, current_threshold(), current_mode());
        close();
        return;
    }
    if (!_target_sig) {
        return;
    }
    emit apply_requested(_target_sig, current_threshold(), current_mode(), false);
    close();
}

void GlitchFilterPopup::on_apply_all_clicked()
{
    if (!_target_sig) {
        return;
    }
    emit apply_requested(_target_sig, current_threshold(), current_mode(), true);
    close();
}

void GlitchFilterPopup::on_cancel_clicked()
{
    close();
}

void GlitchFilterPopup::on_max_changed(int val)
{
    (void)val;
    const uint32_t cap = _max_spinbox ? (uint32_t)_max_spinbox->value() : 30;
    // 用户修改了统计上限 → 用新 cap 重建直方图 + 更新滑块范围。
    // rebuild_histogram 内部已 blockSignals + clamp 当前值 + 同步显示。
    rebuild_histogram();

    // 直方图控件更新(柱子数 = cap,与滑块上限同步)
    if (_histogram) {
        _histogram->setNumBars((int)cap);
        _histogram->setData(_cached_hist);
        _histogram->setThresholds(_recommended_threshold, current_threshold());
        _histogram->setFilterThreshold(current_threshold());
    }

    update_stats();
    if (_is_batch_mode) {
        emit preview_batch_changed(_target_sigs, current_threshold(), current_mode());
    } else if (_target_sig) {
        emit preview_changed(_target_sig, current_threshold(), current_mode());
    }
    // 强制直方图控件重绘(setData 内部未必触发 repaint)
    if (_histogram) {
        _histogram->update();
    }
}

void GlitchFilterPopup::on_auto_apply_toggled(bool checked)
{
    // 用户勾选后,将标志写入 Core SessionData。
    // 实际的"采集完成后重新应用"逻辑在 SigSession 的
    // RevEndPacket handler 中执行:检测到 _glitch_filter_auto_apply
    // 且 thresholds/modes 非空时调用 _filter_processor->set_glitch_filter()。
    _view.session().set_glitch_filter_auto_apply(checked);
}

void GlitchFilterPopup::update_histogram_coloring()
{
    // PulseHistogramWidget::setFilterThreshold 内部完成着色,
    // 这里只需把当前阈值同步过去
    if (_histogram) {
        _histogram->setFilterThreshold(current_threshold());
    }
}

void GlitchFilterPopup::update_stats()
{
    auto filtered = pv::data::PulseAnalyzer::preview_filter(
        _cached_pulses, current_threshold(), current_mode());
    const int total = (int)_cached_pulses.size();
    const int filtered_count = (int)filtered.size();
    const int remain_count = total - filtered_count;

    // UI 已将 "将滤除: " / " 个脉冲" 拆分为独立 label,这里只更新数字 label
    _filter_count_lbl->setText(QString::number(filtered_count));
    _remain_count_lbl->setText(QString::number(remain_count));
}

void GlitchFilterPopup::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QWidget::keyPressEvent(event);
}

void GlitchFilterPopup::closeEvent(QCloseEvent* event)
{
    // 重置批量模式 UI 状态,确保下次以单通道模式打开时控件状态正确
    if (_is_batch_mode) {
        _is_batch_mode = false;
        _target_sigs.clear();
        if (_apply_all_btn) {
            _apply_all_btn->setVisible(true);
        }
        if (_apply_one_btn) {
            _apply_one_btn->setText(QStringLiteral("应用到本通道"));
        }
    }
    emit closed();
    QWidget::closeEvent(event);
}

void GlitchFilterPopup::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
}

} // namespace view
} // namespace pv
