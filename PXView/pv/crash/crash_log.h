/*
 * This file is part of the PXView project.
 *
 * Crash log format contract — shared between crash_handler.cpp (writer,
 * signal-safe context) and crash_reporter.cpp (reader, normal Qt context).
 *
 * This header is dependency-free (no Qt, no windows.h) so it can be included
 * from both the Win32-only handler and the Qt-side reporter without pulling
 * in unwanted symbols.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PXVIEW_PV_CRASH_CRASH_LOG_H
#define PXVIEW_PV_CRASH_CRASH_LOG_H

#include <cstddef>

namespace pv {
namespace crash {

// Fixed crash log filename (overwrites with the latest crash; next launch
// only needs to check one well-known path). Full path is assembled at
// install time as <TempLocation>/<CRASH_LOG_FILENAME>.
constexpr const char* CRASH_LOG_FILENAME = "pxview_crash_last.txt";

// Magic first line of the crash log — lets the reporter sanity-check the file
// before parsing.
constexpr const char* CRASH_LOG_MAGIC = "PXVIEW_CRASH_LOG_V1";

// CaptureStackBackTrace hard upper bound on Windows is 64 frames.
constexpr int MAX_FRAMES = 64;

// Windows MAX_PATH without pulling in windows.h (keeps this header clean).
constexpr int CRASH_PATH_MAX = 260;

// Pre-computed at install time (normal context), read-only inside the crash
// handler (signal-safe context). All strings are UTF-8, NUL-terminated.
// Using fixed char arrays instead of QString/std::string because the handler
// cannot touch the heap.
struct CrashLogContext
{
    char log_path[CRASH_PATH_MAX];  // <TempLocation>/pxview_crash_last.txt
    char exe_path[CRASH_PATH_MAX];  // GetModuleFileNameW(NULL), UTF-8
    void* exe_base;                 // GetModuleHandleW(NULL), main module base
};

} // namespace crash
} // namespace pv

#endif // PXVIEW_PV_CRASH_CRASH_LOG_H
