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
        t += QStringLiteral("%% Erzeugt von ViviaMeals – Kapitel zum Einbinden mit \\input{} in ein Master-Dokument der Klasse %1.\n"
                            "%% Benötigt: graphicx, booktabs, tabularx, xcolor (Farben vmblau, vmorange), textcomp.\n"
                            "%% \\definecolor{vmblau}{HTML}{1474BC} \\definecolor{vmorange}{HTML}{E0620D}\n\n")
                 .arg(scr ? QStringLiteral("scrbook") : QStringLiteral("book"));
    }
    t += QStringLiteral("\\chapter{%1}\n\\label{rez:%2}\n").arg(escape(r.title), r.id.left(8));
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
    t += QStringLiteral("\n\n\\section*{%1}\\markright{%1}\n").arg(tr("Zutaten"));
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
    t += QStringLiteral("\\end{tabularx}\n\n\\section*{%1}\\markright{%1}\n").arg(tr("Zubereitung"));
    for (const QString &p : r.instructions.split(QStringLiteral("\n\n"), Qt::SkipEmptyParts))
        t += escape(p.trimmed()) + QStringLiteral("\n\n");
    if (opt.nutrition && r.nutrition.valid() && r.nutrition.matched > 0) {
        const Nutrients &p = r.nutrition.perServing, &a = r.nutrition.total;
        const Nutrients ri = Nutrition::referenceIntake();
        auto row = [&](const QString &label, double pv, double tv, double ref, const QString &unit, int dec) {
            return QStringLiteral("%1 & %2~%5 & %3~%5 & %4\\\\\n")
                .arg(label, Fmt::number(pv, dec), Fmt::number(tv, dec), ref > 0 ? Fmt::number(pv / ref * 100, 0) + QStringLiteral("\\,\\%") : QString(), unit);
        };
        t += QStringLiteral("\\section*{%1}\\markright{%1}\n{\\small\n\\begin{tabular}{@{}lrrr@{}}\n\\toprule\n").arg(tr("Nährwerte"));
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

QString masterDocument(const MasterOptions &m, const QStringList &inputs, const QString &halfTitleFile)
{
    const bool scr = m.docClass == DocClass::ScrBook;
    const QString bcor = QString::number(m.bindingMm, 'f', m.bindingMm == std::floor(m.bindingMm) ? 0 : 1);
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
                            "  headinclude=true,footinclude=false,titlepage=true,open=right]{scrbook}\n").arg(bcor);
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
        "\\usepackage{eso-pic}     %% Hintergrundbild (Schmutztitel)\n"
        "\\graphicspath{{./}{bilder/}}\n"
        "%% Bild mit Unterschrift genau an dieser Stelle:  \\rezeptbild[0.8]{datei}{Unterschrift}\n"
        "\\newcommand{\\rezeptbild}[3][0.8]{\\begin{figure}[H]\\centering\\includegraphics[width=#1\\linewidth]{#2}\\caption{#3}\\end{figure}}\n"
        "%% Bild rechts, vom Text umflossen:                \\randbild{datei}{Unterschrift}\n"
        "\\newcommand{\\randbild}[2]{\\begin{wrapfigure}{r}{0.4\\linewidth}\\centering\\includegraphics[width=\\linewidth]{#1}\\caption{#2}\\end{wrapfigure}}\n");
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
            "\\pagestyle{fancy}\n"
            "\\fancyhf{}\n"
            "\\renewcommand{\\chaptermark}[1]{\\markboth{\\thechapter\\ #1}{}}\n"
            "\\fancyhead[LE]{\\small\\leftmark}   %% gerade Seiten: Kapitel\n"
            "\\fancyhead[RO]{\\small\\rightmark}  %% ungerade Seiten: Abschnitt\n"
            "\\fancyfoot[C]{\\thepage}\n"
            "\\renewcommand{\\headrulewidth}{0.4pt}\n"
            "\\renewcommand{\\footrulewidth}{0.4pt}\n"
            "\\fancypagestyle{plain}{\\fancyhf{}\\fancyfoot[C]{\\thepage}\\renewcommand{\\headrulewidth}{0pt}\\renewcommand{\\footrulewidth}{0.4pt}}\n");
    }
    // Titelangaben
    t += QStringLiteral("\n%% ---- Titelangaben\n\\title{%1}\n").arg(escape(m.title));
    if (!m.subtitle.trimmed().isEmpty()) {
        if (scr) t += QStringLiteral("\\subtitle{%1}\n").arg(escape(m.subtitle));
        else t += QStringLiteral("%% Untertitel: siehe Titelseite unten\n");
    }
    t += QStringLiteral("\\author{%1}\n").arg(escape(m.author));
    t += m.noDate ? QStringLiteral("\\date{}   %% kein Datum auf dem Titelblatt\n") : QStringLiteral("\\date{\\today}\n");
    t += QStringLiteral("\n\\begin{document}\n\\frontmatter\n");
    // Schmutztitel mit Hintergrundbild
    t += QStringLiteral("\n%% ---- Schmutztitel (mit Hintergrundbild)\n\\begin{titlepage}\n\\thispagestyle{empty}\n");
    if (!halfTitleFile.isEmpty())
        t += QStringLiteral("\\AddToShipoutPictureBG*{\\includegraphics[width=\\paperwidth,height=\\paperheight]{%1}}\n")
                 .arg(QFileInfo(halfTitleFile).completeBaseName());
    t += QStringLiteral("\\vspace*{0.30\\textheight}\n\\begin{center}\n"
                        "\\colorbox{white}{\\parbox{0.8\\textwidth}{\\centering\\vspace{1em}{\\Huge\\bfseries\\color{vmblau}%1\\par}\\vspace{1em}}}\n"
                        "\\end{center}\n\\end{titlepage}\n")
             .arg(escape(m.title));
    // Titelseite
    t += QStringLiteral("\n%% ---- Titelseite\n");
    if (scr || m.subtitle.trimmed().isEmpty()) {
        t += QStringLiteral("\\maketitle\n");
    } else {
        t += QStringLiteral("\\begin{titlepage}\n\\centering\n\\vspace*{0.25\\textheight}\n{\\Huge\\bfseries %1\\par}\n\\vspace{1em}\n{\\Large %2\\par}\n"
                            "\\vspace{3em}\n{\\large %3\\par}\n%4\\end{titlepage}\n")
                 .arg(escape(m.title), escape(m.subtitle), escape(m.author), m.noDate ? QString() : QStringLiteral("\\vspace{1em}\n{\\large\\today\\par}\n"));
    }
    if (m.toc) {
        t += QStringLiteral("\n%% ---- Inhaltsverzeichnis (ohne Kopfzeile)\n");
        t += scr ? QStringLiteral("\\cleardoubleoddpage\n\\pagestyle{plain.scrheadings}\n\\tableofcontents\n\\cleardoubleoddpage\n\\pagestyle{scrheadings}\n")
                 : QStringLiteral("\\cleardoublepage\n\\pagestyle{plain}\n\\tableofcontents\n\\cleardoublepage\n\\pagestyle{fancy}\n");
    }
    t += QStringLiteral("\n\\mainmatter\n\n%% ---- Rezepte\n");
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

bool writeMaster(const QString &file, const MasterOptions &m, const QStringList &inputs, QString *error)
{
    const QFileInfo fi(file);
    QString halfTitle;
    if (!m.halfTitleImage.isEmpty()) {
        QImage img(m.halfTitleImage);
        if (img.isNull()) {
            if (error) *error = tr("Das Hintergrundbild „%1“ kann nicht gelesen werden.").arg(QDir::toNativeSeparators(m.halfTitleImage));
            return false;
        }
        halfTitle = fi.absolutePath() + QStringLiteral("/schmutztitel-hintergrund.jpg");
        if (img.hasAlphaChannel()) {
            QImage flat(img.size(), QImage::Format_RGB32);
            flat.fill(Qt::white);
            QPainter p(&flat);
            p.drawImage(0, 0, img);
            p.end();
            img = flat;
        }
        if (!img.save(halfTitle, "JPG", 92)) {
            if (error) *error = tr("Das Hintergrundbild kann nicht gespeichert werden.");
            return false;
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
    f.write(masterDocument(m, inputs, halfTitle).toUtf8());
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

bool addToMaster(const QString &masterFile, const QString &inputPath, QString *error, bool *already)
{
    if (already) *already = false;
    QFile f(masterFile);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    QString text = QString::fromUtf8(f.readAll());
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
    int pos = text.indexOf(masterMarker());
    if (pos < 0) {
        static const QRegularExpression back(QStringLiteral("^\\s*\\\\(?:backmatter|appendix|printindex|end\\{document\\})"), QRegularExpression::MultilineOption);
        const auto m = back.match(text);
        if (!m.hasMatch()) {
            if (error) *error = tr("Im Master-Dokument fehlen \\end{document} und die Marke „%1“.").arg(masterMarker());
            return false;
        }
        pos = int(m.capturedStart());
        while (pos < text.size() && (text.at(pos) == QLatin1Char('\n') || text.at(pos) == QLatin1Char('\r'))) ++pos;
    } else {
        // an den Zeilenanfang der Marke
        while (pos > 0 && text.at(pos - 1) != QLatin1Char('\n')) --pos;
    }
    text.insert(pos, line + QLatin1Char('\n'));
    QSaveFile out(masterFile);
    if (!out.open(QIODevice::WriteOnly) || out.write(text.toUtf8()) < 0 || !out.commit()) {
        if (error) *error = out.errorString();
        return false;
    }
    return true;
}

} // namespace LatexExport
