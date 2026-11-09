// VitaMeals – Motivation, Erklärungen und Alltagstipps zu Abnehmen, Gewicht halten und Zunehmen
//
// Texte für den Assistenten (Übersicht und Reiter „Tipps“). Grundlage sind allgemein anerkannte
// Empfehlungen, vor allem „Gut essen und trinken“ der Deutschen Gesellschaft für Ernährung (DGE, 2024)
// und die Bewegungsempfehlung der WHO (mindestens 150 Minuten mäßige Bewegung pro Woche).
// KEIN Medizinprodukt – allgemeine Hinweise, keine individuelle Beratung.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "profiles.h"

#include <QList>
#include <QString>

namespace Coaching {

struct Swap {
    QString instead;    // statt …
    QString better;     // lieber …
    QString why;        // warum / Ersparnis
};

// kurzer, persönlicher Motivationstext (Fortschritt seit dem ersten Eintrag, nächster Schritt)
QString motivation(const Profile &p);
// kurze Erklärung, wie Abnehmen/Halten/Zunehmen funktioniert
QString explanation(Profile::Goal goal);
// Austauschtipps „statt … lieber …“
QList<Swap> swaps(Profile::Goal goal);
// allgemeine Regeln (DGE) und zielbezogene Gewohnheiten
QStringList habits(Profile::Goal goal);
// alles als HTML für den Reiter „Tipps“
QString tipsHtml(const Profile &p, const QString &primary, const QString &accent);

} // namespace Coaching
