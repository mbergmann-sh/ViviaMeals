// ViviaMeals – Mahlzeit-Arten erkennen und Rezepte gesundheitlich einordnen
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

// Nährwert-Tags: eiweißreich, kalorienarm, fettarm, hochkalorisch, vitaminreich, reduktionsdiät.
// Sie werden aus den berechneten Nährwerten (und den Gemüse-/Obstmengen) abgeleitet und bei jeder
// Anwendung neu bestimmt – nicht mehr zutreffende werden entfernt. Schwellen (je Portion bzw. 100 g):
//   eiweißreich    ≥ 20 % der Energie aus Eiweiß und ≥ 15 g Eiweiß je Portion
//                  (20 %: Anhang der EU-Verordnung 1924/2006 „hoher Proteingehalt“)
//   fettarm        ≤ 3 g Fett je 100 g (EU-Verordnung 1924/2006 „fettarm“, feste Lebensmittel)
//   kalorienarm    ≤ 80 kcal je 100 g und ≤ 400 kcal je Portion (an Mahlzeiten angepasst;
//                  die EU-Angabe „energiearm“ verlangt ≤ 40 kcal je 100 g)
//   hochkalorisch  ≥ 250 kcal je 100 g oder ≥ 800 kcal je Portion
//   vitaminreich   ≥ 200 g Gemüse, Obst und Kräuter je Portion bzw. ≥ 50 % des Portionsgewichts
//                  (die Nährwerttabelle enthält keine Vitaminwerte – Gemüse und Obst sind die
//                  wichtigsten Vitaminquellen, daher dieser Näherungswert); nicht bei Kuchen/Gebäck,
//                  bei Desserts nur mit ≥ 200 g Obst je Portion
//   reduktionsdiät ≤ 600 kcal je Portion, ≤ 150 kcal je 100 g, ≤ 35 % Energie aus Fett,
//                  ≤ 20 % aus Zucker, ≥ 15 % aus Eiweiß – kein Dessert/Gebäck
// Nur bei Rezepten mit Nährwerten für mindestens 80 % der Zutaten; sonst bleiben die Tags unverändert.
QStringList nutrientTagNames();
QStringList nutrientTags(const Recipe &r);
int applyNutrientTags(Recipe &r);   // gibt die Zahl der Änderungen zurück
double produceGrams(const Recipe &r);   // Gemüse, Obst, Kräuter je Portion

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
