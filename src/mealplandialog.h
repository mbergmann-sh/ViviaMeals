// VitaMeals – Wochenmenü-Plan anzeigen, anpassen, drucken und Einkaufsliste erstellen
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "mealplanner.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

class MealPlanDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MealPlanDialog(const QString &profileId = QString(), QWidget *parent = nullptr);

signals:
    void showRecipe(const QString &id);

private:
    void profileChanged();
    void create();
    void fillTable();
    void cellMenu(const QPoint &pos);
    void chooseRecipe(int day, int meal);
    void shoppingList();
    void print();
    void save();
    void updateNotes();
    MealPlanner::Options options() const;
    QComboBox *m_profile;
    QSpinBox *m_kcal;
    QDateEdit *m_start;
    QSpinBox *m_days;
    QCheckBox *m_snack;
    QTableWidget *m_table;
    QLabel *m_info;
    QLabel *m_notes;
    QPushButton *m_healthy;
    MealPlan m_plan;
};
