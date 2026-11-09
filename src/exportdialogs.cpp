// SPDX-License-Identifier: MIT AND LicenseRef-Commons-Clause-1.0 (siehe LICENSE)
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
                                     QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(QStringLiteral("VitaMeals-Website")))
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
    auto *lay = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    form->addRow(tr("Rezept:"), new QLabel(QStringLiteral("<b>%1</b>").arg(r.title.toHtmlEscaped())));
    m_class = new QComboBox;
    m_class->addItem(QStringLiteral("scrbook (KOMA-Script)"), int(LatexExport::DocClass::ScrBook));
    m_class->addItem(QStringLiteral("book (LaTeX-Standard)"), int(LatexExport::DocClass::Book));
    m_class->setCurrentIndex(s.value(QStringLiteral("tex/class"), 0).toInt());
    form->addRow(tr("Dokumentklasse:"), m_class);
    m_standalone = new QCheckBox(tr("Vollständiges Dokument (sonst nur das Kapitel zum Einbinden mit \\input)"));
    m_standalone->setChecked(s.value(QStringLiteral("tex/standalone"), true).toBool());
    form->addRow(QString(), m_standalone);
    m_image = new QCheckBox(tr("Rezeptfoto einbinden (wird als JPEG neben der .tex-Datei gespeichert)"));
    m_image->setChecked(!r.image.isEmpty());
    m_image->setEnabled(!r.image.isEmpty());
    form->addRow(QString(), m_image);
    m_nutrition = new QCheckBox(tr("Nährwerttabelle"));
    m_nutrition->setChecked(s.value(QStringLiteral("tex/nutrition"), true).toBool());
    form->addRow(QString(), m_nutrition);
    auto *fl = new QHBoxLayout;
    const QString dir = s.value(QStringLiteral("tex/dir"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    QString base = r.title;
    base.replace(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]+")), QStringLiteral("_"));
    m_file = new QLineEdit(QDir::toNativeSeparators(QDir(dir).filePath(base + QStringLiteral(".tex"))));
    auto *browse = new QPushButton(Icons::get(QStringLiteral("open")), QString());
    fl->addWidget(m_file, 1);
    fl->addWidget(browse);
    form->addRow(tr("Datei:"), fl);
    lay->addLayout(form);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Exportieren"));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &LatexExportDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getSaveFileName(this, tr("LaTeX-Datei speichern"), m_file->text(), tr("LaTeX (*.tex)"));
        if (!f.isEmpty()) m_file->setText(QDir::toNativeSeparators(f));
    });
    resize(620, sizeHint().height());
}

void LatexExportDialog::accept()
{
    QString file = QDir::fromNativeSeparators(m_file->text().trimmed());
    if (file.isEmpty()) return;
    if (!file.endsWith(QLatin1String(".tex"), Qt::CaseInsensitive)) file += QStringLiteral(".tex");
    LatexExport::Options opt;
    opt.docClass = LatexExport::DocClass(m_class->currentData().toInt());
    opt.standalone = m_standalone->isChecked();
    opt.image = m_image->isChecked();
    opt.nutrition = m_nutrition->isChecked();
    QSettings s;
    s.setValue(QStringLiteral("tex/class"), m_class->currentIndex());
    s.setValue(QStringLiteral("tex/standalone"), opt.standalone);
    s.setValue(QStringLiteral("tex/nutrition"), opt.nutrition);
    s.setValue(QStringLiteral("tex/dir"), QFileInfo(file).absolutePath());
    QString err;
    if (!LatexExport::exportRecipe(m_recipe, file, opt, &err)) {
        QMessageBox::warning(this, windowTitle(), tr("Export fehlgeschlagen: %1").arg(err));
        return;
    }
    QMessageBox::information(this, windowTitle(), tr("Gespeichert: %1").arg(QDir::toNativeSeparators(file)));
    QDialog::accept();
}
