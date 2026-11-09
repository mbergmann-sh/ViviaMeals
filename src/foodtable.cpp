// SPDX-License-Identifier: MIT
#include "foodtable.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <algorithm>

// ---------------------------------------------------------------- Food

bool Food::isAnimal() const
{
    return category == QLatin1String("fleisch") || category == QLatin1String("fisch")
           || category == QLatin1String("gelatine");
}

bool Food::isNonVegan() const
{
    return isAnimal() || category == QLatin1String("milch") || category == QLatin1String("kaese")
           || category == QLatin1String("ei") || category == QLatin1String("honig");
}

QJsonObject Food::toJson() const
{
    QJsonObject units;
    for (auto it = unitGrams.constBegin(); it != unitGrams.constEnd(); ++it) units[it.key()] = it.value();
    return QJsonObject{ { QStringLiteral("key"), key },
                        { QStringLiteral("name"), name },
                        { QStringLiteral("category"), category },
                        { QStringLiteral("kcal"), per100.kcal },
                        { QStringLiteral("protein"), per100.protein },
                        { QStringLiteral("fat"), per100.fat },
                        { QStringLiteral("saturatedFat"), per100.satFat },
                        { QStringLiteral("carbohydrates"), per100.carbs },
                        { QStringLiteral("sugar"), per100.sugar },
                        { QStringLiteral("fiber"), per100.fiber },
                        { QStringLiteral("salt"), per100.salt },
                        { QStringLiteral("density"), density },
                        { QStringLiteral("piece"), piece },
                        { QStringLiteral("units"), units },
                        { QStringLiteral("aliases"), QJsonArray::fromStringList(aliases) },
                        { QStringLiteral("source"), source } };
}

Food Food::fromJson(const QJsonObject &o)
{
    Food f;
    f.key = o.value(QStringLiteral("key")).toString();
    f.name = o.value(QStringLiteral("name")).toString();
    f.category = o.value(QStringLiteral("category")).toString(QStringLiteral("sonstiges"));
    f.per100 = Nutrients::fromJson(o);
    f.density = o.value(QStringLiteral("density")).toDouble(1.0);
    if (f.density <= 0) f.density = 1.0;
    f.piece = o.value(QStringLiteral("piece")).toDouble();
    const QJsonObject u = o.value(QStringLiteral("units")).toObject();
    for (auto it = u.constBegin(); it != u.constEnd(); ++it) f.unitGrams.insert(it.key(), it.value().toDouble());
    for (const auto &a : o.value(QStringLiteral("aliases")).toArray()) f.aliases << a.toString().toLower();
    f.source = o.value(QStringLiteral("source")).toString();
    return f;
}

// ---------------------------------------------------------------- FoodTable

FoodTable &FoodTable::instance()
{
    static FoodTable t;
    return t;
}

FoodTable::FoodTable() { load(); }

QString FoodTable::userFile() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) dir = QDir::homePath() + QStringLiteral("/.vitameals");
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("lebensmittel_eigene.json"));
}

void FoodTable::reload()
{
    m_foods.clear();
    m_builtin.clear();
    m_userFoods.clear();
    m_learned.clear();
    load();
}

void FoodTable::load()
{
    QFile f(QStringLiteral(":/data/lebensmittel.json"));
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        for (const auto &v : o.value(QStringLiteral("foods")).toArray()) {
            const Food food = Food::fromJson(v.toObject());
            m_builtin.insert(food.key, food);
        }
    }
    QFile uf(userFile());
    if (uf.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(uf.readAll()).object();
        for (const auto &v : o.value(QStringLiteral("foods")).toArray()) {
            Food food = Food::fromJson(v.toObject());
            food.user = true;
            if (!food.key.isEmpty()) m_userFoods.insert(food.key, food);
        }
        const QJsonObject l = o.value(QStringLiteral("learned")).toObject();
        for (auto it = l.constBegin(); it != l.constEnd(); ++it) m_learned.insert(it.key(), it.value().toString());
    }
    m_foods = m_builtin;
    for (auto it = m_userFoods.constBegin(); it != m_userFoods.constEnd(); ++it) m_foods.insert(it.key(), it.value());
    rebuildAliases();
}

void FoodTable::rebuildAliases()
{
    m_aliases.clear();
    m_exact.clear();
    // eigene Einträge zuerst, damit ihre Namen gewinnen
    QList<const Food *> order;
    for (const Food &f : m_foods)
        if (f.user) order << &f;
    for (const Food &f : m_foods)
        if (!f.user) order << &f;
    for (const Food *f : order) {
        QStringList names = f->aliases;
        names << f->name.toLower();
        for (const QString &a : names) {
            const QString n = normalize(a);
            if (n.isEmpty() || m_exact.contains(n)) continue;
            m_exact.insert(n, f->key);
            m_aliases.append({ n, f->key });
        }
    }
    std::stable_sort(m_aliases.begin(), m_aliases.end(),
                     [](const Alias &a, const Alias &b) { return a.text.size() > b.text.size(); });
}

void FoodTable::saveUser() const
{
    QJsonArray foods;
    for (const Food &f : m_userFoods) foods.append(f.toJson());
    QJsonObject learned;
    for (auto it = m_learned.constBegin(); it != m_learned.constEnd(); ++it) learned[it.key()] = it.value();
    QSaveFile f(userFile());
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(QJsonObject{ { QStringLiteral("format"), QStringLiteral("VitaMeals-UserFoods") },
                                       { QStringLiteral("foods"), foods },
                                       { QStringLiteral("learned"), learned } })
                .toJson());
    f.commit();
}

void FoodTable::saveUserFood(const Food &food)
{
    Food f = food;
    f.user = true;
    if (f.key.isEmpty()) {
        QString base = normalize(f.name);
        base.replace(QLatin1Char(' '), QLatin1Char('_'));
        f.key = QStringLiteral("u_") + base;
        int n = 2;
        while (m_foods.contains(f.key) && !m_userFoods.contains(f.key)) f.key = QStringLiteral("u_%1_%2").arg(base).arg(n++);
    }
    m_userFoods.insert(f.key, f);
    m_foods.insert(f.key, f);
    rebuildAliases();
    saveUser();
}

void FoodTable::removeUserFood(const QString &key)
{
    m_userFoods.remove(key);
    if (m_builtin.contains(key)) m_foods.insert(key, m_builtin.value(key));
    else m_foods.remove(key);
    rebuildAliases();
    saveUser();
}

int FoodTable::saveUserFoods(const QList<Food> &foods, bool replaceExisting)
{
    int n = 0;
    for (Food f : foods) {
        if (f.key.isEmpty() || f.name.isEmpty()) continue;
        if (!replaceExisting && m_userFoods.contains(f.key)) continue;
        f.user = true;
        m_userFoods.insert(f.key, f);
        m_foods.insert(f.key, f);
        ++n;
    }
    if (n > 0) {
        rebuildAliases();
        saveUser();
    }
    return n;
}

int FoodTable::removeUserFoodsFrom(const QString &source)
{
    QStringList keys;
    for (const Food &f : m_userFoods)
        if (f.source == source) keys << f.key;
    for (const QString &k : keys) {
        m_userFoods.remove(k);
        if (m_builtin.contains(k)) m_foods.insert(k, m_builtin.value(k));
        else m_foods.remove(k);
    }
    if (!keys.isEmpty()) {
        rebuildAliases();
        saveUser();
    }
    return int(keys.size());
}

QStringList FoodTable::userSources() const
{
    QStringList l;
    for (const Food &f : m_userFoods)
        if (!f.source.isEmpty() && !l.contains(f.source)) l << f.source;
    l.sort(Qt::CaseInsensitive);
    return l;
}

QList<const Food *> FoodTable::userFoods() const
{
    QList<const Food *> l;
    for (const Food &f : m_foods)
        if (f.user) l << &f;
    std::sort(l.begin(), l.end(), [](const Food *a, const Food *b) { return QString::localeAwareCompare(a->name, b->name) < 0; });
    return l;
}

void FoodTable::learn(const QString &ingredientName, const QString &foodKey)
{
    const QString n = normalize(ingredientName);
    if (n.isEmpty()) return;
    if (foodKey.isEmpty()) m_learned.remove(n);
    else m_learned.insert(n, foodKey);
    saveUser();
}

QString FoodTable::learnedFor(const QString &ingredientName) const
{
    return m_learned.value(normalize(ingredientName));
}

const Food *FoodTable::find(const QString &key) const
{
    auto it = m_foods.constFind(key);
    return it == m_foods.constEnd() ? nullptr : &*it;
}

QList<const Food *> FoodTable::all() const
{
    QList<const Food *> l;
    for (const Food &f : m_foods) l << &f;
    std::sort(l.begin(), l.end(), [](const Food *a, const Food *b) { return QString::localeAwareCompare(a->name, b->name) < 0; });
    return l;
}

QString FoodTable::normalize(const QString &name)
{
    QString s = name.toLower();
    s.remove(QRegularExpression(QStringLiteral("\\([^)]*\\)")));            // Klammerzusätze
    s.replace(QRegularExpression(QStringLiteral("[\\d½¼¾⅓⅔⅛%]+")), QStringLiteral(" "));
    s.replace(QRegularExpression(QStringLiteral("[^\\p{L}\\- ]")), QStringLiteral(" "));
    s.replace(QLatin1Char('-'), QLatin1Char(' '));
    static const QSet<QString> filler{
        QStringLiteral("frisch"), QStringLiteral("frische"), QStringLiteral("frischer"), QStringLiteral("frisches"),
        QStringLiteral("frischen"), QStringLiteral("fein"), QStringLiteral("feine"), QStringLiteral("feiner"),
        QStringLiteral("grob"), QStringLiteral("grobe"), QStringLiteral("grober"), QStringLiteral("klein"),
        QStringLiteral("kleine"), QStringLiteral("kleiner"), QStringLiteral("kleinen"), QStringLiteral("groß"),
        QStringLiteral("große"), QStringLiteral("großer"), QStringLiteral("großen"), QStringLiteral("mittelgroß"),
        QStringLiteral("mittelgroße"), QStringLiteral("gehackt"), QStringLiteral("gehackte"),
        QStringLiteral("gehackter"), QStringLiteral("gewürfelt"), QStringLiteral("gewürfelte"),
        QStringLiteral("gerieben"), QStringLiteral("geriebene"), QStringLiteral("geriebener"),
        QStringLiteral("geschält"), QStringLiteral("geschälte"), QStringLiteral("gemahlen"),
        QStringLiteral("gemahlene"), QStringLiteral("gemahlener"), QStringLiteral("getrocknet"),
        QStringLiteral("getrocknete"), QStringLiteral("getrockneter"), QStringLiteral("getrocknetes"), QStringLiteral("davon"),
        QStringLiteral("tk"), QStringLiteral("tiefgekühlt"), QStringLiteral("tiefgekühlte"), QStringLiteral("bio"),
        QStringLiteral("reif"), QStringLiteral("reife"), QStringLiteral("reifer"), QStringLiteral("weich"),
        QStringLiteral("kalt"), QStringLiteral("kalte"), QStringLiteral("kaltes"), QStringLiteral("warm"),
        QStringLiteral("lauwarm"), QStringLiteral("lauwarme"), QStringLiteral("gekocht"), QStringLiteral("roh"),
        QStringLiteral("ca"), QStringLiteral("etwa"), QStringLiteral("etwas"), QStringLiteral("evtl"),
        QStringLiteral("nach"), QStringLiteral("belieben"), QStringLiteral("geschmack"), QStringLiteral("zum"),
        QStringLiteral("für"), QStringLiteral("den"), QStringLiteral("die"), QStringLiteral("das"),
        QStringLiteral("der"), QStringLiteral("dem"), QStringLiteral("in"), QStringLiteral("scheiben"),
        QStringLiteral("stücken"), QStringLiteral("streifen"), QStringLiteral("würfeln"), QStringLiteral("ringe"),
        QStringLiteral("ringen"), QStringLiteral("abgetropft"), QStringLiteral("zimmerwarm"),
        QStringLiteral("aus"), QStringLiteral("vom"), QStringLiteral("von"), QStringLiteral("mit"),
        QStringLiteral("ohne"), QStringLiteral("einige"), QStringLiteral("wenig"), QStringLiteral("viel"),
        QStringLiteral("fresh"), QStringLiteral("chopped"), QStringLiteral("finely"), QStringLiteral("minced"),
        QStringLiteral("diced"), QStringLiteral("sliced"), QStringLiteral("grated"), QStringLiteral("shredded"),
        QStringLiteral("peeled"), QStringLiteral("softened"), QStringLiteral("melted"), QStringLiteral("cold"),
        QStringLiteral("frozen"), QStringLiteral("cooked"), QStringLiteral("canned"), QStringLiteral("drained"),
        QStringLiteral("to"), QStringLiteral("taste"), QStringLiteral("of"), QStringLiteral("the"),
        QStringLiteral("a"), QStringLiteral("an"), QStringLiteral("for"), QStringLiteral("about"),
        QStringLiteral("optional"), QStringLiteral("packed"), QStringLiteral("firmly"), QStringLiteral("lightly"),
    };
    QStringList words;
    for (const QString &w : s.split(QLatin1Char(' '), Qt::SkipEmptyParts))
        if (!filler.contains(w)) words << w;
    return words.join(QLatin1Char(' '));
}

const Food *FoodTable::match(const QString &ingredientName) const
{
    const QString n = normalize(ingredientName);
    if (n.isEmpty()) return nullptr;
    // 1. vom Benutzer gelernt
    auto l = m_learned.constFind(n);
    if (l != m_learned.constEnd()) {
        if (const Food *f = find(*l)) return f;
    }
    // 2. genauer Name
    auto e = m_exact.constFind(n);
    if (e != m_exact.constEnd()) return find(*e);
    // 3. längster Name, der als Wort, als Wortende (Komposita: „Rinderhackfleisch“) oder mit
    //    Beugungsendung („Zwiebeln“, „Tomaten“) vorkommt
    const QStringList words = n.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QString padded = QLatin1Char(' ') + n + QLatin1Char(' ');
    const Alias *best = nullptr;
    int bestScore = 0;
    for (const Alias &a : m_aliases) {
        const int len = int(a.text.size());
        if (len * 3 < bestScore) break;   // kürzere können nicht mehr gewinnen
        int score = 0;
        if (padded.contains(QLatin1Char(' ') + a.text + QLatin1Char(' '))) {
            score = len * 3;
        } else if (!a.text.contains(QLatin1Char(' ')) && len >= 4) {
            for (const QString &w : words) {
                if (w.size() > len && w.endsWith(a.text)) score = qMax(score, len * 2);
                else if (w.startsWith(a.text) && w.size() - len <= 3) {
                    static const QStringList endings{ QStringLiteral("n"), QStringLiteral("en"), QStringLiteral("e"),
                                                      QStringLiteral("s"), QStringLiteral("es"), QStringLiteral("er"),
                                                      QStringLiteral("ern"), QStringLiteral("nen") };
                    if (endings.contains(w.mid(len))) score = qMax(score, len * 3 - 1);
                }
            }
        } else if (a.text.contains(QLatin1Char(' ')) && padded.contains(QLatin1Char(' ') + a.text)) {
            score = len * 2;   // mehrteiliger Name mit Endung („granny smith apples“)
        }
        if (score > bestScore) {
            bestScore = score;
            best = &a;
        }
    }
    return best ? find(best->key) : nullptr;
}

QStringList FoodTable::categories()
{
    return { QStringLiteral("gemuese"), QStringLiteral("obst"), QStringLiteral("kartoffel"), QStringLiteral("getreide"),
             QStringLiteral("backwaren"), QStringLiteral("huelsen"), QStringLiteral("nuss"), QStringLiteral("milch"),
             QStringLiteral("kaese"), QStringLiteral("ei"), QStringLiteral("fleisch"), QStringLiteral("fisch"),
             QStringLiteral("fett"), QStringLiteral("zucker"), QStringLiteral("suess"), QStringLiteral("gewuerz"),
             QStringLiteral("kraeuter"), QStringLiteral("sosse"), QStringLiteral("getraenk"), QStringLiteral("alkohol"),
             QStringLiteral("honig"), QStringLiteral("gelatine"), QStringLiteral("sonstiges") };
}

QString FoodTable::categoryName(const QString &c)
{
    static const QHash<QString, const char *> n{
        { QStringLiteral("gemuese"), "Gemüse" }, { QStringLiteral("obst"), "Obst" },
        { QStringLiteral("kartoffel"), "Kartoffeln" }, { QStringLiteral("getreide"), "Getreide, Nudeln, Reis" },
        { QStringLiteral("backwaren"), "Brot und Backwaren" }, { QStringLiteral("huelsen"), "Hülsenfrüchte, Soja" },
        { QStringLiteral("nuss"), "Nüsse und Saaten" }, { QStringLiteral("milch"), "Milch und Milchprodukte" },
        { QStringLiteral("kaese"), "Käse" }, { QStringLiteral("ei"), "Eier" }, { QStringLiteral("fleisch"), "Fleisch und Wurst" },
        { QStringLiteral("fisch"), "Fisch und Meeresfrüchte" }, { QStringLiteral("fett"), "Fette und Öle" },
        { QStringLiteral("zucker"), "Zucker und Sirup" }, { QStringLiteral("suess"), "Süßwaren" },
        { QStringLiteral("gewuerz"), "Gewürze" }, { QStringLiteral("kraeuter"), "Kräuter" },
        { QStringLiteral("sosse"), "Soßen, Brühe, Würzmittel" }, { QStringLiteral("getraenk"), "Getränke" },
        { QStringLiteral("alkohol"), "Alkoholische Getränke" }, { QStringLiteral("honig"), "Honig" },
        { QStringLiteral("gelatine"), "Gelatine" }, { QStringLiteral("sonstiges"), "Sonstiges" },
    };
    return QCoreApplication::translate("FoodTable", n.value(c, "Sonstiges"));
}
