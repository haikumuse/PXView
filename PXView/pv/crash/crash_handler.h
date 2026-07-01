/*
 * This file is part of the PXView project.
 *
 * Crash handler — installs Win32/CRT exception hooks that capture a stack
 * backtrace, write a plain-text crash log to %TEMP%, show a Win32 message
 * box (game-style immediate feedback), and terminate the process.
 *
 * Signal-safety: the handler runs in a damaged-process context. It MUST NOT
 * touch Qt, the heap, or CRT buffered I/O. Only CaptureStackBackTrace,
 * _open/_write, vsnprintf (stack buffer), MessageBoxA and _exit are used.
 *
 * Windows-only. Call install_crash_handler() once from the GUI branch of
 * main.cpp, after pxv_log_init() and before AppControl::Init().
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PXVIEW_PV_CRASH_CRASH_HANDLER_H
#define PXVIEW_PV_CRASH_CRASH_HANDLER_H

namespace pv {
namespace crash {

// Install the Win32 unhandled-exception filter plus CRT purecall/invalid-
// parameter/SIGABRT handlers. Idempotent. GUI mode only — the headless
// branch is intentionally not instrumented (caller-managed service process).
void install_crash_handler();

} // namespace crash
} // namespace pv

#endif // PXVIEW_PV_CRASH_CRASH_HANDLER_H
