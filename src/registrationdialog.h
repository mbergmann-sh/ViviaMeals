// ViviaMeals – Hilfe → Registrierung eingeben …
// Nimmt E-Mail-Adresse und Registrierungsschlüssel entgegen (prüft schon beim Tippen), zeigt den Stand
// und entfernt eine Registrierung. Registration::require() sperrt die Komfortfunktionen ohne gültigen
// Schlüssel und bietet den Weg zur Registrierung an. Prüfung und Schlüsselformat: license.h.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class RegistrationDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RegistrationDialog(QWidget *parent = nullptr, const QString &reason = QString());

    void setInput(const QString &email, const QString &key);   // für Bildschirmfotos

    void accept() override;

private:
    void validate();
    void pasteFromClipboard();
    void removeRegistration();
    void showState();

    QLabel *m_state = nullptr;
    QLineEdit *m_email = nullptr;
    QPlainTextEdit *m_key = nullptr;
    QLabel *m_check = nullptr;
    QPushButton *m_ok = nullptr;
    QPushButton *m_remove = nullptr;
};

namespace Registration {
// Liste der Komfortfunktionen für Texte und Handbuch
QString comfortFeatures();
// true, wenn registriert; sonst Hinweis „Komfortfunktion – Registrierung erforderlich“ mit Weg zum
// Registrierungsdialog (true, wenn danach registriert)
bool require(QWidget *parent, const QString &feature);
// Statuszeilen-Zusatz für gesperrte Aktionen
QString statusSuffix();
}
