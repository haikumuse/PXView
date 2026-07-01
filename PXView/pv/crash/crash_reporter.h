/*
 * This file is part of the PXView project.
 *
 * Crash reporter — runs on next launch (normal Qt context). Detects the
 * crash log written by crash_handler.cpp, symbolicates the saved addresses
 * via bin/addr2line.exe, and shows a DSDialog with the full stack trace.
 *
 * Windows-only (mirrors crash_handler). The reporter itself is Qt-based and
 * may be called from the GUI thread only.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PXVIEW_PV_CRASH_CRASH_REPORTER_H
#define PXVIEW_PV_CRASH_CRASH_REPORTER_H

class QWidget;

namespace pv {
namespace crash {

// Called from main.cpp (GUI branch) after the main window is shown. If a
// crash log from a previous run is present, parses it, symbolicates the
// frames with addr2line.exe, shows a modal report dialog, and deletes the
// log on close. No-op (returns false) when no crash log exists.
// parent is the main window (for dialog modality).
bool show_crash_report_if_exists(QWidget *parent);

} // namespace crash
} // namespace pv

#endif // PXVIEW_PV_CRASH_CRASH_REPORTER_H
