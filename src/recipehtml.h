// VitaMeals – Rezept als HTML (Anzeigefenster und Website-Export)
//
// Aufbau: Kopf mit Titel, Portionen, kcal/kJ und Tags, rechts oben das Rezeptfoto; darunter
// Zutaten (deutsche Mengenangaben), Zubereitung und die Nährwerttabelle (je Portion, gesamt,
// Anteil an der Referenzmenge). Das HTML ist auf den Teil von HTML/CSS beschränkt, den
// QTextBrowser darstellt, und wird im Website-Export zusätzlich per CSS gestaltet.
// SPDX-License-Identifier: MIT AND LicenseRef-Commons-Clause-1.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

#include <QColor>
#include <QString>

namespace RecipeHtml {

struct Style {
    QColor text = QColor(0x20, 0x20, 0x20);
    QColor muted = QColor(0x70, 0x70, 0x70);
    QColor accent = QColor(0xE0, 0x62, 0x0D);    // Orange der Vita-Programme
    QColor primary = QColor(0x14, 0x74, 0xBC);   // Blau der Vita-Programme
    QColor border = QColor(0xd0, 0xd0, 0xd0);
    QColor headBg = QColor(0xf2, 0xf6, 0xfa);
    int imageWidth = 300;          // Breite des Fotos in px
    double fontScale = 1.0;
    QString imageSrc;              // URL des Bildes (leer = Rezeptbild als data:-URI einbetten)
    bool forWeb = false;           // Website: CSS-Klassen statt Inline-Farben
};

// servings ≤ 0: Portionen des Rezepts; sonst Mengen auf diese Portionenzahl umrechnen
QString render(const Recipe &r, const Style &style, double servings = 0);
QString nutritionTable(const Recipe &r, const Style &style, double servings = 0);
QString ingredientsTable(const Recipe &r, const Style &style, double factor = 1.0);
QString instructionsHtml(const Recipe &r);
QString escape(const QString &s);
// Hinweis zur Sprache der Anleitung („Aus dem Englischen übersetzt (DeepL)“), leer bei deutschen Originalen
QString languageNote(const Recipe &r);
QString languageName(const QString &code);   // "en" → „Englisch“
QString tagLabel(const QString &tag);     // „fleischgerichte“ → „Fleischgerichte“
QString emptyPage(const Style &style, const QString &title, const QString &text);

} // namespace RecipeHtml
