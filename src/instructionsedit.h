// ViviaMeals – Eingabefeld für die Zubereitung mit Formatleiste
// Fett, kursiv, unterstrichen, Aufzählung und nummerierte Liste wirken auf den markierten Text bzw. die
// markierten Absätze. Gespeichert wird als einfacher Auszeichnungstext (siehe markup.h).
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QTextEdit>
#include <QWidget>

class QAction;
class QToolBar;
class QTextDocument;

class InstructionsEdit : public QWidget
{
public:
    explicit InstructionsEdit(QWidget *parent = nullptr);

    void setMarkup(const QString &markup);
    QString markup() const;
    void setPlaceholderText(const QString &text);
    QTextEdit *editor() const { return m_edit; }

    // QTextDocument → Auszeichnungstext (nur fett/kursiv/unterstrichen, Absätze und Listen)
    static QString markupFromDocument(const QTextDocument *doc);

private:
    void updateButtons();
    void setCharFormat(int which, bool on);
    void toggleList(bool numbered);

    QTextEdit *m_edit;
    QToolBar *m_bar;
    QAction *m_bold, *m_italic, *m_underline, *m_bullets, *m_numbers;
};
