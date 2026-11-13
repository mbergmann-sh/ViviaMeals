// ViviaMeals – Sonderzeichen für die Oberfläche
// Fehlt ein Zeichen in der Oberflächenschrift (unter Windows Segoe UI), sucht Qt eine Ersatzschrift und liest
// dafür beim ersten Mal *alle* installierten Schriften ein (bei ~440 Schriften 0,7 s, kalt mehrere Sekunden).
// Sym::get() liefert deshalb das gewünschte Zeichen nur, wenn die Oberflächenschrift es selbst enthält,
// sonst ein einfacheres Ersatzzeichen.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QFont>
#include <QString>

class QAbstractButton;

namespace Sym {

enum Id { Check, Cross, Warning, Info, Star, StarEmpty, Busy, Left, Right, Box, RotateLeft, RotateRight, Sync };

// reiner Text: Wunschzeichen, wenn die Oberflächenschrift es hat, sonst Ersatzzeichen
QString get(Id id);
// HTML: Wunschzeichen; fehlt es in der Oberflächenschrift, in einer Symbolschrift von Windows
// (<span style="font-family:'Segoe UI Symbol'">), die ohne Ersatzschriftsuche geladen wird; sonst Ersatzzeichen
QString html(Id id);
// Schrift für ein Widget/Tabellenfeld, das nur (oder überwiegend) das Zeichen zeigt; false = base reicht
bool font(Id id, const QFont &base, QFont *out);
// das Wunschzeichen selbst (zusammen mit font() verwenden)
QString wanted(Id id);
// Knopf mit Zeichen (davor/danach Text) – nimmt bei Bedarf die Symbolschrift für den Knopf
void setButton(QAbstractButton *b, Id id, const QString &before = QString(), const QString &after = QString());
// alle Wunschzeichen in einem Text (z. B. Hinweise oder HTML aus dem Kern) durch die passenden ersetzen
QString fix(const QString &text);
// dasselbe für HTML (Wunschzeichen wie bei html())
QString fixHtml(const QString &html);
// reinen Text nach HTML wandeln und dabei die Zeichen wie bei html() setzen
QString plainToHtml(const QString &text);
// für --profile: welche Wunschzeichen fehlen in der Oberflächenschrift
QString report();

} // namespace Sym
