// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "menuexport.h"
#include "odtexport.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QLocale>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>

namespace MenuExport {

namespace {

QString tr(const char *s) { return QCoreApplication::translate("MenuExport", s); }
QString esc(const QString &s) { return s.toHtmlEscaped(); }

QString personsText(int n) { return n == 1 ? tr("für 1 Person") : tr("für %1 Personen").arg(n); }

Recipe prepared(const Menu::MenuData &menu, const Recipe &r, const Options &opt)
{
    Recipe p = opt.scaleToPersons ? Menu::scaled(r, menu.persons) : r;
    if (!opt.images) p.image.clear();
    return p;
}

QString colorHex(const QString &c)
{
    QString h = c.trimmed();
    if (h.startsWith(QLatin1Char('#'))) h = h.mid(1);
    if (!QRegularExpression(QStringLiteral("^[0-9A-Fa-f]{6}$")).match(h).hasMatch()) h = QStringLiteral("8A6D3B");
    return h.toUpper();
}

} // namespace

QString dateText(const QDate &d)
{
    if (!d.isValid()) return QString();
    return QLocale(QLocale::German, QLocale::Germany).toString(d, QStringLiteral("dddd, d. MMMM yyyy"));
}

QString defaultFileName(const Menu::MenuData &menu, const QString &suffix)
{
    QString n = menu.title.trimmed().isEmpty() ? tr("Menü") : menu.title.trimmed();
    n.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("-"));
    if (suffix == QLatin1String("tex")) {
        // LaTeX-Dateinamen ohne Leerzeichen und Umlaute (sicher für \input und makeindex)
        n.replace(QStringLiteral("ä"), QStringLiteral("ae")).replace(QStringLiteral("ö"), QStringLiteral("oe")).replace(QStringLiteral("ü"), QStringLiteral("ue"));
        n.replace(QStringLiteral("Ä"), QStringLiteral("Ae")).replace(QStringLiteral("Ö"), QStringLiteral("Oe")).replace(QStringLiteral("Ü"), QStringLiteral("Ue"));
        n.replace(QStringLiteral("ß"), QStringLiteral("ss"));
        n.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]+")), QStringLiteral("-"));
        n = n.toLower();
    }
    return n.simplified() + QLatin1Char('.') + suffix;
}

// ---------------------------------------------------------------- LaTeX

QString toLatex(const Menu::MenuData &menu, const QList<Recipe> &recipes, const Options &opt, const QStringList &imageFiles)
{
    using LatexExport::escape;
    const bool scr = opt.docClass == LatexExport::DocClass::ScrBook;
    QString t;
    t += QStringLiteral("%% Erzeugt von ViviaMeals – Menü „%1“. Übersetzen: latexmk -pdf <datei>.tex (zweimal pdflatex genügt auch)\n").arg(menu.title);
    t += scr ? QStringLiteral("\\documentclass[paper=a4,fontsize=11pt,parskip=half,headings=normal,oneside,DIV=12]{scrbook}\n")
             : QStringLiteral("\\documentclass[a4paper,11pt,oneside]{book}\n\\usepackage[a4paper,margin=25mm]{geometry}\n"
                              "\\setlength{\\parindent}{0pt}\n\\setlength{\\parskip}{0.5\\baselineskip}\n");
    t += QStringLiteral("\\usepackage{iftex}\n\\ifPDFTeX\n  \\usepackage[utf8]{inputenc}\n  \\usepackage[T1]{fontenc}\n  \\usepackage{lmodern}\n"
                        "\\else\n  \\usepackage{fontspec}\n\\fi\n\\usepackage{textcomp}\n\\usepackage[ngerman]{babel}\n"
                        "\\usepackage{graphicx}\n\\usepackage{booktabs}\n\\usepackage{tabularx}\n\\usepackage[table]{xcolor}\n"
                        "\\usepackage{tikz}\n\\usepackage{eso-pic}\n\\usepackage{pifont}\n"
                        "%% verzierte Rahmenecken, wenn das Paket pgfornament installiert ist (sonst schlichte Ornamente)\n"
                        "\\newif\\ifvmornament\n\\IfFileExists{pgfornament.sty}{\\usepackage{pgfornament}\\vmornamenttrue}{}\n"
                        "\\usepackage[hidelinks]{hyperref}\n");
    t += QStringLiteral("\\definecolor{vmblau}{HTML}{1474BC}\n\\definecolor{vmorange}{HTML}{E0620D}\n\\definecolor{vmrahmen}{HTML}{%1}\n").arg(colorHex(opt.frameColor));
    if (scr) t += QStringLiteral("\\addtokomafont{disposition}{\\color{vmblau}}\n");
    t += QStringLiteral("\\setcounter{secnumdepth}{-1}\n\\pagestyle{plain}\n");
    t += QStringLiteral("\\hypersetup{pdftitle={%1}}\n").arg(escape(menu.title));
    // Schmuckrahmen (A4, Maße in mm von der linken unteren Papierecke)
    t += QStringLiteral(
        "\n%% ---- Schmuckrahmen des Deckblatts\n"
        "\\newcommand{\\vmRahmen}{\\AtPageLowerLeft{\\begin{tikzpicture}[x=1mm,y=1mm,line cap=round]\n"
        "  \\useasboundingbox (0,0) rectangle (210,297);\n"
        "  \\draw[vmrahmen,line width=1.8pt] (13,13) rectangle (197,284);\n"
        "  \\draw[vmrahmen,line width=0.5pt] (17,17) rectangle (193,280);\n"
        "  \\ifvmornament\n"
        "    \\node[anchor=north west,inner sep=0pt] at (19,278) {\\pgfornament[width=30mm,color=vmrahmen]{61}};\n"
        "    \\node[anchor=north east,inner sep=0pt] at (191,278) {\\pgfornament[width=30mm,color=vmrahmen,symmetry=v]{61}};\n"
        "    \\node[anchor=south west,inner sep=0pt] at (19,19) {\\pgfornament[width=30mm,color=vmrahmen,symmetry=h]{61}};\n"
        "    \\node[anchor=south east,inner sep=0pt] at (191,19) {\\pgfornament[width=30mm,color=vmrahmen,symmetry=c]{61}};\n"
        "  \\else\n"
        "    \\foreach \\x/\\y/\\dx/\\dy in {17/17/1/1,193/17/-1/1,17/280/1/-1,193/280/-1/-1} {\n"
        "      \\fill[vmrahmen] (\\x,\\y) circle (2.4);\n"
        "      \\draw[vmrahmen,line width=0.6pt] (\\x+\\dx*6,\\y) arc[start angle={90-90*\\dx}, end angle={90-90*\\dx+90*\\dx*\\dy}, radius=6];\n"
        "      \\draw[vmrahmen,line width=0.6pt] (\\x+\\dx*4,\\y+\\dy*10) -- (\\x+\\dx*4,\\y+\\dy*22);\n"
        "      \\draw[vmrahmen,line width=0.6pt] (\\x+\\dx*10,\\y+\\dy*4) -- (\\x+\\dx*22,\\y+\\dy*4);\n"
        "    }\n"
        "  \\fi\n"
        "  %% Rauten in der Mitte jeder Seite\n"
        "  \\foreach \\x/\\y in {105/282,105/15,15/148.5,195/148.5}\n"
        "    \\fill[vmrahmen] (\\x,\\y+2.6) -- (\\x+2.6,\\y) -- (\\x,\\y-2.6) -- (\\x-2.6,\\y) -- cycle;\n"
        "\\end{tikzpicture}}}\n"
        "\\newcommand{\\vmTrenner}{\\par\\vspace{0.9em}{\\color{vmrahmen}\\large\\ding{167}}\\par\\vspace{0.9em}}\n\n");
    // Deckblatt: Inhalt in eine Box setzen, auf der Seite mittig in den Rahmen stellen und bei Bedarf verkleinern
    const int n = int(menu.items.size());
    const QString gap = n >= 5 ? QStringLiteral("0.55em") : n == 4 ? QStringLiteral("0.75em") : QStringLiteral("1em");
    t += QStringLiteral("\\renewcommand{\\vmTrenner}{\\par\\vspace{%1}{\\color{vmrahmen}\\large\\ding{167}}\\par\\vspace{%1}}\n").arg(gap);
    t += QStringLiteral("\\newsavebox{\\vmDeckblatt}\n\\newlength{\\vmDeckblattHoehe}\n");
    t += QStringLiteral("\\begin{document}\n\n%% ---- Menüdeckblatt (Inhalt)\n\\sbox{\\vmDeckblatt}{\\begin{minipage}{128mm}\\centering\n");
    t += QStringLiteral("{\\color{vmrahmen}\\Large\\ding{166}}\\par\\vspace{1em}\n");
    t += QStringLiteral("{\\fontsize{30}{36}\\selectfont\\itshape\\color{vmrahmen} %1\\par}\n").arg(escape(menu.title.isEmpty() ? tr("Menü") : menu.title));
    if (!menu.occasion.trimmed().isEmpty()) t += QStringLiteral("\\vspace{0.6em}{\\Large %1\\par}\n").arg(escape(menu.occasion.trimmed()));
    QStringList info;
    if (menu.date.isValid()) info << escape(dateText(menu.date));
    if (menu.persons > 0) info << escape(personsText(menu.persons));
    if (!info.isEmpty()) t += QStringLiteral("\\vspace{0.5em}{\\normalsize\\color{darkgray}%1\\par}\n").arg(info.join(QStringLiteral(" \\textperiodcentered{} ")));
    t += QStringLiteral("\\vspace{2em}\n");
    for (int i = 0; i < n; ++i) {
        const Menu::Item &it = menu.items[i];
        if (i > 0) t += QStringLiteral("\\vmTrenner\n");
        t += QStringLiteral("{\\small\\scshape\\color{vmrahmen}%1}\\par\\vspace{0.25em}\n").arg(escape(Menu::courseName(it.course)));
        t += QStringLiteral("{\\Large\\bfseries %1}\\par\n").arg(escape(it.title.isEmpty() ? tr("(noch offen)") : it.title));
        if (!it.description.trimmed().isEmpty())
            t += QStringLiteral("\\vspace{0.2em}{\\itshape\\color{darkgray}%1}\\par\n").arg(escape(it.description.trimmed()));
    }
    t += QStringLiteral("\\vspace{1.6em}{\\color{vmrahmen}\\Large\\ding{166}}\\par\n\\end{minipage}}\n");
    t += QStringLiteral(
        "\\setlength{\\vmDeckblattHoehe}{\\dimexpr\\ht\\vmDeckblatt+\\dp\\vmDeckblatt\\relax}\n"
        "%% höchstens 228 mm hoch (innerhalb des Rahmens), sonst verkleinert\n"
        "\\begin{titlepage}\n\\AddToShipoutPictureBG*{\\vmRahmen}\n"
        "\\AddToShipoutPictureFG*{\\AtPageCenter{\\makebox[0pt]{%%\n"
        "  \\ifdim\\vmDeckblattHoehe>228mm \\resizebox{!}{228mm}{\\usebox{\\vmDeckblatt}}\\else\\usebox{\\vmDeckblatt}\\fi}}}\n"
        "\\mbox{}\n\\end{titlepage}\n");
    // Rezepte
    if (opt.recipes) {
        for (int i = 0; i < menu.items.size() && i < recipes.size(); ++i) {
            if (recipes[i].id.isEmpty()) continue;
            const Recipe r = prepared(menu, recipes[i], opt);
            LatexExport::Options lo;
            lo.docClass = opt.docClass;
            lo.mode = LatexExport::Mode::Chapter;
            lo.heading = LatexExport::Heading::Section;
            lo.newPage = false;
            lo.image = opt.images;
            lo.nutrition = opt.nutrition;
            lo.indexEntries = false;
            t += QStringLiteral("\n%% ---- %1\n\\clearpage\n{\\small\\scshape\\color{vmrahmen}%1}\\par\\vspace{-0.6\\baselineskip}\n")
                     .arg(escape(Menu::courseName(menu.items[i].course)));
            QString body = LatexExport::toLatex(r, lo, imageFiles.value(i));
            // Kopfkommentar der Einzelrezepte entfällt im Menü
            QStringList lines = body.split(QLatin1Char('\n'));
            while (!lines.isEmpty() && (lines.first().startsWith(QLatin1Char('%')) || lines.first().trimmed().isEmpty())) lines.removeFirst();
            t += lines.join(QLatin1Char('\n'));
        }
    }
    t += QStringLiteral("\n\\end{document}\n");
    return t;
}

bool writeLatex(const QString &file, const Menu::MenuData &menu, const QList<Recipe> &recipes, const Options &opt, QString *error)
{
    QStringList images;
    const QFileInfo fi(file);
    for (int i = 0; i < recipes.size(); ++i) {
        QString img;
        if (opt.recipes && opt.images && !recipes[i].image.isEmpty()) {
            QImage q;
            q.loadFromData(QByteArray::fromBase64(recipes[i].image.toLatin1()));
            if (!q.isNull()) {
                img = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral("-gang%1.jpg").arg(i + 1);
                if (q.hasAlphaChannel()) {
                    QImage flat(q.size(), QImage::Format_RGB32);
                    flat.fill(Qt::white);
                    QPainter(&flat).drawImage(0, 0, q);
                    q = flat;
                }
                if (!q.save(img, "JPG", 90)) img.clear();
            }
        }
        images << img;
    }
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(toLatex(menu, recipes, opt, images).toUtf8());
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- LibreOffice Writer

QByteArray toOdt(const Menu::MenuData &menu, const QList<Recipe> &recipes, const Options &opt)
{
    const QString c = QLatin1Char('#') + colorHex(opt.frameColor).toLower();
    const QString extraStyles = QStringLiteral(
        "<style:style style:name=\"VM_Menutitel\" style:display-name=\"Menütitel\" style:family=\"paragraph\" style:parent-style-name=\"Standard\">"
        "<style:paragraph-properties fo:text-align=\"center\" fo:margin-bottom=\"0.3cm\"/>"
        "<style:text-properties style:font-name=\"Liberation Serif\" fo:font-size=\"30pt\" fo:font-style=\"italic\" fo:color=\"%1\"/></style:style>"
        "<style:style style:name=\"VM_Anlass\" style:display-name=\"Menü-Anlass\" style:family=\"paragraph\" style:parent-style-name=\"Standard\">"
        "<style:paragraph-properties fo:text-align=\"center\" fo:margin-bottom=\"0.15cm\"/>"
        "<style:text-properties style:font-name=\"Liberation Serif\" fo:font-size=\"15pt\"/></style:style>"
        "<style:style style:name=\"VM_Menuinfo\" style:display-name=\"Menü-Info\" style:family=\"paragraph\" style:parent-style-name=\"Standard\">"
        "<style:paragraph-properties fo:text-align=\"center\" fo:margin-bottom=\"0.9cm\"/>"
        "<style:text-properties style:font-name=\"Liberation Serif\" fo:font-size=\"11pt\" fo:color=\"#555555\"/></style:style>"
        "<style:style style:name=\"VM_Gang\" style:display-name=\"Gang\" style:family=\"paragraph\" style:parent-style-name=\"Standard\">"
        "<style:paragraph-properties fo:text-align=\"center\" fo:margin-bottom=\"0.08cm\" fo:keep-with-next=\"always\"/>"
        "<style:text-properties style:font-name=\"Liberation Serif\" fo:font-size=\"10.5pt\" fo:font-variant=\"small-caps\" fo:letter-spacing=\"0.05cm\" fo:color=\"%1\"/></style:style>"
        "<style:style style:name=\"VM_Gangtitel\" style:display-name=\"Gang-Titel\" style:family=\"paragraph\" style:parent-style-name=\"Standard\">"
        "<style:paragraph-properties fo:text-align=\"center\" fo:margin-bottom=\"0.06cm\" fo:keep-with-next=\"always\"/>"
        "<style:text-properties style:font-name=\"Liberation Serif\" fo:font-size=\"16pt\" fo:font-weight=\"bold\"/></style:style>"
        "<style:style style:name=\"VM_Gangtext\" style:display-name=\"Gang-Beschreibung\" style:family=\"paragraph\" style:parent-style-name=\"Standard\">"
        "<style:paragraph-properties fo:text-align=\"center\" fo:margin-left=\"1.5cm\" fo:margin-right=\"1.5cm\" fo:margin-bottom=\"0cm\"/>"
        "<style:text-properties style:font-name=\"Liberation Serif\" fo:font-size=\"11.5pt\" fo:font-style=\"italic\" fo:color=\"#555555\"/></style:style>"
        "<style:style style:name=\"VM_Ornament\" style:display-name=\"Ornament\" style:family=\"paragraph\" style:parent-style-name=\"Standard\">"
        "<style:paragraph-properties fo:text-align=\"center\" fo:margin-top=\"0.3cm\" fo:margin-bottom=\"0.3cm\"/>"
        "<style:text-properties style:font-name=\"Liberation Serif\" fo:font-size=\"16pt\" fo:color=\"%1\"/></style:style>"
        "<style:style style:name=\"VM_Rahmenzier\" style:display-name=\"Rahmenzier\" style:family=\"paragraph\" style:parent-style-name=\"Standard\">"
        "<style:paragraph-properties fo:margin-bottom=\"0cm\"><style:tab-stops><style:tab-stop style:position=\"8.7cm\" style:type=\"center\"/>"
        "<style:tab-stop style:position=\"17.4cm\" style:type=\"right\"/></style:tab-stops></style:paragraph-properties>"
        "<style:text-properties style:font-name=\"Liberation Serif\" fo:font-size=\"20pt\" fo:color=\"%1\"/></style:style>"
        "<style:style style:name=\"VM_Gangkopf\" style:display-name=\"Gang über dem Rezept\" style:family=\"paragraph\" style:parent-style-name=\"Standard\">"
        "<style:paragraph-properties fo:margin-bottom=\"0cm\" fo:keep-with-next=\"always\"/>"
        "<style:text-properties style:font-name=\"Liberation Sans\" fo:font-size=\"10pt\" fo:font-variant=\"small-caps\" fo:letter-spacing=\"0.04cm\" fo:color=\"%1\"/></style:style>")
                                    .arg(c);
    const QString layouts = QStringLiteral(
        "<style:page-layout style:name=\"pmDeckblatt\">"
        "<style:page-layout-properties fo:page-width=\"21cm\" fo:page-height=\"29.7cm\" style:print-orientation=\"portrait\""
        " fo:margin-top=\"1.3cm\" fo:margin-bottom=\"1.3cm\" fo:margin-left=\"1.3cm\" fo:margin-right=\"1.3cm\""
        " fo:border=\"3pt double %1\" style:border-line-width=\"0.7pt 0.6pt 1.6pt\" fo:padding=\"0.5cm\"/>"
        "<style:header-style><style:header-footer-properties fo:min-height=\"0.9cm\" fo:margin-bottom=\"0.4cm\"/></style:header-style>"
        "<style:footer-style><style:header-footer-properties fo:min-height=\"0.9cm\" fo:margin-top=\"0.4cm\"/></style:footer-style>"
        "</style:page-layout>")
                                .arg(c);
    const QString masters = QStringLiteral("<style:master-page style:name=\"Deckblatt\" style:display-name=\"Menüdeckblatt\" style:page-layout-name=\"pmDeckblatt\""
                                           " style:next-style-name=\"Standard\">"
                                           "<style:header><text:p text:style-name=\"VM_Rahmenzier\">❦<text:tab/>✥<text:tab/>❦</text:p></style:header>"
                                           "<style:footer><text:p text:style-name=\"VM_Rahmenzier\">❧<text:tab/>✥<text:tab/>☙</text:p></style:footer>"
                                           "</style:master-page>");
    // automatische Absatzvorlagen: erste Zeile des Deckblatts (Seitenvorlage), erste/weitere Rezeptseiten (Umbruch)
    const int n = int(menu.items.size());
    const double top = n >= 5 ? 0.4 : n == 4 ? 1.6 : 3.2;
    const QString autoStyles = QStringLiteral(
        "<style:style style:name=\"P_Deckblatt\" style:family=\"paragraph\" style:parent-style-name=\"VM_Ornament\" style:master-page-name=\"Deckblatt\">"
        "<style:paragraph-properties style:page-number=\"auto\" fo:margin-top=\"%1cm\"/></style:style>"
        "<style:style style:name=\"P_Rezept1\" style:family=\"paragraph\" style:parent-style-name=\"VM_Gangkopf\" style:master-page-name=\"Standard\">"
        "<style:paragraph-properties style:page-number=\"auto\"/></style:style>"
        "<style:style style:name=\"P_Rezept\" style:family=\"paragraph\" style:parent-style-name=\"VM_Gangkopf\">"
        "<style:paragraph-properties fo:break-before=\"page\"/></style:style>")
                                   .arg(QString::number(top, 'f', 1));

    QString b;
    b += QStringLiteral("<text:p text:style-name=\"P_Deckblatt\">❦</text:p>");
    b += QStringLiteral("<text:p text:style-name=\"VM_Menutitel\">%1</text:p>").arg(esc(menu.title.isEmpty() ? tr("Menü") : menu.title));
    if (!menu.occasion.trimmed().isEmpty()) b += QStringLiteral("<text:p text:style-name=\"VM_Anlass\">%1</text:p>").arg(esc(menu.occasion.trimmed()));
    QStringList info;
    if (menu.date.isValid()) info << dateText(menu.date);
    if (menu.persons > 0) info << personsText(menu.persons);
    b += QStringLiteral("<text:p text:style-name=\"VM_Menuinfo\">%1</text:p>").arg(esc(info.join(QStringLiteral(" · "))));
    for (int i = 0; i < n; ++i) {
        const Menu::Item &it = menu.items[i];
        if (i > 0) b += QStringLiteral("<text:p text:style-name=\"VM_Ornament\">❧</text:p>");
        b += QStringLiteral("<text:p text:style-name=\"VM_Gang\">%1</text:p>").arg(esc(Menu::courseName(it.course)));
        b += QStringLiteral("<text:p text:style-name=\"VM_Gangtitel\">%1</text:p>").arg(esc(it.title.isEmpty() ? tr("(noch offen)") : it.title));
        if (!it.description.trimmed().isEmpty()) b += QStringLiteral("<text:p text:style-name=\"VM_Gangtext\">%1</text:p>").arg(esc(it.description.trimmed()));
    }
    b += QStringLiteral("<text:p text:style-name=\"VM_Ornament\">✥ ❦ ✥</text:p>");

    QList<OdtExport::Part> parts;
    if (opt.recipes) {
        bool first = true;
        for (int i = 0; i < n && i < recipes.size(); ++i) {
            if (recipes[i].id.isEmpty()) continue;
            OdtExport::Options oo;
            oo.nutrition = opt.nutrition;
            const OdtExport::Part p =
                OdtExport::recipeBody(prepared(menu, recipes[i], opt), oo, i + 1, first ? QStringLiteral("P_Rezept1") : QStringLiteral("P_Rezept"),
                                      Menu::courseName(menu.items[i].course));
            first = false;
            b += p.xml;
            parts << p;
        }
    }
    const QString title = menu.title.isEmpty() ? tr("Menü") : menu.title;
    return OdtExport::package(title, b, parts, autoStyles, OdtExport::styles(title, extraStyles, layouts, masters));
}

bool writeOdt(const QString &file, const Menu::MenuData &menu, const QList<Recipe> &recipes, const Options &opt, QString *error)
{
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(toOdt(menu, recipes, opt));
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

} // namespace MenuExport
