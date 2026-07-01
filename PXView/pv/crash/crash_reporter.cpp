/*
 * This file is part of the PXView project.
 *
 * Implementation of the crash reporter. See crash_reporter.h for the design.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "crash_reporter.h"
#include "crash_log.h"

#ifdef _WIN32

#include <QCoreApplication>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include "../dialogs/dsdialog.h"

namespace pv {
namespace crash {

namespace {

// ---- Parsed crash log ----

struct ParsedCrashLog
{
    bool valid = false;
    QString timestamp;
    QString ex_code_name;
    quint64 ex_code = 0;
    quint64 ex_addr = 0;
    QString exe_path;
    quint64 exe_base = 0;
    QList<quint64> frames;
};

ParsedCrashLog parse_crash_log(const QString &path)
{
    ParsedCrashLog r;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return r;

    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    QString first = in.readLine();
    if (first.trimmed() != QString::fromLatin1(CRASH_LOG_MAGIC))
        return r;

    bool in_frames = false;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line == QLatin1String("[frames]")) { in_frames = true; continue; }
        if (line == QLatin1String("[end_frames]")) { in_frames = false; continue; }

        if (in_frames) {
            bool ok = false;
            quint64 a = line.trimmed().toULongLong(&ok, 16);
            if (ok) r.frames.append(a);
            continue;
        }

        int eq = line.indexOf('=');
        if (eq < 0) continue;
        QString key = line.left(eq);
        QString val = line.mid(eq + 1);

        if (key == QLatin1String("timestamp"))          r.timestamp = val;
        else if (key == QLatin1String("exception_code_name")) r.ex_code_name = val;
        else if (key == QLatin1String("exception_code"))     r.ex_code = val.toULongLong(nullptr, 16);
        else if (key == QLatin1String("exception_address"))  r.ex_addr = val.toULongLong(nullptr, 16);
        else if (key == QLatin1String("exe_path"))           r.exe_path = val;
        else if (key == QLatin1String("exe_base"))           r.exe_base = val.toULongLong(nullptr, 16);
    }

    r.valid = !r.frames.isEmpty();
    return r;
}

// ---- addr2line discovery ----

QString locate_addr2line()
{
    // 1. Beside the executable (install dir / bin).
    QString candidate = QCoreApplication::applicationDirPath() + QLatin1String("/addr2line.exe");
    if (QFileInfo::exists(candidate))
        return candidate;
    // 2. Development environment fallback (MSYS2 mingw64).
    candidate = QLatin1String("D:/msys64/mingw64/bin/addr2line.exe");
    if (QFileInfo::exists(candidate))
        return candidate;
    return QString();
}

// ---- Symbolication ----

// One QProcess call for all in-module frames. addr2line outputs 2 lines per
// address (function name, then file:line). Returns parallel lists.
struct SymbolicatedFrame
{
    quint64 addr = 0;
    QString function;   // "??"
    QString location;   // "file:line" or "??"
    bool external = false;
};

QList<SymbolicatedFrame> symbolicate(const QString &addr2line_exe,
                                     const QString &target_exe,
                                     quint64 exe_base,
                                     const QList<quint64> &frames)
{
    QList<SymbolicatedFrame> out;
    out.reserve(frames.size());

    // Partition: in-module frames go to addr2line; external frames are marked.
    QList<int> in_module_idx;
    QStringList args;
    args << QLatin1String("-e") << target_exe
         << QLatin1String("-f") << QLatin1String("-C");

    // 256 MB upper bound for the main module image — PXView.exe is well under
    // that, and addr2line just returns "??" for any stray out-of-range addr.
    const quint64 module_size_guess = 0x10000000ULL;
    for (int i = 0; i < frames.size(); ++i) {
        SymbolicatedFrame sf;
        sf.addr = frames[i];
        if (frames[i] >= exe_base && frames[i] < exe_base + module_size_guess) {
            in_module_idx.append(i);
            args << (QLatin1String("0x") + QString::number(frames[i], 16).toUpper());
        } else {
            sf.external = true;
        }
        out.append(sf);
    }

    if (in_module_idx.isEmpty() || addr2line_exe.isEmpty())
        return out;

    QProcess proc;
    proc.start(addr2line_exe, args);
    if (!proc.waitForFinished(3000)) {
        proc.kill();
        return out;
    }
    QString outstr = QString::fromLocal8Bit(proc.readAllStandardOutput());
    QStringList lines = outstr.split('\n', Qt::SkipEmptyParts);

    // Each address produces exactly 2 lines with -f -C (no -i).
    for (int k = 0; k < in_module_idx.size(); ++k) {
        int li = k * 2;
        if (li + 1 >= lines.size()) break;
        SymbolicatedFrame &sf = out[in_module_idx[k]];
        sf.function = lines[li].trimmed();
        sf.location = lines[li + 1].trimmed();
    }

    // Fallback: if every in-module frame came back "??", the exe was likely
    // relocated (PIE/ASLR). Retry with offset = addr - exe_base.
    bool all_unknown = true;
    for (int idx : in_module_idx) {
        if (out[idx].function != QLatin1String("??") ||
            out[idx].location != QLatin1String("??")) {
            all_unknown = false;
            break;
        }
    }
    if (all_unknown && exe_base != 0) {
        QStringList args2;
        args2 << QLatin1String("-e") << target_exe
              << QLatin1String("-f") << QLatin1String("-C");
        for (int idx : in_module_idx)
            args2 << (QLatin1String("0x") + QString::number(frames[idx] - exe_base, 16).toUpper());

        QProcess p2;
        p2.start(addr2line_exe, args2);
        if (p2.waitForFinished(3000)) {
            QString s2 = QString::fromLocal8Bit(p2.readAllStandardOutput());
            QStringList l2 = s2.split('\n', Qt::SkipEmptyParts);
            for (int k = 0; k < in_module_idx.size(); ++k) {
                int li = k * 2;
                if (li + 1 >= l2.size()) break;
                out[in_module_idx[k]].function = l2[li].trimmed();
                out[in_module_idx[k]].location = l2[li + 1].trimmed();
            }
        }
    }

    return out;
}

// ---- Report dialog ----

class CrashReportDialog : public pv::dialogs::DSDialog
{
public:
    CrashReportDialog(const ParsedCrashLog &log, const QList<SymbolicatedFrame> &frames, QWidget *parent)
        : pv::dialogs::DSDialog(parent, true)
        , m_log(log)
        , m_frames(frames)
    {
        setTitle(QLatin1String("PXView Crash Report"));
        build_ui();
    }

private:
    void build_ui()
    {
        QVBoxLayout *root = layout();

        // ---- Summary label ----
        QString summary;
        QTextStream ts(&summary);
        ts << "PXView crashed during the previous run.\n"
           << "Time: " << m_log.timestamp << "\n"
           << "Exception: " << m_log.ex_code_name
           << " (0x" << QString::number(m_log.ex_code, 16).toUpper() << ")\n"
           << "Address: 0x" << QString::number(m_log.ex_addr, 16).toUpper();
        QLabel *lbl = new QLabel(summary);
        lbl->setWordWrap(true);
        root->addWidget(lbl);

        // ---- Stack trace view ----
        m_view = new QPlainTextEdit;
        m_view->setReadOnly(true);
        QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        m_view->setFont(mono);
        m_view->setPlainText(format_trace());
        root->addWidget(m_view);

        // ---- Button row ----
        QHBoxLayout *btns = new QHBoxLayout;
        btns->addStretch(1);

        QPushButton *btn_copy = new QPushButton(QLatin1String("Copy"));
        connect(btn_copy, &QPushButton::clicked, this, [this]() {
            m_view->selectAll();
            // Copy both the summary and the trace.
            QString full = m_view->toPlainText();
            QGuiApplication::clipboard()->setText(full);
        });
        btns->addWidget(btn_copy);

        QPushButton *btn_folder = new QPushButton(QLatin1String("Open log folder"));
        connect(btn_folder, &QPushButton::clicked, this, [this]() {
            QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        });
        btns->addWidget(btn_folder);

        QPushButton *btn_close = new QPushButton(QLatin1String("Close"));
        connect(btn_close, &QPushButton::clicked, this, &QDialog::accept);
        btns->addWidget(btn_close);

        root->addLayout(btns);
    }

    QString format_trace() const
    {
        QString text;
        QTextStream ts(&text);
        for (int i = 0; i < m_frames.size(); ++i) {
            const SymbolicatedFrame &sf = m_frames[i];
            ts << "#" << i << "  ";
            if (sf.external) {
                ts << "[external: 0x"
                   << QString::number(sf.addr, 16).toUpper() << "]";
            } else if (sf.function == QLatin1String("??") &&
                       sf.location == QLatin1String("??")) {
                ts << "[unknown: 0x"
                   << QString::number(sf.addr, 16).toUpper() << "]";
            } else {
                ts << sf.function << "  " << sf.location;
            }
            ts << "\n";
        }
        return text;
    }

    const ParsedCrashLog &m_log;
    const QList<SymbolicatedFrame> &m_frames;
    QPlainTextEdit *m_view = nullptr;
};

} // anonymous namespace

// ---- Public entry ----

bool show_crash_report_if_exists(QWidget *parent)
{
    QString temp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString log_path = temp + QLatin1String("/") + QLatin1String(CRASH_LOG_FILENAME);

    if (!QFileInfo::exists(log_path))
        return false;

    ParsedCrashLog log = parse_crash_log(log_path);
    if (!log.valid) {
        // Corrupt or partial — remove so we don't loop.
        QFile::remove(log_path);
        return false;
    }

    QString addr2line = locate_addr2line();
    QList<SymbolicatedFrame> frames = symbolicate(addr2line, log.exe_path,
                                                  log.exe_base, log.frames);

    CrashReportDialog dlg(log, frames, parent);
    dlg.resize(720, 520);
    dlg.exec();

    // Delete the log so the dialog does not reappear on subsequent launches.
    QFile::remove(log_path);
    return true;
}

} // namespace crash
} // namespace pv

#endif // _WIN32
