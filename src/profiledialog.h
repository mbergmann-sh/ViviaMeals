// VitaMeals – Personenprofil bearbeiten (Körperdaten, Ziel, Vorlieben)
// SPDX-License-Identifier: MIT AND LicenseRef-Commons-Clause-1.0 (siehe LICENSE)
#pragma once

#include "profiles.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;

class ProfileDialog : public QDialog
{
    Q_OBJECT
public:
    ProfileDialog(const Profile &p, QWidget *parent = nullptr);
    Profile profile() const;

protected:
    void accept() override;

private:
    Profile m_p;
    QLineEdit *m_name;
    QComboBox *m_sex;
    QSpinBox *m_year;
    QDoubleSpinBox *m_height;
    QDoubleSpinBox *m_weight;
    QComboBox *m_pal;
    QComboBox *m_goal;
    QDoubleSpinBox *m_target;
    QDoubleSpinBox *m_rate;
    QComboBox *m_diet;
    QLineEdit *m_excluded;
    QLineEdit *m_liked;
    QCheckBox *m_snack;
};
