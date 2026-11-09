// SPDX-License-Identifier: MIT
#include "editordialog.h"
#include "fooddialog.h"
#include "foodtable.h"
#include "glossary.h"
#include "icons.h"
#include "ingredientparser.h"
#include "nutrition.h"
#include "recipedb.h"
#include "units.h"

#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

EditorDialog::EditorDialog(const Recipe &r, QWidget *parent) : QDialog(parent), m_recipe(r)
{
    setWindowTitle(r.title.isEmpty() ? tr("Neues Rezept") : tr("Rezept bearbeiten – %1").arg(r.title));
    setWindowFlag(Qt::WindowMaximizeButtonHint, true);
    for (const Ingredient &i : r.ingredients)
        if (i.foodManual && !i.food.isEmpty()) m_manual.insert(i.name.toLower(), i.food);

    auto *lay = new QVBoxLayout(this);

    // ---- Kopf: Felder links, Bild rechts
    auto *head = new QHBoxLayout;
    auto *form = new QFormLayout;
    m_title = new QLineEdit(r.title);
    m_title->setPlaceholderText(tr("Name des Rezepts"));
    QFont tf = m_title->font();
    tf.setPointSizeF(tf.pointSizeF() * 1.2);
    tf.setBold(true);
    m_title->setFont(tf);
    form->addRow(tr("&Titel:"), m_title);
    auto *sv = new QHBoxLayout;
    m_servings = new QDoubleSpinBox;
    m_servings->setRange(0.5, 500);
    m_servings->setDecimals(1);
    m_servings->setValue(r.servings);
    sv->addWidget(m_servings);
    m_yield = new QComboBox;
    m_yield->setEditable(true);
    m_yield->addItems({ tr("Portionen"), tr("Stück"), tr("Kuchen"), tr("Gläser"), tr("Rezept") });
    m_yield->setCurrentText(r.yieldUnit.isEmpty() ? tr("Portionen") : r.yieldUnit);
    sv->addWidget(m_yield);
    sv->addSpacing(16);
    sv->addWidget(new QLabel(tr("Zubereitungszeit:")));
    m_prep = new QSpinBox;
    m_prep->setRange(0, 2000);
    m_prep->setSuffix(tr(" min"));
    m_prep->setSpecialValueText(tr("–"));
    m_prep->setValue(r.prepMinutes);
    sv->addWidget(m_prep);
    sv->addStretch(1);
    form->addRow(tr("Menge:"), sv);
    m_tags = new QLineEdit(r.tags.join(QStringLiteral(", ")));
    m_tags->setPlaceholderText(tr("z. B. vegetarisch, backen, desserts, menüs"));
    auto *comp = new QCompleter(RecipeDb::instance().allTags() + QStringList{ QStringLiteral("vegan"), QStringLiteral("vegetarisch"),
                                                                              QStringLiteral("fleischgerichte"), QStringLiteral("fisch"),
                                                                              QStringLiteral("backen"), QStringLiteral("desserts"),
                                                                              QStringLiteral("menüs"), QStringLiteral("frühstück"),
                                                                              QStringLiteral("hauptgerichte"), QStringLiteral("suppen"),
                                                                              QStringLiteral("salate") },
                                this);
    comp->setCaseSensitivity(Qt::CaseInsensitive);
    m_tags->setCompleter(comp);
    auto *tl = new QHBoxLayout;
    tl->addWidget(m_tags, 1);
    auto *autoTags = new QPushButton(Icons::get(QStringLiteral("tag")), tr("Ergänzen"));
    autoTags->setToolTip(tr("Tags aus den Zutaten ableiten: vegan, vegetarisch, fleischgerichte, fisch"));
    tl->addWidget(autoTags);
    form->addRow(tr("Tags:"), tl);
    m_source = new QLineEdit(r.source);
    form->addRow(tr("&Quelle:"), m_source);
    m_fav = new QCheckBox(tr("Favorit"));
    m_fav->setChecked(r.favorite);
    form->addRow(QString(), m_fav);
    head->addLayout(form, 1);

    auto *imgBox = new QVBoxLayout;
    m_image = new QLabel;
    m_image->setFixedSize(220, 165);
    m_image->setAlignment(Qt::AlignCenter);
    m_image->setFrameShape(QFrame::StyledPanel);
    imgBox->addWidget(m_image);
    auto *ib = new QHBoxLayout;
    auto *pick = new QPushButton(Icons::get(QStringLiteral("image")), tr("Bild wählen …"));
    auto *clear = new QPushButton(Icons::get(QStringLiteral("delete")), QString());
    clear->setToolTip(tr("Bild entfernen"));
    ib->addWidget(pick, 1);
    ib->addWidget(clear);
    imgBox->addLayout(ib);
    imgBox->addStretch(1);
    head->addLayout(imgBox);
    lay->addLayout(head);

    // ---- Zutaten | Zubereitung | Nährwerte
    auto *split = new QSplitter(Qt::Horizontal);
    split->setObjectName(QStringLiteral("editorSplit"));
    auto *ingBox = new QGroupBox(tr("&Zutaten (eine je Zeile)"));
    auto *il = new QVBoxLayout(ingBox);
    m_ingredients = new QPlainTextEdit(r.ingredientsText());
    m_ingredients->setPlaceholderText(tr("200 g Mehl\n2 Eier\n1 Prise Salz\n== Für die Füllung ==\n250 g Quark\n…\n\n"
                                         "Englische Maße (cups, oz, tbsp) werden umgerechnet."));
    m_ingredients->setLineWrapMode(QPlainTextEdit::NoWrap);
    il->addWidget(m_ingredients);
    split->addWidget(ingBox);
    auto *insBox = new QGroupBox(tr("Zu&bereitung"));
    auto *inl = new QVBoxLayout(insBox);
    m_instructions = new QPlainTextEdit(r.instructions);
    m_instructions->setPlaceholderText(tr("Arbeitsschritte – Absätze durch eine Leerzeile trennen."));
    inl->addWidget(m_instructions);
    split->addWidget(insBox);
    auto *nutBox = new QGroupBox(tr("Nährwertanalyse"));
    auto *nl = new QVBoxLayout(nutBox);
    m_summary = new QLabel;
    m_summary->setWordWrap(true);
    m_summary->setTextFormat(Qt::RichText);
    nl->addWidget(m_summary);
    m_table = new QTableWidget(0, 4);
    m_table->setHorizontalHeaderLabels({ tr("Zutat"), tr("Gewicht"), tr("Lebensmittel"), tr("kcal") });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setToolTip(tr("Doppelklick: Lebensmittel für die Nährwerte zuordnen"));
    m_table->verticalHeader()->setDefaultSectionSize(m_table->fontMetrics().height() + 6);
    m_table->setWordWrap(false);
    nl->addWidget(m_table, 1);
    auto *assign = new QPushButton(Icons::get(QStringLiteral("nutrition")), tr("Zuordnung ändern …"));
    nl->addWidget(assign);
    split->addWidget(nutBox);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 4);
    split->setStretchFactor(2, 3);
    lay->addWidget(split, 1);
    const QByteArray st = QSettings().value(QStringLiteral("editor/split")).toByteArray();
    if (!st.isEmpty()) split->restoreState(st);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &EditorDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this, &QDialog::finished, this, [split] { QSettings().setValue(QStringLiteral("editor/split"), split->saveState()); });

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(350);
    connect(m_timer, &QTimer::timeout, this, &EditorDialog::analyze);
    connect(m_ingredients, &QPlainTextEdit::textChanged, m_timer, qOverload<>(&QTimer::start));
    connect(m_servings, qOverload<double>(&QDoubleSpinBox::valueChanged), m_timer, qOverload<>(&QTimer::start));
    connect(pick, &QPushButton::clicked, this, &EditorDialog::chooseImage);
    connect(clear, &QPushButton::clicked, this, [this] {
        m_recipe.image.clear();
        updateImage();
    });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) { assignFood(row); });
    connect(assign, &QPushButton::clicked, this, [this] {
        if (m_table->currentRow() >= 0) assignFood(m_table->currentRow());
    });
    connect(autoTags, &QPushButton::clicked, this, [this] {
        collect();
        Nutrition::applyTagSuggestions(m_recipe);
        m_tags->setText(m_recipe.tags.join(QStringLiteral(", ")));
    });
    updateImage();
    analyze();
    resize(1180, 760);
}

void EditorDialog::updateImage()
{
    if (m_recipe.image.isEmpty()) {
        m_image->setPixmap(QPixmap());
        m_image->setText(tr("Kein Bild"));
        return;
    }
    QPixmap pm;
    pm.loadFromData(QByteArray::fromBase64(m_recipe.image.toLatin1()));
    m_image->setPixmap(pm.scaled(m_image->size() - QSize(4, 4), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void EditorDialog::chooseImage()
{
    QSettings s;
    QString dir = s.value(QStringLiteral("editor/imageDir"), QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
    const QString file = QFileDialog::getOpenFileName(this, tr("Rezeptbild wählen"), dir, tr("Bilder (*.png *.jpg *.jpeg *.webp *.bmp *.gif)"));
    if (file.isEmpty()) return;
    s.setValue(QStringLiteral("editor/imageDir"), QFileInfo(file).absolutePath());
    QImageReader reader(file);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) {
        QMessageBox::warning(this, windowTitle(), tr("Das Bild kann nicht gelesen werden: %1").arg(reader.errorString()));
        return;
    }
    // auf 800 px verkleinern und als JPEG einbetten
    if (img.width() > 800 || img.height() > 800) img = img.scaled(800, 800, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    img.convertToFormat(QImage::Format_RGB32).save(&buf, "JPG", 85);
    m_recipe.image = QString::fromLatin1(ba.toBase64());
    updateImage();
}

void EditorDialog::collect()
{
    m_recipe.title = m_title->text().trimmed();
    m_recipe.servings = m_servings->value();
    const QString y = m_yield->currentText().trimmed();
    m_recipe.yieldUnit = (y == tr("Portionen") || y == tr("Portion")) ? QString() : y;
    m_recipe.prepMinutes = m_prep->value();
    m_recipe.tags.clear();
    for (const QString &t : m_tags->text().split(QLatin1Char(','), Qt::SkipEmptyParts)) m_recipe.addTag(t);
    m_recipe.source = m_source->text().trimmed();
    m_recipe.favorite = m_fav->isChecked();
    m_recipe.instructions = m_instructions->toPlainText().trimmed();
    // Zutaten neu zerlegen; Originalzeilen und manuelle Zuordnungen erhalten
    QHash<QString, QString> originals;
    for (const Ingredient &i : m_recipe.ingredients)
        if (!i.original.isEmpty()) originals.insert(i.displayLine(), i.original);
    QList<Ingredient> list = IngredientParser::parseText(m_ingredients->toPlainText());
    for (Ingredient &i : list) {
        auto it = m_manual.constFind(i.name.toLower());
        if (it != m_manual.constEnd()) {
            i.food = *it;
            i.foodManual = !it->isEmpty();
        }
        i.original = originals.value(i.displayLine());
    }
    m_recipe.ingredients = list;
    if (m_recipe.language == QLatin1String("en") && !Glossary::looksEnglish(m_recipe.instructions)) m_recipe.language = QStringLiteral("de");
}

void EditorDialog::analyze()
{
    collect();
    Nutrition::analyze(m_recipe);
    const NutritionInfo &n = m_recipe.nutrition;
    m_table->setRowCount(0);
    const FoodTable &ft = FoodTable::instance();
    for (const Ingredient &i : m_recipe.ingredients) {
        if (i.heading) continue;
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        auto *name = new QTableWidgetItem(i.name);
        name->setToolTip(i.displayLine());
        name->setData(Qt::UserRole, i.name);
        m_table->setItem(r, 0, name);
        auto *g = new QTableWidgetItem(i.grams > 0 ? Fmt::number(i.grams, 0) + QStringLiteral(" g") : QStringLiteral("–"));
        g->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(r, 1, g);
        const Food *f = i.food.isEmpty() ? nullptr : ft.find(i.food);
        auto *fi = new QTableWidgetItem(f ? f->name + (i.foodManual ? QStringLiteral(" ✓") : QString()) : tr("— nicht zugeordnet —"));
        const bool counted = i.amount > 0 && Units::kindOf(i.unit) != Units::Kind::Vague;
        if (!f && counted) fi->setForeground(QColor(0xd0, 0x40, 0x20));
        if (!f && !counted) fi->setText(tr("(ohne Menge)"));
        if (f && i.grams <= 0 && counted) {
            fi->setForeground(QColor(0xd0, 0x80, 0x10));
            fi->setToolTip(tr("Menge kann nicht in Gramm umgerechnet werden – bitte Gewicht angeben (z. B. „1 Stück (150 g)“)."));
        }
        m_table->setItem(r, 2, fi);
        const double kcal = f && i.grams > 0 ? f->per100.kcal * i.grams / 100.0 : 0;
        auto *k = new QTableWidgetItem(kcal > 0 ? Fmt::kcal(kcal) : QString());
        k->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(r, 3, k);
    }
    m_table->resizeColumnToContents(1);
    m_table->resizeColumnToContents(3);
    const Nutrients &p = n.perServing;
    QString s = tr("<b style=\"font-size:large;\">%1 kcal / %2 kJ</b> je Portion<br/>"
                   "Eiweiß %3 g · Fett %4 g (ges. %5 g) · Kohlenhydrate %6 g (Zucker %7 g) · Ballaststoffe %8 g · Salz %9 g")
                    .arg(Fmt::kcal(p.kcal), Fmt::kcal(p.kJ()), Fmt::number(p.protein), Fmt::number(p.fat), Fmt::number(p.satFat),
                         Fmt::number(p.carbs), Fmt::number(p.sugar), Fmt::number(p.fiber), Fmt::number(p.salt, 2));
    s += QStringLiteral("<br/>") + tr("Gesamt: %1 kcal · %2 von %3 Zutaten berechnet").arg(Fmt::kcal(n.total.kcal)).arg(n.matched).arg(n.countable);
    if (!n.unmatched.isEmpty())
        s += QStringLiteral("<br/><span style=\"color:#d04020;\">%1</span>").arg(tr("Ohne Nährwerte: %1").arg(n.unmatched.join(QStringLiteral(", ")).toHtmlEscaped()));
    m_summary->setText(s);
}

void EditorDialog::assignFood(int row)
{
    if (row < 0 || row >= m_table->rowCount()) return;
    const QString name = m_table->item(row, 0)->data(Qt::UserRole).toString();
    QString current;
    for (const Ingredient &i : m_recipe.ingredients)
        if (!i.heading && i.name == name) current = i.food;
    FoodPickerDialog d(name, current, this);
    if (d.exec() != QDialog::Accepted) return;
    const QString key = d.selectedKey();
    m_manual.insert(name.toLower(), key);
    if (d.remember()) FoodTable::instance().learn(name, key);
    analyze();
}

void EditorDialog::accept()
{
    collect();
    if (m_recipe.title.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), tr("Bitte einen Titel eingeben."));
        m_title->setFocus();
        return;
    }
    if (m_recipe.sourceFormat.isEmpty()) m_recipe.sourceFormat = QStringLiteral("VitaMeals");
    Nutrition::analyze(m_recipe);
    QDialog::accept();
}
