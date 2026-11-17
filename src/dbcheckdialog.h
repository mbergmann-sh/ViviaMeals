// ViviaMeals – Extras → Datenbank prüfen …
// Prüft die geöffnete Datenbank (nur lesend), zeigt die Befunde gruppiert mit Häkchen und behebt
// die gewählten (DbCheck::apply). Siehe dbcheck.h.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "dbcheck.h"

#include <QDialog>

class QLabel;
class QProgressBar;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class DbCheckDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DbCheckDialog(QWidget *parent = nullptr);

signals:
    void openRecipe(const QString &id);   // Doppelklick auf ein Rezept im Befund

private:
    void runCheck();
    void fix();
    void setBusy(bool busy);
    void updateFixButton();
    QTreeWidgetItem *group(const QString &text, const QString &tip, bool checked, int count);

    QLabel *m_info = nullptr;
    QTreeWidget *m_tree = nullptr;
    QProgressBar *m_bar = nullptr;
    QLabel *m_note = nullptr;
    QPushButton *m_check = nullptr;
    QPushButton *m_fix = nullptr;
    QPushButton *m_close = nullptr;
    DbCheck::Report m_report;
    QTreeWidgetItem *m_gBig = nullptr, *m_gBroken = nullptr, *m_gCorrupt = nullptr, *m_gStrays = nullptr, *m_gEmpty = nullptr;
    bool m_busy = false;
    bool m_cancel = false;
};
