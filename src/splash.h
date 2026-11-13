// ViviaMeals – Startbildschirm mit Ladefortschritt
// Zeigt beim Programmstart, was gerade geladen wird (Einstellungen, Oberfläche, Nährwerttabelle,
// Rezeptdatenbank mit „Rezepte werden eingelesen: 120 von 450“ …). Abschaltbar unter
// Extras → Einstellungen → Allgemein → Programmstart (QSettings ui/splash) und mit --no-splash.
// Alle Funktionen tun nichts, wenn kein Startbildschirm angezeigt wird.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QString>

class QWidget;

namespace Splash {

bool enabledInSettings();
void start(bool force = false);                      // anzeigen (nur wenn eingeschaltet bzw. force)
void message(const QString &text, int percent);      // Stand 0–100 %
void finish(QWidget *mainWindow);                    // ausblenden, sobald das Hauptfenster da ist
bool active();
QWidget *widget();                                   // für Bildschirmfotos

} // namespace Splash
