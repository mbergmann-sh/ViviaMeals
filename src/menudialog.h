// ViviaMeals – Menüassistent (Datei → Export → Menüassistent)
//   1. Rahmen: Titel, Anlass, Datum, Personen, 3, 4 oder 5 Gänge
//   2. Vorlieben: Ernährungsform, Charakter, Vorspeise, Hauptgang, Dessert, Thema, ausgeschlossene Zutaten
//   3. Menü: Vorschlag mit Hinweisen zur Harmonie; jeder Gang bleibt auswählbar und lässt sich durch
//      einen anderen Vorschlag oder ein frei gewähltes Rezept ersetzen; Kurzbeschreibungen änderbar
//   4. Abschluss: speichern, als LaTeX oder LibreOffice-Writer-Dokument exportieren
// MenuListDialog zeigt gespeicherte Menüs zum Öffnen und Löschen.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "menu.h"

#include <QDialog>
#include <QWizard>

#include <memory>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTimer;
class QTreeWidget;
class RecipeView;

class MenuWizard : public QWizard
{
    Q_OBJECT
public:
    enum PageId { PageFrame, PagePrefs, PageMenu, PageFinish };
    explicit MenuWizard(QWidget *parent = nullptr, const Menu::MenuData *existing = nullptr);
    const Menu::MenuData &menu() const { return m_menu; }
    // für Bildschirmfotos und Tests
    void proposeNow(quint32 seed = 0);
    void selectCourse(int row);

protected:
    void initializePage(int id) override;
    void accept() override;
    void reject() override;

private:
    QWizardPage *framePage();
    QWizardPage *prefsPage();
    QWizardPage *menuPage();
    QWizardPage *finishPage();
    Menu::Prefs prefsFromUi() const;
    void readFrame();
    void updateAvailability();
    void rebuildComposer();
    void fillTable();
    void updateNotes();
    void setRecipe(int row, const QString &id);
    void showPreview();
    void chooseRecipe();
    void exportLatex();
    void exportOdt();
    bool loadRecipes(QList<Recipe> *out);
    bool save(bool quiet);
    int currentRow() const;

    Menu::MenuData m_menu;
    std::unique_ptr<Menu::Composer> m_comp;
    bool m_prefsDirty = true;
    bool m_updatingTable = false;

    // Seite 1
    QLineEdit *m_title = nullptr, *m_occasion = nullptr;
    QCheckBox *m_useDate = nullptr;
    QDateEdit *m_date = nullptr;
    QSpinBox *m_persons = nullptr;
    QButtonGroup *m_courses = nullptr;
    QLabel *m_order = nullptr;
    // Seite 2
    QComboBox *m_diet = nullptr, *m_character = nullptr, *m_starter = nullptr, *m_main = nullptr, *m_dessert = nullptr, *m_theme = nullptr;
    QLineEdit *m_exclude = nullptr;
    QCheckBox *m_favorites = nullptr, *m_withImage = nullptr;
    QLabel *m_available = nullptr;
    QPushButton *m_addSamples = nullptr;
    QTimer *m_availTimer = nullptr;
    // Seite 3
    QTableWidget *m_table = nullptr;
    RecipeView *m_preview = nullptr;
    QLabel *m_notes = nullptr;
    // Seite 4
    QLabel *m_summary = nullptr;
    QComboBox *m_docClass = nullptr;
    QCheckBox *m_withRecipes = nullptr, *m_images = nullptr, *m_nutrition = nullptr, *m_scale = nullptr;
    QPushButton *m_texButton = nullptr;
};

// Rezept für einen Gang frei wählen (beste Vorschläge zuerst, auf Wunsch alle Rezepte)
class MenuCandidateDialog : public QDialog
{
    Q_OBJECT
public:
    MenuCandidateDialog(const Menu::Composer &comp, const QList<Menu::Item> &items, int index, QWidget *parent = nullptr);
    QString recipeId() const;

private:
    void fill();
    const Menu::Composer &m_comp;
    QList<Menu::Item> m_items;
    int m_index;
    QLineEdit *m_filter;
    QCheckBox *m_all;
    QTreeWidget *m_list;
};

class MenuListDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MenuListDialog(QWidget *parent = nullptr);

private:
    void fill();
    QTreeWidget *m_list;
};
