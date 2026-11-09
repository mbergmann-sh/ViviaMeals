// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "units.h"

#include <QHash>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>

namespace Units {

namespace {

struct Def { const char *de; Kind kind; double base; };

// Deutsche Einheiten (kanonisch)
const QHash<QString, Def> &germanDefs()
{
    static const QHash<QString, Def> d{
        { QStringLiteral("g"), { "g", Kind::Mass, 1 } },
        { QStringLiteral("kg"), { "kg", Kind::Mass, 1000 } },
        { QStringLiteral("mg"), { "mg", Kind::Mass, 0.001 } },
        { QStringLiteral("ml"), { "ml", Kind::Volume, 1 } },
        { QStringLiteral("cl"), { "cl", Kind::Volume, 10 } },
        { QStringLiteral("dl"), { "dl", Kind::Volume, 100 } },
        { QStringLiteral("l"), { "l", Kind::Volume, 1000 } },
        { QStringLiteral("EL"), { "EL", Kind::Volume, 15 } },
        { QStringLiteral("TL"), { "TL", Kind::Volume, 5 } },
        { QStringLiteral("Tasse"), { "Tasse", Kind::Volume, 150 } },
        { QStringLiteral("Spritzer"), { "Spritzer", Kind::Volume, 1 } },
        { QStringLiteral("Schuss"), { "Schuss", Kind::Volume, 20 } },
        { QStringLiteral("Tropfen"), { "Tropfen", Kind::Volume, 0.05 } },
        { QStringLiteral("Prise"), { "Prise", Kind::Mass, 0.4 } },
        { QStringLiteral("Msp."), { "Msp.", Kind::Mass, 0.3 } },
        { QStringLiteral("Stück"), { "Stück", Kind::Piece, 0 } },
        { QStringLiteral("Pck."), { "Pck.", Kind::Piece, 0 } },
        { QStringLiteral("Dose"), { "Dose", Kind::Piece, 0 } },
        { QStringLiteral("Bund"), { "Bund", Kind::Piece, 0 } },
        { QStringLiteral("Zehe"), { "Zehe", Kind::Piece, 0 } },
        { QStringLiteral("Scheibe"), { "Scheibe", Kind::Piece, 0 } },
        { QStringLiteral("Becher"), { "Becher", Kind::Piece, 0 } },
        { QStringLiteral("Glas"), { "Glas", Kind::Piece, 0 } },
        { QStringLiteral("Würfel"), { "Würfel", Kind::Piece, 0 } },
        { QStringLiteral("Blatt"), { "Blatt", Kind::Piece, 0 } },
        { QStringLiteral("Zweig"), { "Zweig", Kind::Piece, 0 } },
        { QStringLiteral("Stange"), { "Stange", Kind::Piece, 0 } },
        { QStringLiteral("Handvoll"), { "Handvoll", Kind::Piece, 0 } },
        { QStringLiteral("Kopf"), { "Kopf", Kind::Piece, 0 } },
        { QStringLiteral("Knolle"), { "Knolle", Kind::Piece, 0 } },
        { QStringLiteral("Tafel"), { "Tafel", Kind::Piece, 0 } },
        { QStringLiteral("Flasche"), { "Flasche", Kind::Piece, 0 } },
        { QStringLiteral("Beutel"), { "Beutel", Kind::Piece, 0 } },
        { QStringLiteral("Schale"), { "Schale", Kind::Piece, 0 } },
        { QStringLiteral("Packung"), { "Packung", Kind::Piece, 0 } },
        { QStringLiteral("Kugel"), { "Kugel", Kind::Piece, 0 } },
        { QStringLiteral("etwas"), { "etwas", Kind::Vague, 0 } },
        { QStringLiteral("wenig"), { "wenig", Kind::Vague, 0 } },
        { QStringLiteral("einige"), { "einige", Kind::Vague, 0 } },
        { QStringLiteral("n. B."), { "n. B.", Kind::Vague, 0 } },
        { QStringLiteral("cm"), { "cm", Kind::Piece, 0 } },
    };
    return d;
}

// Schreibweisen → kanonische deutsche Einheit (klein geschrieben verglichen)
const QHash<QString, QString> &germanAliases()
{
    static const QHash<QString, QString> a{
        { QStringLiteral("g"), QStringLiteral("g") }, { QStringLiteral("gr"), QStringLiteral("g") },
        { QStringLiteral("gr."), QStringLiteral("g") }, { QStringLiteral("gramm"), QStringLiteral("g") },
        { QStringLiteral("kg"), QStringLiteral("kg") }, { QStringLiteral("kilo"), QStringLiteral("kg") },
        { QStringLiteral("kilogramm"), QStringLiteral("kg") }, { QStringLiteral("mg"), QStringLiteral("mg") },
        { QStringLiteral("ml"), QStringLiteral("ml") }, { QStringLiteral("milliliter"), QStringLiteral("ml") },
        { QStringLiteral("cl"), QStringLiteral("cl") }, { QStringLiteral("dl"), QStringLiteral("dl") },
        { QStringLiteral("l"), QStringLiteral("l") }, { QStringLiteral("ltr"), QStringLiteral("l") },
        { QStringLiteral("ltr."), QStringLiteral("l") }, { QStringLiteral("liter"), QStringLiteral("l") },
        { QStringLiteral("el"), QStringLiteral("EL") }, { QStringLiteral("essl."), QStringLiteral("EL") },
        { QStringLiteral("essl"), QStringLiteral("EL") }, { QStringLiteral("eßl."), QStringLiteral("EL") },
        { QStringLiteral("esslöffel"), QStringLiteral("EL") }, { QStringLiteral("eßlöffel"), QStringLiteral("EL") },
        { QStringLiteral("tl"), QStringLiteral("TL") }, { QStringLiteral("teel."), QStringLiteral("TL") },
        { QStringLiteral("teel"), QStringLiteral("TL") }, { QStringLiteral("teelöffel"), QStringLiteral("TL") },
        { QStringLiteral("tasse"), QStringLiteral("Tasse") }, { QStringLiteral("tassen"), QStringLiteral("Tasse") },
        { QStringLiteral("prise"), QStringLiteral("Prise") }, { QStringLiteral("prisen"), QStringLiteral("Prise") },
        { QStringLiteral("pr."), QStringLiteral("Prise") },
        { QStringLiteral("msp."), QStringLiteral("Msp.") }, { QStringLiteral("msp"), QStringLiteral("Msp.") },
        { QStringLiteral("messerspitze"), QStringLiteral("Msp.") },
        { QStringLiteral("stück"), QStringLiteral("Stück") }, { QStringLiteral("st."), QStringLiteral("Stück") },
        { QStringLiteral("stk."), QStringLiteral("Stück") }, { QStringLiteral("stk"), QStringLiteral("Stück") },
        { QStringLiteral("pck."), QStringLiteral("Pck.") }, { QStringLiteral("pck"), QStringLiteral("Pck.") },
        { QStringLiteral("pack."), QStringLiteral("Pck.") }, { QStringLiteral("pkg."), QStringLiteral("Pck.") },
        { QStringLiteral("päckchen"), QStringLiteral("Pck.") }, { QStringLiteral("pk."), QStringLiteral("Pck.") },
        { QStringLiteral("packung"), QStringLiteral("Packung") }, { QStringLiteral("pckg."), QStringLiteral("Packung") },
        { QStringLiteral("dose"), QStringLiteral("Dose") }, { QStringLiteral("dosen"), QStringLiteral("Dose") },
        { QStringLiteral("ds."), QStringLiteral("Dose") },
        { QStringLiteral("bund"), QStringLiteral("Bund") }, { QStringLiteral("bd."), QStringLiteral("Bund") },
        { QStringLiteral("zehe"), QStringLiteral("Zehe") }, { QStringLiteral("zehen"), QStringLiteral("Zehe") },
        { QStringLiteral("scheibe"), QStringLiteral("Scheibe") }, { QStringLiteral("scheiben"), QStringLiteral("Scheibe") },
        { QStringLiteral("scheib."), QStringLiteral("Scheibe") }, { QStringLiteral("sch."), QStringLiteral("Scheibe") },
        { QStringLiteral("becher"), QStringLiteral("Becher") }, { QStringLiteral("glas"), QStringLiteral("Glas") },
        { QStringLiteral("gläser"), QStringLiteral("Glas") },
        { QStringLiteral("würfel"), QStringLiteral("Würfel") }, { QStringLiteral("wuerfel"), QStringLiteral("Würfel") },
        { QStringLiteral("blatt"), QStringLiteral("Blatt") }, { QStringLiteral("blätter"), QStringLiteral("Blatt") },
        { QStringLiteral("zweig"), QStringLiteral("Zweig") }, { QStringLiteral("zweige"), QStringLiteral("Zweig") },
        { QStringLiteral("stange"), QStringLiteral("Stange") }, { QStringLiteral("stangen"), QStringLiteral("Stange") },
        { QStringLiteral("handvoll"), QStringLiteral("Handvoll") }, { QStringLiteral("hand"), QStringLiteral("Handvoll") },
        { QStringLiteral("kopf"), QStringLiteral("Kopf") }, { QStringLiteral("köpfe"), QStringLiteral("Kopf") },
        { QStringLiteral("knolle"), QStringLiteral("Knolle") }, { QStringLiteral("knollen"), QStringLiteral("Knolle") },
        { QStringLiteral("tafel"), QStringLiteral("Tafel") }, { QStringLiteral("tafeln"), QStringLiteral("Tafel") },
        { QStringLiteral("flasche"), QStringLiteral("Flasche") }, { QStringLiteral("fl."), QStringLiteral("Flasche") },
        { QStringLiteral("beutel"), QStringLiteral("Beutel") }, { QStringLiteral("btl."), QStringLiteral("Beutel") },
        { QStringLiteral("schale"), QStringLiteral("Schale") },
        { QStringLiteral("spritzer"), QStringLiteral("Spritzer") }, { QStringLiteral("schuss"), QStringLiteral("Schuss") },
        { QStringLiteral("schuß"), QStringLiteral("Schuss") }, { QStringLiteral("tropfen"), QStringLiteral("Tropfen") },
        { QStringLiteral("etwas"), QStringLiteral("etwas") }, { QStringLiteral("evtl."), QStringLiteral("etwas") },
        { QStringLiteral("wenig"), QStringLiteral("wenig") }, { QStringLiteral("einige"), QStringLiteral("einige") },
        { QStringLiteral("kugel"), QStringLiteral("Kugel") }, { QStringLiteral("kugeln"), QStringLiteral("Kugel") },
        { QStringLiteral("stueck"), QStringLiteral("Stück") }, { QStringLiteral("stck."), QStringLiteral("Stück") },
        { QStringLiteral("loeffel"), QStringLiteral("EL") }, { QStringLiteral("löffel"), QStringLiteral("EL") },
        { QStringLiteral("essloeffel"), QStringLiteral("EL") }, { QStringLiteral("teeloeffel"), QStringLiteral("TL") },
        { QStringLiteral("paeckchen"), QStringLiteral("Pck.") }, { QStringLiteral("messersp."), QStringLiteral("Msp.") },
        { QStringLiteral("zweiglein"), QStringLiteral("Zweig") }, { QStringLiteral("stiel"), QStringLiteral("Zweig") },
        { QStringLiteral("stiele"), QStringLiteral("Zweig") }, { QStringLiteral("tl."), QStringLiteral("TL") },
        { QStringLiteral("el."), QStringLiteral("EL") }, { QStringLiteral("glaeser"), QStringLiteral("Glas") },
        { QStringLiteral("n.b."), QStringLiteral("n. B.") }, { QStringLiteral("n. b."), QStringLiteral("n. B.") },
        { QStringLiteral("nb"), QStringLiteral("n. B.") }, { QStringLiteral("cm"), QStringLiteral("cm") },
    };
    return a;
}

struct EnDef { const char *de; Kind kind; double factor; const char *note; };

// Englische/US-Einheiten und MealMaster-Kürzel (klein geschrieben, außer „T“ bzw. „t“)
const QHash<QString, EnDef> &englishDefs()
{
    static const QHash<QString, EnDef> e{
        // Volumen → ml
        { QStringLiteral("c"), { "ml", Kind::Volume, 236.6, "" } },
        { QStringLiteral("c."), { "ml", Kind::Volume, 236.6, "" } },
        { QStringLiteral("cup"), { "ml", Kind::Volume, 236.6, "" } },
        { QStringLiteral("cups"), { "ml", Kind::Volume, 236.6, "" } },
        { QStringLiteral("fl"), { "ml", Kind::Volume, 29.57, "" } },
        { QStringLiteral("floz"), { "ml", Kind::Volume, 29.57, "" } },
        { QStringLiteral("fl.oz."), { "ml", Kind::Volume, 29.57, "" } },
        { QStringLiteral("pt"), { "ml", Kind::Volume, 473.2, "" } },
        { QStringLiteral("pint"), { "ml", Kind::Volume, 473.2, "" } },
        { QStringLiteral("pints"), { "ml", Kind::Volume, 473.2, "" } },
        { QStringLiteral("qt"), { "ml", Kind::Volume, 946.4, "" } },
        { QStringLiteral("quart"), { "ml", Kind::Volume, 946.4, "" } },
        { QStringLiteral("quarts"), { "ml", Kind::Volume, 946.4, "" } },
        { QStringLiteral("ga"), { "l", Kind::Volume, 3785.4, "" } },
        { QStringLiteral("gal"), { "l", Kind::Volume, 3785.4, "" } },
        { QStringLiteral("gallon"), { "l", Kind::Volume, 3785.4, "" } },
        { QStringLiteral("cb"), { "ml", Kind::Volume, 1, "" } },
        // Löffel → EL/TL (deutsche Küchenmaße, keine Umrechnung der Zahl)
        { QStringLiteral("T"), { "EL", Kind::Volume, 1, "" } },
        { QStringLiteral("tb"), { "EL", Kind::Volume, 1, "" } },
        { QStringLiteral("tbs"), { "EL", Kind::Volume, 1, "" } },
        { QStringLiteral("tbsp"), { "EL", Kind::Volume, 1, "" } },
        { QStringLiteral("tbsp."), { "EL", Kind::Volume, 1, "" } },
        { QStringLiteral("tablespoon"), { "EL", Kind::Volume, 1, "" } },
        { QStringLiteral("tablespoons"), { "EL", Kind::Volume, 1, "" } },
        { QStringLiteral("t"), { "TL", Kind::Volume, 1, "" } },
        { QStringLiteral("ts"), { "TL", Kind::Volume, 1, "" } },
        { QStringLiteral("tsp"), { "TL", Kind::Volume, 1, "" } },
        { QStringLiteral("tsp."), { "TL", Kind::Volume, 1, "" } },
        { QStringLiteral("teaspoon"), { "TL", Kind::Volume, 1, "" } },
        { QStringLiteral("teaspoons"), { "TL", Kind::Volume, 1, "" } },
        // Gewicht → g
        { QStringLiteral("oz"), { "g", Kind::Mass, 28.35, "" } },
        { QStringLiteral("oz."), { "g", Kind::Mass, 28.35, "" } },
        { QStringLiteral("ounce"), { "g", Kind::Mass, 28.35, "" } },
        { QStringLiteral("ounces"), { "g", Kind::Mass, 28.35, "" } },
        { QStringLiteral("lb"), { "g", Kind::Mass, 453.6, "" } },
        { QStringLiteral("lb."), { "g", Kind::Mass, 453.6, "" } },
        { QStringLiteral("lbs"), { "g", Kind::Mass, 453.6, "" } },
        { QStringLiteral("lbs."), { "g", Kind::Mass, 453.6, "" } },
        { QStringLiteral("pound"), { "g", Kind::Mass, 453.6, "" } },
        { QStringLiteral("pounds"), { "g", Kind::Mass, 453.6, "" } },
        { QStringLiteral("cg"), { "g", Kind::Mass, 0.01, "" } },
        { QStringLiteral("dg"), { "g", Kind::Mass, 0.1, "" } },
        { QStringLiteral("stick"), { "g", Kind::Mass, 113.4, "" } },
        { QStringLiteral("sticks"), { "g", Kind::Mass, 113.4, "" } },
        // kleine Mengen
        { QStringLiteral("pn"), { "Prise", Kind::Mass, 1, "" } },
        { QStringLiteral("pinch"), { "Prise", Kind::Mass, 1, "" } },
        { QStringLiteral("pinches"), { "Prise", Kind::Mass, 1, "" } },
        { QStringLiteral("ds"), { "Spritzer", Kind::Volume, 1, "" } },
        { QStringLiteral("dash"), { "Spritzer", Kind::Volume, 1, "" } },
        { QStringLiteral("dashes"), { "Spritzer", Kind::Volume, 1, "" } },
        { QStringLiteral("dr"), { "Tropfen", Kind::Volume, 1, "" } },
        { QStringLiteral("drop"), { "Tropfen", Kind::Volume, 1, "" } },
        { QStringLiteral("drops"), { "Tropfen", Kind::Volume, 1, "" } },
        // Stück-/Packungseinheiten
        { QStringLiteral("x"), { "", Kind::Piece, 1, "" } },
        { QStringLiteral("ea"), { "", Kind::Piece, 1, "" } },
        { QStringLiteral("each"), { "", Kind::Piece, 1, "" } },
        { QStringLiteral("sm"), { "", Kind::Piece, 1, "klein" } },
        { QStringLiteral("small"), { "", Kind::Piece, 1, "klein" } },
        { QStringLiteral("md"), { "", Kind::Piece, 1, "mittelgroß" } },
        { QStringLiteral("medium"), { "", Kind::Piece, 1, "mittelgroß" } },
        { QStringLiteral("lg"), { "", Kind::Piece, 1, "groß" } },
        { QStringLiteral("large"), { "", Kind::Piece, 1, "groß" } },
        { QStringLiteral("cn"), { "Dose", Kind::Piece, 1, "" } },
        { QStringLiteral("can"), { "Dose", Kind::Piece, 1, "" } },
        { QStringLiteral("cans"), { "Dose", Kind::Piece, 1, "" } },
        { QStringLiteral("pk"), { "Pck.", Kind::Piece, 1, "" } },
        { QStringLiteral("pkg"), { "Pck.", Kind::Piece, 1, "" } },
        { QStringLiteral("package"), { "Pck.", Kind::Piece, 1, "" } },
        { QStringLiteral("packages"), { "Pck.", Kind::Piece, 1, "" } },
        { QStringLiteral("envelope"), { "Pck.", Kind::Piece, 1, "" } },
        { QStringLiteral("ct"), { "Packung", Kind::Piece, 1, "" } },
        { QStringLiteral("carton"), { "Packung", Kind::Piece, 1, "" } },
        { QStringLiteral("jar"), { "Glas", Kind::Piece, 1, "" } },
        { QStringLiteral("bn"), { "Bund", Kind::Piece, 1, "" } },
        { QStringLiteral("bunch"), { "Bund", Kind::Piece, 1, "" } },
        { QStringLiteral("sl"), { "Scheibe", Kind::Piece, 1, "" } },
        { QStringLiteral("slice"), { "Scheibe", Kind::Piece, 1, "" } },
        { QStringLiteral("slices"), { "Scheibe", Kind::Piece, 1, "" } },
        { QStringLiteral("clove"), { "Zehe", Kind::Piece, 1, "" } },
        { QStringLiteral("cloves"), { "Zehe", Kind::Piece, 1, "" } },
        { QStringLiteral("head"), { "Kopf", Kind::Piece, 1, "" } },
        { QStringLiteral("heads"), { "Kopf", Kind::Piece, 1, "" } },
        { QStringLiteral("sprig"), { "Zweig", Kind::Piece, 1, "" } },
        { QStringLiteral("sprigs"), { "Zweig", Kind::Piece, 1, "" } },
        { QStringLiteral("stalk"), { "Stange", Kind::Piece, 1, "" } },
        { QStringLiteral("stalks"), { "Stange", Kind::Piece, 1, "" } },
        { QStringLiteral("handful"), { "Handvoll", Kind::Piece, 1, "" } },
        { QStringLiteral("bottle"), { "Flasche", Kind::Piece, 1, "" } },
        { QStringLiteral("bag"), { "Beutel", Kind::Piece, 1, "" } },
        { QStringLiteral("cube"), { "Würfel", Kind::Piece, 1, "" } },
        { QStringLiteral("cubes"), { "Würfel", Kind::Piece, 1, "" } },
        { QStringLiteral("leaf"), { "Blatt", Kind::Piece, 1, "" } },
        { QStringLiteral("leaves"), { "Blatt", Kind::Piece, 1, "" } },
        { QStringLiteral("inch"), { "cm", Kind::Piece, 2.54, "" } },
    };
    return e;
}

} // namespace

bool lookup(const QString &token, Unit *out)
{
    const QString t = token.trimmed();
    if (t.isEmpty()) return false;
    // MealMaster: „T“ = Esslöffel, „t“ = Teelöffel – Groß-/Kleinschreibung zählt
    if (t == QLatin1String("T") || t == QLatin1String("t")) {
        const EnDef &d = englishDefs().value(t);
        *out = Unit{ QString::fromUtf8(d.de), d.kind, d.factor, true, {} };
        return true;
    }
    QString l = t.toLower();
    // RezKonv-Schreibweisen „Prise/n“, „Dose/n“, „Kugel(n)“, „Scheibe/n“
    static const QRegularExpression plural(QStringLiteral("(?:/n|/en|/e|\\(n\\)|\\(en\\)|\\(e\\))$"));
    if (l.size() > 4) l.remove(plural);
    auto g = germanAliases().constFind(l);
    if (g != germanAliases().constEnd()) {
        const Def &d = germanDefs().value(*g);
        *out = Unit{ *g, d.kind, d.base, false, {} };
        return true;
    }
    auto e = englishDefs().constFind(l);
    if (e != englishDefs().constEnd()) {
        *out = Unit{ QString::fromUtf8(e->de), e->kind, e->factor, true, QString::fromUtf8(e->note) };
        return true;
    }
    if (l == QLatin1String("fl.") || l == QLatin1String("fl oz")) {
        *out = Unit{ QStringLiteral("ml"), Kind::Volume, 29.57, true, {} };
        return true;
    }
    return false;
}

double mlPer(const QString &u)
{
    auto it = germanDefs().constFind(u);
    return it != germanDefs().constEnd() && it->kind == Kind::Volume ? it->base : 0.0;
}

double gramsPer(const QString &u)
{
    auto it = germanDefs().constFind(u);
    return it != germanDefs().constEnd() && it->kind == Kind::Mass ? it->base : 0.0;
}

Kind kindOf(const QString &u)
{
    if (u.isEmpty()) return Kind::Piece;
    auto it = germanDefs().constFind(u);
    return it == germanDefs().constEnd() ? Kind::None : it->kind;
}

double defaultPieceGrams(const QString &u)
{
    static const QHash<QString, double> d{
        { QStringLiteral("Dose"), 400 }, { QStringLiteral("Becher"), 200 }, { QStringLiteral("Glas"), 350 },
        { QStringLiteral("Pck."), 125 }, { QStringLiteral("Packung"), 250 }, { QStringLiteral("Bund"), 40 },
        { QStringLiteral("Zehe"), 4 }, { QStringLiteral("Scheibe"), 25 }, { QStringLiteral("Würfel"), 10 },
        { QStringLiteral("Blatt"), 1 }, { QStringLiteral("Zweig"), 1 }, { QStringLiteral("Stange"), 100 },
        { QStringLiteral("Handvoll"), 30 }, { QStringLiteral("Kopf"), 500 }, { QStringLiteral("Knolle"), 50 },
        { QStringLiteral("Tafel"), 100 }, { QStringLiteral("Flasche"), 700 }, { QStringLiteral("Beutel"), 125 },
        { QStringLiteral("Schale"), 250 }, { QStringLiteral("Kugel"), 50 },
    };
    return d.value(u, 0.0);
}

QStringList germanUnits()
{
    return { QStringLiteral("g"), QStringLiteral("kg"), QStringLiteral("ml"), QStringLiteral("l"), QStringLiteral("EL"),
             QStringLiteral("TL"), QStringLiteral("Prise"), QStringLiteral("Msp."), QStringLiteral("Tasse"),
             QStringLiteral("Stück"), QStringLiteral("Pck."), QStringLiteral("Dose"), QStringLiteral("Bund"),
             QStringLiteral("Zehe"), QStringLiteral("Scheibe"), QStringLiteral("Becher"), QStringLiteral("Glas"),
             QStringLiteral("Würfel"), QStringLiteral("Blatt"), QStringLiteral("Zweig"), QStringLiteral("Stange"),
             QStringLiteral("Spritzer"), QStringLiteral("Schuss"), QStringLiteral("Handvoll"), QStringLiteral("Kopf"),
             QStringLiteral("Knolle"), QStringLiteral("Tafel"), QStringLiteral("Flasche"), QStringLiteral("Beutel"),
             QStringLiteral("Kugel"), QStringLiteral("etwas"), QStringLiteral("wenig"), QStringLiteral("einige"), QStringLiteral("n. B.") };
}

QString convertTextMeasures(const QString &text)
{
    QString s = text;
    // Temperaturen: 350 F, 350°F, 350 degrees F, 350 degrees
    static const QRegularExpression temp(
        QStringLiteral("(\\d{3})\\s*(?:°|º|deg\\.?|degrees?)?\\s*F(?:ahrenheit)?\\b|(\\d{3})\\s*degrees\\b(?!\\s*C)"),
        QRegularExpression::CaseInsensitiveOption);
    QString out;
    int last = 0;
    auto it = temp.globalMatch(s);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString num = !m.captured(1).isEmpty() ? m.captured(1) : m.captured(2);
        const double f = num.toDouble();
        if (f < 200 || f > 550) continue;
        const int c = int(std::round((f - 32) * 5.0 / 9.0 / 5.0) * 5);
        out += s.mid(last, m.capturedStart() - last) + QStringLiteral("%1 °C").arg(c);
        last = m.capturedEnd();
    }
    out += s.mid(last);
    s = out;
    // Backformen: 9x13 inch, 9 x 13-inch
    static const QRegularExpression pan(QStringLiteral("(\\d+)\\s*[x×]\\s*(\\d+)[\\s-]*(?:inch(?:es)?|in\\.|\")"),
                                        QRegularExpression::CaseInsensitiveOption);
    out.clear();
    last = 0;
    it = pan.globalMatch(s);
    while (it.hasNext()) {
        const auto m = it.next();
        out += s.mid(last, m.capturedStart() - last)
               + QStringLiteral("%1 × %2 cm").arg(std::lround(m.captured(1).toDouble() * 2.54)).arg(std::lround(m.captured(2).toDouble() * 2.54));
        last = m.capturedEnd();
    }
    out += s.mid(last);
    s = out;
    // einzelne Zoll-Angaben: 1/2 inch, 2-inch
    static const QRegularExpression inch(QStringLiteral("(\\d+(?:\\.\\d+)?|\\d+/\\d+)[\\s-]*(?:inch(?:es)?)\\b"),
                                         QRegularExpression::CaseInsensitiveOption);
    out.clear();
    last = 0;
    it = inch.globalMatch(s);
    while (it.hasNext()) {
        const auto m = it.next();
        double v = 0;
        const QString n = m.captured(1);
        if (n.contains(QLatin1Char('/'))) {
            const QStringList p = n.split(QLatin1Char('/'));
            v = p[0].toDouble() / qMax(1.0, p[1].toDouble());
        } else {
            v = n.toDouble();
        }
        const double cm = v * 2.54;
        out += s.mid(last, m.capturedStart() - last)
               + QStringLiteral("%1 cm").arg(cm < 10 ? QString::number(std::round(cm * 2) / 2.0).replace(QLatin1Char('.'), QLatin1Char(','))
                                                     : QString::number(std::lround(cm)));
        last = m.capturedEnd();
    }
    out += s.mid(last);
    return out;
}

} // namespace Units
