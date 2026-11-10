// ViviaMeals – Küchenmaßeinheiten (deutsch, englisch/US, MealMaster-Kürzel, RezKonv-Kürzel)
//
// Alle Einheiten werden auf eine deutsche Küchenmaßeinheit abgebildet. Englische Maße werden beim
// Import umgerechnet: Cups → ml bzw. g (über die Dichte der Zutat), Unzen/Pfund → g, Flüssigunzen,
// Pints, Quarts → ml, tablespoon → EL, teaspoon → TL, pinch → Prise …
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QString>

namespace Units {

enum class Kind { None, Mass, Volume, Piece, Vague };

struct Unit {
    QString de;            // kanonische deutsche Einheit, leer = ohne Einheit (Stückzahl)
    Kind kind = Kind::None;
    double base = 0;       // g (Mass) bzw. ml (Volume) je Einheit; bei englischen Einheiten der Umrechnungsfaktor
    bool english = false;  // englisches Maß, muss umgerechnet werden
    QString note;          // Zusatz, z. B. „groß“ bei MealMaster „lg“
};

// Einheit erkennen. caseSensitive: MealMaster unterscheidet „t“ (TL) und „T“ (EL).
// Gibt false zurück, wenn token keine bekannte Einheit ist.
bool lookup(const QString &token, Unit *out);

// Volumen bzw. Gewicht einer deutschen Einheit (ml bzw. g), 0 = unbekannt
double mlPer(const QString &deUnit);
double gramsPer(const QString &deUnit);
Kind kindOf(const QString &deUnit);

// Typisches Gewicht einer Packungs-/Stückeinheit ohne Angabe zur Zutat (Dose, Becher …), 0 = unbekannt
double defaultPieceGrams(const QString &deUnit);

// Alle deutschen Einheiten für Auswahllisten/Completer
QStringList germanUnits();

// Temperaturen und Maße in englischen Texten umrechnen: „350 °F“ → „175 °C“, „9x13 inch“ → „23 × 33 cm“
QString convertTextMeasures(const QString &text);

} // namespace Units
