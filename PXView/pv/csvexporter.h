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

#ifndef PXVIEW_PV_CSVEXPORTER_H
#define PXVIEW_PV_CSVEXPORTER_H

#include <QString>
#include <QList>

#include <libsigrok.h>

namespace pv {

class StoreSession;
class SigSession;

namespace data {
class Snapshot;
}

/**
 * CsvExporter — CSV/file-format export concern.
 *
 * Extracted from StoreSession (Task 21, purify-architecture-concepts spec).
 * Drives the libsigrok sr_output_module pipeline (csv/vcd/...) against a
 * snapshot, plus the GUI path for picking an export file name and listing
 * supported formats.
 *
 * Like PxcSerializer, this class holds a non-owning pointer back to its
 * parent StoreSession and accesses shared state (file name, suffix, export
 * channel filter, progress counters, cancel flag, ...) through StoreSession's
 * private interface via friendship — pure mechanical move, no behavior change.
 */
class CsvExporter
{
public:
    explicit CsvExporter(StoreSession *store);

    ~CsvExporter();

    // ---- public entry points (delegated from StoreSession) ----

    /** Kick off CSV/file-format export: resolve sr_output_module by suffix,
     *  then spawn export_proc on StoreSession's worker thread. */
    bool export_start();

    /** Prompt (or synthesize) the export file name. bDlg=true shows
     *  QFileDialog with the supported-format filter list; bDlg=false builds
     *  the name from history. */
    QString MakeExportFile(bool bDlg);

    /** List supported export formats (described by sr_output_module list),
     *  filtered by the current device work mode. */
    QList<QString> getSuportedExportFormats();

private:
    // ---- internal helpers (migrated verbatim from StoreSession) ----

    void export_proc(pv::data::Snapshot *snapshot);
    void export_exec(pv::data::Snapshot *snapshot);

private:
    StoreSession *_store;
};

} // pv

#endif // PXVIEW_PV_CSVEXPORTER_H
