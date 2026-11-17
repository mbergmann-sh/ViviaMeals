// ViviaMeals – Rezept per Web-Adresse übernehmen (Datei → Import → Rezept von einer Webseite …)
//
// Eine oder mehrere Adressen von Rezeptseiten einfügen; ViviaMeals lädt jede Seite einmal, liest das
// maschinenlesbare Rezept (schema.org, siehe webrecipe.h), lädt das Rezeptfoto und gibt alles durch die
// übliche Import-Nachbearbeitung. Es werden nur die angegebenen Seiten abgerufen – keine weiteren Links.
// Verweigert eine Website den Abruf durch Programme, lässt sich die im Browser gespeicherte Seite
// („Webseite, nur HTML“) öffnen.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "recipe.h"

#include <QDialog>
#include <QList>
#include <QPointer>
#include <QStringList>
#include <QUrl>

class QCheckBox;
class QLabel;
class QListWidget;
class QNetworkAccessManager;
class QNetworkReply;
class QPlainTextEdit;
class QPushButton;

class WebImportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit WebImportDialog(QWidget *parent = nullptr);
    ~WebImportDialog() override;

    QList<Recipe> recipes() const { return m_recipes; }   // fertig aufbereitete Rezepte (mit Foto)
    bool review() const;                                 // einzelnes Rezept vor dem Speichern im Editor zeigen

private:
    void start();
    void openFile();
    void next();
    void fetchPage(const QUrl &url);
    void pageLoaded(const QUrl &url, const QByteArray &body, const QString &contentType);
    void fetchImage(const QUrl &image);
    void finishCurrent(const QByteArray &imageData);
    void report(bool ok, const QString &text);
    void setBusy(bool busy);
    QString summaryText() const;
    QNetworkReply *get(const QUrl &url, const QByteArray &accept);

    QPlainTextEdit *m_urls = nullptr;
    QListWidget *m_log = nullptr;
    QLabel *m_status = nullptr;
    QCheckBox *m_review = nullptr;
    QPushButton *m_start = nullptr;
    QPushButton *m_file = nullptr;
    QPushButton *m_done = nullptr;
    QNetworkAccessManager *m_nam = nullptr;
    QPointer<QNetworkReply> m_reply;

    QList<QUrl> m_queue;
    QUrl m_current;          // Seite, die gerade bearbeitet wird
    Recipe m_pending;        // Rezept der aktuellen Seite, wartet auf das Foto
    QList<Recipe> m_recipes;
    int m_failed = 0;
    bool m_fileMode = false;   // gespeicherte Seiten: nacheinander, ohne Warteschlange
};
