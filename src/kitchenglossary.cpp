// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "kitchenglossary.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace KitchenGlossary {

namespace {

QStringList toList(const QJsonValue &v)
{
    QStringList out;
    for (const QJsonValue &e : v.toArray()) out << e.toString();
    return out;
}

struct Data {
    QList<Entry> list;
    QStringList cats;
    QHash<QString, int> byName;   // gefaltete Begriffe und Schreibweisen
    QList<QString> foldedTerm, foldedSyn, foldedText;
};

const Data &data()
{
    static const Data d = [] {
        Data d;
        QFile f(QStringLiteral(":/data/kuechenglossar.json"));
        if (!f.open(QIODevice::ReadOnly)) return d;
        const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("eintraege")).toArray();
        for (const QJsonValue &v : arr) {
            const QJsonObject o = v.toObject();
            Entry e;
            e.term = o.value(QStringLiteral("begriff")).toString();
            e.category = o.value(QStringLiteral("kategorie")).toString();
            e.text = o.value(QStringLiteral("text")).toString();
            e.synonyms = toList(o.value(QStringLiteral("synonyme")));
            e.seeAlso = toList(o.value(QStringLiteral("siehe")));
            if (e.term.isEmpty()) continue;
            d.list << e;
        }
        std::sort(d.list.begin(), d.list.end(), [](const Entry &a, const Entry &b) {
            return fold(a.term).localeAwareCompare(fold(b.term)) < 0;
        });
        // feste Reihenfolge der Kategorien (vom Garen über Saucen bis zum Service)
        static const char *order[] = { "Garverfahren", "Schneiden & Formen", "Techniken & Vorbereitung", "Fonds, Saucen & Buttermischungen",
                                       "Suppen", "Gerichte & Zubereitungen", "Garnituren & Beilagen", "Fleisch, Geflügel & Fisch",
                                       "Pâtisserie, Teige & Massen", "Zutaten & Würzen", "Geräte & Utensilien",
                                       "Küchenbrigade & Organisation", "Menü & Service" };
        for (const char *c : order) d.cats << QString::fromUtf8(c);
        for (int i = 0; i < d.list.size(); ++i) {
            const Entry &e = d.list.at(i);
            if (!d.cats.contains(e.category)) d.cats << e.category;
            d.foldedTerm << fold(e.term);
            d.foldedSyn << fold(e.synonyms.join(QLatin1Char('\n')));
            d.foldedText << fold(e.text);
            d.byName.insert(fold(e.term), i);
        }
        for (int i = 0; i < d.list.size(); ++i)
            for (const QString &s : d.list.at(i).synonyms)
                if (!d.byName.contains(fold(s))) d.byName.insert(fold(s), i);
        return d;
    }();
    return d;
}

} // namespace

QString fold(const QString &s)
{
    QString out;
    const QString n = s.toLower().normalized(QString::NormalizationForm_D);
    out.reserve(n.size());
    for (const QChar c : n) {
        if (c.category() == QChar::Mark_NonSpacing) continue;   // Akzente
        if (c == QChar(0x00DF)) out += QStringLiteral("ss");
        else if (c == QChar(0x0153)) out += QStringLiteral("oe");   // œ
        else if (c == QChar(0x00E6)) out += QStringLiteral("ae");   // æ
        else out += c;
    }
    return out.simplified();
}

const QList<Entry> &entries()
{
    return data().list;
}

QStringList categories()
{
    return data().cats;
}

int indexOf(const QString &term)
{
    return data().byName.value(fold(term), -1);
}

QList<int> search(const QString &query, const QString &category)
{
    const Data &d = data();
    const QStringList words = fold(query).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QList<QPair<int, int>> scored;   // (Rang, Index)
    for (int i = 0; i < d.list.size(); ++i) {
        if (!category.isEmpty() && d.list.at(i).category != category) continue;
        if (words.isEmpty()) {
            scored << qMakePair(0, i);
            continue;
        }
        int rank = 0;
        bool all = true;
        for (const QString &w : words) {
            const QString &t = d.foldedTerm.at(i);
            int r;
            if (t == w) r = 0;
            else if (t.startsWith(w)) r = 1;
            else if (t.contains(w)) r = 2;
            else if (d.foldedSyn.at(i).contains(w)) r = 3;
            else if (d.foldedText.at(i).contains(w)) r = 5;
            else {
                all = false;
                break;
            }
            rank += r;
        }
        if (all) scored << qMakePair(rank, i);
    }
    std::stable_sort(scored.begin(), scored.end(), [](const QPair<int, int> &a, const QPair<int, int> &b) { return a.first < b.first; });
    QList<int> out;
    for (const auto &p : scored) out << p.second;
    return out;
}

} // namespace KitchenGlossary
