// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "mealplanner.h"
#include "foodtable.h"
#include "mealtypes.h"
#include "recipedb.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QLocale>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <cmath>

namespace {
QString tr(const char *s) { return QCoreApplication::translate("MealPlanner", s); }

using Role = MealPlanner::Role;
constexpr Role Breakfast = Role::Breakfast, Warm = Role::Warm, Light = Role::Light, Snack = Role::Snack;
using Slot = Role;

// Merkmale für die Abfolge: Sättigungsbeilage und Eiweißquelle
struct Features {
    QSet<QString> starch;   // nudeln, reis, kartoffel, brot, getreide
    bool meat = false, fish = false, legumes = false;
};

Features featuresOf(const QStringList &foods)
{
    Features f;
    for (const QString &k : foods) {
        const Food *food = FoodTable::instance().find(k);
        if (!food) continue;
        const QString &c = food->category;
        if (c == QLatin1String("fleisch")) f.meat = true;
        else if (c == QLatin1String("fisch")) f.fish = true;
        else if (c == QLatin1String("huelsen") && k != QLatin1String("sojadrink") && k != QLatin1String("sojasahne")) f.legumes = true;
        else if (c == QLatin1String("kartoffel")) f.starch.insert(QStringLiteral("kartoffel"));
        else if (k.contains(QLatin1String("nudel")) || k == QLatin1String("spaetzle")) f.starch.insert(QStringLiteral("nudeln"));
        else if (k.contains(QLatin1String("reis"))) f.starch.insert(QStringLiteral("reis"));
        else if (k == QLatin1String("brot") || k == QLatin1String("vollkornbrot") || k == QLatin1String("broetchen")
                 || k == QLatin1String("knaeckebrot") || k == QLatin1String("toastbrot") || k == QLatin1String("tortilla"))
            f.starch.insert(QStringLiteral("brot"));
        else if (k == QLatin1String("couscous") || k == QLatin1String("bulgur") || k == QLatin1String("quinoa") || k == QLatin1String("hirse"))
            f.starch.insert(QStringLiteral("getreide"));
    }
    return f;
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
    double health;         // Abzug (positiv = schlechter) je nach Ziel
    Features feat;
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

QList<Candidate> candidates(const MealPlanner::Options &opt, const MealPlanner::Progress &progress = {}, int span = 0, bool *canceled = nullptr)
{
    QList<Candidate> out;
    const QList<RecipeSummary> &all = RecipeDb::instance().summaries();
    const int total = int(all.size());
    int i = 0;
    for (const RecipeSummary &s : all) {
        // Fortschritt: Rezepte prüfen = die ersten „span“ Schritte
        if (progress && (i % 25 == 0) && !progress(span > 0 ? i * span / qMax(1, total) : i, span > 0 ? span + 100 : total,
                                                   tr("Rezepte werden geprüft (%1 von %2) …").arg(i).arg(total))) {
            if (canceled) *canceled = true;
            return {};
        }
        ++i;
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
        Candidate c{ &s, s.perServing.kcal, {}, 0, 0, featuresOf(s.foods) };
        const bool light = types & MealTypes::Light;
        const bool gain = opt.goal == Profile::Goal::Gain;
        // Frühstück: nur ausgewiesene Frühstücksrezepte; beim Abnehmen nichts Süßes mit viel Zucker
        if (breakfast && !(lose && (dessert || sugar >= 0.40))) c.fits << Breakfast;
        // warme Hauptmahlzeit: Hauptgerichte, keine Desserts, Kuchen oder reinen Frühstücksrezepte und nichts Kaltes
        static const QRegularExpression cold(QStringLiteral(
            "salat|brot|stulle|quark|rohkost|carpaccio|antipasti|wrap|sandwich|gazpacho|bruschetta|hüttenkäse|tatar|salad"),
            QRegularExpression::CaseInsensitiveOption);
        const bool coldDish = light && s.title.contains(cold);
        if (main && !dessert && !(baking && !main) && !coldDish) c.fits << Warm;
        else if (!breakfast && !snack && !dessert && !baking && !light && types == MealTypes::None) c.fits << Warm;
        // leichte Mahlzeit (Abendbrot, Salat, Suppe …); beim Abnehmen nichts Zuckerreiches
        if (light && !dessert && !(lose && sugar >= 0.30)) c.fits << Light;
        // Zwischenmahlzeit: ausgewiesene Snacks oder leichte Frühstücksrezepte; Desserts/Kuchen nur beim Zunehmen
        if ((snack || (breakfast && s.perServing.kcal <= 320)) && !(lose && (dessert || baking || sugar >= 0.45))) c.fits << Snack;
        else if ((dessert || baking) && gain) c.fits << Snack;
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
    case Light: return 500;
    default: return 750;
    }
}

double portionsFor(double kcalPerPortion, double target, Slot slot, double servingGrams)
{
    if (kcalPerPortion <= 0) return 1;
    double p = target / kcalPerPortion;
    const bool small = slot == Breakfast || slot == Snack || slot == Light;
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

// Stand der Planung für die Abfolge-Regeln
struct Context {
    QHash<QString, int> usedWeek;
    QSet<QString> usedDay;
    QSet<QString> yesterday;              // Rezepte des Vortags
    QList<Features> mainsToday;           // bereits gewählte Haupt-/leichte Mahlzeiten des Tages
    int fish = 0, legumes = 0, meat = 0;  // bisher in der Woche
    int fishMin = 1, fishMax = 2, legumesMin = 1, legumesMax = 3, meatMax = 3;
    bool lightFallback = false;           // zu wenige leichte Rezepte → kleine Hauptgerichte zulassen
    int maxUses = 2;
};

const Candidate *pick(const QList<Candidate> &cands, Slot slot, double target, const Context &ctx)
{
    const Candidate *best = nullptr;
    double bestScore = 1e18;
    auto *rng = QRandomGenerator::global();
    const bool mainMeal = slot == Warm || slot == Light;
    // nur passende Rezepte – lieber ein leerer Platz mit Hinweis als Grießflammeri zum Frühstück
    for (int pass = 0; pass < 2 && !best; ++pass) {
        for (const Candidate &c : cands) {
            double extra = 0;
            if (!c.fits.contains(slot)) {
                // Notlösung: kleines Hauptgericht als leichte Mahlzeit
                if (!(slot == Light && ctx.lightFallback && c.fits.contains(Warm))) continue;
                extra += 0.5;
            }
            if (ctx.usedDay.contains(c.s->id)) continue;
            const int used = ctx.usedWeek.value(c.s->id);
            if (pass == 0 && used >= ctx.maxUses) continue;   // 2. Durchgang: Wiederholungen erlaubt
            const double p = portionsFor(c.kcal, target, slot, c.s->servingGrams);
            const double diff = std::fabs(c.kcal * p - target) / qMax(1.0, target);
            // ungewöhnliche Portionsgrößen leicht bestrafen
            const double portionPenalty = std::fabs(p - 1.0) * 0.08;
            if (ctx.yesterday.contains(c.s->id)) extra += 0.5;
            // Hauptmahlzeit, die selbst mit der größten sinnvollen Menge deutlich zu wenig liefert (leichte Suppe als
            // warmes Mittagessen) – sonst bleibt der Tag weit unter dem Ziel
            const double under = (target - c.kcal * p) / qMax(1.0, target);
            if (mainMeal && under > 0.15) extra += (under - 0.15) * 2.5;
            if (mainMeal) {
                // Abfolge im Tag: keine gleiche Beilage, nicht zweimal Fleisch bzw. Fisch
                for (const Features &f : ctx.mainsToday) {
                    if (!(f.starch & c.feat.starch).isEmpty()) extra += 0.6;
                    if (f.meat && c.feat.meat) extra += 0.6;
                    if (f.fish && c.feat.fish) extra += 0.6;
                    if (f.legumes && c.feat.legumes) extra += 0.45;
                }
                // Wochenrichtwerte (DGE)
                if (c.feat.fish) extra += ctx.fish < ctx.fishMin ? -0.35 : ctx.fish < ctx.fishMax ? -0.1 : 0.7;
                if (c.feat.legumes) extra += ctx.legumes < ctx.legumesMin ? -0.35 : ctx.legumes < ctx.legumesMax ? -0.1 : 0.35;
                if (c.feat.meat) extra += ctx.meat >= ctx.meatMax ? 0.8 : 0.05;
            }
            const double score = diff * 1.6 + portionPenalty + used * 0.6 - c.bonus * 0.08 + c.health + extra + rng->generateDouble() * 0.25;
            if (score < bestScore) {
                bestScore = score;
                best = &c;
            }
        }
    }
    return best;
}

int lightCount(const QList<Candidate> &cands)
{
    int n = 0;
    for (const Candidate &c : cands)
        if (c.fits.contains(Light)) ++n;
    return n;
}

void scaleQuotas(Context &ctx, int days)
{
    const double w = qMax(1, days) / 7.0;
    ctx.fishMin = days >= 4 ? 1 : 0;
    ctx.fishMax = qMax(1, int(std::round(2 * w)));
    ctx.legumesMin = days >= 4 ? qMax(1, int(std::round(1 * w))) : 0;
    ctx.legumesMax = qMax(1, int(std::round(3 * w)));
    ctx.meatMax = qMax(1, int(std::round(3 * w)));
}

void count(Context &ctx, const Features &f)
{
    if (f.fish) ++ctx.fish;
    if (f.legumes) ++ctx.legumes;
    if (f.meat) ++ctx.meat;
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
    QJsonArray ro;
    for (MealPlanner::Role r : roles) ro.append(int(r));
    return QJsonObject{ { QStringLiteral("start"), start.toString(Qt::ISODate) },
                        { QStringLiteral("meals"), QJsonArray::fromStringList(meals) },
                        { QStringLiteral("shares"), sh },
                        { QStringLiteral("roles"), ro },
                        { QStringLiteral("goal"), goal },
                        { QStringLiteral("diet"), diet },
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
    for (const auto &v : o.value(QStringLiteral("roles")).toArray()) p.roles << MealPlanner::Role(qBound(0, v.toInt(), 3));
    // Pläne vor 0.6.1: Frühstück, Mittag, Abend (beide warm), Zwischenmahlzeit
    if (p.roles.size() != p.meals.size()) {
        p.roles.clear();
        const MealPlanner::Role legacy[] = { MealPlanner::Role::Breakfast, MealPlanner::Role::Warm, MealPlanner::Role::Warm, MealPlanner::Role::Snack };
        for (int i = 0; i < p.meals.size(); ++i) p.roles << legacy[qMin(i, 3)];
    }
    p.goal = o.value(QStringLiteral("goal")).toInt(1);
    p.diet = o.value(QStringLiteral("diet")).toInt(0);
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
    o.warmEvening = p.warmEvening;
    o.goal = p.goal;
    o.diet = p.diet;
    o.excluded = p.excluded;
    o.likedTags = p.likedTags;
    return o;
}

Layout layout(const Options &opt)
{
    Layout l;
    const QString breakfast = tr("Frühstück"), lunch = tr("Mittagessen"), dinner = tr("Abendessen");
    const QString snackAm = tr("Zwischenmahlzeit vormittags"), snackPm = tr("Zwischenmahlzeit nachmittags"), late = tr("Spätmahlzeit");
    const Role lunchRole = opt.warmEvening ? Light : Warm, dinnerRole = opt.warmEvening ? Warm : Light;
    auto add = [&l](const QString &n, double share, Role r) {
        l.names << n;
        l.shares << share;
        l.roles << r;
    };
    switch (opt.goal) {
    case Profile::Goal::Gain:
        // 5–6 Mahlzeiten, beide Hauptmahlzeiten dürfen warm sein
        if (opt.snack) {
            add(breakfast, 0.20, Breakfast);
            add(snackAm, 0.10, Snack);
            add(lunch, 0.28, Warm);
            add(snackPm, 0.10, Snack);
            add(dinner, 0.24, Warm);
            add(late, 0.08, Snack);
        } else {
            add(breakfast, 0.22, Breakfast);
            add(snackAm, 0.10, Snack);
            add(lunch, 0.32, Warm);
            add(snackPm, 0.11, Snack);
            add(dinner, 0.25, Warm);
        }
        break;
    case Profile::Goal::Lose: {
        const double warm = opt.snack ? 0.40 : 0.45, light = 0.25, bf = opt.snack ? 0.25 : 0.30;
        add(breakfast, bf, Breakfast);
        add(lunch, lunchRole == Warm ? warm : light, lunchRole);
        if (opt.snack) add(snackPm, 0.10, Snack);
        add(dinner, dinnerRole == Warm ? warm : light, dinnerRole);
        break;
    }
    case Profile::Goal::Keep: {
        const double warm = opt.snack ? 0.35 : 0.40, light = 0.30, bf = opt.snack ? 0.25 : 0.30;
        add(breakfast, bf, Breakfast);
        add(lunch, lunchRole == Warm ? warm : light, lunchRole);
        if (opt.snack) add(snackPm, 0.10, Snack);
        add(dinner, dinnerRole == Warm ? warm : light, dinnerRole);
        break;
    }
    }
    return l;
}

QString roleName(Role r)
{
    switch (r) {
    case Role::Breakfast: return tr("Frühstück");
    case Role::Warm: return tr("warm");
    case Role::Light: return tr("leicht");
    case Role::Snack: return tr("klein");
    }
    return {};
}

int candidateCount(const Options &opt, const Progress &progress) { return int(candidates(opt, progress).size()); }

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
    const Layout lay = layout(opt);
    if (n[int(Breakfast)] == 0)
        l << tr("In der Datenbank gibt es kein passendes Frühstücksrezept (Tag „frühstück“). Legen Sie eigene an, vergeben Sie "
                "den Tag bei vorhandenen Rezepten oder übernehmen Sie die mitgelieferten gesunden Vorschläge.");
    else if (n[int(Breakfast)] < 3)
        l << tr("Nur %1 Frühstücksrezept(e) – für mehr Abwechslung weitere mit dem Tag „frühstück“ anlegen.").arg(n[int(Breakfast)]);
    if (lay.roles.contains(Snack) && n[int(Snack)] == 0)
        l << tr("Es gibt keine passende Zwischenmahlzeit (Tag „zwischenmahlzeit“) – die mitgelieferten Vorschläge helfen.");
    if (lay.roles.contains(Light) && n[int(Light)] < 3)
        l << tr("Nur %1 leichte Rezepte (Tag „leichte mahlzeit“: Salat, Suppe, Brotzeit, Quark …) – als zweite Mahlzeit des Tages "
                "werden deshalb auch kleine Portionen von Hauptgerichten verplant. Die mitgelieferten Vorschläge enthalten "
                "leichte Abendessen.").arg(n[int(Light)]);
    if (n[int(Warm)] < 4) l << tr("Nur %1 Hauptgerichte passen – der Plan wiederholt sich.").arg(n[int(Warm)]);
    return l;
}

QString portionText(const PlanEntry &e)
{
    QString t = e.portions <= 1.0 ? tr("%1 Portion").arg(Fmt::amount(e.portions, QString())) : tr("%1 Portionen").arg(Fmt::amount(e.portions, QString()));
    if (e.grams >= 1) t += tr(" (ca. %1 g)").arg(qRound(e.grams / 5.0) * 5);
    return t;
}

MealPlan create(const Options &opt, const Progress &progress)
{
    // Fortschritt: 0–900 Rezepte prüfen, 900–1000 Tage zusammenstellen
    constexpr int kScan = 900;
    MealPlan plan;
    plan.start = opt.start;
    const Layout lay = layout(opt);
    plan.meals = lay.names;
    plan.shares = lay.shares;
    plan.roles = lay.roles;
    plan.goal = int(opt.goal);
    plan.diet = int(opt.diet);
    plan.targetKcal = opt.targetKcal;
    bool canceled = false;
    const QList<Candidate> cands = candidates(opt, progress, kScan, &canceled);
    if (canceled) return MealPlan();
    Context ctx;
    scaleQuotas(ctx, opt.days);
    ctx.lightFallback = lightCount(cands) < 3;
    // Abwechslung: jedes Rezept höchstens zweimal – bei wenigen Rezepten entsprechend öfter
    const int slotsTotal = opt.days * int(plan.meals.size());
    ctx.maxUses = qMax(2, int(std::ceil(double(slotsTotal) / qMax(1, int(cands.size())))) + 1);
    // Reihenfolge der Auswahl: zuerst die warme Mahlzeit, dann die leichte passend dazu
    QList<int> order;
    for (Role r : { Warm, Light, Breakfast, Snack })
        for (int m = 0; m < plan.roles.size(); ++m)
            if (plan.roles[m] == r) order << m;
    QSet<QString> previous;
    for (int d = 0; d < opt.days; ++d) {
        if (progress && !progress(kScan + 100 * d / qMax(1, opt.days), kScan + 100, tr("Tag %1 von %2 wird zusammengestellt …").arg(d + 1).arg(opt.days)))
            return MealPlan();
        QList<PlanEntry> day;
        for (int m = 0; m < plan.meals.size(); ++m) day << PlanEntry{};
        ctx.usedDay.clear();
        ctx.mainsToday.clear();
        ctx.yesterday = previous;
        previous.clear();
        for (int m : order) {
            const double target = opt.targetKcal * plan.shares[m];
            const Role role = plan.roles[m];
            const Candidate *c = pick(cands, role, target, ctx);
            if (!c) continue;
            ctx.usedWeek[c->s->id]++;
            ctx.usedDay.insert(c->s->id);
            previous.insert(c->s->id);
            if (role == Warm || role == Light) {
                ctx.mainsToday << c->feat;
                count(ctx, c->feat);
            }
            day[m] = entryFor(*c->s, target, role);
        }
        plan.days << day;
    }
    if (progress) progress(kScan + 100, kScan + 100, tr("Fertig"));
    return plan;
}

void reroll(MealPlan &plan, int day, int meal, const Options &opt, const Progress &progress)
{
    if (day < 0 || day >= plan.days.size() || meal < 0 || meal >= plan.meals.size()) return;
    bool canceled = false;
    const QList<Candidate> cands = candidates(opt, progress, 0, &canceled);
    if (canceled) return;
    const Role role = plan.roles.value(meal, Warm);
    Context ctx;
    scaleQuotas(ctx, int(plan.days.size()));
    ctx.lightFallback = lightCount(cands) < 3;
    for (int d = 0; d < plan.days.size(); ++d)
        for (int m = 0; m < plan.days[d].size(); ++m) {
            const PlanEntry &e = plan.days[d][m];
            if (e.recipeId.isEmpty() || (d == day && m == meal)) continue;
            ctx.usedWeek[e.recipeId]++;
            const Role r = plan.roles.value(m, Warm);
            if (r != Warm && r != Light) continue;
            if (const RecipeSummary *s = RecipeDb::instance().summary(e.recipeId)) {
                const Features f = featuresOf(s->foods);
                count(ctx, f);
                if (d == day) ctx.mainsToday << f;
            }
        }
    for (const PlanEntry &e : plan.days[day]) ctx.usedDay.insert(e.recipeId);   // auch das bisherige ausschließen
    if (day > 0)
        for (const PlanEntry &e : plan.days[day - 1]) ctx.yesterday.insert(e.recipeId);
    ctx.maxUses = qMax(2, int(std::ceil(double(plan.days.size() * plan.meals.size()) / qMax(1, int(cands.size())))) + 1);
    const double target = plan.targetKcal * plan.shares.value(meal, 0.33);
    if (const Candidate *c = pick(cands, role, target, ctx)) plan.days[day][meal] = entryFor(*c->s, target, role);
}

void setRecipe(MealPlan &plan, int day, int meal, const QString &recipeId)
{
    const RecipeSummary *s = RecipeDb::instance().summary(recipeId);
    if (!s || day < 0 || day >= plan.days.size() || meal < 0 || meal >= plan.days[day].size()) return;
    plan.days[day][meal] = entryFor(*s, plan.targetKcal * plan.shares.value(meal, 0.33), plan.roles.value(meal, Warm));
}

WeekCheck weekCheck(const MealPlan &plan)
{
    WeekCheck w;
    Context ctx;
    scaleQuotas(ctx, int(plan.days.size()));
    w.fishMin = ctx.fishMin;
    w.fishMax = ctx.fishMax;
    w.legumesMin = ctx.legumesMin;
    w.meatMax = ctx.meatMax;
    for (const auto &day : plan.days)
        for (int m = 0; m < day.size(); ++m) {
            if (day[m].recipeId.isEmpty()) continue;
            if (const RecipeSummary *s = RecipeDb::instance().summary(day[m].recipeId)) {
                const Features f = featuresOf(s->foods);
                if (f.fish) ++w.fish;
                if (f.legumes) ++w.legumes;
                if (f.meat) ++w.meat;
            }
        }
    return w;
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
    // Wochenrichtwerte (DGE) – nur, wenn die Ernährungsform sie betrifft
    if (plan.days.size() >= 4) {
        const WeekCheck w = weekCheck(plan);
        QStringList wk;
        const int days = int(plan.days.size());
        if (plan.diet == int(Profile::Diet::All)) {
            wk << tr("Fisch %1× (Richtwert %2–%3×)%4").arg(w.fish).arg(w.fishMin).arg(w.fishMax)
                      .arg(w.fish >= w.fishMin && w.fish <= w.fishMax ? QStringLiteral(" ✓") : QString());
            wk << tr("Fleisch %1× (höchstens etwa %2× bzw. 300 g pro Woche)%3").arg(w.meat).arg(w.meatMax)
                      .arg(w.meat <= w.meatMax ? QStringLiteral(" ✓") : QString());
        }
        wk << tr("Hülsenfrüchte %1× (mindestens %2×)%3").arg(w.legumes).arg(w.legumesMin)
                  .arg(w.legumes >= w.legumesMin ? QStringLiteral(" ✓") : QString());
        t += QLatin1Char(' ') + tr("In %1 Tagen (DGE): %2.").arg(days).arg(wk.join(QStringLiteral(" · ")));
    }
    const double rest = plan.targetKcal - avg.kcal;
    if (rest > plan.targetKcal * 0.06)
        t += QLatin1Char(' ') + tr("Es bleiben rund %1 kcal je Tag frei – Platz für ein Stück Obst, ein Glas Milch oder eine kleine Handvoll Nüsse.")
                                    .arg(qRound(rest / 10.0) * 10);
    return t;
}

} // namespace MealPlanner
