// VitaMeals – Schriftgröße automatisch an den Bildschirm anpassen
//
// „Automatisch“: auf kleinen Bildschirmen (freie Höhe < 800 px, z. B. 1366 × 768) etwas kleiner,
// auf großen (≥ 1400 px) etwas größer. Zusätzlich frei wählbar (90 … 140 %) unter
// Extras → Einstellungen. Wirkt auf die Programmschrift und die Rezeptansicht.
// SPDX-License-Identifier: MIT AND LicenseRef-Commons-Clause-1.0 (siehe LICENSE)
#pragma once

#include <QList>
#include <QPair>
#include <QString>

class QApplication;

namespace UiScale {
void apply(QApplication &app);       // beim Start und nach Änderung
double factor();                     // wirksamer Faktor
int setting();                       // 0 = automatisch, sonst Prozent
void setSetting(int percent);
QList<QPair<int, QString>> choices();
}
