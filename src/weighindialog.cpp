// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "weighindialog.h"
#include "icons.h"
#include "profiles.h"
#include "recipe.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>
#include <cmath>

bool WeighInDialog::hasProfiles() { return !Profiles::instance().all().isEmpty(); }

WeighInDialog::WeighInDialog(const QString &profileId, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Gewichtskontrolle"));
    setWindowIcon(Icons::get(QStringLiteral("scale")));
    auto *lay = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Gewicht eintragen – am besten immer am selben Wochentag, morgens vor dem Frühstück."));
    intro->setWordWrap(true);
    lay->addWidget(intro);
    auto *form = new QFormLayout;
    m_date = new QDateEdit(QDate::currentDate());
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat(QStringLiteral("dddd, dd.MM.yyyy"));
    m_date->setMaximumDate(QDate::currentDate());
    form->addRow(tr("&Datum:"), m_date);
    m_profile = new QComboBox;
    m_profile->setMinimumWidth(240);
    for (const Profile &p : Profiles::instance().all()) m_profile->addItem(Icons::get(QStringLiteral("user")), p.name, p.id);
    form->addRow(tr("&Person:"), m_profile);
    m_kg = new QDoubleSpinBox;
    m_kg->setRange(30, 300);
    m_kg->setDecimals(1);
    m_kg->setSingleStep(0.1);
    m_kg->setSuffix(tr(" kg"));
    QFont f = m_kg->font();
    f.setPointSizeF(f.pointSizeF() * 1.4);
    f.setBold(true);
    m_kg->setFont(f);
    form->addRow(tr("&Gewicht:"), m_kg);
    m_note = new QLineEdit;
    m_note->setPlaceholderText(tr("optional, z. B. „nach dem Urlaub“"));
    form->addRow(tr("&Notiz:"), m_note);
    lay->addLayout(form);
    m_info = new QLabel;
    m_info->setWordWrap(true);
    m_info->setTextFormat(Qt::RichText);
    lay->addWidget(m_info);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_ok = bb->button(QDialogButtonBox::Ok);
    m_ok->setText(tr("Eintragen"));
    m_ok->setIcon(Icons::get(QStringLiteral("add")));
    lay->addWidget(bb);
    setMinimumWidth(460);
    connect(bb, &QDialogButtonBox::accepted, this, &WeighInDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QString select = profileId;
    if (select.isEmpty()) {
        // zuerst eine Person mit fälliger Wiegetag-Erinnerung, sonst die zuletzt gewählte
        for (const Profile &p : Profiles::instance().all())
            if (p.weighReminderDue()) {
                select = p.id;
                break;
            }
        if (select.isEmpty()) select = QSettings().value(QStringLiteral("weighIn/profile")).toString();
    }
    const int idx = m_profile->findData(select);
    m_profile->setCurrentIndex(idx >= 0 ? idx : 0);
    connect(m_profile, qOverload<int>(&QComboBox::currentIndexChanged), this, &WeighInDialog::profileChanged);
    connect(m_date, &QDateEdit::dateChanged, this, &WeighInDialog::updateInfo);
    connect(m_kg, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &WeighInDialog::updateInfo);
    profileChanged();
    m_ok->setEnabled(m_profile->count() > 0);
    m_kg->setFocus();
    m_kg->selectAll();
}

QString WeighInDialog::profileId() const { return m_profile->currentData().toString(); }

void WeighInDialog::profileChanged()
{
    const Profile *p = Profiles::instance().find(profileId());
    const double kg = p ? p->currentKg() : 0;
    m_kg->setValue(kg > 0 ? kg : 70);
    updateInfo();
}

void WeighInDialog::updateInfo()
{
    const Profile *p = Profiles::instance().find(profileId());
    if (!p) {
        m_info->setText(tr("<i>Noch keine Person angelegt – bitte zuerst im Assistenten Gewichtskontrolle eine Person anlegen.</i>"));
        return;
    }
    QStringList lines;
    // letzter Eintrag vor dem gewählten Datum
    const WeightEntry *prev = nullptr;
    const WeightEntry *same = nullptr;
    for (const WeightEntry &e : p->weights) {
        if (e.date < m_date->date()) prev = &e;
        if (e.date == m_date->date()) same = &e;
    }
    const double kg = m_kg->value();
    if (prev) {
        const double d = kg - prev->kg;
        const QString diff = (d > 0.05 ? QStringLiteral("+") : QString()) + Fmt::number(std::fabs(d) < 0.05 ? 0.0 : d) + QStringLiteral("&nbsp;kg");
        QString color = QStringLiteral("#555555");
        if ((p->goal == Profile::Goal::Lose && d < -0.05) || (p->goal == Profile::Goal::Gain && d > 0.05)) color = QStringLiteral("#2e9e6b");
        else if ((p->goal == Profile::Goal::Lose && d > 0.05) || (p->goal == Profile::Goal::Gain && d < -0.05)) color = QStringLiteral("#d06010");
        lines << tr("Letzter Eintrag: %1 kg am %2 – Veränderung&nbsp;<b style=\"color:%3;\">%4</b>")
                     .arg(Fmt::number(prev->kg), prev->date.toString(QStringLiteral("dd.MM.yyyy")), color, diff);
    } else {
        lines << tr("Erster Eintrag für %1.").arg(p->name.toHtmlEscaped());
    }
    const double bmi = p->bmi(kg);
    if (bmi > 0) lines << tr("BMI: %1 (%2)").arg(Fmt::number(bmi), Profile::bmiClass(bmi));
    if (p->targetKg > 0 && p->goal != Profile::Goal::Keep) {
        const double rest = p->goal == Profile::Goal::Lose ? kg - p->targetKg : p->targetKg - kg;
        lines << (rest > 0.05 ? tr("Bis zum Zielgewicht (%1 kg): noch %2 kg").arg(Fmt::number(p->targetKg), Fmt::number(rest))
                              : tr("Zielgewicht (%1 kg) erreicht!").arg(Fmt::number(p->targetKg)));
    }
    if (same) lines << tr("<i>Für diesen Tag gibt es schon einen Eintrag (%1 kg) – er wird ersetzt.</i>").arg(Fmt::number(same->kg));
    if (p->weighReminderDue()) lines << tr("Wiegetag: %1 – die Erinnerung wird mit dem Eintrag erledigt.").arg(Profile::dayName(p->weighDay));
    m_info->setText(lines.join(QStringLiteral("<br/>")));
}

void WeighInDialog::accept()
{
    Profile *p = Profiles::instance().find(profileId());
    if (!p) {
        reject();
        return;
    }
    p->addWeight(m_date->date(), m_kg->value(), m_note->text().trimmed());
    Profiles::instance().save();
    QSettings().setValue(QStringLiteral("weighIn/profile"), p->id);
    QDialog::accept();
}
