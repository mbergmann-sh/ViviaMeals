// ViviaMeals – Bilder vom Scanner holen
//
// Windows: Scandialog des Systems über WIA (wiascan_win.cpp) – funktioniert mit jedem Scanner bzw.
//          Multifunktionsgerät, für das ein WIA-Treiber installiert ist (Standard bei Windows 10/11).
// Linux:   ein Scanprogramm wird gestartet. XSane und scanimage (SANE, ohne Oberfläche) speichern
//          direkt in eine von ViviaMeals vorgegebene Datei, die automatisch übernommen wird; bei
//          GNOME Dokumentenscanner (simple-scan), Skanpage und Skanlite (KDE) speichert man selbst
//          und öffnet das Bild danach mit „Bild öffnen …“.
// macOS:   „Digitale Bilder“ (Image Capture) bzw. Vorschau („Ablage → Von Scanner importieren“) –
//          danach „Bild öffnen …“.
// Überall: ein eigener Befehl mit dem Platzhalter %f für die Zieldatei.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

class QWidget;

namespace Scanner {

struct Program {
    QString id;            // "wia", "xsane", "scanimage", "simple-scan", "skanpage", "skanlite", "imagecapture", "preview", "custom"
    QString name;          // Anzeige
    QString program;       // ausführbare Datei
    QStringList args;      // %f = Zieldatei
    bool writesFile = false;   // legt das Bild selbst unter %f ab → automatische Übernahme
    bool toStdout = false;     // Bild kommt über stdout (scanimage)
    bool native = false;       // Windows-Scandialog
    QString hint;          // Hinweis nach dem Start
};

// Programme dieser Plattform (auch nicht installierte, mit installed())
QList<Program> known();
bool installed(const Program &p);
// erstes installiertes Programm der Plattform
QString defaultId();
// Einstellung „scan/program“ (bzw. eigener Befehl „scan/command“)
Program current();
Program byId(const QString &id);
// eigener Befehl: Programm und Argumente aus einer Befehlszeile („naps2.console -o %f“)
Program fromCommand(const QString &command);

enum class Status { Ok, Cancelled, Failed };
// Windows-Scandialog (blockiert, bis gescannt oder abgebrochen ist)
Status acquireNative(const QString &file, bool color, QString *error);

// Zieldatei für automatische Übernahme
QString targetFile();

} // namespace Scanner
