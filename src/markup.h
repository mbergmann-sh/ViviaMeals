// ViviaMeals – einfache Textauszeichnung für die Zubereitung
//
// Die Zubereitung bleibt reiner Text (Übersetzung, Umlautkorrektur, Suche, Abgleich und ältere Rezepte
// funktionieren unverändert) mit wenigen, gut lesbaren Auszeichnungen:
//   **fett**   *kursiv*   __unterstrichen__        (\* bzw. \_ für ein wörtliches Zeichen)
//   - Punkt                                        (Aufzählung, auch „* “ und „• “)
//   1. Schritt                                     (nummerierte Liste, auch „1) “)
// Absätze werden durch eine Leerzeile getrennt; ein einfacher Zeilenumbruch bleibt ein Zeilenumbruch.
// Ein Auszeichnungszeichen wirkt nur paarweise und nur direkt an Text („5 * 3“ bleibt, wie es ist).
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QList>
#include <QString>

#include <functional>

namespace Markup {

struct Run {
    QString text;   // kann '\n' (Zeilenumbruch) enthalten
    bool bold = false;
    bool italic = false;
    bool underline = false;
};

struct Block {
    enum Kind { Paragraph, Bullet, Number };
    Kind kind = Paragraph;
    int number = 0;   // nur Number: Nummer dieses Punkts
    QList<Run> runs;
};

QList<Block> parse(const QString &text);
QString serialize(const QList<Block> &blocks);   // Gegenstück zu parse()

// Text eines Absatzes/Punkts mit Auszeichnung → Läufe (für eigene Ausgaben)
QList<Run> parseInline(const QString &text);
QString serializeInline(const QList<Run> &runs);

// Ausgaben
QString toHtml(const QString &text);    // <p>, <ul>/<ol>, <b>, <i>, <u>
QString toPlain(const QString &text);   // ohne Auszeichnung (Listenpunkte als „• “/„1. “)

// LaTeX: esc wandelt reinen Text um (Sonderzeichen); Listen als itemize/enumerate
QString toLatex(const QString &text, const std::function<QString(const QString &)> &esc);

// ODF (Writer): esc wie bei XML; Zeichenformate T_b/T_i/T_u (verschachtelt), Absatzformat paraStyle,
// Listen mit den Listenformaten L_Punkt und L_Nummer
QString toOdt(const QString &text, const std::function<QString(const QString &)> &esc, const QString &paraStyle);

// true, wenn der Text Auszeichnung oder Listen enthält (z. B. für Hinweise)
bool hasMarkup(const QString &text);

} // namespace Markup
