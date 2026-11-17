// ViviaMeals – Registrierung (Lizenzschlüssel)
//
// Verfahren: digitale Signatur (EdDSA mit Monocypher, 3rdparty/monocypher). Das Registrierungsprogramm
// (tools/keygen) besitzt den geheimen Schlüssel und unterschreibt
//     „ViviaMeals-1|<e-mail klein>|<seriennummer>|<ausstellungstag>“.
// ViviaMeals enthält nur den öffentlichen Schlüssel (license_pubkey.h) und prüft damit offline, ob ein
// Schlüssel zu der angegebenen E-Mail-Adresse passt. Neue Schlüssel brauchen kein neues Kompilieren.
//
// Schlüsselformat: „VM1-“ + Crockford-Base32 von 69 Bytes (Version 1 Byte, Seriennummer 2 Bytes,
// Ausstellungstag seit 1.1.2026 2 Bytes, Signatur 64 Bytes), in Fünfergruppen. Beim Lesen werden
// Leerzeichen, Bindestriche, Zeilenumbrüche und Kleinschreibung ignoriert, O/I/L wie 0/1 gelesen.
// Gesperrte Seriennummern stehen in license_revoked.h (vom Registrierungsprogramm erzeugt).
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QByteArray>
#include <QDate>
#include <QSet>
#include <QString>

namespace License {

constexpr int kMaxSerial = 10000;

struct Info {
    bool valid = false;
    QString email;      // normalisiert
    int serial = 0;
    QDate issued;
    QString error;      // bei !valid: Grund für den Benutzer
};

QString normalizeEmail(const QString &email);

// Schlüssel erzeugen (Registrierungsprogramm); secretKey = 64 Bytes (Monocypher)
QString makeKey(const QByteArray &secretKey, const QString &email, int serial, const QDate &issued);
// Schlüsselpaar aus 32 Zufallsbytes (seed); gibt secret (64) und public (32) zurück
bool keyPair(const QByteArray &seed, QByteArray *secretKey, QByteArray *publicKey);
QByteArray randomSeed();   // 32 Bytes aus dem Zufallsgenerator des Betriebssystems

// Prüfen mit einem gegebenen öffentlichen Schlüssel und einer Sperrliste
Info verify(const QString &email, const QString &key, const QByteArray &publicKey, const QSet<int> &revoked = {});

// --- in ViviaMeals: mit dem eingebauten öffentlichen Schlüssel
bool hasPublicKey();                          // false, solange license_pubkey.h nur den Platzhalter enthält
Info check(const QString &email, const QString &key);
Info current();                               // gespeicherte Registrierung (QSettings license/*), zwischengespeichert
bool isRegistered();
bool store(const QString &email, const QString &key);   // prüfen und speichern; false bei ungültigem Schlüssel
void remove();
// nur für Tests und Bildschirmfotos: anderen Prüfschlüssel verwenden (leer = eingebauter)
void setTestPublicKey(const QByteArray &publicKey);
void setDemo(bool on);                        // Demo-/Bildschirmfoto-Modus: alles freigeschaltet

// Text eines eingefügten Registrierungsschreibens auswerten („E-Mail: … / Schlüssel: VM1-…“)
bool parsePasted(const QString &text, QString *email, QString *key);

// Hilfen (auch für Tests)
QString encodeBase32(const QByteArray &data);
QByteArray decodeBase32(const QString &text, bool *ok = nullptr);
QString group(const QString &text, int size = 5);

} // namespace License
