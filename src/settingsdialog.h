// VitaMeals – Einstellungen: Darstellung (Theme, Schriftgröße), Import, Start
// SPDX-License-Identifier: MIT
#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

signals:
    void appearanceChanged();

protected:
    void accept() override;
    void reject() override;

private:
    QComboBox *m_theme;
    QComboBox *m_font;
    QCheckBox *m_translate;
    QCheckBox *m_autoTags;
    QCheckBox *m_notice;
    QCheckBox *m_random;
    QString m_origTheme;
};
