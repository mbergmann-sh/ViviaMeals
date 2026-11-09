// SPDX-License-Identifier: MIT AND LicenseRef-Commons-Clause-1.0 (siehe LICENSE)
#include "searchdialog.h"
#include "icons.h"
#include "recipedb.h"
#include "recipehtml.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

SearchDialog::SearchDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Rezepte suchen"));
    auto *lay = new QVBoxLayout(this);
    auto *split = new QSplitter(Qt::Horizontal);

    auto *left = new QWidget;
    auto *ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    auto *form = new QFormLayout;
    m_text = new QLineEdit;
    m_text->setPlaceholderText(tr("Rezeptname oder Teil davon …"));
    m_text->setClearButtonEnabled(true);
    form->addRow(tr("&Suchbegriff:"), m_text);
    m_inIngredients = new QCheckBox(tr("auch in den &Zutaten suchen"));
    m_inIngredients->setChecked(QSettings().value(QStringLiteral("search/ingredients"), true).toBool());
    form->addRow(QString(), m_inIngredients);
    m_maxKcal = new QSpinBox;
    m_maxKcal->setRange(0, 5000);
    m_maxKcal->setSingleStep(50);
    m_maxKcal->setSpecialValueText(tr("beliebig"));
    m_maxKcal->setSuffix(tr(" kcal"));
    form->addRow(tr("höchstens je Portion:"), m_maxKcal);
    m_onlyFav = new QCheckBox(tr("nur Favoriten"));
    m_onlyImage = new QCheckBox(tr("nur mit Foto"));
    form->addRow(QString(), m_onlyFav);
    form->addRow(QString(), m_onlyImage);
    ll->addLayout(form);
    auto *tagBox = new QGroupBox(tr("&Tags"));
    auto *tl = new QVBoxLayout(tagBox);
    m_all = new QCheckBox(tr("alle gewählten Tags müssen passen"));
    m_all->setChecked(true);
    tl->addWidget(m_all);
    m_tags = new QListWidget;
    const auto &db = RecipeDb::instance();
    QHash<QString, int> counts;
    for (const RecipeSummary &s : db.summaries())
        for (const QString &t : s.tags) counts[t.toLower()]++;
    QStringList tags = counts.keys();
    std::sort(tags.begin(), tags.end(), [](const QString &a, const QString &b) { return QString::localeAwareCompare(a, b) < 0; });
    for (const QString &t : tags) {
        auto *it = new QListWidgetItem(QStringLiteral("%1 (%2)").arg(RecipeHtml::tagLabel(t)).arg(counts.value(t)));
        it->setData(Qt::UserRole, t);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Unchecked);
        m_tags->addItem(it);
    }
    tl->addWidget(m_tags);
    ll->addWidget(tagBox, 1);
    split->addWidget(left);

    auto *right = new QWidget;
    auto *rl = new QVBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    m_count = new QLabel;
    rl->addWidget(m_count);
    m_results = new QTableWidget(0, 3);
    m_results->setHorizontalHeaderLabels({ tr("Rezept"), tr("kcal/Portion"), tr("Tags") });
    m_results->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_results->verticalHeader()->hide();
    m_results->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_results->setSelectionMode(QAbstractItemView::SingleSelection);
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->setSortingEnabled(true);
    rl->addWidget(m_results, 1);
    split->addWidget(right);
    split->setStretchFactor(1, 2);
    lay->addWidget(split, 1);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Anzeigen"));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, [this] {
        if (!selectedId().isEmpty()) accept();
    });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_results, &QTableWidget::cellDoubleClicked, this, &QDialog::accept);
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(200);
    connect(timer, &QTimer::timeout, this, &SearchDialog::search);
    connect(m_text, &QLineEdit::textChanged, timer, qOverload<>(&QTimer::start));
    connect(m_inIngredients, &QCheckBox::toggled, this, [this](bool on) {
        QSettings().setValue(QStringLiteral("search/ingredients"), on);
        search();
    });
    connect(m_all, &QCheckBox::toggled, this, &SearchDialog::search);
    connect(m_onlyFav, &QCheckBox::toggled, this, &SearchDialog::search);
    connect(m_onlyImage, &QCheckBox::toggled, this, &SearchDialog::search);
    connect(m_maxKcal, qOverload<int>(&QSpinBox::valueChanged), timer, qOverload<>(&QTimer::start));
    connect(m_tags, &QListWidget::itemChanged, this, &SearchDialog::search);
    search();
    resize(900, 600);
}

bool SearchDialog::matches(const RecipeSummary &s, const QString &text, bool inIngredients)
{
    const QStringList words = text.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &w : words) {
        bool hit = s.title.toLower().contains(w);
        if (!hit)
            for (const QString &t : s.tags)
                if (t.contains(w)) hit = true;
        if (!hit && inIngredients)
            for (const QString &i : s.ingredientNames)
                if (i.contains(w)) hit = true;
        if (!hit) return false;
    }
    return true;
}

void SearchDialog::search()
{
    QStringList tags;
    for (int i = 0; i < m_tags->count(); ++i)
        if (m_tags->item(i)->checkState() == Qt::Checked) tags << m_tags->item(i)->data(Qt::UserRole).toString();
    m_results->setSortingEnabled(false);
    m_results->setRowCount(0);
    for (const RecipeSummary &s : RecipeDb::instance().summaries()) {
        if (!matches(s, m_text->text(), m_inIngredients->isChecked())) continue;
        if (!tags.isEmpty()) {
            int hits = 0;
            for (const QString &t : tags)
                if (s.tags.contains(t, Qt::CaseInsensitive)) ++hits;
            if (m_all->isChecked() ? hits < tags.size() : hits == 0) continue;
        }
        if (m_maxKcal->value() > 0 && (!s.hasNutrition || s.perServing.kcal > m_maxKcal->value())) continue;
        if (m_onlyFav->isChecked() && !s.favorite) continue;
        if (m_onlyImage->isChecked() && !s.hasImage) continue;
        const int r = m_results->rowCount();
        m_results->insertRow(r);
        auto *t = new QTableWidgetItem(s.title);
        t->setData(Qt::UserRole, s.id);
        if (s.favorite) t->setIcon(Icons::get(QStringLiteral("star-filled")));
        m_results->setItem(r, 0, t);
        auto *k = new QTableWidgetItem;
        if (s.hasNutrition) k->setData(Qt::DisplayRole, qRound(s.perServing.kcal));
        k->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_results->setItem(r, 1, k);
        m_results->setItem(r, 2, new QTableWidgetItem(s.tags.join(QStringLiteral(", "))));
    }
    m_results->setSortingEnabled(true);
    m_results->sortByColumn(0, Qt::AscendingOrder);
    m_count->setText(tr("%1 von %2 Rezepten").arg(m_results->rowCount()).arg(RecipeDb::instance().count()));
    if (m_results->rowCount() > 0) m_results->selectRow(0);
}

QString SearchDialog::selectedId() const
{
    const int r = m_results->currentRow();
    if (r < 0) return {};
    return m_results->item(r, 0)->data(Qt::UserRole).toString();
}
