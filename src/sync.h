// ViviaMeals – Datenbank auf mehreren Geräten abgleichen (Synchronisation)
//
// Jedes Gerät behält seine vollständige lokale Datenbank; abgeglichen wird mit einem „Sync-Speicher“ –
// entweder auf der eigenen Website (PHP-Skript viviameals-sync.php, von ViviaMeals erzeugt) oder auf
// einem Gerät im lokalen Netz (eingebauter Server, siehe syncserver.h). Beide sprechen dasselbe
// HTTP-Protokoll; der freigebende Rechner gleicht seine eigene Datenbank direkt (LocalTransport) ab.
//
// Abgeglichen werden (logische Pfade):
//   db/viviameals.vmdb, db/doubletten-ignoriert.json, db/rezepte/<id>.vmr          (Datenbankordner)
//   app/profile.json (Profile, Gewichtsverlauf, Wochenpläne), app/tagebuch/<id>.json,
//   app/lebensmittel_eigene.json, app/einstellungen.json (gemeinsame Einstellungen)     (AppData)
// Nicht: index.json, Sicherungen, Fenstergrößen, DeepL-Schlüssel, Scanner-Einstellungen.
//
// Verfahren: Manifest (Revision, je Datei SHA-256) + Drei-Wege-Vergleich gegen den Stand des letzten
// Abgleichs (<Datenbank>/.sync/zustand.json). Löschungen als „Grabsteine“. Konflikte (auf zwei Geräten
// geändert): Rezepte – beide behalten (die fremde Fassung als zweites Rezept „… (Konflikt: Gerät, Datum)“);
// Profile, Tagebuch, eigene Lebensmittel, Doubletten-Ausnahmen, Einstellungen – eintragsweise zusammengeführt.
// Gleichzeitigkeit: Sperre (5 min) + erwartete Revision beim Abschließen.
//
// HTTP-Protokoll (Kopf „X-ViviaMeals-Token“, Aktion im Parameter a):
//   GET  a=info | a=manifest | a=get&p=<pfad>
//   POST a=init (uuid, name) | a=put&p=<pfad>&h=<sha256> (Rohdaten) | a=del&p=<pfad>
//   POST a=lock | a=unlock (device) | a=commit (expect, files, deleted, device) | a=pair&code= (nur LAN)
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

#include <functional>

class QNetworkAccessManager;

namespace Sync {

constexpr int kProtocolVersion = 1;
constexpr int kLockSeconds = 300;
constexpr int kTombstoneDays = 365;

struct FileEntry {
    QString hash;     // SHA-256, hex
    qint64 size = 0;
    QString time;     // ISO-Zeit der Änderung
    QString device;   // Gerät, das die Fassung hochgeladen hat
};

struct Manifest {
    int revision = 0;
    QMap<QString, FileEntry> files;
    QMap<QString, QJsonObject> deleted;   // Pfad → {t, d}
    QJsonObject toJson() const;
    static Manifest fromJson(const QJsonObject &o);
};

struct StoreInfo {
    bool initialized = false;
    QString uuid, name;
    int revision = 0;
    int files = 0;
};

bool validPath(const QString &path);
QString sha256(const QByteArray &data);
QString deviceId();      // dauerhafte Kennung dieses Geräts
QString deviceName();    // Rechnername (änderbar über QSettings sync/deviceName)

// ---------------------------------------------------------------- Speicher (Dateien in einem Ordner)
// Für den eingebauten LAN-Server und den freigebenden Rechner selbst; das PHP-Skript macht dasselbe.
class Store
{
public:
    explicit Store(const QString &dir);
    QString dir() const { return m_dir; }
    StoreInfo info() const;
    bool init(const QString &uuid, const QString &name, QString *error = nullptr);
    Manifest manifest() const;
    bool get(const QString &path, QByteArray *data) const;
    bool put(const QString &path, const QByteArray &data, const QString &hash, QString *error = nullptr);
    bool remove(const QString &path);
    bool lock(const QString &device, QString *holder = nullptr);
    void unlock(const QString &device);
    // schreibt das Manifest mit Revision expect+1; false bei veralteter Revision (conflict = true)
    bool commit(int expect, const QJsonObject &body, int *newRevision, bool *conflict, QString *error = nullptr);

private:
    QString filePath(const QString &path) const;
    QString m_dir;
};

// ---------------------------------------------------------------- Transportwege
class Transport
{
public:
    virtual ~Transport() = default;
    virtual bool info(StoreInfo *out) = 0;
    virtual bool init(const QString &uuid, const QString &name) = 0;
    virtual bool manifest(Manifest *out) = 0;
    virtual bool get(const QString &path, QByteArray *data) = 0;
    virtual bool put(const QString &path, const QByteArray &data) = 0;
    virtual bool remove(const QString &path) = 0;
    virtual bool lock(const QString &device, QString *holder) = 0;
    virtual void unlock(const QString &device) = 0;
    virtual bool commit(int expect, const Manifest &m, const QString &device, int *newRevision, bool *conflict) = 0;
    QString lastError() const { return m_error; }

protected:
    QString m_error;
};

class LocalTransport : public Transport
{
public:
    explicit LocalTransport(const QString &storeDir) : m_store(storeDir) {}
    bool info(StoreInfo *out) override;
    bool init(const QString &uuid, const QString &name) override;
    bool manifest(Manifest *out) override;
    bool get(const QString &path, QByteArray *data) override;
    bool put(const QString &path, const QByteArray &data) override;
    bool remove(const QString &path) override;
    bool lock(const QString &device, QString *holder) override;
    void unlock(const QString &device) override;
    bool commit(int expect, const Manifest &m, const QString &device, int *newRevision, bool *conflict) override;

private:
    Store m_store;
};

class HttpTransport : public Transport
{
public:
    HttpTransport(const QString &url, const QString &token);
    ~HttpTransport() override;
    bool info(StoreInfo *out) override;
    bool init(const QString &uuid, const QString &name) override;
    bool manifest(Manifest *out) override;
    bool get(const QString &path, QByteArray *data) override;
    bool put(const QString &path, const QByteArray &data) override;
    bool remove(const QString &path) override;
    bool lock(const QString &device, QString *holder) override;
    void unlock(const QString &device) override;
    bool commit(int expect, const Manifest &m, const QString &device, int *newRevision, bool *conflict) override;
    // Kopplung mit einem Gerät im lokalen Netz: Kopplungscode → dauerhafter Zugangsschlüssel
    static bool pair(const QString &url, const QString &code, QString *token, QString *error);

private:
    // status: HTTP-Status; false bei Netzwerkfehler
    bool request(const QString &method, const QString &action, const QMap<QString, QString> &params, const QByteArray &body,
                 int *status, QByteArray *reply);
    QString m_url, m_token;
    QNetworkAccessManager *m_nam;
};

// ---------------------------------------------------------------- Verbindung einer Datenbank
// gespeichert in <Datenbank>/.sync/verbindung.json (bleibt auf diesem Gerät, wird nicht abgeglichen)
struct Connection {
    enum class Kind { None, Website, Lan, Host };   // Host = dieser Rechner gibt die Datenbank im LAN frei
    Kind kind = Kind::None;
    QString url;       // Website: …/viviameals-sync.php; LAN: http://<ip>:<port>/
    QString token;
    QString deviceLabel;   // LAN: Name des freigebenden Geräts
    bool autoSync = true;  // beim Öffnen und Schließen abgleichen
    bool valid() const { return kind == Kind::Host || (kind != Kind::None && !url.isEmpty() && !token.isEmpty()); }
    static Connection load(const QString &dbFolder);
    bool save(const QString &dbFolder) const;
    // Einladungscode: „vmsync1:“ + Base64(JSON {u, k, n})
    QString invitation(const QString &name = QString()) const;
    static Connection fromInvitation(const QString &code, QString *name = nullptr);
};
QString hostStoreDir(const QString &dbFolder);          // Sync-Speicher des freigebenden Rechners (AppData)
Transport *makeTransport(const Connection &c, const QString &dbFolder);   // nullptr, wenn keine Verbindung

// ---------------------------------------------------------------- Abgleich
struct Result {
    bool ok = false;
    bool canceled = false;
    QString error;
    int downloaded = 0, uploaded = 0, deletedLocal = 0, deletedRemote = 0, merged = 0;
    QStringList conflicts;   // Titel der Rezepte, die doppelt angelegt wurden
    bool dbChanged = false;  // Rezepte/Datenbankdateien lokal geändert → Datenbank neu einlesen
    bool appChanged = false; // Profile/Tagebuch/Lebensmittel/Einstellungen lokal geändert → neu laden
    int revision = 0;
    QString summary() const;
};

using Progress = std::function<bool(int done, int total, const QString &text)>;

class Engine
{
public:
    // appDir leer = AppData von ViviaMeals
    Engine(Transport *t, const QString &dbFolder, const QString &appDir = QString());
    Result run(const Progress &progress = {});

    // lokale Dateien: logischer Pfad → Inhalt bzw. Hash
    QMap<QString, QString> localHashes();
    QString localFile(const QString &path) const;   // leer für virtuelle Dateien
    bool readLocal(const QString &path, QByteArray *data) const;
    bool writeLocal(const QString &path, const QByteArray &data);
    bool removeLocal(const QString &path);
    // eintragsweise zusammenführen (Konflikt bei JSON-Dateien); leer = nicht zusammenführbar
    static QByteArray merge(const QString &path, const QByteArray &local, const QByteArray &remote);

private:
    Result runOnce(const Progress &progress, bool *retry);
    QJsonObject loadState() const;
    void saveState(const QJsonObject &s) const;
    Transport *m_t;
    QString m_db, m_app;
    QJsonObject m_cache;   // Hash-Zwischenspeicher: Pfad → {s, m, h}
};

// Inhalt des PHP-Skripts für die eigene Website (Zugangsschlüssel nur als SHA-256 enthalten)
QString phpScript(const QString &tokenHash);
QString htaccessData();   // für den Datenordner: Zugriff von außen sperren
QString newToken();       // zufälliger Zugangsschlüssel (43 Zeichen, Base64url)

} // namespace Sync
