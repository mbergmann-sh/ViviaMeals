// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "duplicatesdialog.h"
#include "symbols.h"
#include "editordialog.h"
#include "icons.h"
#include "recipedb.h"
#include "recipehtml.h"
#include "recipeview.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr int IdRole = Qt::UserRole;
constexpr int GroupRole = Qt::UserRole + 1;
}

DuplicatesDialog::DuplicatesDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Doubletten suchen"));
    setWindowFlag(Qt::WindowMaximizeButtonHint, true);
    auto *lay = new QVBoxLayout(this);
    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Suche:")));
    m_strict = new QComboBox;
    m_strict->addItem(tr("nur gleiche Titel"), int(Duplicates::Strictness::TitleOnly));
    m_strict->addItem(tr("gleiche und ähnliche Titel, gleiche Zutaten (empfohlen)"), int(Duplicates::Strictness::Normal));
    m_strict->addItem(tr("großzügig (auch entferntere Ähnlichkeit)"), int(Duplicates::Strictness::Generous));
    m_strict->setCurrentIndex(qBound(0, QSettings().value(QStringLiteral("duplicates/strictness"), 1).toInt(), 2));
    top->addWidget(m_strict);
    auto *again = new QPushButton(Icons::get(QStringLiteral("search")), tr("Neu suchen"));
    top->addWidget(again);
    top->addStretch(1);
    lay->addLayout(top);
    m_info = new QLabel;
    m_info->setWordWrap(true);
    lay->addWidget(m_info);

    auto *split = new QSplitter(Qt::Horizontal);
    split->setObjectName(QStringLiteral("duplicatesSplit"));
    // links: Gruppen
    auto *left = new QWidget;
    auto *ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    m_tree = new QTreeWidget;
    m_tree->setColumnCount(7);
    m_tree->setHeaderLabels({ tr("Rezept"), tr("Portionen"), tr("kcal/Portion"), tr("Zutaten"), tr("Bild"), tr("Favorit"), tr("Geändert") });
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setAlternatingRowColors(true);
    m_tree->setRootIsDecorated(true);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < 7; ++c) m_tree->header()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    m_tree->header()->setStretchLastSection(false);
    ll->addWidget(m_tree, 1);
    auto *bl = new QHBoxLayout;
    m_edit = new QPushButton(Icons::get(QStringLiteral("edit")), tr("&Bearbeiten …"));
    m_delete = new QPushButton(Icons::get(QStringLiteral("delete")), tr("&Löschen …"));
    m_keep = new QPushButton(Icons::get(QStringLiteral("check")), tr("Dieses &behalten, übrige zusammenführen …"));
    m_keep->setToolTip(tr("Das ausgewählte Rezept bleibt. Fehlendes Bild, Quelle, Zubereitungszeit, alle Tags und die Favoriten-Markierung "
                          "werden aus den übrigen Rezepten der Gruppe übernommen; danach werden diese gelöscht."));
    m_ignore = new QPushButton(Icons::get(QStringLiteral("close")), tr("&Keine Doubletten"));
    m_ignore->setToolTip(tr("Die Gruppe (bzw. das ausgewählte Rezept) künftig nicht mehr als Doublette melden.\n"
                            "Gespeichert in der Datenbank (doubletten-ignoriert.json)."));
    for (QPushButton *b : { m_edit, m_delete, m_keep, m_ignore }) bl->addWidget(b);
    bl->addStretch(1);
    ll->addLayout(bl);
    split->addWidget(left);
    // rechts: Vergleich
    auto *right = new QWidget;
    auto *rl = new QVBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    auto *ch = new QHBoxLayout;
    ch->addWidget(new QLabel(tr("Ausgewählt")), 1);
    ch->addWidget(new QLabel(tr("Vergleich mit:")));
    m_compare = new QComboBox;
    m_compare->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_compare->setMinimumContentsLength(18);
    ch->addWidget(m_compare, 1);
    rl->addLayout(ch);
    auto *cmp = new QSplitter(Qt::Horizontal);
    m_left = new QTextBrowser;
    m_right = new QTextBrowser;
    cmp->addWidget(m_left);
    cmp->addWidget(m_right);
    rl->addWidget(cmp, 1);
    split->addWidget(right);
    split->setStretchFactor(0, 4);
    split->setStretchFactor(1, 6);
    lay->addWidget(split, 1);
    const QByteArray st = QSettings().value(QStringLiteral("duplicates/split")).toByteArray();
    if (!st.isEmpty()) split->restoreState(st);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(this, &QDialog::finished, this, [split] { QSettings().setValue(QStringLiteral("duplicates/split"), split->saveState()); });
    connect(again, &QPushButton::clicked, this, &DuplicatesDialog::search);
    connect(m_strict, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int i) {
        QSettings().setValue(QStringLiteral("duplicates/strictness"), i);
        search();
    });
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &DuplicatesDialog::selectionChanged);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it) {
        if (!it->data(0, IdRole).toString().isEmpty()) editCurrent();
    });
    connect(m_compare, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        showPreview(m_right, m_compare->currentData().toString());
    });
    connect(m_edit, &QPushButton::clicked, this, &DuplicatesDialog::editCurrent);
    connect(m_delete, &QPushButton::clicked, this, &DuplicatesDialog::deleteSelected);
    connect(m_keep, &QPushButton::clicked, this, &DuplicatesDialog::keepAndMerge);
    connect(m_ignore, &QPushButton::clicked, this, &DuplicatesDialog::notDuplicates);
    resize(1300, 760);
    search();
}

void DuplicatesDialog::search()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const auto strict = Duplicates::Strictness(m_strict->currentData().toInt());
    m_groups = Duplicates::find(RecipeDb::instance().summaries(), Duplicates::loadIgnored(), strict);
    QApplication::restoreOverrideCursor();
    fill(currentId());
}

void DuplicatesDialog::fill(const QString &selectId)
{
    const QLocale de(QLocale::German);
    RecipeDb &db = RecipeDb::instance();
    m_tree->blockSignals(true);
    m_tree->clear();
    QTreeWidgetItem *select = nullptr;
    int recipes = 0;
    for (int g = 0; g < m_groups.size(); ++g) {
        const Duplicates::Group &grp = m_groups[g];
        const RecipeSummary *first = db.summary(grp.ids.value(0));
        if (!first) continue;
        auto *gi = new QTreeWidgetItem(m_tree);
        gi->setText(0, tr("%1  (%2 Rezepte – %3)").arg(first->title).arg(grp.ids.size()).arg(Duplicates::reasonText(grp.reasons)));
        gi->setData(0, GroupRole, g);
        QFont f = gi->font(0);
        f.setBold(true);
        gi->setFont(0, f);
        gi->setFirstColumnSpanned(true);
        for (const QString &id : grp.ids) {
            const RecipeSummary *s = db.summary(id);
            if (!s) continue;
            ++recipes;
            auto *it = new QTreeWidgetItem(gi);
            it->setIcon(0, Icons::get(QStringLiteral("recipe")));
            it->setText(0, s->title);
            it->setText(1, Fmt::number(s->servings, 1));
            it->setText(2, s->hasNutrition ? QString::number(qRound(s->perServing.kcal)) : QStringLiteral("–"));
            it->setText(3, QString::number(s->ingredientNames.size()));
            it->setText(4, s->hasImage ? Sym::get(Sym::Check) : QString());
            it->setText(5, s->favorite ? Sym::get(Sym::Star) : QString());
            QFont sf;
            if (s->hasImage && Sym::font(Sym::Check, it->font(4), &sf)) {
                it->setFont(4, sf);
                it->setText(4, Sym::wanted(Sym::Check));
            }
            if (s->favorite && Sym::font(Sym::Star, it->font(5), &sf)) {
                it->setFont(5, sf);
                it->setText(5, Sym::wanted(Sym::Star));
            }
            it->setText(6, s->modified.isValid() ? de.toString(s->modified.date(), QLocale::ShortFormat) : QString());
            for (int c = 1; c < 7; ++c) it->setTextAlignment(c, Qt::AlignCenter);
            it->setData(0, IdRole, id);
            it->setData(0, GroupRole, g);
            it->setToolTip(0, id == grp.ids.first() ? tr("Vorschlag zum Behalten (vollständigstes Rezept)") : QString());
            if (id == grp.ids.first()) it->setIcon(0, Icons::get(QStringLiteral("check")));
            if (id == selectId) select = it;
        }
        gi->setExpanded(true);
    }
    m_tree->blockSignals(false);
    if (m_groups.isEmpty())
        m_info->setText(tr("<b>Keine Doubletten gefunden</b> unter %1 Rezepten.").arg(db.count()));
    else
        m_info->setText(tr("<b>%1 Gruppen</b> mit zusammen %2 Rezepten. Das Häkchen markiert jeweils das vollständigste Rezept "
                           "(Nährwerte, Bild, Zutaten, Favorit) als Vorschlag zum Behalten. Doppelklick bearbeitet ein Rezept.")
                            .arg(m_groups.size())
                            .arg(recipes));
    if (!select && m_tree->topLevelItemCount() > 0) select = m_tree->topLevelItem(0)->child(0);
    if (select) m_tree->setCurrentItem(select);
    selectionChanged();
}

QString DuplicatesDialog::currentId() const
{
    const QTreeWidgetItem *it = m_tree->currentItem();
    return it ? it->data(0, IdRole).toString() : QString();
}

QStringList DuplicatesDialog::selectedIds() const
{
    QStringList ids;
    for (const QTreeWidgetItem *it : m_tree->selectedItems()) {
        const QString id = it->data(0, IdRole).toString();
        if (!id.isEmpty() && !ids.contains(id)) ids << id;
    }
    return ids;
}

int DuplicatesDialog::currentGroup() const
{
    const QTreeWidgetItem *it = m_tree->currentItem();
    return it ? it->data(0, GroupRole).toInt() : -1;
}

void DuplicatesDialog::showPreview(QTextBrowser *b, const QString &id)
{
    Recipe r;
    if (id.isEmpty() || !RecipeDb::instance().load(id, &r)) {
        b->clear();
        return;
    }
    RecipeHtml::Style st = RecipeView::paletteStyle(120);
    b->setHtml(RecipeHtml::render(r, st));
}

void DuplicatesDialog::selectionChanged()
{
    const QString id = currentId();
    const int g = currentGroup();
    const bool recipe = !id.isEmpty();
    m_edit->setEnabled(recipe);
    m_delete->setEnabled(!selectedIds().isEmpty());
    m_keep->setEnabled(recipe && g >= 0 && g < m_groups.size());
    m_ignore->setEnabled(g >= 0 && g < m_groups.size());
    m_ignore->setText(recipe ? tr("&Keine Doublette") : tr("&Keine Doubletten"));
    QString shown = id;
    if (shown.isEmpty() && g >= 0 && g < m_groups.size()) shown = m_groups[g].ids.value(0);
    showPreview(m_left, shown);
    // Vergleichspartner: die anderen Rezepte der Gruppe
    m_compare->blockSignals(true);
    m_compare->clear();
    if (g >= 0 && g < m_groups.size()) {
        int n = 0;
        for (const QString &other : m_groups[g].ids) {
            ++n;
            if (other == shown) continue;
            if (const RecipeSummary *s = RecipeDb::instance().summary(other)) m_compare->addItem(QStringLiteral("%1. %2").arg(n).arg(s->title), other);
        }
    }
    m_compare->blockSignals(false);
    showPreview(m_right, m_compare->currentData().toString());
}

void DuplicatesDialog::editCurrent()
{
    const QString id = currentId();
    Recipe r;
    if (id.isEmpty() || !RecipeDb::instance().load(id, &r)) return;
    EditorDialog d(r, this);
    if (d.exec() != QDialog::Accepted) return;
    Recipe nr = d.recipe();
    QString err;
    if (!RecipeDb::instance().save(nr, &err)) {
        QMessageBox::warning(this, windowTitle(), err);
        return;
    }
    ++m_changes;
    search();
}

void DuplicatesDialog::deleteSelected()
{
    const QStringList ids = selectedIds();
    if (ids.isEmpty()) return;
    QStringList titles;
    for (const QString &id : ids)
        if (const RecipeSummary *s = RecipeDb::instance().summary(id)) titles << QStringLiteral("• ") + s->title;
    const QString q = ids.size() == 1 ? tr("Dieses Rezept endgültig löschen?\n\n%1").arg(titles.join(QLatin1Char('\n')))
                                      : tr("Diese %1 Rezepte endgültig löschen?\n\n%2").arg(ids.size()).arg(titles.join(QLatin1Char('\n')));
    if (QMessageBox::question(this, tr("Doubletten löschen"), q) != QMessageBox::Yes) return;
    RecipeDb &db = RecipeDb::instance();
    db.beginBatch();
    for (const QString &id : ids)
        if (db.remove(id)) ++m_changes;
    db.endBatch();
    search();
}

void DuplicatesDialog::keepAndMerge()
{
    const QString keepId = currentId();
    const int g = currentGroup();
    if (keepId.isEmpty() || g < 0 || g >= m_groups.size()) return;
    RecipeDb &db = RecipeDb::instance();
    Recipe keep;
    if (!db.load(keepId, &keep)) return;
    QList<Recipe> others;
    QStringList titles;
    for (const QString &id : m_groups[g].ids) {
        Recipe r;
        if (id == keepId || !db.load(id, &r)) continue;
        others << r;
        titles << QStringLiteral("• ") + r.title;
    }
    if (others.isEmpty()) return;
    Recipe merged = keep;
    const QStringList taken = Duplicates::mergeInto(merged, others);
    QString text = tr("„%1“ behalten und diese %2 Rezepte löschen?\n\n%3").arg(keep.title).arg(others.size()).arg(titles.join(QLatin1Char('\n')));
    text += QStringLiteral("\n\n") + (taken.isEmpty() ? tr("Es gibt nichts zu übernehmen.")
                                                      : tr("Übernommen werden: %1.").arg(taken.join(QStringLiteral("; "))));
    if (QMessageBox::question(this, tr("Doubletten zusammenführen"), text) != QMessageBox::Yes) return;
    QString err;
    if (!taken.isEmpty() && !db.save(merged, &err)) {
        QMessageBox::warning(this, windowTitle(), err);
        return;
    }
    db.beginBatch();
    for (const Recipe &r : others) db.remove(r.id);
    db.endBatch();
    m_changes += int(others.size()) + 1;
    search();
}

void DuplicatesDialog::notDuplicates()
{
    const int g = currentGroup();
    if (g < 0 || g >= m_groups.size()) return;
    const QString id = currentId();
    QSet<QString> ign = Duplicates::loadIgnored();
    const QStringList ids = m_groups[g].ids;
    if (!id.isEmpty()) {
        // nur dieses Rezept gegenüber den übrigen der Gruppe
        for (const QString &o : ids)
            if (o != id) ign.insert(Duplicates::pairKey(id, o));
    } else {
        for (int a = 0; a < ids.size(); ++a)
            for (int b = a + 1; b < ids.size(); ++b) ign.insert(Duplicates::pairKey(ids[a], ids[b]));
    }
    if (!Duplicates::saveIgnored(ign)) QMessageBox::warning(this, windowTitle(), tr("Die Ausnahmen konnten nicht gespeichert werden."));
    search();
}
