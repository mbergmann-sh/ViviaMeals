// ViviaMeals – Netzwerkscanner über eSCL („AirScan“, Mopria Scan, AirPrint-Scan)
//
// Viele WLAN-Multifunktionsgeräte (Epson, HP, Canon, Brother …) sprechen das offene eSCL-Protokoll:
// einfache HTTP-Aufrufe an http://<Adresse>/eSCL – ganz ohne Treiber, auf Windows, Linux und macOS.
//   GET  /eSCL/ScannerCapabilities   Modell, Auflösungen, Farbmodi, Bildformate, Scanfläche
//   GET  /eSCL/ScannerStatus         Idle / Processing …
//   POST /eSCL/ScanJobs              Scanauftrag (ScanSettings-XML) → 201 + Location
//   GET  <Location>/NextDocument     das gescannte Bild (503 = noch nicht fertig, 404 = keine weitere Seite)
// Gescannt wird vom Vorlagenglas mit 300 dpi (bzw. der nächstliegenden Auflösung), Farbe oder
// Graustufen, als JPEG (sonst PNG). Geräte mit selbst signiertem HTTPS-Zertifikat werden akzeptiert
// (nur für die eingetragene Adresse).
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QImage>
#include <QObject>
#include <QStringList>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace Escl {

struct Capabilities {
    QString makeAndModel;
    QString version = QStringLiteral("2.0");
    QList<int> resolutions;      // dpi (Vorlagenglas)
    QStringList colorModes;      // RGB24, Grayscale8, BlackAndWhite1
    QStringList formats;         // image/jpeg, image/png, application/pdf
    int maxWidth = 2550;         // 1/300 Zoll (A4 ≈ 2480 × 3508)
    int maxHeight = 3508;
    bool platen = true;
};

// „192.168.178.101“, „drucker.fritz.box“, „http://…/eSCL“ → Basisadresse …/eSCL
QUrl baseUrl(const QString &address, bool https = false);
Capabilities parseCapabilities(const QByteArray &xml);
QByteArray scanSettings(const Capabilities &caps, bool color, int dpi);
int chooseResolution(const Capabilities &caps, int wanted);
QString chooseFormat(const Capabilities &caps);

// Ein Scanvorgang (asynchron). Ablauf: Fähigkeiten → Auftrag → Bild abholen.
class Scanner : public QObject
{
    Q_OBJECT
public:
    explicit Scanner(QObject *parent = nullptr);
    ~Scanner() override;
    // nur Verbindung prüfen (Signal checked)
    void check(const QString &address);
    void scan(const QString &address, bool color, int dpi = 300);
    void cancel();
    bool busy() const { return m_busy; }

signals:
    void checked(bool ok, const QString &model, const QString &error);
    void progress(const QString &text);
    void finished(const QImage &image);
    void failed(const QString &error);

private:
    void getCapabilities(bool scanAfter);
    void postJob();
    void fetchDocument();
    void fail(const QString &e);
    QString describe(QNetworkReply *r) const;
    QNetworkReply *track(QNetworkReply *r);
    QNetworkAccessManager *m_nam;
    QTimer *m_timeout;
    QUrl m_base;
    QString m_address;
    bool m_triedHttps = false;
    bool m_color = true;
    int m_dpi = 300;
    int m_retries = 0;
    bool m_busy = false;
    QUrl m_job;
    Capabilities m_caps;
    QNetworkReply *m_reply = nullptr;
};

} // namespace Escl
