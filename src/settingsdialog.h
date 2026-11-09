// VitaMeals – Einstellungen: Darstellung (Theme, Schriftgröße), Import, Übersetzung (DeepL), Start
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

signals:
    void appearanceChanged();

private slots:
    void testDeepL();

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
    QCheckBox *m_deepl;
    QLineEdit *m_deeplKey;
    QLabel *m_deeplStatus;
    QCheckBox *m_notice;
    QCheckBox *m_random;
    QString m_origTheme;
};
