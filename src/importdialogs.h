// VitaMeals – Import-Ablauf: Dateien wählen (einzeln oder mehrere), Vorschau, Doubletten-Abfrage
// SPDX-License-Identifier: MIT
#pragma once

#include "importers.h"

#include <QDialog>

class QCheckBox;
class QTableWidget;

namespace ImportFlow {
// Gibt die ID des ersten importierten Rezepts zurück (leer = nichts importiert)
QString run(QWidget *parent, Importers::Format format, const QStringList &files = {});
}

// Vorschau der gefundenen Rezepte mit Auswahl
class ImportPreviewDialog : public QDialog
{
    Q_OBJECT
public:
    ImportPreviewDialog(const QList<Recipe> &recipes, const QStringList &origins, const QStringList &warnings, QWidget *parent = nullptr);
    QList<int> selected() const;

private:
    QTableWidget *m_table;
};

// Abfrage bei einer Doublette
class DuplicateDialog : public QDialog
{
    Q_OBJECT
public:
    enum Choice { Skip, Replace, KeepBoth };
    DuplicateDialog(const Recipe &incoming, const Recipe &existing, QWidget *parent = nullptr);
    Choice choice() const { return m_choice; }
    bool forAll() const;

private:
    Choice m_choice = Skip;
    QCheckBox *m_all;
};
