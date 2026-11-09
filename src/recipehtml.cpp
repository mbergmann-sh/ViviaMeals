// SPDX-License-Identifier: MIT
#include "recipehtml.h"
#include "nutrition.h"

#include <QCoreApplication>
#include <QStringList>

namespace RecipeHtml {

namespace {
QString tr(const char *s) { return QCoreApplication::translate("RecipeHtml", s); }
QString col(const QColor &c) { return c.name(); }
}

QString escape(const QString &s) { return s.toHtmlEscaped(); }

QString tagLabel(const QString &tag)
{
    if (tag.isEmpty()) return tag;
    QString t = tag;
    t[0] = t[0].toUpper();
    return t;
}

QString ingredientsTable(const Recipe &r, const Style &st, double factor)
{
    QString h;
    if (st.forWeb) h += QStringLiteral("<table class=\"ingredients\">");
    else h += QStringLiteral("<table cellspacing=\"0\" cellpadding=\"3\" width=\"100%\">");
    for (const Ingredient &src : r.ingredients) {
        if (src.heading) {
            h += st.forWeb ? QStringLiteral("<tr class=\"group\"><td colspan=\"2\">%1</td></tr>").arg(escape(src.name))
                           : QStringLiteral("<tr><td colspan=\"2\" style=\"padding-top:8px;\"><b style=\"color:%1;\">%2</b></td></tr>")
                                 .arg(col(st.primary), escape(src.name));
            continue;
        }
        Ingredient i = src;
        i.amount *= factor;
        i.amountMax *= factor;
        QString amount = i.amountText();
        if (!i.unit.isEmpty()) amount += (amount.isEmpty() ? QString() : QStringLiteral("&nbsp;")) + escape(i.unit);
        QString name = escape(i.name);
        if (!i.note.isEmpty())
            name += st.forWeb ? QStringLiteral(" <span class=\"note\">%1</span>").arg(escape(i.note))
                              : QStringLiteral(" <span style=\"color:%1;\">%2</span>").arg(col(st.muted), escape(i.note));
        if (st.forWeb)
            h += QStringLiteral("<tr><td class=\"amount\">%1</td><td>%2</td></tr>").arg(amount, name);
        else
            h += QStringLiteral("<tr><td align=\"right\" width=\"110\" style=\"white-space:nowrap; padding-right:10px;\"><b>%1</b></td><td>%2</td></tr>")
                     .arg(amount, name);
    }
    h += QStringLiteral("</table>");
    return h;
}

QString instructionsHtml(const Recipe &r)
{
    QString h;
    int n = 0;
    for (const QString &p : r.instructions.split(QStringLiteral("\n\n"), Qt::SkipEmptyParts)) {
        const QString t = p.trimmed();
        if (t.isEmpty()) continue;
        ++n;
        h += QStringLiteral("<p>%1</p>").arg(escape(t).replace(QLatin1Char('\n'), QStringLiteral("<br/>")));
    }
    Q_UNUSED(n)
    return h;
}

QString nutritionTable(const Recipe &r, const Style &st, double servings)
{
    const NutritionInfo &n = r.nutrition;
    if (!n.valid() || n.matched == 0)
        return QStringLiteral("<p><i>%1</i></p>").arg(tr("Keine Nährwerte berechnet – den Zutaten sind keine Lebensmittel zugeordnet."));
    const double sv = servings > 0 ? servings : r.servings;
    Q_UNUSED(sv)
    const Nutrients &p = n.perServing;
    const Nutrients &t = n.total;
    const Nutrients ri = Nutrition::referenceIntake();
    auto pct = [](double v, double ref) { return ref > 0 ? Fmt::number(v / ref * 100.0, 0) + QStringLiteral("&nbsp;%") : QString(); };
    struct Row { QString label; double per; double total; double ref; QString unit; bool sub; int dec; };
    const QList<Row> rows{
        { tr("Energie"), p.kJ(), t.kJ(), 0, QStringLiteral("kJ"), false, 0 },
        { QString(), p.kcal, t.kcal, ri.kcal, QStringLiteral("kcal"), false, 0 },
        { tr("Fett"), p.fat, t.fat, ri.fat, QStringLiteral("g"), false, 1 },
        { tr("davon gesättigte Fettsäuren"), p.satFat, t.satFat, ri.satFat, QStringLiteral("g"), true, 1 },
        { tr("Kohlenhydrate"), p.carbs, t.carbs, ri.carbs, QStringLiteral("g"), false, 1 },
        { tr("davon Zucker"), p.sugar, t.sugar, ri.sugar, QStringLiteral("g"), true, 1 },
        { tr("Ballaststoffe"), p.fiber, t.fiber, ri.fiber, QStringLiteral("g"), false, 1 },
        { tr("Eiweiß"), p.protein, t.protein, ri.protein, QStringLiteral("g"), false, 1 },
        { tr("Salz"), p.salt, t.salt, ri.salt, QStringLiteral("g"), false, 2 },
    };
    QString h;
    const QString perHead = r.yieldUnit.isEmpty() ? tr("je Portion") : tr("je %1").arg(escape(r.yieldUnit == QStringLiteral("Stück") ? tr("Stück") : r.yieldUnit));
    if (st.forWeb) {
        h += QStringLiteral("<table class=\"nutrition\"><thead><tr><th>%1</th><th>%2</th><th>%3</th><th>%4*</th></tr></thead><tbody>")
                 .arg(tr("Nährwerte"), perHead, tr("gesamt"), tr("RM"));
    } else {
        h += QStringLiteral("<table cellspacing=\"0\" cellpadding=\"4\" style=\"border-collapse:collapse;\" border=\"1\" bordercolor=\"%1\">")
                 .arg(col(st.border));
        h += QStringLiteral("<tr style=\"background-color:%1;\"><th align=\"left\">%2</th><th align=\"right\">%3</th><th align=\"right\">%4</th><th align=\"right\">%5*</th></tr>")
                 .arg(col(st.headBg), tr("Nährwerte"), perHead, tr("gesamt"), tr("RM"));
    }
    for (const Row &row : rows) {
        const QString label = row.sub ? (st.forWeb ? QStringLiteral("<span class=\"sub\">%1</span>").arg(row.label)
                                                   : QStringLiteral("&nbsp;&nbsp;&nbsp;%1").arg(row.label))
                                      : row.label;
        const QString a = Fmt::number(row.per, row.dec) + QStringLiteral("&nbsp;") + row.unit;
        const QString b = Fmt::number(row.total, row.dec) + QStringLiteral("&nbsp;") + row.unit;
        const QString c = row.ref > 0 ? pct(row.per, row.ref) : QString();
        if (st.forWeb)
            h += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td></tr>").arg(label, a, b, c);
        else
            h += QStringLiteral("<tr><td>%1</td><td align=\"right\">%2</td><td align=\"right\">%3</td><td align=\"right\">%4</td></tr>")
                     .arg(label, a, b, c);
    }
    h += st.forWeb ? QStringLiteral("</tbody></table>") : QStringLiteral("</table>");
    const QString mstyle = st.forWeb ? QStringLiteral("class=\"small\"") : QStringLiteral("style=\"color:%1; font-size:small;\"").arg(col(st.muted));
    QString foot = tr("Berechnet aus %1 von %2 Zutaten mit Mengenangabe.").arg(n.matched).arg(n.countable);
    if (!n.unmatched.isEmpty())
        foot += QLatin1Char(' ') + tr("Ohne Nährwerte: %1.").arg(escape(n.unmatched.join(QStringLiteral(", "))));
    foot += QStringLiteral("<br/>") + tr("Richtwerte aus der VitaMeals-Nährwerttabelle – Schätzwerte, keine Analysewerte. "
                                         "* RM = Referenzmenge für einen durchschnittlichen Erwachsenen (8 400 kJ / 2 000 kcal); "
                                         "Ballaststoffe: Richtwert 30 g.");
    h += QStringLiteral("<p %1>%2</p>").arg(mstyle, foot);
    return h;
}

QString render(const Recipe &r, const Style &st, double servings)
{
    const double sv = servings > 0 ? servings : r.servings;
    const double factor = r.servings > 0 ? sv / r.servings : 1.0;
    const double base = 10.0 * st.fontScale;
    Q_UNUSED(base)
    QString h = QStringLiteral("<html><body style=\"color:%1;\">").arg(col(st.text));
    // Kopf: links Titel und Eckdaten, rechts oben das Foto
    QString img;
    if (!r.image.isEmpty()) {
        const QString src = st.imageSrc.isEmpty() ? QStringLiteral("data:image/jpeg;base64,") + r.image : st.imageSrc;
        img = QStringLiteral("<img src=\"%1\" width=\"%2\" alt=\"%3\"/>").arg(src).arg(st.imageWidth).arg(escape(r.title));
    }
    h += QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\"><tr><td valign=\"top\">");
    h += QStringLiteral("<h1 style=\"color:%1; margin-bottom:4px;\">%2%3</h1>")
             .arg(col(st.primary), r.favorite ? QStringLiteral("<span style=\"color:%1;\">★</span> ").arg(col(st.accent)) : QString(),
                  escape(r.title));
    QString meta;
    QString portions = r.yieldUnit.isEmpty() ? (sv == 1.0 ? tr("1 Portion") : tr("%1 Portionen").arg(Fmt::number(sv, 1)))
                                             : Fmt::number(sv, 1) + QLatin1Char(' ') + escape(r.yieldUnit);
    meta += QStringLiteral("<b>%1</b>").arg(portions);
    if (r.prepMinutes > 0) meta += QStringLiteral(" &nbsp;·&nbsp; ") + tr("%1 Minuten").arg(r.prepMinutes);
    h += QStringLiteral("<p style=\"margin-top:0;\">%1</p>").arg(meta);
    if (r.nutrition.valid() && r.nutrition.matched > 0) {
        const Nutrients &p = r.nutrition.perServing;
        const QString per = r.yieldUnit.isEmpty() ? tr("je Portion") : tr("je %1").arg(escape(r.yieldUnit == QStringLiteral("Stück") ? tr("Stück") : r.yieldUnit));
        h += QStringLiteral("<p style=\"margin:0;\"><span style=\"color:%1; font-size:x-large;\"><b>%2&nbsp;kcal</b></span>"
                            " &nbsp;<span style=\"color:%3;\">%4&nbsp;kJ &nbsp;%5</span></p>")
                 .arg(col(st.accent), Fmt::kcal(p.kcal), col(st.muted), Fmt::kcal(p.kJ()), per);
        h += QStringLiteral("<p style=\"margin-top:2px; color:%1;\">%2</p>")
                 .arg(col(st.muted),
                      tr("Eiweiß %1 g · Fett %2 g · Kohlenhydrate %3 g · gesamt %4 kcal / %5 kJ")
                          .arg(Fmt::number(p.protein), Fmt::number(p.fat), Fmt::number(p.carbs),
                               Fmt::kcal(r.nutrition.total.kcal), Fmt::kcal(r.nutrition.total.kJ())));
    }
    if (!r.tags.isEmpty()) {
        QStringList t;
        for (const QString &tag : r.tags)
            t << QStringLiteral("<span style=\"background-color:%1; color:#ffffff;\">&nbsp;%2&nbsp;</span>")
                     .arg(col(tag == QLatin1String("vegan") || tag == QLatin1String("vegetarisch") ? QColor(0x2e, 0x9e, 0x6b) : st.primary),
                          escape(tagLabel(tag)));
        h += QStringLiteral("<p>%1</p>").arg(t.join(QStringLiteral(" ")));
    }
    h += QStringLiteral("</td>");
    if (!img.isEmpty()) h += QStringLiteral("<td valign=\"top\" align=\"right\" width=\"%1\">%2</td>").arg(st.imageWidth + 12).arg(img);
    h += QStringLiteral("</tr></table>");

    h += QStringLiteral("<h2 style=\"color:%1;\">%2</h2>").arg(col(st.primary), tr("Zutaten"));
    h += ingredientsTable(r, st, factor);

    h += QStringLiteral("<h2 style=\"color:%1;\">%2</h2>").arg(col(st.primary), tr("Zubereitung"));
    if (r.language == QLatin1String("en"))
        h += QStringLiteral("<p style=\"color:%1;\"><i>%2</i></p>")
                 .arg(col(st.muted), tr("Anleitung im englischen Original – Temperaturen und Maße wurden in °C und cm umgerechnet."));
    h += r.instructions.trimmed().isEmpty() ? QStringLiteral("<p><i>%1</i></p>").arg(tr("Keine Zubereitung angegeben.")) : instructionsHtml(r);

    h += QStringLiteral("<h2 style=\"color:%1;\">%2</h2>").arg(col(st.primary), tr("Nährwerte"));
    if (factor != 1.0)
        h += QStringLiteral("<p style=\"color:%1;\">%2</p>").arg(col(st.muted), tr("Die Nährwerte je Portion gelten unabhängig von der gewählten Portionenzahl."));
    h += nutritionTable(r, st);
    if (!r.source.isEmpty())
        h += QStringLiteral("<p style=\"color:%1;\">%2 %3</p>").arg(col(st.muted), tr("Quelle:"), escape(r.source));
    h += QStringLiteral("</body></html>");
    return h;
}

QString emptyPage(const Style &st, const QString &title, const QString &text)
{
    return QStringLiteral("<html><body style=\"color:%1;\"><h1 style=\"color:%2;\">%3</h1><p>%4</p></body></html>")
        .arg(col(st.text), col(st.primary), escape(title), text);
}

} // namespace RecipeHtml
