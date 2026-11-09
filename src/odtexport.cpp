// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "odtexport.h"
#include "nutrition.h"
#include "mealtypes.h"
#include "recipehtml.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QImage>
#include <QRegularExpression>
#include <QSaveFile>

namespace OdtExport {

namespace {

QString tr(const char *s) { return QCoreApplication::translate("OdtExport", s); }

QString esc(const QString &s) { return s.toHtmlEscaped(); }

quint32 crc32(const QByteArray &data)
{
    static quint32 table[256];
    static bool init = false;
    if (!init) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    quint32 crc = 0xFFFFFFFFu;
    for (char ch : data) crc = table[(crc ^ quint8(ch)) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

void put16(QByteArray &b, quint16 v)
{
    b.append(char(v & 0xFF));
    b.append(char(v >> 8));
}

void put32(QByteArray &b, quint32 v)
{
    for (int i = 0; i < 4; ++i) b.append(char((v >> (8 * i)) & 0xFF));
}

const char *kNamespaces =
    " xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\""
    " xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\""
    " xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\""
    " xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\""
    " xmlns:draw=\"urn:oasis:names:tc:opendocument:xmlns:drawing:1.0\""
    " xmlns:fo=\"urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0\""
    " xmlns:xlink=\"http://www.w3.org/1999/xlink\""
    " xmlns:dc=\"http://purl.org/dc/elements/1.1/\""
    " xmlns:meta=\"urn:oasis:names:tc:opendocument:xmlns:meta:1.0\""
    " xmlns:svg=\"urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0\""
    " office:version=\"1.2\"";

const char *kFonts =
    "<office:font-face-decls>"
    "<style:font-face style:name=\"Liberation Sans\" svg:font-family=\"'Liberation Sans'\" style:font-family-generic=\"swiss\" style:font-pitch=\"variable\"/>"
    "<style:font-face style:name=\"Liberation Serif\" svg:font-family=\"'Liberation Serif'\" style:font-family-generic=\"roman\" style:font-pitch=\"variable\"/>"
    "</office:font-face-decls>";

QString stylesXml(const QString &footerLeft)
{
    const QString accent = QStringLiteral("#0f67ab");
    QString s = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<office:document-styles") + QString::fromLatin1(kNamespaces) + QStringLiteral(">");
    s += QString::fromLatin1(kFonts);
    s += QStringLiteral(
             "<office:styles>"
             "<style:default-style style:family=\"paragraph\">"
             "<style:paragraph-properties fo:hyphenation-ladder-count=\"no-limit\" style:writing-mode=\"page\"/>"
             "<style:text-properties style:font-name=\"Liberation Serif\" fo:font-size=\"11pt\" fo:language=\"de\" fo:country=\"DE\" fo:hyphenate=\"true\""
             " fo:hyphenation-remain-char-count=\"2\" fo:hyphenation-push-char-count=\"2\"/>"
             "</style:default-style>"
             "<style:default-style style:family=\"table\"><style:table-properties table:border-model=\"collapsing\"/></style:default-style>"
             "<style:style style:name=\"Standard\" style:family=\"paragraph\" style:class=\"text\">"
             "<style:paragraph-properties fo:margin-top=\"0cm\" fo:margin-bottom=\"0.18cm\" fo:line-height=\"115%\"/></style:style>"
             "<style:style style:name=\"Text_20_body\" style:display-name=\"Text body\" style:family=\"paragraph\" style:parent-style-name=\"Standard\" style:class=\"text\">"
             "<style:paragraph-properties fo:margin-bottom=\"0.25cm\" fo:text-align=\"justify\"/></style:style>"
             "<style:style style:name=\"Heading\" style:family=\"paragraph\" style:parent-style-name=\"Standard\" style:next-style-name=\"Text_20_body\" style:class=\"text\">"
             "<style:paragraph-properties fo:margin-top=\"0.42cm\" fo:margin-bottom=\"0.21cm\" fo:keep-with-next=\"always\"/>"
             "<style:text-properties style:font-name=\"Liberation Sans\" fo:font-weight=\"bold\" fo:color=\"%1\"/></style:style>"
             "<style:style style:name=\"Heading_20_1\" style:display-name=\"Heading 1\" style:family=\"paragraph\" style:parent-style-name=\"Heading\" style:default-outline-level=\"1\" style:class=\"text\">"
             "<style:paragraph-properties fo:margin-top=\"0cm\" fo:margin-bottom=\"0.25cm\"/><style:text-properties fo:font-size=\"20pt\"/></style:style>"
             "<style:style style:name=\"Heading_20_2\" style:display-name=\"Heading 2\" style:family=\"paragraph\" style:parent-style-name=\"Heading\" style:default-outline-level=\"2\" style:class=\"text\">"
             "<style:text-properties fo:font-size=\"14pt\"/></style:style>"
             "<style:style style:name=\"VM_Info\" style:display-name=\"Rezept-Info\" style:family=\"paragraph\" style:parent-style-name=\"Standard\">"
             "<style:text-properties style:font-name=\"Liberation Sans\" fo:font-size=\"10.5pt\"/></style:style>"
             "<style:style style:name=\"VM_Energie\" style:display-name=\"Rezept-Energie\" style:family=\"paragraph\" style:parent-style-name=\"VM_Info\">"
             "<style:text-properties fo:font-size=\"13pt\" fo:font-weight=\"bold\" fo:color=\"#e0620d\"/></style:style>"
             "<style:style style:name=\"VM_Tags\" style:display-name=\"Rezept-Tags\" style:family=\"paragraph\" style:parent-style-name=\"VM_Info\">"
             "<style:text-properties fo:font-size=\"9pt\" fo:color=\"#555555\"/></style:style>"
             "<style:style style:name=\"VM_Zutat\" style:display-name=\"Zutat\" style:family=\"paragraph\" style:parent-style-name=\"Standard\">"
             "<style:paragraph-properties fo:margin-bottom=\"0.06cm\">"
             "<style:tab-stops><style:tab-stop style:position=\"2.3cm\" style:type=\"right\"/><style:tab-stop style:position=\"2.7cm\"/></style:tab-stops>"
             "</style:paragraph-properties></style:style>"
             "<style:style style:name=\"VM_Zutatengruppe\" style:display-name=\"Zutatengruppe\" style:family=\"paragraph\" style:parent-style-name=\"Standard\">"
             "<style:paragraph-properties fo:margin-top=\"0.2cm\" fo:margin-bottom=\"0.06cm\" fo:keep-with-next=\"always\"/>"
             "<style:text-properties style:font-name=\"Liberation Sans\" fo:font-weight=\"bold\" fo:color=\"%1\"/></style:style>"
             "<style:style style:name=\"VM_Klein\" style:display-name=\"Hinweis\" style:family=\"paragraph\" style:parent-style-name=\"Standard\">"
             "<style:text-properties fo:font-size=\"9pt\" fo:color=\"#555555\"/></style:style>"
             "<style:style style:name=\"Table_20_Contents\" style:display-name=\"Table Contents\" style:family=\"paragraph\" style:parent-style-name=\"Standard\">"
             "<style:paragraph-properties fo:margin-bottom=\"0cm\"/><style:text-properties style:font-name=\"Liberation Sans\" fo:font-size=\"9.5pt\"/></style:style>"
             "<style:style style:name=\"VM_Zahl\" style:display-name=\"Tabellenzahl\" style:family=\"paragraph\" style:parent-style-name=\"Table_20_Contents\">"
             "<style:paragraph-properties fo:text-align=\"end\"/></style:style>"
             "<style:style style:name=\"Footer\" style:family=\"paragraph\" style:parent-style-name=\"Standard\" style:class=\"extra\">"
             "<style:paragraph-properties fo:margin-bottom=\"0cm\" fo:padding-top=\"0.12cm\" fo:border-top=\"0.5pt solid #808080\" fo:border-bottom=\"none\""
             " fo:border-left=\"none\" fo:border-right=\"none\">"
             "<style:tab-stops><style:tab-stop style:position=\"17cm\" style:type=\"right\"/></style:tab-stops></style:paragraph-properties>"
             "<style:text-properties style:font-name=\"Liberation Sans\" fo:font-size=\"9pt\" fo:color=\"#555555\"/></style:style>"
             "<style:style style:name=\"Graphics\" style:family=\"graphic\">"
             "<style:graphic-properties text:anchor-type=\"paragraph\" svg:x=\"0cm\" svg:y=\"0cm\" style:wrap=\"dynamic\" style:vertical-pos=\"top\""
             " style:vertical-rel=\"paragraph\" style:horizontal-pos=\"center\" style:horizontal-rel=\"paragraph\"/></style:style>"
             "</office:styles>")
             .arg(accent);
    s += QStringLiteral(
        "<office:automatic-styles>"
        "<style:page-layout style:name=\"pm1\">"
        "<style:page-layout-properties fo:page-width=\"21cm\" fo:page-height=\"29.7cm\" style:print-orientation=\"portrait\""
        " fo:margin-top=\"2cm\" fo:margin-bottom=\"1.5cm\" fo:margin-left=\"2cm\" fo:margin-right=\"2cm\"/>"
        "<style:footer-style><style:header-footer-properties fo:min-height=\"0.6cm\" fo:margin-top=\"0.5cm\"/></style:footer-style>"
        "</style:page-layout>"
        "</office:automatic-styles>");
    s += QStringLiteral("<office:master-styles><style:master-page style:name=\"Standard\" style:page-layout-name=\"pm1\"><style:footer>"
                        "<text:p text:style-name=\"Footer\">%1<text:tab/>%2 <text:page-number text:select-page=\"current\">1</text:page-number>"
                        " %3 <text:page-count>1</text:page-count></text:p>"
                        "</style:footer></style:master-page></office:master-styles></office:document-styles>")
             .arg(esc(footerLeft), tr("Seite"), tr("von"));
    return s;
}

QString number(double v, int dec) { return Fmt::number(v, dec); }

} // namespace

// ---------------------------------------------------------------- ZIP

void Zip::add(const QString &name, const QByteArray &data, bool compress)
{
    Entry e;
    e.name = name.toUtf8();
    e.crc = crc32(data);
    e.size = quint32(data.size());
    QByteArray payload = data;
    e.method = 0;
    if (compress && data.size() > 64) {
        // qCompress: 4 Byte Länge + zlib-Strom (2 Byte Kopf … 4 Byte Adler-32) → rohes deflate
        const QByteArray z = qCompress(data, 9);
        if (z.size() > 10) {
            const QByteArray raw = z.mid(6, z.size() - 10);
            if (raw.size() < data.size()) {
                payload = raw;
                e.method = 8;
            }
        }
    }
    e.csize = quint32(payload.size());
    e.offset = quint32(m_data.size());
    put32(m_data, 0x04034b50);
    put16(m_data, 20);                       // benötigte Version
    put16(m_data, 0x0800);                   // Bit 11: Dateinamen in UTF-8
    put16(m_data, e.method);
    put16(m_data, 0);                        // Zeit
    put16(m_data, 0x5B69);                   // Datum (11.11.2025 – unerheblich)
    put32(m_data, e.crc);
    put32(m_data, e.csize);
    put32(m_data, e.size);
    put16(m_data, quint16(e.name.size()));
    put16(m_data, 0);
    m_data += e.name;
    m_data += payload;
    m_entries << e;
}

QByteArray Zip::finish()
{
    const quint32 cdStart = quint32(m_data.size());
    for (const Entry &e : m_entries) {
        put32(m_data, 0x02014b50);
        put16(m_data, 20);
        put16(m_data, 20);
        put16(m_data, 0x0800);
        put16(m_data, e.method);
        put16(m_data, 0);
        put16(m_data, 0x5B69);
        put32(m_data, e.crc);
        put32(m_data, e.csize);
        put32(m_data, e.size);
        put16(m_data, quint16(e.name.size()));
        put16(m_data, 0);   // extra
        put16(m_data, 0);   // Kommentar
        put16(m_data, 0);   // Datenträger
        put16(m_data, 0);   // interne Attribute
        put32(m_data, 0);   // externe Attribute
        put32(m_data, e.offset);
        m_data += e.name;
    }
    const quint32 cdSize = quint32(m_data.size()) - cdStart;
    put32(m_data, 0x06054b50);
    put16(m_data, 0);
    put16(m_data, 0);
    put16(m_data, quint16(m_entries.size()));
    put16(m_data, quint16(m_entries.size()));
    put32(m_data, cdSize);
    put32(m_data, cdStart);
    put16(m_data, 0);
    return m_data;
}

// ---------------------------------------------------------------- Dokument

QByteArray toOdt(const Recipe &r, const Options &opt)
{
    const double servings = opt.servings > 0 ? opt.servings : r.servings;
    const double factor = r.servings > 0 ? servings / r.servings : 1.0;

    // Bild
    QByteArray img;
    QString imgName, imgMime;
    double imgW = 0, imgH = 0;
    if (!r.image.isEmpty()) {
        img = QByteArray::fromBase64(r.image.toLatin1());
        QImage q;
        if (q.loadFromData(img) && q.width() > 0) {
            const bool png = img.startsWith("\x89PNG");
            imgName = png ? QStringLiteral("Pictures/rezeptbild.png") : QStringLiteral("Pictures/rezeptbild.jpg");
            imgMime = png ? QStringLiteral("image/png") : QStringLiteral("image/jpeg");
            imgW = opt.imageWidthCm;
            imgH = imgW * q.height() / q.width();
        } else {
            img.clear();
        }
    }

    QString b;   // office:text
    b += QStringLiteral("<text:h text:style-name=\"Heading_20_1\" text:outline-level=\"1\">%1</text:h>").arg(esc(r.title));
    // Portionen + Energie; das Bild hängt an diesem Absatz (rechts, Text fließt links vorbei)
    QString frame;
    if (!img.isEmpty())
        frame = QStringLiteral("<draw:frame draw:style-name=\"fr1\" draw:name=\"Rezeptbild\" text:anchor-type=\"paragraph\" svg:width=\"%1cm\" svg:height=\"%2cm\" draw:z-index=\"0\">"
                               "<draw:image xlink:href=\"%3\" xlink:type=\"simple\" xlink:show=\"embed\" xlink:actuate=\"onLoad\"/>"
                               "<svg:title>%4</svg:title></draw:frame>")
                    .arg(QString::number(imgW, 'f', 2), QString::number(imgH, 'f', 2), imgName, esc(r.title));
    QString servingsText = Fmt::amount(servings, QString()) + QLatin1Char(' ')
                           + (r.yieldUnit.isEmpty() ? (servings == 1 ? tr("Portion") : tr("Portionen")) : r.yieldUnit);
    const double grams = MealTypes::servingGrams(r);
    if (grams >= 10 && !Nutrition::isMenu(r)) servingsText += tr(" (ca. %1 g je Portion)").arg(qRound(grams / 5.0) * 5);
    if (r.prepMinutes > 0) servingsText += tr(" · Zubereitung %1 min").arg(r.prepMinutes);
    b += QStringLiteral("<text:p text:style-name=\"VM_Info\">%1%2</text:p>").arg(frame, esc(servingsText));
    const bool hasNut = r.nutrition.valid() && r.nutrition.matched > 0 && !Nutrition::isMenu(r);
    if (hasNut) {
        const Nutrients &p = r.nutrition.perServing;
        b += QStringLiteral("<text:p text:style-name=\"VM_Energie\">%1</text:p>")
                 .arg(esc(tr("%1 kcal / %2 kJ je Portion").arg(Fmt::kcal(p.kcal), Fmt::kcal(p.kJ()))));
        b += QStringLiteral("<text:p text:style-name=\"VM_Info\">%1</text:p>")
                 .arg(esc(tr("Eiweiß %1 g · Fett %2 g · Kohlenhydrate %3 g · Ballaststoffe %4 g")
                              .arg(number(p.protein, 1), number(p.fat, 1), number(p.carbs, 1), number(p.fiber, 1))));
    }
    if (!r.tags.isEmpty()) {
        QStringList t;
        for (const QString &tag : r.tags) t << RecipeHtml::tagLabel(tag);
        b += QStringLiteral("<text:p text:style-name=\"VM_Tags\">%1</text:p>").arg(esc(t.join(QStringLiteral(" · "))));
    }

    // Zutaten
    b += QStringLiteral("<text:h text:style-name=\"Heading_20_2\" text:outline-level=\"2\">%1</text:h>")
             .arg(Nutrition::isMenu(r) ? tr("Menüfolge") : tr("Zutaten"));
    for (const Ingredient &src : r.ingredients) {
        if (src.heading) {
            b += QStringLiteral("<text:p text:style-name=\"VM_Zutatengruppe\">%1</text:p>").arg(esc(src.name));
            continue;
        }
        Ingredient i = src;
        i.amount *= factor;
        i.amountMax *= factor;
        QString amount = i.amountText();
        if (!i.unit.isEmpty()) amount += (amount.isEmpty() ? QString() : QStringLiteral(" ")) + i.unit;
        QString name = esc(i.name);
        if (!i.note.isEmpty()) name += QStringLiteral(", <text:span text:style-name=\"T_note\">%1</text:span>").arg(esc(i.note));
        b += QStringLiteral("<text:p text:style-name=\"VM_Zutat\"><text:tab/>%1<text:tab/>%2</text:p>").arg(esc(amount), name);
    }

    // Zubereitung
    if (!r.instructions.trimmed().isEmpty()) {
        b += QStringLiteral("<text:h text:style-name=\"Heading_20_2\" text:outline-level=\"2\">%1</text:h>").arg(tr("Zubereitung"));
        const QString note = RecipeHtml::languageNote(r);
        if (!note.isEmpty()) b += QStringLiteral("<text:p text:style-name=\"VM_Klein\">%1</text:p>").arg(esc(note));
        for (const QString &para : r.instructions.split(QRegularExpression(QStringLiteral("\\n\\s*\\n")), Qt::SkipEmptyParts)) {
            QString t = esc(para.trimmed());
            t.replace(QLatin1Char('\n'), QStringLiteral("<text:line-break/>"));
            b += QStringLiteral("<text:p text:style-name=\"Text_20_body\">%1</text:p>").arg(t);
        }
    }

    // Nährwerte
    if (hasNut && opt.nutrition) {
        const Nutrients &p = r.nutrition.perServing;
        const Nutrients &t = r.nutrition.total;
        const Nutrients ri = Nutrition::referenceIntake();
        b += QStringLiteral("<text:h text:style-name=\"Heading_20_2\" text:outline-level=\"2\">%1</text:h>").arg(tr("Nährwerte"));
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
        auto cell = [](const QString &text, bool num, bool head = false) {
            return QStringLiteral("<table:table-cell table:style-name=\"%1\" office:value-type=\"string\"><text:p text:style-name=\"%2\">%3</text:p></table:table-cell>")
                .arg(head ? QStringLiteral("TC_head") : QStringLiteral("TC"), num ? QStringLiteral("VM_Zahl") : QStringLiteral("Table_20_Contents"), text);
        };
        const QString perHead = r.yieldUnit.isEmpty() ? tr("je Portion") : tr("je %1").arg(r.yieldUnit);
        b += QStringLiteral("<table:table table:name=\"Naehrwerte\" table:style-name=\"Tab\">"
                            "<table:table-column table:style-name=\"Tab.A\"/><table:table-column table:style-name=\"Tab.B\" table:number-columns-repeated=\"3\"/>");
        b += QStringLiteral("<table:table-header-rows><table:table-row>") + cell(QStringLiteral("<text:span text:style-name=\"T_bold\">%1</text:span>").arg(tr("Nährwerte")), false, true)
             + cell(QStringLiteral("<text:span text:style-name=\"T_bold\">%1</text:span>").arg(esc(perHead)), true, true)
             + cell(QStringLiteral("<text:span text:style-name=\"T_bold\">%1</text:span>").arg(tr("gesamt")), true, true)
             + cell(QStringLiteral("<text:span text:style-name=\"T_bold\">%1*</text:span>").arg(tr("RM")), true, true)
             + QStringLiteral("</table:table-row></table:table-header-rows>");
        for (const Row &row : rows) {
            const QString label = row.sub ? QStringLiteral("<text:s text:c=\"3\"/>") + esc(row.label) : esc(row.label);
            b += QStringLiteral("<table:table-row>") + cell(label, false)
                 + cell(esc(number(row.per, row.dec) + QLatin1Char(' ') + row.unit), true)
                 + cell(esc(number(row.total, row.dec) + QLatin1Char(' ') + row.unit), true)
                 + cell(row.ref > 0 ? esc(number(row.per / row.ref * 100.0, 0) + QStringLiteral(" %")) : QString(), true)
                 + QStringLiteral("</table:table-row>");
        }
        b += QStringLiteral("</table:table>");
        b += QStringLiteral("<text:p text:style-name=\"VM_Klein\">%1</text:p>")
                 .arg(esc(tr("* Referenzmenge für einen durchschnittlichen Erwachsenen (8 400 kJ / 2 000 kcal). Nährwerte berechnet "
                             "aus %1 von %2 Zutaten – Schätzwerte, kein Medizinprodukt.")
                              .arg(r.nutrition.matched)
                              .arg(r.nutrition.countable)));
    }
    if (!r.source.isEmpty())
        b += QStringLiteral("<text:p text:style-name=\"VM_Klein\">%1 %2</text:p>").arg(tr("Quelle:"), esc(r.source));

    QString content = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<office:document-content") + QString::fromLatin1(kNamespaces) + QStringLiteral(">");
    content += QString::fromLatin1(kFonts);
    content += QStringLiteral(
        "<office:automatic-styles>"
        "<style:style style:name=\"fr1\" style:family=\"graphic\" style:parent-style-name=\"Graphics\">"
        "<style:graphic-properties style:wrap=\"left\" style:number-wrapped-paragraphs=\"no-limit\" style:wrap-contour=\"false\""
        " fo:margin-left=\"0.4cm\" fo:margin-right=\"0cm\" fo:margin-top=\"0cm\" fo:margin-bottom=\"0.3cm\""
        " style:vertical-pos=\"top\" style:vertical-rel=\"paragraph\" style:horizontal-pos=\"right\" style:horizontal-rel=\"paragraph\""
        " fo:border=\"none\" style:mirror=\"none\" fo:clip=\"rect(0cm, 0cm, 0cm, 0cm)\" draw:luminance=\"0%\" draw:contrast=\"0%\" draw:red=\"0%\""
        " draw:green=\"0%\" draw:blue=\"0%\" draw:gamma=\"100%\" draw:color-inversion=\"false\" draw:image-opacity=\"100%\" draw:color-mode=\"standard\"/>"
        "</style:style>"
        "<style:style style:name=\"T_note\" style:family=\"text\"><style:text-properties fo:color=\"#555555\" fo:font-style=\"italic\"/></style:style>"
        "<style:style style:name=\"T_bold\" style:family=\"text\"><style:text-properties fo:font-weight=\"bold\"/></style:style>"
        "<style:style style:name=\"Tab\" style:family=\"table\"><style:table-properties style:width=\"12cm\" style:may-break-between-rows=\"false\" table:align=\"left\" fo:margin-top=\"0.1cm\" fo:margin-bottom=\"0.2cm\"/></style:style>"
        "<style:style style:name=\"Tab.A\" style:family=\"table-column\"><style:table-column-properties style:column-width=\"5.4cm\"/></style:style>"
        "<style:style style:name=\"Tab.B\" style:family=\"table-column\"><style:table-column-properties style:column-width=\"2.2cm\"/></style:style>"
        "<style:style style:name=\"TC\" style:family=\"table-cell\"><style:table-cell-properties fo:padding=\"0.06cm\" fo:border-bottom=\"0.5pt solid #b0b0b0\" fo:border-top=\"none\" fo:border-left=\"none\" fo:border-right=\"none\"/></style:style>"
        "<style:style style:name=\"TC_head\" style:family=\"table-cell\"><style:table-cell-properties fo:padding=\"0.06cm\" fo:background-color=\"#e8f0f8\" fo:border-bottom=\"0.75pt solid #0f67ab\" fo:border-top=\"none\" fo:border-left=\"none\" fo:border-right=\"none\"/></style:style>"
        "</office:automatic-styles>");
    content += QStringLiteral("<office:body><office:text>") + b + QStringLiteral("</office:text></office:body></office:document-content>");

    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QString meta = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<office:document-meta") + QString::fromLatin1(kNamespaces)
                         + QStringLiteral("><office:meta><meta:generator>VitaMeals/%1</meta:generator><dc:title>%2</dc:title>"
                                          "<dc:language>de-DE</dc:language><meta:creation-date>%3</meta:creation-date></office:meta></office:document-meta>")
                               .arg(esc(QCoreApplication::applicationVersion()), esc(r.title), now);
    QString manifest = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                      "<manifest:manifest xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\" manifest:version=\"1.2\">"
                                      "<manifest:file-entry manifest:full-path=\"/\" manifest:version=\"1.2\" manifest:media-type=\"application/vnd.oasis.opendocument.text\"/>"
                                      "<manifest:file-entry manifest:full-path=\"content.xml\" manifest:media-type=\"text/xml\"/>"
                                      "<manifest:file-entry manifest:full-path=\"styles.xml\" manifest:media-type=\"text/xml\"/>"
                                      "<manifest:file-entry manifest:full-path=\"meta.xml\" manifest:media-type=\"text/xml\"/>");
    if (!img.isEmpty()) manifest += QStringLiteral("<manifest:file-entry manifest:full-path=\"%1\" manifest:media-type=\"%2\"/>").arg(imgName, imgMime);
    manifest += QStringLiteral("</manifest:manifest>");

    Zip zip;
    zip.add(QStringLiteral("mimetype"), QByteArrayLiteral("application/vnd.oasis.opendocument.text"), false);   // erste Datei, unkomprimiert
    zip.add(QStringLiteral("content.xml"), content.toUtf8());
    zip.add(QStringLiteral("styles.xml"), stylesXml(r.title).toUtf8());
    zip.add(QStringLiteral("meta.xml"), meta.toUtf8());
    if (!img.isEmpty()) zip.add(imgName, img, false);
    zip.add(QStringLiteral("META-INF/manifest.xml"), manifest.toUtf8());
    return zip.finish();
}

bool write(const Recipe &r, const QString &file, const Options &opt, QString *error)
{
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(toOdt(r, opt));
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

QString defaultFileName(const Recipe &r)
{
    QString n = r.title;
    n.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("-"));
    n = n.simplified();
    if (n.isEmpty()) n = QStringLiteral("rezept");
    return n + QStringLiteral(".odt");
}

} // namespace OdtExport
