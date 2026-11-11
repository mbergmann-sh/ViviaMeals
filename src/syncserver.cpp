// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "syncserver.h"
#include "sync.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>
#include <QUrl>
#include <QUrlQuery>

namespace Sync {

namespace {
const QByteArray kDiscover = "VIVIAMEALS-DISCOVER 1";
constexpr int kMaxRequest = 24 * 1024 * 1024;

QByteArray json(const QJsonObject &o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); }
QByteArray err(const QString &m) { return json(QJsonObject{ { QStringLiteral("error"), m } }); }
} // namespace

Server::Server(QObject *parent) : QObject(parent)
{
    for (const QString &t : QSettings().value(QStringLiteral("sync/serverTokens")).toStringList()) m_tokens.insert(t);
}

Server::~Server() { stop(); }

bool Server::start(const QString &storeDir, const QString &name, quint16 port, QString *error)
{
    stop();
    m_dir = storeDir;
    m_name = name;
    m_tcp = new QTcpServer(this);
    if (!m_tcp->listen(QHostAddress::AnyIPv4, port)) {
        if (error) *error = tr("Port %1 ist belegt oder gesperrt: %2").arg(port).arg(m_tcp->errorString());
        delete m_tcp;
        m_tcp = nullptr;
        return false;
    }
    connect(m_tcp, &QTcpServer::newConnection, this, &Server::onConnection);
    m_udp = new QUdpSocket(this);
    if (m_udp->bind(QHostAddress::AnyIPv4, kDiscoveryPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
        connect(m_udp, &QUdpSocket::readyRead, this, &Server::onDatagram);
    newPairingCode();
    return true;
}

void Server::stop()
{
    if (m_tcp) {
        m_tcp->close();
        delete m_tcp;
        m_tcp = nullptr;
    }
    if (m_udp) {
        delete m_udp;
        m_udp = nullptr;
    }
    m_buf.clear();
}

bool Server::isRunning() const { return m_tcp && m_tcp->isListening(); }
quint16 Server::port() const { return m_tcp ? m_tcp->serverPort() : 0; }

void Server::newPairingCode()
{
    m_code = QStringLiteral("%1").arg(QRandomGenerator::system()->bounded(1000000), 6, 10, QLatin1Char('0'));
    m_failedPairs = 0;
}

QStringList Server::addresses() const
{
    QStringList l;
    for (const QHostAddress &a : QNetworkInterface::allAddresses())
        if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback())
            l << QStringLiteral("http://%1:%2/").arg(a.toString()).arg(port());
    if (l.isEmpty()) l << QStringLiteral("http://127.0.0.1:%1/").arg(port());
    return l;
}

void Server::addToken(const QString &token)
{
    m_tokens.insert(sha256(token.toLatin1()));
    if (m_persist) {
        QStringList l(m_tokens.begin(), m_tokens.end());
        QSettings().setValue(QStringLiteral("sync/serverTokens"), l);
    }
}

bool Server::tokenOk(const QByteArray &token) const { return !token.isEmpty() && m_tokens.contains(sha256(token)); }

void Server::onConnection()
{
    while (QTcpSocket *s = m_tcp->nextPendingConnection()) {
        connect(s, &QTcpSocket::readyRead, this, [this, s] { onReadyRead(s); });
        connect(s, &QTcpSocket::disconnected, this, [this, s] {
            m_buf.remove(s);
            s->deleteLater();
        });
    }
}

void Server::onReadyRead(QTcpSocket *s)
{
    QByteArray &b = m_buf[s];
    b += s->readAll();
    if (b.size() > kMaxRequest) {
        reply(s, 413, err(tr("Anfrage zu groß")));
        return;
    }
    const int end = b.indexOf("\r\n\r\n");
    if (end < 0) return;
    const QList<QByteArray> lines = b.left(end).split('\n');
    const QList<QByteArray> first = lines.value(0).trimmed().split(' ');
    if (first.size() < 2) {
        reply(s, 400, err(tr("Ungültige Anfrage")));
        return;
    }
    QHash<QByteArray, QByteArray> headers;
    for (int i = 1; i < lines.size(); ++i) {
        const int c = lines[i].indexOf(':');
        if (c > 0) headers.insert(lines[i].left(c).trimmed().toLower(), lines[i].mid(c + 1).trimmed());
    }
    const int len = headers.value("content-length", "0").toInt();
    if (b.size() < end + 4 + len) return;   // Rumpf noch nicht vollständig
    const QByteArray body = b.mid(end + 4, len);
    b.clear();
    handle(s, first[0], first[1], headers, body);
}

void Server::reply(QTcpSocket *s, int status, const QByteArray &data, const QByteArray &type)
{
    static const QHash<int, QByteArray> text{ { 200, "OK" },        { 400, "Bad Request" }, { 401, "Unauthorized" }, { 403, "Forbidden" },
                                              { 404, "Not Found" }, { 409, "Conflict" },    { 413, "Too Large" },    { 423, "Locked" },
                                              { 500, "Error" } };
    QByteArray h = "HTTP/1.1 " + QByteArray::number(status) + ' ' + text.value(status, "Status") + "\r\n";
    h += "Content-Type: " + type + "\r\nContent-Length: " + QByteArray::number(data.size()) + "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
    s->write(h + data);
    s->disconnectFromHost();
}

void Server::handle(QTcpSocket *s, const QByteArray &method, const QByteArray &target, const QHash<QByteArray, QByteArray> &headers,
                    const QByteArray &body)
{
    ++m_requests;
    const QUrlQuery q(QUrl(QString::fromLatin1(target)).query());
    const QString a = q.queryItemValue(QStringLiteral("a"));
    const QString p = q.queryItemValue(QStringLiteral("p"), QUrl::FullyDecoded);
    const QJsonObject bo = QJsonDocument::fromJson(body).object();
    Store store(m_dir);

    if (a == QLatin1String("pair")) {
        // Kopplung: richtiger Code → neuer Zugangsschlüssel; nach 5 Fehlversuchen gilt ein neuer Code
        if (method != "POST" || q.queryItemValue(QStringLiteral("code")) != m_code || m_failedPairs >= 5) {
            if (++m_failedPairs >= 5) newPairingCode();
            reply(s, 403, err(tr("Kopplungscode falsch")));
            return;
        }
        const QString token = newToken();
        addToken(token);
        newPairingCode();   // jeder Code gilt nur einmal
        emit paired(bo.value(QStringLiteral("device")).toString());
        reply(s, 200, json(QJsonObject{ { QStringLiteral("token"), token }, { QStringLiteral("name"), m_name } }));
        return;
    }
    if (!tokenOk(headers.value("x-viviameals-token"))) {
        reply(s, 401, err(tr("Zugang verweigert")));
        return;
    }
    if (a == QLatin1String("info")) {
        const StoreInfo i = store.info();
        reply(s, 200, json(QJsonObject{ { QStringLiteral("format"), QStringLiteral("ViviaMeals-Sync") },
                                        { QStringLiteral("version"), kProtocolVersion },
                                        { QStringLiteral("initialized"), i.initialized },
                                        { QStringLiteral("uuid"), i.uuid },
                                        { QStringLiteral("name"), i.name },
                                        { QStringLiteral("revision"), i.revision },
                                        { QStringLiteral("files"), i.files } }));
    } else if (a == QLatin1String("init")) {
        QString e;
        if (!store.init(bo.value(QStringLiteral("uuid")).toString(), bo.value(QStringLiteral("name")).toString(), &e)) reply(s, 500, err(e));
        else reply(s, 200, json(QJsonObject{ { QStringLiteral("ok"), true } }));
    } else if (a == QLatin1String("manifest")) {
        reply(s, 200, json(store.manifest().toJson()));
    } else if (a == QLatin1String("get")) {
        QByteArray data;
        if (!store.get(p, &data)) reply(s, 404, err(tr("Nicht gefunden")));
        else reply(s, 200, data, "application/octet-stream");
    } else if (a == QLatin1String("put")) {
        QString e;
        if (!store.put(p, body, q.queryItemValue(QStringLiteral("h")), &e)) reply(s, 400, err(e));
        else reply(s, 200, json(QJsonObject{ { QStringLiteral("ok"), true } }));
    } else if (a == QLatin1String("del")) {
        if (!store.remove(p)) reply(s, 400, err(tr("Ungültiger Pfad")));
        else reply(s, 200, json(QJsonObject{ { QStringLiteral("ok"), true } }));
    } else if (a == QLatin1String("lock")) {
        QString holder;
        if (store.lock(bo.value(QStringLiteral("device")).toString(), &holder)) reply(s, 200, json(QJsonObject{ { QStringLiteral("ok"), true } }));
        else reply(s, 423, json(QJsonObject{ { QStringLiteral("holder"), holder } }));
    } else if (a == QLatin1String("unlock")) {
        store.unlock(bo.value(QStringLiteral("device")).toString());
        reply(s, 200, json(QJsonObject{ { QStringLiteral("ok"), true } }));
    } else if (a == QLatin1String("commit")) {
        int rev = 0;
        bool conflict = false;
        QString e;
        if (store.commit(bo.value(QStringLiteral("expect")).toInt(-1), bo, &rev, &conflict, &e)) {
            reply(s, 200, json(QJsonObject{ { QStringLiteral("revision"), rev } }));
            emit committed(bo.value(QStringLiteral("device")).toString().section(QLatin1Char('|'), 1));
        } else {
            reply(s, conflict ? 409 : 500, err(e));
        }
    } else {
        reply(s, 400, err(tr("Unbekannte Aktion")));
    }
}

void Server::onDatagram()
{
    while (m_udp && m_udp->hasPendingDatagrams()) {
        QByteArray d;
        d.resize(int(m_udp->pendingDatagramSize()));
        QHostAddress from;
        quint16 fromPort = 0;
        m_udp->readDatagram(d.data(), d.size(), &from, &fromPort);
        if (!d.startsWith(kDiscover)) continue;
        const StoreInfo i = Store(m_dir).info();
        const QJsonObject o{ { QStringLiteral("format"), QStringLiteral("ViviaMeals-Freigabe") },
                             { QStringLiteral("name"), m_name.isEmpty() ? i.name : m_name },
                             { QStringLiteral("device"), deviceName() },
                             { QStringLiteral("uuid"), i.uuid },
                             { QStringLiteral("port"), int(port()) } };
        m_udp->writeDatagram(json(o), from, fromPort);
    }
}

QList<Found> discover(int ms, quint16 port)
{
    QList<Found> out;
    QUdpSocket sock;
    if (!sock.bind(QHostAddress::AnyIPv4, 0)) return out;
    auto send = [&] {
        sock.writeDatagram(kDiscover, QHostAddress::Broadcast, port);
        sock.writeDatagram(kDiscover, QHostAddress::LocalHost, port);   // auch eine Freigabe auf diesem Rechner
        for (const QNetworkInterface &ni : QNetworkInterface::allInterfaces())
            for (const QNetworkAddressEntry &e : ni.addressEntries())
                if (e.ip().protocol() == QAbstractSocket::IPv4Protocol && !e.broadcast().isNull()) sock.writeDatagram(kDiscover, e.broadcast(), port);
    };
    send();
    QElapsedTimer t;
    t.start();
    QSet<QString> seen;
    while (t.elapsed() < ms) {
        QEventLoop loop;
        QTimer::singleShot(100, &loop, &QEventLoop::quit);
        QObject::connect(&sock, &QUdpSocket::readyRead, &loop, &QEventLoop::quit);
        loop.exec();
        while (sock.hasPendingDatagrams()) {
            QByteArray d;
            d.resize(int(sock.pendingDatagramSize()));
            QHostAddress from;
            sock.readDatagram(d.data(), d.size(), &from);
            const QJsonObject o = QJsonDocument::fromJson(d).object();
            if (o.value(QStringLiteral("format")).toString() != QLatin1String("ViviaMeals-Freigabe")) continue;
            QHostAddress ip(from.toIPv4Address());
            const QString url = QStringLiteral("http://%1:%2/").arg(ip.toString()).arg(o.value(QStringLiteral("port")).toInt());
            const QString key = o.value(QStringLiteral("uuid")).toString() + QLatin1Char('@') + o.value(QStringLiteral("device")).toString();
            if (seen.contains(key)) continue;   // derselbe Rechner über mehrere Wege
            seen.insert(key);
            out << Found{ o.value(QStringLiteral("name")).toString(), o.value(QStringLiteral("device")).toString(), o.value(QStringLiteral("uuid")).toString(), url };
        }
    }
    return out;
}

} // namespace Sync
