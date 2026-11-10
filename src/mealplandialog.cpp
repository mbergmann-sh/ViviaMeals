// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "mealplandialog.h"
#include "fridge.h"
#include "icons.h"
#include "recipedb.h"
#include "searchdialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include "importdialogs.h"
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTextDocument>
#include <QVBoxLayout>

MealPlanDialog::MealPlanDialog(const QString &profileId, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Wochenmenü-Plan"));
    setWindowFlag(Qt::WindowMaximizeButtonHint, true);
    auto *lay = new QVBoxLayout(this);
    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Für:")));
    m_profile = new QComboBox;
    m_profile->addItem(tr("ohne Profil"), QString());
    for (const Profile &p : Profiles::instance().all()) m_profile->addItem(Icons::get(QStringLiteral("user")), p.name, p.id);
    top->addWidget(m_profile, 1);
    top->addWidget(new QLabel(tr("Ziel:")));
    m_kcal = new QSpinBox;
    m_kcal->setRange(1000, 5000);
    m_kcal->setSingleStep(50);
    m_kcal->setValue(2000);
    m_kcal->setSuffix(tr(" kcal/Tag"));
    top->addWidget(m_kcal);
    top->addWidget(new QLabel(tr("ab:")));
    m_start = new QDateEdit(QDate::currentDate());
    m_start->setCalendarPopup(true);
    m_start->setDisplayFormat(QStringLiteral("ddd dd.MM.yyyy"));
    top->addWidget(m_start);
    m_days = new QSpinBox;
    m_days->setRange(1, 14);
    m_days->setValue(7);
    m_days->setSuffix(tr(" Tage"));
    top->addWidget(m_days);
    m_snack = new QCheckBox(tr("Zwischenmahlzeit"));
    m_snack->setToolTip(tr("Abnehmen/Halten: eine Zwischenmahlzeit am Nachmittag · Zunehmen: zusätzlich eine Spätmahlzeit"));
    top->addWidget(m_snack);
    m_warm = new QComboBox;
    m_warm->addItem(tr("warm mittags"), false);
    m_warm->addItem(tr("warm abends"), true);
    m_warm->setToolTip(tr("Eine warme Hauptmahlzeit am Tag, die andere ist leicht (Salat, Suppe, Brotzeit …).\n"
                          "Beim Zunehmen dürfen beide Hauptmahlzeiten warm sein."));
    top->addWidget(m_warm);
    auto *make = new QPushButton(Icons::get(QStringLiteral("dice")), tr("Neu planen"));
    top->addWidget(make);
    lay->addLayout(top);

    m_table = new QTableWidget;
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setWordWrap(true);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    lay->addWidget(m_table, 1);
    m_info = new QLabel;
    m_info->setWordWrap(true);
    lay->addWidget(m_info);
    auto *nl = new QHBoxLayout;
    m_notes = new QLabel;
    m_notes->setWordWrap(true);
    m_notes->setStyleSheet(QStringLiteral("QLabel { color: #b85400; }"));
    nl->addWidget(m_notes, 1);
    m_healthy = new QPushButton(Icons::get(QStringLiteral("add")), tr("Gesunde Vorschläge übernehmen"));
    m_healthy->setToolTip(tr("Mitgelieferte Rezepte für Frühstück, Zwischenmahlzeiten und leichte Abendessen (Porridge, Vollkornbrot mit Quark, "
                             "Gemüsesticks mit Dip, Linsensalat, Tomatensuppe, Omelett mit Gemüse …) in die Datenbank übernehmen"));
    nl->addWidget(m_healthy);
    lay->addLayout(nl);
    connect(m_healthy, &QPushButton::clicked, this, [this] {
        const int n = ImportFlow::addHealthySuggestions(this);
        if (n > 0) create();
    });

    auto *bl = new QHBoxLayout;
    auto *shop = new QPushButton(Icons::get(QStringLiteral("cart")), tr("Einkaufsliste …"));
    auto *pr = new QPushButton(Icons::get(QStringLiteral("print")), tr("Drucken …"));
    auto *sv = new QPushButton(Icons::get(QStringLiteral("save")), tr("Im Profil speichern"));
    bl->addWidget(shop);
    bl->addWidget(pr);
    bl->addWidget(sv);
    bl->addStretch(1);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    bl->addWidget(bb);
    lay->addLayout(bl);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(make, &QPushButton::clicked, this, &MealPlanDialog::create);
    connect(shop, &QPushButton::clicked, this, &MealPlanDialog::shoppingList);
    connect(pr, &QPushButton::clicked, this, &MealPlanDialog::print);
    connect(sv, &QPushButton::clicked, this, &MealPlanDialog::save);
    connect(m_profile, qOverload<int>(&QComboBox::currentIndexChanged), this, &MealPlanDialog::profileChanged);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &MealPlanDialog::cellMenu);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int r, int c) {
        if (r < m_plan.days.size() && c < m_plan.meals.size() && !m_plan.days[r][c].recipeId.isEmpty()) emit showRecipe(m_plan.days[r][c].recipeId);
    });

    const int idx = m_profile->findData(profileId);
    m_profile->setCurrentIndex(idx >= 0 ? idx : (m_profile->count() > 1 ? 1 : 0));
    profileChanged();
    resize(1100, 640);
}

MealPlanner::Options MealPlanDialog::options() const
{
    MealPlanner::Options o;
    if (Profile *p = Profiles::instance().find(m_profile->currentData().toString())) o = MealPlanner::fromProfile(*p);
    o.targetKcal = m_kcal->value();
    o.snack = m_snack->isChecked();
    o.warmEvening = m_warm->currentData().toBool();
    o.days = m_days->value();
    o.start = m_start->date();
    return o;
}

void MealPlanDialog::profileChanged()
{
    Profile *p = Profiles::instance().find(m_profile->currentData().toString());
    if (p) {
        const double t = p->targetKcal();
        if (t > 0) m_kcal->setValue(qRound(t / 10.0) * 10);
        m_snack->setChecked(p->snack);
        m_warm->setCurrentIndex(p->warmEvening ? 1 : 0);
        if (!p->lastPlan.isEmpty()) {
            m_plan = MealPlan::fromJson(p->lastPlan);
            if (!m_plan.isEmpty()) {
                m_start->setDate(m_plan.start);
                m_days->setValue(int(m_plan.days.size()));
                fillTable();
                m_info->setText(tr("Gespeicherter Plan geladen. „Neu planen“ erstellt einen neuen."));
                return;
            }
        }
    }
    create();
}

void MealPlanDialog::create()
{
    const MealPlanner::Options o = options();
    const int n = MealPlanner::candidateCount(o);
    if (n == 0) {
        m_plan = MealPlan();
        m_table->clear();
        m_table->setRowCount(0);
        m_table->setColumnCount(0);
        updateNotes();
        m_info->setText(tr("<b>Keine passenden Rezepte.</b> Für den Wochenplan werden Rezepte mit berechneten Nährwerten gebraucht, "
                           "die zur Ernährungsform und den ausgeschlossenen Zutaten passen."));
        return;
    }
    // Einstellungen „Zwischenmahlzeit“ und „warm mittags/abends“ im Profil merken
    if (Profile *p = Profiles::instance().find(m_profile->currentData().toString())) {
        if (p->snack != o.snack || p->warmEvening != o.warmEvening) {
            p->snack = o.snack;
            p->warmEvening = o.warmEvening;
            Profiles::instance().save();
        }
    }
    m_plan = MealPlanner::create(o);
    m_plan.profileId = m_profile->currentData().toString();
    fillTable();
}

void MealPlanDialog::fillTable()
{
    const QLocale de(QLocale::German);
    m_table->clear();
    m_table->setRowCount(int(m_plan.days.size()));
    m_table->setColumnCount(int(m_plan.meals.size()) + 1);
    QStringList head;
    for (int m = 0; m < m_plan.meals.size(); ++m) {
        const MealPlanner::Role r = m_plan.roles.value(m, MealPlanner::Role::Warm);
        head << ((r == MealPlanner::Role::Warm || r == MealPlanner::Role::Light)
                     ? QStringLiteral("%1\n(%2)").arg(m_plan.meals[m], MealPlanner::roleName(r))
                     : m_plan.meals[m]);
    }
    head << tr("Summe");
    m_table->setHorizontalHeaderLabels(head);
    QStringList rows;
    double sum = 0;
    for (int d = 0; d < m_plan.days.size(); ++d) {
        rows << de.toString(m_plan.start.addDays(d), QStringLiteral("ddd\ndd.MM."));
        for (int m = 0; m < m_plan.days[d].size(); ++m) {
            const PlanEntry &e = m_plan.days[d][m];
            auto *it = new QTableWidgetItem(e.recipeId.isEmpty() ? tr("— kein passendes Rezept —")
                                                                 : QStringLiteral("%1\n%2 · %3 kcal").arg(e.title, MealPlanner::portionText(e))
                                                                       .arg(qRound(e.kcal)));
            if (e.recipeId.isEmpty()) it->setForeground(QColor(0xd0, 0x60, 0x10));
            it->setToolTip(tr("Doppelklick: Rezept anzeigen · Rechtsklick: austauschen"));
            m_table->setItem(d, m, it);
        }
        const double k = m_plan.dayKcal(d);
        sum += k;
        auto *s = new QTableWidgetItem(QStringLiteral("%1 kcal").arg(qRound(k)));
        s->setTextAlignment(Qt::AlignCenter);
        const double dev = m_plan.targetKcal > 0 ? (k - m_plan.targetKcal) / m_plan.targetKcal : 0;
        if (std::abs(dev) > 0.15) s->setForeground(QColor(0xd0, 0x60, 0x10));
        m_table->setItem(d, int(m_plan.meals.size()), s);
    }
    m_table->setVerticalHeaderLabels(rows);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(int(m_plan.meals.size()), QHeaderView::ResizeToContents);
    Q_UNUSED(sum)
    if (!m_plan.days.isEmpty())
        m_info->setText(MealPlanner::summary(m_plan).toHtmlEscaped() + QStringLiteral("<br/><small>")
                        + tr("Eine warme Hauptmahlzeit am Tag, die zweite leicht (beim Zunehmen dürfen beide warm sein); beide ohne gleiche Beilage und nicht "
                             "zweimal Fleisch oder Fisch. Portionen gerundet (Zwischenmahlzeit auf halbe, sonst auf Viertel; höchstens etwa 450 g zum "
                             "Frühstück, 350 g als Zwischenmahlzeit, 500 g als leichte und 750 g als warme Mahlzeit), Gramm = Gewicht der "
                             "Zutaten. Rechtsklick auf eine Mahlzeit, um sie auszutauschen.")
                        + QStringLiteral("</small>"));
    updateNotes();
}

void MealPlanDialog::updateNotes()
{
    const QStringList n = MealPlanner::notes(options());
    m_notes->setText(n.join(QLatin1Char('\n')));
    m_notes->setVisible(!n.isEmpty());
    const QList<int> c = MealPlanner::slotCounts(options());
    const MealPlanner::Layout lay = MealPlanner::layout(options());
    m_healthy->setVisible(c.value(0) < 3 || (lay.roles.contains(MealPlanner::Role::Snack) && c.value(3) < 3)
                          || (lay.roles.contains(MealPlanner::Role::Light) && c.value(2) < 3));
}

void MealPlanDialog::cellMenu(const QPoint &pos)
{
    const QModelIndex idx = m_table->indexAt(pos);
    if (!idx.isValid() || idx.column() >= m_plan.meals.size()) return;
    const int d = idx.row(), m = idx.column();
    QMenu menu(this);
    menu.addAction(Icons::get(QStringLiteral("dice")), tr("Anderes Rezept vorschlagen"), this, [this, d, m] {
        MealPlanner::reroll(m_plan, d, m, options());
        fillTable();
    });
    menu.addAction(Icons::get(QStringLiteral("search")), tr("Rezept auswählen …"), this, [this, d, m] { chooseRecipe(d, m); });
    if (!m_plan.days[d][m].recipeId.isEmpty())
        menu.addAction(Icons::get(QStringLiteral("recipe")), tr("Rezept anzeigen"), this, [this, d, m] { emit showRecipe(m_plan.days[d][m].recipeId); });
    menu.addAction(Icons::get(QStringLiteral("delete")), tr("Mahlzeit leeren"), this, [this, d, m] {
        m_plan.days[d][m] = PlanEntry{};
        fillTable();
    });
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void MealPlanDialog::chooseRecipe(int day, int meal)
{
    SearchDialog d(this);
    if (d.exec() != QDialog::Accepted || d.selectedId().isEmpty()) return;
    const RecipeSummary *s = RecipeDb::instance().summary(d.selectedId());
    if (!s || !s->hasNutrition) {
        QMessageBox::information(this, windowTitle(), tr("Für dieses Rezept sind keine Nährwerte berechnet."));
        return;
    }
    MealPlanner::setRecipe(m_plan, day, meal, d.selectedId());
    fillTable();
}

void MealPlanDialog::shoppingList()
{
    if (m_plan.isEmpty()) return;
    QDialog dlg(this);
    dlg.setObjectName(QStringLiteral("shoppingListDialog"));
    dlg.setWindowTitle(tr("Einkaufsliste zum Wochenplan"));
    auto *lay = new QVBoxLayout(&dlg);
    auto *useStock = new QCheckBox(tr("Was laut Kühlschrank-Liste vorhanden ist, nicht aufführen"));
    useStock->setChecked(true);
    lay->addWidget(useStock);
    auto *view = new QTextBrowser;
    lay->addWidget(view, 1);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    auto *copy = bb->addButton(tr("Kopieren"), QDialogButtonBox::ActionRole);
    auto *pr = bb->addButton(tr("Drucken …"), QDialogButtonBox::ActionRole);
    auto *sv = bb->addButton(tr("Speichern …"), QDialogButtonBox::ActionRole);
    copy->setIcon(Icons::get(QStringLiteral("copy")));
    pr->setIcon(Icons::get(QStringLiteral("print")));
    sv->setIcon(Icons::get(QStringLiteral("save")));
    lay->addWidget(bb);
    QString text;
    const QString title = tr("Einkaufsliste %1 – %2")
                              .arg(m_plan.start.toString(QStringLiteral("dd.MM.")), m_plan.start.addDays(int(m_plan.days.size()) - 1).toString(QStringLiteral("dd.MM.yyyy")));
    auto fill = [&] {
        const QList<ShoppingItem> list = MealPlanner::shoppingList(m_plan, useStock->isChecked() ? Fridge::stock() : QStringList());
        view->setHtml(Shopping::toHtml(list, title));
        text = Shopping::toText(list, title);
    };
    fill();
    connect(useStock, &QCheckBox::toggled, &dlg, fill);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
    connect(copy, &QPushButton::clicked, &dlg, [&] { QApplication::clipboard()->setText(text); });
    connect(pr, &QPushButton::clicked, &dlg, [&] {
        QPrinter printer(QPrinter::HighResolution);
        QPrintPreviewDialog pd(&printer, &dlg);
        connect(&pd, &QPrintPreviewDialog::paintRequested, &dlg, [&](QPrinter *p) { view->document()->print(p); });
        pd.exec();
    });
    connect(sv, &QPushButton::clicked, &dlg, [&] {
        const QString f = QFileDialog::getSaveFileName(&dlg, tr("Einkaufsliste speichern"), tr("Einkaufsliste.txt"), tr("Text (*.txt)"));
        if (f.isEmpty()) return;
        QSaveFile out(f);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(text.toUtf8());
            out.commit();
        }
    });
    dlg.resize(560, 640);
    dlg.exec();
}

void MealPlanDialog::print()
{
    if (m_plan.isEmpty()) return;
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageOrientation(QPageLayout::Landscape);
    QPrintPreviewDialog pd(&printer, this);
    connect(&pd, &QPrintPreviewDialog::paintRequested, this, [this](QPrinter *p) {
        QTextDocument doc;
        doc.setHtml(MealPlanner::toHtml(m_plan, m_profile->currentText()));
        doc.print(p);
    });
    pd.exec();
}

void MealPlanDialog::save()
{
    Profile *p = Profiles::instance().find(m_profile->currentData().toString());
    if (!p) {
        QMessageBox::information(this, windowTitle(), tr("Bitte zuerst eine Person wählen (Assistent Gewichtskontrolle)."));
        return;
    }
    p->lastPlan = m_plan.toJson();
    Profiles::instance().save();
    m_info->setText(tr("Der Wochenplan wurde im Profil von %1 gespeichert.").arg(p->name));
}
