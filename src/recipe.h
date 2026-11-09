// VitaMeals – Rezeptmodell und Eigenformat .vmr
//
// Ein Rezept wird als JSON-Datei mit der Endung .vmr gespeichert (UTF-8, Bild als Base64-PNG bzw. -JPEG
// eingebettet). Aufbau siehe Recipe::toJson(). Die berechneten Nährwerte werden mitgespeichert,
// damit Liste, Suche, Wochenplan und Website ohne Neuberechnung auskommen.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

// Nährwerte einer Menge (gesamt oder je Portion)
struct Nutrients {
    double kcal = 0;
    double protein = 0;     // g
    double fat = 0;         // g
    double satFat = 0;      // g – davon gesättigte Fettsäuren
    double carbs = 0;       // g
    double sugar = 0;       // g – davon Zucker
    double fiber = 0;       // g – Ballaststoffe
    double salt = 0;        // g

    double kJ() const { return kcal * 4.184; }
    Nutrients &operator+=(const Nutrients &o);
    Nutrients scaled(double f) const;
    bool isEmpty() const { return kcal <= 0.0 && protein <= 0.0 && fat <= 0.0 && carbs <= 0.0; }
    QJsonObject toJson() const;
    static Nutrients fromJson(const QJsonObject &o);
};

// Ergebnis der Nährwertanalyse – im Rezept gespeichert
struct NutritionInfo {
    Nutrients total;
    Nutrients perServing;
    int matched = 0;            // Zutaten mit Nährwerten
    int countable = 0;          // Zutaten, die zählen (ohne Wasser/„nach Belieben“ ohne Menge)
    QStringList unmatched;      // Zutaten ohne Zuordnung bzw. ohne umrechenbare Menge
    QDateTime computed;
    bool valid() const { return computed.isValid(); }
    double coverage() const { return countable > 0 ? double(matched) / countable : 0.0; }
    QJsonObject toJson() const;
    static NutritionInfo fromJson(const QJsonObject &o);
};

struct Ingredient {
    bool heading = false;   // Zwischenüberschrift („Für den Teig“)
    double amount = 0;      // 0 = ohne Mengenangabe
    double amountMax = 0;   // bei Bereichen („2–3“), sonst 0
    QString unit;           // deutsche Küchenmaßeinheit (g, kg, ml, l, EL, TL, Prise, Stück …) oder leer
    QString name;           // Zutat
    QString note;           // Zusatz („fein gehackt“)
    QString food;           // Schlüssel in der Nährwerttabelle (manuell oder automatisch)
    bool foodManual = false;// Zuordnung vom Benutzer festgelegt
    double grams = 0;       // berechnetes Gewicht (für Nährwerte), 0 = unbekannt
    QString original;       // Originalzeile des Imports (englisch …)

    QString amountText() const;          // „1 ½“, „2–3“
    QString displayLine() const;         // „200 g Mehl, gesiebt“
    QJsonObject toJson() const;
    static Ingredient fromJson(const QJsonObject &o);
};

struct Recipe {
    QString id;                 // UUID, zugleich Dateiname <id>.vmr
    QString title;
    double servings = 4;        // Anzahl Portionen
    QString yieldUnit;          // „Portionen“ (leer) oder z. B. „Stück“, „Kuchen“
    QStringList tags;           // vegan, vegetarisch, fleischgerichte, backen, desserts, menüs …
    QList<Ingredient> ingredients;
    QString instructions;       // Zubereitung (Absätze durch Leerzeilen getrennt)
    QString image;              // Base64-PNG (zugeschnitten im Editor) oder -JPEG (ältere Rezepte)
    QString source;             // Quelle / Autor
    QString sourceFormat;       // "VitaMeals", "RezKonv", "MealMaster", "VTM"
    QString language = QStringLiteral("de");   // Sprache der Anleitung ("de", "en", "fr" …)
    QString originalLanguage;   // bei übersetzten Rezepten: Sprache des Originals ("en", "fr" …)
    QString translatedBy;       // "DeepL" oder "Glossar" (nur Zutaten)
    int prepMinutes = 0;
    QDateTime created;
    QDateTime modified;
    NutritionInfo nutrition;
    bool favorite = false;

    QString servingsText() const;            // „4 Portionen“
    QString ingredientsText() const;         // eine Zutat je Zeile, Überschriften als „== … ==“
    QJsonObject toJson() const;
    static Recipe fromJson(const QJsonObject &o);
    bool save(const QString &file, QString *error = nullptr) const;
    static bool load(const QString &file, Recipe *out, QString *error = nullptr);
    bool hasTag(const QString &tag) const;
    void addTag(const QString &tag);
    static QString newId();
    static QString normalizedTitle(const QString &t);   // für Doubletten-Prüfung
};

// Zahlen deutsch formatieren (1,5; Brüche für EL/TL/Stück)
namespace Fmt {
QString number(double v, int decimals = 1);
QString amount(double v, const QString &unit);   // „½“, „1 ¼“, „250“
QString kcal(double v);
}
