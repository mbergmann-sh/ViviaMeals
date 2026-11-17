// ViviaMeals – Datenbank prüfen (Extras → Datenbank prüfen …)
//
// Eine ViviaMeals-Datenbank ist ein Ordner mit einer .vmr-Datei je Rezept; eine Reorganisation im
// klassischen Sinn braucht sie nicht. Mit der Zeit können sich aber Dinge ansammeln, die diese
// Prüfung findet und auf Wunsch behebt:
//   – unlesbare Rezeptdateien (beschädigt, abgeschnitten, kein ViviaMeals-Format)
//   – Fremdes im Rezeptordner (Reste abgebrochener Speichervorgänge, kopierte Dateien)
//   – leere Rezepte (ohne Titel, Zutaten und Zubereitung)
//   – Fotos, die sich nicht lesen lassen
//   – übergroße Fotos (größer als nötig: kosten Platz, Ladezeit und Abgleichszeit)
// Beheben: Fotos verkleinern bzw. entfernen; Dateien und leere Rezepte in den Unterordner
// „aussortiert/<Datum>“ der Datenbank verschieben – nichts wird endgültig gelöscht. Danach wird das
// Inhaltsverzeichnis neu aufgebaut.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>

#include <functional>

namespace DbCheck {

constexpr int kMaxSide = 1200;              // längste Fotoseite, ab der ein Foto als übergroß gilt
constexpr qint64 kMaxBytes = 350 * 1024;    // ab dieser Dateigröße ebenfalls
constexpr int kTargetSide = 1000;           // Zielgröße beim Verkleinern (wie beim Web-Import)

struct BigImage {
    QString id;
    QString title;
    QSize size;
    qint64 bytes = 0;
};

struct Report {
    int files = 0;                 // geprüfte .vmr-Dateien
    qint64 totalBytes = 0;         // Größe aller Rezeptdateien
    QStringList corrupt;           // Dateinamen unlesbarer .vmr-Dateien
    QStringList corruptReasons;    // Grund je Datei (gleiche Reihenfolge)
    QStringList strays;            // fremde Dateien im Rezeptordner
    QStringList empty;             // IDs leerer Rezepte
    QStringList brokenImages;      // IDs mit unlesbarem Foto
    QList<BigImage> bigImages;
    qint64 bigImageBytes = 0;      // Summe der übergroßen Fotos
    bool cancelled = false;
    bool clean() const;
};

using Progress = std::function<bool(int done, int total)>;   // false = abbrechen

// Nur lesen: Rezeptordner der geöffneten Datenbank prüfen
Report analyze(const Progress &progress = {});

// Foto (Base64) auf kTargetSide verkleinern; true, wenn das Ergebnis kleiner ist (dann in base64 ersetzt)
bool shrinkImage(QString *base64, qint64 *savedBytes = nullptr);

struct Actions {
    bool shrinkImages = true;
    bool removeBrokenImages = true;
    bool moveCorrupt = true;
    bool moveStrays = true;
    bool moveEmpty = false;
};

struct Result {
    int shrunk = 0;
    qint64 savedBytes = 0;
    int imagesRemoved = 0;
    int moved = 0;
    QString sortedFolder;          // Zielordner der verschobenen Dateien (leer = nichts verschoben)
    QStringList errors;
};

// Beheben (verändert die Datenbank); baut danach das Inhaltsverzeichnis neu auf
Result apply(const Report &report, const Actions &actions, const Progress &progress = {});

QString sizeText(qint64 bytes);    // „12,3 MB“

} // namespace DbCheck
