// VitaMeals – Nährwertanalyse eines Rezepts
//
// Für jede Zutat: Lebensmittel zuordnen (manuell festgelegt > gelernt > automatisch), Menge in
// Gramm umrechnen (Gewicht direkt, Volumen über die Dichte, Stück/Packungen über Stückgewichte),
// Nährwerte aufsummieren; Ergebnis gesamt und je Portion. Zutaten ohne Menge („Salz nach
// Belieben“) zählen nicht; Zutaten ohne Zuordnung werden gemeldet.
// SPDX-License-Identifier: MIT
#pragma once

#include "recipe.h"

struct Food;

namespace Nutrition {

// Gewicht einer Zutat in g (0 = nicht bestimmbar)
double gramsOf(const Ingredient &ing, const Food *food);

// Menüfolge statt Rezept (Kategorie „Menü“, nur Gerichtnamen ohne Mengen)
bool isMenu(const Recipe &r);
// Zuordnen, Gramm berechnen, Nährwerte gesamt/je Portion bestimmen und im Rezept speichern
void analyze(Recipe &r);

// Tags aus den Zutaten ableiten (vegan, vegetarisch, fleischgerichte, fisch)
// – nur, wenn genug Zutaten zugeordnet sind. Gibt die Vorschläge zurück.
QStringList suggestTags(const Recipe &r);
// Vorschläge übernehmen und widersprüchliche Tags entfernen (z. B. „vegan“ bei Fleisch)
void applyTagSuggestions(Recipe &r);

// Referenzmengen für Erwachsene (EU-Lebensmittelinformationsverordnung, Anhang XIII)
Nutrients referenceIntake();

} // namespace Nutrition
