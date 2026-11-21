// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "foodtable.h"

#include <QCoreApplication>
#include <QCryptographicHash>
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
    QJsonObject o{ { QStringLiteral("key"), key },
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
    if (!barcode.isEmpty()) o[QStringLiteral("barcode")] = barcode;
    return o;
}

Food Food::fromJson(const QJsonObject &o)
{
    Food f;
    f.key = o.value(QStringLiteral("key")).toString();
    f.name = o.value(QStringLiteral("name")).toString();
    f.barcode = o.value(QStringLiteral("barcode")).toString();
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
    if (dir.isEmpty()) dir = QDir::homePath() + QStringLiteral("/.viviameals");
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("lebensmittel_eigene.json"));
}

void FoodTable::reload()
{
    m_foods.clear();
    m_builtin.clear();
    m_userFoods.clear();
    m_learned.clear();
    m_matchCache.clear();
    load();
}

void FoodTable::load()
{
    QFile f(QStringLiteral(":/data/lebensmittel.json"));
    if (f.open(QIODevice::ReadOnly)) {
        const QByteArray data = f.readAll();
        m_builtinStamp = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex().left(16));
        const QJsonObject o = QJsonDocument::fromJson(data).object();
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
        for (auto it = l.constBegin(); it != l.constEnd(); ++it) m_learned.insert(normalize(it.key()), it.value().toString());
    }
    m_foods = m_builtin;
    for (auto it = m_userFoods.constBegin(); it != m_userFoods.constEnd(); ++it) m_foods.insert(it.key(), it.value());
    rebuildAliases();
}

void FoodTable::rebuildAliases()
{
    m_aliases.clear();
    m_exact.clear();
    m_matchCache.clear();
    // eigene Einträge zuerst, damit ihre Namen gewinnen
    // feste Reihenfolge (nach Schlüssel) statt der zufälligen der Hash-Tabelle: gleiche Zuordnung bei jedem Start
    QList<const Food *> order;
    for (const Food &f : m_foods)
        if (f.user) order << &f;
    for (const Food &f : m_foods)
        if (!f.user) order << &f;
    std::stable_sort(order.begin(), order.end(), [](const Food *a, const Food *b) {
        if (a->user != b->user) return a->user;
        return a->key < b->key;
    });
    // erst alle Namen aus den Namenslisten, dann die Anzeigenamen – sonst würde „Makrele (geräuchert)“ ohne
    // Klammerzusatz zu „makrele“ und könnte der frischen Makrele den Namen wegnehmen
    // (eigene Lebensmittel behalten den Vorrang auch mit ihrem Namen)
    QList<QPair<const Food *, QString>> names;
    for (const Food *f : order) {
        for (const QString &a : f->aliases) names << qMakePair(f, a);
        if (f->user) names << qMakePair(f, f->name.toLower());
    }
    for (const Food *f : order)
        if (!f->user) names << qMakePair(f, f->name.toLower());
    for (const auto &entry : names) {
        const Food *f = entry.first;
        const QString &a = entry.second;
        {
            const QString n = normalize(a);
            if (n.isEmpty() || m_exact.contains(n)) continue;
            m_exact.insert(n, f->key);
            Alias al;
            al.text = n;
            al.key = f->key;
            al.spaced = QLatin1Char(' ') + n + QLatin1Char(' ');
            al.leading = QLatin1Char(' ') + n;
            al.multiWord = n.contains(QLatin1Char(' '));
            m_aliases.append(al);
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
    f.write(QJsonDocument(QJsonObject{ { QStringLiteral("format"), QStringLiteral("ViviaMeals-UserFoods") },
                                       { QStringLiteral("foods"), foods },
                                       { QStringLiteral("learned"), learned } })
                .toJson());
    f.commit();
}

QString FoodTable::saveUserFood(const Food &food)
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
    return f.key;
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
    m_matchCache.clear();
    saveUser();
}

QString FoodTable::learnedFor(const QString &ingredientName) const
{
    return m_learned.value(normalize(ingredientName));
}

const Food *FoodTable::findBarcode(const QString &code) const
{
    if (code.isEmpty()) return nullptr;
    for (const Food &f : m_userFoods)
        if (f.barcode == code) return &m_foods.find(f.key).value();
    return nullptr;
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
    QString lower = name.toLower();
    // alte DOS-Sammlungen (CP850 als Windows-1252 gelesen): „Eiweiá“ = Eiweiß, „fraŒche“ = fraîche
    static const QRegularExpression mojibakeSz(QStringLiteral("(?<=[aeiouäöü])á"));
    lower.replace(mojibakeSz, QStringLiteral("ß"));
    lower.replace(QChar(0x0153), QChar(0x00EE));   // œ (kleingeschriebenes Œ) → î
    QString s = foldUmlauts(lower);
    static const QRegularExpression kRe282_0(QStringLiteral("\\([^)]*\\)"));
    s.remove(kRe282_0);            // Klammerzusätze
    static const QRegularExpression kRe283_0(QStringLiteral("[\\d½¼¾⅓⅔⅛%]+"));
    s.replace(kRe283_0, QStringLiteral(" "));
    static const QRegularExpression kRe284_0(QStringLiteral("[^\\p{L}\\- ]"));
    s.replace(kRe284_0, QStringLiteral(" "));
    s.replace(QLatin1Char('-'), QLatin1Char(' '));
    static const QSet<QString> fillerRaw{
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
        // Mengen- und Gebindewörter, die im Namen stehen geblieben sind („Dose Bambussprossen“, „4 zw Peterli“)
        QStringLiteral("po"), QStringLiteral("zw"), QStringLiteral("stück"), QStringLiteral("stücke"), QStringLiteral("stk"),
        QStringLiteral("handvoll"), QStringLiteral("dose"), QStringLiteral("glas"), QStringLiteral("paket"),
        QStringLiteral("packung"), QStringLiteral("päckchen"), QStringLiteral("beutel"), QStringLiteral("tüte"),
        QStringLiteral("briefchen"), QStringLiteral("döschen"), QStringLiteral("kapsel"), QStringLiteral("flasche"),
        QStringLiteral("fl"), QStringLiteral("topf"), QStringLiteral("schüssel"), QStringLiteral("spritzer"),
        QStringLiteral("tropfen"), QStringLiteral("tr"), QStringLiteral("zweig"), QStringLiteral("zweige"),
        QStringLiteral("stengel"), QStringLiteral("stängel"), QStringLiteral("stiele"), QStringLiteral("blätter"),
        QStringLiteral("blatt"),
        // Beschreibungen, die das Lebensmittel nicht ändern
        QStringLiteral("junge"), QStringLiteral("junger"), QStringLiteral("junges"), QStringLiteral("jungen"),
        QStringLiteral("dünne"), QStringLiteral("dünnen"), QStringLiteral("dicke"), QStringLiteral("abgehangene"),
        QStringLiteral("ausgelöst"), QStringLiteral("ausgelöste"), QStringLiteral("ausgelöstes"), QStringLiteral("küchenfertig"),
        QStringLiteral("küchenfertige"), QStringLiteral("bratfertige"), QStringLiteral("eingeweichte"), QStringLiteral("eingelegte"),
        QStringLiteral("eingel"), QStringLiteral("trockene"), QStringLiteral("trockener"), QStringLiteral("pikante"),
        QStringLiteral("kräftig"), QStringLiteral("extrascharfe"), QStringLiteral("abgeriebene"), QStringLiteral("ungezuckertes"),
        QStringLiteral("ungesalzen"), QStringLiteral("ungesalzene"), QStringLiteral("geschälte"), QStringLiteral("frischen"),
    };
    static const QSet<QString> filler = [] {
        QSet<QString> f;
        for (const QString &w : fillerRaw) f.insert(foldUmlauts(w));
        return f;
    }();
    QStringList words;
    for (const QString &w : s.split(QLatin1Char(' '), Qt::SkipEmptyParts))
        if (!filler.contains(w)) words << w;
    return words.join(QLatin1Char(' '));
}

// „Gemüsebrühe“ und „Gemuesebruehe“, „Gruyère“ und „Gruyere“ gleich behandeln
QString FoodTable::foldUmlauts(const QString &s)
{
    QString o;
    o.reserve(s.size() + 8);
    for (QChar c : s) {
        switch (c.unicode()) {
        case 0x00E4: o += QStringLiteral("ae"); break;
        case 0x00F6: o += QStringLiteral("oe"); break;
        case 0x00FC: o += QStringLiteral("ue"); break;
        case 0x00DF: o += QStringLiteral("ss"); break;
        case 0x00E0: case 0x00E1: case 0x00E2: o += QLatin1Char('a'); break;
        case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB: o += QLatin1Char('e'); break;
        case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF: o += QLatin1Char('i'); break;
        case 0x00F2: case 0x00F3: case 0x00F4: o += QLatin1Char('o'); break;
        case 0x00F9: case 0x00FA: case 0x00FB: o += QLatin1Char('u'); break;
        case 0x00E7: o += QLatin1Char('c'); break;
        case 0x00F1: o += QLatin1Char('n'); break;
        default: o += c;
        }
    }
    return o;
}

namespace {
// Wortteile, die an einen Lebensmittelnamen angehängt werden, ohne das Lebensmittel zu ändern
bool isPartWord(QString rest)
{
    static const QStringList parts{
        QStringLiteral("filet"), QStringLiteral("filets"), QStringLiteral("koerner"), QStringLiteral("korn"),
        QStringLiteral("pulver"), QStringLiteral("ruecken"), QStringLiteral("brust"), QStringLiteral("brueste"),
        QStringLiteral("keule"), QStringLiteral("keulen"), QStringLiteral("schenkel"), QStringLiteral("blaetter"),
        QStringLiteral("blatt"), QStringLiteral("stuecke"), QStringLiteral("stueckchen"), QStringLiteral("scheiben"),
        QStringLiteral("wuerfel"), QStringLiteral("streifen"), QStringLiteral("roellchen"), QStringLiteral("schrot"),
        QStringLiteral("flocken"), QStringLiteral("spitzen"), QStringLiteral("ringe"), QStringLiteral("haelften"),
        QStringLiteral("samen"), QStringLiteral("kerne"), QStringLiteral("stangen"), QStringLiteral("roeschen"),
        QStringLiteral("knollen"), QStringLiteral("zehen"), QStringLiteral("fleisch"), QStringLiteral("sirup"),
        QStringLiteral("blaettchen"), QStringLiteral("zweig"), QStringLiteral("zweige"), QStringLiteral("zweiglein"),
        QStringLiteral("stengel"), QStringLiteral("stiele"), QStringLiteral("wuerfelchen"), QStringLiteral("splitter"),
        QStringLiteral("floeckchen"), QStringLiteral("stueck"), QStringLiteral("pueree"), QStringLiteral("herzen"),
        QStringLiteral("stauden"), QStringLiteral("raspeln"), QStringLiteral("rispen"), QStringLiteral("wurzelstueck"),
        QStringLiteral("viertel"), QStringLiteral("medaillons"), QStringLiteral("stangenstueck"),
    };
    for (const QString &link : { QString(), QStringLiteral("n"), QStringLiteral("en"), QStringLiteral("s"), QStringLiteral("es") }) {
        if (!rest.startsWith(link)) continue;
        if (parts.contains(rest.mid(link.size()))) return true;
    }
    return false;
}
} // namespace

// Beschreibungen, die beim Normalisieren als Füllwort wegfallen („frisch“, „ausgelöst“, „getrocknet“, „Dose“),
// hier aber über das Lebensmittel entscheiden; geprüft vor dem gelernten und dem genauen Namen
const Food *FoodTable::matchByRule(const QString &text) const
{
    struct Rule {
        const char *what;    // Teil des Namens (regulärer Ausdruck)
        const char *with;    // zusätzlich nötiges Wort
        const char *key;
    };
    static const Rule rules[] = {
        { "^(?!.*(jakobs|grünlipp|scallop|green)).*(muschel|clam|mussel)", "ausgelöst|ohne schale|shelled|shucked|canned|chopped", "muschelfleisch" },
        { "kabeljau|dorsch|\\bcod\\b", "getrocknet|gesalzen|dried|salted", "stockfisch" },
        { "thunfisch|tunfisch|\\btuna", "frisch|fresh|steak|\\broh|sushi|sashimi", "thunfisch_frisch" },
        { "thunfisch|tunfisch|\\btuna", "in öl|olivenöl|pflanzenöl|in oil", "thunfisch_oel" },
        { "sardine", "in öl|olivenöl|\\bdose|dosen|in oil|canned|tinned", "sardinen_oel" },
        { "sardelle|anchovi|anchovy|\\bsprotte", "frisch|fresh", "sardinen" },
        { "sardelle|anchovi|anchovy|boquerones", "essig|vinagre|mariniert|eingelegt|marinated|pickled", "sardellen_essig" },
        { "jackfruit|jackfrucht", "jung|grün|lake|natur|pulled|dose|brine|young|green|canned", "jackfruit_jung" },
        // getrocknetes Obst ist Trockenobst (Datteln, Rosinen, Goji haben eigene Einträge)
        { "aprikose|marille|feige|pflaume|zwetschge|cranberr|preiselbeer|kirsche|mango|ananas|banane|apfel|äpfel|birne|papaya|kaki|"
          "apricot|\\bfigs?\\b|prune|\\bplums?\\b|cherr|pineapple|banana|apple|\\bpears?\\b",
          "getrocknet|gedörrt|dörr|soft|dried|sun-dried", "trockenobst" },
        { "makrele|mackerel", "geräuchert|räucher|smoked", "makrele_geraeuchert" },
        { "forelle|saibling|trout|char\\b", "geräuchert|räucher|smoked", "forelle_geraeuchert" },
        { "\\baal\\b|\\beel\\b", "geräuchert|räucher|smoked", "aal_geraeuchert" },
        { "hering|herring", "geräuchert|räucher|smoked", "bueckling" },
        // Füllwörter („getrocknet“, „gekocht“, „gehackt“ …) entfallen beim Vereinfachen – hier entscheiden sie
        { "tomate|tomato", "getrocknet|sun-dried|sundried|dried", "getrocknete_tomaten" },
        { "tomate|tomato", "\\bdose|dosen|gehackt|geschält|stückig|passiert|canned|tinned|diced|peeled|crushed|stewed|\\bcan\\b", "tomaten_dose" },
        { "erbse|\\bpeas?\\b", "getrocknet|trocken|dried|\\bsplit\\b", "bohnen_trocken" },
        { "bohne|\\bbeans?\\b", "getrocknet|trocken|dried|\\bdry\\b", "bohnen_trocken" },
        { "thymian|oregano|rosmarin|bohnenkraut|estragon|thyme|rosemary", "getrocknet|gerebelt|dried", "gewuerze" },
        { "^(?!.*(kokosmilch|kokosöl|kokosfett|kokoswasser|coconut milk|coconut oil|coconut water)).*(kokos|coconut)", "frisch|fresh", "kokosnuss_frisch" },
        { "reis\\b|rice", "gekocht|gegart|\\bcooked|\\bboiled|\\bsteamed", "reis_gekocht" },
        { "quark", "40|20|sahne|rahm", "quark_sahne" },
        { "milch|\\bmilk", "1,5|1\\.5|fettarm|teilentrahmt|low-?fat|reduced-?fat|semi-?skimmed|\\b2 ?%", "milch_fettarm" },
        { "joghurt|jogurt|yogh?urt", "10 ?%|rahm|sahne|full[ -]fat", "rahmjoghurt" },
        { "zitrone|limone|\\blemon", "abgerieben|abrieb|schale|\\bzest|\\bpeel", "zitronenschale" },
        { "\\bmsg\\b|glutamat", "\\bmsg\\b|glutamat", "glutamat" },
        { "cream of tartar|weinstein", "cream of tartar|weinstein", "zitronensaeure" },
    };
    static const QList<QPair<QRegularExpression, QPair<QRegularExpression, QString>>> compiled = [] {
        QList<QPair<QRegularExpression, QPair<QRegularExpression, QString>>> out;
        const auto opt = QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption;
        for (const Rule &r : rules)
            out << qMakePair(QRegularExpression(QString::fromUtf8(r.what), opt),
                             qMakePair(QRegularExpression(QString::fromUtf8(r.with), opt), QString::fromLatin1(r.key)));
        return out;
    }();
    for (const auto &r : compiled) {
        if (text.contains(r.first) && text.contains(r.second.first)) {
            if (const Food *f = find(r.second.second)) return f;
        }
    }
    return nullptr;
}

const Food *FoodTable::match(const QString &ingredientName) const
{
    const QString n = normalize(ingredientName);
    if (n.isEmpty()) return nullptr;
    if (const Food *f = matchByRule(ingredientName)) return f;
    // 1. vom Benutzer gelernt
    auto l = m_learned.constFind(n);
    if (l != m_learned.constEnd()) {
        if (const Food *f = find(*l)) return f;
    }
    // 2. genauer Name
    auto e = m_exact.constFind(n);
    if (e != m_exact.constEnd()) return find(*e);
    // schon einmal gesucht? (Ergebnis hängt nur vom normalisierten Namen ab)
    auto cached = m_matchCache.constFind(n);
    if (cached != m_matchCache.constEnd()) return cached->isEmpty() ? nullptr : find(*cached);
    // 3. längster Name, der als Wort, als Wortende (Komposita: „Rinderhackfleisch“) oder mit
    //    Beugungsendung („Zwiebeln“, „Tomaten“) vorkommt
    const QStringList words = n.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QString padded = QLatin1Char(' ') + n + QLatin1Char(' ');
    const Alias *best = nullptr;
    int bestScore = 0;
    for (const Alias &a : m_aliases) {
        const int len = int(a.text.size());
        if (len * 3 < bestScore) break;   // kürzere können nicht mehr gewinnen
        if (len > padded.size()) continue;   // länger als der ganze Name: kann nirgends vorkommen
        int score = 0;
        if (padded.contains(a.spaced)) {
            score = len * 3;
        } else if (!a.multiWord && len >= 4) {
            for (const QString &w : words) {
                if (w.size() > len && w.endsWith(a.text)) score = qMax(score, len * 2);
                else if (len >= 5 && w.size() > len + 2 && w.startsWith(a.text) && isPartWord(w.mid(len)))
                    score = qMax(score, len * 2 - 1);   // „Zanderfilets“, „Nelkenpulver“, „Hasenrücken“
                else if (w.startsWith(a.text) && w.size() - len <= 3) {
                    static const QStringList endings{ QStringLiteral("n"), QStringLiteral("en"), QStringLiteral("e"),
                                                      QStringLiteral("s"), QStringLiteral("es"), QStringLiteral("er"),
                                                      QStringLiteral("ern"), QStringLiteral("nen") };
                    if (endings.contains(w.mid(len))) score = qMax(score, len * 3 - 1);
                }
            }
        } else if (a.multiWord && padded.contains(a.leading)) {
            score = len * 2;   // mehrteiliger Name mit Endung („granny smith apples“)
        }
        if (score > bestScore) {
            bestScore = score;
            best = &a;
        }
    }
    // Einzahl zur Mehrzahl in der Tabelle: „Champignon“ → „champignons“, „Nelke“ → „nelken“, „Mandel“ → „mandeln“
    if (!best) {
        static const QStringList plural{ QStringLiteral("n"), QStringLiteral("en"), QStringLiteral("e"), QStringLiteral("s"),
                                         QStringLiteral("es"), QStringLiteral("er"), QStringLiteral("nen") };
        for (const Alias &a : m_aliases) {
            if (a.multiWord) continue;
            for (const QString &w : words) {
                if (w.size() < 4 || a.text.size() <= w.size() || a.text.size() - w.size() > 3 || !a.text.startsWith(w)) continue;
                if (!plural.contains(a.text.mid(w.size()))) continue;
                const int score = int(w.size()) * 3 - 2;
                if (score > bestScore) {
                    bestScore = score;
                    best = &a;
                }
            }
        }
    }
    // Tippfehler („Pfefffer“, „Zwiebl“, „Puderzucekr“): ein Zeichen zu viel, zu wenig, falsch oder vertauscht –
    // nur für längere Wörter und nur, wenn sonst nichts passt
    if (!best) {
        int bestLen = 0;
        for (const Alias &a : m_aliases) {
            if (a.multiWord || a.text.size() < 6) continue;
            for (const QString &w : words) {
                if (w.size() < 6 || qAbs(int(w.size()) - int(a.text.size())) > 1 || w == a.text) continue;
                if (editDistanceAtMostOne(w, a.text) && a.text.size() > bestLen) {
                    bestLen = int(a.text.size());
                    best = &a;
                }
            }
        }
    }
    m_matchCache.insert(n, best ? best->key : QString());
    return best ? find(best->key) : nullptr;
}

bool FoodTable::isNonFood(const QString &ingredientName)
{
    // Küchenhelfer und Verpackung
    static const QSet<QString> things{
        QStringLiteral("backpapier"), QStringLiteral("alufolie"), QStringLiteral("aluminiumfolie"), QStringLiteral("zahnstocher"),
        QStringLiteral("kuechengarn"), QStringLiteral("kuechenschnur"), QStringLiteral("bindfaden"), QStringLiteral("bambusmatte"),
        QStringLiteral("sushimatte"), QStringLiteral("klarsichtfolie"), QStringLiteral("frischhaltefolie"), QStringLiteral("pergamentpapier"),
        QStringLiteral("kuechenpapier"), QStringLiteral("gefrierbeutel"), QStringLiteral("einmachglaeser"), QStringLiteral("passiertuch"),
        QStringLiteral("mulltuch"), QStringLiteral("nylontuch"), QStringLiteral("moerser"), QStringLiteral("loetlampe"), QStringLiteral("wok"),
        QStringLiteral("ziegelstein"), QStringLiteral("tumbler"), QStringLiteral("shaker"), QStringLiteral("muffinblech"),
        QStringLiteral("springform"), QStringLiteral("tortenring"), QStringLiteral("backrahmen"), QStringLiteral("auflaufform"),
        QStringLiteral("stollenform"), QStringLiteral("raeucherspaene"), QStringLiteral("blattgold"), QStringLiteral("buchstabenausstecher"),
        QStringLiteral("baender"), QStringLiteral("schablone"), QStringLiteral("kochbeutel"), QStringLiteral("twist off glas"),
        QStringLiteral("gugelhupf form"), QStringLiteral("stern form"), QStringLiteral("dessertring"), QStringLiteral("toertchen"),
        QStringLiteral("kalbsdarm"), QStringLiteral("schweinedaerme"), QStringLiteral("schweinedarm"), QStringLiteral("saitlinge"),
        QStringLiteral("jakobsmuschelschalen"),
    };
    // Wortenden, die immer Geräte bezeichnen („Papierbackförmchen“, „Holzspieße“, „Muffinform“)
    static const QStringList endings{ QStringLiteral("foermchen"), QStringLiteral("folie"), QStringLiteral("spiesse"),
                                      QStringLiteral("spiesschen"), QStringLiteral("spieszchen"), QStringLiteral("staebchen"),
                                      QStringLiteral("form"), QStringLiteral("matte"), QStringLiteral("ausstecher"),
                                      QStringLiteral("schablone"), QStringLiteral("glaeser") };
    const QString n = normalize(ingredientName);
    if (n.isEmpty()) return false;
    if (things.contains(n)) return true;
    for (const QString &w : n.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        if (things.contains(w)) return true;
        if (w == QLatin1String("reispapier") || w.endsWith(QLatin1String("reispapier"))) continue;
        if (w.endsWith(QLatin1String("papier")) && w.size() > 6) return true;
        for (const QString &e : endings)
            if (w.endsWith(e)) return true;
    }
    return false;
}

bool FoodTable::editDistanceAtMostOne(const QString &a, const QString &b)
{
    const int la = int(a.size()), lb = int(b.size());
    if (qAbs(la - lb) > 1) return false;
    int i = 0;
    while (i < la && i < lb && a[i] == b[i]) ++i;
    if (i == la && i == lb) return true;
    if (la == lb) {
        if (a.mid(i + 1) == b.mid(i + 1)) return true;                                                  // ersetzt
        return i + 1 < la && a[i] == b[i + 1] && a[i + 1] == b[i] && a.mid(i + 2) == b.mid(i + 2);      // vertauscht
    }
    return la > lb ? a.mid(i + 1) == b.mid(i) : a.mid(i) == b.mid(i + 1);                               // eingefügt/fehlt
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
