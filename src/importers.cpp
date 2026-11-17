// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "importers.h"
#include "ambrosia.h"
#include "season.h"
#include "glossary.h"
#include "ingredientparser.h"
#include "nutrition.h"
#include "textcodec.h"
#include "umlauts.h"
#include "mealtypes.h"
#include "units.h"
#include "foodtable.h"
#include "markup.h"

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
    QString instructions;          // fertig ausgezeichnet (Datenbank-Importe) – hat Vorrang vor directionLines
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
    QString dir = raw.instructions.isEmpty() ? joinDirections(raw.directionLines) : raw.instructions;
    if (!german) dir = Units::convertTextMeasures(dir);
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

} // namespace

void normalizeTexts(Recipe &r)
{
    r.title = Units::normalizeCelsius(repairMojibake(r.title));
    r.instructions = Units::normalizeCelsius(repairMojibake(r.instructions));
    r.source = repairMojibake(r.source);
    for (QString &t : r.tags) t = repairMojibake(t);
    for (Ingredient &i : r.ingredients) {
        i.name = Units::normalizeCelsius(repairMojibake(i.name));
        i.note = Units::normalizeCelsius(repairMojibake(i.note));
    }
}

QString repairMojibake(const QString &text)
{
    if (!text.contains(QChar(0x00E1)) && !text.contains(QChar(0x0152))) return text;
    QString s = text;
    // CP850 „ß“ (0xE1) erscheint als „á“ – im Deutschen nur nach Vokal („Weiá“, „Nuá“, „süáe“); „Csángó“, „está“ bleiben
    static const QRegularExpression sz(QStringLiteral("(?<=[aeiouäöüAEIOUÄÖÜ])\\x{00E1}(?=[a-zäöü]|[^\\p{L}]|$)"));
    s.replace(sz, QStringLiteral("ß"));
    // CP850 „î“ (0x8C) erscheint als „Œ“: „fraŒche“
    static const QRegularExpression ic(QStringLiteral("(?<=[a-z])\\x{0152}(?=[a-z])"));
    s.replace(ic, QStringLiteral("î"));
    return s;
}

int tidyIngredients(Recipe &r)
{
    static const QRegularExpression credit(
        QStringLiteral("erfasst|konvertiert|angepa|\\bisbn\\b|\\bverlag\\b|^quelle\\b|^notiert\\b|überarbeitet|essen\\s*&\\s*trinken|"
                       "^rezept aus\\b|\\bkhb\\b|\\d+:\\d+/\\d+|@|gagnaux|heinzmann|\\bboller\\b|vollenweider|wildeisen|peschl|"
                       "^\\s*[>:]"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression info(QStringLiteral("^pro (portion|stück|stuck)\\b|\\bkcal\\b|^zubereitungszeit\\b|^\\d+\\s*(stunden?|minuten)\\b"),
                                         QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression participle(
        QStringLiteral("(geschnitten|gehackt|gewürfelt|zerdrückt|zerstoßen|zerstossen|gezupft|feingehackt|feingeschnitten|"
                       "feingemahlen|gemahlen|gerieben|geteilt|halbiert|entkernt|geputzt|gestrichen|durchgehackt|gestiftet|"
                       "vermischt|abgetropft|gewässert|eingeweicht|aufheben\\)?)\\.?$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QSet<QString> leadWords{ QStringLiteral("und"), QStringLiteral("oder"), QStringLiteral("sowie"), QStringLiteral("bzw"),
                                          QStringLiteral("ca"), QStringLiteral("etwa"), QStringLiteral("entsprechend") };
    static const QSet<QString> prepositions{ QStringLiteral("in"), QStringLiteral("ohne"), QStringLiteral("mit"), QStringLiteral("aus"),
                                             QStringLiteral("zu"), QStringLiteral("zum"), QStringLiteral("zur"), QStringLiteral("für"),
                                             QStringLiteral("nur"), QStringLiteral("am"), QStringLiteral("vom"), QStringLiteral("von"),
                                             QStringLiteral("bei"), QStringLiteral("nach"), QStringLiteral("je") };
    int changes = 0;
    QStringList sourceAdd, infoAdd;
    QList<Ingredient> out;
    for (const Ingredient &ing : r.ingredients) {
        if (ing.heading || ing.amount > 0) {
            out << ing;
            continue;
        }
        // „Scheiben geschnitten“: der Zerleger hat „Scheiben“ für eine Einheit gehalten
        const QString name = (ing.unit.isEmpty() ? ing.name : ing.unit + QLatin1Char(' ') + ing.name).trimmed();
        const QString line = ing.note.trimmed().isEmpty() ? name : name + QStringLiteral(", ") + ing.note.trimmed();
        if (name.isEmpty()) {
            ++changes;
            continue;
        }
        if (credit.match(line).hasMatch()) {
            QString c = line;
            while (c.startsWith(QLatin1Char('>')) || c.startsWith(QLatin1Char(':'))) c = c.mid(1).trimmed();
            if (!r.source.contains(c) && !sourceAdd.contains(c)) sourceAdd << c;
            ++changes;
            continue;
        }
        if (info.match(name).hasMatch()) {
            if (!infoAdd.contains(line)) infoAdd << line;
            ++changes;
            continue;
        }
        if (FoodTable::isNonFood(name) || FoodTable::instance().match(ing.name)) {
            out << ing;
            continue;
        }
        // „Teig“, „Marinade“, „Für den Teigmantel“ ohne Menge → Zwischenüberschrift
        static const QSet<QString> sections{ QStringLiteral("teig"), QStringLiteral("sauce"), QStringLiteral("soße"), QStringLiteral("marinade"),
                                             QStringLiteral("füllung"), QStringLiteral("glasur"), QStringLiteral("guss"), QStringLiteral("belag"),
                                             QStringLiteral("dekoration"), QStringLiteral("garnitur"), QStringLiteral("menü"), QStringLiteral("mahlzeiten"),
                                             QStringLiteral("streusel"), QStringLiteral("gebäck"), QStringLiteral("teigmantel"), QStringLiteral("dip"),
                                             QStringLiteral("dressing"), QStringLiteral("zubereitung"), QStringLiteral("vorbereitung") };
        static const QSet<QString> verbs{ QStringLiteral("abschmecken"), QStringLiteral("fritieren"), QStringLiteral("frittieren"),
                                          QStringLiteral("bestreichen"), QStringLiteral("begießen"), QStringLiteral("einfetten"),
                                          QStringLiteral("garnieren"), QStringLiteral("bestäuben"), QStringLiteral("servieren"),
                                          QStringLiteral("anbraten"), QStringLiteral("dekorieren"), QStringLiteral("verzieren"),
                                          QStringLiteral("ausbacken"), QStringLiteral("würzen") };
        const QString lowerName = name.toLower();
        if (sections.contains(lowerName) || (lowerName.startsWith(QLatin1String("für ")) && name.size() < 40)) {
            Ingredient h;
            h.heading = true;
            h.name = name;
            h.original = ing.original;
            out << h;
            ++changes;
            continue;
        }
        // „MITTAGESSEN“, „2. FRÜHSTÜCK“, „SAUCE:“ → Zwischenüberschrift
        int letters = 0, upper = 0;
        for (const QChar c : name)
            if (c.isLetter()) {
                ++letters;
                if (c.isUpper()) ++upper;
            }
        if ((letters >= 4 && upper == letters) || (name.endsWith(QLatin1Char(':')) && letters >= 3)) {
            Ingredient h;
            h.heading = true;
            QString t = name;
            if (t.endsWith(QLatin1Char(':'))) t.chop(1);
            h.name = t.trimmed();
            h.original = ing.original;
            out << h;
            ++changes;
            continue;
        }
        // umbrochener Rest der Vorzeile
        Ingredient *prev = nullptr;
        for (int k = int(out.size()) - 1; k >= 0; --k)
            if (!out[k].heading) {
                prev = &out[k];
                break;
            }
        const QStringList words = name.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        const QString first = words.value(0).toLower().remove(QLatin1Char('.'));
        bool capitalWord = false;
        for (const QString &w : words)
            if (!w.isEmpty() && w.at(0).isUpper()) capitalWord = true;
        const bool fragment = name.startsWith(QLatin1Char('(')) || (name.endsWith(QLatin1Char(')')) && !name.contains(QLatin1Char('(')))
                              || leadWords.contains(first) || participle.match(name).hasMatch() || verbs.contains(lowerName)
                              || (name.at(0).isLower() && (!capitalWord || prepositions.contains(first)));
        if (fragment && prev) {
            QString add = line;
            if (add.endsWith(QLatin1Char(')')) && !add.contains(QLatin1Char('('))) add.chop(1);
            if (add.startsWith(QLatin1Char('(')) && !add.contains(QLatin1Char(')'))) add = add.mid(1);
            add = add.trimmed();
            if (!add.isEmpty()) prev->note = prev->note.isEmpty() ? add : prev->note + QStringLiteral(" ") + add;
            ++changes;
            continue;
        }
        out << ing;
    }
    if (changes == 0) return 0;
    r.ingredients = out;
    if (!sourceAdd.isEmpty()) r.source = (QStringList{ r.source } + sourceAdd).join(QStringLiteral("; ")).trimmed();
    while (r.source.startsWith(QLatin1Char(';'))) r.source = r.source.mid(1).trimmed();
    if (!infoAdd.isEmpty()) {
        QList<Markup::Run> runs;
        Markup::Run rr;
        rr.italic = true;
        rr.text = QObject::tr("Angaben der Vorlage: %1").arg(infoAdd.join(QStringLiteral("; ")));
        runs << rr;
        const QString p = Markup::serializeInline(runs);
        if (!r.instructions.contains(infoAdd.first())) r.instructions = r.instructions.trimmed() + QStringLiteral("\n\n") + p;
    }
    return changes;
}

namespace {

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
    normalizeTexts(r);   // „200GradC“ → „200° C“ (auch nach DeepL)
    tidyIngredients(r);  // Erfasser, Nährwertangaben und umbrochene Reste aus der Zutatenliste
    Nutrition::analyze(r);
    if (opt.autoTags) Nutrition::applyTagSuggestions(r);
    if (opt.mealTags && !Nutrition::isMenu(r)) MealTypes::applyTags(r);
    if (opt.nutrientTags) MealTypes::applyNutrientTags(r);
    if (opt.seasonTags && !Nutrition::isMenu(r)) Season::applyTags(r);
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
    while (c.name.startsWith(QLatin1Char(';')) || c.name.startsWith(QLatin1Char(','))) c.name = c.name.mid(1).trimmed();   // „1 1/2 tb ;Salz“
    c.original = part.trimmed();
    return c;
}

} // namespace

Format detect(const QString &text)
{
    const QString head = text.left(20000);
    // Feldnamen entscheiden: MealMaster „Title:“, RezKonv „Titel:“. RezKonv kann auch MealMaster
    // schreiben – dann steht „*REZKONV* … nach Meal-Master“ in der Startzeile (z. B. Kochkiste-Archive).
    static const QRegularExpression enTitle(QStringLiteral("^\\s*Title\\s*:"), QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression deTitle(QStringLiteral("^\\s*Titel\\s*:"), QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
    const bool en = head.contains(enTitle), de = head.contains(deTitle);
    if (en && !de) return Format::MealMaster;
    if (de && !en) return Format::RezKonv;
    if (head.contains(QRegularExpression(QStringLiteral("^[=M]{5}(?!.*Meal-?Master).*REZKONV"),
                                         QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption)))
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
    case Format::Vtm: return QStringLiteral("ViviaMeals-Liste (.vtm)");
    case Format::Ambrosia: return QStringLiteral("Ambrosia");
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
    enum State { Outside, Header, Ingredients, Source, Directions };
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
            // RezKonv nach MealMaster: „MMMMM-----Quelle-----“ / „MMMMM-----Erfasst *RK* am … von-----“ sind keine Zutaten
            static const QRegularExpression sourceHead(QStringLiteral("^(?:quelle|erfasst|source|posted|from)\\b"),
                                                       QRegularExpression::CaseInsensitiveOption);
            const QString label = d.hasMatch() ? d.captured(1).trimmed() : QString();
            if (label.contains(sourceHead)) {
                if (!label.startsWith(QLatin1String("Quelle"), Qt::CaseInsensitive) && !label.startsWith(QLatin1String("Source"), Qt::CaseInsensitive))
                    raw.sourceLines << (label == label.toUpper() ? Umlauts::headingCase(label) : label);   // „Erfasst *RK* am 24.11.00 von“ + Name in der nächsten Zeile
                st = Source;
                continue;
            }
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
        if (st == Source) {
            if (blank(line)) {   // Quellenblock endet mit einer Leerzeile – danach Zutaten bzw. Zubereitung
                st = Ingredients;
                continue;
            }
            QString src = t;
            while (src.startsWith(QLatin1Char('-'))) src = src.mid(1).trimmed();
            if (!src.isEmpty()) raw.sourceLines << src;
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
    // „MMMMM----- *REZKONV* … nach Meal-Master“ ist MealMaster-Format (parseMealMaster)
    static const QRegularExpression start(QStringLiteral("^[=M]{5}(?!.*Meal-?Master).*REZKONV"), QRegularExpression::CaseInsensitiveOption);
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
            static const QRegularExpression kRe533_0(QStringLiteral("^[\\d½¼¾]"));
            const bool startsWithAmount = IngredientParser::parseNumber(firstTok, &dummy)
                                          || firstTok.contains(kRe533_0);
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
        res.warnings << QObject::tr("Die Datei ist keine ViviaMeals-Liste (.vtm).");
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
        normalizeTexts(r);
        Nutrition::analyze(r);
        if (opt.autoTags) Nutrition::applyTagSuggestions(r);
        if (opt.seasonTags && !Nutrition::isMenu(r)) Season::applyTags(r);
        res.recipes << r;
    }
    return res;
}

Result fromParts(const TextParts &parts, const QString &format, const Options &opt)
{
    Result res;
    RawRecipe raw;
    raw.title = parts.title;
    raw.yield = parts.yield;
    raw.categories = parts.categories;
    raw.ingredientLines = parts.ingredientLines;
    raw.directionLines = parts.directionLines;
    raw.sourceLines = parts.sourceLines;
    raw.format = format;
    res.recipes << finish(raw, opt, format, &res);
    return res;
}

Recipe fromStructured(const StructuredRecipe &in, const QString &format, const Options &opt, Result *res)
{
    RawRecipe raw;
    raw.title = in.title;
    raw.yield = in.yield;
    raw.categories = in.categories;
    for (const StructuredIngredient &si : in.ingredients) {
        RawRecipe::ColumnIngredient c;
        c.heading = si.heading;
        c.amount = si.amount;
        c.unit = si.unit;
        c.name = si.name;
        if (!si.note.trimmed().isEmpty()) c.more << si.note.trimmed();
        c.original = (QStringList{ si.amount, si.unit, si.name }.join(QLatin1Char(' ')).simplified()
                      + (si.note.trimmed().isEmpty() ? QString() : QStringLiteral(", ") + si.note.trimmed()));
        raw.columns << c;
    }
    raw.instructions = in.instructions;
    raw.directionLines = in.instructions.split(QLatin1Char('\n'));   // nur für Sprach- und Umlauterkennung
    if (!in.source.trimmed().isEmpty()) raw.sourceLines << in.source;
    raw.format = format;
    Recipe r = finish(raw, opt, format, res);
    if (in.prepMinutes > 0) r.prepMinutes = in.prepMinutes;
    if (!in.image.isEmpty()) r.image = in.image;
    if (in.created.isValid()) r.created = in.created;
    return r;
}

Result importFile(const QString &path, Format format, const Options &opt)
{
    Result res;
    if (format == Format::Ambrosia || path.endsWith(QLatin1String(".mdb"), Qt::CaseInsensitive)) return Ambrosia::importFile(path, opt);
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
    // gemischte Sammlungen (MealMaster- und RezKonv-Rezepte in einer Datei): beide Leser, Ergebnisse zusammen
    static const QRegularExpression mmStart(QStringLiteral("^(?:MMMMM|-----).*Meal-?Master"), QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rkStart(QStringLiteral("^[=M]{5}(?!.*Meal-?Master).*REZKONV"), QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
    if (text.contains(mmStart) && text.contains(rkStart)) {
        res = parseMealMaster(text, opt);
        const Result rk = parseRezKonv(text, opt);
        res.recipes << rk.recipes;
        res.warnings << rk.warnings;
        res.umlautsFixed += rk.umlautsFixed;
        res.translated += rk.translated;
        res.encoding = enc;
        return res;
    }
    if (f == Format::RezKonv) res = parseRezKonv(text, opt);
    else if (f == Format::MealMaster) res = parseMealMaster(text, opt);
    else res.warnings << QObject::tr("Das Format der Datei wurde nicht erkannt.");
    res.encoding = enc;
    return res;
}

} // namespace Importers
