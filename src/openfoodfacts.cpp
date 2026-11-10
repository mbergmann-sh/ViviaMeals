// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "openfoodfacts.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrlQuery>

namespace OpenFoodFacts {

namespace {

QString tr(const char *s) { return QCoreApplication::translate("OpenFoodFacts", s); }

const char *const kFields = "code,product_name,product_name_de,generic_name_de,brands,quantity,product_quantity,product_quantity_unit,"
                            "serving_size,serving_quantity,nutriments,nutrition_data_per,categories_tags,image_front_small_url";

double num(const QJsonObject &o, const QString &key, bool *present = nullptr)
{
    const QJsonValue v = o.value(key);
    bool ok = false;
    double d = 0;
    if (v.isDouble()) {
        d = v.toDouble();
        ok = true;
    } else if (v.isString()) {
        d = QString(v.toString()).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
    }
    if (present) *present = ok;
    return ok ? qMax(0.0, d) : 0.0;
}

} // namespace

QString baseUrl()
{
    const QString env = qEnvironmentVariable("VIVIA_OFF_URL");
    QString b = env.isEmpty() ? QStringLiteral("https://world.openfoodfacts.org") : env;
    while (b.endsWith(QLatin1Char('/'))) b.chop(1);
    return b;
}

QByteArray userAgent()
{
    // Open Food Facts verlangt eine eigene Kennung (Programm/Version, Kontakt)
    return QStringLiteral("ViviaMeals/%1 (https://github.com/mbergmann-sh/ViviaMeals)")
        .arg(QCoreApplication::applicationVersion().isEmpty() ? QStringLiteral("0") : QCoreApplication::applicationVersion())
        .toLatin1();
}

bool validGtin(const QString &d)
{
    if (!(d.size() == 8 || d.size() == 12 || d.size() == 13 || d.size() == 14)) return false;
    int sum = 0;
    // von rechts (ohne Prüfziffer): abwechselnd ×3 und ×1
    for (int i = int(d.size()) - 2, k = 0; i >= 0; --i, ++k) sum += (d.at(i).unicode() - '0') * (k % 2 == 0 ? 3 : 1);
    return (10 - sum % 10) % 10 == d.at(d.size() - 1).unicode() - '0';
}

QString normalizeBarcode(const QString &text, QString *error)
{
    QString d;
    for (const QChar c : text)
        if (c.isDigit()) d += c;
        else if (!c.isSpace() && c != QLatin1Char('-')) {
            if (error) *error = tr("Ein Barcode besteht nur aus Ziffern.");
            return {};
        }
    if (d.isEmpty()) {
        if (error) *error = tr("Bitte die Ziffern unter dem Strichcode eingeben.");
        return {};
    }
    if (!(d.size() == 8 || d.size() == 12 || d.size() == 13 || d.size() == 14)) {
        if (error) *error = tr("Ein Barcode hat 8 oder 13 Ziffern (UPC 12, GTIN 14) – eingegeben: %1.").arg(d.size());
        return {};
    }
    if (!validGtin(d)) {
        if (error) *error = tr("Die Prüfziffer (letzte Ziffer) passt nicht – bitte auf Tippfehler prüfen.");
        return {};
    }
    if (error) error->clear();
    return d;
}

QUrl productUrl(const QString &code)
{
    QUrl u(baseUrl() + QStringLiteral("/api/v2/product/%1.json").arg(code));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("fields"), QLatin1String(kFields));
    u.setQuery(q);
    return u;
}

QUrl searchUrl(const QString &terms, int pageSize)
{
    QUrl u(baseUrl() + QStringLiteral("/cgi/search.pl"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("search_terms"), terms.simplified());
    q.addQueryItem(QStringLiteral("search_simple"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("action"), QStringLiteral("process"));
    q.addQueryItem(QStringLiteral("json"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("page_size"), QString::number(pageSize));
    q.addQueryItem(QStringLiteral("fields"), QLatin1String(kFields));
    u.setQuery(q);
    return u;
}

QString productPage(const QString &code)
{
    return QStringLiteral("https://de.openfoodfacts.org/produkt/%1").arg(code);
}

double parseQuantity(const QString &text, bool *perMl)
{
    // „400 g“, „1,5 l“, „250ml“, „6 x 125 g“, „0.5 kg“
    static const QRegularExpression re(QStringLiteral("(?:(\\d+)\\s*[x×]\\s*)?(\\d+(?:[.,]\\d+)?)\\s*(kg|g|mg|ml|cl|dl|l|liter)\\b"),
                                       QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(text);
    if (!m.hasMatch()) return 0;
    double v = QString(m.captured(2)).replace(QLatin1Char(','), QLatin1Char('.')).toDouble();
    if (!m.captured(1).isEmpty()) v *= m.captured(1).toInt();
    const QString u = m.captured(3).toLower();
    bool ml = false;
    if (u == QLatin1String("kg")) v *= 1000;
    else if (u == QLatin1String("mg")) v /= 1000;
    else if (u == QLatin1String("ml")) ml = true;
    else if (u == QLatin1String("cl")) v *= 10, ml = true;
    else if (u == QLatin1String("dl")) v *= 100, ml = true;
    else if (u == QLatin1String("l") || u == QLatin1String("liter")) v *= 1000, ml = true;
    if (perMl) *perMl = ml;
    return v;
}

QString categoryFromTags(const QStringList &tags)
{
    // spezifisch vor allgemein
    struct Rule { const char *tag; const char *cat; };
    static const Rule rules[] = {
        { "en:alcoholic-beverages", "alkohol" }, { "en:cheeses", "kaese" },       { "en:fishes", "fisch" },
        { "en:seafood", "fisch" },               { "en:meats", "fleisch" },       { "en:sausages", "fleisch" },
        { "en:eggs", "ei" },                     { "en:honeys", "honig" },        { "en:sugars", "zucker" },
        { "en:syrups", "zucker" },               { "en:chocolates", "suess" },    { "en:confectioneries", "suess" },
        { "en:biscuits-and-cakes", "backwaren" }, { "en:breads", "backwaren" },   { "en:sauces", "sosse" },
        { "en:condiments", "sosse" },            { "en:dressings", "sosse" },     { "en:soups", "sosse" },
        { "en:broths", "sosse" },                { "en:spices", "gewuerz" },      { "en:herbs", "kraeuter" },
        { "en:fats", "fett" },                   { "en:vegetable-oils", "fett" }, { "en:butters", "fett" },
        { "en:margarines", "fett" },             { "en:dairies", "milch" },       { "en:milks", "milch" },
        { "en:yogurts", "milch" },               { "en:legumes", "huelsen" },     { "en:tofu", "huelsen" },
        { "en:nuts", "nuss" },                   { "en:seeds", "nuss" },          { "en:potatoes", "kartoffel" },
        { "en:pastas", "getreide" },             { "en:rices", "getreide" },      { "en:flours", "getreide" },
        { "en:cereals-and-their-products", "getreide" }, { "en:breakfast-cereals", "getreide" },
        { "en:fruits", "obst" },                 { "en:vegetables", "gemuese" },  { "en:beverages", "getraenk" },
        { "en:desserts", "suess" }, { "en:sweet-spreads", "suess" },              { "en:sweet-snacks", "suess" },  { "en:snacks", "sonstiges" },
    };
    for (const Rule &r : rules)
        if (tags.contains(QLatin1String(r.tag))) return QLatin1String(r.cat);
    return QString();
}

Product parseProduct(const QJsonObject &p)
{
    Product r;
    r.code = p.value(QStringLiteral("code")).toString();
    for (const char *k : { "product_name_de", "product_name", "generic_name_de" }) {
        r.name = p.value(QLatin1String(k)).toString().simplified();
        if (!r.name.isEmpty()) break;
    }
    r.brand = p.value(QStringLiteral("brands")).toString().section(QLatin1Char(','), 0, 0).simplified();
    r.quantity = p.value(QStringLiteral("quantity")).toString().simplified();
    const QString qUnit = p.value(QStringLiteral("product_quantity_unit")).toString().toLower();
    bool ml = false;
    r.packageAmount = parseQuantity(r.quantity, &ml);
    if (r.packageAmount <= 0) {
        r.packageAmount = num(p, QStringLiteral("product_quantity"));
        ml = qUnit == QLatin1String("ml");
    }
    r.perMl = ml || qUnit == QLatin1String("ml");
    r.servingSize = p.value(QStringLiteral("serving_size")).toString().simplified();
    r.servingGrams = num(p, QStringLiteral("serving_quantity"));
    QStringList tags;
    for (const QJsonValue &v : p.value(QStringLiteral("categories_tags")).toArray()) tags << v.toString();
    r.category = categoryFromTags(tags);
    r.imageUrl = p.value(QStringLiteral("image_front_small_url")).toString();

    const QJsonObject n = p.value(QStringLiteral("nutriments")).toObject();
    bool has = false, any = false;
    double kcal = num(n, QStringLiteral("energy-kcal_100g"), &has);
    if (!has) {
        const double kj = num(n, QStringLiteral("energy-kj_100g"), &has);
        if (has) kcal = kj / 4.184;
        else {
            const double e = num(n, QStringLiteral("energy_100g"), &has);   // in kJ
            if (has) kcal = e / 4.184;
        }
    }
    if (has) any = true;
    else r.missing << tr("Energie");
    r.per100.kcal = kcal;
    struct F { const char *key; double Nutrients::*m; const char *label; };
    static const F fields[] = {
        { "fat_100g", &Nutrients::fat, "Fett" },
        { "saturated-fat_100g", &Nutrients::satFat, "gesättigte Fettsäuren" },
        { "carbohydrates_100g", &Nutrients::carbs, "Kohlenhydrate" },
        { "sugars_100g", &Nutrients::sugar, "Zucker" },
        { "fiber_100g", &Nutrients::fiber, "Ballaststoffe" },
        { "proteins_100g", &Nutrients::protein, "Eiweiß" },
    };
    for (const F &f : fields) {
        bool ok = false;
        r.per100.*f.m = num(n, QLatin1String(f.key), &ok);
        if (ok) any = true;
        else r.missing << tr(f.label);
    }
    bool ok = false;
    r.per100.salt = num(n, QStringLiteral("salt_100g"), &ok);
    if (!ok) {
        const double sodium = num(n, QStringLiteral("sodium_100g"), &ok);
        if (ok) r.per100.salt = sodium * 2.5;
        else r.missing << tr("Salz");
    }
    if (ok) any = true;
    r.hasNutrition = any;
    return r;
}

bool parseProductReply(const QByteArray &json, Product *out, QString *error)
{
    QJsonParseError pe;
    const QJsonDocument d = QJsonDocument::fromJson(json, &pe);
    if (!d.isObject()) {
        if (error) *error = tr("Unerwartete Antwort von Open Food Facts (%1).").arg(pe.errorString());
        return false;
    }
    const QJsonObject o = d.object();
    const QJsonObject p = o.value(QStringLiteral("product")).toObject();
    if (o.value(QStringLiteral("status")).toInt() != 1 || p.isEmpty()) {
        if (error) *error = tr("Dieses Produkt ist in Open Food Facts (noch) nicht erfasst.");
        return false;
    }
    Product r = parseProduct(p);
    if (r.code.isEmpty()) r.code = o.value(QStringLiteral("code")).toString();
    if (out) *out = r;
    return true;
}

QList<Product> parseSearchReply(const QByteArray &json, QString *error)
{
    QList<Product> list;
    const QJsonDocument d = QJsonDocument::fromJson(json);
    if (!d.isObject()) {
        if (error) *error = tr("Unerwartete Antwort von Open Food Facts.");
        return list;
    }
    for (const QJsonValue &v : d.object().value(QStringLiteral("products")).toArray()) {
        const Product p = parseProduct(v.toObject());
        if (!p.name.isEmpty() && !p.code.isEmpty()) list << p;
    }
    if (error) error->clear();
    return list;
}

// ---------------------------------------------------------------- Client

Client::Client(QObject *parent) : QObject(parent), m_nam(new QNetworkAccessManager(this)) {}

void Client::lookup(const QString &code) { get(productUrl(code), false); }
void Client::search(const QString &terms) { get(searchUrl(terms), true); }

void Client::cancel()
{
    if (!m_reply) return;
    QNetworkReply *r = m_reply;
    m_reply = nullptr;
    r->disconnect(this);
    r->abort();
    r->deleteLater();
}

void Client::get(const QUrl &url, bool isSearch)
{
    cancel();
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", userAgent());
    req.setRawHeader("Accept", "application/json");
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setTransferTimeout(isSearch ? 30000 : 15000);
#endif
    m_reply = m_nam->get(req);
    QNetworkReply *r = m_reply;
    connect(r, &QNetworkReply::finished, this, [this, r, isSearch] {
        if (r != m_reply) return;
        m_reply = nullptr;
        r->deleteLater();
        const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = r->readAll();
        if (status == 404 && !isSearch) {
            // v2 antwortet bei unbekannten Produkten mit 404 und status 0
            Product p;
            QString e;
            if (!parseProductReply(body, &p, &e)) emit failed(e.isEmpty() ? tr("Dieses Produkt ist in Open Food Facts (noch) nicht erfasst.") : e);
            else emit productFound(p);
            return;
        }
        if (status == 429) {
            emit failed(tr("Open Food Facts erlaubt nur wenige Suchen je Minute – bitte kurz warten und erneut versuchen."));
            return;
        }
        if (r->error() != QNetworkReply::NoError) {
            emit failed(tr("Open Food Facts ist nicht erreichbar: %1").arg(r->errorString()));
            return;
        }
        QString e;
        if (isSearch) {
            const QList<Product> list = parseSearchReply(body, &e);
            if (!e.isEmpty()) emit failed(e);
            else emit searchFinished(list);
        } else {
            Product p;
            if (parseProductReply(body, &p, &e)) emit productFound(p);
            else emit failed(e);
        }
    });
}

} // namespace OpenFoodFacts
