// ViviaMeals Mobil – Plattform-Hilfen (Android über JNI, sonst Ersatz für die Desktop-Probe)
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QString>

namespace Platform {
void setKeepScreenOn(bool on);                            // Bildschirm beim Kochen anlassen
bool shareText(const QString &title, const QString &text); // Android: „Teilen“-Auswahl; sonst Zwischenablage (false)
bool isAndroid();
}
