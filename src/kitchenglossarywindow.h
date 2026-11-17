// ViviaMeals – Küchen-Glossar (Hilfe → Küchen-Glossar …) und „Tipp des Tages“
//
// GlossaryWindow: nicht-modales Nachschlagefenster mit Suche (ohne Rücksicht auf Akzente und
// Groß-/Kleinschreibung), Kategorie-Auswahl, Begriffsliste und Erklärung; Verweise („Siehe auch“)
// sind anklickbar, Zurück/Vor folgen den angeklickten Verweisen.
// TipOfTheDayDialog: zeigt nach dem Programmstart einen zufälligen Begriff; Zurück/Weiter blättern
// durch eine zufällige Reihenfolge, „Im Glossar öffnen“ springt zum Begriff. Abschaltbar im Dialog
// und unter Extras → Einstellungen → Programmstart (ui/tipOfTheDay).
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QDialog>
#include <QList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTextBrowser;
class QToolButton;

namespace KitchenGlossary {
QString entryHtml(int index, bool withHeading);   // Erklärung als HTML mit Verweisen „glossar:Begriff“
}

class GlossaryWindow : public QWidget
{
    Q_OBJECT
public:
    explicit GlossaryWindow(QWidget *parent = nullptr);
    void showTerm(const QString &term);   // Begriff auswählen (Filter werden ggf. zurückgesetzt)

private:
    void refill();
    void showIndex(int index, bool record);
    void go(int step);

    QLineEdit *m_search = nullptr;
    QComboBox *m_category = nullptr;
    QLabel *m_count = nullptr;
    QListWidget *m_list = nullptr;
    QTextBrowser *m_text = nullptr;
    QToolButton *m_back = nullptr;
    QToolButton *m_forward = nullptr;
    QList<int> m_history;   // angezeigte Einträge für Zurück/Vor
    int m_pos = -1;
    int m_current = -1;
};

class TipOfTheDayDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TipOfTheDayDialog(QWidget *parent = nullptr);

signals:
    void openInGlossary(const QString &term);

private:
    void showAt(int pos);

    QLabel *m_heading = nullptr;
    QTextBrowser *m_text = nullptr;
    QLabel *m_counter = nullptr;
    QPushButton *m_prev = nullptr;
    QPushButton *m_next = nullptr;
    QCheckBox *m_onStart = nullptr;
    QList<int> m_order;   // zufällige Reihenfolge aller Einträge
    int m_pos = 0;
};
