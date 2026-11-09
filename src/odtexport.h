// VitaMeals – Rezept als LibreOffice-Writer-Dokument (.odt, OpenDocument Text 1.2)
//
// Aufbau: Titel, darunter rechts das Rezeptbild (vom Text umflossen), Portionen und Energie,
// Tags, Zutaten (Menge rechtsbündig an einem Tabulator), Zubereitung, Nährwerttabelle, Quelle.
// Fußzeile mit Trennlinie: Rezepttitel links, „Seite x von y“ rechts. A4, Liberation-Schriften.
// Die Datei wird ohne Fremdbibliothek als ZIP-Archiv geschrieben (deflate über qCompress).
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

#include <QByteArray>
#include <QString>

namespace OdtExport {

struct Options {
    double servings = 0;        // 0 = wie im Rezept
    bool nutrition = true;      // Nährwerttabelle anhängen
    double imageWidthCm = 6.5;  // Breite des Rezeptbilds
};

QByteArray toOdt(const Recipe &r, const Options &opt = {});
bool write(const Recipe &r, const QString &file, const Options &opt = {}, QString *error = nullptr);
QString defaultFileName(const Recipe &r);

// kleiner ZIP-Schreiber (auch für Tests)
class Zip
{
public:
    void add(const QString &name, const QByteArray &data, bool compress = true);
    QByteArray finish();

private:
    struct Entry { QByteArray name; quint32 crc; quint32 size; quint32 csize; quint16 method; quint32 offset; };
    QByteArray m_data;
    QList<Entry> m_entries;
};

} // namespace OdtExport
