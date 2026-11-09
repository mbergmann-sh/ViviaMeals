// SPDX-License-Identifier: MIT AND LicenseRef-Commons-Clause-1.0 (siehe LICENSE)
#include "umlauts.h"

#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace Umlauts {

namespace {

bool isVowel(QChar c)
{
    static const QString v = QStringLiteral("aeiouyäöüAEIOUYÄÖÜ");
    return v.contains(c);
}

// Wörter (klein), in denen ae/oe/ue keine Umlaute sind – als Teil eines Wortes gesucht
const QStringList &substringExceptions()
{
    static const QStringList l{
        // ae
        QStringLiteral("israel"), QStringLiteral("michael"), QStringLiteral("raphael"), QStringLiteral("paella"),
        QStringLiteral("aerob"), QStringLiteral("aero"), QStringLiteral("maestr"), QStringLiteral("gael"),
        QStringLiteral("caesar"), QStringLiteral("aethyl"),
        // oe
        QStringLiteral("oetker"), QStringLiteral("goethe"), QStringLiteral("aloe"), QStringLiteral("boeuf"),
        QStringLiteral("oeuvre"), QStringLiteral("poesie"), QStringLiteral("poet"), QStringLiteral("phoen"),
        QStringLiteral("oboe"), QStringLiteral("koeffiz"), QStringLiteral("itzehoe"), QStringLiteral("soest"),
        QStringLiteral("coeur"), QStringLiteral("foehn"),
        // ue
        QStringLiteral("zuerst"), QStringLiteral("fondue"), QStringLiteral("barbecue"), QStringLiteral("statue"),
        QStringLiteral("aktuell"), QStringLiteral("eventuell"), QStringLiteral("manuell"), QStringLiteral("virtuell"),
        QStringLiteral("individuell"), QStringLiteral("visuell"), QStringLiteral("sexuell"), QStringLiteral("punktuell"),
        QStringLiteral("spirituell"), QStringLiteral("intellektuell"), QStringLiteral("rituell"), QStringLiteral("textuell"),
        QStringLiteral("duell"), QStringLiteral("duett"), QStringLiteral("menuett"), QStringLiteral("baguette"),
        QStringLiteral("silhouette"), QStringLiteral("pirouette"), QStringLiteral("statuette"), QStringLiteral("guerilla"),
        QStringLiteral("suede"), QStringLiteral("blues"), QStringLiteral("frequen"), QStringLiteral("sequen"),
        QStringLiteral("konsequen"), QStringLiteral("influen"), QStringLiteral("abenteuer"),
        // zusammengesetzte Wörter mit „o“ + „e…“ (Balsamicoessig, Bioeier, Avocadoeis)
        QStringLiteral("oessig"), QStringLiteral("oessenz"), QStringLiteral("bioe"), QStringLiteral("oeier"),
        QStringLiteral("oeis"), QStringLiteral("oerbse"), QStringLiteral("oextrakt"),
    };
    return l;
}

// ganze Wörter (klein), die unverändert bleiben (englische Wörter in deutschen Rezepten u. ä.)
const QSet<QString> &wordExceptions()
{
    static const QSet<QString> s{ QStringLiteral("blue"), QStringLiteral("true"), QStringLiteral("cue"), QStringLiteral("glue"),
                                  QStringLiteral("value"), QStringLiteral("venue"), QStringLiteral("avenue"), QStringLiteral("revue"),
                                  QStringLiteral("queue"), QStringLiteral("joel"), QStringLiteral("noel"), QStringLiteral("does"),
                                  QStringLiteral("toe"), QStringLiteral("shoe"), QStringLiteral("shoes"), QStringLiteral("canoe"),
                                  QStringLiteral("mae"), QStringLiteral("rae"), QStringLiteral("issue"), QStringLiteral("tissue") };
    return s;
}

// Wortstämme mit ß (nach der Umlautkorrektur, klein, mit „ss“ geschrieben)
const QStringList &szStems()
{
    static const QStringList l{
        QStringLiteral("heiss"), QStringLiteral("weiss"), QStringLiteral("gross"), QStringLiteral("gröss"),
        QStringLiteral("giess"), QStringLiteral("schliess"), QStringLiteral("aussen"), QStringLiteral("ausser"),
        QStringLiteral("äusser"), QStringLiteral("draussen"), QStringLiteral("fliess"), QStringLiteral("fuss"),
        QStringLiteral("füss"), QStringLiteral("gruss"), QStringLiteral("grüss"), QStringLiteral("kloss"),
        QStringLiteral("klöss"), QStringLiteral("mäss"), QStringLiteral("soss"), QStringLiteral("spiess"),
        QStringLiteral("strass"), QStringLiteral("strauss"), QStringLiteral("süss"), QStringLiteral("spass"),
        QStringLiteral("bloss"), QStringLiteral("fleiss"), QStringLiteral("gefäss"), QStringLiteral("griess"),
        QStringLiteral("stoss"), QStringLiteral("stöss"), QStringLiteral("beiss"), QStringLiteral("schweiss"),
        QStringLiteral("meissel"), QStringLiteral("scheuss"), QStringLiteral("zerreiss"), QStringLiteral("reissen"),
        QStringLiteral("abreiss"), QStringLiteral("einreiss"), QStringLiteral("aufreiss"), QStringLiteral("gemäss"),
        QStringLiteral("massvoll"), QStringLiteral("reisser"), QStringLiteral("ausserdem"), QStringLiteral("grösst"), QStringLiteral("spiessig"),
    };
    return l;
}

// ein ae/oe/ue an Position i (klein geschrieben in lower) umwandeln?
bool convertPair(const QString &lower, int i)
{
    if (i == 0) return true;
    const QChar p = lower[i - 1];
    // ue nach Vokal: Feuer, neue, Sauerbraten, Steuer; nach q: Quelle
    if (lower[i] == QLatin1Char('u')) return !isVowel(p) && p != QLatin1Char('q');
    // oe/ae nach a/o/u: Kakaoei, Kanuer – nach e/i dagegen Umlaut (eingeölt, Chiliöl)
    return p != QLatin1Char('a') && p != QLatin1Char('o') && p != QLatin1Char('u');
}

} // namespace

QString fixWord(const QString &word)
{
    if (word.size() < 3) return word;
    QString lower = word.toLower();
    if (wordExceptions().contains(lower)) return word;
    // Ausnahmebereiche markieren
    QList<QPair<int, int>> blocked;
    for (const QString &ex : substringExceptions()) {
        int p = 0;
        while ((p = int(lower.indexOf(ex, p))) >= 0) {
            blocked.append({ p, p + int(ex.size()) });
            p += int(ex.size());
        }
    }
    auto isBlocked = [&](int i) {
        for (const auto &b : blocked)
            if (i >= b.first && i < b.second) return true;
        return false;
    };
    QString out;
    out.reserve(word.size());
    for (int i = 0; i < word.size(); ++i) {
        const QChar c = lower[i];
        if (i + 1 < word.size() && lower[i + 1] == QLatin1Char('e')
            && (c == QLatin1Char('a') || c == QLatin1Char('o') || c == QLatin1Char('u')) && !isBlocked(i) && convertPair(lower, i)) {
            const bool upper = word[i].isUpper();
            QChar u = c == QLatin1Char('a') ? QChar(0x00E4) : c == QLatin1Char('o') ? QChar(0x00F6) : QChar(0x00FC);
            out += upper ? u.toUpper() : u;
            ++i;   // „e“ überspringen
            continue;
        }
        out += word[i];
    }
    // ß in bekannten Stämmen (nicht in Großschreibung)
    if (out.contains(QStringLiteral("ss"))) {
        const bool allCaps = out.toUpper() == out;
        if (!allCaps) {
            QString l2 = out.toLower();
            for (const QString &stem : szStems()) {
                int p = 0;
                while ((p = int(l2.indexOf(stem, p))) >= 0) {
                    const int ss = int(l2.indexOf(QStringLiteral("ss"), p));
                    if (ss >= 0 && ss < p + stem.size()) {
                        out.replace(ss, 2, QString(QChar(0x00DF)));
                        l2.replace(ss, 2, QString(QChar(0x00DF)));
                    }
                    p += int(stem.size()) - 1;
                }
            }
        }
    }
    return out;
}

QString fixText(const QString &text)
{
    QString out;
    out.reserve(text.size());
    int i = 0;
    while (i < text.size()) {
        if (text[i].isLetter()) {
            int j = i;
            while (j < text.size() && text[j].isLetter()) ++j;
            out += fixWord(text.mid(i, j - i));
            i = j;
        } else {
            out += text[i++];
        }
    }
    return out;
}

bool looksTranscribed(const QString &text)
{
    // Wörter mit echten Umlauten gegen Wörter zählen, die die Regeln ändern würden.
    // Einzelne echte Umlaute (z. B. nachträglich von Hand ergänzt) verhindern die Korrektur nicht.
    static const QRegularExpression word(QStringLiteral("\\p{L}+"));
    static const QRegularExpression um(QStringLiteral("[äöüÄÖÜß]"));
    int real = 0, fixable = 0;
    auto it = word.globalMatch(text);
    while (it.hasNext()) {
        const QString w = it.next().captured();
        if (w.contains(um)) ++real;
        else if (fixWord(w) != w) ++fixable;
    }
    return fixable > 0 && fixable >= 2 * real + 1;
}

QString headingCase(const QString &text)
{
    const QString t = text.trimmed();
    // nur reine Großschreibung umwandeln
    bool hasLetters = false;
    for (QChar c : t)
        if (c.isLetter()) {
            hasLetters = true;
            if (c.isLower()) return t;
        }
    if (!hasLetters || t.size() < 3) return t;
    static const QSet<QString> small{ QStringLiteral("und"), QStringLiteral("oder"), QStringLiteral("mit"), QStringLiteral("für"),
                                      QStringLiteral("fuer"), QStringLiteral("zum"), QStringLiteral("zur"), QStringLiteral("den"),
                                      QStringLiteral("der"), QStringLiteral("die"), QStringLiteral("das"), QStringLiteral("aus"),
                                      QStringLiteral("auf"), QStringLiteral("in"), QStringLiteral("im"), QStringLiteral("vom"),
                                      QStringLiteral("von"), QStringLiteral("an"), QStringLiteral("am"), QStringLiteral("dem"),
                                      QStringLiteral("and"), QStringLiteral("with"), QStringLiteral("for"), QStringLiteral("the"),
                                      QStringLiteral("of"), QStringLiteral("or") };
    QString out;
    bool start = true;
    int wordIndex = 0;
    QString word;
    auto flush = [&] {
        if (word.isEmpty()) return;
        const QString l = word.toLower();
        if (wordIndex > 0 && small.contains(l)) out += l;
        else out += l.left(1).toUpper() + l.mid(1);
        ++wordIndex;
        word.clear();
    };
    for (QChar c : t) {
        if (c.isLetter()) {
            word += c;
        } else {
            flush();
            out += c;
        }
    }
    flush();
    Q_UNUSED(start)
    return out;
}

QString allRecipeText(const Recipe &r)
{
    QStringList parts{ r.title, r.instructions, r.source, r.tags.join(QLatin1Char(' ')) };
    for (const Ingredient &i : r.ingredients) parts << i.name << i.note;
    return parts.join(QLatin1Char('\n'));
}

int fixRecipe(Recipe &r)
{
    int changed = 0;
    auto fix = [&](QString &s) {
        const QString f = fixText(s);
        if (f != s) {
            // grob die Anzahl geänderter Wörter zählen
            const QStringList a = s.split(QRegularExpression(QStringLiteral("\\s+"))), b = f.split(QRegularExpression(QStringLiteral("\\s+")));
            for (int k = 0; k < qMin(a.size(), b.size()); ++k)
                if (a[k] != b[k]) ++changed;
            s = f;
        }
    };
    fix(r.title);
    fix(r.instructions);
    fix(r.source);
    fix(r.yieldUnit);
    for (QString &t : r.tags) fix(t);
    for (Ingredient &i : r.ingredients) {
        fix(i.name);
        fix(i.note);
        if (i.heading) i.name = headingCase(i.name);
    }
    return changed;
}

} // namespace Umlauts
