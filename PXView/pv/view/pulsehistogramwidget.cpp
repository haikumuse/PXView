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

#include "pulsehistogramwidget.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QRectF>
#include <algorithm>
#include <cstdint>

namespace pv {
namespace view {

// 固定 30 根柱子(对应参考实现 HistogramWidget::nBars = 30,与 PulseAnalyzer
// 的 HIST_MAX_WIDTH 一致)。即使实际数据 max_width 不足 30 也画满 30 根,
// 保持视觉稳定。
namespace {
const int kNumBars = 30;
const double kBarGap = 2.0;
const int kPadTop = 20;      // 顶部留给"推荐/当前"标签
const int kPadBottom = 18;   // 底部留给 X 轴标签
const int kPadX = 8;

const QColor kBgColor("#1a1d24");
const QColor kBorderColor("#2a2f38");
const QColor kBarColor("#4a5060");
const QColor kFilterColor("#ff5252");
const QColor kRecommendLine("#ff9800");
const QColor kCurrentLine("#42a5f5");
const QColor kTickColor("#9e9e9e");
} // namespace

PulseHistogramWidget::PulseHistogramWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMinimumSize(minimumSizeHint());
}

void PulseHistogramWidget::setData(const pv::data::PulseAnalyzer::Histogram& hist)
{
    _hist = hist;
    _has_data = true;
    update();
}

void PulseHistogramWidget::setThresholds(uint32_t recommended, uint32_t current)
{
    _recommended_threshold = recommended;
    _current_threshold = current;
    update();
}

void PulseHistogramWidget::setFilterThreshold(uint32_t threshold)
{
    _filter_threshold = threshold;
    update();
}

void PulseHistogramWidget::setRecommendedThreshold(uint32_t recommended)
{
    _recommended_threshold = recommended;
    update();
}

QSize PulseHistogramWidget::sizeHint() const
{
    return QSize(388, 160);
}

QSize PulseHistogramWidget::minimumSizeHint() const
{
    return QSize(200, 120);
}

void PulseHistogramWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect();
    const QRect barsArea(kPadX, kPadTop,
                         r.width() - 2 * kPadX,
                         r.height() - kPadTop - kPadBottom);

    // 1) 背景 #1a1d24,边框 #2a2f38,圆角 4px
    p.setPen(QPen(kBorderColor, 1));
    p.setBrush(kBgColor);
    p.drawRoundedRect(r.adjusted(0, 0, -1, -1), 4, 4);

    if (!barsArea.isValid() || barsArea.width() <= 0 || barsArea.height() <= 0)
        return;

    // 2) 计算 barW (30 根柱子 + 29 个 gap)
    const double barW = (barsArea.width() - (kNumBars - 1) * kBarGap) / kNumBars;

    // 3) 取 maxCount (跨所有宽度 1..30)
    int maxCount = 1;
    if (_has_data) {
        for (const auto& kv : _hist.width_counts) {
            if (kv.first >= 1 && kv.first <= kNumBars && kv.second > maxCount)
                maxCount = kv.second;
        }
    }

    // 4) 阈值线 X 坐标辅助
    auto thresholdX = [&](int t) -> double {
        int tt = std::clamp(t, 1, kNumBars);
        return barsArea.left() + (double)(tt - 1) / (kNumBars - 1) * barsArea.width();
    };

    // 5) 绘制柱子 (颜色:width <= currentThreshold → 红 #ff5252,否则 #4a5060)
    p.setPen(Qt::NoPen);
    for (int i = 0; i < kNumBars; ++i) {
        int w = i + 1;
        int count = 0;
        if (_has_data) {
            auto it = _hist.width_counts.find((uint32_t)w);
            if (it != _hist.width_counts.end())
                count = it->second;
        }
        double h = (count > 0) ? (double)count / maxCount * barsArea.height() : 0;
        if (count > 0) h = std::max(h, 2.0);  // min-height 2px

        double x = barsArea.left() + i * (barW + kBarGap);
        double y = barsArea.bottom() - h;

        QColor c = (_filter_threshold > 0 && w <= (int)_filter_threshold)
                       ? kFilterColor
                       : kBarColor;
        p.setBrush(c);
        p.drawRoundedRect(QRectF(x, y, barW, h), 2, 2, Qt::AbsoluteSize);
    }

    // 6) 推荐阈值线 (橙 #ff9800, 2px, 带"推荐"标签)
    QFont lblFont("Segoe UI", 7);
    QFontMetrics fm(lblFont);

    if (_recommended_threshold >= 1 && _recommended_threshold <= (uint32_t)kNumBars) {
        double recX = thresholdX((int)_recommended_threshold);
        p.setPen(QPen(kRecommendLine, 2));
        p.drawLine(QPointF(recX, barsArea.top()), QPointF(recX, barsArea.bottom()));

        p.setPen(Qt::NoPen);
        p.setBrush(kRecommendLine);
        p.setFont(lblFont);
        QString recText = QStringLiteral("推荐");
        int recTextW = fm.horizontalAdvance(recText) + 8;
        QRectF recTag(recX - recTextW / 2.0, barsArea.top() - 16, recTextW, 12);
        p.drawRoundedRect(recTag, 2, 2);
        p.setPen(QColor("#000"));
        p.drawText(recTag, Qt::AlignCenter, recText);
    }

    // 7) 当前阈值线 (蓝 #42a5f5, 2px, 带"当前"标签)
    if (_current_threshold >= 1 && _current_threshold <= (uint32_t)kNumBars) {
        double curX = thresholdX((int)_current_threshold);
        p.setPen(QPen(kCurrentLine, 2));
        p.drawLine(QPointF(curX, barsArea.top()), QPointF(curX, barsArea.bottom()));

        p.setPen(Qt::NoPen);
        p.setBrush(kCurrentLine);
        QString curText = QStringLiteral("当前");
        int curTextW = fm.horizontalAdvance(curText) + 8;
        QRectF curTag(curX - curTextW / 2.0, barsArea.top() - 16, curTextW, 12);
        // 避免和推荐标签重叠:若太近,把当前标签放到推荐标签右侧
        if (_recommended_threshold >= 1 && _recommended_threshold <= (uint32_t)kNumBars) {
            double recX = thresholdX((int)_recommended_threshold);
            int recTextW = fm.horizontalAdvance(QStringLiteral("推荐")) + 8;
            if (std::abs(curX - recX) < (recTextW + curTextW) / 2.0 + 2 &&
                _current_threshold != _recommended_threshold) {
                curTag.moveLeft(recX + recTextW / 2.0 + 2);
            }
        }
        p.drawRoundedRect(curTag, 2, 2);
        p.setPen(QColor("#fff"));
        p.drawText(curTag, Qt::AlignCenter, curText);
    }

    // 8) X 轴标签:1 和每 5 一标
    p.setPen(kTickColor);
    p.setFont(QFont("Segoe UI", 7));
    for (int w = 1; w <= kNumBars; ++w) {
        if (w == 1 || w % 5 == 0) {
            double x = thresholdX(w);
            QRectF tag(x - 10, barsArea.bottom() + 3, 20, 12);
            p.drawText(tag, Qt::AlignCenter, QString::number(w));
        }
    }
}

} // namespace view
} // namespace pv
