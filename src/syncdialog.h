// ViviaMeals – Dialoge zum Abgleich (Synchronisation) der Datenbank
//   SyncSetupDialog  Abgleich für die geöffnete Datenbank einrichten (Website, Gerät im LAN, Freigabe)
//   SyncLoadDialog   neue Installation: vorhandene Datenbank über denselben Weg holen und lokal speichern
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "sync.h"

#include <QDialog>

class QButtonGroup;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QStackedWidget;

namespace Sync {
class Server;
}

// gemeinsame Seiten „eigene Website“ und „Gerät im lokalen Netz“
class SyncWayWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SyncWayWidget(bool allowHost, QWidget *parent = nullptr);
    void setConnection(const Sync::Connection &c);
    Sync::Connection connection() const;   // Kind::None, wenn unvollständig
    void setServer(Sync::Server *server, const QString &dbName);
    bool check(QString *message);          // Verbindung prüfen
    void updateHostInfo();

signals:
    void kindChanged();

private:
    void discover();
    void pairSelected();
    void saveScript();
    QButtonGroup *m_kind;
    QStackedWidget *m_pages;
    QLineEdit *m_url, *m_key, *m_lanUrl, *m_code;
    QListWidget *m_found;
    QLabel *m_lanState, *m_hostInfo;
    QString m_lanToken, m_lanDevice;
    Sync::Server *m_server = nullptr;
    QString m_dbName;
    bool m_allowHost;
};

class SyncSetupDialog : public QDialog
{
    Q_OBJECT
public:
    SyncSetupDialog(const QString &dbFolder, const QString &dbName, Sync::Server *server, QWidget *parent = nullptr);
    Sync::Connection connection() const;

private:
    void accept() override;
    QString m_db;
    SyncWayWidget *m_way;
    QCheckBox *m_auto;
    QLineEdit *m_device;
};

class SyncLoadDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SyncLoadDialog(QWidget *parent = nullptr);
    QString folder() const { return m_result; }   // Ordner der geladenen Datenbank

private:
    void accept() override;
    SyncWayWidget *m_way;
    QLineEdit *m_parent;
    QLineEdit *m_name;
    QString m_result;
};

namespace SyncUi {
// führt einen Abgleich mit Fortschrittsanzeige aus (quiet: ohne Dialog, nur kurz)
Sync::Result run(QWidget *parent, const Sync::Connection &c, const QString &dbFolder, bool quiet);
QString lastSyncText(const QString &dbFolder);   // „zuletzt abgeglichen: …“
}
