// SPDX-License-Identifier: MIT AND LicenseRef-Commons-Clause-1.0 (siehe LICENSE)
#include "deepl.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUrl>

namespace DeepL {

namespace {

QString tr(const char *s) { return QCoreApplication::translate("DeepL", s); }

QString cleanKey(const QString &key) { return key.trimmed(); }

// eine Anfrage ausführen; gibt den Antwortinhalt zurück oder setzt error
bool request(const QString &key, const QString &path, const QByteArray *body, QByteArray *answer, QString *error, int timeoutMs)
{
    if (cleanKey(key).isEmpty()) {
        if (error) *error = tr("Kein DeepL-API-Schlüssel eingetragen.");
        return false;
    }
    QNetworkAccessManager nam;
    QNetworkRequest req(QUrl(baseUrl(key) + path));
    req.setRawHeader("Authorization", "DeepL-Auth-Key " + cleanKey(key).toUtf8());
    req.setRawHeader("User-Agent", "VitaMeals/" + QCoreApplication::applicationVersion().toUtf8());
    QNetworkReply *reply = nullptr;
    if (body) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        reply = nam.post(req, *body);
    } else {
        reply = nam.get(req);
    }
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    bool timedOut = false;
    QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
        timedOut = true;
        reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();
    const QNetworkReply::NetworkError netErr = reply->error();
    const QString netText = reply->errorString();
    reply->deleteLater();
    if (timedOut) {
        if (error) *error = tr("Zeitüberschreitung – DeepL hat nicht geantwortet.");
        return false;
    }
    if (status == 200) {
        *answer = data;
        return true;
    }
    if (error) {
        QString msg = QJsonDocument::fromJson(data).object().value(QStringLiteral("message")).toString();
        switch (status) {
        case 403: *error = tr("Der API-Schlüssel wurde abgelehnt (bitte prüfen, ob er vollständig ist)."); break;
        case 456: *error = tr("Das monatliche Zeichenkontingent ist aufgebraucht."); break;
        case 429: *error = tr("Zu viele Anfragen – bitte später erneut versuchen."); break;
        case 413: *error = tr("Die Anfrage ist zu groß."); break;
        case 0: *error = netErr != QNetworkReply::NoError ? tr("Keine Verbindung: %1").arg(netText) : tr("Keine Verbindung."); break;
        default: *error = tr("DeepL meldet Fehler %1%2").arg(status).arg(msg.isEmpty() ? QString() : QStringLiteral(": ") + msg);
        }
    }
    return false;
}

} // namespace

bool isFreeKey(const QString &key) { return cleanKey(key).endsWith(QLatin1String(":fx")); }

QString baseUrl(const QString &key)
{
    const QByteArray env = qgetenv("VITAMEALS_DEEPL_URL");
    if (!env.isEmpty()) return QString::fromUtf8(env);
    return isFreeKey(key) ? QStringLiteral("https://api-free.deepl.com") : QStringLiteral("https://api.deepl.com");
}

bool usage(const QString &key, Usage *out, QString *error)
{
    QByteArray data;
    if (!request(key, QStringLiteral("/v2/usage"), nullptr, &data, error, 15000)) return false;
    const QJsonObject o = QJsonDocument::fromJson(data).object();
    if (out) {
        out->characterCount = qint64(o.value(QStringLiteral("character_count")).toDouble());
        out->characterLimit = qint64(o.value(QStringLiteral("character_limit")).toDouble());
    }
    return true;
}

bool translate(const QString &key, QStringList &texts, const QString &sourceLang, QString *detected, QString *error, int timeoutMs)
{
    // DeepL: höchstens 50 Texte und 128 KiB je Anfrage – in Pakete aufteilen
    int start = 0;
    while (start < texts.size()) {
        int end = start, bytes = 0;
        while (end < texts.size() && end - start < 50) {
            const int b = int(texts[end].toUtf8().size()) + 16;
            if (end > start && bytes + b > 100 * 1024) break;
            bytes += b;
            ++end;
        }
        QJsonArray arr;
        for (int i = start; i < end; ++i) arr.append(texts[i]);
        QJsonObject body{ { QStringLiteral("text"), arr },
                          { QStringLiteral("target_lang"), QStringLiteral("DE") },
                          { QStringLiteral("preserve_formatting"), true } };
        if (!sourceLang.isEmpty()) body[QStringLiteral("source_lang")] = sourceLang.toUpper();
        const QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
        QByteArray data;
        if (!request(key, QStringLiteral("/v2/translate"), &json, &data, error, timeoutMs)) return false;
        const QJsonArray tr_ = QJsonDocument::fromJson(data).object().value(QStringLiteral("translations")).toArray();
        if (tr_.size() != end - start) {
            if (error) *error = tr("Unerwartete Antwort von DeepL.");
            return false;
        }
        for (int i = 0; i < tr_.size(); ++i) {
            const QJsonObject t = tr_[i].toObject();
            texts[start + i] = t.value(QStringLiteral("text")).toString();
            if (detected && detected->isEmpty()) *detected = t.value(QStringLiteral("detected_source_language")).toString().toLower();
        }
        start = end;
    }
    return true;
}

namespace {

// alle zu übersetzenden Texte eines Rezepts sammeln (doppelte nur einmal)
struct Collector {
    QStringList texts;
    QHash<QString, int> index;
    int add(const QString &s)
    {
        const QString t = s.trimmed();
        if (t.isEmpty()) return -1;
        auto it = index.constFind(t);
        if (it != index.constEnd()) return *it;
        texts << t;
        index.insert(t, int(texts.size()) - 1);
        return int(texts.size()) - 1;
    }
};

} // namespace

int characterCount(const Recipe &r)
{
    Collector c;
    c.add(r.title);
    c.add(r.instructions);
    for (const Ingredient &i : r.ingredients) {
        c.add(i.name);
        c.add(i.note);
    }
    int n = 0;
    for (const QString &t : c.texts) n += int(t.size());
    return n;
}

bool translateRecipe(Recipe &r, const QString &key, QString *error)
{
    Collector c;
    const int title = c.add(r.title);
    // Anleitung absatzweise (die Absätze bleiben so erhalten)
    const QStringList paragraphs = r.instructions.split(QStringLiteral("\n\n"));
    QList<int> para;
    for (const QString &p : paragraphs) para << c.add(p);
    QList<QPair<int, int>> ing;
    for (const Ingredient &i : r.ingredients) ing.append({ c.add(i.name), c.add(i.note) });
    if (c.texts.isEmpty()) return true;
    QStringList texts = c.texts;
    QString detected;
    const QString src = r.originalLanguage.isEmpty() && r.language != QLatin1String("de") ? r.language : r.originalLanguage;
    // Quellsprache nur vorgeben, wenn die eigene Erkennung sicher ist (en); sonst DeepL erkennen lassen
    if (!translate(key, texts, src == QLatin1String("en") ? src : QString(), &detected, error)) return false;
    auto get = [&](int k, const QString &orig) { return k < 0 ? orig : texts.value(k, orig); };
    r.title = get(title, r.title);
    QStringList out;
    for (int k = 0; k < paragraphs.size(); ++k) out << get(para[k], paragraphs[k]);
    r.instructions = out.join(QStringLiteral("\n\n")).trimmed();
    for (int k = 0; k < r.ingredients.size(); ++k) {
        Ingredient &i = r.ingredients[k];
        i.name = get(ing[k].first, i.name);
        i.note = get(ing[k].second, i.note);
        // Zutatennamen ohne Satzpunkt („Mehl.“ → „Mehl“)
        while (i.name.endsWith(QLatin1Char('.'))) i.name.chop(1);
    }
    r.originalLanguage = !detected.isEmpty() ? detected : (src.isEmpty() ? QStringLiteral("en") : src);
    r.language = QStringLiteral("de");
    r.translatedBy = QStringLiteral("DeepL");
    r.tags.removeAll(QStringLiteral("englisch"));
    return true;
}

Settings Settings::load()
{
    QSettings s;
    Settings d;
    d.enabled = s.value(QStringLiteral("deepl/enabled"), false).toBool();
    d.key = s.value(QStringLiteral("deepl/key")).toString();
    return d;
}

void Settings::save() const
{
    QSettings s;
    s.setValue(QStringLiteral("deepl/enabled"), enabled);
    s.setValue(QStringLiteral("deepl/key"), key.trimmed());
}

} // namespace DeepL
