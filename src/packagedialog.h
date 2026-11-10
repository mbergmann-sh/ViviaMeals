// ViviaMeals – Rezeptsammlung weitergeben (Export als .vmpack) und Rezeptpaket übernehmen
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipepackage.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QRadioButton;

class PackageExportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PackageExportDialog(const QStringList &visibleIds, QWidget *parent = nullptr);
    QString savedFile() const { return m_file; }

protected:
    void accept() override;

private:
    QStringList chosenIds() const;
    void update();
    QStringList m_visible;
    QLineEdit *m_name, *m_author;
    QPlainTextEdit *m_desc;
    QRadioButton *m_all, *m_shown, *m_favs, *m_tagged;
    QComboBox *m_tag;
    QCheckBox *m_images, *m_foods, *m_keepFav;
    QLabel *m_info;
    QString m_file;
};

// Vorschau eines Pakets vor dem Übernehmen
class PackageImportDialog : public QDialog
{
    Q_OBJECT
public:
    enum Target { Merge, NewDatabase };
    PackageImportDialog(const RecipePackage::Content &c, const QString &file, bool dbOpen, QWidget *parent = nullptr);
    Target target() const;
    bool importFoods() const;

private:
    QRadioButton *m_merge, *m_new;
    QCheckBox *m_foods;
};
