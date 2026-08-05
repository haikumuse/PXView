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

#include <QPainter>
#include <QPainterPath>
#include <algorithm>

namespace pv {
namespace view {

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
// SignalPixmapPass — skeleton, delegates to ViewportPainter's existing
// cached-pixmap logic. Full migration deferred to avoid behavioral risk.
// ---------------------------------------------------------------------------

void SignalPixmapPass::render(QPainter &p, const RenderContext &ctx) {
  // Blit the cached signal pixmap if one exists.
  // The actual pixmap rebuild is currently handled by ViewportPainter::paintSignals.
  // This pass will be wired in once paintSignals is fully decomposed.
  (void)p;
  (void)ctx;
  // TODO: migrate pixmap rebuild + blit logic here
}

// ---------------------------------------------------------------------------
// DecodeTracePass — skeleton
// ---------------------------------------------------------------------------

bool DecodeTracePass::should_run(const RenderContext &ctx) const {
  if (!ctx.traces)
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
// CursorOverlayPass — skeleton
// ---------------------------------------------------------------------------

void CursorOverlayPass::render(QPainter &p, const RenderContext &ctx) {
  // Cursor overlay rendering is currently handled by
  // ViewportPainter::paintCursors. This pass will be wired in
  // once paintCursors is migrated to the pipeline.
  (void)p;
  (void)ctx;
  // TODO: migrate cursor overlay rendering here
}

} // namespace view
} // namespace pv
