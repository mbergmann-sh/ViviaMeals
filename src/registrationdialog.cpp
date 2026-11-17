// ViviaMeals – Registrierungsdialog und Sperre der Komfortfunktionen (siehe registrationdialog.h)
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "registrationdialog.h"

#include "icons.h"
#include "license.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace Registration {

QString comfortFeatures()
{
    return QObject::tr("Rezepte scannen, Rezepte aus Bildern und Fotos erkennen (Texterkennung), "
                       "Export als Website, als LaTeX-Datei und als LibreOffice-Writer-Datei");
}

QString statusSuffix()
{
    return License::isRegistered() ? QString() : QObject::tr(" – Komfortfunktion (Registrierung erforderlich)");
}

bool require(QWidget *parent, const QString &feature)
{
    if (License::isRegistered()) return true;
    QMessageBox box(parent);
    box.setWindowTitle(QObject::tr("Komfortfunktion – Registrierung erforderlich"));
    box.setIconPixmap(Icons::get(QStringLiteral("lock")).pixmap(48, 48));
    box.setText(QObject::tr("<b>%1</b> ist eine Komfortfunktion und steht nach der Registrierung zur Verfügung.").arg(feature.toHtmlEscaped()));
    box.setInformativeText(QObject::tr("Freigeschaltet werden: %1.<br/><br/>Alle übrigen Funktionen – Rezeptverwaltung, Import, Nährwerte, "
                                       "Kühlschrank, Wochenpläne, Assistenten, Tagebuch und Drucken – bleiben ohne Einschränkung nutzbar.")
                               .arg(comfortFeatures().toHtmlEscaped()));
    QPushButton *reg = box.addButton(QObject::tr("Registrierung eingeben …"), QMessageBox::AcceptRole);
    reg->setIcon(Icons::get(QStringLiteral("key")));
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(reg);
    box.exec();
    if (box.clickedButton() != reg) return false;
    RegistrationDialog d(parent);
    d.exec();
    return License::isRegistered();
}

}   // namespace Registration

RegistrationDialog::RegistrationDialog(QWidget *parent, const QString &reason) : QDialog(parent)
{
    setWindowTitle(tr("Registrierung"));
    setWindowIcon(Icons::get(QStringLiteral("key")));
    auto *lay = new QVBoxLayout(this);

    auto *top = new QHBoxLayout;
    auto *icon = new QLabel;
    icon->setPixmap(Icons::get(QStringLiteral("key")).pixmap(48, 48));
    icon->setAlignment(Qt::AlignTop);
    top->addWidget(icon);
    auto *intro = new QLabel(
        (reason.isEmpty() ? QString() : QStringLiteral("<p><b>%1</b></p>").arg(reason.toHtmlEscaped()))
        + tr("<p>Mit einem Registrierungsschlüssel werden die <b>Komfortfunktionen</b> freigeschaltet: %1.</p>"
             "<p>Alle anderen Funktionen sind ohne Registrierung uneingeschränkt nutzbar. Der Schlüssel gehört zu Ihrer "
             "E-Mail-Adresse; geben Sie beides genau so ein, wie Sie es erhalten haben. Groß- und Kleinschreibung, "
             "Leerzeichen und Zeilenumbrüche im Schlüssel spielen keine Rolle.</p>")
              .arg(Registration::comfortFeatures().toHtmlEscaped()));
    intro->setWordWrap(true);
    intro->setTextFormat(Qt::RichText);
    top->addWidget(intro, 1);
    lay->addLayout(top);

    m_state = new QLabel;
    m_state->setWordWrap(true);
    m_state->setTextFormat(Qt::RichText);
    m_state->setFrameShape(QFrame::StyledPanel);
    m_state->setMargin(8);
    lay->addWidget(m_state);

    auto *form = new QFormLayout;
    m_email = new QLineEdit;
    m_email->setPlaceholderText(tr("name@beispiel.de"));
    m_email->setClearButtonEnabled(true);
    form->addRow(tr("&E-Mail-Adresse:"), m_email);
    m_key = new QPlainTextEdit;
    m_key->setPlaceholderText(tr("VM1-…"));
    m_key->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_key->setTabChangesFocus(true);
    m_key->setFixedHeight(m_key->fontMetrics().lineSpacing() * 4 + 16);
    form->addRow(tr("&Schlüssel:"), m_key);
    m_check = new QLabel;
    m_check->setWordWrap(true);
    m_check->setTextFormat(Qt::RichText);
    form->addRow(QString(), m_check);
    lay->addLayout(form);

    auto *bb = new QDialogButtonBox;
    QPushButton *paste = bb->addButton(tr("Aus &Zwischenablage einfügen"), QDialogButtonBox::ActionRole);
    paste->setIcon(Icons::get(QStringLiteral("copy")));
    paste->setToolTip(tr("Übernimmt E-Mail-Adresse und Schlüssel aus einem kopierten Registrierungsschreiben"));
    m_remove = bb->addButton(tr("Registrierung &entfernen"), QDialogButtonBox::ResetRole);
    m_remove->setIcon(Icons::get(QStringLiteral("delete")));
    m_ok = bb->addButton(tr("&Registrieren"), QDialogButtonBox::AcceptRole);
    m_ok->setIcon(Icons::get(QStringLiteral("check")));
    m_ok->setDefault(true);
    bb->addButton(QDialogButtonBox::Close);
    lay->addWidget(bb);

    connect(paste, &QPushButton::clicked, this, &RegistrationDialog::pasteFromClipboard);
    connect(m_remove, &QPushButton::clicked, this, &RegistrationDialog::removeRegistration);
    connect(m_ok, &QPushButton::clicked, this, &RegistrationDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_email, &QLineEdit::textChanged, this, &RegistrationDialog::validate);
    connect(m_key, &QPlainTextEdit::textChanged, this, &RegistrationDialog::validate);

    const License::Info cur = License::current();
    if (cur.valid && cur.email != QLatin1String("demo")) m_email->setText(cur.email);
    showState();
    validate();
    resize(qMax(sizeHint().width(), 560), sizeHint().height());
}

void RegistrationDialog::setInput(const QString &email, const QString &key)
{
    m_email->setText(email);
    m_key->setPlainText(key);
}

void RegistrationDialog::showState()
{
    const License::Info cur = License::current();
    QString html;
    if (cur.valid && cur.email == QLatin1String("demo"))
        html = tr("✓ <b>Demo-Modus</b> – alle Komfortfunktionen sind zum Ausprobieren freigeschaltet.");
    else if (cur.valid)
        html = tr("✓ <b>Registriert</b> für %1 (Nr. %2, ausgestellt am %3). Alle Komfortfunktionen sind freigeschaltet.")
                   .arg(cur.email.toHtmlEscaped())
                   .arg(cur.serial)
                   .arg(QLocale().toString(cur.issued, QLocale::ShortFormat));
    else if (!License::hasPublicKey())
        html = tr("<b>Dieser Programmstand enthält noch keinen Prüfschlüssel</b> und kann deshalb keine Registrierung annehmen. "
                  "Der Prüfschlüssel wird mit dem Registrierungsprogramm (tools/keygen) erzeugt und in "
                  "<tt>src/license_pubkey.h</tt> geschrieben; danach ViviaMeals einmal neu bauen.");
    else
        html = tr("<b>Nicht registriert.</b> Die Komfortfunktionen sind gesperrt.");
    m_state->setText(html);
    m_remove->setEnabled(cur.valid && cur.email != QLatin1String("demo"));
}

void RegistrationDialog::validate()
{
    const QString email = m_email->text().trimmed();
    const QString key = m_key->toPlainText().trimmed();
    if (email.isEmpty() || key.isEmpty()) {
        m_check->setText(QString());
        m_ok->setEnabled(false);
        return;
    }
    const License::Info i = License::check(email, key);
    if (i.valid)
        m_check->setText(tr("<span style=\"color:#2e9d4f\">✓ Der Schlüssel passt zur E-Mail-Adresse (Nr. %1).</span>").arg(i.serial));
    else
        m_check->setText(QStringLiteral("<span style=\"color:#c0392b\">✗ %1</span>").arg(i.error.toHtmlEscaped()));
    m_ok->setEnabled(i.valid);
}

void RegistrationDialog::pasteFromClipboard()
{
    QString email, key;
    const QString text = QApplication::clipboard()->text();
    if (License::parsePasted(text, &email, &key)) {
        if (!email.isEmpty()) m_email->setText(email);
        m_key->setPlainText(key);
    } else {
        QMessageBox::information(this, tr("Einfügen"),
                                 tr("In der Zwischenablage wurde kein Registrierungsschlüssel gefunden. Kopieren Sie das ganze "
                                    "Registrierungsschreiben oder nur den Schlüssel (er beginnt mit „VM1-“)."));
    }
}

void RegistrationDialog::accept()
{
    const QString email = m_email->text().trimmed();
    const QString key = m_key->toPlainText().trimmed();
    const License::Info i = License::check(email, key);
    if (!i.valid) {
        QMessageBox::warning(this, tr("Registrierung"), i.error);
        return;
    }
    if (!License::store(email, key)) {
        QMessageBox::warning(this, tr("Registrierung"), tr("Die Registrierung konnte nicht gespeichert werden."));
        return;
    }
    QMessageBox::information(this, tr("Registrierung"),
                             tr("Vielen Dank! ViviaMeals ist für %1 registriert; die Komfortfunktionen sind ab sofort freigeschaltet.")
                                 .arg(i.email));
    QDialog::accept();
}

void RegistrationDialog::removeRegistration()
{
    if (QMessageBox::question(this, tr("Registrierung entfernen"),
                              tr("Die Registrierung auf diesem Rechner entfernen? Die Komfortfunktionen sind danach wieder gesperrt; "
                                 "mit demselben Schlüssel lässt sich ViviaMeals jederzeit erneut registrieren."))
        != QMessageBox::Yes)
        return;
    License::remove();
    m_key->clear();
    showState();
    validate();
}
