// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "recipescan.h"
#include "units.h"
#include "foodtable.h"

#include <QColor>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <climits>
#include <cmath>
#include <vector>

namespace RecipeScan {

// ---------------------------------------------------------------- Bild

QImage normalize(const QImage &img)
{
    if (img.isNull()) return img;
    QImage out = img.convertToFormat(QImage::Format_RGB32);
    const int longSide = qMax(out.width(), out.height());
    // Handyfotos und kleine Scans vergrößern (Tesseract mag ca. 30 px hohe Großbuchstaben),
    // riesige Bilder verkleinern (Laufzeit)
    if (longSide < 1600) out = out.scaled(out.size() * 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    else if (longSide > 5200) out = out.scaled(QSize(5200, 5200), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return out;
}

QList<QRect> findPhotos(const QImage &src)
{
    QList<QRect> found;
    if (src.isNull() || src.width() < 50 || src.height() < 50) return found;
    // Stichprobe ohne Glättung (nächster Nachbar): Text bleibt schwarz/weiß, Fotos behalten ihre
    // Mitteltöne und Farben. Geglättet würde Text zu grauer Fläche.
    const int W = 400;
    const QImage s = src.convertToFormat(QImage::Format_RGB32).scaledToWidth(W, Qt::FastTransformation);
    const int T = 10;
    const int cols = s.width() / T, rows = s.height() / T;
    if (cols < 4 || rows < 4) return found;
    std::vector<char> photo(size_t(cols * rows), 0);
    for (int ty = 0; ty < rows; ++ty) {
        for (int tx = 0; tx < cols; ++tx) {
            int n = 0, mid = 0, colorful = 0;
            for (int y = ty * T; y < (ty + 1) * T; ++y) {
                const QRgb *line = reinterpret_cast<const QRgb *>(s.constScanLine(y));
                for (int x = tx * T; x < (tx + 1) * T; ++x) {
                    const QRgb p = line[x];
                    const int r = qRed(p), g = qGreen(p), b = qBlue(p);
                    const int mx = qMax(r, qMax(g, b)), mn = qMin(r, qMin(g, b));
                    const int lum = (r * 299 + g * 587 + b * 114) / 1000;
                    ++n;
                    if (lum >= 60 && lum <= 195) ++mid;
                    if (mx > 40 && mx < 250 && (mx - mn) * 100 > 28 * mx) ++colorful;
                }
            }
            photo[size_t(ty * cols + tx)] = (colorful * 100 > 40 * n || mid * 100 > 55 * n) ? 1 : 0;
        }
    }
    // Zusammenhängende Bereiche (Lücken von einer Kachel werden überbrückt)
    std::vector<int> label(size_t(cols * rows), -1);
    auto at = [&](int x, int y) { return x >= 0 && y >= 0 && x < cols && y < rows && photo[size_t(y * cols + x)]; };
    struct Comp { int minX, minY, maxX, maxY, count; };
    std::vector<Comp> comps;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            if (!at(x, y) || label[size_t(y * cols + x)] >= 0) continue;
            Comp c{ x, y, x, y, 0 };
            const int id = int(comps.size());
            std::vector<std::pair<int, int>> stack{ { x, y } };
            label[size_t(y * cols + x)] = id;
            while (!stack.empty()) {
                const auto [cx, cy] = stack.back();
                stack.pop_back();
                ++c.count;
                c.minX = qMin(c.minX, cx); c.maxX = qMax(c.maxX, cx);
                c.minY = qMin(c.minY, cy); c.maxY = qMax(c.maxY, cy);
                for (int dy = -2; dy <= 2; ++dy)
                    for (int dx = -2; dx <= 2; ++dx) {
                        const int nx = cx + dx, ny = cy + dy;
                        if (!at(nx, ny) || label[size_t(ny * cols + nx)] >= 0) continue;
                        label[size_t(ny * cols + nx)] = id;
                        stack.push_back({ nx, ny });
                    }
            }
            comps.push_back(c);
        }
    }
    const double fx = double(src.width()) / s.width(), fy = double(src.height()) / s.height();
    const int total = cols * rows;
    for (const Comp &c : comps) {
        const int w = c.maxX - c.minX + 1, h = c.maxY - c.minY + 1;
        if (w < 4 || h < 4) continue;                          // Linien, farbige Überschriften
        if (c.count < qMax(16, total / 60)) continue;          // zu klein
        if (c.count * 100 < 45 * w * h) continue;              // zu löchrig (Textblock mit Farbe)
        if (w * h * 100 > 85 * total) continue;                // ganze Seite farbig/grau → kein Foto
        QRect r(int(c.minX * T * fx), int(c.minY * T * fy), int(w * T * fx), int(h * T * fy));
        r.adjust(-int(T * fx / 2), -int(T * fy / 2), int(T * fx / 2), int(T * fy / 2));
        found << r.intersected(src.rect());
    }
    std::sort(found.begin(), found.end(), [](const QRect &a, const QRect &b) {
        return a.width() * a.height() > b.width() * b.height();
    });
    return found;
}

QImage blank(const QImage &img, const QList<QRect> &areas)
{
    if (areas.isEmpty()) return img;
    QImage out = img.convertToFormat(QImage::Format_RGB32);
    QPainter p(&out);
    for (const QRect &r : areas) p.fillRect(r, Qt::white);
    return out;
}

// ---------------------------------------------------------------- Zeilen korrigieren

namespace {

QString foldWord(const QString &w)
{
    QString f = w;
    f.replace(QChar(0xE4), QLatin1Char('a')).replace(QChar(0xF6), QLatin1Char('o')).replace(QChar(0xFC), QLatin1Char('u'));
    return f;
}

struct Vocabulary {
    QHash<QString, QString> umlaut;   // „krauter“ → „kräuter“
    QSet<QString> plain;              // bekannte Wörter ohne Umlaut („apfel“, „quark“)
};

const Vocabulary &vocabulary()
{
    static const Vocabulary v = [] {
        Vocabulary voc;
        auto add = [&voc](const QString &text) {
            static const QRegularExpression sep(QStringLiteral("[^\\p{L}]+"));
            for (const QString &raw : text.toLower().split(sep, Qt::SkipEmptyParts)) {
                if (raw.size() < 2) continue;
                const QString f = foldWord(raw);
                if (f != raw) {
                    if (!voc.umlaut.contains(f)) voc.umlaut.insert(f, raw);
                } else {
                    voc.plain.insert(raw);
                }
            }
        };
        for (const Food *f : FoodTable::instance().all()) {
            add(f->name);
            for (const QString &a : f->aliases) add(a);
        }
        // häufige Wörter in Rezepten
        add(QStringLiteral(
            "für über schälen dünsten andünsten würfeln würfel würfelchen öl äpfel möhren rührei rühren unterrühren verrühren "
            "glattrühren brühe gemüse kräuter hähnchen knödel löffel teelöffel esslöffel stück stücke stückchen päckchen füllung "
            "soße süß süße säuerlich größe groß große großen größere gießen aufgießen übergießen angießen häufig fläche schüssel "
            "backröhre rösten anrösten bräunen köcheln hälfte hälften würze würzen würzig frühlingszwiebeln frühstück mürbeteig "
            "blätterteig käse rührteig kühlschrank kühl kühlen abkühlen auskühlen grün grüne grünen gewürze gewürz nüsse "
            "mandelblättchen körner brötchen röllchen würstchen weiß weiße rübe rüben bärlauch rücken früchte grünkohl spätzle "
            "klöße stärke speisestärke röstzwiebeln ungefähr nächste später dafür darüber zurück müssen möglichst gleichmäßig "
            "dünn dünne dünnen hinzufügen zufügen fügen hüllen löst lösen auflösen schöpfen abschöpfen übrige übrigen täglich "
            "gefäß förmchen backförmchen höhe höchstens mürbe zähflüssig flüssig flüssigkeit trüb hitze herdplatte ofenhitze "
            "wärme warmhalten erwärmen aufwärmen vorwärmen bröseln semmelbrösel paniermehl öffnen geöffnet schütteln "
            "füllen gefüllt stürzen überbacken überbrühen übrig rühre flädle fladle kräuterflädle backröhre röstbrot"));
        return voc;
    }();
    return v;
}

QString caseLike(const QString &model, const QString &word)
{
    if (model.isEmpty() || word.isEmpty()) return word;
    if (model.size() > 1 && model == model.toUpper()) return word.toUpper();
    if (model.at(0).isUpper()) return word.at(0).toUpper() + word.mid(1);
    return word;
}

// „Krauterquark“ → „Kräuterquark“, „Gemuse“ → „Gemüse“, „Ol“ → „Öl“ (Tesseract verliert die Pünktchen)
QString restoreWord(const QString &w)
{
    const Vocabulary &v = vocabulary();
    const QString lower = w.toLower();
    if (lower.size() < 2 || foldWord(lower) != lower || v.plain.contains(lower)) return w;
    const auto it = v.umlaut.constFind(lower);
    if (it != v.umlaut.constEnd()) return caseLike(w, it.value());
    if (lower.size() < 7) return w;
    // Zusammensetzungen aus zwei bekannten Wörtern, mindestens eines mit Umlaut
    for (int i = 3; i <= lower.size() - 3; ++i) {
        const QString a = lower.left(i), b = lower.mid(i);
        const bool aU = v.umlaut.contains(a), bU = v.umlaut.contains(b);
        if (!aU && !bU) continue;
        if (!(aU || v.plain.contains(a)) || !(bU || v.plain.contains(b))) continue;
        const QString joined = (aU ? v.umlaut.value(a) : a) + (bU ? v.umlaut.value(b) : b);
        return caseLike(w, joined);
    }
    return w;
}

} // namespace

QString restoreUmlauts(const QString &line)
{
    static const QRegularExpression word(QStringLiteral("\\p{L}+"));
    QString out;
    int last = 0;
    auto it = word.globalMatch(line);
    while (it.hasNext()) {
        const auto m = it.next();
        out += line.mid(last, m.capturedStart() - last) + restoreWord(m.captured());
        last = m.capturedEnd();
    }
    return out + line.mid(last);
}

QString fixLine(const QString &line)
{
    QString s = line;
    s.replace(QChar(0xFB01), QStringLiteral("fi")).replace(QChar(0xFB02), QStringLiteral("fl"))
        .replace(QChar(0xFB00), QStringLiteral("ff")).replace(QChar(0x2044), QLatin1Char('/'));
    s.replace(QChar(0x201E), QLatin1Char('"')).replace(QChar(0x201C), QLatin1Char('"')).replace(QChar(0x201D), QLatin1Char('"'));
    s = s.simplified();
    // Rahmen- und Schmutzzeichen am Anfang
    static const QRegularExpression junk(QStringLiteral("^[|_~`'\\\\]+\\s*"));
    s.remove(junk);
    // Brüche: l/2, I/2, |/2 → ½; V2/Va am Zeilenanfang (so liest Tesseract oft „½“)
    // Aufzählungszeichen vor einer Menge, oft als °, «, ¢, e, o gelesen
    static const QRegularExpression bullet(QStringLiteral("^(?:[•·°«»¢©®*■□▪●◦○>\\-–—]|[eoc](?=\\s+[\\d½¼¾⅓⅔]))\\s*(?=[\\d½¼¾⅓⅔%]|[lI|]/[24]|V[2a]\\s)"));
    s.remove(bullet);
    static const QRegularExpression vHalf(QStringLiteral("^(?:V[2a]|%|Y2|y2)\\s+"));
    s.replace(vHalf, QStringLiteral("½ "));
    static const QRegularExpression half(QStringLiteral("(^|\\s)[lI|]/2(?=\\s|$)"));
    s.replace(half, QStringLiteral("\\1½"));
    static const QRegularExpression quarter(QStringLiteral("(^|\\s)[lI|]/4(?=\\s|$)"));
    s.replace(quarter, QStringLiteral("\\1¼"));
    // Zahlen mit O/o statt 0 bzw. I/l statt 1 („1OO g“, „l2 Eier“) – nur reine Zahlwörter;
    // ein l am Ende bleibt (1l = 1 Liter)
    static const QRegularExpression numTok(QStringLiteral("(?<![\\p{L}\\d])([\\dOoIl]*\\d[\\dOoIl]*)(?![\\p{L}\\d])"));
    QString out;
    int last = 0;
    auto it = numTok.globalMatch(s);
    while (it.hasNext()) {
        const auto m = it.next();
        QString tok = m.captured(1);
        QString fixed;
        for (int i = 0; i < tok.size(); ++i) {
            const QChar c = tok.at(i);
            if (c == QLatin1Char('O') || c == QLatin1Char('o')) fixed += QLatin1Char('0');
            else if ((c == QLatin1Char('I') || c == QLatin1Char('l')) && i + 1 < tok.size()) fixed += QLatin1Char('1');
            else fixed += c;
        }
        out += s.mid(last, m.capturedStart(1) - last) + fixed;
        last = m.capturedEnd(1);
    }
    s = out + s.mid(last);
    // Einheiten: El/EI → EL, Tl/TI → TL nach einer Menge
    static const QRegularExpression el(QStringLiteral("([\\d½¼¾])\\s*E[lI1|](?=[\\s.]|$)"));
    s.replace(el, QStringLiteral("\\1 EL"));
    static const QRegularExpression tl(QStringLiteral("([\\d½¼¾])\\s*T[lI1|](?=[\\s.]|$)"));
    s.replace(tl, QStringLiteral("\\1 TL"));
    // „75g Mehl“, „6EL Sahne“, „1l Brühe“ → mit Leerzeichen (wie im Rezepteditor üblich)
    static const QRegularExpression glued(QStringLiteral("^((?:ca\\.\\s*)?[\\d½¼¾][\\d,./½¼¾]*)(g|kg|ml|l|cl|dl|EL|TL)(?=\\s|$)"));
    s.replace(glued, QStringLiteral("\\1 \\2"));
    // „1 l“ wird oft als „11“ gelesen: vor Flüssigkeiten ist „11 Brühe“ bzw. „0,51 Milch“ fast sicher ein Liter-Wert
    static const QRegularExpression liter(QStringLiteral(
        "^([1-9]|\\d+,\\d)1\\s+(?=(?:\\p{L}+\\s)?\\p{L}*(?:brühe|bruhe|fond|milch|wasser|wein|saft|bier|sahne|öl\\b|essig))"),
        QRegularExpression::CaseInsensitiveOption);
    s.replace(liter, QStringLiteral("\\1 l "));
    // Temperaturen: 180 *C, 180 ºC, 180°C, 180 Grad C → 180° C
    static const QRegularExpression deg(QStringLiteral("(\\d)\\s*\\*\\s*C\\b"));
    s.replace(deg, QStringLiteral("\\1°C"));
    s = Units::normalizeCelsius(s);
    return restoreUmlauts(s).trimmed();
}

// ---------------------------------------------------------------- Aufteilen

namespace {

const QRegularExpression &ingHeaderRe()
{
    static const QRegularExpression re(QStringLiteral(
        "^(?:die\\s+)?(?:zutaten|ingredients?|einkaufsliste|das brauchen sie|sie brauchen|man braucht|"
        "you(?:'|’)?ll need|what you need)\\b\\s*:?\\s*(.{0,40})$"), QRegularExpression::CaseInsensitiveOption);
    return re;
}

const QRegularExpression &dirHeaderRe()
{
    static const QRegularExpression re(QStringLiteral(
        "^(?:zubereitung|so wird(?:'|’|´)?s gemacht|so geht(?:'|’|´)?s|und so geht(?:'|’|´)?s|anleitung|arbeitsschritte|"
        "zubereitungsschritte|directions|instructions|method|preparation)\\b\\s*:?\\s*(.*)$"),
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

const QRegularExpression &metaRe()
{
    static const QRegularExpression re(QStringLiteral(
        "^(?:zubereitungszeit|vorbereitungszeit|arbeitszeit|garzeit|backzeit|kochzeit|ruhezeit|gesamtzeit|zeitaufwand|zeit|"
        "schwierigkeit(?:sgrad)?|pro portion|pro stück|pro person|nährwerte|kalorien|brennwert|energie|"
        "prep(?:aration)? time|cook(?:ing)? time|total time|difficulty|per serving|nutrition)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

const QRegularExpression &amountRe()
{
    // Menge am Zeilenanfang, aber keine Schrittnummer „1. Den Ofen …“
    static const QRegularExpression re(QStringLiteral(
        "^(?:ca\\.?\\s*|etwa\\s+|je\\s+)?(?:\\d+(?:[.,/]\\d+)?|[½¼¾⅓⅔⅛]|\\d+\\s*[½¼¾])(?!\\s*[.)]\\s+\\p{Lu}\\p{Ll}{2,}\\s+\\p{Ll})"),
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

const QRegularExpression &unitStartRe()
{
    static const QRegularExpression re(QStringLiteral(
        "^(?:EL|TL|Prise|Prisen|Msp\\.?|Messerspitze|Bund|Dose|Pck\\.?|Päckchen|etwas|einige|wenig|reichlich|"
        "Salz|Pfeffer|Öl\\b|Butter für|Mehl für|Fett für|n\\.\\s*B\\.|nach Belieben|evtl\\.|"
        "a pinch|pinch|salt|pepper|some)\\b"));
    return re;
}

const QRegularExpression &bulletRe()
{
    static const QRegularExpression re(QStringLiteral("^(?:[•\\-–—*■□▪●·◦○»«>°¢©®]|[oe](?=\\s+\\p{Lu}|\\s+\\d))\\s*"));
    return re;
}

const QRegularExpression &stepRe()
{
    static const QRegularExpression re(QStringLiteral("^\\d{1,2}\\s*[.)]\\s+\\p{Lu}"));
    return re;
}

// nur eine Mengenangabe („75 g“, „1“, „½ Bund“, „1 Handvoll“) – die Zutat steht in einer eigenen Spalte
bool amountOnly(const QString &l)
{
    static const QRegularExpression re(QStringLiteral(
        "^(?:ca\\.?\\s*)?[\\d½¼¾⅓⅔⅛][\\d.,/½¼¾\\s–-]*\\s*"
        "(?:g|kg|mg|ml|l|cl|dl|EL|TL|Msp\\.?|Messerspitzen?|Prisen?|Bund|Handvoll|Stück|Stk\\.?|Pck\\.?|Päckchen|Dosen?|Becher|"
        "Zehen?|Tassen?|Scheiben?|Gläser|Glas|Würfel|Blatt|Blätter|Zweige?|Stangen?|Kugeln?|Knollen?|Köpfe?|Kopf|Liter|Gramm|"
        "cups?|tbsp|tsp|oz|lbs?|pinch)?\\.?$"), QRegularExpression::CaseInsensitiveOption);
    return l.size() <= 16 && re.match(l.trimmed()).hasMatch();
}

// Nährwert- und Energieangaben („200 kcal / 837 kJ je Portion“, „Eiweiß 7,4 g · Fett 11,4 g …“)
bool nutritionLine(const QString &l)
{
    static const QRegularExpression energy(QStringLiteral("\\b\\d+\\s*(?:kcal|kj|kalorien)\\b"), QRegularExpression::CaseInsensitiveOption);
    if (l.contains(energy)) return true;
    static const QRegularExpression nutr(QStringLiteral("eiweiß|eiweiss|protein|fett|kohlenhydrate|ballaststoffe|zucker|salz|broteinheit"),
                                         QRegularExpression::CaseInsensitiveOption);
    int n = 0;
    auto it = nutr.globalMatch(l);
    while (it.hasNext()) {
        it.next();
        ++n;
    }
    return n >= 2;
}

// Kategorien-/Tag-Zeile im Kopf: „Suppen · Einlage · Pfannkuchen · Vegetarisch“
QStringList categoryLine(const QString &l)
{
    static const QRegularExpression sep(QStringLiteral("\\s+[·•|∙\\-–—:]\\s+"));
    const QStringList parts = l.split(sep, Qt::SkipEmptyParts);
    if (parts.size() < 3) return {};
    static const QRegularExpression word(QStringLiteral("^\\p{L}[\\p{L}\\-]*(?:\\s\\p{L}+)?$"));
    for (const QString &p : parts)
        if (!word.match(p.trimmed()).hasMatch()) return {};
    QStringList out;
    for (const QString &p : parts) out << p.trimmed();
    return out;
}

bool ingredientLike(const QString &l)
{
    if (l.size() > 70) return false;
    if (stepRe().match(l).hasMatch()) return false;
    return amountRe().match(l).hasMatch() || unitStartRe().match(l).hasMatch()
           || (bulletRe().match(l).hasMatch() && bulletRe().match(l).capturedLength() > 0 && l.size() < 60);
}

bool sentenceLike(const QString &l)
{
    return l.size() >= 45 || (l.size() >= 20 && (l.endsWith(QLatin1Char('.')) || l.endsWith(QLatin1Char('!'))));
}

// Portionen aus einer Zeile; Rückgabe: Text für Importers (z. B. „4 Portionen“)
QString yieldIn(const QString &l)
{
    static const QRegularExpression y1(QStringLiteral(
        "\\b(?:für|for|ergibt|makes|serves)\\s+(?:ca\\.\\s*)?(\\d+(?:\\s*[-–]\\s*\\d+)?)\\s*"
        "(personen|portionen|pers\\.?|stück|stk\\.?|people|persons|servings|portions)?\\b"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression y2(QStringLiteral(
        "^\\s*(?:portionen|personen|servings|serves|yield|ergibt|menge)\\s*:?\\s*(\\d+)\\s*(.*)$"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression y3(QStringLiteral("\\b(\\d+)\\s*(portionen|personen|servings|portions)\\b"), QRegularExpression::CaseInsensitiveOption);
    auto m = y2.match(l);
    if (m.hasMatch()) return m.captured(1) + QLatin1Char(' ') + (m.captured(2).trimmed().isEmpty() ? QStringLiteral("Portionen") : m.captured(2).trimmed());
    m = y1.match(l);
    if (m.hasMatch() && (!m.captured(2).isEmpty() || l.size() < 30)) {
        QString unit = m.captured(2);
        if (unit.isEmpty() || unit.startsWith(QLatin1String("pers"), Qt::CaseInsensitive) || unit.startsWith(QLatin1String("people"), Qt::CaseInsensitive))
            unit = QStringLiteral("Portionen");
        return m.captured(1) + QLatin1Char(' ') + unit;
    }
    m = y3.match(l);
    if (m.hasMatch()) return m.captured(1) + QStringLiteral(" Portionen");
    return QString();
}

int minutesIn(const QString &l)
{
    static const QRegularExpression h(QStringLiteral("(\\d+(?:[.,]\\d+)?)\\s*(?:std\\.?|stunden?|h\\b|hours?|hrs?)"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression m(QStringLiteral("(\\d+)\\s*(?:min\\.?|minuten|minutes|mins?)\\b"), QRegularExpression::CaseInsensitiveOption);
    double total = 0;
    const auto hm = h.match(l);
    if (hm.hasMatch()) total += QString(hm.captured(1)).replace(QLatin1Char(','), QLatin1Char('.')).toDouble() * 60;
    const auto mm = m.match(l);
    if (mm.hasMatch()) total += mm.captured(1).toInt();
    return int(std::lround(total));
}

// zwei Zutaten in einer Zeile (zweispaltige Liste): „200 g Mehl 100 g Zucker“
QStringList splitColumns(const QString &l)
{
    if (!amountRe().match(l).hasMatch()) return { l };   // „Eiweiß 7,4 g · Fett …“ nicht zerlegen
    static const QRegularExpression re(QStringLiteral(
        "(?<=[\\p{L}).])\\s+(?=(?:\\d+(?:[.,/]\\d+)?|[½¼¾])\\s*(?:g|kg|ml|l|cl|dl|EL|TL|Msp\\.?|Pck\\.?|Prise|Prisen|Becher|Dose|Dosen|Bund|Stück|Stk\\.?|Zehe|Zehen|Tasse|Tassen|Scheiben?)\\b)"));
    QStringList parts;
    int last = 0;
    auto it = re.globalMatch(l);
    while (it.hasNext()) {
        const auto m = it.next();
        parts << l.mid(last, m.capturedStart() - last).trimmed();
        last = m.capturedEnd();
    }
    parts << l.mid(last).trimmed();
    parts.removeAll(QString());
    return parts;
}

bool startsLower(const QString &l)
{
    for (const QChar c : l) {
        if (c.isLetter()) return c.isLower();
        if (!c.isSpace() && c != QLatin1Char('(')) return false;
    }
    return false;
}

} // namespace

Parts split(const QList<Ocr::Line> &input)
{
    Parts out;
    struct L { QString text; int block, par, height; QRect box; bool used = false; };
    QList<L> raw;
    for (const Ocr::Line &ol : input) {
        const QString t = fixLine(ol.text);
        if (!t.isEmpty()) raw << L{ t, ol.block, ol.par, ol.height(), ol.box };
    }
    // Mengen in eigener Spalte (rechtsbündig vor den Zutaten): Tesseract liefert „75 g“ und „Mehl“ oft als
    // getrennte Zeilen bzw. Blöcke – über die Lage auf der Seite wieder zusammenfügen
    for (int i = 0; i < raw.size(); ++i) {
        if (raw[i].used || !amountOnly(raw[i].text) || raw[i].box.isEmpty()) continue;
        const QRect a = raw[i].box;
        int best = -1, bestGap = INT_MAX;
        for (int j = 0; j < raw.size(); ++j) {
            if (j == i || raw[j].used || raw[j].box.isEmpty() || amountOnly(raw[j].text)) continue;
            const QRect b = raw[j].box;
            const int overlap = qMin(a.bottom(), b.bottom()) - qMax(a.top(), b.top());
            if (overlap * 2 < qMin(a.height(), b.height())) continue;          // nicht auf derselben Höhe
            const int gap = b.left() - a.right();
            if (gap < -a.height() / 2 || gap > 8 * qMax(a.height(), b.height())) continue;   // nicht rechts daneben
            if (gap < bestGap) {
                bestGap = gap;
                best = j;
            }
        }
        if (best < 0) continue;
        // Zutat beginnt in derselben Zeile schon mit Text? Dann ist die Menge davor zu setzen
        raw[i].text += QLatin1Char(' ') + raw[best].text;
        raw[i].box = raw[i].box.united(raw[best].box);
        raw[best].used = true;
    }
    QList<L> lines;
    static const QRegularExpression letters(QStringLiteral("\\p{L}"));
    for (const L &l : raw) {
        if (l.used) continue;
        if (!l.text.contains(letters) && l.text.size() < 5) continue;   // Seitenzahlen, Striche
        lines << L{ l.text, l.block, l.par, l.height, l.box };
    }
    if (lines.isEmpty()) return out;

    // ---- Titel: größte Schrift am Seitenanfang (vor den Zutaten), sonst erste Textzeile
    QList<int> heights;
    for (const L &l : lines)
        if (l.text.size() >= 3) heights << l.height;
    std::sort(heights.begin(), heights.end());
    const int median = heights.isEmpty() ? 0 : heights.at(heights.size() / 2);
    int headEnd = 0;
    while (headEnd < lines.size() && headEnd < 12 && !ingHeaderRe().match(lines.at(headEnd).text).hasMatch()
           && !ingredientLike(lines.at(headEnd).text) && !dirHeaderRe().match(lines.at(headEnd).text).hasMatch())
        ++headEnd;
    if (headEnd == 0) headEnd = qMin(3, int(lines.size()));
    int best = -1;
    for (int i = 0; i < headEnd; ++i) {
        const L &l = lines.at(i);
        if (metaRe().match(l.text).hasMatch() || (!yieldIn(l.text).isEmpty() && l.text.size() < 40)) continue;
        if (best < 0 || l.height > lines.at(best).height) best = i;
    }
    if (best >= 0 && median > 0 && lines.at(best).height * 100 < 125 * median) {
        // keine auffällig große Schrift: erste brauchbare Zeile
        for (int i = 0; i < headEnd; ++i) {
            const L &l = lines.at(i);
            if (l.text.size() >= 3 && l.text.size() <= 80 && !metaRe().match(l.text).hasMatch() && yieldIn(l.text).isEmpty()) {
                best = i;
                break;
            }
        }
    }
    if (best >= 0) {
        QStringList title{ lines.at(best).text };
        lines[best].used = true;
        // zweizeilige Titel
        for (int i = best + 1; i < headEnd; ++i) {
            const L &l = lines.at(i);
            if (l.block != lines.at(best).block || l.height * 100 < 85 * lines.at(best).height) break;
            title << l.text;
            lines[i].used = true;
        }
        // Trennstrich am Zeilenende: vor Großbuchstaben bleibt er (Kartoffel-Lauch-Suppe), sonst Silbentrennung
        QString t;
        for (const QString &part : title) {
            if (t.endsWith(QLatin1Char('-')) && t.size() > 1 && t.at(t.size() - 2).isLetter()) {
                if (startsLower(part)) t.chop(1);
                t += part;
            } else {
                t += (t.isEmpty() ? QString() : QStringLiteral(" ")) + part;
            }
        }
        out.text.title = t.simplified();
    }

    // ---- Abschnittsüberschriften in der Zutatenliste („Für den Teig“, „Kräuterflädle“): kurze Zeile ohne
    // Menge vor Zeilen mit Mengen, erkennbar am Doppelpunkt, an größerer Schrift oder daran, dass sie links
    // vor der (rechtsbündigen) Mengenspalte beginnt
    for (int k = 0; k + 1 < lines.size(); ++k) {
        L &h = lines[k];
        if (h.used) continue;
        QString t = h.text;
        if (ingredientLike(t) || amountOnly(t) || ingHeaderRe().match(t).hasMatch() || dirHeaderRe().match(t).hasMatch()) continue;
        if (t.size() > 40 || t.count(QLatin1Char(' ')) > 3 || t.endsWith(QLatin1Char('.')) || t.endsWith(QLatin1Char(','))) continue;
        int minLeft = INT_MAX, nextHeight = 0, amounts = 0;
        for (int n = k + 1; n < lines.size() && amounts < 12; ++n) {
            if (lines.at(n).used) continue;
            if (!amountRe().match(lines.at(n).text).hasMatch()) break;
            ++amounts;
            if (!nextHeight) nextHeight = lines.at(n).height;
            if (!lines.at(n).box.isEmpty()) minLeft = qMin(minLeft, lines.at(n).box.left());
        }
        if (amounts == 0) continue;
        const bool colon = t.endsWith(QLatin1Char(':'));
        const bool bigger = nextHeight > 0 && h.height * 100 >= 112 * nextHeight;
        const bool leftOf = !h.box.isEmpty() && minLeft != INT_MAX && h.box.left() < minLeft - h.height / 3;
        if (!(colon || bigger || leftOf)) continue;
        while (t.endsWith(QLatin1Char(':'))) t.chop(1);
        h.text = QStringLiteral("== %1 ==").arg(t.trimmed());
    }

    // ---- Absätze in Abschnitte teilen und zuordnen
    enum Mode { None, Ing, Dir };
    Mode mode = None;
    QStringList &ing = out.text.ingredientLines;
    QStringList &dir = out.text.directionLines;
    auto addDir = [&dir](const QString &t) {
        // Tesseract trennt manchmal mitten im Satz einen Absatz ab („verrüh-“ | „ren und …“)
        if (startsLower(t) && dir.size() >= 2 && dir.last().isEmpty()) {
            const QString &prev = dir.at(dir.size() - 2);
            if (!prev.endsWith(QLatin1Char('.')) && !prev.endsWith(QLatin1Char('!')) && !prev.endsWith(QLatin1Char('?'))
                && !prev.endsWith(QLatin1Char(':')))
                dir.removeLast();
        }
        dir << t;
    };
    auto endDirParagraph = [&dir] {
        if (!dir.isEmpty() && !dir.last().isEmpty()) dir << QString();
    };
    auto addIngredient = [&ing](QString t) {
        if (t.startsWith(QLatin1String("== "))) {   // Abschnittsüberschrift
            ing << t;
            return;
        }
        const auto b = bulletRe().match(t);
        if (b.hasMatch() && b.capturedLength() > 0) t = t.mid(b.capturedLength()).trimmed();
        if (t.isEmpty()) return;
        // Fortsetzung der vorigen Zutat
        if (!ing.isEmpty() && !ing.last().endsWith(QLatin1Char(':'))
            && (ing.last().endsWith(QLatin1Char(',')) || ing.last().endsWith(QLatin1Char('-')) || startsLower(t))
            && !amountRe().match(t).hasMatch() && !unitStartRe().match(t).hasMatch()) {
            QString &prev = ing.last();
            if (prev.endsWith(QLatin1Char('-')) && prev.size() > 1 && prev.at(prev.size() - 2).isLetter()) {
                prev.chop(1);
                prev += t;
            } else {
                prev += QLatin1Char(' ') + t;
            }
            return;
        }
        ing << splitColumns(t);
    };

    int i = 0;
    while (i < lines.size()) {
        // ein Absatz
        int j = i;
        while (j < lines.size() && lines.at(j).block == lines.at(i).block && lines.at(j).par == lines.at(i).par) ++j;
        // Abschnitte innerhalb des Absatzes (an Überschriften getrennt)
        QList<QString> seg;
        auto flush = [&] {
            if (seg.isEmpty()) return;
            int strong = 0, longish = 0;
            for (const QString &t : seg) {
                if (ingredientLike(t)) ++strong;
                if (sentenceLike(t)) ++longish;
            }
            const int n = int(seg.size());
            Mode cls;
            for (const QString &t : seg)
                if (t.startsWith(QLatin1String("== "))) ++strong;
            if (strong * 2 >= n && longish * 2 < n + strong) cls = Ing;
            else if (mode == Ing && longish * 2 < n) cls = Ing;               // Zutaten ohne Menge („Salz“)
            else if (mode == Ing) cls = Dir;                                   // fehlende Überschrift „Zubereitung“
            else if (mode == Dir) cls = Dir;
            else if (!ing.isEmpty() && longish * 2 >= n) cls = Dir;
            else cls = None;
            if (cls == Ing) {
                for (const QString &t : seg) addIngredient(t);
                if (mode == None) mode = Ing;
            } else if (cls == Dir) {
                for (const QString &t : seg) addDir(t);
                endDirParagraph();
                mode = Dir;
            } else {
                out.unassigned << seg;
            }
            seg.clear();
        };
        for (int k = i; k < j; ++k) {
            if (lines.at(k).used) continue;
            const QString &t = lines.at(k).text;
            auto m = ingHeaderRe().match(t);
            if (m.hasMatch()) {
                flush();
                mode = Ing;
                const QString y = yieldIn(t);
                if (!y.isEmpty() && out.text.yield.isEmpty()) out.text.yield = y;
                continue;
            }
            m = dirHeaderRe().match(t);
            if (m.hasMatch()) {
                flush();
                mode = Dir;
                endDirParagraph();
                if (!m.captured(1).trimmed().isEmpty()) seg << m.captured(1).trimmed();
                continue;
            }
            // Kopf- und Fußzeilen gedruckter Rezepte: Seitenzahl, wiederholter Titel
            static const QRegularExpression pageNo(QStringLiteral("^(?:seite|page|s\\.)\\s*\\d+(?:\\s*(?:von|/|of)\\s*\\d+)?$"),
                                                   QRegularExpression::CaseInsensitiveOption);
            if (t.contains(pageNo)) {
                flush();
                continue;
            }
            if (!out.text.title.isEmpty() && foldWord(t.toLower()).simplified() == foldWord(out.text.title.toLower()).simplified()) {
                flush();
                continue;
            }
            // Nährwertangaben gehören nicht in die Zutaten
            if (nutritionLine(t)) {
                flush();
                out.unassigned << t;
                continue;
            }
            // Kategorien im Kopf („Suppen · Einlage · Vegetarisch“) → Tags
            if (mode == None && ing.isEmpty()) {
                const QStringList cats = categoryLine(t);
                if (!cats.isEmpty()) {
                    flush();
                    out.text.categories << cats;
                    continue;
                }
            }
            if (metaRe().match(t).hasMatch()) {
                flush();
                static const QRegularExpression timeWord(QStringLiteral("zeit|time"), QRegularExpression::CaseInsensitiveOption);
                if (t.contains(timeWord) && out.prepMinutes == 0) out.prepMinutes = minutesIn(t);
                out.unassigned << t;
                continue;
            }
            const QString y = yieldIn(t);
            static const QRegularExpression yieldWord(QStringLiteral("portion|person|serving|^(?:für|for|serves|ergibt|makes)\\b"),
                                                      QRegularExpression::CaseInsensitiveOption);
            if (!y.isEmpty() && t.size() < 40 && t.contains(yieldWord)) {
                // „Für 4 Personen“, „4 Portionen“, „Serves 4“ – eigene Zeile
                flush();
                if (out.text.yield.isEmpty()) out.text.yield = y;
                continue;
            }
            seg << t;
        }
        flush();
        i = j;
    }
    while (!dir.isEmpty() && dir.last().isEmpty()) dir.removeLast();
    return out;
}

Recipe toRecipe(const Parts &parts, const Importers::Options &opt)
{
    Importers::Result res = Importers::fromParts(parts.text, QStringLiteral("Scan"), opt);
    Recipe r = res.recipes.value(0);
    if (parts.prepMinutes > 0) r.prepMinutes = parts.prepMinutes;
    return r;
}

} // namespace RecipeScan
