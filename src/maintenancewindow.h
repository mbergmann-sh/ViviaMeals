// ViviaMeals – Wartungslisten für die stückweise Pflege der Sammlung (Extras)
//
//   Rezepte ohne Nährwerte bearbeiten:   Rezepte mit Zutaten, bei denen keine oder nicht alle Zutaten
//                                        Nährwerte haben (unbekanntes Lebensmittel, Menge nicht umrechenbar)
//   Rezepte ohne Zutatenliste bearbeiten: Rezepte ohne eine einzige Zutat
//
// Nicht-modales Fenster rechts neben dem Hauptfenster, je Art höchstens einmal offen. Ein Klick öffnet das
// Rezept im Editor; nach dem Speichern aktualisiert sich die Liste von selbst (erledigte Rezepte fallen
// heraus, die Auswahl rückt zum nächsten). Unter der Liste stehen für das gewählte Rezept die Zutaten
// ohne Nährwerte bzw. der Anfang der Zubereitung.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

class MaintenanceWindow : public QWidget
{
    Q_OBJECT
public:
    enum Mode { MissingNutrition, MissingIngredients };
    MaintenanceWindow(Mode mode, QWidget *mainWindow);

    Mode mode() const { return m_mode; }
    void placeBesideMain();        // rechts neben das Hauptfenster (bzw. an den rechten Bildschirmrand)
    static QString title(Mode mode);

signals:
    void editRequested(const QString &recipeId);

protected:
    void hideEvent(QHideEvent *e) override;

private:
    void refresh();
    void applyFilter();
    void showDetails(QTreeWidgetItem *item);
    void open(QTreeWidgetItem *item);

    Mode m_mode;
    QWidget *m_main = nullptr;
    QLabel *m_info = nullptr;
    QLineEdit *m_filter = nullptr;
    QTreeWidget *m_list = nullptr;
    QLabel *m_details = nullptr;
    QTimer *m_refresh = nullptr;
    QString m_lastOpened;          // zuletzt geöffnetes Rezept
};
