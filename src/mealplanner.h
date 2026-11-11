// ViviaMeals – Wochenmenü-Plan nach Kalorienziel und Vorlieben
//
// Für jeden Tag und jede Mahlzeit wird ein passendes Rezept gewählt und die Portionsgröße so
// bemessen, dass die Tagessumme dem Ziel nahekommt. Nur Rezepte mit berechneten Nährwerten.
//
// Tagesaufbau je Ziel (Rolle der Mahlzeit in Klammern):
//   Abnehmen  Frühstück 30 % · warme Hauptmahlzeit 45 % · leichte Mahlzeit 25 %
//             (mit Zwischenmahlzeit 25 / 40 / 10 / 25 %)
//   Halten    30 / 40 / 30 % (mit Zwischenmahlzeit 25 / 35 / 10 / 30 %), ebenfalls nur eine warme
//             Hauptmahlzeit, die zweite leicht
//   Zunehmen  5 Mahlzeiten: Frühstück, Zwischenmahlzeit, Mittagessen, Zwischenmahlzeit, Abendessen
//             (beide Hauptmahlzeiten dürfen warm sein), optional Spätmahlzeit; energiedichte Rezepte
// Ob warm mittags oder abends gegessen wird, ist einstellbar (Vorgabe mittags).
// Die Rolle bestimmt die Auswahl: Frühstück nur aus „frühstück“, warm aus Hauptgerichten, leicht aus
// „leichte mahlzeit“, Zwischenmahlzeiten aus „zwischenmahlzeit“ (siehe mealtypes.h).
//
// Abfolge innerhalb eines Tages: kein Rezept zweimal, die beiden Hauptmahlzeiten ohne gemeinsame
// Sättigungsbeilage (Nudeln, Reis, Kartoffeln, Brot) und ohne zweimal Fleisch oder Fisch; dasselbe
// Rezept nicht an zwei Tagen hintereinander.
// Wochenrichtwerte (DGE „Gut essen und trinken“, 2024; auf die Planlänge umgerechnet): Fisch 1–2-mal,
// Hülsenfrüchte mindestens einmal (gern öfter), Fleisch höchstens etwa 300 g – im Plan höchstens
// drei Fleischmahlzeiten. Beim Abnehmen keine Desserts/Süßspeisen und Vorrang für leichte,
// eiweiß- und ballaststoffreiche Rezepte mit wenig Zucker; Energiedefizit über das Kalorienziel
// des Profils (S3-Leitlinie Adipositas: etwa 500–600 kcal/Tag). Richtwerte, kein Medizinprodukt.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <functional>

#include "fridge.h"
#include "profiles.h"

#include <QDate>
#include <QJsonObject>
#include <QList>
#include <QString>

struct PlanEntry {
    QString recipeId;
    QString title;
    double portions = 1;     // Anzahl Portionen (bzw. Stück) für diese Person
    double grams = 0;        // Gewicht der Mahlzeit (Portionen × Portionsgewicht, 0 = unbekannt)
    double kcal = 0;         // kcal der Mahlzeit
    double protein = 0, fat = 0, carbs = 0, sugar = 0, fiber = 0;
};

namespace MealPlanner {
// Rolle einer Mahlzeit im Tagesablauf
enum class Role { Breakfast = 0, Warm = 1, Light = 2, Snack = 3 };
}

struct MealPlan {
    QDate start;
    QStringList meals;                 // Namen der Mahlzeiten (wie im Ernährungstagebuch)
    QList<double> shares;              // Anteil am Tagesbedarf je Mahlzeit
    QList<MealPlanner::Role> roles;    // Rolle je Mahlzeit
    int goal = 1;                      // Profile::Goal
    int diet = 0;                      // Profile::Diet
    double targetKcal = 0;
    QList<QList<PlanEntry>> days;      // days[Tag][Mahlzeit]
    QString profileId;

    double dayKcal(int day) const;
    Nutrients dayNutrients(int day) const;
    bool isEmpty() const { return days.isEmpty(); }
    QJsonObject toJson() const;
    static MealPlan fromJson(const QJsonObject &o);
};

namespace MealPlanner {

struct Options {
    int days = 7;
    QDate start = QDate::currentDate();
    double targetKcal = 2000;
    bool snack = false;        // Zwischenmahlzeit (Abnehmen/Halten) bzw. Spätmahlzeit (Zunehmen)
    bool warmEvening = false;  // warme Hauptmahlzeit abends
    Profile::Goal goal = Profile::Goal::Keep;   // Abnehmen: leichte, sättigende Rezepte bevorzugen
    Profile::Diet diet = Profile::Diet::All;
    QStringList excluded;
    QStringList likedTags;
    QStringList onlyTags;      // nur Rezepte mit einem dieser Tags (leer = alle)
};

Options fromProfile(const Profile &p);

// Tagesaufbau: Namen, Anteile und Rollen der Mahlzeiten
struct Layout {
    QStringList names;
    QList<double> shares;
    QList<Role> roles;
};
Layout layout(const Options &opt);
QString roleName(Role r);   // „warm“, „leicht“ …
// Rückmeldung für längere Berechnungen: (erledigt, gesamt, Text) → false = abbrechen
using Progress = std::function<bool(int done, int total, const QString &step)>;
MealPlan create(const Options &opt, const Progress &progress = {});
// eine einzelne Mahlzeit neu würfeln
void reroll(MealPlan &plan, int day, int meal, const Options &opt, const Progress &progress = {});
// gezielt ein Rezept einsetzen, Portionen passend zum Ziel
void setRecipe(MealPlan &plan, int day, int meal, const QString &recipeId);
// Einkaufsliste über den ganzen Plan; Bestand aus dem Kühlschrank abziehen, wenn gewünscht
QList<ShoppingItem> shoppingList(const MealPlan &plan, const QStringList &stock = {});
QString toHtml(const MealPlan &plan, const QString &personName);
// Tagesdurchschnitt der Nährwerte mit Hinweisen nach DGE-Richtwerten
QString summary(const MealPlan &plan);
int candidateCount(const Options &opt, const Progress &progress = {});
// passende Rezepte je Rolle (Frühstück, warm, leicht, Zwischenmahlzeit)
QList<int> slotCounts(const Options &opt);
// Hinweise zum Plan (fehlende Frühstücksrezepte …)
QStringList notes(const Options &opt);
// Wochenbilanz nach DGE: Fisch-, Hülsenfrucht- und Fleischmahlzeiten
struct WeekCheck {
    int fish = 0, legumes = 0, meat = 0;
    int fishMin = 1, fishMax = 2, legumesMin = 1, meatMax = 3;   // auf die Planlänge umgerechnet
};
WeekCheck weekCheck(const MealPlan &plan);
// Text „1 ½ Portionen (ca. 380 g)“
QString portionText(const PlanEntry &e);

} // namespace MealPlanner
