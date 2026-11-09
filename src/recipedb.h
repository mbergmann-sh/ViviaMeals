// VitaMeals – Rezeptdatenbank als Ordner
//
//   <Ordner>/vitameals.vmdb     Kennung der Datenbank (JSON: Name, Formatversion, angelegt)
//   <Ordner>/rezepte/<id>.vmr   ein Rezept je Datei (siehe recipe.h)
//   <Ordner>/index.json         Zwischenspeicher mit Kurzfassungen (wird bei Bedarf neu aufgebaut)
//
// Im Speicher stehen nur die Kurzfassungen (Titel, Tags, Nährwerte, Zutatennamen); vollständige
// Rezepte (mit Bild) werden bei Bedarf geladen. So bleiben auch große Sammlungen schnell.
// SPDX-License-Identifier: MIT
#pragma once

#include "recipe.h"

#include <QHash>
#include <QObject>

struct RecipeSummary {
    QString id;
    QString title;
    QString normTitle;
    QStringList tags;
    double servings = 1;
    QString yieldUnit;
    Nutrients perServing;
    double coverage = 0;            // Anteil der Zutaten mit Nährwerten
    bool hasNutrition = false;
    QStringList ingredientNames;    // Namen klein geschrieben
    QStringList foods;              // zugeordnete Lebensmittel-Schlüssel
    bool hasImage = false;
    bool favorite = false;
    QString language;
    QDateTime modified;
    qint64 fileTime = 0;            // Änderungszeit der .vmr-Datei (ms)

    static RecipeSummary from(const Recipe &r);
    QJsonObject toJson() const;
    static RecipeSummary fromJson(const QJsonObject &o);
};

class RecipeDb : public QObject
{
    Q_OBJECT
public:
    static RecipeDb &instance();
    static QString markerName() { return QStringLiteral("vitameals.vmdb"); }

    // Neue Datenbank in einem (leeren oder neuen) Ordner anlegen
    bool create(const QString &folder, const QString &name, QString *error = nullptr);
    // Ordner oder Kennungsdatei öffnen
    bool open(const QString &path, QString *error = nullptr);
    void close();
    bool isOpen() const { return !m_folder.isEmpty(); }
    QString folder() const { return m_folder; }
    QString name() const { return m_name; }
    static bool isDatabaseFolder(const QString &folder);

    const QList<RecipeSummary> &summaries() const { return m_list; }
    const RecipeSummary *summary(const QString &id) const;
    int count() const { return int(m_list.size()); }
    bool load(const QString &id, Recipe *out, QString *error = nullptr) const;
    bool save(Recipe &r, QString *error = nullptr);     // setzt modified/created
    bool remove(const QString &id);
    QString fileFor(const QString &id) const;

    QStringList duplicatesOf(const Recipe &r) const;    // IDs mit gleichem (normalisiertem) Titel
    QStringList allTags() const;                        // alle Tags, nach Häufigkeit
    QString randomId() const;

    // Mehrere Änderungen bündeln (Import): Index erst am Ende schreiben, ein changed()-Signal
    void beginBatch();
    void endBatch();

signals:
    void opened();
    void changed();

private:
    RecipeDb() = default;
    void rebuildIndex();
    void writeIndex();
    void touched();
    QString m_folder, m_name;
    QList<RecipeSummary> m_list;
    QHash<QString, int> m_pos;
    int m_batch = 0;
    bool m_dirty = false;
};
