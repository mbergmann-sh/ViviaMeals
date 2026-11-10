// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "escl.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QTimer>
#include <QXmlStreamReader>

#include <algorithm>
#include <cstdlib>

namespace Escl {

QUrl baseUrl(const QString &address, bool https)
{
    QString a = address.trimmed();
    if (a.isEmpty()) return {};
    if (!a.contains(QLatin1String("://"))) a = (https ? QStringLiteral("https://") : QStringLiteral("http://")) + a;
    QUrl u(a);
    if (!u.isValid() || u.host().isEmpty()) return {};
    if (https && u.scheme() == QLatin1String("http")) u.setScheme(QStringLiteral("https"));
    QString path = u.path();
    while (path.endsWith(QLatin1Char('/'))) path.chop(1);
    if (!path.endsWith(QLatin1String("/eSCL"), Qt::CaseInsensitive)) path += QStringLiteral("/eSCL");
    u.setPath(path);
    return u;
}

Capabilities parseCapabilities(const QByteArray &xml)
{
    Capabilities c;
    c.resolutions.clear();
    QXmlStreamReader r(xml);
    bool inPlaten = false, seenPlaten = false;
    QString section;   // Platen / Adf
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            const QString n = r.name().toString();
            if (n == QLatin1String("Platen")) {
                inPlaten = true;
                seenPlaten = true;
            } else if (n == QLatin1String("Adf")) {
                section = QStringLiteral("Adf");
            } else if (n == QLatin1String("MakeAndModel")) {
                c.makeAndModel = r.readElementText().trimmed();
            } else if (n == QLatin1String("Version") && c.version == QLatin1String("2.0")) {
                const QString v = r.readElementText().trimmed();
                if (!v.isEmpty()) c.version = v;
            } else if (inPlaten || !seenPlaten) {
                if (n == QLatin1String("MaxWidth")) c.maxWidth = r.readElementText().toInt();
                else if (n == QLatin1String("MaxHeight")) c.maxHeight = r.readElementText().toInt();
                else if (n == QLatin1String("ColorMode")) {
                    const QString m = r.readElementText().trimmed();
                    if (!c.colorModes.contains(m)) c.colorModes << m;
                } else if (n == QLatin1String("XResolution")) {
                    const int d = r.readElementText().toInt();
                    if (d > 0 && !c.resolutions.contains(d)) c.resolutions << d;
                } else if (n == QLatin1String("DocumentFormatExt") || n == QLatin1String("DocumentFormat")) {
                    const QString f = r.readElementText().trimmed();
                    if (!f.isEmpty() && !c.formats.contains(f)) c.formats << f;
                }
            }
        } else if (r.isEndElement() && r.name().toString() == QLatin1String("Platen")) {
            inPlaten = false;
        }
    }
    std::sort(c.resolutions.begin(), c.resolutions.end());
    if (c.maxWidth <= 0) c.maxWidth = 2550;
    if (c.maxHeight <= 0) c.maxHeight = 3508;
    c.platen = seenPlaten || section.isEmpty();
    return c;
}

int chooseResolution(const Capabilities &caps, int wanted)
{
    if (caps.resolutions.isEmpty()) return wanted;
    int best = caps.resolutions.first();
    for (int r : caps.resolutions)
        if (std::abs(r - wanted) < std::abs(best - wanted) || (std::abs(r - wanted) == std::abs(best - wanted) && r > best)) best = r;
    return best;
}

QString chooseFormat(const Capabilities &caps)
{
    for (const char *f : { "image/jpeg", "image/png" })
        if (caps.formats.isEmpty() || caps.formats.contains(QLatin1String(f))) return QLatin1String(f);
    return caps.formats.value(0, QStringLiteral("image/jpeg"));
}

QByteArray scanSettings(const Capabilities &caps, bool color, int dpi)
{
    QString mode = color ? QStringLiteral("RGB24") : QStringLiteral("Grayscale8");
    if (!caps.colorModes.isEmpty() && !caps.colorModes.contains(mode)) mode = caps.colorModes.first();
    const int res = chooseResolution(caps, dpi);
    const QString fmt = chooseFormat(caps);
    // A4 bzw. die ganze Scanfläche, falls kleiner (Einheit 1/300 Zoll)
    const int w = qMin(caps.maxWidth, 2480), h = qMin(caps.maxHeight, 3508);
    return QStringLiteral(
               "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<scan:ScanSettings xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\" "
               "xmlns:pwg=\"http://www.pwg.org/schemas/2010/12/sm\">\n"
               "  <pwg:Version>%1</pwg:Version>\n"
               "  <scan:Intent>%2</scan:Intent>\n"
               "  <pwg:ScanRegions>\n"
               "    <pwg:ScanRegion>\n"
               "      <pwg:ContentRegionUnits>escl:ThreeHundredthsOfInches</pwg:ContentRegionUnits>\n"
               "      <pwg:XOffset>0</pwg:XOffset>\n"
               "      <pwg:YOffset>0</pwg:YOffset>\n"
               "      <pwg:Width>%3</pwg:Width>\n"
               "      <pwg:Height>%4</pwg:Height>\n"
               "    </pwg:ScanRegion>\n"
               "  </pwg:ScanRegions>\n"
               "  <pwg:InputSource>Platen</pwg:InputSource>\n"
               "  <scan:ColorMode>%5</scan:ColorMode>\n"
               "  <scan:XResolution>%6</scan:XResolution>\n"
               "  <scan:YResolution>%6</scan:YResolution>\n"
               "  <pwg:DocumentFormat>%7</pwg:DocumentFormat>\n"
               "  <scan:DocumentFormatExt>%7</scan:DocumentFormatExt>\n"
               "</scan:ScanSettings>\n")
        .arg(caps.version, color ? QStringLiteral("Photo") : QStringLiteral("TextAndGraphic"))
        .arg(w)
        .arg(h)
        .arg(mode)
        .arg(res)
        .arg(fmt)
        .toUtf8();
}

// ---------------------------------------------------------------- Scanner

Scanner::Scanner(QObject *parent) : QObject(parent), m_nam(new QNetworkAccessManager(this)), m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        if (m_reply) m_reply->abort();
    });
#ifndef QT_NO_SSL
    // Drucker im eigenen Netz haben meist ein selbst signiertes Zertifikat – nur für die gewählte Adresse akzeptieren
    connect(m_nam, &QNetworkAccessManager::sslErrors, this, [this](QNetworkReply *r, const QList<QSslError> &) {
        if (r->url().host() == m_base.host()) r->ignoreSslErrors();
    });
#endif
}

Scanner::~Scanner()
{
    cancel();
}

QNetworkReply *Scanner::track(QNetworkReply *r)
{
    m_reply = r;
    m_timeout->start(30000);
    return r;
}

QString Scanner::describe(QNetworkReply *r) const
{
    const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (r->error() == QNetworkReply::OperationCanceledError) return tr("Keine Antwort vom Scanner (Zeitüberschreitung).");
    if (status > 0) return tr("Der Scanner antwortet mit HTTP %1 %2.").arg(status).arg(r->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
    return r->errorString();
}

void Scanner::fail(const QString &e)
{
    m_busy = false;
    m_timeout->stop();
    m_reply = nullptr;
    emit failed(e);
}

void Scanner::cancel()
{
    m_timeout->stop();
    if (m_reply) {
        QNetworkReply *r = m_reply;
        m_reply = nullptr;
        r->disconnect(this);
        r->abort();
        r->deleteLater();
    }
    // laufenden Auftrag beim Gerät löschen (sonst bleibt es eine Weile „beschäftigt“)
    if (m_busy && m_job.isValid()) m_nam->deleteResource(QNetworkRequest(m_job));
    m_busy = false;
}

void Scanner::check(const QString &address)
{
    cancel();
    m_address = address;
    m_base = baseUrl(address);
    m_triedHttps = false;
    if (!m_base.isValid()) {
        emit checked(false, QString(), tr("Bitte die IP-Adresse oder den Namen des Scanners eintragen."));
        return;
    }
    m_busy = true;
    getCapabilities(false);
}

void Scanner::scan(const QString &address, bool color, int dpi)
{
    cancel();
    m_address = address;
    m_base = baseUrl(address);
    m_triedHttps = false;
    m_color = color;
    m_dpi = dpi;
    m_job = QUrl();
    if (!m_base.isValid()) {
        emit failed(tr("Für den Netzwerkscanner ist keine Adresse eingetragen (Extras → Einstellungen → Scannen)."));
        return;
    }
    m_busy = true;
    emit progress(tr("Verbinde mit dem Scanner %1 …").arg(m_base.host()));
    getCapabilities(true);
}

void Scanner::getCapabilities(bool scanAfter)
{
    QUrl u = m_base;
    u.setPath(m_base.path() + QStringLiteral("/ScannerCapabilities"));
    QNetworkRequest req(u);
    QNetworkReply *r = track(m_nam->get(req));
    connect(r, &QNetworkReply::finished, this, [this, r, scanAfter] {
        r->deleteLater();
        if (r != m_reply) return;
        m_reply = nullptr;
        m_timeout->stop();
        const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (r->error() != QNetworkReply::NoError || status != 200) {
            // manche Geräte bieten eSCL nur über HTTPS (Port 443)
            if (!m_triedHttps && m_base.scheme() == QLatin1String("http") && !m_address.contains(QLatin1String("://"))) {
                m_triedHttps = true;
                m_base = baseUrl(m_address, true);
                getCapabilities(scanAfter);
                return;
            }
            const QString e = tr("Der Scanner unter %1 antwortet nicht auf eSCL: %2\n\nIst das Gerät eingeschaltet und im selben "
                                 "Netzwerk? Stimmt die Adresse? Manche Geräte müssen „AirPrint“/„Mopria“ bzw. „eSCL“ in ihrer "
                                 "Weboberfläche erst einschalten.")
                                  .arg(m_base.host(), describe(r));
            m_busy = false;
            if (scanAfter) emit failed(e);
            else emit checked(false, QString(), e);
            return;
        }
        m_caps = parseCapabilities(r->readAll());
        if (!scanAfter) {
            m_busy = false;
            emit checked(true, m_caps.makeAndModel.isEmpty() ? tr("eSCL-Scanner") : m_caps.makeAndModel, QString());
            return;
        }
        postJob();
    });
}

void Scanner::postJob()
{
    QUrl u = m_base;
    u.setPath(m_base.path() + QStringLiteral("/ScanJobs"));
    QNetworkRequest req(u);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("text/xml"));
    emit progress(tr("Scanne mit %1 (%2 dpi) …").arg(m_caps.makeAndModel.isEmpty() ? m_base.host() : m_caps.makeAndModel)
                      .arg(chooseResolution(m_caps, m_dpi)));
    QNetworkReply *r = track(m_nam->post(req, scanSettings(m_caps, m_color, m_dpi)));
    connect(r, &QNetworkReply::finished, this, [this, r] {
        r->deleteLater();
        if (r != m_reply) return;
        m_reply = nullptr;
        m_timeout->stop();
        const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 503) {
            fail(tr("Der Scanner ist gerade beschäftigt. Bitte gleich noch einmal versuchen."));
            return;
        }
        if (r->error() != QNetworkReply::NoError || (status != 201 && status != 200)) {
            fail(tr("Der Scanner hat den Auftrag abgelehnt: %1").arg(describe(r)));
            return;
        }
        QString loc = QString::fromUtf8(r->rawHeader("Location")).trimmed();
        if (loc.isEmpty()) {
            fail(tr("Der Scanner hat keinen Auftrag zurückgemeldet."));
            return;
        }
        m_job = m_base.resolved(QUrl(loc));
        // relative Angabe „/eSCL/ScanJobs/…“ bzw. absolute URL mit anderem Host (z. B. Gerätename) – Host beibehalten
        if (m_job.host() != m_base.host()) m_job.setHost(m_base.host());
        m_retries = 0;
        fetchDocument();
    });
}

void Scanner::fetchDocument()
{
    QUrl u = m_job;
    QString path = u.path();
    while (path.endsWith(QLatin1Char('/'))) path.chop(1);
    u.setPath(path + QStringLiteral("/NextDocument"));
    QNetworkReply *r = track(m_nam->get(QNetworkRequest(u)));
    m_timeout->start(120000);   // Scannen bei 300 dpi dauert je nach Gerät 10–60 s
    connect(r, &QNetworkReply::finished, this, [this, r] {
        r->deleteLater();
        if (r != m_reply) return;
        m_reply = nullptr;
        m_timeout->stop();
        const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 503 && m_retries < 60) {   // noch nicht fertig (Aufwärmen)
            ++m_retries;
            QTimer::singleShot(1000, this, [this] {
                if (m_busy) fetchDocument();
            });
            return;
        }
        if (r->error() != QNetworkReply::NoError || status != 200) {
            fail(tr("Das gescannte Bild konnte nicht abgeholt werden: %1").arg(describe(r)));
            return;
        }
        const QByteArray data = r->readAll();
        QImage img;
        img.loadFromData(data);
        if (img.isNull()) {
            fail(data.startsWith("%PDF") ? tr("Der Scanner hat eine PDF-Datei statt eines Bildes geliefert.")
                                         : tr("Das gescannte Bild kann nicht gelesen werden (%1 Bytes).").arg(data.size()));
            return;
        }
        m_busy = false;
        m_job = QUrl();
        emit finished(img);
    });
}

} // namespace Escl
