// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "splash.h"
#include "icons.h"
#include "perf.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QPointer>
#include <QScreen>
#include <QSettings>
#include <QSplashScreen>

namespace Splash {

namespace {

constexpr int kW = 560, kH = 320;

class Screen : public QSplashScreen
{
public:
    explicit Screen(const QPixmap &pm) : QSplashScreen(pm) {}
    void set(const QString &text, int percent)
    {
        m_text = text;
        m_percent = qBound(0, percent, 100);
        repaint();   // sofort zeichnen – die Ereignisschleife läuft während des Ladens noch nicht
    }

protected:
    // QSplashScreen::event() wartet beim Show-Ereignis, bis das Fenster sichtbar ist – das wird es aber erst
    // nach diesem Ereignis. Unter Windows (Qt 6.x) lief das jedes Mal in die volle Wartezeit von 1 s.
    // Das Show-Ereignis deshalb direkt an QWidget geben; alles andere (Zeichnen) wie gehabt.
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::Show) return QWidget::event(e);
        return QSplashScreen::event(e);
    }

    void drawContents(QPainter *p) override
    {
        p->setRenderHint(QPainter::Antialiasing);
        const int left = 248, right = kW - 24;
        // Meldung
        QFont f = font();
        f.setPixelSize(13);
        p->setFont(f);
        p->setPen(QColor(0x44, 0x44, 0x44));
        p->drawText(QRect(left, kH - 70, right - left, 22), Qt::AlignLeft | Qt::AlignVCenter,
                    p->fontMetrics().elidedText(m_text, Qt::ElideRight, right - left));
        // Fortschrittsbalken
        const QRectF bar(left, kH - 42, right - left, 10);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0xE6, 0xEB, 0xF0));
        p->drawRoundedRect(bar, 5, 5);
        if (m_percent > 0) {
            QRectF done = bar;
            done.setWidth(qMax(10.0, bar.width() * m_percent / 100.0));
            p->setBrush(Icons::accent());
            p->drawRoundedRect(done, 5, 5);
        }
    }

private:
    QString m_text;
    int m_percent = 0;
};

QPointer<Screen> &instance()
{
    static QPointer<Screen> s;
    return s;
}

QPixmap background(qreal dpr)
{
    QPixmap pm(int(kW * dpr), int(kH * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::white);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    // links das Bild aus dem Über-Dialog
    const QImage koch(QStringLiteral(":/images/splash-koch.jpg"));
    const QRect img(0, 0, 224, kH);
    if (!koch.isNull()) {
        const QImage scaled = koch.scaled(QSize(img.width(), img.height()) * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const QRect src((scaled.width() - int(img.width() * dpr)) / 2, (scaled.height() - int(img.height() * dpr)) / 2, int(img.width() * dpr),
                        int(img.height() * dpr));
        p.drawImage(img, scaled, src);
    }
    // blaue Kante und Rahmen
    p.fillRect(QRect(224, 0, 4, kH), Icons::primary());
    p.setPen(QPen(QColor(0xC8, 0xD2, 0xDC), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(0.5, 0.5, kW - 1, kH - 1));
    // Logo, Name, Version
    const int left = 248;
    p.drawPixmap(QRect(left, 26, 56, 56), Icons::app().pixmap(QSize(56, 56) * dpr));
    QFont title = QApplication::font();
    title.setPixelSize(34);
    title.setBold(true);
    p.setFont(title);
    p.setPen(Icons::primary());
    p.drawText(QRect(left + 68, 24, kW - left - 80, 44), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("ViviaMeals"));
    QFont small = QApplication::font();
    small.setPixelSize(13);
    p.setFont(small);
    p.setPen(QColor(0x66, 0x66, 0x66));
    p.drawText(QRect(left + 70, 66, kW - left - 80, 20), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Version %1").arg(QApplication::applicationVersion()));
    p.setPen(Icons::accent());
    p.drawLine(QPointF(left, 104), QPointF(kW - 24, 104));
    QFont text = QApplication::font();
    text.setPixelSize(14);
    p.setFont(text);
    p.setPen(QColor(0x33, 0x33, 0x33));
    p.drawText(QRect(left, 118, kW - left - 24, 90), Qt::AlignLeft | Qt::TextWordWrap,
               QApplication::translate("Splash", "Rezeptverwaltung mit Nährwertanalyse, Kühlschrank-Vorschlägen, Wochen- und Menüplanung "
                                                 "und Assistenten für die Gewichtskontrolle."));
    small.setPixelSize(11);
    p.setFont(small);
    p.setPen(QColor(0x99, 0x99, 0x99));
    p.drawText(QRect(left, kH - 22, kW - left - 24, 16), Qt::AlignLeft | Qt::AlignVCenter,
               QApplication::translate("Splash", "© 2026 Michael Bergmann · kein Medizinprodukt"));
    return pm;
}

} // namespace

bool enabledInSettings() { return QSettings().value(QStringLiteral("ui/splash"), true).toBool(); }

void start(bool force)
{
    if (instance() || (!force && !enabledInSettings())) return;
    const QScreen *scr = QGuiApplication::primaryScreen();
    const qreal dpr = scr ? scr->devicePixelRatio() : 1.0;
    const QPixmap bg = background(dpr);
    Perf::mark("Startbildschirm: Bild gezeichnet");
    auto *s = new Screen(bg);
    s->setAttribute(Qt::WA_DeleteOnClose);
    instance() = s;
    Perf::mark("Startbildschirm: Objekt erzeugt");
    (void)s->winId();   // natives Fenster getrennt messen
    Perf::mark("Startbildschirm: natives Fenster");
    s->show();
    Perf::mark("Startbildschirm: Fenster erzeugt");
    s->set(QApplication::translate("Splash", "ViviaMeals wird gestartet …"), 2);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);   // Fenster einmal sichtbar machen
    Perf::mark("Startbildschirm: sichtbar");
}

void message(const QString &text, int percent)
{
    if (Screen *s = instance()) s->set(text, percent);
}

void finish(QWidget *mainWindow)
{
    if (Screen *s = instance()) {
        s->set(QApplication::translate("Splash", "Fertig"), 100);
        if (mainWindow) s->finish(mainWindow);   // wartet, bis das Hauptfenster gezeichnet ist
        Perf::mark("Startbildschirm: Hauptfenster sichtbar");
        s->close();
        instance() = nullptr;
    }
}

bool active() { return !instance().isNull(); }

QWidget *widget() { return instance(); }

} // namespace Splash
