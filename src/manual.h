// ViviaMeals – Handbuch öffnen (Hilfe → Handbuch, F1)
//   Windows:       ViviaMeals-Handbuch.chm (Windows-Hilfe, hh.exe); ersatzweise HTML bzw. PDF
//   Linux, macOS:  html/index.html im Standard-Browser; ersatzweise PDF
// Gesucht wird im Ordner „handbuch“ neben dem Programm, in <prefix>/share/viviameals/handbuch (Linux),
// in Contents/Resources/handbuch (macOS-Bundle), im Quellbaum (docs/handbuch, Entwicklung) und in
// der Umgebungsvariablen VIVIA_HANDBUCH. Erzeugt wird das Handbuch mit tools/make_handbuch.py.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QString>
#include <QStringList>

class QWidget;

namespace Manual {

QStringList searchDirs();
// bevorzugte Datei für dieses System (leer = kein Handbuch gefunden)
QString file();
// öffnet das Handbuch; false = nicht gefunden (Meldung wurde angezeigt)
bool open(QWidget *parent);

} // namespace Manual
