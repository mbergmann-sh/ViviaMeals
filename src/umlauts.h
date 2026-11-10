// ViviaMeals – Umlaute in deutschen Rezepttexten wiederherstellen (ae → ä, oe → ö, ue → ü, ss → ß)
//
// Viele ältere RezKonv- und MealMaster-Sammlungen sind in reinem ASCII erfasst („Kaese“,
// „Gemuesebruehe“, „heiss“). Die Korrektur arbeitet wortweise mit Regeln und Ausnahmen:
//  * ae/oe/ue nach einem Vokal oder q bleiben („Feuer“, „neue“, „Quelle“, „Sauerbraten“),
//  * bekannte Ausnahmen bleiben („zuerst“, „aktuell“, „Baguette“, „Fondue“, „Oetker“, „Paella“ …),
//  * ss wird nur in bekannten ß-Wortstämmen zu ß („heiß“, „Weißwein“, „gießen“, „süß“, „Grieß“,
//    „gleichmäßig“ …) – „Wasser“, „Schüssel“, „Masse“, „Essig“ bleiben.
//  * Zwischenüberschriften in Großbuchstaben werden in normale Schreibweise gebracht.
// Angewendet nur auf deutschsprachige Rezepte, die noch keine Umlaute enthalten (automatisch)
// oder auf Wunsch (Editor, Extras).
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

#include <QString>

namespace Umlauts {

QString fixWord(const QString &word);
QString fixText(const QString &text);
// Text überwiegend ohne ä/ö/ü/ß, in dem die Regeln etwas ändern würden – vermutlich Umschrift
bool looksTranscribed(const QString &text);
// „GRUENKERNKUECHLE“ → „Grünkernküchle“, „SAUCE UND DEKORATION“ → „Sauce und Dekoration“
QString headingCase(const QString &text);
// alle Texte eines Rezepts korrigieren; gibt die Anzahl geänderter Wörter zurück
int fixRecipe(Recipe &r);
QString allRecipeText(const Recipe &r);

} // namespace Umlauts
