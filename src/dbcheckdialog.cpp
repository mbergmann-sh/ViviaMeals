// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "dbcheckdialog.h"

#include "icons.h"
#include "recipedb.h"
#include "sync.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {
const int kIdRole = Qt::UserRole + 1;
const int kMaxChildren = 500;   // Einzelaufstellung je Gruppe begrenzen
}

DbCheckDialog::DbCheckDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Datenbank prüfen"));
    setWindowIcon(Icons::get(QStringLiteral("db-check")));
    auto *lay = new QVBoxLayout(this);
    m_info = new QLabel(tr("Die Prüfung liest alle Rezeptdateien der Datenbank „%1“ und sucht nach unlesbaren Dateien, Fremdem im "
                           "Rezeptordner, leeren Rezepten sowie kaputten und übergroßen Fotos. Dabei wird noch nichts verändert – "
                           "was behoben werden soll, wählen Sie danach aus.")
                            .arg(RecipeDb::instance().name()));
    m_info->setWordWrap(true);
    lay->addWidget(m_info);
    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({ tr("Befund"), tr("Details") });
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setStretchLastSection(false);
    m_tree->setToolTip(tr("Doppelklick auf ein Rezept öffnet es im Hauptfenster"));
    lay->addWidget(m_tree, 1);
    m_bar = new QProgressBar;
    m_bar->setVisible(false);
    lay->addWidget(m_bar);
    m_note = new QLabel;
    m_note->setWordWrap(true);
    m_note->setTextFormat(Qt::RichText);
    m_note->setOpenExternalLinks(false);
    lay->addWidget(m_note);
    auto *bl = new QHBoxLayout;
    m_check = new QPushButton(Icons::get(QStringLiteral("db-check")), tr("&Prüfen"));
    m_fix = new QPushButton(Icons::get(QStringLiteral("check")), tr("Ausgewähltes &beheben"));
    m_fix->setEnabled(false);
    m_close = new QPushButton(tr("&Schließen"));
    bl->addWidget(m_check);
    bl->addStretch(1);
    bl->addWidget(m_fix);
    bl->addWidget(m_close);
    lay->addLayout(bl);
    connect(m_check, &QPushButton::clicked, this, &DbCheckDialog::runCheck);
    connect(m_fix, &QPushButton::clicked, this, &DbCheckDialog::fix);
    connect(m_close, &QPushButton::clicked, this, [this] {
        if (m_busy) m_cancel = true;
        else reject();
    });
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *, int) { updateFixButton(); });
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it, int) {
        const QString id = it->data(0, kIdRole).toString();
        if (!id.isEmpty()) emit openRecipe(id);
    });
    auto openFolder = [](const QString &link) { QDesktopServices::openUrl(QUrl::fromLocalFile(link)); };
    connect(m_note, &QLabel::linkActivated, this, openFolder);
    connect(m_info, &QLabel::linkActivated, this, openFolder);
    resize(680, 520);
}

void DbCheckDialog::setBusy(bool busy)
{
    m_busy = busy;
    m_cancel = false;
    m_check->setEnabled(!busy);
    if (busy) m_fix->setEnabled(false);
    m_tree->setEnabled(!busy);
    m_bar->setVisible(busy);
    m_close->setText(busy ? tr("Abbrechen") : tr("&Schließen"));
    if (busy) QApplication::setOverrideCursor(Qt::BusyCursor);
    else QApplication::restoreOverrideCursor();
    if (!busy) updateFixButton();
}

QTreeWidgetItem *DbCheckDialog::group(const QString &text, const QString &tip, bool checked, int count)
{
    if (count == 0) return nullptr;
    auto *g = new QTreeWidgetItem(m_tree);
    g->setText(0, text);
    g->setToolTip(0, tip);
    g->setFlags(g->flags() | Qt::ItemIsUserCheckable);
    g->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
    QFont f = g->font(0);
    f.setBold(true);
    g->setFont(0, f);
    return g;
}

void DbCheckDialog::runCheck()
{
    RecipeDb &db = RecipeDb::instance();
    if (!db.isOpen()) return;
    m_tree->clear();
    m_gBig = m_gBroken = m_gCorrupt = m_gStrays = m_gEmpty = nullptr;
    m_note->clear();
    setBusy(true);
    m_bar->setRange(0, 0);
    m_report = DbCheck::analyze([this](int done, int total) {
        m_bar->setRange(0, total);
        m_bar->setValue(done);
        QApplication::processEvents();
        return !m_cancel;
    });
    setBusy(false);
    if (m_report.cancelled) {
        m_info->setText(tr("Die Prüfung wurde abgebrochen."));
        return;
    }
    const DbCheck::Report &r = m_report;
    auto titleOf = [&](const QString &id) {
        const RecipeSummary *s = db.summary(id);
        return s && !s->title.isEmpty() ? s->title : id;
    };
    m_tree->blockSignals(true);
    // übergroße Fotos
    m_gBig = group(r.bigImages.size() == 1 ? tr("1 übergroßes Foto verkleinern") : tr("%1 übergroße Fotos verkleinern").arg(r.bigImages.size()),
                   tr("Fotos mit mehr als %1 Pixeln Kantenlänge oder über %2 werden auf %3 Pixel verkleinert (JPEG). Für Bildschirm, "
                      "Website und Druck reicht das gut.")
                       .arg(DbCheck::kMaxSide)
                       .arg(DbCheck::sizeText(DbCheck::kMaxBytes))
                       .arg(DbCheck::kTargetSide),
                   true, int(r.bigImages.size()));
    if (m_gBig) {
        m_gBig->setText(1, tr("zusammen %1").arg(DbCheck::sizeText(r.bigImageBytes)));
        int n = 0;
        for (const DbCheck::BigImage &b : r.bigImages) {
            if (++n > kMaxChildren) break;
            auto *c = new QTreeWidgetItem(m_gBig, { b.title.isEmpty() ? b.id : b.title,
                                                    QStringLiteral("%1 × %2, %3").arg(b.size.width()).arg(b.size.height()).arg(DbCheck::sizeText(b.bytes)) });
            c->setData(0, kIdRole, b.id);
        }
    }
    // kaputte Fotos
    m_gBroken = group(r.brokenImages.size() == 1 ? tr("1 unlesbares Foto entfernen") : tr("%1 unlesbare Fotos entfernen").arg(r.brokenImages.size()),
                      tr("Die Fotodaten dieser Rezepte lassen sich nicht anzeigen. Das Rezept bleibt erhalten, nur das Foto wird entfernt."),
                      true, int(r.brokenImages.size()));
    if (m_gBroken) {
        for (const QString &id : r.brokenImages.mid(0, kMaxChildren)) {
            auto *c = new QTreeWidgetItem(m_gBroken, { titleOf(id) });
            c->setData(0, kIdRole, id);
        }
    }
    // unlesbare Dateien
    m_gCorrupt = group(r.corrupt.size() == 1 ? tr("1 unlesbare Rezeptdatei aussortieren") : tr("%1 unlesbare Rezeptdateien aussortieren").arg(r.corrupt.size()),
                       tr("Diese Dateien sind beschädigt oder leer und erscheinen nicht in der Rezeptliste. Sie werden in den Ordner "
                          "„aussortiert“ der Datenbank verschoben, nicht gelöscht."),
                       true, int(r.corrupt.size()));
    if (m_gCorrupt)
        for (int i = 0; i < r.corrupt.size() && i < kMaxChildren; ++i) new QTreeWidgetItem(m_gCorrupt, { r.corrupt.at(i), r.corruptReasons.value(i) });
    // fremde Dateien
    m_gStrays = group(r.strays.size() == 1 ? tr("1 fremde Datei im Rezeptordner aussortieren") : tr("%1 fremde Dateien im Rezeptordner aussortieren").arg(r.strays.size()),
                      tr("Dateien ohne Endung .vmr, z. B. Reste abgebrochener Speichervorgänge. Sie werden in den Ordner „aussortiert“ verschoben."),
                      true, int(r.strays.size()));
    if (m_gStrays)
        for (const QString &f : r.strays.mid(0, kMaxChildren)) new QTreeWidgetItem(m_gStrays, { f });
    // leere Rezepte
    m_gEmpty = group(r.empty.size() == 1 ? tr("1 leeres Rezept aussortieren") : tr("%1 leere Rezepte aussortieren").arg(r.empty.size()),
                     tr("Rezepte ganz ohne Titel, Zutaten und Zubereitung. Standardmäßig nicht ausgewählt."), false, int(r.empty.size()));
    if (m_gEmpty) {
        for (const QString &id : r.empty.mid(0, kMaxChildren)) {
            auto *c = new QTreeWidgetItem(m_gEmpty, { tr("(leer, Kennung %1)").arg(id) });
            c->setData(0, kIdRole, id);
        }
    }
    m_tree->blockSignals(false);

    const QString summary = tr("%1 Rezeptdateien geprüft, zusammen %2.").arg(r.files).arg(DbCheck::sizeText(r.totalBytes));
    if (r.clean()) {
        m_info->setText(QStringLiteral("<b>%1</b> %2").arg(tr("Alles in Ordnung."), summary));
        auto *ok = new QTreeWidgetItem(m_tree, { tr("Keine Probleme gefunden – die Datenbank braucht keine Pflege.") });
        ok->setIcon(0, Icons::get(QStringLiteral("check")));
    } else {
        m_info->setText(summary + QLatin1Char(' ') + tr("Wählen Sie aus, was behoben werden soll; Doppelklick auf ein Rezept zeigt es im Hauptfenster."));
    }
    // Hinweis zum Abgleich
    if (!r.clean() && Sync::Connection::load(db.folder()).valid())
        m_note->setText(tr("<small><b>Abgleich:</b> Verkleinerte Fotos werden beim nächsten Abgleich hochgeladen; aussortierte "
                           "Rezepte gelten dann als gelöscht und verschwinden auch auf den anderen Geräten.</small>"));
    updateFixButton();
}

void DbCheckDialog::updateFixButton()
{
    bool any = false;
    for (QTreeWidgetItem *g : { m_gBig, m_gBroken, m_gCorrupt, m_gStrays, m_gEmpty })
        if (g && g->checkState(0) == Qt::Checked) any = true;
    m_fix->setEnabled(any && !m_busy);
}

void DbCheckDialog::fix()
{
    auto on = [](QTreeWidgetItem *g) { return g && g->checkState(0) == Qt::Checked; };
    DbCheck::Actions act;
    act.shrinkImages = on(m_gBig);
    act.removeBrokenImages = on(m_gBroken);
    act.moveCorrupt = on(m_gCorrupt);
    act.moveStrays = on(m_gStrays);
    act.moveEmpty = on(m_gEmpty);
    setBusy(true);
    m_bar->setRange(0, 0);
    const DbCheck::Result res = DbCheck::apply(m_report, act, [this](int done, int total) {
        m_bar->setRange(0, total);
        m_bar->setValue(done);
        QApplication::processEvents();
        return !m_cancel;
    });
    setBusy(false);
    QStringList parts;
    if (res.shrunk) parts << (res.shrunk == 1 ? tr("1 Foto verkleinert") : tr("%1 Fotos verkleinert").arg(res.shrunk))
                                 + tr(" (%1 gespart)").arg(DbCheck::sizeText(res.savedBytes));
    if (res.imagesRemoved) parts << (res.imagesRemoved == 1 ? tr("1 unlesbares Foto entfernt") : tr("%1 unlesbare Fotos entfernt").arg(res.imagesRemoved));
    if (res.moved) parts << (res.moved == 1 ? tr("1 Datei aussortiert") : tr("%1 Dateien aussortiert").arg(res.moved));
    QString text = parts.isEmpty() ? tr("Es wurde nichts geändert.") : parts.join(QStringLiteral(", ")) + QLatin1Char('.');
    text += QLatin1Char(' ') + tr("Das Inhaltsverzeichnis wurde neu aufgebaut.");
    if (!res.sortedFolder.isEmpty())
        text += QStringLiteral("<br>") + tr("Aussortiertes liegt in <a href=\"%1\">%2</a>.")
                                             .arg(res.sortedFolder.toHtmlEscaped(), QDir::toNativeSeparators(res.sortedFolder).toHtmlEscaped());
    if (!res.errors.isEmpty()) text += QStringLiteral("<br><b>%1</b> %2").arg(tr("Probleme:"), res.errors.mid(0, 5).join(QStringLiteral("; ")).toHtmlEscaped());
    m_tree->clear();
    m_gBig = m_gBroken = m_gCorrupt = m_gStrays = m_gEmpty = nullptr;
    m_info->setText(text);
    m_note->setText(tr("<small>„Prüfen“ zeigt den neuen Stand.</small>"));
    updateFixButton();
}
