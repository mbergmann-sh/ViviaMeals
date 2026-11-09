// VitaMeals – Hauptfenster (Rezeptfenster)
//
// Links die Liste der Rezepte der geöffneten Datenbank (mit Schnellfilter und Themenauswahl),
// rechts die HTML-Ansicht des gewählten Rezepts. Beim Start wird die zuletzt geöffnete Datenbank
// wieder geöffnet und ein zufälliges Rezept gezeigt; gibt es keine Datenbank, wird der Benutzer
// aufgefordert, eine anzulegen.
// SPDX-License-Identifier: MIT
#pragma once

#include "importers.h"

#include <QMainWindow>

class QAction;
class QComboBox;
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
    // vitameals.vmdb/Ordner → Datenbank öffnen, .vmr → Rezept übernehmen, .rk/.mmf/.vtm → Import
    void openFiles(const QStringList &files);
    QStringList visibleIds() const;
    RecipeView *view() const { return m_view; }
    QListWidget *list() const { return m_list; }

public slots:
    void newDatabase();
    void openDatabaseDialog();
    void newRecipe();
    void editRecipe();
    void deleteRecipe();
    void search();
    void importFormat(Importers::Format f);
    void exportWebsite();
    void exportLatex();
    void exportVmr();
    void printRecipe();
    void showRandom();
    void openFridge();
    void openAssistant();
    void openMealPlan(const QString &profileId = QString());
    void openFoodTable();
    void recalcAll();
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
