// ViviaMeals – Nährwerte von Fertigprodukten aus Open Food Facts (https://openfoodfacts.org)
//
// Abfrage per Barcode (EAN-8/EAN-13/UPC-A/GTIN-14, mit Prüfziffer) über /api/v2/product/<code>.json
// oder Suche nach Namen über /cgi/search.pl (höchstens 10 Suchen je Minute erlaubt). Gesendet wird nur
// der Barcode bzw. Suchbegriff, mit eigener User-Agent-Kennung wie von Open Food Facts verlangt.
// Die Daten stehen unter der Open Database License (ODbL) – Herkunft wird im Lebensmittel vermerkt.
// Für Tests: Umgebungsvariable VIVIA_OFF_URL (z. B. http://127.0.0.1:8080).
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

#include <QJsonObject>
#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace OpenFoodFacts {

struct Product {
    QString code;
    QString name;
    QString brand;
    QString quantity;          // „400 g“, „1 l“ (wie angegeben)
    double packageAmount = 0;  // Inhalt in g bzw. ml
    bool perMl = false;        // Flüssigkeit: Nährwerte je 100 ml, Inhalt in ml
    double servingGrams = 0;   // Portionsangabe des Herstellers
    QString servingSize;       // „15 g“
    Nutrients per100;          // je 100 g bzw. 100 ml
    bool hasNutrition = false;
    QStringList missing;       // fehlende Angaben („Ballaststoffe“, „Salz“ …)
    QString category;          // ViviaMeals-Kategorie aus categories_tags
    QString imageUrl;
};

QString baseUrl();
QByteArray userAgent();
// Ziffern herausziehen und prüfen; leer bei Fehler (Fehlertext in error)
QString normalizeBarcode(const QString &text, QString *error = nullptr);
bool validGtin(const QString &digits);
QUrl productUrl(const QString &code);
QUrl searchUrl(const QString &terms, int pageSize = 20);
QString productPage(const QString &code);   // Link zur Produktseite (zum Nachsehen/Ergänzen)

Product parseProduct(const QJsonObject &p);
// Antwort der Barcode-Abfrage; false + Fehlertext, wenn nicht gefunden oder unlesbar
bool parseProductReply(const QByteArray &json, Product *out, QString *error = nullptr);
QList<Product> parseSearchReply(const QByteArray &json, QString *error = nullptr);
QString categoryFromTags(const QStringList &tags);
double parseQuantity(const QString &text, bool *perMl = nullptr);   // „400 g“, „1,5 l“, „6 x 125 g“ → g bzw. ml

class Client : public QObject
{
    Q_OBJECT
public:
    explicit Client(QObject *parent = nullptr);
    void lookup(const QString &code);
    void search(const QString &terms);
    void cancel();
    bool busy() const { return m_reply != nullptr; }

signals:
    void productFound(const OpenFoodFacts::Product &p);
    void searchFinished(const QList<OpenFoodFacts::Product> &results);
    void failed(const QString &error);

private:
    void get(const QUrl &url, bool isSearch);
    QNetworkAccessManager *m_nam;
    QNetworkReply *m_reply = nullptr;
};

} // namespace OpenFoodFacts
