// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "mealtypes.h"
#include "foodtable.h"
#include "nutrition.h"
#include "recipedb.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace MealTypes {

namespace {

QString tr(const char *s) { return QCoreApplication::translate("MealTypes", s); }

// Wort-/Wortteil-Suche in Titel und Tags (klein, Umlaute wie geschrieben)
bool containsAny(const QString &text, std::initializer_list<const char *> words)
{
    for (const char *w : words)
        if (text.contains(QString::fromUtf8(w))) return true;
    return false;
}

bool tagAny(const QStringList &tags, std::initializer_list<const char *> words)
{
    for (const QString &t : tags)
        for (const char *w : words)
            if (t.compare(QString::fromUtf8(w), Qt::CaseInsensitive) == 0) return true;
    return false;
}

bool hasMeatOrFish(const QStringList &foods)
{
    for (const QString &k : foods)
        if (const Food *f = FoodTable::instance().find(k))
            if (f->category == QLatin1String("fleisch") || f->category == QLatin1String("fisch")) return true;
    return false;
}

} // namespace

QString tagFor(Type t)
{
    switch (t) {
    case Breakfast: return QStringLiteral("frühstück");
    case Main: return QStringLiteral("hauptgerichte");
    case Snack: return QStringLiteral("zwischenmahlzeit");
    case Dessert: return QStringLiteral("desserts");
    case Sweet: return QStringLiteral("süßspeise");
    case Baking: return QStringLiteral("backen");
    default: return {};
    }
}

QStringList mealTags()
{
    return { tagFor(Breakfast), tagFor(Main), tagFor(Snack), tagFor(Dessert), tagFor(Sweet), tagFor(Baking) };
}

Input from(const Recipe &r)
{
    Input in;
    in.title = r.title;
    in.tags = r.tags;
    for (const Ingredient &i : r.ingredients) {
        if (i.heading) continue;
        in.ingredientNames << i.name.toLower();
        if (!i.food.isEmpty() && !in.foods.contains(i.food)) in.foods << i.food;
    }
    in.perServing = r.nutrition.perServing;
    in.hasNutrition = r.nutrition.valid() && r.nutrition.matched > 0;
    in.servingGrams = servingGrams(r);
    return in;
}

Input from(const RecipeSummary &s)
{
    Input in;
    in.title = s.title;
    in.tags = s.tags;
    in.ingredientNames = s.ingredientNames;
    in.foods = s.foods;
    in.perServing = s.perServing;
    in.hasNutrition = s.hasNutrition;
    in.servingGrams = s.servingGrams;
    return in;
}

double servingGrams(const Recipe &r)
{
    double g = 0;
    for (const Ingredient &i : r.ingredients)
        if (!i.heading && i.food != QLatin1String("wasser")) g += i.grams;
    // Wasser verdampft bzw. bleibt im Nudel-/Kartoffelkochwasser – nur bei Suppen/Brühen zählen
    for (const Ingredient &i : r.ingredients)
        if (i.food == QLatin1String("wasser") && (r.hasTag(QStringLiteral("suppen")) || r.hasTag(QStringLiteral("eintöpfe")))) g += i.grams;
    return r.servings > 0 ? g / r.servings : g;
}

int explicitTypes(const QStringList &tags)
{
    int t = None;
    if (tagAny(tags, { "frühstück", "brunch", "müsli" })) t |= Breakfast;
    if (tagAny(tags, { "hauptgerichte", "hauptgericht", "hauptspeise", "fleischgerichte", "eintöpfe", "aufläufe" })) t |= Main;
    if (tagAny(tags, { "zwischenmahlzeit", "snacks", "snack", "fingerfood" })) t |= Snack;
    if (tagAny(tags, { "desserts", "nachspeise", "nachtisch", "eis", "parfait" })) t |= Dessert;
    if (tagAny(tags, { "süßspeise", "süßspeisen", "süßigkeiten" })) t |= Sweet;
    if (tagAny(tags, { "backen", "kuchen", "torten", "gebäck", "brot", "plätzchen" })) t |= Baking;
    if (tagAny(tags, { "grundrezepte", "soßen", "saucen", "getränke", "dips", "marinade", "menüs" })) t |= Basic;
    return t;
}

int classify(const Input &in)
{
    int t = explicitTypes(in.tags);
    const QString title = in.title.toLower();
    const QString tagText = in.tags.join(QLatin1Char(' ')).toLower();
    const QString text = title + QLatin1Char(' ') + tagText;
    const bool meat = hasMeatOrFish(in.foods);
    const bool starter = tagAny(in.tags, { "vorspeisen", "vorspeise", "salate", "salat", "appetizers", "starters" });
    const double kcal = in.perServing.kcal;
    const double sugar = sugarShare(in);

    if (containsAny(text, { "frühstück", "müsli", "muesli", "porridge", "haferbrei", "overnight oats", "granola", "rührei",
                            "spiegelei", "omelett", "omelette", "frühstücksei", "smoothie bowl", "bircher", "brunch", "breakfast",
                            "french toast", "arme ritter", "frühstücks" }))
        t |= Breakfast;
    if (containsAny(text, { "dessert", "nachspeise", "nachtisch", "pudding", "mousse", "parfait", "sorbet", "kompott", "grütze",
                            "flammeri", "tiramisu", "panna cotta", "crumble", "zabaione", "eisbecher", "vanilleeis", "erdbeereis",
                            "creme brulee", "crème brûlée", "trifle", "quarkspeise", "götterspeise", "rote grütze" }) &&
        !starter && !meat)
        t |= Dessert;
    if (containsAny(text, { "süßspeise", "milchreis", "grießbrei", "griesbrei", "dampfnudel", "kaiserschmarrn", "germknödel",
                            "pfannkuchen mit apfel", "apfelstrudel", "waffel", "crêpe", "crepe", "schokoladen" }))
        t |= Sweet;
    // Pfannkuchen, Eierkuchen, Reibekuchen werden in der Pfanne gebacken – kein „Backen“
    QString bakeTitle = title;
    for (const char *pan : { "pfannkuchen", "eierkuchen", "reibekuchen", "kartoffelkuchen", "fischkuchen", "pancake" })
        bakeTitle.remove(QLatin1String(pan));
    if (!meat && containsAny(title, { "pfannkuchen", "eierkuchen", "pancake" }) && !containsAny(title, { "käse", "spinat", "schinken", "speck", "lachs", "pilz" }))
        t |= Sweet;
    if (containsAny(bakeTitle, { "kuchen", "torte", "gebäck", "plätzchen", "kekse", "cookies", "muffin", "brownie",
                            "cake", "bread", "pie", "tarte", "strudel", "stollen", "zopf", "hefeteig", "scones" }))
        t |= Baking;
    if ((t & Baking) && meat && !(explicitTypes(in.tags) & Baking)) t &= ~Baking;   // „Putenschnitzel mit Maisplätzchen“
    // „Bauernbrot“, „Dinkelbrötchen“ (aber nicht „Vollkornbrot mit Quark“)
    static const QRegularExpression bread(QStringLiteral("(brot|brötchen|baguette|ciabatta|semmeln)$"));
    if (title.trimmed().contains(bread)) t |= Baking;
    if (containsAny(text, { "snack", "zwischenmahlzeit", "dip", "aufstrich", "häppchen", "fingerfood", "riegel", "energy ball",
                            "energiebällchen", "gemüsesticks", "rohkost" }))
        t |= Snack;
    if (containsAny(text, { "grundrezept", "soße", "sauce", "dressing", "vinaigrette", "marinade", "fond", "gewürzmischung",
                            "getränk", "likör", "bowle", "punsch", "sirup", "pesto", "menü " }) &&
        !(t & (Main | Breakfast | Dessert)))
        t |= Basic;
    if (title.startsWith(QStringLiteral("menü"))) t |= Basic;
    if (containsAny(text, { "hauptgericht", "hauptspeise", "auflauf", "eintopf", "gulasch", "braten", "schnitzel", "geschnetzeltes",
                            "lasagne", "risotto", "curry", "chili con", "pfanne", "gratin", "frikadelle", "bulette", "burger",
                            "pizza", "spaghetti", "nudeln", "pasta", "spätzle", "knödel mit", "rouladen", "filet", "steak",
                            "ragout", "frikassee", "eintöpfe", "aufläufe", "fleischgerichte", "geflügel", "fisch", "suppe",
                            "bratkartoffeln", "reispfanne", "bratlinge", "küchle", "gemüse-", "stew", "casserole", "main dish" }) &&
        !(t & (Dessert | Baking | Basic)) && !(starter && in.perServing.kcal > 0 && in.perServing.kcal < 350))
        t |= Main;
    // nach Nährwerten (nur, wenn nichts eindeutig ist)
    if (in.hasNutrition && !(t & (Main | Dessert | Sweet | Baking | Basic | Breakfast | Snack))) {
        if (starter && kcal < 350) t |= Snack;
        else if (meat && kcal >= 250) t |= Main;
        else if (sugar >= 0.30 && !meat && kcal >= 120) t |= Sweet;
        else if (kcal >= 350) t |= Main;
        else if (kcal > 0 && kcal < 250) t |= Snack;
    }
    // Süßes mit viel Zucker, aber ohne eigenes Tag → zusätzlich Süßspeise
    if ((t & Dessert) && in.hasNutrition && sugar >= 0.35) t |= Sweet;
    return t;
}

QStringList suggestTags(const Recipe &r)
{
    const Input in = from(r);
    const int t = classify(in);
    QStringList out;
    for (Type x : { Breakfast, Main, Snack, Dessert, Sweet, Baking })
        if ((t & x) && !r.hasTag(tagFor(x))) out << tagFor(x);
    return out;
}

int applyTags(Recipe &r)
{
    const QStringList s = suggestTags(r);
    for (const QString &t : s) r.addTag(t);
    return int(s.size());
}

QStringList nutrientTagNames()
{
    return { QStringLiteral("eiweißreich"), QStringLiteral("kalorienarm"), QStringLiteral("fettarm"),
             QStringLiteral("hochkalorisch"), QStringLiteral("vitaminreich"), QStringLiteral("reduktionsdiät") };
}

double produceGrams(const Recipe &r)
{
    double g = 0;
    for (const Ingredient &i : r.ingredients) {
        if (i.heading || i.food.isEmpty()) continue;
        const Food *f = FoodTable::instance().find(i.food);
        if (!f) continue;
        // Kartoffeln zählen nicht (stärkereich), Trockenobst/Konfitüre stehen nicht unter „obst“/„gemuese“ bzw. werden ausgenommen
        if ((f->category == QLatin1String("gemuese") || f->category == QLatin1String("obst") || f->category == QLatin1String("kraeuter"))
            && f->key != QLatin1String("rosinen") && f->key != QLatin1String("trockenobst") && f->key != QLatin1String("oliven")
            && f->key != QLatin1String("avocado") && f->key != QLatin1String("apfelmus"))
            g += i.grams;
        else if (f->key == QLatin1String("avocado"))
            g += i.grams;   // fettreich, aber vitaminreich
    }
    return r.servings > 0 ? g / r.servings : g;
}

QStringList nutrientTags(const Recipe &r)
{
    QStringList t;
    if (Nutrition::isMenu(r) || !r.nutrition.valid() || r.nutrition.matched == 0 || r.nutrition.coverage() < 0.8) return t;
    const Input in = from(r);
    const Nutrients &p = r.nutrition.perServing;
    if (p.kcal <= 0) return t;
    const double grams = in.servingGrams;
    const double per100 = grams > 20 ? 100.0 / grams : 0;      // Umrechnung Portion → 100 g
    const double ed = energyDensity(in);
    const double fatShare = p.fat * 9.0 / p.kcal;
    const int types = classify(in);
    if (proteinShare(in) >= 0.20 && p.protein >= 15) t << QStringLiteral("eiweißreich");
    if (per100 > 0 && p.fat * per100 <= 3.0) t << QStringLiteral("fettarm");
    if (ed > 0 && ed <= 80 && p.kcal <= 400) t << QStringLiteral("kalorienarm");
    if ((ed >= 250) || p.kcal >= 800) t << QStringLiteral("hochkalorisch");
    const double produce = produceGrams(r);
    // Kuchen und Gebäck nie; Desserts/Süßspeisen nur mit viel Obst (≥ 200 g je Portion)
    const bool sweet = types & (Dessert | Sweet);
    if (!(types & Baking) && (produce >= 200 || (!sweet && grams > 0 && produce >= 0.5 * grams && produce >= 100)))
        t << QStringLiteral("vitaminreich");
    if (p.kcal <= 600 && ed > 0 && ed <= 150 && fatShare <= 0.35 && sugarShare(in) <= 0.20 && proteinShare(in) >= 0.15
        && !(types & (Dessert | Sweet | Baking)))
        t << QStringLiteral("reduktionsdiät");
    return t;
}

int applyNutrientTags(Recipe &r)
{
    // ohne (ausreichende) Nährwerte nichts ändern – auch vorhandene Tags bleiben
    if (Nutrition::isMenu(r) || !r.nutrition.valid() || r.nutrition.matched == 0 || r.nutrition.coverage() < 0.8 || r.nutrition.perServing.kcal <= 0)
        return 0;
    const QStringList want = nutrientTags(r);
    int changes = 0;
    for (const QString &t : nutrientTagNames()) {
        const bool has = r.hasTag(t);
        if (want.contains(t) && !has) {
            r.addTag(t);
            ++changes;
        } else if (!want.contains(t) && has) {
            r.tags.removeAll(t);
            ++changes;
        }
    }
    return changes;
}

double energyDensity(const Input &in)
{
    if (in.servingGrams <= 20 || in.perServing.kcal <= 0) return 0;
    return in.perServing.kcal * 100.0 / in.servingGrams;
}

double sugarShare(const Input &in)
{
    return in.perServing.kcal > 0 ? qBound(0.0, in.perServing.sugar * 4.0 / in.perServing.kcal, 1.0) : 0;
}

double proteinShare(const Input &in)
{
    return in.perServing.kcal > 0 ? qBound(0.0, in.perServing.protein * 4.0 / in.perServing.kcal, 1.0) : 0;
}

double fiberPer100kcal(const Input &in)
{
    return in.perServing.kcal > 0 ? in.perServing.fiber * 100.0 / in.perServing.kcal : 0;
}

QStringList healthNotes(const Input &in)
{
    QStringList n;
    if (!in.hasNutrition) return n;
    const double ed = energyDensity(in);
    if (ed > 0 && ed < 120) n << tr("leicht");
    else if (ed > 250) n << tr("energiereich");
    if (proteinShare(in) >= 0.20) n << tr("eiweißreich");
    if (fiberPer100kcal(in) >= 2.0) n << tr("ballaststoffreich");
    if (sugarShare(in) >= 0.40) n << tr("viel Zucker");
    return n;
}

} // namespace MealTypes
