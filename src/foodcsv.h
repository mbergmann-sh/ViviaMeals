// VitaMeals – Nährwerte aus CSV-Dateien importieren und eigene Nährwerte als CSV exportieren
//
// Unterstützt werden
//  * flache Tabellen (eine Zeile je Lebensmittel, Werte je 100 g): eigene Tabellen aus Excel/
//    LibreOffice, die VitaMeals-Vorlage, CSV-Exporte von Open Food Facts u. ä. Trennzeichen
//    (; , Tab |), Zeichensatz (UTF-8, Windows-1252, CP850) und Dezimalkomma werden erkannt, die
//    Spalten anhand der Überschriften zugeordnet (deutsch/englisch) und lassen sich ändern.
//  * USDA FoodData Central: der entpackte CSV-Ordner (food.csv, nutrient.csv, food_nutrient.csv,
//    food_category.csv), z. B. „Foundation Foods“ oder „SR Legacy“ – gemeinfrei (CC0).
// Importierte Einträge sind „eigene Lebensmittel“ mit einer Herkunftsangabe und lassen sich je
// Herkunft wieder entfernen.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "foodtable.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <functional>

namespace FoodCsv {

struct Table {
    QStringList header;
    QList<QStringList> rows;
    QChar delimiter;
    QString encoding;
};

enum Field { Name, Kcal, KJ, Protein, Fat, SatFat, Carbs, Sugar, Fiber, Salt, Sodium, Category, Aliases, Density, Piece, FieldCount };

struct Mapping {
    int col[FieldCount];
    bool sodiumMg = false;          // Natrium in mg je 100 g (sonst g)
    Mapping() { for (int &c : col) c = -1; }
};

struct Options {
    QString source;                 // Herkunftsangabe der importierten Einträge
    QString defaultCategory = QStringLiteral("sonstiges");
    QString filter;                 // nur Zeilen, deren Name diesen Text enthält
    bool overrideBuiltin = false;   // Werte eingebauter Lebensmittel gleichen Namens ersetzen
};

QString fieldName(Field f);
bool readTable(const QString &path, Table *out, QString *error = nullptr, int maxRows = -1);
Mapping autoMap(const QStringList &header);
double number(const QString &text, bool *ok = nullptr);
QList<Food> convert(const Table &t, const Mapping &m, const Options &opt, QStringList *warnings = nullptr);

// USDA FoodData Central (entpackter CSV-Ordner)
bool isUsdaFolder(const QString &folder);
bool readUsda(const QString &folder, const Options &opt, QList<Food> *out, QString *error = nullptr,
              const std::function<bool(int)> &progress = {});

// Kategorie aus einem Text raten (Kategorie-Spalte, USDA-Gruppe oder Name)
QString guessCategory(const QString &text, const QString &fallback);

// eigene Lebensmittel bzw. eine leere Vorlage als CSV (Semikolon, Dezimalkomma, UTF-8 mit BOM – für Excel)
bool exportUserFoods(const QString &path, QString *error = nullptr);
bool writeTemplate(const QString &path, QString *error = nullptr);

} // namespace FoodCsv
