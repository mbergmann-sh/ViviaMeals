// ViviaMeals – Freigabe der Datenbank im lokalen Netz
//
// Ein kleiner eingebauter HTTP-Server (gleiches Protokoll wie das PHP-Skript, siehe sync.h) stellt den
// Sync-Speicher des freigebenden Rechners bereit. Andere Geräte finden ihn per UDP-Rundruf und koppeln
// sich einmalig mit einem sechsstelligen Kopplungscode; danach gilt ein dauerhafter Zugangsschlüssel
// (hier nur als SHA-256 in den Einstellungen sync/serverTokens gespeichert).
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

class QTcpServer;
class QTcpSocket;
class QUdpSocket;

namespace Sync {

constexpr quint16 kLanPort = 47420;
constexpr quint16 kDiscoveryPort = 47421;

class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);
    ~Server() override;

    // storeDir: Sync-Speicher (hostStoreDir); name: Datenbankname für die Suche im Netz
    bool start(const QString &storeDir, const QString &name, quint16 port = kLanPort, QString *error = nullptr);
    void stop();
    bool isRunning() const;
    quint16 port() const;
    QString pairingCode() const { return m_code; }
    void newPairingCode();
    QStringList addresses() const;   // http://<IPv4>:<port>/ dieses Rechners
    // Zugangsschlüssel (Tests; sonst über die Kopplung)
    void addToken(const QString &token);
    void setPersistTokens(bool on) { m_persist = on; }
    int requests() const { return m_requests; }

signals:
    void committed(const QString &device);   // ein anderes Gerät hat abgeglichen
    void paired(const QString &device);

private:
    void onConnection();
    void onReadyRead(QTcpSocket *s);
    void handle(QTcpSocket *s, const QByteArray &method, const QByteArray &target, const QHash<QByteArray, QByteArray> &headers,
                const QByteArray &body);
    void reply(QTcpSocket *s, int status, const QByteArray &data, const QByteArray &type = "application/json; charset=utf-8");
    void onDatagram();
    bool tokenOk(const QByteArray &token) const;

    QTcpServer *m_tcp = nullptr;
    QUdpSocket *m_udp = nullptr;
    QString m_dir, m_name, m_code;
    QSet<QString> m_tokens;   // SHA-256
    QHash<QTcpSocket *, QByteArray> m_buf;
    bool m_persist = true;
    int m_requests = 0;
    int m_failedPairs = 0;
};

struct Found {
    QString name, device, uuid, url;
};
// sucht freigegebene ViviaMeals-Datenbanken im lokalen Netz (blockiert ms Millisekunden)
QList<Found> discover(int ms = 1500, quint16 port = kDiscoveryPort);

} // namespace Sync
