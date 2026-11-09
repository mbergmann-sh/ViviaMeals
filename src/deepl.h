// VitaMeals – optionale Übersetzung fremdsprachiger Rezepte mit der DeepL-API
//
// Jeder Benutzer trägt in den Einstellungen seinen eigenen DeepL-API-Schlüssel ein (kostenloser
// Tarif „DeepL API Free“: 500.000 Zeichen im Monat). Der Schlüssel ist nicht im Programm enthalten.
// Schlüssel mit der Endung „:fx“ gehören zum Free-Tarif (api-free.deepl.com), alle anderen zum
// Pro-Tarif (api.deepl.com). Übersetzt werden nur nicht-deutsche Rezepte; Ziel ist immer Deutsch.
// Ohne Schlüssel oder bei Fehlern übersetzt das eingebaute Glossar die Zutaten.
//
// Die Aufrufe blockieren (eigene Ereignisschleife mit Zeitlimit) und sind für den Import gedacht.
// Zum Testen kann die Umgebungsvariable VITAMEALS_DEEPL_URL einen anderen Server angeben.
// SPDX-License-Identifier: MIT
#pragma once

#include "recipe.h"

#include <QString>
#include <QStringList>

namespace DeepL {

struct Usage {
    qint64 characterCount = 0;
    qint64 characterLimit = 0;
};

bool isFreeKey(const QString &key);
QString baseUrl(const QString &key);     // „https://api-free.deepl.com“
// Verbindung und Kontingent prüfen
bool usage(const QString &key, Usage *out, QString *error = nullptr);
// Texte ins Deutsche übersetzen (an Ort und Stelle). sourceLang leer = automatisch erkennen.
bool translate(const QString &key, QStringList &texts, const QString &sourceLang = {}, QString *detected = nullptr,
               QString *error = nullptr, int timeoutMs = 30000);
// Titel, Zutaten (Name, Zusatz, Überschriften) und Anleitung eines Rezepts übersetzen.
// Setzt language = "de", originalLanguage und translatedBy = "DeepL".
bool translateRecipe(Recipe &r, const QString &key, QString *error = nullptr);
// Anzahl Zeichen, die translateRecipe senden würde (für die Kontingent-Anzeige)
int characterCount(const Recipe &r);

// Einstellungen (QSettings „deepl/…“)
struct Settings {
    bool enabled = false;
    QString key;
    static Settings load();
    void save() const;
    bool usable() const { return enabled && !key.trimmed().isEmpty(); }
};

} // namespace DeepL
