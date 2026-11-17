// ViviaMeals – Rezeptsammlung als Website exportieren (HTML5 + PHP)
//
// Erzeugt einen Ordner, der nur noch auf den eigenen Webserver hochgeladen werden muss:
//   index.html            Startseite mit Themen-Menü (Tags) und allen Rezepten
//   themen/<thema>.html   eine Seite je Thema (vegan, backen, desserts …)
//   rezepte/<name>.html   eine Seite je Rezept (Foto, Zutaten, Zubereitung, Nährwerte)
//   bilder/               Rezeptfotos (JPEG)
//   suche.php             Suche auf dem Server (Titel, Zutaten, Themen) – PHP 7.4 oder neuer
//   daten/rezepte.json    Suchindex (auch für die Sofortsuche per JavaScript ohne PHP)
//   assets/               style.css, suche.js, Logo
//   impressum.html        Impressum (§ 5 DDG, § 18 Abs. 2 MStV) aus den Angaben im Export-Dialog
//   datenschutz.html      Datenschutzhinweise (DSGVO), passend zur erzeugten Website: statisch, ohne
//                         Cookies, Tracking oder eingebundene Fremdinhalte; Server-Logdateien des Hosters
// Die Texte sind Muster nach deutschem Recht und keine Rechtsberatung.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QString>
#include <QStringList>
#include <functional>

namespace WebsiteExport {

// Persönliche Angaben für Impressum und Datenschutzhinweise
struct Legal {
    bool enabled = true;     // Impressum und Datenschutz erzeugen und verlinken
    QString name;            // Name bzw. Betreiber (Pflicht)
    QString street;          // Straße und Hausnummer (Pflicht)
    QString zipCity;         // PLZ und Ort (Pflicht)
    QString country;         // optional, z. B. „Deutschland“
    QString email;           // Pflicht
    QString phone;           // optional
    QString responsible;     // verantwortlich nach § 18 Abs. 2 MStV; leer = Betreiber
    QString hoster;          // Webhoster mit Anschrift, optional
    QStringList missing() const;   // Namen fehlender Pflichtangaben
};

struct Options {
    QString title = QStringLiteral("Meine Rezepte");
    QString subtitle;
    QString author;
    bool nutrition = true;
    QStringList onlyIds;         // leer = alle Rezepte der Datenbank
    Legal legal;
};

// Inhalt (für <main>) der beiden Rechtsseiten – auch für Tests
QString impressumBody(const Options &opt);
QString datenschutzBody(const Options &opt);
QString obfuscatedMail(const QString &email);   // E-Mail als HTML-Zeichenreferenzen (erschwert Adresssammler)

// Fortschritt: (erledigt, gesamt) – Rückgabe false bricht ab
bool exportSite(const QString &folder, const Options &opt, QString *error = nullptr,
                const std::function<bool(int, int)> &progress = {});

QString slug(const QString &text);

} // namespace WebsiteExport
