// VitaMeals – Export-Dialoge: Website (ganze Sammlung) und LaTeX (einzelnes Rezept)
// SPDX-License-Identifier: MIT AND LicenseRef-Commons-Clause-1.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QRadioButton;

class WebsiteExportDialog : public QDialog
{
    Q_OBJECT
public:
    WebsiteExportDialog(const QStringList &filteredIds, QWidget *parent = nullptr);

protected:
    void accept() override;

private:
    QStringList m_filtered;
    QLineEdit *m_title, *m_subtitle, *m_author, *m_folder;
    QCheckBox *m_nutrition, *m_open;
    QRadioButton *m_all, *m_onlyFiltered;
};

class LatexExportDialog : public QDialog
{
    Q_OBJECT
public:
    LatexExportDialog(const Recipe &r, QWidget *parent = nullptr);

protected:
    void accept() override;

private:
    Recipe m_recipe;
    QComboBox *m_class;
    QCheckBox *m_standalone, *m_image, *m_nutrition;
    QLineEdit *m_file;
};
