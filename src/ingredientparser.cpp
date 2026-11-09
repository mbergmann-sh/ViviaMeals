// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "ingredientparser.h"
#include "foodtable.h"
#include "glossary.h"
#include "units.h"

#include <QRegularExpression>
#include <cmath>

namespace IngredientParser {

namespace {

double vulgar(QChar c)
{
    switch (c.unicode()) {
    case 0x00BD: return 0.5;
    case 0x00BC: return 0.25;
    case 0x00BE: return 0.75;
    case 0x2153: return 1.0 / 3;
    case 0x2154: return 2.0 / 3;
    case 0x215B: return 0.125;
    case 0x2155: return 0.2;
    default: return 0;
    }
}

// Mengenausdruck am Anfang: Zahl, Bruch, gemischter Bruch, Bereich
const QRegularExpression &amountRe()
{
    static const QString num = QStringLiteral("(?:\\d+\\s+\\d+/\\d+|\\d+-\\d+/\\d+|\\d+/\\d+|\\d+\\s*[½¼¾⅓⅔⅛]|[½¼¾⅓⅔⅛]|\\d+[.,]\\d+|\\d+)");
    static const QRegularExpression re(
        QStringLiteral("^\\s*(?:ca\\.?\\s*|etwa\\s+|about\\s+|approx\\.?\\s*)?(?<a>") + num
        + QStringLiteral(")(?:\\s*(?:-|–|bis|to|or)\\s*(?<b>") + num + QStringLiteral("))?(?=\\s|$|[a-zA-ZäöüÄÖÜ])"),
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

QString cleanName(QString s)
{
    s = s.trimmed();
    while (s.startsWith(QLatin1Char('-')) || s.startsWith(QLatin1Char('*'))) s = s.mid(1).trimmed();
    return s.simplified();
}

// Name und Zusatz trennen: „Mehl, gesiebt“, „Butter (weich)“, „Eier; verquirlt“
void splitNote(const QString &text, QString *name, QString *note)
{
    QString s = text.trimmed();
    QStringList notes;
    static const QRegularExpression paren(QStringLiteral("\\s*\\(([^)]*)\\)\\s*$"));
    auto m = paren.match(s);
    if (m.hasMatch() && m.capturedStart() > 0) {
        notes << m.captured(1).trimmed();
        s = s.left(m.capturedStart()).trimmed();
    }
    // „Zitrone: Saft“, „Petersilie; gehackt“, „Zwiebel, fein gewürfelt“
    const int c = s.indexOf(QRegularExpression(QStringLiteral("[,;]|:\\s")));
    if (c > 0) {
        notes.prepend(s.mid(c + 1).trimmed());
        s = s.left(c).trimmed();
    }
    *name = s;
    notes.removeAll(QString());
    *note = notes.join(QStringLiteral(", "));
}

// Größenangaben in deutschen Rezepten („2 gr. Zwiebeln“, „1 kl. Dose“)
bool sizeWord(const QString &t, QString *note)
{
    const QString l = t.toLower();
    if (l == QLatin1String("gr.") || l == QStringLiteral("groß") || l == QStringLiteral("große") || l == QStringLiteral("großer")
        || l == QLatin1String("gross") || l == QLatin1String("grosse") || l == QLatin1String("gross.") || l == QStringLiteral("groß.")) {
        *note = QStringLiteral("groß");
        return true;
    }
    if (l == QLatin1String("kl.") || l == QLatin1String("klein") || l == QLatin1String("klein.") || l == QLatin1String("kleine") || l == QLatin1String("kleiner")) {
        *note = QStringLiteral("klein");
        return true;
    }
    if (l == QLatin1String("mittl.") || l == QStringLiteral("mittelgroß") || l == QStringLiteral("mittelgroße")
        || l == QLatin1String("mittelgr.")) {
        *note = QStringLiteral("mittelgroß");
        return true;
    }
    return false;
}

// Englische Einheiten umrechnen (nach der Zuordnung zur Nährwerttabelle, wegen der Dichte)
void germanize(Ingredient &ing, const Units::Unit &u)
{
    if (!u.english) {
        ing.unit = u.de;
        return;
    }
    if (!u.note.isEmpty()) ing.note = ing.note.isEmpty() ? u.note : u.note + QStringLiteral(", ") + ing.note;
    const Food *food = FoodTable::instance().match(ing.name);
    if (u.de == QLatin1String("ml") || u.de == QLatin1String("l")) {
        double ml = ing.amount * u.base, mlMax = ing.amountMax * u.base;
        // feste Zutaten (Dichte deutlich ≠ 1 bzw. keine Flüssigkeit) in Gramm
        const bool liquid = !food || food->category == QLatin1String("getraenk") || food->category == QLatin1String("alkohol")
                            || food->key == QLatin1String("milch") || food->key == QLatin1String("milch_fettarm")
                            || food->key == QLatin1String("buttermilch") || food->key == QLatin1String("sahne")
                            || food->key.startsWith(QLatin1String("oel")) || food->key == QLatin1String("olivenoel")
                            || food->key == QLatin1String("sesamoel") || food->key == QLatin1String("bruehe")
                            || food->key == QLatin1String("zitronensaft") || food->key == QLatin1String("essig")
                            || food->key == QLatin1String("sojasosse") || food->key == QLatin1String("kondensmilch")
                            || food->key == QLatin1String("kokosmilch") || food->key == QLatin1String("sojadrink")
                            || food->key == QLatin1String("wasser");
        if (!liquid && food) {
            ing.amount = ml * food->density;
            ing.amountMax = mlMax * food->density;
            ing.unit = QStringLiteral("g");
        } else if (ml >= 1000) {
            ing.amount = std::round(ml / 50.0) / 20.0;       // auf 0,05 l
            ing.amountMax = std::round(mlMax / 50.0) / 20.0;
            ing.unit = QStringLiteral("l");
        } else {
            ing.amount = ml;
            ing.amountMax = mlMax;
            ing.unit = QStringLiteral("ml");
        }
    } else if (u.de == QLatin1String("g")) {
        ing.amount *= u.base;
        ing.amountMax *= u.base;
        ing.unit = ing.amount >= 1000 && std::fmod(ing.amount, 250) < 1 ? QStringLiteral("kg") : QStringLiteral("g");
        if (ing.unit == QLatin1String("kg")) {
            ing.amount /= 1000.0;
            ing.amountMax /= 1000.0;
        }
    } else if (u.de == QLatin1String("cm")) {
        ing.amount *= u.base;
        ing.amountMax *= u.base;
        ing.unit = QStringLiteral("cm");
    } else {
        ing.unit = u.de;   // EL, TL, Prise, Dose, Zehe … (Zahl bleibt)
    }
    // Rundung auf sinnvolle Küchenwerte
    auto round = [&](double v) {
        if (ing.unit == QLatin1String("g") || ing.unit == QLatin1String("ml"))
            return v >= 100 ? std::round(v / 5.0) * 5.0 : v >= 10 ? std::round(v) : std::round(v * 2) / 2.0;
        return v;
    };
    ing.amount = round(ing.amount);
    ing.amountMax = round(ing.amountMax);
}

void translateIfWanted(Ingredient &ing, const Options &opt)
{
    if (!opt.translate) return;
    if (!ing.note.isEmpty()) ing.note = Glossary::note(ing.note);
    QString lead;
    const QString de = Glossary::ingredient(ing.name, &lead);
    if (!de.isEmpty() && de != ing.name) ing.name = de;
    if (!lead.isEmpty()) ing.note = ing.note.isEmpty() ? lead : lead + QStringLiteral(", ") + ing.note;
}

} // namespace

bool parseNumber(const QString &input, double *out)
{
    QString s = input.trimmed();
    if (s.isEmpty()) return false;
    static const QRegularExpression mixed(QStringLiteral("^(\\d+)[\\s-]+(\\d+)/(\\d+)$"));
    static const QRegularExpression frac(QStringLiteral("^(\\d+)/(\\d+)$"));
    static const QRegularExpression withVulgar(QStringLiteral("^(\\d*)\\s*([½¼¾⅓⅔⅛⅕])$"));
    QRegularExpressionMatch m;
    if ((m = mixed.match(s)).hasMatch()) {
        const double d = m.captured(3).toDouble();
        if (d <= 0) return false;
        *out = m.captured(1).toDouble() + m.captured(2).toDouble() / d;
        return true;
    }
    if ((m = frac.match(s)).hasMatch()) {
        const double d = m.captured(2).toDouble();
        if (d <= 0) return false;
        *out = m.captured(1).toDouble() / d;
        return true;
    }
    if ((m = withVulgar.match(s)).hasMatch()) {
        *out = m.captured(1).toDouble() + vulgar(m.captured(2).at(0));
        return true;
    }
    s.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double v = s.toDouble(&ok);
    if (ok) *out = v;
    return ok;
}

bool isHeading(const QString &line, QString *title)
{
    QString s = line.trimmed();
    static const QRegularExpression marked(QStringLiteral("^(?:MMMMM)?[=#]{2,}\\s*(.*?)\\s*[=#]*$"));
    static const QRegularExpression dashed(QStringLiteral("^(?:MMMMM)?[\\-_*]{2,}\\s*(.*?)\\s*[\\-_*]{2,}$"));
    auto m = marked.match(s);
    if (!m.hasMatch()) m = dashed.match(s);
    if (m.hasMatch() && !m.captured(1).isEmpty()) {
        if (title) *title = m.captured(1).trimmed();
        return true;
    }
    // „Für den Teig:“, „FOR THE SAUCE:“ – ohne Mengenangabe
    if (s.endsWith(QLatin1Char(':')) && s.size() > 2 && s.size() < 50 && !amountRe().match(s).hasMatch()) {
        if (title) *title = s.left(s.size() - 1).trimmed();
        return true;
    }
    return false;
}

Ingredient fromParts(const QString &amountText, const QString &unitText, const QString &nameText, const Options &opt)
{
    Ingredient ing;
    QString a = amountText.trimmed();
    // Bereich „2-3“
    static const QRegularExpression range(QStringLiteral("^(.+?)\\s*(?:-|–|bis)\\s*(.+)$"));
    double v = 0;
    if (!a.isEmpty()) {
        if (parseNumber(a, &v)) {
            ing.amount = v;
        } else if (auto m = range.match(a); m.hasMatch()) {
            double lo = 0, hi = 0;
            if (parseNumber(m.captured(1), &lo) && parseNumber(m.captured(2), &hi)) {
                if (hi < lo && hi < 1) {
                    ing.amount = lo + hi;            // „1-1/2“ = 1½
                } else {
                    ing.amount = lo;
                    ing.amountMax = hi;
                }
            }
        }
    }
    QString name, note;
    splitNote(cleanName(nameText), &name, &note);
    // Stückgewicht im Namen: „Zanderfilets a 125 g“, „Entenbrüste à ca. 300 g“ → Zusatz
    static const QRegularExpression each(QStringLiteral("\\s+(?:à|a|á|je|zu je)\\s+((?:ca\\.\\s*)?\\d+(?:[.,]\\d+)?\\s*(?:g|kg|ml))\\b\\s*(.*)$"));
    if (const auto e = each.match(name); e.hasMatch() && e.capturedStart() > 0) {
        QStringList extra{ QStringLiteral("à ") + e.captured(1) };
        if (!e.captured(2).trimmed().isEmpty()) extra << e.captured(2).trimmed();
        if (!note.isEmpty()) extra << note;
        note = extra.join(QStringLiteral(", "));
        name = name.left(e.capturedStart()).trimmed();
    }
    ing.name = name;
    ing.note = note;
    Units::Unit u;
    const QString ut = unitText.trimmed();
    QString sizeNote;
    if (!ut.isEmpty() && Units::lookup(ut, &u)) {
        translateIfWanted(ing, opt);
        germanize(ing, u);
    } else {
        if (!ut.isEmpty() && sizeWord(ut, &sizeNote))
            ing.note = ing.note.isEmpty() ? sizeNote : sizeNote + QStringLiteral(", ") + ing.note;
        else if (!ut.isEmpty())
            ing.name = ut + QLatin1Char(' ') + ing.name;   // unbekannte Einheit gehört zum Namen
        translateIfWanted(ing, opt);
    }
    // „Salz und Pfeffer nach Belieben“ → Zusatz
    static const QRegularExpression taste(QStringLiteral("\\s+(nach (?:Belieben|Geschmack)|zum (?:Bestreuen|Garnieren|Bestäuben|Einfetten|Braten)|n\\. ?B\\.|to taste|for garnish)$"),
                                          QRegularExpression::CaseInsensitiveOption);
    if (auto t = taste.match(ing.name); t.hasMatch()) {
        const QString n = opt.translate ? Glossary::note(t.captured(1)) : t.captured(1);
        ing.note = ing.note.isEmpty() ? n : n + QStringLiteral(", ") + ing.note;
        ing.name = ing.name.left(t.capturedStart()).trimmed();
    }
    // Dosengröße im Zusatz („1 Dose Tomaten (14 oz)“) → Gewicht
    if (ing.amount > 0 && (ing.unit == QLatin1String("Dose") || ing.unit == QLatin1String("Pck.") || ing.unit == QLatin1String("Glas")
                           || ing.unit == QLatin1String("Packung") || ing.unit == QLatin1String("Becher"))) {
        static const QRegularExpression sz(QStringLiteral("^(\\d+(?:[.,]\\d+)?)\\s*(oz|ounces?|g|ml)\\.?$"), QRegularExpression::CaseInsensitiveOption);
        QStringList notes = ing.note.split(QStringLiteral(", "), Qt::SkipEmptyParts);
        for (int k = 0; k < notes.size(); ++k) {
            const auto m = sz.match(notes[k].trimmed());
            if (!m.hasMatch()) continue;
            double size = 0;
            parseNumber(m.captured(1), &size);
            const QString su = m.captured(2).toLower();
            const double factor = su.startsWith(QLatin1Char('o')) ? 28.35 : 1.0;
            const double total = ing.amount * size * factor;
            ing.amount = total >= 100 ? std::round(total / 5.0) * 5.0 : std::round(total);
            ing.unit = su == QLatin1String("ml") ? QStringLiteral("ml") : QStringLiteral("g");
            notes.removeAt(k);
            ing.note = notes.join(QStringLiteral(", "));
            break;
        }
    }
    return ing;
}

Ingredient parseLine(const QString &line, const Options &opt)
{
    QString title;
    if (isHeading(line, &title)) {
        Ingredient h;
        h.heading = true;
        h.name = opt.translate ? Glossary::ingredient(title) : title;
        return h;
    }
    QString s = line.trimmed();
    QString amount;
    const auto m = amountRe().match(s);
    if (m.hasMatch()) {
        amount = m.captured(QStringLiteral("a"));
        if (!m.captured(QStringLiteral("b")).isEmpty()) amount += QStringLiteral("-") + m.captured(QStringLiteral("b"));
        s = s.mid(m.capturedEnd()).trimmed();
    }
    // Packungsgröße „1 (16 oz) can tomatoes“ / „2 (400 g) Dosen Tomaten“
    static const QRegularExpression pack(QStringLiteral("^\\((\\d+(?:[.,]\\d+)?)[\\s-]*(oz|ounce|ounces|g|ml|lb|fl\\.? ?oz)\\.?\\)\\s*(\\S+)\\s+(.*)$"),
                                         QRegularExpression::CaseInsensitiveOption);
    if (auto p = pack.match(s); p.hasMatch()) {
        double n = 1, size = 0;
        parseNumber(amount.isEmpty() ? QStringLiteral("1") : amount, &n);
        parseNumber(p.captured(1), &size);
        Units::Unit pu, cont;
        QString rest = p.captured(4);
        if (!Units::lookup(p.captured(3), &cont)) rest = p.captured(3) + QLatin1Char(' ') + rest;
        if (Units::lookup(p.captured(2), &pu)) {
            QString unitTok = p.captured(2);
            const double total = n * size;
            return fromParts(QString::number(total), unitTok, rest, opt);
        }
    }
    // Einheit: erstes Wort (bzw. „fl oz“)
    QString unit;
    static const QRegularExpression twoWord(QStringLiteral("^(fl\\.?\\s*oz\\.?|n\\.\\s?b\\.)\\s+"), QRegularExpression::CaseInsensitiveOption);
    if (auto t = twoWord.match(s); t.hasMatch()) {
        unit = t.captured(1).remove(QLatin1Char(' '));
        if (unit.toLower().startsWith(QLatin1String("fl"))) unit = QStringLiteral("floz");
        s = s.mid(t.capturedEnd());
    } else {
        const int sp = s.indexOf(QLatin1Char(' '));
        const QString first = sp < 0 ? s : s.left(sp);
        Units::Unit u;
        QString dummy;
        // Einheit nur, wenn danach noch ein Name folgt (sonst ist „Zehe“ allein der Name)
        if (sp > 0 && (Units::lookup(first, &u) || sizeWord(first, &dummy))) {
            // „gr.“ ist bei kleinen Mengen „groß“, sonst Gramm
            double a = 0;
            parseNumber(amount, &a);
            if (first.compare(QLatin1String("gr."), Qt::CaseInsensitive) == 0 && a > 0 && a < 5) {
                unit = QStringLiteral("groß");
            } else {
                unit = first;
            }
            s = s.mid(sp + 1);
        } else if (amount.isEmpty() && (first.compare(QLatin1String("etwas"), Qt::CaseInsensitive) == 0)) {
            unit = first;
            s = s.mid(sp + 1);
        }
    }
    return fromParts(amount, unit, s, opt);
}

QList<Ingredient> parseText(const QString &text, const Options &opt)
{
    QList<Ingredient> list;
    for (const QString &raw : text.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith(QLatin1Char('-')) && !isHeading(line) && !list.isEmpty() && !list.last().heading) {
            // Fortsetzung der vorigen Zutat („-sifted“, „-- zum Bestäuben“)
            QString add = line;
            while (add.startsWith(QLatin1Char('-'))) add = add.mid(1);
            add = add.trimmed();
            if (opt.translate) add = Glossary::note(add);
            Ingredient &prev = list.last();
            prev.note = prev.note.isEmpty() ? add : prev.note + QStringLiteral(", ") + add;
            continue;
        }
        list << parseLine(line, opt);
    }
    return list;
}

} // namespace IngredientParser
