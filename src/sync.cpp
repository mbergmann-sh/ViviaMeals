// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "sync.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>

#include <algorithm>

namespace Sync {

namespace {

QString tr(const char *s) { return QCoreApplication::translate("Sync", s); }

QString nowIso() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODate); }

QJsonObject readJson(const QString &file)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

bool writeFile(const QString &file, const QByteArray &data, QString *error = nullptr)
{
    QDir().mkpath(QFileInfo(file).absolutePath());
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

QString defaultAppDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) dir = QDir::homePath() + QStringLiteral("/.viviameals");
    return dir;
}

const QString kSettingsFile = QStringLiteral("app/einstellungen.json");

} // namespace

// ---------------------------------------------------------------- Allgemeines

QJsonObject Manifest::toJson() const
{
    QJsonObject files;
    for (auto it = this->files.begin(); it != this->files.end(); ++it)
        files[it.key()] = QJsonObject{ { QStringLiteral("h"), it->hash },
                                       { QStringLiteral("s"), double(it->size) },
                                       { QStringLiteral("t"), it->time },
                                       { QStringLiteral("d"), it->device } };
    QJsonObject del;
    for (auto it = deleted.begin(); it != deleted.end(); ++it) del[it.key()] = it.value();
    return QJsonObject{ { QStringLiteral("format"), QStringLiteral("ViviaMeals-SyncManifest") },
                        { QStringLiteral("revision"), revision },
                        { QStringLiteral("files"), files },
                        { QStringLiteral("deleted"), del } };
}

Manifest Manifest::fromJson(const QJsonObject &o)
{
    Manifest m;
    m.revision = o.value(QStringLiteral("revision")).toInt();
    const QJsonObject files = o.value(QStringLiteral("files")).toObject();
    for (auto it = files.begin(); it != files.end(); ++it) {
        const QJsonObject f = it.value().toObject();
        if (!validPath(it.key())) continue;
        m.files.insert(it.key(), FileEntry{ f.value(QStringLiteral("h")).toString(), qint64(f.value(QStringLiteral("s")).toDouble()),
                                            f.value(QStringLiteral("t")).toString(), f.value(QStringLiteral("d")).toString() });
    }
    const QJsonObject del = o.value(QStringLiteral("deleted")).toObject();
    for (auto it = del.begin(); it != del.end(); ++it)
        if (validPath(it.key())) m.deleted.insert(it.key(), it.value().toObject());
    return m;
}

bool validPath(const QString &path)
{
    static const QRegularExpression re(QStringLiteral("^(db|app)/[A-Za-z0-9_.-]+(/[A-Za-z0-9_.-]+)*$"));
    return re.match(path).hasMatch() && !path.contains(QLatin1String("..")) && path.size() < 200;
}

QString sha256(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString deviceId()
{
    QSettings s;
    QString id = s.value(QStringLiteral("sync/deviceId")).toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        s.setValue(QStringLiteral("sync/deviceId"), id);
    }
    return id;
}

QString deviceName()
{
    const QString n = QSettings().value(QStringLiteral("sync/deviceName")).toString().trimmed();
    if (!n.isEmpty()) return n;
    const QString host = QSysInfo::machineHostName();
    return host.isEmpty() ? tr("Gerät") : host;
}

QString newToken()
{
    QByteArray raw(32, Qt::Uninitialized);
    for (int i = 0; i < raw.size(); ++i) raw[i] = char(QRandomGenerator::system()->bounded(256));
    return QString::fromLatin1(raw.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

// ---------------------------------------------------------------- Store

Store::Store(const QString &dir) : m_dir(dir) {}

QString Store::filePath(const QString &path) const { return QDir(m_dir).filePath(QStringLiteral("files/") + path); }

StoreInfo Store::info() const
{
    StoreInfo i;
    const QJsonObject o = readJson(QDir(m_dir).filePath(QStringLiteral("store.json")));
    i.initialized = !o.value(QStringLiteral("uuid")).toString().isEmpty();
    i.uuid = o.value(QStringLiteral("uuid")).toString();
    i.name = o.value(QStringLiteral("name")).toString();
    const Manifest m = manifest();
    i.revision = m.revision;
    i.files = int(m.files.size());
    return i;
}

bool Store::init(const QString &uuid, const QString &name, QString *error)
{
    if (info().initialized) return true;
    const QJsonObject o{ { QStringLiteral("format"), QStringLiteral("ViviaMeals-SyncStore") },
                         { QStringLiteral("version"), kProtocolVersion },
                         { QStringLiteral("uuid"), uuid },
                         { QStringLiteral("name"), name },
                         { QStringLiteral("created"), nowIso() } };
    return writeFile(QDir(m_dir).filePath(QStringLiteral("store.json")), QJsonDocument(o).toJson(), error);
}

Manifest Store::manifest() const { return Manifest::fromJson(readJson(QDir(m_dir).filePath(QStringLiteral("manifest.json")))); }

bool Store::get(const QString &path, QByteArray *data) const
{
    if (!validPath(path)) return false;
    QFile f(filePath(path));
    if (!f.open(QIODevice::ReadOnly)) return false;
    *data = f.readAll();
    return true;
}

bool Store::put(const QString &path, const QByteArray &data, const QString &hash, QString *error)
{
    if (!validPath(path)) {
        if (error) *error = tr("Ungültiger Pfad");
        return false;
    }
    if (!hash.isEmpty() && sha256(data) != hash) {
        if (error) *error = tr("Prüfsumme stimmt nicht – Übertragung unvollständig");
        return false;
    }
    return writeFile(filePath(path), data, error);
}

bool Store::remove(const QString &path)
{
    if (!validPath(path)) return false;
    QFile::remove(filePath(path));
    return true;
}

bool Store::lock(const QString &device, QString *holder)
{
    const QString file = QDir(m_dir).filePath(QStringLiteral("lock.json"));
    const QJsonObject o = readJson(file);
    const QDateTime until = QDateTime::fromString(o.value(QStringLiteral("until")).toString(), Qt::ISODate);
    const QString owner = o.value(QStringLiteral("device")).toString();
    if (!owner.isEmpty() && owner != device && until.isValid() && until > QDateTime::currentDateTimeUtc()) {
        if (holder) *holder = o.value(QStringLiteral("name")).toString(owner);
        return false;
    }
    const QStringList parts = device.split(QLatin1Char('|'));
    const QJsonObject n{ { QStringLiteral("device"), device },
                         { QStringLiteral("name"), parts.value(1, device) },
                         { QStringLiteral("until"), QDateTime::currentDateTimeUtc().addSecs(kLockSeconds).toString(Qt::ISODate) } };
    return writeFile(file, QJsonDocument(n).toJson());
}

void Store::unlock(const QString &device)
{
    const QString file = QDir(m_dir).filePath(QStringLiteral("lock.json"));
    if (readJson(file).value(QStringLiteral("device")).toString() == device) QFile::remove(file);
}

bool Store::commit(int expect, const QJsonObject &body, int *newRevision, bool *conflict, QString *error)
{
    if (conflict) *conflict = false;
    const Manifest cur = manifest();
    if (cur.revision != expect) {
        if (conflict) *conflict = true;
        if (error) *error = tr("Ein anderes Gerät hat inzwischen abgeglichen.");
        return false;
    }
    QJsonObject m = body;
    m[QStringLiteral("format")] = QStringLiteral("ViviaMeals-SyncManifest");
    m[QStringLiteral("revision")] = expect + 1;
    m[QStringLiteral("time")] = nowIso();
    if (!writeFile(QDir(m_dir).filePath(QStringLiteral("manifest.json")), QJsonDocument(m).toJson(QJsonDocument::Compact), error)) return false;
    if (newRevision) *newRevision = expect + 1;
    return true;
}

// ---------------------------------------------------------------- LocalTransport

bool LocalTransport::info(StoreInfo *out)
{
    *out = m_store.info();
    return true;
}

bool LocalTransport::init(const QString &uuid, const QString &name) { return m_store.init(uuid, name, &m_error); }

bool LocalTransport::manifest(Manifest *out)
{
    *out = m_store.manifest();
    return true;
}

bool LocalTransport::get(const QString &path, QByteArray *data)
{
    if (m_store.get(path, data)) return true;
    m_error = tr("Datei fehlt im Sync-Speicher: %1").arg(path);
    return false;
}

bool LocalTransport::put(const QString &path, const QByteArray &data) { return m_store.put(path, data, sha256(data), &m_error); }
bool LocalTransport::remove(const QString &path) { return m_store.remove(path); }
bool LocalTransport::lock(const QString &device, QString *holder) { return m_store.lock(device, holder); }
void LocalTransport::unlock(const QString &device) { m_store.unlock(device); }

bool LocalTransport::commit(int expect, const Manifest &m, const QString &device, int *newRevision, bool *conflict)
{
    QJsonObject body = m.toJson();
    body[QStringLiteral("device")] = device;
    return m_store.commit(expect, body, newRevision, conflict, &m_error);
}

// ---------------------------------------------------------------- HttpTransport

HttpTransport::HttpTransport(const QString &url, const QString &token) : m_url(url.trimmed()), m_token(token.trimmed()), m_nam(new QNetworkAccessManager) {}

HttpTransport::~HttpTransport() { delete m_nam; }

bool HttpTransport::request(const QString &method, const QString &action, const QMap<QString, QString> &params, const QByteArray &body, int *status,
                            QByteArray *replyData)
{
    QUrl u(m_url);
    QUrlQuery q(u);
    q.addQueryItem(QStringLiteral("a"), action);
    for (auto it = params.begin(); it != params.end(); ++it) q.addQueryItem(it.key(), it.value());
    u.setQuery(q);
    QNetworkRequest req(u);
    req.setRawHeader("X-ViviaMeals-Token", m_token.toLatin1());
    req.setRawHeader("User-Agent", QStringLiteral("ViviaMeals/%1").arg(QCoreApplication::applicationVersion()).toLatin1());
    req.setHeader(QNetworkRequest::ContentTypeHeader, action == QLatin1String("put") ? QStringLiteral("application/octet-stream")
                                                                                       : QStringLiteral("application/json"));
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setTransferTimeout(60000);
#endif
    QNetworkReply *r = method == QLatin1String("GET") ? m_nam->get(req) : m_nam->post(req, body);
    QEventLoop loop;
    QObject::connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    *status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    *replyData = r->readAll();
    const bool netOk = r->error() == QNetworkReply::NoError || *status > 0;
    if (!netOk) m_error = tr("Keine Verbindung: %1").arg(r->errorString());
    else if (*status >= 400) {
        const QString msg = QJsonDocument::fromJson(*replyData).object().value(QStringLiteral("error")).toString();
        if (*status == 401 || *status == 403) m_error = tr("Zugang verweigert – Zugangsschlüssel bzw. Kopplung prüfen.");
        else if (*status == 404 && action != QLatin1String("get")) m_error = tr("Sync-Adresse nicht gefunden (HTTP 404) – Adresse prüfen.");
        else m_error = msg.isEmpty() ? tr("Fehler vom Server: HTTP %1").arg(*status) : msg;
    }
    r->deleteLater();
    return netOk;
}

bool HttpTransport::info(StoreInfo *out)
{
    int st = 0;
    QByteArray rep;
    if (!request(QStringLiteral("GET"), QStringLiteral("info"), {}, {}, &st, &rep) || st != 200) {
        if (st == 200) m_error = tr("Unerwartete Antwort");
        return false;
    }
    const QJsonObject o = QJsonDocument::fromJson(rep).object();
    if (o.value(QStringLiteral("format")).toString() != QLatin1String("ViviaMeals-Sync")) {
        m_error = tr("Unter dieser Adresse antwortet kein ViviaMeals-Sync-Skript.");
        return false;
    }
    out->initialized = o.value(QStringLiteral("initialized")).toBool();
    out->uuid = o.value(QStringLiteral("uuid")).toString();
    out->name = o.value(QStringLiteral("name")).toString();
    out->revision = o.value(QStringLiteral("revision")).toInt();
    out->files = o.value(QStringLiteral("files")).toInt();
    return true;
}

bool HttpTransport::init(const QString &uuid, const QString &name)
{
    int st = 0;
    QByteArray rep;
    const QByteArray body = QJsonDocument(QJsonObject{ { QStringLiteral("uuid"), uuid }, { QStringLiteral("name"), name } }).toJson(QJsonDocument::Compact);
    return request(QStringLiteral("POST"), QStringLiteral("init"), {}, body, &st, &rep) && st == 200;
}

bool HttpTransport::manifest(Manifest *out)
{
    int st = 0;
    QByteArray rep;
    if (!request(QStringLiteral("GET"), QStringLiteral("manifest"), {}, {}, &st, &rep) || st != 200) return false;
    *out = Manifest::fromJson(QJsonDocument::fromJson(rep).object());
    return true;
}

bool HttpTransport::get(const QString &path, QByteArray *data)
{
    int st = 0;
    if (!request(QStringLiteral("GET"), QStringLiteral("get"), { { QStringLiteral("p"), path } }, {}, &st, data)) return false;
    if (st != 200) {
        m_error = tr("Datei fehlt im Sync-Speicher: %1").arg(path);
        return false;
    }
    return true;
}

bool HttpTransport::put(const QString &path, const QByteArray &data)
{
    int st = 0;
    QByteArray rep;
    return request(QStringLiteral("POST"), QStringLiteral("put"), { { QStringLiteral("p"), path }, { QStringLiteral("h"), sha256(data) } }, data, &st, &rep)
           && st == 200;
}

bool HttpTransport::remove(const QString &path)
{
    int st = 0;
    QByteArray rep;
    return request(QStringLiteral("POST"), QStringLiteral("del"), { { QStringLiteral("p"), path } }, "{}", &st, &rep) && st == 200;
}

bool HttpTransport::lock(const QString &device, QString *holder)
{
    int st = 0;
    QByteArray rep;
    const QByteArray body = QJsonDocument(QJsonObject{ { QStringLiteral("device"), device } }).toJson(QJsonDocument::Compact);
    if (!request(QStringLiteral("POST"), QStringLiteral("lock"), {}, body, &st, &rep)) return false;
    if (st == 423) {
        if (holder) *holder = QJsonDocument::fromJson(rep).object().value(QStringLiteral("holder")).toString();
        m_error.clear();
        return false;
    }
    return st == 200;
}

void HttpTransport::unlock(const QString &device)
{
    int st = 0;
    QByteArray rep;
    request(QStringLiteral("POST"), QStringLiteral("unlock"), {}, QJsonDocument(QJsonObject{ { QStringLiteral("device"), device } }).toJson(QJsonDocument::Compact),
            &st, &rep);
}

bool HttpTransport::commit(int expect, const Manifest &m, const QString &device, int *newRevision, bool *conflict)
{
    if (conflict) *conflict = false;
    QJsonObject body = m.toJson();
    body[QStringLiteral("expect")] = expect;
    body[QStringLiteral("device")] = device;
    int st = 0;
    QByteArray rep;
    if (!request(QStringLiteral("POST"), QStringLiteral("commit"), {}, QJsonDocument(body).toJson(QJsonDocument::Compact), &st, &rep)) return false;
    if (st == 409) {
        if (conflict) *conflict = true;
        return false;
    }
    if (st != 200) return false;
    if (newRevision) *newRevision = QJsonDocument::fromJson(rep).object().value(QStringLiteral("revision")).toInt();
    return true;
}

bool HttpTransport::pair(const QString &url, const QString &code, QString *token, QString *error)
{
    HttpTransport t(url, QString());
    int st = 0;
    QByteArray rep;
    const QByteArray body = QJsonDocument(QJsonObject{ { QStringLiteral("device"), deviceName() } }).toJson(QJsonDocument::Compact);
    if (!t.request(QStringLiteral("POST"), QStringLiteral("pair"), { { QStringLiteral("code"), code.trimmed() } }, body, &st, &rep) || st != 200) {
        if (error) *error = st == 403 ? tr("Der Kopplungscode stimmt nicht.") : t.lastError();
        return false;
    }
    *token = QJsonDocument::fromJson(rep).object().value(QStringLiteral("token")).toString();
    return !token->isEmpty();
}

// ---------------------------------------------------------------- Verbindung

Connection Connection::load(const QString &dbFolder)
{
    Connection c;
    const QJsonObject o = readJson(QDir(dbFolder).filePath(QStringLiteral(".sync/verbindung.json")));
    const QString k = o.value(QStringLiteral("kind")).toString();
    c.kind = k == QLatin1String("website") ? Kind::Website : k == QLatin1String("lan") ? Kind::Lan : k == QLatin1String("host") ? Kind::Host : Kind::None;
    c.url = o.value(QStringLiteral("url")).toString();
    c.token = o.value(QStringLiteral("token")).toString();
    c.deviceLabel = o.value(QStringLiteral("device")).toString();
    c.autoSync = o.value(QStringLiteral("auto")).toBool(true);
    return c;
}

bool Connection::save(const QString &dbFolder) const
{
    const char *k = kind == Kind::Website ? "website" : kind == Kind::Lan ? "lan" : kind == Kind::Host ? "host" : "none";
    const QJsonObject o{ { QStringLiteral("kind"), QLatin1String(k) },
                         { QStringLiteral("url"), url },
                         { QStringLiteral("token"), token },
                         { QStringLiteral("device"), deviceLabel },
                         { QStringLiteral("auto"), autoSync } };
    return writeFile(QDir(dbFolder).filePath(QStringLiteral(".sync/verbindung.json")), QJsonDocument(o).toJson());
}

QString Connection::invitation(const QString &name) const
{
    const QJsonObject o{ { QStringLiteral("u"), url }, { QStringLiteral("k"), token }, { QStringLiteral("n"), name } };
    return QStringLiteral("vmsync1:") + QString::fromLatin1(QJsonDocument(o).toJson(QJsonDocument::Compact).toBase64(QByteArray::Base64UrlEncoding));
}

Connection Connection::fromInvitation(const QString &code, QString *name)
{
    Connection c;
    QString t = code.trimmed();
    if (!t.startsWith(QLatin1String("vmsync1:"))) return c;
    const QJsonObject o = QJsonDocument::fromJson(QByteArray::fromBase64(t.mid(8).toLatin1(), QByteArray::Base64UrlEncoding)).object();
    c.url = o.value(QStringLiteral("u")).toString();
    c.token = o.value(QStringLiteral("k")).toString();
    if (name) *name = o.value(QStringLiteral("n")).toString();
    if (!c.url.isEmpty() && !c.token.isEmpty()) c.kind = c.url.contains(QLatin1String(".php")) ? Kind::Website : Kind::Lan;
    return c;
}

QString hostStoreDir(const QString &dbFolder)
{
    const QString key = sha256(QDir(dbFolder).absolutePath().toUtf8()).left(16);
    return QDir(defaultAppDir()).filePath(QStringLiteral("sync-freigabe/") + key);
}

Transport *makeTransport(const Connection &c, const QString &dbFolder)
{
    switch (c.kind) {
    case Connection::Kind::Website:
    case Connection::Kind::Lan: return c.valid() ? new HttpTransport(c.url, c.token) : nullptr;
    case Connection::Kind::Host: return new LocalTransport(hostStoreDir(dbFolder));
    case Connection::Kind::None: break;
    }
    return nullptr;
}

QString Result::summary() const
{
    if (!ok) return canceled ? tr("Abgleich abgebrochen.") : tr("Abgleich fehlgeschlagen: %1").arg(error);
    QStringList parts;
    if (downloaded) parts << tr("%1 geholt").arg(downloaded);
    if (uploaded) parts << tr("%1 gesendet").arg(uploaded);
    if (deletedLocal + deletedRemote) parts << tr("%1 gelöscht").arg(deletedLocal + deletedRemote);
    if (merged) parts << tr("%1 zusammengeführt").arg(merged);
    if (!conflicts.isEmpty()) parts << tr("%1 Konflikt(e) – beide Fassungen behalten").arg(conflicts.size());
    return parts.isEmpty() ? tr("Alles auf dem neuesten Stand.") : parts.join(QStringLiteral(", ")) + QLatin1Char('.');
}

// ---------------------------------------------------------------- Engine

Engine::Engine(Transport *t, const QString &dbFolder, const QString &appDir)
    : m_t(t), m_db(QDir(dbFolder).absolutePath()), m_app(appDir.isEmpty() ? defaultAppDir() : appDir)
{
}

QJsonObject Engine::loadState() const { return readJson(QDir(m_db).filePath(QStringLiteral(".sync/zustand.json"))); }

void Engine::saveState(const QJsonObject &s) const
{
    writeFile(QDir(m_db).filePath(QStringLiteral(".sync/zustand.json")), QJsonDocument(s).toJson(QJsonDocument::Compact));
}

QString Engine::localFile(const QString &path) const
{
    if (path == kSettingsFile) return {};
    if (path.startsWith(QLatin1String("db/"))) return QDir(m_db).filePath(path.mid(3));
    if (path.startsWith(QLatin1String("app/"))) return QDir(m_app).filePath(path.mid(4));
    return {};
}

bool Engine::readLocal(const QString &path, QByteArray *data) const
{
    if (path == kSettingsFile) {
        // gemeinsame Einstellungen als virtuelle Datei (sortiert, damit der Hash stabil bleibt)
        QSettings s;
        QStringList ignored = s.value(QStringLiteral("foods/ignored")).toStringList();
        ignored.sort();
        const QJsonObject o{ { QStringLiteral("format"), QStringLiteral("ViviaMeals-SharedSettings") },
                             { QStringLiteral("foodsIgnored"), QJsonArray::fromStringList(ignored) },
                             { QStringLiteral("defaultSource"), s.value(QStringLiteral("editor/defaultSource")).toString() } };
        *data = QJsonDocument(o).toJson(QJsonDocument::Compact);
        return true;
    }
    QFile f(localFile(path));
    if (!f.open(QIODevice::ReadOnly)) return false;
    *data = f.readAll();
    return true;
}

bool Engine::writeLocal(const QString &path, const QByteArray &data)
{
    if (path == kSettingsFile) {
        const QJsonObject o = QJsonDocument::fromJson(data).object();
        QSettings s;
        QStringList l;
        for (const auto &v : o.value(QStringLiteral("foodsIgnored")).toArray()) l << v.toString();
        s.setValue(QStringLiteral("foods/ignored"), l);
        s.setValue(QStringLiteral("editor/defaultSource"), o.value(QStringLiteral("defaultSource")).toString());
        return true;
    }
    const QString file = localFile(path);
    if (file.isEmpty() || !writeFile(file, data)) return false;
    const QFileInfo fi(file);
    m_cache[path] = QJsonObject{ { QStringLiteral("s"), double(fi.size()) },
                                 { QStringLiteral("m"), double(fi.lastModified().toMSecsSinceEpoch()) },
                                 { QStringLiteral("h"), sha256(data) } };
    return true;
}

bool Engine::removeLocal(const QString &path)
{
    m_cache.remove(path);
    const QString file = localFile(path);
    return file.isEmpty() || !QFileInfo::exists(file) || QFile::remove(file);
}

QMap<QString, QString> Engine::localHashes()
{
    QMap<QString, QString> out;
    QStringList paths;
    auto addIf = [&](const QString &path) {
        if (QFileInfo::exists(localFile(path))) paths << path;
    };
    addIf(QStringLiteral("db/viviameals.vmdb"));
    addIf(QStringLiteral("db/vitameals.vmdb"));   // ältere Datenbanken
    addIf(QStringLiteral("db/doubletten-ignoriert.json"));
    QDirIterator it(QDir(m_db).filePath(QStringLiteral("rezepte")), { QStringLiteral("*.vmr") }, QDir::Files);
    while (it.hasNext()) {
        const QString p = QStringLiteral("db/rezepte/") + QFileInfo(it.next()).fileName();
        if (validPath(p)) paths << p;
    }
    QDirIterator mt(QDir(m_db).filePath(QStringLiteral("menues")), { QStringLiteral("*.vmmenu") }, QDir::Files);   // gespeicherte Menüs
    while (mt.hasNext()) {
        const QString p = QStringLiteral("db/menues/") + QFileInfo(mt.next()).fileName();
        if (validPath(p)) paths << p;
    }
    addIf(QStringLiteral("app/profile.json"));
    addIf(QStringLiteral("app/lebensmittel_eigene.json"));
    QDirIterator dt(QDir(m_app).filePath(QStringLiteral("tagebuch")), { QStringLiteral("*.json") }, QDir::Files);
    while (dt.hasNext()) {
        const QString p = QStringLiteral("app/tagebuch/") + QFileInfo(dt.next()).fileName();
        if (validPath(p)) paths << p;
    }
    for (const QString &p : paths) {
        const QFileInfo fi(localFile(p));
        const QJsonObject c = m_cache.value(p).toObject();
        if (qint64(c.value(QStringLiteral("s")).toDouble()) == fi.size()
            && qint64(c.value(QStringLiteral("m")).toDouble()) == fi.lastModified().toMSecsSinceEpoch() && !c.value(QStringLiteral("h")).toString().isEmpty()) {
            out.insert(p, c.value(QStringLiteral("h")).toString());
            continue;
        }
        QByteArray data;
        if (!readLocal(p, &data)) continue;
        const QString h = sha256(data);
        m_cache[p] = QJsonObject{ { QStringLiteral("s"), double(fi.size()) },
                                  { QStringLiteral("m"), double(fi.lastModified().toMSecsSinceEpoch()) },
                                  { QStringLiteral("h"), h } };
        out.insert(p, h);
    }
    QByteArray settings;
    readLocal(kSettingsFile, &settings);
    out.insert(kSettingsFile, sha256(settings));
    return out;
}

namespace {

// Arrays von Objekten nach Schlüssel vereinigen; bei gleichem Schlüssel gewinnt die lokale Fassung
QJsonArray unionBy(const QJsonArray &local, const QJsonArray &remote, const QString &key)
{
    QJsonArray out = local;
    QSet<QString> have;
    for (const auto &v : local) have.insert(v.toObject().value(key).toVariant().toString());
    for (const auto &v : remote) {
        const QString k = v.toObject().value(key).toVariant().toString();
        if (k.isEmpty() || have.contains(k)) continue;
        have.insert(k);
        out.append(v);
    }
    return out;
}

QJsonArray unionStrings(const QJsonArray &a, const QJsonArray &b)
{
    QStringList l;
    for (const auto &v : a) l << v.toString();
    for (const auto &v : b)
        if (!l.contains(v.toString())) l << v.toString();
    l.sort();
    return QJsonArray::fromStringList(l);
}

} // namespace

QByteArray Engine::merge(const QString &path, const QByteArray &localData, const QByteArray &remoteData)
{
    const QJsonObject l = QJsonDocument::fromJson(localData).object(), r = QJsonDocument::fromJson(remoteData).object();
    if (l.isEmpty() || r.isEmpty()) return {};
    QJsonObject out = l;
    if (path == QLatin1String("app/profile.json")) {
        // Personen vereinigen; bei derselben Person Gewichtsverlauf vereinigen (gleicher Tag: lokal), Wochenplan lokal, sonst fremd
        QJsonArray merged;
        QMap<QString, QJsonObject> remoteById;
        for (const auto &v : r.value(QStringLiteral("profiles")).toArray()) remoteById.insert(v.toObject().value(QStringLiteral("id")).toString(), v.toObject());
        QSet<QString> seen;
        for (const auto &v : l.value(QStringLiteral("profiles")).toArray()) {
            QJsonObject p = v.toObject();
            const QString id = p.value(QStringLiteral("id")).toString();
            seen.insert(id);
            if (remoteById.contains(id)) {
                const QJsonObject rp = remoteById.value(id);
                QJsonArray w = unionBy(p.value(QStringLiteral("weights")).toArray(), rp.value(QStringLiteral("weights")).toArray(), QStringLiteral("date"));
                QJsonArray sorted = w;
                QList<QJsonObject> lw;
                for (const auto &x : w) lw << x.toObject();
                std::sort(lw.begin(), lw.end(), [](const QJsonObject &a, const QJsonObject &b) {
                    return a.value(QStringLiteral("date")).toString() < b.value(QStringLiteral("date")).toString();
                });
                sorted = QJsonArray();
                for (const auto &x : lw) sorted.append(x);
                p[QStringLiteral("weights")] = sorted;
                if (p.value(QStringLiteral("lastPlan")).toObject().isEmpty()) p[QStringLiteral("lastPlan")] = rp.value(QStringLiteral("lastPlan"));
            }
            merged.append(p);
        }
        for (auto it = remoteById.begin(); it != remoteById.end(); ++it)
            if (!seen.contains(it.key())) merged.append(it.value());
        out[QStringLiteral("profiles")] = merged;
    } else if (path.startsWith(QLatin1String("app/tagebuch/"))) {
        QJsonArray e = unionBy(l.value(QStringLiteral("entries")).toArray(), r.value(QStringLiteral("entries")).toArray(), QStringLiteral("id"));
        QList<QJsonObject> list;
        for (const auto &x : e) list << x.toObject();
        std::sort(list.begin(), list.end(), [](const QJsonObject &a, const QJsonObject &b) {
            return a.value(QStringLiteral("when")).toString() < b.value(QStringLiteral("when")).toString();
        });
        QJsonArray sorted;
        for (const auto &x : list) sorted.append(x);
        out[QStringLiteral("entries")] = sorted;
    } else if (path == QLatin1String("app/lebensmittel_eigene.json")) {
        out[QStringLiteral("foods")] = unionBy(l.value(QStringLiteral("foods")).toArray(), r.value(QStringLiteral("foods")).toArray(), QStringLiteral("key"));
        QJsonObject learned = r.value(QStringLiteral("learned")).toObject();
        const QJsonObject ll = l.value(QStringLiteral("learned")).toObject();
        for (auto it = ll.begin(); it != ll.end(); ++it) learned[it.key()] = it.value();
        out[QStringLiteral("learned")] = learned;
    } else if (path == kSettingsFile) {
        out[QStringLiteral("foodsIgnored")] = unionStrings(l.value(QStringLiteral("foodsIgnored")).toArray(), r.value(QStringLiteral("foodsIgnored")).toArray());
        if (out.value(QStringLiteral("defaultSource")).toString().isEmpty()) out[QStringLiteral("defaultSource")] = r.value(QStringLiteral("defaultSource"));
        return QJsonDocument(out).toJson(QJsonDocument::Compact);
    } else if (path == QLatin1String("db/doubletten-ignoriert.json")) {
        out[QStringLiteral("pairs")] = unionStrings(l.value(QStringLiteral("pairs")).toArray(), r.value(QStringLiteral("pairs")).toArray());
    } else {
        return {};
    }
    return QJsonDocument(out).toJson();
}

Result Engine::run(const Progress &progress)
{
    m_cache = loadState().value(QStringLiteral("cache")).toObject();
    Result res;
    for (int attempt = 0; attempt < 3; ++attempt) {
        bool retry = false;
        res = runOnce(progress, &retry);
        if (!retry) break;
    }
    return res;
}

Result Engine::runOnce(const Progress &progress, bool *retry)
{
    Result res;
    *retry = false;
    auto step = [&](int done, int total, const QString &text) { return !progress || progress(done, total, text); };
    if (!step(0, 100, tr("Verbindung zum Sync-Speicher …"))) {
        res.canceled = true;
        return res;
    }
    StoreInfo info;
    if (!m_t->info(&info)) {
        res.error = m_t->lastError();
        return res;
    }
    if (!info.initialized) {
        // neuer, leerer Speicher: mit dieser Datenbank anlegen
        const QString name = readJson(QDir(m_db).filePath(QStringLiteral("viviameals.vmdb"))).value(QStringLiteral("name")).toString();
        if (!m_t->init(QUuid::createUuid().toString(QUuid::WithoutBraces), name) || !m_t->info(&info) || !info.initialized) {
            res.error = m_t->lastError().isEmpty() ? tr("Der Sync-Speicher konnte nicht angelegt werden.") : m_t->lastError();
            return res;
        }
    }
    QJsonObject state = loadState();
    if (state.value(QStringLiteral("store")).toString() != info.uuid) state = QJsonObject{ { QStringLiteral("store"), info.uuid } };   // anderer Speicher: neu vergleichen
    const QString dev = deviceId() + QLatin1Char('|') + deviceName();
    QString holder;
    if (!m_t->lock(dev, &holder)) {
        res.error = holder.isEmpty() ? m_t->lastError() : tr("Gerade gleicht „%1“ ab – bitte in einer Minute erneut versuchen.").arg(holder);
        return res;
    }
    struct Unlock {
        Transport *t;
        QString dev;
        ~Unlock() { t->unlock(dev); }
    } unlocker{ m_t, dev };

    Manifest remote;
    if (!m_t->manifest(&remote)) {
        res.error = m_t->lastError();
        return res;
    }
    step(5, 100, tr("Lokale Dateien werden geprüft …"));
    const QMap<QString, QString> local = localHashes();
    const QJsonObject baseObj = state.value(QStringLiteral("base")).toObject();
    QMap<QString, QString> base;
    for (auto it = baseObj.begin(); it != baseObj.end(); ++it) base.insert(it.key(), it.value().toString());

    // ---- Plan
    enum Action { Download, Upload, DeleteLocal, DeleteRemote, Conflict };
    QList<QPair<QString, Action>> plan;
    QSet<QString> all;
    for (auto it = local.begin(); it != local.end(); ++it) all.insert(it.key());
    for (auto it = remote.files.begin(); it != remote.files.end(); ++it) all.insert(it.key());
    for (auto it = base.begin(); it != base.end(); ++it) all.insert(it.key());
    QStringList sortedAll(all.begin(), all.end());
    sortedAll.sort();
    for (const QString &p : sortedAll) {
        if (!validPath(p)) continue;
        const QString l = local.value(p), r = remote.files.value(p).hash, b = base.value(p);
        if (!l.isEmpty() && l == r) continue;
        if (!l.isEmpty() && r.isEmpty()) {
            if (!b.isEmpty() && l == b && (remote.deleted.contains(p) || remote.revision > 0)) plan << qMakePair(p, DeleteLocal);   // anderswo gelöscht
            else plan << qMakePair(p, Upload);
        } else if (l.isEmpty() && !r.isEmpty()) {
            if (!b.isEmpty() && r == b) plan << qMakePair(p, DeleteRemote);   // hier gelöscht
            else plan << qMakePair(p, Download);
        } else if (!l.isEmpty() && !r.isEmpty()) {
            if (l == b) plan << qMakePair(p, Download);
            else if (r == b) plan << qMakePair(p, Upload);
            else plan << qMakePair(p, Conflict);
        }
    }

    // ---- Ausführen
    Manifest next = remote;
    bool remoteChanged = false;
    const QString nowT = nowIso();
    auto uploaded = [&](const QString &p, const QByteArray &data) {
        next.files.insert(p, FileEntry{ sha256(data), data.size(), nowT, deviceName() });
        next.deleted.remove(p);
        remoteChanged = true;
        ++res.uploaded;
    };
    int done = 0;
    for (const auto &a : plan) {
        const QString &p = a.first;
        if (!step(10 + 85 * done++ / qMax(1, int(plan.size())), 100, tr("Abgleich: %1").arg(QFileInfo(p).fileName()))) {
            res.canceled = true;
            return res;
        }
        const bool isDb = p.startsWith(QLatin1String("db/"));
        switch (a.second) {
        case Download: {
            QByteArray data;
            if (!m_t->get(p, &data) || sha256(data) != remote.files.value(p).hash) {
                res.error = m_t->lastError().isEmpty() ? tr("Übertragung von %1 unvollständig.").arg(p) : m_t->lastError();
                return res;
            }
            if (!writeLocal(p, data)) {
                res.error = tr("%1 kann nicht gespeichert werden.").arg(p);
                return res;
            }
            ++res.downloaded;
            (isDb ? res.dbChanged : res.appChanged) = true;
            break;
        }
        case Upload: {
            QByteArray data;
            if (!readLocal(p, &data) || !m_t->put(p, data)) {
                res.error = m_t->lastError().isEmpty() ? tr("%1 kann nicht gesendet werden.").arg(p) : m_t->lastError();
                return res;
            }
            uploaded(p, data);
            break;
        }
        case DeleteLocal:
            removeLocal(p);
            ++res.deletedLocal;
            (isDb ? res.dbChanged : res.appChanged) = true;
            break;
        case DeleteRemote:
            m_t->remove(p);
            next.files.remove(p);
            next.deleted.insert(p, QJsonObject{ { QStringLiteral("t"), nowT }, { QStringLiteral("d"), deviceName() } });
            remoteChanged = true;
            ++res.deletedRemote;
            break;
        case Conflict: {
            QByteArray mine, theirs;
            if (!readLocal(p, &mine) || !m_t->get(p, &theirs)) {
                res.error = m_t->lastError();
                return res;
            }
            if (p.startsWith(QLatin1String("db/rezepte/"))) {
                // beide behalten: die fremde Fassung wird ein zweites Rezept
                QJsonObject o = QJsonDocument::fromJson(theirs).object();
                const QString newId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                const QString from = remote.files.value(p).device.isEmpty() ? tr("anderes Gerät") : remote.files.value(p).device;
                const QString title = tr("%1 (Konflikt: %2, %3)")
                                          .arg(o.value(QStringLiteral("title")).toString(), from,
                                               QLocale(QLocale::German).toString(QDate::currentDate(), QLocale::ShortFormat));
                o[QStringLiteral("id")] = newId;
                o[QStringLiteral("title")] = title;
                const QByteArray copy = QJsonDocument(o).toJson(QJsonDocument::Indented);
                const QString cp = QStringLiteral("db/rezepte/") + newId + QStringLiteral(".vmr");
                if (!writeLocal(cp, copy) || !m_t->put(cp, copy) || !m_t->put(p, mine)) {
                    res.error = m_t->lastError().isEmpty() ? tr("Konflikt bei %1 konnte nicht gelöst werden.").arg(p) : m_t->lastError();
                    return res;
                }
                uploaded(cp, copy);
                --res.uploaded;   // die Kopie zählt als Konflikt, nicht als Sendung
                uploaded(p, mine);
                res.conflicts << title;
                res.dbChanged = true;
            } else {
                QByteArray merged = merge(p, mine, theirs);
                if (merged.isEmpty()) merged = mine;   // nicht zusammenführbar (z. B. Datenbankname): diese Fassung gilt
                if (!writeLocal(p, merged) || !m_t->put(p, merged)) {
                    res.error = m_t->lastError().isEmpty() ? tr("%1 kann nicht zusammengeführt werden.").arg(p) : m_t->lastError();
                    return res;
                }
                uploaded(p, merged);
                --res.uploaded;
                ++res.merged;
                (isDb ? res.dbChanged : res.appChanged) = true;
            }
            break;
        }
        }
    }
    // alte Grabsteine aufräumen
    const QDateTime limit = QDateTime::currentDateTimeUtc().addDays(-kTombstoneDays);
    for (auto it = next.deleted.begin(); it != next.deleted.end();) {
        if (QDateTime::fromString(it->value(QStringLiteral("t")).toString(), Qt::ISODate) < limit) {
            it = next.deleted.erase(it);
            remoteChanged = true;
        } else {
            ++it;
        }
    }
    res.revision = remote.revision;
    if (remoteChanged) {
        step(97, 100, tr("Abgleich wird abgeschlossen …"));
        bool conflict = false;
        int rev = 0;
        if (!m_t->commit(remote.revision, next, dev, &rev, &conflict)) {
            if (conflict) {
                *retry = true;   // ein anderes Gerät war schneller: neu vergleichen
                res.error = tr("Ein anderes Gerät hat gleichzeitig abgeglichen.");
            } else {
                res.error = m_t->lastError();
            }
            return res;
        }
        res.revision = rev;
    }
    // Stand merken: nach dem Abgleich sind lokal und Speicher gleich
    QJsonObject nb;
    for (auto it = next.files.begin(); it != next.files.end(); ++it) nb[it.key()] = it->hash;
    state[QStringLiteral("store")] = info.uuid;
    state[QStringLiteral("base")] = nb;
    state[QStringLiteral("revision")] = res.revision;
    state[QStringLiteral("last")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    state[QStringLiteral("cache")] = m_cache;
    saveState(state);
    step(100, 100, tr("Fertig"));
    res.ok = true;
    return res;
}

// ---------------------------------------------------------------- PHP-Skript für die eigene Website

QString htaccessData()
{
    return QStringLiteral("# ViviaMeals-Sync: Datenordner von außen sperren\n"
                          "<IfModule mod_authz_core.c>\n  Require all denied\n</IfModule>\n"
                          "<IfModule !mod_authz_core.c>\n  Order allow,deny\n  Deny from all\n</IfModule>\n");
}

QString phpScript(const QString &tokenHash)
{
    QString php = QStringLiteral(R"PHP(<?php
// ViviaMeals-Sync – Abgleich einer ViviaMeals-Rezeptdatenbank über die eigene Website.
// Erzeugt von ViviaMeals. Benötigt PHP 7.4 oder neuer (getestet mit 8.x), HTTPS empfohlen.
// Hochladen: diese Datei in einen Ordner der Website; der Datenordner „vm-sync-daten“ wird daneben
// automatisch angelegt und per .htaccess gesperrt (bei nginx: Zugriff auf diesen Ordner selbst sperren).
// Zugangsschlüssel: nur als SHA-256 gespeichert – den Schlüssel selbst kennt nur ViviaMeals.
declare(strict_types=1);

const TOKEN_SHA256 = '@TOKEN@';
const DATA_DIR = __DIR__ . '/vm-sync-daten';
const LOCK_SECONDS = 300;
const MAX_FILE = 20 * 1024 * 1024;

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

function fail(int $code, string $msg): void {
    http_response_code($code);
    echo json_encode(['error' => $msg], JSON_UNESCAPED_UNICODE);
    exit;
}
function readJson(string $file): array {
    if (!is_file($file)) return [];
    $d = json_decode((string)file_get_contents($file), true);
    return is_array($d) ? $d : [];
}
function writeAtomic(string $file, string $data): void {
    $dir = dirname($file);
    if (!is_dir($dir) && !mkdir($dir, 0750, true) && !is_dir($dir)) fail(500, 'Ordner kann nicht angelegt werden');
    $tmp = $file . '.tmp' . bin2hex(random_bytes(4));
    if (file_put_contents($tmp, $data, LOCK_EX) === false) fail(500, 'Datei kann nicht geschrieben werden');
    if (!rename($tmp, $file)) { @unlink($tmp); fail(500, 'Datei kann nicht umbenannt werden'); }
}
function validPath(string $p): bool {
    return strlen($p) < 200 && strpos($p, '..') === false && preg_match('#^(db|app)/[A-Za-z0-9_.-]+(/[A-Za-z0-9_.-]+)*$#', $p) === 1;
}
function filePath(string $p): string { return DATA_DIR . '/files/' . $p; }
function body(): array {
    $d = json_decode((string)file_get_contents('php://input'), true);
    return is_array($d) ? $d : [];
}

// Datenordner anlegen und sperren
if (!is_dir(DATA_DIR)) { @mkdir(DATA_DIR, 0750, true); }
if (!is_file(DATA_DIR . '/.htaccess')) { @file_put_contents(DATA_DIR . '/.htaccess', "@HTACCESS@"); }
if (!is_file(DATA_DIR . '/index.html')) { @file_put_contents(DATA_DIR . '/index.html', ''); }

$token = $_SERVER['HTTP_X_VIVIAMEALS_TOKEN'] ?? '';
if (!is_string($token) || $token === '' || !hash_equals(TOKEN_SHA256, hash('sha256', $token))) fail(401, 'Zugang verweigert');

$a = $_GET['a'] ?? '';
$store = readJson(DATA_DIR . '/store.json');
$manifestFile = DATA_DIR . '/manifest.json';

switch ($a) {
case 'info':
    $m = readJson($manifestFile);
    echo json_encode(['format' => 'ViviaMeals-Sync', 'version' => 1, 'initialized' => isset($store['uuid']),
        'uuid' => $store['uuid'] ?? '', 'name' => $store['name'] ?? '', 'revision' => $m['revision'] ?? 0,
        'files' => isset($m['files']) ? count($m['files']) : 0, 'php' => PHP_VERSION], JSON_UNESCAPED_UNICODE);
    break;
case 'init':
    if (!isset($store['uuid'])) {
        $b = body();
        if (!preg_match('/^[0-9a-f-]{36}$/', (string)($b['uuid'] ?? ''))) fail(400, 'Ungültige Kennung');
        writeAtomic(DATA_DIR . '/store.json', json_encode(['format' => 'ViviaMeals-SyncStore', 'version' => 1,
            'uuid' => $b['uuid'], 'name' => (string)($b['name'] ?? ''), 'created' => gmdate('c')], JSON_UNESCAPED_UNICODE));
    }
    echo json_encode(['ok' => true]);
    break;
case 'manifest':
    $m = readJson($manifestFile);
    echo json_encode($m ?: ['revision' => 0, 'files' => new stdClass(), 'deleted' => new stdClass()], JSON_UNESCAPED_UNICODE);
    break;
case 'get':
    $p = (string)($_GET['p'] ?? '');
    if (!validPath($p) || !is_file(filePath($p))) fail(404, 'Nicht gefunden');
    header('Content-Type: application/octet-stream');
    readfile(filePath($p));
    break;
case 'put':
    $p = (string)($_GET['p'] ?? '');
    if (!validPath($p)) fail(400, 'Ungültiger Pfad');
    $data = (string)file_get_contents('php://input');
    if (strlen($data) > MAX_FILE) fail(413, 'Datei zu groß');
    if (!hash_equals((string)($_GET['h'] ?? ''), hash('sha256', $data))) fail(400, 'Prüfsumme stimmt nicht');
    writeAtomic(filePath($p), $data);
    echo json_encode(['ok' => true]);
    break;
case 'del':
    $p = (string)($_GET['p'] ?? '');
    if (!validPath($p)) fail(400, 'Ungültiger Pfad');
    if (is_file(filePath($p))) @unlink(filePath($p));
    echo json_encode(['ok' => true]);
    break;
case 'lock':
    $b = body();
    $dev = (string)($b['device'] ?? '');
    $l = readJson(DATA_DIR . '/lock.json');
    if (!empty($l['device']) && $l['device'] !== $dev && strtotime((string)($l['until'] ?? '')) > time()) {
        http_response_code(423);
        echo json_encode(['holder' => $l['name'] ?? $l['device']], JSON_UNESCAPED_UNICODE);
        break;
    }
    $parts = explode('|', $dev, 2);
    writeAtomic(DATA_DIR . '/lock.json', json_encode(['device' => $dev, 'name' => $parts[1] ?? $dev,
        'until' => gmdate('Y-m-d\TH:i:s\Z', time() + LOCK_SECONDS)], JSON_UNESCAPED_UNICODE));
    echo json_encode(['ok' => true]);
    break;
case 'unlock':
    $b = body();
    $l = readJson(DATA_DIR . '/lock.json');
    if (($l['device'] ?? '') === ($b['device'] ?? '')) @unlink(DATA_DIR . '/lock.json');
    echo json_encode(['ok' => true]);
    break;
case 'commit':
    $b = body();
    $m = readJson($manifestFile);
    if ((int)($m['revision'] ?? 0) !== (int)($b['expect'] ?? -1)) fail(409, 'Ein anderes Gerät hat inzwischen abgeglichen');
    $rev = (int)($m['revision'] ?? 0) + 1;
    writeAtomic($manifestFile, json_encode(['format' => 'ViviaMeals-SyncManifest', 'revision' => $rev, 'time' => gmdate('c'),
        'device' => (string)($b['device'] ?? ''), 'files' => $b['files'] ?? new stdClass(),
        'deleted' => $b['deleted'] ?? new stdClass()], JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES));
    echo json_encode(['revision' => $rev]);
    break;
default:
    fail(400, 'Unbekannte Aktion');
}
)PHP");
    QString ht = htaccessData();
    ht.replace(QLatin1String("\n"), QLatin1String("\\n"));
    php.replace(QLatin1String("@TOKEN@"), tokenHash);
    php.replace(QLatin1String("@HTACCESS@"), ht);
    return php;
}

} // namespace Sync
