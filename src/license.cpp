// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "license.h"

// Prüfschlüssel und Sperrliste schreibt der Schlüsselgenerator (tools/keygen) nach src/. Sie gehören dem
// Herausgeber und werden NICHT mit Quelltext-Paketen ausgeliefert – so überschreibt ein neuer Quelltextstand
// den eigenen Prüfschlüssel nicht. Fehlen sie, baut ViviaMeals ohne Prüfschlüssel (kein Schlüssel gültig).
#if __has_include("license_pubkey.h")
#include "license_pubkey.h"
#else
static const unsigned char kViviaMealsPublicKey[32] = { 0 };
#endif
#if __has_include("license_revoked.h")
#include "license_revoked.h"
#else
static const int kViviaMealsRevokedSerials[] = { 0 };
#endif

extern "C" {
#include "../3rdparty/monocypher/monocypher.h"
}

#include <QCoreApplication>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>

namespace License {

namespace {

const char kAlphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";   // Crockford: ohne I, L, O, U
const QDate kEpoch(2026, 1, 1);
constexpr int kPayload = 1 + 2 + 2;   // Version, Seriennummer, Tag
constexpr int kVersion = 1;

QString tr(const char *s)
{
    return QCoreApplication::translate("License", s);
}

QByteArray message(const QString &email, int serial, const QDate &issued)
{
    return QStringLiteral("ViviaMeals-1|%1|%2|%3").arg(normalizeEmail(email)).arg(serial).arg(issued.toString(Qt::ISODate)).toUtf8();
}

struct Cache {
    bool loaded = false;
    bool demo = false;
    QByteArray testKey;   // nur Tests/Bildschirmfotos (setTestPublicKey)
    Info info;
};

Cache &cache()
{
    static Cache c;
    return c;
}

QByteArray builtinPublicKey()
{
    if (!cache().testKey.isEmpty()) return cache().testKey;
    return QByteArray(reinterpret_cast<const char *>(kViviaMealsPublicKey), 32);
}

QSet<int> builtinRevoked()
{
    QSet<int> s;
    for (int n : kViviaMealsRevokedSerials)
        if (n > 0) s.insert(n);
    return s;
}

} // namespace

QString normalizeEmail(const QString &email)
{
    return email.trimmed().toLower();
}

QString encodeBase32(const QByteArray &data)
{
    QString out;
    quint32 buffer = 0;
    int bits = 0;
    for (const char c : data) {
        buffer = (buffer << 8) | quint8(c);
        bits += 8;
        while (bits >= 5) {
            out += QLatin1Char(kAlphabet[(buffer >> (bits - 5)) & 31]);
            bits -= 5;
        }
    }
    if (bits > 0) out += QLatin1Char(kAlphabet[(buffer << (5 - bits)) & 31]);
    return out;
}

QByteArray decodeBase32(const QString &text, bool *ok)
{
    QByteArray out;
    quint32 buffer = 0;
    int bits = 0;
    if (ok) *ok = true;
    for (QChar ch : text) {
        ch = ch.toUpper();
        if (ch == QLatin1Char('-') || ch.isSpace()) continue;
        if (ch == QLatin1Char('O')) ch = QLatin1Char('0');
        if (ch == QLatin1Char('I') || ch == QLatin1Char('L')) ch = QLatin1Char('1');
        const char *p = ch.unicode() < 128 ? strchr(kAlphabet, char(ch.unicode())) : nullptr;
        if (!p || !*p) {
            if (ok) *ok = false;
            return {};
        }
        buffer = (buffer << 5) | quint32(p - kAlphabet);
        bits += 5;
        if (bits >= 8) {
            out += char((buffer >> (bits - 8)) & 0xFF);
            bits -= 8;
        }
    }
    return out;
}

QString group(const QString &text, int size)
{
    QString out;
    for (int i = 0; i < text.size(); i += size) {
        if (!out.isEmpty()) out += QLatin1Char('-');
        out += text.mid(i, size);
    }
    return out;
}

QByteArray randomSeed()
{
    QByteArray seed(32, '\0');
    QRandomGenerator::system()->generate(reinterpret_cast<quint32 *>(seed.data()), reinterpret_cast<quint32 *>(seed.data() + seed.size()));
    return seed;
}

bool keyPair(const QByteArray &seed, QByteArray *secretKey, QByteArray *publicKey)
{
    if (seed.size() != 32 || !secretKey || !publicKey) return false;
    QByteArray s = seed;   // Monocypher löscht den Seed nach Gebrauch
    secretKey->resize(64);
    publicKey->resize(32);
    crypto_eddsa_key_pair(reinterpret_cast<uint8_t *>(secretKey->data()), reinterpret_cast<uint8_t *>(publicKey->data()),
                          reinterpret_cast<uint8_t *>(s.data()));
    return true;
}

QString makeKey(const QByteArray &secretKey, const QString &email, int serial, const QDate &issued)
{
    if (secretKey.size() != 64 || serial < 1 || serial > kMaxSerial || !issued.isValid()) return {};
    const int day = int(kEpoch.daysTo(issued));
    if (day < 0 || day > 0xFFFF) return {};
    const QByteArray msg = message(email, serial, issued);
    QByteArray sig(64, '\0');
    crypto_eddsa_sign(reinterpret_cast<uint8_t *>(sig.data()), reinterpret_cast<const uint8_t *>(secretKey.constData()),
                      reinterpret_cast<const uint8_t *>(msg.constData()), size_t(msg.size()));
    QByteArray raw;
    raw += char(kVersion);
    raw += char((serial >> 8) & 0xFF);
    raw += char(serial & 0xFF);
    raw += char((day >> 8) & 0xFF);
    raw += char(day & 0xFF);
    raw += sig;
    return QStringLiteral("VM1-") + group(encodeBase32(raw));
}

Info verify(const QString &email, const QString &key, const QByteArray &publicKey, const QSet<int> &revoked)
{
    Info info;
    info.email = normalizeEmail(email);
    if (!info.email.contains(QLatin1Char('@'))) {
        info.error = tr("Bitte die E-Mail-Adresse angeben, für die der Schlüssel ausgestellt wurde.");
        return info;
    }
    QString k = key.trimmed().toUpper();
    k.remove(QRegularExpression(QStringLiteral("\\s")));
    if (k.startsWith(QLatin1String("VM1-"))) k = k.mid(4);
    else if (k.startsWith(QLatin1String("VM1"))) k = k.mid(3);
    else {
        info.error = tr("Das ist kein ViviaMeals-Schlüssel (er beginnt mit „VM1-“).");
        return info;
    }
    bool ok = false;
    const QByteArray raw = decodeBase32(k, &ok);
    if (!ok || raw.size() < kPayload + 64) {
        info.error = ok ? tr("Der Schlüssel ist unvollständig – bitte vollständig einfügen.")
                        : tr("Der Schlüssel enthält ungültige Zeichen.");
        return info;
    }
    if (quint8(raw.at(0)) != kVersion) {
        info.error = tr("Dieser Schlüssel gehört zu einer anderen Programmversion.");
        return info;
    }
    info.serial = (quint8(raw.at(1)) << 8) | quint8(raw.at(2));
    info.issued = kEpoch.addDays((quint8(raw.at(3)) << 8) | quint8(raw.at(4)));
    const QByteArray sig = raw.mid(kPayload, 64);
    const QByteArray msg = message(info.email, info.serial, info.issued);
    if (publicKey.size() != 32 || publicKey == QByteArray(32, '\0')) {
        info.error = tr("In dieser ViviaMeals-Fassung ist noch kein Prüfschlüssel eingebaut.");
        return info;
    }
    if (crypto_eddsa_check(reinterpret_cast<const uint8_t *>(sig.constData()), reinterpret_cast<const uint8_t *>(publicKey.constData()),
                           reinterpret_cast<const uint8_t *>(msg.constData()), size_t(msg.size()))
        != 0) {
        info.error = tr("Schlüssel und E-Mail-Adresse passen nicht zusammen. Bitte beides genau so eingeben wie im Registrierungsschreiben.");
        return info;
    }
    if (info.serial < 1 || info.serial > kMaxSerial) {
        info.error = tr("Die Seriennummer dieses Schlüssels ist ungültig.");
        return info;
    }
    if (revoked.contains(info.serial)) {
        info.error = tr("Dieser Schlüssel wurde gesperrt.");
        return info;
    }
    info.valid = true;
    return info;
}

bool hasPublicKey()
{
    return builtinPublicKey() != QByteArray(32, '\0');
}

Info check(const QString &email, const QString &key)
{
    return verify(email, key, builtinPublicKey(), builtinRevoked());
}

Info current()
{
    Cache &c = cache();
    if (!c.loaded) {
        QSettings s;
        const QString email = s.value(QStringLiteral("license/email")).toString();
        const QString key = s.value(QStringLiteral("license/key")).toString();
        c.info = key.isEmpty() ? Info() : check(email, key);
        c.loaded = true;
    }
    if (c.demo) {
        Info d = c.info;
        if (!d.valid) {
            d.valid = true;
            d.email = QStringLiteral("demo");
        }
        return d;
    }
    return c.info;
}

bool isRegistered()
{
    return current().valid;
}

bool store(const QString &email, const QString &key)
{
    const Info info = check(email, key);
    if (!info.valid) return false;
    QSettings s;
    s.setValue(QStringLiteral("license/email"), info.email);
    s.setValue(QStringLiteral("license/key"), key.trimmed());
    cache().info = info;
    cache().loaded = true;
    return true;
}

void remove()
{
    QSettings s;
    s.remove(QStringLiteral("license"));
    cache().info = Info();
    cache().loaded = true;
}

void setDemo(bool on)
{
    cache().demo = on;
}

void setTestPublicKey(const QByteArray &publicKey)
{
    cache().testKey = publicKey.size() == 32 ? publicKey : QByteArray();
    cache().loaded = false;   // gespeicherte Registrierung neu prüfen
}

bool parsePasted(const QString &text, QString *email, QString *key)
{
    static const QRegularExpression mail(QStringLiteral("[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}"));
    static const QRegularExpression vm(QStringLiteral("VM1[-\\s]*([0-9A-Za-z][0-9A-Za-z\\s-]{100,})"), QRegularExpression::CaseInsensitiveOption);
    bool found = false;
    const auto m = mail.match(text);
    if (m.hasMatch() && email) {
        *email = m.captured(0);
        found = true;
    }
    const auto k = vm.match(text);
    if (k.hasMatch() && key) {
        QString body = k.captured(1);
        body.remove(QRegularExpression(QStringLiteral("[\\s-]")));
        *key = QStringLiteral("VM1-") + group(body.left(111).toUpper());
        found = true;
    }
    return found;
}

} // namespace License
