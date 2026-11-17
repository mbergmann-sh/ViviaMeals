// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "ambrosia.h"
#include "jetdb.h"

#include <QBuffer>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace Ambrosia {

namespace {

// Windows-1252: 0x80–0x9F
const char16_t cp1252High[32] = { 0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
                                  0x2039, 0x0152, 0x008D, 0x017D, 0x008F, 0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
                                  0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178 };

QChar fromCp1252(int b)
{
    if (b >= 0x80 && b <= 0x9F) return QChar(cp1252High[b - 0x80]);
    return QChar(b);
}

int toCp1252(char16_t u)
{
    if (u < 0x80 || (u >= 0xA0 && u <= 0xFF)) return u;
    for (int i = 0; i < 32; ++i)
        if (cp1252High[i] == u) return 0x80 + i;
    return u <= 0xFF ? int(u) : -1;
}

// ---------------------------------------------------------------- RTF

struct RtfState {
    bool bold = false, italic = false, underline = false;
    bool skip = false;      // Zielgruppe ohne sichtbaren Text (Schriftentabelle, Listenzeichen …)
    bool pntext = false;    // Listenzeichen des Absatzes („·“ bzw. „1.“)
    int uc = 1;             // Zeichen, die nach \\uN zu überspringen sind
};

class RtfReader
{
public:
    explicit RtfReader(const QString &rtf) : m_s(rtf) {}

    QList<Markup::Block> run()
    {
        QList<RtfState> stack;
        RtfState st;
        int skipChars = 0;
        bool groupStart = false;   // erstes Steuerwort einer Gruppe (für \\* und Zielgruppen)
        const int n = int(m_s.size());
        int i = 0;
        while (i < n) {
            const QChar c = m_s[i];
            if (c == QLatin1Char('{')) {
                stack << st;
                groupStart = true;
                ++i;
                continue;
            }
            if (c == QLatin1Char('}')) {
                if (st.pntext) m_pntext += m_pnBuf;
                m_pnBuf.clear();
                st = stack.isEmpty() ? RtfState() : stack.takeLast();
                groupStart = false;
                ++i;
                continue;
            }
            if (c == QLatin1Char('\\')) {
                ++i;
                if (i >= n) break;
                const QChar d = m_s[i];
                if (d.isLetter() && d.unicode() < 128) {
                    int j = i;
                    while (j < n && m_s[j].isLetter() && m_s[j].unicode() < 128) ++j;
                    const QString word = m_s.mid(i, j - i);
                    bool hasParam = false;
                    int param = 0;
                    int k = j;
                    if (k < n && (m_s[k] == QLatin1Char('-') || m_s[k].isDigit())) {
                        int e = k + 1;
                        while (e < n && m_s[e].isDigit()) ++e;
                        param = m_s.mid(k, e - k).toInt();
                        hasParam = true;
                        k = e;
                    }
                    if (k < n && m_s[k] == QLatin1Char(' ')) ++k;   // Trennzeichen gehört zum Steuerwort
                    i = k;
                    control(word, hasParam, param, st, groupStart, skipChars);
                    groupStart = false;
                    continue;
                }
                // Steuersymbole
                groupStart = false;
                if (d == QLatin1Char('\'')) {
                    const int b = m_s.mid(i + 1, 2).toInt(nullptr, 16);
                    i += 3;
                    if (skipChars > 0) {
                        --skipChars;
                        continue;
                    }
                    put(st, fromCp1252(b));
                    continue;
                }
                ++i;
                if (d == QLatin1Char('*')) {
                    st.skip = true;   // unbekannte Zielgruppe
                    continue;
                }
                if (d == QLatin1Char('~')) put(st, QLatin1Char(' '));
                else if (d == QLatin1Char('_')) put(st, QLatin1Char('-'));
                else if (d == QLatin1Char('\\') || d == QLatin1Char('{') || d == QLatin1Char('}')) put(st, d);
                else if (d == QLatin1Char('\n') || d == QLatin1Char('\r')) newParagraph();
                continue;   // \\- (weiches Trennzeichen) und Unbekanntes: nichts
            }
            groupStart = false;
            ++i;
            if (c == QLatin1Char('\n') || c == QLatin1Char('\r')) continue;   // Zeilenumbrüche im RTF-Quelltext zählen nicht
            if (skipChars > 0) {
                --skipChars;
                continue;
            }
            put(st, c);
        }
        newParagraph();
        return m_blocks;
    }

private:
    void control(const QString &w, bool hasParam, int param, RtfState &st, bool groupStart, int &skipChars)
    {
        static const QSet<QString> destinations{ QStringLiteral("fonttbl"), QStringLiteral("colortbl"), QStringLiteral("stylesheet"),
                                                 QStringLiteral("info"), QStringLiteral("pict"), QStringLiteral("object"),
                                                 QStringLiteral("header"), QStringLiteral("footer"), QStringLiteral("headerl"),
                                                 QStringLiteral("headerr"), QStringLiteral("footerl"), QStringLiteral("footerr"),
                                                 QStringLiteral("fldinst"), QStringLiteral("themedata"), QStringLiteral("datastore"),
                                                 QStringLiteral("latentstyles"), QStringLiteral("listtable"),
                                                 QStringLiteral("listoverridetable"), QStringLiteral("generator"),
                                                 QStringLiteral("xmlnstbl"), QStringLiteral("rsidtbl"), QStringLiteral("pn") };
        if (groupStart && destinations.contains(w)) {
            st.skip = true;
            return;
        }
        if (w == QLatin1String("pntext") || w == QLatin1String("listtext")) {
            st.skip = true;
            st.pntext = true;
            m_pnBuf.clear();
            return;
        }
        const bool on = !hasParam || param != 0;
        if (w == QLatin1String("par") || w == QLatin1String("sect") || w == QLatin1String("page")) newParagraph();
        else if (w == QLatin1String("line")) put(st, QLatin1Char('\n'));
        else if (w == QLatin1String("tab")) put(st, QLatin1Char(' '));
        else if (w == QLatin1String("b")) st.bold = on;
        else if (w == QLatin1String("i")) st.italic = on;
        else if (w == QLatin1String("ul")) st.underline = on;
        else if (w == QLatin1String("ulnone")) st.underline = false;
        else if (w == QLatin1String("plain")) {
            st.bold = st.italic = st.underline = false;
        } else if (w == QLatin1String("uc")) st.uc = param;
        else if (w == QLatin1String("u")) {
            int code = param;
            if (code < 0) code += 65536;
            put(st, QChar(char16_t(code)));
            skipChars = st.uc;
        } else if (w == QLatin1String("emdash")) put(st, QChar(0x2014));
        else if (w == QLatin1String("endash")) put(st, QChar(0x2013));
        else if (w == QLatin1String("bullet")) put(st, QChar(0x2022));
        else if (w == QLatin1String("lquote")) put(st, QChar(0x2018));
        else if (w == QLatin1String("rquote")) put(st, QChar(0x2019));
        else if (w == QLatin1String("ldblquote")) put(st, QChar(0x201C));
        else if (w == QLatin1String("rdblquote")) put(st, QChar(0x201D));
    }

    void put(const RtfState &st, QChar ch)
    {
        if (st.pntext) {
            m_pnBuf += ch;
            return;
        }
        if (st.skip) return;
        QList<Markup::Run> &runs = m_cur.runs;
        if (!runs.isEmpty() && runs.last().bold == st.bold && runs.last().italic == st.italic && runs.last().underline == st.underline) {
            runs.last().text += ch;
        } else {
            Markup::Run r;
            r.text = ch;
            r.bold = st.bold;
            r.italic = st.italic;
            r.underline = st.underline;
            runs << r;
        }
    }

    void newParagraph()
    {
        // Leerraum am Rand entfernen, Formatierung von reinem Leerraum vergessen
        QList<Markup::Run> runs;
        for (Markup::Run r : m_cur.runs) {
            r.text.replace(QChar(0x00A0), QLatin1Char(' '));
            if (r.text.trimmed().isEmpty()) r.bold = r.italic = r.underline = false;
            if (!runs.isEmpty() && runs.last().bold == r.bold && runs.last().italic == r.italic && runs.last().underline == r.underline)
                runs.last().text += r.text;
            else
                runs << r;
        }
        while (!runs.isEmpty() && runs.first().text.trimmed().isEmpty()) runs.removeFirst();
        while (!runs.isEmpty() && runs.last().text.trimmed().isEmpty()) runs.removeLast();
        if (!runs.isEmpty()) {
            int a = 0;
            while (a < runs.first().text.size() && runs.first().text[a].isSpace()) ++a;
            runs.first().text = runs.first().text.mid(a);
            QString &t = runs.last().text;
            while (!t.isEmpty() && t.back().isSpace()) t.chop(1);
            Markup::Block b;
            b.runs = runs;
            const QString mark = m_pntext.trimmed();
            if (!mark.isEmpty()) {
                bool num = false;
                const int v = QString(mark).remove(QLatin1Char('.')).remove(QLatin1Char(')')).toInt(&num);
                if (num) {
                    b.kind = Markup::Block::Number;
                    b.number = v;
                } else {
                    b.kind = Markup::Block::Bullet;
                }
            }
            m_blocks << b;
        }
        m_cur = Markup::Block();
        m_pntext.clear();
    }

    QString m_s;
    QList<Markup::Block> m_blocks;
    Markup::Block m_cur;
    QString m_pntext;   // Listenzeichen des laufenden Absatzes
    QString m_pnBuf;
};

// ---------------------------------------------------------------- Hilfen

QString str(const QVariantMap &row, const char *key) { return row.value(QLatin1String(key)).toString().trimmed(); }
qint64 num(const QVariantMap &row, const char *key) { return row.value(QLatin1String(key)).toLongLong(); }

QString amountText(double v)
{
    if (v <= 0) return {};
    QString s = QString::number(v, 'f', 4);
    while (s.contains(QLatin1Char('.')) && (s.endsWith(QLatin1Char('0')) || s.endsWith(QLatin1Char('.')))) s.chop(1);
    return s;
}

QDateTime fromAccessDate(const QVariant &v)
{
    if (!v.isValid()) return {};
    const double d = v.toDouble();
    if (d < 1) return {};
    const QDateTime base(QDate(1899, 12, 30), QTime(0, 0));
    return base.addMSecs(qint64(std::llround(d * 86400000.0)));
}

} // namespace

// ---------------------------------------------------------------- öffentliche Hilfen

QList<Markup::Block> rtfBlocks(const QString &rtf)
{
    if (!rtf.trimmed().startsWith(QLatin1String("{\\rtf"))) {
        // kein RTF: Text unverändert als Absätze
        QList<Markup::Block> out;
        for (const QString &p : rtf.split(QLatin1Char('\n'))) {
            if (p.trimmed().isEmpty()) continue;
            Markup::Block b;
            Markup::Run r;
            r.text = p.trimmed();
            b.runs << r;
            out << b;
        }
        return out;
    }
    return RtfReader(rtf).run();
}

QString rtfToMarkup(const QString &rtf) { return Markup::serialize(rtfBlocks(rtf)); }

QString rtfToPlain(const QString &rtf) { return Markup::toPlain(rtfToMarkup(rtf)); }

QByteArray imageFromBlob(const QByteArray &blob)
{
    QByteArray b = blob;
    // Ambrosia legt Bilder als Text ab: jedes Byte als Windows-1252-Zeichen in UTF-16 („FF 00 D8 00 …“)
    if (b.size() >= 8 && b.size() % 2 == 0 && quint8(b[1]) == 0 && quint8(b[3]) == 0
        && !(quint8(b[0]) == 0xFF && quint8(b[1]) == 0xD8)) {
        QByteArray narrow;
        narrow.reserve(b.size() / 2);
        bool ok = true;
        for (int i = 0; i + 1 < b.size(); i += 2) {
            const char16_t u = char16_t(quint8(b[i]) | (quint8(b[i + 1]) << 8));
            const int c = toCp1252(u);
            if (c < 0) {
                ok = false;
                break;
            }
            narrow += char(c);
        }
        if (ok) b = narrow;
    }
    // Bildanfang suchen (OLE-Objekte haben einen Kopf vor den Daten)
    static const QList<QByteArray> magic{ QByteArray("\xFF\xD8\xFF", 3), QByteArray("\x89PNG", 4), QByteArray("GIF8", 4) };
    for (const QByteArray &m : magic) {
        const int at = int(b.indexOf(m));
        if (at >= 0 && at < 4096) return b.mid(at);
    }
    if (b.startsWith("BM")) return b;
    return {};
}

bool knownUnit(int id)
{
    QString note;
    return id <= 0 || !unitName(id, 1, &note).isEmpty() || !note.isEmpty();
}

QString unitName(int id, double amount, QString *note)
{
    Q_UNUSED(amount)
    // Nummern aus Ambrosia 2.x; in exportierten Datenbanken ist T_UNIT leer. Ermittelt aus Beispielrezepten,
    // ergänzbar, sobald weitere Nummern bekannt sind.
    switch (id) {
    case 7: return QStringLiteral("Prise");
    case 11: return QStringLiteral("Bund");
    case 13: return QStringLiteral("Scheibe");
    case 15: return QStringLiteral("TL");
    case 17: return QStringLiteral("EL");
    case 25: return QStringLiteral("ml");
    case 28: return QStringLiteral("l");
    case 31: return QStringLiteral("g");
    case 32: return QStringLiteral("kg");
    case 33:
        if (note) *note = QStringLiteral("etwas");
        return {};
    case 34:
        if (note) *note = QStringLiteral("nach Belieben");
        return {};
    case 37: return QStringLiteral("Zehe");
    default: return {};
    }
}

bool isAmbrosia(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray head = f.read(32);
    if (!(head.mid(4, 15) == "Standard Jet DB" || head.mid(4, 15) == "Standard ACE DB")) return false;
    f.close();
    Jet::Database db;
    return db.open(path) && db.hasTable(QStringLiteral("T_REZ_KOPF")) && db.hasTable(QStringLiteral("T_REZ_ZUTAT"));
}

Importers::Result importFile(const QString &path, const Importers::Options &opt)
{
    Importers::Result res;
    Jet::Database db;
    QString err;
    if (!db.open(path, &err)) {
        res.warnings << err;
        return res;
    }
    if (!db.hasTable(QStringLiteral("T_REZ_KOPF")) || !db.hasTable(QStringLiteral("T_REZ_ZUTAT"))) {
        res.warnings << QObject::tr("Die Datenbank enthält keine Ambrosia-Rezepte (Tabelle T_REZ_KOPF fehlt).");
        return res;
    }
    res.encoding = QStringLiteral("Access (JET 4)");

    // Nachschlagetabellen
    QHash<qint64, QString> yieldUnits, categories, units;
    for (const QVariantMap &r : db.rows(QStringLiteral("T_REZ_KOPFEINHEIT"))) yieldUnits.insert(num(r, "ID"), str(r, "NameD"));
    for (const QVariantMap &r : db.rows(QStringLiteral("T_CAT"))) categories.insert(num(r, "ID"), str(r, "CATNameD"));
    for (const QVariantMap &r : db.rows(QStringLiteral("T_UNIT"))) {
        QString u = str(r, "UnitShortD");
        if (u.isEmpty()) u = str(r, "UnitNameD");
        if (!u.isEmpty()) units.insert(num(r, "ID"), u);
    }
    struct Lit {
        QString title, issue;
        qint64 year = 0;
    };
    QHash<qint64, Lit> literature;
    for (const QVariantMap &r : db.rows(QStringLiteral("T_LITERATUR"))) {
        Lit l;
        l.title = str(r, "Title").simplified();
        l.issue = str(r, "AusgabeNr");
        l.year = num(r, "Jahrgang");
        literature.insert(num(r, "QuelleID"), l);
    }
    QHash<qint64, QList<QVariantMap>> ingredients, directions;
    for (const QVariantMap &r : db.rows(QStringLiteral("T_REZ_ZUTAT"))) ingredients[num(r, "ID")] << r;
    for (const QVariantMap &r : db.rows(QStringLiteral("T_REZ_ANWEIS"))) directions[num(r, "ID")] << r;
    QHash<qint64, QStringList> recipeCats;
    {
        QList<QVariantMap> idx = db.rows(QStringLiteral("T_REZ_CAT_NDX"));
        std::sort(idx.begin(), idx.end(), [](const QVariantMap &a, const QVariantMap &b) {
            return num(a, "CatPrimary") != num(b, "CatPrimary") ? num(a, "CatPrimary") > num(b, "CatPrimary") : num(a, "No") < num(b, "No");
        });
        for (const QVariantMap &r : idx) {
            const QString c = categories.value(num(r, "CatID"));
            if (!c.isEmpty() && !recipeCats[num(r, "ID")].contains(c)) recipeCats[num(r, "ID")] << c;
        }
    }
    QHash<qint64, QByteArray> photos;   // Rezept → erstes Foto (Typ 5)
    {
        QList<QVariantMap> blobs = db.rows(QStringLiteral("T_BLOB"));
        std::sort(blobs.begin(), blobs.end(), [](const QVariantMap &a, const QVariantMap &b) { return num(a, "IDNum") < num(b, "IDNum"); });
        for (const QVariantMap &r : blobs) {
            if (num(r, "IDType") != 5 || photos.contains(num(r, "IDReference"))) continue;
            const QByteArray img = imageFromBlob(r.value(QStringLiteral("Blob")).toByteArray());
            if (!img.isEmpty()) photos.insert(num(r, "IDReference"), img);
        }
    }

    QList<QVariantMap> heads = db.rows(QStringLiteral("T_REZ_KOPF"));
    std::sort(heads.begin(), heads.end(), [](const QVariantMap &a, const QVariantMap &b) {
        return QString::localeAwareCompare(str(a, "RezTitle"), str(b, "RezTitle")) < 0;
    });
    QHash<qint64, QString> unknownUnits;   // Nummer → Beispiel
    int skipped = 0;
    int done = 0;
    for (const QVariantMap &h : heads) {
        if (opt.progress && !opt.progress(done, int(heads.size()))) {
            res.warnings << QObject::tr("Import abgebrochen – %1 von %2 Rezepten gelesen.").arg(done).arg(heads.size());
            break;
        }
        ++done;
        if (h.value(QStringLiteral("FlagDeleted")).toBool() || h.value(QStringLiteral("Template")).toBool()) {
            ++skipped;
            continue;
        }
        const qint64 id = num(h, "ID");
        Importers::StructuredRecipe s;
        s.title = str(h, "RezTitle");
        if (s.title.isEmpty()) s.title = str(h, "RezMenucard");
        const qint64 yield = num(h, "RezYield");
        if (yield > 0) s.yield = QStringLiteral("%1 %2").arg(yield).arg(yieldUnits.value(num(h, "RezYieldEinh"), QStringLiteral("Portionen")));
        s.categories = recipeCats.value(id);

        // Zutaten
        QList<QVariantMap> zut = ingredients.value(id);
        std::sort(zut.begin(), zut.end(), [](const QVariantMap &a, const QVariantMap &b) { return num(a, "No") < num(b, "No"); });
        QList<Markup::Block> fallback;   // Arbeitsschritte an den Zutaten, falls die Zubereitung fehlt
        for (const QVariantMap &z : zut) {
            Importers::StructuredIngredient si;
            si.name = str(z, "ZutText");
            if (z.value(QStringLiteral("ZutTrenner")).toBool()) {
                si.heading = true;
                if (!si.name.isEmpty()) s.ingredients << si;
                continue;
            }
            if (si.name.isEmpty()) continue;
            const double amount = z.value(QStringLiteral("ZutAnz")).toDouble();
            const int unit = int(num(z, "ZutEinh"));
            QString note;
            QString u = units.value(unit);
            if (u.isEmpty()) u = unitName(unit, amount, &note);
            if (u.isEmpty() && note.isEmpty() && unit > 0 && !unknownUnits.contains(unit))
                unknownUnits.insert(unit, QStringLiteral("%1 %2 (%3)").arg(amountText(amount), si.name, s.title));
            double a = amount;
            // Bruchteile großer Einheiten lesbarer: 0,2 kg → 200 g, 0,25 l → 250 ml
            if (a > 0 && a < 1 && (u == QLatin1String("kg") || u == QLatin1String("l"))) {
                a = std::round(a * 1000.0);
                u = u == QLatin1String("kg") ? QStringLiteral("g") : QStringLiteral("ml");
            }
            // Einheit steckt schon im Namen („1 Zehe Knoblauchzehen“)
            if (u.size() >= 4 && si.name.contains(u, Qt::CaseInsensitive)) u.clear();
            si.amount = amountText(a);
            si.unit = u;
            QStringList notes;
            if (!str(z, "ZutVerarbeitung").isEmpty()) notes << str(z, "ZutVerarbeitung");
            if (!note.isEmpty()) notes << note;
            si.note = notes.join(QStringLiteral(", "));
            s.ingredients << si;
            const QString step = rtfToPlain(z.value(QStringLiteral("ZutZubereitung")).toString()).simplified();
            if (!step.isEmpty()) {
                Markup::Block b;
                b.kind = Markup::Block::Number;
                b.number = int(fallback.size()) + 1;
                Markup::Run r;
                r.text = step;
                b.runs << r;
                fallback << b;
            }
        }

        // Zubereitung (RTF), sonst die Arbeitsschritte an den Zutaten
        QList<QVariantMap> anw = directions.value(id);
        std::sort(anw.begin(), anw.end(), [](const QVariantMap &a, const QVariantMap &b) { return num(a, "No") < num(b, "No"); });
        QList<Markup::Block> blocks;
        for (const QVariantMap &a : anw) blocks << rtfBlocks(a.value(QStringLiteral("AnwText")).toString());
        if (blocks.isEmpty()) blocks = fallback;
        // Notizen und Weintipp anhängen
        auto addSection = [&blocks](const QString &heading, const QList<Markup::Block> &content) {
            if (content.isEmpty()) return;
            Markup::Block hb;
            Markup::Run hr;
            hr.text = heading;
            hr.bold = true;
            hb.runs << hr;
            blocks << hb << content;
        };
        addSection(QObject::tr("Notizen"), rtfBlocks(h.value(QStringLiteral("RezNotizen")).toString()));
        addSection(QObject::tr("Weintipp"), rtfBlocks(h.value(QStringLiteral("RezWeinTipp")).toString()));
        s.instructions = Markup::serialize(blocks);

        // Quelle
        QString src = str(h, "RezQuelle");
        if (src.startsWith(QLatin1String("{\\rtf"))) src = rtfToPlain(src).simplified();
        const Lit lit = literature.value(num(h, "RezQuelleID"));
        if (src.isEmpty() && !lit.title.isEmpty()) {
            src = lit.title;
            if (!lit.issue.isEmpty() && lit.year > 0) src += QStringLiteral(" %1/%2").arg(lit.issue).arg(lit.year);
            else if (lit.year > 0) src += QStringLiteral(" (%1)").arg(lit.year);
        }
        const QString page = str(h, "RezQuellePage");
        if (!src.isEmpty() && !page.isEmpty() && page != QLatin1String("0")) src += QObject::tr(", S. %1").arg(page);
        s.source = src;

        // Zeiten: Vorbereitung + Kochen (als Uhrzeit am 30.12.1899 gespeichert)
        s.prepMinutes = Jet::Database::minutes(h.value(QStringLiteral("RezZeitVorher"))) + Jet::Database::minutes(h.value(QStringLiteral("RezZeitKochen")));
        s.created = fromAccessDate(h.value(QStringLiteral("RezImportDate")));

        // Foto: JPEG/PNG direkt übernehmen, große Bilder verkleinern
        const QByteArray photo = photos.value(id);
        if (!photo.isEmpty()) {
            const bool direct = (photo.startsWith("\xFF\xD8") || photo.startsWith("\x89PNG")) && photo.size() <= 400 * 1024;
            QImage img;
            if (img.loadFromData(photo)) {
                if (direct && img.width() <= 1600 && img.height() <= 1600) {
                    s.image = QString::fromLatin1(photo.toBase64());
                } else {
                    if (img.width() > 1200 || img.height() > 1200) img = img.scaled(1200, 1200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    QByteArray out;
                    QBuffer buf(&out);
                    buf.open(QIODevice::WriteOnly);
                    img.convertToFormat(QImage::Format_RGB32).save(&buf, "JPEG", 88);
                    if (!out.isEmpty()) s.image = QString::fromLatin1(out.toBase64());
                }
            }
        }
        res.recipes << Importers::fromStructured(s, QStringLiteral("Ambrosia"), opt, &res);
    }
    if (!unknownUnits.isEmpty()) {
        QStringList ex;
        for (auto it = unknownUnits.cbegin(); it != unknownUnits.cend(); ++it) ex << QStringLiteral("Nr. %1: %2").arg(it.key()).arg(it.value());
        std::sort(ex.begin(), ex.end());
        res.warnings << QObject::tr("Unbekannte Ambrosia-Einheiten (Menge ohne Einheit übernommen): %1").arg(ex.join(QStringLiteral("; ")));
    }
    if (skipped > 0) res.warnings << QObject::tr("%1 gelöschte Rezepte bzw. Vorlagen übersprungen.").arg(skipped);
    return res;
}

} // namespace Ambrosia
