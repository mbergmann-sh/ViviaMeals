// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "syncdialog.h"
#include "busyprogress.h"
#include "icons.h"
#include "recipedb.h"
#include "syncserver.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QSysInfo>
#include <QVBoxLayout>

#include <memory>

namespace {
enum Page { PageNone, PageWebsite, PageLan, PageHost };

QLabel *note(const QString &text)
{
    auto *l = new QLabel(text);
    l->setWordWrap(true);
    l->setTextFormat(Qt::RichText);
    l->setOpenExternalLinks(true);
    return l;
}
} // namespace

// ---------------------------------------------------------------- Weg zum Sync-Speicher

SyncWayWidget::SyncWayWidget(bool allowHost, QWidget *parent) : QWidget(parent), m_allowHost(allowHost)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_kind = new QButtonGroup(this);
    auto *kinds = new QGroupBox(tr("Abgleich über"));
    auto *kl = new QVBoxLayout(kinds);
    auto addKind = [&](int id, const QString &icon, const QString &text) {
        auto *r = new QRadioButton(text);
        r->setIcon(Icons::get(icon));
        m_kind->addButton(r, id);
        kl->addWidget(r);
    };
    if (allowHost) addKind(PageNone, QStringLiteral("close"), tr("&Kein Abgleich – Datenbank nur auf diesem Gerät"));
    addKind(PageWebsite, QStringLiteral("web"), tr("die eigene &Website (PHP-Skript im eigenen Webspace)"));
    addKind(PageLan, QStringLiteral("network"), tr("ein anderes &Gerät im lokalen Netz, das die Datenbank freigibt"));
    if (allowHost) addKind(PageHost, QStringLiteral("database"), tr("&Freigabe: dieser Rechner stellt die Datenbank im lokalen Netz bereit"));
    lay->addWidget(kinds);

    m_pages = new QStackedWidget;
    m_pages->addWidget(note(tr("Die Datenbank wird nicht abgeglichen.")));

    // Website
    auto *web = new QWidget;
    auto *wl = new QVBoxLayout(web);
    wl->setContentsMargins(0, 0, 0, 0);
    wl->addWidget(note(tr("ViviaMeals erzeugt ein kleines PHP-Skript (ab PHP 7.4, z. B. 8.3). Laden Sie es in einen Ordner Ihrer Website hoch – "
                          "es legt daneben einen gesperrten Datenordner an. Alle Geräte verwenden dieselbe Adresse und denselben "
                          "Zugangsschlüssel. Bitte möglichst <b>https://</b> verwenden.")));
    auto *wf = new QFormLayout;
    m_url = new QLineEdit;
    m_url->setPlaceholderText(QStringLiteral("https://www.meine-seite.de/viviameals/viviameals-sync.php"));
    wf->addRow(tr("Adresse des Skripts:"), m_url);
    m_key = new QLineEdit;
    m_key->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    m_key->setPlaceholderText(tr("Zugangsschlüssel"));
    auto *keyRow = new QHBoxLayout;
    keyRow->addWidget(m_key, 1);
    auto *gen = new QPushButton(Icons::get(QStringLiteral("refresh")), tr("Neu"));
    gen->setToolTip(tr("Neuen zufälligen Zugangsschlüssel erzeugen (das Skript muss danach neu hochgeladen werden)"));
    keyRow->addWidget(gen);
    wf->addRow(tr("Zugangsschlüssel:"), keyRow);
    wl->addLayout(wf);
    auto *wb = new QHBoxLayout;
    auto *script = new QPushButton(Icons::get(QStringLiteral("save")), tr("Sync-Skript speichern …"));
    auto *copyInv = new QPushButton(Icons::get(QStringLiteral("copy")), tr("Einladungscode kopieren"));
    copyInv->setToolTip(tr("Adresse und Schlüssel als einen Code in die Zwischenablage – auf dem anderen Gerät einfügen"));
    auto *pasteInv = new QPushButton(Icons::get(QStringLiteral("link")), tr("Einladungscode einfügen …"));
    wb->addWidget(script);
    wb->addWidget(copyInv);
    wb->addWidget(pasteInv);
    wb->addStretch();
    wl->addLayout(wb);
    wl->addStretch();
    m_pages->addWidget(web);
    connect(gen, &QPushButton::clicked, this, [this] {
        m_key->setText(Sync::newToken());
        m_key->setEchoMode(QLineEdit::Normal);
    });
    connect(script, &QPushButton::clicked, this, &SyncWayWidget::saveScript);
    connect(copyInv, &QPushButton::clicked, this, [this] {
        Sync::Connection c = connection();
        if (c.kind != Sync::Connection::Kind::Website) {
            QMessageBox::information(this, tr("Einladungscode"), tr("Bitte zuerst Adresse und Zugangsschlüssel eintragen."));
            return;
        }
        QApplication::clipboard()->setText(c.invitation(m_dbName));
        QMessageBox::information(this, tr("Einladungscode"),
                                 tr("Der Einladungscode liegt in der Zwischenablage. Fügen Sie ihn auf dem anderen Gerät unter "
                                    "<b>Datei → Abgleich → Datenbank aus dem Abgleich laden</b> ein.<br><br>"
                                    "Der Code enthält den Zugangsschlüssel – bitte nur an eigene Geräte weitergeben."));
    });
    connect(pasteInv, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const QString code = QInputDialog::getText(this, tr("Einladungscode einfügen"), tr("Einladungscode (beginnt mit vmsync1:):"), QLineEdit::Normal,
                                                   QApplication::clipboard()->text().trimmed().startsWith(QLatin1String("vmsync1:"))
                                                       ? QApplication::clipboard()->text().trimmed()
                                                       : QString(),
                                                   &ok);
        if (!ok) return;
        const Sync::Connection c = Sync::Connection::fromInvitation(code);
        if (!c.valid()) {
            QMessageBox::warning(this, tr("Einladungscode"), tr("Das ist kein gültiger Einladungscode."));
            return;
        }
        setConnection(c);
    });

    // Gerät im LAN
    auto *lan = new QWidget;
    auto *ll = new QVBoxLayout(lan);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->addWidget(note(tr("Auf dem Rechner, der die Datenbank bereitstellt, unter <b>Datei → Abgleich → Abgleich einrichten</b> "
                          "„Freigabe“ wählen. Dort wird ein sechsstelliger Kopplungscode angezeigt.")));
    auto *fr = new QHBoxLayout;
    m_found = new QListWidget;
    m_found->setMaximumHeight(110);
    fr->addWidget(m_found, 1);
    auto *search = new QPushButton(Icons::get(QStringLiteral("search")), tr("Suchen"));
    fr->addWidget(search, 0, Qt::AlignTop);
    ll->addLayout(fr);
    auto *lf = new QFormLayout;
    m_lanUrl = new QLineEdit;
    m_lanUrl->setPlaceholderText(QStringLiteral("http://192.168.1.20:%1/").arg(Sync::kLanPort));
    lf->addRow(tr("Adresse:"), m_lanUrl);
    auto *codeRow = new QHBoxLayout;
    m_code = new QLineEdit;
    m_code->setInputMask(QStringLiteral("999999"));
    m_code->setMaximumWidth(110);
    codeRow->addWidget(m_code);
    auto *pair = new QPushButton(Icons::get(QStringLiteral("link")), tr("Koppeln"));
    codeRow->addWidget(pair);
    codeRow->addStretch();
    lf->addRow(tr("Kopplungscode:"), codeRow);
    ll->addLayout(lf);
    m_lanState = note(QString());
    ll->addWidget(m_lanState);
    ll->addStretch();
    m_pages->addWidget(lan);
    connect(search, &QPushButton::clicked, this, &SyncWayWidget::discover);
    connect(m_found, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) m_lanUrl->setText(m_found->item(row)->data(Qt::UserRole).toString());
    });
    connect(pair, &QPushButton::clicked, this, &SyncWayWidget::pairSelected);
    connect(m_lanUrl, &QLineEdit::textEdited, this, [this] {
        m_lanToken.clear();
        m_lanState->setText(tr("Noch nicht gekoppelt."));
    });

    // Freigabe
    m_hostInfo = note(QString());
    auto *host = new QWidget;
    auto *hl = new QVBoxLayout(host);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->addWidget(m_hostInfo);
    auto *newCode = new QPushButton(Icons::get(QStringLiteral("refresh")), tr("Neuen Kopplungscode"));
    hl->addWidget(newCode, 0, Qt::AlignLeft);
    hl->addStretch();
    m_pages->addWidget(host);
    connect(newCode, &QPushButton::clicked, this, [this] {
        if (m_server) m_server->newPairingCode();
        updateHostInfo();
    });

    lay->addWidget(m_pages, 1);
    connect(m_kind, &QButtonGroup::idClicked, this, [this](int id) {
        m_pages->setCurrentIndex(id);
        updateHostInfo();
        emit kindChanged();
        if (id == PageLan && m_found->count() == 0 && m_lanUrl->text().isEmpty()) discover();
    });
    if (QAbstractButton *b = m_kind->button(allowHost ? PageNone : PageWebsite)) b->setChecked(true);
    m_pages->setCurrentIndex(allowHost ? PageNone : PageWebsite);
}

void SyncWayWidget::setServer(Sync::Server *server, const QString &dbName)
{
    m_server = server;
    m_dbName = dbName;
    updateHostInfo();
}

void SyncWayWidget::updateHostInfo()
{
    if (m_kind->checkedId() != PageHost) return;
    if (!m_server || !m_server->isRunning()) {
        m_hostInfo->setText(tr("Mit <b>OK</b> wird die Freigabe gestartet. Sie ist aktiv, solange ViviaMeals auf diesem Rechner läuft "
                               "(Port %1; die Firewall muss Verbindungen im privaten Netz zulassen). Danach erscheint hier der "
                               "Kopplungscode für die anderen Geräte.")
                                .arg(Sync::kLanPort));
        return;
    }
    m_hostInfo->setText(tr("<p>Die Datenbank ist im lokalen Netz freigegeben (solange ViviaMeals auf diesem Rechner läuft).</p>"
                           "<p>Kopplungscode für ein weiteres Gerät: <span style='font-size:20pt; font-weight:bold; letter-spacing:4px'>%1</span><br>"
                           "(gilt für eine Kopplung; danach erscheint ein neuer)</p>"
                           "<p>Adresse, falls das andere Gerät die Freigabe nicht findet:<br><b>%2</b></p>"
                           "<p>Falls keine Verbindung zustande kommt: In der Firewall Verbindungen für ViviaMeals im privaten Netz "
                           "zulassen (TCP-Port %3, UDP-Port %4).</p>")
                            .arg(m_server->pairingCode(), m_server->addresses().join(QStringLiteral("<br>")))
                            .arg(m_server->port())
                            .arg(Sync::kDiscoveryPort));
}

void SyncWayWidget::setConnection(const Sync::Connection &c)
{
    int id = PageNone;
    switch (c.kind) {
    case Sync::Connection::Kind::Website:
        id = PageWebsite;
        m_url->setText(c.url);
        m_key->setText(c.token);
        break;
    case Sync::Connection::Kind::Lan:
        id = PageLan;
        m_lanUrl->setText(c.url);
        m_lanToken = c.token;
        m_lanDevice = c.deviceLabel;
        m_lanState->setText(c.token.isEmpty() ? tr("Noch nicht gekoppelt.")
                                              : tr("Gekoppelt mit <b>%1</b>.").arg((c.deviceLabel.isEmpty() ? c.url : c.deviceLabel).toHtmlEscaped()));
        break;
    case Sync::Connection::Kind::Host: id = PageHost; break;
    case Sync::Connection::Kind::None: break;
    }
    if (!m_allowHost && (id == PageNone || id == PageHost)) id = PageWebsite;
    if (QAbstractButton *b = m_kind->button(id)) b->setChecked(true);
    m_pages->setCurrentIndex(id);
    updateHostInfo();
}

Sync::Connection SyncWayWidget::connection() const
{
    Sync::Connection c;
    switch (m_kind->checkedId()) {
    case PageWebsite:
        c.kind = Sync::Connection::Kind::Website;
        c.url = m_url->text().trimmed();
        c.token = m_key->text().trimmed();
        if (!c.url.isEmpty() && !c.url.contains(QLatin1String("://"))) c.url.prepend(QStringLiteral("https://"));
        break;
    case PageLan:
        c.kind = Sync::Connection::Kind::Lan;
        c.url = m_lanUrl->text().trimmed();
        c.token = m_lanToken;
        c.deviceLabel = m_lanDevice;
        break;
    case PageHost: c.kind = Sync::Connection::Kind::Host; break;
    default: break;
    }
    if (c.kind != Sync::Connection::Kind::Host && !c.valid()) c.kind = Sync::Connection::Kind::None;
    return c;
}

void SyncWayWidget::discover()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_found->clear();
    const QList<Sync::Found> found = Sync::discover(1500);
    QApplication::restoreOverrideCursor();
    for (const Sync::Found &f : found) {
        auto *it = new QListWidgetItem(Icons::get(QStringLiteral("database")), tr("%1 – auf %2").arg(f.name, f.device), m_found);
        it->setData(Qt::UserRole, f.url);
        it->setData(Qt::UserRole + 1, f.device);
        it->setToolTip(f.url);
    }
    if (found.isEmpty()) {
        auto *it = new QListWidgetItem(tr("Keine Freigabe gefunden – Adresse unten von Hand eintragen."), m_found);
        it->setFlags(Qt::NoItemFlags);
    } else if (found.size() == 1) {
        m_found->setCurrentRow(0);
    }
}

void SyncWayWidget::pairSelected()
{
    QString url = m_lanUrl->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::information(this, tr("Koppeln"), tr("Bitte eine Freigabe auswählen oder ihre Adresse eintragen."));
        return;
    }
    if (!url.contains(QLatin1String("://"))) url.prepend(QStringLiteral("http://"));
    if (!url.section(QLatin1Char('/'), 2, 2).contains(QLatin1Char(':'))) url = url.section(QLatin1Char('/'), 0, 2) + QStringLiteral(":%1/").arg(Sync::kLanPort);
    if (!url.endsWith(QLatin1Char('/'))) url += QLatin1Char('/');
    m_lanUrl->setText(url);
    QString token, err;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = Sync::HttpTransport::pair(url, m_code->text(), &token, &err);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        m_lanState->setText(tr("<span style='color:#c0392b'>Kopplung fehlgeschlagen: %1</span>").arg(err.toHtmlEscaped()));
        return;
    }
    m_lanToken = token;
    const QListWidgetItem *it = m_found->currentItem();
    m_lanDevice = it && it->data(Qt::UserRole).toString() == url ? it->data(Qt::UserRole + 1).toString() : url.section(QLatin1Char('/'), 2, 2);
    m_lanState->setText(tr("Gekoppelt mit <b>%1</b>.").arg(m_lanDevice.toHtmlEscaped()));
    m_code->clear();
}

void SyncWayWidget::saveScript()
{
    if (m_key->text().trimmed().isEmpty()) {
        m_key->setText(Sync::newToken());
        m_key->setEchoMode(QLineEdit::Normal);
    }
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Ordner für das Sync-Skript wählen"),
                                                          QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    if (dir.isEmpty()) return;
    const QString file = QDir(dir).filePath(QStringLiteral("viviameals-sync.php"));
    QSaveFile f(file);
    const QByteArray data = Sync::phpScript(Sync::sha256(m_key->text().trimmed().toLatin1())).toUtf8();
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit()) {
        QMessageBox::warning(this, tr("Sync-Skript"), f.errorString());
        return;
    }
    QMessageBox::information(this, tr("Sync-Skript"),
                             tr("<p><b>%1</b> wurde gespeichert.</p>"
                                "<ol><li>Laden Sie die Datei per FTP/SFTP in einen Ordner Ihrer Website hoch, z. B. <i>/viviameals/</i>.</li>"
                                "<li>Tragen Sie hier die Adresse ein, z. B. <i>https://www.meine-seite.de/viviameals/viviameals-sync.php</i>.</li>"
                                "<li>Mit <b>Verbindung prüfen</b> testen.</li></ol>"
                                "<p>Der Zugangsschlüssel steht nur als Prüfsumme im Skript. Wird ein neuer Schlüssel erzeugt, "
                                "muss das Skript erneut gespeichert und hochgeladen werden.</p>"
                                "<p>Hinweis für nginx-Server: dort wirkt die .htaccess-Sperre nicht – bitte den Ordner "
                                "<i>vm-sync-daten</i> in der Serverkonfiguration sperren.</p>")
                                 .arg(QDir::toNativeSeparators(file).toHtmlEscaped()));
}

bool SyncWayWidget::check(QString *message)
{
    const Sync::Connection c = connection();
    if (c.kind == Sync::Connection::Kind::None || c.kind == Sync::Connection::Kind::Host) {
        *message = c.kind == Sync::Connection::Kind::Host ? tr("Die Freigabe läuft auf diesem Rechner.") : tr("Bitte die Angaben vervollständigen (bzw. koppeln).");
        return c.kind == Sync::Connection::Kind::Host;
    }
    Sync::HttpTransport t(c.url, c.token);
    Sync::StoreInfo i;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = t.info(&i);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        *message = t.lastError();
        return false;
    }
    *message = i.initialized ? tr("Verbindung in Ordnung. Im Sync-Speicher liegt die Datenbank „%1“ (%2 Dateien, Stand %3).").arg(i.name).arg(i.files).arg(i.revision)
                             : tr("Verbindung in Ordnung. Der Sync-Speicher ist noch leer – beim ersten Abgleich wird diese Datenbank hochgeladen.");
    return true;
}

// ---------------------------------------------------------------- Einrichten

SyncSetupDialog::SyncSetupDialog(const QString &dbFolder, const QString &dbName, Sync::Server *server, QWidget *parent)
    : QDialog(parent), m_db(dbFolder)
{
    setObjectName(QStringLiteral("SyncSetupDialog"));
    setWindowTitle(tr("Abgleich einrichten – %1").arg(dbName));
    setWindowIcon(Icons::get(QStringLiteral("sync")));
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(note(tr("Mit dem Abgleich bleibt diese Rezeptdatenbank auf mehreren Geräten gleich – einschließlich Profilen, "
                           "Gewichtsverlauf, Wochenplänen, Ernährungstagebuch und eigenen Lebensmitteln. Jedes Gerät behält eine "
                           "vollständige Kopie und kann auch ohne Verbindung arbeiten. Wurde dasselbe Rezept auf zwei Geräten "
                           "geändert, bleiben beide Fassungen erhalten.")));
    m_way = new SyncWayWidget(true);
    m_way->setServer(server, dbName);
    m_way->setConnection(Sync::Connection::load(dbFolder));
    lay->addWidget(m_way, 1);
    auto *opts = new QFormLayout;
    m_device = new QLineEdit(Sync::deviceName());
    m_device->setToolTip(tr("Erscheint bei Konflikten und in der Liste der Freigaben"));
    opts->addRow(tr("Name dieses Geräts:"), m_device);
    lay->addLayout(opts);
    m_auto = new QCheckBox(tr("Beim Öffnen und Schließen der Datenbank automatisch abgleichen"));
    m_auto->setChecked(Sync::Connection::load(dbFolder).autoSync);
    lay->addWidget(m_auto);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QPushButton *test = bb->addButton(tr("Verbindung prüfen"), QDialogButtonBox::ActionRole);
    test->setIcon(Icons::get(QStringLiteral("check")));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &SyncSetupDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(test, &QPushButton::clicked, this, [this] {
        QString msg;
        if (m_way->check(&msg)) QMessageBox::information(this, tr("Verbindung prüfen"), msg);
        else QMessageBox::warning(this, tr("Verbindung prüfen"), msg);
    });
    resize(640, 600);
}

Sync::Connection SyncSetupDialog::connection() const
{
    Sync::Connection c = m_way->connection();
    c.autoSync = m_auto->isChecked();
    return c;
}

void SyncSetupDialog::accept()
{
    const QString dev = m_device->text().trimmed();
    QSettings().setValue(QStringLiteral("sync/deviceName"), dev == QSysInfo::machineHostName() ? QString() : dev);
    QDialog::accept();
}

// ---------------------------------------------------------------- Laden (neue Installation)

SyncLoadDialog::SyncLoadDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("SyncLoadDialog"));
    setWindowTitle(tr("Datenbank aus dem Abgleich laden"));
    setWindowIcon(Icons::get(QStringLiteral("cloud-down")));
    auto *lay = new QVBoxLayout(this);
    lay->addWidget(note(tr("Holt eine Rezeptdatenbank, die auf einem anderen Gerät bereits abgeglichen wird, vollständig auf dieses Gerät "
                           "(mit Profilen, Tagebuch und eigenen Lebensmitteln) und hält sie künftig auf demselben Weg aktuell.")));
    m_way = new SyncWayWidget(false);
    lay->addWidget(m_way, 1);
    auto *f = new QFormLayout;
    auto *pr = new QHBoxLayout;
    m_parent = new QLineEdit(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    pr->addWidget(m_parent, 1);
    auto *browse = new QPushButton(Icons::get(QStringLiteral("open")), tr("…"));
    pr->addWidget(browse);
    f->addRow(tr("Speichern in:"), pr);
    m_name = new QLineEdit;
    m_name->setPlaceholderText(tr("wie auf dem anderen Gerät"));
    f->addRow(tr("Ordnername:"), m_name);
    lay->addLayout(f);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, tr("Ordner wählen, in dem die Datenbank gespeichert wird"), m_parent->text());
        if (!d.isEmpty()) m_parent->setText(QDir::toNativeSeparators(d));
    });
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Laden"));
    bb->button(QDialogButtonBox::Ok)->setIcon(Icons::get(QStringLiteral("cloud-down")));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &SyncLoadDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    resize(640, 560);
}

void SyncLoadDialog::accept()
{
    Sync::Connection c = m_way->connection();
    if (!c.valid()) {
        QMessageBox::information(this, windowTitle(), tr("Bitte Adresse und Zugangsschlüssel eintragen bzw. mit dem freigebenden Gerät koppeln."));
        return;
    }
    Sync::HttpTransport t(c.url, c.token);
    Sync::StoreInfo info;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = t.info(&info);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, windowTitle(), t.lastError());
        return;
    }
    if (!info.initialized || info.files == 0) {
        QMessageBox::information(this, windowTitle(),
                                 tr("Im Sync-Speicher liegt noch keine Datenbank. Richten Sie den Abgleich zuerst auf dem Gerät ein, "
                                    "das die Datenbank enthält (Datei → Abgleich → Abgleich einrichten), und gleichen Sie dort einmal ab."));
        return;
    }
    QString name = m_name->text().trimmed();
    if (name.isEmpty()) name = info.name.isEmpty() ? tr("Meine Rezepte") : info.name;
    name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    const QString folder = QDir(QDir::fromNativeSeparators(m_parent->text().trimmed())).filePath(name);
    if (QDir(folder).exists() && !QDir(folder).isEmpty()) {
        QMessageBox::warning(this, windowTitle(), tr("Der Ordner %1 existiert bereits und ist nicht leer – bitte einen anderen Namen wählen.")
                                                      .arg(QDir::toNativeSeparators(folder)));
        return;
    }
    if (!QDir().mkpath(folder)) {
        QMessageBox::warning(this, windowTitle(), tr("Der Ordner %1 kann nicht angelegt werden.").arg(QDir::toNativeSeparators(folder)));
        return;
    }
    c.autoSync = true;
    c.save(folder);
    const Sync::Result r = SyncUi::run(this, c, folder, false);
    if (!r.ok || !RecipeDb::isDatabaseFolder(folder)) {
        QMessageBox::warning(this, windowTitle(), r.ok ? tr("Die geladenen Daten enthalten keine Rezeptdatenbank.") : r.summary());
        if (!r.ok) QDir(folder).removeRecursively();
        return;
    }
    m_result = folder;
    QDialog::accept();
}

// ---------------------------------------------------------------- Ausführen

namespace SyncUi {

Sync::Result run(QWidget *parent, const Sync::Connection &c, const QString &dbFolder, bool quiet)
{
    std::unique_ptr<Sync::Transport> t(Sync::makeTransport(c, dbFolder));
    if (!t) {
        Sync::Result r;
        r.error = QCoreApplication::translate("SyncUi", "Kein Abgleich eingerichtet.");
        return r;
    }
    Sync::Engine e(t.get(), dbFolder);
    if (quiet && c.kind == Sync::Connection::Kind::Host) return e.run();   // nur lokale Dateien: sofort fertig
    BusyProgress prog(parent, QCoreApplication::translate("SyncUi", "Datenbank abgleichen"), 100);
    Sync::Result r = e.run(prog.callback());
    if (!r.ok && c.kind == Sync::Connection::Kind::Lan && !r.canceled && !quiet) {   // automatischer Abgleich: nicht im Netz suchen
        // Adresse des freigebenden Rechners geändert (DHCP)? Im Netz neu suchen
        const QString store = QJsonDocument::fromJson([&] {
                                  QFile f(QDir(dbFolder).filePath(QStringLiteral(".sync/zustand.json")));
                                  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
                              }())
                                  .object()
                                  .value(QStringLiteral("store"))
                                  .toString();
        if (!store.isEmpty()) {
            for (const Sync::Found &f : Sync::discover(1200)) {
                if (f.uuid != store || f.url == c.url) continue;
                Sync::Connection moved = c;
                moved.url = f.url;
                moved.save(dbFolder);
                Sync::HttpTransport ht(moved.url, moved.token);
                Sync::Engine e2(&ht, dbFolder);
                r = e2.run(prog.callback());
                break;
            }
        }
    }
    return r;
}

QString lastSyncText(const QString &dbFolder)
{
    QFile f(QDir(dbFolder).filePath(QStringLiteral(".sync/zustand.json")));
    if (!f.open(QIODevice::ReadOnly)) return QCoreApplication::translate("SyncUi", "noch nicht abgeglichen");
    const QDateTime t = QDateTime::fromString(QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("last")).toString(), Qt::ISODate);
    if (!t.isValid()) return QCoreApplication::translate("SyncUi", "noch nicht abgeglichen");
    const QLocale de(QLocale::German);
    return t.date() == QDate::currentDate() ? QCoreApplication::translate("SyncUi", "abgeglichen um %1").arg(de.toString(t.time(), QStringLiteral("HH:mm")))
                                            : QCoreApplication::translate("SyncUi", "abgeglichen am %1").arg(de.toString(t, QStringLiteral("dd.MM.yyyy HH:mm")));
}

} // namespace SyncUi
