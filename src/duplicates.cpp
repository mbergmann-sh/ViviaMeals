// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "duplicates.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

#include <algorithm>
#include <climits>
#include <functional>

namespace Duplicates {

namespace {

QString tr(const char *s) { return QCoreApplication::translate("Duplicates", s); }

QString ignoreFile()
{
    const QString f = RecipeDb::instance().folder();
    return f.isEmpty() ? QString() : QDir(f).filePath(QStringLiteral("doubletten-ignoriert.json"));
}

int levenshtein(const QString &a, const QString &b)
{
    const int n = int(a.size()), m = int(b.size());
    if (n == 0) return m;
    if (m == 0) return n;
    QVector<int> prev(m + 1), cur(m + 1);
    for (int j = 0; j <= m; ++j) prev[j] = j;
    for (int i = 1; i <= n; ++i) {
        cur[0] = i;
        for (int j = 1; j <= m; ++j)
            cur[j] = std::min({ prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a.at(i - 1) == b.at(j - 1) ? 0 : 1) });
        std::swap(prev, cur);
    }
    return prev[m];
}

// Füllwörter, die über die Gleichheit nichts aussagen
const QSet<QString> &stopWords()
{
    static const QSet<QString> s{ QStringLiteral("mit"), QStringLiteral("und"), QStringLiteral("an"), QStringLiteral("auf"),
                                  QStringLiteral("aus"), QStringLiteral("in"), QStringLiteral("im"), QStringLiteral("vom"),
                                  QStringLiteral("von"), QStringLiteral("der"), QStringLiteral("die"), QStringLiteral("das"),
                                  QStringLiteral("den"), QStringLiteral("dem"), QStringLiteral("nach"), QStringLiteral("art"),
                                  QStringLiteral("a"), QStringLiteral("la"), QStringLiteral("with"), QStringLiteral("and"),
                                  QStringLiteral("the"), QStringLiteral("of"), QStringLiteral("omas"), QStringLiteral("mamas"),
                                  QStringLiteral("einfache"), QStringLiteral("einfacher"), QStringLiteral("schnelle"),
                                  QStringLiteral("schneller"), QStringLiteral("leckere"), QStringLiteral("leckerer") };
    return s;
}

QSet<QString> tokens(const QString &key)
{
    QSet<QString> t;
    for (const QString &w : key.split(QLatin1Char(' '), Qt::SkipEmptyParts))
        if (w.size() >= 3 && !stopWords().contains(w)) t.insert(w);
    return t;
}

QSet<QString> ingredientSet(const RecipeSummary &s)
{
    QSet<QString> set;
    for (const QString &f : s.foods)
        if (f != QLatin1String("salz") && f != QLatin1String("pfeffer") && f != QLatin1String("wasser")) set.insert(f);
    if (set.size() < 2)   // ohne Zuordnung: Zutatennamen
        for (const QString &n : s.ingredientNames) set.insert(titleKey(n));
    return set;
}

double jaccard(const QSet<QString> &a, const QSet<QString> &b)
{
    if (a.isEmpty() || b.isEmpty()) return 0;
    int inter = 0;
    for (const QString &x : a)
        if (b.contains(x)) ++inter;
    return double(inter) / double(a.size() + b.size() - inter);
}

} // namespace

QString titleKey(const QString &title)
{
    QString s = title.toLower();
    s.replace(QStringLiteral("ä"), QStringLiteral("a")).replace(QStringLiteral("ö"), QStringLiteral("o"))
        .replace(QStringLiteral("ü"), QStringLiteral("u")).replace(QStringLiteral("ß"), QStringLiteral("ss"));
    s.replace(QStringLiteral("ae"), QStringLiteral("a")).replace(QStringLiteral("oe"), QStringLiteral("o"))
        .replace(QStringLiteral("ue"), QStringLiteral("u"));
    // Akzente (Crème, Purée) entfernen
    s = s.normalized(QString::NormalizationForm_D);
    s.remove(QRegularExpression(QStringLiteral("\\p{Mn}")));
    s.replace(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]+")), QStringLiteral(" "));
    return s.simplified();
}

namespace {

// Wörter gleich oder mit Tippfehler (höchstens etwa jeder achte Buchstabe anders)
bool sameWord(const QString &a, const QString &b)
{
    if (a == b) return true;
    const int len = int(qMax(a.size(), b.size()));
    if (len < 5 || std::abs(int(a.size()) - int(b.size())) > 2) return false;
    return levenshtein(a, b) * 8 <= len;
}

double tokenSimilarity(const QString &ka, const QString &kb, const QSet<QString> &ta, const QSet<QString> &tb)
{
    if (ka.isEmpty() || kb.isEmpty()) return 0;
    if (ka == kb) return 1;
    // ohne bedeutungstragende Wörter: ganze Titel vergleichen
    if (ta.isEmpty() || tb.isEmpty()) return sameWord(ka, kb) ? 0.95 : 0;
    // Wortweise mit Tippfehler-Toleranz; zusammengesetzte Wörter wie „Apfelkuchen“/„Nudelkuchen“ bleiben verschieden
    int matched = 0;
    QSet<QString> used;
    for (const QString &x : ta) {
        if (tb.contains(x) && !used.contains(x)) {
            used.insert(x);
            ++matched;
            continue;
        }
        for (const QString &y : tb)
            if (!used.contains(y) && sameWord(x, y)) {
                used.insert(y);
                ++matched;
                break;
            }
    }
    return 0.7 * double(matched) / double(qMin(ta.size(), tb.size())) + 0.3 * double(matched) / double(qMax(ta.size(), tb.size()));
}

} // namespace

double titleSimilarity(const QString &a, const QString &b)
{
    const QString ka = titleKey(a), kb = titleKey(b);
    return tokenSimilarity(ka, kb, tokens(ka), tokens(kb));
}

double ingredientSimilarity(const RecipeSummary &a, const RecipeSummary &b)
{
    return jaccard(ingredientSet(a), ingredientSet(b));
}

QString reasonText(int reasons)
{
    QStringList l;
    if (reasons & SameTitle) l << tr("gleicher Titel");
    if (reasons & SimilarTitle) l << tr("ähnlicher Titel");
    if (reasons & SameIngredients) l << tr("gleiche Zutaten");
    return l.join(QStringLiteral(", "));
}

int completeness(const RecipeSummary &s)
{
    int c = 0;
    if (s.hasNutrition) c += 4 + int(s.coverage * 4);
    if (s.hasImage) c += 3;
    if (s.favorite) c += 3;
    c += qMin(15, int(s.ingredientNames.size()));
    if (s.language == QLatin1String("de")) c += 1;
    return c;
}

QList<Group> find(const QList<RecipeSummary> &recipes, const QSet<QString> &ignoredPairs, Strictness strictness)
{
    const int n = int(recipes.size());
    QVector<QString> keys(n);
    QVector<QSet<QString>> ings(n), toks(n);
    QHash<QString, QList<int>> byKey, byToken, byFood;
    for (int i = 0; i < n; ++i) {
        keys[i] = titleKey(recipes[i].title);
        ings[i] = ingredientSet(recipes[i]);
        byKey[keys[i]] << i;
        toks[i] = tokens(keys[i]);
        for (const QString &t : toks[i]) byToken[t] << i;
        for (const QString &f : ings[i]) byFood[f] << i;
    }
    // Union-Find
    QVector<int> parent(n);
    for (int i = 0; i < n; ++i) parent[i] = i;
    std::function<int(int)> root = [&](int x) { return parent[x] == x ? x : parent[x] = root(parent[x]); };
    QHash<int, int> reasons;          // Wurzel → Gründe (nach dem Zusammenfassen neu berechnet)
    struct Hit { int a, b, reasons; double score; };
    QList<Hit> hits;
    QSet<qint64> seen;
    auto consider = [&](int i, int j) {
        if (i == j) return;
        const int a = qMin(i, j), b = qMax(i, j);
        const qint64 code = qint64(a) * n + b;
        if (seen.contains(code)) return;
        seen.insert(code);
        if (ignoredPairs.contains(pairKey(recipes[a].id, recipes[b].id))) return;
        int why = 0;
        double score = 0;
        if (keys[a] == keys[b]) {
            why |= SameTitle;
            score = 1;
        } else if (strictness != Strictness::TitleOnly) {
            const bool generous = strictness == Strictness::Generous;
            const double is = jaccard(ings[a], ings[b]);
            const bool noIngredients = ings[a].isEmpty() || ings[b].isEmpty();
            // schneller Ausschluss: Zutaten zu verschieden für jede Regel
            if (!noIngredients && is < (generous ? 0.35 : 0.5)) return;
            const double ts = tokenSimilarity(keys[a], keys[b], toks[a], toks[b]);
            if ((ts >= (generous ? 0.80 : 0.88) && (is >= (generous ? 0.35 : 0.5) || noIngredients))
                || (ts >= (generous ? 0.6 : 0.7) && is >= (generous ? 0.7 : 0.8))) {
                why |= SimilarTitle;
                score = qMax(score, 0.6 * ts + 0.4 * is);
            }
            if (is >= (generous ? 0.85 : 0.92) && ings[a].size() >= 5 && ings[b].size() >= 5) {
                why |= SameIngredients;
                score = qMax(score, is * 0.95);
            }
        }
        if (!why) return;
        hits << Hit{ a, b, why, score };
    };
    for (auto it = byKey.constBegin(); it != byKey.constEnd(); ++it)
        for (int x = 0; x < it.value().size(); ++x)
            for (int y = x + 1; y < it.value().size(); ++y) consider(it.value()[x], it.value()[y]);
    if (strictness != Strictness::TitleOnly) {
        // Kandidaten: gemeinsames, nicht zu häufiges Titelwort bzw. gemeinsames seltenes Lebensmittel
        for (auto it = byToken.constBegin(); it != byToken.constEnd(); ++it) {
            const QList<int> &l = it.value();
            if (l.size() < 2 || l.size() > 300) continue;
            for (int x = 0; x < l.size(); ++x)
                for (int y = x + 1; y < l.size(); ++y) consider(l[x], l[y]);
        }
        for (int i = 0; i < n; ++i) {
            if (ings[i].size() < 5) continue;
            // seltenstes Lebensmittel des Rezepts als Schlüssel
            QString rare;
            int rareCount = INT_MAX;
            for (const QString &f : ings[i])
                if (byFood[f].size() < rareCount) {
                    rareCount = int(byFood[f].size());
                    rare = f;
                }
            if (rareCount > 400) continue;
            for (int j : byFood[rare])
                if (j > i && ings[j].size() >= 5) consider(i, j);
        }
    }
    for (const Hit &h : hits) parent[root(h.a)] = root(h.b);
    QHash<int, Group> groups;
    for (const Hit &h : hits) {
        Group &g = groups[root(h.a)];
        g.reasons |= h.reasons;
        g.score = qMax(g.score, h.score);
    }
    for (int i = 0; i < n; ++i) {
        const int r = root(i);
        if (groups.contains(r)) groups[r].ids << recipes[i].id;
    }
    QList<Group> out;
    QHash<QString, int> idx;
    for (int i = 0; i < n; ++i) idx.insert(recipes[i].id, i);
    for (Group g : groups) {
        if (g.ids.size() < 2) continue;
        std::sort(g.ids.begin(), g.ids.end(), [&](const QString &x, const QString &y) {
            const RecipeSummary &a = recipes[idx.value(x)], &b = recipes[idx.value(y)];
            const int ca = completeness(a), cb = completeness(b);
            if (ca != cb) return ca > cb;
            return a.modified > b.modified;
        });
        out << g;
    }
    std::sort(out.begin(), out.end(), [&](const Group &a, const Group &b) {
        if (a.score != b.score) return a.score > b.score;
        return recipes[idx.value(a.ids.first())].title.localeAwareCompare(recipes[idx.value(b.ids.first())].title) < 0;
    });
    return out;
}

QString pairKey(const QString &a, const QString &b)
{
    return a < b ? a + QLatin1Char('|') + b : b + QLatin1Char('|') + a;
}

QSet<QString> loadIgnored()
{
    QSet<QString> s;
    QFile f(ignoreFile());
    if (!f.open(QIODevice::ReadOnly)) return s;
    for (const auto &v : QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("pairs")).toArray()) s.insert(v.toString());
    return s;
}

bool saveIgnored(const QSet<QString> &pairs)
{
    const QString file = ignoreFile();
    if (file.isEmpty()) return false;
    QStringList l(pairs.begin(), pairs.end());
    l.sort();
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(QJsonObject{ { QStringLiteral("format"), QStringLiteral("ViviaMeals-IgnoredDuplicates") },
                                       { QStringLiteral("pairs"), QJsonArray::fromStringList(l) } })
                .toJson());
    return f.commit();
}

QStringList mergeInto(Recipe &keep, const QList<Recipe> &others)
{
    QStringList done;
    for (const Recipe &o : others) {
        if (keep.image.isEmpty() && !o.image.isEmpty()) {
            keep.image = o.image;
            done << tr("Bild aus „%1“").arg(o.title);
        }
        if (keep.source.trimmed().isEmpty() && !o.source.trimmed().isEmpty()) {
            keep.source = o.source;
            done << tr("Quelle „%1“").arg(o.source);
        }
        if (keep.prepMinutes <= 0 && o.prepMinutes > 0) {
            keep.prepMinutes = o.prepMinutes;
            done << tr("Zubereitungszeit %1 min").arg(o.prepMinutes);
        }
        if (keep.instructions.trimmed().isEmpty() && !o.instructions.trimmed().isEmpty()) {
            keep.instructions = o.instructions;
            done << tr("Zubereitung aus „%1“").arg(o.title);
        }
        if (keep.ingredients.isEmpty() && !o.ingredients.isEmpty()) {
            keep.ingredients = o.ingredients;
            done << tr("Zutaten aus „%1“").arg(o.title);
        }
        QStringList newTags;
        for (const QString &t : o.tags)
            if (!keep.hasTag(t)) {
                keep.addTag(t);
                newTags << t;
            }
        if (!newTags.isEmpty()) done << tr("Tags: %1").arg(newTags.join(QStringLiteral(", ")));
        if (o.favorite && !keep.favorite) {
            keep.favorite = true;
            done << tr("Favorit");
        }
    }
    return done;
}

} // namespace Duplicates
