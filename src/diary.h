// VitaMeals – Ernährungstagebuch je Person
//
// Jede Person (Profil aus dem Assistenten Gewichtskontrolle) hat ein eigenes Tagebuch:
//   <AppData>/tagebuch/<Profil-ID>.json
// Ein Eintrag ist eine gegessene Speise mit Datum und Uhrzeit, Mahlzeit (Frühstück …), Menge
// (Portionen bzw. Gramm) und den daraus berechneten Nährwerten. Speisen stammen
//   * aus einem Rezept der Datenbank – Titel, Nährwerte je Portion und Portionsgewicht werden beim
//     Eintragen kopiert, damit das Tagebuch auch ohne Datenbank (oder nach dem Löschen des Rezepts)
//     stimmt, oder
//   * aus einem eigenen Rezept („Eigenes Rezept“, im Editor erfasst) – es wird ohne Bild im Eintrag
//     gespeichert und kann auf Wunsch zusätzlich in die Rezeptdatenbank übernommen werden.
// Restkalorien = Zielzufuhr der Person (Profile::targetKcal) − Summe des Tages.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace Diary {

// Mahlzeiten in Tagesreihenfolge
QStringList mealNames();                  // Frühstück, Zwischenmahlzeit (vormittags), Mittagessen, …
QString mealForTime(const QTime &t);      // Vorschlag nach Uhrzeit
QTime defaultTime(const QString &meal);   // typische Uhrzeit einer Mahlzeit

struct Entry {
    QString id;
    QDateTime when;
    QString meal;
    QString title;
    QString recipeId;          // Rezept der Datenbank (leer bei eigenem Rezept, das nicht übernommen wurde)
    bool own = false;          // eigenes Rezept (im Editor erfasst)
    QJsonObject ownRecipe;     // eigenes Rezept ohne Bild (Recipe::toJson)
    double portions = 1;       // gegessene Portionen
    bool byGrams = false;      // Menge wurde in Gramm angegeben
    double grams = 0;          // gegessene Menge in g (0 = unbekannt)
    Nutrients perServing;      // Nährwerte je Portion (Kopie beim Eintragen)
    double servingGrams = 0;   // Gewicht einer Portion (Kopie)
    QString yieldUnit;         // „Stück“ … (leer = Portion)
    QString note;

    Nutrients eaten() const;   // perServing × portions
    QString amountText() const;   // „1 ½ Portionen (ca. 380 g)“ bzw. „250 g“
    void setPortions(double p);   // setzt portions (und grams, wenn das Portionsgewicht bekannt ist)
    void setGrams(double g);      // setzt grams und rechnet in Portionen um
    QJsonObject toJson() const;
    static Entry fromJson(const QJsonObject &o);
    // aus einem Rezept (Nährwerte und Gewicht werden kopiert)
    static Entry fromRecipe(const Recipe &r, bool own);
    Recipe recipe() const;     // eigenes Rezept wiederherstellen
};

class Book
{
public:
    explicit Book(const QString &profileId = QString());
    bool load(const QString &profileId);
    bool save(QString *error = nullptr) const;
    QString profileId() const { return m_profile; }
    QString file() const;
    static QString fileFor(const QString &profileId);
    static void removeFor(const QString &profileId);   // Tagebuch einer gelöschten Person entfernen

    const QList<Entry> &entries() const { return m_entries; }
    QList<Entry> day(const QDate &d) const;            // nach Uhrzeit sortiert
    Nutrients dayTotal(const QDate &d) const;
    QList<QDate> daysWithEntries() const;
    Entry *find(const QString &id);
    QString add(Entry e);                               // gibt die ID zurück
    void update(const Entry &e);
    void remove(const QString &id);

private:
    QString m_profile;
    QList<Entry> m_entries;
};

} // namespace Diary
