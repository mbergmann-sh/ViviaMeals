// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "exportdialogs.h"
#include "icons.h"
#include "latexexport.h"
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

LatexExportDialog::LatexExportDialog(const Recipe &r, QWidget *parent) : QDialog(parent), m_recipe(r)
{
    setWindowTitle(tr("Als LaTeX-Datei exportieren"));
    QSettings s;
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    auto *lay = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    form->addRow(tr("Rezept:"), new QLabel(QStringLiteral("<b>%1</b>").arg(r.title.toHtmlEscaped())));
    m_class = new QComboBox;
    m_class->addItem(QStringLiteral("scrbook (KOMA-Script)"), int(LatexExport::DocClass::ScrBook));
    m_class->addItem(QStringLiteral("book (LaTeX-Standard)"), int(LatexExport::DocClass::Book));
    m_class->setCurrentIndex(s.value(QStringLiteral("tex/class"), 0).toInt());
    form->addRow(tr("Dokumentklasse:"), m_class);
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
    m_author = new QLineEdit(s.value(QStringLiteral("tex/author"), s.value(QStringLiteral("editor/defaultSource"))).toString());
    nf->addRow(tr("Autor:"), m_author);
    m_noDate = new QCheckBox(tr("kein Datum auf dem Titelblatt"));
    m_noDate->setChecked(s.value(QStringLiteral("tex/noDate"), true).toBool());
    nf->addRow(QString(), m_noDate);
    auto *bgr = new QHBoxLayout;
    m_bgImage = new QLineEdit(QDir::toNativeSeparators(s.value(QStringLiteral("tex/bgImage")).toString()));
    m_bgImage->setPlaceholderText(tr("optional – Bild für den Schmutztitel (füllt die ganze Seite)"));
    auto *bgBrowse = new QPushButton(Icons::get(QStringLiteral("open")), QString());
    bgr->addWidget(m_bgImage, 1);
    bgr->addWidget(bgBrowse);
    nf->addRow(tr("Schmutztitel-Bild:"), bgr);
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
    lay->addWidget(bb);
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
        pickFile(m_bgImage, tr("Hintergrundbild für den Schmutztitel"), tr("Bilder (*.png *.jpg *.jpeg *.bmp *.webp)"), false);
    });
    for (QRadioButton *b : { m_modeChapter, m_modeNewMaster, m_modeStandalone }) connect(b, &QRadioButton::toggled, this, &LatexExportDialog::updateMode);
    connect(m_register, &QCheckBox::toggled, this, &LatexExportDialog::updateMode);
    updateMode();
    resize(660, sizeHint().height());
}

QString LatexExportDialog::pickFile(QLineEdit *edit, const QString &title, const QString &filter, bool save)
{
    const QString start = edit->text().isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) : edit->text();
    const QString f = save ? QFileDialog::getSaveFileName(this, title, start, filter) : QFileDialog::getOpenFileName(this, title, start, filter);
    if (!f.isEmpty()) edit->setText(QDir::toNativeSeparators(f));
    return f;
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

void LatexExportDialog::updateMode()
{
    proposeFile();
    m_registerRow->setEnabled(m_modeChapter->isChecked());
    m_masterFile->setEnabled(m_register->isChecked());
    m_newMasterBox->setVisible(m_modeNewMaster->isChecked());
    m_indexEntries->setEnabled(!m_modeStandalone->isChecked());
    adjustSize();
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
    s.setValue(QStringLiteral("tex/dir"), QFileInfo(file).absolutePath());
    QString err;
    if (!LatexExport::exportRecipe(m_recipe, file, opt, &err)) {
        QMessageBox::warning(this, windowTitle(), tr("Export fehlgeschlagen: %1").arg(err));
        return;
    }
    QString msg = tr("Gespeichert: %1").arg(QDir::toNativeSeparators(file));
    if (m_modeNewMaster->isChecked()) {
        LatexExport::MasterOptions m;
        m.docClass = opt.docClass;
        m.title = m_bookTitle->text().trimmed();
        m.subtitle = m_subtitle->text().trimmed();
        m.author = m_author->text().trimmed();
        m.noDate = m_noDate->isChecked();
        m.halfTitleImage = QDir::fromNativeSeparators(m_bgImage->text().trimmed());
        m.toc = m_toc->isChecked();
        m.glossary = m_glossary->isChecked();
        m.index = m_index->isChecked();
        m.bindingMm = m_bcor->value();
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
        s.setValue(QStringLiteral("tex/bgImage"), m.halfTitleImage);
        s.setValue(QStringLiteral("tex/toc"), m.toc);
        s.setValue(QStringLiteral("tex/glossary"), m.glossary);
        s.setValue(QStringLiteral("tex/index"), m.index);
        s.setValue(QStringLiteral("tex/bcor"), m_bcor->value());
        msg += QStringLiteral("\n") + tr("Master-Dokument: %1\nÜbersetzen mit: latexmk -pdf %2\n\nWeitere Rezepte werden beim nächsten Export automatisch in dieses "
                                         "Master-Dokument eingetragen.")
                                          .arg(QDir::toNativeSeparators(master), QFileInfo(master).fileName());
    } else if (!master.isEmpty()) {
        bool already = false;
        if (!LatexExport::addToMaster(master, inputPath, &err, &already)) {
            QMessageBox::warning(this, windowTitle(), tr("Das Rezept wurde gespeichert, aber nicht ins Master-Dokument eingetragen:\n%1").arg(err));
            return;
        }
        s.setValue(QStringLiteral("tex/master"), master);
        msg += QStringLiteral("\n") + (already ? tr("Im Master-Dokument war es bereits eingetragen.")
                                               : tr("Eingetragen in %1 als \\input{%2}.").arg(QDir::toNativeSeparators(master), inputPath));
    }
    QMessageBox::information(this, windowTitle(), msg);
    QDialog::accept();
}
