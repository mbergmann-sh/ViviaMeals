// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "barcodedialog.h"
#include "barcode.h"
#include "icons.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>

#include <algorithm>
#include <QStandardPaths>
#include <QVBoxLayout>

#ifdef VIVIA_CAMERA
#include <QCamera>
#include <QCameraDevice>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QVideoFrame>
#include <QVideoSink>
#include <QVideoWidget>
#endif

bool BarcodeScanDialog::cameraAvailable()
{
#ifdef VIVIA_CAMERA
    return !QMediaDevices::videoInputs().isEmpty();
#else
    return false;
#endif
}

BarcodeScanDialog::BarcodeScanDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Strichcode scannen"));
    auto *lay = new QVBoxLayout(this);
    // ohne Kamera: Symbol und Hinweis statt einer leeren Vorschaufläche
    m_noCamBox = new QWidget;
    {
        auto *row = new QHBoxLayout(m_noCamBox);
        row->setContentsMargins(0, 0, 0, 0);
        auto *pic = new QLabel;
        pic->setPixmap(Icons::get(QStringLiteral("barcode")).pixmap(64, 64));
        pic->setAlignment(Qt::AlignTop);
        row->addWidget(pic);
        auto *txt = new QLabel(tr("Fotografieren Sie den Strichcode mit dem Handy oder einer Kamera und öffnen Sie das Foto hier. "
                                  "Am besten gelingt es, wenn der Code möglichst groß, scharf und ohne Spiegelung im Bild ist; "
                                  "schräg oder auf dem Kopf stehend ist kein Problem."));
        txt->setWordWrap(true);
        txt->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        row->addWidget(txt, 1, Qt::AlignTop);
    }
    lay->addWidget(m_noCamBox);
#ifdef VIVIA_CAMERA
    m_camBox = new QWidget;
    auto *camLay = new QVBoxLayout(m_camBox);
    camLay->setContentsMargins(0, 0, 0, 0);
    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Kamera:")));
    m_cameras = new QComboBox;
    m_cameras->setToolTip(tr("Alle angeschlossenen Kameras. Eine USB-Webcam erscheint hier, sobald sie eingesteckt ist."));
    top->addWidget(m_cameras, 1);
    camLay->addLayout(top);
    m_view = new QVideoWidget;
    m_view->setMinimumSize(480, 300);
    camLay->addWidget(m_view, 1);
    lay->addWidget(m_camBox, 1);
    m_session = new QMediaCaptureSession(this);
    m_session->setVideoOutput(m_view);
    connect(m_view->videoSink(), &QVideoSink::videoFrameChanged, this, &BarcodeScanDialog::onFrame);
    connect(m_cameras, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
        if (i < 0) return;
        const QByteArray id = m_cameras->itemData(i).toByteArray();
        QSettings s;
        s.setValue(QStringLiteral("barcode/cameraId"), id);
        s.setValue(QStringLiteral("barcode/camera"), m_cameras->itemText(i));
        startCamera(id);
    });
    // Kameras ein- und ausstecken, während der Dialog offen ist
    m_devices = new QMediaDevices(this);
    connect(m_devices, &QMediaDevices::videoInputsChanged, this, [this]() { refreshCameras(true); });
#endif
    m_status = new QLabel;
    m_status->setWordWrap(true);
    lay->addWidget(m_status);
    m_stretch = new QWidget;   // füllt den Platz, wenn keine Vorschau da ist
    m_stretch->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    lay->addWidget(m_stretch, 1);
    auto *bl = new QHBoxLayout;
    auto *photo = new QPushButton(Icons::get(QStringLiteral("image-open")), tr("&Foto öffnen …"));
    photo->setToolTip(tr("Ein Foto oder einen Scan des Strichcodes auswerten (JPEG, PNG …)"));
    bl->addWidget(photo);
    bl->addStretch(1);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Cancel);
    bl->addWidget(bb);
    lay->addLayout(bl);
    connect(photo, &QPushButton::clicked, this, &BarcodeScanDialog::openPhoto);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
#ifdef VIVIA_CAMERA
    refreshCameras(false);
    resize(m_cameras->count() ? QSize(720, 600) : QSize(520, 180));
#else
    m_stretch->show();
    m_status->setText(tr("Diese ViviaMeals-Fassung wurde ohne Kameraunterstützung (Qt Multimedia) gebaut. "
                         "Die Webcam steht deshalb nicht zur Verfügung."));
    resize(520, 180);
#endif
}

BarcodeScanDialog::~BarcodeScanDialog()
{
#ifdef VIVIA_CAMERA
    stopCamera();
#endif
}

#ifdef VIVIA_CAMERA
void BarcodeScanDialog::refreshCameras(bool changed)
{
    const QList<QCameraDevice> devices = QMediaDevices::videoInputs();
    QStringList names;
    QList<QByteArray> ids;
    for (const QCameraDevice &d : devices) {
        names << d.description();
        ids << d.id();
    }
    // Wunschkamera: die laufende, sonst die zuletzt gewählte (Kennung, ältere Fassungen: Name),
    // sonst die Standardkamera des Systems, sonst die erste
    const QByteArray running = m_camera ? m_camera->cameraDevice().id() : QByteArray();
    QSettings s;
    const QByteArray savedId = s.value(QStringLiteral("barcode/cameraId")).toByteArray();
    const QString savedName = s.value(QStringLiteral("barcode/camera")).toString();
    const int pick = Barcode::pickCamera(ids, names, running, savedId, savedName, QMediaDevices::defaultVideoInput().id());

    const QStringList labels = Barcode::uniqueLabels(names);
    m_cameras->blockSignals(true);
    m_cameras->clear();
    for (int i = 0; i < devices.size(); ++i) m_cameras->addItem(labels.at(i), ids.at(i));
    m_cameras->setCurrentIndex(pick);
    m_cameras->blockSignals(false);

    const bool have = !devices.isEmpty();
    m_camBox->setVisible(have);
    m_noCamBox->setVisible(!have);
    m_stretch->setVisible(!have);
    if (!have) {
        stopCamera();
        m_status->setText(changed ? tr("Die Kamera wurde getrennt. Sie können ein Foto des Strichcodes öffnen oder eine Kamera anschließen.")
                                  : tr("Es wurde keine Kamera gefunden. Eine USB-Webcam können Sie auch jetzt noch anschließen."));
        return;
    }
    const QByteArray want = ids.at(pick);
    const bool lostRunning = !running.isEmpty() && running != want;
    if (want != running) startCamera(want);
    if (changed && (width() < 600 || height() < 400)) resize(std::max(width(), 720), std::max(height(), 600));
    QString text = tr("Halten Sie den Strichcode ruhig und formatfüllend vor die Kamera – waagerecht oder senkrecht, "
                      "gut beleuchtet und ohne Spiegelung. Erkannte Codes werden automatisch übernommen.");
    if (lostRunning)
        text = tr("Die bisherige Kamera wurde getrennt – jetzt: %1. ").arg(labels.at(pick)) + text;
    else if (changed)
        text = tr("Kameraliste aktualisiert (%n Kamera(s)). ", nullptr, int(devices.size())) + text;
    m_status->setText(text);
}

void BarcodeScanDialog::startCamera(const QByteArray &id)
{
    stopCamera();
    m_candidate.clear();
    const auto devices = QMediaDevices::videoInputs();
    for (const QCameraDevice &d : devices) {
        if (d.id() != id) continue;
        m_camera = new QCamera(d, this);
        connect(m_camera, &QCamera::errorOccurred, this, [this](QCamera::Error, const QString &msg) {
            m_status->setText(tr("Die Kamera meldet einen Fehler: %1").arg(msg));
        });
        m_session->setCamera(m_camera);
        m_camera->start();
        m_lastDecode.start();
        return;
    }
}

void BarcodeScanDialog::stopCamera()
{
    if (!m_camera) return;
    m_camera->stop();
    m_session->setCamera(nullptr);
    delete m_camera;
    m_camera = nullptr;
}

void BarcodeScanDialog::onFrame(const QVideoFrame &frame)
{
    // nicht jedes Bild auswerten: etwa fünfmal je Sekunde reicht und hält die Vorschau flüssig
    if (m_busy || !m_code.isEmpty() || !frame.isValid() || m_lastDecode.elapsed() < 180) return;
    m_busy = true;
    m_lastDecode.restart();
    QImage img = frame.toImage();
    if (img.width() > 1280) img = img.scaledToWidth(1280, Qt::FastTransformation);
    const Barcode::Result r = Barcode::decode(img, 32);
    m_busy = false;
    if (!r.ok()) return;
    // sicher: viele Zeilen einig, oder dasselbe Ergebnis in zwei Bildern nacheinander
    if (r.votes >= 3 || r.code == m_candidate) {
        finish(r.code);
        return;
    }
    m_candidate = r.code;
    m_status->setText(tr("Erkannt: %1 – wird bestätigt …").arg(r.code));
}
#endif

void BarcodeScanDialog::finish(const QString &code)
{
    m_code = code;
    QApplication::beep();
#ifdef VIVIA_CAMERA
    stopCamera();
#endif
    accept();
}

QString BarcodeScanDialog::codeFromPhoto(QWidget *parent, QString *error)
{
    QSettings s;
    const QString dir = s.value(QStringLiteral("barcode/photoDir"), QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
    const QString file = QFileDialog::getOpenFileName(parent, tr("Foto des Strichcodes öffnen"), dir,
                                                      tr("Bilder (*.jpg *.jpeg *.png *.webp *.bmp *.gif *.tif *.tiff);;Alle Dateien (*)"));
    if (file.isEmpty()) return {};
    s.setValue(QStringLiteral("barcode/photoDir"), QFileInfo(file).absolutePath());
    QImageReader reader(file);
    reader.setAutoTransform(true);   // Handyfotos: EXIF-Drehung beachten
    const QImage img = reader.read();
    if (img.isNull()) {
        if (error) *error = tr("Das Bild kann nicht gelesen werden: %1").arg(reader.errorString());
        return {};
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const Barcode::Result r = Barcode::decode(img, 64);
    QApplication::restoreOverrideCursor();
    if (!r.ok() && error)
        *error = tr("Im Foto wurde kein EAN-Strichcode erkannt. Tipp: Strichcode möglichst groß, scharf und gerade aufnehmen, "
                    "ohne Spiegelung – oder die Ziffern unter dem Strichcode eintippen.");
    return r.code;
}

void BarcodeScanDialog::openPhoto()
{
    QString err;
    const QString code = codeFromPhoto(this, &err);
    if (!code.isEmpty()) finish(code);
    else if (!err.isEmpty()) m_status->setText(err);
}
