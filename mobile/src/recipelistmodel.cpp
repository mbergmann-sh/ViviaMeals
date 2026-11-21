// ViviaMeals Mobil – Rezeptliste (siehe recipelistmodel.h)
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "recipelistmodel.h"

#include "recipedb.h"
#include "recipehtml.h"
#include "season.h"

#include <QCollator>
#include <QDate>
#include <algorithm>

namespace {

QString fold(QString s)
{
    s = s.toLower();
    s.replace(QStringLiteral("ä"), QStringLiteral("a")).replace(QStringLiteral("ö"), QStringLiteral("o")).replace(QStringLiteral("ü"), QStringLiteral("u"));
    s.replace(QStringLiteral("ß"), QStringLiteral("ss"));
    return s;
}

} // namespace

RecipeListModel::RecipeListModel(QObject *parent) : QAbstractListModel(parent) {}

int RecipeListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

int RecipeListModel::total() const
{
    return RecipeDb::instance().count();
}

QString RecipeListModel::perUnit(const QString &yieldUnit)
{
    const QString u = yieldUnit.trimmed();
    if (u.isEmpty()) return QStringLiteral("Portion");
    static const QList<QPair<QString, QString>> plural = {
        { QStringLiteral("Portionen"), QStringLiteral("Portion") }, { QStringLiteral("Stücke"), QStringLiteral("Stück") },
        { QStringLiteral("Scheiben"), QStringLiteral("Scheibe") },  { QStringLiteral("Personen"), QStringLiteral("Person") },
        { QStringLiteral("Gläser"), QStringLiteral("Glas") },       { QStringLiteral("Bleche"), QStringLiteral("Blech") },
        { QStringLiteral("Kekse"), QStringLiteral("Keks") },        { QStringLiteral("Muffins"), QStringLiteral("Muffin") },
        { QStringLiteral("Brötchen"), QStringLiteral("Brötchen") }, { QStringLiteral("servings"), QStringLiteral("Portion") },
    };
    for (const auto &p : plural)
        if (u.compare(p.first, Qt::CaseInsensitive) == 0) return p.second;
    return u;
}

QHash<int, QByteArray> RecipeListModel::roleNames() const
{
    return { { IdRole, "recipeId" },     { TitleRole, "title" },       { SubtitleRole, "subtitle" }, { KcalRole, "kcal" },
             { FavoriteRole, "favorite" }, { HasImageRole, "hasImage" }, { TagsRole, "tags" },       { LetterRole, "letter" } };
}

QVariant RecipeListModel::data(const QModelIndex &index, int role) const
{
    const auto &all = RecipeDb::instance().summaries();
    if (!index.isValid() || index.row() >= m_rows.size()) return {};
    const int i = m_rows.at(index.row());
    if (i < 0 || i >= all.size()) return {};
    const RecipeSummary &s = all.at(i);
    switch (role) {
    case IdRole: return s.id;
    case TitleRole: return s.title;
    case SubtitleRole: {
        QStringList parts;
        if (s.hasNutrition && s.perServing.kcal > 0)
            parts << QStringLiteral("%1 kcal je %2").arg(qRound(s.perServing.kcal)).arg(perUnit(s.yieldUnit));
        QStringList labels;
        for (const QString &t : s.tags) {
            if (labels.size() >= 3) break;
            labels << RecipeHtml::tagLabel(t);
        }
        if (!labels.isEmpty()) parts << labels.join(QStringLiteral(", "));
        return parts.join(QStringLiteral(" · "));
    }
    case KcalRole: return s.hasNutrition ? qRound(s.perServing.kcal) : 0;
    case FavoriteRole: return s.favorite;
    case HasImageRole: return s.hasImage;
    case TagsRole: return s.tags;
    case LetterRole: return s.title.isEmpty() ? QString() : fold(s.title.left(1)).toUpper();
    }
    return {};
}

void RecipeListModel::setQuery(const QString &q)
{
    if (q == m_query) return;
    m_query = q;
    emit filterChanged();
    refresh();
}

void RecipeListModel::setTheme(const QString &t)
{
    if (t == m_theme) return;
    m_theme = t;
    emit filterChanged();
    refresh();
}

QString RecipeListModel::idAt(int row) const
{
    const auto &all = RecipeDb::instance().summaries();
    if (row < 0 || row >= m_rows.size()) return {};
    const int i = m_rows.at(row);
    return i >= 0 && i < all.size() ? all.at(i).id : QString();
}

int RecipeListModel::indexOf(const QString &id) const
{
    const auto &all = RecipeDb::instance().summaries();
    for (int r = 0; r < m_rows.size(); ++r)
        if (all.at(m_rows.at(r)).id == id) return r;
    return -1;
}

void RecipeListModel::refresh()
{
    const auto &all = RecipeDb::instance().summaries();
    const QStringList words = fold(m_query).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QString season = Season::seasonTagFor(QDate::currentDate());
    QVector<int> rows;
    rows.reserve(all.size());
    for (int i = 0; i < all.size(); ++i) {
        const RecipeSummary &s = all.at(i);
        if (m_theme == QLatin1String(":fav")) {
            if (!s.favorite) continue;
        } else if (m_theme == QLatin1String(":season")) {
            if (!s.tags.contains(season)) continue;
        } else if (!m_theme.isEmpty() && !s.tags.contains(m_theme)) {
            continue;
        }
        if (!words.isEmpty()) {
            const QString hay = fold(s.title) + QLatin1Char(' ') + fold(s.ingredientNames.join(QLatin1Char(' '))) + QLatin1Char(' ')
                                + fold(s.tags.join(QLatin1Char(' ')));
            bool all_ = true;
            for (const QString &w : words)
                if (!hay.contains(w)) {
                    all_ = false;
                    break;
                }
            if (!all_) continue;
        }
        rows << i;
    }
    QCollator coll(QLocale(QLocale::German, QLocale::Germany));
    coll.setCaseSensitivity(Qt::CaseInsensitive);
    coll.setNumericMode(true);
    std::sort(rows.begin(), rows.end(), [&](int a, int b) { return coll.compare(all.at(a).title, all.at(b).title) < 0; });
    beginResetModel();
    m_rows = rows;
    endResetModel();
    emit countChanged();
}
