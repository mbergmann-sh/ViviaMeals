// ViviaMeals – Import von RezKonv-, MealMaster- und alten ViviaMeals-Listen (.vtm)
//
// Beide Textformate können beliebig viele Rezepte in einer Datei enthalten (Rezeptsammlungen).
// Zutaten werden zerlegt (Menge, Einheit, Name, Zusatz), englische Maße in deutsche umgerechnet,
// englische Zutatennamen über das Glossar übersetzt, Temperaturen/Zoll in der Anleitung
// umgerechnet und die Nährwerte berechnet.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

#include <QDateTime>
#include <QList>
#include <QString>

#include <functional>

namespace Importers {

enum class Format { Unknown, RezKonv, MealMaster, Vtm, Ambrosia };

struct Options {
    bool translate = true;    // englische Rezepte: Zutaten übersetzen, Maße umrechnen
    bool autoTags = true;     // vegan/vegetarisch/fleischgerichte/fisch aus den Zutaten ergänzen
    bool mealTags = true;     // frühstück/hauptgerichte/zwischenmahlzeit/desserts/süßspeise/backen ergänzen
    bool nutrientTags = true; // eiweißreich/kalorienarm/fettarm/hochkalorisch/vitaminreich/reduktionsdiät
    bool seasonTags = true;   // frühling/sommer/herbst/winter/ganzjährig nach frischen Zutaten
    bool fixUmlauts = true;   // deutsche Rezepte in ASCII-Umschrift: ae/oe/ue/ss → ä/ö/ü/ß
    // Übersetzer für fremdsprachige Rezepte (z. B. DeepL). Gesetzt → Zutaten werden nicht über das Glossar
    // übersetzt, sondern das fertige Rezept wird übergeben. Liefert false (mit Fehlertext), greift das Glossar.
    // Deutschsprachige Rezepte werden nie übergeben.
    std::function<bool(Recipe &, QString *)> translator;
    // Fortschritt bei großen Datenbanken (Ambrosia): gelesene/alle Rezepte; false = abbrechen
    std::function<bool(int, int)> progress;
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

// Rezept aus bereits getrennten Textteilen (z. B. Texterkennung eines gescannten Rezepts).
// Durchläuft dieselbe Nachbearbeitung wie die Dateiimporte: Spracherkennung, Umlaut-Korrektur,
// Übersetzung, Zutatenzerlegung, Nährwerte und Tags.
struct TextParts {
    QString title;
    QString yield;               // z. B. „4 Portionen“, „für 6 Personen“
    QStringList categories;
    QStringList ingredientLines; // eine Zutat je Zeile, „== Abschnitt ==“ für Überschriften
    QStringList directionLines;
    QStringList sourceLines;
};
Result fromParts(const TextParts &parts, const QString &format, const Options &opt = {});
// Rezepte aus Datenbanken (z. B. Ambrosia), deren Zutaten schon in Spalten vorliegen. Durchläuft dieselbe
// Nachbearbeitung wie die Textformate (Sprache, Umlaute, Übersetzung, Umrechnung, Nährwerte, Tags).
struct StructuredIngredient {
    bool heading = false;
    QString amount, unit, name, note;
};
struct StructuredRecipe {
    QString title;
    QString yield;                    // „4 Portionen“, „8 Stück“
    QStringList categories;
    QList<StructuredIngredient> ingredients;
    QString instructions;             // bereits in ViviaMeals-Auszeichnung (siehe markup.h)
    QString source;
    int prepMinutes = 0;
    QString image;                    // Base64 (JPEG/PNG)
    QDateTime created;
};
Recipe fromStructured(const StructuredRecipe &in, const QString &format, const Options &opt, Result *res);
// Schreibweisen vereinheitlichen (Celsius: „200GradC“, „200 °C“ → „200° C“) in Titel, Anleitung und Zutaten
void normalizeTexts(Recipe &r);
// Zeichen alter DOS-Sammlungen, die als Windows-1252 gelesen wurden: „Eiweiá“ → „Eiweiß“, „fraŒche“ → „fraîche“
QString repairMojibake(const QString &text);
// Zutatenliste aufräumen (beim Import und beim Abgleich): Zeilen ohne Menge, die keine Zutat sind –
//   Erfasser-/Quellenangaben („Erfasst von …“, „K.-H. Boller …“) → Quelle,
//   Nährwert- und Zeitangaben der Vorlage („Pro Portion 44 g E …“) → Hinweis am Ende der Zubereitung,
//   Mahlzeiten in GROSSBUCHSTABEN bzw. mit Doppelpunkt („MITTAGESSEN“, „SAUCE:“) → Zwischenüberschrift,
//   umbrochene Reste der Vorzeile („geschnitten“, „in dünnen Scheiben“, „Schneiden)“) → Zusatz der Vorzeile.
// Rückgabe: Anzahl der Änderungen
int tidyIngredients(Recipe &r);

} // namespace Importers
