// ViviaMeals – Scannen unter Windows über WIA (Windows Image Acquisition)
//
// Nutzt den Scandialog von Windows (WIA-Automatisierung „WIA.CommonDialog“, seit Windows Vista
// Bestandteil des Systems): Scanner wählen, Vorschau, Bereich, Farbe/Graustufen, Auflösung.
// Angesprochen per COM-Spätbindung (IDispatch) – keine Typbibliothek, keine zusätzlichen
// Bibliotheken außer ole32/oleaut32. Bewusst ohne Qt, damit die Datei auch für sich übersetzt
// und geprüft werden kann.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#ifdef _WIN32

namespace WiaScan {

enum class Result { Ok, Cancelled, NoDevice, NotAvailable, Failed };

// WIA-Automatisierung vorhanden (Windows-Bestandteil, fehlt nur auf „N“-/Server-Systemen ohne
// Desktopdarstellung)?
bool available();

// Scandialog zeigen und das Bild als PNG (bzw. im Format des Treibers) unter path speichern.
// error erhält bei Failed eine Beschreibung (UTF-16, nullterminiert, höchstens errorLen Zeichen).
// colorIntent: true = Farbe (Fotos bleiben erhalten), false = Graustufen
Result acquire(const wchar_t *path, bool colorIntent, wchar_t *error, int errorLen);

} // namespace WiaScan

#endif
