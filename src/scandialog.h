// ViviaMeals – Rezept scannen
//
// Ablauf: Seite scannen (Windows-Scandialog, unter Linux/macOS ein Scanprogramm) oder Bild/Foto
// öffnen → Fotos auf der Seite werden markiert (orange; anklicken entfernt, auf freier Fläche
// aufziehen fügt hinzu) → Texterkennung mit Tesseract → Titel, Portionen, Zutaten und Zubereitung
// werden vorbelegt und lassen sich korrigieren → „Im Editor öffnen“ übernimmt das Rezept (Nährwerte,
// Tags, Umlaute, ggf. DeepL wie beim Import) in den Rezepteditor, der den Scan als Vorlage daneben zeigt.
// Mehrseitige Rezepte: einfach weitere Seiten scannen oder öffnen.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include "ocr.h"
#include "recipe.h"
#include "recipescan.h"
#include "escl.h"

#include <QDateTime>
#include <QHash>
#include <QSet>

#include <QDialog>
#include <QImage>
#include <QPixmap>
#include <QScrollArea>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProcess;
class QProgressBar;
class QPushButton;
class QTabWidget;
class QTimer;
class QToolButton;

// Bildfläche mit Zoom und (optional) bearbeitbaren Fotobereichen
class ScanCanvas : public QWidget
{
    Q_OBJECT
public:
    explicit ScanCanvas(QWidget *parent = nullptr);
    void setImage(const QImage &img);
    void setRegions(const QList<QRect> &r);
    QList<QRect> regions() const { return m_regions; }
    void setEditable(bool e) { m_editable = e; }
    void setZoom(double z);
    double zoom() const { return m_zoom; }
    const QImage &image() const { return m_img; }
    QSize sizeHint() const override;

signals:
    void regionsChanged();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    QPoint toImage(const QPoint &p) const;
    QImage m_img;
    QPixmap m_scaled;
    QList<QRect> m_regions;
    double m_zoom = 0.25;
    bool m_editable = false;
    bool m_dragging = false;
    QPoint m_dragStart, m_dragEnd;   // Bildkoordinaten
};

class ScanImageView : public QScrollArea
{
    Q_OBJECT
public:
    explicit ScanImageView(QWidget *parent = nullptr);
    ScanCanvas *canvas() const { return m_canvas; }
    void setImage(const QImage &img);
    void zoomIn();
    void zoomOut();
    void fitWidth();

protected:
    void resizeEvent(QResizeEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

private:
    void applyFit();
    ScanCanvas *m_canvas;
    bool m_fit = true;
};

// Scan als Vorlage im Rezepteditor (Seitenauswahl und Zoom)
class ScanReferencePanel : public QWidget
{
    Q_OBJECT
public:
    explicit ScanReferencePanel(const QList<QImage> &pages, QWidget *parent = nullptr);

private:
    ScanImageView *m_view;
    QList<QImage> m_pages;
};

class ScanDialog : public QDialog
{
    Q_OBJECT
public:
    enum Start { StartNothing, StartScan, StartOpen };
    explicit ScanDialog(Start start = StartNothing, QWidget *parent = nullptr);
    ~ScanDialog() override;
    Recipe recipe() const { return m_recipe; }
    QList<QImage> pages() const;
    // Seite hinzufügen (wie „Bild öffnen“); busy(): Texterkennung läuft
    bool addImage(const QImage &img, const QString &origin);
    bool busy() const;

protected:
    void accept() override;

private:
    struct Page {
        QImage img;
        QList<QRect> photos;
        QList<Ocr::Line> lines;
        bool done = false;
    };
    void scan();
    void openImage();
    void showPage(int i);
    void removePage();
    void recognize();
    void runNext();
    void ocrFinished();
    void fillFields(bool force);
    void choosePhoto();
    void setPhoto(const QImage &img);
    void pollFile();
    void setBusy(const QString &text);
    void setStatus(const QString &text, bool warning = false);
    void updateButtons();

    QList<Page> m_pages;
    int m_current = -1;
    ScanImageView *m_view;
    QComboBox *m_pageBox;
    QPushButton *m_scanBtn, *m_openBtn, *m_removeBtn, *m_ocrBtn, *m_refillBtn;
    QLabel *m_status;
    QProgressBar *m_busy;
    QLabel *m_photoLabel;
    QPushButton *m_photoBtn, *m_photoClear;
    QLineEdit *m_title, *m_yield;
    QPlainTextEdit *m_ingredients, *m_directions, *m_rest, *m_raw;
    QTabWidget *m_tabs;
    QString m_tesseract, m_langs;
    QProcess *m_ocr = nullptr;
    QProcess *m_ext = nullptr;
    int m_ocrPage = -1;
    QTimer *m_poll;
    qint64 m_lastSize = -1;
    // überwachter Ordner (Herstellerprogramm)
    QString m_watchDir;
    QDateTime m_watchSince;
    QHash<QString, qint64> m_watchSizes;   // Datei → Größe beim letzten Blick (noch im Schreiben?)
    QSet<QString> m_watchDone;
    Escl::Scanner *m_escl = nullptr;
    bool m_filling = false, m_edited = false;
    RecipeScan::Parts m_parts;
    QImage m_photo;
    Recipe m_recipe;
};
