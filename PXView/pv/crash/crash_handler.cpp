/*
 * This file is part of the PXView project.
 *
 * Implementation of the crash handler. See crash_handler.h for the design.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "crash_handler.h"
#include "crash_log.h"

#ifdef _WIN32

#include <windows.h>

#include <QStandardPaths>
#include <QString>
#include <QDir>

#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <io.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

namespace pv {
namespace crash {

// Pre-computed context filled in install_crash_handler() (normal context),
// read-only inside the signal-safe handlers.
static CrashLogContext g_ctx;
static LPTOP_LEVEL_EXCEPTION_FILTER g_prev_filter = nullptr;
static bool g_installed = false;

// Custom codes for non-SEH crash sources. High bit set; chosen outside the
// 0xC0xxxxxx NT-status range Windows uses so they cannot collide.
constexpr DWORD CRASH_PURECALL      = 0xE0000001;
constexpr DWORD CRASH_INVALID_PARAM = 0xE0000002;
constexpr DWORD CRASH_SIGABRT       = 0xE0000003;

static const char *exception_code_name(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:         return "EXCEPTION_ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_BREAKPOINT:               return "EXCEPTION_BREAKPOINT";
    case EXCEPTION_DATATYPE_MISALIGNMENT:    return "EXCEPTION_DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DENORMAL_OPERAND:     return "EXCEPTION_FLT_DENORMAL_OPERAND";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INEXACT_RESULT:       return "EXCEPTION_FLT_INEXACT_RESULT";
    case EXCEPTION_FLT_INVALID_OPERATION:    return "EXCEPTION_FLT_INVALID_OPERATION";
    case EXCEPTION_FLT_OVERFLOW:             return "EXCEPTION_FLT_OVERFLOW";
    case EXCEPTION_FLT_STACK_CHECK:          return "EXCEPTION_FLT_STACK_CHECK";
    case EXCEPTION_FLT_UNDERFLOW:            return "EXCEPTION_FLT_UNDERFLOW";
    case EXCEPTION_ILLEGAL_INSTRUCTION:      return "EXCEPTION_ILLEGAL_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR:            return "EXCEPTION_IN_PAGE_ERROR";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "EXCEPTION_INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:             return "EXCEPTION_INT_OVERFLOW";
    case EXCEPTION_INVALID_DISPOSITION:      return "EXCEPTION_INVALID_DISPOSITION";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
    case EXCEPTION_PRIV_INSTRUCTION:         return "EXCEPTION_PRIV_INSTRUCTION";
    case EXCEPTION_SINGLE_STEP:              return "EXCEPTION_SINGLE_STEP";
    case EXCEPTION_STACK_OVERFLOW:           return "EXCEPTION_STACK_OVERFLOW";
    case CRASH_PURECALL:                     return "PURE_VIRTUAL_CALL";
    case CRASH_INVALID_PARAM:                return "INVALID_PARAMETER";
    case CRASH_SIGABRT:                      return "SIGABRT";
    default:                                 return "UNKNOWN";
    }
}

// vsnprintf into a fixed stack buffer, tracking the write offset. C99
// vsnprintf always NUL-terminates and returns the number of chars that
// *would* have been written (without the NUL). We clamp on truncation.
static void append_buf(char *buf, int &off, int cap, const char *fmt, ...)
{
    if (off >= cap - 1)
        return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + off, cap - off, fmt, ap);
    va_end(ap);
    if (n < 0) {
        buf[cap - 1] = '\0';
        return;
    }
    if (n >= cap - off)
        n = cap - off - 1;  // truncated
    off += n;
}

// Signal-safe: stack buffer + _open/_write (unbuffered syscalls), no malloc.
static void write_crash_log(const CrashLogContext *ctx,
                            DWORD ex_code, ULONG_PTR ex_addr,
                            void *const *frames, int frame_count)
{
    SYSTEMTIME st;
    GetLocalTime(&st);

    char buf[8192];
    int off = 0;
    const int cap = (int)sizeof(buf);

    append_buf(buf, off, cap, "%s\n", CRASH_LOG_MAGIC);
    append_buf(buf, off, cap, "timestamp=%04d-%02d-%02d %02d:%02d:%02d\n",
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    append_buf(buf, off, cap, "exception_code=0x%08X\n", (unsigned int)ex_code);
    append_buf(buf, off, cap, "exception_code_name=%s\n", exception_code_name(ex_code));
    append_buf(buf, off, cap, "exception_address=0x%016llX\n", (unsigned long long)ex_addr);
    append_buf(buf, off, cap, "exe_path=%s\n", ctx->exe_path);
    append_buf(buf, off, cap, "exe_base=0x%016llX\n", (unsigned long long)(uintptr_t)ctx->exe_base);
    append_buf(buf, off, cap, "frame_count=%d\n", frame_count);
    append_buf(buf, off, cap, "[frames]\n");
    for (int i = 0; i < frame_count; ++i) {
        append_buf(buf, off, cap, "0x%016llX\n", (unsigned long long)(uintptr_t)frames[i]);
    }
    append_buf(buf, off, cap, "[end_frames]\n");

    int fd = _open(ctx->log_path, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY,
                   _S_IREAD | _S_IWRITE);
    if (fd < 0)
        return;
    _write(fd, buf, off);
    _close(fd);
}

// Win32 MessageBoxA — independent message loop, no Qt, no process heap.
static void show_crash_message_box(DWORD ex_code, ULONG_PTR ex_addr, const char *log_path)
{
    char msg[1024];
    int off = 0;
    const int cap = (int)sizeof(msg);
    append_buf(msg, off, cap,
        "PXView encountered a fatal error and must close.\r\n\r\n"
        "Exception: %s (0x%08X)\r\n"
        "Address: 0x%016llX\r\n\r\n"
        "A detailed crash log has been saved to:\r\n%s\r\n\r\n"
        "The full symbolicated stack trace will be shown on next launch.",
        exception_code_name(ex_code), (unsigned int)ex_code,
        (unsigned long long)ex_addr, log_path);
    MessageBoxA(NULL, msg, "PXView Crash", MB_OK | MB_ICONERROR);
}

// Common path: capture stack, write log, message box, terminate.
// __declspec(noreturn) so the compiler does not generate fall-through code
// after the _exit() in release builds.
static __declspec(noreturn) void handle_crash(DWORD ex_code, ULONG_PTR ex_addr)
{
    void *frames[MAX_FRAMES];
    // RtlCaptureStackBackTrace is in kernel32 (no dbghelp dependency),
    // stack-only, signal-safe.
    int n = (int)RtlCaptureStackBackTrace(0, MAX_FRAMES, frames, NULL);
    write_crash_log(&g_ctx, ex_code, ex_addr, frames, n);
    show_crash_message_box(ex_code, ex_addr, g_ctx.log_path);
    // _exit (not exit) — skip atexit/C++ static DTORs that may re-crash.
    _exit(1);
}

static LONG WINAPI crash_exception_filter(EXCEPTION_POINTERS *ep)
{
    if (IsDebuggerPresent()) {
        // During development let the debugger take the breakpoint.
        return g_prev_filter ? g_prev_filter(ep) : EXCEPTION_CONTINUE_SEARCH;
    }
    DWORD code = ep ? ep->ExceptionRecord->ExceptionCode : 0;
    ULONG_PTR addr = ep ? (ULONG_PTR)ep->ExceptionRecord->ExceptionAddress : 0;
    handle_crash(code, addr);
}

static void __cdecl crash_purecall_handler()
{
    if (IsDebuggerPresent()) DebugBreak();
    handle_crash(CRASH_PURECALL, 0);
}

static void __cdecl crash_invalid_param_handler(const wchar_t *, const wchar_t *,
                                                const wchar_t *, unsigned int,
                                                uintptr_t)
{
    if (IsDebuggerPresent()) DebugBreak();
    handle_crash(CRASH_INVALID_PARAM, 0);
}

static void __cdecl crash_sigabrt_handler(int)
{
    if (IsDebuggerPresent()) DebugBreak();
    handle_crash(CRASH_SIGABRT, 0);
}

void install_crash_handler()
{
    if (g_installed)
        return;

    // ---- Compute log path (Qt context, handler cannot use QString) ----
    QString temp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString log_path = temp + QDir::separator() + QString::fromLatin1(CRASH_LOG_FILENAME);
    QByteArray log_utf8 = log_path.toUtf8();

    // ---- exe path + base ----
    wchar_t wexe[CRASH_PATH_MAX];
    DWORD wlen = GetModuleFileNameW(NULL, wexe, CRASH_PATH_MAX);
    char exe_utf8[CRASH_PATH_MAX];
    int exe_len = WideCharToMultiByte(CP_UTF8, 0, wexe, wlen,
                                      exe_utf8, CRASH_PATH_MAX, NULL, NULL);
    if (exe_len < 0) exe_len = 0;
    exe_utf8[exe_len] = '\0';
    // Normalize backslashes → forward slashes for addr2line friendliness.
    for (int i = 0; i < exe_len; ++i)
        if (exe_utf8[i] == '\\') exe_utf8[i] = '/';

    // ---- Fill fixed context ----
    memset(&g_ctx, 0, sizeof(g_ctx));
    strncpy(g_ctx.log_path, log_utf8.constData(), CRASH_PATH_MAX - 1);
    strncpy(g_ctx.exe_path, exe_utf8, CRASH_PATH_MAX - 1);
    g_ctx.exe_base = GetModuleHandleW(NULL);

    // ---- Register handlers ----
    // SetUnhandledExceptionFilter is the main line of defense — Win32 layer,
    // not affected by Python's CRT signal() calls during AppControl::Init().
    g_prev_filter = SetUnhandledExceptionFilter(crash_exception_filter);
    _set_purecall_handler(crash_purecall_handler);
    _set_invalid_parameter_handler(crash_invalid_param_handler);
    signal(SIGABRT, crash_sigabrt_handler);

    g_installed = true;
}

} // namespace crash
} // namespace pv

#endif // _WIN32
