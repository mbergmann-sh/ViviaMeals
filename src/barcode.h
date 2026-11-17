// ViviaMeals – Strichcodes (EAN-13, EAN-8, UPC-A) in Bildern erkennen
//
// Eigener, kleiner Leser ohne Zusatzbibliothek: Das Bild wird zeilenweise (waagerecht und senkrecht)
// abgetastet, jede Zeile mit örtlicher Schwelle in Striche und Lücken zerlegt, und die Breitenfolgen werden
// mit den EAN-Mustern verglichen. Gültig ist nur ein Code mit stimmender Prüfziffer; gezählt wird, wie viele
// Zeilen denselben Code ergeben (je mehr, desto sicherer). Für Kamerabilder und Fotos von Verpackungen.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QStringList>
#include <QString>

namespace Barcode {

struct Result {
    QString code;     // Ziffern (EAN-13 bzw. EAN-8; UPC-A als EAN-13 mit führender 0)
    int votes = 0;    // Zahl der Abtastzeilen mit diesem Ergebnis
    bool ok() const { return !code.isEmpty(); }
};

// rows: Zahl der Abtastzeilen je Richtung (mehr = gründlicher, langsamer)
Result decode(const QImage &image, int rows = 48);

// EAN-Prüfziffer stimmt (8 oder 13 Ziffern)
bool checksumOk(const QString &digits);

// Kameraauswahl (ohne Qt Multimedia, damit testbar):
// gleichnamige Kameras durchnummerieren – „USB-Kamera“, „USB-Kamera (2)“
QStringList uniqueLabels(const QStringList &names);
// Index der zu verwendenden Kamera: laufende, gespeicherte Kennung, gespeicherter Name, Systemstandard, erste;
// -1 bei leerer Liste
int pickCamera(const QList<QByteArray> &ids, const QStringList &names, const QByteArray &running,
               const QByteArray &savedId, const QString &savedName, const QByteArray &defaultId);

} // namespace Barcode
