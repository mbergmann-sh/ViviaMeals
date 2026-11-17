// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "maintenancewindow.h"

#include "icons.h"
#include "recipe.h"
#include "recipedb.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {
const int kIdRole = Qt::UserRole + 1;

// Spalte „erfasst“ nach Zahl sortieren, nicht nach Text („100 %“ vor „40 %“)
class Item : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;
    bool operator<(const QTreeWidgetItem &o) const override
    {
        const int c = treeWidget() ? treeWidget()->sortColumn() : 0;
        if (c == 1) {
            const int a = data(1, Qt::UserRole).toInt(), b = o.data(1, Qt::UserRole).toInt();
            if (a != b) return a < b;
            return text(0).localeAwareCompare(o.text(0)) < 0;
        }
        return text(c).localeAwareCompare(o.text(c)) < 0;
    }
};
} // namespace

QString MaintenanceWindow::title(Mode mode)
{
    return mode == MissingNutrition ? tr("Rezepte ohne Nährwerte bearbeiten") : tr("Rezepte ohne Zutatenliste bearbeiten");
}

MaintenanceWindow::MaintenanceWindow(Mode mode, QWidget *mainWindow)
    : QWidget(mainWindow, Qt::Tool), m_mode(mode), m_main(mainWindow)
{
    setAttribute(Qt::WA_DeleteOnClose);
    // Lage bestimmt placeBesideMain(), nicht die gemerkte Fenstergeometrie
    setProperty("vbGeometryRestored", true);
    setWindowTitle(title(mode));
    setWindowIcon(Icons::get(mode == MissingNutrition ? QStringLiteral("maintain-nutrition") : QStringLiteral("maintain-ingredients")));
    auto *lay = new QVBoxLayout(this);
    m_info = new QLabel;
    m_info->setWordWrap(true);
    lay->addWidget(m_info);
    m_filter = new QLineEdit;
    m_filter->setPlaceholderText(tr("Liste filtern (Titel) …"));
    m_filter->setClearButtonEnabled(true);
    lay->addWidget(m_filter);
    m_list = new QTreeWidget;
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setSortingEnabled(true);
    m_list->setAlternatingRowColors(true);
    if (mode == MissingNutrition) {
        m_list->setHeaderLabels({ tr("Rezept"), tr("erfasst") });
        m_list->headerItem()->setToolTip(1, tr("Anteil der Zutaten, für die Nährwerte bekannt sind"));
        m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_list->header()->setStretchLastSection(false);
    } else {
        m_list->setHeaderLabels({ tr("Rezept") });
    }
    m_list->setToolTip(tr("Klick öffnet das Rezept im Editor"));
    lay->addWidget(m_list, 1);
    m_details = new QLabel;
    m_details->setWordWrap(true);
    m_details->setTextFormat(Qt::RichText);
    m_details->setMinimumHeight(m_details->fontMetrics().lineSpacing() * 3);
    m_details->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    lay->addWidget(m_details);
    auto *bl = new QHBoxLayout;
    auto *reload = new QPushButton(Icons::get(QStringLiteral("refresh")), tr("&Aktualisieren"));
    bl->addWidget(reload);
    bl->addStretch(1);
    auto *close = new QPushButton(tr("&Schließen"));
    bl->addWidget(close);
    lay->addLayout(bl);

    m_refresh = new QTimer(this);
    m_refresh->setSingleShot(true);
    m_refresh->setInterval(300);
    connect(m_refresh, &QTimer::timeout, this, &MaintenanceWindow::refresh);
    connect(&RecipeDb::instance(), &RecipeDb::changed, m_refresh, qOverload<>(&QTimer::start));
    connect(&RecipeDb::instance(), &RecipeDb::opened, m_refresh, qOverload<>(&QTimer::start));
    connect(reload, &QPushButton::clicked, this, &MaintenanceWindow::refresh);
    connect(close, &QPushButton::clicked, this, &QWidget::close);
    connect(m_filter, &QLineEdit::textChanged, this, &MaintenanceWindow::applyFilter);
    connect(m_list, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *it) { open(it); });
    connect(m_list, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *it) { open(it); });   // Eingabetaste
    connect(m_list, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *it) { showDetails(it); });
    refresh();
}

void MaintenanceWindow::placeBesideMain()
{
    QSettings s;
    const int w = std::max(300, s.value(QStringLiteral("maintenance/width"), 400).toInt());
    const QRect main = m_main ? m_main->frameGeometry() : QRect();
    QScreen *scr = m_main && m_main->screen() ? m_main->screen() : QApplication::primaryScreen();
    const QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1280, 800);
    const int frame = frameGeometry().width() - geometry().width();   // Fensterrahmen (vor dem ersten Anzeigen 0)
    const int h = std::min(avail.height() - 40, main.isValid() ? main.height() - 40 : 640);
    int x = main.isValid() ? main.right() + 1 : avail.right() - w;
    // passt es nicht daneben, an den rechten Bildschirmrand (über das Hauptfenster)
    if (x + w + frame > avail.right()) x = avail.right() - w - frame - 8;
    const int y = main.isValid() ? std::max(avail.top(), main.top()) : avail.top() + 40;
    resize(w, std::max(320, h));
    move(std::max(avail.left(), x), y);
}

void MaintenanceWindow::hideEvent(QHideEvent *e)
{
    QSettings().setValue(QStringLiteral("maintenance/width"), width());
    QWidget::hideEvent(e);
}

void MaintenanceWindow::refresh()
{
    RecipeDb &db = RecipeDb::instance();
    const QString keep = m_list->currentItem() ? m_list->currentItem()->data(0, kIdRole).toString() : QString();
    const int keepRow = m_list->currentItem() ? m_list->indexOfTopLevelItem(m_list->currentItem()) : -1;
    m_list->setSortingEnabled(false);
    m_list->clear();
    int total = 0;
    if (db.isOpen()) {
        for (const RecipeSummary &s : db.summaries()) {
            const bool noIngredients = s.ingredientNames.isEmpty();
            bool listed = false;
            if (m_mode == MissingIngredients) listed = noIngredients;
            else listed = !noIngredients && (!s.hasNutrition || s.coverage < 0.999);
            if (!listed) continue;
            ++total;
            auto *it = new Item(m_list);
            it->setText(0, s.title.isEmpty() ? tr("(ohne Titel)") : s.title);
            it->setData(0, kIdRole, s.id);
            if (m_mode == MissingNutrition) {
                const int pct = s.hasNutrition ? int(std::floor(s.coverage * 100)) : 0;
                it->setText(1, QStringLiteral("%1 %").arg(pct));
                it->setData(1, Qt::UserRole, pct);
                it->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
            }
        }
    }
    m_list->setSortingEnabled(true);
    m_list->sortItems(m_mode == MissingNutrition ? 1 : 0, Qt::AscendingOrder);
    if (!db.isOpen())
        m_info->setText(tr("Keine Datenbank geöffnet."));
    else if (total == 0)
        m_info->setText(m_mode == MissingNutrition ? tr("<b>Alles erledigt:</b> Bei allen Rezepten mit Zutaten sind die Nährwerte vollständig.")
                                                   : tr("<b>Alles erledigt:</b> Alle Rezepte haben eine Zutatenliste."));
    else
        m_info->setText(m_mode == MissingNutrition
                            ? (total == 1 ? tr("<b>1 Rezept</b> hat keine oder unvollständige Nährwerte")
                                          : tr("<b>%1 Rezepte</b> haben keine oder unvollständige Nährwerte").arg(total))
                                  + tr(" – meist fehlt ein Lebensmittel in der Nährwerttabelle oder eine Menge lässt sich nicht in Gramm "
                                       "umrechnen. Klicken Sie ein Rezept an, um es im Editor zu bearbeiten.")
                            : (total == 1 ? tr("<b>1 Rezept</b> hat keine Zutatenliste.") : tr("<b>%1 Rezepte</b> haben keine Zutatenliste.").arg(total))
                                  + tr(" Klicken Sie ein Rezept an, um die Zutaten im Editor nachzutragen."));
    applyFilter();
    // Auswahl halten bzw. nach dem Bearbeiten zum nächsten Rezept weiterrücken
    QTreeWidgetItem *sel = nullptr;
    for (int i = 0; i < m_list->topLevelItemCount() && !keep.isEmpty(); ++i)
        if (m_list->topLevelItem(i)->data(0, kIdRole).toString() == keep) sel = m_list->topLevelItem(i);
    if (!sel && m_list->topLevelItemCount() > 0)
        sel = m_list->topLevelItem(keepRow < 0 ? 0 : std::min(keepRow, m_list->topLevelItemCount() - 1));
    if (sel) {
        m_list->blockSignals(true);   // nur auswählen, nicht erneut öffnen
        m_list->setCurrentItem(sel);
        m_list->blockSignals(false);
        m_list->scrollToItem(sel);
        showDetails(sel);
    } else {
        showDetails(nullptr);
    }
}

void MaintenanceWindow::applyFilter()
{
    const QString f = m_filter->text().trimmed();
    for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = m_list->topLevelItem(i);
        it->setHidden(!f.isEmpty() && !it->text(0).contains(f, Qt::CaseInsensitive));
    }
}

void MaintenanceWindow::showDetails(QTreeWidgetItem *item)
{
    if (!item) {
        m_details->clear();
        return;
    }
    Recipe r;
    if (!RecipeDb::instance().loadLight(item->data(0, kIdRole).toString(), &r)) {
        m_details->clear();
        return;
    }
    if (m_mode == MissingNutrition) {
        QStringList missing = r.nutrition.unmatched;
        missing.removeDuplicates();
        if (missing.isEmpty())
            m_details->setText(tr("Für keine Zutat mit Mengenangabe sind Nährwerte bekannt."));
        else
            m_details->setText(tr("<b>Ohne Nährwerte:</b> %1").arg(missing.mid(0, 12).join(QStringLiteral(", ")).toHtmlEscaped()
                                                                   + (missing.size() > 12 ? QStringLiteral(" …") : QString())));
    } else {
        QString text = r.instructions.simplified();
        if (text.size() > 220) text = text.left(220) + QStringLiteral(" …");
        m_details->setText(text.isEmpty() ? tr("Auch die Zubereitung ist leer.") : tr("<b>Zubereitung:</b> %1").arg(text.toHtmlEscaped()));
    }
}

void MaintenanceWindow::open(QTreeWidgetItem *item)
{
    if (!item) return;
    const QString id = item->data(0, kIdRole).toString();
    if (id.isEmpty()) return;
    m_lastOpened = id;
    emit editRequested(id);
}
