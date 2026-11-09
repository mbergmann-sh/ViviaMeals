// SPDX-License-Identifier: MIT
#include "recipe.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <cmath>

// ---------------------------------------------------------------- Nutrients

Nutrients &Nutrients::operator+=(const Nutrients &o)
{
    kcal += o.kcal; protein += o.protein; fat += o.fat; satFat += o.satFat;
    carbs += o.carbs; sugar += o.sugar; fiber += o.fiber; salt += o.salt;
    return *this;
}

Nutrients Nutrients::scaled(double f) const
{
    Nutrients n;
    n.kcal = kcal * f; n.protein = protein * f; n.fat = fat * f; n.satFat = satFat * f;
    n.carbs = carbs * f; n.sugar = sugar * f; n.fiber = fiber * f; n.salt = salt * f;
    return n;
}

static double r1(double v) { return std::round(v * 10.0) / 10.0; }

QJsonObject Nutrients::toJson() const
{
    return QJsonObject{ { QStringLiteral("kcal"), std::round(kcal) },
                        { QStringLiteral("kJ"), std::round(kJ()) },
                        { QStringLiteral("protein"), r1(protein) },
                        { QStringLiteral("fat"), r1(fat) },
                        { QStringLiteral("saturatedFat"), r1(satFat) },
                        { QStringLiteral("carbohydrates"), r1(carbs) },
                        { QStringLiteral("sugar"), r1(sugar) },
                        { QStringLiteral("fiber"), r1(fiber) },
                        { QStringLiteral("salt"), std::round(salt * 100.0) / 100.0 } };
}

Nutrients Nutrients::fromJson(const QJsonObject &o)
{
    Nutrients n;
    n.kcal = o.value(QStringLiteral("kcal")).toDouble();
    n.protein = o.value(QStringLiteral("protein")).toDouble();
    n.fat = o.value(QStringLiteral("fat")).toDouble();
    n.satFat = o.value(QStringLiteral("saturatedFat")).toDouble();
    n.carbs = o.value(QStringLiteral("carbohydrates")).toDouble();
    n.sugar = o.value(QStringLiteral("sugar")).toDouble();
    n.fiber = o.value(QStringLiteral("fiber")).toDouble();
    n.salt = o.value(QStringLiteral("salt")).toDouble();
    return n;
}

QJsonObject NutritionInfo::toJson() const
{
    return QJsonObject{ { QStringLiteral("total"), total.toJson() },
                        { QStringLiteral("perServing"), perServing.toJson() },
                        { QStringLiteral("matched"), matched },
                        { QStringLiteral("countable"), countable },
                        { QStringLiteral("unmatched"), QJsonArray::fromStringList(unmatched) },
                        { QStringLiteral("computed"), computed.toString(Qt::ISODate) } };
}

NutritionInfo NutritionInfo::fromJson(const QJsonObject &o)
{
    NutritionInfo n;
    n.total = Nutrients::fromJson(o.value(QStringLiteral("total")).toObject());
    n.perServing = Nutrients::fromJson(o.value(QStringLiteral("perServing")).toObject());
    n.matched = o.value(QStringLiteral("matched")).toInt();
    n.countable = o.value(QStringLiteral("countable")).toInt();
    for (const auto &v : o.value(QStringLiteral("unmatched")).toArray()) n.unmatched << v.toString();
    n.computed = QDateTime::fromString(o.value(QStringLiteral("computed")).toString(), Qt::ISODate);
    return n;
}

// ---------------------------------------------------------------- Formatierung

namespace Fmt {

QString number(double v, int decimals)
{
    QString s = QLocale(QLocale::German).toString(v, 'f', decimals);
    if (decimals > 0 && s.contains(QLatin1Char(','))) {
        while (s.endsWith(QLatin1Char('0'))) s.chop(1);
        if (s.endsWith(QLatin1Char(','))) s.chop(1);
    }
    return s;
}

QString amount(double v, const QString &unit)
{
    if (v <= 0) return {};
    const bool metric = unit == QLatin1String("g") || unit == QLatin1String("ml") || unit == QLatin1String("kg")
                        || unit == QLatin1String("l") || unit == QLatin1String("cm");
    if (metric) {
        if (unit == QLatin1String("g") || unit == QLatin1String("ml")) {
            if (v >= 100) return number(std::round(v / 5.0) * 5.0, 0);
            if (v >= 10) return number(std::round(v), 0);
            return number(v, 1);
        }
        return number(v, 2);
    }
    // Brüche für Löffel, Stück, Tassen …
    const double whole = std::floor(v + 1e-9);
    const double frac = v - whole;
    struct F { double v; const char *s; };
    static const F fr[] = { { 0.125, "⅛" }, { 0.25, "¼" }, { 1.0 / 3, "⅓" }, { 0.5, "½" },
                            { 2.0 / 3, "⅔" }, { 0.75, "¾" } };
    if (frac < 0.06) return number(whole, 0);
    for (const F &f : fr)
        if (std::fabs(frac - f.v) < 0.04)
            return whole > 0 ? number(whole, 0) + QLatin1Char(' ') + QString::fromUtf8(f.s) : QString::fromUtf8(f.s);
    if (frac > 0.94) return number(whole + 1, 0);
    return number(v, 2);
}

QString kcal(double v) { return number(std::round(v), 0); }

} // namespace Fmt

// ---------------------------------------------------------------- Ingredient

QString Ingredient::amountText() const
{
    if (amount <= 0) return {};
    QString s = Fmt::amount(amount, unit);
    if (amountMax > amount) s += QStringLiteral("–") + Fmt::amount(amountMax, unit);
    return s;
}

QString Ingredient::displayLine() const
{
    if (heading) return QStringLiteral("== %1 ==").arg(name);
    QStringList parts;
    const QString a = amountText();
    if (!a.isEmpty()) parts << a;
    if (!unit.isEmpty()) parts << unit;
    QString n = name;
    if (!note.isEmpty()) n += QStringLiteral(", ") + note;
    parts << n;
    return parts.join(QLatin1Char(' '));
}

QJsonObject Ingredient::toJson() const
{
    QJsonObject o;
    if (heading) {
        o[QStringLiteral("heading")] = name;
        return o;
    }
    if (amount > 0) o[QStringLiteral("amount")] = amount;
    if (amountMax > 0) o[QStringLiteral("amountMax")] = amountMax;
    if (!unit.isEmpty()) o[QStringLiteral("unit")] = unit;
    o[QStringLiteral("name")] = name;
    if (!note.isEmpty()) o[QStringLiteral("note")] = note;
    if (!food.isEmpty()) o[QStringLiteral("food")] = food;
    if (foodManual) o[QStringLiteral("foodManual")] = true;
    if (grams > 0) o[QStringLiteral("grams")] = std::round(grams * 10.0) / 10.0;
    if (!original.isEmpty()) o[QStringLiteral("original")] = original;
    return o;
}

Ingredient Ingredient::fromJson(const QJsonObject &o)
{
    Ingredient i;
    if (o.contains(QStringLiteral("heading"))) {
        i.heading = true;
        i.name = o.value(QStringLiteral("heading")).toString();
        return i;
    }
    i.amount = o.value(QStringLiteral("amount")).toDouble();
    i.amountMax = o.value(QStringLiteral("amountMax")).toDouble();
    i.unit = o.value(QStringLiteral("unit")).toString();
    i.name = o.value(QStringLiteral("name")).toString();
    i.note = o.value(QStringLiteral("note")).toString();
    i.food = o.value(QStringLiteral("food")).toString();
    i.foodManual = o.value(QStringLiteral("foodManual")).toBool();
    i.grams = o.value(QStringLiteral("grams")).toDouble();
    i.original = o.value(QStringLiteral("original")).toString();
    return i;
}

// ---------------------------------------------------------------- Recipe

QString Recipe::servingsText() const
{
    const QString n = Fmt::number(servings, 1);
    if (!yieldUnit.isEmpty()) return n + QLatin1Char(' ') + yieldUnit;
    return servings == 1.0 ? QStringLiteral("1 Portion") : n + QStringLiteral(" Portionen");
}

QString Recipe::ingredientsText() const
{
    QStringList l;
    for (const Ingredient &i : ingredients) l << i.displayLine();
    return l.join(QLatin1Char('\n'));
}

QJsonObject Recipe::toJson() const
{
    QJsonArray ing;
    for (const Ingredient &i : ingredients) ing.append(i.toJson());
    QJsonObject o{ { QStringLiteral("format"), QStringLiteral("VitaMeals-Recipe") },
                   { QStringLiteral("formatVersion"), 1 },
                   { QStringLiteral("id"), id },
                   { QStringLiteral("title"), title },
                   { QStringLiteral("servings"), servings },
                   { QStringLiteral("tags"), QJsonArray::fromStringList(tags) },
                   { QStringLiteral("ingredients"), ing },
                   { QStringLiteral("instructions"), instructions },
                   { QStringLiteral("language"), language },
                   { QStringLiteral("created"), created.toString(Qt::ISODate) },
                   { QStringLiteral("modified"), modified.toString(Qt::ISODate) } };
    if (!yieldUnit.isEmpty()) o[QStringLiteral("yieldUnit")] = yieldUnit;
    if (!image.isEmpty()) o[QStringLiteral("image")] = image;
    if (!source.isEmpty()) o[QStringLiteral("source")] = source;
    if (!sourceFormat.isEmpty()) o[QStringLiteral("sourceFormat")] = sourceFormat;
    if (prepMinutes > 0) o[QStringLiteral("prepMinutes")] = prepMinutes;
    if (favorite) o[QStringLiteral("favorite")] = true;
    if (nutrition.valid()) o[QStringLiteral("nutrition")] = nutrition.toJson();
    return o;
}

Recipe Recipe::fromJson(const QJsonObject &o)
{
    Recipe r;
    r.id = o.value(QStringLiteral("id")).toString();
    r.title = o.value(QStringLiteral("title")).toString();
    r.servings = o.value(QStringLiteral("servings")).toDouble(4);
    if (r.servings <= 0) r.servings = 1;
    r.yieldUnit = o.value(QStringLiteral("yieldUnit")).toString();
    for (const auto &t : o.value(QStringLiteral("tags")).toArray()) r.tags << t.toString();
    for (const auto &i : o.value(QStringLiteral("ingredients")).toArray()) r.ingredients << Ingredient::fromJson(i.toObject());
    r.instructions = o.value(QStringLiteral("instructions")).toString();
    r.language = o.value(QStringLiteral("language")).toString(QStringLiteral("de"));
    r.image = o.value(QStringLiteral("image")).toString();
    r.source = o.value(QStringLiteral("source")).toString();
    r.sourceFormat = o.value(QStringLiteral("sourceFormat")).toString();
    r.prepMinutes = o.value(QStringLiteral("prepMinutes")).toInt();
    r.favorite = o.value(QStringLiteral("favorite")).toBool();
    r.created = QDateTime::fromString(o.value(QStringLiteral("created")).toString(), Qt::ISODate);
    r.modified = QDateTime::fromString(o.value(QStringLiteral("modified")).toString(), Qt::ISODate);
    if (o.contains(QStringLiteral("nutrition")))
        r.nutrition = NutritionInfo::fromJson(o.value(QStringLiteral("nutrition")).toObject());
    if (r.id.isEmpty()) r.id = newId();
    return r;
}

bool Recipe::save(const QString &file, QString *error) const
{
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

bool Recipe::load(const QString &file, Recipe *out, QString *error)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    QJsonParseError pe;
    const QJsonDocument d = QJsonDocument::fromJson(f.readAll(), &pe);
    if (!d.isObject() || d.object().value(QStringLiteral("format")).toString() != QLatin1String("VitaMeals-Recipe")) {
        if (error) *error = pe.error != QJsonParseError::NoError ? pe.errorString() : QStringLiteral("kein VitaMeals-Rezept");
        return false;
    }
    *out = fromJson(d.object());
    return true;
}

bool Recipe::hasTag(const QString &tag) const
{
    return tags.contains(tag, Qt::CaseInsensitive);
}

void Recipe::addTag(const QString &tag)
{
    const QString t = tag.trimmed().toLower();
    if (!t.isEmpty() && !hasTag(t)) tags << t;
}

QString Recipe::newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString Recipe::normalizedTitle(const QString &t)
{
    QString s = t.toLower();
    s.replace(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]+")), QStringLiteral(" "));
    return s.simplified();
}
