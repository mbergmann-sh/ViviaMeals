// ViviaMeals Mobil – Rezeptliste für QML mit Suche, Thema, Favoriten und „Saisonal“
// Arbeitet nur auf den Zusammenfassungen der Datenbank (RecipeDb::summaries) – schnell auch bei Tausenden Rezepten.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QAbstractListModel>
#include <QVector>

class RecipeListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY filterChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY filterChanged)   // Tag, „:fav“ = Favoriten, „:season“ = saisonal
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int total READ total NOTIFY countChanged)
public:
    static QString perUnit(const QString &yieldUnit);   // „Stücke“ → „Stück“, leer → „Portion“
    enum Roles { IdRole = Qt::UserRole + 1, TitleRole, SubtitleRole, KcalRole, FavoriteRole, HasImageRole, TagsRole, LetterRole };
    explicit RecipeListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString query() const { return m_query; }
    void setQuery(const QString &q);
    QString theme() const { return m_theme; }
    void setTheme(const QString &t);
    int count() const { return int(m_rows.size()); }
    int total() const;

    Q_INVOKABLE QString idAt(int row) const;
    Q_INVOKABLE int indexOf(const QString &id) const;

public slots:
    void refresh();   // nach Änderungen an der Datenbank

signals:
    void filterChanged();
    void countChanged();

private:
    QString m_query, m_theme;
    QVector<int> m_rows;   // Indizes in RecipeDb::summaries()
};
