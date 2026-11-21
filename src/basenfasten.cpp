// ViviaMeals – Tag „säure-basen-fasten“ (siehe basenfasten.h)
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "basenfasten.h"

#include "foodtable.h"
#include "nutrition.h"
#include "recipe.h"

#include <QObject>
#include <QRegularExpression>
#include <QSet>

namespace Basenfasten {

QString tagName()
{
    return QStringLiteral("säure-basen-fasten");
}

namespace {

QSet<QString> toSet(std::initializer_list<const char *> l)
{
    QSet<QString> s;
    for (const char *k : l) s.insert(QString::fromLatin1(k));
    return s;
}

// Lebensmittel der Nährwerttabelle, die auf der Liste der erlaubten stehen
const QSet<QString> &allowedKeys()
{
    static const QSet<QString> s = toSet({
        // Gemüse (frisch oder gedünstet), Pilze, Sprossen, Algen
        "zwiebel", "schalotte", "fruehlingszwiebel", "knoblauch", "lauch", "moehre", "sellerie", "staudensellerie", "paprika", "chili",
        "tomate", "kirschtomate", "gurke", "zucchini", "aubergine", "kuerbis", "brokkoli", "blumenkohl", "weisskohl", "rosenkohl",
        "gruenkohl", "spinat", "salat", "champignons", "mais", "spargel", "rote_bete", "radieschen", "fenchel", "kohlrabi", "pastinake",
        "suppengruen", "gemuesemix", "sprossen", "artischocke", "algen", "pimientos_padron", "trueffel",
        // Kartoffeln (ohne Fett), Süßkartoffeln
        "kartoffel", "suesskartoffel",
        // Obst, frisch und reif; Trockenobst in kleinen Mengen
        "avocado", "apfel", "birne", "banane", "orange", "zitrone", "zitronensaft", "erdbeere", "beeren", "kirschen", "pflaume",
        "aprikose", "ananas", "mango", "trauben", "kiwi", "melone", "wassermelone", "honigmelone", "pfirsich", "nektarine",
        "sauerkirschen", "mirabellen", "himbeeren", "heidelbeeren", "brombeeren", "johannisbeeren", "johannisbeeren_schwarz",
        "stachelbeeren", "mandarine", "grapefruit", "limette", "kumquat", "zitrusfrucht_sonst", "papaya", "maracuja", "kaki",
        "granatapfel", "feige", "litschi", "guave", "physalis", "sternfrucht", "pitahaya", "nashi", "quitte",
        "rosinen", "trockenobst", "datteln",
        // Kräuter und Gewürze (Salz sparsam)
        "kraeuter", "vanilleschote", "zitronenschale", "salz", "pfeffer", "gewuerze", "ingwer",
        // Nüsse und Samen (sparsam): Mandeln, Kürbiskerne, Sonnenblumenkerne, Leinsamen, Sesam, Chia; Mandelmus
        "mandeln", "samen", "tahin",
        // kaltgepresste Öle
        "olivenoel", "sesamoel",
        // Getränke
        "wasser",
    });
    return s;
}

// Gruppen, die ganz verboten sind
const QSet<QString> &forbiddenCategories()
{
    static const QSet<QString> s = toSet({ "fleisch", "fisch", "ei", "milch", "kaese", "getreide", "backwaren", "huelsen", "zucker",
                                           "honig", "suess", "gelatine", "alkohol" });
    return s;
}

// einzelne verbotene Lebensmittel aus sonst erlaubten Gruppen
const QSet<QString> &forbiddenKeys()
{
    static const QSet<QString> s = toSet({
        "haselnuesse", "walnuesse", "nuesse", "erdnuesse", "erdnussbutter",                        // säurebildende Nüsse
        "margarine", "schmalz", "butterschmalz", "kokosfett",                                       // Fette
        "kaffee", "limonade",                                                                       // Getränke
        "senf", "ketchup", "mayonnaise", "salatcreme", "sojasosse", "hoisinsauce", "sahnesosse",    // Fertigsaucen
        "bruehpulver", "glutamat", "wuerzpaste",                                                    // Geschmacksverstärker
        "tomaten_dose", "tomatenmark", "bambussprossen", "pimientos_piquillo",                      // Konserven
        "kartoffelpueree", "pommes", "patatas_bravas", "patatas_alioli", "papas_arrugadas",         // Fertig/Frittiertes
        "knochen", "garnelenpaste", "suessstoff", "speiseeis", "vanillezucker", "puddingpulver",
    });
    return s;
}

// Tierisches, Süßes, Milchprodukte und Fertigprodukte am Namen – auch wenn die Zutat einem erlaubten
// Lebensmittel zugeordnet ist („Hühnerbrühe“ → Brühe, „Zitronenjoghurt“ …)
bool forbiddenByName(const QString &text)
{
    static const QRegularExpression re(
        QStringLiteral("fleischbrühe|hühnerbrühe|rinderbrühe|kalbsbrühe|geflügelbrühe|knochenbrühe|fond\\b|brühwürfel|brühpulver|"
                       "instant|würzmischung|\\bfix\\b|tüte|\\bdose\\b|aus der dose|konserve|\\bglas\\b|tiefgekühlt|\\btk\\b|"
                       "gezuckert|kandiert|(?<!un)geschwefelt|(?<!un)gesüßt|sirup|zucker(?!schote|mais|melone)|honig(?!melone)|"
                       "sahne|butter(?!nut)|käse|joghurt|quark|(?<!mandel)milch|\\bei\\b|\\beier\\b|\\beiern\\b|eigelb|eiweiß|"
                       "frittiert|paniert|chips|ketchup|mayonnaise|\\bsenf\\b|sojasauce|sojasoße|essiggurke|gewürzgurke|eingelegt"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    return text.contains(re);
}

} // namespace

Verdict judge(const Ingredient &ing)
{
    if (ing.heading) return Verdict::Ignored;
    // Einheit mitprüfen: „1 Dose Tomaten“, „1 Glas Rote Bete“ sind Konserven
    const QString text = (ing.name + QLatin1Char(' ') + ing.note + QLatin1Char(' ') + ing.unit).trimmed();
    const QString lower = text.toLower();
    FoodTable &table = FoodTable::instance();
    const Food *f = ing.food.isEmpty() ? nullptr : table.find(ing.food);
    if (!f && FoodTable::isNonFood(ing.name)) return Verdict::Ignored;
    if (!f) f = table.match(ing.name);
    if (!f) return Verdict::Unknown;
    if (f->key == QLatin1String("wasser") && !lower.contains(QStringLiteral("kohlensäure")) && !lower.contains(QStringLiteral("sprudel")))
        return Verdict::Ignored;
    // Mandelmus und ungesüßte Mandelmilch sind erlaubt, auch wenn die Tabelle sie anders einordnet
    if (lower.contains(QStringLiteral("mandelmus")) || lower.contains(QStringLiteral("mandelmilch")) || lower.contains(QStringLiteral("mandeldrink")))
        return lower.contains(QStringLiteral("gesüßt")) && !lower.contains(QStringLiteral("ungesüßt")) ? Verdict::Forbidden : Verdict::Allowed;
    if (forbiddenCategories().contains(f->category) || forbiddenKeys().contains(f->key)) return Verdict::Forbidden;
    // ausdrücklich erlaubte Varianten in sonst gemischten Gruppen
    if (f->key == QLatin1String("oel")) {
        static const QRegularExpression cold(QStringLiteral("lein|hanf|walnuss|kürbiskern|oliven|sesam"), QRegularExpression::CaseInsensitiveOption);
        return text.contains(cold) ? Verdict::Allowed : Verdict::Unknown;
    }
    if (f->key == QLatin1String("bruehe"))
        return lower.contains(QStringLiteral("gemüse")) && !forbiddenByName(text.toLower().remove(QStringLiteral("gemüsebrühe"))) ? Verdict::Allowed
                                                                                                                               : Verdict::Forbidden;
    if (f->key == QLatin1String("essig")) return lower.contains(QStringLiteral("apfel")) ? Verdict::Allowed : Verdict::Forbidden;
    if (f->key == QLatin1String("orangensaft")) return lower.contains(QStringLiteral("frisch")) ? Verdict::Allowed : Verdict::Forbidden;
    if (f->key == QLatin1String("hefe") || f->key == QLatin1String("trockenhefe"))
        return lower.contains(QStringLiteral("flocken")) ? Verdict::Allowed : Verdict::Forbidden;
    if (!allowedKeys().contains(f->key)) return Verdict::Unknown;
    if (forbiddenByName(text)) return Verdict::Forbidden;
    return Verdict::Allowed;
}

bool qualifies(const Recipe &r, QStringList *reasons)
{
    if (Nutrition::isMenu(r)) return false;
    int allowed = 0;
    bool ok = true;
    for (const Ingredient &ing : r.ingredients) {
        const Verdict v = judge(ing);
        if (v == Verdict::Allowed) ++allowed;
        if (v == Verdict::Forbidden || v == Verdict::Unknown) {
            ok = false;
            if (!reasons) break;
            *reasons << (v == Verdict::Forbidden ? QObject::tr("%1: nicht erlaubt") : QObject::tr("%1: nicht auf der Liste")).arg(ing.name);
        }
    }
    return ok && allowed > 0;
}

int applyTag(Recipe &r)
{
    const QString tag = tagName();
    const bool want = qualifies(r);
    const bool has = r.hasTag(tag);
    if (want && !has) {
        r.addTag(tag);
        return 1;
    }
    if (!want && has) {
        r.tags.removeAll(tag);
        return 1;
    }
    return 0;
}

} // namespace Basenfasten
