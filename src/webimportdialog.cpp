// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "webimportdialog.h"

#include "icons.h"
#include "importdialogs.h"
#include "textcodec.h"
#include "webrecipe.h"

#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QClipboard>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {

QByteArray userAgent()
{
    // ehrliche Kennung statt eines vorgetäuschten Browsers
    return QStringLiteral("ViviaMeals/%1 (Rezeptimport einzelner Seiten; https://github.com/mbergmann-sh/ViviaMeals)")
        .arg(QCoreApplication::applicationVersion().isEmpty() ? QStringLiteral("0") : QCoreApplication::applicationVersion())
        .toLatin1();
}

// Zeichensatz der Seite: HTTP-Kopf, sonst <meta charset>, sonst UTF-8 bzw. Erkennung
QString decodePage(const QByteArray &body, const QString &contentType)
{
    QString charset = contentType.section(QStringLiteral("charset="), 1).section(QLatin1Char(';'), 0, 0).trimmed().remove(QLatin1Char('"')).toLower();
    if (charset.isEmpty()) {
        static const QRegularExpression meta(QStringLiteral("<meta[^>]+charset\\s*=\\s*[\"']?([A-Za-z0-9_-]+)"), QRegularExpression::CaseInsensitiveOption);
        charset = meta.match(QString::fromLatin1(body.left(4096))).captured(1).toLower();
    }
    if (charset.isEmpty() || charset == QLatin1String("utf-8") || charset == QLatin1String("utf8")) {
        const QString s = QString::fromUtf8(body);
        if (!charset.isEmpty() || s.count(QChar(0xFFFD)) < 3) return s;
    }
    return TextCodec::decode(body);
}

QString imageToBase64(const QByteArray &data)
{
    QImage img;
    if (data.isEmpty() || !img.loadFromData(data)) return {};
    if (img.width() > 1000 || img.height() > 1000) img = img.scaled(1000, 1000, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);
    img.convertToFormat(QImage::Format_RGB32).save(&buf, "JPEG", 88);
    return out.isEmpty() ? QString() : QString::fromLatin1(out.toBase64());
}

Importers::Options importOptions()
{
    QSettings s;
    Importers::Options opt;
    opt.translate = s.value(QStringLiteral("import/translate"), true).toBool();
    opt.autoTags = s.value(QStringLiteral("import/autoTags"), true).toBool();
    opt.fixUmlauts = s.value(QStringLiteral("import/fixUmlauts"), true).toBool();
    opt.mealTags = s.value(QStringLiteral("import/mealTags"), true).toBool();
    opt.nutrientTags = s.value(QStringLiteral("import/nutrientTags"), true).toBool();
    opt.seasonTags = s.value(QStringLiteral("import/seasonTags"), true).toBool();
    opt.translator = ImportFlow::deeplTranslator();
    return opt;
}

} // namespace

WebImportDialog::WebImportDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Rezept von einer Webseite übernehmen"));
    setWindowIcon(Icons::get(QStringLiteral("web-import")));
    auto *lay = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Fügen Sie die Adresse einer Rezeptseite ein – für mehrere Rezepte eine Adresse je Zeile. "
                                "ViviaMeals liest das Rezept, das die Seite für Suchmaschinen bereitstellt (Titel, Zutaten, "
                                "Zubereitung, Foto), rechnet Maße um und berechnet die Nährwerte. Abgerufen werden nur die "
                                "angegebenen Seiten; die Adresse wird als Quelle beim Rezept vermerkt."));
    intro->setWordWrap(true);
    lay->addWidget(intro);
    m_urls = new QPlainTextEdit;
    m_urls->setPlaceholderText(QStringLiteral("https://www.beispiel.de/rezepte/linsen-dal/"));
    m_urls->setTabChangesFocus(true);
    m_urls->setMinimumHeight(80);
    lay->addWidget(m_urls);
    // Adressen aus der Zwischenablage vorschlagen
    const QStringList clip = WebRecipe::addressesIn(QApplication::clipboard()->text());
    if (!clip.isEmpty()) m_urls->setPlainText(clip.join(QLatin1Char('\n')));
    m_review = new QCheckBox(tr("Einzelnes Rezept vor dem Speichern im &Editor prüfen"));
    m_review->setChecked(QSettings().value(QStringLiteral("webimport/review"), true).toBool());
    m_review->setToolTip(tr("Bei mehreren Adressen werden die Rezepte direkt gespeichert (Doubletten werden übersprungen)."));
    lay->addWidget(m_review);
    m_log = new QListWidget;
    m_log->setMinimumHeight(110);
    m_log->setWordWrap(true);
    lay->addWidget(m_log, 1);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    lay->addWidget(m_status);
    auto *bl = new QHBoxLayout;
    m_file = new QPushButton(Icons::get(QStringLiteral("open")), tr("Gespeicherte &Seite öffnen …"));
    m_file->setToolTip(tr("Eine im Browser gespeicherte Rezeptseite (.html) auswerten – für Websites, die den Abruf durch Programme verweigern"));
    bl->addWidget(m_file);
    bl->addStretch(1);
    m_start = new QPushButton(Icons::get(QStringLiteral("import")), tr("&Übernehmen"));
    m_start->setDefault(true);
    bl->addWidget(m_start);
    m_done = new QPushButton(tr("Abbrechen"));
    bl->addWidget(m_done);
    lay->addLayout(bl);
    connect(m_start, &QPushButton::clicked, this, &WebImportDialog::start);
    connect(m_file, &QPushButton::clicked, this, &WebImportDialog::openFile);
    connect(m_done, &QPushButton::clicked, this, [this] {
        if (m_reply) {   // laufenden Abruf abbrechen, Bisheriges behalten
            m_queue.clear();
            m_reply->abort();
            return;
        }
        if (m_recipes.isEmpty()) reject();
        else accept();
    });
    m_nam = new QNetworkAccessManager(this);
    resize(640, 420);
}

WebImportDialog::~WebImportDialog()
{
    if (m_reply) m_reply->abort();
    QSettings().setValue(QStringLiteral("webimport/review"), m_review->isChecked());
}

bool WebImportDialog::review() const
{
    return m_review->isChecked() && m_recipes.size() == 1;
}

void WebImportDialog::setBusy(bool busy)
{
    m_start->setEnabled(!busy);
    m_file->setEnabled(!busy);
    m_urls->setReadOnly(busy);
    m_done->setText(busy ? tr("Abbrechen") : (m_recipes.isEmpty() ? tr("Schließen") : tr("Fertig")));
    if (busy) QApplication::setOverrideCursor(Qt::BusyCursor);
    else QApplication::restoreOverrideCursor();
}

void WebImportDialog::report(bool ok, const QString &text)
{
    auto *it = new QListWidgetItem(Icons::get(ok ? QStringLiteral("check") : QStringLiteral("warning")), text);
    m_log->addItem(it);
    m_log->scrollToBottom();
    if (!ok) ++m_failed;
}

QString WebImportDialog::summaryText() const
{
    const int n = int(m_recipes.size());
    QString t = n == 0 ? tr("Es wurde kein Rezept übernommen.") : n == 1 ? tr("1 Rezept ist bereit.") : tr("%1 Rezepte sind bereit.").arg(n);
    if (m_failed == 1) t += QLatin1Char(' ') + tr("Eine Seite ergab kein Rezept.");
    else if (m_failed > 1) t += QLatin1Char(' ') + tr("%1 Seiten ergaben kein Rezept.").arg(m_failed);
    if (n > 0) t += QLatin1Char(' ') + tr("„Fertig“ speichert sie in der Datenbank.");
    return t;
}

void WebImportDialog::start()
{
    const QStringList urls = WebRecipe::addressesIn(m_urls->toPlainText());
    if (urls.isEmpty()) {
        m_status->setText(tr("Bitte mindestens eine Adresse eingeben, die mit http:// oder https:// beginnt."));
        return;
    }
    m_queue.clear();
    for (const QString &u : urls) m_queue << QUrl(u);
    m_failed = 0;
    setBusy(true);
    next();
}

void WebImportDialog::next()
{
    if (m_queue.isEmpty()) {
        setBusy(false);
        m_status->setText(summaryText());
        // eine einzelne erfolgreiche Seite: gleich weiter (Editor bzw. Speichern)
        if (m_recipes.size() == 1 && m_failed == 0) accept();
        return;
    }
    m_current = m_queue.takeFirst();
    m_status->setText(tr("Lade %1 …").arg(m_current.toString()));
    fetchPage(m_current);
}

QNetworkReply *WebImportDialog::get(const QUrl &url, const QByteArray &accept)
{
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", userAgent());
    req.setRawHeader("Accept", accept);
    req.setRawHeader("Accept-Language", "de,en;q=0.7");
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setTransferTimeout(25000);
#endif
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#endif
    return m_nam->get(req);
}

void WebImportDialog::fetchPage(const QUrl &url)
{
    m_reply = get(url, "text/html,application/xhtml+xml;q=0.9,*/*;q=0.5");
    QNetworkReply *r = m_reply;
    connect(r, &QNetworkReply::finished, this, [this, r, url] {
        r->deleteLater();
        const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (r->error() == QNetworkReply::OperationCanceledError) {
            report(false, tr("%1: abgebrochen").arg(url.toString()));
            next();
            return;
        }
        if (status == 401 || status == 403 || status == 429 || status == 503) {
            report(false, tr("%1: Die Website verweigert den Abruf durch Programme (Fehler %2). Öffnen Sie die Seite im Browser, "
                             "speichern Sie sie (Strg+S, „Webseite, nur HTML“) und wählen Sie „Gespeicherte Seite öffnen …“.")
                              .arg(url.toString())
                              .arg(status));
            next();
            return;
        }
        if (status == 404 || status == 410) {
            report(false, tr("%1: Diese Seite gibt es nicht (Fehler %2) – ist die Adresse vollständig?").arg(url.toString()).arg(status));
            next();
            return;
        }
        if (r->error() != QNetworkReply::NoError) {
            const QString why = r->error() == QNetworkReply::HostNotFoundError ? tr("Die Website wurde nicht gefunden – Adresse oder Internetverbindung prüfen.")
                                : r->error() == QNetworkReply::TimeoutError || r->error() == QNetworkReply::OperationCanceledError
                                    ? tr("Die Website antwortet nicht.")
                                : status >= 400 ? tr("Die Website meldet Fehler %1.").arg(status)
                                                : r->errorString();
            report(false, tr("%1: %2").arg(url.toString(), why));
            next();
            return;
        }
        const QByteArray body = r->read(8 * 1024 * 1024);
        const QUrl finalUrl = r->url().isValid() ? r->url() : url;
        pageLoaded(finalUrl, body, r->header(QNetworkRequest::ContentTypeHeader).toString());
    });
}

void WebImportDialog::openFile()
{
    QSettings s;
    const QString dir = s.value(QStringLiteral("webimport/dir"), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).toString();
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Gespeicherte Rezeptseite öffnen"), dir, tr("Webseiten (*.html *.htm *.xhtml);;Alle Dateien (*)"));
    if (files.isEmpty()) return;
    s.setValue(QStringLiteral("webimport/dir"), QFileInfo(files.first()).absolutePath());
    m_failed = 0;
    m_fileMode = true;
    setBusy(true);
    // Dateien nacheinander: die Adresse der Seite steht meist als <link rel="canonical"> darin
    for (const QString &f : files) {
        QFile file(f);
        if (!file.open(QIODevice::ReadOnly)) {
            report(false, tr("%1: %2").arg(QFileInfo(f).fileName(), file.errorString()));
            continue;
        }
        const QByteArray body = file.readAll();
        const QString html = decodePage(body, QString());
        static const QRegularExpression canon(QStringLiteral("<link[^>]+rel\\s*=\\s*[\"']canonical[\"'][^>]*href\\s*=\\s*[\"']([^\"']+)|"
                                                             "<meta[^>]+property\\s*=\\s*[\"']og:url[\"'][^>]*content\\s*=\\s*[\"']([^\"']+)"),
                                              QRegularExpression::CaseInsensitiveOption);
        const auto m = canon.match(html);
        const QString found = m.captured(1).isEmpty() ? m.captured(2) : m.captured(1);
        const QUrl url = found.startsWith(QLatin1String("http")) ? QUrl(found) : QUrl::fromLocalFile(f);
        m_current = url;
        pageLoaded(url, body, QStringLiteral("text/html"));
        // auf das Foto warten (läuft asynchron), dann die nächste Datei
        while (m_reply) QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 100);
    }
    m_fileMode = false;
    setBusy(false);
    m_status->setText(summaryText());
    if (m_recipes.size() == 1 && m_failed == 0) accept();
}

void WebImportDialog::pageLoaded(const QUrl &url, const QByteArray &body, const QString &contentType)
{
    const QString html = decodePage(body, contentType);
    const WebRecipe::Parsed p = WebRecipe::parseHtml(html, url);
    const QString name = url.isLocalFile() ? QFileInfo(url.toLocalFile()).fileName() : url.toString();
    if (!p.ok) {
        report(false, QStringLiteral("%1: %2").arg(name, p.error));
        if (!m_fileMode) next();
        return;
    }
    m_status->setText(tr("Bereite „%1“ auf …").arg(p.parts.title));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_pending = WebRecipe::toRecipe(p, url, importOptions());
    QApplication::restoreOverrideCursor();
    m_pending.sourceFormat = QStringLiteral("Web");
    if (!p.imageUrl.isEmpty() && QUrl(p.imageUrl).scheme().startsWith(QLatin1String("http"))) {
        m_status->setText(tr("Lade das Foto zu „%1“ …").arg(m_pending.title));
        fetchImage(QUrl(p.imageUrl));
    } else {
        finishCurrent({});
    }
}

void WebImportDialog::fetchImage(const QUrl &image)
{
    m_reply = get(image, "image/jpeg,image/png,image/*;q=0.8");
    QNetworkReply *r = m_reply;
    connect(r, &QNetworkReply::finished, this, [this, r] {
        r->deleteLater();
        const QByteArray data = r->error() == QNetworkReply::NoError ? r->read(12 * 1024 * 1024) : QByteArray();
        m_reply = nullptr;
        finishCurrent(data);
    });
}

void WebImportDialog::finishCurrent(const QByteArray &imageData)
{
    m_reply = nullptr;
    const QString img = imageToBase64(imageData);
    if (!img.isEmpty()) m_pending.image = img;
    const int n = int(m_pending.ingredients.size());
    report(true, (n == 1 ? tr("„%1“ – 1 Zutat").arg(m_pending.title) : tr("„%1“ – %2 Zutaten").arg(m_pending.title).arg(n))
                     + (imageData.isEmpty() ? QString() : img.isEmpty() ? tr(", Foto nicht lesbar") : tr(", mit Foto"))
                     + (m_pending.originalLanguage.isEmpty() ? QString() : tr(", übersetzt aus „%1“").arg(m_pending.originalLanguage)));
    m_recipes << m_pending;
    m_pending = Recipe();
    if (!m_fileMode) next();
}
