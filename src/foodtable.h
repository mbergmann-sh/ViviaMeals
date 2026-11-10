// ViviaMeals – Nährwerttabelle (eingebaut + eigene Einträge + gelernte Zuordnungen)
//
// Eingebaut:  :/data/lebensmittel.json (erzeugt mit tools/make_foods.py, Richtwerte je 100 g)
// Eigene:     <AppData>/lebensmittel_eigene.json – eigene Lebensmittel und abweichende Werte
//             (gleicher Schlüssel überschreibt den eingebauten Eintrag) sowie gelernte Zuordnungen
//             „Zutatenname → Lebensmittel“, die der Benutzer im Editor festgelegt hat.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

struct Food {
    QString key;
    QString name;
    QString category;           // siehe tools/make_foods.py
    Nutrients per100;           // je 100 g
    double density = 1.0;       // g je ml
    double piece = 0;           // g je Stück (0 = unbekannt)
    QHash<QString, double> unitGrams;   // g je Einheit (Pck., Dose, Zehe …)
    QStringList aliases;        // Namen (klein geschrieben), deutsch und englisch
    bool user = false;          // eigener bzw. geänderter Eintrag
    QString source;             // Herkunft eigener Einträge („USDA FoodData Central“, Dateiname des CSV-Imports …)

    bool isAnimal() const;      // Fleisch, Fisch, Gelatine
    bool isNonVegan() const;    // zusätzlich Milch, Käse, Ei, Honig
    QJsonObject toJson() const;
    static Food fromJson(const QJsonObject &o);
};

class FoodTable
{
public:
    static FoodTable &instance();

    const Food *find(const QString &key) const;
    // Zutatennamen einem Lebensmittel zuordnen (nullptr = keine Zuordnung)
    const Food *match(const QString &ingredientName) const;
    QList<const Food *> all() const;             // nach Name sortiert
    static QString categoryName(const QString &cat);
    static QStringList categories();

    // eigene Einträge
    void saveUserFood(const Food &f);
    void removeUserFood(const QString &key);     // eingebauter Eintrag gilt dann wieder
    // viele Einträge auf einmal (CSV-Import); gibt die Anzahl übernommener Einträge zurück
    int saveUserFoods(const QList<Food> &foods, bool replaceExisting);
    int removeUserFoodsFrom(const QString &source);   // alle Einträge einer Herkunft entfernen
    QStringList userSources() const;
    QList<const Food *> userFoods() const;
    bool isBuiltin(const QString &key) const { return m_builtin.contains(key); }
    void learn(const QString &ingredientName, const QString &foodKey);
    QString learnedFor(const QString &ingredientName) const;

    static QString foldUmlauts(const QString &s);
    static QString normalize(const QString &name);   // Zutatennamen für den Abgleich vereinfachen
    QString userFile() const;
    void reload();

private:
    FoodTable();
    void load();
    void rebuildAliases();
    void saveUser() const;
    QHash<QString, Food> m_foods;          // wirksame Einträge
    QHash<QString, Food> m_builtin;
    QHash<QString, Food> m_userFoods;
    QHash<QString, QString> m_learned;     // normalisierter Name → Schlüssel
    struct Alias { QString text; QString key; };
    QList<Alias> m_aliases;                // nach Länge absteigend
    QHash<QString, QString> m_exact;       // Alias → Schlüssel
};
