// SPDX-License-Identifier: MIT AND LicenseRef-Commons-Clause-1.0 (siehe LICENSE)
#include "latexexport.h"
#include "nutrition.h"
#include "recipehtml.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>

namespace LatexExport {

namespace {
QString tr(const char *s) { return QCoreApplication::translate("LatexExport", s); }
}

QString escape(const QString &in)
{
    QString out;
    out.reserve(in.size() + 16);
    for (const QChar c : in) {
        switch (c.unicode()) {
        case '\\': out += QStringLiteral("\\textbackslash{}"); break;
        case '&': out += QStringLiteral("\\&"); break;
        case '%': out += QStringLiteral("\\%"); break;
        case '$': out += QStringLiteral("\\$"); break;
        case '#': out += QStringLiteral("\\#"); break;
        case '_': out += QStringLiteral("\\_"); break;
        case '{': out += QStringLiteral("\\{"); break;
        case '}': out += QStringLiteral("\\}"); break;
        case '~': out += QStringLiteral("\\textasciitilde{}"); break;
        case '^': out += QStringLiteral("\\textasciicircum{}"); break;
        case 0x00BD: out += QStringLiteral("\\textonehalf{}"); break;
        case 0x00BC: out += QStringLiteral("\\textonequarter{}"); break;
        case 0x00BE: out += QStringLiteral("\\textthreequarters{}"); break;
        case 0x2153: out += QStringLiteral("1/3"); break;
        case 0x2154: out += QStringLiteral("2/3"); break;
        case 0x215B: out += QStringLiteral("1/8"); break;
        case 0x00D7: out += QStringLiteral("$\\times$"); break;
        case 0x00B0: out += QStringLiteral("\\textdegree{}"); break;
        case 0x00B7: out += QStringLiteral("\\textperiodcentered{}"); break;
        case 0x2605: break;   // Stern (Favorit)
        case 0x2013: out += QStringLiteral("--"); break;
        case 0x2014: out += QStringLiteral("---"); break;
        case 0x201E: out += QStringLiteral("\\glqq{}"); break;
        case 0x201C: out += QStringLiteral("\\grqq{}"); break;
        case 0x00A0: out += QLatin1Char('~'); break;
        default: out += c;
        }
    }
    return out;
}

QString toLatex(const Recipe &r, const Options &opt, const QString &imageFile)
{
    QString t;
    const bool scr = opt.docClass == DocClass::ScrBook;
    if (opt.standalone) {
        t += QStringLiteral("%% Erzeugt von VitaMeals – Rezept „%1“\n").arg(r.title);
        t += scr ? QStringLiteral("\\documentclass[paper=a4,fontsize=11pt,parskip=half,headings=normal]{scrbook}\n")
                 : QStringLiteral("\\documentclass[a4paper,11pt]{book}\n");
        t += QStringLiteral("\\usepackage[utf8]{inputenc}\n\\usepackage[T1]{fontenc}\n\\usepackage{lmodern}\n\\usepackage{textcomp}\n"
                            "\\usepackage[ngerman]{babel}\n\\usepackage{graphicx}\n\\usepackage{booktabs}\n\\usepackage{tabularx}\n"
                            "\\usepackage[table]{xcolor}\n");
        if (!scr) t += QStringLiteral("\\usepackage[a4paper,margin=25mm]{geometry}\n\\setlength{\\parindent}{0pt}\n\\setlength{\\parskip}{0.5\\baselineskip}\n");
        t += QStringLiteral("\\definecolor{vmblau}{HTML}{1474BC}\n\\definecolor{vmorange}{HTML}{E0620D}\n");
        if (scr) t += QStringLiteral("\\addtokomafont{disposition}{\\color{vmblau}}\n");
        t += QStringLiteral("\\begin{document}\n\n");
    } else {
        t += QStringLiteral("%% Erzeugt von VitaMeals – zum Einbinden mit \\input{} in ein Dokument der Klasse %1.\n"
                            "%% Benötigt: graphicx, booktabs, tabularx, xcolor (Farben vmblau, vmorange), textcomp.\n"
                            "%% \\definecolor{vmblau}{HTML}{1474BC} \\definecolor{vmorange}{HTML}{E0620D}\n\n")
                 .arg(scr ? QStringLiteral("scrbook") : QStringLiteral("book"));
    }
    t += QStringLiteral("\\chapter{%1}\n\\label{rez:%2}\n\n").arg(escape(r.title), r.id.left(8));
    // Kopf: links Eckdaten, rechts das Foto
    const bool img = opt.image && !imageFile.isEmpty();
    t += QStringLiteral("\\noindent\\begin{minipage}[t]{%1\\textwidth}\\vspace{0pt}\n").arg(img ? QStringLiteral("0.52") : QStringLiteral("1.0"));
    t += QStringLiteral("\\textbf{%1}").arg(escape(r.servingsText()));
    if (r.prepMinutes > 0) t += QStringLiteral(" \\textperiodcentered{} %1").arg(escape(tr("%1 Minuten").arg(r.prepMinutes)));
    t += QStringLiteral("\\par\n");
    if (opt.nutrition && r.nutrition.valid() && r.nutrition.matched > 0) {
        const Nutrients &p = r.nutrition.perServing;
        t += QStringLiteral("{\\Large\\color{vmorange}\\textbf{%1~kcal}} \\quad %2~kJ %3\\par\n")
                 .arg(Fmt::kcal(p.kcal), Fmt::kcal(p.kJ()), escape(r.yieldUnit.isEmpty() ? tr("je Portion") : tr("je %1").arg(r.yieldUnit)));
    }
    if (!r.tags.isEmpty()) {
        QStringList tags;
        for (const QString &tag : r.tags) tags << escape(RecipeHtml::tagLabel(tag));
        t += QStringLiteral("\\smallskip{\\small\\color{vmblau}%1}\\par\n").arg(tags.join(QStringLiteral(" \\textperiodcentered{} ")));
    }
    t += QStringLiteral("\\end{minipage}");
    if (img)
        t += QStringLiteral("\\hfill\n\\begin{minipage}[t]{0.44\\textwidth}\\vspace{0pt}\\raggedleft\n"
                            "\\includegraphics[width=\\linewidth,keepaspectratio]{%1}\n\\end{minipage}")
                 .arg(QFileInfo(imageFile).completeBaseName());
    t += QStringLiteral("\n\n\\section*{%1}\n").arg(tr("Zutaten"));
    t += QStringLiteral("\\begin{tabularx}{\\textwidth}{@{}r@{\\hspace{0.8em}}X@{}}\n");
    for (const Ingredient &i : r.ingredients) {
        if (i.heading) {
            t += QStringLiteral("\\multicolumn{2}{@{}l}{\\rule{0pt}{1.6em}\\textbf{\\color{vmblau}%1}}\\\\\n").arg(escape(i.name));
            continue;
        }
        QString amount = i.amountText();
        if (!i.unit.isEmpty()) amount += (amount.isEmpty() ? QString() : QStringLiteral("~")) + i.unit;
        QString name = escape(i.name);
        if (!i.note.isEmpty()) name += QStringLiteral(" {\\color{gray}%1}").arg(escape(i.note));
        t += QStringLiteral("\\textbf{%1} & %2\\\\\n").arg(escape(amount).replace(QStringLiteral("\\textasciitilde{}"), QStringLiteral("~")), name);
    }
    t += QStringLiteral("\\end{tabularx}\n\n\\section*{%1}\n").arg(tr("Zubereitung"));
    for (const QString &p : r.instructions.split(QStringLiteral("\n\n"), Qt::SkipEmptyParts))
        t += escape(p.trimmed()) + QStringLiteral("\n\n");
    if (opt.nutrition && r.nutrition.valid() && r.nutrition.matched > 0) {
        const Nutrients &p = r.nutrition.perServing, &a = r.nutrition.total;
        const Nutrients ri = Nutrition::referenceIntake();
        auto row = [&](const QString &label, double pv, double tv, double ref, const QString &unit, int dec) {
            return QStringLiteral("%1 & %2~%5 & %3~%5 & %4\\\\\n")
                .arg(label, Fmt::number(pv, dec), Fmt::number(tv, dec), ref > 0 ? Fmt::number(pv / ref * 100, 0) + QStringLiteral("\\,\\%") : QString(), unit);
        };
        t += QStringLiteral("\\section*{%1}\n{\\small\n\\begin{tabular}{@{}lrrr@{}}\n\\toprule\n").arg(tr("Nährwerte"));
        t += QStringLiteral("& %1 & %2 & %3\\\\\n\\midrule\n")
                 .arg(escape(r.yieldUnit.isEmpty() ? tr("je Portion") : tr("je %1").arg(r.yieldUnit)), tr("gesamt"), tr("RM*"));
        t += QStringLiteral("%1 & %2~kJ / %3~kcal & %4~kJ / %5~kcal & %6\\\\\n")
                 .arg(tr("Energie"), Fmt::kcal(p.kJ()), Fmt::kcal(p.kcal), Fmt::kcal(a.kJ()), Fmt::kcal(a.kcal),
                      Fmt::number(p.kcal / ri.kcal * 100, 0) + QStringLiteral("\\,\\%"));
        t += row(tr("Fett"), p.fat, a.fat, ri.fat, QStringLiteral("g"), 1);
        t += row(QStringLiteral("\\quad ") + tr("davon gesättigte Fettsäuren"), p.satFat, a.satFat, ri.satFat, QStringLiteral("g"), 1);
        t += row(tr("Kohlenhydrate"), p.carbs, a.carbs, ri.carbs, QStringLiteral("g"), 1);
        t += row(QStringLiteral("\\quad ") + tr("davon Zucker"), p.sugar, a.sugar, ri.sugar, QStringLiteral("g"), 1);
        t += row(tr("Ballaststoffe"), p.fiber, a.fiber, ri.fiber, QStringLiteral("g"), 1);
        t += row(tr("Eiweiß"), p.protein, a.protein, ri.protein, QStringLiteral("g"), 1);
        t += row(tr("Salz"), p.salt, a.salt, ri.salt, QStringLiteral("g"), 2);
        t += QStringLiteral("\\bottomrule\n\\end{tabular}\\par\n\\smallskip\n{\\footnotesize %1}\n}\n")
                 .arg(escape(tr("* RM = Referenzmenge für einen durchschnittlichen Erwachsenen (8 400 kJ / 2 000 kcal). "
                                "Richtwerte, berechnet von VitaMeals – keine Analysewerte.")));
    }
    if (!r.source.isEmpty()) t += QStringLiteral("\n\\medskip{\\small\\color{gray}%1 %2}\n").arg(tr("Quelle:"), escape(r.source));
    if (opt.standalone) t += QStringLiteral("\n\\end{document}\n");
    return t;
}

bool exportRecipe(const Recipe &r, const QString &file, const Options &opt, QString *error)
{
    QString imageFile;
    if (opt.image && !r.image.isEmpty()) {
        QImage img;
        img.loadFromData(QByteArray::fromBase64(r.image.toLatin1()));
        if (!img.isNull()) {
            const QFileInfo fi(file);
            imageFile = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral("-foto.jpg");
            img.save(imageFile, "JPG", 90);
        }
    }
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(toLatex(r, opt, imageFile).toUtf8());
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

} // namespace LatexExport
