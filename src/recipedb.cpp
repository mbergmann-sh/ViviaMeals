// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "recipedb.h"
#include "mealtypes.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QRandomGenerator>
#include <QSaveFile>
#include <algorithm>

namespace {
const char *kIndex = "index.json";
const char *kRecipes = "rezepte";
constexpr int kIndexVersion = 3;

QStringList arr(const QJsonValue &v)
{
    QStringList l;
    for (const auto &x : v.toArray()) l << x.toString();
    return l;
}
}

// ---------------------------------------------------------------- RecipeSummary

RecipeSummary RecipeSummary::from(const Recipe &r)
{
    RecipeSummary s;
    s.id = r.id;
    s.title = r.title;
    s.normTitle = Recipe::normalizedTitle(r.title);
    s.tags = r.tags;
    s.servings = r.servings;
    s.yieldUnit = r.yieldUnit;
    s.perServing = r.nutrition.perServing;
    s.servingGrams = MealTypes::servingGrams(r);
    s.coverage = r.nutrition.coverage();
    s.hasNutrition = r.nutrition.valid() && r.nutrition.matched > 0;
    for (const Ingredient &i : r.ingredients) {
        if (i.heading) continue;
        s.ingredientNames << i.name.toLower();
        if (!i.food.isEmpty() && !s.foods.contains(i.food)) s.foods << i.food;
    }
    s.hasImage = !r.image.isEmpty();
    s.favorite = r.favorite;
    s.language = r.language;
    s.modified = r.modified;
    return s;
}

QJsonObject RecipeSummary::toJson() const
{
    return QJsonObject{ { QStringLiteral("id"), id },
                        { QStringLiteral("title"), title },
                        { QStringLiteral("tags"), QJsonArray::fromStringList(tags) },
                        { QStringLiteral("servings"), servings },
                        { QStringLiteral("yieldUnit"), yieldUnit },
                        { QStringLiteral("perServing"), perServing.toJson() },
                        { QStringLiteral("servingGrams"), servingGrams },
                        { QStringLiteral("coverage"), coverage },
                        { QStringLiteral("hasNutrition"), hasNutrition },
                        { QStringLiteral("ingredients"), QJsonArray::fromStringList(ingredientNames) },
                        { QStringLiteral("foods"), QJsonArray::fromStringList(foods) },
                        { QStringLiteral("hasImage"), hasImage },
                        { QStringLiteral("favorite"), favorite },
                        { QStringLiteral("language"), language },
                        { QStringLiteral("modified"), modified.toString(Qt::ISODate) },
                        { QStringLiteral("fileTime"), double(fileTime) } };
}

RecipeSummary RecipeSummary::fromJson(const QJsonObject &o)
{
    RecipeSummary s;
    s.id = o.value(QStringLiteral("id")).toString();
    s.title = o.value(QStringLiteral("title")).toString();
    s.normTitle = Recipe::normalizedTitle(s.title);
    s.tags = arr(o.value(QStringLiteral("tags")));
    s.servings = o.value(QStringLiteral("servings")).toDouble(1);
    s.yieldUnit = o.value(QStringLiteral("yieldUnit")).toString();
    s.perServing = Nutrients::fromJson(o.value(QStringLiteral("perServing")).toObject());
    s.servingGrams = o.value(QStringLiteral("servingGrams")).toDouble();
    s.coverage = o.value(QStringLiteral("coverage")).toDouble();
    s.hasNutrition = o.value(QStringLiteral("hasNutrition")).toBool();
    s.ingredientNames = arr(o.value(QStringLiteral("ingredients")));
    s.foods = arr(o.value(QStringLiteral("foods")));
    s.hasImage = o.value(QStringLiteral("hasImage")).toBool();
    s.favorite = o.value(QStringLiteral("favorite")).toBool();
    s.language = o.value(QStringLiteral("language")).toString();
    s.modified = QDateTime::fromString(o.value(QStringLiteral("modified")).toString(), Qt::ISODate);
    s.fileTime = qint64(o.value(QStringLiteral("fileTime")).toDouble());
    return s;
}

// ---------------------------------------------------------------- RecipeDb

RecipeDb &RecipeDb::instance()
{
    static RecipeDb db;
    return db;
}

bool RecipeDb::isDatabaseFolder(const QString &folder)
{
    return QFileInfo::exists(QDir(folder).filePath(markerName())) || QFileInfo::exists(QDir(folder).filePath(legacyMarkerName()));
}

bool RecipeDb::create(const QString &folder, const QString &name, QString *error)
{
    QDir d(folder);
    if (!d.exists() && !QDir().mkpath(folder)) {
        if (error) *error = tr("Der Ordner %1 kann nicht angelegt werden.").arg(folder);
        return false;
    }
    if (isDatabaseFolder(folder)) return open(folder, error);
    d.mkpath(QLatin1String(kRecipes));
    QSaveFile f(d.filePath(markerName()));
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    const QJsonObject o{ { QStringLiteral("format"), QStringLiteral("ViviaMeals-Database") },
                         { QStringLiteral("formatVersion"), 1 },
                         { QStringLiteral("name"), name },
                         { QStringLiteral("created"), QDateTime::currentDateTime().toString(Qt::ISODate) } };
    f.write(QJsonDocument(o).toJson());
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return open(folder, error);
}

bool RecipeDb::open(const QString &path, QString *error)
{
    QString folder = path;
    QFileInfo fi(path);
    if (fi.isFile()) folder = fi.absolutePath();
    folder = QDir(folder).absolutePath();
    // Datenbank aus der Zeit vor der Umbenennung (VitaMeals): Kennungsdatei übernehmen
    if (!QFileInfo::exists(QDir(folder).filePath(markerName())) && QFileInfo::exists(QDir(folder).filePath(legacyMarkerName())))
        if (!QFile::copy(QDir(folder).filePath(legacyMarkerName()), QDir(folder).filePath(markerName())))
            QFile::rename(QDir(folder).filePath(legacyMarkerName()), QDir(folder).filePath(markerName()));
    QFile f(QDir(folder).filePath(markerName()));
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = tr("%1 ist keine ViviaMeals-Rezeptdatenbank.").arg(QDir::toNativeSeparators(folder));
        return false;
    }
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    const QString fmt = o.value(QStringLiteral("format")).toString();
    if (fmt != QLatin1String("ViviaMeals-Database") && fmt != QLatin1String("VitaMeals-Database")) {   // auch vor der Umbenennung angelegte
        if (error) *error = tr("Die Kennungsdatei der Datenbank ist beschädigt.");
        return false;
    }
    m_folder = folder;
    m_name = o.value(QStringLiteral("name")).toString();
    if (m_name.isEmpty()) m_name = QFileInfo(folder).fileName();
    QDir(folder).mkpath(QLatin1String(kRecipes));
    rebuildIndex();
    emit opened();
    emit changed();
    return true;
}

void RecipeDb::close()
{
    m_folder.clear();
    m_name.clear();
    m_list.clear();
    m_pos.clear();
    emit changed();
}

QString RecipeDb::fileFor(const QString &id) const
{
    return QDir(m_folder).filePath(QStringLiteral("%1/%2.vmr").arg(QLatin1String(kRecipes), id));
}

void RecipeDb::rebuildIndex()
{
    // vorhandenen Zwischenspeicher lesen
    QHash<QString, RecipeSummary> cached;
    QFile f(QDir(m_folder).filePath(QLatin1String(kIndex)));
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        if (o.value(QStringLiteral("version")).toInt() == kIndexVersion)
            for (const auto &v : o.value(QStringLiteral("recipes")).toArray()) {
                const RecipeSummary s = RecipeSummary::fromJson(v.toObject());
                cached.insert(s.id, s);
            }
    }
    m_list.clear();
    m_pos.clear();
    bool changedIndex = false;
    QDirIterator it(QDir(m_folder).filePath(QLatin1String(kRecipes)), { QStringLiteral("*.vmr") }, QDir::Files);
    while (it.hasNext()) {
        const QString file = it.next();
        const QFileInfo fi(file);
        const QString id = fi.completeBaseName();
        const qint64 t = fi.lastModified().toMSecsSinceEpoch();
        auto c = cached.constFind(id);
        if (c != cached.constEnd() && c->fileTime == t) {
            m_list << *c;
            continue;
        }
        Recipe r;
        if (!Recipe::load(file, &r)) continue;
        if (r.id != id) r.id = id;      // Dateiname ist maßgeblich
        RecipeSummary s = RecipeSummary::from(r);
        s.fileTime = t;
        m_list << s;
        changedIndex = true;
    }
    if (m_list.size() != cached.size()) changedIndex = true;
    std::sort(m_list.begin(), m_list.end(), [](const RecipeSummary &a, const RecipeSummary &b) {
        return QString::localeAwareCompare(a.title, b.title) < 0;
    });
    for (int i = 0; i < m_list.size(); ++i) m_pos.insert(m_list[i].id, i);
    if (changedIndex) writeIndex();
}

void RecipeDb::writeIndex()
{
    if (m_folder.isEmpty()) return;
    QJsonArray a;
    for (const RecipeSummary &s : m_list) a.append(s.toJson());
    QSaveFile f(QDir(m_folder).filePath(QLatin1String(kIndex)));
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(QJsonObject{ { QStringLiteral("version"), kIndexVersion }, { QStringLiteral("recipes"), a } })
                .toJson(QJsonDocument::Compact));
    f.commit();
}

void RecipeDb::touched()
{
    if (m_batch > 0) {
        m_dirty = true;
        return;
    }
    writeIndex();
    emit changed();
}

void RecipeDb::beginBatch() { ++m_batch; }

void RecipeDb::endBatch()
{
    if (m_batch > 0 && --m_batch == 0 && m_dirty) {
        m_dirty = false;
        std::sort(m_list.begin(), m_list.end(), [](const RecipeSummary &a, const RecipeSummary &b) {
            return QString::localeAwareCompare(a.title, b.title) < 0;
        });
        m_pos.clear();
        for (int i = 0; i < m_list.size(); ++i) m_pos.insert(m_list[i].id, i);
        writeIndex();
        emit changed();
    }
}

const RecipeSummary *RecipeDb::summary(const QString &id) const
{
    auto it = m_pos.constFind(id);
    return it == m_pos.constEnd() ? nullptr : &m_list[*it];
}

bool RecipeDb::load(const QString &id, Recipe *out, QString *error) const
{
    if (!Recipe::load(fileFor(id), out, error)) return false;
    out->id = id;
    return true;
}

bool RecipeDb::save(Recipe &r, QString *error)
{
    if (!isOpen()) {
        if (error) *error = tr("Keine Rezeptdatenbank geöffnet.");
        return false;
    }
    if (r.id.isEmpty()) r.id = Recipe::newId();
    const QDateTime now = QDateTime::currentDateTime();
    if (!r.created.isValid()) r.created = now;
    r.modified = now;
    const QString file = fileFor(r.id);
    if (!r.save(file, error)) return false;
    RecipeSummary s = RecipeSummary::from(r);
    s.fileTime = QFileInfo(file).lastModified().toMSecsSinceEpoch();
    auto it = m_pos.constFind(r.id);
    if (it != m_pos.constEnd()) {
        m_list[*it] = s;
    } else {
        // alphabetisch einsortieren
        auto pos = std::lower_bound(m_list.begin(), m_list.end(), s, [](const RecipeSummary &a, const RecipeSummary &b) {
            return QString::localeAwareCompare(a.title, b.title) < 0;
        });
        m_list.insert(pos, s);
        m_pos.clear();
        for (int i = 0; i < m_list.size(); ++i) m_pos.insert(m_list[i].id, i);
    }
    touched();
    return true;
}

bool RecipeDb::remove(const QString &id)
{
    auto it = m_pos.constFind(id);
    if (it == m_pos.constEnd()) return false;
    QFile::remove(fileFor(id));
    m_list.removeAt(*it);
    m_pos.clear();
    for (int i = 0; i < m_list.size(); ++i) m_pos.insert(m_list[i].id, i);
    touched();
    return true;
}

int RecipeDb::clear(bool backup, QString *backupFolder, QString *error)
{
    if (m_folder.isEmpty()) return 0;
    QDir dir(QDir(m_folder).filePath(QLatin1String(kRecipes)));
    const QStringList files = dir.entryList({ QStringLiteral("*.vmr") }, QDir::Files);
    QString target;
    if (backup && !files.isEmpty()) {
        target = QDir(m_folder).filePath(QStringLiteral("sicherung-%1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd-hhmmss"))));
        if (!QDir().mkpath(target)) {
            if (error) *error = tr("Der Sicherungsordner %1 kann nicht angelegt werden.").arg(QDir::toNativeSeparators(target));
            return -1;
        }
        if (backupFolder) *backupFolder = target;
    }
    int n = 0;
    for (const QString &f : files) {
        const QString src = dir.filePath(f);
        const bool ok = target.isEmpty() ? QFile::remove(src) : QFile::rename(src, QDir(target).filePath(f));
        if (ok) ++n;
    }
    rebuildIndex();
    writeIndex();
    emit changed();
    return n;
}

QStringList RecipeDb::duplicatesOf(const Recipe &r) const
{
    const QString n = Recipe::normalizedTitle(r.title);
    QStringList ids;
    for (const RecipeSummary &s : m_list)
        if (s.id != r.id && s.normTitle == n) ids << s.id;
    return ids;
}

QStringList RecipeDb::allTags() const
{
    QMap<QString, int> count;
    for (const RecipeSummary &s : m_list)
        for (const QString &t : s.tags) count[t.toLower()]++;
    QStringList l = count.keys();
    std::stable_sort(l.begin(), l.end(), [&](const QString &a, const QString &b) { return count[a] > count[b]; });
    return l;
}

QString RecipeDb::randomId() const
{
    if (m_list.isEmpty()) return {};
    return m_list[int(QRandomGenerator::global()->bounded(int(m_list.size())))].id;
}
