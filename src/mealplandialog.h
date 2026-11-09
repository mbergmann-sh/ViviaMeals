// VitaMeals – Wochenmenü-Plan anzeigen, anpassen, drucken und Einkaufsliste erstellen
// SPDX-License-Identifier: MIT AND LicenseRef-Commons-Clause-1.0 (siehe LICENSE)
#pragma once

#include "mealplanner.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
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
    MealPlanner::Options options() const;
    QComboBox *m_profile;
    QSpinBox *m_kcal;
    QDateEdit *m_start;
    QSpinBox *m_days;
    QCheckBox *m_snack;
    QTableWidget *m_table;
    QLabel *m_info;
    MealPlan m_plan;
};
