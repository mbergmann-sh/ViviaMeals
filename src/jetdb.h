// ViviaMeals – Microsoft-Access-Datenbanken (.mdb, JET 4 / ACE) nur lesen
//
// Kleiner, eigenständiger Leser ohne ODBC-Treiber oder Access-Installation (läuft daher auch unter
// Linux und macOS). Unterstützt das, was Rezeptprogramme wie Ambrosia brauchen: Tabellenverzeichnis,
// Zeilen mit Ja/Nein, Zahl, Währung, Datum, Text (auch komprimiert), Memo und OLE-Objekt.
// Nicht unterstützt: Access 97 (JET 3), verschlüsselte Datenbanken, Indizes (werden nicht gebraucht).
// Aufbau des Dateiformats nach der Beschreibung des mdbtools-Projekts (HACKING.md).
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

namespace Jet {

struct Column {
    QString name;
    int type = 0;          // 1 Ja/Nein, 2 Byte, 3 Integer, 4 Long, 5 Währung, 6 Single, 7 Double, 8 Datum,
                           // 9 Binär, 10 Text, 11 OLE, 12 Memo, 15 GUID, 16 Dezimal
    int number = 0;        // Spaltennummer (Null-Bitmaske)
    int varIndex = 0;      // Index unter den Spalten variabler Länge
    bool fixed = false;
    int fixedOffset = 0;
    int size = 0;
};

class Database
{
public:
    bool open(const QString &path, QString *error = nullptr);
    bool openData(const QByteArray &data, QString *error = nullptr);
    bool isOpen() const { return !m_data.isEmpty(); }
    QStringList tables() const;
    bool hasTable(const QString &name) const;
    // alle Zeilen einer Tabelle; Werte: bool, qint64, double (auch Datum: Tage seit 30.12.1899),
    // QString (Text, Memo), QByteArray (OLE, Binär); NULL = ungültiger QVariant
    QList<QVariantMap> rows(const QString &table, QString *error = nullptr) const;
    QList<Column> columns(const QString &table) const;

    // Datum/Uhrzeit aus Access (Tage seit 30.12.1899) – für Zeitspannen: Minuten
    static int minutes(const QVariant &v);

private:
    struct Table {
        QList<Column> columns;
        int varColumns = 0;
    };
    quint8 u8(qint64 off) const;
    quint16 u16(qint64 off) const;
    quint32 u32(qint64 off) const;
    qint64 pageStart(quint32 page) const { return qint64(page) * m_pageSize; }
    int pageCount() const { return int(m_data.size() / m_pageSize); }
    bool readTable(quint32 tdefPage, Table *t, QString *error) const;
    QList<QVariantMap> readRows(quint32 tdefPage, const Table &t) const;
    QVariantMap crackRow(const Table &t, qint64 start, qint64 end) const;
    QByteArray rowData(quint32 pointer) const;   // Zeile über Zeiger (Zeilennummer | Seite << 8)
    QByteArray longValue(qint64 field, int size) const;
    static QString text(const char *p, int size);

    QByteArray m_data;
    int m_pageSize = 4096;
    QHash<QString, quint32> m_tables;   // Name (klein) → Seite der Tabellendefinition
    QHash<QString, QString> m_names;    // Name (klein) → Originalname
};

} // namespace Jet
