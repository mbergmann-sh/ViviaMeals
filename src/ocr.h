// ViviaMeals – Texterkennung (OCR) mit Tesseract
//
// Tesseract (https://github.com/tesseract-ocr/tesseract, Apache-2.0) wird als eigenes Programm
// aufgerufen, nicht eingelinkt: „tesseract bild.png stdout -l deu+eng --psm 3 tsv“. Die TSV-Ausgabe
// liefert jedes Wort mit Position, Block-/Absatz-/Zeilennummer und Erkennungssicherheit; daraus
// werden Textzeilen mit Lage und Schrifthöhe gebaut, die RecipeScan in Titel, Zutaten und
// Zubereitung aufteilt.
//
// Suchreihenfolge für das Programm: Einstellung → mitgeliefert (<Programmordner>/tesseract/) →
// PATH → übliche Installationsorte (Windows: C:\Program Files\Tesseract-OCR, macOS: Homebrew).
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QImage>
#include <QList>
#include <QRect>
#include <QString>
#include <QStringList>

namespace Ocr {

struct Word {
    QString text;
    QRect box;
    double conf = 0;     // 0–100
    int block = 0, par = 0, line = 0;
};

struct Line {
    QString text;
    QRect box;
    double conf = 0;     // Mittel der Wörter
    int block = 0, par = 0;
    int height() const { return box.height(); }
};

// Programmpfad finden; leer, wenn Tesseract nicht installiert ist
QString findTesseract(const QString &configured = QString());
// Kandidaten in der Suchreihenfolge (für Hinweise im Einstellungsdialog)
QStringList searchLocations();
// Version („5.3.4“) bzw. leer
QString version(const QString &exe);
// installierte Sprachen (deu, eng, …)
QStringList languages(const QString &exe);
// sinnvolle Sprachauswahl aus den installierten: deu+eng, sonst was da ist
QString defaultLanguages(const QStringList &installed);

// Aufrufargumente für eine Bilddatei (Ausgabe TSV auf stdout)
QStringList arguments(const QString &imageFile, const QString &langs);
// zusätzliche Umgebung (TESSDATA_PREFIX für die mitgelieferte Version)
QStringList environment(const QString &exe);

QList<Word> parseTsv(const QByteArray &tsv);
QList<Line> toLines(const QList<Word> &words, double minConf = 20);
QString plainText(const QList<Line> &lines);

// Synchroner Aufruf (Tests, Kommandozeile). GUI startet den Prozess selbst mit arguments().
bool recognize(const QImage &img, const QString &exe, const QString &langs, QList<Line> *lines, QString *error,
               int timeoutMs = 180000);

// Hinweistext mit Bezugsquelle, wenn Tesseract fehlt
QString installHint();

} // namespace Ocr
