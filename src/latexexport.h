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

#include <QImage>
#include <QRectF>

namespace LatexExport {

enum class DocClass { Book, ScrBook };
enum class Mode { Chapter, Standalone };
enum class Heading { Chapter, Section };   // Rezept als \\chapter oder als \\section in einem Themen-Kapitel

struct Options {
    DocClass docClass = DocClass::ScrBook;
    Mode mode = Mode::Chapter;
    bool standalone = false;    // veraltet, entspricht mode == Standalone
    bool image = true;
    bool nutrition = true;
    bool source = false;        // Quelle ausgeben (Vorgabe: nein)
    bool indexEntries = true;   // Stichwörter (\index) für Titel und Zutaten
    QString imagePrefix;        // Pfad des Fotos relativ zum Master-Dokument (leer = neben der Rezeptdatei)
    Heading heading = Heading::Chapter;
    bool newPage = true;        // bei \\section: jedes Rezept auf einer neuen Seite
};

struct MasterOptions {
    DocClass docClass = DocClass::ScrBook;
    QString title = QStringLiteral("Mein Kochbuch");
    QString subtitle;
    QString author;
    bool noDate = true;         // kein Datum auf dem Titelblatt
    QString halfTitleImage;     // Hintergrundbild (Datei; leer = ohne)
    enum class BgTarget { HalfTitle, TitlePage };
    enum class BgPlacement { FillPage, Centered, Free };
    BgTarget bgTarget = BgTarget::HalfTitle;      // Schmutztitel oder Titelseite
    BgPlacement bgPlacement = BgPlacement::FillPage;
    double bgWidthMm = 140;     // Breite des Bildes (Centered, Free)
    double bgLeftMm = 45;       // Abstand von der linken Papierkante (Free)
    double bgBottomMm = 60;     // Abstand von der unteren Papierkante (Centered, Free)
    bool bgShiftBinding = true; // um die Bindekorrektur nach rechts verschieben (\BindungsHSpace)
    bool bgTrim = true;         // gleichmäßigen weißen Rand des Bildes abschneiden
    QRectF bgCrop;              // Zuschnitt, relativ (0…1) zum gedrehten Bild; leer = ganzes Bild
    int bgRotation = 0;         // Drehung in Grad (Vielfache von 90)
    // dasselbe Bild zusätzlich auf der Titelseite, zwischen Überschrift (Titel/Untertitel) und Autor
    bool titleImage = true;
    double titleImageWidthMm = 100;
    double titleImageGapAboveMm = 15;   // Abstand zur Überschrift
    double titleImageGapBelowMm = 15;   // Abstand zum Autor
    bool titleImageActive() const { return titleImage && !halfTitleImage.isEmpty() && bgTarget == BgTarget::HalfTitle; }
    QString titleColor = QStringLiteral("#000000");      // Schriftfarbe Buchtitel
    QString subtitleColor = QStringLiteral("#000000");   // Schriftfarbe Untertitel
    bool titleShadow = true;                             // Titel und Untertitel werfen Schatten
    QString shadowColor = QStringLiteral("#8C8C8C");
    QString firstChapter;       // bei Rezepten als \\section: Themen-Kapitel für das erste Rezept
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
QString masterDocument(const MasterOptions &m, const QStringList &inputs, const QString &halfTitleFile = QString(),
                       const QString &titleImageFile = QString());
QString glossaryFile();
QString prefaceFile(DocClass cls);   // vorwort.tex (\\addchap{Vorwort} …)
// Hintergrundbild aufbereiten: weißen Rand abschneiden (trim), bei FillPage auf A4-Format zuschneiden
QImage prepareBackground(const QImage &src, const MasterOptions &m);
QImage trimWhite(const QImage &src, int threshold = 245);
// schreibt das Master-Dokument, glossar.tex und das Schmutztitelbild daneben (vorhandenes glossar.tex bleibt)
bool writeMaster(const QString &file, const MasterOptions &m, const QStringList &inputs, QString *error = nullptr);
// \input-Pfad einer Rezeptdatei relativ zum Master-Dokument (Schrägstriche, ohne .tex)
QString relativeInput(const QString &masterFile, const QString &recipeFile);
// trägt \input{path} in ein bestehendes Master-Dokument ein; already = war schon eingetragen
// chapter: Themen-Kapitel (\\chapter{…}) – wird angelegt, wenn es noch fehlt; leer = vor die Marke
bool addToMaster(const QString &masterFile, const QString &inputPath, QString *error = nullptr, bool *already = nullptr,
                 const QString &chapter = QString(), bool *chapterCreated = nullptr);
// Kapitelüberschriften (\\chapter, \\addchap) im Hauptteil eines Master-Dokuments, lesbar (ohne LaTeX-Maskierung)
QStringList masterChapters(const QString &masterText);
QStringList defaultChapters();                 // Vorspeisen, Suppen, Salate, Hauptgerichte …
QString suggestChapter(const Recipe &r);       // passendes Themen-Kapitel aus Tags und Titel
// Hintergrundbild: maximale Breite in mm, damit es nicht über die A4-Seite hinausragt
double maxBackgroundWidth(const MasterOptions &m, double heightPerWidth);
QString masterMarker();

// Vorhandene LaTeX-Installation (TeX Live, MiKTeX, MacTeX) suchen: pdflatex/lualatex/xelatex im Suchpfad
// und an den üblichen Installationsorten. Ergebnis wird zwischengespeichert; refresh = neu suchen.
struct Installation {
    QString program;   // gefundenes Programm (z. B. …/pdflatex.exe), leer = keine Installation
    QString name;      // „TeX Live 2025“, „MiKTeX“, „MacTeX“ …
    bool found() const { return !program.isEmpty(); }
};
Installation findInstallation(bool refresh = false);
QStringList searchDirs();   // zusätzliche Suchorte außerhalb des Suchpfads   // „% ViviaMeals: weitere Rezepte hier“

} // namespace LatexExport
