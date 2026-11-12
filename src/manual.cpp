// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "manual.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QUrl>

namespace Manual {

namespace {
const QString kChm = QStringLiteral("ViviaMeals-Handbuch.chm");
const QString kPdf = QStringLiteral("ViviaMeals-Handbuch.pdf");
const QString kHtml = QStringLiteral("html/index.html");
}

QStringList searchDirs()
{
    QStringList dirs;
    const QString env = qEnvironmentVariable("VIVIA_HANDBUCH");
    if (!env.isEmpty()) dirs << env;
    const QDir app(QCoreApplication::applicationDirPath());
    dirs << app.filePath(QStringLiteral("handbuch"))
         << app.filePath(QStringLiteral("../share/viviameals/handbuch"))   // Linux: make install
         << app.filePath(QStringLiteral("../Resources/handbuch"))          // macOS-Bundle
         << app.filePath(QStringLiteral("../handbuch"));                   // Windows: Build-Ordner release/debug
#ifdef VIVIAMEALS_SOURCE_DIR
    dirs << QStringLiteral(VIVIAMEALS_SOURCE_DIR "/docs/handbuch");          // Entwicklung: Quellbaum
#endif
    QStringList out;
    for (const QString &d : dirs) {
        const QString c = QDir::cleanPath(d);
        if (!out.contains(c)) out << c;
    }
    return out;
}

QString file()
{
#ifdef Q_OS_WIN
    const QStringList order{ kChm, kHtml, kPdf };
#else
    const QStringList order{ kHtml, kPdf };
#endif
    // erst die bevorzugte Form in allen Ordnern suchen, dann die nächste
    for (const QString &name : order)
        for (const QString &d : searchDirs()) {
            const QString f = QDir(d).filePath(name);
            if (QFileInfo::exists(f)) return QFileInfo(f).absoluteFilePath();
        }
    return QString();
}

bool open(QWidget *parent)
{
    const QString f = file();
    if (f.isEmpty()) {
        QStringList shown;
        for (const QString &d : searchDirs()) shown << QDir::toNativeSeparators(d);
        QMessageBox::information(parent, QCoreApplication::translate("Manual", "Handbuch"),
                                 QCoreApplication::translate("Manual", "Das Handbuch wurde nicht gefunden. Gesucht in:\n\n%1\n\n"
                                                                       "Bitte ViviaMeals neu installieren bzw. das Handbuch mit "
                                                                       "„python3 tools/make_handbuch.py“ erzeugen.")
                                     .arg(shown.join(QLatin1Char('\n'))));
        return false;
    }
#ifdef Q_OS_WIN
    if (f.endsWith(QLatin1String(".chm"), Qt::CaseInsensitive)) {
        // Windows-Hilfe direkt mit hh.exe öffnen; sonst über die Dateizuordnung
        if (QProcess::startDetached(QStringLiteral("hh.exe"), { QDir::toNativeSeparators(f) })) return true;
    }
#endif
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(f))) return true;
    QMessageBox::warning(parent, QCoreApplication::translate("Manual", "Handbuch"),
                         QCoreApplication::translate("Manual", "Das Handbuch konnte nicht geöffnet werden:\n%1").arg(QDir::toNativeSeparators(f)));
    return false;
}

} // namespace Manual
