/*
 * This file is part of the PulseView project.
 * PXView is based on PulseView.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
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

#ifndef PXVIEW_PV_PXCSERIALIZER_H
#define PXVIEW_PV_PXCSERIALIZER_H

#include <QString>
#include <QList>
#include <QJsonArray>

#include <libsigrok.h>

#include "ZipMaker.h"

namespace pv {

class StoreSession;
class SigSession;

namespace data {
class Snapshot;
class LogicSnapshot;
class AnalogSnapshot;
class DsoSnapshot;
}

namespace dock {
class ProtocolDock;
}

/**
 * PxcSerializer — .pxc/.pxl file serialization concern.
 *
 * Extracted from StoreSession (Task 21, purify-architecture-concepts spec).
 * Owns the .pxc ZIP container construction (header/decoders/session segments
 * + per-channel sample chunks) and the decoder-stack JSON serialization
 * (gen_decoders_json / load_decoders, which are also reused by
 * MainWindow::gen_config_json for the inline "decoder" segment).
 *
 * The class holds a non-owning pointer back to its parent StoreSession
 * (the coordinator) and accesses shared state (file name, progress counters,
 * ZipMaker, cancel flag, ...) through StoreSession's private interface via
 * friendship. This keeps the migration a pure mechanical move — no behavior
 * change, no new public accessors leaked on StoreSession.
 */
class PxcSerializer
{
public:
    explicit PxcSerializer(StoreSession *store);

    ~PxcSerializer();

    // ---- public entry points (delegated from StoreSession) ----

    /** Kick off the .pxc save: build header/decoders/session segments, then
     *  spawn save_proc on StoreSession's worker thread. */
    bool save_start();

    /** Serialize all decoder stacks into a JSON array. Also used by
     *  MainWindow::gen_config_json for the top-level "decoder" segment. */
    bool gen_decoders_json(QJsonArray &array);

    /** Load decoder stacks from a JSON array (paired with gen_decoders_json).
     *  Used by MainWindow::load_config_from_json / demo loader. */
    bool load_decoders(dock::ProtocolDock *widget, QJsonArray &dec_array);

    /** Prompt (or synthesize) the .pxl save file name. bDlg=true shows
     *  QFileDialog; bDlg=false builds the name from history without UI. */
    QString MakeSaveFile(bool bDlg);

private:
    // ---- internal helpers (migrated verbatim from StoreSession) ----

    void save_proc(pv::data::Snapshot *snapshot);
    void save_logic(pv::data::LogicSnapshot *logic_snapshot);
    void save_analog(pv::data::AnalogSnapshot *analog_snapshot);
    void save_dso(pv::data::DsoSnapshot *dso_snapshot);
    bool meta_gen(data::Snapshot *snapshot, std::string &str);
    bool decoders_gen(std::string &str);
    double get_integer(GVariant *var);
    void MakeChunkName(char *chunk_name, int chunk_num, int index,
                       int type, int version);

private:
    StoreSession *_store;
};

} // pv

#endif // PXVIEW_PV_PXCSERIALIZER_H
