// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "diary.h"
#include "mealtypes.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

namespace Diary {

namespace {
QString tr(const char *s) { return QCoreApplication::translate("Diary", s); }
}

QStringList mealNames()
{
    return { tr("Frühstück"), tr("Zwischenmahlzeit vormittags"), tr("Mittagessen"), tr("Zwischenmahlzeit nachmittags"),
             tr("Abendessen"), tr("Spätmahlzeit"), tr("Getränk") };
}

QString mealForTime(const QTime &t)
{
    const QStringList m = mealNames();
    const int min = t.hour() * 60 + t.minute();
    if (min < 10 * 60) return m[0];
    if (min < 11 * 60 + 30) return m[1];
    if (min < 14 * 60 + 30) return m[2];
    if (min < 17 * 60 + 30) return m[3];
    if (min < 21 * 60) return m[4];
    return m[5];
}

QTime defaultTime(const QString &meal)
{
    const QStringList m = mealNames();
    static const QTime times[] = { QTime(7, 30), QTime(10, 0), QTime(12, 30), QTime(15, 30), QTime(18, 30), QTime(21, 30), QTime(12, 0) };
    const int i = int(m.indexOf(meal));
    return i >= 0 ? times[i] : QTime::currentTime();
}

// ---------------------------------------------------------------- Entry

Nutrients Entry::eaten() const
{
    Nutrients n = perServing;
    const double f = portions;
    n.kcal *= f;
    n.protein *= f;
    n.fat *= f;
    n.satFat *= f;
    n.carbs *= f;
    n.sugar *= f;
    n.fiber *= f;
    n.salt *= f;
    return n;
}

QString Entry::amountText() const
{
    if (byGrams && grams > 0) return tr("%1 g").arg(qRound(grams));
    const QString unit = yieldUnit.isEmpty() ? (portions <= 1.0 ? tr("Portion") : tr("Portionen")) : yieldUnit;
    QString t = Fmt::amount(portions, QString()) + QLatin1Char(' ') + unit;
    if (grams >= 1) t += tr(" (ca. %1 g)").arg(qRound(grams / 5.0) * 5);
    return t;
}

void Entry::setPortions(double p)
{
    portions = qMax(0.0, p);
    byGrams = false;
    grams = servingGrams > 0 ? servingGrams * portions : 0;
}

void Entry::setGrams(double g)
{
    grams = qMax(0.0, g);
    byGrams = true;
    if (servingGrams > 0) portions = grams / servingGrams;
}

QJsonObject Entry::toJson() const
{
    QJsonObject o{ { QStringLiteral("id"), id },
                   { QStringLiteral("when"), when.toString(Qt::ISODate) },
                   { QStringLiteral("meal"), meal },
                   { QStringLiteral("title"), title },
                   { QStringLiteral("portions"), portions },
                   { QStringLiteral("byGrams"), byGrams },
                   { QStringLiteral("grams"), grams },
                   { QStringLiteral("perServing"), perServing.toJson() },
                   { QStringLiteral("servingGrams"), servingGrams } };
    if (!recipeId.isEmpty()) o[QStringLiteral("recipeId")] = recipeId;
    if (own) {
        o[QStringLiteral("own")] = true;
        o[QStringLiteral("ownRecipe")] = ownRecipe;
    }
    if (!yieldUnit.isEmpty()) o[QStringLiteral("yieldUnit")] = yieldUnit;
    if (!note.isEmpty()) o[QStringLiteral("note")] = note;
    return o;
}

Entry Entry::fromJson(const QJsonObject &o)
{
    Entry e;
    e.id = o.value(QStringLiteral("id")).toString();
    e.when = QDateTime::fromString(o.value(QStringLiteral("when")).toString(), Qt::ISODate);
    e.meal = o.value(QStringLiteral("meal")).toString();
    e.title = o.value(QStringLiteral("title")).toString();
    e.recipeId = o.value(QStringLiteral("recipeId")).toString();
    e.own = o.value(QStringLiteral("own")).toBool();
    e.ownRecipe = o.value(QStringLiteral("ownRecipe")).toObject();
    e.portions = o.value(QStringLiteral("portions")).toDouble(1);
    e.byGrams = o.value(QStringLiteral("byGrams")).toBool();
    e.grams = o.value(QStringLiteral("grams")).toDouble();
    e.perServing = Nutrients::fromJson(o.value(QStringLiteral("perServing")).toObject());
    e.servingGrams = o.value(QStringLiteral("servingGrams")).toDouble();
    e.yieldUnit = o.value(QStringLiteral("yieldUnit")).toString();
    e.note = o.value(QStringLiteral("note")).toString();
    return e;
}

Entry Entry::fromRecipe(const Recipe &r, bool own)
{
    Entry e;
    e.title = r.title;
    e.recipeId = own ? QString() : r.id;
    e.own = own;
    if (own) {
        Recipe copy = r;
        copy.image.clear();   // Tagebuch klein halten
        e.ownRecipe = copy.toJson();
    }
    e.perServing = r.nutrition.perServing;
    e.servingGrams = MealTypes::servingGrams(r);
    e.yieldUnit = r.yieldUnit;
    e.setPortions(1);
    return e;
}

Recipe Entry::recipe() const
{
    Recipe r = Recipe::fromJson(ownRecipe);
    if (r.title.isEmpty()) r.title = title;
    return r;
}

// ---------------------------------------------------------------- Book

Book::Book(const QString &profileId)
{
    if (!profileId.isEmpty()) load(profileId);
}

QString Book::fileFor(const QString &profileId)
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) dir = QDir::homePath() + QStringLiteral("/.viviameals");
    dir = QDir(dir).filePath(QStringLiteral("tagebuch"));
    QDir().mkpath(dir);
    return QDir(dir).filePath(profileId + QStringLiteral(".json"));
}

QString Book::file() const { return fileFor(m_profile); }

void Book::removeFor(const QString &profileId)
{
    if (!profileId.isEmpty()) QFile::remove(fileFor(profileId));
}

bool Book::load(const QString &profileId)
{
    m_profile = profileId;
    m_entries.clear();
    QFile f(file());
    if (!f.open(QIODevice::ReadOnly)) return false;
    for (const auto &v : QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("entries")).toArray()) {
        Entry e = Entry::fromJson(v.toObject());
        if (e.when.isValid()) m_entries << e;
    }
    return true;
}

bool Book::save(QString *error) const
{
    if (m_profile.isEmpty()) return false;
    QJsonArray a;
    for (const Entry &e : m_entries) a.append(e.toJson());
    QSaveFile f(file());
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(QJsonDocument(QJsonObject{ { QStringLiteral("format"), QStringLiteral("ViviaMeals-Tagebuch") },
                                       { QStringLiteral("version"), 1 },
                                       { QStringLiteral("profile"), m_profile },
                                       { QStringLiteral("entries"), a } })
                .toJson(QJsonDocument::Compact));
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

QList<Entry> Book::day(const QDate &d) const
{
    QList<Entry> l;
    for (const Entry &e : m_entries)
        if (e.when.date() == d) l << e;
    std::sort(l.begin(), l.end(), [](const Entry &a, const Entry &b) { return a.when < b.when; });
    return l;
}

Nutrients Book::dayTotal(const QDate &d) const
{
    Nutrients t;
    for (const Entry &e : m_entries) {
        if (e.when.date() != d) continue;
        const Nutrients n = e.eaten();
        t.kcal += n.kcal;
        t.protein += n.protein;
        t.fat += n.fat;
        t.satFat += n.satFat;
        t.carbs += n.carbs;
        t.sugar += n.sugar;
        t.fiber += n.fiber;
        t.salt += n.salt;
    }
    return t;
}

QList<QDate> Book::daysWithEntries() const
{
    QList<QDate> d;
    for (const Entry &e : m_entries)
        if (!d.contains(e.when.date())) d << e.when.date();
    std::sort(d.begin(), d.end());
    return d;
}

Entry *Book::find(const QString &id)
{
    for (Entry &e : m_entries)
        if (e.id == id) return &e;
    return nullptr;
}

QString Book::add(Entry e)
{
    if (e.id.isEmpty()) e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_entries << e;
    return e.id;
}

void Book::update(const Entry &e)
{
    for (Entry &x : m_entries)
        if (x.id == e.id) x = e;
}

void Book::remove(const QString &id)
{
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(), [&](const Entry &e) { return e.id == id; }), m_entries.end());
}

} // namespace Diary
