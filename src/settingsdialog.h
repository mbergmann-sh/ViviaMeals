// ViviaMeals – Einstellungen: Darstellung (Theme, Schriftgröße), Import, Übersetzung (DeepL), Start, Scannen und Texterkennung
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
namespace Escl { class Scanner; }

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

signals:
    void appearanceChanged();

private slots:
    void testDeepL();
    void checkTesseract();

protected:
    void accept() override;
    void reject() override;

private:
    QComboBox *m_theme;
    QComboBox *m_font;
    QCheckBox *m_translate;
    QCheckBox *m_autoTags;
    QCheckBox *m_umlauts;
    QCheckBox *m_mealTags;
    QCheckBox *m_nutrientTags;
    QCheckBox *m_offerFoods;
    QCheckBox *m_deepl;
    QLineEdit *m_deeplKey;
    QLabel *m_deeplStatus;
    QCheckBox *m_notice;
    QCheckBox *m_random;
    QString m_origTheme;
    QComboBox *m_scanner;
    QLineEdit *m_esclAddress;
    QLabel *m_esclStatus;
    QLineEdit *m_watchDir;
    QLineEdit *m_watchProgram;
    Escl::Scanner *m_escl;
    QLineEdit *m_scanCommand;
    QCheckBox *m_scanColor;
    QLineEdit *m_tesseract;
    QLineEdit *m_ocrLangs;
    QLabel *m_ocrStatus;
};
