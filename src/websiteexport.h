// VitaMeals – Rezeptsammlung als Website exportieren (HTML5 + PHP)
//
// Erzeugt einen Ordner, der nur noch auf den eigenen Webserver hochgeladen werden muss:
//   index.html            Startseite mit Themen-Menü (Tags) und allen Rezepten
//   themen/<thema>.html   eine Seite je Thema (vegan, backen, desserts …)
//   rezepte/<name>.html   eine Seite je Rezept (Foto, Zutaten, Zubereitung, Nährwerte)
//   bilder/               Rezeptfotos (JPEG)
//   suche.php             Suche auf dem Server (Titel, Zutaten, Themen) – PHP 7.4 oder neuer
//   daten/rezepte.json    Suchindex (auch für die Sofortsuche per JavaScript ohne PHP)
//   assets/               style.css, suche.js, Logo
// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <QStringList>
#include <functional>

namespace WebsiteExport {

struct Options {
    QString title = QStringLiteral("Meine Rezepte");
    QString subtitle;
    QString author;
    bool nutrition = true;
    QStringList onlyIds;         // leer = alle Rezepte der Datenbank
};

// Fortschritt: (erledigt, gesamt) – Rückgabe false bricht ab
bool exportSite(const QString &folder, const Options &opt, QString *error = nullptr,
                const std::function<bool(int, int)> &progress = {});

QString slug(const QString &text);

} // namespace WebsiteExport
