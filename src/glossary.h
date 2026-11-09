// VitaMeals – Offline-Glossar Englisch → Deutsch für importierte Rezepte
//
// Übersetzt Zutatennamen, Zusätze („chopped“ → „gehackt“) und Kategorien phrasenweise (längste
// bekannte Wortfolge zuerst). Unbekannte Wörter bleiben unverändert. Ganze Anleitungstexte
// werden nicht übersetzt; darin werden nur Temperaturen und Maße umgerechnet (Units).
// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <QStringList>

namespace Glossary {

bool looksEnglish(const QString &text);          // Sprache eines Rezepts schätzen
// Sprache über häufige Funktionswörter schätzen: "de", "en", "fr", "it", "es", "nl" (Vorgabe "de")
QString detectLanguage(const QString &text);
// „all-purpose flour“ → „Weizenmehl“; vorangestellte Zubereitungswörter („chopped walnuts“) kommen in *leadNote
QString ingredient(const QString &englishName, QString *leadNote = nullptr);
QString note(const QString &englishNote);        // „finely chopped“ → „fein gehackt“
QString category(const QString &englishCategory);// „Desserts“ → „desserts“, „Meats“ → „fleischgerichte“
QString germanCategory(const QString &cat);      // RezKonv-Kategorien vereinheitlichen
// Anteil der Wörter, die übersetzt werden konnten (0…1)
double coverage(const QString &englishName);

} // namespace Glossary
