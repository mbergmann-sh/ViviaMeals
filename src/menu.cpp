// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "menu.h"
#include "foodtable.h"
#include "mealtypes.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <cmath>

namespace Menu {

namespace {

QString tr(const char *s) { return QCoreApplication::translate("Menu", s); }

bool containsAny(const QString &text, std::initializer_list<const char *> words)
{
    for (const char *w : words)
        if (text.contains(QString::fromUtf8(w))) return true;
    return false;
}

const char *courseKey(Course c)
{
    switch (c) {
    case Course::Starter: return "vorspeise";
    case Course::Soup: return "suppe";
    case Course::Intermediate: return "zwischengang";
    case Course::Main: return "hauptgang";
    case Course::Dessert: return "dessert";
    }
    return "hauptgang";
}

Course courseFromKey(const QString &k)
{
    for (Course c : { Course::Starter, Course::Soup, Course::Intermediate, Course::Main, Course::Dessert })
        if (k == QLatin1String(courseKey(c))) return c;
    return Course::Main;
}

double courseShare(Course c)
{
    switch (c) {
    case Course::Starter: return 0.15;
    case Course::Soup: return 0.12;
    case Course::Intermediate: return 0.18;
    case Course::Main: return 0.38;
    case Course::Dessert: return 0.17;
    }
    return 0.2;
}

// Tags, die nichts über Küche/Thema sagen
bool genericTag(const QString &t)
{
    static const QSet<QString> skip{ QStringLiteral("vegan"), QStringLiteral("vegetarisch"), QStringLiteral("fleischgerichte"), QStringLiteral("fisch"),
                                     QStringLiteral("menüs"), QStringLiteral("vorspeisen"), QStringLiteral("suppen"), QStringLiteral("salate"),
                                     QStringLiteral("beilagen"), QStringLiteral("geflügel"), QStringLiteral("zwischengang"),
                                     QStringLiteral("hauptgerichte"), QStringLiteral("desserts"), QStringLiteral("leichte mahlzeit"), QStringLiteral("eintopf") };
    return skip.contains(t) || MealTypes::mealTags().contains(t) || MealTypes::nutrientTagNames().contains(t);
}

} // namespace

QString courseName(Course c)
{
    switch (c) {
    case Course::Starter: return tr("Vorspeise");
    case Course::Soup: return tr("Suppe");
    case Course::Intermediate: return tr("Zwischengang");
    case Course::Main: return tr("Hauptgang");
    case Course::Dessert: return tr("Dessert");
    }
    return QString();
}

QList<Course> courseOrder(int count)
{
    if (count >= 5) return { Course::Starter, Course::Soup, Course::Intermediate, Course::Main, Course::Dessert };
    if (count == 4) return { Course::Starter, Course::Soup, Course::Main, Course::Dessert };
    return { Course::Starter, Course::Main, Course::Dessert };
}

// ---------------------------------------------------------------- Vorlieben

double Prefs::targetKcal() const
{
    const double base = character == Character::Light ? 850 : character == Character::Festive ? 1600 : 1200;
    return base * (1.0 + 0.12 * (qBound(3, courses, 5) - 3));
}

QJsonObject Prefs::toJson() const
{
    return QJsonObject{ { QStringLiteral("courses"), courses },
                        { QStringLiteral("diet"), int(diet) },
                        { QStringLiteral("character"), int(character) },
                        { QStringLiteral("starter"), int(starter) },
                        { QStringLiteral("main"), int(mainCourse) },
                        { QStringLiteral("dessert"), int(dessert) },
                        { QStringLiteral("theme"), theme },
                        { QStringLiteral("exclude"), QJsonArray::fromStringList(exclude) },
                        { QStringLiteral("favorites"), favorites },
                        { QStringLiteral("withImage"), withImage } };
}

Prefs Prefs::fromJson(const QJsonObject &o)
{
    Prefs p;
    p.courses = qBound(3, o.value(QStringLiteral("courses")).toInt(3), 5);
    p.diet = Diet(qBound(0, o.value(QStringLiteral("diet")).toInt(), 2));
    p.character = Character(qBound(0, o.value(QStringLiteral("character")).toInt(1), 2));
    p.starter = Starter(qBound(0, o.value(QStringLiteral("starter")).toInt(), 4));
    p.mainCourse = Main(qBound(0, o.value(QStringLiteral("main")).toInt(), 6));
    p.dessert = Dessert(qBound(0, o.value(QStringLiteral("dessert")).toInt(), 5));
    p.theme = o.value(QStringLiteral("theme")).toString();
    for (const auto &v : o.value(QStringLiteral("exclude")).toArray()) p.exclude << v.toString();
    p.favorites = o.value(QStringLiteral("favorites")).toBool(true);
    p.withImage = o.value(QStringLiteral("withImage")).toBool(false);
    return p;
}

// ---------------------------------------------------------------- Menü speichern/laden

QJsonObject MenuData::toJson() const
{
    QJsonArray arr;
    for (const Item &i : items)
        arr.append(QJsonObject{ { QStringLiteral("course"), QLatin1String(courseKey(i.course)) },
                                { QStringLiteral("recipe"), i.recipeId },
                                { QStringLiteral("title"), i.title },
                                { QStringLiteral("description"), i.description } });
    QJsonObject o{ { QStringLiteral("format"), QStringLiteral("ViviaMeals-Menu") },
                   { QStringLiteral("formatVersion"), 1 },
                   { QStringLiteral("id"), id },
                   { QStringLiteral("title"), title },
                   { QStringLiteral("occasion"), occasion },
                   { QStringLiteral("persons"), persons },
                   { QStringLiteral("prefs"), prefs.toJson() },
                   { QStringLiteral("items"), arr },
                   { QStringLiteral("created"), created.toString(Qt::ISODate) },
                   { QStringLiteral("modified"), modified.toString(Qt::ISODate) } };
    if (date.isValid()) o[QStringLiteral("date")] = date.toString(Qt::ISODate);
    return o;
}

MenuData MenuData::fromJson(const QJsonObject &o)
{
    MenuData m;
    m.id = o.value(QStringLiteral("id")).toString();
    m.title = o.value(QStringLiteral("title")).toString();
    m.occasion = o.value(QStringLiteral("occasion")).toString();
    m.date = QDate::fromString(o.value(QStringLiteral("date")).toString(), Qt::ISODate);
    m.persons = qMax(1, o.value(QStringLiteral("persons")).toInt(4));
    m.prefs = Prefs::fromJson(o.value(QStringLiteral("prefs")).toObject());
    for (const auto &v : o.value(QStringLiteral("items")).toArray()) {
        const QJsonObject i = v.toObject();
        m.items << Item{ courseFromKey(i.value(QStringLiteral("course")).toString()), i.value(QStringLiteral("recipe")).toString(),
                         i.value(QStringLiteral("title")).toString(), i.value(QStringLiteral("description")).toString() };
    }
    m.created = QDateTime::fromString(o.value(QStringLiteral("created")).toString(), Qt::ISODate);
    m.modified = QDateTime::fromString(o.value(QStringLiteral("modified")).toString(), Qt::ISODate);
    return m;
}

bool MenuData::save(const QString &file, QString *error) const
{
    QDir().mkpath(QFileInfo(file).absolutePath());
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(QJsonDocument(toJson()).toJson());
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

bool MenuData::load(const QString &file, MenuData *out, QString *error)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    if (o.value(QStringLiteral("format")).toString() != QLatin1String("ViviaMeals-Menu")) {
        if (error) *error = tr("Keine ViviaMeals-Menüdatei.");
        return false;
    }
    *out = fromJson(o);
    if (out->id.isEmpty()) out->id = QFileInfo(file).completeBaseName();
    return true;
}

QString MenuData::newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

QString menuFolder(const QString &dbFolder) { return QDir(dbFolder).filePath(QStringLiteral("menues")); }

QList<MenuData> savedMenus(const QString &dbFolder)
{
    QList<MenuData> out;
    QDirIterator it(menuFolder(dbFolder), { QStringLiteral("*.vmmenu") }, QDir::Files);
    while (it.hasNext()) {
        MenuData m;
        if (MenuData::load(it.next(), &m)) out << m;
    }
    std::sort(out.begin(), out.end(), [](const MenuData &a, const MenuData &b) { return a.modified > b.modified; });
    return out;
}

bool saveMenu(const QString &dbFolder, MenuData &m, QString *error)
{
    if (m.id.isEmpty()) m.id = MenuData::newId();
    const QDateTime now = QDateTime::currentDateTime();
    if (!m.created.isValid()) m.created = now;
    m.modified = now;
    return m.save(QDir(menuFolder(dbFolder)).filePath(m.id + QStringLiteral(".vmmenu")), error);
}

bool removeMenu(const QString &dbFolder, const QString &id)
{
    return QFile::remove(QDir(menuFolder(dbFolder)).filePath(id + QStringLiteral(".vmmenu")));
}

// ---------------------------------------------------------------- Kurzbeschreibung, Umrechnen

namespace {
const QRegularExpression &stapleRe()
{
    static const QRegularExpression re(QStringLiteral("^(salz|pfeffer|zucker|puderzucker|vanillezucker|öl|olivenöl|rapsöl|sonnenblumenöl|butter|wasser|mehl|"
                                                      "zwiebel|zwiebeln|knoblauch|knoblauchzehe|knoblauchzehen|ei|eier|eigelb|milch|muskat|muskatnuss|essig|"
                                                      "brühe|gemüsebrühe|backpulver|speisestärke|prise|salz und pfeffer|zitronensaft|cayennepfeffer|paprikapulver)\\b"),
                                       QRegularExpression::CaseInsensitiveOption);
    return re;
}

QString cleanName(QString n)
{
    n = n.section(QLatin1Char(','), 0, 0);
    n.remove(QRegularExpression(QStringLiteral("\\([^)]*\\)")));
    return n.simplified();
}

QString joinMit(QStringList parts)
{
    if (parts.isEmpty()) return QString();
    if (parts.size() == 1) return QCoreApplication::translate("Menu", "mit %1").arg(parts[0]);
    const QString last = parts.takeLast();
    return QCoreApplication::translate("Menu", "mit %1 und %2").arg(parts.join(QStringLiteral(", ")), last);
}
} // namespace

QString describe(const RecipeSummary &s)
{
    QStringList parts;
    for (const QString &n : s.ingredientNames) {
        QString t = cleanName(n);
        if (t.size() < 3 || stapleRe().match(t).hasMatch()) continue;
        // Substantiv steht meist am Ende: „rote zwiebel“ → „rote Zwiebel“
        QStringList w = t.split(QLatin1Char(' '));
        QStringList parts2 = w.last().split(QLatin1Char('-'));
        for (QString &x : parts2)
            if (!x.isEmpty()) x[0] = x[0].toUpper();
        w.last() = parts2.join(QLatin1Char('-'));
        t = w.join(QLatin1Char(' '));
        if (parts.contains(t)) continue;
        parts << t;
        if (parts.size() == 3) break;
    }
    return joinMit(parts);
}

QString describe(const Recipe &r)
{
    struct C { int pos; double grams; QString name; };
    QList<C> list;
    QStringList seen;
    int pos = 0;
    for (const Ingredient &i : r.ingredients) {
        ++pos;
        if (i.heading) continue;
        const QString t = cleanName(i.name);
        if (t.size() < 3 || stapleRe().match(t).hasMatch() || seen.contains(t.toLower())) continue;
        seen << t.toLower();
        list << C{ pos, i.grams, t };
    }
    // die gewichtigsten drei, in der Reihenfolge des Rezepts
    std::stable_sort(list.begin(), list.end(), [](const C &a, const C &b) { return a.grams > b.grams; });
    if (list.size() > 3) list = list.mid(0, 3);
    std::sort(list.begin(), list.end(), [](const C &a, const C &b) { return a.pos < b.pos; });
    QStringList parts;
    for (const C &c : list) parts << c.name;
    return joinMit(parts);
}

Recipe scaled(const Recipe &r, double servings)
{
    Recipe s = r;
    if (servings <= 0 || r.servings <= 0 || qFuzzyCompare(servings, r.servings)) return s;
    const double f = servings / r.servings;
    for (Ingredient &i : s.ingredients) {
        if (i.heading) continue;
        i.amount *= f;
        i.amountMax *= f;
        i.grams *= f;
    }
    s.servings = servings;
    s.nutrition.total = r.nutrition.total.scaled(f);
    return s;
}

// ---------------------------------------------------------------- Zusammenstellen


Composer::Composer(const QList<RecipeSummary> &recipes, const Prefs &prefs, quint32 seed) : m_prefs(prefs), m_seed(seed)
{
    const FoodTable &table = FoodTable::instance();
    for (const RecipeSummary &s : recipes) {
        Info in;
        in.title = s.title.toLower();
        for (const QString &t : s.tags) in.tags << t.toLower();
        for (const QString &t : in.tags)
            if (!genericTag(t)) in.themes << t;
        in.ingredients = s.ingredientNames.join(QLatin1Char('|')).toLower();
        in.types = MealTypes::classify(MealTypes::from(s));
        in.kcal = s.hasNutrition ? s.perServing.kcal : 0;
        in.menu = in.tags.contains(QStringLiteral("menüs")) || s.yieldUnit == QLatin1String("Menü");
        const QString &ti = in.title;
        auto tagHas = [&](std::initializer_list<const char *> words) {
            for (const QString &t : in.tags)
                if (containsAny(t, words)) return true;
            return false;
        };
        in.soup = tagHas({ "suppe" })
                  || containsAny(ti, { "suppe", "brühe", "bouillon", "consommé", "consomme", "bisque", "velouté", "gazpacho", "minestrone", "borschtsch",
                                       "soup", "chowder" });
        in.salad = tagHas({ "salat" }) || containsAny(ti, { "salat", "salad" });
        in.cold = in.salad || containsAny(ti, { "carpaccio", "tatar", "terrine", "vitello", "antipast", "bruschetta", "crostini", "cocktail", "tapas", "dip",
                                                "rohkost", "gazpacho", "sushi", "ceviche", "aufstrich", "mousse von" });
        in.dessert = (in.types & (MealTypes::Dessert)) || tagHas({ "dessert", "nachtisch", "nachspeise" });
        in.sweet = in.dessert || (in.types & (MealTypes::Sweet | MealTypes::Baking)) || tagHas({ "süßspeise" });
        in.cake = (in.types & MealTypes::Baking) || containsAny(ti, { "kuchen", "torte", "tarte", "strudel", "crumble", "muffin", "brownie", "gebäck", "cookie",
                                                                     "kekse", "plätzchen", "cake" });
        in.sorbet = containsAny(ti, { "sorbet", "granita" });

        // Hauptzutat, Beilagen, Sahne, auffällige Zutaten
        bool animal = false, nonVegan = false;
        auto setProtein = [&](const QString &p) {
            if (in.protein.isEmpty()) in.protein = p;
        };
        for (const QString &key : s.foods) {
            const Food *f = table.find(key);
            if (f) {
                animal |= f->isAnimal();
                nonVegan |= f->isNonVegan();
            }
            if (key == QLatin1String("rindfleisch") || key == QLatin1String("kalbfleisch") || key == QLatin1String("hackfleisch")) setProtein(tr("Rind"));
            else if (key == QLatin1String("schweinefleisch") || key == QLatin1String("speck") || key == QLatin1String("schinken")
                     || key == QLatin1String("rohschinken") || key == QLatin1String("wurst"))
                setProtein(tr("Schwein"));
            else if (key == QLatin1String("gefluegel") || key == QLatin1String("haehnchenschenkel") || key == QLatin1String("ente")
                     || key == QLatin1String("gefluegelleber"))
                setProtein(tr("Geflügel"));
            else if (key == QLatin1String("lamm") || key == QLatin1String("wild") || key == QLatin1String("kaninchen")) setProtein(tr("Lamm/Wild"));
            else if (key == QLatin1String("fisch_mager") || key == QLatin1String("lachs") || key == QLatin1String("thunfisch")) setProtein(tr("Fisch"));
            else if (key == QLatin1String("garnelen")) setProtein(tr("Meeresfrüchte"));
            if (key.startsWith(QLatin1String("nudeln")) || key == QLatin1String("vollkornnudeln")) in.starches << tr("Nudeln");
            else if (key.startsWith(QLatin1String("reis")) || key == QLatin1String("vollkornreis")) in.starches << tr("Reis");
            else if (key.startsWith(QLatin1String("kartoffel")) || key == QLatin1String("pommes")) in.starches << tr("Kartoffeln");
            else if (key == QLatin1String("brot") || key == QLatin1String("vollkornbrot") || key == QLatin1String("broetchen")) in.starches << tr("Brot");
            else if (key == QLatin1String("couscous") || key == QLatin1String("polenta") || key == QLatin1String("quinoa") || key == QLatin1String("gruenkern"))
                in.starches << tr("Getreide");
            if (key == QLatin1String("sahne") || key == QLatin1String("creme_double") || key == QLatin1String("sahnesosse") || key == QLatin1String("quark_sahne"))
                in.cream = true;
            if (f) {
                static const QSet<QString> skipCat{ QStringLiteral("gewuerz"), QStringLiteral("fett"), QStringLiteral("zucker"), QStringLiteral("kraeuter"),
                                                    QStringLiteral("getraenk"), QStringLiteral("alkohol"), QStringLiteral("sonstiges"), QStringLiteral("sosse"),
                                                    QStringLiteral("milch"), QStringLiteral("getreide"), QStringLiteral("fleisch"), QStringLiteral("fisch"),
                                                    QStringLiteral("ei"), QStringLiteral("kartoffel"), QStringLiteral("backwaren") };
                if (!skipCat.contains(f->category) && !key.contains(QLatin1String("zwiebel")) && !key.contains(QLatin1String("knoblauch"))
                    && !key.startsWith(QLatin1String("zitrone")))
                    in.signature.insert(key);
                if (f->category == QLatin1String("obst") && !key.startsWith(QLatin1String("zitrone"))) in.fruit = true;
            }
        }
        // Namen als Ergänzung (nicht zugeordnete Zutaten)
        const QString &ing = in.ingredients;
        if (in.protein.isEmpty()) {
            if (containsAny(ing, { "lachs", "forelle", "kabeljau", "zander", "seelachs", "dorade", "thunfisch", "fischfilet", "scholle", "hering" }))
                in.protein = tr("Fisch");
            else if (containsAny(ing, { "garnele", "scampi", "muschel", "krabbe", "hummer", "tintenfisch", "calamar" })) in.protein = tr("Meeresfrüchte");
            else if (containsAny(ing, { "rind", "kalb", "beef", "hackfleisch" })) in.protein = tr("Rind");
            else if (containsAny(ing, { "schwein", "speck", "schinken", "bacon", "chorizo", "salami" })) in.protein = tr("Schwein");
            else if (containsAny(ing, { "hähnchen", "huhn", "hühn", "pute", "ente", "gans", "chicken", "geflügel" })) in.protein = tr("Geflügel");
            else if (containsAny(ing, { "lamm", "reh", "hirsch", "wildschwein", "kaninchen", "hase" })) in.protein = tr("Lamm/Wild");
        }
        if (!in.protein.isEmpty()) animal = true;
        if (containsAny(ing, { "sahne", "crème fraîche", "creme fraiche", "schmand", "mascarpone" }) && !containsAny(ing, { "saure sahne" })) in.cream = true;
        if (containsAny(ing, { "nudel", "spaghetti", "tagliatelle", "penne", "pasta", "lasagne", "ravioli", "gnocchi" })) in.starches << tr("Nudeln");
        if (containsAny(ing, { "risotto", "reis" })) in.starches << tr("Reis");
        if (containsAny(ing, { "kartoffel", "rösti", "pommes" })) in.starches << tr("Kartoffeln");
        if (containsAny(ing, { "baguette", "ciabatta", "toast", "brötchen", "weißbrot", "brot" })) in.starches << tr("Brot");
        in.starches.removeDuplicates();
        in.vegan = in.tags.contains(QStringLiteral("vegan")) || (!animal && !nonVegan && !s.foods.isEmpty()
                                                                 && !containsAny(ing, { "ei", "milch", "käse", "honig", "joghurt", "quark", "sahne", "butter" }));
        in.vegetarian = in.vegan || in.tags.contains(QStringLiteral("vegetarisch")) || (!animal && !s.foods.isEmpty());
        if (in.tags.contains(QStringLiteral("fleischgerichte")) || in.tags.contains(QStringLiteral("fisch"))) in.vegetarian = in.vegan = false;
        // Dessert-Art
        const QString all = ti + QLatin1Char('|') + ing;
        in.fruit = in.fruit || containsAny(ti, { "beere", "apfel", "äpfel", "obst", "frucht", "früchte", "zitrone", "orange", "mango", "erdbeer", "himbeer",
                                                 "kirsch", "pflaume", "zwetsch", "birne", "rhabarber", "aprikose", "pfirsich", "ananas", "feige" });
        in.chocolate = containsAny(all, { "schokolade", "kakao", "nougat", "brownie", "mousse au chocolat", "zartbitter" });
        in.creamy = containsAny(ti, { "creme", "crème", "mousse", "panna cotta", "pudding", "quark", "tiramisu", "joghurt", "flammeri", "bayerisch" });
        in.frozen = containsAny(ti, { "eis", "parfait", "sorbet", "semifreddo", "granita" }) && !ti.contains(QLatin1String("eisbein"));
        in.pastry = in.cake || containsAny(ti, { "pfannkuchen", "crêpe", "crepe", "waffel", "küchlein", "schmarrn", "dampfnudel", "auflauf" });
        in.favorite = s.favorite;
        in.image = s.hasImage;
        m_index.insert(s.id, int(m_list.size()));
        m_list << s;
        m_info << in;
    }
}

const RecipeSummary *Composer::summary(const QString &id) const
{
    const int i = m_index.value(id, -1);
    return i < 0 ? nullptr : &m_list[i];
}

double Composer::fit(const RecipeSummary &s, Course c) const
{
    const int idx = m_index.value(s.id, -1);
    if (idx < 0) return 0;
    const Info &in = m_info[idx];
    if (in.menu || (in.types & MealTypes::Basic)) return 0;
    // Ernährungsform, ausgeschlossene Zutaten
    if (m_prefs.diet == Prefs::Diet::Vegan && !in.vegan) return 0;
    if (m_prefs.diet == Prefs::Diet::Vegetarian && !in.vegetarian) return 0;
    for (const QString &x : m_prefs.exclude)
        if (!x.trimmed().isEmpty() && (in.ingredients.contains(x.trimmed().toLower()) || in.title.contains(x.trimmed().toLower()))) return 0;
    const bool savoryCourse = c != Course::Dessert;
    if (savoryCourse && in.sweet && !(c == Course::Intermediate && in.sorbet)) return 0;
    const bool breakfastOnly = (in.types & MealTypes::Breakfast) && !(in.types & (MealTypes::Main | MealTypes::Light | MealTypes::Dessert));

    double f = 0;
    switch (c) {
    case Course::Starter:
        if (in.soup || breakfastOnly || in.cake || (in.kcal > 650)) return 0;
        if (in.tags.contains(QStringLiteral("vorspeisen")) || containsAny(in.tags.join(QLatin1Char('|')), { "vorspeise", "antipasti", "fingerfood", "tapas", "starter" }))
            f = 3;
        else if (in.salad) f = 2.5;
        else if (in.cold) f = 2.4;
        else if (in.types & (MealTypes::Light | MealTypes::Snack)) f = 1.5;
        else if ((in.types & MealTypes::Main) && in.kcal > 0 && in.kcal <= 450) f = 0.8;
        else if (in.kcal > 0 && in.kcal <= 500) f = 0.4;
        else return 0;
        switch (m_prefs.starter) {
        case Prefs::Starter::Salad: f *= in.salad ? 2.0 : 0.35; break;
        case Prefs::Starter::Cold: f *= in.cold ? 1.8 : 0.4; break;
        case Prefs::Starter::Warm: f *= in.cold ? 0.35 : 1.8; break;
        case Prefs::Starter::Fish: f *= (in.protein == tr("Fisch") || in.protein == tr("Meeresfrüchte")) ? 2.2 : 0.35; break;
        case Prefs::Starter::Any: break;
        }
        break;
    case Course::Soup:
        if (!in.soup) return 0;
        f = 3;
        if ((in.tags.contains(QStringLiteral("eintopf")) || in.title.contains(QLatin1String("eintopf"))) && (in.kcal == 0 || in.kcal > 450)) f = 0.4;
        break;
    case Course::Intermediate:
        if (in.sorbet || in.tags.contains(QStringLiteral("zwischengang"))) {
            f = 3;
            break;
        }
        if (in.soup || breakfastOnly || in.cake || in.kcal > 600) return 0;
        if ((in.protein == tr("Fisch") || in.protein == tr("Meeresfrüchte")) && !in.salad) f = 2.6;
        else if (containsAny(in.title, { "risotto", "gnocchi", "ravioli", "tortellini", "quiche", "tarte", "flammkuchen", "soufflé", "souffle", "gratin",
                                         "strudel", "pastete", "jakobsmuschel", "garnele" }))
            f = 2.2;
        else if ((in.types & MealTypes::Light) && !in.cold) f = 1.5;
        else if (!in.cold && (in.types & MealTypes::Main) && in.kcal > 0 && in.kcal <= 450) f = 1.0;
        else if (in.salad) f = 0.4;
        else return 0;
        break;
    case Course::Main:
        if (in.soup && !(in.types & MealTypes::Main)) return 0;
        if (breakfastOnly || in.cake) return 0;
        if (in.types & MealTypes::Main) f = in.tags.contains(QStringLiteral("hauptgerichte")) ? 3 : 2.4;
        else if ((in.tags.contains(QStringLiteral("fleischgerichte")) || in.tags.contains(QStringLiteral("fisch"))) && in.kcal >= 300) f = 2;
        else return 0;
        if (in.salad && in.kcal < 450) f *= 0.4;
        if (in.soup) f *= 0.3;
        if (in.tags.contains(QStringLiteral("zwischengang")) || in.tags.contains(QStringLiteral("vorspeisen"))) f *= 0.3;   // als kleiner Gang gedacht
        switch (m_prefs.mainCourse) {
        case Prefs::Main::Beef: f *= in.protein == tr("Rind") ? 2.5 : 0.12; break;
        case Prefs::Main::Pork: f *= in.protein == tr("Schwein") ? 2.5 : 0.12; break;
        case Prefs::Main::Poultry: f *= in.protein == tr("Geflügel") ? 2.5 : 0.12; break;
        case Prefs::Main::LambGame: f *= in.protein == tr("Lamm/Wild") ? 2.5 : 0.12; break;
        case Prefs::Main::Fish: f *= (in.protein == tr("Fisch") || in.protein == tr("Meeresfrüchte")) ? 2.5 : 0.12; break;
        case Prefs::Main::Vegetarian: f *= in.vegetarian ? 2.5 : 0.12; break;
        case Prefs::Main::Any: break;
        }
        break;
    case Course::Dessert:
        if (breakfastOnly && !in.dessert) return 0;
        if (in.cake) f = in.dessert ? 1.2 : 0.6;   // Kuchen, Kekse: als Dessert eher Notlösung (außer „Gebäck“ gewünscht)
        else if (in.dessert) f = 3;
        else if (in.sweet) f = 2;
        else return 0;
        if (in.tags.contains(QStringLiteral("zwischengang"))) f *= 0.4;   // Sorbet als Zwischengang gedacht
        switch (m_prefs.dessert) {
        case Prefs::Dessert::Fruity: f *= in.fruit ? 2.2 : 0.4; break;
        case Prefs::Dessert::Chocolate: f *= in.chocolate ? 2.6 : 0.35; break;
        case Prefs::Dessert::Creamy: f *= in.creamy ? 2.2 : 0.4; break;
        case Prefs::Dessert::Frozen: f *= in.frozen ? 2.6 : 0.35; break;
        case Prefs::Dessert::Pastry: f *= in.pastry ? (in.cake ? 5.0 : 2.4) : 0.4; break;
        case Prefs::Dessert::Any: break;
        }
        break;
    }
    // Frühstücksgerichte (Rührei, Porridge …) passen selten in ein Menü
    if ((in.types & MealTypes::Breakfast) && c != Course::Dessert) f *= 0.25;
    // Energie passend zum Gang und zum Charakter des Menüs
    double shareSum = 0;
    for (Course o : courseOrder(m_prefs.courses)) shareSum += courseShare(o);
    const double target = m_prefs.targetKcal() * courseShare(c) / qMax(0.1, shareSum);
    if (in.kcal > 0) {
        const double d = (in.kcal - target) / (0.6 * target);
        f *= 0.4 + 0.6 * std::exp(-d * d);
    } else {
        f *= 0.7;
    }
    if (!m_prefs.theme.isEmpty() && in.tags.contains(m_prefs.theme.toLower())) f *= 1.6;
    if (m_prefs.favorites && in.favorite) f *= 1.25;
    if (m_prefs.withImage) f *= in.image ? 1.2 : 0.8;
    return f;
}

double Composer::harmony(const QList<Item> &items, QStringList *notes) const
{
    double h = 0;
    QList<const Info *> infos;
    QList<Course> courses;
    double total = 0;
    bool allKnown = true;
    for (const Item &it : items) {
        const int i = m_index.value(it.recipeId, -1);
        if (i < 0) continue;
        infos << &m_info[i];
        courses << it.course;
        total += m_info[i].kcal;
        allKnown &= m_info[i].kcal > 0;
    }
    QSet<QString> ids;
    for (const Item &it : items)
        if (!it.recipeId.isEmpty()) {
            if (ids.contains(it.recipeId)) h -= 5;
            ids.insert(it.recipeId);
        }
    // Hauptzutaten
    QHash<QString, int> proteins;
    QStringList proteinOrder;
    for (const Info *in : infos)
        if (!in->protein.isEmpty()) {
            if (++proteins[in->protein] == 2) {
                h -= 1.6;
                if (notes) *notes << tr("⚠ %1 in zwei Gängen").arg(in->protein);
            }
            proteinOrder << in->protein;
        }
    // Sättigungsbeilagen (nicht im Dessert)
    QHash<QString, int> starch;
    for (int i = 0; i < infos.size(); ++i) {
        if (courses[i] == Course::Dessert) continue;
        for (const QString &s : infos[i]->starches)
            if (++starch[s] == 2) {
                h -= 1.2;
                if (notes) *notes << tr("⚠ %1 in zwei Gängen").arg(s);
            }
    }
    // Sahne
    int cream = 0;
    for (const Info *in : infos) cream += in->cream ? 1 : 0;
    if (cream > 1) {
        h -= 0.7 * (cream - 1);
        if (notes) *notes << tr("⚠ Sahne in %1 Gängen – recht schwer").arg(cream);
    }
    // gleiche auffällige Zutaten
    QStringList shared;
    for (int a = 0; a < infos.size(); ++a)
        for (int b = a + 1; b < infos.size(); ++b)
            for (const QString &k : infos[a]->signature)
                if (infos[b]->signature.contains(k)) {
                    const Food *f = FoodTable::instance().find(k);
                    const QString name = f ? f->name : k;
                    if (!shared.contains(name)) shared << name;
                }
    if (!shared.isEmpty()) {
        h -= 0.5 * qMin(4, int(shared.size()));
        if (notes) *notes << tr("⚠ mehrfach: %1").arg(shared.join(QStringLiteral(", ")));
    }
    // gemeinsames Thema
    int themePairs = 0;
    QStringList themes;
    for (int a = 0; a < infos.size(); ++a)
        for (int b = a + 1; b < infos.size(); ++b)
            for (const QString &t : infos[a]->themes)
                if (infos[b]->themes.contains(t)) {
                    ++themePairs;
                    if (!themes.contains(t)) themes << t;
                }
    h += 0.35 * qMin(4, themePairs);
    if (notes && !themes.isEmpty()) *notes << tr("✓ verbindendes Thema: %1").arg(themes.join(QStringLiteral(", ")));
    // Abwechslung bei den Hauptzutaten
    if (proteinOrder.size() >= 2 && proteins.size() == proteinOrder.size()) {
        h += 0.5;
        if (notes) *notes << tr("✓ abwechslungsreich: %1").arg(proteinOrder.join(QStringLiteral(" – ")));
    }
    // nach üppigem Hauptgang ein leichtes oder fruchtiges Dessert
    for (int i = 0; i < infos.size(); ++i)
        if (courses[i] == Course::Main && infos[i]->kcal > 700)
            for (int j = 0; j < infos.size(); ++j)
                if (courses[j] == Course::Dessert && (infos[j]->fruit || infos[j]->frozen) && infos[j]->kcal < 350) {
                    h += 0.4;
                    if (notes) *notes << tr("✓ leichtes Dessert nach kräftigem Hauptgang");
                }
    // Energie des ganzen Menüs
    if (total > 0) {
        const double target = m_prefs.targetKcal();
        const double dev = std::abs(total - target) / target;
        h -= 1.5 * qMax(0.0, dev - 0.1);
        if (notes) {
            if (total > target * 1.25) *notes << tr("⚠ üppig: etwa %1 kcal je Person (Ziel etwa %2)").arg(qRound(total / 10) * 10).arg(qRound(target / 10) * 10);
            else if (total < target * 0.7 && allKnown)
                *notes << tr("ℹ leicht: etwa %1 kcal je Person (Ziel etwa %2)").arg(qRound(total / 10) * 10).arg(qRound(target / 10) * 10);
            else *notes << tr("✓ etwa %1 kcal je Person").arg(qRound(total / 10) * 10);
        }
    }
    return h;
}

double Composer::score(const QList<Item> &items) const
{
    double s = 0;
    for (const Item &it : items) {
        const RecipeSummary *rs = summary(it.recipeId);
        if (!rs) {
            s -= 3;   // leerer Gang
            continue;
        }
        const double f = fit(*rs, it.course);
        s += f > 0 ? std::log(1.0 + f) * 2.0 : -2.0;
    }
    return s + harmony(items);
}

QList<Item> Composer::propose()
{
    const QList<Course> order = courseOrder(m_prefs.courses);
    QRandomGenerator rng = m_seed ? QRandomGenerator(m_seed) : QRandomGenerator(QRandomGenerator::global()->generate());
    // je Gang die besten Kandidaten
    QList<QList<QPair<double, int>>> pools;
    for (Course c : order) {
        QList<QPair<double, int>> pool;
        for (int i = 0; i < m_list.size(); ++i) {
            const double f = fit(m_list[i], c);
            if (f > 0) pool << qMakePair(f, i);
        }
        std::sort(pool.begin(), pool.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
        if (pool.size() > 30) pool = pool.mid(0, 30);
        pools << pool;
    }
    auto pick = [&](const QList<QPair<double, int>> &pool) -> int {
        if (pool.isEmpty()) return -1;
        double sum = 0;
        for (const auto &p : pool) sum += p.first * p.first;
        double r = rng.generateDouble() * sum;
        for (const auto &p : pool) {
            r -= p.first * p.first;
            if (r <= 0) return p.second;
        }
        return pool.last().second;
    };
    QList<Item> best;
    double bestScore = -1e9;
    for (int iter = 0; iter < 400; ++iter) {
        QList<Item> items;
        for (int c = 0; c < order.size(); ++c) {
            Item it;
            it.course = order[c];
            const int i = pick(pools[c]);
            if (i >= 0) {
                it.recipeId = m_list[i].id;
                it.title = m_list[i].title;
            }
            items << it;
        }
        const double s = score(items);
        if (s > bestScore) {
            bestScore = s;
            best = items;
        }
    }
    // Feinschliff: jeden Gang gegen die anderen optimieren
    for (int pass = 0; pass < 2; ++pass)
        for (int c = 0; c < order.size(); ++c)
            for (const auto &p : pools[c]) {
                QList<Item> trial = best;
                trial[c].recipeId = m_list[p.second].id;
                trial[c].title = m_list[p.second].title;
                const double s = score(trial);
                if (s > bestScore + 1e-9) {
                    bestScore = s;
                    best = trial;
                }
            }
    for (Item &it : best)
        if (const RecipeSummary *rs = summary(it.recipeId)) it.description = describe(*rs);
    m_tried.clear();
    return best;
}

QList<Composer::Candidate> Composer::candidates(const QList<Item> &items, int index, int max) const
{
    QList<Candidate> out;
    if (index < 0 || index >= items.size()) return out;
    const Course c = items[index].course;
    QStringList baseNotes;
    QList<Item> without = items;
    without[index].recipeId.clear();
    harmony(without, &baseNotes);
    for (int i = 0; i < m_list.size(); ++i) {
        const double f = fit(m_list[i], c);
        if (f <= 0) continue;
        bool used = false;
        for (int j = 0; j < items.size(); ++j)
            if (j != index && items[j].recipeId == m_list[i].id) used = true;
        if (used) continue;
        QList<Item> trial = items;
        trial[index].recipeId = m_list[i].id;
        Candidate cand;
        cand.id = m_list[i].id;
        cand.score = score(trial);
        QStringList n;
        harmony(trial, &n);
        for (const QString &x : n)
            if (!baseNotes.contains(x) && (x.startsWith(QStringLiteral("⚠")) || x.startsWith(QStringLiteral("✓ verbind")))) cand.notes << x;
        out << cand;
    }
    std::sort(out.begin(), out.end(), [](const Candidate &a, const Candidate &b) { return a.score > b.score; });
    if (out.size() > max) out = out.mid(0, max);
    return out;
}

QString Composer::alternative(const QList<Item> &items, int index)
{
    const QList<Candidate> list = candidates(items, index, 40);
    QStringList &tried = m_tried[index];
    if (!items.value(index).recipeId.isEmpty() && !tried.contains(items[index].recipeId)) tried << items[index].recipeId;
    for (const Candidate &c : list)
        if (!tried.contains(c.id)) {
            tried << c.id;
            return c.id;
        }
    // alle gezeigt: von vorn
    tried.clear();
    for (const Candidate &c : list)
        if (c.id != items[index].recipeId) {
            tried << c.id;
            return c.id;
        }
    return QString();
}

QStringList Composer::notes(const QList<Item> &items) const
{
    QStringList n;
    harmony(items, &n);
    for (const Item &it : items)
        if (it.recipeId.isEmpty()) n.prepend(tr("⚠ Für „%1“ passt kein Rezept – bitte selbst wählen oder Vorlieben lockern.").arg(courseName(it.course)));
    return n;
}

double Composer::kcal(const QList<Item> &items) const
{
    double k = 0;
    for (const Item &it : items) {
        const int i = m_index.value(it.recipeId, -1);
        if (i >= 0) k += m_info[i].kcal;
    }
    return k;
}

int Composer::suitableCount(Course c) const
{
    int n = 0;
    for (const RecipeSummary &s : m_list)
        if (fit(s, c) > 0) ++n;
    return n;
}

} // namespace Menu
