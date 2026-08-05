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

#ifndef PXVIEW_PV_VIEW_VIEW_DERIVED_TRACES_H
#define PXVIEW_PV_VIEW_VIEW_DERIVED_TRACES_H

#include <cstdint>
#include <list>
#include <memory>
#include <vector>
#include <QPoint>

struct srd_decoder;
class DecoderStatus;

namespace pv {
namespace data {
class DecoderStack;
namespace decode {
class Decoder;
} // namespace decode
} // namespace data

namespace view {

class View;
class DecodeTrace;
class SpectrumTrace;
class MathTrace;
class LissajousTrace;

// ViewDerivedTraces — delegate for View's decoder / spectrum / math /
// lissajous derived-trace responsibilities. Extracted from the View
// God-class during Phase E of the modernize-view-layer-v2 spec. All
// derived-trace state (_own_decode_traces / _own_spectrum_traces /
// _own_math_trace / _own_lissajous_trace / _derived_traces_dirty) lives
// in this class. View declares `friend class ViewDerivedTraces;` so the
// delegate can access View's private widget members and call private
// helper methods (e.g. mark_derived_traces_dirty, signals_changed).
class ViewDerivedTraces {
public:
  explicit ViewDerivedTraces(View *view) : _view(view) {}

  // -- decoder lifecycle -------------------------------------------------
  bool add_decoder(srd_decoder *const dec, bool silent, DecoderStatus *dstatus,
                   std::list<pv::data::decode::Decoder *> &sub_decoders,
                   std::shared_ptr<pv::data::DecoderStack> &out_stack);
  bool rst_decoder_by_key_handel(void *handel, QPoint anchor = QPoint());
  void remove_decoder(DecodeTrace *trace);
  void remove_decoder(int index);
  void remove_decoder_by_key_handel(void *key_handel);
  void clear_all_decoders();

  // -- lazy sync / dirty flag -------------------------------------------
  void sync_derived_traces();
  void mark_derived_traces_dirty();

  std::vector<DecodeTrace *> &own_decode_traces() { return _own_decode_traces; }
  std::vector<SpectrumTrace *> &own_spectrum_traces() { return _own_spectrum_traces; }
  MathTrace *own_math_trace() { return _own_math_trace; }
  LissajousTrace *own_lissajous_trace() { return _own_lissajous_trace; }
  bool derived_traces_dirty() const { return _derived_traces_dirty; }
  void set_derived_traces_dirty(bool v) { _derived_traces_dirty = v; }

  void set_own_math_trace(MathTrace *t) { _own_math_trace = t; }
  void set_own_lissajous_trace(LissajousTrace *t) { _own_lissajous_trace = t; }

  // -- cleanup (called by View destructor) ------------------------------
  // Deletes all View-owned wrapper traces and clears the lists.
  // This centralizes cleanup so View does not need friend access.
  void cleanup();

private:
  View *_view;

  std::vector<DecodeTrace *> _own_decode_traces;
  std::vector<SpectrumTrace *> _own_spectrum_traces;
  MathTrace *_own_math_trace = nullptr;
  LissajousTrace *_own_lissajous_trace = nullptr;
  bool _derived_traces_dirty = true;
};

} // namespace view
} // namespace pv

#endif // PXVIEW_PV_VIEW_VIEW_DERIVED_TRACES_H
