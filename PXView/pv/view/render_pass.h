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

#ifndef PXVIEW_PV_VIEW_RENDER_PASS_H
#define PXVIEW_PV_VIEW_RENDER_PASS_H

#include <QColor>
#include <QRect>
#include <cstdint>
#include <vector>

class QPainter;

namespace pv {
namespace view {

class Viewport;
class Trace;
class Signal;
struct SignalGroup;

/**
 * @brief Context passed to each RenderPass during a paint cycle.
 *
 * Contains all the state needed by rendering strategies: the view
 * being painted (with access to configuration), the viewport
 * widget, the traces to render, current scale/offset, colors,
 * and cached geometry.
 */
struct RenderContext {
  class View *view = nullptr;        // The owning View (provides config access)
  Viewport *viewport = nullptr;     // The viewport widget being painted
  int type = 0;            // TIME_VIEW or FFT_VIEW
  double scale = 1.0;
  int64_t offset = 0;
  int signalHeight = 0;
  int vOffset = 0;
  QColor fore;
  QColor back;
  std::vector<Trace *> *traces = nullptr;
  const std::vector<SignalGroup> *groups = nullptr;
  int viewWidth = 0;
  bool is_logic_mode = false;
};

/**
 * @brief Abstract rendering strategy interface.
 *
 * Phase 5 refactoring: each RenderPass is responsible for one layer
 * of the viewport painting pipeline (e.g. group card backgrounds,
 * logic signals, DSO signals, decode traces, cursor overlays,
 * measurement overlays, progress indicators).
 *
 * Passes are executed in z_order() order (ascending). Lower values
 * are painted first (background), higher values painted later
 * (foreground overlays).
 */
class RenderPass {
public:
  virtual ~RenderPass() = default;

  /** Render this pass. Called with an active QPainter on the viewport. */
  virtual void render(QPainter &p, const RenderContext &ctx) = 0;

  /** Z-order for this pass. Lower = painted first (background). */
  virtual int z_order() const = 0;

  /** Whether this pass should execute for the given context.
   *  Default: always true. Passes can override to skip when
   *  e.g. no traces of their type exist. */
  virtual bool should_run(const RenderContext &ctx) const {
    (void)ctx;
    return true;
  }
};

/**
 * @brief Manages an ordered pipeline of RenderPass instances.
 *
 * The pipeline sorts passes by z_order on each render call and
 * executes them in sequence. ViewportPainter can use this to
 * decompose its monolithic doPaint() into composable strategies.
 */
class RenderPipeline {
public:
  /** Add a pass to the pipeline. Takes ownership. */
  void add_pass(std::unique_ptr<RenderPass> pass);

  /** Execute all passes whose should_run() returns true, in
   *  z_order ascending sequence. */
  void render(QPainter &p, const RenderContext &ctx);

  /** Remove all passes. */
  void clear();

private:
  std::vector<std::unique_ptr<RenderPass>> _passes;
};

// ---- Concrete passes ----

/**
 * Renders the rounded-rectangle card backgrounds for signal groups.
 * Painted first (lowest z_order) so all signal content appears
 * above the card.
 */
class GroupCardBackgroundPass : public RenderPass {
public:
  void render(QPainter &p, const RenderContext &ctx) override;
  int z_order() const override { return 10; }
  bool should_run(const RenderContext &ctx) const override;
};

/**
 * Renders the cached pixmap containing logic signal waveforms and
 * non-decoder traces. Uses double-buffered QPixmap rendering for
 * performance.
 */
class SignalPixmapPass : public RenderPass {
public:
  void render(QPainter &p, const RenderContext &ctx) override;
  int z_order() const override { return 20; }
  bool should_run(const RenderContext &ctx) const override;
};

/**
 * Renders decode traces directly on the widget (not via QPixmap)
 * to ensure crisp text rendering.
 */
class DecodeTracePass : public RenderPass {
public:
  void render(QPainter &p, const RenderContext &ctx) override;
  int z_order() const override { return 30; }
  bool should_run(const RenderContext &ctx) const override;
};

/**
 * Renders cursor markers and measurement overlays on top of
 * all signal content.
 */
class CursorOverlayPass : public RenderPass {
public:
  void render(QPainter &p, const RenderContext &ctx) override;
  int z_order() const override { return 40; }
  bool should_run(const RenderContext &ctx) const override;
};

} // namespace view
} // namespace pv

#endif // PXVIEW_PV_VIEW_RENDER_PASS_H
