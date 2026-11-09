// SPDX-License-Identifier: MIT AND LicenseRef-Commons-Clause-1.0 (siehe LICENSE)
#include "assistantdialog.h"
#include "icons.h"
#include "mealplanner.h"
#include "profiledialog.h"
#include "profiles.h"
#include "recipe.h"
#include "weightchart.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

AssistantDialog::AssistantDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Assistent Gewichtskontrolle"));
    setWindowFlag(Qt::WindowMaximizeButtonHint, true);
    auto *lay = new QVBoxLayout(this);
    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Person:")));
    m_profiles = new QComboBox;
    m_profiles->setMinimumWidth(200);
    top->addWidget(m_profiles, 1);
    auto *add = new QPushButton(Icons::get(QStringLiteral("user-add")), tr("Neu …"));
    m_edit = new QPushButton(Icons::get(QStringLiteral("edit")), tr("Bearbeiten …"));
    m_del = new QPushButton(Icons::get(QStringLiteral("delete")), QString());
    m_del->setToolTip(tr("Person löschen"));
    top->addWidget(add);
    top->addWidget(m_edit);
    top->addWidget(m_del);
    lay->addLayout(top);

    auto *tabs = new QTabWidget;
    m_summary = new QTextBrowser;
    tabs->addTab(m_summary, Icons::get(QStringLiteral("scale")), tr("Übersicht"));
    auto *wpage = new QWidget;
    auto *wl = new QVBoxLayout(wpage);
    auto *split = new QSplitter(Qt::Vertical);
    m_chart = new WeightChart;
    split->addWidget(m_chart);
    auto *lower = new QWidget;
    auto *ll = new QVBoxLayout(lower);
    ll->setContentsMargins(0, 0, 0, 0);
    auto *entry = new QHBoxLayout;
    m_date = new QDateEdit(QDate::currentDate());
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    m_kg = new QDoubleSpinBox;
    m_kg->setRange(30, 300);
    m_kg->setDecimals(1);
    m_kg->setSuffix(tr(" kg"));
    m_note = new QLineEdit;
    m_note->setPlaceholderText(tr("Notiz (optional)"));
    auto *addW = new QPushButton(Icons::get(QStringLiteral("add")), tr("Eintragen"));
    entry->addWidget(m_date);
    entry->addWidget(m_kg);
    entry->addWidget(m_note, 1);
    entry->addWidget(addW);
    ll->addLayout(entry);
    m_weights = new QTableWidget(0, 4);
    m_weights->setHorizontalHeaderLabels({ tr("Datum"), tr("Gewicht"), tr("BMI"), tr("Notiz") });
    m_weights->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_weights->verticalHeader()->hide();
    m_weights->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_weights->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ll->addWidget(m_weights, 1);
    auto *delW = new QPushButton(Icons::get(QStringLiteral("delete")), tr("Eintrag löschen"));
    ll->addWidget(delW, 0, Qt::AlignLeft);
    split->addWidget(lower);
    wl->addWidget(split);
    tabs->addTab(wpage, Icons::get(QStringLiteral("chart")), tr("Gewichtsverlauf"));
    lay->addWidget(tabs, 1);

    auto *bl = new QHBoxLayout;
    auto *note = new QLabel(tr("<small>VitaMeals ist <b>kein Medizinprodukt</b>. Die Werte sind allgemeine Richtwerte und ersetzen "
                               "keine ärztliche oder Ernährungsberatung.</small>"));
    note->setWordWrap(true);
    bl->addWidget(note, 1);
    m_plan = new QPushButton(Icons::get(QStringLiteral("plan")), tr("Wochenplan erstellen …"));
    m_plan->setDefault(true);
    bl->addWidget(m_plan);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    bl->addWidget(bb);
    lay->addLayout(bl);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::accept);

    connect(m_profiles, qOverload<int>(&QComboBox::currentIndexChanged), this, &AssistantDialog::showProfile);
    connect(add, &QPushButton::clicked, this, [this] {
        Profile p;
        p.weights << WeightEntry{ QDate::currentDate(), 70, {} };
        ProfileDialog d(p, this);
        if (d.exec() != QDialog::Accepted) return;
        reloadProfiles(Profiles::instance().add(d.profile()));
    });
    connect(m_edit, &QPushButton::clicked, this, [this] {
        Profile *p = Profiles::instance().find(currentId());
        if (!p) return;
        ProfileDialog d(*p, this);
        if (d.exec() != QDialog::Accepted) return;
        Profiles::instance().update(d.profile());
        reloadProfiles(p->id);
    });
    connect(m_del, &QPushButton::clicked, this, [this] {
        Profile *p = Profiles::instance().find(currentId());
        if (!p) return;
        if (QMessageBox::question(this, windowTitle(), tr("Person „%1“ mit Gewichtsverlauf löschen?").arg(p->name)) != QMessageBox::Yes) return;
        Profiles::instance().remove(p->id);
        reloadProfiles();
    });
    connect(addW, &QPushButton::clicked, this, [this] {
        Profile *p = Profiles::instance().find(currentId());
        if (!p) return;
        bool replaced = false;
        for (WeightEntry &e : p->weights)
            if (e.date == m_date->date()) {
                e.kg = m_kg->value();
                e.note = m_note->text();
                replaced = true;
            }
        if (!replaced) p->weights << WeightEntry{ m_date->date(), m_kg->value(), m_note->text() };
        std::sort(p->weights.begin(), p->weights.end(), [](const WeightEntry &a, const WeightEntry &b) { return a.date < b.date; });
        Profiles::instance().save();
        m_note->clear();
        showProfile();
    });
    connect(delW, &QPushButton::clicked, this, [this] {
        Profile *p = Profiles::instance().find(currentId());
        const int r = m_weights->currentRow();
        if (!p || r < 0) return;
        const QDate d = m_weights->item(r, 0)->data(Qt::UserRole).toDate();
        p->weights.erase(std::remove_if(p->weights.begin(), p->weights.end(), [&](const WeightEntry &e) { return e.date == d; }), p->weights.end());
        Profiles::instance().save();
        showProfile();
    });
    connect(m_plan, &QPushButton::clicked, this, [this] {
        if (!currentId().isEmpty()) emit planRequested(currentId());
    });
    reloadProfiles();
    resize(860, 640);
}

QString AssistantDialog::currentId() const { return m_profiles->currentData().toString(); }

void AssistantDialog::reloadProfiles(const QString &select)
{
    const QString keep = select.isEmpty() ? currentId() : select;
    m_profiles->blockSignals(true);
    m_profiles->clear();
    for (const Profile &p : Profiles::instance().all()) m_profiles->addItem(Icons::get(QStringLiteral("user")), p.name, p.id);
    const int idx = m_profiles->findData(keep);
    m_profiles->setCurrentIndex(idx >= 0 ? idx : 0);
    m_profiles->blockSignals(false);
    showProfile();
}

void AssistantDialog::showProfile()
{
    Profile *pp = Profiles::instance().find(currentId());
    m_edit->setEnabled(pp);
    m_del->setEnabled(pp);
    m_plan->setEnabled(pp);
    if (!pp) {
        m_summary->setHtml(tr("<h2>Willkommen beim Assistenten</h2><p>Legen Sie mit <b>Neu …</b> eine Person an – mit Größe, "
                              "Gewicht, Aktivität und Ziel. Der Assistent berechnet daraus den täglichen Energiebedarf und erstellt "
                              "einen Wochenplan aus Ihren Rezepten, der zu Ihren Vorlieben passt.</p>"));
        m_chart->setProfile(Profile());
        m_weights->setRowCount(0);
        return;
    }
    const Profile &p = *pp;
    const double kg = p.currentKg();
    const double bmi = p.bmi();
    QString hint;
    const double target = p.targetKcal(&hint);
    const QString blue = QStringLiteral("#1474BC"), orange = QStringLiteral("#E0620D");
    QString h = QStringLiteral("<h2 style=\"color:%1;\">%2</h2>").arg(blue, p.name.toHtmlEscaped());
    h += QStringLiteral("<table cellpadding=\"4\">");
    auto row = [&](const QString &a, const QString &b) { h += QStringLiteral("<tr><td>%1</td><td><b>%2</b></td></tr>").arg(a, b); };
    row(tr("Alter, Größe"), tr("%1 Jahre, %2 cm").arg(p.age()).arg(Fmt::number(p.heightCm, 0)));
    row(tr("Aktuelles Gewicht"), kg > 0 ? Fmt::number(kg) + QStringLiteral(" kg") : tr("– bitte eintragen –"));
    if (bmi > 0) row(tr("BMI"), QStringLiteral("%1 (%2)").arg(Fmt::number(bmi), Profile::bmiClass(bmi)));
    row(tr("Grundumsatz"), tr("%1 kcal/Tag").arg(Fmt::kcal(p.bmr())));
    row(tr("Gesamtumsatz (PAL %1)").arg(Fmt::number(p.pal)), tr("%1 kcal/Tag").arg(Fmt::kcal(p.tdee())));
    row(tr("Ziel"), Profile::goalName(p.goal) + (p.goal != Profile::Goal::Keep ? tr(" (%1 kg/Woche)").arg(Fmt::number(p.ratePerWeek, 2)) : QString()));
    h += QStringLiteral("<tr><td>%1</td><td><span style=\"color:%2; font-size:x-large;\"><b>%3 kcal</b></span> &nbsp;(%4 kJ) je Tag</td></tr>")
             .arg(tr("Empfohlene Zufuhr"), orange, Fmt::kcal(target), Fmt::kcal(target * 4.184));
    if (p.targetKg > 0 && kg > 0 && p.goal != Profile::Goal::Keep) {
        const double diff = std::fabs(kg - p.targetKg);
        const double weeks = p.ratePerWeek > 0 ? diff / p.ratePerWeek : 0;
        row(tr("Zielgewicht"), tr("%1 kg – noch %2 kg, bei diesem Tempo etwa %3 Wochen (um den %4)")
                                   .arg(Fmt::number(p.targetKg))
                                   .arg(Fmt::number(diff))
                                   .arg(qRound(weeks))
                                   .arg(QDate::currentDate().addDays(qRound(weeks * 7)).toString(QStringLiteral("dd.MM.yyyy"))));
        if (p.goal == Profile::Goal::Lose && p.bmi(p.targetKg) < 18.5)
            hint += QLatin1Char(' ') + tr("Das Zielgewicht liegt im Bereich Untergewicht (BMI unter 18,5).");
    }
    // Nährstoffverteilung (Richtwerte DGE: 15 % Eiweiß, 30 % Fett, 55 % Kohlenhydrate)
    row(tr("Verteilung (Richtwert)"), tr("Eiweiß %1 g · Fett %2 g · Kohlenhydrate %3 g")
                                         .arg(qRound(target * 0.15 / 4))
                                         .arg(qRound(target * 0.30 / 9))
                                         .arg(qRound(target * 0.55 / 4)));
    row(tr("Ernährung"), Profile::dietName(p.diet));
    if (!p.excluded.isEmpty()) row(tr("Nicht verwenden"), p.excluded.join(QStringLiteral(", ")).toHtmlEscaped());
    if (!p.likedTags.isEmpty()) row(tr("Bevorzugt"), p.likedTags.join(QStringLiteral(", ")).toHtmlEscaped());
    h += QStringLiteral("</table>");
    if (!hint.isEmpty()) h += QStringLiteral("<p style=\"color:%1;\"><b>%2</b></p>").arg(orange, hint.toHtmlEscaped());
    const int cands = MealPlanner::candidateCount(MealPlanner::fromProfile(p));
    h += QStringLiteral("<p>%1</p>").arg(tr("Für den Wochenplan passen <b>%1</b> Rezepte der geöffneten Datenbank (mit Nährwerten und passend zu den Vorlieben).").arg(cands));
    if (!p.lastPlan.isEmpty()) h += QStringLiteral("<p>%1</p>").arg(tr("Ein zuletzt erstellter Wochenplan ist gespeichert."));
    h += QStringLiteral("<p><small>%1</small></p>").arg(tr("Berechnung: Grundumsatz nach Mifflin-St Jeor, multipliziert mit dem PAL-Wert; "
                                                          "Energiedefizit bzw. -überschuss aus dem Wochenziel (ca. 7 000 kcal je kg), begrenzt auf "
                                                          "höchstens 1 000 kcal weniger bzw. 700 kcal mehr je Tag und nicht unter den Grundumsatz."));
    m_summary->setHtml(h);
    m_chart->setProfile(p);
    m_kg->setValue(kg > 0 ? kg : 70);
    m_weights->setRowCount(0);
    for (int i = int(p.weights.size()) - 1; i >= 0; --i) {
        const WeightEntry &e = p.weights[i];
        const int r = m_weights->rowCount();
        m_weights->insertRow(r);
        auto *d = new QTableWidgetItem(e.date.toString(QStringLiteral("dd.MM.yyyy")));
        d->setData(Qt::UserRole, e.date);
        m_weights->setItem(r, 0, d);
        auto *k = new QTableWidgetItem(Fmt::number(e.kg) + QStringLiteral(" kg"));
        k->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_weights->setItem(r, 1, k);
        auto *b = new QTableWidgetItem(Fmt::number(p.bmi(e.kg)));
        b->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_weights->setItem(r, 2, b);
        m_weights->setItem(r, 3, new QTableWidgetItem(e.note));
    }
}
