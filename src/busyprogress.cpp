// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "busyprogress.h"

#include <QApplication>
#include <QProgressDialog>

BusyProgress::BusyProgress(QWidget *parent, const QString &title, int maximum, bool cancellable)
    : m_dlg(new QProgressDialog(parent))
{
    m_dlg->setWindowTitle(title);
    m_dlg->setLabelText(title + QStringLiteral(" …"));
    m_dlg->setRange(0, qMax(0, maximum));
    m_dlg->setWindowModality(Qt::WindowModal);
    m_dlg->setMinimumDuration(0);
    m_dlg->setAutoClose(false);
    m_dlg->setAutoReset(false);
    m_dlg->setMinimumWidth(420);
    if (cancellable) m_dlg->setCancelButtonText(QApplication::translate("BusyProgress", "Abbrechen"));
    else m_dlg->setCancelButton(nullptr);
    m_dlg->setValue(0);
    m_dlg->show();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    m_timer.start();
}

BusyProgress::~BusyProgress()
{
    m_dlg->close();
    delete m_dlg;
}

void BusyProgress::setMaximum(int maximum)
{
    m_dlg->setMaximum(qMax(0, maximum));
}

bool BusyProgress::canceled() const
{
    return m_dlg->wasCanceled();
}

bool BusyProgress::step(int value, const QString &label)
{
    if (m_timer.elapsed() >= 40 || value >= m_dlg->maximum() || m_dlg->value() <= 0) {
        m_timer.restart();
        if (!label.isEmpty() && label != m_label) {
            m_label = label;
            m_dlg->setLabelText(label);
        }
        m_dlg->setValue(qMin(value, m_dlg->maximum()));
        QApplication::processEvents();
    }
    return !m_dlg->wasCanceled();
}

std::function<bool(int, int, const QString &)> BusyProgress::callback()
{
    return [this](int done, int total, const QString &label) {
        if (total != m_dlg->maximum()) setMaximum(total);
        return step(done, label);
    };
}
