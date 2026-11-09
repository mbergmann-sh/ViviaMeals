// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "fridgedialog.h"
#include "foodtable.h"
#include "fridge.h"
#include "icons.h"
#include "recipedb.h"
#include "recipehtml.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTextDocument>
#include <QVBoxLayout>

FridgeDialog::FridgeDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Kühlschrank – was koche ich heute?"));
    setWindowFlag(Qt::WindowMaximizeButtonHint, true);
    auto *lay = new QVBoxLayout(this);
    auto *split = new QSplitter(Qt::Horizontal);

    // ---- Bestand
    auto *stockBox = new QGroupBox(tr("Im Kühlschrank / Vorrat"));
    auto *sl = new QVBoxLayout(stockBox);
    auto *il = new QHBoxLayout;
    m_input = new QLineEdit;
    m_input->setPlaceholderText(tr("z. B. Eier, Hackfleisch, Paprika …"));
    QStringList names;
    for (const Food *f : FoodTable::instance().all()) names << f->name;
    auto *comp = new QCompleter(names, this);
    comp->setCaseSensitivity(Qt::CaseInsensitive);
    comp->setFilterMode(Qt::MatchContains);
    m_input->setCompleter(comp);
    il->addWidget(m_input, 1);
    auto *add = new QPushButton(Icons::get(QStringLiteral("add")), QString());
    add->setToolTip(tr("Hinzufügen"));
    il->addWidget(add);
    sl->addLayout(il);
    m_stock = new QListWidget;
    m_stock->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_stock->addItems(Fridge::stock());
    sl->addWidget(m_stock, 1);
    auto *rl = new QHBoxLayout;
    auto *remove = new QPushButton(Icons::get(QStringLiteral("remove")), tr("Entfernen"));
    auto *clear = new QPushButton(Icons::get(QStringLiteral("delete")), tr("Leeren"));
    rl->addWidget(remove);
    rl->addWidget(clear);
    sl->addLayout(rl);
    sl->addWidget(new QLabel(tr("Grundvorrat (immer vorhanden):")));
    m_basics = new QLineEdit(Fridge::basics().join(QStringLiteral(", ")));
    sl->addWidget(m_basics);
    split->addWidget(stockBox);

    // ---- Vorschläge
    auto *sugBox = new QGroupBox(tr("Passende Rezepte"));
    auto *gl = new QVBoxLayout(sugBox);
    auto *fl = new QHBoxLayout;
    fl->addWidget(new QLabel(tr("Thema:")));
    m_tag = new QComboBox;
    m_tag->addItem(tr("alle"), QString());
    for (const QString &t : RecipeDb::instance().allTags()) m_tag->addItem(RecipeHtml::tagLabel(t), t);
    fl->addWidget(m_tag, 1);
    fl->addWidget(new QLabel(tr("höchstens fehlend:")));
    m_maxMissing = new QSpinBox;
    m_maxMissing->setRange(0, 10);
    m_maxMissing->setValue(QSettings().value(QStringLiteral("fridge/maxMissing"), 4).toInt());
    fl->addWidget(m_maxMissing);
    gl->addLayout(fl);
    m_table = new QTableWidget(0, 3);
    m_table->setHorizontalHeaderLabels({ tr("Rezept"), tr("vorhanden"), tr("es fehlt") });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    gl->addWidget(m_table, 2);
    gl->addWidget(new QLabel(tr("Einkaufsliste für das gewählte Rezept:")));
    m_shopping = new QTextBrowser;
    gl->addWidget(m_shopping, 1);
    auto *bl = new QHBoxLayout;
    auto *show = new QPushButton(Icons::get(QStringLiteral("recipe")), tr("Rezept anzeigen"));
    auto *copy = new QPushButton(Icons::get(QStringLiteral("copy")), tr("Kopieren"));
    auto *print = new QPushButton(Icons::get(QStringLiteral("print")), tr("Drucken …"));
    auto *save = new QPushButton(Icons::get(QStringLiteral("save")), tr("Speichern …"));
    bl->addWidget(show);
    bl->addStretch(1);
    bl->addWidget(copy);
    bl->addWidget(print);
    bl->addWidget(save);
    gl->addLayout(bl);
    split->addWidget(sugBox);
    split->setStretchFactor(1, 2);
    lay->addWidget(split, 1);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::accept);

    connect(add, &QPushButton::clicked, this, &FridgeDialog::addItem);
    connect(m_input, &QLineEdit::returnPressed, this, &FridgeDialog::addItem);
    connect(remove, &QPushButton::clicked, this, [this] {
        qDeleteAll(m_stock->selectedItems());
        saveStock();
    });
    connect(clear, &QPushButton::clicked, this, [this] {
        m_stock->clear();
        saveStock();
    });
    connect(m_basics, &QLineEdit::editingFinished, this, [this] {
        QStringList b;
        for (const QString &x : m_basics->text().split(QLatin1Char(','), Qt::SkipEmptyParts)) b << x.trimmed();
        Fridge::setBasics(b);
        suggest();
    });
    connect(m_tag, qOverload<int>(&QComboBox::currentIndexChanged), this, &FridgeDialog::suggest);
    connect(m_maxMissing, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        QSettings().setValue(QStringLiteral("fridge/maxMissing"), v);
        suggest();
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &FridgeDialog::showShopping);
    auto showSel = [this] {
        const int r = m_table->currentRow();
        if (r >= 0) emit showRecipe(m_table->item(r, 0)->data(Qt::UserRole).toString());
    };
    connect(show, &QPushButton::clicked, this, showSel);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, showSel);
    connect(copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(m_shoppingText); });
    connect(print, &QPushButton::clicked, this, [this] {
        QPrinter printer(QPrinter::HighResolution);
        QPrintDialog d(&printer, this);
        if (d.exec() != QDialog::Accepted) return;
        m_shopping->document()->print(&printer);
    });
    connect(save, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getSaveFileName(this, tr("Einkaufsliste speichern"), tr("Einkaufsliste.txt"), tr("Text (*.txt)"));
        if (f.isEmpty()) return;
        QSaveFile out(f);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(m_shoppingText.toUtf8());
            out.commit();
        }
    });
    suggest();
    resize(1000, 640);
}

void FridgeDialog::addItem()
{
    for (const QString &part : m_input->text().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString t = part.trimmed();
        if (t.isEmpty() || !m_stock->findItems(t, Qt::MatchFixedString).isEmpty()) continue;
        m_stock->addItem(t);
    }
    m_input->clear();
    saveStock();
}

void FridgeDialog::saveStock()
{
    QStringList l;
    for (int i = 0; i < m_stock->count(); ++i) l << m_stock->item(i)->text();
    Fridge::setStock(l);
    suggest();
}

void FridgeDialog::suggest()
{
    QStringList stock;
    for (int i = 0; i < m_stock->count(); ++i) stock << m_stock->item(i)->text();
    const QString tag = m_tag->currentData().toString();
    const QList<Fridge::Suggestion> list = Fridge::suggest(stock, m_maxMissing->value(), tag.isEmpty() ? QStringList() : QStringList{ tag });
    m_table->setRowCount(0);
    for (const Fridge::Suggestion &s : list) {
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        auto *t = new QTableWidgetItem(s.title);
        t->setData(Qt::UserRole, s.id);
        if (s.missing.isEmpty()) t->setIcon(Icons::get(QStringLiteral("star-filled")));
        m_table->setItem(r, 0, t);
        auto *h = new QTableWidgetItem(QStringLiteral("%1 / %2").arg(s.have).arg(s.total));
        h->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(r, 1, h);
        m_table->setItem(r, 2, new QTableWidgetItem(s.missing.isEmpty() ? tr("– alles da –") : s.missing.join(QStringLiteral(", "))));
    }
    m_table->resizeColumnToContents(1);
    if (list.isEmpty())
        m_shopping->setHtml(tr("<p><i>Keine passenden Rezepte. Tragen Sie links ein, was im Kühlschrank ist, "
                               "oder erlauben Sie mehr fehlende Zutaten.</i></p>"));
    else m_table->selectRow(0);
}

void FridgeDialog::showShopping()
{
    const int r = m_table->currentRow();
    if (r < 0) return;
    Recipe rec;
    if (!RecipeDb::instance().load(m_table->item(r, 0)->data(Qt::UserRole).toString(), &rec)) return;
    QStringList stock;
    for (int i = 0; i < m_stock->count(); ++i) stock << m_stock->item(i)->text();
    const QList<ShoppingItem> list = Fridge::missingFor(rec, stock);
    const QString title = tr("Einkaufsliste: %1").arg(rec.title);
    if (list.isEmpty()) {
        m_shopping->setHtml(tr("<p><b>Alles vorhanden</b> – es muss nichts eingekauft werden.</p>"));
        m_shoppingText = title + QStringLiteral("\n\n") + tr("Alles vorhanden.");
        return;
    }
    m_shopping->setHtml(Shopping::toHtml(list, title));
    m_shoppingText = Shopping::toText(list, title);
}
