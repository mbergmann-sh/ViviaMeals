// ViviaMeals – Rezeptpakete (.vmpack) zum Weitergeben einer Rezeptsammlung
//
// Ein Paket ist ein gewöhnliches ZIP-Archiv (unkomprimiert gespeichert, mit jedem Entpackprogramm lesbar):
//   viviameals-paket.json   Kennung „ViviaMeals-Package“, Name, Beschreibung, Datum, Programmversion, Anzahl
//   rezepte/<id>.vmr        die Rezepte im ViviaMeals-Format (mit oder ohne Foto)
//   lebensmittel.json       eigene Lebensmittel, auf die die Rezepte verweisen, und gelernte Zuordnungen
//                           der vorkommenden Zutatennamen – damit beim Empfänger dieselben Nährwerte entstehen
// Bewusst NICHT enthalten: Benutzerprofile, Ziele, Tagebuch, Wochenpläne, Einstellungen, Sicherungskopien,
// Doubletten-Ausnahmen – und auf Wunsch auch keine Favoriten-Markierungen.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "foodtable.h"
#include "recipe.h"

#include <QHash>

namespace RecipePackage {

QString suffix();   // "vmpack"

struct Options {
    QString name;          // Name der Sammlung (wird beim Empfänger Datenbankname bzw. Hinweis)
    QString description;
    QString author;        // optional, frei eingegeben
    bool images = true;
    bool foods = true;     // eigene Lebensmittel und Zuordnungen mitgeben
    bool favorites = false;
};

struct Content {
    QString name, description, author, app;
    QDateTime created;
    QList<Recipe> recipes;
    QList<Food> foods;
    QHash<QString, QString> learned;   // vereinfachter Zutatenname → Lebensmittel-Schlüssel
    QStringList warnings;
};

// Eigene Lebensmittel und Zuordnungen, die zu diesen Rezepten gehören
void collectFoods(const QList<Recipe> &recipes, QList<Food> *foods, QHash<QString, QString> *learned);

QByteArray build(const QList<Recipe> &recipes, const Options &o, int *foodCount = nullptr);
bool write(const QString &file, const QList<Recipe> &recipes, const Options &o, QString *error = nullptr, int *foodCount = nullptr);
bool parse(const QByteArray &zip, Content *out, QString *error = nullptr);
bool read(const QString &file, Content *out, QString *error = nullptr);

// Lebensmittel des Pakets übernehmen, ohne vorhandene Einträge oder Zuordnungen zu überschreiben.
// Gibt die Zahl neuer Lebensmittel zurück; learnedAdded = neue Zuordnungen
int importFoods(const Content &c, int *learnedAdded = nullptr);

// Minimaler ZIP-Leser (nur unkomprimiert gespeicherte Einträge, mit CRC-Prüfung)
QHash<QString, QByteArray> unzip(const QByteArray &zip, QString *error = nullptr);

} // namespace RecipePackage
