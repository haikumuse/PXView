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

#include "render_pass.h"
#include "viewport.h"
#include "view.h"
#include "trace.h"
#include "signal.h"
#include "logicsignal.h"
#include "cursor.h"
#include "xcursor.h"
#include "dsosignal.h"
#include "analogsignal.h"
#include "lissajoustrace.h"
#include "ruler.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <algorithm>
#include <cmath>
#include <list>
#include <vector>

#include "../config/appconfig.h"
#include "../sigsession.h"
#include "../ui/dockfonts.h"
#include "../ui/fn.h"
#include "../ui/langresource.h"
#include "../pxvdef.h"
#include "../deviceagent.h"
#include <libsigrok/libsigrok.h>

namespace pv {
namespace view {

// ---------------------------------------------------------------------------
// Static helpers (migrated from viewport_painter.cpp for MeasureOverlayPass)
// ---------------------------------------------------------------------------

struct BrutalStyle {
  QColor bg;
  QColor text;
};

static BrutalStyle getBrutalStyle(const QColor &back, const QColor &panelBg,
                                  const QColor &panelText) {
  double luminance =
      (back.red() * 0.299 + back.green() * 0.587 + back.blue() * 0.114);
  bool isDark = luminance < 128;

  if (isDark) {
    return {panelBg, panelText};
  } else {
    return {panelText, panelBg};
  }
}

static void drawFloatingPanel(QPainter &p, const QPointF &cursorPos,
                              double viewWidth, double viewHeight,
                              const QColor &back, const QColor &panelBg,
                              const QColor &panelText,
                              const std::vector<std::pair<QString, QString>> &rows) {
  BrutalStyle style = getBrutalStyle(back, panelBg, panelText);

  QFont labelFont = p.font();
  labelFont.setPixelSize(floating_panel_font_label_size());
  labelFont.setWeight(QFont::Black);
  labelFont.setCapitalization(QFont::AllUppercase);
  labelFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
  apply_global_font_strategy(labelFont);

  QFont valueFont = p.font();
  valueFont.setPixelSize(floating_panel_font_value_size());
  valueFont.setWeight(QFont::Black);
  valueFont.setFamily("Space Mono, Courier New, monospace");
  apply_global_font_strategy(valueFont);

  QFontMetrics fmLabel(labelFont);
  QFontMetrics fmValue(valueFont);

  const int pad = 14;
  const int gridGapH = 14;
  const int gridGapV = 10;
  const int labelValueGap = 2;

  bool hasLabels = false;
  for (const auto &row : rows) {
    if (!row.first.isEmpty()) {
      hasLabels = true;
      break;
    }
  }

  int cols = (hasLabels && rows.size() >= 2) ? 2 : 1;
  int gridRows = ((int)rows.size() + cols - 1) / cols;

  int cellH = fmLabel.height() + labelValueGap + fmValue.height();
  int cellH_noLabel = fmValue.height();

  int colWidths[2] = {0, 0};
  for (size_t i = 0; i < rows.size(); i++) {
    int col = (int)i % cols;
    QString cleanLabel = rows[i].first.trimmed().toUpper();
    if (cleanLabel.endsWith(':'))
      cleanLabel.chop(1);
    int labelW =
        cleanLabel.isEmpty() ? 0 : fmLabel.horizontalAdvance(cleanLabel);

    QString val = rows[i].second;
    if (val.startsWith('+'))
      val.remove(0, 1);
    int valW = fmValue.horizontalAdvance(val);

    colWidths[col] = qMax(colWidths[col], qMax(labelW, valW));
  }

  double panelW, panelH;
  if (cols == 2)
    panelW = pad * 2 + colWidths[0] + gridGapH + colWidths[1];
  else
    panelW = pad * 2 + colWidths[0];

  int usedCellH = hasLabels ? cellH : cellH_noLabel;
  panelH = pad * 2 + gridRows * usedCellH + (gridRows - 1) * gridGapV;

  const double offsetX = 15, offsetY = 20;
  double px = cursorPos.x() + offsetX;
  double py = cursorPos.y() + offsetY;
  if (px + panelW > viewWidth)
    px = cursorPos.x() - panelW - offsetX;
  if (py + panelH > viewHeight)
    py = cursorPos.y() - panelH - offsetY;

  QRectF panelRect(px, py, panelW, panelH);

  p.setRenderHint(QPainter::Antialiasing, false);

  p.setPen(Qt::NoPen);
  p.setBrush(style.bg);
  p.drawRect(panelRect);

  double y = panelRect.top() + pad;
  for (size_t i = 0; i < rows.size(); i++) {
    int col = (int)i % cols;
    int row = (int)i / cols;

    double cellX = panelRect.left() + pad + col * (colWidths[0] + gridGapH);
    double cellY = y + row * (usedCellH + gridGapV);

    QString cleanLabel = rows[i].first.trimmed();
    if (cleanLabel.endsWith(':') || cleanLabel.endsWith(QChar(0xFF1A)))
      cleanLabel.chop(1);
    cleanLabel = cleanLabel.trimmed();

    if (!cleanLabel.isEmpty()) {
      p.setFont(labelFont);
      p.setPen(style.text);
      QString upperLabel = cleanLabel.toUpper();
      double labelY = cellY + fmLabel.ascent();
      p.drawText(QPointF(cellX, labelY), upperLabel);
    }

    p.setFont(valueFont);
    p.setPen(style.text);
    double valueY = cleanLabel.isEmpty() ? cellY + fmValue.ascent()
                                         : cellY + fmLabel.height() +
                                               labelValueGap + fmValue.ascent();

    QString valText = rows[i].second;
    if (valText.startsWith('+'))
      valText.remove(0, 1);
    p.drawText(QPointF(cellX, valueY), valText);
  }
}

// ---------------------------------------------------------------------------
// RenderPipeline
// ---------------------------------------------------------------------------

void RenderPipeline::add_pass(std::unique_ptr<RenderPass> pass) {
  _passes.push_back(std::move(pass));
}

void RenderPipeline::render(QPainter &p, const RenderContext &ctx) {
  // Sort passes by z_order (stable to preserve insertion order for
  // equal z_orders).
  std::stable_sort(_passes.begin(), _passes.end(),
                   [](const std::unique_ptr<RenderPass> &a,
                      const std::unique_ptr<RenderPass> &b) {
                     return a->z_order() < b->z_order();
                   });

  for (auto &pass : _passes) {
    if (pass->should_run(ctx)) {
      pass->render(p, ctx);
    }
  }
}

void RenderPipeline::clear() {
  _passes.clear();
}

// ---------------------------------------------------------------------------
// GroupCardBackgroundPass
// ---------------------------------------------------------------------------

bool GroupCardBackgroundPass::should_run(const RenderContext &ctx) const {
  return ctx.type == 0 /* TIME_VIEW */ && ctx.is_logic_mode &&
         ctx.groups && !ctx.groups->empty();
}

void GroupCardBackgroundPass::render(QPainter &p, const RenderContext &ctx) {
  if (!ctx.groups || !ctx.view)
    return;

  // Sort group indices by their first trace's v_offset
  std::vector<size_t> group_indices(ctx.groups->size());
  for (size_t i = 0; i < ctx.groups->size(); i++)
    group_indices[i] = i;
  std::sort(group_indices.begin(), group_indices.end(),
            [&groups = *ctx.groups](size_t a, size_t b) {
              if (groups[a].traces.empty())
                return false;
              if (groups[b].traces.empty())
                return true;
              return groups[a].traces[0]->get_v_offset() <
                     groups[b].traces[0]->get_v_offset();
            });

  for (size_t idx = 0; idx < group_indices.size(); idx++) {
    const auto &group = (*ctx.groups)[group_indices[idx]];
    if (group.traces.empty())
      continue;

    double groupTop = 1e9;
    double groupBottom = -1e9;
    for (auto gt : group.traces) {
      double traceTop = gt->get_v_offset() - gt->get_totalHeight() * 0.5 -
                        View::SignalMargin;
      double traceBottom = gt->get_v_offset() +
                           gt->get_totalHeight() * 0.5 + View::SignalMargin;
      groupTop = std::min(groupTop, traceTop);
      groupBottom = std::max(groupBottom, traceBottom);
    }

    double cardTop = groupTop - View::GroupGap * 0.5;
    double cardHeight = groupBottom - groupTop + View::GroupGap;

    QRectF cardRect(-View::GroupCardRadius, cardTop,
                    ctx.viewWidth + View::GroupCardRadius + 1,
                    cardHeight);
    QPainterPath groupPath;
    groupPath.addRoundedRect(cardRect, View::GroupCardRadius,
                             View::GroupCardRadius);

    if (ctx.view->is_colored_card_mode()) {
      // Per-trace colored rectangles clipped within the card path
      p.save();
      p.setClipPath(groupPath);
      p.setPen(Qt::NoPen);

      for (size_t i = 0; i < group.traces.size(); i++) {
        auto gt = group.traces[i];
        double tTop = gt->get_v_offset() - gt->get_totalHeight() * 0.5 -
                      View::SignalMargin;
        double tBottom = gt->get_v_offset() + gt->get_totalHeight() * 0.5 +
                         View::SignalMargin;

        if (i == 0)
          tTop -= View::GroupGap * 0.5;
        if (i == group.traces.size() - 1)
          tBottom += View::GroupGap * 0.5;

        QRectF traceRect(-View::GroupCardRadius, tTop,
                         ctx.viewWidth + View::GroupCardRadius + 1,
                         tBottom - tTop);
        p.setBrush(ctx.view->get_trace_card_color(gt));
        p.drawRect(traceRect);
      }
      p.restore();
    } else {
      // Single-color filled card
      p.setPen(Qt::NoPen);
      p.setBrush(ctx.view->get_group_card_color());
      p.drawPath(groupPath);
    }
  }
}

// ---------------------------------------------------------------------------
// SignalPixmapPass — cached pixmap rebuild + blit for signal waveforms.
// In logic mode: logic signals use paint_mid_align_sample, non-decoder traces
// use paint_mid, decoder traces are excluded (handled by DecodeTracePass).
// In non-logic mode: all enabled traces (except Lissajous-skipped) go into
// the pixmap via paint_mid.
// ---------------------------------------------------------------------------

bool SignalPixmapPass::should_run(const RenderContext &ctx) const {
  return ctx.viewport && ctx.view && ctx.traces && !ctx.traces->empty();
}

void SignalPixmapPass::render(QPainter &p, const RenderContext &ctx) {
  Viewport *vp = ctx.viewport;
  View *view = ctx.view;
  const auto &traces = *ctx.traces;

  if (ctx.is_logic_mode) {
    // Determine if view parameters changed (requires full logic signal rebuild)
    bool view_params_changed =
        (view->scale() != vp->_curScale ||
         view->offset() != vp->_curOffset ||
         view->get_signalHeight() != vp->_curSignalHeight ||
         view->get_vOffset() != vp->_curVOffset);

    const qreal dpr = vp->devicePixelRatioF();
    const QSize pixmapSize = (QSizeF(vp->size()) * dpr).toSize();
    const bool pixmap_changed =
        vp->_pixmap.isNull() ||
        vp->_pixmap.size() != pixmapSize ||
        !qFuzzyCompare(vp->_pixmap.devicePixelRatioF(), dpr);

    if (view_params_changed || vp->_need_update || pixmap_changed) {
      vp->_curScale = view->scale();
      vp->_curOffset = view->offset();
      vp->_curSignalHeight = view->get_signalHeight();
      vp->_curVOffset = view->get_vOffset();

      // Reuse the existing QPixmap when size & DPR match (avoids heap
      // alloc/dealloc on every frame in realtime refresh mode).
      if (pixmap_changed) {
        vp->_pixmap = QPixmap(pixmapSize);
        vp->_pixmap.setDevicePixelRatio(dpr);
      }
      vp->_pixmap.fill(Qt::transparent);

      QPainter dbp(&vp->_pixmap);
      dbp.translate(0, -view->get_vOffset());

      bool bFirst = true;
      uint64_t end_align_sample = 0;

      for (auto t : traces) {
        if (t->enabled()) {
          std::list<int> _index_list = t->get_index_list();
          int idx = *_index_list.begin() % 8;
          QString token = QString("@logic-channel-%1").arg(idx);
          QColor color = AppConfig::Instance().GetThemeColor(token);
          if (!color.isValid()) {
            color = Viewport::PROBE_COLORS[idx];
          }
          if (t->signal_type() == SR_CHANNEL_LOGIC) {
            LogicSignal *logic_signal = (LogicSignal *)t;
            if (bFirst && logic_signal->data())
              end_align_sample =
                  logic_signal->data()->get_ring_sample_count();
            logic_signal->paint_mid_align_sample(
                dbp, 0, t->get_view_rect().right(), color, ctx.back,
                end_align_sample);
            bFirst = false;
          } else if (t->signal_type() != SR_CHANNEL_DECODER) {
            // Non-logic, non-decoder traces go into the cached pixmap
            t->paint_mid(dbp, 0, t->get_view_rect().right(), ctx.fore,
                         ctx.back);
          }
        }
      }
      vp->_need_update = false;
    }

    // 1. Blit the cached logic signal pixmap (cheap: just a memcpy)
    p.drawPixmap(0, 0, vp->_pixmap);
  } else {
    // Non-logic mode (DSO/analog)
    const qreal dpr = vp->devicePixelRatioF();
    const QSize pixmapSize = (QSizeF(vp->size()) * dpr).toSize();
    const bool pixmap_changed =
        vp->_pixmap.isNull() ||
        vp->_pixmap.size() != pixmapSize ||
        !qFuzzyCompare(vp->_pixmap.devicePixelRatioF(), dpr);

    if (view->scale() != vp->_curScale ||
        view->offset() != vp->_curOffset ||
        view->get_signalHeight() != vp->_curSignalHeight ||
        view->get_vOffset() != vp->_curVOffset ||
        vp->_need_update || pixmap_changed) {

      vp->_curScale = view->scale();
      vp->_curOffset = view->offset();
      vp->_curSignalHeight = view->get_signalHeight();
      vp->_curVOffset = view->get_vOffset();

      // Reuse the existing QPixmap when size & DPR match (avoids heap
      // alloc/dealloc on every frame in DSO continuous mode).
      if (pixmap_changed) {
        vp->_pixmap = QPixmap(pixmapSize);
        vp->_pixmap.setDevicePixelRatio(dpr);
      }
      vp->_pixmap.fill(Qt::transparent);

      QPainter dbp(&vp->_pixmap);
      dbp.translate(0, -view->get_vOffset());

      bool isLissa = false;

      if (view->get_work_mode() == DSO) {
        auto lis_trace = view->get_own_lissajous_trace();
        if (lis_trace && lis_trace->enabled()) {
          isLissa = true;
        }
      }

      for (auto t : traces) {
        if (t->enabled()) {
          if (isLissa && t->signal_type() == SR_CHANNEL_DSO)
            continue;
          if (isLissa && t->signal_type() == SR_CHANNEL_MATH)
            continue;
          t->paint_mid(dbp, 0, t->get_view_rect().right(), ctx.fore,
                       ctx.back);
        }
      }
      vp->_need_update = false;
    }
    p.drawPixmap(0, 0, vp->_pixmap);
  }
}

// ---------------------------------------------------------------------------
// DecodeTracePass — renders decode traces directly on the widget (not via
// QPixmap) to ensure crisp text rendering. Only runs in logic mode; in
// non-logic mode, decoder traces are included in the cached pixmap.
// ---------------------------------------------------------------------------

bool DecodeTracePass::should_run(const RenderContext &ctx) const {
  if (!ctx.traces || !ctx.is_logic_mode)
    return false;
  for (auto t : *ctx.traces) {
    if (t->enabled() && t->signal_type() == SR_CHANNEL_DECODER)
      return true;
  }
  return false;
}

void DecodeTracePass::render(QPainter &p, const RenderContext &ctx) {
  if (!ctx.traces)
    return;

  p.save();
  p.translate(0, -ctx.vOffset);

  for (auto t : *ctx.traces) {
    if (t->enabled() && t->signal_type() == SR_CHANNEL_DECODER) {
      t->paint_mid(p, 0, t->get_view_rect().right(), ctx.fore, ctx.back);
    }
  }
  p.restore();
}

// ---------------------------------------------------------------------------
// CursorOverlayPass — renders regular cursors, xcursors, trigger cursor,
// and search cursor on top of all signal content.
// ---------------------------------------------------------------------------

bool CursorOverlayPass::should_run(const RenderContext &ctx) const {
  return ctx.type == TIME_VIEW && ctx.viewport && ctx.view;
}

void CursorOverlayPass::render(QPainter &p, const RenderContext &ctx) {
  View *view = ctx.view;
  const QRect xrect = view->get_view_rect();
  const QPoint &hover = view->hover_point();

  // 1. Regular cursors
  if (view->cursors_shown()) {
    auto &cursor_list = view->get_cursorList();
    for (auto cursor : cursor_list) {
      const int64_t cursorX = view->index2pixel(cursor->index());
      if (xrect.contains(hover.x(), hover.y()) &&
          qAbs(cursorX - hover.x()) <= Viewport::HitCursorMargin)
        cursor->paint(p, xrect, 1,
                      view->session().is_stopped_status());
      else
        cursor->paint(p, xrect, 0,
                      view->session().is_stopped_status());
    }
  }

  // 2. X-cursors
  if (view->xcursors_shown()) {
    auto &xcursor_list = view->get_xcursorList();
    auto i = xcursor_list.begin();
    bool hovered = false;

    while (i != xcursor_list.end()) {
      const double cursorX =
          xrect.left() + (*i)->value(XCursor::XCur_Y) * xrect.width();
      const double cursorY0 =
          xrect.top() + (*i)->value(XCursor::XCur_X0) * xrect.height();
      const double cursorY1 =
          xrect.top() + (*i)->value(XCursor::XCur_X1) * xrect.height();

      if (!hovered &&
          ((*i)->get_close_rect(xrect).contains(hover) ||
           (*i)->get_map_rect(xrect).contains(hover))) {
        (*i)->paint(p, xrect, XCursor::XCur_All);
        hovered = true;
      } else if (!hovered && xrect.contains(hover)) {
        if (qAbs(cursorX - hover.x()) <= Viewport::HitCursorMargin &&
hover.y() > std::min(cursorY0, cursorY1) &&
hover.y() < std::max(cursorY0, cursorY1)) {
          (*i)->paint(p, xrect, XCursor::XCur_Y);
          hovered = true;
        } else if (qAbs(cursorY0 - hover.y()) <=
                   Viewport::HitCursorMargin) {
          (*i)->paint(p, xrect, XCursor::XCur_X0);
          hovered = true;
        } else if (qAbs(cursorY1 - hover.y()) <=
                   Viewport::HitCursorMargin) {
          (*i)->paint(p, xrect, XCursor::XCur_X1);
          hovered = true;
        } else {
          (*i)->paint(p, xrect, XCursor::XCur_None);
        }
      } else {
        (*i)->paint(p, xrect, XCursor::XCur_None);
      }

      i++;
    }
  }

  // 3. Trigger cursor
  if (view->trig_cursor_shown()) {
    view->get_trig_cursor()->paint(p, xrect, 0, false);
  }

  // 4. Search cursor
  if (view->search_cursor_shown()) {
    const int64_t searchX =
        view->index2pixel(view->get_search_cursor()->index());
    if (xrect.contains(hover.x(), hover.y()) &&
        qAbs(searchX - hover.x()) <= Viewport::HitCursorMargin)
      view->get_search_cursor()->paint(p, xrect, 1, -1);
    else
      view->get_search_cursor()->paint(p, xrect, 0, -1);
  }
}

// ---------------------------------------------------------------------------
// MeasureOverlayPass — renders measurement overlays: logic frequency arrows,
// DSO hover lines, DSO Y-measure, DSO X-measure, logic edge/jump markers.
// ---------------------------------------------------------------------------

bool MeasureOverlayPass::should_run(const RenderContext &ctx) const {
  return ctx.type == TIME_VIEW && ctx.viewport && ctx.view;
}

void MeasureOverlayPass::render(QPainter &p, const RenderContext &ctx) {
  Viewport *vp = ctx.viewport;
  View *view = ctx.view;

  QColor active_color = ctx.back.black() > 0x80 ? View::Orange : View::Purple;
  vp->_hover_hit = false;

  int v_offset = view->get_vOffset();
  int screen_midY = vp->_cur_midY - v_offset;
  int screen_preY = vp->_cur_preY - v_offset;
  int screen_aftY = vp->_cur_aftY - v_offset;
  QPointF screen_hover_point = view->hover_point() - QPointF(0, v_offset);

  // 1. Logic frequency measurement
  if (vp->_action_type == NO_ACTION && vp->_measure_type == LOGIC_FREQ) {
    p.setPen(active_color);
    p.drawLine(QLineF(vp->_cur_preX, screen_midY,
                      vp->_cur_aftX, screen_midY));
    p.drawLine(QLineF(vp->_cur_preX, screen_midY, vp->_cur_preX + 2,
                      screen_midY - 2));
    p.drawLine(QLineF(vp->_cur_preX, screen_midY, vp->_cur_preX + 2,
                      screen_midY + 2));
    p.drawLine(QLineF(vp->_cur_aftX - 2, screen_midY - 2,
                      vp->_cur_aftX, screen_midY));
    p.drawLine(QLineF(vp->_cur_aftX - 2, screen_midY + 2,
                      vp->_cur_aftX, screen_midY));
    if (vp->_thd_sample != 0) {
      p.drawLine(QLineF(vp->_cur_aftX, screen_midY, vp->_cur_thdX,
                        screen_midY));
      p.drawLine(QLineF(vp->_cur_aftX, screen_midY, vp->_cur_aftX + 2,
                        screen_midY - 2));
      p.drawLine(QLineF(vp->_cur_aftX, screen_midY, vp->_cur_aftX + 2,
                        screen_midY + 2));
      p.drawLine(QLineF(vp->_cur_thdX - 2, screen_midY - 2,
                        vp->_cur_thdX, screen_midY));
      p.drawLine(QLineF(vp->_cur_thdX - 2, screen_midY + 2,
                        vp->_cur_thdX, screen_midY));
    }

    if (vp->_measure_en) {
      std::vector<std::pair<QString, QString>> rows = {
          {L_S(STR_PAGE_DLG, S_ID(IDS_DLG_FREQUENCY), "Frequency: "),
           vp->_mm_freq},
          {L_S(STR_PAGE_DLG, S_ID(IDS_DLG_PERIOD), "Period: "),
           vp->_mm_period},
          {L_S(STR_PAGE_DLG, S_ID(IDS_DLG_WIDTH), "Width: "),
           vp->_mm_width},
          {L_S(STR_PAGE_DLG, S_ID(IDS_DLG_DUTY_CYCLE), "Duty Cycle: "),
           vp->_mm_duty}};

      drawFloatingPanel(p, screen_hover_point,
                        view->get_view_width(),
                        view->viewport()->height(), ctx.back,
                        vp->_panelBgColor, vp->_panelTextColor,
                        rows);
    }
  }

  // 2. DSO value hover lines
  if (vp->_action_type == NO_ACTION && vp->_measure_type == DSO_VALUE) {
    for (auto s : view->get_own_signals()) {
      if (s->signal_type() == SR_CHANNEL_DSO) {
        uint64_t index;
        double value;
        QPointF hpoint;
        if (((DsoSignal *)s)->get_hover(index, hpoint, value)) {
          p.setPen(QPen(ctx.fore, 1, Qt::DashLine));
          p.setBrush(Qt::NoBrush);
          p.drawLine(hpoint.x(), s->get_view_rect().top(), hpoint.x(),
                     s->get_view_rect().bottom());
        }
      } else if (s->signal_type() == SR_CHANNEL_ANALOG) {
        uint64_t index;
        double value;
        QPointF hpoint;
        if (((AnalogSignal *)s)->get_hover(index, hpoint, value)) {
          p.setPen(QPen(ctx.fore, 1, Qt::DashLine));
          p.setBrush(Qt::NoBrush);
          p.drawLine(hpoint.x(), s->get_view_rect().top(), hpoint.x(),
                     s->get_view_rect().bottom());
        }
      }
    }
  }

  // 3. DSO Y-measure
  if (vp->_dso_ym_valid) {
    for (auto s : view->get_own_signals()) {
      if (s->signal_type() == SR_CHANNEL_DSO) {
        DsoSignal *dsoSig = (DsoSignal *)s;
        if (dsoSig->get_index() == vp->_dso_ym_sig_index) {
          p.setPen(QPen(dsoSig->get_colour(), 1, Qt::DotLine));
          QFontMetrics fm(p.font());
          const int text_height = fm.height();
          const int64_t x = view->index2pixel(vp->_dso_ym_index);
          p.drawLine(x - 10, vp->_dso_ym_start, x + 10,
                     vp->_dso_ym_start);
          p.drawLine(x, vp->_dso_ym_start, x, vp->_dso_ym_end);
          p.drawLine(0, vp->_dso_ym_end,
                     view->get_view_width(),
                     vp->_dso_ym_end);

          // -- vertical delta value
          double hrate = (vp->_dso_ym_start - vp->_dso_ym_end) *
                         1.0f / view->get_view_height();
          double value = hrate * dsoSig->get_vDialValue() *
                         dsoSig->get_factor() * DS_CONF_DSO_VDIVS;
          QString value_str =
              abs(value) > 1000
                  ? QString::number(value / 1000.0, 'f', 2) + "V"
                  : QString::number(value, 'f', 2) + "mV";
          int value_rect_width = p.boundingRect(
                                      0, 0, INT_MAX, INT_MAX,
                                      Qt::AlignLeft | Qt::AlignVCenter, value_str)
                                      .width();
          p.drawText(QRect(x + 10,
                           abs(vp->_dso_ym_start +
                                vp->_dso_ym_end) / 2,
                           value_rect_width, text_height),
                     value_str);

          // -- start value
          value_str = abs(vp->_dso_ym_sig_value) > 1000
                          ? QString::number(
                                vp->_dso_ym_sig_value / 1000.0, 'f', 2) +
                                "V"
                          : QString::number(vp->_dso_ym_sig_value, 'f',
                                            2) +
                                "mV";
          value_rect_width = p.boundingRect(
                                 0, 0, INT_MAX, INT_MAX,
                                 Qt::AlignLeft | Qt::AlignVCenter, value_str)
                                 .width();
          int str_y = value > 0 ? vp->_dso_ym_start
                                : vp->_dso_ym_start - text_height;
          p.drawText(QRect(x - 0.5 * value_rect_width, str_y,
                           value_rect_width, text_height),
                     value_str);

          // -- end value
          double end_value = vp->_dso_ym_sig_value + value;
          value_str = abs(end_value) > 1000
                          ? QString::number(end_value / 1000.0, 'f', 2) + "V"
                          : QString::number(end_value, 'f', 2) + "mV";
          value_rect_width = p.boundingRect(
                                 0, 0, INT_MAX, INT_MAX,
                                 Qt::AlignLeft | Qt::AlignVCenter, value_str)
                                 .width();
          str_y = value > 0 ? vp->_dso_ym_end - text_height
                            : vp->_dso_ym_end;
          p.drawText(QRect(x - 0.5 * value_rect_width, str_y,
                           value_rect_width, text_height),
                     value_str);
          break;
        }
      }
    }
  }

  // 4. DSO X-measure
  if (vp->_dso_xm_valid) {
    p.setPen(QPen(Qt::red, 1, Qt::DotLine));
    int measure_line_count = 6;
    const int text_height =
        p.boundingRect(0, 0, INT_MAX, INT_MAX, Qt::AlignLeft | Qt::AlignTop,
                       "W")
            .height();
    const uint64_t sample_rate = view->session().cur_snap_samplerate();
    QLineF *line;
    QLineF *const measure_lines = new QLineF[measure_line_count];
    line = measure_lines;
    int64_t x[Viewport::DsoMeasureStages];
    int dso_xm_stage = 0;
    if (vp->_action_type == DSO_XM_STEP1)
      dso_xm_stage = 1;
    else if (vp->_action_type == DSO_XM_STEP2)
      dso_xm_stage = 2;
    else
      dso_xm_stage = 3;

    for (int i = 0; i < dso_xm_stage; i++) {
      x[i] = view->index2pixel(vp->_dso_xm_index[i]);
    }
    measure_line_count = 0;
    if (dso_xm_stage > 0) {
      *line++ = QLine(x[0], vp->_dso_xm_y - 10, x[0],
                      vp->_dso_xm_y + 10);
      measure_line_count += 1;
    }
    if (dso_xm_stage > 1) {
      *line++ = QLine(x[1], vp->_dso_xm_y - 10, x[1],
                      vp->_dso_xm_y + 10);
      *line++ = QLine(x[0], vp->_dso_xm_y, x[1], vp->_dso_xm_y);
      vp->_mm_width = view->get_ruler()->format_real_time(
          vp->_dso_xm_index[1] - vp->_dso_xm_index[0],
          sample_rate);

      // -- width show
      const QString w_ctr = "W=" + vp->_mm_width;
      int w_rect_width = p.boundingRect(
                             0, 0, INT_MAX, INT_MAX,
                             Qt::AlignLeft | Qt::AlignVCenter, w_ctr)
                             .width();
      p.drawText(QRect(x[0] + 10, vp->_dso_xm_y - text_height,
                       w_rect_width, text_height),
                 w_ctr);
      measure_line_count += 2;
    }
    if (dso_xm_stage > 2) {
      *line++ = QLineF(x[0], vp->_dso_xm_y + 20, x[0],
                       vp->_dso_xm_y + 40);
      *line++ = QLineF(x[0], vp->_dso_xm_y + 30, x[2],
                       vp->_dso_xm_y + 30);
      *line++ = QLineF(x[2], vp->_dso_xm_y + 20, x[2],
                       vp->_dso_xm_y + 40);
      vp->_mm_period = view->get_ruler()->format_real_time(
          vp->_dso_xm_index[2] - vp->_dso_xm_index[0],
          sample_rate);
      vp->_mm_freq = view->get_ruler()->format_real_freq(
          vp->_dso_xm_index[2] - vp->_dso_xm_index[0],
          sample_rate);
      vp->_mm_duty =
          QString::number((vp->_dso_xm_index[1] -
                            vp->_dso_xm_index[0]) *
                               100.0 /
                               (vp->_dso_xm_index[2] -
                                vp->_dso_xm_index[0]),
                           'f', 2) +
          "%";

      // -- period show
      const QString p_ctr = "P=" + vp->_mm_period;
      int p_rect_width = p.boundingRect(
                             0, 0, INT_MAX, INT_MAX,
                             Qt::AlignLeft | Qt::AlignVCenter, p_ctr)
                             .width();
      p.drawText(QRect(x[0] + 10, vp->_dso_xm_y + 30 - text_height,
                       p_rect_width, text_height),
                 p_ctr);

      // -- frequency show
      const QString f_ctr = "F=" + vp->_mm_freq;
      int f_rect_width = p.boundingRect(
                             0, 0, INT_MAX, INT_MAX,
                             Qt::AlignLeft | Qt::AlignVCenter, f_ctr)
                             .width();
      p.drawText(QRect(x[0] + 20 + p_rect_width,
                       vp->_dso_xm_y + 30 - text_height, f_rect_width,
                       text_height),
                 f_ctr);

      // -- duty show
      const QString d_ctr = "D=" + vp->_mm_duty;
      int d_rect_width = p.boundingRect(
                             0, 0, INT_MAX, INT_MAX,
                             Qt::AlignLeft | Qt::AlignVCenter, d_ctr)
                             .width();
      p.drawText(QRect(x[1] + 10, vp->_dso_xm_y - 0.5 * text_height,
                       d_rect_width, text_height),
                 d_ctr);

      measure_line_count += 3;
    }
    p.drawLines(measure_lines, measure_line_count);
    if (dso_xm_stage < Viewport::DsoMeasureStages) {
      p.drawLine(x[dso_xm_stage - 1], vp->_dso_xm_y,
                 vp->_mouse_point.x(), vp->_dso_xm_y);
      p.drawLine(vp->_mouse_point.x(), 0,
                 vp->_mouse_point.x(), vp->height());
    }
    delete[] measure_lines;
    vp->measure_updated();
  }

  // 5. Logic edge measurement
  if (vp->_action_type == LOGIC_EDGE &&
      view->session().have_view_data()) {
    p.setPen(active_color);
    p.drawLine(
        QLineF(vp->_cur_preX, screen_midY - 5, vp->_cur_preX,
               screen_midY + 5));
    p.drawLine(
        QLineF(vp->_cur_aftX, screen_midY - 5, vp->_cur_aftX,
               screen_midY + 5));
    p.drawLine(QLineF(vp->_cur_preX, screen_midY, vp->_cur_aftX,
                      screen_midY));

    std::vector<std::pair<QString, QString>> rows = {{"", vp->_em_edges},
                                           {"", vp->_em_rising},
                                           {"", vp->_em_falling}};

    drawFloatingPanel(p, screen_hover_point,
                      view->get_view_width(),
                      view->viewport()->height(), ctx.back,
                      vp->_panelBgColor, vp->_panelTextColor,
                      rows);
  }

  // 6. Logic jump measurement
  if (vp->_action_type == LOGIC_JUMP) {
    p.setPen(active_color);
    p.setBrush(Qt::NoBrush);
    const QPoint pre_points[] = {
        QPoint(vp->_cur_preX, screen_preY),
        QPoint(vp->_cur_preX - 1, screen_preY - 1),
        QPoint(vp->_cur_preX + 1, screen_preY - 1),
        QPoint(vp->_cur_preX - 1, screen_preY + 1),
        QPoint(vp->_cur_preX + 1, screen_preY + 1),
        QPoint(vp->_cur_preX - 2, screen_preY - 2),
        QPoint(vp->_cur_preX + 2, screen_preY - 2),
        QPoint(vp->_cur_preX - 2, screen_preY + 2),
        QPoint(vp->_cur_preX + 2, screen_preY + 2),
    };
    p.drawPoints(pre_points, countof(pre_points));
    if (abs(vp->_cur_aftX - vp->_cur_preX) +
            abs(vp->_cur_aftY - vp->_cur_preY) >
        20) {
      if (vp->_edge_hit) {
        const QPoint aft_points[] = {
            QPoint(vp->_cur_aftX, screen_aftY),
            QPoint(vp->_cur_aftX - 1, screen_aftY - 1),
            QPoint(vp->_cur_aftX + 1, screen_aftY - 1),
            QPoint(vp->_cur_aftX - 1, screen_aftY + 1),
            QPoint(vp->_cur_aftX + 1, screen_aftY + 1),
            QPoint(vp->_cur_aftX - 2, screen_aftY - 2),
            QPoint(vp->_cur_aftX + 2, screen_aftY - 2),
            QPoint(vp->_cur_aftX - 2, screen_aftY + 2),
            QPoint(vp->_cur_aftX + 2, screen_aftY + 2),
        };
        p.drawPoints(aft_points, countof(aft_points));
      }
      int64_t delta = std::max(vp->_edge_start, vp->_edge_end) -
                      std::min(vp->_edge_start, vp->_edge_end);
      QString delta_text =
          view->get_index_delta(vp->_edge_start,
                                vp->_edge_end) +
          "/" + QString::number(delta);

      std::vector<std::pair<QString, QString>> rows = {{"", delta_text}};

      drawFloatingPanel(p, screen_hover_point,
                        view->get_view_width(),
                        view->viewport()->height(), ctx.back,
                        vp->_panelBgColor, vp->_panelTextColor,
                        rows);

      QPainterPath path(QPoint(vp->_cur_preX, screen_preY));
      QPoint c1((vp->_cur_preX + vp->_cur_aftX) / 2,
                screen_preY);
      QPoint c2((vp->_cur_preX + vp->_cur_aftX) / 2,
                screen_aftY);
      path.cubicTo(c1, c2, QPoint(vp->_cur_aftX, screen_aftY));
      p.drawPath(path);
    }
  }
}

// ---------------------------------------------------------------------------
// TriggerInfoPass — renders DSO trigger status text and out-of-range warning.
// ---------------------------------------------------------------------------

bool TriggerInfoPass::should_run(const RenderContext &ctx) const {
  if (ctx.type != TIME_VIEW || !ctx.viewport || !ctx.view)
    return false;
  auto *dev = ctx.view->data_source()->device();
  return ctx.view->get_work_mode() == DSO &&
         ctx.view->session().is_running_status() && dev &&
         dev->is_dsl_device();
}

void TriggerInfoPass::render(QPainter &p, const RenderContext &ctx) {
  Viewport *vp = ctx.viewport;
  View *view = ctx.view;

  auto *dev = view->data_source()->device();
  int type;
  bool roll = false;
  QString type_str = "";
  bool ret = false;

  dev->get_config_bool(SR_CONF_ROLL, roll);

  ret = dev->get_config_byte(SR_CONF_TRIGGER_SOURCE, type);
  if (ret) {
    bool bDot = false;

    if (type == DSO_TRIGGER_AUTO && roll) {
      type_str =
          L_S(STR_PAGE_DLG, S_ID(IDS_DLG_AUTO_ROLL), "Auto(Roll)");

      if (view->session().is_instant()) {
        type_str += ", ";
        type_str += L_S(STR_PAGE_DLG, S_ID(IDS_DLG_VIEW_CAPTURE),
                        "Capturing");
        bDot = true;
      }
    } else if (type == DSO_TRIGGER_AUTO &&
               !view->session().trigd()) {
      type_str = L_S(STR_PAGE_DLG, S_ID(IDS_DLG_AUTO), "Auto");

      if (view->session().is_instant()) {
        type_str += ", ";
        type_str += L_S(STR_PAGE_DLG, S_ID(IDS_DLG_VIEW_CAPTURE),
                        "Capturing");
        bDot = true;
      }
    } else if (vp->_waiting_trig > 0) {
      type_str = L_S(STR_PAGE_DLG, S_ID(IDS_DLG_WAITING_TRIG),
                     "Waiting Trig");
      bDot = true;
    } else {
      type_str = L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIG_D), "Trig'd");
    }

    if (bDot) {
      for (int i = 0; i < vp->_tigger_wait_times; i++) {
        type_str += ".";
      }

      high_resolution_clock::time_point cur_time =
          high_resolution_clock::now();
      milliseconds timeInterval = std::chrono::duration_cast<milliseconds>(
          cur_time - vp->_lst_wait_tigger_time);
      int64_t time_keep = timeInterval.count();

      if (time_keep >= 500) {
        vp->_tigger_wait_times++;
        vp->_lst_wait_tigger_time = cur_time;
      }

      if (vp->_tigger_wait_times > 4)
        vp->_tigger_wait_times = 0;
    }
  }
  p.setPen(ctx.fore);
  p.drawText(view->get_view_rect(),
             Qt::AlignLeft | Qt::AlignTop, type_str);

  if (dev->is_hardware()) {
    if (view->session().dso_data_is_out_off_range()) {
      QString data_status = L_S(STR_PAGE_DLG,
                                S_ID(IDS_DLG_DATA_OUT_OFF_RANGE),
                                "Out off range");
      data_status += "! ";
      QColor warnRed = AppConfig::Instance().GetThemeColor("@warn-red");
      if (!warnRed.isValid())
        warnRed = QColor(255, 0, 0, 200);
      p.setPen(warnRed);
      p.drawText(view->get_view_rect(),
                 Qt::AlignRight | Qt::AlignTop, data_status);
      p.setPen(ctx.fore);
    }
  }
}

} // namespace view
} // namespace pv
