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

// Phase E (modernize-view-layer-v2): cursor / xcursor behaviour extracted
// from the View God-class. ViewCursors is declared a friend of View so it
// can touch the private cursor state (_logic_cursors / _dso_cursors /
// _trig_cursor / _search_cursor / _show_cursors / _show_trig_cursor /
// _show_search_cursor / _search_pos / _search_hit / _xcursorList /
// _show_xcursors) and emit the protected Qt signals cursor_update() /
// xcursor_update(). Cross-method calls that remain on View (e.g.
// set_scale_offset, get_view_width, document_snapshot_source) go through
// _view->… so the public View API is unchanged.

#include "view_cursors.h"

#include <assert.h>
#include <list>

#include <QPalette>
#include <QWidget>

#include "view.h"
#include "ruler.h"

#include "../config/appconfig.h"
#include "../data/datasource.h"
#include "../data/signalmodel.h"
#include "../deviceagent.h"
#include "../sigsession.h"

using namespace std;

namespace pv {
namespace view {

void ViewCursors::show_cursors(bool show) {
  _view->_show_cursors = show;
  _view->_ruler->update();
  _view->viewport_update();
}

void ViewCursors::show_trig_cursor(bool show) {
  _view->_show_trig_cursor = show;
  _view->_ruler->update();
  _view->viewport_update();
}

void ViewCursors::show_search_cursor(bool show) {
  _view->_show_search_cursor = show;
  _view->_ruler->update();
  _view->viewport_update();
}

void ViewCursors::set_trig_cursor_posistion(uint64_t trig_pos) {
  const double time =
      trig_pos * 1.0 / _view->document_snapshot_source()->cur_snap_samplerate();
  _view->_trig_cursor->set_index(trig_pos);

  int width = _view->get_view_width();
  assert(width > 0);

  // B2 fix: query Core trigger state instead of ds_trigger_get_en().
  // Trigger is enabled if any logic channel has a non-NONTRIG trig_type
  // (Simple mode), or if trigger_config mode is Adv/Serial (always enabled).
  bool trigger_enabled = false;
  const auto &trig_cfg = _view->_data_source->trigger_config();
  if (trig_cfg.mode() != pv::data::TriggerConfig::Simple) {
    trigger_enabled = true;
  } else {
    for (const auto &m : _view->_data_source->get_signal_models()) {
      if (m && m->type() == SR_CHANNEL_LOGIC &&
          m->trig_type() != pv::data::SignalModel::NONTRIG) {
        trigger_enabled = true;
        break;
      }
    }
  }
  if (trigger_enabled || _view->_device_agent->is_virtual() ||
      _view->get_work_mode() == DSO) {
    _view->_show_trig_cursor = true;

    AppConfig &app = AppConfig::Instance();
    if (app.appOptions.trigPosDisplayInMid) {
      _view->set_scale_offset(_view->_scale, (time / _view->_scale) - (width / 2));
    }
  }

  _view->_ruler->update();
  _view->viewport_update();
}

void ViewCursors::set_search_pos(uint64_t search_pos, bool hit) {
  QColor fore(_view->palette().color(_view->foregroundRole()));
  fore.setAlpha(View::BackAlpha);

  const double time =
      search_pos * 1.0 / _view->document_snapshot_source()->cur_snap_samplerate();
  _view->_search_pos = search_pos;
  _view->_search_hit = hit;
  _view->_search_cursor->set_index(search_pos);
  _view->_search_cursor->set_colour(hit ? View::Blue : fore);

  int width = _view->get_view_width();
  assert(width);

  if (hit) {
    _view->set_scale_offset(_view->_scale, (time / _view->_scale) - (width / 2));
    _view->_ruler->update();
    _view->viewport_update();
  }
}

std::list<Cursor *> &ViewCursors::get_cursorList() {
  if (_view->_device_agent->get_work_mode() == LOGIC) {
    return _view->_logic_cursors;
  } else {
    return _view->_dso_cursors;
  }
}

Cursor *ViewCursors::get_cursor_by_index(int index) {
  int dex = 0;
  auto &cursors = get_cursorList();

  for (auto c : cursors) {
    if (dex == index) {
      return c;
    }
    dex++;
  }
  return NULL;
}

void ViewCursors::make_cursors_order() {
  int dex = 1;

  for (auto cursor : get_cursorList()) {
    cursor->set_order(dex++);
  }

  dex = 1;
  for (auto cursor : _view->get_xcursorList()) {
    cursor->set_order(dex++);
  }
}

void ViewCursors::add_cursor(QColor color, uint64_t sampleIndex) {
  (void)color;
  Cursor *newCursor = new Cursor(*_view, -1, sampleIndex);
  get_cursorList().push_back(newCursor);
  make_cursors_order();
  _view->cursor_update();
}

void ViewCursors::add_cursor(uint64_t sampleIndex) {
  static int lastOrder = 1;
  Cursor *newCursor = new Cursor(*_view, lastOrder++, sampleIndex);
  get_cursorList().push_back(newCursor);
  make_cursors_order();
  _view->cursor_update();
}

void ViewCursors::del_cursor(Cursor *cursor) {
  assert(cursor);

  get_cursorList().remove(cursor);
  delete cursor;
  make_cursors_order();

  _view->cursor_update();
}

void ViewCursors::clear_cursors() {
  auto &lst = get_cursorList();
  for (auto c : lst) {
    delete c;
  }

  lst.clear();
}

void ViewCursors::set_cursor_middle(int index) {
  auto &lst = get_cursorList();
  int size = lst.size();
  (void)size;
  assert(index < size);

  int width = _view->get_view_width();

  auto i = lst.begin();

  while (index-- != 0) {
    i++;
  }

  _view->set_scale_offset(
      _view->_scale,
      (*i)->index() /
          (_view->document_snapshot_source()->cur_snap_samplerate() *
           _view->_scale) -
          (width / 2));
}

void ViewCursors::add_xcursor(double value0, double value1) {
  static int lastXCursorOrder = 1;
  XCursor *newXCursor = new XCursor(*_view, lastXCursorOrder++, value0, value1);
  _view->_xcursorList.push_back(newXCursor);
  make_cursors_order();
  _view->xcursor_update();
}

void ViewCursors::del_xcursor(XCursor *xcursor) {
  assert(xcursor);

  _view->_xcursorList.remove(xcursor);
  delete xcursor;
  make_cursors_order();
  _view->xcursor_update();
}

uint64_t ViewCursors::get_cursor_samples(int index) {
  auto &lst = get_cursorList();
  assert(index < (int)lst.size());

  uint64_t ret = 0;
  int curIndex = 0;
  for (list<Cursor *>::iterator i = lst.begin(); i != lst.end(); i++) {
    if (index == curIndex) {
      ret = (*i)->index();
    }
    curIndex++;
  }
  return ret;
}

QString ViewCursors::get_cm_time(int index) {
  uint64_t sampleIndex = get_cursor_samples(index);
  uint64_t sampleRate = _view->document_snapshot_source()->cur_snap_samplerate();
  return _view->_ruler->format_real_time(sampleIndex, sampleRate);
}

QString ViewCursors::get_cm_delta(int index1, int index2) {
  if (index1 == index2)
    return "0";

  uint64_t samples1 = get_cursor_samples(index1);
  uint64_t samples2 = get_cursor_samples(index2);
  uint64_t delta_sample =
      (samples1 > samples2) ? samples1 - samples2 : samples2 - samples1;
  return _view->_ruler->format_real_time(
      delta_sample, _view->document_snapshot_source()->cur_snap_samplerate());
}

int ViewCursors::get_cursor_index_by_key(uint64_t key) {
  auto &lst = get_cursorList();

  int dex = 0;
  for (auto c : lst) {
    if (c->get_key() == key) {
      return dex;
    }
    ++dex;
  }
  return -1;
}

} // namespace view
} // namespace pv
