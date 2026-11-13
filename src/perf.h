// ViviaMeals – einfache Zeitmessung für Start und Ladevorgänge
// Mit der Option --profile oder der Umgebungsvariablen VIVIA_PROFILE=1 schreibt Perf::mark() die seit
// Programmstart vergangene Zeit und die Dauer seit der letzten Marke nach stderr und zusätzlich in die
// Datei logFile() (Windows-Programme haben keine Konsole); sonst kostet ein Aufruf praktisch nichts.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QString>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Perf {

inline bool &enabledFlag()
{
    static bool on = std::getenv("VIVIA_PROFILE") != nullptr;
    return on;
}

inline bool enabled() { return enabledFlag(); }

// Protokolldatei im Temp-Ordner (vor QApplication nutzbar: liest TEMP/TMP/TMPDIR selbst)
inline QString logFile()
{
    static const QString path = [] {
        QString dir;
        for (const char *v : { "TEMP", "TMP", "TMPDIR" }) {
            if (const char *e = std::getenv(v); e && *e) { dir = QString::fromLocal8Bit(e); break; }
        }
        if (dir.isEmpty()) dir = QStringLiteral("/tmp");
        return QDir::cleanPath(dir + QStringLiteral("/ViviaMeals-Startzeiten.txt"));
    }();
    return path;
}

inline std::FILE *&logHandle()
{
    static std::FILE *f = nullptr;
    return f;
}

// aus main() vor QApplication aufrufen (argv nach --profile durchsuchen)
inline void init(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--profile") == 0) enabledFlag() = true;
    if (!enabledFlag()) return;
#ifdef Q_OS_WIN
    logHandle() = _wfopen(reinterpret_cast<const wchar_t *>(logFile().utf16()), L"w");
#else
    logHandle() = std::fopen(QFile::encodeName(logFile()).constData(), "w");
#endif
}

inline void finish()
{
    if (logHandle()) {
        std::fclose(logHandle());
        logHandle() = nullptr;
    }
}

inline QElapsedTimer &clock()
{
    static QElapsedTimer t = [] {
        QElapsedTimer e;
        e.start();
        return e;
    }();
    return t;
}

inline void mark(const char *what)
{
    if (!enabled()) return;
    static qint64 last = 0;
    const qint64 now = clock().nsecsElapsed() / 1000;
    std::fprintf(stderr, "[perf] %8.1f ms  (+%7.1f ms)  %s\n", now / 1000.0, (now - last) / 1000.0, what);
    std::fflush(stderr);
    if (std::FILE *f = logHandle()) {
        std::fprintf(f, "%8.1f ms  (+%7.1f ms)  %s\n", now / 1000.0, (now - last) / 1000.0, what);
        std::fflush(f);
    }
    last = now;
}

} // namespace Perf
