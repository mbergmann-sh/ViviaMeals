// VitaMeals – Personenprofile für die Assistenten Gewichtsverlust/-zunahme und den Gewichtsverlauf
//
// Gespeichert in <AppData>/profile.json (nicht in der Rezeptdatenbank – die Rezepte können geteilt
// werden, die Profile sind persönlich). Die Berechnungen sind allgemeine Richtwerte:
//   Grundumsatz nach Mifflin-St Jeor, Gesamtbedarf = Grundumsatz × PAL (körperliche Aktivität),
//   Energiedefizit bzw. -überschuss aus dem Wochenziel (≈ 7 000 kcal je kg Körpergewicht).
// VitaMeals ist KEIN Medizinprodukt – die Werte ersetzen keine ärztliche oder Ernährungsberatung.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QDate>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

struct WeightEntry {
    QDate date;
    double kg = 0;
    QString note;
};

struct Profile {
    enum class Goal { Lose, Keep, Gain };
    enum class Diet { All, Vegetarian, Vegan };

    QString id;
    QString name;
    bool female = true;
    int birthYear = 1980;
    double heightCm = 170;
    double pal = 1.4;                  // körperliche Aktivität (1,2 … 2,0)
    Goal goal = Goal::Keep;
    double targetKg = 0;               // Zielgewicht (0 = keins)
    double ratePerWeek = 0.5;          // kg je Woche
    Diet diet = Diet::All;
    QStringList excluded;              // Zutaten, die nicht vorkommen sollen („Pilze“, „Koriander“)
    QStringList likedTags;             // bevorzugte Themen („italienisch“, „suppen“)
    bool snack = false;                // Zwischenmahlzeit einplanen
    int weighDay = 0;                  // Wiegetag: 0 = keiner, 1 = Montag … 7 = Sonntag
    QDate weighDone;                   // Wiegetag, dessen Erinnerung als „erledigt“ markiert wurde
    QList<WeightEntry> weights;        // Gewichtsverlauf, nach Datum sortiert
    QJsonObject lastPlan;              // zuletzt erstellter Wochenplan

    double currentKg() const;          // letzter Eintrag (0 = keiner)
    // letzter Wiegetag bis heute (ungültig, wenn kein Wiegetag eingestellt ist)
    QDate lastWeighDay(const QDate &today = QDate::currentDate()) const;
    // Erinnerung fällig: Wiegetag erreicht, noch nicht „erledigt“ und seitdem kein Gewicht eingetragen
    bool weighReminderDue(const QDate &today = QDate::currentDate()) const;
    void markWeighDone(const QDate &today = QDate::currentDate());
    static QString dayName(int weekday);   // 1 → „Montag“
    int age(const QDate &on = QDate::currentDate()) const;
    double bmi(double kg = -1) const;
    double bmr() const;                // Grundumsatz kcal/Tag
    double tdee() const;               // Gesamtumsatz kcal/Tag
    double targetKcal(QString *hint = nullptr) const;   // Zielzufuhr kcal/Tag (mit Hinweis bei Begrenzung)
    double minimumKcal() const;
    static QString bmiClass(double bmi);
    static QString goalName(Goal g);
    static QString dietName(Diet d);
    static QList<QPair<double, QString>> palLevels();

    QJsonObject toJson() const;
    static Profile fromJson(const QJsonObject &o);
};

class Profiles
{
public:
    static Profiles &instance();
    QList<Profile> &all() { return m_list; }
    Profile *find(const QString &id);
    QString add(const Profile &p);     // gibt die ID zurück
    void update(const Profile &p);
    void remove(const QString &id);
    void save();
    QString file() const;
    void reload();

private:
    Profiles();
    QList<Profile> m_list;
};
