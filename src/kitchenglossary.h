// ViviaMeals – Küchen-Glossar (Hilfe → Küchen-Glossar)
//
// Fachbegriffe der gehobenen Küche mit Kategorie, kurzer Erklärung, anderen Schreibweisen und Verweisen.
// Die Daten stehen in resources/data/kuechenglossar.json (erzeugt von tools/make_glossar.py).
// Die Suche ist unabhängig von Groß-/Kleinschreibung und Akzenten („bechamel“ findet „Béchamel“) und
// ordnet Treffer im Begriff vor Treffern in anderen Schreibweisen vor Treffern in der Erklärung.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace KitchenGlossary {

struct Entry {
    QString term;
    QString category;
    QString text;
    QStringList synonyms;
    QStringList seeAlso;
};

const QList<Entry> &entries();          // alphabetisch nach Begriff
QStringList categories();               // in Anzeigereihenfolge
int indexOf(const QString &term);       // Begriff oder andere Schreibweise → Index, -1 = unbekannt
QString fold(const QString &s);         // klein, ohne Akzente, ß → ss

// Indizes der passenden Einträge, die besten zuerst; leere Anfrage = alle (alphabetisch)
QList<int> search(const QString &query, const QString &category = QString());

} // namespace KitchenGlossary
