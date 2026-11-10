// ViviaMeals – Ernährungstagebuch (Planen → Ernährungstagebuch)
//
// DiaryDialog: Person und Tag wählen, Mahlzeiten des Tages mit Uhrzeit, Menge und Nährwerten,
// Ziel/gegessen/Restkalorien, Wochenübersicht. Einträge aus einem Rezept der Datenbank, als eigenes
// Rezept (Editor) oder aus dem gespeicherten Wochenplan.
// DiaryEntryDialog: einen Eintrag anlegen oder ändern (Rezeptauswahl, Datum/Uhrzeit, Mahlzeit,
// gegessene Menge in Portionen oder Gramm, Notiz; eigenes Rezept bearbeiten und auf Wunsch in die
// Rezeptdatenbank übernehmen).
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "diary.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QDateTimeEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTableWidget;

class DiaryEntryDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Mode { Recipe, Own, Edit };
    // usedKcal: bereits gegessene kcal des Tages ohne diesen Eintrag; targetKcal: Tagesziel
    DiaryEntryDialog(Mode mode, const Diary::Entry &e, double usedKcal, double targetKcal, QWidget *parent = nullptr);
    Diary::Entry entry() const { return m_entry; }
    bool editOwnRecipe();   // Editor für ein eigenes Rezept öffnen (false = abgebrochen)

protected:
    void accept() override;

private:
    void fillRecipes();
    void recipeChosen();
    void amountChanged();
    void unitChanged();
    void updateInfo();
    void loadEntry();
    Mode m_mode;
    Diary::Entry m_entry;
    double m_used, m_target;
    QLineEdit *m_filter = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_title;
    QDateTimeEdit *m_when;
    QComboBox *m_meal;
    QDoubleSpinBox *m_amount;
    QComboBox *m_unit;
    QLineEdit *m_note;
    QLabel *m_info;
    QPushButton *m_editOwn = nullptr;
    QCheckBox *m_saveToDb = nullptr;
    QPushButton *m_ok;
    bool m_updating = false;
};

class DiaryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DiaryDialog(QWidget *parent = nullptr);
    // direkt ein Rezept eintragen (aus dem Kontextmenü der Rezeptliste)
    void addRecipe(const QString &recipeId);
    QString profileId() const;

private:
    void profileChanged();
    void refresh();
    void addFromRecipe();
    void addOwn();
    void addFromPlan();
    void editEntry();
    void removeEntry();
    void copyEntryToToday();
    void store();
    QString selectedId() const;
    double target() const;
    QComboBox *m_profile;
    QDateEdit *m_date;
    QLabel *m_summary;
    QProgressBar *m_bar;
    QLabel *m_macros;
    QTableWidget *m_table;
    QLabel *m_week;
    QPushButton *m_fromRecipe, *m_own, *m_fromPlan, *m_edit, *m_remove, *m_copy;
    Diary::Book m_book;
};
