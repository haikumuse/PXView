/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
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

#include "tabcontext.h"
#include "sigsession.h"
#include "view/view.h"
#include "data/sessionsnapshot.h"
#include "data/datasource.h"

namespace pv {

int TabContext::_next_session_id = 1;

TabContext::TabContext(view::View *view, SigSession *session) :
    _view(view),
    _session(session),
    _snapshot(nullptr),
    _title(QString("Session %1").arg(_next_session_id)),
    _file_path(""),
    _is_live(true),
    _timestamp(QDateTime::currentDateTime())
{
    _next_session_id++;
}

TabContext::~TabContext()
{
    if (_snapshot)
        delete _snapshot;
}

void TabContext::activate()
{
    if (_is_live) {
        _view->set_data_source(_session);
    } else if (_snapshot) {
        _view->set_data_source(_snapshot);
    } else {
        _view->set_data_source(_session);
    }
}

void TabContext::deactivate()
{
    if (_is_live && _session->have_view_data()) {
        if (_snapshot)
            delete _snapshot;
        _snapshot = _session->capture_snapshot();
    }
}

data::DataSource* TabContext::get_data_source()
{
    if (_is_live)
        return _session;
    else if (_snapshot)
        return _snapshot;
    else
        return _session;
}

void TabContext::capture_snapshot()
{
    if (_snapshot)
        delete _snapshot;
    _snapshot = _session->capture_snapshot();
    _is_live = false;
}

void TabContext::make_live()
{
    _is_live = true;
}

} // namespace pv
