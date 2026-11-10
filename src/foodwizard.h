// ViviaMeals – Assistent für eigene Lebensmittel und Liste fehlender Lebensmittel
//
// FoodWizard führt in vier Schritten zu einem eigenen Eintrag der Nährwerttabelle – gedacht für
// Fertigprodukte („Buttaris“, „Fix für Bolognese“, eine bestimmte Fertigsoße):
//   1. Bezeichnung, Marke, Kategorie, weitere Namen (oder doch ein vorhandenes Lebensmittel zuordnen)
//   2. Nährwerte wie auf der Verpackung: je 100 g, je 100 ml oder je Portion; Energie in kcal oder kJ;
//      Fett, gesättigte Fettsäuren, Kohlenhydrate, Zucker, Ballaststoffe, Eiweiß, Salz (oder Natrium).
//      Laufende Plausibilitätsprüfung (Energie aus den Nährstoffen, Zucker ≤ KH, gesättigte ≤ Fett …)
//   3. Dichte, Packungsinhalt, Stückgewicht – damit „1 Pck.“, „2 EL“ oder „1 Würfel“ umgerechnet werden
//   4. Übersicht; die Zutat aus dem Rezept wird auf Wunsch künftig immer diesem Eintrag zugeordnet
//
// MissingFoodsDialog listet Zutaten ohne Nährwerte (nach dem Import, beim Speichern eines neuen
// Rezepts, im Editor) und ruft für jede den Assistenten bzw. die Zuordnung auf; Zutaten, die keine
// Lebensmittel sind („Alufolie“), lassen sich dauerhaft ausblenden.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "foodtable.h"
#include "nutrition.h"
#include "openfoodfacts.h"

#include <QDialog>
#include <QSet>
#include <QWizard>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QTableWidget;

class FoodWizard : public QWizard
{
    Q_OBJECT
public:
    // ingredientName: Zutat aus einem Rezept (leer = frei anlegen)
    explicit FoodWizard(const QString &ingredientName = QString(), QWidget *parent = nullptr, bool offerExisting = true);
    void setInitialName(const QString &name);
    void setEntered(const Nutrients &n, const QString &brand = QString());   // Vorbelegung (Bildschirmfotos, Tests)
    // Angaben eines Open-Food-Facts-Produkts übernehmen (auch für Bildschirmfotos)
    void applyProduct(const OpenFoodFacts::Product &p);   // Vorbelegung ohne Bezug zu einer Rezeptzutat
    // Schlüssel des angelegten bzw. zugeordneten Lebensmittels (leer = abgebrochen)
    QString foodKey() const { return m_result; }
    bool createdNew() const { return m_created; }
    Food food() const;   // aus den Eingaben (für Tests/Vorschau)

    // Nährwerte auf 100 g umrechnen: basis 0 = je 100 g, 1 = je 100 ml, 2 = je Portion (portionGrams)
    static Nutrients toPer100g(const Nutrients &entered, int basis, double portion, double density);
    static QString guessCategory(const QString &name);

protected:
    void accept() override;

private:
    QWizardPage *namePage();
    QWizardPage *valuePage();
    QWizardPage *packPage();
    QWizardPage *summaryPage();
    Nutrients entered() const;
    Nutrients per100() const;
    void updateCheck();
    void updateSummary();
    QString check(QStringList *warnings) const;   // Fehler (leer = in Ordnung)

    QString m_ingredient;
    bool m_offerExisting = true;
    QString m_result;
    bool m_created = false;
    QLineEdit *m_name = nullptr, *m_brand = nullptr, *m_aliases = nullptr;
    QComboBox *m_category = nullptr;
    QComboBox *m_basis = nullptr;
    QDoubleSpinBox *m_portion = nullptr;
    QComboBox *m_energyUnit = nullptr;
    QDoubleSpinBox *m_values[8] = {};   // Energie, Fett, ges. FS, KH, Zucker, Ballaststoffe, Eiweiß, Salz
    QCheckBox *m_sodium = nullptr;
    QLabel *m_check = nullptr;
    QDoubleSpinBox *m_density = nullptr, *m_pack = nullptr, *m_piece = nullptr;
    QComboBox *m_packUnit = nullptr;
    QLabel *m_summary = nullptr;
    QLineEdit *m_barcode = nullptr;
    QLabel *m_offStatus = nullptr;
    OpenFoodFacts::Client *m_off = nullptr;
    QString m_code;          // Barcode des übernommenen Produkts
    QString m_offBrand;      // Marke laut Open Food Facts (für die Herkunft)
    void offLookup();
    void offSearch();
    void setOffStatus(const QString &html, bool busy = false);
    QCheckBox *m_learn = nullptr;
};

class MissingFoodsDialog : public QDialog
{
    Q_OBJECT
public:
    MissingFoodsDialog(const QList<Nutrition::MissingFood> &missing, const QString &intro, QWidget *parent = nullptr);
    bool changed() const { return m_changed; }

    // dauerhaft ausgeblendete Zutaten („kein Lebensmittel“), vereinfachte Namen
    static QSet<QString> ignoredNames();
    static void setIgnored(const QString &key, bool ignored);
    // Einstellung „fehlende Lebensmittel anbieten“ (nach dem Import und beim Speichern neuer Rezepte)
    static bool offerEnabled();
    static void setOfferEnabled(bool on);

private:
    void create();
    void assign();
    void ignore();
    int currentRow() const;
    void setStatus(int row, const QString &text);
    QTableWidget *m_table;
    QList<Nutrition::MissingFood> m_missing;
    bool m_changed = false;
};
