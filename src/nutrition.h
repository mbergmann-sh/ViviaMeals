// ViviaMeals – Nährwertanalyse eines Rezepts
//
// Für jede Zutat: Lebensmittel zuordnen (manuell festgelegt > gelernt > automatisch), Menge in
// Gramm umrechnen (Gewicht direkt, Volumen über die Dichte, Stück/Packungen über Stückgewichte),
// Nährwerte aufsummieren; Ergebnis gesamt und je Portion. Zutaten ohne Menge („Salz nach
// Belieben“) zählen nicht; Zutaten ohne Zuordnung werden gemeldet.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

#include <QSet>

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

// Zutaten ohne Lebensmittel-Zuordnung in mehreren Rezepten (für den Lebensmittel-Assistenten):
// zusammengefasst nach dem vereinfachten Namen, häufigste zuerst. Menüfolgen, Abschnitte und
// Namen aus „ignored“ (vereinfacht, siehe FoodTable::normalize) zählen nicht.
struct MissingFood {
    QString name;          // Name, wie er im Rezept steht (erstes Vorkommen)
    QString key;           // vereinfachter Name
    int count = 0;
    QStringList examples;  // Zutatenzeilen
    QStringList recipes;   // Rezepttitel
};
QList<MissingFood> missingFoods(const QList<Recipe> &recipes, const QSet<QString> &ignored = {});
// Energie aus den Hauptnährstoffen (Eiweiß/KH 4, Fett 9, Ballaststoffe 2 kcal je g) – Plausibilitätsprüfung
double energyFromMacros(const Nutrients &n);

} // namespace Nutrition
