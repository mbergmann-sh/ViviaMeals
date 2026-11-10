// ViviaMeals – Wochenmenü-Plan nach Kalorienziel und Vorlieben
//
// Für jeden Tag und jede Mahlzeit (Frühstück, Mittagessen, Abendessen, optional Zwischenmahlzeit)
// wird ein passendes Rezept gewählt und die Portionsgröße so bemessen, dass die Tagessumme dem
// Ziel nahekommt. Frühstück kommt nur aus Rezepten mit „frühstück“, Zwischenmahlzeiten aus
// „zwischenmahlzeit“ (bzw. leichten Frühstücksrezepten), Mittag- und Abendessen aus Hauptgerichten
// (siehe mealtypes.h). Beim Abnehmen werden Desserts und Süßspeisen nicht verplant und leichte,
// eiweiß- und ballaststoffreiche Rezepte mit wenig Zucker bevorzugt. Berücksichtigt werden Ernährungsform (vegan/vegetarisch), ausgeschlossene
// Zutaten, bevorzugte Themen und Abwechslung (kein Rezept zweimal am selben Tag, möglichst
// selten in der Woche). Nur Rezepte mit berechneten Nährwerten werden verplant.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

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

struct MealPlan {
    QDate start;
    QStringList meals;                 // Namen der Mahlzeiten
    QList<double> shares;              // Anteil am Tagesbedarf je Mahlzeit
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
    bool snack = false;
    Profile::Goal goal = Profile::Goal::Keep;   // Abnehmen: leichte, sättigende Rezepte bevorzugen
    Profile::Diet diet = Profile::Diet::All;
    QStringList excluded;
    QStringList likedTags;
    QStringList onlyTags;      // nur Rezepte mit einem dieser Tags (leer = alle)
};

Options fromProfile(const Profile &p);
QStringList mealNames(bool snack);
MealPlan create(const Options &opt);
// eine einzelne Mahlzeit neu würfeln
void reroll(MealPlan &plan, int day, int meal, const Options &opt);
// gezielt ein Rezept einsetzen, Portionen passend zum Ziel
void setRecipe(MealPlan &plan, int day, int meal, const QString &recipeId);
// Einkaufsliste über den ganzen Plan; Bestand aus dem Kühlschrank abziehen, wenn gewünscht
QList<ShoppingItem> shoppingList(const MealPlan &plan, const QStringList &stock = {});
QString toHtml(const MealPlan &plan, const QString &personName);
// Tagesdurchschnitt der Nährwerte mit Hinweisen nach DGE-Richtwerten
QString summary(const MealPlan &plan);
int candidateCount(const Options &opt);
// passende Rezepte je Mahlzeit (Frühstück, Mittag, Abend, Zwischenmahlzeit)
QList<int> slotCounts(const Options &opt);
// Hinweise zum Plan (fehlende Frühstücksrezepte …)
QStringList notes(const Options &opt);
// Anteile am Tagesbedarf (DGE-Orientierung): 25/35/30/10 % mit, 30/40/30 % ohne Zwischenmahlzeit
QList<double> shares(bool snack);
// Text „1 ½ Portionen (ca. 380 g)“
QString portionText(const PlanEntry &e);

} // namespace MealPlanner
