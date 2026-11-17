// ViviaMeals – Rezept von einer Webseite übernehmen
//
// Fast alle Rezeptseiten (WordPress-Rezept-Plugins, Chefkoch, Kochportale, Blogs) beschreiben ihr Rezept
// für Suchmaschinen maschinenlesbar nach schema.org/Recipe – als JSON-LD im Seitenkopf oder als Microdata
// (itemprop-Attribute) im Text. Dieser Teil liest daraus Titel, Portionen, Zutaten, Schritte, Kategorien,
// Zeiten, Bildadresse, Autor und Quelle. Er arbeitet ohne Netzwerk (Seite herunterladen: WebImportDialog)
// und gibt die Teile an Importers::fromParts weiter – Spracherkennung, Übersetzung, Umrechnung in deutsche
// Maße, Nährwerte und Tags laufen wie bei jedem anderen Import.
// Übernommen wird nur das eine Rezept der angegebenen Seite, für die eigene Sammlung; die Quelle (Adresse,
// Autor, Website) wird beim Rezept vermerkt.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "importers.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace WebRecipe {

struct Parsed {
    bool ok = false;
    Importers::TextParts parts;
    QString imageUrl;      // absolute Adresse des Rezeptfotos (leer = keins)
    int prepMinutes = 0;   // Gesamtzeit bzw. Vorbereitung + Garen
    QString author;
    QString site;          // Name der Website (publisher) bzw. Rechnername
    QString via;           // "JSON-LD" oder "Microdata"
    QString error;         // bei !ok: verständlicher Grund
};

// HTML einer Seite auswerten; url = Adresse der Seite (für relative Bildadressen und die Quellenangabe)
Parsed parseHtml(const QString &html, const QUrl &url);

// Fertiges Rezept: Teile durch die Import-Nachbearbeitung, Quelle und Zeit setzen (ohne Bild)
Recipe toRecipe(const Parsed &p, const QUrl &url, const Importers::Options &opt = {});

// Hilfen (auch für Tests)
QString decodeEntities(const QString &s);   // &amp; &#8211; &nbsp; … und HTML-Tags entfernen
int isoMinutes(const QString &iso);         // „PT1H30M“ → 90, „P0DT45M“ → 45; 0 = unbekannt
QStringList addressesIn(const QString &text);   // http(s)-Adressen aus einem Text (eine je Zeile oder durcheinander)

} // namespace WebRecipe
