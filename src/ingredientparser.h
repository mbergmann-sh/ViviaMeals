// VitaMeals – Zutatenzeilen zerlegen
//
//   „1 1/2 c all-purpose flour, sifted“  →  200 g | Weizenmehl | gesiebt   (englisch, umgerechnet)
//   „2-3 EL Olivenöl“                     →  2–3 EL | Olivenöl
//   „1 (16 oz) can tomatoes“              →  455 g | Tomaten
//   „== Für den Teig ==“, „Teig:“          →  Zwischenüberschrift
// SPDX-License-Identifier: MIT AND LicenseRef-Commons-Clause-1.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

namespace IngredientParser {

struct Options {
    bool translate = false;        // Zutatennamen/Zusätze mit dem Glossar übersetzen (englische Rezepte)
    bool mealMasterUnits = false;  // MealMaster: „t“ = TL, „T“ = EL (sonst ebenso)
};

// Zahl aus „1 1/2“, „1½“, „1,5“, „½“, „3/4“ lesen; false, wenn keine Zahl
bool parseNumber(const QString &s, double *out);

// Eine Zeile zerlegen. Englische Einheiten werden immer in deutsche umgerechnet (Cups von festen
// Zutaten über die Dichte der Nährwerttabelle in Gramm).
Ingredient parseLine(const QString &line, const Options &opt = {});

// Bereits zerlegte Teile (z. B. aus festen Spalten) zusammensetzen und umrechnen
Ingredient fromParts(const QString &amount, const QString &unit, const QString &name, const Options &opt = {});

// Mehrzeiligen Text (eine Zutat je Zeile) zerlegen; Fortsetzungszeilen mit „-“ werden angehängt
QList<Ingredient> parseText(const QString &text, const Options &opt = {});

// ist die Zeile eine Zwischenüberschrift? (Text ohne Markierungen in *title)
bool isHeading(const QString &line, QString *title = nullptr);

} // namespace IngredientParser
