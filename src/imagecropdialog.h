// ViviaMeals – Rezeptbild zuschneiden
//
// Nach der Bildauswahl im Editor: Ausschnitt mit der Maus festlegen (verschieben, an den Ecken
// ziehen, außerhalb neu aufziehen), Seitenverhältnis 4:3 / 3:2 / 1:1 / 16:9 / frei, drehen.
// Ergebnis: der Ausschnitt, auf höchstens 800 px Breite verkleinert. Der Editor speichert ihn als
// PNG im Rezept – das Originalbild wird nicht übernommen.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QDialog>
#include <QImage>
#include <QWidget>

class QComboBox;
class QLabel;

class CropWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CropWidget(QWidget *parent = nullptr);
    void setImage(const QImage &img);
    const QImage &image() const { return m_img; }
    void setAspect(double aspect);   // Breite/Höhe, 0 = frei
    QRect selection() const;         // in Bildpunkten
    void selectAll();
    QSize sizeHint() const override { return { 720, 520 }; }

signals:
    void selectionChanged();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void resizeEvent(QResizeEvent *) override;

private:
    enum Drag { None, Move, New, TopLeft, TopRight, BottomLeft, BottomRight };
    QRectF viewRect() const;                 // Bild im Widget
    QPointF toImage(const QPointF &p) const;
    QRectF toView(const QRectF &r) const;
    void fitAspect(QRectF &r, Drag corner) const;
    void clampSel();
    QImage m_img;
    QRectF m_sel;      // Bildkoordinaten
    double m_aspect = 4.0 / 3.0;
    Drag m_drag = None;
    QPointF m_anchor;  // Bildkoordinaten beim Drücken
    QRectF m_start;
};

class ImageCropDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ImageCropDialog(const QImage &img, QWidget *parent = nullptr);
    QImage result() const;   // zugeschnitten und verkleinert
    static int maxWidth() { return 800; }

private:
    void updateInfo();
    CropWidget *m_crop;
    QComboBox *m_aspect;
    QLabel *m_info;
};
