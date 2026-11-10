// ViviaMeals – Assistent für Gewichtsverlust und Gewichtszunahme
//
// Personen wählen/anlegen, Kennzahlen (BMI, Grund- und Gesamtumsatz, Zielzufuhr, voraussichtliche
// Dauer bis zum Zielgewicht), Gewichtsverlauf mit Grafik und Start des Wochenplans.
// KEIN Medizinprodukt – die Werte sind allgemeine Richtwerte.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QDialog>

class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTextBrowser;
class WeightChart;

class AssistantDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AssistantDialog(QWidget *parent = nullptr);

signals:
    void planRequested(const QString &profileId);

private:
    void reloadProfiles(const QString &select = QString());
    void showProfile();
    QString currentId() const;
    QComboBox *m_profiles;
    QTextBrowser *m_summary;
    QTextBrowser *m_tips;
    QWidget *m_reminder;
    QLabel *m_reminderText;
    QTableWidget *m_weights;
    WeightChart *m_chart;
    QDateEdit *m_date;
    QDoubleSpinBox *m_kg;
    QLineEdit *m_note;
    QPushButton *m_edit, *m_del, *m_plan;
};
