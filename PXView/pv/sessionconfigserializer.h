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

#ifndef PXVIEW_PV_SESSIONCONFIGSERIALIZER_H
#define PXVIEW_PV_SESSIONCONFIGSERIALIZER_H

#include <QJsonObject>
#include <QString>

namespace pv {

class SigSession;
class MainWindow;

/**
 * SessionConfigSerializer — extracts .pxc session config serialization
 * orchestration from MainWindow (purify-architecture-concepts Task 20).
 *
 * Owns the gen_config_json / load_config_from_json / save_config_to_file
 * logic. MainWindow holds a unique_ptr<SessionConfigSerializer> and forwards
 * calls. The serializer is constructed with pointers to MainWindow (for
 * accessing current_view / current_context / device_agent / trigger_widget /
 * sampling_bar / protocol_widget) and SigSession.
 *
 * This is a pure mechanical migration: method bodies are unchanged except
 * for the this->member access being rewritten to _main_window->accessor() /
 * _session->. No behaviour change.
 */
class SessionConfigSerializer
{
public:
    SessionConfigSerializer(MainWindow *main_window, SigSession *session);
    ~SessionConfigSerializer();

    // Generate the top-level JSON config object from current session state.
    // Returns false if the device exposes no SR_CONF_DEVICE_SESSIONS list.
    bool gen_config_json(QJsonObject &sessionVar);

    // Load session config from a JSON document. Sets haveDecoder=true when the
    // document contained a non-empty "decoder" segment. Returns true on success.
    bool load_config_from_json(QJsonDocument &doc, bool &haveDecoder);

    // Save current config to a .pxc file. Returns true on success.
    bool save_config_to_file(QString name);

private:
    MainWindow *_main_window;
    SigSession *_session;
};

} // namespace pv

#endif // PXVIEW_PV_SESSIONCONFIGSERIALIZER_H
