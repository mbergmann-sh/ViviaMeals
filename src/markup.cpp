// ViviaMeals – einfache Textauszeichnung für die Zubereitung (siehe markup.h)
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "markup.h"

#include <QRegularExpression>
#include <QStringList>

namespace Markup {

namespace {

enum MarkType { Bold, Italic, Underline };

struct Token {
    int pos = 0;
    int len = 0;
    MarkType type = Bold;
    bool canOpen = false;
    bool canClose = false;
    bool paired = false;
};

bool isSpaceAt(const QString &s, int i)
{
    return i < 0 || i >= s.size() || s.at(i).isSpace();
}

// Auszeichnungszeichen finden (\* und \_ sind wörtlich)
QList<Token> tokens(const QString &s)
{
    QList<Token> out;
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (c == QLatin1Char('\\') && i + 1 < s.size()
            && (s.at(i + 1) == QLatin1Char('*') || s.at(i + 1) == QLatin1Char('_') || s.at(i + 1) == QLatin1Char('\\'))) {
            ++i;
            continue;
        }
        Token t;
        t.pos = i;
        if (c == QLatin1Char('*') && i + 1 < s.size() && s.at(i + 1) == QLatin1Char('*')) {
            t.len = 2;
            t.type = Bold;
        } else if (c == QLatin1Char('_') && i + 1 < s.size() && s.at(i + 1) == QLatin1Char('_')) {
            t.len = 2;
            t.type = Underline;
        } else if (c == QLatin1Char('*')) {
            t.len = 1;
            t.type = Italic;
        } else {
            continue;
        }
        t.canOpen = !isSpaceAt(s, i + t.len);
        t.canClose = !isSpaceAt(s, i - 1);
        out << t;
        i += t.len - 1;
    }
    // je Art paarweise zuordnen: Öffner mit dem nächsten passenden Schließer
    for (int a = 0; a < out.size(); ++a) {
        Token &open = out[a];
        if (open.paired || !open.canOpen) continue;
        for (int b = a + 1; b < out.size(); ++b) {
            Token &close = out[b];
            if (close.paired || close.type != open.type || !close.canClose) continue;
            if (close.pos <= open.pos + open.len) continue;   // leerer Inhalt
            open.paired = close.paired = true;
            break;
        }
    }
    return out;
}

bool escapable(QChar c)
{
    // * _ \ für Auszeichnung; - • . ) damit ein Absatz nicht als Listenpunkt gelesen wird
    return c == QLatin1Char('*') || c == QLatin1Char('_') || c == QLatin1Char('\\') || c == QLatin1Char('-') || c == QChar(0x2022)
        || c == QLatin1Char('.') || c == QLatin1Char(')');
}

QString unescape(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (int i = 0; i < s.size(); ++i) {
        if (s.at(i) == QLatin1Char('\\') && i + 1 < s.size() && escapable(s.at(i + 1))) {
            out += s.at(++i);
            continue;
        }
        out += s.at(i);
    }
    return out;
}

// wörtliche Zeichen so schützen, dass parseInline() sie nicht als Auszeichnung liest
QString escapeLiteral(const QString &s)
{
    QString out;
    out.reserve(s.size() + 4);
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (c == QLatin1Char('\\') && i + 1 < s.size() && escapable(s.at(i + 1))) {
            out += QStringLiteral("\\\\");
            continue;
        }
        if (c == QLatin1Char('*')) {
            // „5 * 3“ bleibt lesbar; nur Sterne direkt an Text schützen
            if (!isSpaceAt(s, i - 1) || !isSpaceAt(s, i + 1)) out += QLatin1Char('\\');
            out += c;
            continue;
        }
        if (c == QLatin1Char('_') && ((i + 1 < s.size() && s.at(i + 1) == QLatin1Char('_')) || (i > 0 && s.at(i - 1) == QLatin1Char('_')))) {
            out += QStringLiteral("\\_");
            continue;
        }
        out += c;
    }
    return out;
}

struct Raw {
    Block::Kind kind = Block::Paragraph;
    int number = 0;
    QString text;
};

const QRegularExpression &bulletRe()
{
    static const QRegularExpression re(QStringLiteral("^\\s*[-*\\x{2022}]\\s+(.*)$"));
    return re;
}

const QRegularExpression &numberRe()
{
    static const QRegularExpression re(QStringLiteral("^\\s*(\\d{1,3})[.)]\\s+(.*)$"));
    return re;
}

QString runsHtml(const QList<Run> &runs)
{
    QString h;
    for (const Run &r : runs) {
        QString t = r.text.toHtmlEscaped();
        t.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
        if (r.underline) t = QStringLiteral("<u>") + t + QStringLiteral("</u>");
        if (r.italic) t = QStringLiteral("<i>") + t + QStringLiteral("</i>");
        if (r.bold) t = QStringLiteral("<b>") + t + QStringLiteral("</b>");
        h += t;
    }
    return h;
}

} // namespace

QList<Run> parseInline(const QString &s)
{
    const QList<Token> toks = tokens(s);
    QList<Run> runs;
    Run cur;
    int last = 0;
    auto flush = [&](int upto) {
        if (upto > last) cur.text += unescape(s.mid(last, upto - last));
        if (!cur.text.isEmpty()) runs << cur;
        cur.text.clear();
    };
    for (const Token &t : toks) {
        if (!t.paired) continue;
        flush(t.pos);
        if (t.type == Bold) cur.bold = !cur.bold;
        else if (t.type == Italic) cur.italic = !cur.italic;
        else cur.underline = !cur.underline;
        last = t.pos + t.len;
    }
    flush(int(s.size()));
    // gleich formatierte Nachbarn zusammenfassen
    QList<Run> merged;
    for (const Run &r : runs) {
        if (!merged.isEmpty() && merged.last().bold == r.bold && merged.last().italic == r.italic && merged.last().underline == r.underline)
            merged.last().text += r.text;
        else
            merged << r;
    }
    return merged;
}

QString serializeInline(const QList<Run> &input)
{
    // Leerraum am Rand formatierter Läufe nach außen schieben (Auszeichnung muss direkt am Text stehen)
    QList<Run> runs;
    for (const Run &r : input) {
        if (r.text.isEmpty()) continue;
        if (!r.bold && !r.italic && !r.underline) {
            runs << r;
            continue;
        }
        int a = 0, b = int(r.text.size());
        while (a < b && r.text.at(a).isSpace()) ++a;
        while (b > a && r.text.at(b - 1).isSpace()) --b;
        if (a > 0) runs << Run{ r.text.left(a), false, false, false };
        if (b > a) {
            Run m = r;
            m.text = r.text.mid(a, b - a);
            runs << m;
        }
        if (b < r.text.size()) runs << Run{ r.text.mid(b), false, false, false };
    }
    QString out;
    bool bold = false, italic = false, underline = false;
    // Reihenfolge beim Öffnen: fett, kursiv, unterstrichen – geschlossen wird umgekehrt
    auto closeAll = [&](bool nb, bool ni, bool nu) {
        if (underline && (!nu || (italic && !ni) || (bold && !nb))) {
            out += QStringLiteral("__");
            underline = false;
        }
        if (italic && (!ni || (bold && !nb))) {
            out += QLatin1Char('*');
            italic = false;
        }
        if (bold && !nb) {
            out += QStringLiteral("**");
            bold = false;
        }
    };
    for (const Run &r : runs) {
        closeAll(r.bold, r.italic, r.underline);
        if (r.bold && !bold) {
            out += QStringLiteral("**");
            bold = true;
        }
        if (r.italic && !italic) {
            out += QLatin1Char('*');
            italic = true;
        }
        if (r.underline && !underline) {
            out += QStringLiteral("__");
            underline = true;
        }
        out += escapeLiteral(r.text);
    }
    closeAll(false, false, false);
    return out;
}

QList<Block> parse(const QString &text)
{
    QList<Raw> raws;
    bool open = false;   // letzter Block nimmt Folgezeilen auf
    const QStringList lines = QString(text).replace(QStringLiteral("\r\n"), QStringLiteral("\n")).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) {
            open = false;
            continue;
        }
        QRegularExpressionMatch m = bulletRe().match(line);
        if (m.hasMatch()) {
            raws << Raw{ Block::Bullet, 0, m.captured(1).trimmed() };
            open = true;
            continue;
        }
        m = numberRe().match(line);
        if (m.hasMatch()) {
            raws << Raw{ Block::Number, m.captured(1).toInt(), m.captured(2).trimmed() };
            open = true;
            continue;
        }
        if (open && !raws.isEmpty()) {
            raws.last().text += QLatin1Char('\n') + line.trimmed();
            continue;
        }
        raws << Raw{ Block::Paragraph, 0, line.trimmed() };
        open = true;
    }
    QList<Block> out;
    for (const Raw &r : raws) {
        Block b;
        b.kind = r.kind;
        b.number = r.number;
        b.runs = parseInline(r.text);
        out << b;
    }
    return out;
}

QString serialize(const QList<Block> &blocks)
{
    QString out;
    int n = 0;
    for (int i = 0; i < blocks.size(); ++i) {
        const Block &b = blocks.at(i);
        const bool sameList = i > 0 && b.kind != Block::Paragraph && blocks.at(i - 1).kind == b.kind;
        if (i > 0) out += sameList ? QStringLiteral("\n") : QStringLiteral("\n\n");
        QString body = serializeInline(b.runs);
        if (b.kind == Block::Bullet) {
            out += QStringLiteral("- ") + body;
        } else if (b.kind == Block::Number) {
            n = sameList ? n + 1 : (b.number > 0 ? b.number : 1);
            out += QStringLiteral("%1. ").arg(n) + body;
        } else {
            // ein Absatz darf nicht wie ein Listenpunkt beginnen
            if (bulletRe().match(body).hasMatch() || numberRe().match(body).hasMatch()) {
                const int k = int(body.indexOf(QRegularExpression(QStringLiteral("\\S"))));
                if (k >= 0) {
                    if (body.at(k).isDigit()) {
                        int e = k;
                        while (e < body.size() && body.at(e).isDigit()) ++e;
                        body.insert(e, QLatin1Char('\\'));   // „1\. …“ bleibt Text
                    } else {
                        body.insert(k, QLatin1Char('\\'));
                    }
                }
            }
            out += body;
        }
    }
    return out;
}

QString toHtml(const QString &text)
{
    const QList<Block> blocks = parse(text);
    QString h;
    for (int i = 0; i < blocks.size(); ++i) {
        const Block &b = blocks.at(i);
        if (b.kind == Block::Paragraph) {
            h += QStringLiteral("<p>") + runsHtml(b.runs) + QStringLiteral("</p>");
            continue;
        }
        const bool num = b.kind == Block::Number;
        h += num ? (b.number > 1 ? QStringLiteral("<ol start=\"%1\">").arg(b.number) : QStringLiteral("<ol>")) : QStringLiteral("<ul>");
        int j = i;
        for (; j < blocks.size() && blocks.at(j).kind == b.kind; ++j) h += QStringLiteral("<li>") + runsHtml(blocks.at(j).runs) + QStringLiteral("</li>");
        h += num ? QStringLiteral("</ol>") : QStringLiteral("</ul>");
        i = j - 1;
    }
    return h;
}

QString toPlain(const QString &text)
{
    const QList<Block> blocks = parse(text);
    QString out;
    int n = 0;
    for (int i = 0; i < blocks.size(); ++i) {
        const Block &b = blocks.at(i);
        const bool sameList = i > 0 && b.kind != Block::Paragraph && blocks.at(i - 1).kind == b.kind;
        if (i > 0) out += sameList ? QStringLiteral("\n") : QStringLiteral("\n\n");
        QString t;
        for (const Run &r : b.runs) t += r.text;
        if (b.kind == Block::Bullet) out += QStringLiteral("\u2022 ") + t;
        else if (b.kind == Block::Number) {
            n = sameList ? n + 1 : (b.number > 0 ? b.number : 1);
            out += QStringLiteral("%1. ").arg(n) + t;
        } else
            out += t;
    }
    return out;
}

QString toLatex(const QString &text, const std::function<QString(const QString &)> &esc)
{
    auto runs = [&](const QList<Run> &rs) {
        QString t;
        for (const Run &r : rs) {
            QString s;
            const QStringList lines = r.text.split(QLatin1Char('\n'));
            for (int k = 0; k < lines.size(); ++k) {
                if (k > 0) s += QStringLiteral("\\newline{}");
                s += esc(lines.at(k));
            }
            if (r.underline) s = QStringLiteral("\\underline{") + s + QLatin1Char('}');
            if (r.italic) s = QStringLiteral("\\emph{") + s + QLatin1Char('}');
            if (r.bold) s = QStringLiteral("\\textbf{") + s + QLatin1Char('}');
            t += s;
        }
        return t;
    };
    const QList<Block> blocks = parse(text);
    QString out;
    for (int i = 0; i < blocks.size(); ++i) {
        const Block &b = blocks.at(i);
        if (b.kind == Block::Paragraph) {
            out += runs(b.runs) + QStringLiteral("\n\n");
            continue;
        }
        const bool num = b.kind == Block::Number;
        out += num ? QStringLiteral("\\begin{enumerate}\n") : QStringLiteral("\\begin{itemize}\n");
        if (num && b.number > 1) out += QStringLiteral("\\setcounter{enumi}{%1}\n").arg(b.number - 1);
        int j = i;
        for (; j < blocks.size() && blocks.at(j).kind == b.kind; ++j) out += QStringLiteral("\\item ") + runs(blocks.at(j).runs) + QLatin1Char('\n');
        out += num ? QStringLiteral("\\end{enumerate}\n\n") : QStringLiteral("\\end{itemize}\n\n");
        i = j - 1;
    }
    return out;
}

QString toOdt(const QString &text, const std::function<QString(const QString &)> &esc, const QString &paraStyle)
{
    auto runs = [&](const QList<Run> &rs) {
        QString t;
        for (const Run &r : rs) {
            QString s = esc(r.text);
            s.replace(QLatin1Char('\n'), QStringLiteral("<text:line-break/>"));
            if (r.underline) s = QStringLiteral("<text:span text:style-name=\"T_u\">") + s + QStringLiteral("</text:span>");
            if (r.italic) s = QStringLiteral("<text:span text:style-name=\"T_i\">") + s + QStringLiteral("</text:span>");
            if (r.bold) s = QStringLiteral("<text:span text:style-name=\"T_b\">") + s + QStringLiteral("</text:span>");
            t += s;
        }
        return t;
    };
    const QString p = QStringLiteral("<text:p text:style-name=\"%1\">").arg(paraStyle);
    const QList<Block> blocks = parse(text);
    QString out;
    for (int i = 0; i < blocks.size(); ++i) {
        const Block &b = blocks.at(i);
        if (b.kind == Block::Paragraph) {
            out += p + runs(b.runs) + QStringLiteral("</text:p>");
            continue;
        }
        const bool num = b.kind == Block::Number;
        out += QStringLiteral("<text:list text:style-name=\"%1\">").arg(num ? QStringLiteral("L_Nummer") : QStringLiteral("L_Punkt"));
        int j = i;
        for (; j < blocks.size() && blocks.at(j).kind == b.kind; ++j) {
            const QString start = (j == i && num && b.number > 1) ? QStringLiteral(" text:start-value=\"%1\"").arg(b.number) : QString();
            out += QStringLiteral("<text:list-item%1>").arg(start) + p + runs(blocks.at(j).runs) + QStringLiteral("</text:p></text:list-item>");
        }
        out += QStringLiteral("</text:list>");
        i = j - 1;
    }
    return out;
}

bool hasMarkup(const QString &text)
{
    for (const Block &b : parse(text)) {
        if (b.kind != Block::Paragraph) return true;
        for (const Run &r : b.runs)
            if (r.bold || r.italic || r.underline) return true;
    }
    return false;
}

} // namespace Markup
