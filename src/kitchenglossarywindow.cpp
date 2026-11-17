// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "kitchenglossarywindow.h"

#include "icons.h"
#include "kitchenglossary.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QTextBrowser>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace KitchenGlossary {

QString entryHtml(int index, bool withHeading)
{
    const QList<Entry> &all = entries();
    if (index < 0 || index >= all.size()) return {};
    const Entry &e = all.at(index);
    QString h;
    if (withHeading)
        h += QStringLiteral("<h2 style=\"margin-bottom:2px\">%1</h2>").arg(e.term.toHtmlEscaped());
    h += QStringLiteral("<p style=\"color:gray; margin-top:0\">%1</p>").arg(e.category.toHtmlEscaped());
    h += QStringLiteral("<p style=\"font-size:large\">%1</p>").arg(e.text.toHtmlEscaped());
    if (!e.synonyms.isEmpty())
        h += QStringLiteral("<p><b>%1</b> %2</p>").arg(QObject::tr("Auch:"), e.synonyms.join(QStringLiteral(", ")).toHtmlEscaped());
    if (!e.seeAlso.isEmpty()) {
        QStringList links;
        for (const QString &s : e.seeAlso)
            links << QStringLiteral("<a href=\"glossar:%1\">%2</a>").arg(QString::fromLatin1(QUrl::toPercentEncoding(s)), s.toHtmlEscaped());
        h += QStringLiteral("<p><b>%1</b> %2</p>").arg(QObject::tr("Siehe auch:"), links.join(QStringLiteral(" · ")));
    }
    return h;
}

} // namespace KitchenGlossary

namespace {

QString termFromUrl(const QUrl &url)
{
    if (url.scheme() != QLatin1String("glossar")) return {};
    return QUrl::fromPercentEncoding(url.toString(QUrl::FullyEncoded).mid(8).toLatin1());
}

} // namespace

// ---------------------------------------------------------------- Glossar-Fenster

GlossaryWindow::GlossaryWindow(QWidget *parent) : QWidget(parent, Qt::Window)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Küchen-Glossar"));
    setWindowIcon(Icons::get(QStringLiteral("glossary")));
    auto *lay = new QVBoxLayout(this);
    auto *top = new QHBoxLayout;
    m_back = new QToolButton;
    m_back->setIcon(Icons::get(QStringLiteral("back")));
    m_back->setToolTip(tr("Zurück (Alt+Links)"));
    m_back->setShortcut(QKeySequence::Back);
    top->addWidget(m_back);
    m_forward = new QToolButton;
    m_forward->setIcon(Icons::get(QStringLiteral("forward")));
    m_forward->setToolTip(tr("Vor (Alt+Rechts)"));
    m_forward->setShortcut(QKeySequence::Forward);
    top->addWidget(m_forward);
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Begriff oder Stichwort suchen, z. B. „Brunoise“, „binden“, „Fisch“ …"));
    m_search->setClearButtonEnabled(true);
    top->addWidget(m_search, 1);
    m_category = new QComboBox;
    m_category->addItem(tr("Alle Kategorien"), QString());
    for (const QString &c : KitchenGlossary::categories()) m_category->addItem(c, c);
    top->addWidget(m_category);
    lay->addLayout(top);
    auto *split = new QSplitter;
    auto *left = new QWidget;
    auto *ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    m_list = new QListWidget;
    m_list->setUniformItemSizes(true);
    ll->addWidget(m_list, 1);
    m_count = new QLabel;
    ll->addWidget(m_count);
    split->addWidget(left);
    m_text = new QTextBrowser;
    m_text->setOpenLinks(false);
    split->addWidget(m_text);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 5);
    lay->addWidget(split, 1);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::rejected, this, &QWidget::close);
    auto *find = new QShortcut(QKeySequence::Find, this);
    connect(find, &QShortcut::activated, this, [this] {
        m_search->setFocus();
        m_search->selectAll();
    });

    connect(m_search, &QLineEdit::textChanged, this, &GlossaryWindow::refill);
    connect(m_category, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GlossaryWindow::refill);
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) showIndex(m_list->item(row)->data(Qt::UserRole).toInt(), true);
    });
    connect(m_text, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
        const QString t = termFromUrl(url);
        if (!t.isEmpty()) showTerm(t);
    });
    connect(m_back, &QToolButton::clicked, this, [this] { go(-1); });
    connect(m_forward, &QToolButton::clicked, this, [this] { go(1); });
    // Eingabetaste im Suchfeld: ersten Treffer zeigen
    connect(m_search, &QLineEdit::returnPressed, this, [this] {
        if (m_list->count() > 0) m_list->setCurrentRow(0);
        m_list->setFocus();
    });
    resize(820, 560);
    refill();
}

void GlossaryWindow::refill()
{
    const QList<int> hits = KitchenGlossary::search(m_search->text(), m_category->currentData().toString());
    const QList<KitchenGlossary::Entry> &all = KitchenGlossary::entries();
    m_list->blockSignals(true);
    m_list->clear();
    int keepRow = -1;
    for (int i : hits) {
        auto *it = new QListWidgetItem(all.at(i).term, m_list);
        it->setData(Qt::UserRole, i);
        it->setToolTip(all.at(i).category);
        if (i == m_current) keepRow = m_list->count() - 1;
    }
    m_list->blockSignals(false);
    m_count->setText(hits.size() == 1 ? tr("1 Begriff") : tr("%1 Begriffe").arg(hits.size()));
    if (hits.isEmpty()) {
        m_text->setHtml(tr("<p>Kein Begriff gefunden. Versuchen Sie einen Wortteil oder „Alle Kategorien“.</p>"));
        return;
    }
    // beim Suchen den besten Treffer zeigen, sonst die Auswahl halten
    const bool searching = !m_search->text().trimmed().isEmpty();
    m_list->setCurrentRow(searching || keepRow < 0 ? 0 : keepRow);
}

void GlossaryWindow::showIndex(int index, bool record)
{
    if (index < 0) return;
    m_current = index;
    m_text->setHtml(KitchenGlossary::entryHtml(index, true));
    if (record && (m_pos < 0 || m_history.value(m_pos, -1) != index)) {
        while (m_history.size() > m_pos + 1) m_history.removeLast();
        m_history << index;
        m_pos = int(m_history.size()) - 1;
    }
    m_back->setEnabled(m_pos > 0);
    m_forward->setEnabled(m_pos >= 0 && m_pos < m_history.size() - 1);
}

void GlossaryWindow::go(int step)
{
    const int p = m_pos + step;
    if (p < 0 || p >= m_history.size()) return;
    m_pos = p;
    const int index = m_history.at(p);
    // in der Liste markieren, ohne einen neuen Verlaufseintrag
    for (int r = 0; r < m_list->count(); ++r) {
        if (m_list->item(r)->data(Qt::UserRole).toInt() == index) {
            m_list->blockSignals(true);
            m_list->setCurrentRow(r);
            m_list->blockSignals(false);
            break;
        }
    }
    showIndex(index, false);
}

void GlossaryWindow::showTerm(const QString &term)
{
    const int index = KitchenGlossary::indexOf(term);
    if (index < 0) return;
    // steht der Begriff nicht in der gefilterten Liste, Filter zurücksetzen
    bool found = false;
    for (int r = 0; r < m_list->count() && !found; ++r) found = m_list->item(r)->data(Qt::UserRole).toInt() == index;
    if (!found) {
        m_search->blockSignals(true);
        m_search->clear();
        m_search->blockSignals(false);
        m_category->blockSignals(true);
        m_category->setCurrentIndex(0);
        m_category->blockSignals(false);
        m_current = index;
        refill();
    }
    for (int r = 0; r < m_list->count(); ++r) {
        if (m_list->item(r)->data(Qt::UserRole).toInt() == index) {
            m_list->setCurrentRow(r);
            m_list->scrollToItem(m_list->item(r), QAbstractItemView::PositionAtCenter);
            break;
        }
    }
}

// ---------------------------------------------------------------- Tipp des Tages

TipOfTheDayDialog::TipOfTheDayDialog(QWidget *parent) : QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Tipp des Tages"));
    setWindowIcon(Icons::get(QStringLiteral("tip")));
    setModal(false);
    auto *lay = new QVBoxLayout(this);
    auto *head = new QHBoxLayout;
    auto *pic = new QLabel;
    pic->setPixmap(Icons::get(QStringLiteral("tip")).pixmap(48, 48));
    pic->setAlignment(Qt::AlignTop);
    head->addWidget(pic);
    auto *hv = new QVBoxLayout;
    auto *kicker = new QLabel(tr("Wussten Sie schon … – aus dem Küchen-Glossar"));
    QFont kf = kicker->font();
    kf.setItalic(true);
    kicker->setFont(kf);
    hv->addWidget(kicker);
    m_heading = new QLabel;
    QFont hf = m_heading->font();
    hf.setPointSizeF(hf.pointSizeF() * 1.6);
    hf.setBold(true);
    m_heading->setFont(hf);
    m_heading->setWordWrap(true);
    hv->addWidget(m_heading);
    head->addLayout(hv, 1);
    lay->addLayout(head);
    m_text = new QTextBrowser;
    m_text->setOpenLinks(false);
    m_text->setMinimumHeight(170);
    lay->addWidget(m_text, 1);

    auto *nav = new QHBoxLayout;
    m_prev = new QPushButton(Icons::get(QStringLiteral("back")), tr("&Zurück"));
    m_next = new QPushButton(Icons::get(QStringLiteral("forward")), tr("&Weiter"));
    m_next->setDefault(true);
    m_counter = new QLabel;
    m_counter->setAlignment(Qt::AlignCenter);
    nav->addWidget(m_prev);
    nav->addWidget(m_counter, 1);
    nav->addWidget(m_next);
    lay->addLayout(nav);

    auto *bottom = new QHBoxLayout;
    m_onStart = new QCheckBox(tr("&Tipps beim Programmstart anzeigen"));
    m_onStart->setChecked(QSettings().value(QStringLiteral("ui/tipOfTheDay"), true).toBool());
    m_onStart->setToolTip(tr("Auch unter Extras → Einstellungen → Programmstart"));
    bottom->addWidget(m_onStart);
    bottom->addStretch(1);
    auto *open = new QPushButton(Icons::get(QStringLiteral("glossary")), tr("Im &Glossar öffnen"));
    bottom->addWidget(open);
    auto *close = new QPushButton(tr("&Schließen"));
    bottom->addWidget(close);
    lay->addLayout(bottom);

    // zufällige Reihenfolge: jeder Start beginnt woanders, Zurück führt zu den schon gezeigten Tipps
    const int n = int(KitchenGlossary::entries().size());
    for (int i = 0; i < n; ++i) m_order << i;
    std::shuffle(m_order.begin(), m_order.end(), *QRandomGenerator::global());

    connect(m_prev, &QPushButton::clicked, this, [this] { showAt(m_pos - 1); });
    connect(m_next, &QPushButton::clicked, this, [this] { showAt(m_pos + 1); });
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    connect(m_onStart, &QCheckBox::toggled, this, [](bool on) { QSettings().setValue(QStringLiteral("ui/tipOfTheDay"), on); });
    connect(open, &QPushButton::clicked, this, [this] {
        if (m_order.isEmpty()) return;
        emit openInGlossary(KitchenGlossary::entries().at(m_order.at(m_pos)).term);
    });
    connect(m_text, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
        const QString t = termFromUrl(url);
        if (!t.isEmpty()) emit openInGlossary(t);
    });
    resize(560, 400);
    showAt(0);
}

void TipOfTheDayDialog::showAt(int pos)
{
    const int n = int(m_order.size());
    if (n == 0) {
        m_heading->setText(tr("Kein Glossar vorhanden"));
        m_prev->setEnabled(false);
        m_next->setEnabled(false);
        return;
    }
    m_pos = (pos % n + n) % n;   // am Ende weiter von vorn
    const int index = m_order.at(m_pos);
    m_heading->setText(KitchenGlossary::entries().at(index).term);
    m_text->setHtml(KitchenGlossary::entryHtml(index, false));
    m_counter->setText(tr("Tipp %1 von %2").arg(m_pos + 1).arg(n));
    m_prev->setEnabled(m_pos > 0);
}
