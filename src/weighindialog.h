// VitaMeals – Gewicht schnell eintragen (Planen → Gewichtskontrolle)
//
// Datum und Person wählen, Gewicht eingeben, „Eintragen“: das Gewicht kommt in den Gewichtsverlauf der
// Person (ein Eintrag desselben Tages wird ersetzt). Zeigt den letzten Eintrag, die Veränderung und den
// BMI. Ein Eintrag am oder nach dem Wiegetag erledigt die Wiegetag-Erinnerung.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QDialog>

class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;

class WeighInDialog : public QDialog
{
    Q_OBJECT
public:
    explicit WeighInDialog(const QString &profileId = QString(), QWidget *parent = nullptr);
    QString profileId() const;
    static bool hasProfiles();

protected:
    void accept() override;

private:
    void profileChanged();
    void updateInfo();
    QComboBox *m_profile;
    QDateEdit *m_date;
    QDoubleSpinBox *m_kg;
    QLineEdit *m_note;
    QLabel *m_info;
    QPushButton *m_ok;
};
