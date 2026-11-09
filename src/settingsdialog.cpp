// SPDX-License-Identifier: MIT
#include "settingsdialog.h"
#include "deepl.h"
#include "icons.h"
#include "themes.h"
#include "uiscale.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Einstellungen"));
    QSettings s;
    auto *lay = new QVBoxLayout(this);
    auto *look = new QGroupBox(tr("Darstellung"));
    auto *lf = new QFormLayout(look);
    m_theme = new QComboBox;
    for (const QString &id : Themes::available()) m_theme->addItem(Themes::displayName(id), id);
    m_origTheme = Themes::current();
    m_theme->setCurrentIndex(qMax(0, m_theme->findData(m_origTheme)));
    lf->addRow(tr("Theme:"), m_theme);
    m_font = new QComboBox;
    for (const auto &c : UiScale::choices()) m_font->addItem(c.second, c.first);
    m_font->setCurrentIndex(qMax(0, m_font->findData(UiScale::setting())));
    lf->addRow(tr("Schriftgröße:"), m_font);
    lay->addWidget(look);

    auto *imp = new QGroupBox(tr("Import"));
    auto *il = new QVBoxLayout(imp);
    m_translate = new QCheckBox(tr("Englische Rezepte ohne DeepL: Zutaten über das Glossar übersetzen, Maße und Temperaturen umrechnen"));
    m_translate->setChecked(s.value(QStringLiteral("import/translate"), true).toBool());
    il->addWidget(m_translate);
    m_autoTags = new QCheckBox(tr("Tags „vegan“, „vegetarisch“, „fleischgerichte“, „fisch“ aus den Zutaten ergänzen"));
    m_autoTags->setChecked(s.value(QStringLiteral("import/autoTags"), true).toBool());
    il->addWidget(m_autoTags);
    m_umlauts = new QCheckBox(tr("Deutsche Rezepte ohne Umlaute korrigieren (Kaese → Käse, heiss → heiß)"));
    m_umlauts->setToolTip(tr("Viele ältere RezKonv- und MealMaster-Sammlungen schreiben ae, oe, ue und ss statt ä, ö, ü und ß.\n"
                             "Wörter wie „Feuer“, „Sauerbraten“, „zuerst“, „Wasser“ oder „Masse“ bleiben unverändert."));
    m_umlauts->setChecked(s.value(QStringLiteral("import/fixUmlauts"), true).toBool());
    il->addWidget(m_umlauts);
    lay->addWidget(imp);

    // ---- DeepL
    const DeepL::Settings ds = DeepL::Settings::load();
    auto *dg = new QGroupBox(tr("Übersetzung fremdsprachiger Rezepte (DeepL)"));
    auto *dl = new QVBoxLayout(dg);
    m_deepl = new QCheckBox(tr("Englische und andere fremdsprachige Rezepte beim Import mit DeepL ins Deutsche übersetzen"));
    m_deepl->setChecked(ds.enabled);
    dl->addWidget(m_deepl);
    auto *kf = new QFormLayout;
    auto *kl = new QHBoxLayout;
    m_deeplKey = new QLineEdit(ds.key);
    m_deeplKey->setEchoMode(QLineEdit::Password);
    m_deeplKey->setPlaceholderText(tr("eigener DeepL-API-Schlüssel, z. B. xxxxxxxx-xxxx-…:fx"));
    m_deeplKey->setObjectName(QStringLiteral("deeplKey"));
    kl->addWidget(m_deeplKey, 1);
    auto *show = new QPushButton(Icons::get(QStringLiteral("eye")), QString());
    show->setCheckable(true);
    show->setToolTip(tr("Schlüssel anzeigen"));
    kl->addWidget(show);
    auto *test = new QPushButton(Icons::get(QStringLiteral("refresh")), tr("Verbindung testen"));
    kl->addWidget(test);
    kf->addRow(tr("API-Schlüssel:"), kl);
    dl->addLayout(kf);
    m_deeplStatus = new QLabel;
    m_deeplStatus->setWordWrap(true);
    dl->addWidget(m_deeplStatus);
    auto *info = new QLabel(tr("<small>Den Schlüssel gibt es kostenlos mit dem Tarif <b>DeepL API Free</b> (500.000 Zeichen im Monat) unter "
                               "<a href=\"https://www.deepl.com/pro-api\">deepl.com/pro-api</a>. Er wird nur auf diesem Rechner gespeichert. "
                               "<b>Datenschutz:</b> Titel, Zutaten und Zubereitung fremdsprachiger Rezepte werden zur Übersetzung an DeepL "
                               "gesendet; deutsche Rezepte nie. Beim Tarif API Free darf DeepL die Texte zeitweise zum Verbessern seiner Übersetzungen "
                               "verwenden, bei API Pro werden sie sofort gelöscht. Ohne Schlüssel übersetzt das eingebaute Glossar die Zutaten.</small>"));
    info->setWordWrap(true);
    info->setOpenExternalLinks(true);
    dl->addWidget(info);
    lay->addWidget(dg);
    connect(show, &QPushButton::toggled, this, [this](bool on) { m_deeplKey->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password); });
    connect(test, &QPushButton::clicked, this, &SettingsDialog::testDeepL);
    auto updateEnabled = [this, test] {
        m_deeplKey->setEnabled(m_deepl->isChecked());
        test->setEnabled(m_deepl->isChecked());
    };
    connect(m_deepl, &QCheckBox::toggled, this, updateEnabled);
    updateEnabled();

    auto *start = new QGroupBox(tr("Programmstart"));
    auto *sl = new QVBoxLayout(start);
    m_random = new QCheckBox(tr("Zufälliges Rezept anzeigen"));
    m_random->setChecked(s.value(QStringLiteral("ui/randomStart"), true).toBool());
    sl->addWidget(m_random);
    m_notice = new QCheckBox(tr("Hinweis „kein Medizinprodukt“ anzeigen"));
    m_notice->setChecked(s.value(QStringLiteral("ui/startNotice"), true).toBool());
    sl->addWidget(m_notice);
    lay->addWidget(start);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    // Theme sofort zeigen
    connect(m_theme, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        Themes::apply(m_theme->currentData().toString());
        emit appearanceChanged();
    });
}

void SettingsDialog::testDeepL()
{
    const QString key = m_deeplKey->text().trimmed();
    m_deeplStatus->setText(tr("Verbindung wird geprüft …"));
    m_deeplStatus->repaint();
    DeepL::Usage u;
    QString err;
    if (DeepL::usage(key, &u, &err)) {
        const QLocale de(QLocale::German);
        m_deeplStatus->setText(tr("✓ Verbunden (%1). Diesen Monat verbraucht: %2 von %3 Zeichen.")
                                   .arg(DeepL::isFreeKey(key) ? tr("DeepL API Free") : tr("DeepL API Pro"),
                                        de.toString(u.characterCount), u.characterLimit > 0 ? de.toString(u.characterLimit) : tr("unbegrenzt")));
    } else {
        m_deeplStatus->setText(tr("✗ %1").arg(err));
    }
}

void SettingsDialog::reject()
{
    if (Themes::current() != m_origTheme) {   // Vorschau zurücknehmen
        Themes::apply(m_origTheme);
        emit appearanceChanged();
    }
    QDialog::reject();
}

void SettingsDialog::accept()
{
    QSettings s;
    s.setValue(QStringLiteral("import/translate"), m_translate->isChecked());
    s.setValue(QStringLiteral("import/autoTags"), m_autoTags->isChecked());
    s.setValue(QStringLiteral("import/fixUmlauts"), m_umlauts->isChecked());
    DeepL::Settings ds;
    ds.enabled = m_deepl->isChecked();
    ds.key = m_deeplKey->text();
    ds.save();
    s.setValue(QStringLiteral("ui/randomStart"), m_random->isChecked());
    s.setValue(QStringLiteral("ui/startNotice"), m_notice->isChecked());
    const int f = m_font->currentData().toInt();
    if (f != UiScale::setting()) {
        UiScale::setSetting(f);
        UiScale::apply(*qApp);
    }
    Themes::apply(m_theme->currentData().toString());
    emit appearanceChanged();
    QDialog::accept();
}
