// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "profiledialog.h"
#include "recipedb.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDate>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QVBoxLayout>

ProfileDialog::ProfileDialog(const Profile &p, QWidget *parent) : QDialog(parent), m_p(p)
{
    setWindowTitle(p.name.isEmpty() ? tr("Neue Person") : tr("Profil – %1").arg(p.name));
    auto *lay = new QVBoxLayout(this);

    auto *body = new QGroupBox(tr("Person"));
    auto *bf = new QFormLayout(body);
    m_name = new QLineEdit(p.name);
    bf->addRow(tr("Name:"), m_name);
    m_sex = new QComboBox;
    m_sex->addItems({ tr("weiblich"), tr("männlich") });
    m_sex->setCurrentIndex(p.female ? 0 : 1);
    m_sex->setToolTip(tr("Für die Berechnung des Grundumsatzes"));
    bf->addRow(tr("Geschlecht:"), m_sex);
    m_year = new QSpinBox;
    m_year->setRange(1900, QDate::currentDate().year() - 14);
    m_year->setValue(qBound(1900, p.birthYear, QDate::currentDate().year() - 14));
    bf->addRow(tr("Geburtsjahr:"), m_year);
    m_height = new QDoubleSpinBox;
    m_height->setRange(100, 230);
    m_height->setDecimals(0);
    m_height->setSuffix(tr(" cm"));
    m_height->setValue(p.heightCm);
    bf->addRow(tr("Größe:"), m_height);
    m_weight = new QDoubleSpinBox;
    m_weight->setRange(30, 300);
    m_weight->setDecimals(1);
    m_weight->setSuffix(tr(" kg"));
    m_weight->setValue(p.currentKg() > 0 ? p.currentKg() : 70);
    m_weight->setToolTip(tr("Ein geänderter Wert wird mit heutigem Datum im Gewichtsverlauf eingetragen."));
    bf->addRow(tr("Aktuelles Gewicht:"), m_weight);
    m_pal = new QComboBox;
    int palIdx = 1;
    const auto levels = Profile::palLevels();
    for (int i = 0; i < levels.size(); ++i) {
        m_pal->addItem(levels[i].second, levels[i].first);
        if (qAbs(levels[i].first - p.pal) < 0.05) palIdx = i;
    }
    m_pal->setCurrentIndex(palIdx);
    bf->addRow(tr("Aktivität (PAL):"), m_pal);
    lay->addWidget(body);

    auto *goal = new QGroupBox(tr("Ziel"));
    auto *gf = new QFormLayout(goal);
    m_goal = new QComboBox;
    for (auto g : { Profile::Goal::Lose, Profile::Goal::Keep, Profile::Goal::Gain }) m_goal->addItem(Profile::goalName(g), int(g));
    m_goal->setCurrentIndex(int(p.goal));
    gf->addRow(tr("Ziel:"), m_goal);
    m_target = new QDoubleSpinBox;
    m_target->setRange(0, 300);
    m_target->setDecimals(1);
    m_target->setSuffix(tr(" kg"));
    m_target->setSpecialValueText(tr("kein Zielgewicht"));
    m_target->setValue(p.targetKg);
    gf->addRow(tr("Zielgewicht:"), m_target);
    m_rate = new QDoubleSpinBox;
    m_rate->setRange(0.1, 1.0);
    m_rate->setSingleStep(0.05);
    m_rate->setDecimals(2);
    m_rate->setSuffix(tr(" kg je Woche"));
    m_rate->setValue(p.ratePerWeek);
    m_rate->setToolTip(tr("Empfohlen sind höchstens 0,5 kg je Woche."));
    gf->addRow(tr("Tempo:"), m_rate);
    lay->addWidget(goal);

    auto *pref = new QGroupBox(tr("Vorlieben für den Wochenplan"));
    auto *pf = new QFormLayout(pref);
    m_diet = new QComboBox;
    for (auto d : { Profile::Diet::All, Profile::Diet::Vegetarian, Profile::Diet::Vegan }) m_diet->addItem(Profile::dietName(d), int(d));
    m_diet->setCurrentIndex(int(p.diet));
    pf->addRow(tr("Ernährung:"), m_diet);
    m_excluded = new QLineEdit(p.excluded.join(QStringLiteral(", ")));
    m_excluded->setPlaceholderText(tr("z. B. Pilze, Koriander, Nüsse"));
    pf->addRow(tr("Nicht verwenden:"), m_excluded);
    m_liked = new QLineEdit(p.likedTags.join(QStringLiteral(", ")));
    m_liked->setPlaceholderText(tr("z. B. italienisch, suppen, salate"));
    auto *comp = new QCompleter(RecipeDb::instance().allTags(), this);
    comp->setCaseSensitivity(Qt::CaseInsensitive);
    m_liked->setCompleter(comp);
    pf->addRow(tr("Bevorzugte Themen:"), m_liked);
    m_snack = new QCheckBox(tr("Zwischenmahlzeit einplanen (beim Zunehmen zusätzlich eine Spätmahlzeit)"));
    m_snack->setChecked(p.snack);
    pf->addRow(QString(), m_snack);
    m_weighDay = new QComboBox;
    m_weighDay->addItem(tr("keine Erinnerung"), 0);
    for (int d = 1; d <= 7; ++d) m_weighDay->addItem(Profile::dayName(d), d);
    m_weighDay->setCurrentIndex(qMax(0, m_weighDay->findData(p.weighDay)));
    m_weighDay->setToolTip(tr("Einmal pro Woche wiegen, am besten morgens vor dem Frühstück – ViviaMeals erinnert an diesem Tag daran."));
    pf->addRow(tr("Wiegetag:"), m_weighDay);
    lay->addWidget(pref);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &ProfileDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

Profile ProfileDialog::profile() const
{
    Profile p = m_p;
    p.name = m_name->text().trimmed();
    p.female = m_sex->currentIndex() == 0;
    p.birthYear = m_year->value();
    p.heightCm = m_height->value();
    p.pal = m_pal->currentData().toDouble();
    p.goal = Profile::Goal(m_goal->currentData().toInt());
    p.targetKg = m_target->value();
    p.ratePerWeek = m_rate->value();
    p.diet = Profile::Diet(m_diet->currentData().toInt());
    p.excluded.clear();
    for (const QString &x : m_excluded->text().split(QLatin1Char(','), Qt::SkipEmptyParts)) p.excluded << x.trimmed();
    p.likedTags.clear();
    for (const QString &x : m_liked->text().split(QLatin1Char(','), Qt::SkipEmptyParts)) p.likedTags << x.trimmed().toLower();
    p.snack = m_snack->isChecked();
    p.weighDay = m_weighDay->currentData().toInt();
    const double kg = m_weight->value();
    if (qAbs(kg - p.currentKg()) > 0.05) {
        const QDate today = QDate::currentDate();
        if (!p.weights.isEmpty() && p.weights.last().date == today) p.weights.last().kg = kg;
        else p.weights << WeightEntry{ today, kg, {} };
    }
    return p;
}

void ProfileDialog::accept()
{
    if (m_name->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, windowTitle(), tr("Bitte einen Namen eingeben."));
        return;
    }
    QDialog::accept();
}
