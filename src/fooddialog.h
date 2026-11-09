// VitaMeals – Nährwerttabelle anzeigen/bearbeiten und Lebensmittel für eine Zutat auswählen
// SPDX-License-Identifier: MIT
#pragma once

#include "foodtable.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;

// Auswahl eines Lebensmittels für eine Zutat (Editor: Zuordnung ändern)
class FoodPickerDialog : public QDialog
{
    Q_OBJECT
public:
    FoodPickerDialog(const QString &ingredientName, const QString &currentKey, QWidget *parent = nullptr);
    QString selectedKey() const;
    bool remember() const;

private:
    void fill();
    QLineEdit *m_filter;
    QTableWidget *m_table;
    QCheckBox *m_remember;
    QString m_current;
};

// Ein Lebensmittel bearbeiten (eigene Werte)
class FoodEditDialog : public QDialog
{
    Q_OBJECT
public:
    FoodEditDialog(const Food &food, QWidget *parent = nullptr);
    Food food() const;

private:
    Food m_food;
    QLineEdit *m_name;
    QComboBox *m_category;
    QDoubleSpinBox *m_values[8];
    QDoubleSpinBox *m_density;
    QDoubleSpinBox *m_piece;
    QPlainTextEdit *m_aliases;
    QLineEdit *m_units;
};

// Übersicht der Nährwerttabelle (Extras → Nährwerttabelle)
class FoodTableDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FoodTableDialog(QWidget *parent = nullptr);
    bool changed() const { return m_changed; }

private:
    void fill();
    void editCurrent();
    QLineEdit *m_filter;
    QComboBox *m_cat;
    QTableWidget *m_table;
    bool m_changed = false;
};
