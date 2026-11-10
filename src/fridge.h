// ViviaMeals – Kühlschrank-Bestandsliste, Rezeptvorschläge und Einkaufslisten
//
// Bestand: Liste von Lebensmitteln („Eier“, „Milch“, „500 g Hackfleisch“), gespeichert in den
// Einstellungen dieses Geräts. Vorschläge: Rezepte, deren Zutaten möglichst vollständig im Bestand
// (bzw. im Grundvorrat wie Salz, Pfeffer, Öl, Wasser) vorhanden sind, mit fehlenden Zutaten.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

#include <QList>
#include <QString>
#include <QStringList>

struct ShoppingItem {
    QString name;
    double amount = 0;      // 0 = ohne Menge
    QString unit;
    QString category;       // Kategorie der Nährwerttabelle (für die Gliederung)
    QStringList recipes;    // Rezepte, die die Zutat brauchen
    QString text() const;   // „750 g Kartoffeln“
};

namespace Shopping {
// Zutaten mehrerer Rezepte (mit Faktor) zusammenfassen
void add(QList<ShoppingItem> &list, const Ingredient &ing, double factor, const QString &recipeTitle);
QString toText(const QList<ShoppingItem> &list, const QString &title);
QString toHtml(const QList<ShoppingItem> &list, const QString &title);
void sort(QList<ShoppingItem> &list);   // nach Kategorie, dann Name
}

namespace Fridge {

struct Suggestion {
    QString id;
    QString title;
    int have = 0;
    int total = 0;
    QStringList missing;    // fehlende Zutaten (Namen)
    double score() const { return total > 0 ? double(have) / total : 0.0; }
};

QStringList stock();                     // gespeicherter Bestand
void setStock(const QStringList &items);
QStringList basics();                    // Grundvorrat (immer vorhanden)
void setBasics(const QStringList &items);
QStringList defaultBasics();

// Vorschläge aus allen Rezepten der geöffneten Datenbank; maxMissing: höchstens so viele fehlende Zutaten
QList<Suggestion> suggest(const QStringList &stock, int maxMissing = 3, const QStringList &requiredTags = {});
// Einkaufsliste der fehlenden Zutaten eines Rezepts (mit Mengen)
QList<ShoppingItem> missingFor(const Recipe &r, const QStringList &stock, double factor = 1.0);
bool inStock(const QString &ingredientName, const QString &foodKey, const QStringList &stock);

} // namespace Fridge
