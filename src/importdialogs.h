// ViviaMeals – Import-Ablauf: Dateien wählen (einzeln oder mehrere), Vorschau, Doubletten-Abfrage
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "importers.h"

#include <QDialog>

class QCheckBox;
class QTableWidget;

class QProgressDialog;

namespace ImportFlow {
// Gibt die ID des ersten importierten Rezepts zurück (leer = nichts importiert)
QString run(QWidget *parent, Importers::Format format, const QStringList &files = {});
// Übersetzer für Importers::Options auf Grundlage der DeepL-Einstellungen (leer, wenn DeepL aus ist).
// Zeigt den Fortschritt im Dialog und bricht nach einem Schlüssel-/Kontingentfehler weitere Versuche ab.
std::function<bool(Recipe &, QString *)> deeplTranslator(QProgressDialog *progress = nullptr);
// Sprache für Listen: „deutsch“, „englisch → DeepL“, „englisch (Glossar)“ …
QString languageLabel(const Recipe &r);
QString umlautNote(int recipes);
// mitgelieferte gesunde Frühstücks- und Zwischenmahlzeit-Rezepte übernehmen (ohne Doubletten); Anzahl neuer Rezepte
int addHealthySuggestions(QWidget *parent);   // Hinweis „In 3 Rezepten wurden fehlende Umlaute ergänzt …“
}

// Vorschau der gefundenen Rezepte mit Auswahl
class ImportPreviewDialog : public QDialog
{
    Q_OBJECT
public:
    ImportPreviewDialog(const QList<Recipe> &recipes, const QStringList &origins, const QStringList &warnings, QWidget *parent = nullptr);
    QList<int> selected() const;

private:
    QTableWidget *m_table;
};

// Abfrage bei einer Doublette
class DuplicateDialog : public QDialog
{
    Q_OBJECT
public:
    enum Choice { Skip, Replace, KeepBoth };
    DuplicateDialog(const Recipe &incoming, const Recipe &existing, QWidget *parent = nullptr);
    Choice choice() const { return m_choice; }
    bool forAll() const;

private:
    Choice m_choice = Skip;
    QCheckBox *m_all;
};
