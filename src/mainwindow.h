// ViviaMeals – Hauptfenster (Rezeptfenster)
//
// Links die Liste der Rezepte der geöffneten Datenbank (mit Schnellfilter und Themenauswahl),
// rechts die HTML-Ansicht des gewählten Rezepts. Beim Start wird die zuletzt geöffnete Datenbank
// wieder geöffnet und ein zufälliges Rezept gezeigt; gibt es keine Datenbank, wird der Benutzer
// aufgefordert, eine anzulegen.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "importers.h"

#include <QMainWindow>

class QAction;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class QMenu;
class QSplitter;
class RecipeView;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    bool ensureDatabase();                     // beim Start: letzte Datenbank öffnen oder anlegen lassen
    bool openDatabase(const QString &path, bool quiet = false);
    void selectRecipe(const QString &id);
    // Dateien aus der Kommandozeile bzw. per Doppelklick (Dateizuordnung des Installers):
    // viviameals.vmdb/Ordner → Datenbank öffnen, .vmr → Rezept übernehmen, .rk/.mmf/.vtm → Import
    void openFiles(const QStringList &files);
    QStringList visibleIds() const;
    RecipeView *view() const { return m_view; }
    QListWidget *list() const { return m_list; }

public slots:
    void newDatabase();
    void openDatabaseDialog();
    void newRecipe();
    void findDuplicates();
    void scanRecipe(bool fromFile);   // Rezept scannen bzw. aus Bild/Foto erkennen
    void saveNewRecipe(const Recipe &r, const QString &title);
    void editRecipe();
    void deleteRecipe();
    void search();
    void importFormat(Importers::Format f);
    void exportWebsite();
    void exportLatex();
    void exportOdt();
    void clearDatabase();
    void mealTagsAll();
    void exportVmr();
    void printRecipe();
    void showRandom();
    void openFridge();
    void openAssistant();
    void updateReminder();
    void openWeighIn();
    void openDiary(const QString &recipeId = QString());
    void openMealPlan(const QString &profileId = QString());
    void openFoodTable();
    void recalcAll();
    void fixUmlautsAll();
    void translateAll();
    void openSettings();
    void about();
    void appearanceChanged();   // nach Theme- oder Schriftwechsel

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    void buildMenus();
    void buildToolBar();
    void refreshList();
    void refreshHeader();
    void refreshTags();
    void showCurrent();
    void updateActions();
    void updateRecentMenu();
    void rememberDatabase(const QString &folder);
    QString currentId() const;

    QLabel *m_headName;
    QLabel *m_headInfo;
    QFrame *m_reminder;
    QAction *m_diaryAction = nullptr;
    QLabel *m_reminderText;
    QLineEdit *m_filter;
    QComboBox *m_tagFilter;
    QListWidget *m_list;
    RecipeView *m_view;
    QSplitter *m_split;
    QLabel *m_statusDb;
    QMenu *m_recent;
    QList<QAction *> m_needRecipe;
    QList<QAction *> m_needDb;
    QString m_pendingSelect;
};
