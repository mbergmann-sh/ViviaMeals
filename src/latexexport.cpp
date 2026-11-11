// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "latexexport.h"
#include "nutrition.h"
#include "recipehtml.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QImage>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTransform>

#include <cmath>

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
        case '"': out += QStringLiteral("\\textquotedbl{}"); break;   // " ist bei babel-ngerman ein Kurzbefehl
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

QString indexEntry(const QString &term, const QString &sub)
{
    // makeindex: " ! @ | maskieren; Sortierschlüssel ohne Umlaute, damit „Äpfel“ bei A steht
    auto clean = [](QString s) {
        s = s.simplified();
        s.remove(QRegularExpression(QStringLiteral("[\\\\{}]")));
        return s;
    };
    auto quote = [](QString s) {
        QString o;
        for (const QChar c : s) {
            if (c == QLatin1Char('"') || c == QLatin1Char('!') || c == QLatin1Char('@') || c == QLatin1Char('|')) o += QLatin1Char('"');
            o += c;
        }
        return o;
    };
    auto sortKey = [](QString s) {
        s = s.toLower();
        s.replace(QStringLiteral("ä"), QStringLiteral("a")).replace(QStringLiteral("ö"), QStringLiteral("o"))
            .replace(QStringLiteral("ü"), QStringLiteral("u")).replace(QStringLiteral("ß"), QStringLiteral("ss"));
        QString n = s.normalized(QString::NormalizationForm_D);
        QString o;
        for (const QChar c : n)
            if (c.category() != QChar::Mark_NonSpacing) o += c;
        return o;
    };
    auto part = [&](const QString &t) {
        const QString c = clean(t);
        const QString printed = escape(c);
        // " ist bei babel-ngerman ein Kurzbefehl – in \index nur als maskiertes "" verwenden
        return quote(sortKey(c)) + QLatin1Char('@') + quote(printed);
    };
    QString e = part(term);
    if (!sub.trimmed().isEmpty()) e += QLatin1Char('!') + part(sub);
    return e;
}

QString toLatex(const Recipe &r, const Options &opt, const QString &imageFile)
{
    QString t;
    const bool scr = opt.docClass == DocClass::ScrBook;
    const bool standalone = opt.standalone || opt.mode == Mode::Standalone;
    if (standalone) {
        t += QStringLiteral("%% Erzeugt von ViviaMeals – Rezept „%1“\n").arg(r.title);
        t += scr ? QStringLiteral("\\documentclass[paper=a4,fontsize=11pt,parskip=half,headings=normal]{scrbook}\n")
                 : QStringLiteral("\\documentclass[a4paper,11pt]{book}\n");
        t += QStringLiteral("\\usepackage{iftex}\n\\ifPDFTeX\n  \\usepackage[utf8]{inputenc}\n  \\usepackage[T1]{fontenc}\n  \\usepackage{lmodern}\n"
                            "\\else\n  \\usepackage{fontspec}\n\\fi\n\\usepackage{textcomp}\n"
                            "\\usepackage[ngerman]{babel}\n\\usepackage{graphicx}\n\\usepackage{booktabs}\n\\usepackage{tabularx}\n"
                            "\\usepackage[table]{xcolor}\n");
        if (!scr) t += QStringLiteral("\\usepackage[a4paper,margin=25mm]{geometry}\n\\setlength{\\parindent}{0pt}\n\\setlength{\\parskip}{0.5\\baselineskip}\n");
        t += QStringLiteral("\\definecolor{vmblau}{HTML}{1474BC}\n\\definecolor{vmorange}{HTML}{E0620D}\n");
        if (scr) t += QStringLiteral("\\addtokomafont{disposition}{\\color{vmblau}}\n");
        t += QStringLiteral("\\begin{document}\n\n");
    } else {
        t += QStringLiteral("%% Erzeugt von ViviaMeals – Rezept zum Einbinden mit \\input{} in ein Master-Dokument der Klasse %1.\n"
                            "%% Benötigt: graphicx, booktabs, tabularx, xcolor (Farben vmblau, vmorange), textcomp.\n"
                            "%% \\definecolor{vmblau}{HTML}{1474BC} \\definecolor{vmorange}{HTML}{E0620D}\n\n")
                 .arg(scr ? QStringLiteral("scrbook") : QStringLiteral("book"));
    }
    const bool asSection = opt.heading == Heading::Section && !standalone;
    if (!standalone && !asSection) {
        // Vorschlag des Nutzers: bei zweiseitigen Dokumenten (twoside) offene Gleitobjekte ausgeben und sauber
        // auf einer neuen Seite beginnen; \\chapter springt danach (open=right) auf die nächste rechte Seite
        t += QStringLiteral("\\clearpage\n%% Empfohlen bei zweiseitigen Dokumenten (twoside), um sauber im Hauptteil zu starten\n");
    }
    // neue Seite – außer für das erste Rezept direkt nach der Kapitelüberschrift (sonst stünde sie allein auf einer Seite)
    if (asSection && opt.newPage)
        t += QStringLiteral("%% neue Seite (twoside) – nicht beim ersten Rezept direkt unter der Kapitelüberschrift\n"
                            "\\ifnum\\value{section}>0 \\clearpage\\fi\n");
    t += QStringLiteral("\\%3{%1}\n\\label{rez:%2}\n").arg(escape(r.title), r.id.left(8), asSection ? QStringLiteral("section") : QStringLiteral("chapter"));
    // Unterüberschriften: bei \\chapter als Abschnitt in der Kopfzeile, bei \\section steht dort der Rezepttitel
    auto heading = [&](const QString &h) {
        return asSection ? QStringLiteral("\\subsection*{%1}\n").arg(h) : QStringLiteral("\\section*{%1}\\markright{%1}\n").arg(h);
    };
    if (opt.indexEntries) {
        t += QStringLiteral("\\index{%1}\n").arg(indexEntry(r.title));
        QStringList seen;
        for (const Ingredient &i : r.ingredients) {
            if (i.heading || i.name.trimmed().isEmpty()) continue;
            const QString key = i.name.trimmed().toLower();
            static const QStringList trivial{ QStringLiteral("salz"), QStringLiteral("pfeffer"), QStringLiteral("wasser"),
                                              QStringLiteral("salz und pfeffer"), QStringLiteral("zucker") };
            if (seen.contains(key) || trivial.contains(key) || key.size() < 3) continue;
            seen << key;
            t += QStringLiteral("\\index{%1}\n").arg(indexEntry(i.name.trimmed(), r.title));
        }
    }
    t += QStringLiteral("\n");
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
    if (img) {
        QString path = opt.imagePrefix + QFileInfo(imageFile).completeBaseName();
        t += QStringLiteral("\\hfill\n\\begin{minipage}[t]{0.44\\textwidth}\\vspace{0pt}\\raggedleft\n"
                            "\\includegraphics[width=\\linewidth,keepaspectratio]{%1}\n\\end{minipage}")
                 .arg(path);
    }
    // Abschnitte ohne Nummer, aber als „Abschnitt“ in der Kopfzeile
    t += QStringLiteral("\n\n") + heading(tr("Zutaten"));
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
    t += QStringLiteral("\\end{tabularx}\n\n") + heading(tr("Zubereitung"));
    for (const QString &p : r.instructions.split(QStringLiteral("\n\n"), Qt::SkipEmptyParts))
        t += escape(p.trimmed()) + QStringLiteral("\n\n");
    if (opt.nutrition && r.nutrition.valid() && r.nutrition.matched > 0) {
        const Nutrients &p = r.nutrition.perServing, &a = r.nutrition.total;
        const Nutrients ri = Nutrition::referenceIntake();
        auto row = [&](const QString &label, double pv, double tv, double ref, const QString &unit, int dec) {
            return QStringLiteral("%1 & %2~%5 & %3~%5 & %4\\\\\n")
                .arg(label, Fmt::number(pv, dec), Fmt::number(tv, dec), ref > 0 ? Fmt::number(pv / ref * 100, 0) + QStringLiteral("\\,\\%") : QString(), unit);
        };
        t += heading(tr("Nährwerte")) + QStringLiteral("{\\small\n\\begin{tabular}{@{}lrrr@{}}\n\\toprule\n");
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
                                "Richtwerte, berechnet von ViviaMeals – keine Analysewerte.")));
    }
    if (opt.source && !r.source.isEmpty()) t += QStringLiteral("\n\\medskip{\\small\\color{gray}%1 %2}\n").arg(tr("Quelle:"), escape(r.source));
    if (standalone) t += QStringLiteral("\n\\end{document}\n");
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

QString masterMarker()
{
    return QStringLiteral("% ViviaMeals: weitere Rezepte hier");
}

QString relativeInput(const QString &masterFile, const QString &recipeFile)
{
    QString rel = QDir(QFileInfo(masterFile).absolutePath()).relativeFilePath(QFileInfo(recipeFile).absoluteFilePath());
    rel = QDir::fromNativeSeparators(rel);
    if (rel.endsWith(QLatin1String(".tex"), Qt::CaseInsensitive)) rel.chop(4);
    return rel;
}

QString prefaceFile(DocClass cls)
{
    QString t = QStringLiteral("%% Vorwort zum Kochbuch – erzeugt von ViviaMeals, beliebig änderbar (wird nicht überschrieben).\n");
    if (cls == DocClass::ScrBook) t += QStringLiteral("\\addchap{Vorwort}\n");
    else t += QStringLiteral("\\chapter*{Vorwort}\n\\addcontentsline{toc}{chapter}{Vorwort}\n\\markboth{Vorwort}{Vorwort}\n");   // book kennt \\addchap nicht
    t += QStringLiteral("Lorem Ipsu verum saudumm\n");
    return t;
}

QString glossaryFile()
{
    struct G { const char *key, *name, *desc; };
    static const G entries[] = {
        { "abloeschen", "ablöschen", "Angebratenes mit Flüssigkeit (Wein, Brühe, Wasser) aufgießen und den Bratsatz lösen." },
        { "abschrecken", "abschrecken", "Gegartes kurz in eiskaltes Wasser geben, um den Garvorgang zu stoppen und Farbe zu erhalten." },
        { "aldente", "al dente", "„Bissfest“ – Nudeln oder Gemüse mit leichtem Widerstand im Kern." },
        { "blanchieren", "blanchieren", "Kurz in sprudelnd kochendem Salzwasser garen und anschließend abschrecken." },
        { "glasieren", "glasieren", "Gargut mit Butter, Zucker oder eingekochtem Fond überziehen, bis es glänzt." },
        { "legieren", "legieren", "Suppen oder Soßen mit Eigelb oder Sahne binden; danach nicht mehr kochen lassen." },
        { "mehlschwitze", "Mehlschwitze", "In Fett angeschwitztes Mehl als Grundlage zum Binden von Soßen (Roux)." },
        { "pochieren", "pochieren", "Knapp unter dem Siedepunkt (etwa 75–95° C) schonend in Flüssigkeit garen." },
        { "reduzieren", "reduzieren", "Flüssigkeit offen einkochen lassen, um Geschmack und Bindung zu verstärken." },
        { "wasserbad", "Wasserbad", "Schüssel über heißem, nicht kochendem Wasser – für Schokolade, Saucen und Cremes." },
    };
    QString t = QStringLiteral("%% Glossar zum Kochbuch – erzeugt von ViviaMeals, beliebig erweiterbar (alphabetisch einfügen).\n"
                               "%% Eintrag:  \\glossareintrag{Begriff}{Erklärung.}\n"
                               "%% Der Begriff erscheint zusätzlich im Stichwortverzeichnis.\n\n");
    for (const G &g : entries)
        t += QStringLiteral("\\glossareintrag{%1}{%2}\n").arg(escape(QString::fromUtf8(g.name)), escape(QString::fromUtf8(g.desc)));
    return t;
}

QString masterDocument(const MasterOptions &m, const QStringList &inputs, const QString &halfTitleFile, const QString &titleImageFile)
{
    const bool scr = m.docClass == DocClass::ScrBook;
    const QString bcor = QString::number(m.bindingMm, 'f', m.bindingMm == std::floor(m.bindingMm) ? 0 : 1);
    const QString bgShift = m.bgShiftBinding ? bcor + QStringLiteral("mm") : QStringLiteral("0mm");
    // Hintergrundbild als eso-pic-Befehl (wirkt auf die nächste ausgegebene Seite)
    QString bgCmd;
    if (!halfTitleFile.isEmpty()) {
        auto mm = [](double v) { return QString::number(v, 'f', v == std::floor(v) ? 0 : 1) + QStringLiteral("mm"); };
        const QString img = QFileInfo(halfTitleFile).completeBaseName();
        switch (m.bgPlacement) {
        case MasterOptions::BgPlacement::FillPage:
            bgCmd = QStringLiteral("%% Hintergrundbild: ganze Seite (auf das Seitenformat zugeschnitten)\n"
                                   "\\AddToShipoutPictureBG*{\\includegraphics[width=\\paperwidth,height=\\paperheight]{%1}}\n").arg(img);
            break;
        case MasterOptions::BgPlacement::Centered:
            bgCmd = QStringLiteral("%% Hintergrundbild: %2 breit, waagerecht mittig im sichtbaren Bereich, %3 über der unteren Papierkante\n"
                                   "\\AddToShipoutPictureBG*{%%\n\t\\AtPageLowerLeft{%%\n\t\t\\hspace*{\\vmBindekorrektur}%%\n"
                                   "\t\t\\makebox[\\dimexpr\\paperwidth-\\vmBindekorrektur\\relax]{\\raisebox{%3}{\\includegraphics[width=%2]{%1}}}%%\n\t}%%\n}\n")
                        .arg(img, mm(m.bgWidthMm), mm(m.bgBottomMm));
            break;
        case MasterOptions::BgPlacement::Free:
            bgCmd = QStringLiteral("%% Hintergrundbild: %2 breit, links %4 ab Papierkante (bei Bindekorrektur automatisch verschoben),\n"
                                   "%% %3 über der unteren Papierkante\n"
                                   "\\AddToShipoutPictureBG*{%%\n\t\\AtPageLowerLeft{%%\n\t\t\\BindungsHSpace{%4}%%\n"
                                   "\t\t\\raisebox{%3}{%%\n\t\t\t\\includegraphics[width=%2]{%1}%%\n\t\t}%%\n\t}%%\n}\n")
                        .arg(img, mm(m.bgWidthMm), mm(m.bgBottomMm), mm(m.bgLeftMm));
            break;
        }
    }
    const bool bgOnHalfTitle = !bgCmd.isEmpty() && m.bgTarget == MasterOptions::BgTarget::HalfTitle;
    const bool bgOnTitle = !bgCmd.isEmpty() && m.bgTarget == MasterOptions::BgTarget::TitlePage;
    QString t;
    t += QStringLiteral(
        "%% ===========================================================================\n"
        "%% %1 – Master-Dokument (Kochbuch), erzeugt von ViviaMeals\n"
        "%% Übersetzen:  latexmk -pdf %2    (oder: pdflatex – 3× aufrufen;\n"
        "%%              LuaLaTeX/XeLaTeX gehen ebenso: latexmk -lualatex %2)\n"
        "%% Das Stichwortverzeichnis wird dabei automatisch erzeugt (makeindex).\n"
        "%% Rezepte aus ViviaMeals: Datei → Export → Als LaTeX-Datei exportieren → „In Master-Dokument eintragen“.\n"
        "%% ===========================================================================\n")
             .arg(m.title, QStringLiteral("<datei>.tex"));
    // Klasse: zweiseitig, Bindekorrektur für die Ringbindung
    if (scr)
        t += QStringLiteral("\\documentclass[paper=a4,fontsize=11pt,twoside=true,BCOR=%1mm,DIV=12,parskip=half,headings=normal,\n"
                            "  headinclude=true,footinclude=false,titlepage=true,open=right,cleardoublepage=empty]{scrbook}\n").arg(bcor);
    else
        t += QStringLiteral("\\documentclass[a4paper,11pt,twoside,openright]{book}\n"
                            "\\usepackage[a4paper,twoside,bindingoffset=%1mm,inner=22mm,outer=22mm,top=28mm,bottom=30mm,headheight=14pt]{geometry}\n"
                            "\\setlength{\\parindent}{0pt}\n\\setlength{\\parskip}{0.5\\baselineskip}\n").arg(bcor);
    t += QStringLiteral(
        "\n%% ---- Schrift und Sprache: deutsche Umlaute mit pdfLaTeX, LuaLaTeX und XeLaTeX\n"
        "\\usepackage{iftex}\n"
        "\\ifPDFTeX\n  \\usepackage[utf8]{inputenc}\n  \\usepackage[T1]{fontenc}\n  \\usepackage{lmodern}\n"
        "\\else\n  \\usepackage{fontspec}\n\\fi\n"
        "\\usepackage[ngerman]{babel}\n"
        "\\usepackage{textcomp}\n"
        "\\usepackage{microtype}\n"
        "\\usepackage[autostyle=true,german=quotes]{csquotes}\n"
        "\n%% ---- Farben (wie ViviaMeals)\n"
        "\\usepackage[table]{xcolor}\n"
        "\\definecolor{vmblau}{HTML}{1474BC}\n\\definecolor{vmorange}{HTML}{E0620D}\n"
        "@TITELFARBEN@"
        "\n%% ---- Professionelle Tabellen\n"
        "\\usepackage{array}       %% eigene Spaltentypen\n"
        "\\usepackage{booktabs}    %% \\toprule, \\midrule, \\bottomrule\n"
        "\\usepackage{tabularx}    %% Tabellen in Textbreite (Spalte X)\n"
        "\\usepackage{longtable}   %% mehrseitige Tabellen\n"
        "\\usepackage{multirow}    %% verbundene Zeilen\n"
        "\\usepackage[locale=DE]{siunitx}  %% Zahlen und Einheiten: \\SI{200}{\\celsius}, Spalte S (Dezimalkomma)\n"
        "\\newcolumntype{L}[1]{>{\\raggedright\\arraybackslash}p{#1}}\n"
        "\\newcolumntype{R}[1]{>{\\raggedleft\\arraybackslash}p{#1}}\n"
        "\\newcolumntype{C}[1]{>{\\centering\\arraybackslash}p{#1}}\n"
        "\\renewcommand{\\arraystretch}{1.15}\n"
        "\n%% ---- Grafiken platzieren\n"
        "\\usepackage{graphicx}\n"
        "\\usepackage{float}       %% [H] = genau hier\n"
        "\\usepackage{wrapfig}     %% vom Text umflossene Bilder\n"
        "\\usepackage[font=small,labelfont=bf,format=hang]{caption}\n"
        "\\usepackage{subcaption}  %% mehrere Bilder nebeneinander\n"
        "\\usepackage{eso-pic}     %% Hintergrundbild (Schmutztitel/Titelseite)\n"
        "%% Horizontaler Abstand ab der linken Papierkante, um die Bindekorrektur verschoben (rechte Seiten)\n"
        "\\newlength{\\vmBindekorrektur}\n"
        "\\setlength{\\vmBindekorrektur}{@BGSHIFT@}\n"
        "\\newcommand{\\BindungsHSpace}[1]{\\hspace*{\\dimexpr#1+\\vmBindekorrektur\\relax}}\n"
        "\\newlength{\\vmTitelbildBreite}\n"
        "\\graphicspath{{./}{bilder/}}\n"
        "%% Bild mit Unterschrift genau an dieser Stelle:  \\rezeptbild[0.8]{datei}{Unterschrift}\n"
        "\\newcommand{\\rezeptbild}[3][0.8]{\\begin{figure}[H]\\centering\\includegraphics[width=#1\\linewidth]{#2}\\caption{#3}\\end{figure}}\n"
        "%% Bild rechts, vom Text umflossen:                \\randbild{datei}{Unterschrift}\n"
        "\\newcommand{\\randbild}[2]{\\begin{wrapfigure}{r}{0.4\\linewidth}\\centering\\includegraphics[width=\\linewidth]{#1}\\caption{#2}\\end{wrapfigure}}\n");
    t.replace(QStringLiteral("@BGSHIFT@"), bgShift);
    {
        // Titel- und Untertitelfarbe, Schatten: \vmSchatten[Breite]{Farbe}{Text} – auch mehrzeilig (zwei übereinander
        // gelegte \parbox, die hintere um 0,06 em nach rechts unten versetzt)
        auto hex = [](const QString &c, const char *fallback) {
            QString h = c.trimmed();
            if (h.startsWith(QLatin1Char('#'))) h = h.mid(1);
            static const QRegularExpression ok(QStringLiteral("^[0-9A-Fa-f]{6}$"));
            return ok.match(h).hasMatch() ? h.toUpper() : QString::fromLatin1(fallback);
        };
        QString c = QStringLiteral("%% Farben von Buchtitel und Untertitel, Schatten (in ViviaMeals einstellbar)\n"
                                   "\\definecolor{vmTitel}{HTML}{%1}\n\\definecolor{vmUntertitel}{HTML}{%2}\n\\definecolor{vmSchatten}{HTML}{%3}\n")
                        .arg(hex(m.titleColor, "000000"), hex(m.subtitleColor, "000000"), hex(m.shadowColor, "8C8C8C"));
        if (m.titleShadow)
            c += QStringLiteral("\\newcommand{\\vmSchatten}[3][\\linewidth]{\\leavevmode\\makebox[0pt][l]{\\raisebox{-0.06em}{\\hspace{0.06em}%\n"
                                "  \\parbox[b]{#1}{\\centering\\color{vmSchatten}#3}}}\\parbox[b]{#1}{\\centering\\color{#2}#3}}\n");
        else
            c += QStringLiteral("\\newcommand{\\vmSchatten}[3][\\linewidth]{\\parbox[b]{#1}{\\centering\\color{#2}#3}}   %% ohne Schatten\n");
        t.replace(QStringLiteral("@TITELFARBEN@"), c);
    }
    if (m.glossary || m.index) t += QStringLiteral("\n%% ---- Verzeichnisse\n");
    if (m.index) t += QStringLiteral("\\usepackage{imakeidx}\n\\makeindex[title=Stichwortverzeichnis,intoc=true,columns=2]\n");
    t += QStringLiteral("\\usepackage[hidelinks,unicode,bookmarksnumbered]{hyperref}\n");
    if (m.glossary)
        t += QStringLiteral("%% Glossar: Einträge in glossar.tex – \\glossareintrag{Begriff}{Erklärung} (ohne Zusatzprogramme)\n"
                            "\\newcommand{\\glossareintrag}[2]{\\item[#1]%1 #2}\n")
                 .arg(m.index ? QStringLiteral("\\index{#1}") : QString());
    // Kopf- und Fußzeilen
    t += QStringLiteral("\n%% ---- Kopfzeile (Kapitel | Abschnitt) und Fußzeile (Seitenzahl), jeweils mit Trennlinie\n");
    if (scr) {
        t += QStringLiteral(
            "\\usepackage[headsepline,footsepline,plainfootsepline]{scrlayer-scrpage}\n"
            "\\clearpairofpagestyles\n"
            "\\automark[section]{chapter}      %% links: Kapitel, rechts: Abschnitt\n"
            "\\ohead{\\headmark}\n"
            "\\cfoot*{\\pagemark}             %% * = auch auf Kapitelanfangs- und Verzeichnisseiten\n"
            "\\addtokomafont{pageheadfoot}{\\normalfont\\small\\color{darkgray}}\n"
            "\\addtokomafont{disposition}{\\color{vmblau}}\n"
            "\\pagestyle{scrheadings}\n");
    } else {
        t += QStringLiteral(
            "\\usepackage{fancyhdr}\n"
            "\\renewcommand{\\chaptermark}[1]{\\markboth{\\thechapter\\ #1}{}}\n"
            "%% eigener Stil (vollständig, damit der Wechsel zu plain nichts verstellt)\n"
            "\\fancypagestyle{kochbuch}{%\n"
            "  \\fancyhf{}%\n"
            "  \\fancyhead[LE]{\\small\\leftmark}%   gerade Seiten: Kapitel\n"
            "  \\fancyhead[RO]{\\small\\rightmark}%  ungerade Seiten: Abschnitt\n"
            "  \\fancyfoot[C]{\\thepage}%\n"
            "  \\renewcommand{\\headrulewidth}{0.4pt}%\n"
            "  \\renewcommand{\\footrulewidth}{0.4pt}}\n"
            "%% Kapitelanfänge und Inhaltsverzeichnis: nur Fußzeile\n"
            "\\fancypagestyle{plain}{%\n"
            "  \\fancyhf{}%\n"
            "  \\fancyfoot[C]{\\thepage}%\n"
            "  \\renewcommand{\\headrulewidth}{0pt}%\n"
            "  \\renewcommand{\\footrulewidth}{0.4pt}}\n"
            "\\pagestyle{kochbuch}\n"
            "%% Vakatseiten (leere linke Seiten vor einem neuen Kapitel) ganz ohne Kopf- und Fußzeile\n"
            "\\makeatletter\n\\renewcommand*{\\cleardoublepage}{\\clearpage\\if@twoside\\ifodd\\c@page\\else\\hbox{}\\thispagestyle{empty}\\newpage"
            "\\if@twocolumn\\hbox{}\\newpage\\fi\\fi\\fi}\n\\makeatother\n");
    }
    // Titelangaben (für die PDF-Eigenschaften; Titelseite wird unten selbst gesetzt)
    t += QStringLiteral("\n%% ---- Titelangaben\n\\title{%1}\n\\author{%2}\n").arg(escape(m.title), escape(m.author));
    t += m.noDate ? QStringLiteral("\\date{}   %% kein Datum auf dem Titelblatt\n") : QStringLiteral("\\date{\\today}\n");
    t += QStringLiteral("\\hypersetup{pdftitle={%1},pdfauthor={%2}}\n").arg(escape(m.title), escape(m.author));
    t += QStringLiteral("\n\\begin{document}\n\\frontmatter\n");
    // Schmutztitel mit Hintergrundbild
    t += QStringLiteral("\n%% ---- Schmutztitel\n\\begin{titlepage}\n\\thispagestyle{empty}\n");
    if (bgOnHalfTitle) t += bgCmd;
    t += QStringLiteral("\\vspace*{0.30\\textheight}\n\\begin{center}\n"
                        "\\colorbox{white}{\\parbox{0.8\\textwidth}{\\centering\\vspace{1em}{\\Huge\\bfseries\\vmSchatten{vmTitel}{%1}\\par}\\vspace{1em}}}\n"
                        "\\end{center}\n\\end{titlepage}\n")
             .arg(escape(m.title));
    // Titelseite: Titel und Untertitel mit Schatten in den gewählten Farben
    t += QStringLiteral("\n%% ---- Titelseite\n");
    if (bgOnTitle) t += QStringLiteral("\\cleardoublepage\n") + bgCmd;
    const bool titleImg = !titleImageFile.isEmpty();
    auto mm = [](double v) { return QString::number(v, 'f', v == std::floor(v) ? 0 : 1) + QStringLiteral("mm"); };
    t += QStringLiteral("\\begin{titlepage}\n\\centering\n\\vspace*{%2\\textheight}\n{\\Huge\\bfseries\\vmSchatten{vmTitel}{%1}\\par}\n")
             .arg(escape(m.title), titleImg ? QStringLiteral("0.10") : QStringLiteral("0.25"));
    if (!m.subtitle.trimmed().isEmpty())
        t += QStringLiteral("\\vspace{1em}\n{\\Large\\vmSchatten{vmUntertitel}{%1}\\par}\n").arg(escape(m.subtitle.trimmed()));
    if (titleImg) {
        // Bild des Schmutztitels: Abstand zur Überschrift, Breite (höchstens Satzspiegelbreite bzw. 45 % der Höhe), Abstand zum Autor
        t += QStringLiteral("%% Titelbild: Abstand zur Überschrift %1, Breite %2, Abstand zum Autor %3 (in ViviaMeals einstellbar)\n"
                            "\\vspace{%1}\n"
                            "\\setlength{\\vmTitelbildBreite}{%2}\\ifdim\\vmTitelbildBreite>\\linewidth\\setlength{\\vmTitelbildBreite}{\\linewidth}\\fi\n"
                            "\\includegraphics[width=\\vmTitelbildBreite,height=0.45\\textheight,keepaspectratio]{%4}\\par\n"
                            "\\vspace{%3}\n")
                 .arg(mm(m.titleImageGapAboveMm), mm(m.titleImageWidthMm), mm(m.titleImageGapBelowMm), QFileInfo(titleImageFile).completeBaseName());
    }
    if (!m.author.trimmed().isEmpty()) t += QStringLiteral("%1{\\large %2\\par}\n").arg(titleImg ? QString() : QStringLiteral("\\vspace{3em}\n"), escape(m.author));
    if (!m.noDate) t += QStringLiteral("\\vspace{1em}\n{\\large\\today\\par}\n");
    t += QStringLiteral("\\end{titlepage}\n");
    if (m.toc) {
        t += QStringLiteral("\n%% ---- Inhaltsverzeichnis (ohne Kopfzeile)\n");
        t += scr ? QStringLiteral("\\cleardoubleoddpage\n\\pagestyle{plain.scrheadings}\n\\tableofcontents\n\\cleardoubleoddpage\n\\pagestyle{scrheadings}\n")
                 : QStringLiteral("\\cleardoublepage\n\\pagestyle{plain}\n\\tableofcontents\n\\cleardoublepage\n\\pagestyle{kochbuch}\n");
    }
    // Vorwort (eigene Datei, wird beim Neuerzeugen nicht überschrieben) – vor dem Hauptteil
    t += QStringLiteral("\n%% ---- Vorwort (vorwort.tex)\n\\input{vorwort}\n");
    t += QStringLiteral("\n\\mainmatter\n\n%% ---- Rezepte\n");
    if (!m.firstChapter.trimmed().isEmpty()) t += QStringLiteral("\\chapter{%1}\n").arg(escape(m.firstChapter.trimmed()));
    for (const QString &in : inputs) t += QStringLiteral("\\input{%1}\n").arg(in);
    t += masterMarker() + QStringLiteral("\n\n\\backmatter\n");
    if (m.glossary)
        t += scr ? QStringLiteral("\\addchap{Glossar}\n\\begin{description}\n\\input{glossar}\n\\end{description}\n")
                 : QStringLiteral("\\chapter*{Glossar}\n\\addcontentsline{toc}{chapter}{Glossar}\n\\markboth{Glossar}{Glossar}\n"
                                  "\\begin{description}\n\\input{glossar}\n\\end{description}\n");
    if (m.index) t += QStringLiteral("\\printindex\n");
    t += QStringLiteral("\n\\end{document}\n");
    return t;
}

QImage trimWhite(const QImage &src, int threshold)
{
    const QImage img = src.convertToFormat(QImage::Format_RGB32);
    auto white = [&](int x, int y) {
        const QRgb c = img.pixel(x, y);
        return qRed(c) >= threshold && qGreen(c) >= threshold && qBlue(c) >= threshold;
    };
    auto rowWhite = [&](int y) {
        for (int x = 0; x < img.width(); ++x)
            if (!white(x, y)) return false;
        return true;
    };
    auto colWhite = [&](int x, int y0, int y1) {
        for (int y = y0; y <= y1; ++y)
            if (!white(x, y)) return false;
        return true;
    };
    int top = 0, bottom = img.height() - 1;
    while (top < bottom && rowWhite(top)) ++top;
    while (bottom > top && rowWhite(bottom)) --bottom;
    int left = 0, right = img.width() - 1;
    while (left < right && colWhite(left, top, bottom)) ++left;
    while (right > left && colWhite(right, top, bottom)) --right;
    if (right - left < 8 || bottom - top < 8) return img;   // ganz weiß: unverändert
    return img.copy(left, top, right - left + 1, bottom - top + 1);
}

QImage prepareBackground(const QImage &src, const MasterOptions &m)
{
    QImage img = src;
    if (img.hasAlphaChannel()) {
        QImage flat(img.size(), QImage::Format_RGB32);
        flat.fill(Qt::white);
        QPainter p(&flat);
        p.drawImage(0, 0, img);
        p.end();
        img = flat;
    }
    if (m.bgRotation % 360 != 0) img = img.transformed(QTransform().rotate(m.bgRotation));
    if (!m.bgCrop.isEmpty()) {
        const QRect r = QRectF(m.bgCrop.x() * img.width(), m.bgCrop.y() * img.height(), m.bgCrop.width() * img.width(), m.bgCrop.height() * img.height())
                            .toRect()
                            .intersected(img.rect());
        if (r.width() > 4 && r.height() > 4) img = img.copy(r);
    }
    if (m.bgTrim) img = trimWhite(img);
    if (m.bgPlacement == MasterOptions::BgPlacement::FillPage) {
        // auf das Seitenverhältnis A4 (210 × 297) mittig zuschneiden, damit nichts verzerrt wird
        const double target = 210.0 / 297.0;
        const double ratio = double(img.width()) / qMax(1, img.height());
        if (ratio > target) {
            const int w = int(img.height() * target);
            img = img.copy((img.width() - w) / 2, 0, w, img.height());
        } else if (ratio < target) {
            const int h = int(img.width() / target);
            img = img.copy(0, (img.height() - h) / 2, img.width(), h);
        }
    }
    return img;
}

bool writeMaster(const QString &file, const MasterOptions &m, const QStringList &inputs, QString *error)
{
    const QFileInfo fi(file);
    QString halfTitle, titleImage;
    if (!m.halfTitleImage.isEmpty()) {
        const QImage src(m.halfTitleImage);
        if (src.isNull()) {
            if (error) *error = tr("Das Hintergrundbild „%1“ kann nicht gelesen werden.").arg(QDir::toNativeSeparators(m.halfTitleImage));
            return false;
        }
        const QImage img = prepareBackground(src, m);
        // Bildschirmfotos und Grafiken verlustfrei als PNG, Fotos als JPEG
        const QString suf = QFileInfo(m.halfTitleImage).suffix().toLower();
        const bool png = suf == QLatin1String("png") || suf == QLatin1String("bmp") || suf == QLatin1String("gif") || suf == QLatin1String("webp");
        halfTitle = fi.absolutePath() + (m.bgTarget == MasterOptions::BgTarget::TitlePage ? QStringLiteral("/titelseite-hintergrund")
                                                                                          : QStringLiteral("/schmutztitel-hintergrund"))
                    + (png ? QStringLiteral(".png") : QStringLiteral(".jpg"));
        if (!img.save(halfTitle, png ? "PNG" : "JPG", png ? -1 : 92)) {
            if (error) *error = tr("Das Hintergrundbild kann nicht gespeichert werden.");
            return false;
        }
        if (m.titleImageActive()) {
            // Titelseite: derselbe Ausschnitt, aber nicht auf das Seitenformat zugeschnitten
            MasterOptions ti = m;
            ti.bgPlacement = MasterOptions::BgPlacement::Free;
            titleImage = fi.absolutePath() + QStringLiteral("/titelbild") + (png ? QStringLiteral(".png") : QStringLiteral(".jpg"));
            if (!prepareBackground(src, ti).save(titleImage, png ? "PNG" : "JPG", png ? -1 : 92)) {
                if (error) *error = tr("Das Titelbild kann nicht gespeichert werden.");
                return false;
            }
        }
    }
    {
        // Vorwort: Vorgabetext des Nutzers; eine vorhandene vorwort.tex bleibt unverändert
        const QString v = fi.absolutePath() + QStringLiteral("/vorwort.tex");
        if (!QFileInfo::exists(v)) {
            QSaveFile vf(v);
            if (!vf.open(QIODevice::WriteOnly) || vf.write(prefaceFile(m.docClass).toUtf8()) < 0 || !vf.commit()) {
                if (error) *error = vf.errorString();
                return false;
            }
        }
    }
    if (m.glossary) {
        const QString g = fi.absolutePath() + QStringLiteral("/glossar.tex");
        if (!QFileInfo::exists(g)) {
            QSaveFile gf(g);
            if (!gf.open(QIODevice::WriteOnly) || gf.write(glossaryFile().toUtf8()) < 0 || !gf.commit()) {
                if (error) *error = gf.errorString();
                return false;
            }
        }
    }
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(masterDocument(m, inputs, halfTitle, titleImage).toUtf8());
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

namespace {

// \chapter{…}-Text lesbar machen (nur die häufigen Maskierungen)
QString unescapeTitle(QString t)
{
    t.replace(QStringLiteral("\\&"), QStringLiteral("&")).replace(QStringLiteral("\\%"), QStringLiteral("%"))
        .replace(QStringLiteral("\\_"), QStringLiteral("_")).replace(QStringLiteral("\\#"), QStringLiteral("#"))
        .replace(QStringLiteral("\\glqq{}"), QStringLiteral("„")).replace(QStringLiteral("\\grqq{}"), QStringLiteral("“"))
        .replace(QStringLiteral("~"), QStringLiteral(" "));
    return t.simplified();
}

const QRegularExpression &chapterLine()
{
    static const QRegularExpression re(QStringLiteral("^\\s*\\\\(?:chapter|addchap)\\*?(?:\\[[^\\]]*\\])?\\{(.*)\\}\\s*(?:%.*)?$"));
    return re;
}

const QRegularExpression &boundaryLine()
{
    static const QRegularExpression re(QStringLiteral("^\\s*\\\\(?:chapter|addchap|part|backmatter|appendix|printindex|printglossary|end\\{document\\})"));
    return re;
}

// Zeilen des Hauptteils: zwischen \mainmatter (falls vorhanden) und \backmatter/\end{document}
void mainRange(const QStringList &lines, int *from, int *to)
{
    *from = 0;
    *to = int(lines.size());
    for (int i = 0; i < lines.size(); ++i) {
        const QString l = lines[i].trimmed();
        if (l.startsWith(QLatin1Char('%'))) continue;
        if (l.startsWith(QLatin1String("\\mainmatter"))) *from = i + 1;
        if (l.startsWith(QLatin1String("\\backmatter")) || l.startsWith(QLatin1String("\\end{document}"))) {
            *to = i;
            break;
        }
    }
}

} // namespace

QStringList masterChapters(const QString &masterText)
{
    const QStringList lines = masterText.split(QLatin1Char('\n'));
    int from = 0, to = 0;
    mainRange(lines, &from, &to);
    QStringList out;
    for (int i = from; i < to; ++i) {
        const auto m = chapterLine().match(lines[i]);
        if (m.hasMatch()) {
            const QString t = unescapeTitle(m.captured(1));
            if (!t.isEmpty() && !out.contains(t)) out << t;
        }
    }
    return out;
}

QStringList defaultChapters()
{
    return { tr("Frühstück"), tr("Vorspeisen"), tr("Suppen"), tr("Salate"), tr("Hauptgerichte"), tr("Beilagen"), tr("Snacks"),
             tr("Süßspeisen"), tr("Nachtisch"), tr("Backen"), tr("Getränke"), tr("Grundrezepte") };
}

QString suggestChapter(const Recipe &r)
{
    QStringList tags;
    for (const QString &t : r.tags) tags << t.toLower();
    const QString title = r.title.toLower();
    auto has = [&](std::initializer_list<const char *> words) {
        for (const char *w : words) {
            const QString k = QString::fromUtf8(w);
            for (const QString &t : tags)
                if (t.contains(k)) return true;
        }
        return false;
    };
    auto inTitle = [&](std::initializer_list<const char *> words) {
        for (const char *w : words)
            if (title.contains(QString::fromUtf8(w))) return true;
        return false;
    };
    if (has({ "getränk", "cocktail", "bowle", "likör" })) return tr("Getränke");
    if (has({ "frühstück", "brunch", "müsli" })) return tr("Frühstück");
    if (has({ "suppe", "eintopf" }) || inTitle({ "suppe", "brühe", "eintopf" })) return tr("Suppen");
    if (has({ "salat" }) || inTitle({ "salat" })) return tr("Salate");
    if (has({ "vorspeise", "antipasti", "appetizer", "starter" })) return tr("Vorspeisen");
    if (has({ "dessert", "nachtisch", "nachspeise", "eis", "parfait" })) return tr("Nachtisch");
    if (has({ "süßspeise", "süßigkeit" })) return tr("Süßspeisen");
    if (has({ "backen", "kuchen", "torte", "gebäck", "brot", "plätzchen", "kekse" })) return tr("Backen");
    if (has({ "beilage" })) return tr("Beilagen");
    if (has({ "snack", "zwischenmahlzeit", "fingerfood", "dip" })) return tr("Snacks");
    if (has({ "grundrezept", "soße", "sauce", "marinade" })) return tr("Grundrezepte");
    return tr("Hauptgerichte");
}

double maxBackgroundWidth(const MasterOptions &m, double heightPerWidth)
{
    const double shift = m.bgShiftBinding ? m.bindingMm : 0;
    double w = 210;
    if (m.bgPlacement == MasterOptions::BgPlacement::Free) w = 210 - shift - m.bgLeftMm;
    else if (m.bgPlacement == MasterOptions::BgPlacement::Centered) w = 210 - shift;
    if (heightPerWidth > 0) w = qMin(w, (297 - m.bgBottomMm) / heightPerWidth);
    return qMax(10.0, std::floor(w * 10) / 10);
}

bool addToMaster(const QString &masterFile, const QString &inputPath, QString *error, bool *already, const QString &chapter, bool *chapterCreated)
{
    if (already) *already = false;
    if (chapterCreated) *chapterCreated = false;
    QFile f(masterFile);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    const QString text = QString::fromUtf8(f.readAll());
    f.close();
    const QString line = QStringLiteral("\\input{%1}").arg(inputPath);
    // schon eingetragen (\input oder \include, mit oder ohne .tex)?
    const QRegularExpression has(QStringLiteral("^[^%\\n]*\\\\(?:input|include|subimport\\{[^}]*\\})\\{%1(?:\\.tex)?\\}")
                                     .arg(QRegularExpression::escape(inputPath)),
                                 QRegularExpression::MultilineOption);
    if (text.contains(has)) {
        if (already) *already = true;
        return true;
    }
    QStringList lines = text.split(QLatin1Char('\n'));
    int from = 0, to = 0;
    mainRange(lines, &from, &to);
    // Stelle vor der Marke bzw. vor \backmatter/\end{document}
    int tail = -1;
    for (int i = from; i < lines.size(); ++i)
        if (lines[i].trimmed() == masterMarker()) {
            tail = i;
            break;
        }
    if (tail < 0) {
        for (int i = from; i < lines.size(); ++i) {
            const QString l = lines[i].trimmed();
            if (l.startsWith(QLatin1String("\\backmatter")) || l.startsWith(QLatin1String("\\appendix")) || l.startsWith(QLatin1String("\\printindex"))
                || l.startsWith(QLatin1String("\\end{document}"))) {
                tail = i;
                break;
            }
        }
    }
    if (tail < 0) {
        if (error) *error = tr("Im Master-Dokument fehlen \\end{document} und die Marke „%1“.").arg(masterMarker());
        return false;
    }
    int insertAt = tail;
    QStringList insert{ line };
    const QString wanted = chapter.trimmed();
    if (!wanted.isEmpty()) {
        int chap = -1;
        for (int i = from; i < to; ++i) {
            const auto m = chapterLine().match(lines[i]);
            if (m.hasMatch() && unescapeTitle(m.captured(1)).compare(wanted, Qt::CaseInsensitive) == 0) {
                chap = i;
                break;
            }
        }
        if (chap >= 0) {
            // Ende des Kapitels: nächste Kapitel-/Teil-Überschrift, die Marke oder das Ende des Hauptteils
            int end = tail;
            for (int i = chap + 1; i < tail; ++i)
                if (boundaryLine().match(lines[i]).hasMatch()) {
                    end = i;
                    break;
                }
            // hinter die letzte nicht leere Zeile des Kapitels
            insertAt = end;
            while (insertAt - 1 > chap && lines[insertAt - 1].trimmed().isEmpty()) --insertAt;
        } else {
            insert = QStringList{ QString(), QStringLiteral("\\chapter{%1}").arg(escape(wanted)), line };
            if (chapterCreated) *chapterCreated = true;
        }
    }
    for (int k = int(insert.size()) - 1; k >= 0; --k) lines.insert(insertAt, insert[k]);
    QSaveFile out(masterFile);
    if (!out.open(QIODevice::WriteOnly) || out.write(lines.join(QLatin1Char('\n')).toUtf8()) < 0 || !out.commit()) {
        if (error) *error = out.errorString();
        return false;
    }
    return true;
}

QStringList searchDirs()
{
    QStringList dirs;
    const QString custom = qEnvironmentVariable("VIVIA_TEX_BIN");
    if (!custom.isEmpty()) dirs << custom;
#ifdef Q_OS_WIN
    // TeX Live: C:\texlive\<Jahr>\bin\windows (ab 2023) bzw. win32/win64
    for (const QString &root : { QStringLiteral("C:/texlive"), QDir::homePath() + QStringLiteral("/texlive") }) {
        QDir d(root);
        QStringList years = d.entryList({ QStringLiteral("20*") }, QDir::Dirs, QDir::Name | QDir::Reversed);
        for (const QString &y : years)
            for (const char *arch : { "windows", "win64", "win32" }) dirs << d.filePath(y + QStringLiteral("/bin/") + QLatin1String(arch));
    }
    // MiKTeX (für alle Benutzer bzw. nur für den aktuellen Benutzer)
    const QString pf = qEnvironmentVariable("ProgramFiles", QStringLiteral("C:/Program Files"));
    const QString local = qEnvironmentVariable("LOCALAPPDATA");
    dirs << pf + QStringLiteral("/MiKTeX/miktex/bin/x64") << pf + QStringLiteral("/MiKTeX/miktex/bin");
    if (!local.isEmpty()) dirs << local + QStringLiteral("/Programs/MiKTeX/miktex/bin/x64") << local + QStringLiteral("/Programs/MiKTeX/miktex/bin");
#elif defined(Q_OS_MACOS)
    dirs << QStringLiteral("/Library/TeX/texbin") << QStringLiteral("/usr/local/texlive/2025/bin/universal-darwin");
#else
    QDir tl(QStringLiteral("/usr/local/texlive"));
    for (const QString &y : tl.entryList({ QStringLiteral("20*") }, QDir::Dirs, QDir::Name | QDir::Reversed))
        dirs << tl.filePath(y + QStringLiteral("/bin/x86_64-linux")) << tl.filePath(y + QStringLiteral("/bin/aarch64-linux"));
#endif
    return dirs;
}

Installation findInstallation(bool refresh)
{
    static bool done = false;
    static Installation cached;
    if (done && !refresh) return cached;
    done = true;
    cached = Installation();
    const QStringList extra = searchDirs();
    for (const char *prog : { "pdflatex", "lualatex", "xelatex" }) {
        QString found = QStandardPaths::findExecutable(QLatin1String(prog));
        if (found.isEmpty()) found = QStandardPaths::findExecutable(QLatin1String(prog), extra);
        if (found.isEmpty()) continue;
        cached.program = QDir::toNativeSeparators(found);
        const QString lower = QDir::fromNativeSeparators(found).toLower();
        static const QRegularExpression year(QStringLiteral("texlive/(20\\d\\d)"));
        const auto m = year.match(lower);
        if (lower.contains(QLatin1String("miktex"))) cached.name = QStringLiteral("MiKTeX");
        else if (m.hasMatch()) cached.name = QStringLiteral("TeX Live %1").arg(m.captured(1));
        else if (lower.startsWith(QLatin1String("/library/tex"))) cached.name = QStringLiteral("MacTeX");
        else cached.name = QStringLiteral("TeX Live");
        break;
    }
    return cached;
}

} // namespace LatexExport
