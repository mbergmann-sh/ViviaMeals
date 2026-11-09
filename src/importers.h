// VitaMeals – Import von RezKonv-, MealMaster- und alten VitaMeals-Listen (.vtm)
//
// Beide Textformate können beliebig viele Rezepte in einer Datei enthalten (Rezeptsammlungen).
// Zutaten werden zerlegt (Menge, Einheit, Name, Zusatz), englische Maße in deutsche umgerechnet,
// englische Zutatennamen über das Glossar übersetzt, Temperaturen/Zoll in der Anleitung
// umgerechnet und die Nährwerte berechnet.
// SPDX-License-Identifier: MIT
#pragma once

#include "recipe.h"

#include <QList>
#include <QString>

namespace Importers {

enum class Format { Unknown, RezKonv, MealMaster, Vtm };

struct Options {
    bool translate = true;    // englische Rezepte: Zutaten übersetzen, Maße umrechnen
    bool autoTags = true;     // vegan/vegetarisch/fleischgerichte/fisch aus den Zutaten ergänzen
};

struct Result {
    QList<Recipe> recipes;
    QStringList warnings;
    QString encoding;
};

Format detect(const QString &text);
QString formatName(Format f);
Result parseRezKonv(const QString &text, const Options &opt = {});
Result parseMealMaster(const QString &text, const Options &opt = {});
Result parseVtm(const QByteArray &json, const Options &opt = {});
// Datei lesen, Format erkennen (oder erzwingen) und zerlegen
Result importFile(const QString &path, Format format, const Options &opt = {});

} // namespace Importers
