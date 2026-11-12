// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "recipepackage.h"
#include "odtexport.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QtEndian>

namespace RecipePackage {

namespace {

const char *const kFormat = "ViviaMeals-Package";
const char *const kMeta = "viviameals-paket.json";
const char *const kFoods = "lebensmittel.json";

quint32 crc32(const QByteArray &data)
{
    static quint32 table[256];
    static bool init = false;
    if (!init) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    quint32 c = 0xFFFFFFFFu;
    for (char ch : data) c = table[(c ^ quint8(ch)) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

// Little-Endian lesen, Byte für Byte mit Bereichsprüfung (außerhalb der Daten: 0)
quint16 get16(const QByteArray &d, qsizetype p)
{
    if (p < 0 || p + 2 > d.size()) return 0;
    return quint16(quint8(d.at(p)) | (quint8(d.at(p + 1)) << 8));
}
quint32 get32(const QByteArray &d, qsizetype p)
{
    if (p < 0 || p + 4 > d.size()) return 0;
    return quint32(quint8(d.at(p))) | (quint32(quint8(d.at(p + 1))) << 8) | (quint32(quint8(d.at(p + 2))) << 16) | (quint32(quint8(d.at(p + 3))) << 24);
}

QString tr(const char *s) { return QCoreApplication::translate("RecipePackage", s); }

} // namespace

QString suffix() { return QStringLiteral("vmpack"); }

QHash<QString, QByteArray> unzip(const QByteArray &zip, QString *error)
{
    QHash<QString, QByteArray> out;
    auto fail = [&](const QString &e) {
        if (error) *error = e;
        return QHash<QString, QByteArray>();
    };
    // Ende des Inhaltsverzeichnisses suchen (höchstens 64 KB Kommentar)
    qsizetype eocd = -1;
    for (qsizetype p = zip.size() - 22; p >= 0 && p >= zip.size() - 22 - 65535; --p)
        if (get32(zip, p) == 0x06054b50) {
            eocd = p;
            break;
        }
    if (eocd < 0) return fail(tr("Die Datei ist kein ZIP-Archiv bzw. kein Rezeptpaket."));
    const int entries = get16(zip, eocd + 10);
    qsizetype p = get32(zip, eocd + 16);
    for (int i = 0; i < entries; ++i) {
        if (p + 46 > zip.size() || get32(zip, p) != 0x02014b50) return fail(tr("Das Inhaltsverzeichnis des Archivs ist beschädigt."));
        const quint16 method = get16(zip, p + 10);
        const quint32 crc = get32(zip, p + 16);
        const quint32 csize = get32(zip, p + 20);
        const quint32 size = get32(zip, p + 24);
        const quint16 nlen = get16(zip, p + 28), xlen = get16(zip, p + 30), clen = get16(zip, p + 32);
        const quint32 local = get32(zip, p + 42);
        const QString name = QString::fromUtf8(zip.mid(p + 46, nlen));
        p += 46 + nlen + xlen + clen;
        if (name.endsWith(QLatin1Char('/'))) continue;
        if (qsizetype(local) + 30 > zip.size() || get32(zip, local) != 0x04034b50) return fail(tr("Archiv beschädigt (%1).").arg(name));
        const qsizetype start = local + 30 + get16(zip, local + 26) + get16(zip, local + 28);
        if (start + qsizetype(csize) > zip.size()) return fail(tr("Archiv unvollständig (%1).").arg(name));
        QByteArray data = zip.mid(start, csize);
        if (method == 8) {
            return fail(tr("„%1“ ist komprimiert gespeichert. ViviaMeals-Pakete werden unkomprimiert geschrieben – bitte das Paket nicht "
                           "mit einem Packprogramm neu packen.").arg(name));
        }
        if (method != 0) return fail(tr("Unbekanntes Packverfahren in „%1“.").arg(name));
        if (quint32(data.size()) != size || crc32(data) != crc) return fail(tr("Prüfsummenfehler in „%1“ – die Datei ist beschädigt.").arg(name));
        out.insert(name, data);
    }
    if (error) error->clear();
    return out;
}

void collectFoods(const QList<Recipe> &recipes, QList<Food> *foods, QHash<QString, QString> *learned)
{
    const FoodTable &ft = FoodTable::instance();
    QSet<QString> keys;
    for (const Recipe &r : recipes)
        for (const Ingredient &i : r.ingredients) {
            if (i.heading) continue;
            if (!i.food.isEmpty()) keys.insert(i.food);
            const QString n = FoodTable::normalize(i.name);
            const QString l = ft.learnedMap().value(n);
            if (!l.isEmpty() && learned) {
                learned->insert(n, l);
                keys.insert(l);
            }
        }
    if (!foods) return;
    QStringList sorted(keys.begin(), keys.end());
    sorted.sort();
    for (const QString &k : sorted) {
        const Food *f = ft.find(k);
        // nur rein eigene Einträge – geänderte eingebaute Werte behält jeder für sich
        if (f && f->user && !ft.isBuiltin(k)) foods->append(*f);
    }
}

QByteArray build(const QList<Recipe> &recipes, const Options &o, int *foodCount)
{
    OdtExport::Zip zip;
    QJsonObject meta;
    meta[QStringLiteral("format")] = QLatin1String(kFormat);
    meta[QStringLiteral("version")] = 1;
    meta[QStringLiteral("name")] = o.name;
    if (!o.description.isEmpty()) meta[QStringLiteral("description")] = o.description;
    if (!o.author.isEmpty()) meta[QStringLiteral("author")] = o.author;
    meta[QStringLiteral("created")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    meta[QStringLiteral("app")] = QStringLiteral("ViviaMeals %1").arg(QCoreApplication::applicationVersion());
    meta[QStringLiteral("count")] = int(recipes.size());
    meta[QStringLiteral("images")] = o.images;
    QList<Food> foods;
    QHash<QString, QString> learned;
    if (o.foods) collectFoods(recipes, &foods, &learned);
    meta[QStringLiteral("foods")] = int(foods.size());
    zip.add(QLatin1String(kMeta), QJsonDocument(meta).toJson(QJsonDocument::Indented), false);
    for (const Recipe &orig : recipes) {
        Recipe r = orig;
        if (!o.images) r.image.clear();
        if (!o.favorites) r.favorite = false;
        zip.add(QStringLiteral("rezepte/%1.vmr").arg(r.id), QJsonDocument(r.toJson()).toJson(QJsonDocument::Compact), false);
    }
    if (o.foods && (!foods.isEmpty() || !learned.isEmpty())) {
        QJsonObject f;
        f[QStringLiteral("format")] = QStringLiteral("ViviaMeals-UserFoods");
        QJsonArray arr;
        for (const Food &food : foods) arr.append(food.toJson());
        f[QStringLiteral("foods")] = arr;
        QJsonObject l;
        QStringList names = learned.keys();
        names.sort();
        for (const QString &n : names) l[n] = learned.value(n);
        f[QStringLiteral("learned")] = l;
        zip.add(QLatin1String(kFoods), QJsonDocument(f).toJson(QJsonDocument::Indented), false);
    }
    if (foodCount) *foodCount = int(foods.size());
    return zip.finish();
}

bool write(const QString &file, const QList<Recipe> &recipes, const Options &o, QString *error, int *foodCount)
{
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(build(recipes, o, foodCount));
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

bool parse(const QByteArray &data, Content *out, QString *error)
{
    QString err;
    const QHash<QString, QByteArray> files = unzip(data, &err);
    if (!err.isEmpty()) {
        if (error) *error = err;
        return false;
    }
    const QJsonObject meta = QJsonDocument::fromJson(files.value(QLatin1String(kMeta))).object();
    if (meta.value(QStringLiteral("format")).toString() != QLatin1String(kFormat)) {
        if (error) *error = tr("Die Datei ist kein ViviaMeals-Rezeptpaket.");
        return false;
    }
    Content c;
    c.name = meta.value(QStringLiteral("name")).toString();
    c.description = meta.value(QStringLiteral("description")).toString();
    c.author = meta.value(QStringLiteral("author")).toString();
    c.app = meta.value(QStringLiteral("app")).toString();
    c.created = QDateTime::fromString(meta.value(QStringLiteral("created")).toString(), Qt::ISODate);
    if (meta.value(QStringLiteral("version")).toInt() > 1)
        c.warnings << tr("Das Paket stammt aus einer neueren ViviaMeals-Version (%1); eventuell gehen Angaben verloren.").arg(c.app);
    QStringList names = files.keys();
    names.sort();
    for (const QString &n : names) {
        if (!n.startsWith(QLatin1String("rezepte/")) || !n.endsWith(QLatin1String(".vmr"))) continue;
        QJsonParseError pe;
        const QJsonDocument d = QJsonDocument::fromJson(files.value(n), &pe);
        if (!d.isObject()) {
            c.warnings << tr("%1: nicht lesbar (%2)").arg(n, pe.errorString());
            continue;
        }
        Recipe r = Recipe::fromJson(d.object());
        if (r.title.isEmpty()) {
            c.warnings << tr("%1: Rezept ohne Titel übersprungen").arg(n);
            continue;
        }
        c.recipes << r;
    }
    const QJsonObject foods = QJsonDocument::fromJson(files.value(QLatin1String(kFoods))).object();
    for (const QJsonValue &v : foods.value(QStringLiteral("foods")).toArray()) {
        Food f = Food::fromJson(v.toObject());
        if (f.key.isEmpty() || f.name.isEmpty()) continue;
        f.user = true;
        if (!f.source.contains(tr("Rezeptpaket"))) {
            const QString pkg = c.name.isEmpty() ? tr("Rezeptpaket") : tr("Rezeptpaket „%1“").arg(c.name);
            f.source = f.source.isEmpty() ? pkg : pkg + QStringLiteral(" – ") + f.source;
        }
        c.foods << f;
    }
    const QJsonObject l = foods.value(QStringLiteral("learned")).toObject();
    for (auto it = l.begin(); it != l.end(); ++it) c.learned.insert(it.key(), it.value().toString());
    if (c.recipes.isEmpty() && c.warnings.isEmpty()) c.warnings << tr("Das Paket enthält keine Rezepte.");
    if (out) *out = c;
    return true;
}

bool read(const QString &file, Content *out, QString *error)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    return parse(f.readAll(), out, error);
}

int importFoods(const Content &c, int *learnedAdded)
{
    FoodTable &ft = FoodTable::instance();
    QList<Food> fresh;
    for (const Food &f : c.foods)
        if (!ft.find(f.key)) fresh << f;
    int added = fresh.isEmpty() ? 0 : ft.saveUserFoods(fresh, false);
    int learned = 0;
    for (auto it = c.learned.begin(); it != c.learned.end(); ++it) {
        if (!ft.learnedFor(it.key()).isEmpty() || !ft.find(it.value())) continue;
        ft.learn(it.key(), it.value());
        ++learned;
    }
    if (learnedAdded) *learnedAdded = learned;
    return added;
}

} // namespace RecipePackage
