// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "mealplanner.h"
#include "foodtable.h"
#include "mealtypes.h"
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
    double health;         // Abzug (positiv = schlechter) je nach Ziel
};

// Gesundheits-Bewertung je nach Ziel (kleiner = besser)
double healthPenalty(const MealTypes::Input &in, Profile::Goal goal, bool sweetish)
{
    const double ed = MealTypes::energyDensity(in);
    const double sugar = MealTypes::sugarShare(in);
    const double prot = MealTypes::proteinShare(in);
    const double fiber = qMin(4.0, MealTypes::fiberPer100kcal(in));
    switch (goal) {
    case Profile::Goal::Lose:
        return (ed > 150 ? (ed - 150) / 100.0 * 0.35 : 0) + qMax(0.0, sugar - 0.15) * 1.6 - qMax(0.0, prot - 0.12) * 1.0 - fiber * 0.05
               + (sweetish ? 0.6 : 0);
    case Profile::Goal::Keep:
        return (ed > 200 ? (ed - 200) / 100.0 * 0.15 : 0) + qMax(0.0, sugar - 0.20) * 0.8 - qMax(0.0, prot - 0.12) * 0.4 - fiber * 0.03
               + (sweetish ? 0.25 : 0);
    case Profile::Goal::Gain:
        return (ed > 0 && ed < 120 ? (120 - ed) / 100.0 * 0.3 : 0) + qMax(0.0, sugar - 0.25) * 0.6 - qMax(0.0, prot - 0.12) * 0.6 - fiber * 0.02;
    }
    return 0;
}

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
        const MealTypes::Input in = MealTypes::from(s);
        const int types = MealTypes::classify(in);
        if (types & MealTypes::Basic && !(types & (MealTypes::Main | MealTypes::Breakfast | MealTypes::Snack))) continue;
        const bool dessert = types & (MealTypes::Dessert | MealTypes::Sweet);
        const bool baking = types & MealTypes::Baking;
        const bool breakfast = types & MealTypes::Breakfast;
        const bool main = types & MealTypes::Main;
        const bool snack = types & MealTypes::Snack;
        const bool lose = opt.goal == Profile::Goal::Lose;
        const double sugar = MealTypes::sugarShare(in);
        Candidate c{ &s, s.perServing.kcal, {}, 0, 0 };
        // Frühstück: nur ausgewiesene Frühstücksrezepte; beim Abnehmen nichts Süßes mit viel Zucker
        if (breakfast && !(lose && (dessert || sugar >= 0.40))) c.fits << Breakfast;
        // Mittag/Abend: Hauptgerichte, keine Desserts, Kuchen oder reinen Frühstücksrezepte
        if (main && !dessert && !(baking && !main)) c.fits << Lunch << Dinner;
        else if (!breakfast && !snack && !dessert && !baking && types == MealTypes::None) c.fits << Lunch << Dinner;
        // Zwischenmahlzeit: ausgewiesene Snacks oder leichte Frühstücksrezepte; Desserts/Kuchen nur ohne Abnehmziel
        if ((snack || (breakfast && s.perServing.kcal <= 320)) && !(lose && (dessert || baking || sugar >= 0.45))) c.fits << Snack;
        else if (!lose && (dessert || baking) && opt.goal == Profile::Goal::Gain) c.fits << Snack;
        if (c.fits.isEmpty()) continue;
        c.health = healthPenalty(in, opt.goal, dessert || baking);
        for (const QString &t : opt.likedTags)
            if (s.tags.contains(t.trimmed(), Qt::CaseInsensitive)) c.bonus += 1;
        if (s.favorite) c.bonus += 1;
        out << c;
    }
    return out;
}

// höchstens sinnvolle Essmengen je Mahlzeit (g) – sonst „1,3 kg Suppe“ bei sehr leichten Rezepten
double maxGrams(Slot slot)
{
    switch (slot) {
    case Breakfast: return 450;
    case Snack: return 350;
    default: return 800;
    }
}

double portionsFor(double kcalPerPortion, double target, Slot slot, double servingGrams)
{
    if (kcalPerPortion <= 0) return 1;
    double p = target / kcalPerPortion;
    const bool small = slot == Breakfast || slot == Snack;
    // Zwischenmahlzeit in halben, sonst in Viertelportionen
    p = slot == Snack ? std::round(p * 2.0) / 2.0 : std::round(p * 4.0) / 4.0;
    p = qBound(0.5, p, small ? 2.0 : 3.0);
    if (servingGrams > 0) {
        const double step = slot == Snack ? 0.5 : 0.25;
        while (p > 0.5 && p * servingGrams > maxGrams(slot)) p -= step;
    }
    return p;
}

PlanEntry entryFor(const RecipeSummary &s, double target, Slot slot)
{
    PlanEntry e;
    e.recipeId = s.id;
    e.title = s.title;
    e.portions = portionsFor(s.perServing.kcal, target, slot, s.servingGrams);
    e.grams = s.servingGrams * e.portions;
    e.kcal = s.perServing.kcal * e.portions;
    e.protein = s.perServing.protein * e.portions;
    e.fat = s.perServing.fat * e.portions;
    e.carbs = s.perServing.carbs * e.portions;
    e.sugar = s.perServing.sugar * e.portions;
    e.fiber = s.perServing.fiber * e.portions;
    return e;
}

const Candidate *pick(const QList<Candidate> &cands, Slot slot, double target, const QHash<QString, int> &usedWeek,
                      const QSet<QString> &usedDay, int maxUses)
{
    const Candidate *best = nullptr;
    double bestScore = 1e18;
    auto *rng = QRandomGenerator::global();
    // nur passende Rezepte – lieber ein leerer Platz mit Hinweis als Grießflammeri zum Frühstück
    for (int pass = 0; pass < 2 && !best; ++pass) {
        for (const Candidate &c : cands) {
            if (!c.fits.contains(slot)) continue;
            if (usedDay.contains(c.s->id)) continue;
            const int used = usedWeek.value(c.s->id);
            if (pass == 0 && used >= maxUses) continue;   // 2. Durchgang: Wiederholungen erlaubt
            const double p = portionsFor(c.kcal, target, slot, c.s->servingGrams);
            const double diff = std::fabs(c.kcal * p - target) / qMax(1.0, target);
            // ungewöhnliche Portionsgrößen leicht bestrafen
            const double portionPenalty = std::fabs(p - 1.0) * 0.08;
            const double score = diff * 1.6 + portionPenalty + used * 0.6 - c.bonus * 0.08 + c.health + rng->generateDouble() * 0.25;
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

Nutrients MealPlan::dayNutrients(int day) const
{
    Nutrients n;
    if (day < 0 || day >= days.size()) return n;
    for (const PlanEntry &e : days[day]) {
        n.kcal += e.kcal;
        n.protein += e.protein;
        n.fat += e.fat;
        n.carbs += e.carbs;
        n.sugar += e.sugar;
        n.fiber += e.fiber;
    }
    return n;
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
                                  { QStringLiteral("grams"), e.grams },
                                  { QStringLiteral("sugar"), e.sugar },
                                  { QStringLiteral("fiber"), e.fiber },
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
            pe.grams = e.value(QStringLiteral("grams")).toDouble();
            pe.sugar = e.value(QStringLiteral("sugar")).toDouble();
            pe.fiber = e.value(QStringLiteral("fiber")).toDouble();
            // ältere Pläne ohne Gewicht: aus der Datenbank nachtragen
            if (pe.grams <= 0 && !pe.recipeId.isEmpty())
                if (const RecipeSummary *rs = RecipeDb::instance().summary(pe.recipeId)) pe.grams = rs->servingGrams * pe.portions;
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
    o.goal = p.goal;
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

QList<double> shares(bool snack)
{
    return snack ? QList<double>{ 0.25, 0.35, 0.30, 0.10 } : QList<double>{ 0.30, 0.40, 0.30 };
}

int candidateCount(const Options &opt) { return int(candidates(opt).size()); }

QList<int> slotCounts(const Options &opt)
{
    QList<int> n{ 0, 0, 0, 0 };
    for (const Candidate &c : candidates(opt))
        for (Slot s : c.fits) n[int(s)]++;
    return n;
}

QStringList notes(const Options &opt)
{
    QStringList l;
    const QList<int> n = slotCounts(opt);
    if (n[Breakfast] == 0)
        l << tr("In der Datenbank gibt es kein passendes Frühstücksrezept (Tag „frühstück“). Legen Sie eigene an, vergeben Sie "
                "den Tag bei vorhandenen Rezepten oder übernehmen Sie die mitgelieferten gesunden Vorschläge.");
    else if (n[Breakfast] < 3)
        l << tr("Nur %1 Frühstücksrezept(e) – für mehr Abwechslung weitere mit dem Tag „frühstück“ anlegen.").arg(n[Breakfast]);
    if (opt.snack && n[Snack] == 0)
        l << tr("Es gibt keine passende Zwischenmahlzeit (Tag „zwischenmahlzeit“) – die mitgelieferten Vorschläge helfen.");
    if (n[Lunch] < 4) l << tr("Nur %1 Hauptgerichte passen – der Plan wiederholt sich.").arg(n[Lunch]);
    return l;
}

QString portionText(const PlanEntry &e)
{
    QString t = e.portions <= 1.0 ? tr("%1 Portion").arg(Fmt::amount(e.portions, QString())) : tr("%1 Portionen").arg(Fmt::amount(e.portions, QString()));
    if (e.grams >= 1) t += tr(" (ca. %1 g)").arg(qRound(e.grams / 5.0) * 5);
    return t;
}

MealPlan create(const Options &opt)
{
    MealPlan plan;
    plan.start = opt.start;
    plan.meals = mealNames(opt.snack);
    plan.shares = shares(opt.snack);
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
            day << entryFor(*c->s, target, fits[m]);
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
    if (const Candidate *c = pick(cands, fits[qMin(meal, 3)], target, usedWeek, usedDay, maxUses))
        plan.days[day][meal] = entryFor(*c->s, target, fits[qMin(meal, 3)]);
}

void setRecipe(MealPlan &plan, int day, int meal, const QString &recipeId)
{
    const RecipeSummary *s = RecipeDb::instance().summary(recipeId);
    if (!s || day < 0 || day >= plan.days.size() || meal < 0 || meal >= plan.days[day].size()) return;
    const Slot fits[] = { Breakfast, Lunch, Dinner, Snack };
    plan.days[day][meal] = entryFor(*s, plan.targetKcal * plan.shares.value(meal, 0.33), fits[qBound(0, meal, 3)]);
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
                     .arg(e.title.toHtmlEscaped(), portionText(e).toHtmlEscaped())
                     .arg(qRound(e.kcal));
        }
        const Nutrients n = plan.dayNutrients(d);
        h += QStringLiteral("<td align=\"right\"><b>%1 kcal</b><br/><small>%2</small></td></tr>")
                 .arg(qRound(n.kcal))
                 .arg(tr("E %1 g · Bst %2 g").arg(qRound(n.protein)).arg(qRound(n.fiber)));
    }
    h += QStringLiteral("</table>");
    h += QStringLiteral("<p>%1</p>").arg(summary(plan).toHtmlEscaped());
    return h;
}

QString summary(const MealPlan &plan)
{
    if (plan.days.isEmpty()) return {};
    Nutrients avg;
    int n = 0;
    for (int d = 0; d < plan.days.size(); ++d) {
        const Nutrients x = plan.dayNutrients(d);
        if (x.kcal <= 0) continue;
        avg.kcal += x.kcal;
        avg.protein += x.protein;
        avg.fat += x.fat;
        avg.carbs += x.carbs;
        avg.sugar += x.sugar;
        avg.fiber += x.fiber;
        ++n;
    }
    if (n == 0) return {};
    avg.kcal /= n;
    avg.protein /= n;
    avg.fat /= n;
    avg.carbs /= n;
    avg.sugar /= n;
    avg.fiber /= n;
    const auto pct = [&](double g, double f) { return avg.kcal > 0 ? qRound(g * f * 100.0 / avg.kcal) : 0; };
    QString t = tr("Durchschnitt je Tag: %1 kcal (Ziel %2) · Eiweiß %3 g (%4 %) · Fett %5 g (%6 %) · Kohlenhydrate %7 g (%8 %) · "
                   "Zucker %9 g · Ballaststoffe %10 g.")
                    .arg(qRound(avg.kcal))
                    .arg(qRound(plan.targetKcal))
                    .arg(qRound(avg.protein))
                    .arg(pct(avg.protein, 4))
                    .arg(qRound(avg.fat))
                    .arg(pct(avg.fat, 9))
                    .arg(qRound(avg.carbs))
                    .arg(pct(avg.carbs, 4))
                    .arg(qRound(avg.sugar))
                    .arg(qRound(avg.fiber));
    // Richtwerte (DGE): Eiweiß mind. ~15 %, Fett ≤ 30–35 %, Ballaststoffe ≥ 30 g, freier Zucker < 10 % der Energie
    QStringList hints;
    if (pct(avg.protein, 4) < 14) hints << tr("eher wenig Eiweiß");
    if (pct(avg.fat, 9) > 35) hints << tr("recht fettreich");
    if (avg.fiber < 25) hints << tr("Ballaststoffe unter dem Richtwert von 30 g – mehr Vollkorn, Gemüse, Hülsenfrüchte");
    if (avg.kcal > 0 && avg.sugar * 4 / avg.kcal > 0.15) hints << tr("viel Zucker");
    if (!hints.isEmpty()) t += QLatin1Char(' ') + tr("Hinweis: %1.").arg(hints.join(QStringLiteral("; ")));
    const double rest = plan.targetKcal - avg.kcal;
    if (rest > plan.targetKcal * 0.06)
        t += QLatin1Char(' ') + tr("Es bleiben rund %1 kcal je Tag frei – Platz für ein Stück Obst, ein Glas Milch oder eine kleine Handvoll Nüsse.")
                                    .arg(qRound(rest / 10.0) * 10);
    return t;
}

} // namespace MealPlanner
