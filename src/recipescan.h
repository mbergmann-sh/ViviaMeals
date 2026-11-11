// ViviaMeals – gescannte Rezepte aufbereiten
//
// 1. Fotos auf der Seite finden (Kacheln mit kräftigen Farben oder vielen Mitteltönen, zu Bereichen
//    zusammengefasst). Sie werden vor der Texterkennung weiß überdeckt – Bildinhalte erzeugen sonst
//    Buchstabensalat – und können im Scandialog als Rezeptbild übernommen werden.
// 2. Die Textzeilen aus der Texterkennung (Ocr::Line, mit Block, Absatz und Schrifthöhe) in Titel,
//    Portionen, Zutaten und Zubereitung aufteilen: Überschriften („Zutaten“, „Zubereitung“, „So wird’s
//    gemacht“, „Ingredients“ …), Zeilen mit Mengenangaben, Satzlänge und Schriftgröße.
// 3. Typische Erkennungsfehler korrigieren (l/2 → ½, 1OO g → 100 g, El → EL, 180 *C → 180° C,
//    Silbentrennung, Aufzählungszeichen).
// Das Ergebnis geht über Importers::fromParts durch dieselbe Nachbearbeitung wie ein Dateiimport.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "importers.h"
#include "ocr.h"

#include <QImage>
#include <QList>
#include <QRect>

namespace RecipeScan {

// Bild für die Texterkennung: gedreht laut EXIF (beim Laden), RGB, kleine Fotos vergrößert,
// riesige verkleinert
QImage normalize(const QImage &img);

// Fotobereiche (in Bildkoordinaten), größte zuerst
QList<QRect> findPhotos(const QImage &img);
// Bereiche weiß überdecken
QImage blank(const QImage &img, const QList<QRect> &areas);

// eine Zeile korrigieren
QString fixLine(const QString &line);
// verlorene Umlaut-Punkte ergänzen (Wörter aus der Nährwerttabelle und häufige Rezeptwörter)
QString restoreUmlauts(const QString &line);

struct Parts {
    Importers::TextParts text;
    int prepMinutes = 0;
    QStringList unassigned;   // Zeilen, die weder Zutat noch Anleitung sind (Einleitung, Nährwerte …)
};
Parts split(const QList<Ocr::Line> &lines);

// fertiges Rezept (Sprache, Umlaute, Übersetzung, Nährwerte, Tags wie beim Import)
Recipe toRecipe(const Parts &parts, const Importers::Options &opt = {});

} // namespace RecipeScan
