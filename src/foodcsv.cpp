// SPDX-License-Identifier: MIT AND LicenseRef-Commons-Clause-1.0 (siehe LICENSE)
#include "foodcsv.h"
#include "textcodec.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <cmath>

namespace FoodCsv {

namespace {

QString tr(const char *s) { return QCoreApplication::translate("FoodCsv", s); }

// Eine Zeile mit Anführungszeichen-Regeln (RFC 4180) zerlegen
QStringList splitLine(const QString &line, QChar delim)
{
    QStringList out;
    QString cur;
    bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (quoted) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < line.size() && line[i + 1] == QLatin1Char('"')) {
                    cur += c;
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                cur += c;
            }
        } else if (c == QLatin1Char('"') && cur.trimmed().isEmpty()) {
            quoted = true;
            cur.clear();
        } else if (c == delim) {
            out << cur.trimmed();
            cur.clear();
        } else {
            cur += c;
        }
    }
    out << cur.trimmed();
    return out;
}

bool openQuote(const QString &line)
{
    return line.count(QLatin1Char('"')) % 2 == 1;
}

// Datei zeilenweise lesen (auch große Dateien), Datensätze mit Zeilenumbruch in Anführungszeichen zusammensetzen
bool forEachRecord(const QString &path, QChar *delim, QString *encoding, QString *error,
                   const std::function<bool(const QStringList &)> &fn)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    // Zeichensatz am Anfang der Datei erkennen
    const QByteArray head = f.peek(256 * 1024);
    QString enc;
    TextCodec::decode(head.left(head.lastIndexOf('\n') > 0 ? head.lastIndexOf('\n') : head.size()), &enc);
    if (encoding) *encoding = enc;
    QChar d = delim && !delim->isNull() ? *delim : QChar();
    if (d.isNull()) {
        // Trennzeichen: das häufigste der ersten Zeile
        const QString first = TextCodec::decode(head.left(head.indexOf('\n') > 0 ? head.indexOf('\n') : head.size()));
        int best = -1;
        for (QChar c : { QLatin1Char(';'), QLatin1Char('\t'), QLatin1Char(','), QLatin1Char('|') }) {
            const int n = int(first.count(c));
            if (n > best) {
                best = n;
                d = c;
            }
        }
        if (delim) *delim = d;
    }
    QString pending;
    bool firstLine = true;
    while (!f.atEnd()) {
        QByteArray raw = f.readLine();
        if (firstLine && raw.startsWith("\xEF\xBB\xBF")) raw = raw.mid(3);
        firstLine = false;
        QString line = enc == QLatin1String("UTF-8") ? QString::fromUtf8(raw) : TextCodec::decode(raw);
        while (line.endsWith(QLatin1Char('\n')) || line.endsWith(QLatin1Char('\r'))) line.chop(1);
        if (!pending.isEmpty()) line = pending + QLatin1Char('\n') + line;
        if (openQuote(line) && !f.atEnd()) {
            pending = line;
            continue;
        }
        pending.clear();
        if (line.trimmed().isEmpty()) continue;
        if (!fn(splitLine(line, d))) break;
    }
    return true;
}

QString normHeader(QString h)
{
    h = h.toLower().trimmed();
    h.remove(QRegularExpression(QStringLiteral("_100g$")));
    h.remove(QRegularExpression(QStringLiteral("\\s*(?:\\(|\\[)?\\s*(?:g|mg|je|pro|per|/)?\\s*100\\s*g\\s*(?:\\)|\\])?")));
    return h.simplified();
}

struct Rule {
    Field field;
    QStringList exact;      // genaue Überschrift (normalisiert)
    QStringList contains;   // enthält
    QStringList exclude;    // darf nicht enthalten
};

const QList<Rule> &rules()
{
    static const QList<Rule> r{
        { SatFat, { QStringLiteral("saturated-fat"), QStringLiteral("gesättigte fettsäuren"), QStringLiteral("ges. fs") },
          { QStringLiteral("gesättigt"), QStringLiteral("gesaettigt"), QStringLiteral("saturated") }, { QStringLiteral("unsatur"), QStringLiteral("ungesätt") } },
        { Sugar, { QStringLiteral("sugars"), QStringLiteral("zucker"), QStringLiteral("davon zucker") }, { QStringLiteral("zucker"), QStringLiteral("sugar") },
          { QStringLiteral("alkohol"), QStringLiteral("austausch"), QStringLiteral("added") } },
        { Fiber, { QStringLiteral("fiber"), QStringLiteral("ballaststoffe") }, { QStringLiteral("ballast"), QStringLiteral("fiber"), QStringLiteral("fibre") }, {} },
        { Kcal, { QStringLiteral("energy-kcal"), QStringLiteral("kcal") }, { QStringLiteral("kcal"), QStringLiteral("kalorien"), QStringLiteral("calories") },
          { QStringLiteral("fat"), QStringLiteral("fett") } },
        { KJ, { QStringLiteral("energy-kj"), QStringLiteral("kj"), QStringLiteral("energy") }, { QStringLiteral("kj"), QStringLiteral("kilojoule"), QStringLiteral("brennwert"), QStringLiteral("energie"), QStringLiteral("energy") },
          { QStringLiteral("kcal"), QStringLiteral("fat"), QStringLiteral("fett"), QStringLiteral("prepared") } },
        { Sodium, { QStringLiteral("sodium"), QStringLiteral("natrium") }, { QStringLiteral("natrium"), QStringLiteral("sodium") }, {} },
        { Salt, { QStringLiteral("salt"), QStringLiteral("salz") }, { QStringLiteral("salz"), QStringLiteral("salt") }, { QStringLiteral("natrium") } },
        { Protein, { QStringLiteral("proteins"), QStringLiteral("protein"), QStringLiteral("eiweiß"), QStringLiteral("eiweiss") },
          { QStringLiteral("eiweiß"), QStringLiteral("eiweiss"), QStringLiteral("protein") }, { QStringLiteral("serum"), QStringLiteral("casein") } },
        { Carbs, { QStringLiteral("carbohydrates"), QStringLiteral("kohlenhydrate"), QStringLiteral("kh") },
          { QStringLiteral("kohlenhydrat"), QStringLiteral("carbohydrate") }, {} },
        { Fat, { QStringLiteral("fat"), QStringLiteral("fett"), QStringLiteral("total fat") }, { QStringLiteral("fett"), QStringLiteral("fat"), QStringLiteral("lipid") },
          { QStringLiteral("satur"), QStringLiteral("gesätt"), QStringLiteral("trans"), QStringLiteral("mono"), QStringLiteral("poly"),
            QStringLiteral("omega"), QStringLiteral("säure"), QStringLiteral("acid"), QStringLiteral("energy"), QStringLiteral("from") } },
        { Name, { QStringLiteral("product_name_de"), QStringLiteral("product_name"), QStringLiteral("name"), QStringLiteral("lebensmittel"), QStringLiteral("bezeichnung"),
                  QStringLiteral("description"), QStringLiteral("produkt") },
          { QStringLiteral("name"), QStringLiteral("bezeichnung"), QStringLiteral("lebensmittel"), QStringLiteral("produkt") },
          { QStringLiteral("generic"), QStringLiteral("andere"), QStringLiteral("brand"), QStringLiteral("marke"), QStringLiteral("abbreviated") } },
        { Category, { QStringLiteral("kategorie"), QStringLiteral("category"), QStringLiteral("main_category"), QStringLiteral("warengruppe") },
          { QStringLiteral("kategorie"), QStringLiteral("gruppe"), QStringLiteral("category") }, { QStringLiteral("_tags"), QStringLiteral("_en") } },
        { Aliases, { QStringLiteral("andere namen"), QStringLiteral("aliases"), QStringLiteral("synonyme") }, { QStringLiteral("synonym"), QStringLiteral("alias"), QStringLiteral("andere namen") }, {} },
        { Density, { QStringLiteral("dichte"), QStringLiteral("density") }, { QStringLiteral("dichte"), QStringLiteral("density") }, {} },
        { Piece, { QStringLiteral("stückgewicht"), QStringLiteral("stueckgewicht") }, { QStringLiteral("stückgewicht"), QStringLiteral("stueckgewicht"), QStringLiteral("gewicht je stück"), QStringLiteral("piece") }, {} },
    };
    return r;
}

QString keyFrom(const QString &prefix, const QString &name)
{
    QString s = FoodTable::normalize(name);
    s.replace(QLatin1Char(' '), QLatin1Char('_'));
    return prefix + s.left(60);
}

} // namespace

QString fieldName(Field f)
{
    switch (f) {
    case Name: return tr("Name");
    case Kcal: return tr("Energie (kcal)");
    case KJ: return tr("Energie (kJ)");
    case Protein: return tr("Eiweiß (g)");
    case Fat: return tr("Fett (g)");
    case SatFat: return tr("davon gesättigte Fettsäuren (g)");
    case Carbs: return tr("Kohlenhydrate (g)");
    case Sugar: return tr("davon Zucker (g)");
    case Fiber: return tr("Ballaststoffe (g)");
    case Salt: return tr("Salz (g)");
    case Sodium: return tr("Natrium");
    case Category: return tr("Kategorie");
    case Aliases: return tr("Andere Namen");
    case Density: return tr("Dichte (g/ml)");
    case Piece: return tr("Stückgewicht (g)");
    case FieldCount: break;
    }
    return {};
}

double number(const QString &text, bool *ok)
{
    QString s = text.trimmed();
    s.remove(QChar(0x00A0));
    s.remove(QLatin1Char(' '));
    if (ok) *ok = false;
    if (s.isEmpty() || s == QLatin1String("-") || s.compare(QLatin1String("n.a."), Qt::CaseInsensitive) == 0
        || s.compare(QLatin1String("n/a"), Qt::CaseInsensitive) == 0)
        return 0;
    if (s.compare(QLatin1String("tr"), Qt::CaseInsensitive) == 0 || s.compare(QLatin1String("spuren"), Qt::CaseInsensitive) == 0) {
        if (ok) *ok = true;
        return 0;
    }
    bool half = false;
    if (s.startsWith(QLatin1Char('<'))) {   // „<0,1“ → halber Wert
        half = true;
        s = s.mid(1);
    }
    s.remove(QRegularExpression(QStringLiteral("[^0-9.,eE+\\-]")));
    const int c = int(s.lastIndexOf(QLatin1Char(','))), p = int(s.lastIndexOf(QLatin1Char('.')));
    if (c >= 0 && p >= 0) {
        // beide vorhanden: das letzte ist das Dezimalzeichen
        if (c > p) {
            s.remove(QLatin1Char('.'));
            s.replace(QLatin1Char(','), QLatin1Char('.'));
        } else {
            s.remove(QLatin1Char(','));
        }
    } else if (c >= 0) {
        s.replace(QLatin1Char(','), QLatin1Char('.'));
    }
    bool good = false;
    double v = s.toDouble(&good);
    if (!good || std::isnan(v) || v < 0) return 0;
    if (ok) *ok = true;
    return half ? v / 2.0 : v;
}

bool readTable(const QString &path, Table *out, QString *error, int maxRows)
{
    *out = Table();
    QChar d;
    bool header = true;
    const bool ok = forEachRecord(path, &d, &out->encoding, error, [&](const QStringList &rec) {
        if (header) {
            out->header = rec;
            header = false;
            return true;
        }
        out->rows << rec;
        return maxRows < 0 || out->rows.size() < maxRows;
    });
    out->delimiter = d;
    if (ok && out->header.size() < 2) {
        if (error) *error = tr("Die Datei hat keine erkennbaren Spalten (Trennzeichen ; , Tab oder |).");
        return false;
    }
    return ok;
}

Mapping autoMap(const QStringList &header)
{
    Mapping m;
    QSet<int> used;
    QStringList norm;
    for (const QString &h : header) norm << normHeader(h);
    for (const Rule &r : rules()) {
        int best = -1, bestScore = 0;
        for (int i = 0; i < norm.size(); ++i) {
            if (used.contains(i)) continue;
            const QString &h = norm[i];
            bool excluded = false;
            for (const QString &x : r.exclude)
                if (h.contains(x)) excluded = true;
            if (excluded) continue;
            int score = 0;
            for (int k = 0; k < r.exact.size(); ++k)
                if (h == r.exact[k]) score = qMax(score, 100 - k);   // frühere Einträge bevorzugt
            if (score == 0)
                for (const QString &c : r.contains)
                    if (h.contains(c)) score = qMax(score, 10);
            if (score > bestScore) {
                bestScore = score;
                best = i;
            }
        }
        if (best >= 0) {
            m.col[r.field] = best;
            used.insert(best);
            if (r.field == Sodium) m.sodiumMg = header[best].contains(QStringLiteral("mg"), Qt::CaseInsensitive);
        }
    }
    return m;
}

QString guessCategory(const QString &text, const QString &fallback)
{
    const QString t = text.toLower();
    // eigene Kategorie-Schlüssel bzw. -Namen
    for (const QString &c : FoodTable::categories())
        if (t == c || t == FoodTable::categoryName(c).toLower()) return c;
    struct K { const char *word; const char *cat; };
    static const K keys[] = {
        { "dairy", "milch" }, { "egg", "ei" }, { "eier", "ei" }, { "cheese", "kaese" }, { "käse", "kaese" }, { "milch", "milch" },
        { "joghurt", "milch" }, { "spice", "gewuerz" }, { "gewürz", "gewuerz" }, { "herb", "kraeuter" }, { "kräuter", "kraeuter" },
        { "fats and oils", "fett" }, { "öl", "fett" }, { "fett", "fett" }, { "oil", "fett" }, { "poultry", "fleisch" },
        { "sausage", "fleisch" }, { "pork", "fleisch" }, { "beef", "fleisch" }, { "lamb", "fleisch" }, { "meat", "fleisch" },
        { "fleisch", "fleisch" }, { "wurst", "fleisch" }, { "geflügel", "fleisch" }, { "fish", "fisch" }, { "shellfish", "fisch" },
        { "fisch", "fisch" }, { "seafood", "fisch" }, { "vegetable", "gemuese" }, { "gemüse", "gemuese" }, { "fruit", "obst" },
        { "obst", "obst" }, { "frucht", "obst" }, { "nut", "nuss" }, { "seed", "nuss" }, { "nüsse", "nuss" }, { "nuss", "nuss" },
        { "legume", "huelsen" }, { "hülsen", "huelsen" }, { "bean", "huelsen" }, { "cereal", "getreide" }, { "grain", "getreide" },
        { "pasta", "getreide" }, { "getreide", "getreide" }, { "nudel", "getreide" }, { "baked", "backwaren" }, { "brot", "backwaren" },
        { "bread", "backwaren" }, { "backwaren", "backwaren" }, { "sweet", "suess" }, { "süß", "suess" }, { "snack", "suess" },
        { "beverage", "getraenk" }, { "getränk", "getraenk" }, { "alcohol", "alkohol" }, { "alkohol", "alkohol" },
        { "soup", "sosse" }, { "sauce", "sosse" }, { "soße", "sosse" }, { "gravy", "sosse" }, { "potato", "kartoffel" },
        { "kartoffel", "kartoffel" }, { "sugar", "zucker" }, { "zucker", "zucker" }, { "honey", "honig" }, { "honig", "honig" },
    };
    for (const K &k : keys)
        if (t.contains(QString::fromUtf8(k.word))) return QString::fromUtf8(k.cat);
    return fallback;
}

QList<Food> convert(const Table &t, const Mapping &m, const Options &opt, QStringList *warnings)
{
    QList<Food> out;
    QSet<QString> keys;
    int skipped = 0;
    auto cell = [&](const QStringList &row, Field f) { return m.col[f] >= 0 && m.col[f] < row.size() ? row[m.col[f]] : QString(); };
    auto num = [&](const QStringList &row, Field f) { return number(cell(row, f)); };
    const QString filter = opt.filter.trimmed().toLower();
    FoodTable &table = FoodTable::instance();
    for (const QStringList &row : t.rows) {
        const QString name = cell(row, Name).simplified();
        if (name.isEmpty()) {
            ++skipped;
            continue;
        }
        if (!filter.isEmpty() && !name.toLower().contains(filter)) continue;
        Food f;
        f.name = name;
        f.source = opt.source;
        bool hasKcal = false, hasKj = false;
        const double kcal = m.col[Kcal] >= 0 ? number(cell(row, Kcal), &hasKcal) : 0;
        const double kj = m.col[KJ] >= 0 ? number(cell(row, KJ), &hasKj) : 0;
        f.per100.kcal = hasKcal && kcal > 0 ? kcal : (hasKj ? kj / 4.184 : 0);
        if (!hasKcal && !hasKj) {   // ohne Energiewert ist der Eintrag für VitaMeals wertlos
            ++skipped;
            continue;
        }
        f.per100.protein = num(row, Protein);
        f.per100.fat = num(row, Fat);
        f.per100.satFat = num(row, SatFat);
        f.per100.carbs = num(row, Carbs);
        f.per100.sugar = num(row, Sugar);
        f.per100.fiber = num(row, Fiber);
        bool hasSalt = false;
        const double salt = m.col[Salt] >= 0 ? number(cell(row, Salt), &hasSalt) : 0;
        if (hasSalt) f.per100.salt = salt;
        else if (m.col[Sodium] >= 0) f.per100.salt = num(row, Sodium) * (m.sodiumMg ? 0.001 : 1.0) * 2.5;
        f.density = m.col[Density] >= 0 && num(row, Density) > 0 ? num(row, Density) : 1.0;
        f.piece = m.col[Piece] >= 0 ? num(row, Piece) : 0;
        f.aliases << name.toLower();
        for (const QString &a : cell(row, Aliases).split(QRegularExpression(QStringLiteral("[,|]")), Qt::SkipEmptyParts))
            if (!a.trimmed().isEmpty()) f.aliases << a.trimmed().toLower();
        // Kategorie: Spalte, sonst ähnliches eingebautes Lebensmittel, sonst Vorgabe
        const Food *similar = table.match(name);
        const QString catText = cell(row, Category);
        f.category = !catText.isEmpty() ? guessCategory(catText, similar ? similar->category : opt.defaultCategory)
                                        : (similar ? similar->category : opt.defaultCategory);
        f.key = keyFrom(QStringLiteral("csv_"), name);
        // eingebauten Eintrag gleichen Namens ersetzen (Namen, Dichte und Stückgewichte bleiben)
        if (opt.overrideBuiltin) {
            for (const Food *b : table.all()) {
                if (!table.isBuiltin(b->key)) continue;
                if (FoodTable::normalize(b->name) == FoodTable::normalize(name) || b->aliases.contains(name.toLower())) {
                    f.key = b->key;
                    for (const QString &a : b->aliases)
                        if (!f.aliases.contains(a)) f.aliases << a;
                    if (m.col[Density] < 0) f.density = b->density;
                    if (f.piece <= 0) f.piece = b->piece;
                    f.unitGrams = b->unitGrams;
                    if (catText.isEmpty()) f.category = b->category;
                    break;
                }
            }
        }
        if (keys.contains(f.key)) {   // gleicher Name mehrfach: erster Eintrag gilt
            ++skipped;
            continue;
        }
        keys.insert(f.key);
        out << f;
    }
    if (warnings && skipped > 0) *warnings << tr("%1 Zeilen ohne Name, ohne Energiewert oder doppelt wurden übersprungen.").arg(skipped);
    return out;
}

// ---------------------------------------------------------------- USDA FoodData Central

bool isUsdaFolder(const QString &folder)
{
    const QDir d(folder);
    return QFileInfo::exists(d.filePath(QStringLiteral("food.csv"))) && QFileInfo::exists(d.filePath(QStringLiteral("nutrient.csv")))
           && QFileInfo::exists(d.filePath(QStringLiteral("food_nutrient.csv")));
}

bool readUsda(const QString &folder, const Options &opt, QList<Food> *out, QString *error, const std::function<bool(int)> &progress)
{
    out->clear();
    if (!isUsdaFolder(folder)) {
        if (error) *error = tr("Der Ordner enthält keine FoodData-Central-Dateien (food.csv, nutrient.csv, food_nutrient.csv).");
        return false;
    }
    const QDir d(folder);
    auto colIndex = [](const QStringList &h, const char *name) { return int(h.indexOf(QString::fromLatin1(name))); };

    // 1. Nährstoffe: ID → (Feld, Rang, Faktor)
    struct Target { Field field; int rank; double factor; };
    QHash<QString, Target> nutrients;
    {
        QStringList h;
        bool first = true;
        QChar delim(QLatin1Char(','));
        if (!forEachRecord(d.filePath(QStringLiteral("nutrient.csv")), &delim, nullptr, error, [&](const QStringList &r) {
                if (first) {
                    h = r;
                    first = false;
                    return true;
                }
                const int iId = colIndex(h, "id"), iName = colIndex(h, "name"), iUnit = colIndex(h, "unit_name");
                if (iId < 0 || iName < 0 || iUnit < 0 || r.size() <= qMax(iId, qMax(iName, iUnit))) return true;
                const QString n = r[iName].toLower(), u = r[iUnit].toUpper();
                Target t{ FieldCount, 9, 1.0 };
                if (n == QLatin1String("energy") && u == QLatin1String("KCAL")) t = { Kcal, 0, 1.0 };
                else if (n == QLatin1String("energy (atwater specific factors)") && u == QLatin1String("KCAL")) t = { Kcal, 1, 1.0 };
                else if (n == QLatin1String("energy (atwater general factors)") && u == QLatin1String("KCAL")) t = { Kcal, 2, 1.0 };
                else if (n == QLatin1String("energy") && u == QLatin1String("KJ")) t = { Kcal, 3, 1.0 / 4.184 };
                else if (n == QLatin1String("protein")) t = { Protein, 0, 1.0 };
                else if (n == QLatin1String("total lipid (fat)")) t = { Fat, 0, 1.0 };
                else if (n == QLatin1String("total fat (nlea)")) t = { Fat, 1, 1.0 };
                else if (n == QLatin1String("fatty acids, total saturated")) t = { SatFat, 0, 1.0 };
                else if (n == QLatin1String("carbohydrate, by difference")) t = { Carbs, 0, 1.0 };
                else if (n == QLatin1String("carbohydrate, by summation")) t = { Carbs, 1, 1.0 };
                else if (n == QLatin1String("sugars, total including nlea")) t = { Sugar, 0, 1.0 };
                else if (n == QLatin1String("sugars, total") || n == QLatin1String("total sugars")) t = { Sugar, 1, 1.0 };
                else if (n == QLatin1String("fiber, total dietary")) t = { Fiber, 0, 1.0 };
                else if (n == QLatin1String("sodium, na")) t = { Sodium, 0, u == QLatin1String("MG") ? 0.001 : 1.0 };
                if (t.field != FieldCount) nutrients.insert(r[iId], t);
                return true;
            }))
            return false;
    }
    // 2. Kategorien
    QHash<QString, QString> categories;
    if (QFileInfo::exists(d.filePath(QStringLiteral("food_category.csv")))) {
        QStringList h;
        bool first = true;
        QChar delim(QLatin1Char(','));
        forEachRecord(d.filePath(QStringLiteral("food_category.csv")), &delim, nullptr, nullptr, [&](const QStringList &r) {
            if (first) {
                h = r;
                first = false;
                return true;
            }
            const int iId = colIndex(h, "id"), iDesc = colIndex(h, "description");
            if (iId >= 0 && iDesc >= 0 && r.size() > qMax(iId, iDesc)) categories.insert(r[iId], r[iDesc]);
            return true;
        });
    }
    // 3. Lebensmittel (nur Grunddaten: Foundation, SR Legacy, FNDDS – keine Einzelproben)
    struct Item { QString name; QString category; double v[FieldCount]; int rank[FieldCount]; };
    QHash<QString, Item> foods;
    const QString filter = opt.filter.trimmed().toLower();
    {
        QStringList h;
        bool first = true;
        QChar delim(QLatin1Char(','));
        if (!forEachRecord(d.filePath(QStringLiteral("food.csv")), &delim, nullptr, error, [&](const QStringList &r) {
                if (first) {
                    h = r;
                    first = false;
                    return true;
                }
                const int iId = colIndex(h, "fdc_id"), iType = colIndex(h, "data_type"), iDesc = colIndex(h, "description"),
                          iCat = colIndex(h, "food_category_id");
                if (iId < 0 || iDesc < 0 || r.size() <= qMax(iId, iDesc)) return true;
                if (iType >= 0 && iType < r.size()) {
                    const QString type = r[iType];
                    if (type != QLatin1String("foundation_food") && type != QLatin1String("sr_legacy_food")
                        && type != QLatin1String("survey_fndds_food") && type != QLatin1String("branded_food"))
                        return true;
                }
                if (!filter.isEmpty() && !r[iDesc].toLower().contains(filter)) return true;
                Item it;
                it.name = r[iDesc].simplified();
                it.category = iCat >= 0 && iCat < r.size() ? categories.value(r[iCat]) : QString();
                for (int k = 0; k < FieldCount; ++k) {
                    it.v[k] = -1;
                    it.rank[k] = 99;
                }
                foods.insert(r[iId], it);
                return true;
            }))
            return false;
    }
    // 4. Nährwerte (große Datei – zeilenweise)
    {
        QStringList h;
        bool first = true;
        int n = 0;
        int iFdc = -1, iNut = -1, iAmt = -1;
        QChar delim(QLatin1Char(','));
        bool cancelled = false;
        if (!forEachRecord(d.filePath(QStringLiteral("food_nutrient.csv")), &delim, nullptr, error, [&](const QStringList &r) {
                if (first) {
                    h = r;
                    iFdc = colIndex(h, "fdc_id");
                    iNut = colIndex(h, "nutrient_id");
                    iAmt = colIndex(h, "amount");
                    first = false;
                    return iFdc >= 0 && iNut >= 0 && iAmt >= 0;
                }
                if (++n % 20000 == 0 && progress && !progress(n)) {
                    cancelled = true;
                    return false;
                }
                if (r.size() <= qMax(iFdc, qMax(iNut, iAmt))) return true;
                auto nit = nutrients.constFind(r[iNut]);
                if (nit == nutrients.constEnd()) return true;
                auto fit = foods.find(r[iFdc]);
                if (fit == foods.end()) return true;
                bool ok = false;
                const double v = r[iAmt].toDouble(&ok);
                if (!ok) return true;
                if (nit->rank < fit->rank[nit->field]) {
                    fit->rank[nit->field] = nit->rank;
                    fit->v[nit->field] = v * nit->factor;
                }
                return true;
            }))
            return false;
        if (cancelled) {
            if (error) *error = tr("Abgebrochen.");
            return false;
        }
    }
    const QString source = opt.source.isEmpty() ? QStringLiteral("USDA FoodData Central") : opt.source;
    for (auto it = foods.constBegin(); it != foods.constEnd(); ++it) {
        const Item &i = it.value();
        if (i.v[Kcal] < 0) continue;   // ohne Energiewert
        Food f;
        f.key = QStringLiteral("usda_") + it.key();
        f.name = i.name;
        f.source = source;
        auto val = [&](Field fl) { return i.v[fl] > 0 ? i.v[fl] : 0.0; };
        f.per100.kcal = val(Kcal);
        f.per100.protein = val(Protein);
        f.per100.fat = val(Fat);
        f.per100.satFat = val(SatFat);
        f.per100.carbs = val(Carbs);
        f.per100.sugar = val(Sugar);
        f.per100.fiber = val(Fiber);
        f.per100.salt = val(Sodium) * 2.5;
        f.category = guessCategory(i.category.isEmpty() ? i.name : i.category, opt.defaultCategory);
        if (f.category == QLatin1String("milch") && i.name.toLower().startsWith(QLatin1String("egg"))) f.category = QStringLiteral("ei");
        f.aliases << i.name.toLower();
        out->append(f);
    }
    std::sort(out->begin(), out->end(), [](const Food &a, const Food &b) { return QString::localeAwareCompare(a.name, b.name) < 0; });
    return true;
}

// ---------------------------------------------------------------- Export

namespace {
QString csvField(const QString &s)
{
    if (s.contains(QLatin1Char(';')) || s.contains(QLatin1Char('"')) || s.contains(QLatin1Char('\n')))
        return QLatin1Char('"') + QString(s).replace(QLatin1Char('"'), QStringLiteral("\"\"")) + QLatin1Char('"');
    return s;
}
QString de(double v, int dec) { return QLocale(QLocale::German).toString(v, 'f', dec); }
const QStringList &exportHeader()
{
    static const QStringList h{ QStringLiteral("Name"), QStringLiteral("Kategorie"), QStringLiteral("Energie (kcal)"),
                                QStringLiteral("Eiweiß (g)"), QStringLiteral("Fett (g)"), QStringLiteral("davon gesättigte Fettsäuren (g)"),
                                QStringLiteral("Kohlenhydrate (g)"), QStringLiteral("davon Zucker (g)"), QStringLiteral("Ballaststoffe (g)"),
                                QStringLiteral("Salz (g)"), QStringLiteral("Dichte (g/ml)"), QStringLiteral("Stückgewicht (g)"),
                                QStringLiteral("Andere Namen") };
    return h;
}
bool writeCsv(const QString &path, const QStringList &lines, QString *error)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write("\xEF\xBB\xBF");   // BOM: Excel erkennt UTF-8
    f.write(lines.join(QStringLiteral("\r\n")).toUtf8());
    f.write("\r\n");
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}
QString foodLine(const Food &f)
{
    QStringList c;
    QStringList aliases = f.aliases;
    aliases.removeAll(f.name.toLower());
    c << csvField(f.name) << csvField(FoodTable::categoryName(f.category)) << de(f.per100.kcal, 0) << de(f.per100.protein, 1)
      << de(f.per100.fat, 1) << de(f.per100.satFat, 1) << de(f.per100.carbs, 1) << de(f.per100.sugar, 1) << de(f.per100.fiber, 1)
      << de(f.per100.salt, 2) << de(f.density, 2) << (f.piece > 0 ? de(f.piece, 0) : QString()) << csvField(aliases.join(QStringLiteral(", ")));
    return c.join(QLatin1Char(';'));
}
} // namespace

bool exportUserFoods(const QString &path, QString *error)
{
    QStringList lines{ exportHeader().join(QLatin1Char(';')) };
    for (const Food *f : FoodTable::instance().userFoods()) lines << foodLine(*f);
    return writeCsv(path, lines, error);
}

bool writeTemplate(const QString &path, QString *error)
{
    Food a;
    a.name = QStringLiteral("Dinkelvollkornmehl");
    a.category = QStringLiteral("getreide");
    a.per100 = Nutrients{ 335, 14, 2.5, 0.4, 61, 1, 10, 0 };
    a.density = 0.55;
    a.aliases = { QStringLiteral("dinkelvollkornmehl"), QStringLiteral("dinkel-vollkornmehl"), QStringLiteral("spelt flour") };
    Food b;
    b.name = QStringLiteral("Hafer-Joghurt natur");
    b.category = QStringLiteral("huelsen");
    b.per100 = Nutrients{ 62, 1.2, 2.8, 0.3, 7.6, 2.4, 1.2, 0.1 };
    b.density = 1.03;
    b.aliases = { QStringLiteral("haferjoghurt"), QStringLiteral("hafer-joghurt") };
    return writeCsv(path, { exportHeader().join(QLatin1Char(';')), foodLine(a), foodLine(b) }, error);
}

} // namespace FoodCsv
