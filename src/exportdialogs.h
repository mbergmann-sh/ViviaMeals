// ViviaMeals – Export-Dialoge: Website (ganze Sammlung) und LaTeX (einzelnes Rezept)
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QRadioButton;
class QSpinBox;

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
    void updateMode();
    QString pickFile(QLineEdit *edit, const QString &title, const QString &filter, bool save);
    Recipe m_recipe;
    QComboBox *m_class;
    QRadioButton *m_modeChapter, *m_modeNewMaster, *m_modeStandalone;
    QCheckBox *m_register;
    QLineEdit *m_masterFile;
    QWidget *m_registerRow;
    QWidget *m_newMasterBox;
    QLineEdit *m_newMasterFile, *m_bookTitle, *m_subtitle, *m_author, *m_bgImage;
    QCheckBox *m_noDate, *m_toc, *m_glossary, *m_index;
    QSpinBox *m_bcor;
    QCheckBox *m_image, *m_nutrition, *m_source, *m_indexEntries;
    QLineEdit *m_file;
    QString m_base;            // Dateiname des Rezepts ohne Endung
    bool m_fileEdited = false; // Rezeptdatei von Hand geändert
    void proposeFile();
};
