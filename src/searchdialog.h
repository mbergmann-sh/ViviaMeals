// VitaMeals – Suchdialog: Rezeptnamen (auch Teilnamen), Zutaten und Tags
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QTableWidget;
struct RecipeSummary;

class SearchDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SearchDialog(QWidget *parent = nullptr);
    QString selectedId() const;
    static bool matches(const RecipeSummary &s, const QString &text, bool inIngredients);

private:
    void search();
    QLineEdit *m_text;
    QCheckBox *m_inIngredients;
    QListWidget *m_tags;
    QCheckBox *m_all;
    QSpinBox *m_maxKcal;
    QCheckBox *m_onlyFav;
    QCheckBox *m_onlyImage;
    QTableWidget *m_results;
    QLabel *m_count;
};
