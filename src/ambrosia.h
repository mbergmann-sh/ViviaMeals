// ViviaMeals – Import aus der Rezeptverwaltung „Ambrosia“ (Microsoft-Access-Datenbank .mdb)
//
// Gelesen werden (Tabellen von Ambrosia 2.x):
//   T_REZ_KOPF          Rezept: Titel, Ertrag, Quelle, Notizen (RTF), Vorbereitungs-/Kochzeit, Weintipp
//   T_REZ_KOPFEINHEIT   Einheit des Ertrags (Portionen, Stück, Personen …)
//   T_REZ_ZUTAT         Zutaten: Menge, Einheit (Nummer), Text, Verarbeitung, Trenner (= Zwischenüberschrift)
//   T_REZ_ANWEIS        Zubereitung (RTF mit Aufzählungen, fett/kursiv/unterstrichen)
//   T_REZ_CAT_NDX/T_CAT Kategorien → Tags
//   T_LITERATUR         Quellen (Zeitschrift/Buch, Ausgabe, Jahrgang) und Seite
//   T_BLOB              Rezeptfotos (Typ 5)
//   T_UNIT              Einheiten – in exportierten Datenbanken meist leer, dann gilt die eingebaute Liste
// Gelöschte Rezepte und Vorlagen werden übersprungen.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "importers.h"
#include "markup.h"

#include <QByteArray>
#include <QString>

namespace Ambrosia {

// Datei ist eine Access-Datenbank mit Ambrosia-Tabellen
bool isAmbrosia(const QString &path);
Importers::Result importFile(const QString &path, const Importers::Options &opt = {});

// Hilfen (auch für Tests)
QList<Markup::Block> rtfBlocks(const QString &rtf);     // RTF → Absätze/Listenpunkte mit Zeichenformaten
QString rtfToMarkup(const QString &rtf);                // RTF → ViviaMeals-Auszeichnung
QString rtfToPlain(const QString &rtf);
QByteArray imageFromBlob(const QByteArray &blob);       // Bilddaten aus einem T_BLOB-Feld (JPEG/PNG/GIF/BMP)
QString unitName(int id, double amount, QString *note); // eingebaute Einheitenliste; leer = keine/unbekannt
bool knownUnit(int id);

} // namespace Ambrosia
