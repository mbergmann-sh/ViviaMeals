// ViviaMeals – Doubletten aufspüren, vergleichen, bearbeiten, zusammenführen und löschen
//
// Links die gefundenen Gruppen (Titel, Grund, Anzahl), darunter die Rezepte mit Portionen, kcal,
// Zutatenzahl, Bild, Favorit und Änderungsdatum – das vollständigste zuerst. Rechts zwei Rezepte
// nebeneinander zum Vergleich. Aktionen: Bearbeiten, Löschen, „Behalten und übrige zusammenführen“
// (Bild, Quelle, Tags, Favorit gehen auf das behaltene Rezept über), „Keine Doubletten“.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "duplicates.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QPushButton;
class QTextBrowser;
class QTreeWidget;
class QTreeWidgetItem;

class DuplicatesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DuplicatesDialog(QWidget *parent = nullptr);
    int changes() const { return m_changes; }

signals:
    void showRecipe(const QString &id);

private:
    void search();
    void fill(const QString &selectId = QString());
    void selectionChanged();
    QString currentId() const;
    QStringList selectedIds() const;
    int currentGroup() const;
    void editCurrent();
    void deleteSelected();
    void keepAndMerge();
    void notDuplicates();
    void showPreview(QTextBrowser *b, const QString &id);

    QComboBox *m_strict;
    QTreeWidget *m_tree;
    QLabel *m_info;
    QTextBrowser *m_left, *m_right;
    QComboBox *m_compare;
    QPushButton *m_edit, *m_delete, *m_keep, *m_ignore;
    QList<Duplicates::Group> m_groups;
    int m_changes = 0;
};
