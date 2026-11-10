// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "packagedialog.h"
#include "icons.h"
#include "recipedb.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

// ================================================================ Export

PackageExportDialog::PackageExportDialog(const QStringList &visibleIds, QWidget *parent) : QDialog(parent), m_visible(visibleIds)
{
    setWindowTitle(tr("Rezeptsammlung weitergeben"));
    const RecipeDb &db = RecipeDb::instance();
    QSettings s;
    auto *lay = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Erstellt ein <b>Rezeptpaket (.vmpack)</b>, das andere ViviaMeals-Anwender über "
                                "<i>Datei → Import → Rezeptpaket</i> in ihre Datenbank übernehmen oder als eigene Datenbank öffnen können."));
    intro->setWordWrap(true);
    lay->addWidget(intro);

    auto *form = new QFormLayout;
    m_name = new QLineEdit(db.name());
    form->addRow(tr("&Name der Sammlung:"), m_name);
    m_author = new QLineEdit(s.value(QStringLiteral("package/author")).toString());
    m_author->setPlaceholderText(tr("optional"));
    form->addRow(tr("&Zusammengestellt von:"), m_author);
    m_desc = new QPlainTextEdit;
    m_desc->setPlaceholderText(tr("optional, z. B. „Unsere Familienrezepte“"));
    m_desc->setMaximumHeight(m_desc->fontMetrics().height() * 4);
    form->addRow(tr("&Beschreibung:"), m_desc);
    lay->addLayout(form);

    auto *scope = new QGroupBox(tr("Welche Rezepte?"));
    auto *sl = new QFormLayout(scope);
    int favs = 0;
    for (const RecipeSummary &r : db.summaries())
        if (r.favorite) ++favs;
    m_all = new QRadioButton(tr("&alle Rezepte der Datenbank (%1)").arg(db.count()));
    m_shown = new QRadioButton(tr("die &angezeigte Liste (%1)").arg(visibleIds.size()));
    m_favs = new QRadioButton(tr("nur &Favoriten (%1)").arg(favs));
    m_tagged = new QRadioButton(tr("mit dem &Tag"));
    m_tag = new QComboBox;
    m_tag->addItems(db.allTags());
    m_all->setChecked(true);
    m_shown->setEnabled(!visibleIds.isEmpty() && visibleIds.size() != db.count());
    m_favs->setEnabled(favs > 0);
    m_tagged->setEnabled(m_tag->count() > 0);
    auto *group = new QButtonGroup(this);
    for (QRadioButton *b : { m_all, m_shown, m_favs, m_tagged }) group->addButton(b);
    sl->addRow(m_all);
    sl->addRow(m_shown);
    sl->addRow(m_favs);
    sl->addRow(m_tagged, m_tag);
    lay->addWidget(scope);

    auto *opt = new QGroupBox(tr("Mitgeben"));
    auto *ol = new QVBoxLayout(opt);
    m_images = new QCheckBox(tr("Rezeptfotos"));
    m_images->setChecked(s.value(QStringLiteral("package/images"), true).toBool());
    m_foods = new QCheckBox(tr("eigene Lebensmittel und Zuordnungen, die in diesen Rezepten vorkommen (damit die Nährwerte gleich bleiben)"));
    m_foods->setChecked(s.value(QStringLiteral("package/foods"), true).toBool());
    m_keepFav = new QCheckBox(tr("Favoriten-Markierungen"));
    m_keepFav->setChecked(s.value(QStringLiteral("package/favorites"), false).toBool());
    ol->addWidget(m_images);
    ol->addWidget(m_foods);
    ol->addWidget(m_keepFav);
    lay->addWidget(opt);

    auto *privacy = new QLabel(tr("<small><b>Nicht enthalten</b> sind Benutzerprofile, Ziele und Gewichtsverlauf, Ernährungstagebuch, "
                                  "Wochenpläne, Einkaufslisten, Einstellungen (z. B. DeepL-Schlüssel) und Sicherungskopien.</small>"));
    privacy->setWordWrap(true);
    lay->addWidget(privacy);
    m_info = new QLabel;
    lay->addWidget(m_info);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Save)->setText(tr("Paket speichern …"));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &PackageExportDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(group, &QButtonGroup::buttonToggled, this, &PackageExportDialog::update);
    connect(m_tag, &QComboBox::currentTextChanged, this, [this] {
        m_tagged->setChecked(true);
        update();
    });
    connect(m_images, &QCheckBox::toggled, this, &PackageExportDialog::update);
    update();
    resize(600, sizeHint().height());
}

QStringList PackageExportDialog::chosenIds() const
{
    const RecipeDb &db = RecipeDb::instance();
    if (m_shown->isChecked()) return m_visible;
    QStringList ids;
    const QString tag = m_tag->currentText();
    for (const RecipeSummary &r : db.summaries()) {
        if (m_favs->isChecked() && !r.favorite) continue;
        if (m_tagged->isChecked() && !r.tags.contains(tag, Qt::CaseInsensitive)) continue;
        ids << r.id;
    }
    return ids;
}

void PackageExportDialog::update()
{
    const QStringList ids = chosenIds();
    int images = 0;
    for (const QString &id : ids)
        if (const RecipeSummary *s = RecipeDb::instance().summary(id); s && s->hasImage) ++images;
    QString t = ids.size() == 1 ? tr("1 Rezept") : tr("%1 Rezepte").arg(ids.size());
    if (m_images->isChecked() && images > 0) t += tr(", davon %1 mit Foto").arg(images);
    m_info->setText(t);
    if (auto *bb = findChild<QDialogButtonBox *>()) bb->button(QDialogButtonBox::Save)->setEnabled(!ids.isEmpty());
}

void PackageExportDialog::accept()
{
    const QStringList ids = chosenIds();
    if (ids.isEmpty()) return;
    QSettings s;
    QString base = m_name->text().trimmed();
    if (base.isEmpty()) base = tr("Rezepte");
    QString fileName = base;
    fileName.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    const QString dir = s.value(QStringLiteral("package/dir"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    const QString file = QFileDialog::getSaveFileName(this, tr("Rezeptpaket speichern"), QDir(dir).filePath(fileName + QStringLiteral(".vmpack")),
                                                      tr("ViviaMeals-Rezeptpaket (*.vmpack)"));
    if (file.isEmpty()) return;
    s.setValue(QStringLiteral("package/dir"), QFileInfo(file).absolutePath());
    s.setValue(QStringLiteral("package/author"), m_author->text().trimmed());
    s.setValue(QStringLiteral("package/images"), m_images->isChecked());
    s.setValue(QStringLiteral("package/foods"), m_foods->isChecked());
    s.setValue(QStringLiteral("package/favorites"), m_keepFav->isChecked());

    QProgressDialog prog(tr("Rezepte werden gelesen …"), tr("Abbrechen"), 0, int(ids.size()), this);
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(400);
    QList<Recipe> recipes;
    for (int i = 0; i < ids.size(); ++i) {
        prog.setValue(i);
        if (prog.wasCanceled()) return;
        Recipe r;
        if (RecipeDb::instance().load(ids[i], &r)) recipes << r;
    }
    prog.setValue(int(ids.size()));
    RecipePackage::Options o;
    o.name = base;
    o.author = m_author->text().trimmed();
    o.description = m_desc->toPlainText().trimmed();
    o.images = m_images->isChecked();
    o.foods = m_foods->isChecked();
    o.favorites = m_keepFav->isChecked();
    QString err;
    int foods = 0;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = RecipePackage::write(file, recipes, o, &err, &foods);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, windowTitle(), tr("Das Paket konnte nicht gespeichert werden:\n%1").arg(err));
        return;
    }
    m_file = file;
    const double mb = QFileInfo(file).size() / 1048576.0;
    QMessageBox::information(this, windowTitle(),
                             tr("%1 Rezepte%2 wurden in „%3“ gespeichert (%4 MB).")
                                 .arg(recipes.size())
                                 .arg(foods > 0 ? tr(" und %1 eigene Lebensmittel").arg(foods) : QString())
                                 .arg(QDir::toNativeSeparators(file))
                                 .arg(QLocale(QLocale::German).toString(mb, 'f', 1)));
    QDialog::accept();
}

// ================================================================ Import

PackageImportDialog::PackageImportDialog(const RecipePackage::Content &c, const QString &file, bool dbOpen, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Rezeptpaket übernehmen"));
    auto *lay = new QVBoxLayout(this);
    QString h = QStringLiteral("<h3>%1</h3>").arg((c.name.isEmpty() ? QFileInfo(file).completeBaseName() : c.name).toHtmlEscaped());
    if (!c.description.isEmpty()) h += QStringLiteral("<p>%1</p>").arg(c.description.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")));
    QStringList facts;
    facts << (c.recipes.size() == 1 ? tr("1 Rezept") : tr("%1 Rezepte").arg(c.recipes.size()));
    int images = 0;
    for (const Recipe &r : c.recipes)
        if (!r.image.isEmpty()) ++images;
    if (images > 0) facts << tr("%1 mit Foto").arg(images);
    if (!c.foods.isEmpty()) facts << tr("%1 eigene Lebensmittel").arg(c.foods.size());
    h += QStringLiteral("<p>%1</p>").arg(facts.join(QStringLiteral(" · ")));
    QStringList from;
    if (!c.author.isEmpty()) from << tr("von %1").arg(c.author.toHtmlEscaped());
    if (c.created.isValid()) from << tr("erstellt am %1").arg(QLocale(QLocale::German).toString(c.created.date(), QLocale::ShortFormat));
    if (!c.app.isEmpty()) from << c.app.toHtmlEscaped();
    if (!from.isEmpty()) h += QStringLiteral("<p><small>%1</small></p>").arg(from.join(QStringLiteral(" · ")));
    if (!c.warnings.isEmpty()) h += QStringLiteral("<p style=\"color:#b85400\">%1</p>").arg(c.warnings.join(QStringLiteral("<br>")).toHtmlEscaped());
    auto *info = new QLabel(h);
    info->setWordWrap(true);
    lay->addWidget(info);
    auto *box = new QGroupBox(tr("Wohin?"));
    auto *bl = new QVBoxLayout(box);
    m_merge = new QRadioButton(dbOpen ? tr("in die geöffnete Datenbank „%1“ übernehmen (mit Doubletten-Prüfung)").arg(RecipeDb::instance().name())
                                      : tr("in die geöffnete Datenbank übernehmen"));
    m_merge->setEnabled(dbOpen);
    m_new = new QRadioButton(tr("als &neue Rezeptdatenbank anlegen"));
    (dbOpen ? m_merge : m_new)->setChecked(true);
    bl->addWidget(m_merge);
    bl->addWidget(m_new);
    lay->addWidget(box);
    m_foods = new QCheckBox(tr("eigene Lebensmittel und Zuordnungen des Pakets in die Nährwerttabelle übernehmen (vorhandene bleiben unverändert)"));
    m_foods->setChecked(true);
    m_foods->setVisible(!c.foods.isEmpty() || !c.learned.isEmpty());
    lay->addWidget(m_foods);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Übernehmen"));
    bb->button(QDialogButtonBox::Ok)->setEnabled(!c.recipes.isEmpty());
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    resize(560, sizeHint().height());
}

PackageImportDialog::Target PackageImportDialog::target() const { return m_new->isChecked() ? NewDatabase : Merge; }
bool PackageImportDialog::importFoods() const { return !m_foods->isHidden() && m_foods->isChecked(); }
