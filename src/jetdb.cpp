// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "jetdb.h"

#include <QFile>
#include <QObject>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Jet {

namespace {

// Seitenarten und Kennzeichen der Zeilenzeiger
constexpr quint8 PageData = 0x01;
constexpr quint8 PageTdef = 0x02;
constexpr quint16 RowDeleted = 0x8000;
constexpr quint16 RowLookup = 0x4000;   // Zeile ist nur ein Verweis auf die eigentliche Zeile (Überlauf)
constexpr quint16 OffsetMask = 0x1fff;

enum Type { Bool = 1, Byte = 2, Int = 3, Long = 4, Money = 5, Float = 6, Double = 7, DateTime = 8,
            Binary = 9, Text = 10, Ole = 11, Memo = 12, Guid = 15, Numeric = 16 };

// Tabellendefinition (JET 4)
constexpr int TdefNumRows = 16;
constexpr int TdefNumVarCols = 43;
constexpr int TdefNumCols = 45;
constexpr int TdefNumRealIdx = 51;
constexpr int TdefColsStart = 63;
constexpr int RealIdxEntry = 12;
constexpr int ColEntry = 25;

inline quint16 le16(const char *p) { return quint16(quint8(p[0]) | (quint8(p[1]) << 8)); }
inline quint32 le32(const char *p)
{
    return quint32(quint8(p[0])) | (quint32(quint8(p[1])) << 8) | (quint32(quint8(p[2])) << 16) | (quint32(quint8(p[3])) << 24);
}

} // namespace

quint8 Database::u8(qint64 off) const { return off >= 0 && off < m_data.size() ? quint8(m_data[int(off)]) : 0; }
quint16 Database::u16(qint64 off) const { return off >= 0 && off + 2 <= m_data.size() ? le16(m_data.constData() + off) : 0; }
quint32 Database::u32(qint64 off) const { return off >= 0 && off + 4 <= m_data.size() ? le32(m_data.constData() + off) : 0; }

bool Database::open(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    return openData(f.readAll(), error);
}

bool Database::openData(const QByteArray &data, QString *error)
{
    m_data.clear();
    m_tables.clear();
    m_names.clear();
    auto fail = [error](const QString &e) {
        if (error) *error = e;
        return false;
    };
    if (data.size() < 4096 || !(data.mid(4, 15) == "Standard Jet DB" || data.mid(4, 15) == "Standard ACE DB"))
        return fail(QObject::tr("Die Datei ist keine Microsoft-Access-Datenbank."));
    const quint8 version = quint8(data[0x14]);
    if (version == 0) return fail(QObject::tr("Datenbanken im Format von Access 97 (JET 3) werden nicht unterstützt."));
    m_pageSize = 4096;
    m_data = data;
    // Tabellenverzeichnis MSysObjects: Tabellendefinition auf Seite 2
    Table sys;
    if (!readTable(2, &sys, error)) {
        m_data.clear();
        return false;
    }
    for (const QVariantMap &row : readRows(2, sys)) {
        if (row.value(QStringLiteral("Type")).toInt() != 1) continue;   // 1 = lokale Tabelle
        const QString name = row.value(QStringLiteral("Name")).toString();
        const quint32 page = quint32(row.value(QStringLiteral("Id")).toLongLong()) & 0x00FFFFFF;
        if (name.isEmpty() || page == 0) continue;
        m_tables.insert(name.toLower(), page);
        m_names.insert(name.toLower(), name);
    }
    if (m_tables.isEmpty()) {
        m_data.clear();
        return fail(QObject::tr("Das Tabellenverzeichnis der Datenbank ist leer oder verschlüsselt."));
    }
    return true;
}

QStringList Database::tables() const
{
    QStringList l;
    for (auto it = m_names.cbegin(); it != m_names.cend(); ++it)
        if (!it.value().startsWith(QLatin1String("MSys"), Qt::CaseInsensitive)) l << it.value();
    std::sort(l.begin(), l.end());
    return l;
}

bool Database::hasTable(const QString &name) const { return m_tables.contains(name.toLower()); }

QList<Column> Database::columns(const QString &table) const
{
    Table t;
    if (!hasTable(table) || !readTable(m_tables.value(table.toLower()), &t, nullptr)) return {};
    return t.columns;
}

QList<QVariantMap> Database::rows(const QString &table, QString *error) const
{
    if (!hasTable(table)) {
        if (error) *error = QObject::tr("Tabelle %1 fehlt.").arg(table);
        return {};
    }
    const quint32 page = m_tables.value(table.toLower());
    Table t;
    if (!readTable(page, &t, error)) return {};
    return readRows(page, t);
}

int Database::minutes(const QVariant &v)
{
    if (!v.isValid()) return 0;
    const double days = v.toDouble();
    // Zeitspannen liegen als Uhrzeit am 30.12.1899 (0 Tage) vor; ganze Tage zählen mit
    const double m = std::fabs(days) * 24.0 * 60.0;
    return int(std::lround(m));
}

bool Database::readTable(quint32 tdefPage, Table *t, QString *error) const
{
    auto fail = [error](const QString &e) {
        if (error) *error = e;
        return false;
    };
    if (tdefPage == 0 || int(tdefPage) >= pageCount() || u8(pageStart(tdefPage)) != PageTdef)
        return fail(QObject::tr("Beschädigte Tabellendefinition (Seite %1).").arg(tdefPage));
    // Definition kann über mehrere Seiten reichen (Folgeseiten ab Byte 8)
    QByteArray buf = m_data.mid(int(pageStart(tdefPage)), m_pageSize);
    quint32 next = u32(pageStart(tdefPage) + 4);
    QSet<quint32> seen{ tdefPage };
    while (next != 0 && int(next) < pageCount() && !seen.contains(next)) {
        seen.insert(next);
        buf += m_data.mid(int(pageStart(next)) + 8, m_pageSize - 8);
        next = u32(pageStart(next) + 4);
    }
    const char *p = buf.constData();
    const int n = buf.size();
    if (n < TdefColsStart) return fail(QObject::tr("Beschädigte Tabellendefinition."));
    t->varColumns = le16(p + TdefNumVarCols);
    const int numCols = le16(p + TdefNumCols);
    const quint32 realIdx = le32(p + TdefNumRealIdx);
    qint64 pos = TdefColsStart + qint64(realIdx) * RealIdxEntry;
    if (numCols <= 0 || numCols > 4096 || pos + qint64(numCols) * ColEntry > n) return fail(QObject::tr("Beschädigte Tabellendefinition."));
    QList<Column> cols;
    for (int i = 0; i < numCols; ++i) {
        const char *e = p + pos + i * ColEntry;
        Column c;
        c.type = quint8(e[0]);
        c.number = le16(e + 5);
        c.varIndex = le16(e + 7);
        c.fixed = (quint8(e[15]) & 0x01) != 0;
        c.fixedOffset = le16(e + 21);
        c.size = le16(e + 23);
        cols << c;
    }
    pos += qint64(numCols) * ColEntry;
    for (int i = 0; i < numCols; ++i) {
        if (pos + 2 > n) return fail(QObject::tr("Beschädigte Tabellendefinition."));
        const int len = le16(p + pos);
        pos += 2;
        if (pos + len > n) return fail(QObject::tr("Beschädigte Tabellendefinition."));
        QString name;
        for (int k = 0; k + 1 < len; k += 2) name += QChar(le16(p + pos + k));
        cols[i].name = name;
        pos += len;
    }
    std::sort(cols.begin(), cols.end(), [](const Column &a, const Column &b) { return a.number < b.number; });
    t->columns = cols;
    return true;
}

QList<QVariantMap> Database::readRows(quint32 tdefPage, const Table &t) const
{
    // Datenseiten der Tabelle finden (Seitenart 1, Besitzer = Tabellendefinition)
    QList<quint32> pages;
    for (int pg = 1; pg < pageCount(); ++pg) {
        const qint64 s = pageStart(quint32(pg));
        if (u8(s) == PageData && u32(s + 4) == tdefPage) pages << quint32(pg);
    }
    // Überlaufzeilen: Ziel nicht zusätzlich als eigene Zeile lesen
    QSet<quint32> targets;
    for (quint32 pg : pages) {
        const qint64 s = pageStart(pg);
        const int count = u16(s + 12);
        for (int r = 0; r < count; ++r) {
            const quint16 off = u16(s + 14 + 2 * r);
            if ((off & RowDeleted) || !(off & RowLookup)) continue;
            targets.insert(u32(s + (off & OffsetMask)));
        }
    }
    QList<QVariantMap> out;
    for (quint32 pg : pages) {
        const qint64 s = pageStart(pg);
        const int count = u16(s + 12);
        for (int r = 0; r < count; ++r) {
            const quint16 off = u16(s + 14 + 2 * r);
            if (off & RowDeleted) continue;
            if (off & RowLookup) {
                const quint32 ptr = u32(s + (off & OffsetMask));
                const quint32 tp = ptr >> 8;
                const int tr = int(ptr & 0xff);
                if (tp == 0 || int(tp) >= pageCount()) continue;
                const qint64 ts = pageStart(tp);
                if (tr >= u16(ts + 12)) continue;
                const quint16 toff = u16(ts + 14 + 2 * tr);
                const qint64 tstart = ts + (toff & OffsetMask);
                const qint64 tend = ts + (tr == 0 ? m_pageSize : (u16(ts + 14 + 2 * (tr - 1)) & OffsetMask));
                if (tend > tstart) out << crackRow(t, tstart, tend);
                continue;
            }
            if (targets.contains((pg << 8) | quint32(r))) continue;
            const qint64 start = s + (off & OffsetMask);
            const qint64 end = s + (r == 0 ? m_pageSize : (u16(s + 14 + 2 * (r - 1)) & OffsetMask));
            if (end > start) out << crackRow(t, start, end);
        }
    }
    return out;
}

QVariantMap Database::crackRow(const Table &t, qint64 start, qint64 endExcl) const
{
    QVariantMap row;
    const qint64 rowEnd = endExcl - 1;   // letztes Byte der Zeile
    const int rowCols = u16(start);
    const int maskSize = (rowCols + 7) / 8;
    const qint64 mask = rowEnd - maskSize + 1;
    if (mask <= start) return row;
    int rowVarCols = 0;
    QList<int> varOffsets;
    if (t.varColumns > 0) {
        rowVarCols = u16(rowEnd - maskSize - 1);
        for (int i = 0; i <= rowVarCols; ++i) varOffsets << u16(rowEnd - maskSize - 3 - i * 2);
    }
    const int rowFixedCols = rowCols - rowVarCols;
    int fixedFound = 0;
    for (const Column &c : t.columns) {
        const bool present = c.number / 8 < maskSize && (u8(mask + c.number / 8) & (1 << (c.number % 8)));
        if (c.type == Bool) {   // Ja/Nein steckt in der Null-Bitmaske
            row.insert(c.name, present);
            if (c.fixed) ++fixedFound;
            continue;
        }
        qint64 fstart = 0;
        int fsize = 0;
        if (c.fixed && fixedFound < rowFixedCols) {
            fstart = start + 2 + c.fixedOffset;
            fsize = c.size;
            ++fixedFound;
        } else if (!c.fixed && c.varIndex < rowVarCols) {
            fstart = start + varOffsets.value(c.varIndex);
            fsize = varOffsets.value(c.varIndex + 1) - varOffsets.value(c.varIndex);
        } else {
            row.insert(c.name, QVariant());
            continue;
        }
        if (!present || fsize < 0 || fstart < start || fstart + fsize > endExcl) {
            row.insert(c.name, QVariant());
            continue;
        }
        const char *f = m_data.constData() + fstart;
        QVariant v;
        switch (c.type) {
        case Byte: v = qint64(quint8(f[0])); break;
        case Int: v = qint64(qint16(le16(f))); break;
        case Long: v = qint64(qint32(le32(f))); break;
        case Money: {
            const qint64 raw = qint64(quint64(le32(f)) | (quint64(le32(f + 4)) << 32));
            v = double(raw) / 10000.0;
            break;
        }
        case Float: {
            const quint32 bits = le32(f);
            float x;
            std::memcpy(&x, &bits, 4);
            v = double(x);
            break;
        }
        case Double:
        case DateTime: {
            const quint64 bits = quint64(le32(f)) | (quint64(le32(f + 4)) << 32);
            double x;
            std::memcpy(&x, &bits, 8);
            v = x;
            break;
        }
        case Text: v = text(f, fsize); break;
        case Memo: {
            const QByteArray b = longValue(fstart, fsize);
            v = text(b.constData(), b.size());
            break;
        }
        case Ole: v = longValue(fstart, fsize); break;
        default: v = QByteArray(f, fsize); break;
        }
        row.insert(c.name, v);
    }
    return row;
}

QByteArray Database::rowData(quint32 pointer) const
{
    const quint32 pg = pointer >> 8;
    const int r = int(pointer & 0xff);
    if (pg == 0 || int(pg) >= pageCount()) return {};
    const qint64 s = pageStart(pg);
    if (r >= u16(s + 12)) return {};
    const qint64 start = s + (u16(s + 14 + 2 * r) & OffsetMask);
    const qint64 end = s + (r == 0 ? m_pageSize : (u16(s + 14 + 2 * (r - 1)) & OffsetMask));
    if (end <= start) return {};
    return m_data.mid(int(start), int(end - start));
}

QByteArray Database::longValue(qint64 field, int size) const
{
    if (size < 12) return {};
    const quint32 head = u32(field);
    const int len = int(head & 0x3FFFFFFF);
    if (head & 0x80000000) return m_data.mid(int(field + 12), qMin(len, size - 12));   // direkt in der Zeile
    quint32 ptr = u32(field + 4);
    if (head & 0x40000000) return rowData(ptr).left(len);                             // eine Zeile auf einer LVAL-Seite
    QByteArray out;                                                                    // Kette von LVAL-Zeilen
    QSet<quint32> seen;
    while (ptr != 0 && out.size() < len && !seen.contains(ptr)) {
        seen.insert(ptr);
        const QByteArray part = rowData(ptr);
        if (part.size() < 4) break;
        ptr = le32(part.constData());
        out += part.mid(4);
    }
    return out.left(len);
}

QString Database::text(const char *p, int size)
{
    // JET 4: UTF-16LE; beginnt der Text mit FF FE, ist er „komprimiert“ – ein Nullbyte schaltet zwischen
    // einem Byte je Zeichen (Latin-1) und zwei Bytes je Zeichen um
    QString s;
    if (size >= 2 && quint8(p[0]) == 0xFF && quint8(p[1]) == 0xFE) {
        bool compressed = true;
        int i = 2;
        while (i < size) {
            if (p[i] == 0) {
                compressed = !compressed;
                ++i;
            } else if (compressed) {
                s += QChar(quint8(p[i]));
                ++i;
            } else if (i + 1 < size) {
                s += QChar(le16(p + i));
                i += 2;
            } else {
                break;
            }
        }
        return s;
    }
    s.reserve(size / 2);
    for (int i = 0; i + 1 < size; i += 2) s += QChar(le16(p + i));
    return s;
}

} // namespace Jet
