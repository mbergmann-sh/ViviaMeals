// SPDX-License-Identifier: MIT
#include "settingsdialog.h"
#include "icons.h"
#include "themes.h"
#include "uiscale.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
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
    m_translate = new QCheckBox(tr("Englische Rezepte: Zutaten übersetzen (Glossar), Maße und Temperaturen umrechnen"));
    m_translate->setChecked(s.value(QStringLiteral("import/translate"), true).toBool());
    il->addWidget(m_translate);
    m_autoTags = new QCheckBox(tr("Tags „vegan“, „vegetarisch“, „fleischgerichte“, „fisch“ aus den Zutaten ergänzen"));
    m_autoTags->setChecked(s.value(QStringLiteral("import/autoTags"), true).toBool());
    il->addWidget(m_autoTags);
    lay->addWidget(imp);

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
