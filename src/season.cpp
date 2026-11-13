// ViviaMeals – Saison-Tags (siehe season.h)
// Saisonkalender: regionale Ware in Deutschland, Freiland bzw. Lager; grobe Richtwerte nach Monaten.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "season.h"

#include "foodtable.h"
#include "recipe.h"

#include <QHash>
#include <QRegularExpression>

namespace Season {

namespace {

// Monate a bis b (1–12), auch über den Jahreswechsel (z. B. 11 bis 2)
constexpr Months span(int a, int b)
{
    Months m = 0;
    for (int i = a;; i = i % 12 + 1) {
        m |= Months(1u << (i - 1));
        if (i == b) break;
    }
    return m;
}

struct NameRule {
    const char *pattern;
    Months months;   // AllYear = ganzjährig frisch
};

// Vor der Nährwerttabelle geprüft, weil genauer (Feldsalat ist Winter, Kopfsalat Sommer)
const QList<QPair<QRegularExpression, Months>> &nameRules()
{
    static const NameRule raw[] = {
        // Gemüse
        { "spargel", span(4, 6) },
        { "rhabarber", span(4, 6) },
        { "bärlauch|baerlauch|wild garlic", span(3, 5) },
        { "frühlingszwiebel|fruehlingszwiebel|lauchzwiebel|spring onion|scallion", span(4, 10) },
        { "radieschen|radish", span(4, 10) },
        { "rettich", span(5, 10) },
        { "kohlrabi", span(5, 10) },
        { "spitzkohl", span(5, 11) },
        { "rucola|rauke|arugula|rocket", span(5, 10) },
        { "feldsalat|rapunzel|lamb's lettuce", span(10, 3) },
        { "chicor[ée]e|chicory", span(10, 4) },
        { "endivie|endive", span(7, 11) },
        { "radicchio", span(7, 11) },
        { "kopfsalat|eisbergsalat|eissalat|lollo|romana|romaine|eichblatt|pflücksalat|blattsalat|lettuce", span(5, 10) },
        { "mangold|chard", span(6, 10) },
        { "spinat|spinach", span(3, 5) | span(9, 11) },
        { "zuckerschote|zuckererbse|kaiserschote|snow pea|sugar snap", span(6, 8) },
        { "grüne bohne|gruene bohne|brechbohne|stangenbohne|buschbohne|prinzessbohne|dicke bohne|saubohne|green bean|broad bean",
          span(7, 10) },
        { "brokkoli|broccoli", span(6, 10) },
        { "blumenkohl|cauliflower", span(6, 10) },
        { "rosenkohl|brussels sprout", span(10, 2) },
        { "grünkohl|gruenkohl|kale", span(11, 2) },
        { "zucchini|courgette", span(6, 10) },
        { "gurke|cucumber", span(6, 9) },
        { "aubergine|eggplant", span(7, 10) },
        { "paprika|bell pepper|peperoni|chili", span(7, 10) },
        { "tomate|tomato", span(7, 10) },
        { "zuckermais|maiskolben|sweet corn|corn on the cob", span(8, 10) },
        { "fenchel|fennel", span(6, 11) },
        { "staudensellerie|stangensellerie|bleichsellerie|celery stalk|celery stick", span(7, 10) },
        { "kürbis|kuerbis|hokkaido|butternut|pumpkin|squash", span(8, 12) },
        { "pastinake|parsnip", span(10, 3) },
        { "schwarzwurzel|salsify", span(10, 3) },
        { "topinambur|jerusalem artichoke", span(10, 3) },
        { "steckrübe|steckruebe|kohlrübe|swede|rutabaga", span(10, 3) },
        { "petersilienwurzel|parsley root", span(10, 3) },
        { "pfifferling|chanterelle", span(7, 10) },
        { "steinpilz|porcini|wildpilz|waldpilz", span(8, 10) },
        { "maronen?\\b|marone|esskastanie|edelkastanie|chestnut", span(10, 12) },
        // Obst
        { "erdbeer|strawberr", span(5, 8) },
        { "kirsche|kirschen|cherry|cherries", span(6, 8) },
        { "johannisbeer|stachelbeer|himbeer|brombeer|heidelbeer|blaubeer|preiselbeer|beeren|currant|gooseberr|raspberr|"
          "blackberr|blueberr|berries",
          span(6, 9) },
        { "holunder|elderberr|elderflower", span(6, 9) },
        { "aprikose|marille|apricot", span(6, 8) },
        { "pfirsich|nektarine|peach|nectarine", span(7, 9) },
        { "mirabelle|reneklode", span(8, 9) },
        { "pflaume|zwetsch|plum", span(7, 10) },
        { "orange|apfelsine|mandarine|clementine|grapefruit|pomelo|tangerine", span(11, 3) },
        { "weintraube|trauben|grape", span(8, 10) },
        { "birne|pear", span(8, 1) },
        { "quitte|quince", span(9, 11) },
        { "melone|melon", span(6, 9) },
        // Fleisch mit Saison
        { "\\bwild(?!reis|kräuter|kraeuter|kräutern)|\\breh|hirsch|wildschwein|fasan|venison", span(10, 1) },
        { "\\bgans\\b|gänse|gaense|goose", span(11, 12) },
        // ganzjährig (Lager, Zucht) – zuletzt, damit z. B. „Frühlingszwiebel“ oder „Apfelsine“ vorher greifen
        { "kartoffel|potato|zwiebel|onion|schalotte|shallot|knoblauch|garlic|möhre|moehre|karotte|mohrrübe|carrot|"
          "knollensellerie|sellerie|celeriac|lauch|porree|leek|rote bete|rote beete|rote rübe|beetroot|weißkohl|weisskohl|"
          "rotkohl|blaukraut|wirsing|cabbage|champignon|mushroom|apfel|äpfel|apple|ingwer|ginger",
          AllYear },
    };
    static const QList<QPair<QRegularExpression, Months>> rules = [] {
        QList<QPair<QRegularExpression, Months>> out;
        for (const NameRule &r : raw)
            out << qMakePair(QRegularExpression(QString::fromUtf8(r.pattern),
                                                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption),
                             r.months);
        return out;
    }();
    return rules;
}

// keine frische Ware: Tiefkühlware, Konserven, Getrocknetes, Verarbeitetes
const QRegularExpression &notFresh()
{
    static const QRegularExpression re(
        QStringLiteral("\\btk\\b|tiefgekühlt|tiefgekuehlt|tiefgefroren|gefroren|\\bdose\\b|dosen|konserve|aus dem glas|\\bglas\\b|getrocknet|"
                       "eingelegt|eingemacht|kompott|marmelade|konfitüre|gelee|saft|nektar|sirup|likör|essig|mus\\b|püree|pueree|pulver|"
                       "mehl|stärke|grieß|gries|flocken|chips|mark\\b|ketchup|passiert|stückig|polpa|sugo|soße|sosse|sauce|pesto|"
                       "kerne\\b|kernen\\b|kernöl|samen|aroma|extrakt|gewürzgurke|essiggurke|cornichon|salzgurke|sauerkraut|rotkohl aus|apfelmus|rosinen|"
                       "frozen|canned|tinned|dried|jarred|juice|powder|paste|"
                       "wildreis|kirschwasser|paprikapulver|edelsüß|rosenscharf|chiliflocken|chilisauce|chilischote, getrocknet"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    return re;
}

// Nährwerttabelle → Saison, wenn der Name keine Regel trifft (nur frische Ware)
Months monthsForFood(const QString &key)
{
    static const QHash<QString, Months> table = {
        { QStringLiteral("spargel"), span(4, 6) },
        { QStringLiteral("rhabarber"), span(4, 6) },
        { QStringLiteral("fruehlingszwiebel"), span(4, 10) },
        { QStringLiteral("radieschen"), span(4, 10) },
        { QStringLiteral("kohlrabi"), span(5, 10) },
        { QStringLiteral("salat"), span(5, 10) },
        { QStringLiteral("spinat"), span(3, 5) | span(9, 11) },
        { QStringLiteral("gruene_bohnen"), span(7, 10) },
        { QStringLiteral("brokkoli"), span(6, 10) },
        { QStringLiteral("blumenkohl"), span(6, 10) },
        { QStringLiteral("rosenkohl"), span(10, 2) },
        { QStringLiteral("gruenkohl"), span(11, 2) },
        { QStringLiteral("zucchini"), span(6, 10) },
        { QStringLiteral("gurke"), span(6, 9) },
        { QStringLiteral("aubergine"), span(7, 10) },
        { QStringLiteral("paprika"), span(7, 10) },
        { QStringLiteral("chili"), span(7, 10) },
        { QStringLiteral("tomate"), span(7, 10) },
        { QStringLiteral("kirschtomate"), span(7, 10) },
        { QStringLiteral("mais"), span(8, 10) },
        { QStringLiteral("fenchel"), span(6, 11) },
        { QStringLiteral("staudensellerie"), span(7, 10) },
        { QStringLiteral("kuerbis"), span(8, 12) },
        { QStringLiteral("pastinake"), span(10, 3) },
        { QStringLiteral("maronen"), span(10, 12) },
        { QStringLiteral("erdbeere"), span(5, 8) },
        { QStringLiteral("kirschen"), span(6, 8) },
        { QStringLiteral("beeren"), span(6, 9) },
        { QStringLiteral("aprikose"), span(6, 8) },
        { QStringLiteral("pflaume"), span(7, 10) },
        { QStringLiteral("trauben"), span(8, 10) },
        { QStringLiteral("birne"), span(8, 1) },
        { QStringLiteral("melone"), span(6, 9) },
        { QStringLiteral("orange"), span(11, 3) },
        { QStringLiteral("wild"), span(10, 1) },
        // ganzjährig frisch (Lager, Zucht, Einfuhr ohne ausgeprägte Saison)
        { QStringLiteral("zwiebel"), AllYear },
        { QStringLiteral("schalotte"), AllYear },
        { QStringLiteral("knoblauch"), AllYear },
        { QStringLiteral("lauch"), AllYear },
        { QStringLiteral("moehre"), AllYear },
        { QStringLiteral("sellerie"), AllYear },
        { QStringLiteral("weisskohl"), AllYear },
        { QStringLiteral("rote_bete"), AllYear },
        { QStringLiteral("champignons"), AllYear },
        { QStringLiteral("suppengruen"), AllYear },
        { QStringLiteral("kartoffel"), AllYear },
        { QStringLiteral("suesskartoffel"), AllYear },
        { QStringLiteral("apfel"), AllYear },
        { QStringLiteral("zitrone"), AllYear },
        { QStringLiteral("banane"), AllYear },
        { QStringLiteral("avocado"), AllYear },
        { QStringLiteral("ananas"), AllYear },
        { QStringLiteral("mango"), AllYear },
        { QStringLiteral("kiwi"), AllYear },
        { QStringLiteral("ingwer"), AllYear },
    };
    return table.value(key, 0);
}

double weightOf(const Ingredient &ing)
{
    return ing.grams > 0 ? ing.grams : 50.0;   // unbekannte Menge: kleine Portion annehmen
}

Months tagMonths(int i)
{
    switch (i) {
    case 0: return span(3, 5);    // Frühling
    case 1: return span(6, 8);    // Sommer
    case 2: return span(9, 11);   // Herbst
    default: return span(12, 2);  // Winter
    }
}

int bits(Months m)
{
    int n = 0;
    for (int i = 0; i < 12; ++i)
        if (m & (1u << i)) ++n;
    return n;
}

} // namespace

Months monthsFor(const Ingredient &ing)
{
    if (ing.heading || ing.name.trimmed().isEmpty()) return 0;
    const QString text = ing.name + QLatin1Char(' ') + ing.note;
    if (text.contains(notFresh())) return 0;
    for (const auto &rule : nameRules())
        if (ing.name.contains(rule.first)) return rule.second;
    const FoodTable &table = FoodTable::instance();
    const Food *f = ing.food.isEmpty() ? table.match(ing.name) : table.find(ing.food);
    return f ? monthsForFood(f->key) : 0;
}

Months recipeMonths(const Recipe &r, bool *hasFresh)
{
    double seasonal = 0, fresh = 0, total = 0;
    double perMonth[12] = {};
    for (const Ingredient &ing : r.ingredients) {
        if (ing.heading) continue;
        const double w = weightOf(ing);
        total += w;
        const Months m = monthsFor(ing);
        if (!m) continue;
        fresh += w;
        if (m == AllYear) continue;
        seasonal += w;
        for (int i = 0; i < 12; ++i)
            if (m & (1u << i)) perMonth[i] += w;
    }
    // „frisch“ nur, wenn frische Zutaten einen nennenswerten Anteil haben (nicht nur eine Zitrone im Kuchen)
    if (hasFresh) *hasFresh = total > 0 && fresh / total >= 0.15;
    if (seasonal <= 0) return 0;
    Months out = 0;
    for (int i = 0; i < 12; ++i)
        if (perMonth[i] >= 0.8 * seasonal) out |= Months(1u << i);
    return out;
}

QStringList tagNames()
{
    return { QStringLiteral("frühling"), QStringLiteral("sommer"), QStringLiteral("herbst"), QStringLiteral("winter"), QStringLiteral("ganzjährig") };
}

QString seasonTagFor(const QDate &date)
{
    const int m = date.isValid() ? date.month() : QDate::currentDate().month();
    const QStringList names = tagNames();
    if (m >= 3 && m <= 5) return names.at(0);
    if (m >= 6 && m <= 8) return names.at(1);
    if (m >= 9 && m <= 11) return names.at(2);
    return names.at(3);
}

QStringList suggestTags(const Recipe &r)
{
    bool fresh = false;
    const Months m = recipeMonths(r, &fresh);
    const QStringList names = tagNames();
    QStringList out;
    if (m) {
        for (int i = 0; i < 4; ++i)
            if (bits(m & tagMonths(i)) >= 2) out << names.at(i);
    } else if (fresh) {
        bool anySeasonal = false;   // Saisonzutaten ohne gemeinsame Saison → kein Tag
        for (const Ingredient &ing : r.ingredients) {
            const Months im = monthsFor(ing);
            if (im && im != AllYear) anySeasonal = true;
        }
        if (!anySeasonal) out << names.at(4);
    }
    return out;
}

int applyTags(Recipe &r)
{
    const QStringList want = suggestTags(r);
    int changes = 0;
    for (const QString &t : tagNames()) {
        const bool has = r.hasTag(t);
        if (want.contains(t) && !has) {
            r.addTag(t);
            ++changes;
        } else if (!want.contains(t) && has) {
            r.tags.removeAll(t);
            ++changes;
        }
    }
    return changes;
}

QString monthsText(Months m)
{
    static const char *const names[] = { "Jan.", "Feb.", "März", "Apr.", "Mai", "Juni", "Juli", "Aug.", "Sep.", "Okt.", "Nov.", "Dez." };
    if (m == AllYear) return QStringLiteral("ganzjährig");
    if (!m) return QString();
    // zusammenhängende Bereiche (auch über den Jahreswechsel)
    int start = 0;
    while (start < 12 && (m & (1u << start)) && (m & (1u << ((start + 11) % 12)))) ++start;   // Beginn eines Bereichs suchen
    QStringList parts;
    for (int k = 0; k < 12; ++k) {
        const int i = (start + k) % 12;
        if (!(m & (1u << i)) || (m & (1u << ((i + 11) % 12)))) continue;
        int j = i;
        while ((m & (1u << ((j + 1) % 12))) && (j + 1) % 12 != i) j = (j + 1) % 12;
        parts << (i == j ? QString::fromUtf8(names[i]) : QString::fromUtf8(names[i]) + QStringLiteral("–") + QString::fromUtf8(names[j]));
    }
    return parts.join(QStringLiteral(", "));
}

} // namespace Season
