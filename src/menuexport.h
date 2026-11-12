// ViviaMeals – Menü exportieren: LaTeX (book/scrbook) und LibreOffice Writer (.odt)
//
// Seite 1 ist das Menüdeckblatt: Menütitel, Anlass, Datum, Personenzahl und je Gang nur Gangname,
// Rezepttitel und Kurzbeschreibung – eingefasst von einem Schmuckrahmen (LaTeX: TikZ, mit dem Paket
// pgfornament verzierte Ecken, falls vorhanden; Writer: doppelte Seitenumrandung mit Ornamenten).
// Auf den Folgeseiten stehen die Rezepte der Gänge (je Gang eine neue Seite), auf Wunsch auf die
// Personenzahl umgerechnet.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "latexexport.h"
#include "menu.h"

namespace MenuExport {

struct Options {
    LatexExport::DocClass docClass = LatexExport::DocClass::ScrBook;
    bool recipes = true;         // Rezepte auf den Folgeseiten
    bool images = true;          // Rezeptbilder
    bool nutrition = true;       // Nährwerte
    bool scaleToPersons = true;  // Mengen auf die Personenzahl umrechnen
    QString frameColor = QStringLiteral("#8A6D3B");   // Schmuckrahmen und Ornamente
};

QString dateText(const QDate &d);   // „Donnerstag, 24. Dezember 2026“
// recipes: zu menu.items gehörige Rezepte (gleiche Reihenfolge; fehlende = leeres Rezept)
QString toLatex(const Menu::MenuData &menu, const QList<Recipe> &recipes, const Options &opt, const QStringList &imageFiles = {});
bool writeLatex(const QString &file, const Menu::MenuData &menu, const QList<Recipe> &recipes, const Options &opt, QString *error = nullptr);
QByteArray toOdt(const Menu::MenuData &menu, const QList<Recipe> &recipes, const Options &opt);
bool writeOdt(const QString &file, const Menu::MenuData &menu, const QList<Recipe> &recipes, const Options &opt, QString *error = nullptr);
QString defaultFileName(const Menu::MenuData &menu, const QString &suffix);

} // namespace MenuExport
