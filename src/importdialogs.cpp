// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "importdialogs.h"
#include "season.h"
#include "busyprogress.h"
#include "foodwizard.h"
#include "mealtypes.h"
#include "nutrition.h"
#include "deepl.h"
#include "icons.h"
#include "recipedb.h"
#include "recipehtml.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTableWidget>
#include <QVBoxLayout>

// ---------------------------------------------------------------- Vorschau

ImportPreviewDialog::ImportPreviewDialog(const QList<Recipe> &recipes, const QStringList &origins, const QStringList &warnings, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Rezepte importieren"));
    auto *lay = new QVBoxLayout(this);
    int dups = 0;
    const RecipeDb &db = RecipeDb::instance();
    for (const Recipe &r : recipes)
        if (!db.duplicatesOf(r).isEmpty()) ++dups;
    auto *info = new QLabel(tr("<b>%1 Rezepte gefunden.</b> %2 davon sind bereits in der Datenbank „%3“ vorhanden – "
                               "für diese wird beim Import nachgefragt.")
                                .arg(recipes.size())
                                .arg(dups)
                                .arg(db.name().toHtmlEscaped()));
    info->setWordWrap(true);
    lay->addWidget(info);
    m_table = new QTableWidget(int(recipes.size()), 6);
    m_table->setHorizontalHeaderLabels({ tr("Rezept"), tr("Status"), tr("Sprache"), tr("Portionen"), tr("kcal/Portion"), tr("Datei") });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (int i = 0; i < recipes.size(); ++i) {
        const Recipe &r = recipes[i];
        auto *t = new QTableWidgetItem(r.title);
        t->setFlags(t->flags() | Qt::ItemIsUserCheckable);
        t->setCheckState(Qt::Checked);
        m_table->setItem(i, 0, t);
        const bool dup = !db.duplicatesOf(r).isEmpty();
        auto *st = new QTableWidgetItem(dup ? tr("Doublette") : tr("neu"));
        if (dup) st->setForeground(QColor(0xd0, 0x70, 0x10));
        m_table->setItem(i, 1, st);
        m_table->setItem(i, 2, new QTableWidgetItem(ImportFlow::languageLabel(r)));
        m_table->setItem(i, 3, new QTableWidgetItem(r.servingsText()));
        auto *k = new QTableWidgetItem(r.nutrition.matched > 0 ? Fmt::kcal(r.nutrition.perServing.kcal) : QStringLiteral("–"));
        k->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        k->setToolTip(tr("%1 von %2 Zutaten mit Nährwerten").arg(r.nutrition.matched).arg(r.nutrition.countable));
        m_table->setItem(i, 4, k);
        m_table->setItem(i, 5, new QTableWidgetItem(QFileInfo(origins.value(i)).fileName()));
    }
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    lay->addWidget(m_table, 1);
    if (!warnings.isEmpty()) {
        auto *w = new QLabel(tr("Hinweise: %1").arg(warnings.join(QStringLiteral("; ")).toHtmlEscaped()));
        w->setWordWrap(true);
        lay->addWidget(w);
    }
    auto *bl = new QHBoxLayout;
    auto *all = new QPushButton(tr("Alle"));
    auto *none = new QPushButton(tr("Keine"));
    auto *newOnly = new QPushButton(tr("Nur neue"));
    bl->addWidget(all);
    bl->addWidget(none);
    bl->addWidget(newOnly);
    bl->addStretch(1);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Importieren"));
    bb->button(QDialogButtonBox::Ok)->setIcon(Icons::get(QStringLiteral("import")));
    bl->addWidget(bb);
    lay->addLayout(bl);
    auto setAll = [this](int mode) {
        for (int i = 0; i < m_table->rowCount(); ++i) {
            const bool dup = m_table->item(i, 1)->text() != tr("neu");
            m_table->item(i, 0)->setCheckState(mode == 1 || (mode == 2 && !dup) ? Qt::Checked : Qt::Unchecked);
        }
    };
    connect(all, &QPushButton::clicked, this, [setAll] { setAll(1); });
    connect(none, &QPushButton::clicked, this, [setAll] { setAll(0); });
    connect(newOnly, &QPushButton::clicked, this, [setAll] { setAll(2); });
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    resize(860, 520);
}

QList<int> ImportPreviewDialog::selected() const
{
    QList<int> l;
    for (int i = 0; i < m_table->rowCount(); ++i)
        if (m_table->item(i, 0)->checkState() == Qt::Checked) l << i;
    return l;
}

// ---------------------------------------------------------------- Doublette

DuplicateDialog::DuplicateDialog(const Recipe &incoming, const Recipe &existing, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Rezept bereits vorhanden"));
    auto *lay = new QVBoxLayout(this);
    auto *head = new QLabel(tr("Das Rezept <b>„%1“</b> ist bereits in der Datenbank vorhanden.").arg(incoming.title.toHtmlEscaped()));
    head->setWordWrap(true);
    lay->addWidget(head);
    auto *grid = new QGridLayout;
    auto describe = [](const Recipe &r) {
        QStringList l;
        l << QCoreApplication::translate("DuplicateDialog", "%1 Zutaten").arg(r.ingredients.size());
        l << r.servingsText();
        if (r.nutrition.matched > 0) l << QCoreApplication::translate("DuplicateDialog", "%1 kcal/Portion").arg(Fmt::kcal(r.nutrition.perServing.kcal));
        if (!r.image.isEmpty()) l << QCoreApplication::translate("DuplicateDialog", "mit Foto");
        if (r.modified.isValid()) l << QCoreApplication::translate("DuplicateDialog", "geändert %1").arg(r.modified.toString(QStringLiteral("dd.MM.yyyy")));
        return l.join(QStringLiteral(" · "));
    };
    grid->addWidget(new QLabel(tr("<b>Vorhanden:</b>")), 0, 0);
    grid->addWidget(new QLabel(describe(existing)), 0, 1);
    grid->addWidget(new QLabel(tr("<b>Import:</b>")), 1, 0);
    grid->addWidget(new QLabel(describe(incoming) + QStringLiteral(" · ") + incoming.sourceFormat), 1, 1);
    lay->addLayout(grid);
    m_all = new QCheckBox(tr("Für alle weiteren Doubletten dieses Imports übernehmen"));
    lay->addWidget(m_all);
    auto *bb = new QDialogButtonBox;
    auto *skip = bb->addButton(tr("Überspringen"), QDialogButtonBox::RejectRole);
    auto *replace = bb->addButton(tr("Ersetzen"), QDialogButtonBox::DestructiveRole);
    auto *both = bb->addButton(tr("Beide behalten"), QDialogButtonBox::AcceptRole);
    skip->setDefault(true);
    lay->addWidget(bb);
    connect(skip, &QPushButton::clicked, this, [this] { m_choice = Skip; accept(); });
    connect(replace, &QPushButton::clicked, this, [this] { m_choice = Replace; accept(); });
    connect(both, &QPushButton::clicked, this, [this] { m_choice = KeepBoth; accept(); });
}

bool DuplicateDialog::forAll() const { return m_all->isChecked(); }

// ---------------------------------------------------------------- Ablauf

namespace ImportFlow {

QString run(QWidget *parent, Importers::Format format, const QStringList &givenFiles)
{
    RecipeDb &db = RecipeDb::instance();
    if (!db.isOpen()) {
        QMessageBox::information(parent, QObject::tr("Import"), QObject::tr("Bitte zuerst eine Rezeptdatenbank öffnen oder anlegen."));
        return {};
    }
    QSettings s;
    QStringList files = givenFiles;
    if (files.isEmpty()) {
        QString filter;
        switch (format) {
        case Importers::Format::RezKonv:
            filter = QObject::tr("RezKonv-Rezepte (*.rk *.rkv *.txt);;Alle Dateien (*)");
            break;
        case Importers::Format::MealMaster:
            filter = QObject::tr("MealMaster-Rezepte (*.mmf *.mm *.mxp *.txt);;Alle Dateien (*)");
            break;
        default:
            filter = QObject::tr("Alte ViviaMeals-Listen (*.vtm);;Alle Dateien (*)");
        }
        const QString dir = s.value(QStringLiteral("import/dir"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
        files = QFileDialog::getOpenFileNames(parent, QObject::tr("%1 importieren").arg(Importers::formatName(format)), dir, filter);
        if (files.isEmpty()) return {};
        s.setValue(QStringLiteral("import/dir"), QFileInfo(files.first()).absolutePath());
    }
    Importers::Options opt;
    opt.translate = s.value(QStringLiteral("import/translate"), true).toBool();
    opt.autoTags = s.value(QStringLiteral("import/autoTags"), true).toBool();
    opt.fixUmlauts = s.value(QStringLiteral("import/fixUmlauts"), true).toBool();
    opt.mealTags = s.value(QStringLiteral("import/mealTags"), true).toBool();
    opt.nutrientTags = s.value(QStringLiteral("import/nutrientTags"), true).toBool();
    opt.seasonTags = s.value(QStringLiteral("import/seasonTags"), true).toBool();

    QList<Recipe> recipes;
    QStringList origins, warnings;
    int umlauts = 0;
    // 1. Durchgang ohne DeepL: Sprache feststellen, nichts verlässt den Rechner
    auto readAll = [&](const Importers::Options &o, QProgressDialog &prog) {
        recipes.clear();
        origins.clear();
        warnings.clear();
        umlauts = 0;
        for (int i = 0; i < files.size(); ++i) {
            prog.setValue(i);
            if (prog.wasCanceled()) return false;
            const Importers::Result res = Importers::importFile(files[i], format, o);
            for (const Recipe &r : res.recipes) {
                recipes << r;
                origins << files[i];
            }
            umlauts += res.umlautsFixed;
            for (const QString &w : res.warnings) warnings << QFileInfo(files[i]).fileName() + QStringLiteral(": ") + w;
        }
        prog.setValue(int(files.size()));
        return true;
    };
    {
        QProgressDialog prog(QObject::tr("Rezepte werden gelesen …"), QObject::tr("Abbrechen"), 0, int(files.size()), parent);
        prog.setWindowModality(Qt::WindowModal);
        prog.setMinimumDuration(400);
        if (!readAll(opt, prog)) return {};
    }
    // 2. fremdsprachige Rezepte mit DeepL übersetzen (nach Rückfrage)
    const DeepL::Settings ds = DeepL::Settings::load();
    if (ds.usable() && format != Importers::Format::Vtm) {
        int foreign = 0, chars = 0;
        for (const Recipe &r : recipes)
            if (r.language != QLatin1String("de")) {
                ++foreign;
                chars += DeepL::characterCount(r);
            }
        if (foreign > 0) {
            const auto answer = QMessageBox::question(
                parent, QObject::tr("Mit DeepL übersetzen?"),
                QObject::tr("%1 der %2 Rezepte sind nicht deutschsprachig.\n\nSollen sie mit DeepL ins Deutsche übersetzt werden? "
                            "Dazu werden Titel, Zutaten und Zubereitung (etwa %3 Zeichen) an DeepL gesendet.\n\n"
                            "„Nein“: Zutaten über das eingebaute Glossar übersetzen, Anleitung im Original lassen.")
                    .arg(foreign)
                    .arg(recipes.size())
                    .arg(QLocale(QLocale::German).toString(chars)),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (answer == QMessageBox::Yes) {
                QProgressDialog prog(QObject::tr("Rezepte werden übersetzt …"), QObject::tr("Abbrechen"), 0, int(files.size()), parent);
                prog.setWindowModality(Qt::WindowModal);
                prog.setMinimumDuration(0);
                Importers::Options o2 = opt;
                o2.translator = deeplTranslator(&prog);
                if (!readAll(o2, prog)) return {};
            }
        }
    }
    if (umlauts > 0)
        warnings.prepend(umlautNote(umlauts));
    if (recipes.isEmpty()) {
        QMessageBox::warning(parent, QObject::tr("Import"),
                             QObject::tr("In den gewählten Dateien wurden keine %1-Rezepte gefunden.").arg(Importers::formatName(format))
                                 + (warnings.isEmpty() ? QString() : QStringLiteral("\n\n") + warnings.join(QLatin1Char('\n'))));
        return {};
    }
    ImportPreviewDialog prev(recipes, origins, warnings, parent);
    if (prev.exec() != QDialog::Accepted) return {};
    const QList<int> sel = prev.selected();

    QList<Recipe> chosen;
    for (int idx : sel) chosen << recipes[idx];
    return store(parent, chosen, warnings);
}

QString store(QWidget *parent, const QList<Recipe> &chosen, QStringList warnings, const QString &title)
{
    RecipeDb &db = RecipeDb::instance();
    int added = 0, replaced = 0, skipped = 0;
    bool haveDefault = false;
    DuplicateDialog::Choice def = DuplicateDialog::Skip;
    QString firstId;
    QStringList savedIds;
    db.beginBatch();
    for (Recipe r : chosen) {
        Importers::normalizeTexts(r);   // auch Rezeptpakete und .vmr: „200GradC“ → „200° C“
        const QStringList dups = db.duplicatesOf(r);
        if (!dups.isEmpty()) {
            DuplicateDialog::Choice c = def;
            if (!haveDefault) {
                Recipe existing;
                db.load(dups.first(), &existing);
                DuplicateDialog d(r, existing, parent);
                d.exec();
                c = d.choice();
                if (d.forAll()) {
                    haveDefault = true;
                    def = c;
                }
            }
            if (c == DuplicateDialog::Skip) {
                ++skipped;
                continue;
            }
            if (c == DuplicateDialog::Replace) {
                Recipe existing;
                if (db.load(dups.first(), &existing)) {
                    r.id = existing.id;
                    r.created = existing.created;
                    if (r.image.isEmpty()) r.image = existing.image;   // Foto behalten
                    r.favorite = existing.favorite;
                }
                ++replaced;
            } else {
                r.id.clear();   // zweites Exemplar mit eigener ID
                ++added;
            }
        } else {
            ++added;
        }
        QString err;
        if (!db.save(r, &err)) {
            warnings << r.title + QStringLiteral(": ") + err;
            continue;
        }
        savedIds << r.id;
        if (firstId.isEmpty()) firstId = r.id;
    }
    db.endBatch();
    QString msg = QObject::tr("%1 Rezepte neu aufgenommen, %2 ersetzt, %3 übersprungen.").arg(added).arg(replaced).arg(skipped);
    QMessageBox::information(parent, title.isEmpty() ? QObject::tr("Import abgeschlossen") : title, msg);
    if (!savedIds.isEmpty() && MissingFoodsDialog::offerEnabled()) offerMissingFoods(parent, savedIds);
    return firstId;
}

int offerMissingFoods(QWidget *parent, const QStringList &ids)
{
    RecipeDb &db = RecipeDb::instance();
    QList<Recipe> list;
    for (const QString &id : ids) {
        Recipe r;
        if (db.loadLight(id, &r)) list << r;   // ohne Foto – nur die Zutaten werden gebraucht
    }
    const auto missing = Nutrition::missingFoods(list, MissingFoodsDialog::ignoredNames());
    if (missing.isEmpty()) return 0;
    int total = 0;
    for (const auto &m : missing) total += m.count;
    const auto b = QMessageBox::question(parent, QObject::tr("Zutaten ohne Nährwerte"),
                                         QObject::tr("%1 verschiedene Zutaten (%2-mal verwendet) haben in der Nährwerttabelle keine Entsprechung, "
                                                     "z. B. %3.\n\nJetzt eigene Lebensmittel anlegen oder vorhandenen zuordnen?")
                                             .arg(missing.size())
                                             .arg(total)
                                             .arg(missing.first().name),
                                         QMessageBox::Yes | QMessageBox::No);
    if (b != QMessageBox::Yes) return 0;
    MissingFoodsDialog d(missing, QObject::tr("Diese Zutaten der importierten Rezepte haben noch keine Nährwerte (häufigste zuerst):"), parent);
    d.exec();
    if (!d.changed()) return 0;
    // Nährwerte der betroffenen Rezepte neu berechnen
    const bool nutrientTags = QSettings().value(QStringLiteral("import/nutrientTags"), true).toBool();
    const bool seasonTags = QSettings().value(QStringLiteral("import/seasonTags"), true).toBool();
    int updated = 0;
    BusyProgress prog(parent, QObject::tr("Kalorien und Nährwerte neu berechnen"), int(list.size()), false);
    int i = 0;
    db.beginBatch();
    for (Recipe r : list) {
        ++i;
        prog.step(i - 1, QObject::tr("Rezept %1 von %2: %3").arg(i).arg(list.size()).arg(r.title));
        const double before = r.nutrition.total.kcal;
        const int matchedBefore = r.nutrition.matched;
        Nutrition::analyze(r);
        if (nutrientTags) MealTypes::applyNutrientTags(r);
        const int seasonChanges = (seasonTags && !Nutrition::isMenu(r)) ? Season::applyTags(r) : 0;
        if (seasonChanges || r.nutrition.matched != matchedBefore || !qFuzzyCompare(1 + before, 1 + r.nutrition.total.kcal)) {
            if (db.save(r)) ++updated;
        }
    }
    db.endBatch();
    return updated;
}

std::function<bool(Recipe &, QString *)> deeplTranslator(QProgressDialog *progress)
{
    const DeepL::Settings ds = DeepL::Settings::load();
    if (!ds.usable()) return {};
    auto stop = std::make_shared<QString>();   // nach einem schweren Fehler nicht weiter versuchen
    const QString key = ds.key;
    return [key, stop, progress](Recipe &r, QString *err) {
        if (!stop->isEmpty()) {
            if (err) *err = *stop;
            return false;
        }
        if (progress) {
            if (progress->wasCanceled()) {
                if (err) *err = QObject::tr("abgebrochen");
                return false;
            }
            progress->setLabelText(QObject::tr("Übersetze „%1“ …").arg(r.title));
        }
        QString e;
        const bool ok = DeepL::translateRecipe(r, key, &e);
        if (!ok) {
            // Schlüssel, Kontingent, keine Verbindung: gilt auch für alle weiteren Rezepte
            if (e.contains(QObject::tr("Schlüssel")) || e.contains(QObject::tr("Kontingent")) || e.contains(QObject::tr("Verbindung")))
                *stop = e;
            if (err) *err = e;
        }
        return ok;
    };
}

QString umlautNote(int recipes)
{
    return (recipes == 1 ? QObject::tr("In einem Rezept") : QObject::tr("In %1 Rezepten").arg(recipes))
           + QObject::tr(" wurden fehlende Umlaute ergänzt (ae → ä, oe → ö, ue → ü, ss → ß).");
}

int addHealthySuggestions(QWidget *parent)
{
    RecipeDb &db = RecipeDb::instance();
    if (!db.isOpen()) return 0;
    const auto res = Importers::importFile(QStringLiteral(":/samples/gesund.rk"), Importers::Format::RezKonv);
    int added = 0;
    db.beginBatch();
    for (Recipe r : res.recipes) {
        if (!db.duplicatesOf(r).isEmpty()) continue;
        r.source = QObject::tr("ViviaMeals – gesunde Vorschläge");
        if (db.save(r)) ++added;
    }
    db.endBatch();
    QMessageBox::information(parent, QObject::tr("Gesunde Vorschläge"),
                             added > 0 ? QObject::tr("%1 Rezepte für Frühstück, Zwischenmahlzeiten und leichte Abendessen wurden übernommen. Sie tragen "
                                                     "die Tags „frühstück“, „zwischenmahlzeit“ bzw. „leichte mahlzeit“ und lassen sich wie alle "
                                                     "Rezepte bearbeiten.")
                                             .arg(added)
                                       : QObject::tr("Die Vorschläge sind bereits in der Datenbank."));
    return added;
}

QString languageLabel(const Recipe &r)
{
    if (r.translatedBy == QLatin1String("DeepL"))
        return QObject::tr("%1 → DeepL").arg(RecipeHtml::languageName(r.originalLanguage).toLower());
    if (r.language == QLatin1String("de")) return QObject::tr("deutsch");
    if (r.language == QLatin1String("en")) return QObject::tr("englisch (Glossar)");
    return QObject::tr("%1 (nicht übersetzt)").arg(RecipeHtml::languageName(r.language).toLower());
}

} // namespace ImportFlow
