// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "imagecropdialog.h"
#include "icons.h"
#include "qtcompat.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSettings>
#include <QTransform>
#include <QVBoxLayout>

// ---------------------------------------------------------------- CropWidget

CropWidget::CropWidget(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(320, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void CropWidget::setImage(const QImage &img)
{
    m_img = img;
    selectAll();
}

void CropWidget::setSelection(const QRectF &r)
{
    if (m_img.isNull() || r.isEmpty()) return;
    m_sel = r;
    clampSel();
    update();
    emit selectionChanged();
}

void CropWidget::selectAll()
{
    if (m_img.isNull()) return;
    QRectF r(0, 0, m_img.width(), m_img.height());
    if (m_aspect > 0) {
        // größter Ausschnitt mit dem Seitenverhältnis, mittig
        double w = r.width(), h = w / m_aspect;
        if (h > r.height()) {
            h = r.height();
            w = h * m_aspect;
        }
        r = QRectF((m_img.width() - w) / 2.0, (m_img.height() - h) / 2.0, w, h);
    }
    m_sel = r;
    update();
    emit selectionChanged();
}

void CropWidget::setAspect(double aspect)
{
    m_aspect = aspect;
    if (m_aspect > 0 && !m_sel.isEmpty()) {
        const QPointF c = m_sel.center();
        QRectF r = m_sel;
        r.setHeight(r.width() / m_aspect);
        if (r.height() > m_img.height()) {
            r.setHeight(m_img.height());
            r.setWidth(r.height() * m_aspect);
        }
        r.moveCenter(c);
        m_sel = r;
        clampSel();
    }
    update();
    emit selectionChanged();
}

QRect CropWidget::selection() const
{
    return m_sel.toAlignedRect().intersected(m_img.rect());
}

QRectF CropWidget::viewRect() const
{
    if (m_img.isNull()) return {};
    const QSizeF s = QSizeF(m_img.size()).scaled(QSizeF(size()) - QSizeF(16, 16), Qt::KeepAspectRatio);
    return QRectF((width() - s.width()) / 2.0, (height() - s.height()) / 2.0, s.width(), s.height());
}

QPointF CropWidget::toImage(const QPointF &p) const
{
    const QRectF v = viewRect();
    if (v.isEmpty()) return {};
    return { (p.x() - v.left()) * m_img.width() / v.width(), (p.y() - v.top()) * m_img.height() / v.height() };
}

QRectF CropWidget::toView(const QRectF &r) const
{
    const QRectF v = viewRect();
    if (m_img.isNull()) return {};
    const double sx = v.width() / m_img.width(), sy = v.height() / m_img.height();
    return { v.left() + r.left() * sx, v.top() + r.top() * sy, r.width() * sx, r.height() * sy };
}

void CropWidget::clampSel()
{
    QRectF r = m_sel;
    if (r.width() > m_img.width()) r.setWidth(m_img.width());
    if (r.height() > m_img.height()) r.setHeight(m_img.height());
    if (r.left() < 0) r.moveLeft(0);
    if (r.top() < 0) r.moveTop(0);
    if (r.right() > m_img.width()) r.moveRight(m_img.width());
    if (r.bottom() > m_img.height()) r.moveBottom(m_img.height());
    m_sel = r;
}

void CropWidget::fitAspect(QRectF &r, Drag corner) const
{
    if (m_aspect <= 0) return;
    // Höhe aus der Breite; die gegenüberliegende Ecke bleibt fest
    const double h = r.width() / m_aspect;
    if (corner == TopLeft || corner == TopRight) r.setTop(r.bottom() - h);
    else r.setBottom(r.top() + h);
}

void CropWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    if (m_img.isNull()) return;
    const QRectF v = viewRect();
    p.drawImage(v, m_img);
    const QRectF s = toView(m_sel);
    QPainterPath outside;
    outside.addRect(v);
    QPainterPath inner;
    inner.addRect(s);
    p.fillPath(outside.subtracted(inner), QColor(0, 0, 0, 140));
    p.setPen(QPen(QColor(224, 98, 13), 2));
    p.drawRect(s);
    // Drittel-Linien
    p.setPen(QPen(QColor(255, 255, 255, 110), 1, Qt::DashLine));
    for (int i = 1; i < 3; ++i) {
        p.drawLine(QPointF(s.left() + s.width() * i / 3.0, s.top()), QPointF(s.left() + s.width() * i / 3.0, s.bottom()));
        p.drawLine(QPointF(s.left(), s.top() + s.height() * i / 3.0), QPointF(s.right(), s.top() + s.height() * i / 3.0));
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(224, 98, 13));
    for (const QPointF &c : { s.topLeft(), s.topRight(), s.bottomLeft(), s.bottomRight() }) p.drawRect(QRectF(c - QPointF(5, 5), QSizeF(10, 10)));
}

void CropWidget::mousePressEvent(QMouseEvent *e)
{
    if (m_img.isNull() || e->button() != Qt::LeftButton) return;
    const QPointF pos = vpPos(e);
    const QRectF s = toView(m_sel);
    auto near = [&](const QPointF &c) { return (pos - c).manhattanLength() < 14; };
    if (near(s.topLeft())) m_drag = TopLeft;
    else if (near(s.topRight())) m_drag = TopRight;
    else if (near(s.bottomLeft())) m_drag = BottomLeft;
    else if (near(s.bottomRight())) m_drag = BottomRight;
    else if (s.contains(pos)) m_drag = Move;
    else m_drag = New;
    m_anchor = toImage(pos);
    m_start = m_sel;
}

void CropWidget::mouseMoveEvent(QMouseEvent *e)
{
    const QPointF pos = vpPos(e);
    if (m_drag == None) {
        const QRectF s = toView(m_sel);
        auto near = [&](const QPointF &c) { return (pos - c).manhattanLength() < 14; };
        if (near(s.topLeft()) || near(s.bottomRight())) setCursor(Qt::SizeFDiagCursor);
        else if (near(s.topRight()) || near(s.bottomLeft())) setCursor(Qt::SizeBDiagCursor);
        else if (s.contains(pos)) setCursor(Qt::SizeAllCursor);
        else setCursor(Qt::CrossCursor);
        return;
    }
    QPointF ip = toImage(pos);
    ip.setX(qBound(0.0, ip.x(), double(m_img.width())));
    ip.setY(qBound(0.0, ip.y(), double(m_img.height())));
    QRectF r = m_start;
    switch (m_drag) {
    case Move:
        r.translate(ip - m_anchor);
        m_sel = r;
        clampSel();
        break;
    case New: {
        r = QRectF(m_anchor, ip).normalized();
        const Drag corner = (ip.y() < m_anchor.y()) ? TopLeft : BottomRight;
        if (m_aspect > 0) {
            if (ip.y() < m_anchor.y()) {
                r.setBottom(m_anchor.y());
                r.setTop(m_anchor.y() - r.width() / m_aspect);
            } else {
                r.setTop(m_anchor.y());
                r.setBottom(m_anchor.y() + r.width() / m_aspect);
            }
        }
        Q_UNUSED(corner)
        if (r.width() >= 16 && r.height() >= 16) m_sel = r.intersected(QRectF(m_img.rect()));
        break;
    }
    default: {
        if (m_drag == TopLeft || m_drag == BottomLeft) r.setLeft(qMin(ip.x(), r.right() - 16));
        else r.setRight(qMax(ip.x(), r.left() + 16));
        if (m_aspect > 0) {
            fitAspect(r, m_drag);
        } else {
            if (m_drag == TopLeft || m_drag == TopRight) r.setTop(qMin(ip.y(), r.bottom() - 16));
            else r.setBottom(qMax(ip.y(), r.top() + 16));
        }
        if (r.top() >= 0 && r.bottom() <= m_img.height() && r.left() >= 0 && r.right() <= m_img.width()) m_sel = r;
    }
    }
    update();
    emit selectionChanged();
}

void CropWidget::mouseReleaseEvent(QMouseEvent *)
{
    m_drag = None;
}

void CropWidget::resizeEvent(QResizeEvent *)
{
    update();
}

// ---------------------------------------------------------------- Dialog

ImageCropDialog::ImageCropDialog(const QImage &img, QWidget *parent, Purpose purpose) : QDialog(parent), m_purpose(purpose)
{
    const bool print = purpose == Purpose::PrintBackground;
    setWindowTitle(print ? tr("Hintergrundbild zuschneiden") : tr("Rezeptbild zuschneiden"));
    auto *lay = new QVBoxLayout(this);
    auto *hint = new QLabel(print ? tr("Ausschnitt mit der Maus verschieben, an den Ecken die Größe ändern oder außerhalb neu aufziehen. "
                                       "Das Originalbild bleibt unverändert; der Ausschnitt wird beim Erstellen des Master-Dokuments "
                                       "in voller Auflösung übernommen.")
                                  : tr("Ausschnitt mit der Maus verschieben, an den Ecken die Größe ändern oder außerhalb neu aufziehen. "
                                       "Gespeichert wird nur der Ausschnitt (PNG, höchstens %1 Pixel breit).")
                                        .arg(maxWidth()));
    hint->setWordWrap(true);
    lay->addWidget(hint);
    m_crop = new CropWidget;
    lay->addWidget(m_crop, 1);
    auto *bar = new QHBoxLayout;
    bar->addWidget(new QLabel(tr("Seitenverhältnis:")));
    m_aspect = new QComboBox;
    m_aspect->addItem(tr("4 : 3 (Querformat)"), 4.0 / 3.0);
    m_aspect->addItem(tr("3 : 2"), 1.5);
    m_aspect->addItem(tr("16 : 9"), 16.0 / 9.0);
    m_aspect->addItem(tr("1 : 1 (quadratisch)"), 1.0);
    m_aspect->addItem(tr("3 : 4 (Hochformat)"), 0.75);
    m_aspect->addItem(tr("frei"), 0.0);
    if (print) {
        m_aspect->addItem(tr("DIN A4 hoch (210 : 297)"), 210.0 / 297.0);
        m_aspect->addItem(tr("DIN A4 quer (297 : 210)"), 297.0 / 210.0);
    }
    QSettings s;
    const QString aspectKey = print ? QStringLiteral("tex/cropAspect") : QStringLiteral("editor/cropAspect");
    m_aspect->setCurrentIndex(qBound(0, s.value(aspectKey, print ? 5 : 0).toInt(), m_aspect->count() - 1));
    bar->addWidget(m_aspect);
    auto *left = new QPushButton(tr("⟲ Drehen"));
    auto *right = new QPushButton(tr("Drehen ⟳"));
    auto *all = new QPushButton(tr("Größter Ausschnitt"));
    bar->addWidget(left);
    bar->addWidget(right);
    bar->addWidget(all);
    bar->addStretch(1);
    m_info = new QLabel;
    bar->addWidget(m_info);
    lay->addLayout(bar);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Zuschneiden"));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, [this, aspectKey] {
        QSettings().setValue(aspectKey, m_aspect->currentIndex());
        accept();
    });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_crop->setAspect(m_aspect->currentData().toDouble());
    m_crop->setImage(img.convertToFormat(QImage::Format_RGB32));
    connect(m_aspect, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { m_crop->setAspect(m_aspect->currentData().toDouble()); });
    auto rotate = [this](int deg) {
        m_rotation = ((m_rotation + deg) % 360 + 360) % 360;
        m_crop->setImage(m_crop->image().transformed(QTransform().rotate(deg)));
    };
    connect(left, &QPushButton::clicked, this, [rotate] { rotate(-90); });
    connect(right, &QPushButton::clicked, this, [rotate] { rotate(90); });
    connect(all, &QPushButton::clicked, m_crop, &CropWidget::selectAll);
    connect(m_crop, &CropWidget::selectionChanged, this, &ImageCropDialog::updateInfo);
    updateInfo();
    resize(820, 680);
}

QRectF ImageCropDialog::normalizedSelection() const
{
    const QRect r = m_crop->selection();
    const QSize sz = m_crop->image().size();
    if (sz.isEmpty() || r.isEmpty()) return {};
    if (r.size() == sz) return {};   // ganzes Bild = kein Zuschnitt
    return QRectF(double(r.x()) / sz.width(), double(r.y()) / sz.height(), double(r.width()) / sz.width(), double(r.height()) / sz.height());
}

void ImageCropDialog::setInitial(int rotation, const QRectF &normalized)
{
    rotation = ((rotation % 360) + 360) % 360;
    if (rotation != 0) {
        m_rotation = rotation;
        m_crop->setImage(m_crop->image().transformed(QTransform().rotate(rotation)));
    }
    const QSize sz = m_crop->image().size();
    if (!normalized.isEmpty())
        m_crop->setSelection(QRectF(normalized.x() * sz.width(), normalized.y() * sz.height(), normalized.width() * sz.width(),
                                    normalized.height() * sz.height()));
}

void ImageCropDialog::updateInfo()
{
    const QRect r = m_crop->selection();
    if (m_purpose == Purpose::PrintBackground) {
        m_info->setText(tr("%1 × %2 Pixel").arg(r.width()).arg(r.height()));
        return;
    }
    const QSize out = r.width() > maxWidth() ? QSize(maxWidth(), qRound(double(r.height()) * maxWidth() / r.width())) : r.size();
    m_info->setText(tr("%1 × %2 Pixel").arg(out.width()).arg(out.height()));
}

QImage ImageCropDialog::result() const
{
    QImage img = m_crop->image().copy(m_crop->selection());
    if (m_purpose == Purpose::PrintBackground) return img;
    if (img.width() > maxWidth()) img = img.scaledToWidth(maxWidth(), Qt::SmoothTransformation);
    if (img.height() > 1200) img = img.scaledToHeight(1200, Qt::SmoothTransformation);
    return img;
}
