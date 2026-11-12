// ViviaMeals – Menüs (3, 4 oder 5 Gänge) zusammenstellen, speichern und laden
//
// Der Menüassistent fragt Vorlieben ab (Ernährungsform, Charakter, Art von Vorspeise, Hauptgang und
// Dessert, Thema, ausgeschlossene Zutaten) und stellt aus den Rezepten der Datenbank ein Menü zusammen.
// Bewertet wird jedes Rezept für seinen Gang (Tags, Titel, Mahlzeit-Art, Energie) und die Harmonie des
// ganzen Menüs:
//   * keine Hauptzutat (Rind, Schwein, Geflügel, Fisch …) und keine Sättigungsbeilage (Nudeln, Reis,
//     Kartoffeln, Brot) zweimal, nicht in mehreren Gängen dieselben auffälligen Zutaten, Sahne höchstens
//     in einem Gang
//   * Energie passend zum Charakter verteilt (Vorspeise ~15 %, Suppe ~12 %, Zwischengang ~18 %,
//     Hauptgang ~38 %, Dessert ~17 %) – auf ein üppiges Hauptgericht folgen leichte Gänge
//   * gemeinsame Themen-Tags (z. B. „italienisch“) verbinden die Gänge
// Gespeichert wird ein Menü als <Datenbank>/menues/<id>.vmmenu (JSON) – es wird mit abgeglichen.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipedb.h"

#include <QDate>
#include <QHash>
#include <QSet>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace Menu {

enum class Course { Starter, Soup, Intermediate, Main, Dessert };
QString courseName(Course c);                 // „Vorspeise“, „Suppe“, „Zwischengang“, „Hauptgang“, „Dessert“
QList<Course> courseOrder(int count);         // 3: V-H-D · 4: V-S-H-D · 5: V-S-Z-H-D

struct Prefs {
    int courses = 3;
    enum class Diet { All, Vegetarian, Vegan } diet = Diet::All;
    enum class Character { Light, Balanced, Festive } character = Character::Balanced;
    enum class Starter { Any, Salad, Cold, Warm, Fish } starter = Starter::Any;
    enum class Main { Any, Beef, Pork, Poultry, LambGame, Fish, Vegetarian } mainCourse = Main::Any;   // nicht „main“: unter Windows ist main ein Makro (qMain)
    enum class Dessert { Any, Fruity, Chocolate, Creamy, Frozen, Pastry } dessert = Dessert::Any;
    QString theme;             // bevorzugter Tag (z. B. „italienisch“), leer = egal
    QStringList exclude;       // Zutaten, die nicht vorkommen sollen (Teilwörter, klein)
    bool favorites = true;     // Favoriten bevorzugen
    bool withImage = false;    // Rezepte mit Foto bevorzugen
    double targetKcal() const; // Richtwert für das ganze Menü je Person
    QJsonObject toJson() const;
    static Prefs fromJson(const QJsonObject &o);
};

struct Item {
    Course course = Course::Main;
    QString recipeId;          // leer = noch kein Rezept
    QString title;
    QString description;       // Kurzbeschreibung fürs Deckblatt (änderbar)
};

struct MenuData {
    QString id;
    QString title;             // „Festtagsmenü“
    QString occasion;          // Anlass/Untertitel, z. B. „Weihnachten 2026“
    QDate date;                // ungültig = ohne Datum
    int persons = 4;
    Prefs prefs;
    QList<Item> items;
    QDateTime created, modified;

    QJsonObject toJson() const;
    static MenuData fromJson(const QJsonObject &o);
    bool save(const QString &file, QString *error = nullptr) const;
    static bool load(const QString &file, MenuData *out, QString *error = nullptr);
    static QString newId();
};

// gespeicherte Menüs einer Datenbank
QString menuFolder(const QString &dbFolder);
QList<MenuData> savedMenus(const QString &dbFolder);   // neueste zuerst
bool saveMenu(const QString &dbFolder, MenuData &m, QString *error = nullptr);
bool removeMenu(const QString &dbFolder, const QString &id);

// Kurzbeschreibung aus den Zutaten: „mit Lachs, Dill und Gurke“
QString describe(const Recipe &r);
QString describe(const RecipeSummary &s);   // Ersatz ohne geladenes Rezept
// Portionen für die Personenzahl umrechnen (Mengen, Gesamtnährwerte)
Recipe scaled(const Recipe &r, double servings);

class Composer
{
public:
    Composer(const QList<RecipeSummary> &recipes, const Prefs &prefs, quint32 seed = 0);

    // Eignung eines Rezepts für einen Gang (0 = ungeeignet) – Tags, Titel, Art, Vorlieben
    double fit(const RecipeSummary &s, Course c) const;
    // Bewertung des ganzen Menüs (Eignung + Harmonie)
    double score(const QList<Item> &items) const;
    // vollständiger Vorschlag (alle Gänge)
    QList<Item> propose();
    // Rezepte für einen Gang, beste zuerst – berücksichtigt die übrigen Gänge (index = Position in items)
    struct Candidate {
        QString id;
        double score = 0;
        QStringList notes;     // „passt zum Thema“, „Fisch wie im Hauptgang“ …
    };
    QList<Candidate> candidates(const QList<Item> &items, int index, int max = 200) const;
    // nächster guter Vorschlag für einen Gang (anders als bisher), leer = keiner
    QString alternative(const QList<Item> &items, int index);
    // Hinweise zur Harmonie („abwechslungsreich: Fisch – Rind – Obst“, „Sahne in zwei Gängen“)
    QStringList notes(const QList<Item> &items) const;
    double kcal(const QList<Item> &items) const;     // je Person
    int suitableCount(Course c) const;
    const RecipeSummary *summary(const QString &id) const;

private:
    struct Info {
        QString title;            // klein
        QStringList tags;         // klein
        QStringList themes;       // Themen-Tags (ohne Mahlzeit-/Nährwert-/Ernährungs-Tags)
        QString ingredients;      // alle Zutatennamen klein, durch | getrennt
        int types = 0;
        double kcal = 0;
        bool soup = false, salad = false, cold = false, sweet = false, dessert = false, cake = false, sorbet = false;
        bool vegetarian = false, vegan = false, menu = false;
        QString protein;          // „Rind“, „Schwein“, „Geflügel“, „Lamm/Wild“, „Fisch“, „Meeresfrüchte“ oder leer
        QStringList starches;     // „Nudeln“, „Reis“, „Kartoffeln“, „Brot“, „Getreide“
        bool cream = false;
        QSet<QString> signature;  // auffällige Zutaten (Lebensmittel-Schlüssel)
        bool fruit = false, chocolate = false, creamy = false, frozen = false, pastry = false;
        bool favorite = false, image = false;
    };

    double harmony(const QList<Item> &items, QStringList *notes = nullptr) const;
    QList<RecipeSummary> m_list;
    QHash<QString, int> m_index;
    QList<Info> m_info;
    Prefs m_prefs;
    quint32 m_seed;
    QHash<int, QStringList> m_tried;   // Gang → schon gezeigte Rezepte (für „anderer Vorschlag“)
};

} // namespace Menu
