// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "exportdialogs.h"
#include "icons.h"
#include "latexexport.h"
#include "imagecropdialog.h"
#include "recipedb.h"
#include "websiteexport.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QPixmap>
#include <QColorDialog>
#include <QFrame>
#include <QGuiApplication>
#include <QScreen>
#include <QScrollArea>
#include <QLocale>
#include <QSlider>
#include <QPainter>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

// ---------------------------------------------------------------- Website

WebsiteExportDialog::WebsiteExportDialog(const QStringList &filteredIds, QWidget *parent) : QDialog(parent), m_filtered(filteredIds)
{
    setWindowTitle(tr("Als Website exportieren"));
    QSettings s;
    auto *lay = new QVBoxLayout(this);
    auto *info = new QLabel(tr("Erzeugt eine themenbasierte Website (HTML5 mit PHP-Suche) aus der Rezeptsammlung. "
                               "Den Ordner anschließend vollständig auf den eigenen Webserver hochladen."));
    info->setWordWrap(true);
    lay->addWidget(info);
    auto *form = new QFormLayout;
    m_title = new QLineEdit(s.value(QStringLiteral("web/title"), RecipeDb::instance().name()).toString());
    form->addRow(tr("Titel der Website:"), m_title);
    m_subtitle = new QLineEdit(s.value(QStringLiteral("web/subtitle"), tr("Rezepte mit Nährwerten")).toString());
    form->addRow(tr("Untertitel:"), m_subtitle);
    m_author = new QLineEdit(s.value(QStringLiteral("web/author")).toString());
    form->addRow(tr("Autor/Betreiber:"), m_author);
    m_all = new QRadioButton(tr("Alle %1 Rezepte").arg(RecipeDb::instance().count()));
    m_onlyFiltered = new QRadioButton(tr("Nur die %1 Rezepte der aktuellen Liste (Filter)").arg(filteredIds.size()));
    m_all->setChecked(true);
    m_onlyFiltered->setEnabled(filteredIds.size() < RecipeDb::instance().count() && !filteredIds.isEmpty());
    form->addRow(tr("Umfang:"), m_all);
    form->addRow(QString(), m_onlyFiltered);
    m_nutrition = new QCheckBox(tr("Nährwerte anzeigen"));
    m_nutrition->setChecked(s.value(QStringLiteral("web/nutrition"), true).toBool());
    form->addRow(QString(), m_nutrition);
    auto *fl = new QHBoxLayout;
    m_folder = new QLineEdit(s.value(QStringLiteral("web/folder"),
                                     QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(QStringLiteral("ViviaMeals-Website")))
                                 .toString());
    auto *browse = new QPushButton(Icons::get(QStringLiteral("open")), QString());
    fl->addWidget(m_folder, 1);
    fl->addWidget(browse);
    form->addRow(tr("Zielordner:"), fl);
    m_open = new QCheckBox(tr("Nach dem Export im Browser öffnen"));
    m_open->setChecked(true);
    form->addRow(QString(), m_open);
    lay->addLayout(form);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Exportieren"));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &WebsiteExportDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, tr("Zielordner wählen"), m_folder->text());
        if (!d.isEmpty()) m_folder->setText(QDir::toNativeSeparators(d));
    });
    resize(560, sizeHint().height());
}

void WebsiteExportDialog::accept()
{
    const QString folder = QDir::fromNativeSeparators(m_folder->text().trimmed());
    if (folder.isEmpty()) return;
    QDir d(folder);
    if (d.exists() && !d.isEmpty() && !QFileInfo::exists(d.filePath(QStringLiteral("index.html")))) {
        if (QMessageBox::question(this, windowTitle(), tr("Der Ordner ist nicht leer. Trotzdem hineinschreiben?")) != QMessageBox::Yes) return;
    }
    QSettings s;
    s.setValue(QStringLiteral("web/title"), m_title->text());
    s.setValue(QStringLiteral("web/subtitle"), m_subtitle->text());
    s.setValue(QStringLiteral("web/author"), m_author->text());
    s.setValue(QStringLiteral("web/folder"), m_folder->text());
    s.setValue(QStringLiteral("web/nutrition"), m_nutrition->isChecked());
    WebsiteExport::Options opt;
    opt.title = m_title->text().trimmed();
    opt.subtitle = m_subtitle->text().trimmed();
    opt.author = m_author->text().trimmed();
    opt.nutrition = m_nutrition->isChecked();
    if (m_onlyFiltered->isChecked()) opt.onlyIds = m_filtered;
    QProgressDialog prog(tr("Website wird erzeugt …"), tr("Abbrechen"), 0, 100, this);
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(300);
    QString err;
    const bool ok = WebsiteExport::exportSite(folder, opt, &err, [&](int done, int total) {
        prog.setMaximum(total);
        prog.setValue(done);
        return !prog.wasCanceled();
    });
    prog.close();
    if (!ok) {
        QMessageBox::warning(this, windowTitle(), tr("Export fehlgeschlagen: %1").arg(err));
        return;
    }
    if (m_open->isChecked()) QDesktopServices::openUrl(QUrl::fromLocalFile(d.filePath(QStringLiteral("index.html"))));
    else QMessageBox::information(this, windowTitle(), tr("Die Website wurde nach %1 exportiert.").arg(QDir::toNativeSeparators(folder)));
    QDialog::accept();
}

// ---------------------------------------------------------------- LaTeX

// Vorschau einer A4-Seite mit Titel und Hintergrundbild (Maße in mm)
class BackgroundPreview : public QWidget
{
public:
    BackgroundPreview() { setMinimumSize(150, 212); setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); }
    QSize sizeHint() const override { return { 150, 212 }; }
    void set(const QImage &img, const LatexExport::MasterOptions &m)
    {
        m_img = img;
        m_opt = m;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const double s = qMin((width() - 2) / 210.0, (height() - 2) / 297.0);
        const QRectF page(1, 1, 210 * s, 297 * s);
        p.fillRect(page, Qt::white);
        const double bcor = m_opt.bgShiftBinding ? m_opt.bindingMm : 0;
        // Bindung links (rechte Seite): grau angedeutet
        p.fillRect(QRectF(page.left(), page.top(), m_opt.bindingMm * s, page.height()), QColor(0, 0, 0, 25));
        if (!m_img.isNull()) {
            QRectF r;
            const double ratio = double(m_img.height()) / qMax(1, m_img.width());
            switch (m_opt.bgPlacement) {
            case LatexExport::MasterOptions::BgPlacement::FillPage: r = page; break;
            case LatexExport::MasterOptions::BgPlacement::Centered: {
                const double w = m_opt.bgWidthMm, h = w * ratio;
                const double x = bcor + (210 - bcor - w) / 2;
                r = QRectF(page.left() + x * s, page.top() + (297 - m_opt.bgBottomMm - h) * s, w * s, h * s);
                break;
            }
            case LatexExport::MasterOptions::BgPlacement::Free: {
                const double w = m_opt.bgWidthMm, h = w * ratio;
                r = QRectF(page.left() + (m_opt.bgLeftMm + bcor) * s, page.top() + (297 - m_opt.bgBottomMm - h) * s, w * s, h * s);
                break;
            }
            }
            p.save();
            p.setClipRect(page);
            p.drawImage(r, m_img);
            p.restore();
            if (!page.contains(r.adjusted(1, 1, -1, -1))) {
                p.setPen(QPen(QColor(QStringLiteral("#c0392b")), 2, Qt::DashLine));
                p.drawRect(r.intersected(page));
            }
        }
        // Titel (Schmutztitel bzw. Titelseite) andeuten
        QFont f = font();
        f.setBold(true);
        f.setPointSizeF(qMax(5.0, 7.0 * s / 0.5));
        const bool title = m_opt.bgTarget == LatexExport::MasterOptions::BgTarget::TitlePage;
        const double y = title ? 297 * 0.30 : 297 * 0.38;
        auto text = [&](const QString &str, const QString &color, double yMm, const QFont &font) {
            p.setFont(font);
            const QRectF r(page.left() + m_opt.bindingMm * s, page.top() + yMm * s - 10, (210 - m_opt.bindingMm) * s, 20);
            if (m_opt.titleShadow) {
                p.setPen(QColor(m_opt.shadowColor));
                p.drawText(r.translated(1, 1), Qt::AlignCenter, str);
            }
            p.setPen(QColor(color));
            p.drawText(r, Qt::AlignCenter, str);
        };
        text(m_opt.title.isEmpty() ? QStringLiteral("Titel") : m_opt.title, m_opt.titleColor, y, f);
        if (title && !m_opt.subtitle.isEmpty()) {
            QFont sf = f;
            sf.setBold(false);
            sf.setPointSizeF(f.pointSizeF() * 0.7);
            text(m_opt.subtitle, m_opt.subtitleColor, y + 12, sf);
        }
        p.setPen(QColor(0, 0, 0, 80));
        p.drawRect(page);
    }

private:
    QImage m_img;
    LatexExport::MasterOptions m_opt;
};

// Vorschau der Titelseite: Titel, Untertitel, Bild mit den eingestellten Abständen, Autor (Maße in mm)
class TitlePagePreview : public QWidget
{
public:
    TitlePagePreview() { setFixedSize(150, 212); }
    void set(const QImage &img, const LatexExport::MasterOptions &m)
    {
        m_img = img;
        m_opt = m;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.setRenderHint(QPainter::Antialiasing);
        const double s = qMin((width() - 2) / 210.0, (height() - 2) / 297.0);
        const QRectF page(1, 1, 210 * s, 297 * s);
        p.fillRect(page, Qt::white);
        // Satzspiegel ungefähr: innen Bindung + 25 mm, außen 25 mm, oben 30 mm, unten 30 mm
        const double left = m_opt.bindingMm + 25, textW = 210 - left - 25, textH = 237;
        double y = 30 + 0.10 * textH;
        auto line = [&](const QString &str, const QString &color, double sizeMm, bool bold, bool shadow) {
            QFont f = font();
            f.setBold(bold);
            f.setPixelSize(qMax(4, int(sizeMm * s)));
            p.setFont(f);
            const QRectF r(page.left() + left * s, page.top() + y * s, textW * s, sizeMm * 1.6 * s);
            if (shadow && m_opt.titleShadow) {
                p.setPen(QColor(m_opt.shadowColor));
                p.drawText(r.translated(0.8, 0.8), Qt::AlignHCenter | Qt::AlignTop, str);
            }
            p.setPen(QColor(color));
            p.drawText(r, Qt::AlignHCenter | Qt::AlignTop, str);
            y += sizeMm * 1.6;
        };
        line(m_opt.title.isEmpty() ? QStringLiteral("Titel") : m_opt.title, m_opt.titleColor, 9, true, true);
        if (!m_opt.subtitle.isEmpty()) {
            y += 3;
            line(m_opt.subtitle, m_opt.subtitleColor, 6, false, true);
        }
        if (!m_img.isNull()) {
            y += m_opt.titleImageGapAboveMm;
            double w = qMin(m_opt.titleImageWidthMm, textW);
            double h = w * m_img.height() / qMax(1, m_img.width());
            if (h > 0.45 * textH) {   // wie keepaspectratio mit height=0.45\textheight
                h = 0.45 * textH;
                w = h * m_img.width() / qMax(1, m_img.height());
            }
            const QRectF r(page.left() + (left + (textW - w) / 2) * s, page.top() + y * s, w * s, h * s);
            p.drawImage(r, m_img);
            y += h + m_opt.titleImageGapBelowMm;
        } else {
            y += 12;
        }
        if (!m_opt.author.isEmpty()) line(m_opt.author, QStringLiteral("#000000"), 5, false, false);
        p.setPen(QColor(0, 0, 0, 80));
        p.setBrush(Qt::NoBrush);
        p.drawRect(page);
        if (y > 297 - 25) {   // ragt über den unteren Rand
            p.setPen(QPen(QColor(QStringLiteral("#c0392b")), 2, Qt::DashLine));
            p.drawLine(QPointF(page.left(), page.top() + (297 - 30) * s), QPointF(page.right(), page.top() + (297 - 30) * s));
        }
    }

private:
    QImage m_img;
    LatexExport::MasterOptions m_opt;
};

LatexExportDialog::LatexExportDialog(const Recipe &r, QWidget *parent) : QDialog(parent), m_recipe(r)
{
    setWindowTitle(tr("Als LaTeX-Datei exportieren"));
    QSettings s;
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    // Alle Optionen in einem rollbaren Bereich – die Knöpfe bleiben auch auf kleinen Bildschirmen erreichbar
    auto *outer = new QVBoxLayout(this);
    auto *scroll = new QScrollArea;
    m_scroll = scroll;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(0, 0, 6, 0);
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);
    auto *form = new QFormLayout;
    form->addRow(tr("Rezept:"), new QLabel(QStringLiteral("<b>%1</b>").arg(r.title.toHtmlEscaped())));
    m_class = new QComboBox;
    m_class->addItem(QStringLiteral("scrbook (KOMA-Script)"), int(LatexExport::DocClass::ScrBook));
    m_class->addItem(QStringLiteral("book (LaTeX-Standard)"), int(LatexExport::DocClass::Book));
    m_class->setCurrentIndex(s.value(QStringLiteral("tex/class"), 0).toInt());
    form->addRow(tr("Dokumentklasse:"), m_class);
    const LatexExport::Installation inst = LatexExport::findInstallation();
    auto *instLabel = new QLabel(inst.found() ? QStringLiteral("<small>%1 – <tt>%2</tt></small>").arg(inst.name.toHtmlEscaped(), inst.program.toHtmlEscaped())
                                              : QStringLiteral("<small><span style=\"color:#c0392b\">%1</span></small>").arg(tr("Keine LaTeX-Installation gefunden.")));
    instLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("LaTeX:"), instLabel);
    lay->addLayout(form);

    // ---- Ausgabeart
    auto *modeBox = new QGroupBox(tr("Ausgabe"));
    auto *ml = new QVBoxLayout(modeBox);
    m_modeChapter = new QRadioButton(tr("Kapitel zum Einbinden in ein Master-Dokument (\\input)"));
    m_modeNewMaster = new QRadioButton(tr("Neues Master-Dokument (Kochbuch) erstellen und das Rezept eintragen"));
    m_modeStandalone = new QRadioButton(tr("Vollständiges Einzeldokument (nur dieses Rezept)"));
    ml->addWidget(m_modeChapter);
    m_registerRow = new QWidget;
    auto *rl = new QHBoxLayout(m_registerRow);
    rl->setContentsMargins(24, 0, 0, 0);
    m_register = new QCheckBox(tr("eintragen in:"));
    m_register->setToolTip(tr("Fügt \\input{…} in das Master-Dokument ein – vor „%1“, sonst vor \\backmatter bzw. \\end{document}")
                               .arg(LatexExport::masterMarker()));
    m_register->setChecked(s.value(QStringLiteral("tex/register"), true).toBool());
    m_masterFile = new QLineEdit(QDir::toNativeSeparators(s.value(QStringLiteral("tex/master")).toString()));
    m_masterFile->setPlaceholderText(tr("bestehendes Master-Dokument (.tex)"));
    auto *masterBrowse = new QPushButton(Icons::get(QStringLiteral("open")), QString());
    rl->addWidget(m_register);
    rl->addWidget(m_masterFile, 1);
    rl->addWidget(masterBrowse);
    ml->addWidget(m_registerRow);
    ml->addWidget(m_modeNewMaster);
    ml->addWidget(m_modeStandalone);
    const int mode = s.value(QStringLiteral("tex/mode"), 0).toInt();
    (mode == 1 ? m_modeNewMaster : mode == 2 ? m_modeStandalone : m_modeChapter)->setChecked(true);
    lay->addWidget(modeBox);

    // ---- Neues Master-Dokument
    auto *nm = new QGroupBox(tr("Neues Master-Dokument"));
    m_newMasterBox = nm;
    auto *nf = new QFormLayout(nm);
    auto *nfr = new QHBoxLayout;
    m_newMasterFile = new QLineEdit(QDir::toNativeSeparators(
        s.value(QStringLiteral("tex/newMaster"), QDir(docs).filePath(QStringLiteral("Kochbuch/kochbuch.tex"))).toString()));
    auto *newBrowse = new QPushButton(Icons::get(QStringLiteral("open")), QString());
    nfr->addWidget(m_newMasterFile, 1);
    nfr->addWidget(newBrowse);
    nf->addRow(tr("Datei:"), nfr);
    m_bookTitle = new QLineEdit(s.value(QStringLiteral("tex/bookTitle"), tr("Mein Kochbuch")).toString());
    nf->addRow(tr("Buchtitel:"), m_bookTitle);
    m_subtitle = new QLineEdit(s.value(QStringLiteral("tex/subtitle")).toString());
    m_subtitle->setPlaceholderText(tr("optional"));
    nf->addRow(tr("Untertitel:"), m_subtitle);
    // Schriftfarben von Buchtitel und Untertitel (Vorgabe Schwarz), Schatten
    auto colorButton = [this](const QString &text, const QString &color) {
        auto *b = new QPushButton(text);
        b->setProperty("color", color);
        auto paint = [b] {
            QPixmap pm(16, 16);
            pm.fill(QColor(b->property("color").toString()));
            b->setIcon(QIcon(pm));
            b->setToolTip(tr("Schriftfarbe: %1 – klicken zum Ändern").arg(b->property("color").toString().toUpper()));
        };
        paint();
        connect(b, &QPushButton::clicked, this, [this, b, paint] {
            const QColor c = QColorDialog::getColor(QColor(b->property("color").toString()), this, tr("Schriftfarbe wählen"));
            if (!c.isValid()) return;
            b->setProperty("color", c.name());
            paint();
            updateBackground();
        });
        return b;
    };
    auto *colors = new QHBoxLayout;
    m_titleColor = colorButton(tr("Titel"), s.value(QStringLiteral("tex/titleColor"), QStringLiteral("#000000")).toString());
    m_subtitleColor = colorButton(tr("Untertitel"), s.value(QStringLiteral("tex/subtitleColor"), QStringLiteral("#000000")).toString());
    m_titleShadow = new QCheckBox(tr("mit Schatten"));
    m_titleShadow->setChecked(s.value(QStringLiteral("tex/titleShadow"), true).toBool());
    m_titleShadow->setToolTip(tr("Buchtitel und Untertitel werfen einen grauen Schatten nach rechts unten"));
    auto *resetColors = new QPushButton(tr("Schwarz"));
    resetColors->setToolTip(tr("Beide Farben auf Schwarz zurücksetzen"));
    colors->addWidget(m_titleColor);
    colors->addWidget(m_subtitleColor);
    colors->addWidget(resetColors);
    colors->addWidget(m_titleShadow);
    colors->addStretch(1);
    connect(resetColors, &QPushButton::clicked, this, [this] {
        for (QPushButton *b : { m_titleColor, m_subtitleColor }) {
            b->setProperty("color", QStringLiteral("#000000"));
            QPixmap pm(16, 16);
            pm.fill(Qt::black);
            b->setIcon(QIcon(pm));
        }
        updateBackground();
    });
    connect(m_titleShadow, &QCheckBox::toggled, this, &LatexExportDialog::updateBackground);
    m_author = new QLineEdit(s.value(QStringLiteral("tex/author"), s.value(QStringLiteral("editor/defaultSource"))).toString());
    nf->addRow(tr("Autor:"), m_author);
    nf->addRow(tr("Schriftfarbe:"), colors);
    m_noDate = new QCheckBox(tr("kein Datum auf dem Titelblatt"));
    m_noDate->setChecked(s.value(QStringLiteral("tex/noDate"), true).toBool());
    nf->addRow(QString(), m_noDate);
    auto *bgr = new QHBoxLayout;
    m_bgImage = new QLineEdit(QDir::toNativeSeparators(s.value(QStringLiteral("tex/bgImage")).toString()));
    m_bgImage->setPlaceholderText(tr("optional – Bild für Schmutztitel oder Titelseite"));
    m_bgImage->setClearButtonEnabled(true);
    auto *bgBrowse = new QPushButton(Icons::get(QStringLiteral("open")), QString());
    m_bgCropButton = new QPushButton(Icons::get(QStringLiteral("image")), tr("Zuschneiden …"));
    m_bgCropButton->setToolTip(tr("Ausschnitt des Bildes wählen und drehen – das Original bleibt unverändert"));
    bgr->addWidget(m_bgImage, 1);
    bgr->addWidget(bgBrowse);
    bgr->addWidget(m_bgCropButton);
    nf->addRow(tr("Hintergrundbild:"), bgr);
    {
        const QRectF c = s.value(QStringLiteral("tex/bgCrop")).toRectF();
        if (c.isValid()) m_bgCrop = c;
        m_bgRotation = s.value(QStringLiteral("tex/bgRotation"), 0).toInt();
    }
    // Platzierung des Hintergrundbilds mit Vorschau
    m_bgOptions = new QWidget;
    auto *bgl = new QHBoxLayout(m_bgOptions);
    bgl->setContentsMargins(0, 0, 0, 0);
    auto *bgf = new QFormLayout;
    m_bgTarget = new QComboBox;
    m_bgTarget->addItem(tr("Schmutztitel"), int(LatexExport::MasterOptions::BgTarget::HalfTitle));
    m_bgTarget->addItem(tr("Titelseite"), int(LatexExport::MasterOptions::BgTarget::TitlePage));
    m_bgTarget->setCurrentIndex(qMax(0, m_bgTarget->findData(s.value(QStringLiteral("tex/bgTarget"), 0).toInt())));
    bgf->addRow(tr("auf:"), m_bgTarget);
    m_bgPlacement = new QComboBox;
    m_bgPlacement->addItem(tr("ganze Seite (zugeschnitten)"), int(LatexExport::MasterOptions::BgPlacement::FillPage));
    m_bgPlacement->addItem(tr("waagerecht mittig"), int(LatexExport::MasterOptions::BgPlacement::Centered));
    m_bgPlacement->addItem(tr("frei positioniert"), int(LatexExport::MasterOptions::BgPlacement::Free));
    m_bgPlacement->setCurrentIndex(qMax(0, m_bgPlacement->findData(s.value(QStringLiteral("tex/bgPlacement"), 0).toInt())));
    bgf->addRow(tr("Platzierung:"), m_bgPlacement);
    auto mmBox = [](double v, double max) {
        auto *b = new QDoubleSpinBox;
        b->setLocale(QLocale(QLocale::German));   // Dezimalkomma
        b->setRange(0, max);
        b->setDecimals(1);
        b->setSingleStep(1);
        b->setSuffix(QStringLiteral(" mm"));
        b->setValue(v);
        return b;
    };
    m_bgWidth = mmBox(s.value(QStringLiteral("tex/bgWidth"), 140).toDouble(), 297);
    m_bgWidth->setMinimum(10);
    m_bgLeft = mmBox(s.value(QStringLiteral("tex/bgLeft"), 45).toDouble(), 210);
    m_bgBottom = mmBox(s.value(QStringLiteral("tex/bgBottom"), 60).toDouble(), 297);
    m_bgLeft->setToolTip(tr("Abstand des linken Bildrands von der linken Papierkante"));
    m_bgBottom->setToolTip(tr("Abstand des unteren Bildrands von der unteren Papierkante"));
    auto *wr = new QHBoxLayout;
    m_bgScale = new QSlider(Qt::Horizontal);
    m_bgScale->setRange(100, 2100);   // 0,1 mm
    m_bgScale->setToolTip(tr("Bild stufenlos skalieren – höchstens so breit, dass es auf die A4-Seite passt"));
    auto *fit = new QPushButton(tr("max."));
    fit->setToolTip(tr("Größtmögliche Breite, ohne über die Seite hinauszuragen"));
    wr->addWidget(m_bgWidth);
    wr->addWidget(m_bgScale, 1);
    wr->addWidget(fit);
    bgf->addRow(tr("Breite:"), wr);
    m_bgMaxInfo = new QLabel;
    bgf->addRow(QString(), m_bgMaxInfo);
    connect(m_bgScale, &QSlider::valueChanged, this, [this](int v) {
        if (!m_updatingBg) m_bgWidth->setValue(v / 10.0);
    });
    connect(fit, &QPushButton::clicked, this, [this] { m_bgWidth->setValue(m_bgWidth->maximum()); });
    bgf->addRow(tr("links:"), m_bgLeft);
    bgf->addRow(tr("unten:"), m_bgBottom);
    m_bgBinding = new QCheckBox(tr("um die Bindekorrektur verschieben"));
    m_bgBinding->setToolTip(tr("Bei Ring- oder Leimbindung rückt das Bild auf der rechten Seite um die Bindekorrektur nach rechts (\\BindungsHSpace)"));
    m_bgBinding->setChecked(s.value(QStringLiteral("tex/bgBinding"), true).toBool());
    m_bgTrim = new QCheckBox(tr("weißen Rand des Bildes abschneiden"));
    m_bgTrim->setToolTip(tr("Z. B. bei Bildschirmfotos: nur der eigentliche Ausschnitt ohne umgebenden Weißraum"));
    m_bgTrim->setChecked(s.value(QStringLiteral("tex/bgTrim"), true).toBool());
    bgf->addRow(QString(), m_bgBinding);
    bgf->addRow(QString(), m_bgTrim);
    bgl->addLayout(bgf, 1);
    m_bgPreview = new BackgroundPreview;
    m_bgPreview->setToolTip(tr("Vorschau (A4, rechte Seite): grau = Bindekorrektur, rot gestrichelt = Bild ragt über die Seite"));
    bgl->addWidget(m_bgPreview, 0, Qt::AlignTop);
    nf->addRow(m_bgOptions);
    // dasselbe Bild auch auf der Titelseite – zwischen Überschrift und Autor
    m_titleImgBox = new QWidget;
    auto *til = new QHBoxLayout(m_titleImgBox);
    til->setContentsMargins(0, 0, 0, 0);
    auto *tif = new QFormLayout;
    m_titleImg = new QCheckBox(tr("Bild auch auf der Titelseite (zwischen Überschrift und Autor)"));
    m_titleImg->setChecked(s.value(QStringLiteral("tex/titleImage"), true).toBool());
    tif->addRow(m_titleImg);
    m_tiWidth = mmBox(s.value(QStringLiteral("tex/titleImageWidth"), 100).toDouble(), 180);
    m_tiWidth->setMinimum(10);
    m_tiWidth->setToolTip(tr("Breite des Bildes auf der Titelseite – höchstens Satzspiegelbreite und 45 % der Satzspiegelhöhe"));
    m_tiAbove = mmBox(s.value(QStringLiteral("tex/titleImageAbove"), 15).toDouble(), 150);
    m_tiBelow = mmBox(s.value(QStringLiteral("tex/titleImageBelow"), 15).toDouble(), 150);
    m_tiAbove->setToolTip(tr("Abstand zwischen Überschrift (Titel bzw. Untertitel) und Bild"));
    m_tiBelow->setToolTip(tr("Abstand zwischen Bild und Autor"));
    tif->addRow(tr("Breite:"), m_tiWidth);
    tif->addRow(tr("Abstand zur Überschrift:"), m_tiAbove);
    tif->addRow(tr("Abstand zum Autor:"), m_tiBelow);
    til->addLayout(tif, 1);
    m_titlePreview = new TitlePagePreview;
    m_titlePreview->setToolTip(tr("Vorschau der Titelseite (rot gestrichelt: Inhalt reicht über den unteren Rand)"));
    til->addWidget(m_titlePreview, 0, Qt::AlignTop);
    nf->addRow(m_titleImgBox);
    auto *parts = new QHBoxLayout;
    m_toc = new QCheckBox(tr("Inhaltsverzeichnis"));
    m_glossary = new QCheckBox(tr("Glossar"));
    m_index = new QCheckBox(tr("Stichwortverzeichnis"));
    m_toc->setChecked(s.value(QStringLiteral("tex/toc"), true).toBool());
    m_glossary->setChecked(s.value(QStringLiteral("tex/glossary"), true).toBool());
    m_index->setChecked(s.value(QStringLiteral("tex/index"), true).toBool());
    parts->addWidget(m_toc);
    parts->addWidget(m_glossary);
    parts->addWidget(m_index);
    parts->addStretch(1);
    nf->addRow(tr("Verzeichnisse:"), parts);
    m_bcor = new QSpinBox;
    m_bcor->setRange(0, 30);
    m_bcor->setSuffix(tr(" mm"));
    m_bcor->setValue(s.value(QStringLiteral("tex/bcor"), 12).toInt());
    m_bcor->setToolTip(tr("Zusätzlicher Innenrand für die Bindung (Ringbindung: etwa 10–15 mm); zweiseitiger Satz"));
    nf->addRow(tr("Bindekorrektur:"), m_bcor);
    auto *nInfo = new QLabel(tr("<small>Zweiseitig mit Kopfzeile (Kapitel | Abschnitt) und Fußzeile (Seitenzahl), jeweils mit Trennlinie – "
                                "nicht auf Titelseiten und im Inhaltsverzeichnis. Enthält Pakete für Tabellen (booktabs, tabularx, longtable, "
                                "siunitx) und Grafiken (float, wrapfig, caption) und funktioniert mit pdfLaTeX, LuaLaTeX und XeLaTeX. "
                                "Übersetzen: <tt>latexmk -pdf kochbuch.tex</tt></small>"));
    nInfo->setWordWrap(true);
    nf->addRow(nInfo);
    lay->addWidget(nm);

    // ---- Inhalt des Rezepts
    auto *cb = new QGroupBox(tr("Rezept"));
    auto *cl = new QVBoxLayout(cb);
    m_image = new QCheckBox(tr("Rezeptfoto einbinden (wird als JPEG neben der .tex-Datei gespeichert)"));
    m_image->setChecked(!r.image.isEmpty());
    m_image->setEnabled(!r.image.isEmpty());
    m_nutrition = new QCheckBox(tr("Nährwerttabelle"));
    m_nutrition->setChecked(s.value(QStringLiteral("tex/nutrition"), true).toBool());
    m_source = new QCheckBox(tr("Quelle angeben"));
    m_source->setChecked(s.value(QStringLiteral("tex/source"), false).toBool());
    m_indexEntries = new QCheckBox(tr("Stichwörter für das Stichwortverzeichnis (Titel und Zutaten)"));
    m_indexEntries->setChecked(s.value(QStringLiteral("tex/indexEntries"), true).toBool());
    auto *hf = new QFormLayout;
    m_heading = new QComboBox;
    m_heading->addItem(tr("eigenes Kapitel (\\chapter)"), int(LatexExport::Heading::Chapter));
    m_heading->addItem(tr("Abschnitt in einem Themen-Kapitel (\\section)"), int(LatexExport::Heading::Section));
    m_heading->setCurrentIndex(qMax(0, m_heading->findData(s.value(QStringLiteral("tex/heading"), 0).toInt())));
    m_heading->setToolTip(tr("Als Abschnitt lassen sich Rezepte in Kapiteln wie „Vorspeisen“, „Hauptgerichte“, „Süßspeisen“ sammeln"));
    hf->addRow(tr("Einbinden als:"), m_heading);
    m_chapterRow = new QWidget;
    auto *chl = new QHBoxLayout(m_chapterRow);
    chl->setContentsMargins(0, 0, 0, 0);
    m_chapter = new QComboBox;
    m_chapter->setEditable(true);
    m_chapter->setInsertPolicy(QComboBox::NoInsert);
    m_chapter->setToolTip(tr("Kapitel des Master-Dokuments; ein neuer Name legt das Kapitel an"));
    m_newPage = new QCheckBox(tr("auf neuer Seite"));
    m_newPage->setChecked(s.value(QStringLiteral("tex/newPage"), true).toBool());
    chl->addWidget(m_chapter, 1);
    chl->addWidget(m_newPage);
    hf->addRow(tr("Themen-Kapitel:"), m_chapterRow);
    m_chapterLabel = hf->labelForField(m_chapterRow);
    cl->addLayout(hf);
    for (QCheckBox *c : { m_image, m_nutrition, m_source, m_indexEntries }) cl->addWidget(c);
    lay->addWidget(cb);

    auto *ff = new QFormLayout;
    auto *fl = new QHBoxLayout;
    const QString dir = s.value(QStringLiteral("tex/dir"), docs).toString();
    // Dateiname nur aus ASCII-Zeichen – LaTeX unter Windows verträgt Umlaute in \\input-Pfaden schlecht
    QString base = r.title;
    base.replace(QStringLiteral("ä"), QStringLiteral("ae")).replace(QStringLiteral("ö"), QStringLiteral("oe")).replace(QStringLiteral("ü"), QStringLiteral("ue"))
        .replace(QStringLiteral("Ä"), QStringLiteral("Ae")).replace(QStringLiteral("Ö"), QStringLiteral("Oe")).replace(QStringLiteral("Ü"), QStringLiteral("Ue"))
        .replace(QStringLiteral("ß"), QStringLiteral("ss"));
    base = base.normalized(QString::NormalizationForm_KD);
    base.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9]+")), QStringLiteral("_"));
    base = base.left(60);
    while (base.endsWith(QLatin1Char('_'))) base.chop(1);
    if (base.isEmpty()) base = QStringLiteral("rezept");
    m_base = base;
    m_file = new QLineEdit(QDir::toNativeSeparators(QDir(dir).filePath(base + QStringLiteral(".tex"))));
    m_file->setToolTip(tr("Mit Master-Dokument: Vorschlag im Unterordner „rezepte“ neben dem Master-Dokument"));
    connect(m_file, &QLineEdit::textEdited, this, [this] { m_fileEdited = true; });
    auto *browse = new QPushButton(Icons::get(QStringLiteral("open")), QString());
    fl->addWidget(m_file, 1);
    fl->addWidget(browse);
    ff->addRow(tr("Rezeptdatei:"), fl);
    lay->addLayout(ff);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Exportieren"));
    outer->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &LatexExportDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(browse, &QPushButton::clicked, this, [this] {
        if (!pickFile(m_file, tr("LaTeX-Datei speichern"), tr("LaTeX (*.tex)"), true).isEmpty()) m_fileEdited = true;
    });
    connect(m_masterFile, &QLineEdit::textChanged, this, &LatexExportDialog::proposeFile);
    connect(m_newMasterFile, &QLineEdit::textChanged, this, &LatexExportDialog::proposeFile);
    connect(masterBrowse, &QPushButton::clicked, this, [this] {
        if (!pickFile(m_masterFile, tr("Master-Dokument wählen"), tr("LaTeX (*.tex)"), false).isEmpty()) m_register->setChecked(true);
    });
    connect(newBrowse, &QPushButton::clicked, this, [this] { pickFile(m_newMasterFile, tr("Neues Master-Dokument"), tr("LaTeX (*.tex)"), true); });
    connect(bgBrowse, &QPushButton::clicked, this, [this] {
        pickFile(m_bgImage, tr("Hintergrundbild für Schmutztitel oder Titelseite"), tr("Bilder (*.png *.jpg *.jpeg *.bmp *.webp)"), false);
    });
    connect(m_bgImage, &QLineEdit::textChanged, this, &LatexExportDialog::updateBackground);
    for (QComboBox *c : { m_bgTarget, m_bgPlacement })
        connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LatexExportDialog::updateBackground);
    for (QDoubleSpinBox *b : { m_bgWidth, m_bgLeft, m_bgBottom })
        connect(b, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &LatexExportDialog::updateBackground);
    for (QCheckBox *c : { m_bgBinding, m_bgTrim }) connect(c, &QCheckBox::toggled, this, &LatexExportDialog::updateBackground);
    connect(m_bcor, QOverload<int>::of(&QSpinBox::valueChanged), this, &LatexExportDialog::updateBackground);
    connect(m_bookTitle, &QLineEdit::textChanged, this, &LatexExportDialog::updateBackground);
    connect(m_subtitle, &QLineEdit::textChanged, this, &LatexExportDialog::updateBackground);
    connect(m_bgCropButton, &QPushButton::clicked, this, &LatexExportDialog::cropBackground);
    connect(m_titleImg, &QCheckBox::toggled, this, &LatexExportDialog::updateBackground);
    for (QDoubleSpinBox *b : { m_tiWidth, m_tiAbove, m_tiBelow })
        connect(b, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &LatexExportDialog::updateBackground);
    connect(m_author, &QLineEdit::textChanged, this, &LatexExportDialog::updateBackground);
    connect(m_bgImage, &QLineEdit::textEdited, this, [this] {
        m_bgCrop = QRectF();
        m_bgRotation = 0;
    });
    connect(m_heading, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LatexExportDialog::updateMode);
    connect(m_masterFile, &QLineEdit::editingFinished, this, &LatexExportDialog::fillChapters);
    connect(m_register, &QCheckBox::toggled, this, &LatexExportDialog::fillChapters);
    for (QRadioButton *b : { m_modeChapter, m_modeNewMaster }) connect(b, &QRadioButton::toggled, this, &LatexExportDialog::fillChapters);
    fillChapters();
    updateBackground();
    for (QRadioButton *b : { m_modeChapter, m_modeNewMaster, m_modeStandalone }) connect(b, &QRadioButton::toggled, this, &LatexExportDialog::updateMode);
    connect(m_register, &QCheckBox::toggled, this, &LatexExportDialog::updateMode);
    updateMode();
    // Vorgabegröße: so hoch wie nötig, höchstens 90 % der Bildschirmhöhe; danach merkt WindowState Größe und Position
    lay->addStretch(1);
    const QRect avail = (parent && parent->screen() ? parent->screen() : QGuiApplication::primaryScreen())->availableGeometry();
    const int wanted = content->sizeHint().height() + bb->sizeHint().height() + 40;
    resize(qMin(700, avail.width() - 40), qMin(wanted, int(avail.height() * 0.9)));
    setMinimumSize(480, 320);
}

QString LatexExportDialog::pickFile(QLineEdit *edit, const QString &title, const QString &filter, bool save)
{
    const QString start = edit->text().isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) : edit->text();
    const QString f = save ? QFileDialog::getSaveFileName(this, title, start, filter) : QFileDialog::getOpenFileName(this, title, start, filter);
    if (!f.isEmpty()) edit->setText(QDir::toNativeSeparators(f));
    return f;
}

LatexExport::MasterOptions LatexExportDialog::masterOptions() const
{
    LatexExport::MasterOptions m;
    m.docClass = LatexExport::DocClass(m_class->currentData().toInt());
    m.title = m_bookTitle->text().trimmed();
    m.subtitle = m_subtitle->text().trimmed();
    m.author = m_author->text().trimmed();
    m.noDate = m_noDate->isChecked();
    m.titleColor = m_titleColor->property("color").toString();
    m.subtitleColor = m_subtitleColor->property("color").toString();
    m.titleShadow = m_titleShadow->isChecked();
    m.halfTitleImage = QDir::fromNativeSeparators(m_bgImage->text().trimmed());
    m.bgTarget = LatexExport::MasterOptions::BgTarget(m_bgTarget->currentData().toInt());
    m.bgPlacement = LatexExport::MasterOptions::BgPlacement(m_bgPlacement->currentData().toInt());
    m.bgWidthMm = m_bgWidth->value();
    m.bgLeftMm = m_bgLeft->value();
    m.bgBottomMm = m_bgBottom->value();
    m.bgShiftBinding = m_bgBinding->isChecked();
    m.bgTrim = m_bgTrim->isChecked();
    m.bgCrop = m_bgCrop;
    m.titleImage = m_titleImg->isChecked();
    m.titleImageWidthMm = m_tiWidth->value();
    m.titleImageGapAboveMm = m_tiAbove->value();
    m.titleImageGapBelowMm = m_tiBelow->value();
    m.bgRotation = m_bgRotation;
    m.toc = m_toc->isChecked();
    m.glossary = m_glossary->isChecked();
    m.index = m_index->isChecked();
    m.bindingMm = m_bcor->value();
    return m;
}

void LatexExportDialog::cropBackground()
{
    const QString file = QDir::fromNativeSeparators(m_bgImage->text().trimmed());
    QImage img(file);
    if (img.isNull()) {
        QMessageBox::information(this, windowTitle(), tr("Bitte zuerst ein Hintergrundbild wählen."));
        return;
    }
    ImageCropDialog d(img, this, ImageCropDialog::Purpose::PrintBackground);
    d.setInitial(m_bgRotation, m_bgCrop);
    if (d.exec() != QDialog::Accepted) return;
    m_bgCrop = d.normalizedSelection();
    m_bgRotation = d.rotation();
    updateBackground();
}

void LatexExportDialog::fillChapters()
{
    const QString current = m_chapter->currentText().trimmed();
    QStringList names;
    QString masterText;
    if (m_modeChapter->isChecked() && m_register->isChecked()) {
        QFile f(QDir::fromNativeSeparators(m_masterFile->text().trimmed()));
        if (f.open(QIODevice::ReadOnly)) masterText = QString::fromUtf8(f.readAll());
    }
    const QStringList existing = LatexExport::masterChapters(masterText);
    names << existing;
    for (const QString &d : LatexExport::defaultChapters())
        if (!names.contains(d, Qt::CaseInsensitive)) names << d;
    m_chapter->clear();
    m_chapter->addItems(names);
    for (int i = 0; i < existing.size(); ++i) m_chapter->setItemData(i, tr("im Master-Dokument vorhanden"), Qt::ToolTipRole);
    QString pick = current.isEmpty() ? LatexExport::suggestChapter(m_recipe) : current;
    for (const QString &n : names)
        if (n.compare(pick, Qt::CaseInsensitive) == 0) pick = n;
    m_chapter->setCurrentText(pick);
}

void LatexExportDialog::updateBackground()
{
    if (m_updatingBg) return;
    m_updatingBg = true;
    const LatexExport::MasterOptions m = masterOptions();
    const bool has = !m.halfTitleImage.isEmpty();
    m_bgOptions->setVisible(has);
    const bool fill = m.bgPlacement == LatexExport::MasterOptions::BgPlacement::FillPage;
    const bool free = m.bgPlacement == LatexExport::MasterOptions::BgPlacement::Free;
    m_bgWidth->setEnabled(!fill);
    m_bgBottom->setEnabled(!fill);
    m_bgLeft->setEnabled(free);
    m_bgBinding->setEnabled(!fill);
    QImage img;
    if (has) {
        if (m.halfTitleImage != m_bgCachePath) {
            m_bgCachePath = m.halfTitleImage;
            m_bgCache = QImage(m.halfTitleImage);
            if (!m_bgCache.isNull() && m_bgCache.width() > 800) m_bgCache = m_bgCache.scaledToWidth(800, Qt::SmoothTransformation);
        }
        if (!m_bgCache.isNull()) img = LatexExport::prepareBackground(m_bgCache, m);
    }
    m_bgScale->setEnabled(!fill);
    m_bgCropButton->setEnabled(has);
    m_bgCropButton->setText(m_bgCrop.isEmpty() && m_bgRotation == 0 ? tr("Zuschneiden …") : tr("Zuschnitt ändern …"));
    // Breite so begrenzen, dass das Bild nicht über die A4-Seite hinausragt
    LatexExport::MasterOptions mm = m;
    if (!fill && !img.isNull()) {
        const double maxW = LatexExport::maxBackgroundWidth(mm, double(img.height()) / qMax(1, img.width()));
        m_bgWidth->setMaximum(maxW);
        m_bgScale->setRange(100, qMax(101, int(maxW * 10)));
        m_bgScale->setValue(int(m_bgWidth->value() * 10));
        m_bgMaxInfo->setText(tr("<small>höchstens %1 mm breit · Bild %2 × %3 mm</small>")
                                 .arg(QLocale(QLocale::German).toString(maxW, 'f', 1))
                                 .arg(QLocale(QLocale::German).toString(m_bgWidth->value(), 'f', 1))
                                 .arg(QLocale(QLocale::German).toString(m_bgWidth->value() * img.height() / qMax(1, img.width()), 'f', 1)));
        m_bgLeft->setMaximum(qMax(0.0, 210 - (m.bgShiftBinding ? m.bindingMm : 0) - 10));
        mm = masterOptions();   // nach der Begrenzung
    } else {
        m_bgMaxInfo->clear();
    }
    m_bgMaxInfo->setVisible(!fill);
    m_bgPreview->set(img, mm);
    // Titelseite: nur, wenn das Bild auf dem Schmutztitel liegt (sonst ist es schon Hintergrund der Titelseite)
    const bool onHalf = m.bgTarget == LatexExport::MasterOptions::BgTarget::HalfTitle;
    m_titleImgBox->setVisible(has && onHalf);
    for (QWidget *wdg : { static_cast<QWidget *>(m_tiWidth), static_cast<QWidget *>(m_tiAbove), static_cast<QWidget *>(m_tiBelow) })
        wdg->setEnabled(m_titleImg->isChecked());
    if (has && onHalf) {
        LatexExport::MasterOptions ti = m;
        ti.bgPlacement = LatexExport::MasterOptions::BgPlacement::Free;
        m_titlePreview->set(m_titleImg->isChecked() && !m_bgCache.isNull() ? LatexExport::prepareBackground(m_bgCache, ti) : QImage(), m);
    }
    m_updatingBg = false;
}

void LatexExportDialog::proposeFile()
{
    if (m_fileEdited) return;
    QString master;
    if (m_modeNewMaster->isChecked()) master = m_newMasterFile->text().trimmed();
    else if (m_modeChapter->isChecked() && m_register->isChecked()) master = m_masterFile->text().trimmed();
    if (master.isEmpty()) return;
    const QString dir = QFileInfo(QDir::fromNativeSeparators(master)).absolutePath() + QStringLiteral("/rezepte");
    m_file->setText(QDir::toNativeSeparators(dir + QLatin1Char('/') + m_base + QStringLiteral(".tex")));
}

// Wird der Inhalt höher (z. B. „Neues Master-Dokument“), das Fenster bis höchstens 90 % der Bildschirmhöhe
// vergrößern – nie über den Bildschirm hinaus; der Rest bleibt rollbar
void LatexExportDialog::growToFit()
{
    if (!isVisible() || isMaximized() || !m_scroll) return;
    QWidget *content = m_scroll->widget();
    content->adjustSize();
    const int need = content->sizeHint().height() - m_scroll->viewport()->height();
    if (need <= 0) return;
    const QRect avail = screen() ? screen()->availableGeometry() : QGuiApplication::primaryScreen()->availableGeometry();
    const int maxH = int(avail.height() * 0.9);
    const int h = qMin(height() + need, maxH);
    if (h <= height()) return;
    QRect g = geometry();
    g.setHeight(h);
    if (g.bottom() > avail.bottom()) g.moveBottom(avail.bottom());
    if (g.top() < avail.top()) g.moveTop(avail.top());
    setGeometry(g);
}

void LatexExportDialog::updateMode()
{
    proposeFile();
    m_registerRow->setEnabled(m_modeChapter->isChecked());
    m_masterFile->setEnabled(m_register->isChecked());
    m_newMasterBox->setVisible(m_modeNewMaster->isChecked());
    m_indexEntries->setEnabled(!m_modeStandalone->isChecked());
    m_heading->setEnabled(!m_modeStandalone->isChecked());
    const bool sectionRow = !m_modeStandalone->isChecked() && m_heading->currentData().toInt() == int(LatexExport::Heading::Section);
    m_chapterRow->setVisible(sectionRow);
    if (m_chapterLabel) m_chapterLabel->setVisible(sectionRow);
    growToFit();
}

void LatexExportDialog::accept()
{
    QString file = QDir::fromNativeSeparators(m_file->text().trimmed());
    if (file.isEmpty()) return;
    if (!file.endsWith(QLatin1String(".tex"), Qt::CaseInsensitive)) file += QStringLiteral(".tex");
    LatexExport::Options opt;
    opt.docClass = LatexExport::DocClass(m_class->currentData().toInt());
    opt.mode = m_modeStandalone->isChecked() ? LatexExport::Mode::Standalone : LatexExport::Mode::Chapter;
    opt.image = m_image->isChecked();
    opt.nutrition = m_nutrition->isChecked();
    opt.source = m_source->isChecked();
    opt.indexEntries = m_indexEntries->isChecked() && !m_modeStandalone->isChecked();
    opt.heading = LatexExport::Heading(m_heading->currentData().toInt());
    opt.newPage = m_newPage->isChecked();
    const bool asSection = opt.heading == LatexExport::Heading::Section && !m_modeStandalone->isChecked();
    const QString chapter = asSection ? m_chapter->currentText().trimmed() : QString();
    if (asSection && chapter.isEmpty()) {
        QMessageBox::information(this, windowTitle(), tr("Bitte ein Themen-Kapitel angeben (z. B. „Hauptgerichte“)."));
        return;
    }

    // Master-Dokument bestimmen
    QString master;
    if (m_modeChapter->isChecked() && m_register->isChecked()) {
        master = QDir::fromNativeSeparators(m_masterFile->text().trimmed());
        if (master.isEmpty() || !QFileInfo::exists(master)) {
            QMessageBox::warning(this, windowTitle(), tr("Bitte ein bestehendes Master-Dokument wählen – oder „eintragen in“ abschalten."));
            return;
        }
    } else if (m_modeNewMaster->isChecked()) {
        master = QDir::fromNativeSeparators(m_newMasterFile->text().trimmed());
        if (master.isEmpty()) return;
        if (!master.endsWith(QLatin1String(".tex"), Qt::CaseInsensitive)) master += QStringLiteral(".tex");
        if (m_bookTitle->text().trimmed().isEmpty()) {
            QMessageBox::information(this, windowTitle(), tr("Bitte einen Buchtitel eingeben."));
            return;
        }
        if (QFileInfo::exists(master)
            && QMessageBox::question(this, windowTitle(), tr("%1 gibt es schon. Überschreiben?").arg(QDir::toNativeSeparators(master)))
                   != QMessageBox::Yes)
            return;
        QDir().mkpath(QFileInfo(master).absolutePath());
    }
    QString inputPath;
    if (!master.isEmpty()) {
        inputPath = LatexExport::relativeInput(master, file);
        // Foto relativ zum Master-Dokument (LaTeX sucht Bilder vom Master aus)
        const QString dirRel = QFileInfo(inputPath).path();
        opt.imagePrefix = (dirRel == QLatin1String(".") || dirRel.isEmpty()) ? QString() : dirRel + QLatin1Char('/');
        if (inputPath.contains(QLatin1Char(' ')))
            QMessageBox::information(this, windowTitle(), tr("Hinweis: Der Pfad enthält Leerzeichen – manche LaTeX-Installationen haben damit Probleme."));
    }
    QDir().mkpath(QFileInfo(file).absolutePath());

    QSettings s;
    s.setValue(QStringLiteral("tex/class"), m_class->currentIndex());
    s.setValue(QStringLiteral("tex/mode"), m_modeNewMaster->isChecked() ? 1 : m_modeStandalone->isChecked() ? 2 : 0);
    s.setValue(QStringLiteral("tex/register"), m_register->isChecked());
    s.setValue(QStringLiteral("tex/nutrition"), opt.nutrition);
    s.setValue(QStringLiteral("tex/source"), opt.source);
    s.setValue(QStringLiteral("tex/indexEntries"), m_indexEntries->isChecked());
    s.setValue(QStringLiteral("tex/heading"), int(opt.heading));
    s.setValue(QStringLiteral("tex/newPage"), opt.newPage);
    s.setValue(QStringLiteral("tex/dir"), QFileInfo(file).absolutePath());
    QString err;
    if (!LatexExport::exportRecipe(m_recipe, file, opt, &err)) {
        QMessageBox::warning(this, windowTitle(), tr("Export fehlgeschlagen: %1").arg(err));
        return;
    }
    QString msg = tr("Gespeichert: %1").arg(QDir::toNativeSeparators(file));
    if (m_modeNewMaster->isChecked()) {
        LatexExport::MasterOptions m = masterOptions();
        m.firstChapter = chapter;
        if (!LatexExport::writeMaster(master, m, { inputPath }, &err)) {
            QMessageBox::warning(this, windowTitle(), tr("Master-Dokument konnte nicht geschrieben werden: %1").arg(err));
            return;
        }
        s.setValue(QStringLiteral("tex/newMaster"), master);
        s.setValue(QStringLiteral("tex/master"), master);   // künftige Rezepte hier eintragen
        s.setValue(QStringLiteral("tex/register"), true);
        s.setValue(QStringLiteral("tex/mode"), 0);
        s.setValue(QStringLiteral("tex/bookTitle"), m.title);
        s.setValue(QStringLiteral("tex/subtitle"), m.subtitle);
        s.setValue(QStringLiteral("tex/author"), m.author);
        s.setValue(QStringLiteral("tex/noDate"), m.noDate);
        s.setValue(QStringLiteral("tex/titleColor"), m.titleColor);
        s.setValue(QStringLiteral("tex/subtitleColor"), m.subtitleColor);
        s.setValue(QStringLiteral("tex/titleShadow"), m.titleShadow);
        s.setValue(QStringLiteral("tex/bgImage"), m.halfTitleImage);
        s.setValue(QStringLiteral("tex/bgTarget"), int(m.bgTarget));
        s.setValue(QStringLiteral("tex/bgPlacement"), int(m.bgPlacement));
        s.setValue(QStringLiteral("tex/bgWidth"), m.bgWidthMm);
        s.setValue(QStringLiteral("tex/bgLeft"), m.bgLeftMm);
        s.setValue(QStringLiteral("tex/bgBottom"), m.bgBottomMm);
        s.setValue(QStringLiteral("tex/bgBinding"), m.bgShiftBinding);
        s.setValue(QStringLiteral("tex/bgTrim"), m.bgTrim);
        s.setValue(QStringLiteral("tex/bgCrop"), m.bgCrop);
        s.setValue(QStringLiteral("tex/titleImage"), m.titleImage);
        s.setValue(QStringLiteral("tex/titleImageWidth"), m.titleImageWidthMm);
        s.setValue(QStringLiteral("tex/titleImageAbove"), m.titleImageGapAboveMm);
        s.setValue(QStringLiteral("tex/titleImageBelow"), m.titleImageGapBelowMm);
        s.setValue(QStringLiteral("tex/bgRotation"), m.bgRotation);
        s.setValue(QStringLiteral("tex/toc"), m.toc);
        s.setValue(QStringLiteral("tex/glossary"), m.glossary);
        s.setValue(QStringLiteral("tex/index"), m.index);
        s.setValue(QStringLiteral("tex/bcor"), m_bcor->value());
        msg += QStringLiteral("\n") + tr("Master-Dokument: %1\nÜbersetzen mit: latexmk -pdf %2\n\nWeitere Rezepte werden beim nächsten Export automatisch in dieses "
                                         "Master-Dokument eingetragen.")
                                          .arg(QDir::toNativeSeparators(master), QFileInfo(master).fileName());
    } else if (!master.isEmpty()) {
        bool already = false, created = false;
        if (!LatexExport::addToMaster(master, inputPath, &err, &already, chapter, &created)) {
            QMessageBox::warning(this, windowTitle(), tr("Das Rezept wurde gespeichert, aber nicht ins Master-Dokument eingetragen:\n%1").arg(err));
            return;
        }
        s.setValue(QStringLiteral("tex/master"), master);
        msg += QStringLiteral("\n") + (already ? tr("Im Master-Dokument war es bereits eingetragen.")
                                               : tr("Eingetragen in %1 als \\input{%2}%3.")
                                                     .arg(QDir::toNativeSeparators(master), inputPath,
                                                          chapter.isEmpty() ? QString()
                                                                            : (created ? tr(" im neuen Kapitel „%1“") : tr(" im Kapitel „%1“")).arg(chapter)));
    }
    QMessageBox::information(this, windowTitle(), msg);
    QDialog::accept();
}
