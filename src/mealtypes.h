// VitaMeals – Mahlzeit-Arten erkennen und Rezepte gesundheitlich einordnen
//
// Aus Titel, Kategorien/Tags, Zutaten und Nährwerten wird geschätzt, wofür ein Rezept taugt:
//   frühstück · hauptgerichte · zwischenmahlzeit · desserts · süßspeise · backen
// Die Tags werden beim Import (abschaltbar), im Editor („Ergänzen“) und unter *Extras* vergeben.
// Der Wochenplaner verplant danach: Frühstück nur aus „frühstück“, Mittag-/Abendessen aus
// Hauptgerichten, Zwischenmahlzeiten aus „zwischenmahlzeit“ – beim Abnehmen ohne Desserts.
//
// Gesundheits-Kennzahlen (je Portion): Energiedichte (kcal/100 g), Zuckeranteil an der Energie,
// Eiweißanteil (sättigt), Ballaststoffe je 100 kcal. Orientierung an den DGE-Empfehlungen
// „Gut essen und trinken“ (2024) – Richtwerte, kein Medizinprodukt.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

#include <QString>
#include <QStringList>

struct RecipeSummary;

namespace MealTypes {

enum Type : int {
    None = 0,
    Breakfast = 1,
    Main = 2,
    Snack = 4,
    Dessert = 8,
    Sweet = 16,
    Baking = 32,
    Basic = 64,      // Grundrezept, Soße, Getränk – nicht als Mahlzeit verplanen
};

// Tag-Namen
QString tagFor(Type t);   // Breakfast → „frühstück“ …
QStringList mealTags();   // alle Mahlzeit-Tags

struct Input {
    QString title;
    QStringList tags;
    QStringList ingredientNames;   // klein
    QStringList foods;             // Lebensmittel-Schlüssel
    Nutrients perServing;
    double servingGrams = 0;       // Gewicht einer Portion (0 = unbekannt)
    bool hasNutrition = false;
};
Input from(const Recipe &r);
Input from(const RecipeSummary &s);

// Arten aus vorhandenen Tags (explizit) bzw. geschätzt (Tags + Titel + Zutaten + Nährwerte)
int explicitTypes(const QStringList &tags);
int classify(const Input &in);
// Tags vorschlagen, die noch fehlen; ergänzen gibt die Anzahl neuer Tags zurück
QStringList suggestTags(const Recipe &r);
int applyTags(Recipe &r);

// Kennzahlen
double energyDensity(const Input &in);   // kcal je 100 g (0 = unbekannt)
double sugarShare(const Input &in);      // Anteil Zucker an der Energie (0…1)
double proteinShare(const Input &in);    // Anteil Eiweiß an der Energie (0…1)
double fiberPer100kcal(const Input &in);
// kurze Einordnung für die Anzeige („leicht“, „sättigend“, „viel Zucker“ …)
QStringList healthNotes(const Input &in);

// Gewicht einer Portion aus den Zutaten (Summe der umgerechneten Gramm / Portionen)
double servingGrams(const Recipe &r);

} // namespace MealTypes
