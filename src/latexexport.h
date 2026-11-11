// ViviaMeals – Rezepte als LaTeX (.tex) für die Klassen book und scrbook
//
// Ausgabearten:
//  * Kapitel (Vorgabe): nur \chapter … zum Einbinden mit \input{} in ein Master-Dokument (Kochbuch);
//    auf Wunsch wird die \input-Zeile gleich in ein bestehendes Master-Dokument eingetragen
//    (vor der Marke „% ViviaMeals: weitere Rezepte hier“, sonst vor \backmatter bzw. \end{document}).
//  * Neues Master-Dokument: Kochbuch mit Titel, Autor (Datum unterdrückbar), Schmutztitel mit
//    Hintergrundbild, Inhaltsverzeichnis, Glossar und Stichwortverzeichnis, zweiseitigem Satz mit
//    Bindekorrektur (Ringbindung), Kopfzeile (Kapitel/Abschnitt) und Fußzeile (Seitenzahl) mit
//    Trennlinien – außer auf Titelseiten und im Inhaltsverzeichnis; Pakete für professionelle Tabellen
//    (booktabs, tabularx, longtable, siunitx …) und Grafiken (float, wrapfig, caption …); deutsche
//    Umlaute mit pdfLaTeX (inputenc/fontenc) und LuaLaTeX/XeLaTeX (fontspec).
//  * Vollständiges Einzeldokument (nur dieses Rezept).
// Das Foto wird als JPEG neben die Rezeptdatei geschrieben; die Quelle wird nur auf Wunsch ausgegeben.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

namespace LatexExport {

enum class DocClass { Book, ScrBook };
enum class Mode { Chapter, Standalone };

struct Options {
    DocClass docClass = DocClass::ScrBook;
    Mode mode = Mode::Chapter;
    bool standalone = false;    // veraltet, entspricht mode == Standalone
    bool image = true;
    bool nutrition = true;
    bool source = false;        // Quelle ausgeben (Vorgabe: nein)
    bool indexEntries = true;   // Stichwörter (\index) für Titel und Zutaten
    QString imagePrefix;        // Pfad des Fotos relativ zum Master-Dokument (leer = neben der Rezeptdatei)
};

struct MasterOptions {
    DocClass docClass = DocClass::ScrBook;
    QString title = QStringLiteral("Mein Kochbuch");
    QString subtitle;
    QString author;
    bool noDate = true;         // kein Datum auf dem Titelblatt
    QString halfTitleImage;     // Hintergrundbild des Schmutztitels (Datei; leer = ohne)
    bool toc = true;
    bool glossary = true;
    bool index = true;
    double bindingMm = 12;      // Bindekorrektur (Ringbindung)
};

QString toLatex(const Recipe &r, const Options &opt, const QString &imageFile = QString());
// schreibt <file> und ggf. <basis>-foto.jpg daneben
bool exportRecipe(const Recipe &r, const QString &file, const Options &opt, QString *error = nullptr);
QString escape(const QString &s);
// Stichwort mit deutscher Sortierung („Äpfel“ → „Apfel@Äpfel“), Sonderzeichen für makeindex maskiert
QString indexEntry(const QString &term, const QString &sub = QString());

// Master-Dokument: inputs = Pfade (relativ zum Master, ohne .tex) der einzubindenden Rezepte
QString masterDocument(const MasterOptions &m, const QStringList &inputs, const QString &halfTitleFile = QString());
QString glossaryFile();
// schreibt das Master-Dokument, glossar.tex und das Schmutztitelbild daneben (vorhandenes glossar.tex bleibt)
bool writeMaster(const QString &file, const MasterOptions &m, const QStringList &inputs, QString *error = nullptr);
// \input-Pfad einer Rezeptdatei relativ zum Master-Dokument (Schrägstriche, ohne .tex)
QString relativeInput(const QString &masterFile, const QString &recipeFile);
// trägt \input{path} in ein bestehendes Master-Dokument ein; already = war schon eingetragen
bool addToMaster(const QString &masterFile, const QString &inputPath, QString *error = nullptr, bool *already = nullptr);
QString masterMarker();   // „% ViviaMeals: weitere Rezepte hier“

} // namespace LatexExport
