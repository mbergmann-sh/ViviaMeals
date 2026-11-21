// ViviaMeals – Tag „säure-basen-fasten“ (Basenfasten)
//
// Ein Rezept bekommt den Tag, wenn ALLE Zutaten auf der Liste der erlaubten (basenbildenden) Lebensmittel
// der gängigen, strengen Variante stehen: Gemüse, Kartoffeln, Süßkartoffeln, Pilze, Sprossen, Algen,
// frisches Obst, wenig Trockenobst, Kräuter und Gewürze, Mandeln und Samen, kaltgepresste Öle (Oliven-,
// Lein-, Hanf-, Walnuss-, Kürbiskern-, Sesamöl), Mandelmus, Gemüsebrühe, Apfelessig, Hefeflocken.
// Eine einzige verbotene (Fleisch, Fisch, Ei, Milchprodukte, Getreide, Hülsenfrüchte, die meisten Nüsse,
// Zucker, Süßes, Alkohol, Kaffee, Fertigprodukte …) oder unbekannte Zutat schließt das Rezept aus –
// lieber ein Rezept zu wenig als eines zu viel. Wasser und Nicht-Lebensmittel (Backpapier …) zählen nicht.
//
// Der Tag wird bei jeder Anwendung neu bestimmt (gesetzt oder entfernt). ViviaMeals ist kein
// Medizinprodukt; eine Wirkung auf den Säure-Basen-Haushalt ist wissenschaftlich nicht belegt.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QString>
#include <QStringList>

struct Recipe;
struct Ingredient;

namespace Basenfasten {

QString tagName();   // „säure-basen-fasten“

enum class Verdict { Allowed, Ignored, Forbidden, Unknown };
// Einordnung einer (zugeordneten) Zutat
Verdict judge(const Ingredient &ing);

// passt das Rezept? *reasons: Zutaten, die es verhindern („Butter: verboten“, „Tamari: unbekannt“)
bool qualifies(const Recipe &r, QStringList *reasons = nullptr);

// setzt oder entfernt den Tag; Rückgabe: 1 bei Änderung, sonst 0
int applyTag(Recipe &r);

} // namespace Basenfasten
