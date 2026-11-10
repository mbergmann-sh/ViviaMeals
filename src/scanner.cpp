// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "scanner.h"
#ifdef Q_OS_WIN
#include "wiascan_win.h"
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

#include <string>

namespace Scanner {

namespace {

Program esclProgram()
{
    Program p;
    p.id = QStringLiteral("escl");
    p.name = QObject::tr("Netzwerkscanner (eSCL / AirScan / Mopria) über die IP-Adresse");
    p.network = true;
    return p;
}

Program folderProgram()
{
    Program p;
    p.id = QStringLiteral("folder");
    p.name = QObject::tr("Programm des Herstellers + Ordner überwachen (z. B. Epson ScanSmart)");
    p.watchFolder = true;
    p.hint = QObject::tr("Scannen Sie mit dem Programm des Herstellers und speichern Sie das Bild (JPEG oder PNG) im überwachten "
                         "Ordner – ViviaMeals übernimmt es automatisch.");
    return p;
}

} // namespace

QList<Program> known()
{
    QList<Program> l;
#if defined(Q_OS_WIN)
    Program wia;
    wia.id = QStringLiteral("wia");
    wia.name = QObject::tr("Windows-Scandialog (WIA)");
    wia.native = true;
    l << wia << esclProgram() << folderProgram();
    Program wfs;
    wfs.id = QStringLiteral("wfs");
    wfs.name = QObject::tr("Windows-Fax und -Scan");
    wfs.program = QStringLiteral("WFS.exe");
    wfs.hint = QObject::tr("Scannen Sie das Rezept in „Windows-Fax und -Scan“, speichern Sie es als Bild und öffnen Sie es "
                           "anschließend mit „Bild öffnen …“.");
    l << wfs;
#elif defined(Q_OS_MACOS)
    Program ic;
    ic.id = QStringLiteral("imagecapture");
    ic.name = QObject::tr("Digitale Bilder (Image Capture)");
    ic.program = QStringLiteral("open");
    ic.args = QStringList{ QStringLiteral("-a"), QStringLiteral("Image Capture") };
    ic.hint = QObject::tr("Scannen Sie das Rezept in „Digitale Bilder“ (Format PNG oder JPEG) und öffnen Sie die Datei "
                          "anschließend mit „Bild öffnen …“.");
    l << ic << esclProgram();
    Program pv;
    pv.id = QStringLiteral("preview");
    pv.name = QObject::tr("Vorschau (Ablage → Von Scanner importieren)");
    pv.program = QStringLiteral("open");
    pv.args = QStringList{ QStringLiteral("-a"), QStringLiteral("Preview") };
    pv.hint = QObject::tr("In „Vorschau“: Ablage → Von „Scanner“ importieren. Das Bild sichern und anschließend mit "
                          "„Bild öffnen …“ laden.");
    l << pv << folderProgram();
#else
    Program xs;
    xs.id = QStringLiteral("xsane");
    xs.name = QObject::tr("XSane");
    xs.program = QStringLiteral("xsane");
    // -s Speichern-Modus, -n keine Modusauswahl, -N feste Zieldatei
    xs.args = QStringList{ QStringLiteral("--save"), QStringLiteral("--no-mode-selection"), QStringLiteral("--force-filename"), QStringLiteral("%f") };
    xs.writesFile = true;
    xs.hint = QObject::tr("XSane ist gestartet. Vorschau holen, Bereich wählen und „Scannen“ – das Bild wird automatisch "
                          "übernommen. Für weitere Seiten einfach erneut scannen.");
    l << xs;
    Program si;
    si.id = QStringLiteral("scanimage");
    si.name = QObject::tr("scanimage (SANE, ohne Oberfläche, erster Scanner)");
    si.program = QStringLiteral("scanimage");
    si.args = QStringList{ QStringLiteral("--format=png"), QStringLiteral("--mode"), QStringLiteral("Color"), QStringLiteral("--resolution"),
                           QStringLiteral("300") };
    si.toStdout = true;
    si.hint = QObject::tr("Scanne mit dem ersten gefundenen Scanner (300 dpi, Farbe) …");
    l << si << esclProgram();
    Program ss;
    ss.id = QStringLiteral("simple-scan");
    ss.name = QObject::tr("Dokumentenscanner (GNOME, simple-scan)");
    ss.program = QStringLiteral("simple-scan");
    ss.hint = QObject::tr("Scannen Sie das Rezept im Dokumentenscanner, speichern Sie es als PNG oder JPEG und öffnen Sie es "
                          "anschließend mit „Bild öffnen …“.");
    l << ss;
    Program sp;
    sp.id = QStringLiteral("skanpage");
    sp.name = QObject::tr("Skanpage (KDE)");
    sp.program = QStringLiteral("skanpage");
    sp.hint = QObject::tr("Scannen Sie das Rezept in Skanpage, speichern Sie es als PNG oder JPEG und öffnen Sie es "
                          "anschließend mit „Bild öffnen …“.");
    l << sp;
    Program sl;
    sl.id = QStringLiteral("skanlite");
    sl.name = QObject::tr("Skanlite (KDE)");
    sl.program = QStringLiteral("skanlite");
    sl.hint = sp.hint;
    sl.hint.replace(QStringLiteral("Skanpage"), QStringLiteral("Skanlite"));
    l << sl << folderProgram();
#endif
    Program c;
    c.id = QStringLiteral("custom");
    c.name = QObject::tr("Eigener Befehl …");
    l << c;
    return l;
}

bool installed(const Program &p)
{
    if (p.native) {
#ifdef Q_OS_WIN
        return WiaScan::available();
#else
        return false;
#endif
    }
    if (p.id == QLatin1String("custom") || p.network || p.watchFolder) return true;
    if (p.program.isEmpty()) return false;
#ifdef Q_OS_MACOS
    if (p.program == QLatin1String("open")) return true;
#endif
#ifdef Q_OS_WIN
    if (p.id == QLatin1String("wfs"))
        return QFileInfo::exists(QDir(QString::fromLocal8Bit(qgetenv("SystemRoot"))).filePath(QStringLiteral("System32/WFS.exe")));
#endif
    return !QStandardPaths::findExecutable(p.program).isEmpty();
}

QString defaultId()
{
    for (const Program &p : known())
        if (p.id != QLatin1String("custom") && installed(p)) return p.id;
    return known().first().id;
}

Program byId(const QString &id)
{
    for (const Program &p : known())
        if (p.id == id) return p;
    return byId(defaultId());
}

Program fromCommand(const QString &command)
{
    Program p;
    p.id = QStringLiteral("custom");
    p.name = QObject::tr("Eigener Befehl");
    QStringList parts = QProcess::splitCommand(command);
    if (parts.isEmpty()) return p;
    p.program = parts.takeFirst();
    p.args = parts;
    p.writesFile = parts.contains(QStringLiteral("%f")) || command.contains(QStringLiteral("%f"));
    p.hint = p.writesFile ? QObject::tr("Der Scanbefehl läuft – das Bild wird automatisch übernommen, sobald es gespeichert ist.")
                          : QObject::tr("Das Scanprogramm ist gestartet. Das gespeicherte Bild anschließend mit „Bild öffnen …“ laden.");
    return p;
}

Program current()
{
    QSettings s;
    const QString id = s.value(QStringLiteral("scan/program"), defaultId()).toString();
    if (id == QLatin1String("custom")) return fromCommand(s.value(QStringLiteral("scan/command")).toString());
    return byId(id);
}

Status acquireNative(const QString &file, bool color, QString *error)
{
#ifdef Q_OS_WIN
    wchar_t err[512];
    const std::wstring path = QDir::toNativeSeparators(file).toStdWString();
    switch (WiaScan::acquire(path.c_str(), color, err, 512)) {
    case WiaScan::Result::Ok: return Status::Ok;
    case WiaScan::Result::Cancelled: return Status::Cancelled;
    default:
        if (error) *error = QString::fromWCharArray(err);
        return Status::Failed;
    }
#else
    Q_UNUSED(file)
    Q_UNUSED(color)
    if (error) *error = QObject::tr("Der Windows-Scandialog steht nur unter Windows zur Verfügung.");
    return Status::Failed;
#endif
}

QString targetFile()
{
    return QDir(QDir::tempPath()).filePath(QStringLiteral("viviameals-scan-%1.png").arg(QCoreApplication::applicationPid()));
}

} // namespace Scanner
