// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "webrecipe.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>

namespace WebRecipe {

namespace {

const auto kOpt = QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption;

QString clean(const QString &s)
{
    return decodeEntities(s).simplified();
}

QString textOf(const QJsonValue &v)
{
    if (v.isString()) return clean(v.toString());
    if (v.isDouble()) return QString::number(v.toDouble());
    if (v.isArray()) {
        QStringList parts;
        for (const QJsonValue &e : v.toArray()) {
            const QString t = textOf(e);
            if (!t.isEmpty()) parts << t;
        }
        return parts.join(QStringLiteral(", "));
    }
    if (v.isObject()) {
        const QJsonObject o = v.toObject();
        for (const char *k : { "name", "text", "@value" })
            if (o.contains(QLatin1String(k))) return textOf(o.value(QLatin1String(k)));
    }
    return {};
}

QStringList listOf(const QJsonValue &v)
{
    QStringList out;
    if (v.isArray()) {
        for (const QJsonValue &e : v.toArray()) out << listOf(e);
    } else if (v.isString()) {
        for (const QString &p : v.toString().split(QRegularExpression(QStringLiteral("\\s*[,;]\\s*")), Qt::SkipEmptyParts))
            out << clean(p);
    } else {
        const QString t = textOf(v);
        if (!t.isEmpty()) out << t;
    }
    out.removeAll(QString());
    return out;
}

bool isType(const QJsonObject &o, const char *type)
{
    const QJsonValue t = o.value(QStringLiteral("@type"));
    QStringList types;
    if (t.isString()) types << t.toString();
    for (const QJsonValue &e : t.toArray()) types << e.toString();
    for (QString s : types) {
        s = s.section(QLatin1Char('/'), -1).section(QLatin1Char(':'), -1);
        if (s.compare(QLatin1String(type), Qt::CaseInsensitive) == 0) return true;
    }
    return false;
}

void findRecipes(const QJsonValue &v, QList<QJsonObject> *out, int depth = 0)
{
    if (depth > 12) return;
    if (v.isArray()) {
        for (const QJsonValue &e : v.toArray()) findRecipes(e, out, depth + 1);
        return;
    }
    if (!v.isObject()) return;
    const QJsonObject o = v.toObject();
    if (isType(o, "Recipe")) out->append(o);
    for (auto it = o.begin(); it != o.end(); ++it)
        if (it.value().isObject() || it.value().isArray()) findRecipes(it.value(), out, depth + 1);
}

// Zubereitung: Text, Liste von Texten, HowToStep, HowToSection (mit Unterschritten), ItemList
void collectSteps(const QJsonValue &v, QStringList *steps, int depth = 0)
{
    if (depth > 8) return;
    if (v.isArray()) {
        for (const QJsonValue &e : v.toArray()) collectSteps(e, steps, depth + 1);
        return;
    }
    if (v.isString()) {
        // ein langer Text: HTML-Absätze und Listenpunkte zu Schritten, sonst Zeilen
        QString s = v.toString();
        s.replace(QRegularExpression(QStringLiteral("<\\s*(br|/p|/li|/h\\d)\\s*/?>"), kOpt), QStringLiteral("\n"));
        for (const QString &l : decodeEntities(s).split(QLatin1Char('\n'))) {
            const QString t = l.simplified();
            if (!t.isEmpty()) steps->append(t);
        }
        return;
    }
    if (!v.isObject()) return;
    const QJsonObject o = v.toObject();
    if (isType(o, "HowToSection") || o.contains(QStringLiteral("itemListElement"))) {
        const QString name = textOf(o.value(QStringLiteral("name")));
        if (!name.isEmpty()) steps->append(QStringLiteral("\x01") + name);   // Abschnitt
        collectSteps(o.value(QStringLiteral("itemListElement")), steps, depth + 1);
        return;
    }
    QString t = o.contains(QStringLiteral("text")) ? textOf(o.value(QStringLiteral("text"))) : textOf(o.value(QStringLiteral("name")));
    if (!t.isEmpty()) steps->append(t);
}

QString yieldOf(const QJsonValue &v)
{
    QStringList cand = v.isArray() ? listOf(v) : QStringList{ textOf(v) };
    QString best;
    for (const QString &c : cand) {
        if (!c.contains(QRegularExpression(QStringLiteral("\\d")))) continue;
        if (best.isEmpty() || (c.contains(QRegularExpression(QStringLiteral("[^\\d\\s.,]"))) && !best.contains(QRegularExpression(QStringLiteral("[^\\d\\s.,]")))))
            best = c;
    }
    if (best.isEmpty()) return {};
    if (!best.contains(QRegularExpression(QStringLiteral("[^\\d\\s.,–-]")))) best += QStringLiteral(" Portionen");
    return best;
}

QString imageOf(const QJsonValue &v)
{
    if (v.isString()) return v.toString().trimmed();
    if (v.isArray()) {
        for (const QJsonValue &e : v.toArray()) {
            const QString s = imageOf(e);
            if (!s.isEmpty()) return s;
        }
        return {};
    }
    const QJsonObject o = v.toObject();
    for (const char *k : { "url", "contentUrl", "@id" }) {
        const QString s = o.value(QLatin1String(k)).toString().trimmed();
        if (s.startsWith(QLatin1String("http")) || s.startsWith(QLatin1Char('/'))) return s;
    }
    return {};
}

QString authorOf(const QJsonValue &v)
{
    if (v.isArray()) {
        QStringList names;
        for (const QJsonValue &e : v.toArray()) {
            const QString n = authorOf(e);
            if (!n.isEmpty()) names << n;
        }
        return names.join(QStringLiteral(", "));
    }
    return textOf(v);
}

// JSON-LD-Blöcke sind oft nicht ganz sauber: Kommentare, CDATA, Zeilenumbrüche in Zeichenketten
QJsonDocument parseJsonLoose(QString s)
{
    s.remove(QRegularExpression(QStringLiteral("^\\s*(<!--|//\\s*<!\\[CDATA\\[|<!\\[CDATA\\[)")));
    s.remove(QRegularExpression(QStringLiteral("(-->|//\\s*\\]\\]>|\\]\\]>)\\s*$")));
    QJsonParseError err;
    QJsonDocument d = QJsonDocument::fromJson(s.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError) return d;
    // Steuerzeichen (rohe Zeilenumbrüche in Zeichenketten) sind in JSON nicht erlaubt
    for (QChar &c : s)
        if (c.unicode() < 0x20) c = QLatin1Char(' ');
    d = QJsonDocument::fromJson(s.toUtf8(), &err);
    return err.error == QJsonParseError::NoError ? d : QJsonDocument();
}

Parsed fromJson(const QJsonObject &o, const QUrl &url)
{
    Parsed p;
    p.via = QStringLiteral("JSON-LD");
    p.parts.title = textOf(o.value(QStringLiteral("name")));
    if (p.parts.title.isEmpty()) p.parts.title = textOf(o.value(QStringLiteral("headline")));
    p.parts.yield = yieldOf(o.value(QStringLiteral("recipeYield")));
    if (p.parts.yield.isEmpty()) p.parts.yield = yieldOf(o.value(QStringLiteral("yield")));
    const QJsonValue ingr = o.contains(QStringLiteral("recipeIngredient")) ? o.value(QStringLiteral("recipeIngredient"))
                                                                           : o.value(QStringLiteral("ingredients"));
    if (ingr.isArray()) {
        for (const QJsonValue &e : ingr.toArray()) {
            const QString t = textOf(e);
            if (!t.isEmpty()) p.parts.ingredientLines << t;
        }
    } else if (ingr.isString()) {
        for (const QString &l : decodeEntities(ingr.toString()).split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts))
            if (!l.simplified().isEmpty()) p.parts.ingredientLines << l.simplified();
    }
    QStringList steps;
    collectSteps(o.value(QStringLiteral("recipeInstructions")), &steps);
    int nr = 0;
    for (const QString &s : steps) {
        if (!p.parts.directionLines.isEmpty()) p.parts.directionLines << QString();
        if (s.startsWith(QLatin1Char('\x01'))) {
            p.parts.directionLines << QStringLiteral("**%1:**").arg(s.mid(1));
            continue;
        }
        QString t = s;
        t.remove(QRegularExpression(QStringLiteral("^\\s*\\d+[.)]\\s+")));   // eigene Nummerierung der Seite
        p.parts.directionLines << QStringLiteral("%1. %2").arg(++nr).arg(t);
    }
    for (const char *k : { "recipeCategory", "recipeCuisine" })
        for (const QString &c : listOf(o.value(QLatin1String(k))))
            if (!p.parts.categories.contains(c, Qt::CaseInsensitive) && c.size() <= 40) p.parts.categories << c;
    const QString img = imageOf(o.value(QStringLiteral("image")));
    if (!img.isEmpty()) p.imageUrl = url.resolved(QUrl(img)).toString();
    p.prepMinutes = isoMinutes(textOf(o.value(QStringLiteral("totalTime"))));
    if (p.prepMinutes <= 0)
        p.prepMinutes = isoMinutes(textOf(o.value(QStringLiteral("prepTime")))) + isoMinutes(textOf(o.value(QStringLiteral("cookTime"))));
    p.author = authorOf(o.value(QStringLiteral("author")));
    p.site = textOf(o.value(QStringLiteral("publisher")));
    return p;
}

// --- Microdata: itemprop-Attribute im Seitentext ---

QString attr(const QString &tag, const char *name)
{
    const QRegularExpression re(QStringLiteral("\\b%1\\s*=\\s*(\"([^\"]*)\"|'([^']*)')").arg(QLatin1String(name)), kOpt);
    const auto m = re.match(tag);
    if (!m.hasMatch()) return {};
    return m.captured(2).isNull() ? m.captured(3) : m.captured(2);
}

Parsed fromMicrodata(const QString &html, const QUrl &url)
{
    Parsed p;
    p.via = QStringLiteral("Microdata");
    const int start = html.indexOf(QRegularExpression(QStringLiteral("itemtype\\s*=\\s*[\"']https?://schema\\.org/Recipe[\"']"), kOpt));
    if (start < 0) return p;
    static const QRegularExpression prop(QStringLiteral("<([a-zA-Z][a-zA-Z0-9]*)\\b[^>]*\\bitemprop\\s*=\\s*[\"']([^\"']+)[\"'][^>]*>"), kOpt);
    auto it = prop.globalMatch(html, start);
    QStringList steps;
    while (it.hasNext()) {
        const auto m = it.next();
        const QString tag = m.captured(0);
        const QString el = m.captured(1).toLower();
        const QStringList props = m.captured(2).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        QString value = attr(tag, "content");
        if (value.isEmpty() && (el == QLatin1String("img") || el == QLatin1String("meta") || el == QLatin1String("link")))
            value = attr(tag, el == QLatin1String("img") ? "src" : "href");
        QString inner;
        if (el != QLatin1String("img") && el != QLatin1String("meta") && el != QLatin1String("link")) {
            const int from = m.capturedEnd();
            const int to = html.indexOf(QStringLiteral("</%1").arg(el), from, Qt::CaseInsensitive);
            if (to > from) inner = html.mid(from, qMin(to - from, 20000));
        }
        if (value.isEmpty()) value = inner;
        for (const QString &pr : props) {
            if (pr == QLatin1String("name") && p.parts.title.isEmpty()) p.parts.title = clean(value);
            else if (pr == QLatin1String("recipeIngredient") || pr == QLatin1String("ingredients")) {
                const QString t = clean(value);
                if (!t.isEmpty()) p.parts.ingredientLines << t;
            } else if (pr == QLatin1String("recipeInstructions")) {
                collectSteps(QJsonValue(value), &steps);
            } else if (pr == QLatin1String("recipeYield") && p.parts.yield.isEmpty()) {
                p.parts.yield = yieldOf(QJsonValue(clean(value)));
            } else if (pr == QLatin1String("image") && p.imageUrl.isEmpty()) {
                const QString src = value.startsWith(QLatin1String("http")) || value.startsWith(QLatin1Char('/')) ? value : attr(tag, "src");
                if (!src.isEmpty()) p.imageUrl = url.resolved(QUrl(decodeEntities(src).trimmed())).toString();
            } else if (pr == QLatin1String("totalTime") && p.prepMinutes <= 0) {
                p.prepMinutes = isoMinutes(value.isEmpty() ? attr(tag, "datetime") : value);
            } else if (pr == QLatin1String("recipeCategory")) {
                const QString c = clean(value);
                if (!c.isEmpty() && c.size() <= 40 && !p.parts.categories.contains(c)) p.parts.categories << c;
            } else if (pr == QLatin1String("author") && p.author.isEmpty()) {
                p.author = clean(value);
            }
        }
    }
    int nr = 0;
    for (const QString &s : steps) {
        if (!p.parts.directionLines.isEmpty()) p.parts.directionLines << QString();
        QString t = s;
        t.remove(QRegularExpression(QStringLiteral("^\\s*\\d+[.)]\\s+")));
        p.parts.directionLines << QStringLiteral("%1. %2").arg(++nr).arg(t);
    }
    return p;
}

QString siteName(const QString &html, const QUrl &url)
{
    static const QRegularExpression og(QStringLiteral("<meta[^>]+property\\s*=\\s*[\"']og:site_name[\"'][^>]*>"), kOpt);
    const auto m = og.match(html);
    if (m.hasMatch()) {
        const QString c = clean(attr(m.captured(0), "content"));
        if (!c.isEmpty()) return c;
    }
    QString host = url.host();
    if (host.startsWith(QLatin1String("www."))) host = host.mid(4);
    return host;
}

} // namespace

QString decodeEntities(const QString &in)
{
    QString s = in;
    s.remove(QRegularExpression(QStringLiteral("<script.*?</script>|<style.*?</style>"), kOpt));
    s.replace(QRegularExpression(QStringLiteral("<[^>]*>")), QStringLiteral(" "));
    if (!s.contains(QLatin1Char('&'))) return s;
    static const QHash<QString, QString> named{
        { "amp", "&" }, { "lt", "<" }, { "gt", ">" }, { "quot", "\"" }, { "apos", "'" }, { "nbsp", " " },
        { "ndash", "–" }, { "mdash", "—" }, { "hellip", "…" }, { "frac12", "½" }, { "frac14", "¼" }, { "frac34", "¾" },
        { "frac13", "⅓" }, { "frac23", "⅔" }, { "deg", "°" }, { "auml", "ä" }, { "ouml", "ö" }, { "uuml", "ü" },
        { "Auml", "Ä" }, { "Ouml", "Ö" }, { "Uuml", "Ü" }, { "szlig", "ß" }, { "eacute", "é" }, { "egrave", "è" },
        { "agrave", "à" }, { "aacute", "á" }, { "ntilde", "ñ" }, { "ccedil", "ç" }, { "iacute", "í" }, { "oacute", "ó" },
        { "uacute", "ú" }, { "ecirc", "ê" }, { "acirc", "â" }, { "ocirc", "ô" }, { "icirc", "î" }, { "times", "×" },
        { "lsquo", "‘" }, { "rsquo", "’" }, { "ldquo", "“" }, { "rdquo", "”" }, { "bdquo", "„" }, { "sbquo", "‚" },
        { "laquo", "«" }, { "raquo", "»" }, { "euro", "€" }, { "copy", "©" }, { "reg", "®" }, { "trade", "™" }, { "middot", "·" },
    };
    static const QRegularExpression ent(QStringLiteral("&(#[xX][0-9a-fA-F]+|#\\d+|[a-zA-Z][a-zA-Z0-9]*);"));
    // zweimal: „&amp;amp;“ und „&amp;#8211;“ kommen in JSON-LD vor
    for (int pass = 0; pass < 2 && s.contains(QLatin1Char('&')); ++pass) {
        QString out;
        int last = 0;
        auto it = ent.globalMatch(s);
        while (it.hasNext()) {
            const auto m = it.next();
            out += s.mid(last, m.capturedStart() - last);
            const QString e = m.captured(1);
            QString rep;
            if (e.startsWith(QLatin1Char('#'))) {
                bool ok = false;
                const uint cp = (e.size() > 1 && (e.at(1) == QLatin1Char('x') || e.at(1) == QLatin1Char('X'))) ? e.mid(2).toUInt(&ok, 16) : e.mid(1).toUInt(&ok, 10);
                if (ok && cp > 0 && cp < 0x110000) {
                    if (cp == 0xA0) rep = QStringLiteral(" ");
                    else if (cp > 0xFFFF) rep = QString(QChar(QChar::highSurrogate(cp))) + QChar(QChar::lowSurrogate(cp));
                    else rep = QString(QChar(ushort(cp)));
                }
            } else {
                rep = named.value(e);
            }
            out += rep.isNull() ? m.captured(0) : rep;
            last = m.capturedEnd();
        }
        out += s.mid(last);
        s = out;
    }
    return s;
}

int isoMinutes(const QString &iso)
{
    static const QRegularExpression re(QStringLiteral("^P(?:(\\d+)D)?(?:T(?:(\\d+(?:\\.\\d+)?)H)?(?:(\\d+(?:\\.\\d+)?)M)?(?:(\\d+)S)?)?$"),
                                       QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(iso.trimmed());
    if (!m.hasMatch()) return 0;
    const double min = m.captured(1).toDouble() * 1440 + m.captured(2).toDouble() * 60 + m.captured(3).toDouble() + m.captured(4).toDouble() / 60.0;
    return int(min + 0.5);
}

QStringList addressesIn(const QString &text)
{
    QStringList out;
    static const QRegularExpression re(QStringLiteral("https?://[^\\s<>\"'„“]+"), QRegularExpression::CaseInsensitiveOption);
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        QString u = it.next().captured(0);
        while (!u.isEmpty() && QStringLiteral(".,;:!?)]}").contains(u.back())) u.chop(1);
        if (!u.isEmpty() && !out.contains(u)) out << u;
    }
    return out;
}

Parsed parseHtml(const QString &html, const QUrl &url)
{
    // 1. JSON-LD
    static const QRegularExpression ld(QStringLiteral("<script[^>]*type\\s*=\\s*[\"']?application/ld\\+json[\"']?[^>]*>(.*?)</script>"), kOpt);
    QList<QJsonObject> found;
    auto it = ld.globalMatch(html);
    while (it.hasNext()) {
        const QJsonDocument d = parseJsonLoose(it.next().captured(1).trimmed());
        if (d.isObject()) findRecipes(d.object(), &found);
        else if (d.isArray()) findRecipes(d.array(), &found);
    }
    Parsed best;
    for (const QJsonObject &o : found) {
        Parsed p = fromJson(o, url);
        if (p.parts.ingredientLines.size() > best.parts.ingredientLines.size() || best.parts.title.isEmpty()) best = p;
    }
    // 2. Microdata, wenn JSON-LD fehlt oder keine Zutaten hat
    if (best.parts.ingredientLines.isEmpty()) {
        const Parsed m = fromMicrodata(html, url);
        if (!m.parts.ingredientLines.isEmpty()) best = m;
    }
    if (best.parts.ingredientLines.isEmpty()) {
        best.ok = false;
        best.error = found.isEmpty()
                         ? QStringLiteral("Auf dieser Seite wurde kein maschinenlesbares Rezept gefunden (schema.org/Recipe). "
                                          "Manche Seiten bieten es nicht an – dann hilft Abtippen oder Datei → Import → Rezept aus Bild oder Foto erkennen (Bildschirmfoto der Seite).")
                         : QStringLiteral("Das Rezept auf dieser Seite enthält keine Zutatenliste.");
        return best;
    }
    if (best.parts.title.isEmpty()) {
        static const QRegularExpression title(QStringLiteral("<title[^>]*>(.*?)</title>"), kOpt);
        best.parts.title = clean(title.match(html).captured(1)).section(QRegularExpression(QStringLiteral("\\s[|–-]\\s")), 0, 0);
    }
    if (best.site.isEmpty()) best.site = siteName(html, url);
    // Quelle: Autor, Website, Adresse
    QStringList src;
    if (!best.author.isEmpty() && best.author.compare(best.site, Qt::CaseInsensitive) != 0) src << best.author;
    if (!best.site.isEmpty()) src << best.site;
    best.parts.sourceLines << (src.isEmpty() ? url.toString() : src.join(QStringLiteral(", ")) + QStringLiteral(" – ") + url.toString());
    best.ok = true;
    return best;
}

Recipe toRecipe(const Parsed &p, const QUrl &url, const Importers::Options &opt)
{
    Importers::Result res = Importers::fromParts(p.parts, QStringLiteral("Web"), opt);
    Recipe r = res.recipes.isEmpty() ? Recipe() : res.recipes.first();
    if (p.prepMinutes > 0) r.prepMinutes = p.prepMinutes;
    if (r.source.trimmed().isEmpty()) r.source = url.toString();
    return r;
}

} // namespace WebRecipe
