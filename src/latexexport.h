// VitaMeals – Einzelnes Rezept als LaTeX-Datei (.tex) für die Klassen book und scrbook
//
// Wahlweise als vollständiges Dokument (direkt mit pdflatex/lualatex übersetzbar) oder als Kapitel
// zum Einbinden mit \input{} in ein eigenes Kochbuch. Das Foto wird als JPEG neben die .tex-Datei
// geschrieben. Benötigte Pakete: inputenc, fontenc, babel (ngerman), graphicx, booktabs, tabularx,
// xcolor, textcomp – in jeder TeX-Distribution enthalten.
// SPDX-License-Identifier: MIT AND LicenseRef-Commons-Clause-1.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

namespace LatexExport {

enum class DocClass { Book, ScrBook };

struct Options {
    DocClass docClass = DocClass::ScrBook;
    bool standalone = true;     // vollständiges Dokument (sonst nur \chapter …)
    bool image = true;
    bool nutrition = true;
};

QString toLatex(const Recipe &r, const Options &opt, const QString &imageFile = QString());
// schreibt <file> und ggf. <basis>-foto.jpg daneben
bool exportRecipe(const Recipe &r, const QString &file, const Options &opt, QString *error = nullptr);
QString escape(const QString &s);

} // namespace LatexExport
