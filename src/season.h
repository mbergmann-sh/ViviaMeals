// ViviaMeals – Saison-Tags nach der Verfügbarkeit frischer Lebensmittel
//
// Grundlage ist ein Saisonkalender für regionale Ware in Deutschland (Freiland bzw. Lagerware), grob nach
// Monaten. Frische Zutaten mit Saison (Spargel, Erdbeeren, Kürbis, Grünkohl …) bestimmen, in welchen Monaten
// ein Rezept „passt“: ein Monat zählt, wenn mindestens 80 % des Gewichts dieser Zutaten dann Saison haben.
// Eine Jahreszeit wird vergeben, wenn mindestens zwei ihrer drei Monate passen. Rezepte mit frischen Zutaten,
// die es das ganze Jahr gibt (Kartoffeln, Zwiebeln, Möhren, Äpfel vom Lager …), bekommen „ganzjährig“.
// Tiefgekühltes, Eingemachtes, Getrocknetes, Säfte und Pulver zählen nicht.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QDate>
#include <QString>
#include <QStringList>

struct Recipe;
struct Ingredient;

namespace Season {

// Bitmaske der Monate: Bit 0 = Januar … Bit 11 = Dezember
using Months = quint16;
constexpr Months AllYear = 0x0FFF;

// Saisonmonate einer Zutat: 0 = keine frische Zutat bzw. zählt nicht, AllYear = ganzjährig
Months monthsFor(const Ingredient &ing);

// Monate, in denen das Rezept passt (0 = keine frischen Saisonzutaten oder Zutaten ohne gemeinsame Saison)
Months recipeMonths(const Recipe &r, bool *hasFresh = nullptr);

QStringList tagNames();                     // frühling, sommer, herbst, winter, ganzjährig
// Tag der Jahreszeit zu einem Datum (meteorologisch: Frühling März–Mai, Sommer Juni–Aug., Herbst Sep.–Nov., Winter Dez.–Feb.)
QString seasonTagFor(const QDate &date);
QStringList suggestTags(const Recipe &r);
int applyTags(Recipe &r);                    // ersetzt vorhandene Saison-Tags; Rückgabe: Anzahl Änderungen

QString monthsText(Months m);                // „Mai–Juli“, „Okt.–Feb.“, „ganzjährig“

} // namespace Season
