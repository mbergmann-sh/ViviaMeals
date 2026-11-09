// VitaMeals – Import von RezKonv-, MealMaster- und alten VitaMeals-Listen (.vtm)
//
// Beide Textformate können beliebig viele Rezepte in einer Datei enthalten (Rezeptsammlungen).
// Zutaten werden zerlegt (Menge, Einheit, Name, Zusatz), englische Maße in deutsche umgerechnet,
// englische Zutatennamen über das Glossar übersetzt, Temperaturen/Zoll in der Anleitung
// umgerechnet und die Nährwerte berechnet.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

#include <QList>
#include <QString>

#include <functional>

namespace Importers {

enum class Format { Unknown, RezKonv, MealMaster, Vtm };

struct Options {
    bool translate = true;    // englische Rezepte: Zutaten übersetzen, Maße umrechnen
    bool autoTags = true;     // vegan/vegetarisch/fleischgerichte/fisch aus den Zutaten ergänzen
    bool mealTags = true;     // frühstück/hauptgerichte/zwischenmahlzeit/desserts/süßspeise/backen ergänzen
    bool fixUmlauts = true;   // deutsche Rezepte in ASCII-Umschrift: ae/oe/ue/ss → ä/ö/ü/ß
    // Übersetzer für fremdsprachige Rezepte (z. B. DeepL). Gesetzt → Zutaten werden nicht über das Glossar
    // übersetzt, sondern das fertige Rezept wird übergeben. Liefert false (mit Fehlertext), greift das Glossar.
    // Deutschsprachige Rezepte werden nie übergeben.
    std::function<bool(Recipe &, QString *)> translator;
};

struct Result {
    QList<Recipe> recipes;
    QStringList warnings;
    QString encoding;
    int umlautsFixed = 0;     // Rezepte, deren ASCII-Umschrift korrigiert wurde
    int translated = 0;       // Rezepte, die der Übersetzer übersetzt hat
};

Format detect(const QString &text);
QString formatName(Format f);
Result parseRezKonv(const QString &text, const Options &opt = {});
Result parseMealMaster(const QString &text, const Options &opt = {});
Result parseVtm(const QByteArray &json, const Options &opt = {});
// Datei lesen, Format erkennen (oder erzwingen) und zerlegen
Result importFile(const QString &path, Format format, const Options &opt = {});

} // namespace Importers
