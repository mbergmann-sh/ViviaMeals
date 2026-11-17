// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "barcode.h"

#include <QHash>
#include <QTransform>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace Barcode {

namespace {

// Breiten (in Modulen) der Ziffern 0–9; L-Code beginnt mit einer Lücke, R-Code mit einem Strich (gleiche Breiten)
const int kL[10][4] = { { 3, 2, 1, 1 }, { 2, 2, 2, 1 }, { 2, 1, 2, 2 }, { 1, 4, 1, 1 }, { 1, 1, 3, 2 },
                        { 1, 2, 3, 1 }, { 1, 1, 1, 4 }, { 1, 3, 1, 2 }, { 1, 2, 1, 3 }, { 3, 1, 1, 2 } };
// G-Code (gerade Parität) = gespiegelter R-Code
const int kG[10][4] = { { 1, 1, 2, 3 }, { 1, 2, 2, 2 }, { 2, 2, 1, 2 }, { 1, 1, 4, 1 }, { 2, 3, 1, 1 },
                        { 1, 3, 2, 1 }, { 4, 1, 1, 1 }, { 2, 1, 3, 1 }, { 3, 1, 2, 1 }, { 2, 1, 1, 3 } };
// erste Ziffer der EAN-13 aus der Parität der linken sechs Ziffern (0 = L, 1 = G)
const char *kParity[10] = { "000000", "001011", "001101", "001110", "010011", "011001", "011100", "010101", "010110", "011010" };

struct Run {
    float width;
    bool dark;
};

// beste Ziffer für vier Breiten; *g = true bei G-Code; Rückgabe -1 = keine
int matchDigit(const float *w, bool allowG, bool *g, float *errOut)
{
    const float sum = w[0] + w[1] + w[2] + w[3];
    if (sum <= 0) return -1;
    float n[4];
    for (int i = 0; i < 4; ++i) n[i] = w[i] * 7.0f / sum;
    int best = -1;
    float bestErr = 1e9f;
    bool bestG = false;
    for (int t = 0; t < (allowG ? 2 : 1); ++t) {
        const int(*tab)[4] = t == 0 ? kL : kG;
        for (int d = 0; d < 10; ++d) {
            float e = 0;
            for (int i = 0; i < 4; ++i) e += std::fabs(n[i] - tab[d][i]);
            if (e < bestErr) {
                bestErr = e;
                best = d;
                bestG = t == 1;
            }
        }
    }
    *g = bestG;
    *errOut = bestErr;
    return bestErr <= 1.7f ? best : -1;
}

bool guardOk(const Run *r, int n, float unit)
{
    for (int i = 0; i < n; ++i)
        if (r[i].width < unit * 0.35f || r[i].width > unit * 2.2f) return false;
    return true;
}

// EAN-13 bzw. EAN-8 in genau total Läufen (beginnt und endet mit einem Strich)
QString decodeWindow(const Run *r, bool ean8)
{
    const int digits = ean8 ? 4 : 6;
    const int total = 3 + digits * 4 + 5 + digits * 4 + 3;
    const int modules = ean8 ? 67 : 95;
    float width = 0;
    for (int i = 0; i < total; ++i) width += r[i].width;
    const float unit = width / modules;
    if (unit < 0.8f) return {};
    if (!guardOk(r, 3, unit) || !guardOk(r + 3 + digits * 4, 5, unit) || !guardOk(r + total - 3, 3, unit)) return {};
    QString left, parity;
    float w[4];
    float errSum = 0;
    for (int d = 0; d < digits; ++d) {
        for (int i = 0; i < 4; ++i) w[i] = r[3 + d * 4 + i].width;
        bool g = false;
        float e = 0;
        const int v = matchDigit(w, !ean8, &g, &e);
        if (v < 0) return {};
        errSum += e;
        left += QChar(u'0' + v);
        parity += g ? QLatin1Char('1') : QLatin1Char('0');
    }
    QString right;
    const int rs = 3 + digits * 4 + 5;
    for (int d = 0; d < digits; ++d) {
        for (int i = 0; i < 4; ++i) w[i] = r[rs + d * 4 + i].width;
        bool g = false;
        float e = 0;
        const int v = matchDigit(w, false, &g, &e);
        if (v < 0) return {};
        errSum += e;
        right += QChar(u'0' + v);
    }
    // einzelne unscharfe Ziffern sind normal, durchweg schlechte Treffer sind Zufall (Text, Muster, Rauschen)
    if (errSum / (2 * digits) > 0.9f) return {};
    QString code;
    if (ean8) {
        code = left + right;
    } else {
        int first = -1;
        for (int i = 0; i < 10; ++i)
            if (parity == QLatin1String(kParity[i])) first = i;
        if (first < 0) return {};
        code = QString::number(first) + left + right;
    }
    return checksumOk(code) ? code : QString();
}

// Helligkeitsverlauf einer Zeile → Läufe (örtliche Schwelle aus Minimum und Maximum der Umgebung)
QVector<Run> runsOf(const QVector<int> &v)
{
    const int n = int(v.size());
    QVector<Run> out;
    if (n < 40) return out;
    const int win = std::max(12, n / 24);
    // gleitendes Minimum/Maximum (einfach, n·win genügt für die Bildgrößen hier)
    QVector<bool> dark(n);
    for (int x = 0; x < n; ++x) {
        int lo = 255, hi = 0;
        for (int k = std::max(0, x - win); k <= std::min(n - 1, x + win); ++k) {
            lo = std::min(lo, v[k]);
            hi = std::max(hi, v[k]);
        }
        dark[x] = hi - lo >= 24 && v[x] < (lo + hi) / 2;
    }
    // Läufe mit Teilpixel-Kanten: Übergang dort, wo der Wert die Schwelle kreuzt
    int x = 0;
    bool cur = dark[0];
    int startX = 0;
    for (x = 1; x <= n; ++x) {
        if (x == n || dark[x] != cur) {
            out.push_back(Run{ float(x - startX), cur });
            if (x < n) {
                cur = dark[x];
                startX = x;
            }
        }
    }
    return out;
}

void scanLine(const QVector<Run> &runs, QHash<QString, int> *votes)
{
    const int n = int(runs.size());
    for (int i = 0; i < n; ++i) {
        if (!runs[i].dark) continue;
        for (bool ean8 : { false, true }) {
            const int total = ean8 ? 43 : 59;
            if (i + total > n || !runs[i + total - 1].dark) continue;
            float width = 0;
            for (int k = i; k < i + total; ++k) width += runs[k].width;
            const float unit = width / (ean8 ? 67 : 95);
            // Ruhezone vor und hinter dem Code
            if (i > 0 && runs[i - 1].width < unit * 3) continue;
            if (i + total < n && runs[i + total].width < unit * 3) continue;
            QVector<Run> w(runs.constBegin() + i, runs.constBegin() + i + total);
            QString c = decodeWindow(w.constData(), ean8);
            if (c.isEmpty()) {   // Code steht auf dem Kopf: Läufe rückwärts lesen
                std::reverse(w.begin(), w.end());
                c = decodeWindow(w.constData(), ean8);
            }
            if (!c.isEmpty()) {
                (*votes)[c] += 1;
                i += total - 1;
                break;
            }
        }
    }
}

} // namespace

bool checksumOk(const QString &digits)
{
    if (digits.size() != 8 && digits.size() != 13) return false;
    int sum = 0;
    const int n = int(digits.size());
    for (int i = 0; i < n - 1; ++i) {
        if (!digits[i].isDigit()) return false;
        const int d = digits[i].unicode() - '0';
        // von rechts gezählt: die Ziffer neben der Prüfziffer hat Gewicht 3
        sum += ((n - 1 - i) % 2 == 1) ? d * 3 : d;
    }
    return digits[n - 1].isDigit() && (10 - sum % 10) % 10 == digits[n - 1].unicode() - '0';
}

Result decode(const QImage &image, int rows)
{
    Result res;
    if (image.isNull()) return res;
    QImage img = image.convertToFormat(QImage::Format_Grayscale8);
    // sehr große Fotos verkleinern, sehr kleine vergrößern (schmale Striche brauchen ≥ 1 Pixel je Modul)
    const int longSide = std::max(img.width(), img.height());
    if (longSide > 1800) img = img.scaled(img.size() * (1800.0 / longSide), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    else if (longSide < 500) img = img.scaled(img.size() * 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QHash<QString, int> votes;
    // waagerecht, senkrecht; nur wenn nichts gefunden: schräg (45° und 135°)
    const int angles[] = { 0, 90, 45, 135 };
    for (int orient = 0; orient < 4; ++orient) {
        if (orient >= 2 && !votes.isEmpty()) break;
        QImage im = orient == 0 ? img : img.transformed(QTransform().rotate(angles[orient]), Qt::SmoothTransformation);
        if (im.format() != QImage::Format_Grayscale8) im = im.convertToFormat(QImage::Format_Grayscale8);
        const int w = im.width(), h = im.height();
        for (int k = 0; k < rows; ++k) {
            const int y = int(h * (0.08 + 0.84 * (k + 0.5) / rows));
            if (y < 1 || y >= h - 1) continue;
            QVector<int> line(w);
            const uchar *a = im.constScanLine(y - 1), *b = im.constScanLine(y), *c = im.constScanLine(y + 1);
            for (int x = 0; x < w; ++x) line[x] = (a[x] + 2 * b[x] + c[x]) / 4;   // drei Zeilen gemittelt gegen Rauschen
            scanLine(runsOf(line), &votes);
        }
        // in der ersten Richtung sicher gefunden: die zweite sparen
        int best = 0;
        for (int v : votes) best = std::max(best, v);
        if (best >= 3) break;
    }
    // ein echter Strichcode trifft mehrere Abtastzeilen; ein einzelner Treffer ist meist Zufall
    for (auto it = votes.cbegin(); it != votes.cend(); ++it)
        if (it.value() >= 2 && (it.value() > res.votes || (it.value() == res.votes && it.key().size() > res.code.size()))) {
            res.votes = it.value();
            res.code = it.key();
        }
    return res;
}

QStringList uniqueLabels(const QStringList &names)
{
    QStringList out;
    for (int i = 0; i < names.size(); ++i) {
        const QString base = names.at(i).trimmed().isEmpty() ? QStringLiteral("Kamera") : names.at(i).trimmed();
        const int total = int(names.count(names.at(i)));
        int nr = 1;
        for (int k = 0; k < i; ++k)
            if (names.at(k) == names.at(i)) ++nr;
        out << (total > 1 && nr > 1 ? QStringLiteral("%1 (%2)").arg(base).arg(nr) : base);
    }
    return out;
}

int pickCamera(const QList<QByteArray> &ids, const QStringList &names, const QByteArray &running,
               const QByteArray &savedId, const QString &savedName, const QByteArray &defaultId)
{
    if (ids.isEmpty()) return -1;
    for (const QByteArray &want : { running, savedId }) {
        if (want.isEmpty()) continue;
        const int i = int(ids.indexOf(want));
        if (i >= 0) return i;
    }
    // ältere Fassungen merkten nur den Namen – bei mehreren Gleichnamigen nicht eindeutig, dann weiter
    if (!savedName.isEmpty() && savedId.isEmpty() && names.count(savedName) == 1) return int(names.indexOf(savedName));
    if (!defaultId.isEmpty()) {
        const int i = int(ids.indexOf(defaultId));
        if (i >= 0) return i;
    }
    return 0;
}

} // namespace Barcode
