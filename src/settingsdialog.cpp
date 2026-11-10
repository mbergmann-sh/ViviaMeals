// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "settingsdialog.h"
#include "deepl.h"
#include "icons.h"
#include "themes.h"
#include "uiscale.h"
#include "ocr.h"
#include "scanner.h"

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
#include <QDir>
#include <QFileDialog>
#include <QTabWidget>
#include <QTimer>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Einstellungen"));
    QSettings s;
    auto *outer = new QVBoxLayout(this);
    auto *tabs = new QTabWidget;
    outer->addWidget(tabs);
    auto *generalPage = new QWidget;
    auto *lay = new QVBoxLayout(generalPage);
    auto *importPage = new QWidget;
    auto *ilay = new QVBoxLayout(importPage);
    auto *scanPage = new QWidget;
    auto *slay = new QVBoxLayout(scanPage);
    tabs->addTab(generalPage, Icons::get(QStringLiteral("settings")), tr("Allgemein"));
    tabs->addTab(importPage, Icons::get(QStringLiteral("import")), tr("Import und Übersetzung"));
    tabs->addTab(scanPage, Icons::get(QStringLiteral("scan")), tr("Scannen"));
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
    m_mealTags = new QCheckBox(tr("Mahlzeit-Tags ergänzen (Frühstück, Hauptgericht, Zwischenmahlzeit, Dessert, Süßspeise, Backen)"));
    m_mealTags->setToolTip(tr("Aus Kategorien, Titel, Zutaten und Nährwerten geschätzt – der Wochenplan verplant danach"));
    m_mealTags->setChecked(s.value(QStringLiteral("import/mealTags"), true).toBool());
    il->addWidget(m_mealTags);
    m_nutrientTags = new QCheckBox(tr("Nährwert-Tags vergeben (eiweißreich, kalorienarm, fettarm, hochkalorisch, vitaminreich, reduktionsdiät)"));
    m_nutrientTags->setToolTip(tr("Aus den berechneten Nährwerten je Portion bzw. 100 g; „vitaminreich“ nach der Menge an Gemüse, Obst und Kräutern"));
    m_nutrientTags->setChecked(s.value(QStringLiteral("import/nutrientTags"), true).toBool());
    il->addWidget(m_nutrientTags);
    ilay->addWidget(imp);

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
    ilay->addWidget(dg);
    ilay->addStretch(1);
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
    lay->addStretch(1);

    // ---- Scannen und Texterkennung
    auto *sg = new QGroupBox(tr("Scanner"));
    auto *sf = new QFormLayout(sg);
    m_scanner = new QComboBox;
    for (const Scanner::Program &p : Scanner::known())
        m_scanner->addItem(Scanner::installed(p) || p.id == QLatin1String("custom") ? p.name : tr("%1 (nicht installiert)").arg(p.name), p.id);
    m_scanner->setCurrentIndex(qMax(0, m_scanner->findData(s.value(QStringLiteral("scan/program"), Scanner::defaultId()).toString())));
    sf->addRow(tr("Scannen mit:"), m_scanner);
    m_scanCommand = new QLineEdit(s.value(QStringLiteral("scan/command")).toString());
    m_scanCommand->setPlaceholderText(tr("z. B. naps2.console -o %f   (%f = Zieldatei; ohne %f: Bild danach selbst öffnen)"));
    sf->addRow(tr("Eigener Befehl:"), m_scanCommand);
    m_scanColor = new QCheckBox(tr("In Farbe scannen (nötig, um Rezeptfotos zu übernehmen)"));
    m_scanColor->setChecked(s.value(QStringLiteral("scan/color"), true).toBool());
    sf->addRow(QString(), m_scanColor);
#if defined(Q_OS_WIN)
    const QString scanInfo = tr("Der Windows-Scandialog funktioniert mit jedem Scanner oder Multifunktionsgerät, für das ein "
                                "WIA-Treiber installiert ist (bei Windows 10/11 meist automatisch).");
#elif defined(Q_OS_MACOS)
    const QString scanInfo = tr("Unter macOS wird „Digitale Bilder“ oder „Vorschau“ geöffnet. Das gespeicherte Bild danach im "
                                "Scandialog mit „Bild öffnen …“ laden.");
#else
    const QString scanInfo = tr("XSane und scanimage legen das Bild direkt für ViviaMeals ab. Bei den anderen Programmen das Bild "
                                "speichern und im Scandialog mit „Bild öffnen …“ laden. Installation z. B. „sudo apt install xsane“.");
#endif
    auto *si = new QLabel(QStringLiteral("<small>%1</small>").arg(scanInfo.toHtmlEscaped()));
    si->setWordWrap(true);
    sf->addRow(si);
    slay->addWidget(sg);
    auto *og = new QGroupBox(tr("Texterkennung (Tesseract)"));
    auto *of = new QFormLayout(og);
    auto *tl = new QHBoxLayout;
    m_tesseract = new QLineEdit(s.value(QStringLiteral("scan/tesseract")).toString());
    m_tesseract->setPlaceholderText(tr("automatisch suchen"));
    tl->addWidget(m_tesseract, 1);
    auto *browse = new QPushButton(Icons::get(QStringLiteral("open")), tr("Wählen …"));
    tl->addWidget(browse);
    auto *check = new QPushButton(Icons::get(QStringLiteral("refresh")), tr("Prüfen"));
    tl->addWidget(check);
    of->addRow(tr("Programm:"), tl);
    m_ocrLangs = new QLineEdit(s.value(QStringLiteral("scan/languages")).toString());
    m_ocrLangs->setPlaceholderText(tr("automatisch (deu+eng)"));
    m_ocrLangs->setToolTip(tr("Sprachen der Texterkennung, mit + verbunden, z. B. deu+eng+fra"));
    of->addRow(tr("Sprachen:"), m_ocrLangs);
    m_ocrStatus = new QLabel;
    m_ocrStatus->setWordWrap(true);
    m_ocrStatus->setOpenExternalLinks(true);
    of->addRow(m_ocrStatus);
    auto *oi = new QLabel(tr("<small>Tesseract (Apache-2.0) ist ein freies Texterkennungsprogramm: "
                             "<a href=\"https://github.com/tesseract-ocr/tesseract\">github.com/tesseract-ocr/tesseract</a>. "
                             "Die Erkennung läuft vollständig auf diesem Rechner.</small>"));
    oi->setWordWrap(true);
    oi->setOpenExternalLinks(true);
    of->addRow(oi);
    slay->addWidget(og);
    slay->addStretch(1);
    auto updateScan = [this] { m_scanCommand->setEnabled(m_scanner->currentData().toString() == QLatin1String("custom")); };
    connect(m_scanner, qOverload<int>(&QComboBox::currentIndexChanged), this, updateScan);
    updateScan();
    connect(browse, &QPushButton::clicked, this, [this] {
#ifdef Q_OS_WIN
        const QString filter = tr("Programme (*.exe)");
#else
        const QString filter;
#endif
        const QString f = QFileDialog::getOpenFileName(this, tr("Tesseract wählen"), m_tesseract->text(), filter);
        if (!f.isEmpty()) {
            m_tesseract->setText(QDir::toNativeSeparators(f));
            checkTesseract();
        }
    });
    connect(check, &QPushButton::clicked, this, &SettingsDialog::checkTesseract);
    QTimer::singleShot(0, this, &SettingsDialog::checkTesseract);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    outer->addWidget(bb);
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

void SettingsDialog::checkTesseract()
{
    const QString exe = Ocr::findTesseract(m_tesseract->text());
    if (exe.isEmpty()) {
        m_ocrStatus->setText(QStringLiteral("✗ ") + Ocr::installHint().toHtmlEscaped());
        return;
    }
    const QStringList langs = Ocr::languages(exe);
    QString text = tr("✓ Tesseract %1 gefunden: %2<br>Installierte Sprachen: %3")
                       .arg(Ocr::version(exe), QDir::toNativeSeparators(exe).toHtmlEscaped(), langs.join(QStringLiteral(", ")));
    if (!langs.contains(QStringLiteral("deu")))
        text += tr("<br><b>Deutsch (deu) fehlt</b> – bitte die deutschen Sprachdaten nachinstallieren.");
    m_ocrStatus->setText(text);
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
    s.setValue(QStringLiteral("import/mealTags"), m_mealTags->isChecked());
    s.setValue(QStringLiteral("import/nutrientTags"), m_nutrientTags->isChecked());
    DeepL::Settings ds;
    ds.enabled = m_deepl->isChecked();
    ds.key = m_deeplKey->text();
    ds.save();
    s.setValue(QStringLiteral("scan/program"), m_scanner->currentData().toString());
    s.setValue(QStringLiteral("scan/command"), m_scanCommand->text().trimmed());
    s.setValue(QStringLiteral("scan/color"), m_scanColor->isChecked());
    s.setValue(QStringLiteral("scan/tesseract"), m_tesseract->text().trimmed());
    s.setValue(QStringLiteral("scan/languages"), m_ocrLangs->text().trimmed());
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
