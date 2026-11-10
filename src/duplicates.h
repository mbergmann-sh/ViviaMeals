// ViviaMeals – Doubletten in der Rezeptdatenbank finden und zusammenführen
//
// Zwei Rezepte gelten als (mögliche) Doubletten, wenn
//   • der Titel gleich ist – ohne Rücksicht auf Groß-/Kleinschreibung, Satzzeichen und Umlaut-
//     Schreibweise („Käsekuchen“ = „Kaesekuchen“ = „KÄSEKUCHEN!“),
//   • der Titel sehr ähnlich ist (Tippfehler, Zusätze wie „Omas …“) und die Zutaten weitgehend
//     übereinstimmen, oder
//   • die Zutaten (zugeordnete Lebensmittel) praktisch gleich sind, bei mindestens fünf Zutaten.
// Paare werden zu Gruppen zusammengefasst. Paare, die der Benutzer als „keine Doubletten“ markiert,
// stehen in <Datenbank>/doubletten-ignoriert.json und werden nicht mehr gemeldet.
//
// Zusammenführen: Das behaltene Rezept übernimmt aus den übrigen fehlende Angaben (Bild, Quelle,
// Zubereitungszeit), alle Tags und die Favoriten-Markierung; danach werden die übrigen gelöscht.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipe.h"
#include "recipedb.h"

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

namespace Duplicates {

enum Reason { SameTitle = 1, SimilarTitle = 2, SameIngredients = 4 };
enum class Strictness { TitleOnly, Normal, Generous };

struct Group {
    QStringList ids;      // nach Bewertung (vollständigstes Rezept zuerst)
    int reasons = 0;
    double score = 0;     // 0…1, höchste Ähnlichkeit in der Gruppe
};

// Titel für den Vergleich: klein, ohne Satzzeichen, Umlaute und ß vereinheitlicht
QString titleKey(const QString &title);
double titleSimilarity(const QString &a, const QString &b);          // 0…1
double ingredientSimilarity(const RecipeSummary &a, const RecipeSummary &b);   // 0…1 (Jaccard)
QString reasonText(int reasons);

QList<Group> find(const QList<RecipeSummary> &recipes, const QSet<QString> &ignoredPairs = {},
                  Strictness strictness = Strictness::Normal);

// „Keine Doubletten“ – gespeichert im Datenbankordner
QString pairKey(const QString &a, const QString &b);
QSet<QString> loadIgnored();
bool saveIgnored(const QSet<QString> &pairs);

// Rezept zum Behalten vorschlagen: Nährwerte, Bild, Zutatenzahl, Favorit, zuletzt geändert
int completeness(const RecipeSummary &s);

// keep übernimmt Fehlendes aus others (siehe oben); gibt die Liste der Übernahmen zurück
QStringList mergeInto(Recipe &keep, const QList<Recipe> &others);

} // namespace Duplicates
