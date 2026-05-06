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
 * Foundation, Inc., 51 Franklin St, Boston, MA  02110-1301 USA
 */

#ifndef DSVIEW_PV_TABCONTEXT_H
#define DSVIEW_PV_TABCONTEXT_H

#include <QString>
#include <QDateTime>
#include <stdint.h>

namespace pv {

namespace view {
class View;
}

namespace data {
class SessionSnapshot;
class DataSource;
}

class SigSession;

class TabContext
{
public:
    TabContext(view::View *view, SigSession *session);
    ~TabContext();

    inline view::View* view() { return _view; }
    inline data::SessionSnapshot* snapshot() { return _snapshot; }
    inline QString title() const { return _title; }
    inline QString file_path() const { return _file_path; }
    inline bool is_live() const { return _is_live; }
    inline bool has_data() const { return _has_data; }
    inline QDateTime timestamp() const { return _timestamp; }

    inline void set_title(const QString &title) { _title = title; }
    inline void set_file_path(const QString &path) { _file_path = path; }

    void activate();
    void deactivate();
    void make_live();

    data::DataSource* get_data_source();

    static int _next_session_id;

private:
    view::View          *_view;
    SigSession          *_session;
    data::SessionSnapshot *_snapshot;
    QString             _title;
    QString             _file_path;
    bool                _is_live;
    bool                _has_data;
    QDateTime           _timestamp;
};

} // namespace pv

#endif
