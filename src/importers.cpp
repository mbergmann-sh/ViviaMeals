// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "importers.h"
#include "glossary.h"
#include "ingredientparser.h"
#include "nutrition.h"
#include "textcodec.h"
#include "umlauts.h"
#include "mealtypes.h"
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
    struct ColumnIngredient {        // MealMaster-Spalten (Zerlegung erst, wenn die Sprache feststeht)
        bool heading = false;
        QString amount, unit, name, original;
        QStringList more;            // Fortsetzungszeilen „-sifted“
    };
    QList<ColumnIngredient> columns;
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
                                       QStringLiteral("pers."), QStringLiteral("port."), QStringLiteral("persone"),
                                       QStringLiteral("pers"), QStringLiteral("portion(en)"), QStringLiteral("person(en)"),
                                       QStringLiteral("portionen à"), QStringLiteral("people"), QStringLiteral("") };
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

// winREZKONV kürzt lange Titel („… Maispla ...“) und legt den vollen Titel als
// „: O-Titel : …“ mit Fortsetzungen „: > …“ in die Anleitung
void extractOriginalTitle(RawRecipe &raw)
{
    static const QRegularExpression head(QStringLiteral("^\\s*:\\s*O-Titel\\s*:\\s*(.*)$"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression cont(QStringLiteral("^\\s*:\\s*>\\s*(.*)$"));
    for (int i = 0; i < raw.directionLines.size(); ++i) {
        const auto m = head.match(raw.directionLines[i]);
        if (!m.hasMatch()) continue;
        QString full = m.captured(1).trimmed();
        int j = i + 1;
        for (; j < raw.directionLines.size(); ++j) {
            const auto c = cont.match(raw.directionLines[j]);
            if (!c.hasMatch()) break;
            const QString add = c.captured(1).trimmed();
            full = full.endsWith(QLatin1Char('-')) ? full + add : full + QLatin1Char(' ') + add;
        }
        raw.directionLines.erase(raw.directionLines.begin() + i, raw.directionLines.begin() + j);
        full = full.simplified();
        // Zeilenumbruch nach Bindestrich: „Blumenkohl-Broccoli- Gemüse“ → „Blumenkohl-Broccoli-Gemüse“
        static const QRegularExpression brokenHyphen(QStringLiteral("(\\p{L})- (\\p{Lu})"));
        full.replace(brokenHyphen, QStringLiteral("\\1-\\2"));
        if (full.isEmpty()) return;
        QString shortTitle = raw.title.trimmed();
        const bool cut = shortTitle.endsWith(QLatin1String("...")) || shortTitle.endsWith(QChar(0x2026));
        while (shortTitle.endsWith(QLatin1Char('.')) || shortTitle.endsWith(QChar(0x2026))) shortTitle.chop(1);
        shortTitle = shortTitle.trimmed();
        if (cut || shortTitle.isEmpty() || full.startsWith(shortTitle, Qt::CaseInsensitive)) raw.title = full;
        else raw.sourceLines << QObject::tr("Originaltitel: %1").arg(full);
        // Leerzeile am Ende entfernen
        while (!raw.directionLines.isEmpty() && raw.directionLines.last().trimmed().isEmpty()) raw.directionLines.removeLast();
        return;
    }
}

QString rawText(const RawRecipe &raw)
{
    QStringList parts{ raw.title, raw.yield, raw.categories.join(QLatin1Char(' ')), raw.directionLines.join(QLatin1Char(' ')),
                       raw.ingredientLines.join(QLatin1Char(' ')) };
    for (const auto &c : raw.columns) parts << c.name << c.more.join(QLatin1Char(' '));
    return parts.join(QLatin1Char(' '));
}

Recipe build(const RawRecipe &raw, const QString &format, const QString &lang, bool glossary)
{
    const bool german = lang == QLatin1String("de");
    Recipe r;
    r.id = Recipe::newId();
    r.title = raw.title.simplified();
    if (r.title.isEmpty()) r.title = QObject::tr("Unbenanntes Rezept");
    r.sourceFormat = format;
    parseYield(raw.yield, r);
    r.language = lang;
    IngredientParser::Options po;
    po.translate = !german && glossary;   // Glossar kennt nur Englisch, schadet bei anderen Sprachen aber nicht
    po.mealMasterUnits = format == QLatin1String("MealMaster");
    // Zutaten: Freitextzeilen zerlegen; Spaltenzutaten liegen schon vor
    if (!raw.ingredientLines.isEmpty()) {
        r.ingredients = IngredientParser::parseText(raw.ingredientLines.join(QLatin1Char('\n')), po);
    } else {
        for (const auto &c : raw.columns) {
            Ingredient i;
            if (c.heading) {
                i.heading = true;
                i.name = po.translate ? Glossary::ingredient(c.name.toLower()) : c.name;
            } else {
                i = IngredientParser::fromParts(c.amount, c.unit, c.name, po);
                for (QString add : c.more) {
                    if (po.translate) add = Glossary::note(add);
                    i.note = i.note.isEmpty() ? add : i.note + QStringLiteral(", ") + add;
                }
            }
            i.original = c.original;
            r.ingredients << i;
        }
    }
    QString dir = joinDirections(raw.directionLines);
    if (!german) dir = Units::convertTextMeasures(dir);
    // RezKonv-Schreibweise „bei 180GradC“ → „bei 180 °C“
    static const QRegularExpression gradC(QStringLiteral("(\\d)\\s*Grad\\s*C\\b"));
    dir.replace(gradC, QStringLiteral("\\1 °C"));
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
        r.addTag(german ? Glossary::germanCategory(c) : Glossary::category(c));
    }
    if (!german && glossary) {
        r.originalLanguage = lang;
        if (lang == QLatin1String("en")) r.addTag(QStringLiteral("englisch"));
    }
    return r;
}

Recipe finish(RawRecipe &raw, const Options &opt, const QString &format, Result *res)
{
    extractOriginalTitle(raw);
    // Sprache aus Titel, Anleitung und Zutaten schätzen
    const QString probe = rawText(raw);
    QString lang = Glossary::detectLanguage(probe);
    if (lang == QLatin1String("de") && Glossary::looksEnglish(probe)) lang = QStringLiteral("en");
    const bool german = lang == QLatin1String("de");
    bool umlauts = false;
    if (german && opt.fixUmlauts && Umlauts::looksTranscribed(probe)) {
        // vor dem Zerlegen, damit „Stueck“, „Essloeffel“ als Einheit erkannt werden
        umlauts = true;
        raw.title = Umlauts::fixText(raw.title);
        raw.yield = Umlauts::fixText(raw.yield);
        for (QString &c : raw.categories) c = Umlauts::fixText(c);
        for (QString &l : raw.ingredientLines) l = Umlauts::fixText(l);
        for (auto &c : raw.columns) c.name = Umlauts::fixText(c.name);
    }
    Recipe r;
    if (!german && opt.translator) {
        r = build(raw, format, lang, false);
        QString err;
        if (opt.translator(r, &err)) {
            ++res->translated;
            r.language = QStringLiteral("de");
            if (r.originalLanguage.isEmpty()) r.originalLanguage = lang;
        } else {
            res->warnings << QObject::tr("„%1“: Übersetzung nicht möglich (%2) – Zutaten über das Glossar übersetzt.").arg(r.title, err);
            r = build(raw, format, lang, true);
        }
    } else {
        r = build(raw, format, lang, opt.translate);
        if (!german && !opt.translate) r.originalLanguage.clear();
    }
    if (umlauts) {
        Umlauts::fixRecipe(r);
        ++res->umlautsFixed;
    }
    Nutrition::analyze(r);
    if (opt.autoTags) Nutrition::applyTagSuggestions(r);
    if (opt.mealTags && !Nutrition::isMenu(r)) MealTypes::applyTags(r);
    if (opt.nutrientTags) MealTypes::applyNutrientTags(r);
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

RawRecipe::ColumnIngredient mmIngredient(const QString &part)
{
    RawRecipe::ColumnIngredient c;
    c.amount = part.left(7).trimmed();
    c.unit = part.mid(8, 2).trimmed();
    c.name = part.mid(11).trimmed();
    c.original = part.trimmed();
    return c;
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
    QList<RawRecipe::ColumnIngredient> right;   // rechte Spalte eines zweispaltigen Blocks (wird am Blockende angehängt)
    auto flushRight = [&] {
        raw.columns << right;
        right.clear();
    };
    auto finishRecipe = [&] {
        flushRight();
        if (!raw.title.isEmpty() || !raw.columns.isEmpty()) {
            res.recipes << finish(raw, opt, QStringLiteral("MealMaster"), &res);
        }
        raw = RawRecipe();
    };
    for (int n = 0; n < lines.size(); ++n) {
        const QString line = lines[n];
        const QString t = line.trimmed();
        if (start.match(t).hasMatch()) {
            if (st != Outside) finishRecipe();
            st = Header;
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
                RawRecipe::ColumnIngredient h;
                h.heading = true;
                h.name = d.captured(1).trimmed();
                h.original = h.name;
                raw.columns << h;
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
                    raw.columns << mmIngredient(line.left(40));
                    right << mmIngredient(line.mid(41));
                } else {
                    RawRecipe::ColumnIngredient i = mmIngredient(line);
                    // Fortsetzungszeile „          -sifted“
                    if (i.amount.isEmpty() && i.unit.isEmpty() && i.name.startsWith(QLatin1Char('-'))
                        && !raw.columns.isEmpty() && !raw.columns.last().heading) {
                        auto &prev = raw.columns.last();
                        prev.more << i.name.mid(1).trimmed();
                        prev.original += QLatin1Char(' ') + i.name;
                        continue;
                    }
                    raw.columns << i;
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
        if (!raw.title.isEmpty() || !raw.ingredientLines.isEmpty()) res.recipes << finish(raw, opt, QStringLiteral("RezKonv"), &res);
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
            // winREZKONV: Fortsetzungszeile „                   -Würfeln“ gehört zur vorigen Zutat
            if (sawIngredient && t.startsWith(QLatin1Char('-')) && !t.startsWith(QLatin1String("--")) && lead >= 6
                && !raw.ingredientLines.isEmpty() && !raw.ingredientLines.last().startsWith(QLatin1String("=="))) {
                QString &prev = raw.ingredientLines.last();
                const QString add = t.mid(1).trimmed();
                if (prev.endsWith(QLatin1Char('-'))) prev += add;   // „Tomaten-“ + „Portwein-Sauce“
                else prev += QLatin1Char(' ') + add;
                continue;
            }
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
