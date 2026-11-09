// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "weightchart.h"
#include "icons.h"
#include "recipe.h"

#include <QPainter>
#include <QPainterPath>
#include <cmath>

WeightChart::WeightChart(QWidget *parent) : QWidget(parent) {}

void WeightChart::setProfile(const Profile &p)
{
    m_p = p;
    update();
}

void WeightChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette pal = palette();
    const bool dark = pal.color(QPalette::Window).lightness() < 110;
    p.fillRect(rect(), pal.color(QPalette::Base));
    const QColor text = pal.color(QPalette::Text);
    const QColor grid = dark ? QColor(0x44, 0x44, 0x44) : QColor(0xe2, 0xe6, 0xea);
    const QColor blue = dark ? QColor(0x4a, 0x9e, 0xe0) : Icons::primary();
    const QColor orange = dark ? QColor(0xF5, 0x8A, 0x3C) : Icons::accent();
    const QList<WeightEntry> &w = m_p.weights;
    if (w.size() < 1) {
        p.setPen(text);
        p.drawText(rect(), Qt::AlignCenter, tr("Noch keine Gewichtseinträge."));
        return;
    }
    const QFontMetrics fm(font());
    const QRectF area(fm.horizontalAdvance(QStringLiteral("000,0 kg")) + 14, 12, width() - fm.horizontalAdvance(QStringLiteral("000,0 kg")) - 30,
                      height() - fm.height() * 2 - 20);
    double lo = 1e9, hi = -1e9;
    for (const WeightEntry &e : w) {
        lo = qMin(lo, e.kg);
        hi = qMax(hi, e.kg);
    }
    if (m_p.targetKg > 0) {
        lo = qMin(lo, m_p.targetKg);
        hi = qMax(hi, m_p.targetKg);
    }
    lo = std::floor(lo - 1);
    hi = std::ceil(hi + 1);
    QDate d0 = w.first().date, d1 = w.last().date;
    if (d0 == d1) {
        d0 = d0.addDays(-7);
        d1 = d1.addDays(1);
    }
    const double span = qMax<qint64>(1, d0.daysTo(d1));
    auto X = [&](const QDate &d) { return area.left() + area.width() * d0.daysTo(d) / span; };
    auto Y = [&](double kg) { return area.bottom() - area.height() * (kg - lo) / (hi - lo); };

    // Raster und Beschriftung
    p.setFont(font());
    const double stepKg = (hi - lo) > 20 ? 5 : (hi - lo) > 8 ? 2 : 1;
    for (double v = std::ceil(lo / stepKg) * stepKg; v <= hi + 1e-9; v += stepKg) {
        p.setPen(grid);
        p.drawLine(QPointF(area.left(), Y(v)), QPointF(area.right(), Y(v)));
        p.setPen(text);
        p.drawText(QRectF(0, Y(v) - fm.height() / 2.0, area.left() - 6, fm.height()), Qt::AlignRight | Qt::AlignVCenter,
                   Fmt::number(v, 0) + QStringLiteral(" kg"));
    }
    p.setPen(text);
    p.drawText(QRectF(area.left(), area.bottom() + 4, 160, fm.height()), Qt::AlignLeft, d0.toString(QStringLiteral("dd.MM.yy")));
    p.drawText(QRectF(area.right() - 160, area.bottom() + 4, 160, fm.height()), Qt::AlignRight, d1.toString(QStringLiteral("dd.MM.yy")));

    // Zielgewicht
    if (m_p.targetKg > 0) {
        QPen tp(orange, 1.5, Qt::DashLine);
        p.setPen(tp);
        p.drawLine(QPointF(area.left(), Y(m_p.targetKg)), QPointF(area.right(), Y(m_p.targetKg)));
        p.drawText(QRectF(area.left() + 4, Y(m_p.targetKg) - fm.height() - 2, 200, fm.height()), Qt::AlignLeft,
                   tr("Ziel %1 kg").arg(Fmt::number(m_p.targetKg)));
    }
    // Verlauf
    QPainterPath path;
    for (int i = 0; i < w.size(); ++i) {
        const QPointF pt(X(w[i].date), Y(w[i].kg));
        if (i == 0) path.moveTo(pt);
        else path.lineTo(pt);
    }
    p.setPen(QPen(blue, 2.2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    p.setBrush(blue);
    for (const WeightEntry &e : w) p.drawEllipse(QPointF(X(e.date), Y(e.kg)), 3.2, 3.2);
    // letzter Wert
    p.setPen(text);
    const WeightEntry &last = w.last();
    p.drawText(QRectF(X(last.date) - 120, Y(last.kg) - fm.height() - 6, 116, fm.height()), Qt::AlignRight,
               Fmt::number(last.kg) + QStringLiteral(" kg"));
}
