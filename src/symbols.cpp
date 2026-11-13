// ViviaMeals – Sonderzeichen für die Oberfläche (siehe symbols.h)
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "symbols.h"

#include <QAbstractButton>
#include <QApplication>
#include <QFontDatabase>
#include <QHash>
#include <QRawFont>
#include <QStringList>

namespace Sym {

namespace {

struct Choice {
    const char16_t *wanted;
    const char16_t *fallback;   // nur Zeichen aus WGL4 (in jeder Windows-Oberflächenschrift)
};

const Choice &choice(Id id)
{
    static const Choice table[] = {
        { u"\u2713", u"\u221A" },        // Check ✓ → √
        { u"\u2717", u"\u00D7" },        // Cross ✗ → ×
        { u"\u26A0", u"!" },             // Warning ⚠
        { u"\u2139", u"i" },             // Info ℹ
        { u"\u2605", u"*" },             // Star ★
        { u"\u2606", u"\u00B7" },        // StarEmpty ☆ → ·
        { u"\u23F3", u"\u2026" },        // Busy ⏳ → …
        { u"\u25C0", u"\u25C4" },        // Left ◀ → ◄
        { u"\u25B6", u"\u25BA" },        // Right ▶ → ►
        { u"\u2610", u"\u25A1" },        // Box ☐ → □
        { u"\u27F2", u"\u2190" },        // RotateLeft ⟲ → ←
        { u"\u27F3", u"\u2192" },        // RotateRight ⟳ → →
        { u"\u21C5", u"\u2191\u2193" },  // Sync ⇅ → ↑↓
    };
    return table[id];
}

bool rawSupports(const QFont &f, const QString &s)
{
    const QRawFont raw = QRawFont::fromFont(f);
    if (!raw.isValid()) return false;
    for (const uint ucs4 : s.toUcs4())
        if (!raw.supportsCharacter(ucs4)) return false;
    return true;
}

// Symbolschrift, die das Zeichen enthält (nur installierte Familien; QRawFont prüft ohne Ersatzschriftsuche)
QString symbolFamily(const QString &s)
{
    static QHash<QString, QString> cache;
    const auto it = cache.constFind(s);
    if (it != cache.constEnd()) return *it;
    QString found;
    if (qobject_cast<QApplication *>(QCoreApplication::instance())) {
        static const QStringList candidates = [] {
            QStringList out;
            const QStringList fams = QFontDatabase::families();
            for (const QString &f : { QStringLiteral("Segoe UI Symbol"), QStringLiteral("Segoe UI Emoji"), QStringLiteral("Apple Symbols"),
                                      QStringLiteral("DejaVu Sans"), QStringLiteral("Noto Sans Symbols"), QStringLiteral("Noto Sans Symbols2") })
                if (fams.contains(f, Qt::CaseInsensitive)) out << f;
            return out;
        }();
        for (const QString &fam : candidates) {
            QFont f(QApplication::font());
            f.setFamilies({ fam });
            if (QRawFont::fromFont(f).familyName().compare(fam, Qt::CaseInsensitive) == 0 && rawSupports(f, s)) {
                found = fam;
                break;
            }
        }
    }
    cache.insert(s, found);
    return found;
}

// prüft nur die Schrift selbst (QRawFont nimmt das erste Engine der Kette) – löst keine Ersatzschriftsuche aus
bool supported(const QString &s)
{
    static QHash<QString, bool> cache;
    const auto it = cache.constFind(s);
    if (it != cache.constEnd()) return *it;
    bool ok = true;
    if (qobject_cast<QApplication *>(QCoreApplication::instance())) {
        const QRawFont raw = QRawFont::fromFont(QApplication::font());
        if (raw.isValid()) {
            for (const uint ucs4 : s.toUcs4())
                if (!raw.supportsCharacter(ucs4)) {
                    ok = false;
                    break;
                }
        }
    }
    cache.insert(s, ok);
    return ok;
}

} // namespace

QString get(Id id)
{
    const Choice &c = choice(id);
    const QString wanted = QString::fromUtf16(c.wanted);
    return supported(wanted) ? wanted : QString::fromUtf16(c.fallback);
}

QString html(Id id)
{
    const Choice &c = choice(id);
    const QString wanted = QString::fromUtf16(c.wanted);
    if (supported(wanted)) return wanted;
    const QString fam = symbolFamily(wanted);
    if (!fam.isEmpty()) return QStringLiteral("<span style=\"font-family:'%1';\">%2</span>").arg(fam, wanted);
    return QString::fromUtf16(c.fallback).toHtmlEscaped();
}

QString wanted(Id id)
{
    return QString::fromUtf16(choice(id).wanted);
}

bool font(Id id, const QFont &base, QFont *out)
{
    const QString wanted = QString::fromUtf16(choice(id).wanted);
    if (supported(wanted)) return false;
    const QString fam = symbolFamily(wanted);
    if (fam.isEmpty()) return false;
    if (out) {
        *out = base;
        out->setFamilies({ fam });
    }
    return true;
}

void setButton(QAbstractButton *b, Id id, const QString &before, const QString &after)
{
    if (!b) return;
    QFont f;
    QString sym;
    if (font(id, b->font(), &f)) {
        b->setFont(f);   // Symbolschriften von Windows enthalten auch lateinische Buchstaben
        sym = QString::fromUtf16(choice(id).wanted);
    } else {
        sym = get(id);
    }
    QString t = sym;
    if (!before.isEmpty()) t = before + QLatin1Char(' ') + t;
    if (!after.isEmpty()) t += QLatin1Char(' ') + after;
    b->setText(t);
}

QString fixHtml(const QString &htmlText)
{
    QString out = htmlText;
    for (int i = Check; i <= Sync; ++i) {
        const QString w = QString::fromUtf16(choice(Id(i)).wanted);
        if (out.contains(w)) out.replace(w, html(Id(i)));
    }
    return out;
}

QString plainToHtml(const QString &text)
{
    return fixHtml(text.toHtmlEscaped());
}

QString fix(const QString &text)
{
    QString out = text;
    for (int i = Check; i <= Sync; ++i) {
        const QString w = QString::fromUtf16(choice(Id(i)).wanted);
        if (out.contains(w)) out.replace(w, get(Id(i)));
    }
    return out;
}

QString report()
{
    QStringList missing;
    for (int i = Check; i <= Sync; ++i) {
        const QString w = QString::fromUtf16(choice(Id(i)).wanted);
        if (!supported(w)) {
            const QString fam = symbolFamily(w);
            missing << (fam.isEmpty() ? w + QStringLiteral("→") + QString::fromUtf16(choice(Id(i)).fallback) : w + QStringLiteral(" (") + fam + QLatin1Char(')'));
        }
    }
    return QStringLiteral("Oberflächenschrift %1: %2")
        .arg(QApplication::font().family(),
             missing.isEmpty() ? QStringLiteral("alle Sonderzeichen vorhanden")
                               : QStringLiteral("fehlt %1").arg(missing.join(QLatin1Char(' '))));
}

} // namespace Sym
