// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "uiscale.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>

namespace UiScale {

namespace {
double g_baseSize = 0;
double g_factor = 1.0;
}

int setting() { return QSettings().value(QStringLiteral("ui/fontScale"), 0).toInt(); }
void setSetting(int percent) { QSettings().setValue(QStringLiteral("ui/fontScale"), percent); }
double factor() { return g_factor; }

QList<QPair<int, QString>> choices()
{
    return { { 0, QCoreApplication::translate("UiScale", "Automatisch (nach Bildschirmgröße)") },
             { 85, QStringLiteral("85 %") }, { 90, QStringLiteral("90 %") }, { 100, QStringLiteral("100 %") },
             { 110, QStringLiteral("110 %") }, { 125, QStringLiteral("125 %") }, { 140, QStringLiteral("140 %") } };
}

void apply(QApplication &app)
{
    QFont f = app.font();
    if (g_baseSize <= 0) g_baseSize = f.pointSizeF() > 0 ? f.pointSizeF() : 10.0;
    const int s = setting();
    if (s > 0) {
        g_factor = s / 100.0;
    } else {
        g_factor = 1.0;
        if (QScreen *scr = QGuiApplication::primaryScreen()) {
            const int h = scr->availableGeometry().height();
            if (h < 700) g_factor = 0.85;
            else if (h < 800) g_factor = 0.92;
            else if (h >= 1400) g_factor = 1.1;
        }
    }
    f.setPointSizeF(g_baseSize * g_factor);
    app.setFont(f);
}

} // namespace UiScale
