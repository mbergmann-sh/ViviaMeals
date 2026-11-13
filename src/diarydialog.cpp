// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "diarydialog.h"
#include "symbols.h"
#include "editordialog.h"
#include "icons.h"
#include "mealplanner.h"
#include "mealtypes.h"
#include "nutrition.h"
#include "profiles.h"
#include "recipedb.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QString kcalText(double v) { return QLocale(QLocale::German).toString(qRound(v)) + QStringLiteral(" kcal"); }
}

// ================================================================ Eintrag

DiaryEntryDialog::DiaryEntryDialog(Mode mode, const Diary::Entry &e, double usedKcal, double targetKcal, QWidget *parent)
    : QDialog(parent), m_mode(mode), m_entry(e), m_used(usedKcal), m_target(targetKcal)
{
    setWindowTitle(mode == Mode::Edit ? tr("Tagebucheintrag bearbeiten") : mode == Mode::Own ? tr("Eigenes Rezept eintragen") : tr("Rezept eintragen"));
    setWindowIcon(Icons::get(QStringLiteral("diary")));
    auto *lay = new QVBoxLayout(this);
    if (mode == Mode::Recipe) {
        m_filter = new QLineEdit;
        m_filter->setPlaceholderText(tr("Rezept suchen (Name, Zutat, Tag) …"));
        m_filter->setClearButtonEnabled(true);
        lay->addWidget(m_filter);
        m_list = new QListWidget;
        m_list->setMinimumHeight(200);
        lay->addWidget(m_list, 1);
        connect(m_filter, &QLineEdit::textChanged, this, &DiaryEntryDialog::fillRecipes);
        connect(m_list, &QListWidget::currentRowChanged, this, &DiaryEntryDialog::recipeChosen);
    }
    m_title = new QLabel;
    m_title->setWordWrap(true);
    QFont tf = m_title->font();
    tf.setBold(true);
    tf.setPointSizeF(tf.pointSizeF() * 1.15);
    m_title->setFont(tf);
    lay->addWidget(m_title);
    if (mode == Mode::Own || (mode == Mode::Edit && e.own)) {
        auto *ol = new QHBoxLayout;
        m_editOwn = new QPushButton(Icons::get(QStringLiteral("edit")), tr("Rezept bearbeiten …"));
        ol->addWidget(m_editOwn);
        m_saveToDb = new QCheckBox(tr("In die Rezeptdatenbank übernehmen"));
        m_saveToDb->setToolTip(tr("Das eigene Rezept zusätzlich als Rezept in der geöffneten Datenbank speichern"));
        m_saveToDb->setEnabled(RecipeDb::instance().isOpen());
        if (!RecipeDb::instance().isOpen()) m_saveToDb->setToolTip(tr("Keine Rezeptdatenbank geöffnet"));
        ol->addWidget(m_saveToDb);
        ol->addStretch(1);
        lay->addLayout(ol);
        connect(m_editOwn, &QPushButton::clicked, this, [this] {
            editOwnRecipe();
            loadEntry();
        });
    }
    auto *form = new QFormLayout;
    m_when = new QDateTimeEdit(e.when.isValid() ? e.when : QDateTime::currentDateTime());
    m_when->setCalendarPopup(true);
    m_when->setDisplayFormat(QStringLiteral("ddd, dd.MM.yyyy  HH:mm"));
    form->addRow(tr("&Datum und Uhrzeit:"), m_when);
    m_meal = new QComboBox;
    m_meal->setEditable(true);
    m_meal->addItems(Diary::mealNames());
    m_meal->setCurrentText(e.meal.isEmpty() ? Diary::mealForTime(m_when->time()) : e.meal);
    form->addRow(tr("&Mahlzeit:"), m_meal);
    auto *al = new QHBoxLayout;
    m_amount = new QDoubleSpinBox;
    m_amount->setRange(0.05, 5000);
    m_amount->setDecimals(2);
    m_amount->setSingleStep(0.25);
    al->addWidget(m_amount);
    m_unit = new QComboBox;
    m_unit->addItem(tr("Portionen"));
    m_unit->addItem(tr("Gramm"));
    al->addWidget(m_unit);
    al->addStretch(1);
    form->addRow(tr("Gegessene Menge:"), al);
    m_note = new QLineEdit(e.note);
    m_note->setPlaceholderText(tr("optional, z. B. „ohne Soße“, „Restaurant“"));
    form->addRow(tr("&Notiz:"), m_note);
    lay->addLayout(form);
    m_info = new QLabel;
    m_info->setWordWrap(true);
    m_info->setTextFormat(Qt::RichText);
    lay->addWidget(m_info);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_ok = bb->button(QDialogButtonBox::Ok);
    m_ok->setText(mode == Mode::Edit ? tr("Übernehmen") : tr("Eintragen"));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &DiaryEntryDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_amount, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &DiaryEntryDialog::amountChanged);
    connect(m_unit, qOverload<int>(&QComboBox::currentIndexChanged), this, &DiaryEntryDialog::unitChanged);
    connect(m_when, &QDateTimeEdit::dateTimeChanged, this, [this](const QDateTime &dt) {
        // Mahlzeit mitführen, solange sie nicht von Hand geändert wurde
        if (Diary::mealNames().contains(m_meal->currentText())) m_meal->setCurrentText(Diary::mealForTime(dt.time()));
    });
    setMinimumWidth(520);
    if (m_list) {
        fillRecipes();
        resize(620, 640);
    }
    loadEntry();
}

void DiaryEntryDialog::fillRecipes()
{
    const QString f = m_filter->text().trimmed().toLower();
    const QString keep = m_entry.recipeId;
    m_list->blockSignals(true);
    m_list->clear();
    QList<const RecipeSummary *> l;
    for (const RecipeSummary &s : RecipeDb::instance().summaries()) {
        if (!f.isEmpty()) {
            bool hit = s.title.toLower().contains(f) || s.tags.join(QLatin1Char(' ')).toLower().contains(f);
            for (const QString &n : s.ingredientNames)
                if (n.contains(f)) hit = true;
            if (!hit) continue;
        }
        l << &s;
    }
    std::sort(l.begin(), l.end(), [](const RecipeSummary *a, const RecipeSummary *b) { return QString::localeAwareCompare(a->title, b->title) < 0; });
    int sel = -1;
    for (const RecipeSummary *s : l) {
        QString t = s->title;
        if (s->hasNutrition) t += QStringLiteral("   ·   ") + tr("%1 je Portion").arg(kcalText(s->perServing.kcal));
        else t += QStringLiteral("   ·   ") + tr("keine Nährwerte");
        auto *it = new QListWidgetItem(Icons::get(s->favorite ? QStringLiteral("star") : QStringLiteral("recipe")), t);
        it->setData(Qt::UserRole, s->id);
        m_list->addItem(it);
        if (s->id == keep) sel = m_list->count() - 1;
    }
    m_list->blockSignals(false);
    if (sel >= 0) m_list->setCurrentRow(sel);
    else if (m_list->count() == 1) m_list->setCurrentRow(0);
}

void DiaryEntryDialog::recipeChosen()
{
    auto *it = m_list->currentItem();
    if (!it) return;
    Recipe r;
    if (!RecipeDb::instance().loadLight(it->data(Qt::UserRole).toString(), &r)) return;
    const Diary::Entry fresh = Diary::Entry::fromRecipe(r, false);
    m_entry.title = fresh.title;
    m_entry.recipeId = fresh.recipeId;
    m_entry.perServing = fresh.perServing;
    m_entry.servingGrams = fresh.servingGrams;
    m_entry.yieldUnit = fresh.yieldUnit;
    m_entry.setPortions(1);
    loadEntry();
}

void DiaryEntryDialog::loadEntry()
{
    m_updating = true;
    QString t = m_entry.title.isEmpty() ? tr("Bitte ein Rezept wählen") : m_entry.title;
    if (m_entry.own) t += QStringLiteral("  ") + tr("(eigenes Rezept)");
    m_title->setText(t);
    const bool grams = m_entry.servingGrams > 0;
    m_unit->setItemText(0, m_entry.yieldUnit.isEmpty() ? tr("Portionen") : m_entry.yieldUnit);
    m_unit->setEnabled(grams || m_entry.byGrams);
    if (m_entry.byGrams && grams) {
        m_unit->setCurrentIndex(1);
        m_amount->setDecimals(0);
        m_amount->setSingleStep(10);
        m_amount->setValue(m_entry.grams);
    } else {
        m_unit->setCurrentIndex(0);
        m_amount->setDecimals(2);
        m_amount->setSingleStep(0.25);
        m_amount->setValue(m_entry.portions);
    }
    m_ok->setEnabled(!m_entry.title.isEmpty());
    m_updating = false;
    updateInfo();
}

void DiaryEntryDialog::amountChanged()
{
    if (m_updating) return;
    if (m_unit->currentIndex() == 1) m_entry.setGrams(m_amount->value());
    else m_entry.setPortions(m_amount->value());
    updateInfo();
}

void DiaryEntryDialog::unitChanged()
{
    if (m_updating) return;
    m_updating = true;
    if (m_unit->currentIndex() == 1) {
        m_entry.byGrams = true;
        if (m_entry.grams <= 0) m_entry.grams = m_entry.servingGrams * m_entry.portions;
        m_amount->setDecimals(0);
        m_amount->setSingleStep(10);
        m_amount->setValue(qRound(m_entry.grams));
    } else {
        m_entry.byGrams = false;
        m_amount->setDecimals(2);
        m_amount->setSingleStep(0.25);
        m_amount->setValue(m_entry.portions);
    }
    m_updating = false;
    amountChanged();
}

void DiaryEntryDialog::updateInfo()
{
    if (m_entry.title.isEmpty()) {
        m_info->clear();
        return;
    }
    const Nutrients n = m_entry.eaten();
    QStringList l;
    if (m_entry.perServing.kcal <= 0) {
        l << tr("<span style=\"color:#d06010;\">Für dieses Rezept sind keine Nährwerte bekannt – bitte im Editor die Zutaten "
                "zuordnen.</span>");
    } else {
        l << tr("<b>%1</b> · Eiweiß %2 g · Fett %3 g · Kohlenhydrate %4 g · Zucker %5 g · Ballaststoffe %6 g")
                 .arg(kcalText(n.kcal), Fmt::number(n.protein), Fmt::number(n.fat), Fmt::number(n.carbs), Fmt::number(n.sugar),
                      Fmt::number(n.fiber));
        if (m_entry.servingGrams > 0)
            l << tr("1 Portion ≈ %1 g · gegessen ≈ %2 g (%3 Portionen)")
                     .arg(qRound(m_entry.servingGrams))
                     .arg(qRound(m_entry.grams))
                     .arg(Fmt::amount(m_entry.portions, QString()));
    }
    if (m_target > 0) {
        const double rest = m_target - m_used - n.kcal;
        l << (rest >= 0 ? tr("Danach verbleiben heute noch <b style=\"color:#2e9e6b;\">%1</b> von %2.").arg(kcalText(rest), kcalText(m_target))
                        : tr("Damit wird das Tagesziel um <b style=\"color:#d06010;\">%1</b> überschritten.").arg(kcalText(-rest)));
    }
    m_info->setText(l.join(QStringLiteral("<br/>")));
}

bool DiaryEntryDialog::editOwnRecipe()
{
    Recipe r = m_entry.recipe();
    if (m_entry.ownRecipe.isEmpty()) {
        r = Recipe();
        r.id = Recipe::newId();
        r.title = m_entry.title;
        r.servings = 1;
    }
    EditorDialog ed(r, this);
    ed.setWindowTitle(tr("Eigenes Rezept"));
    if (ed.exec() != QDialog::Accepted) return false;
    Recipe nr = ed.recipe();
    Nutrition::analyze(nr);
    const double portions = m_entry.ownRecipe.isEmpty() ? 1.0 : m_entry.portions;
    const bool byGrams = m_entry.byGrams;
    const double grams = m_entry.grams;
    const Diary::Entry fresh = Diary::Entry::fromRecipe(nr, true);
    m_entry.title = fresh.title;
    m_entry.own = true;
    m_entry.ownRecipe = fresh.ownRecipe;
    m_entry.perServing = fresh.perServing;
    m_entry.servingGrams = fresh.servingGrams;
    m_entry.yieldUnit = fresh.yieldUnit;
    if (byGrams && grams > 0) m_entry.setGrams(grams);
    else m_entry.setPortions(portions);
    return true;
}

void DiaryEntryDialog::accept()
{
    if (m_entry.title.isEmpty()) return;
    m_entry.when = m_when->dateTime();
    m_entry.when.setTime(QTime(m_entry.when.time().hour(), m_entry.when.time().minute()));
    m_entry.meal = m_meal->currentText().trimmed();
    m_entry.note = m_note->text().trimmed();
    if (m_saveToDb && m_saveToDb->isChecked() && m_entry.own) {
        Recipe r = m_entry.recipe();
        if (r.id.isEmpty() || RecipeDb::instance().summary(r.id)) r.id = Recipe::newId();
        if (r.sourceFormat.isEmpty()) r.sourceFormat = QStringLiteral("ViviaMeals");
        if (r.source.isEmpty()) r.source = tr("Ernährungstagebuch");
        MealTypes::applyTags(r);
        MealTypes::applyNutrientTags(r);
        QString err;
        if (!RecipeDb::instance().save(r, &err)) {
            QMessageBox::warning(this, windowTitle(), tr("Das Rezept konnte nicht gespeichert werden:\n%1").arg(err));
            return;
        }
        // ab jetzt ein Rezept der Datenbank
        m_entry.own = false;
        m_entry.ownRecipe = QJsonObject();
        m_entry.recipeId = r.id;
    }
    QDialog::accept();
}

// ================================================================ Tagebuch

DiaryDialog::DiaryDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Ernährungstagebuch"));
    setWindowIcon(Icons::get(QStringLiteral("diary")));
    auto *lay = new QVBoxLayout(this);
    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Person:")));
    m_profile = new QComboBox;
    m_profile->setMinimumWidth(200);
    for (const Profile &p : Profiles::instance().all()) m_profile->addItem(Icons::get(QStringLiteral("user")), p.name, p.id);
    top->addWidget(m_profile);
    top->addSpacing(16);
    auto *prev = new QToolButton;
    Sym::setButton(prev, Sym::Left);
    prev->setToolTip(tr("Vortag"));
    m_date = new QDateEdit(QDate::currentDate());
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat(QStringLiteral("dddd, dd.MM.yyyy"));
    auto *next = new QToolButton;
    Sym::setButton(next, Sym::Right);
    next->setToolTip(tr("Folgetag"));
    auto *today = new QPushButton(tr("Heute"));
    top->addWidget(prev);
    top->addWidget(m_date);
    top->addWidget(next);
    top->addWidget(today);
    top->addStretch(1);
    lay->addLayout(top);

    m_summary = new QLabel;
    QFont sf = m_summary->font();
    sf.setPointSizeF(sf.pointSizeF() * 1.15);
    m_summary->setFont(sf);
    m_summary->setTextFormat(Qt::RichText);
    lay->addWidget(m_summary);
    m_bar = new QProgressBar;
    m_bar->setTextVisible(true);
    lay->addWidget(m_bar);
    m_macros = new QLabel;
    m_macros->setWordWrap(true);
    lay->addWidget(m_macros);

    m_table = new QTableWidget(0, 8);
    m_table->setHorizontalHeaderLabels({ tr("Uhrzeit"), tr("Mahlzeit"), tr("Speise"), tr("Menge"), tr("kcal"), tr("Eiweiß"), tr("Fett"), tr("KH") });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    for (int c : { 0, 1, 3, 4, 5, 6, 7 }) m_table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    lay->addWidget(m_table, 1);

    auto *bl = new QHBoxLayout;
    m_fromRecipe = new QPushButton(Icons::get(QStringLiteral("recipe")), tr("Aus &Rezept …"));
    m_fromRecipe->setToolTip(tr("Ein Rezept der Datenbank mit der tatsächlich gegessenen Menge eintragen"));
    m_own = new QPushButton(Icons::get(QStringLiteral("recipe-add")), tr("&Eigenes Rezept …"));
    m_own->setToolTip(tr("Eine Speise im Editor erfassen (Zutaten und Mengen) – auf Wunsch auch in die Rezeptdatenbank übernehmen"));
    m_fromPlan = new QPushButton(Icons::get(QStringLiteral("plan")), tr("Aus &Wochenplan"));
    m_fromPlan->setToolTip(tr("Die für diesen Tag geplanten Mahlzeiten eintragen"));
    m_edit = new QPushButton(Icons::get(QStringLiteral("edit")), tr("&Bearbeiten …"));
    m_copy = new QPushButton(Icons::get(QStringLiteral("copy")), tr("Auf &heute kopieren"));
    m_copy->setToolTip(tr("Die gewählte Speise noch einmal für heute eintragen (z. B. das tägliche Frühstück)"));
    m_remove = new QPushButton(Icons::get(QStringLiteral("delete")), tr("&Löschen"));
    for (QPushButton *b : { m_fromRecipe, m_own, m_fromPlan, m_edit, m_copy, m_remove }) bl->addWidget(b);
    bl->addStretch(1);
    lay->addLayout(bl);

    m_week = new QLabel;
    m_week->setWordWrap(true);
    m_week->setTextFormat(Qt::RichText);
    lay->addWidget(m_week);
    auto *foot = new QHBoxLayout;
    auto *disc = new QLabel(tr("<small>Die Nährwerte sind aus den Zutaten berechnete Schätzwerte. ViviaMeals ist <b>kein Medizinprodukt</b>.</small>"));
    disc->setWordWrap(true);
    foot->addWidget(disc, 1);
    auto *close = new QPushButton(Icons::get(QStringLiteral("quit")), tr("Schließen"));
    foot->addWidget(close);
    lay->addLayout(foot);

    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(prev, &QToolButton::clicked, this, [this] { m_date->setDate(m_date->date().addDays(-1)); });
    connect(next, &QToolButton::clicked, this, [this] { m_date->setDate(m_date->date().addDays(1)); });
    connect(today, &QPushButton::clicked, this, [this] { m_date->setDate(QDate::currentDate()); });
    connect(m_date, &QDateEdit::dateChanged, this, &DiaryDialog::refresh);
    connect(m_profile, qOverload<int>(&QComboBox::currentIndexChanged), this, &DiaryDialog::profileChanged);
    connect(m_fromRecipe, &QPushButton::clicked, this, &DiaryDialog::addFromRecipe);
    connect(m_own, &QPushButton::clicked, this, &DiaryDialog::addOwn);
    connect(m_fromPlan, &QPushButton::clicked, this, &DiaryDialog::addFromPlan);
    connect(m_edit, &QPushButton::clicked, this, &DiaryDialog::editEntry);
    connect(m_copy, &QPushButton::clicked, this, &DiaryDialog::copyEntryToToday);
    connect(m_remove, &QPushButton::clicked, this, &DiaryDialog::removeEntry);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &DiaryDialog::editEntry);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] {
        const bool sel = !selectedId().isEmpty();
        m_edit->setEnabled(sel);
        m_remove->setEnabled(sel);
        m_copy->setEnabled(sel && m_date->date() != QDate::currentDate());
    });
    connect(m_table, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        if (selectedId().isEmpty()) return;
        QMenu menu(this);
        menu.addAction(Icons::get(QStringLiteral("edit")), tr("Bearbeiten …"), this, &DiaryDialog::editEntry);
        if (m_date->date() != QDate::currentDate())
            menu.addAction(Icons::get(QStringLiteral("copy")), tr("Auf heute kopieren"), this, &DiaryDialog::copyEntryToToday);
        menu.addAction(Icons::get(QStringLiteral("delete")), tr("Löschen"), this, &DiaryDialog::removeEntry);
        menu.exec(m_table->viewport()->mapToGlobal(pos));
    });

    QSettings s;
    const int idx = m_profile->findData(s.value(QStringLiteral("diary/profile")).toString());
    m_profile->setCurrentIndex(idx >= 0 ? idx : 0);
    profileChanged();
    resize(980, 680);
}

QString DiaryDialog::profileId() const { return m_profile->currentData().toString(); }

double DiaryDialog::target() const
{
    const Profile *p = Profiles::instance().find(const_cast<DiaryDialog *>(this)->profileId());
    return p ? p->targetKcal() : 0;
}

void DiaryDialog::profileChanged()
{
    m_book.load(profileId());
    QSettings().setValue(QStringLiteral("diary/profile"), profileId());
    refresh();
}

QString DiaryDialog::selectedId() const
{
    const int r = m_table->currentRow();
    if (r < 0 || !m_table->item(r, 0)) return {};
    return m_table->item(r, 0)->data(Qt::UserRole).toString();
}

void DiaryDialog::store()
{
    QString err;
    if (!m_book.save(&err)) QMessageBox::warning(this, windowTitle(), tr("Das Tagebuch konnte nicht gespeichert werden:\n%1").arg(err));
}

void DiaryDialog::refresh()
{
    const bool hasProfile = !profileId().isEmpty();
    const bool dbOpen = RecipeDb::instance().isOpen();
    m_fromRecipe->setEnabled(hasProfile && dbOpen);
    m_fromRecipe->setToolTip(dbOpen ? tr("Ein Rezept der Datenbank mit der tatsächlich gegessenen Menge eintragen") : tr("Keine Rezeptdatenbank geöffnet"));
    m_own->setEnabled(hasProfile);
    const QDate day = m_date->date();
    const QList<Diary::Entry> entries = m_book.day(day);
    m_table->setRowCount(0);
    for (const Diary::Entry &e : entries) {
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        const Nutrients n = e.eaten();
        auto set = [&](int c, const QString &t, bool right = false) {
            auto *it = new QTableWidgetItem(t);
            if (right) it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_table->setItem(r, c, it);
            return it;
        };
        set(0, e.when.toString(QStringLiteral("HH:mm")))->setData(Qt::UserRole, e.id);
        set(1, e.meal);
        QTableWidgetItem *t = set(2, e.title + (e.note.isEmpty() ? QString() : QStringLiteral(" – ") + e.note));
        t->setIcon(Icons::get(e.own ? QStringLiteral("recipe-add") : QStringLiteral("recipe")));
        t->setToolTip(e.own ? tr("eigenes Rezept") : tr("Rezept der Datenbank"));
        set(3, e.amountText());
        set(4, QLocale(QLocale::German).toString(qRound(n.kcal)), true);
        set(5, Fmt::number(n.protein) + QStringLiteral(" g"), true);
        set(6, Fmt::number(n.fat) + QStringLiteral(" g"), true);
        set(7, Fmt::number(n.carbs) + QStringLiteral(" g"), true);
    }
    // Tagessumme und Restkalorien
    const Nutrients tot = m_book.dayTotal(day);
    const double goal = target();
    if (!hasProfile) {
        m_summary->setText(tr("<i>Bitte zuerst im Assistenten Gewichtskontrolle eine Person anlegen.</i>"));
        m_bar->hide();
    } else if (goal > 0) {
        const double rest = goal - tot.kcal;
        m_summary->setText(tr("Ziel <b>%1</b> · gegessen <b>%2</b> · %3")
                               .arg(kcalText(goal), kcalText(tot.kcal),
                                    rest >= 0 ? tr("noch verfügbar <b style=\"color:#2e9e6b;\">%1</b>").arg(kcalText(rest))
                                              : tr("überschritten um <b style=\"color:#d06010;\">%1</b>").arg(kcalText(-rest))));
        m_bar->show();
        m_bar->setRange(0, qMax(1, qRound(goal)));
        m_bar->setValue(qMin(qRound(tot.kcal), qRound(goal)));
        m_bar->setFormat(tr("%1 %").arg(qRound(tot.kcal * 100.0 / goal)));
        m_bar->setStyleSheet(rest < 0 ? QStringLiteral("QProgressBar::chunk { background: #d06010; }") : QString());
    }
    if (tot.kcal > 0) {
        const auto pct = [&](double g, double f) { return qRound(g * f * 100.0 / tot.kcal); };
        m_macros->setText(tr("Eiweiß %1 g (%2 %) · Fett %3 g (%4 %) · Kohlenhydrate %5 g (%6 %) · Zucker %7 g · Ballaststoffe %8 g · Salz %9 g")
                              .arg(Fmt::number(tot.protein))
                              .arg(pct(tot.protein, 4))
                              .arg(Fmt::number(tot.fat))
                              .arg(pct(tot.fat, 9))
                              .arg(Fmt::number(tot.carbs))
                              .arg(pct(tot.carbs, 4))
                              .arg(Fmt::number(tot.sugar))
                              .arg(Fmt::number(tot.fiber))
                              .arg(Fmt::number(tot.salt)));
    } else {
        m_macros->setText(tr("Für diesen Tag ist noch nichts eingetragen."));
    }
    // Wochenübersicht (die 7 Tage bis zum gewählten Tag)
    QStringList w;
    double sum = 0;
    int n = 0;
    const QLocale de(QLocale::German);
    for (int i = 6; i >= 0; --i) {
        const QDate d = day.addDays(-i);
        const double k = m_book.dayTotal(d).kcal;
        QString cell = QStringLiteral("%1 %2").arg(de.toString(d, QStringLiteral("ddd")), k > 0 ? de.toString(qRound(k)) : QStringLiteral("–"));
        if (k > 0 && goal > 0 && k > goal * 1.05) cell = QStringLiteral("<span style=\"color:#d06010;\">%1</span>").arg(cell);
        if (d == day) cell = QStringLiteral("<b>%1</b>").arg(cell);
        w << cell;
        if (k > 0) {
            sum += k;
            ++n;
        }
    }
    m_week->setText(tr("<b>Letzte 7 Tage (kcal):</b> %1%2")
                        .arg(w.join(QStringLiteral(" · ")),
                             n > 0 ? tr(" — Ø %1 kcal an %2 Tagen mit Einträgen").arg(de.toString(qRound(sum / n))).arg(n) : QString()));
    // Wochenplan für diesen Tag vorhanden?
    bool planned = false;
    if (const Profile *p = Profiles::instance().find(profileId())) {
        const MealPlan plan = MealPlan::fromJson(p->lastPlan);
        const int idx = int(plan.start.daysTo(day));
        planned = !plan.isEmpty() && idx >= 0 && idx < plan.days.size();
    }
    m_fromPlan->setEnabled(planned && dbOpen);
    m_fromPlan->setToolTip(planned ? tr("Die für diesen Tag geplanten Mahlzeiten eintragen") : tr("Für diesen Tag gibt es keinen gespeicherten Wochenplan"));
    m_edit->setEnabled(false);
    m_remove->setEnabled(false);
    m_copy->setEnabled(false);
}

void DiaryDialog::addRecipe(const QString &recipeId)
{
    Recipe r;
    if (profileId().isEmpty() || !RecipeDb::instance().load(recipeId, &r)) return;
    Diary::Entry e = Diary::Entry::fromRecipe(r, false);
    QDateTime when(m_date->date(), m_date->date() == QDate::currentDate() ? QTime::currentTime() : QTime(12, 30));
    e.when = when;
    DiaryEntryDialog d(DiaryEntryDialog::Mode::Edit, e, m_book.dayTotal(m_date->date()).kcal, target(), this);
    d.setWindowTitle(tr("Rezept eintragen"));
    if (d.exec() != QDialog::Accepted) return;
    m_book.add(d.entry());
    store();
    m_date->setDate(d.entry().when.date());
    refresh();
}

void DiaryDialog::addFromRecipe()
{
    Diary::Entry e;
    const QDate day = m_date->date();
    e.when = QDateTime(day, day == QDate::currentDate() ? QTime::currentTime() : QTime(12, 30));
    DiaryEntryDialog d(DiaryEntryDialog::Mode::Recipe, e, m_book.dayTotal(day).kcal, target(), this);
    if (d.exec() != QDialog::Accepted) return;
    m_book.add(d.entry());
    store();
    m_date->setDate(d.entry().when.date());
    refresh();
}

void DiaryDialog::addOwn()
{
    Diary::Entry e;
    const QDate day = m_date->date();
    e.when = QDateTime(day, day == QDate::currentDate() ? QTime::currentTime() : QTime(12, 30));
    e.own = true;
    DiaryEntryDialog d(DiaryEntryDialog::Mode::Own, e, m_book.dayTotal(day).kcal, target(), this);
    if (!d.editOwnRecipe()) return;   // zuerst die Speise im Editor erfassen
    // der Editor hat den Eintrag gefüllt – Dialog neu aufbauen
    DiaryEntryDialog d2(DiaryEntryDialog::Mode::Own, d.entry(), m_book.dayTotal(day).kcal, target(), this);
    if (d2.exec() != QDialog::Accepted) return;
    m_book.add(d2.entry());
    store();
    m_date->setDate(d2.entry().when.date());
    refresh();
}

void DiaryDialog::addFromPlan()
{
    const Profile *p = Profiles::instance().find(profileId());
    if (!p) return;
    const MealPlan plan = MealPlan::fromJson(p->lastPlan);
    const QDate day = m_date->date();
    const int idx = int(plan.start.daysTo(day));
    if (idx < 0 || idx >= plan.days.size()) return;
    QStringList titles;
    for (const PlanEntry &pe : plan.days[idx])
        if (!pe.recipeId.isEmpty()) titles << QStringLiteral("• %1 – %2").arg(pe.title, MealPlanner::portionText(pe));
    if (titles.isEmpty()) return;
    if (QMessageBox::question(this, tr("Aus Wochenplan"),
                              tr("Diese geplanten Mahlzeiten eintragen?\n\n%1\n\nUhrzeit und Menge lassen sich danach anpassen.").arg(titles.join(QLatin1Char('\n'))))
        != QMessageBox::Yes)
        return;
    // Mahlzeitnamen des Plans auf die des Tagebuchs abbilden
    const QStringList dm = Diary::mealNames();
    int added = 0;
    for (int m = 0; m < plan.days[idx].size(); ++m) {
        const PlanEntry &pe = plan.days[idx][m];
        if (pe.recipeId.isEmpty()) continue;
        Recipe r;
        Diary::Entry e;
        if (RecipeDb::instance().loadLight(pe.recipeId, &r)) {
            e = Diary::Entry::fromRecipe(r, false);
        } else {
            e.title = pe.title;
            e.recipeId = pe.recipeId;
            e.perServing.kcal = pe.portions > 0 ? pe.kcal / pe.portions : pe.kcal;
            e.perServing.protein = pe.portions > 0 ? pe.protein / pe.portions : 0;
            e.perServing.fat = pe.portions > 0 ? pe.fat / pe.portions : 0;
            e.perServing.carbs = pe.portions > 0 ? pe.carbs / pe.portions : 0;
        }
        e.setPortions(pe.portions);
        // seit 0.6.1 tragen Pläne die Namen des Tagebuchs; ältere: Frühstück, Mittag, Abend, Zwischenmahlzeit
        const QString planName = plan.meals.value(m);
        const QString mealName = dm.contains(planName) ? planName : m == 0 ? dm[0] : m == 1 ? dm[2] : m == 2 ? dm[4] : dm[3];
        e.meal = mealName;
        e.when = QDateTime(day, Diary::defaultTime(mealName));
        m_book.add(e);
        ++added;
    }
    store();
    refresh();
}

void DiaryDialog::editEntry()
{
    Diary::Entry *e = m_book.find(selectedId());
    if (!e) return;
    const double used = m_book.dayTotal(e->when.date()).kcal - e->eaten().kcal;
    DiaryEntryDialog d(DiaryEntryDialog::Mode::Edit, *e, used, target(), this);
    if (d.exec() != QDialog::Accepted) return;
    m_book.update(d.entry());
    store();
    m_date->setDate(d.entry().when.date());
    refresh();
}

void DiaryDialog::copyEntryToToday()
{
    const Diary::Entry *e = m_book.find(selectedId());
    if (!e) return;
    Diary::Entry c = *e;
    c.id.clear();
    c.when = QDateTime(QDate::currentDate(), e->when.time());
    m_book.add(c);
    store();
    m_date->setDate(QDate::currentDate());
    refresh();
}

void DiaryDialog::removeEntry()
{
    const Diary::Entry *e = m_book.find(selectedId());
    if (!e) return;
    if (QMessageBox::question(this, tr("Eintrag löschen"), tr("„%1“ (%2 Uhr) aus dem Tagebuch löschen?").arg(e->title, e->when.toString(QStringLiteral("HH:mm"))))
        != QMessageBox::Yes)
        return;
    m_book.remove(e->id);
    store();
    refresh();
}
