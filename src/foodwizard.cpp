// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "foodwizard.h"
#include "fooddialog.h"
#include "icons.h"

#include <QApplication>
#include <QCheckBox>
#include <QDesktopServices>
#include <QGroupBox>
#include <QKeyEvent>
#include <QUrl>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>

#include <functional>

namespace {

// Seite mit eigener Prüfung beim „Weiter“
class CheckedPage : public QWizardPage
{
public:
    std::function<bool()> validator;
    bool validatePage() override { return validator ? validator() : true; }
};

// Eingabefeld für den Barcode: Eingabetaste (auch von Handscannern) startet die Abfrage statt „Weiter“
class BarcodeEdit : public QLineEdit
{
public:
    std::function<void()> onEnter;
protected:
    void keyPressEvent(QKeyEvent *e) override
    {
        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && onEnter) {
            onEnter();
            e->accept();
            return;
        }
        QLineEdit::keyPressEvent(e);
    }
};

// Trefferliste der Namenssuche
class OffSearchDialog : public QDialog
{
public:
    OffSearchDialog(const QString &terms, QWidget *parent) : QDialog(parent)
    {
        setWindowTitle(QObject::tr("Open Food Facts – Suche nach „%1“").arg(terms));
        setObjectName(QStringLiteral("OffSearchDialog"));   // Größe und Position merken (WindowState)
        auto *lay = new QVBoxLayout(this);
        m_status = new QLabel(QObject::tr("Suche läuft …"));
        lay->addWidget(m_status);
        m_table = new QTableWidget(0, 4);
        m_table->setHorizontalHeaderLabels({ QObject::tr("Produkt"), QObject::tr("Marke"), QObject::tr("Inhalt"), QObject::tr("kcal/100") });
        m_table->verticalHeader()->hide();
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        lay->addWidget(m_table, 1);
        auto *note = new QLabel(QObject::tr("<small>Daten: Open Food Facts, von Freiwilligen erfasst (ODbL). Gesendet wird nur der Suchbegriff.</small>"));
        lay->addWidget(note);
        auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        bb->button(QDialogButtonBox::Ok)->setText(QObject::tr("Übernehmen"));
        bb->button(QDialogButtonBox::Ok)->setEnabled(false);
        lay->addWidget(bb);
        connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(m_table, &QTableWidget::cellDoubleClicked, this, &QDialog::accept);
        connect(m_table, &QTableWidget::itemSelectionChanged, this, [this, bb] { bb->button(QDialogButtonBox::Ok)->setEnabled(selected() >= 0); });
        auto *client = new OpenFoodFacts::Client(this);
        connect(client, &OpenFoodFacts::Client::searchFinished, this, [this](const QList<OpenFoodFacts::Product> &list) {
            m_results = list;
            m_status->setText(list.isEmpty() ? QObject::tr("Keine Treffer – anderen Begriff oder den Barcode versuchen.")
                                             : QObject::tr("%1 Treffer – bitte das passende Produkt wählen (Inhalt und Marke vergleichen):").arg(list.size()));
            for (const auto &p : list) {
                const int r = m_table->rowCount();
                m_table->insertRow(r);
                m_table->setItem(r, 0, new QTableWidgetItem(p.name));
                m_table->setItem(r, 1, new QTableWidgetItem(p.brand));
                m_table->setItem(r, 2, new QTableWidgetItem(p.quantity));
                auto *k = new QTableWidgetItem(p.hasNutrition ? Fmt::number(p.per100.kcal, 0) : QObject::tr("–"));
                k->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                m_table->setItem(r, 3, k);
            }
            m_table->resizeColumnToContents(1);
            m_table->resizeColumnToContents(2);
            if (!list.isEmpty()) m_table->selectRow(0);
        });
        connect(client, &OpenFoodFacts::Client::failed, this, [this](const QString &e) { m_status->setText(e); });
        client->search(terms);
        resize(720, 460);
    }
    int selected() const
    {
        const auto sel = m_table->selectionModel()->selectedRows();
        return sel.isEmpty() ? -1 : sel.first().row();
    }
    OpenFoodFacts::Product product() const { return m_results.value(selected()); }

private:
    QLabel *m_status;
    QTableWidget *m_table;
    QList<OpenFoodFacts::Product> m_results;
};

const char *const kValueLabels[8] = { "Energie:", "Fett:", "davon gesättigte Fettsäuren:", "Kohlenhydrate:", "davon Zucker:",
                                      "Ballaststoffe:", "Eiweiß:", "Salz:" };

QString cap(const QString &s)
{
    QString t = s.simplified();
    if (!t.isEmpty()) t[0] = t.at(0).toUpper();
    return t;
}

} // namespace

// ================================================================ FoodWizard

FoodWizard::FoodWizard(const QString &ingredientName, QWidget *parent, bool offerExisting)
    : QWizard(parent), m_ingredient(ingredientName.trimmed()), m_offerExisting(offerExisting)
{
    setWindowTitle(tr("Eigenes Lebensmittel anlegen"));
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoBackButtonOnStartPage, true);
    setButtonText(QWizard::FinishButton, tr("Anlegen"));
    setButtonText(QWizard::NextButton, tr("Weiter >"));
    setButtonText(QWizard::BackButton, tr("< Zurück"));
    setButtonText(QWizard::CancelButton, tr("Abbrechen"));
    setPixmap(QWizard::LogoPixmap, Icons::get(QStringLiteral("nutrition")).pixmap(48, 48));
    addPage(namePage());
    addPage(valuePage());
    addPage(packPage());
    addPage(summaryPage());
    connect(this, &QWizard::currentIdChanged, this, [this](int id) {
        if (id == 3) updateSummary();
    });
    resize(640, 600);
}

QString FoodWizard::guessCategory(const QString &name)
{
    const QString n = name.toLower();
    struct Rule { const char *cat; const char *re; };
    static const Rule rules[] = {
        { "fett", "butter|margarine|öl\\b|fett|schmalz|buttaris|rama|palmin" },
        { "sosse", "soße|sauce|fix\\b|fix für|dressing|brühe|fond|würze|ketchup|senf|mayonnaise|paste" },
        { "milch", "milch|joghurt|quark|sahne|creme fraiche|crème fraîche|schmand|kefir|pudding" },
        { "kaese", "käse|gouda|emmentaler|mozzarella|feta|parmesan|camembert" },
        { "fleisch", "wurst|schinken|salami|fleisch|hack|speck|hähnchen|pute|rind|schwein|lamm" },
        { "fisch", "fisch|lachs|thunfisch|hering|forelle|garnele|krabben" },
        { "getreide", "nudel|spaghetti|reis|mehl|grieß|haferflocken|müsli|couscous|bulgur" },
        { "backwaren", "brot|brötchen|toast|knäcke|zwieback|teig|kuchenboden|keks|waffel" },
        { "suess", "schokolade|riegel|bonbon|gummi|praline|nougat|marzipan|kuchenglasur|eis\\b" },
        { "zucker", "zucker|sirup|süßstoff" },
        { "getraenk", "saft|limonade|cola|schorle|tee\\b|kaffee|smoothie" },
        { "alkohol", "wein|bier|likör|schnaps|rum|sekt" },
        { "gemuese", "gemüse|tomate|paprika|zwiebel|möhre|erbsen|mais|bohnen|spinat|kohl" },
        { "obst", "obst|apfel|birne|beeren|kirsch|pfirsich|ananas|banane|mango" },
        { "huelsen", "tofu|soja|linsen|kichererbsen|seitan|tempeh" },
        { "nuss", "nuss|nüsse|mandel|sesam|samen|kerne" },
        { "gewuerz", "gewürz|pfeffer|salz|curry|paprikapulver" },
    };
    for (const Rule &r : rules)
        if (n.contains(QRegularExpression(QString::fromUtf8(r.re)))) return QLatin1String(r.cat);
    return QStringLiteral("sonstiges");
}

QWizardPage *FoodWizard::namePage()
{
    auto *p = new CheckedPage;
    p->setTitle(tr("Bezeichnung"));
    p->setSubTitle(m_ingredient.isEmpty() ? tr("Wie heißt das Lebensmittel bzw. Produkt?")
                                          : tr("Für die Zutat „%1“ gibt es noch keine Nährwerte. Legen Sie sie hier an – z. B. "
                                               "mit den Angaben von der Verpackung.")
                                                .arg(m_ingredient));
    auto *form = new QFormLayout(p);
    m_name = new QLineEdit(cap(m_ingredient));
    m_name->setPlaceholderText(tr("z. B. Buttaris, Fix für Spaghetti Bolognese"));
    form->addRow(tr("&Name:"), m_name);
    m_brand = new QLineEdit;
    m_brand->setPlaceholderText(tr("optional, z. B. Tegut, Knorr, Maggi"));
    form->addRow(tr("&Marke/Hersteller:"), m_brand);
    m_category = new QComboBox;
    for (const QString &c : FoodTable::categories()) m_category->addItem(FoodTable::categoryName(c), c);
    m_category->setCurrentIndex(qMax(0, m_category->findData(guessCategory(m_ingredient))));
    form->addRow(tr("&Kategorie:"), m_category);
    m_aliases = new QLineEdit;
    m_aliases->setPlaceholderText(tr("weitere Namen, durch Komma getrennt (z. B. Butterschmalz-Ersatz)"));
    form->addRow(tr("&Andere Namen:"), m_aliases);
    auto *hint = new QLabel(tr("<small>Unter diesen Namen erkennt ViviaMeals das Lebensmittel künftig in allen Rezepten – auch "
                               "in Zusammensetzungen („Buttaris-Flöckchen“).</small>"));
    hint->setWordWrap(true);
    form->addRow(hint);

    // Open Food Facts: Barcode oder Name → Nährwerte übernehmen
    auto *off = new QGroupBox(tr("Nährwerte aus Open Food Facts holen (optional)"));
    auto *ol = new QVBoxLayout(off);
    auto *orow = new QHBoxLayout;
    auto *barcode = new BarcodeEdit;
    m_barcode = barcode;
    m_barcode->setPlaceholderText(tr("Barcode (EAN), z. B. 3017620422003"));
    m_barcode->setToolTip(tr("Die Ziffern unter dem Strichcode eintippen – oder mit einem USB-Barcodescanner einlesen"));
    m_barcode->setClearButtonEnabled(true);
    orow->addWidget(m_barcode, 1);
    auto *fetch = new QPushButton(Icons::get(QStringLiteral("web")), tr("&Abrufen"));
    orow->addWidget(fetch);
    auto *byName = new QPushButton(Icons::get(QStringLiteral("search")), tr("Nach &Namen suchen …"));
    byName->setToolTip(tr("Open Food Facts nach dem eingegebenen Namen (und der Marke) durchsuchen"));
    orow->addWidget(byName);
    ol->addLayout(orow);
    m_offStatus = new QLabel(tr("<small>Gesendet wird nur der Barcode bzw. Suchbegriff. Die Daten werden von Freiwilligen erfasst "
                                "(Open Database License) – bitte kurz mit der Verpackung vergleichen.</small>"));
    m_offStatus->setWordWrap(true);
    m_offStatus->setTextFormat(Qt::RichText);
    m_offStatus->setOpenExternalLinks(true);
    ol->addWidget(m_offStatus);
    form->addRow(off);
    barcode->onEnter = [this] { offLookup(); };
    connect(fetch, &QPushButton::clicked, this, &FoodWizard::offLookup);
    connect(byName, &QPushButton::clicked, this, &FoodWizard::offSearch);
    if (!m_ingredient.isEmpty() && m_offerExisting) {
        auto *existing = new QPushButton(Icons::get(QStringLiteral("search")), tr("Doch ein vorhandenes Lebensmittel zuordnen …"));
        existing->setToolTip(tr("Wenn die Zutat nur anders heißt als ein Eintrag der Tabelle (z. B. „Pflanzenfett“ → Margarine)"));
        form->addRow(existing);
        connect(existing, &QPushButton::clicked, this, [this] {
            FoodPickerDialog d(m_ingredient, QString(), this);
            if (d.exec() != QDialog::Accepted || d.selectedKey().isEmpty()) return;
            m_result = d.selectedKey();
            if (d.remember()) FoodTable::instance().learn(m_ingredient, m_result);
            QDialog::accept();   // Assistent ohne neuen Eintrag beenden
        });
    }
    p->validator = [this] {
        if (m_name->text().trimmed().isEmpty()) {
            QMessageBox::information(this, windowTitle(), tr("Bitte einen Namen eingeben."));
            return false;
        }
        return true;
    };
    return p;
}

QWizardPage *FoodWizard::valuePage()
{
    auto *p = new CheckedPage;
    p->setTitle(tr("Nährwerte"));
    p->setSubTitle(tr("Übertragen Sie die Angaben von der Verpackung (Nährwerttabelle). Unbekannte Werte bei 0 lassen."));
    auto *lay = new QVBoxLayout(p);
    auto *bl = new QHBoxLayout;
    auto *basisLabel = new QLabel(tr("Angaben &je:"));
    bl->addWidget(basisLabel);
    m_basis = new QComboBox;
    basisLabel->setBuddy(m_basis);
    m_basis->addItem(tr("100 g"));
    m_basis->addItem(tr("100 ml"));
    m_basis->addItem(tr("Portion von …"));
    bl->addWidget(m_basis);
    m_portion = new QDoubleSpinBox;
    m_portion->setRange(1, 5000);
    m_portion->setDecimals(0);
    m_portion->setValue(30);
    m_portion->setSuffix(tr(" g"));
    m_portion->setEnabled(false);
    bl->addWidget(m_portion);
    bl->addStretch(1);
    lay->addLayout(bl);
    auto *form = new QFormLayout;
    for (int i = 0; i < 8; ++i) {
        m_values[i] = new QDoubleSpinBox;
        m_values[i]->setRange(0, i == 0 ? 4000 : 100);
        m_values[i]->setDecimals(i == 0 ? 0 : (i == 7 ? 2 : 1));
        m_values[i]->setSingleStep(i == 0 ? 10 : 0.5);
        m_values[i]->setSuffix(i == 0 ? QString() : tr(" g"));
        m_values[i]->setAlignment(Qt::AlignRight);
        if (i == 0) {
            auto *row = new QHBoxLayout;
            row->addWidget(m_values[i], 1);
            m_energyUnit = new QComboBox;
            m_energyUnit->addItem(QStringLiteral("kcal"));
            m_energyUnit->addItem(QStringLiteral("kJ"));
            row->addWidget(m_energyUnit);
            form->addRow(tr(kValueLabels[i]), row);
        } else {
            form->addRow(tr(kValueLabels[i]), m_values[i]);
        }
    }
    m_sodium = new QCheckBox(tr("Angabe ist Natrium statt Salz (Salz = Natrium × 2,5)"));
    form->addRow(QString(), m_sodium);
    lay->addLayout(form);
    m_check = new QLabel;
    m_check->setWordWrap(true);
    m_check->setTextFormat(Qt::RichText);
    lay->addWidget(m_check);
    lay->addStretch(1);
    connect(m_basis, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int i) {
        m_portion->setEnabled(i == 2);
        for (int k = 1; k < 8; ++k) m_values[k]->setRange(0, i == 2 ? 2000 : 100);
        updateCheck();
    });
    connect(m_portion, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &FoodWizard::updateCheck);
    connect(m_energyUnit, qOverload<int>(&QComboBox::currentIndexChanged), this, &FoodWizard::updateCheck);
    connect(m_sodium, &QCheckBox::toggled, this, &FoodWizard::updateCheck);
    for (QDoubleSpinBox *v : m_values) connect(v, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &FoodWizard::updateCheck);
    p->validator = [this] {
        QStringList warnings;
        const QString err = check(&warnings);
        if (!err.isEmpty()) {
            QMessageBox::warning(this, windowTitle(), err);
            return false;
        }
        if (!warnings.isEmpty())
            return QMessageBox::question(this, windowTitle(), tr("%1\n\nTrotzdem weiter?").arg(warnings.join(QLatin1Char('\n')))) == QMessageBox::Yes;
        return true;
    };
    updateCheck();
    return p;
}

QWizardPage *FoodWizard::packPage()
{
    auto *p = new QWizardPage;
    p->setTitle(tr("Menge und Packung"));
    p->setSubTitle(tr("Damit ViviaMeals Angaben wie „1 Pck.“, „2 EL“ oder „1 Würfel“ in Gramm umrechnen kann. Unbekanntes einfach leer lassen."));
    auto *form = new QFormLayout(p);
    m_density = new QDoubleSpinBox;
    m_density->setRange(0.05, 3);
    m_density->setDecimals(2);
    m_density->setSingleStep(0.05);
    m_density->setValue(1.0);
    m_density->setSuffix(tr(" g/ml"));
    m_density->setToolTip(tr("Gewicht je Milliliter: Wasser, Milch, Soßen ≈ 1,0 · Öl 0,92 · flüssige Butter/Margarine 0,9 · Sirup 1,3 · Mehl ≈ 0,55"));
    form->addRow(tr("&Dichte:"), m_density);
    auto *pr = new QHBoxLayout;
    m_pack = new QDoubleSpinBox;
    m_pack->setRange(0, 10000);
    m_pack->setDecimals(0);
    m_pack->setSuffix(tr(" g"));
    m_pack->setSpecialValueText(tr("unbekannt"));
    pr->addWidget(m_pack, 1);
    pr->addWidget(new QLabel(tr("je")));
    m_packUnit = new QComboBox;
    m_packUnit->setEditable(true);
    m_packUnit->addItems({ tr("Pck."), tr("Becher"), tr("Dose"), tr("Flasche"), tr("Glas"), tr("Tube"), tr("Beutel"), tr("Tafel"), tr("Würfel"),
                           tr("Riegel"), tr("Scheibe") });
    pr->addWidget(m_packUnit);
    form->addRow(tr("&Packungsinhalt:"), pr);
    m_piece = new QDoubleSpinBox;
    m_piece->setRange(0, 5000);
    m_piece->setDecimals(0);
    m_piece->setSuffix(tr(" g"));
    m_piece->setSpecialValueText(tr("unbekannt"));
    form->addRow(tr("Gewicht je &Stück:"), m_piece);
    auto *hint = new QLabel(tr("<small>Ein Esslöffel ergibt sich aus der Dichte (15 ml), ein Teelöffel ebenso (5 ml).</small>"));
    hint->setWordWrap(true);
    form->addRow(hint);
    connect(m_basis, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int i) {
        if (i == 2 && m_piece->value() == 0) m_piece->setValue(m_portion->value());
    });
    return p;
}

QWizardPage *FoodWizard::summaryPage()
{
    auto *p = new QWizardPage;
    p->setTitle(tr("Übersicht"));
    p->setSubTitle(tr("So wird das Lebensmittel in die Nährwerttabelle übernommen. „Anlegen“ speichert es."));
    auto *lay = new QVBoxLayout(p);
    m_summary = new QLabel;
    m_summary->setTextFormat(Qt::RichText);
    m_summary->setWordWrap(true);
    lay->addWidget(m_summary);
    m_learn = new QCheckBox;
    m_learn->setChecked(true);
    m_learn->setVisible(!m_ingredient.isEmpty());
    lay->addWidget(m_learn);
    lay->addStretch(1);
    return p;
}

Nutrients FoodWizard::entered() const
{
    Nutrients n;
    n.kcal = m_energyUnit->currentIndex() == 1 ? m_values[0]->value() / 4.184 : m_values[0]->value();
    n.fat = m_values[1]->value();
    n.satFat = m_values[2]->value();
    n.carbs = m_values[3]->value();
    n.sugar = m_values[4]->value();
    n.fiber = m_values[5]->value();
    n.protein = m_values[6]->value();
    n.salt = m_sodium->isChecked() ? m_values[7]->value() * 2.5 : m_values[7]->value();
    return n;
}

Nutrients FoodWizard::toPer100g(const Nutrients &e, int basis, double portion, double density)
{
    switch (basis) {
    case 1: return e.scaled(1.0 / qMax(0.05, density));          // 100 ml = 100 × Dichte g
    case 2: return e.scaled(100.0 / qMax(1.0, portion));
    default: return e;
    }
}

Nutrients FoodWizard::per100() const
{
    return toPer100g(entered(), m_basis->currentIndex(), m_portion->value(), m_density ? m_density->value() : 1.0);
}

QString FoodWizard::check(QStringList *warnings) const
{
    const Nutrients e = entered();
    if (e.satFat > e.fat + 0.05) return tr("Die gesättigten Fettsäuren können nicht mehr sein als das Fett insgesamt.");
    if (e.sugar > e.carbs + 0.05) return tr("Der Zucker kann nicht mehr sein als die Kohlenhydrate insgesamt.");
    const Nutrients n = per100();
    const double mass = n.fat + n.carbs + n.protein + n.fiber + n.salt;
    if (mass > 102) return tr("Fett, Kohlenhydrate, Eiweiß, Ballaststoffe und Salz ergeben zusammen %1 g je 100 g – das ist mehr als 100 g. "
                              "Bitte die Angaben (und „je 100 g / Portion“) prüfen.").arg(qRound(mass));
    if (n.kcal <= 0 && mass <= 0) return tr("Bitte mindestens die Energie (kcal oder kJ) eintragen.");
    if (n.kcal > 905) return tr("Mehr als 900 kcal je 100 g ist nicht möglich (reines Fett hat etwa 900 kcal).");
    const double calc = Nutrition::energyFromMacros(n);
    if (warnings && n.kcal > 0 && calc > 0 && std::abs(calc - n.kcal) > qMax(25.0, 0.25 * n.kcal))
        *warnings << tr("Die Energie (%1 kcal je 100 g) passt nicht recht zu den Nährstoffen (daraus berechnet etwa %2 kcal). "
                        "Steht die Angabe vielleicht in kJ, oder fehlt ein Wert?")
                         .arg(qRound(n.kcal))
                         .arg(qRound(calc));
    if (warnings && n.kcal <= 0 && mass > 0)
        *warnings << tr("Ohne Energieangabe wird sie aus den Nährstoffen berechnet (etwa %1 kcal je 100 g).").arg(qRound(calc));
    return QString();
}

void FoodWizard::updateCheck()
{
    QStringList warnings;
    const QString err = check(&warnings);
    const Nutrients n = per100();
    QString t = tr("Je 100 g: <b>%1 kcal</b> (%2 kJ) · Fett %3 g · KH %4 g · Eiweiß %5 g")
                    .arg(qRound(n.kcal))
                    .arg(qRound(n.kJ()))
                    .arg(Fmt::number(n.fat, 1), Fmt::number(n.carbs, 1), Fmt::number(n.protein, 1));
    if (m_basis->currentIndex() == 1) t += tr(" <small>(bei der Dichte aus dem nächsten Schritt)</small>");
    if (!err.isEmpty()) t += QStringLiteral("<br><span style=\"color:#c0392b\">✗ %1</span>").arg(err.toHtmlEscaped());
    else if (!warnings.isEmpty()) t += QStringLiteral("<br><span style=\"color:#b85400\">⚠ %1</span>").arg(warnings.join(QLatin1Char(' ')).toHtmlEscaped());
    else if (n.kcal > 0) t += QStringLiteral("<br><span style=\"color:#2e7d32\">✓ %1</span>").arg(tr("Angaben passen zusammen."));
    m_check->setText(t);
}

Food FoodWizard::food() const
{
    Food f;
    f.name = m_name->text().trimmed();
    const QString brand = m_brand->text().trimmed();
    if (!brand.isEmpty() && !f.name.contains(brand, Qt::CaseInsensitive)) f.name += QStringLiteral(" (%1)").arg(brand);
    f.category = m_category->currentData().toString();
    f.per100 = per100();
    if (f.per100.kcal <= 0) f.per100.kcal = Nutrition::energyFromMacros(f.per100);
    f.density = m_density->value();
    f.piece = m_piece->value();
    if (m_pack->value() > 0) {
        const QString unit = m_packUnit->currentText().trimmed();
        f.unitGrams.insert(unit.isEmpty() ? QStringLiteral("Pck.") : unit, m_pack->value());
        if (unit == tr("Pck.")) f.unitGrams.insert(QStringLiteral("Packung"), m_pack->value());
    }
    QStringList al{ m_name->text().trimmed().toLower() };
    if (!m_ingredient.isEmpty()) al << m_ingredient.toLower();
    for (const QString &a : m_aliases->text().split(QLatin1Char(','), Qt::SkipEmptyParts))
        if (!a.trimmed().isEmpty()) al << a.trimmed().toLower();
    al.removeDuplicates();
    f.aliases = al;
    if (!m_code.isEmpty()) {
        f.barcode = m_code;
        f.source = tr("Open Food Facts (ODbL), EAN %1").arg(m_code);
        if (!brand.isEmpty()) f.source += QStringLiteral(" – ") + brand;
    } else {
        f.source = brand.isEmpty() ? tr("eigene Eingabe") : tr("eigene Eingabe – %1").arg(brand);
    }
    f.user = true;
    return f;
}

void FoodWizard::updateSummary()
{
    const Food f = food();
    const Nutrients &n = f.per100;
    QString h = QStringLiteral("<h3>%1</h3><p>%2 · %3</p>").arg(f.name.toHtmlEscaped(), FoodTable::categoryName(f.category).toHtmlEscaped(),
                                                               f.source.toHtmlEscaped());
    h += QStringLiteral("<table cellspacing=\"0\" cellpadding=\"3\" border=\"1\" style=\"border-collapse:collapse;\"><tr><th></th><th>%1</th>")
             .arg(tr("je 100 g"));
    const double pack = m_pack->value();
    if (pack > 0) h += QStringLiteral("<th>%1</th>").arg(tr("je %1 (%2 g)").arg(m_packUnit->currentText().toHtmlEscaped()).arg(qRound(pack)));
    h += QStringLiteral("</tr>");
    const double v[8] = { n.kcal, n.fat, n.satFat, n.carbs, n.sugar, n.fiber, n.protein, n.salt };
    for (int i = 0; i < 8; ++i) {
        const QString unit = i == 0 ? QStringLiteral(" kcal") : QStringLiteral(" g");
        h += QStringLiteral("<tr><td>%1</td><td align=\"right\">%2%3</td>")
                 .arg(tr(kValueLabels[i]).chopped(1), Fmt::number(v[i], i == 0 ? 0 : (i == 7 ? 2 : 1)), unit);
        if (pack > 0) h += QStringLiteral("<td align=\"right\">%1%2</td>").arg(Fmt::number(v[i] * pack / 100.0, i == 0 ? 0 : 1), unit);
        h += QStringLiteral("</tr>");
    }
    h += QStringLiteral("</table><p>%1</p>").arg(tr("Erkannt unter: %1").arg(f.aliases.join(QStringLiteral(", "))).toHtmlEscaped());
    m_summary->setText(h);
    m_learn->setText(tr("Zutat „%1“ künftig immer diesem Lebensmittel zuordnen").arg(m_ingredient));
}

void FoodWizard::accept()
{
    Food f = food();
    // vorhandenen eigenen Eintrag gleichen Namens nicht stillschweigend überschreiben
    const QString base = QStringLiteral("u_") + FoodTable::normalize(f.name).replace(QLatin1Char(' '), QLatin1Char('_'));
    if (const Food *old = FoodTable::instance().find(base); old && old->user) {
        const auto b = QMessageBox::question(this, windowTitle(), tr("Es gibt bereits ein eigenes Lebensmittel „%1“. Ersetzen?").arg(old->name),
                                             QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (b == QMessageBox::Cancel) return;
        if (b == QMessageBox::No) {
            int n = 2;
            while (FoodTable::instance().find(QStringLiteral("%1_%2").arg(base).arg(n))) ++n;
            f.key = QStringLiteral("%1_%2").arg(base).arg(n);
        }
    }
    m_result = FoodTable::instance().saveUserFood(f);
    m_created = true;
    if (!m_ingredient.isEmpty() && m_learn->isChecked() && !m_result.isEmpty()) FoodTable::instance().learn(m_ingredient, m_result);
    QWizard::accept();
}

void FoodWizard::setInitialName(const QString &name)
{
    if (name.isEmpty()) return;
    m_name->setText(cap(name));
    m_category->setCurrentIndex(qMax(0, m_category->findData(guessCategory(name))));
}

void FoodWizard::setEntered(const Nutrients &n, const QString &brand)
{
    const double v[8] = { n.kcal, n.fat, n.satFat, n.carbs, n.sugar, n.fiber, n.protein, n.salt };
    for (int i = 0; i < 8; ++i) m_values[i]->setValue(v[i]);
    if (!brand.isEmpty()) m_brand->setText(brand);
}

void FoodWizard::setOffStatus(const QString &html, bool busy)
{
    m_offStatus->setText(busy ? QStringLiteral("⏳ ") + html : html);
}

void FoodWizard::offLookup()
{
    QString err;
    const QString code = OpenFoodFacts::normalizeBarcode(m_barcode->text(), &err);
    if (code.isEmpty()) {
        m_offStatus->setText(QStringLiteral("<span style=\"color:#c0392b\">✗ %1</span>").arg(err.toHtmlEscaped()));
        m_barcode->setFocus();
        return;
    }
    if (const Food *have = FoodTable::instance().findBarcode(code)) {
        if (QMessageBox::question(this, windowTitle(), tr("Dieses Produkt ist bereits als „%1“ angelegt. Diesen Eintrag verwenden?").arg(have->name))
            == QMessageBox::Yes) {
            m_result = have->key;
            if (!m_ingredient.isEmpty()) FoodTable::instance().learn(m_ingredient, m_result);
            QDialog::accept();
            return;
        }
    }
    if (!m_off) {
        m_off = new OpenFoodFacts::Client(this);
        connect(m_off, &OpenFoodFacts::Client::productFound, this, [this](const OpenFoodFacts::Product &p) {
            setOffStatus(QString());
            applyProduct(p);
        });
        connect(m_off, &OpenFoodFacts::Client::failed, this, [this](const QString &e) {
            QString t = QStringLiteral("<span style=\"color:#c0392b\">✗ %1</span>").arg(e.toHtmlEscaped());
            const QString code = OpenFoodFacts::normalizeBarcode(m_barcode->text());
            if (!code.isEmpty() && e.contains(tr("nicht erfasst")))
                t += QStringLiteral(" <a href=\"%1\">%2</a>").arg(OpenFoodFacts::productPage(code), tr("Produkt bei Open Food Facts eintragen"));
            t += QStringLiteral("<br><small>%1</small>").arg(tr("Die Werte lassen sich auf der nächsten Seite auch von Hand eintragen."));
            setOffStatus(t);
        });
    }
    setOffStatus(tr("Frage Open Food Facts nach %1 …").arg(code), true);
    m_off->lookup(code);
}

void FoodWizard::offSearch()
{
    QString terms = m_name->text().trimmed();
    if (!m_brand->text().trimmed().isEmpty() && !terms.contains(m_brand->text().trimmed(), Qt::CaseInsensitive))
        terms += QLatin1Char(' ') + m_brand->text().trimmed();
    if (terms.size() < 3) {
        QMessageBox::information(this, windowTitle(), tr("Bitte zuerst einen Namen (und gern die Marke) eingeben."));
        return;
    }
    OffSearchDialog d(terms, this);
    if (d.exec() != QDialog::Accepted || d.selected() < 0) return;
    const OpenFoodFacts::Product p = d.product();
    m_barcode->setText(p.code);
    if (const Food *have = FoodTable::instance().findBarcode(p.code);
        have && QMessageBox::question(this, windowTitle(), tr("Dieses Produkt ist bereits als „%1“ angelegt. Diesen Eintrag verwenden?").arg(have->name))
                    == QMessageBox::Yes) {
        m_result = have->key;
        if (!m_ingredient.isEmpty()) FoodTable::instance().learn(m_ingredient, m_result);
        QDialog::accept();
        return;
    }
    applyProduct(p);
}

void FoodWizard::applyProduct(const OpenFoodFacts::Product &p)
{
    m_code = p.code;
    m_barcode->setText(p.code);
    if (!p.name.isEmpty()) m_name->setText(p.name);
    m_brand->setText(p.brand);
    const QString cat = !p.category.isEmpty() ? p.category : guessCategory(p.name + QLatin1Char(' ') + m_ingredient);
    m_category->setCurrentIndex(qMax(0, m_category->findData(cat)));
    QString t = tr("✓ Gefunden: <b>%1</b>").arg((p.name + (p.brand.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(p.brand))).toHtmlEscaped());
    if (!p.quantity.isEmpty()) t += QStringLiteral(", ") + p.quantity.toHtmlEscaped();
    if (p.hasNutrition) {
        m_basis->setCurrentIndex(p.perMl ? 1 : 0);
        m_energyUnit->setCurrentIndex(0);
        m_sodium->setChecked(false);
        const Nutrients &n = p.per100;
        const double v[8] = { n.kcal, n.fat, n.satFat, n.carbs, n.sugar, n.fiber, n.protein, n.salt };
        for (int i = 0; i < 8; ++i) m_values[i]->setValue(v[i]);
        t += QStringLiteral(" – ") + (p.perMl ? tr("Nährwerte je 100 ml übernommen") : tr("Nährwerte je 100 g übernommen"));
        if (!p.missing.isEmpty()) t += QStringLiteral("<br><span style=\"color:#b85400\">⚠ %1</span>").arg(tr("Nicht erfasst: %1 – bitte von der Verpackung ergänzen.").arg(p.missing.join(QStringLiteral(", "))).toHtmlEscaped());
    } else {
        t += QStringLiteral("<br><span style=\"color:#b85400\">⚠ %1</span>").arg(tr("Für dieses Produkt sind noch keine Nährwerte erfasst – bitte auf der nächsten Seite von der Verpackung eintragen."));
    }
    if (p.packageAmount > 0) {
        m_pack->setValue(p.packageAmount * (p.perMl ? m_density->value() : 1.0));
        const QString unit = p.perMl ? tr("Flasche") : tr("Pck.");
        m_packUnit->setCurrentText(unit);
    }
    if (p.perMl && m_category->currentData().toString() != QLatin1String("fett")) m_density->setValue(1.0);
    t += QStringLiteral(" · <a href=\"%1\">%2</a>").arg(OpenFoodFacts::productPage(p.code), tr("Produktseite"));
    m_offStatus->setText(t);
    updateCheck();
}

// ================================================================ MissingFoodsDialog

QSet<QString> MissingFoodsDialog::ignoredNames()
{
    const QStringList l = QSettings().value(QStringLiteral("foods/ignored")).toStringList();
    return QSet<QString>(l.begin(), l.end());
}

void MissingFoodsDialog::setIgnored(const QString &key, bool ignored)
{
    QSet<QString> s = ignoredNames();
    if (ignored) s.insert(key);
    else s.remove(key);
    QStringList l(s.begin(), s.end());
    l.sort();
    QSettings().setValue(QStringLiteral("foods/ignored"), l);
}

bool MissingFoodsDialog::offerEnabled()
{
    return QSettings().value(QStringLiteral("foods/offerWizard"), true).toBool();
}

void MissingFoodsDialog::setOfferEnabled(bool on)
{
    QSettings().setValue(QStringLiteral("foods/offerWizard"), on);
}

MissingFoodsDialog::MissingFoodsDialog(const QList<Nutrition::MissingFood> &missing, const QString &intro, QWidget *parent)
    : QDialog(parent), m_missing(missing)
{
    setWindowTitle(tr("Zutaten ohne Nährwerte"));
    auto *lay = new QVBoxLayout(this);
    auto *info = new QLabel(intro + QStringLiteral("<br><small>")
                            + tr("<b>Anlegen</b> öffnet den Assistenten für ein eigenes Lebensmittel (z. B. ein Fertigprodukt mit den "
                                 "Werten von der Verpackung). <b>Zuordnen</b> wählt ein vorhandenes Lebensmittel, wenn die Zutat nur "
                                 "anders heißt. <b>Kein Lebensmittel</b> blendet Dinge wie „Alufolie“ künftig aus.")
                            + QStringLiteral("</small>"));
    info->setWordWrap(true);
    lay->addWidget(info);
    m_table = new QTableWidget(int(missing.size()), 4);
    m_table->setHorizontalHeaderLabels({ tr("Zutat"), tr("Anzahl"), tr("Beispiel"), tr("Erledigt") });
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (int i = 0; i < missing.size(); ++i) {
        const auto &m = missing[i];
        auto *n = new QTableWidgetItem(m.name);
        n->setToolTip(tr("in: %1").arg(m.recipes.mid(0, 8).join(QStringLiteral(", "))));
        m_table->setItem(i, 0, n);
        auto *c = new QTableWidgetItem(QString::number(m.count));
        c->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 1, c);
        m_table->setItem(i, 2, new QTableWidgetItem(m.examples.join(QStringLiteral(" | "))));
        m_table->setItem(i, 3, new QTableWidgetItem);
    }
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    if (!missing.isEmpty()) m_table->selectRow(0);
    lay->addWidget(m_table, 1);
    auto *bl = new QHBoxLayout;
    auto *create = new QPushButton(Icons::get(QStringLiteral("add")), tr("&Anlegen …"));
    auto *assign = new QPushButton(Icons::get(QStringLiteral("nutrition")), tr("&Zuordnen …"));
    auto *ignore = new QPushButton(Icons::get(QStringLiteral("close")), tr("&Kein Lebensmittel"));
    bl->addWidget(create);
    bl->addWidget(assign);
    bl->addWidget(ignore);
    bl->addStretch(1);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    bl->addWidget(bb);
    lay->addLayout(bl);
    auto *offer = new QCheckBox(tr("Nach dem Import und beim Speichern neuer Rezepte anbieten"));
    offer->setChecked(offerEnabled());
    lay->addWidget(offer);
    connect(offer, &QCheckBox::toggled, this, [](bool on) { setOfferEnabled(on); });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(create, &QPushButton::clicked, this, &MissingFoodsDialog::create);
    connect(assign, &QPushButton::clicked, this, &MissingFoodsDialog::assign);
    connect(ignore, &QPushButton::clicked, this, &MissingFoodsDialog::ignore);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &MissingFoodsDialog::create);
    resize(860, 480);
}

int MissingFoodsDialog::currentRow() const
{
    const auto sel = m_table->selectionModel()->selectedRows();
    return sel.isEmpty() ? -1 : sel.first().row();
}

void MissingFoodsDialog::setStatus(int row, const QString &text)
{
    m_table->item(row, 3)->setText(text);
    for (int c = 0; c < 4; ++c) m_table->item(row, c)->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
    // nächste offene Zeile
    for (int r = row + 1; r < m_table->rowCount(); ++r)
        if (m_table->item(r, 3)->text().isEmpty()) {
            m_table->selectRow(r);
            return;
        }
}

void MissingFoodsDialog::create()
{
    const int row = currentRow();
    if (row < 0) return;
    FoodWizard w(m_missing[row].name, this);
    if (w.exec() != QDialog::Accepted || w.foodKey().isEmpty()) return;
    m_changed = true;
    const Food *f = FoodTable::instance().find(w.foodKey());
    setStatus(row, (w.createdNew() ? tr("✓ angelegt: %1") : tr("✓ zugeordnet: %1")).arg(f ? f->name : w.foodKey()));
}

void MissingFoodsDialog::assign()
{
    const int row = currentRow();
    if (row < 0) return;
    FoodPickerDialog d(m_missing[row].name, QString(), this);
    if (d.exec() != QDialog::Accepted || d.selectedKey().isEmpty()) return;
    FoodTable::instance().learn(m_missing[row].name, d.selectedKey());
    m_changed = true;
    const Food *f = FoodTable::instance().find(d.selectedKey());
    setStatus(row, tr("✓ zugeordnet: %1").arg(f ? f->name : d.selectedKey()));
}

void MissingFoodsDialog::ignore()
{
    const int row = currentRow();
    if (row < 0) return;
    setIgnored(m_missing[row].key, true);
    setStatus(row, tr("ausgeblendet"));
}
