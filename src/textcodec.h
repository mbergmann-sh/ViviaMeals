// VitaMeals – Zeichensatz von Rezeptdateien erkennen
//
// RezKonv- und MealMaster-Dateien stammen oft aus DOS- (CP437/CP850) oder Windows-Zeiten
// (CP1252/ISO-8859-1), neuere sind UTF-8. Erkannt wird: UTF-8 (auch mit BOM), sonst anhand der
// typischen Umlaut-Bytes CP850 oder CP1252.
// SPDX-License-Identifier: MIT
#pragma once

#include <QByteArray>
#include <QString>

namespace TextCodec {
QString decode(const QByteArray &data, QString *detected = nullptr);
QString readFile(const QString &path, QString *error = nullptr, QString *detected = nullptr);
}
