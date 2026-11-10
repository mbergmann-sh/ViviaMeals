// ViviaMeals – Rezeptanzeige (HTML-Ansicht rechts im Hauptfenster)
//
// Zeigt das gewählte Rezept als HTML: rechts oben das Foto, daneben Titel, Portionen, kcal/kJ,
// Zutatenliste, Zubereitung und Nährwerttabelle. Über der Ansicht kann die Portionenzahl
// geändert werden (Mengen werden umgerechnet). Das Foto passt sich der Fensterbreite an.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QTextBrowser;
class QTextDocument;
class QTimer;
class QToolButton;

class RecipeView : public QWidget
{
    Q_OBJECT
public:
    explicit RecipeView(QWidget *parent = nullptr);
    void setRecipe(const Recipe &r);
    void showMessage(const QString &title, const QString &html);
    const Recipe &recipe() const { return m_recipe; }
    bool hasRecipe() const { return m_has; }
    double servings() const;         // eingestellte Portionenzahl
    void fillDocument(QTextDocument *doc) const;   // für den Druck (helle Farben)
    void refresh();                  // nach Theme-Wechsel

signals:
    void editRequested();
    void favoriteToggled(bool on);

protected:
    void resizeEvent(QResizeEvent *e) override;
    void changeEvent(QEvent *e) override;

private:
    void render();
    QTextBrowser *m_browser;
    QDoubleSpinBox *m_servings;
    QToolButton *m_fav;
    QToolButton *m_reset;
    QLabel *m_servLabel;
    QTimer *m_resizeTimer;
    Recipe m_recipe;
    bool m_has = false;
    int m_lastWidth = 0;
};
