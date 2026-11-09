// VitaMeals – Kühlschrank: Bestandsliste pflegen, passende Rezepte vorschlagen, Einkaufsliste
// SPDX-License-Identifier: MIT
#pragma once

#include <QDialog>

class QComboBox;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QTableWidget;
class QTextBrowser;

class FridgeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FridgeDialog(QWidget *parent = nullptr);

signals:
    void showRecipe(const QString &id);

private:
    void addItem();
    void saveStock();
    void suggest();
    void showShopping();
    QLineEdit *m_input;
    QListWidget *m_stock;
    QLineEdit *m_basics;
    QComboBox *m_tag;
    QSpinBox *m_maxMissing;
    QTableWidget *m_table;
    QTextBrowser *m_shopping;
    QString m_shoppingText;
};
