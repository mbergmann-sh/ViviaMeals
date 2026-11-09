// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "nutrition.h"
#include "foodtable.h"
#include "units.h"

#include <QRegularExpression>
#include <QSet>

namespace Nutrition {

double gramsOf(const Ingredient &ing, const Food *food)
{
    if (ing.heading || ing.amount <= 0) return 0;
    const double amount = ing.amountMax > ing.amount ? (ing.amount + ing.amountMax) / 2.0 : ing.amount;
    const QString &u = ing.unit;
    // eigenes Gewicht der Zutat für diese Einheit (z. B. 1 Pck. Vanillezucker = 8 g, 1 EL Brühpulver = 12 g)
    if (food) {
        auto it = food->unitGrams.constFind(u.isEmpty() ? QStringLiteral("Stück") : u);
        if (it != food->unitGrams.constEnd()) return amount * it.value();
    }
    switch (Units::kindOf(u)) {
    case Units::Kind::Mass:
        return amount * Units::gramsPer(u);
    case Units::Kind::Volume:
        return amount * Units::mlPer(u) * (food ? food->density : 1.0);
    case Units::Kind::Piece: {
        // Gewicht im Zusatz: „1 Süßkartoffel (ca. 400 g)“
        static const QRegularExpression g(QStringLiteral("(?:^|\\D)(\\d+(?:[.,]\\d+)?)\\s*(g|kg)\\b"));
        const auto m = g.match(ing.note);
        if (m.hasMatch()) {
            double v = 0;
            v = m.captured(1).replace(QLatin1Char(','), QLatin1Char('.')).toDouble();
            if (v > 0) return amount * v * (m.captured(2) == QLatin1String("kg") ? 1000.0 : 1.0);
        }
        if (u.isEmpty() || u == QStringLiteral("Stück")) return food && food->piece > 0 ? amount * food->piece : 0;
        if (u == QLatin1String("cm")) return 0;
        if (const double dg = Units::defaultPieceGrams(u)) return amount * dg;
        return 0;
    }
    case Units::Kind::Vague:
    case Units::Kind::None:
        return 0;
    }
    return 0;
}

bool isMenu(const Recipe &r)
{
    if (!r.hasTag(QStringLiteral("menüs")) && r.yieldUnit != QStringLiteral("Menü")) return false;
    for (const Ingredient &i : r.ingredients)
        if (!i.heading && i.amount > 0) return false;
    return true;
}

void analyze(Recipe &r)
{
    FoodTable &table = FoodTable::instance();
    NutritionInfo info;
    // Menüfolge („Vorspeise: Lauch-Käse-Suppe …“): Gerichte statt Zutaten – keine Nährwerte
    if (isMenu(r)) {
        for (Ingredient &ing : r.ingredients) {
            ing.food.clear();
            ing.grams = 0;
        }
        r.nutrition = NutritionInfo();
        return;
    }
    for (Ingredient &ing : r.ingredients) {
        if (ing.heading) continue;
        const Food *food = nullptr;
        if (!ing.food.isEmpty() && ing.foodManual) food = table.find(ing.food);
        if (!food) {
            const QString learned = table.learnedFor(ing.name);
            if (!learned.isEmpty()) food = table.find(learned);
        }
        if (!food) food = table.match(ing.name);
        // „1 Zitrone, Saft davon“ / „abgeriebene Schale von 1 Orange“
        if (food && !ing.foodManual && (food->key == QLatin1String("zitrone") || food->key == QLatin1String("orange"))) {
            const QString n = (ing.note + QLatin1Char(' ') + ing.name).toLower();
            if (n.contains(QStringLiteral("saft"))) food = table.find(food->key == QLatin1String("zitrone") ? QStringLiteral("zitronensaft") : QStringLiteral("orangensaft"));
            else if (n.contains(QStringLiteral("schale")) || n.contains(QStringLiteral("abrieb"))) food = table.find(QStringLiteral("zitronenschale"));
        }
        ing.food = food ? food->key : QString();
        if (!food) ing.foodManual = false;
        const Units::Kind kind = Units::kindOf(ing.unit);
        const bool countable = ing.amount > 0 && kind != Units::Kind::Vague;
        ing.grams = gramsOf(ing, food);
        // Saft bzw. Schale „von 1 Zitrone“: Stückzahl in Gramm umrechnen
        if (food && ing.grams <= 0 && ing.amount > 0 && (ing.unit.isEmpty() || ing.unit == QStringLiteral("Stück"))) {
            if (food->key == QLatin1String("zitronensaft")) ing.grams = ing.amount * 40;
            else if (food->key == QLatin1String("orangensaft")) ing.grams = ing.amount * 80;
            else if (food->key == QLatin1String("zitronenschale")) ing.grams = ing.amount * 5;
        }
        if (!countable) continue;
        // Wasser zählt nicht mit
        if (food && food->key == QLatin1String("wasser")) continue;
        ++info.countable;
        if (food && ing.grams > 0) {
            info.total += food->per100.scaled(ing.grams / 100.0);
            ++info.matched;
        } else {
            info.unmatched << ing.name;
        }
    }
    const double s = r.servings > 0 ? r.servings : 1.0;
    info.perServing = info.total.scaled(1.0 / s);
    info.computed = QDateTime::currentDateTime();
    r.nutrition = info;
}

QStringList suggestTags(const Recipe &r)
{
    if (isMenu(r)) return {};
    FoodTable &table = FoodTable::instance();
    bool meat = false, fish = false, gelatine = false, nonVegan = false, meatStock = false;
    int known = 0, all = 0;
    for (const Ingredient &ing : r.ingredients) {
        if (ing.heading) continue;
        ++all;
        const Food *f = ing.food.isEmpty() ? table.match(ing.name) : table.find(ing.food);
        if (!f) continue;
        ++known;
        if (f->category == QLatin1String("fleisch")) meat = true;
        if (f->category == QLatin1String("fisch")) fish = true;
        if (f->category == QLatin1String("gelatine")) gelatine = true;
        if (f->isNonVegan()) nonVegan = true;
        // Brühe/Fond aus Fleisch oder Fisch steht in der Tabelle unter „Brühe“ – trotzdem nicht vegetarisch
        static const QRegularExpression animalStock(QStringLiteral("fleischbrühe|hühnerbrühe|rinderbrühe|kalbsbrühe|geflügelbrühe|"
                                                                   "kalbsfond|wildfond|fischfond|geflügelfond|rinderfond|entenfond|knochenbrühe|"
                                                                   "suppenknochen|chicken (broth|stock)|beef (broth|stock)|fish stock"),
                                                    QRegularExpression::CaseInsensitiveOption);
        if (ing.name.contains(animalStock)) meatStock = true;
    }
    QStringList tags;
    if (meat) tags << QStringLiteral("fleischgerichte");
    if (fish) tags << QStringLiteral("fisch");
    // vegetarisch/vegan nur, wenn (fast) alle Zutaten bekannt sind
    if (all > 0 && known >= all * 0.85 && !meat && !fish && !gelatine && !meatStock) {
        tags << QStringLiteral("vegetarisch");
        if (!nonVegan) tags << QStringLiteral("vegan");
    }
    return tags;
}

void applyTagSuggestions(Recipe &r)
{
    const QStringList s = suggestTags(r);
    if (s.contains(QStringLiteral("fleischgerichte")) || s.contains(QStringLiteral("fisch"))) {
        r.tags.removeAll(QStringLiteral("vegan"));
        r.tags.removeAll(QStringLiteral("vegetarisch"));
    }
    if (s.contains(QStringLiteral("vegetarisch")) && !s.contains(QStringLiteral("vegan"))) r.tags.removeAll(QStringLiteral("vegan"));
    for (const QString &t : s) r.addTag(t);
}

Nutrients referenceIntake()
{
    Nutrients n;
    n.kcal = 2000;
    n.fat = 70;
    n.satFat = 20;
    n.carbs = 260;
    n.sugar = 90;
    n.protein = 50;
    n.salt = 6;
    n.fiber = 30;   // keine EU-Referenzmenge; Richtwert der DGE für Erwachsene
    return n;
}

} // namespace Nutrition
