// ViviaMeals – Strichcode mit der Webcam scannen oder aus einem Foto lesen
//
// Kamera: Qt Multimedia (Qt 6). Fehlt das Modul beim Bauen (oder unter Qt 5), bietet der Dialog nur
// „Foto öffnen …“ an. Erkannt wird mit Barcode::decode (EAN-13, EAN-8, UPC-A); ein Kamerabild gilt erst,
// wenn mehrere Abtastzeilen bzw. zwei Bilder nacheinander denselben Code mit gültiger Prüfziffer ergeben.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QDialog>
#include <QElapsedTimer>
#include <QImage>

class QComboBox;
class QLabel;
class QPushButton;
#ifdef VIVIA_CAMERA
class QCamera;
class QMediaCaptureSession;
class QVideoFrame;
class QMediaDevices;
class QVideoWidget;
#endif

class BarcodeScanDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BarcodeScanDialog(QWidget *parent = nullptr);
    ~BarcodeScanDialog() override;
    QString code() const { return m_code; }
    static bool cameraAvailable();   // Kameraunterstützung eingebaut und mindestens eine Kamera vorhanden

    // Foto wählen und auswerten (ohne Dialog); leer = abgebrochen oder nicht erkannt (*error gesetzt)
    static QString codeFromPhoto(QWidget *parent, QString *error);

private:
    void openPhoto();
    void finish(const QString &code);
#ifdef VIVIA_CAMERA
    void refreshCameras(bool changed);   // Liste neu aufbauen (beim Öffnen und beim Ein-/Ausstecken)
    void startCamera(const QByteArray &id);
    void stopCamera();
    void onFrame(const QVideoFrame &frame);
    QMediaCaptureSession *m_session = nullptr;
    QCamera *m_camera = nullptr;
    QVideoWidget *m_view = nullptr;
    QComboBox *m_cameras = nullptr;   // Text: Name (bei Gleichnamigen nummeriert), Daten: Geräte-Kennung
    QMediaDevices *m_devices = nullptr;
    QWidget *m_camBox = nullptr;
    QElapsedTimer m_lastDecode;
    QString m_candidate;   // im vorigen Bild erkannt (Bestätigung durch ein zweites Bild)
    bool m_busy = false;
#endif
    QWidget *m_noCamBox = nullptr;
    QWidget *m_stretch = nullptr;
    QLabel *m_status = nullptr;
    QString m_code;
};
