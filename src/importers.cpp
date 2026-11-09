// SPDX-License-Identifier: MIT
#include "importers.h"
#include "glossary.h"
#include "ingredientparser.h"
#include "nutrition.h"
#include "textcodec.h"
#include "units.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace Importers {

namespace {

// ---------------------------------------------------------------- gemeinsame Hilfen

struct RawRecipe {
    QString title;
    QStringList categories;
    QString yield;
    QStringList ingredientLines;   // bereits als „Menge|Einheit|Name“ (MealMaster-Spalten) oder Freitext
    QList<Ingredient> ingredients; // fertig zerlegt (aus Spalten)
    QStringList directionLines;
    QStringList sourceLines;
    QString format;
};

bool blank(const QString &s) { return s.trimmed().isEmpty(); }

int leadingSpaces(const QString &s)
{
    int n = 0;
    while (n < s.size() && s[n] == QLatin1Char(' ')) ++n;
    return n;
}

void parseYield(const QString &y, Recipe &r)
{
    static const QRegularExpression re(QStringLiteral("^\\s*(\\d+\\s+\\d+/\\d+|\\d+/\\d+|\\d+[.,]\\d+|\\d+\\s*[½¼¾]|\\d+|[½¼¾])(?:\\s*(?:-|bis|to)\\s*[\\d.,]+)?\\s*(.*)$"));
    const auto m = re.match(y);
    double n = 0;
    if (!m.hasMatch() || !IngredientParser::parseNumber(m.captured(1), &n) || n <= 0) {
        r.servings = 4;
        return;
    }
    r.servings = n;
    const QString unit = m.captured(2).trimmed().toLower();
    static const QStringList portions{ QStringLiteral("portion"), QStringLiteral("portionen"), QStringLiteral("person"),
                                       QStringLiteral("personen"), QStringLiteral("serving"), QStringLiteral("servings"),
                                       QStringLiteral("pers."), QStringLiteral("port."), QStringLiteral("") };
    if (portions.contains(unit)) return;
    static const QHash<QString, QString> en{
        { QStringLiteral("cookies"), QStringLiteral("Stück") }, { QStringLiteral("pieces"), QStringLiteral("Stück") },
        { QStringLiteral("dozen"), QStringLiteral("Dutzend") }, { QStringLiteral("loaf"), QStringLiteral("Laib") },
        { QStringLiteral("loaves"), QStringLiteral("Laibe") }, { QStringLiteral("cake"), QStringLiteral("Kuchen") },
        { QStringLiteral("pie"), QStringLiteral("Kuchen") }, { QStringLiteral("batch"), QStringLiteral("Rezept") },
        { QStringLiteral("cups"), QStringLiteral("Tassen") }, { QStringLiteral("quarts"), QStringLiteral("Liter (ca.)") },
        { QStringLiteral("muffins"), QStringLiteral("Muffins") }, { QStringLiteral("rezept"), QStringLiteral("Rezept") },
    };
    r.yieldUnit = en.value(unit, m.captured(2).trimmed());
    if (unit == QLatin1String("dozen")) {
        r.servings *= 12;
        r.yieldUnit = QStringLiteral("Stück");
    }
}

// Zeilenumbrüche der Textformate zu Absätzen zusammenfügen
QString joinDirections(const QStringList &lines)
{
    QStringList paragraphs;
    QString cur;
    static const QRegularExpression listStart(QStringLiteral("^\\s*(?:\\d+[.)]|[-*•])\\s+"));
    for (const QString &raw : lines) {
        const QString l = raw.trimmed();
        if (l.isEmpty()) {
            if (!cur.isEmpty()) paragraphs << cur;
            cur.clear();
            continue;
        }
        if (cur.isEmpty()) {
            cur = l;
        } else if (listStart.match(l).hasMatch() || cur.endsWith(QLatin1Char(':'))) {
            paragraphs << cur;
            cur = l;
        } else if (cur.endsWith(QLatin1Char('-')) && !cur.endsWith(QLatin1String(" -"))) {
            cur.chop(1);   // Silbentrennung
            cur += l;
        } else {
            cur += QLatin1Char(' ') + l;
        }
    }
    if (!cur.isEmpty()) paragraphs << cur;
    return paragraphs.join(QStringLiteral("\n\n"));
}

Recipe finish(RawRecipe &raw, const Options &opt, const QString &format)
{
    Recipe r;
    r.id = Recipe::newId();
    r.title = raw.title.simplified();
    if (r.title.isEmpty()) r.title = QObject::tr("Unbenanntes Rezept");
    r.sourceFormat = format;
    parseYield(raw.yield, r);
    // Sprache aus Anleitung und Zutaten schätzen
    QString probe = raw.directionLines.join(QLatin1Char(' '));
    for (const Ingredient &i : raw.ingredients) probe += QLatin1Char(' ') + i.name;
    for (const QString &l : raw.ingredientLines) probe += QLatin1Char(' ') + l;
    const bool english = Glossary::looksEnglish(probe);
    r.language = english ? QStringLiteral("en") : QStringLiteral("de");
    IngredientParser::Options po;
    po.translate = english && opt.translate;
    po.mealMasterUnits = format == QLatin1String("MealMaster");
    // Zutaten: Freitextzeilen zerlegen; Spaltenzutaten liegen schon vor (englisch → übersetzen)
    QList<Ingredient> list;
    if (!raw.ingredientLines.isEmpty()) list = IngredientParser::parseText(raw.ingredientLines.join(QLatin1Char('\n')), po);
    else list = raw.ingredients;
    r.ingredients = list;
    QString dir = joinDirections(raw.directionLines);
    if (english) dir = Units::convertTextMeasures(dir);
    r.instructions = dir;
    QStringList src;
    for (QString s : raw.sourceLines) {
        s = s.trimmed();
        while (s.startsWith(QLatin1Char('-'))) s = s.mid(1).trimmed();
        if (!s.isEmpty()) src << s;
    }
    r.source = src.join(QLatin1Char(' ')).simplified();
    for (const QString &c : raw.categories) {
        if (c.trimmed().isEmpty() || c.trimmed().compare(QLatin1String("None"), Qt::CaseInsensitive) == 0) continue;
        r.addTag(english ? Glossary::category(c) : Glossary::germanCategory(c));
    }
    if (english) r.addTag(QStringLiteral("englisch"));
    Nutrition::analyze(r);
    if (opt.autoTags) Nutrition::applyTagSuggestions(r);
    return r;
}

// ---------------------------------------------------------------- MealMaster

bool mmLayout(const QString &line)
{
    // Spalten: Menge 0–6, Einheit 8–9, Zutat ab 11
    if (line.size() < 12) return false;
    for (int i = 0; i < 7; ++i) {
        const QChar c = line[i];
        if (!(c.isDigit() || c == QLatin1Char(' ') || c == QLatin1Char('/') || c == QLatin1Char('.') || c == QLatin1Char('-')))
            return false;
    }
    if (line[7] != QLatin1Char(' ') || line[10] != QLatin1Char(' ')) return false;
    for (int i = 8; i < 10; ++i)
        if (!(line[i].isLetter() || line[i] == QLatin1Char(' '))) return false;
    return !line[11].isSpace();
}

Ingredient mmIngredient(const QString &part, const IngredientParser::Options &po)
{
    const QString amount = part.left(7).trimmed();
    const QString unit = part.mid(8, 2).trimmed();
    const QString name = part.mid(11).trimmed();
    Ingredient i = IngredientParser::fromParts(amount, unit, name, po);
    i.original = part.trimmed();
    return i;
}

} // namespace

Format detect(const QString &text)
{
    const QString head = text.left(4000);
    if (head.contains(QRegularExpression(QStringLiteral("^[=M]{5}.*REZKONV"), QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption)))
        return Format::RezKonv;
    if (head.contains(QRegularExpression(QStringLiteral("^(MMMMM|-----).*Meal-?Master"), QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption)))
        return Format::MealMaster;
    if (head.trimmed().startsWith(QLatin1Char('['))) return Format::Vtm;
    if (head.contains(QStringLiteral("Titel:"))) return Format::RezKonv;
    if (head.contains(QStringLiteral("Title:"))) return Format::MealMaster;
    return Format::Unknown;
}

QString formatName(Format f)
{
    switch (f) {
    case Format::RezKonv: return QStringLiteral("RezKonv");
    case Format::MealMaster: return QStringLiteral("MealMaster");
    case Format::Vtm: return QStringLiteral("VitaMeals-Liste (.vtm)");
    case Format::Unknown: break;
    }
    return QObject::tr("unbekannt");
}

Result parseMealMaster(const QString &text, const Options &opt)
{
    Result res;
    const QStringList lines = text.split(QLatin1Char('\n'));
    static const QRegularExpression start(QStringLiteral("^(MMMMM|-----).*Meal-?Master"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression divider(QStringLiteral("^(?:MMMMM|-----)-*\\s*([^-].*?)\\s*-*$"));
    static const QRegularExpression field(QStringLiteral("^\\s*(Title|Categories|Yield|Servings)\\s*:\\s*(.*)$"), QRegularExpression::CaseInsensitiveOption);
    enum State { Outside, Header, Ingredients, Directions };
    State st = Outside;
    RawRecipe raw;
    QList<Ingredient> right;      // rechte Spalte eines zweispaltigen Blocks (wird am Blockende angehängt)
    IngredientParser::Options po;   // translate wird je Rezeptblock gesetzt
    po.mealMasterUnits = true;
    auto flushRight = [&] {
        raw.ingredients << right;
        right.clear();
    };
    auto finishRecipe = [&] {
        flushRight();
        if (!raw.title.isEmpty() || !raw.ingredients.isEmpty()) {
            res.recipes << finish(raw, opt, QStringLiteral("MealMaster"));
        }
        raw = RawRecipe();
    };
    for (int n = 0; n < lines.size(); ++n) {
        const QString line = lines[n];
        const QString t = line.trimmed();
        if (start.match(t).hasMatch()) {
            if (st != Outside) finishRecipe();
            st = Header;
            // Sprache des ganzen Rezeptblocks vorab bestimmen (für Übersetzung der Spaltenzutaten)
            QString block;
            for (int k = n + 1; k < lines.size(); ++k) {
                const QString bt = lines[k].trimmed();
                if (bt == QLatin1String("MMMMM") || bt == QLatin1String("-----") || start.match(bt).hasMatch()) break;
                block += lines[k] + QLatin1Char(' ');
            }
            po.translate = opt.translate && Glossary::looksEnglish(block);
            continue;
        }
        if (st == Outside) continue;
        if (t == QLatin1String("MMMMM") || t == QLatin1String("-----")) {
            finishRecipe();
            st = Outside;
            continue;
        }
        if (st == Header) {
            const auto m = field.match(line);
            if (m.hasMatch()) {
                const QString k = m.captured(1).toLower();
                if (k == QLatin1String("title")) raw.title = m.captured(2).trimmed();
                else if (k == QLatin1String("categories"))
                    for (const QString &c : m.captured(2).split(QLatin1Char(','), Qt::SkipEmptyParts)) raw.categories << c.trimmed();
                else raw.yield = m.captured(2).trimmed();
                continue;
            }
            if (blank(line)) continue;
            st = Ingredients;   // erste Zeile nach dem Kopf
        }
        // Abschnitts-Trenner „MMMMM-----SAUCE-----“
        if (t.startsWith(QLatin1String("MMMMM")) || (t.startsWith(QLatin1String("-----")) && t.size() > 10)) {
            const auto d = divider.match(t);
            flushRight();
            if (d.hasMatch() && !d.captured(1).trimmed().isEmpty()) {
                Ingredient h;
                h.heading = true;
                h.name = d.captured(1).trimmed();
                h.original = h.name;
                if (po.translate) h.name = Glossary::ingredient(h.name.toLower());
                raw.ingredients << h;
            }
            st = Ingredients;
            continue;
        }
        if (st == Ingredients) {
            if (blank(line)) {
                flushRight();
                continue;
            }
            if (mmLayout(line)) {
                if (line.size() > 52 && mmLayout(line.mid(41))) {
                    raw.ingredients << mmIngredient(line.left(40), po);
                    right << mmIngredient(line.mid(41), po);
                } else {
                    Ingredient i = mmIngredient(line, po);
                    // Fortsetzungszeile „          -sifted“
                    if (i.amount <= 0 && i.unit.isEmpty() && line.mid(11).trimmed().startsWith(QLatin1Char('-'))
                        && !raw.ingredients.isEmpty() && !raw.ingredients.last().heading) {
                        Ingredient &prev = raw.ingredients.last();
                        QString add = line.mid(11).trimmed().mid(1).trimmed();
                        if (po.translate) add = Glossary::note(add);
                        prev.note = prev.note.isEmpty() ? add : prev.note + QStringLiteral(", ") + add;
                        prev.original += QLatin1Char(' ') + line.mid(11).trimmed();
                        continue;
                    }
                    raw.ingredients << i;
                }
                continue;
            }
            st = Directions;
        }
        if (st == Directions) raw.directionLines << line;
    }
    if (st != Outside) {
        res.warnings << QObject::tr("Letztes Rezept ohne Endmarke (MMMMM) – trotzdem übernommen.");
        finishRecipe();
    }
    return res;
}

Result parseRezKonv(const QString &text, const Options &opt)
{
    Result res;
    const QStringList lines = text.split(QLatin1Char('\n'));
    static const QRegularExpression start(QStringLiteral("^[=M]{5}.*REZKONV"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression section(QStringLiteral("^={3,}\\s*(.*?)\\s*={3,}$"));
    static const QRegularExpression field(QStringLiteral("^\\s*(Titel|Kategorien|Menge|Rezeptname|Rezept Name|Portionen)\\s*:\\s*(.*)$"),
                                          QRegularExpression::CaseInsensitiveOption);
    enum State { Outside, Header, Ingredients, Source, Directions };
    State st = Outside;
    RawRecipe raw;
    bool sawIngredient = false;
    auto finishRecipe = [&] {
        if (!raw.title.isEmpty() || !raw.ingredientLines.isEmpty()) res.recipes << finish(raw, opt, QStringLiteral("RezKonv"));
        raw = RawRecipe();
        sawIngredient = false;
    };
    for (const QString &line : lines) {
        const QString t = line.trimmed();
        if (start.match(t).hasMatch()) {
            if (st != Outside) finishRecipe();
            st = Header;
            continue;
        }
        if (st == Outside) continue;
        if (t == QLatin1String("=====") || t == QLatin1String("MMMMM")) {
            finishRecipe();
            st = Outside;
            continue;
        }
        if (st == Header) {
            const auto m = field.match(line);
            if (m.hasMatch()) {
                const QString k = m.captured(1).toLower();
                if (k == QLatin1String("titel") || k.startsWith(QLatin1String("rezept"))) raw.title = m.captured(2).trimmed();
                else if (k == QLatin1String("kategorien"))
                    for (const QString &c : m.captured(2).split(QLatin1Char(','), Qt::SkipEmptyParts)) raw.categories << c.trimmed();
                else raw.yield = m.captured(2).trimmed();
                continue;
            }
            if (blank(line)) continue;
            st = Ingredients;
        }
        // Abschnitte „===== QUELLE =====“, „===== Teig =====“
        if (const auto s = section.match(t); s.hasMatch()) {
            const QString name = s.captured(1).trimmed();
            if (name.compare(QLatin1String("QUELLE"), Qt::CaseInsensitive) == 0) {
                st = Source;
            } else if (name.compare(QLatin1String("ZUBEREITUNG"), Qt::CaseInsensitive) == 0) {
                st = Directions;
            } else if (name.compare(QLatin1String("ZUTATEN"), Qt::CaseInsensitive) == 0) {
                st = Ingredients;
            } else if (!name.isEmpty()) {
                raw.ingredientLines << QStringLiteral("== %1 ==").arg(name);
                st = Ingredients;
            }
            continue;
        }
        if (st == Source) {
            if (blank(line)) {
                if (!raw.sourceLines.isEmpty()) st = sawIngredient ? Directions : Ingredients;
                continue;
            }
            raw.sourceLines << t;
            continue;
        }
        if (st == Ingredients) {
            if (blank(line)) continue;
            if (t.compare(QLatin1String("Zutaten:"), Qt::CaseInsensitive) == 0 || t.compare(QLatin1String("Zutaten"), Qt::CaseInsensitive) == 0)
                continue;
            if (t.compare(QLatin1String("Zubereitung:"), Qt::CaseInsensitive) == 0) {
                st = Directions;
                continue;
            }
            const int lead = leadingSpaces(line);
            double dummy = 0;
            const QString firstTok = t.section(QLatin1Char(' '), 0, 0);
            const bool startsWithAmount = IngredientParser::parseNumber(firstTok, &dummy)
                                          || firstTok.contains(QRegularExpression(QStringLiteral("^[\\d½¼¾]")));
            // Zutatenzeilen sind eingerückt (Mengenspalte rechtsbündig) bzw. beginnen mit einer Menge;
            // die Zubereitung steht linksbündig bzw. nur leicht eingerückt
            if ((lead >= 4 && (startsWithAmount || lead >= 6)) || (startsWithAmount && lead >= 1 && t.size() < 60)) {
                raw.ingredientLines << t;
                sawIngredient = true;
                continue;
            }
            if (t.startsWith(QLatin1Char('-')) && sawIngredient) {
                raw.ingredientLines << t;   // Fortsetzung
                continue;
            }
            if (!sawIngredient && lead >= 2) {
                raw.ingredientLines << t;
                sawIngredient = true;
                continue;
            }
            st = Directions;
        }
        if (st == Directions) raw.directionLines << line;
    }
    if (st != Outside) {
        res.warnings << QObject::tr("Letztes Rezept ohne Endmarke (=====) – trotzdem übernommen.");
        finishRecipe();
    }
    return res;
}

Result parseVtm(const QByteArray &json, const Options &opt)
{
    Result res;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray()) {
        res.warnings << QObject::tr("Die Datei ist keine VitaMeals-Liste (.vtm).");
        return res;
    }
    for (const auto &v : doc.array()) {
        const QJsonObject o = v.toObject();
        Recipe r;
        r.id = Recipe::newId();
        r.title = o.value(QStringLiteral("title")).toString();
        r.sourceFormat = o.value(QStringLiteral("format")).toString(QStringLiteral("VTM"));
        r.image = o.value(QStringLiteral("image")).toString();
        r.instructions = o.value(QStringLiteral("instructions")).toString().trimmed();
        for (const auto &t : o.value(QStringLiteral("tags")).toArray()) r.addTag(t.toString());
        const QString ing = o.value(QStringLiteral("ingredients")).toString();
        IngredientParser::Options po;
        po.translate = opt.translate && Glossary::looksEnglish(r.instructions + QLatin1Char(' ') + ing);
        r.language = po.translate ? QStringLiteral("en") : QStringLiteral("de");
        r.ingredients = IngredientParser::parseText(ing, po);
        if (po.translate) r.instructions = Units::convertTextMeasures(r.instructions);
        Nutrition::analyze(r);
        if (opt.autoTags) Nutrition::applyTagSuggestions(r);
        res.recipes << r;
    }
    return res;
}

Result importFile(const QString &path, Format format, const Options &opt)
{
    Result res;
    if (format == Format::Vtm || path.endsWith(QLatin1String(".vtm"), Qt::CaseInsensitive)) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            res.warnings << f.errorString();
            return res;
        }
        return parseVtm(f.readAll(), opt);
    }
    QString err, enc;
    const QString text = TextCodec::readFile(path, &err, &enc);
    if (!err.isEmpty()) {
        res.warnings << err;
        return res;
    }
    Format f = format == Format::Unknown ? detect(text) : format;
    // erzwungenes Format, aber Datei passt offensichtlich zum anderen
    const Format d = detect(text);
    if (d == Format::RezKonv || d == Format::MealMaster) f = d;
    if (f == Format::RezKonv) res = parseRezKonv(text, opt);
    else if (f == Format::MealMaster) res = parseMealMaster(text, opt);
    else res.warnings << QObject::tr("Das Format der Datei wurde nicht erkannt.");
    res.encoding = enc;
    return res;
}

} // namespace Importers
