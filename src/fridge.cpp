// SPDX-License-Identifier: MIT AND LicenseRef-Commons-Clause-1.0 (siehe LICENSE)
#include "fridge.h"
#include "foodtable.h"
#include "nutrition.h"
#include "recipedb.h"
#include "units.h"

#include <QCoreApplication>
#include <QMap>
#include <QRegularExpression>
#include <QSettings>
#include <algorithm>

// ---------------------------------------------------------------- ShoppingItem / Shopping

QString ShoppingItem::text() const
{
    if (amount <= 0) return unit.isEmpty() || Units::kindOf(unit) == Units::Kind::Vague ? name : unit + QLatin1Char(' ') + name;
    QString a = Fmt::amount(amount, unit);
    if (!unit.isEmpty()) a += QLatin1Char(' ') + unit;
    return a + QLatin1Char(' ') + name;
}

namespace Shopping {

void add(QList<ShoppingItem> &list, const Ingredient &ing, double factor, const QString &recipeTitle)
{
    if (ing.heading || ing.name.trimmed().isEmpty()) return;
    const Food *food = ing.food.isEmpty() ? FoodTable::instance().match(ing.name) : FoodTable::instance().find(ing.food);
    // Gewicht und Volumen zusammenfassen (kg → g, l → ml), sonst je Einheit
    QString unit = ing.unit;
    double amount = (ing.amountMax > ing.amount ? ing.amountMax : ing.amount) * factor;
    if (unit == QLatin1String("kg")) { unit = QStringLiteral("g"); amount *= 1000; }
    if (unit == QLatin1String("l")) { unit = QStringLiteral("ml"); amount *= 1000; }
    const QString key = food ? food->key : FoodTable::normalize(ing.name);
    for (ShoppingItem &it : list) {
        const Food *f2 = FoodTable::instance().match(it.name);
        const QString k2 = f2 ? f2->key : FoodTable::normalize(it.name);
        if (k2 != key) continue;
        if (it.unit == unit || (it.amount <= 0 && amount <= 0)) {
            it.amount += amount;
            if (!it.recipes.contains(recipeTitle)) it.recipes << recipeTitle;
            return;
        }
        // Löffel/Stück zur Gramm-Menge: über Gewicht addieren, wenn bekannt
        if (it.unit == QLatin1String("g") && food) {
            Ingredient tmp = ing;
            tmp.amount *= factor;
            tmp.amountMax *= factor;
            const double g = Nutrition::gramsOf(tmp, food);
            if (g > 0) {
                it.amount += g;
                if (!it.recipes.contains(recipeTitle)) it.recipes << recipeTitle;
                return;
            }
        }
    }
    ShoppingItem it;
    it.name = ing.name;
    it.amount = amount;
    it.unit = unit;
    it.category = food ? food->category : QStringLiteral("sonstiges");
    it.recipes << recipeTitle;
    list << it;
}

void sort(QList<ShoppingItem> &list)
{
    const QStringList order = FoodTable::categories();
    for (ShoppingItem &it : list) {
        // große Mengen lesbar machen
        if (it.unit == QLatin1String("g") && it.amount >= 1000) { it.unit = QStringLiteral("kg"); it.amount /= 1000.0; }
        if (it.unit == QLatin1String("ml") && it.amount >= 1000) { it.unit = QStringLiteral("l"); it.amount /= 1000.0; }
    }
    std::stable_sort(list.begin(), list.end(), [&](const ShoppingItem &a, const ShoppingItem &b) {
        const int ca = int(order.indexOf(a.category)), cb = int(order.indexOf(b.category));
        if (ca != cb) return ca < cb;
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
}

QString toText(const QList<ShoppingItem> &list, const QString &title)
{
    QString t = title + QLatin1Char('\n') + QString(title.size(), QLatin1Char('=')) + QLatin1Char('\n');
    QString cat;
    for (const ShoppingItem &it : list) {
        if (it.category != cat) {
            cat = it.category;
            t += QLatin1Char('\n') + FoodTable::categoryName(cat) + QStringLiteral(":\n");
        }
        t += QStringLiteral("  [ ] ") + it.text() + QLatin1Char('\n');
    }
    return t;
}

QString toHtml(const QList<ShoppingItem> &list, const QString &title)
{
    QString h = QStringLiteral("<h2>%1</h2>").arg(title.toHtmlEscaped());
    QString cat;
    bool open = false;
    for (const ShoppingItem &it : list) {
        if (it.category != cat) {
            if (open) h += QStringLiteral("</table>");
            cat = it.category;
            h += QStringLiteral("<h3>%1</h3><table cellspacing=\"0\" cellpadding=\"2\">").arg(FoodTable::categoryName(cat).toHtmlEscaped());
            open = true;
        }
        h += QStringLiteral("<tr><td>☐&nbsp;</td><td>%1</td><td style=\"color:gray;\">&nbsp;&nbsp;%2</td></tr>")
                 .arg(it.text().toHtmlEscaped(), it.recipes.join(QStringLiteral(", ")).toHtmlEscaped());
    }
    if (open) h += QStringLiteral("</table>");
    return h;
}

} // namespace Shopping

// ---------------------------------------------------------------- Fridge

namespace Fridge {

QStringList defaultBasics()
{
    return { QStringLiteral("Wasser"), QStringLiteral("Salz"), QStringLiteral("Pfeffer"), QStringLiteral("Öl"),
             QStringLiteral("Zucker"), QStringLiteral("Essig"), QStringLiteral("Gewürze") };
}

QStringList stock() { return QSettings().value(QStringLiteral("fridge/stock")).toStringList(); }
void setStock(const QStringList &items) { QSettings().setValue(QStringLiteral("fridge/stock"), items); }
QStringList basics() { return QSettings().value(QStringLiteral("fridge/basics"), defaultBasics()).toStringList(); }
void setBasics(const QStringList &items) { QSettings().setValue(QStringLiteral("fridge/basics"), items); }

namespace {
struct StockEntry { QString norm; QString food; };

QList<StockEntry> prepare(const QStringList &stockList)
{
    QList<StockEntry> l;
    static const QRegularExpression amount(QStringLiteral("^[\\d.,/½¼¾\\s]+(?:g|kg|ml|l|stück|st\\.|pck\\.|dose|becher)?\\s+"),
                                           QRegularExpression::CaseInsensitiveOption);
    for (QString s : stockList + basics()) {
        s = s.trimmed();
        s.remove(amount);
        if (s.isEmpty()) continue;
        const Food *f = FoodTable::instance().match(s);
        l << StockEntry{ FoodTable::normalize(s), f ? f->key : QString() };
    }
    return l;
}

bool matches(const QString &name, const QString &food, const QList<StockEntry> &st)
{
    const QString n = FoodTable::normalize(name);
    for (const StockEntry &e : st) {
        // beide zugeordnet: nur dasselbe Lebensmittel zählt („Zucker“ ersetzt keinen „Vanillezucker“)
        if (!food.isEmpty() && !e.food.isEmpty()) {
            if (e.food == food) return true;
            continue;
        }
        if (!e.norm.isEmpty() && (n == e.norm || (QLatin1Char(' ') + n + QLatin1Char(' ')).contains(QLatin1Char(' ') + e.norm + QLatin1Char(' '))))
            return true;
    }
    return false;
}
} // namespace

bool inStock(const QString &ingredientName, const QString &foodKey, const QStringList &stockList)
{
    return matches(ingredientName, foodKey, prepare(stockList));
}

QList<Suggestion> suggest(const QStringList &stockList, int maxMissing, const QStringList &requiredTags)
{
    const QList<StockEntry> st = prepare(stockList);
    FoodTable &table = FoodTable::instance();
    QList<Suggestion> out;
    for (const RecipeSummary &s : RecipeDb::instance().summaries()) {
        bool tagsOk = true;
        for (const QString &t : requiredTags)
            if (!s.tags.contains(t, Qt::CaseInsensitive)) tagsOk = false;
        if (!tagsOk) continue;
        Suggestion sg;
        sg.id = s.id;
        sg.title = s.title;
        int fromStock = 0;
        for (const QString &name : s.ingredientNames) {
            const Food *f = table.match(name);
            const QString key = f ? f->key : QString();
            if (key == QLatin1String("wasser")) continue;
            ++sg.total;
            if (matches(name, key, st)) {
                ++sg.have;
                // zählt nur, wenn es nicht bloß Grundvorrat ist
                bool basic = false;
                for (const QString &b : basics()) {
                    const Food *bf = table.match(b);
                    if ((bf && bf->key == key) || FoodTable::normalize(b) == FoodTable::normalize(name)) basic = true;
                }
                if (!basic) ++fromStock;
            } else {
                QString display = name;
                if (!display.isEmpty()) display[0] = display[0].toUpper();
                sg.missing << display;
            }
        }
        if (sg.total == 0 || fromStock == 0 || sg.missing.size() > maxMissing) continue;
        out << sg;
    }
    std::sort(out.begin(), out.end(), [](const Suggestion &a, const Suggestion &b) {
        if (a.missing.size() != b.missing.size()) return a.missing.size() < b.missing.size();
        if (a.have != b.have) return a.have > b.have;
        return QString::localeAwareCompare(a.title, b.title) < 0;
    });
    return out;
}

QList<ShoppingItem> missingFor(const Recipe &r, const QStringList &stockList, double factor)
{
    const QList<StockEntry> st = prepare(stockList);
    QList<ShoppingItem> list;
    for (const Ingredient &i : r.ingredients) {
        if (i.heading || i.food == QLatin1String("wasser")) continue;
        if (matches(i.name, i.food, st)) continue;
        Shopping::add(list, i, factor, r.title);
    }
    Shopping::sort(list);
    return list;
}

} // namespace Fridge
