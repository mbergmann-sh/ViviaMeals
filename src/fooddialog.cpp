// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "fooddialog.h"
#include "foodwizard.h"
#include "icons.h"
#include "recipe.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QDir>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QGroupBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QTableWidgetItem *item(const QString &text, const QString &key = QString(), bool right = false)
{
    auto *it = new QTableWidgetItem(text);
    it->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    if (!key.isEmpty()) it->setData(Qt::UserRole, key);
    if (right) it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

bool matchesFilter(const Food *f, const QString &filter)
{
    if (filter.isEmpty()) return true;
    const QString l = filter.toLower();
    if (f->name.toLower().contains(l)) return true;
    for (const QString &a : f->aliases)
        if (a.contains(l)) return true;
    return false;
}

} // namespace

// ---------------------------------------------------------------- FoodPickerDialog

FoodPickerDialog::FoodPickerDialog(const QString &ingredientName, const QString &currentKey, QWidget *parent)
    : QDialog(parent), m_current(currentKey)
{
    setWindowTitle(tr("Lebensmittel zuordnen"));
    auto *lay = new QVBoxLayout(this);
    auto *info = new QLabel(tr("Welchem Lebensmittel der Nährwerttabelle entspricht die Zutat <b>%1</b>?").arg(ingredientName.toHtmlEscaped()));
    info->setWordWrap(true);
    lay->addWidget(info);
    m_filter = new QLineEdit;
    m_filter->setPlaceholderText(tr("Suchen (deutsch oder englisch) …"));
    m_filter->setClearButtonEnabled(true);
    lay->addWidget(m_filter);
    m_table = new QTableWidget(0, 3);
    m_table->setHorizontalHeaderLabels({ tr("Lebensmittel"), tr("Kategorie"), tr("kcal/100 g") });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    lay->addWidget(m_table, 1);
    m_remember = new QCheckBox(tr("Zuordnung für „%1“ in allen Rezepten merken").arg(ingredientName));
    m_remember->setChecked(true);
    lay->addWidget(m_remember);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto *none = bb->addButton(tr("Keine Zuordnung"), QDialogButtonBox::ResetRole);
    auto *create = bb->addButton(tr("Neues Lebensmittel anlegen …"), QDialogButtonBox::ActionRole);
    create->setIcon(Icons::get(QStringLiteral("add")));
    create->setToolTip(tr("Eigenes Lebensmittel mit den Nährwerten von der Verpackung anlegen (z. B. ein Fertigprodukt)"));
    lay->addWidget(bb);
    connect(create, &QPushButton::clicked, this, [this, ingredientName] {
        FoodWizard w(ingredientName, this, false);
        if (w.exec() != QDialog::Accepted || w.foodKey().isEmpty()) return;
        m_current = w.foodKey();
        m_filter->clear();
        fill();
        accept();
    });
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(none, &QPushButton::clicked, this, [this] {
        m_table->clearSelection();
        m_current.clear();
        accept();
    });
    connect(m_filter, &QLineEdit::textChanged, this, &FoodPickerDialog::fill);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &QDialog::accept);
    // Vorschlag: bestes Wort aus dem Zutatennamen als Filter
    const Food *guess = FoodTable::instance().match(ingredientName);
    if (!currentKey.isEmpty() || !guess) fill();
    else {
        m_current = guess->key;
        fill();
    }
    resize(560, 560);
}

void FoodPickerDialog::fill()
{
    const QString f = m_filter->text().trimmed();
    m_table->setRowCount(0);
    int select = -1;
    for (const Food *food : FoodTable::instance().all()) {
        if (!matchesFilter(food, f)) continue;
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        m_table->setItem(r, 0, item(food->name + (food->user ? tr(" (eigener Wert)") : QString()), food->key));
        m_table->setItem(r, 1, item(FoodTable::categoryName(food->category)));
        m_table->setItem(r, 2, item(Fmt::number(food->per100.kcal, 0), QString(), true));
        if (food->key == m_current) select = r;
    }
    if (select >= 0) {
        m_table->selectRow(select);
        m_table->scrollToItem(m_table->item(select, 0), QAbstractItemView::PositionAtCenter);
    }
}

QString FoodPickerDialog::selectedKey() const
{
    const auto sel = m_table->selectedItems();
    return sel.isEmpty() ? QString() : m_table->item(sel.first()->row(), 0)->data(Qt::UserRole).toString();
}

bool FoodPickerDialog::remember() const { return m_remember->isChecked(); }

// ---------------------------------------------------------------- FoodEditDialog

FoodEditDialog::FoodEditDialog(const Food &food, QWidget *parent) : QDialog(parent), m_food(food)
{
    setWindowTitle(food.key.isEmpty() ? tr("Neues Lebensmittel") : tr("Lebensmittel bearbeiten"));
    auto *lay = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    m_name = new QLineEdit(food.name);
    form->addRow(tr("Name:"), m_name);
    m_category = new QComboBox;
    for (const QString &c : FoodTable::categories()) m_category->addItem(FoodTable::categoryName(c), c);
    m_category->setCurrentIndex(qMax(0, m_category->findData(food.category.isEmpty() ? QStringLiteral("sonstiges") : food.category)));
    form->addRow(tr("Kategorie:"), m_category);
    const char *labels[8] = { "Energie (kcal):", "Eiweiß (g):", "Fett (g):", "davon gesättigt (g):", "Kohlenhydrate (g):",
                              "davon Zucker (g):", "Ballaststoffe (g):", "Salz (g):" };
    const double vals[8] = { food.per100.kcal, food.per100.protein, food.per100.fat, food.per100.satFat,
                             food.per100.carbs, food.per100.sugar, food.per100.fiber, food.per100.salt };
    auto *hint = new QLabel(tr("<b>Nährwerte je 100 g</b>"));
    form->addRow(hint);
    for (int i = 0; i < 8; ++i) {
        m_values[i] = new QDoubleSpinBox;
        m_values[i]->setRange(0, i == 0 ? 1000 : 100);
        m_values[i]->setDecimals(i == 0 ? 0 : 1);
        m_values[i]->setValue(vals[i]);
        form->addRow(tr(labels[i]), m_values[i]);
    }
    m_density = new QDoubleSpinBox;
    m_density->setRange(0.05, 3);
    m_density->setDecimals(2);
    m_density->setSingleStep(0.05);
    m_density->setValue(food.density > 0 ? food.density : 1.0);
    m_density->setToolTip(tr("Gramm je Milliliter – für die Umrechnung von EL, TL, Tassen und ml"));
    form->addRow(tr("Dichte (g/ml):"), m_density);
    m_piece = new QDoubleSpinBox;
    m_piece->setRange(0, 5000);
    m_piece->setDecimals(0);
    m_piece->setValue(food.piece);
    m_piece->setSpecialValueText(tr("unbekannt"));
    form->addRow(tr("Gewicht je Stück (g):"), m_piece);
    QStringList units;
    for (auto it = food.unitGrams.constBegin(); it != food.unitGrams.constEnd(); ++it) units << QStringLiteral("%1=%2").arg(it.key()).arg(it.value());
    m_units = new QLineEdit(units.join(QStringLiteral("; ")));
    m_units->setPlaceholderText(tr("z. B. Pck.=8; Dose=400"));
    form->addRow(tr("Gewicht je Einheit:"), m_units);
    m_aliases = new QPlainTextEdit(food.aliases.join(QStringLiteral(", ")));
    m_aliases->setPlaceholderText(tr("weitere Namen, durch Komma getrennt (auch englisch)"));
    m_aliases->setMaximumHeight(90);
    form->addRow(tr("Andere Namen:"), m_aliases);
    lay->addLayout(form);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, [this] {
        if (m_name->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, windowTitle(), tr("Bitte einen Namen eingeben."));
            return;
        }
        accept();
    });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

Food FoodEditDialog::food() const
{
    Food f = m_food;
    f.name = m_name->text().trimmed();
    f.category = m_category->currentData().toString();
    f.per100.kcal = m_values[0]->value();
    f.per100.protein = m_values[1]->value();
    f.per100.fat = m_values[2]->value();
    f.per100.satFat = m_values[3]->value();
    f.per100.carbs = m_values[4]->value();
    f.per100.sugar = m_values[5]->value();
    f.per100.fiber = m_values[6]->value();
    f.per100.salt = m_values[7]->value();
    f.density = m_density->value();
    f.piece = m_piece->value();
    f.unitGrams.clear();
    for (const QString &u : m_units->text().split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        const QStringList kv = u.split(QLatin1Char('='));
        if (kv.size() == 2) f.unitGrams.insert(kv[0].trimmed(), kv[1].trimmed().replace(QLatin1Char(','), QLatin1Char('.')).toDouble());
    }
    f.aliases.clear();
    for (const QString &a : m_aliases->toPlainText().split(QLatin1Char(','), Qt::SkipEmptyParts))
        if (!a.trimmed().isEmpty()) f.aliases << a.trimmed().toLower();
    return f;
}

// ---------------------------------------------------------------- FoodTableDialog

FoodTableDialog::FoodTableDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Nährwerttabelle"));
    auto *lay = new QVBoxLayout(this);
    auto *info = new QLabel(tr("Richtwerte je 100 g. Eigene Einträge und geänderte Werte haben Vorrang vor der eingebauten Tabelle "
                               "und werden in allen Rezepten verwendet (nach „Nährwerte neu berechnen“)."));
    info->setWordWrap(true);
    lay->addWidget(info);
    auto *top = new QHBoxLayout;
    m_filter = new QLineEdit;
    m_filter->setPlaceholderText(tr("Suchen …"));
    m_filter->setClearButtonEnabled(true);
    top->addWidget(m_filter, 2);
    m_cat = new QComboBox;
    m_cat->addItem(tr("Alle Kategorien"), QString());
    for (const QString &c : FoodTable::categories()) m_cat->addItem(FoodTable::categoryName(c), c);
    top->addWidget(m_cat, 1);
    lay->addLayout(top);
    m_table = new QTableWidget(0, 10);
    m_table->setHorizontalHeaderLabels({ tr("Lebensmittel"), tr("kcal"), tr("Eiweiß"), tr("Fett"), tr("ges. FS"), tr("KH"),
                                         tr("Zucker"), tr("Ballast."), tr("Salz"), tr("Herkunft") });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    lay->addWidget(m_table, 1);
    auto *bl = new QHBoxLayout;
    auto *add = new QPushButton(Icons::get(QStringLiteral("add")), tr("Neu …"));
    auto *edit = new QPushButton(Icons::get(QStringLiteral("edit")), tr("Bearbeiten …"));
    auto *reset = new QPushButton(Icons::get(QStringLiteral("refresh")), tr("Eigenen Wert entfernen"));
    bl->addWidget(add);
    bl->addWidget(edit);
    bl->addWidget(reset);
    bl->addStretch(1);
    auto *csv = new QPushButton(Icons::get(QStringLiteral("import")), tr("Importieren / Exportieren"));
    auto *csvMenu = new QMenu(csv);
    csv->setMenu(csvMenu);
    bl->addWidget(csv);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    bl->addWidget(bb);
    lay->addLayout(bl);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::accept);
    csvMenu->addAction(Icons::get(QStringLiteral("import")), tr("Nährwerte importieren (CSV, USDA) …"), this, [this] {
        FoodImportDialog d(this);
        if (d.exec() == QDialog::Accepted && d.imported() > 0) {
            m_changed = true;
            fill();
        }
    });
    csvMenu->addAction(Icons::get(QStringLiteral("save")), tr("Eigene Lebensmittel als CSV exportieren …"), this, [this] {
        const QString f = QFileDialog::getSaveFileName(this, tr("Eigene Lebensmittel exportieren"),
                                                       QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(tr("eigene-naehrwerte.csv")),
                                                       tr("CSV (*.csv)"));
        QString err;
        if (!f.isEmpty() && !FoodCsv::exportUserFoods(f, &err)) QMessageBox::warning(this, windowTitle(), err);
    });
    csvMenu->addAction(Icons::get(QStringLiteral("recipe-add")), tr("Leere CSV-Vorlage speichern …"), this, [this] {
        const QString f = QFileDialog::getSaveFileName(this, tr("CSV-Vorlage speichern"),
                                                       QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(tr("naehrwerte-vorlage.csv")),
                                                       tr("CSV (*.csv)"));
        QString err;
        if (!f.isEmpty() && !FoodCsv::writeTemplate(f, &err)) QMessageBox::warning(this, windowTitle(), err);
    });
    csvMenu->addSeparator();
    csvMenu->addAction(Icons::get(QStringLiteral("delete")), tr("Importierte Einträge einer Herkunft entfernen …"), this, [this] {
        const QStringList sources = FoodTable::instance().userSources();
        if (sources.isEmpty()) {
            QMessageBox::information(this, windowTitle(), tr("Es gibt keine importierten Einträge."));
            return;
        }
        bool ok = false;
        const QString s = QInputDialog::getItem(this, windowTitle(), tr("Alle Einträge dieser Herkunft entfernen:"), sources, 0, false, &ok);
        if (!ok) return;
        const int n = FoodTable::instance().removeUserFoodsFrom(s);
        m_changed = true;
        fill();
        QMessageBox::information(this, windowTitle(), tr("%1 Einträge entfernt.").arg(n));
    });
    connect(m_filter, &QLineEdit::textChanged, this, &FoodTableDialog::fill);
    connect(m_cat, qOverload<int>(&QComboBox::currentIndexChanged), this, &FoodTableDialog::fill);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &FoodTableDialog::editCurrent);
    connect(edit, &QPushButton::clicked, this, &FoodTableDialog::editCurrent);
    connect(add, &QPushButton::clicked, this, [this] {
        FoodWizard w(QString(), this, false);
        w.setInitialName(m_filter->text().trimmed());
        if (w.exec() != QDialog::Accepted || w.foodKey().isEmpty()) return;
        m_changed = true;
        fill();
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        const auto sel = m_table->selectedItems();
        if (sel.isEmpty()) return;
        const QString key = m_table->item(sel.first()->row(), 0)->data(Qt::UserRole).toString();
        const Food *f = FoodTable::instance().find(key);
        if (!f || !f->user) return;
        const QString q = FoodTable::instance().isBuiltin(key) ? tr("Eigene Werte für „%1“ verwerfen und die eingebauten Werte verwenden?").arg(f->name)
                                                               : tr("Eigenes Lebensmittel „%1“ löschen?").arg(f->name);
        if (QMessageBox::question(this, windowTitle(), q) != QMessageBox::Yes) return;
        FoodTable::instance().removeUserFood(key);
        m_changed = true;
        fill();
    });
    fill();
    resize(900, 620);
}

void FoodTableDialog::fill()
{
    const QString f = m_filter->text().trimmed();
    const QString cat = m_cat->currentData().toString();
    m_table->setRowCount(0);
    for (const Food *food : FoodTable::instance().all()) {
        if (!cat.isEmpty() && food->category != cat) continue;
        if (!matchesFilter(food, f)) continue;
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        auto *name = item(food->name, food->key);
        if (food->user) {
            QFont fn = name->font();
            fn.setItalic(true);
            name->setFont(fn);
            name->setToolTip(tr("eigener Wert"));
        }
        m_table->setItem(r, 0, name);
        const Nutrients &n = food->per100;
        const double v[8] = { n.kcal, n.protein, n.fat, n.satFat, n.carbs, n.sugar, n.fiber, n.salt };
        for (int c = 0; c < 8; ++c) m_table->setItem(r, c + 1, item(Fmt::number(v[c], c == 0 ? 0 : (c == 7 ? 2 : 1)), QString(), true));
        m_table->setItem(r, 9, item(food->user ? (food->source.isEmpty() ? tr("eigener Wert") : food->source) : tr("ViviaMeals")));
    }
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    setWindowTitle(tr("Nährwerttabelle – %1 Lebensmittel").arg(m_table->rowCount()));
}

void FoodTableDialog::editCurrent()
{
    const auto sel = m_table->selectedItems();
    if (sel.isEmpty()) return;
    const QString key = m_table->item(sel.first()->row(), 0)->data(Qt::UserRole).toString();
    const Food *f = FoodTable::instance().find(key);
    if (!f) return;
    FoodEditDialog d(*f, this);
    if (d.exec() != QDialog::Accepted) return;
    FoodTable::instance().saveUserFood(d.food());
    m_changed = true;
    fill();
}

// ---------------------------------------------------------------- FoodImportDialog

FoodImportDialog::FoodImportDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Nährwerte importieren"));
    setWindowFlag(Qt::WindowMaximizeButtonHint, true);
    auto *lay = new QVBoxLayout(this);
    auto *src = new QGroupBox(tr("Quelle"));
    auto *sl = new QVBoxLayout(src);
    m_csv = new QRadioButton(tr("CSV-Datei – eigene Tabelle (Excel/LibreOffice), ViviaMeals-Vorlage, Open-Food-Facts-Export …"));
    m_usda = new QRadioButton(tr("USDA FoodData Central – entpackter CSV-Ordner (z. B. „Foundation Foods“ oder „SR Legacy“, gemeinfrei)"));
    m_csv->setChecked(true);
    sl->addWidget(m_csv);
    sl->addWidget(m_usda);
    auto *pl = new QHBoxLayout;
    m_path = new QLineEdit;
    m_path->setObjectName(QStringLiteral("foodImportPath"));
    m_path->setPlaceholderText(tr("Datei bzw. Ordner wählen …"));
    auto *browse = new QPushButton(Icons::get(QStringLiteral("open")), tr("Wählen …"));
    pl->addWidget(m_path, 1);
    pl->addWidget(browse);
    sl->addLayout(pl);
    m_info = new QLabel;
    m_info->setWordWrap(true);
    sl->addWidget(m_info);
    lay->addWidget(src);

    auto *mid = new QHBoxLayout;
    m_mapBox = new QGroupBox(tr("Spalten (Werte je 100 g)"));
    auto *mf = new QFormLayout(m_mapBox);
    for (int f = 0; f < FoodCsv::FieldCount; ++f) {
        m_map[f] = new QComboBox;
        m_map[f]->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_map[f]->setMinimumContentsLength(18);
        if (f == FoodCsv::Sodium) {
            auto *row = new QHBoxLayout;
            m_sodiumUnit = new QComboBox;
            m_sodiumUnit->addItems({ tr("g"), tr("mg") });
            row->addWidget(m_map[f], 1);
            row->addWidget(m_sodiumUnit);
            mf->addRow(FoodCsv::fieldName(FoodCsv::Field(f)) + QLatin1Char(':'), row);
        } else {
            mf->addRow(FoodCsv::fieldName(FoodCsv::Field(f)) + QLatin1Char(':'), m_map[f]);
        }
        connect(m_map[f], qOverload<int>(&QComboBox::currentIndexChanged), this, &FoodImportDialog::preview);
    }
    connect(m_sodiumUnit, qOverload<int>(&QComboBox::currentIndexChanged), this, &FoodImportDialog::preview);
    mid->addWidget(m_mapBox);

    auto *right = new QVBoxLayout;
    auto *opt = new QGroupBox(tr("Optionen"));
    auto *of = new QFormLayout(opt);
    m_defCat = new QComboBox;
    for (const QString &c : FoodTable::categories()) m_defCat->addItem(FoodTable::categoryName(c), c);
    m_defCat->setCurrentIndex(m_defCat->findData(QStringLiteral("sonstiges")));
    m_defCat->setToolTip(tr("Für Einträge, deren Kategorie sich weder aus der Tabelle noch aus dem Namen ergibt"));
    of->addRow(tr("Standardkategorie:"), m_defCat);
    m_filter = new QLineEdit;
    m_filter->setPlaceholderText(tr("z. B. „Käse“ – leer = alle Zeilen"));
    of->addRow(tr("Nur Namen mit:"), m_filter);
    m_source = new QLineEdit;
    m_source->setToolTip(tr("Wird bei jedem importierten Eintrag vermerkt; darüber lassen sich alle Einträge eines Imports wieder entfernen."));
    of->addRow(tr("Herkunft:"), m_source);
    m_replace = new QCheckBox(tr("Bereits importierte Einträge gleichen Namens ersetzen"));
    m_replace->setChecked(true);
    of->addRow(QString(), m_replace);
    m_override = new QCheckBox(tr("Eingebaute Werte gleichen Namens überschreiben"));
    m_override->setToolTip(tr("Zum Beispiel „Butter“ aus der eigenen Tabelle statt der eingebauten Richtwerte verwenden"));
    of->addRow(QString(), m_override);
    right->addWidget(opt);
    auto *pv = new QGroupBox(tr("Vorschau"));
    auto *pvl = new QVBoxLayout(pv);
    m_count = new QLabel;
    pvl->addWidget(m_count);
    m_preview = new QTableWidget(0, 6);
    m_preview->setHorizontalHeaderLabels({ tr("Name"), tr("Kategorie"), tr("kcal"), tr("Eiweiß"), tr("Fett"), tr("KH") });
    m_preview->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_preview->verticalHeader()->hide();
    m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_preview->verticalHeader()->setDefaultSectionSize(m_preview->fontMetrics().height() + 6);
    pvl->addWidget(m_preview, 1);
    right->addWidget(pv, 1);
    mid->addLayout(right, 1);
    lay->addLayout(mid, 1);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Importieren"));
    bb->button(QDialogButtonBox::Ok)->setIcon(Icons::get(QStringLiteral("import")));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &FoodImportDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(browse, &QPushButton::clicked, this, &FoodImportDialog::browse);
    connect(m_path, &QLineEdit::editingFinished, this, &FoodImportDialog::load);
    auto modeChanged = [this] {
        m_mapBox->setEnabled(m_csv->isChecked());
        m_path->clear();
        m_table = FoodCsv::Table();
        m_info->setText(m_usda->isChecked()
                            ? tr("Download unter fdc.nal.usda.gov → „Download Data“ (CSV), entpacken und den Ordner mit food.csv wählen. "
                                 "Die Daten sind gemeinfrei (CC0); die USDA bittet um Nennung von „FoodData Central“ als Quelle. "
                                 "Namen sind englisch – im Rezepteditor über „Zuordnung ändern“ auswählbar.")
                            : tr("Eine Zeile je Lebensmittel, Werte je 100 g. Trennzeichen, Zeichensatz und Dezimalkomma werden erkannt, "
                                 "die Spalten anhand der Überschriften zugeordnet. Eine Vorlage gibt es unter „Leere CSV-Vorlage speichern“."));
        m_source->setText(m_usda->isChecked() ? QStringLiteral("USDA FoodData Central") : QString());
        preview();
    };
    connect(m_csv, &QRadioButton::toggled, this, modeChanged);
    auto *filterTimer = new QTimer(this);
    filterTimer->setSingleShot(true);
    filterTimer->setInterval(300);
    connect(filterTimer, &QTimer::timeout, this, &FoodImportDialog::preview);
    connect(m_filter, &QLineEdit::textChanged, filterTimer, qOverload<>(&QTimer::start));
    connect(m_override, &QCheckBox::toggled, this, &FoodImportDialog::preview);
    connect(m_defCat, qOverload<int>(&QComboBox::currentIndexChanged), this, &FoodImportDialog::preview);
    modeChanged();
    resize(1100, 700);
}

FoodCsv::Options FoodImportDialog::options() const
{
    FoodCsv::Options o;
    o.source = m_source->text().trimmed();
    o.defaultCategory = m_defCat->currentData().toString();
    o.filter = m_filter->text();
    o.overrideBuiltin = m_override->isChecked();
    return o;
}

void FoodImportDialog::browse()
{
    QSettings s;
    const QString dir = s.value(QStringLiteral("foodimport/dir"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    QString p;
    if (m_usda->isChecked()) p = QFileDialog::getExistingDirectory(this, tr("FoodData-Central-Ordner wählen"), dir);
    else p = QFileDialog::getOpenFileName(this, tr("CSV-Datei wählen"), dir, tr("CSV/Text (*.csv *.txt *.tsv);;Alle Dateien (*)"));
    if (p.isEmpty()) return;
    s.setValue(QStringLiteral("foodimport/dir"), m_usda->isChecked() ? p : QFileInfo(p).absolutePath());
    m_path->setText(QDir::toNativeSeparators(p));
    load();
}

void FoodImportDialog::load()
{
    const QString p = QDir::fromNativeSeparators(m_path->text().trimmed());
    m_foods.clear();
    if (p.isEmpty()) return;
    QString err;
    if (m_usda->isChecked()) {
        if (!FoodCsv::isUsdaFolder(p)) {
            m_info->setText(tr("<b>Kein FoodData-Central-Ordner:</b> food.csv, nutrient.csv und food_nutrient.csv fehlen."));
            preview();
            return;
        }
        m_info->setText(tr("FoodData-Central-Ordner erkannt."));
        preview();
        return;
    }
    if (QFileInfo(p).size() > 400LL * 1024 * 1024) {
        m_info->setText(tr("<b>Die Datei ist sehr groß.</b> Bitte vorher filtern (z. B. Suchergebnis bei Open Food Facts als CSV laden)."));
        return;
    }
    if (!FoodCsv::readTable(p, &m_table, &err)) {
        m_info->setText(tr("<b>Fehler:</b> %1").arg(err.toHtmlEscaped()));
        return;
    }
    const QString delim = m_table.delimiter == QLatin1Char('\t') ? tr("Tabulator") : QString(m_table.delimiter);
    m_info->setText(tr("%1 Zeilen, %2 Spalten · Trennzeichen „%3“ · Zeichensatz %4")
                        .arg(m_table.rows.size())
                        .arg(m_table.header.size())
                        .arg(delim, m_table.encoding));
    if (m_source->text().isEmpty()) m_source->setText(QFileInfo(p).fileName());
    const FoodCsv::Mapping m = FoodCsv::autoMap(m_table.header);
    for (int f = 0; f < FoodCsv::FieldCount; ++f) {
        m_map[f]->blockSignals(true);
        m_map[f]->clear();
        m_map[f]->addItem(tr("– keine –"), -1);
        for (int c = 0; c < m_table.header.size(); ++c) m_map[f]->addItem(m_table.header[c], c);
        m_map[f]->setCurrentIndex(m.col[f] + 1);
        m_map[f]->blockSignals(false);
    }
    m_sodiumUnit->blockSignals(true);
    m_sodiumUnit->setCurrentIndex(m.sodiumMg ? 1 : 0);
    m_sodiumUnit->blockSignals(false);
    preview();
}

void FoodImportDialog::preview()
{
    m_foods.clear();
    m_preview->setRowCount(0);
    const QString p = QDir::fromNativeSeparators(m_path->text().trimmed());
    QStringList warnings;
    if (m_usda->isChecked()) {
        // Vorschau erst beim Import (Datei food_nutrient.csv ist groß)
        m_count->setText(p.isEmpty() || !FoodCsv::isUsdaFolder(p) ? QString()
                                                                  : tr("Die Lebensmittel werden beim Import gelesen (kann bei „SR Legacy“ eine Minute dauern)."));
        return;
    }
    if (m_table.header.isEmpty()) {
        m_count->clear();
        return;
    }
    FoodCsv::Mapping m;
    for (int f = 0; f < FoodCsv::FieldCount; ++f) m.col[f] = m_map[f]->currentData().toInt();
    m.sodiumMg = m_sodiumUnit->currentIndex() == 1;
    if (m.col[FoodCsv::Name] < 0 || (m.col[FoodCsv::Kcal] < 0 && m.col[FoodCsv::KJ] < 0)) {
        m_count->setText(tr("<b>Bitte mindestens die Spalten „Name“ und „Energie“ (kcal oder kJ) zuordnen.</b>"));
        return;
    }
    m_foods = FoodCsv::convert(m_table, m, options(), &warnings);
    const int shown = qMin(200, int(m_foods.size()));
    m_preview->setRowCount(shown);
    for (int i = 0; i < shown; ++i) {
        const Food &f = m_foods[i];
        m_preview->setItem(i, 0, item(f.name));
        m_preview->setItem(i, 1, item(FoodTable::categoryName(f.category)));
        m_preview->setItem(i, 2, item(Fmt::number(f.per100.kcal, 0), QString(), true));
        m_preview->setItem(i, 3, item(Fmt::number(f.per100.protein), QString(), true));
        m_preview->setItem(i, 4, item(Fmt::number(f.per100.fat), QString(), true));
        m_preview->setItem(i, 5, item(Fmt::number(f.per100.carbs), QString(), true));
    }
    m_count->setText(tr("<b>%1 Lebensmittel</b> werden übernommen%2. %3")
                         .arg(m_foods.size())
                         .arg(m_foods.size() > shown ? tr(" (Vorschau: die ersten %1)").arg(shown) : QString())
                         .arg(warnings.join(QLatin1Char(' ')).toHtmlEscaped()));
}

void FoodImportDialog::accept()
{
    const QString p = QDir::fromNativeSeparators(m_path->text().trimmed());
    if (m_usda->isChecked()) {
        QProgressDialog prog(tr("FoodData Central wird gelesen …"), tr("Abbrechen"), 0, 0, this);
        prog.setWindowModality(Qt::WindowModal);
        prog.setMinimumDuration(0);
        QString err;
        if (!FoodCsv::readUsda(p, options(), &m_foods, &err, [&](int n) {
                prog.setLabelText(tr("FoodData Central wird gelesen … %1 Nährwertzeilen").arg(n));
                QApplication::processEvents();
                return !prog.wasCanceled();
            })) {
            prog.close();
            QMessageBox::warning(this, windowTitle(), err);
            return;
        }
        prog.close();
        if (m_foods.isEmpty()) {
            QMessageBox::warning(this, windowTitle(), tr("Im Ordner wurden keine Lebensmittel mit Energiewert gefunden."));
            return;
        }
        if (QMessageBox::question(this, windowTitle(), tr("%1 Lebensmittel aus FoodData Central übernehmen?").arg(m_foods.size())) != QMessageBox::Yes)
            return;
    }
    if (m_foods.isEmpty()) {
        QMessageBox::information(this, windowTitle(), tr("Es gibt nichts zu importieren."));
        return;
    }
    if (m_source->text().trimmed().isEmpty()) {
        QMessageBox::information(this, windowTitle(), tr("Bitte eine Herkunft angeben – darüber lassen sich die Einträge später wieder entfernen."));
        m_source->setFocus();
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_imported = FoodTable::instance().saveUserFoods(m_foods, m_replace->isChecked());
    QApplication::restoreOverrideCursor();
    QMessageBox::information(this, windowTitle(), tr("%1 Lebensmittel übernommen.").arg(m_imported));
    QDialog::accept();
}
