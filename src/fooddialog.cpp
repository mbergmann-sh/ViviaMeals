// SPDX-License-Identifier: MIT
#include "fooddialog.h"
#include "icons.h"
#include "recipe.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
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
    lay->addWidget(bb);
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
    m_table = new QTableWidget(0, 9);
    m_table->setHorizontalHeaderLabels({ tr("Lebensmittel"), tr("kcal"), tr("Eiweiß"), tr("Fett"), tr("ges. FS"), tr("KH"),
                                         tr("Zucker"), tr("Ballast."), tr("Salz") });
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
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    bl->addWidget(bb);
    lay->addLayout(bl);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(m_filter, &QLineEdit::textChanged, this, &FoodTableDialog::fill);
    connect(m_cat, qOverload<int>(&QComboBox::currentIndexChanged), this, &FoodTableDialog::fill);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &FoodTableDialog::editCurrent);
    connect(edit, &QPushButton::clicked, this, &FoodTableDialog::editCurrent);
    connect(add, &QPushButton::clicked, this, [this] {
        Food f;
        f.category = QStringLiteral("sonstiges");
        f.name = m_filter->text().trimmed();
        FoodEditDialog d(f, this);
        if (d.exec() != QDialog::Accepted) return;
        FoodTable::instance().saveUserFood(d.food());
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
    }
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
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
