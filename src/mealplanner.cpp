// SPDX-License-Identifier: MIT AND LicenseRef-Commons-Clause-1.0 (siehe LICENSE)
#include "mealplanner.h"
#include "foodtable.h"
#include "recipedb.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QLocale>
#include <QRandomGenerator>
#include <QSet>
#include <cmath>

namespace {
QString tr(const char *s) { return QCoreApplication::translate("MealPlanner", s); }

enum Slot { Breakfast, Lunch, Dinner, Snack };

bool hasAny(const QStringList &tags, std::initializer_list<const char *> l)
{
    for (const char *t : l)
        if (tags.contains(QString::fromUtf8(t), Qt::CaseInsensitive)) return true;
    return false;
}

bool dietOk(const RecipeSummary &s, Profile::Diet diet)
{
    if (diet == Profile::Diet::All) return true;
    if (diet == Profile::Diet::Vegan && s.tags.contains(QStringLiteral("vegan"))) return true;
    if (diet == Profile::Diet::Vegetarian && (s.tags.contains(QStringLiteral("vegetarisch")) || s.tags.contains(QStringLiteral("vegan")))) return true;
    // ohne Tag: über die zugeordneten Lebensmittel prüfen (nur bei vollständiger Zuordnung)
    if (s.coverage < 0.9) return false;
    for (const QString &k : s.foods) {
        const Food *f = FoodTable::instance().find(k);
        if (!f) continue;
        if (diet == Profile::Diet::Vegan ? f->isNonVegan() : f->isAnimal()) return false;
    }
    return true;
}

struct Candidate {
    const RecipeSummary *s;
    double kcal;           // je Portion
    QList<Slot> fits;
    double bonus;
};

QList<Candidate> candidates(const MealPlanner::Options &opt)
{
    QList<Candidate> out;
    for (const RecipeSummary &s : RecipeDb::instance().summaries()) {
        if (!s.hasNutrition || s.coverage < 0.6 || s.perServing.kcal < 40) continue;
        if (!dietOk(s, opt.diet)) continue;
        bool excluded = false;
        for (const QString &x : opt.excluded) {
            const QString e = x.trimmed().toLower();
            if (e.isEmpty()) continue;
            if (s.title.toLower().contains(e)) excluded = true;
            for (const QString &n : s.ingredientNames)
                if (n.contains(e)) excluded = true;
        }
        if (excluded) continue;
        if (!opt.onlyTags.isEmpty()) {
            bool any = false;
            for (const QString &t : opt.onlyTags)
                if (s.tags.contains(t, Qt::CaseInsensitive)) any = true;
            if (!any) continue;
        }
        Candidate c{ &s, s.perServing.kcal, {}, 0 };
        const bool breakfast = hasAny(s.tags, { "frühstück", "müsli", "brunch" });
        const bool sweet = hasAny(s.tags, { "desserts", "backen", "süßigkeiten", "snacks", "kuchen", "obst" });
        const bool drink = hasAny(s.tags, { "getränke", "soßen", "dips", "grundrezepte" });
        const bool main = hasAny(s.tags, { "hauptgerichte", "fleischgerichte", "fisch", "suppen", "eintöpfe", "aufläufe",
                                           "nudeln", "reis", "kartoffeln", "salate", "gemüse", "menüs" });
        if (drink && !main) continue;
        if (breakfast) c.fits << Breakfast;
        if (main || (!breakfast && !sweet)) c.fits << Lunch << Dinner;
        if (sweet || breakfast) c.fits << Snack;
        if (sweet && !breakfast && !main && !c.fits.contains(Snack)) c.fits << Snack;
        for (const QString &t : opt.likedTags)
            if (s.tags.contains(t.trimmed(), Qt::CaseInsensitive)) c.bonus += 1;
        if (s.favorite) c.bonus += 1;
        out << c;
    }
    return out;
}

double portionsFor(double kcalPerPortion, double target)
{
    if (kcalPerPortion <= 0) return 1;
    double p = target / kcalPerPortion;
    p = std::round(p * 4.0) / 4.0;     // Viertelportionen
    return qBound(0.5, p, 3.0);
}

PlanEntry entryFor(const RecipeSummary &s, double target)
{
    PlanEntry e;
    e.recipeId = s.id;
    e.title = s.title;
    e.portions = portionsFor(s.perServing.kcal, target);
    e.kcal = s.perServing.kcal * e.portions;
    e.protein = s.perServing.protein * e.portions;
    e.fat = s.perServing.fat * e.portions;
    e.carbs = s.perServing.carbs * e.portions;
    return e;
}

const Candidate *pick(const QList<Candidate> &cands, Slot slot, double target, const QHash<QString, int> &usedWeek,
                      const QSet<QString> &usedDay, int maxUses)
{
    const Candidate *best = nullptr;
    double bestScore = 1e18;
    auto *rng = QRandomGenerator::global();
    for (int pass = 0; pass < 2 && !best; ++pass) {
        for (const Candidate &c : cands) {
            if (pass == 0 && !c.fits.contains(slot)) continue;
            if (usedDay.contains(c.s->id)) continue;
            const int used = usedWeek.value(c.s->id);
            if (used >= maxUses) continue;
            const double p = portionsFor(c.kcal, target);
            const double diff = std::fabs(c.kcal * p - target) / qMax(1.0, target);
            // ungewöhnliche Portionsgrößen leicht bestrafen
            const double portionPenalty = std::fabs(p - 1.0) * 0.08;
            const double score = diff + portionPenalty + used * 0.6 - c.bonus * 0.08 + rng->generateDouble() * 0.25;
            if (score < bestScore) {
                bestScore = score;
                best = &c;
            }
        }
    }
    return best;
}

} // namespace

double MealPlan::dayKcal(int day) const
{
    double k = 0;
    if (day < 0 || day >= days.size()) return 0;
    for (const PlanEntry &e : days[day]) k += e.kcal;
    return k;
}

QJsonObject MealPlan::toJson() const
{
    QJsonArray d;
    for (const QList<PlanEntry> &day : days) {
        QJsonArray m;
        for (const PlanEntry &e : day)
            m.append(QJsonObject{ { QStringLiteral("id"), e.recipeId },
                                  { QStringLiteral("title"), e.title },
                                  { QStringLiteral("portions"), e.portions },
                                  { QStringLiteral("kcal"), e.kcal },
                                  { QStringLiteral("protein"), e.protein },
                                  { QStringLiteral("fat"), e.fat },
                                  { QStringLiteral("carbs"), e.carbs } });
        d.append(m);
    }
    QJsonArray sh;
    for (double s : shares) sh.append(s);
    return QJsonObject{ { QStringLiteral("start"), start.toString(Qt::ISODate) },
                        { QStringLiteral("meals"), QJsonArray::fromStringList(meals) },
                        { QStringLiteral("shares"), sh },
                        { QStringLiteral("targetKcal"), targetKcal },
                        { QStringLiteral("profile"), profileId },
                        { QStringLiteral("days"), d } };
}

MealPlan MealPlan::fromJson(const QJsonObject &o)
{
    MealPlan p;
    p.start = QDate::fromString(o.value(QStringLiteral("start")).toString(), Qt::ISODate);
    for (const auto &v : o.value(QStringLiteral("meals")).toArray()) p.meals << v.toString();
    for (const auto &v : o.value(QStringLiteral("shares")).toArray()) p.shares << v.toDouble();
    p.targetKcal = o.value(QStringLiteral("targetKcal")).toDouble();
    p.profileId = o.value(QStringLiteral("profile")).toString();
    for (const auto &dv : o.value(QStringLiteral("days")).toArray()) {
        QList<PlanEntry> day;
        for (const auto &mv : dv.toArray()) {
            const QJsonObject e = mv.toObject();
            PlanEntry pe;
            pe.recipeId = e.value(QStringLiteral("id")).toString();
            pe.title = e.value(QStringLiteral("title")).toString();
            pe.portions = e.value(QStringLiteral("portions")).toDouble(1);
            pe.kcal = e.value(QStringLiteral("kcal")).toDouble();
            pe.protein = e.value(QStringLiteral("protein")).toDouble();
            pe.fat = e.value(QStringLiteral("fat")).toDouble();
            pe.carbs = e.value(QStringLiteral("carbs")).toDouble();
            day << pe;
        }
        p.days << day;
    }
    return p;
}

namespace MealPlanner {

Options fromProfile(const Profile &p)
{
    Options o;
    o.targetKcal = p.targetKcal();
    if (o.targetKcal <= 0) o.targetKcal = 2000;
    o.snack = p.snack;
    o.diet = p.diet;
    o.excluded = p.excluded;
    o.likedTags = p.likedTags;
    return o;
}

QStringList mealNames(bool snack)
{
    QStringList l{ tr("Frühstück"), tr("Mittagessen"), tr("Abendessen") };
    if (snack) l << tr("Zwischenmahlzeit");
    return l;
}

static QList<double> sharesFor(bool snack)
{
    return snack ? QList<double>{ 0.25, 0.35, 0.30, 0.10 } : QList<double>{ 0.25, 0.40, 0.35 };
}

int candidateCount(const Options &opt) { return int(candidates(opt).size()); }

MealPlan create(const Options &opt)
{
    MealPlan plan;
    plan.start = opt.start;
    plan.meals = mealNames(opt.snack);
    plan.shares = sharesFor(opt.snack);
    plan.targetKcal = opt.targetKcal;
    const QList<Candidate> cands = candidates(opt);
    QHash<QString, int> usedWeek;
    const Slot fits[] = { Breakfast, Lunch, Dinner, Snack };
    // Abwechslung: jedes Rezept höchstens zweimal – bei wenigen Rezepten entsprechend öfter
    const int slotsTotal = opt.days * int(plan.meals.size());
    const int maxUses = qMax(2, int(std::ceil(double(slotsTotal) / qMax(1, int(cands.size())))) + 1);
    for (int d = 0; d < opt.days; ++d) {
        QList<PlanEntry> day;
        QSet<QString> usedDay;
        for (int m = 0; m < plan.meals.size(); ++m) {
            const double target = opt.targetKcal * plan.shares[m];
            const Candidate *c = pick(cands, fits[m], target, usedWeek, usedDay, maxUses);
            if (!c) {
                day << PlanEntry{};
                continue;
            }
            usedWeek[c->s->id]++;
            usedDay.insert(c->s->id);
            day << entryFor(*c->s, target);
        }
        plan.days << day;
    }
    return plan;
}

void reroll(MealPlan &plan, int day, int meal, const Options &opt)
{
    if (day < 0 || day >= plan.days.size() || meal < 0 || meal >= plan.meals.size()) return;
    const QList<Candidate> cands = candidates(opt);
    QHash<QString, int> usedWeek;
    for (const auto &d : plan.days)
        for (const PlanEntry &e : d)
            if (!e.recipeId.isEmpty()) usedWeek[e.recipeId]++;
    QSet<QString> usedDay;
    for (const PlanEntry &e : plan.days[day]) usedDay.insert(e.recipeId);   // auch das bisherige ausschließen
    const Slot fits[] = { Breakfast, Lunch, Dinner, Snack };
    const double target = plan.targetKcal * plan.shares.value(meal, 0.33);
    const int maxUses = qMax(2, int(std::ceil(double(plan.days.size() * plan.meals.size()) / qMax(1, int(cands.size())))) + 1);
    if (const Candidate *c = pick(cands, fits[qMin(meal, 3)], target, usedWeek, usedDay, maxUses)) plan.days[day][meal] = entryFor(*c->s, target);
}

void setRecipe(MealPlan &plan, int day, int meal, const QString &recipeId)
{
    const RecipeSummary *s = RecipeDb::instance().summary(recipeId);
    if (!s || day < 0 || day >= plan.days.size() || meal < 0 || meal >= plan.days[day].size()) return;
    plan.days[day][meal] = entryFor(*s, plan.targetKcal * plan.shares.value(meal, 0.33));
}

QList<ShoppingItem> shoppingList(const MealPlan &plan, const QStringList &stock)
{
    QList<ShoppingItem> list;
    QHash<QString, double> portions;   // Rezept → Summe der Portionen
    QHash<QString, QString> titles;
    for (const auto &d : plan.days)
        for (const PlanEntry &e : d)
            if (!e.recipeId.isEmpty()) {
                portions[e.recipeId] += e.portions;
                titles[e.recipeId] = e.title;
            }
    for (auto it = portions.constBegin(); it != portions.constEnd(); ++it) {
        Recipe r;
        if (!RecipeDb::instance().load(it.key(), &r)) continue;
        const double factor = it.value() / (r.servings > 0 ? r.servings : 1.0);
        for (const Ingredient &i : r.ingredients) {
            if (i.heading || i.food == QLatin1String("wasser")) continue;
            if (!stock.isEmpty() && Fridge::inStock(i.name, i.food, stock)) continue;
            Shopping::add(list, i, factor, r.title);
        }
    }
    Shopping::sort(list);
    return list;
}

QString toHtml(const MealPlan &plan, const QString &personName)
{
    const QLocale de(QLocale::German);
    QString h = QStringLiteral("<h2>%1</h2>").arg(tr("Wochenplan für %1").arg(personName.toHtmlEscaped()));
    h += QStringLiteral("<p>%1</p>").arg(tr("Ziel: %1 kcal je Tag · ab %2").arg(qRound(plan.targetKcal)).arg(de.toString(plan.start, QStringLiteral("dddd, d. MMMM yyyy"))));
    h += QStringLiteral("<table cellspacing=\"0\" cellpadding=\"4\" border=\"1\" style=\"border-collapse:collapse;\"><tr><th>%1</th>").arg(tr("Tag"));
    for (const QString &m : plan.meals) h += QStringLiteral("<th>%1</th>").arg(m.toHtmlEscaped());
    h += QStringLiteral("<th>%1</th></tr>").arg(tr("Summe"));
    for (int d = 0; d < plan.days.size(); ++d) {
        h += QStringLiteral("<tr><td><b>%1</b></td>").arg(de.toString(plan.start.addDays(d), QStringLiteral("ddd d.M.")));
        for (const PlanEntry &e : plan.days[d]) {
            if (e.recipeId.isEmpty()) {
                h += QStringLiteral("<td>–</td>");
                continue;
            }
            h += QStringLiteral("<td>%1<br/><small>%2 · %3 kcal</small></td>")
                     .arg(e.title.toHtmlEscaped(), tr("%1 Port.").arg(Fmt::amount(e.portions, QString())))
                     .arg(qRound(e.kcal));
        }
        h += QStringLiteral("<td align=\"right\"><b>%1</b></td></tr>").arg(qRound(plan.dayKcal(d)));
    }
    h += QStringLiteral("</table>");
    return h;
}

} // namespace MealPlanner
