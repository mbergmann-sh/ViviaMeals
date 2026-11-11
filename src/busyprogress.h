// ViviaMeals – Fortschrittsbalken für längere Berechnungen (Nährwerte, Wochenplan …)
// Erscheint sofort (nicht erst nach einer Wartezeit), aktualisiert die Oberfläche höchstens alle 40 ms
// und lässt sich abbrechen. Verwendung:
//   BusyProgress p(this, tr("Nährwerte neu berechnen"), n);
//   for (…) { if (!p.step(i, tr("Rezept %1 von %2").arg(i + 1).arg(n))) break; … }
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#pragma once

#include <QElapsedTimer>
#include <QString>

#include <functional>

class QProgressDialog;
class QWidget;

class BusyProgress
{
public:
    BusyProgress(QWidget *parent, const QString &title, int maximum, bool cancellable = true);
    ~BusyProgress();
    // false = abgebrochen
    bool step(int value, const QString &label = QString());
    void setMaximum(int maximum);
    bool canceled() const;
    // als Rückmeldung für Kernfunktionen (z. B. MealPlanner::create)
    std::function<bool(int, int, const QString &)> callback();

private:
    QProgressDialog *m_dlg;
    QElapsedTimer m_timer;
    QString m_label;
};
