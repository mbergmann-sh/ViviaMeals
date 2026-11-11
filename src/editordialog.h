// ViviaMeals – Rezepteditor
//
// Eingabefelder für Titel, Rezeptbild (mit Dateiauswahl), Portionen, Tags, Zutatenliste (eine
// Zutat je Zeile, „== Überschrift ==“ für Abschnitte) und Zubereitung. Aus der Zutatenliste und
// den erkannten Mengenangaben werden laufend Kalorien/Joule und alle Nährwerte je Portion
// berechnet; die Zuordnung einer Zutat zur Nährwerttabelle lässt sich per Doppelklick ändern.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

#include <QDialog>
#include <QImage>
#include <QList>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QSplitter;
class QTableWidget;
class QTimer;

class EditorDialog : public QDialog
{
    Q_OBJECT
public:
    EditorDialog(const Recipe &r, QWidget *parent = nullptr);
    Recipe recipe() const { return m_recipe; }
    // gescannte Vorlage rechts neben dem Editor anzeigen (Rezept scannen)
    void setReference(const QList<QImage> &pages);

protected:
    void accept() override;

private:
    void analyze();
    void updateImage();
    void chooseImage();
    void assignFood(int row);
    bool offerMissingFoods(bool ask);
    static QString defaultSource();                       // Einstellungen → Allgemein → Standard-Quelle
    static QString sourceOrDefault(const QString &source);   // true = Nährwerttabelle geändert
    void collect();
    void showRecipe();     // m_recipe in die Eingabefelder übernehmen
    void fixUmlauts();
    void translateDeepL();
    QLineEdit *m_title;
    QLabel *m_image;
    QDoubleSpinBox *m_servings;
    QComboBox *m_yield;
    QSpinBox *m_prep;
    QLineEdit *m_tags;
    QCheckBox *m_fav;
    QLineEdit *m_source;
    QPlainTextEdit *m_ingredients;
    QPlainTextEdit *m_instructions;
    QTableWidget *m_table;
    QLabel *m_summary;
    QTimer *m_timer;
    QSplitter *m_split;
    Recipe m_recipe;
    bool m_isNew = false;
    bool m_offered = false;
    QHash<QString, QString> m_manual;   // Zutatenname (klein) → manuell gewähltes Lebensmittel
};
