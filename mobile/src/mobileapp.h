// ViviaMeals Mobil – Schnittstelle zwischen Kern und QML-Oberfläche
//
// Ein Objekt „app“ für alle Seiten: Rezepte (Ansicht, Bearbeiten, Favoriten), Einkaufsliste, Kühlschrank,
// Wochenplan (aus dem Abgleich), Tagebuch und Gewicht für eine Person, Küchen-Glossar, Import von
// Rezeptpaketen, Abgleich mit dem PC und Einstellungen. Lang laufende Arbeiten (Import, Abgleich, Suche im
// Heimnetz) laufen in einem eigenen Thread; das Ergebnis kommt als Signal zurück.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipelistmodel.h"

#include <QJsonArray>
#include <QObject>
#include <QTextStream>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

struct Recipe;

class MobileApp : public QObject
{
    Q_OBJECT
    Q_PROPERTY(RecipeListModel *recipes READ recipes CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString productUrl READ productUrl CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString busyText READ busyText NOTIFY busyChanged)
    Q_PROPERTY(int busyDone READ busyDone NOTIFY busyChanged)
    Q_PROPERTY(int busyTotal READ busyTotal NOTIFY busyChanged)
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY settingsChanged)       // 0 System, 1 hell, 2 dunkel
    Q_PROPERTY(bool keepScreenOn READ keepScreenOn WRITE setKeepScreenOn NOTIFY settingsChanged)
    Q_PROPERTY(bool firstRun READ firstRun NOTIFY settingsChanged)
    Q_PROPERTY(bool isAndroid READ isAndroid CONSTANT)
    Q_PROPERTY(int shoppingOpen READ shoppingOpen NOTIFY shoppingChanged)
    Q_PROPERTY(QString seasonLabel READ seasonLabel CONSTANT)
    Q_PROPERTY(bool syncConfigured READ syncConfigured NOTIFY syncConfigChanged)
    Q_PROPERTY(QString syncStatus READ syncStatus NOTIFY syncConfigChanged)

public:
    explicit MobileApp(QObject *parent = nullptr);
    ~MobileApp() override;

    bool start(QString *error = nullptr);   // Datenbank öffnen bzw. beim ersten Start mit Beispielrezepten anlegen
    static QString sampleSource();            // Quelle der mitgelieferten Beispielrezepte
    static QString databaseFolder();

    RecipeListModel *recipes() { return m_recipes; }
    QString version() const;
    QString productUrl() const;
    bool busy() const { return m_busy; }
    QString busyText() const { return m_busyText; }
    int busyDone() const { return m_busyDone; }
    int busyTotal() const { return m_busyTotal; }
    int themeMode() const;
    void setThemeMode(int m);
    bool keepScreenOn() const;
    void setKeepScreenOn(bool on);
    bool firstRun() const;
    bool isAndroid() const;
    int shoppingOpen() const;
    QString seasonLabel() const;
    bool syncConfigured() const;
    QString syncStatus() const;

    // ---- Rezepte
    Q_INVOKABLE QVariantMap recipe(const QString &id, double servings = 0) const;
    Q_INVOKABLE QVariantList themes() const;
    Q_INVOKABLE bool toggleFavorite(const QString &id);
    Q_INVOKABLE QString randomRecipe() const;
    Q_INVOKABLE QVariantMap recipeForEdit(const QString &id) const;          // leere ID = neues Rezept
    Q_INVOKABLE QVariantMap previewNutrition(const QString &ingredients, double servings) const;
    Q_INVOKABLE QString saveRecipe(const QVariantMap &data);                  // gibt die ID zurück, leer bei Fehler
    Q_INVOKABLE bool deleteRecipe(const QString &id);
    Q_INVOKABLE QString recipeText(const QString &id) const;                 // zum Teilen
    Q_INVOKABLE void setCooking(bool on);                                     // Kochansicht: Bildschirm anlassen
    Q_INVOKABLE void share(const QString &title, const QString &text);

    // ---- Einkaufsliste und Kühlschrank
    Q_INVOKABLE QVariantList shoppingItems() const;
    Q_INVOKABLE int addRecipeToShopping(const QString &id, double servings, bool onlyMissing);
    Q_INVOKABLE void addShoppingText(const QString &text);
    Q_INVOKABLE void setShoppingChecked(int index, bool checked);
    Q_INVOKABLE void removeShoppingItem(int index);
    Q_INVOKABLE int removeCheckedShopping();
    Q_INVOKABLE void clearShopping();
    Q_INVOKABLE QString shoppingText() const;
    Q_INVOKABLE QStringList stock() const;
    Q_INVOKABLE void setStock(const QStringList &items);
    Q_INVOKABLE void addStock(const QString &item);
    Q_INVOKABLE void removeStock(const QString &item);
    Q_INVOKABLE QStringList basics() const;
    Q_INVOKABLE QVariantList fridgeSuggestions(int maxMissing) const;

    // ---- Wochenplan (wird am PC erstellt und kommt über den Abgleich)
    Q_INVOKABLE QVariantMap plan() const;
    Q_INVOKABLE int planToShopping();

    // ---- Person, Tagebuch, Gewicht (eine Person je Gerät)
    Q_INVOKABLE QVariantMap profile() const;
    Q_INVOKABLE void saveProfile(const QVariantMap &data);
    Q_INVOKABLE QVariantList profileChoices() const;   // nach dem Abgleich: Personen vom PC
    Q_INVOKABLE void chooseProfile(const QString &id);
    Q_INVOKABLE QVariantList palLevels() const;
    Q_INVOKABLE QStringList mealNames() const;
    Q_INVOKABLE QString mealForNow() const;
    Q_INVOKABLE QVariantMap diaryDay(const QString &isoDate) const;
    Q_INVOKABLE bool addDiaryRecipe(const QString &recipeId, double portions, const QString &meal, const QString &isoDate);
    Q_INVOKABLE bool addDiaryFree(const QString &title, double kcal, const QString &meal, const QString &isoDate);
    Q_INVOKABLE void removeDiaryEntry(const QString &id);
    Q_INVOKABLE QVariantList weights() const;
    Q_INVOKABLE void addWeight(const QString &isoDate, double kg);
    Q_INVOKABLE void removeWeight(const QString &isoDate);

    // ---- Küchen-Glossar
    Q_INVOKABLE QVariantList glossary(const QString &query) const;

    // ---- Import, Abgleich
    Q_INVOKABLE void importFile(const QUrl &url);
    Q_INVOKABLE QVariantMap syncConfig() const;
    Q_INVOKABLE QString setSyncWebsite(const QString &url, const QString &key);   // Fehlertext oder leer
    Q_INVOKABLE QString setSyncInvitation(const QString &code);
    Q_INVOKABLE void discoverLan();
    Q_INVOKABLE void pairLan(const QString &url, const QString &code, const QString &name);
    Q_INVOKABLE void removeSync();
    Q_INVOKABLE void setSyncAuto(bool on);
    Q_INVOKABLE int sampleCount() const;
    Q_INVOKABLE void syncNow(bool removeSamples = false);
    Q_INVOKABLE void autoSync();   // beim Start und beim Wechsel in den Hintergrund, falls eingerichtet

    // ---- Hinweis auf die große Version
    Q_INVOKABLE bool showEditorHint();      // höchstens jedes fünfte Mal, abschaltbar
    Q_INVOKABLE void disableEditorHint();
    Q_INVOKABLE void finishFirstRun();
    Q_INVOKABLE QString licenseText() const;   // LICENSE aus den Ressourcen

    // ---- Prüfen ohne Oberfläche (--selftest)
    int selfTest(QTextStream &out);

signals:
    void busyChanged();
    void settingsChanged();
    void shoppingChanged();
    void diaryChanged();
    void profileChanged();
    void databaseChanged();
    void syncConfigChanged();
    void message(const QString &text);
    void importFinished(bool ok, const QString &text);
    void syncFinished(bool ok, const QString &text);
    void lanFound(const QVariantList &devices);
    void lanPaired(bool ok, const QString &text);

private:
    void setBusy(bool on, const QString &text = QString(), int done = 0, int total = 0);
    QString profileId() const;
    void ensureProfile();
    QJsonArray loadShopping() const;
    void saveShopping(const QJsonArray &a);
    void reopenDatabase();
    bool installSamples(QString *error);

    RecipeListModel *m_recipes = nullptr;
    bool m_busy = false;
    QString m_busyText;
    int m_busyDone = 0, m_busyTotal = 0;
    QString m_lastSync;
};
