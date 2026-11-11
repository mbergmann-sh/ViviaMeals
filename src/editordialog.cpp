// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "editordialog.h"
#include "foodwizard.h"
#include "scandialog.h"
#include "screenfit.h"
#include "fooddialog.h"
#include "imagecropdialog.h"
#include "mealtypes.h"
#include "foodtable.h"
#include "glossary.h"
#include "icons.h"
#include "ingredientparser.h"
#include "nutrition.h"
#include "recipedb.h"
#include "units.h"
#include "umlauts.h"
#include "deepl.h"
#include "recipehtml.h"

#include <QApplication>
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

EditorDialog::EditorDialog(const Recipe &r, QWidget *parent) : QDialog(parent), m_recipe(r), m_isNew(r.id.isEmpty())
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
                                                                              QStringLiteral("salate"), QStringLiteral("zwischenmahlzeit"),
                                                                              QStringLiteral("süßspeise") },
                                this);
    comp->setCaseSensitivity(Qt::CaseInsensitive);
    m_tags->setCompleter(comp);
    auto *tl = new QHBoxLayout;
    tl->addWidget(m_tags, 1);
    auto *autoTags = new QPushButton(Icons::get(QStringLiteral("tag")), tr("Ergänzen"));
    autoTags->setToolTip(tr("Tags aus Titel, Zutaten und Nährwerten ableiten: vegan, vegetarisch, fleischgerichte, fisch, "
                            "frühstück, hauptgerichte, zwischenmahlzeit, desserts, süßspeise, backen sowie aus den Nährwerten "
                            "eiweißreich, kalorienarm, fettarm, hochkalorisch, vitaminreich, reduktionsdiät"));
    tl->addWidget(autoTags);
    form->addRow(tr("Tags:"), tl);
    m_source = new QLineEdit(sourceOrDefault(r.source));
    m_source->setClearButtonEnabled(true);
    if (!defaultSource().isEmpty())
        m_source->setToolTip(tr("Ohne eigene Angabe steht hier die Standard-Quelle „%1“ (Einstellungen → Allgemein). "
                                "Einfach überschreiben oder löschen.").arg(defaultSource()));
    form->addRow(tr("&Quelle:"), m_source);
    m_fav = new QCheckBox(tr("Favorit"));
    m_fav->setChecked(r.favorite);
    auto *fl = new QHBoxLayout;
    fl->addWidget(m_fav);
    fl->addStretch(1);
    auto *umlautBtn = new QPushButton(Icons::get(QStringLiteral("umlaut")), tr("Umlaute korrigieren"));
    umlautBtn->setToolTip(tr("ae, oe, ue und ss in Titel, Zutaten und Zubereitung zu ä, ö, ü und ß machen, wo es passt"));
    fl->addWidget(umlautBtn);
    auto *deeplBtn = new QPushButton(Icons::get(QStringLiteral("translate")), tr("Mit DeepL übersetzen"));
    deeplBtn->setToolTip(tr("Fremdsprachiges Rezept mit DeepL ins Deutsche übersetzen (API-Schlüssel in den Einstellungen)"));
    deeplBtn->setEnabled(DeepL::Settings::load().usable());
    if (!deeplBtn->isEnabled())
        deeplBtn->setToolTip(tr("Dazu unter Extras → Einstellungen die DeepL-Übersetzung einschalten und einen eigenen API-Schlüssel eintragen."));
    fl->addWidget(deeplBtn);
    form->addRow(QString(), fl);
    connect(umlautBtn, &QPushButton::clicked, this, &EditorDialog::fixUmlauts);
    connect(deeplBtn, &QPushButton::clicked, this, &EditorDialog::translateDeepL);
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
    m_split = split;
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
    auto *missing = new QPushButton(Icons::get(QStringLiteral("add")), tr("Fehlende Lebensmittel anlegen …"));
    missing->setToolTip(tr("Für Zutaten ohne Nährwerte eigene Lebensmittel anlegen (z. B. Fertigprodukte mit den Angaben der Verpackung)"));
    auto *nbl = new QHBoxLayout;
    nbl->addWidget(assign);
    nbl->addWidget(missing);
    nl->addLayout(nbl);
    connect(missing, &QPushButton::clicked, this, [this] { offerMissingFoods(false); });
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
    connect(this, &QDialog::finished, this, [split] {
        // mit Scanvorlage (4 Spalten) eigener Zustand
        QSettings().setValue(split->count() > 3 ? QStringLiteral("editor/splitScan") : QStringLiteral("editor/split"), split->saveState());
    });

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
        if (!Nutrition::isMenu(m_recipe)) MealTypes::applyTags(m_recipe);
        MealTypes::applyNutrientTags(m_recipe);
        m_tags->setText(m_recipe.tags.join(QStringLiteral(", ")));
    });
    updateImage();
    analyze();
    resize(1180, 760);
}

void EditorDialog::setReference(const QList<QImage> &pages)
{
    if (pages.isEmpty() || m_split->count() > 3) return;
    auto *box = new QGroupBox(tr("Vorlage (Scan)"));
    auto *bl = new QVBoxLayout(box);
    bl->addWidget(new ScanReferencePanel(pages));
    m_split->addWidget(box);
    const QByteArray st = QSettings().value(QStringLiteral("editor/splitScan")).toByteArray();
    if (st.isEmpty() || !m_split->restoreState(st)) {
        const int w = qMax(1000, m_split->width());
        m_split->setSizes({ w * 22 / 100, w * 30 / 100, w * 20 / 100, w * 28 / 100 });
    }
    setWindowTitle(tr("Gescanntes Rezept bearbeiten"));
    setProperty("windowStateKey", QStringLiteral("EditorDialogScan"));   // eigene Fenstergröße
    resize(qMax(width(), 1500), height());
    ScreenFit::fit(this);
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
    // zuschneiden; nur der Ausschnitt wird gespeichert (PNG), nicht das Original
    ImageCropDialog crop(img, this);
    if (crop.exec() != QDialog::Accepted) return;
    img = crop.result();
    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    img.convertToFormat(QImage::Format_RGB32).save(&buf, "PNG");
    m_recipe.image = QString::fromLatin1(ba.toBase64());
    updateImage();
}

QString EditorDialog::defaultSource()
{
    return QSettings().value(QStringLiteral("editor/defaultSource")).toString().trimmed();
}

QString EditorDialog::sourceOrDefault(const QString &source)
{
    return source.trimmed().isEmpty() ? defaultSource() : source;
}

void EditorDialog::showRecipe()
{
    m_title->setText(m_recipe.title);
    m_ingredients->setPlainText(m_recipe.ingredientsText());
    m_instructions->setPlainText(m_recipe.instructions);
    m_source->setText(sourceOrDefault(m_recipe.source));
    m_tags->setText(m_recipe.tags.join(QStringLiteral(", ")));
    if (!m_recipe.yieldUnit.isEmpty()) m_yield->setCurrentText(m_recipe.yieldUnit);
    analyze();
}

void EditorDialog::fixUmlauts()
{
    collect();
    const int n = Umlauts::fixRecipe(m_recipe);
    if (n == 0) {
        QMessageBox::information(this, tr("Umlaute"), tr("Es wurde nichts gefunden, das korrigiert werden müsste."));
        return;
    }
    showRecipe();
    QMessageBox::information(this, tr("Umlaute"), (n == 1 ? tr("Ein Wort korrigiert.") : tr("%1 Wörter korrigiert.").arg(n)) + QLatin1Char(' ')
                                 + tr("Bitte kurz durchsehen – „Speichern“ übernimmt die Änderung."));
}

void EditorDialog::translateDeepL()
{
    collect();
    const DeepL::Settings ds = DeepL::Settings::load();
    if (m_recipe.language == QLatin1String("de") && Glossary::detectLanguage(Umlauts::allRecipeText(m_recipe)) == QLatin1String("de")) {
        if (QMessageBox::question(this, tr("Mit DeepL übersetzen"),
                                  tr("Das Rezept scheint bereits deutschsprachig zu sein. Trotzdem an DeepL senden?"))
            != QMessageBox::Yes)
            return;
    }
    if (QMessageBox::question(this, tr("Mit DeepL übersetzen"),
                              tr("Titel, Zutaten und Zubereitung (etwa %1 Zeichen) werden an DeepL gesendet und durch die deutsche "
                                 "Übersetzung ersetzt. Fortfahren?")
                                  .arg(DeepL::characterCount(m_recipe)))
        != QMessageBox::Yes)
        return;
    // Englische Rezepte wurden beim Import ggf. schon über das Glossar übersetzt – DeepL erkennt die Sprache selbst
    if (m_recipe.originalLanguage.isEmpty() && m_recipe.language != QLatin1String("de")) m_recipe.originalLanguage = m_recipe.language;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString err;
    Recipe r = m_recipe;
    const bool ok = DeepL::translateRecipe(r, ds.key, &err);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, tr("Mit DeepL übersetzen"), tr("Die Übersetzung ist fehlgeschlagen:\n%1").arg(err));
        return;
    }
    m_manual.clear();   // Zutatennamen sind jetzt deutsch
    m_recipe = r;
    showRecipe();
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

bool EditorDialog::offerMissingFoods(bool ask)
{
    collect();
    Nutrition::analyze(m_recipe);
    const auto missing = Nutrition::missingFoods({ m_recipe }, MissingFoodsDialog::ignoredNames());
    if (missing.isEmpty()) {
        if (!ask) QMessageBox::information(this, windowTitle(), tr("Alle Zutaten haben Nährwerte."));
        return false;
    }
    if (ask) {
        QStringList names;
        for (const auto &m : missing) names << m.name;
        const auto b = QMessageBox::question(this, windowTitle(),
                                             (missing.size() == 1 ? tr("Für eine Zutat gibt es noch keine Nährwerte:\n%1\n\nJetzt ein eigenes Lebensmittel anlegen oder zuordnen?")
                                                                  : tr("Für %1 Zutaten gibt es noch keine Nährwerte:\n%2\n\nJetzt eigene Lebensmittel anlegen oder zuordnen?").arg(missing.size()))
                                                 .arg(names.mid(0, 8).join(QStringLiteral(", ")) + (names.size() > 8 ? QStringLiteral(" …") : QString())),
                                             QMessageBox::Yes | QMessageBox::No);
        if (b != QMessageBox::Yes) return false;
    }
    MissingFoodsDialog d(missing, tr("Diese Zutaten des Rezepts haben noch keine Nährwerte:"), this);
    d.exec();
    if (d.changed()) analyze();
    return d.changed();
}

void EditorDialog::accept()
{
    collect();
    if (m_recipe.title.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), tr("Bitte einen Titel eingeben."));
        m_title->setFocus();
        return;
    }
    if (m_recipe.sourceFormat.isEmpty()) m_recipe.sourceFormat = QStringLiteral("ViviaMeals");
    Nutrition::analyze(m_recipe);
    // neues Rezept: fehlende Lebensmittel einmalig anbieten
    if (m_isNew && !m_offered && MissingFoodsDialog::offerEnabled()) {
        m_offered = true;
        offerMissingFoods(true);
    }
    QDialog::accept();
}
