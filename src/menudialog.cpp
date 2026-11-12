// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "menudialog.h"
#include "icons.h"
#include "importers.h"
#include "latexexport.h"
#include "mealtypes.h"
#include "menuexport.h"
#include "recipedb.h"
#include "recipehtml.h"
#include "recipeview.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QLabel *note(const QString &text)
{
    auto *l = new QLabel(text);
    l->setWordWrap(true);
    l->setTextFormat(Qt::RichText);
    return l;
}

QString orderText(int courses)
{
    QStringList l;
    for (Menu::Course c : Menu::courseOrder(courses)) l << Menu::courseName(c);
    return l.join(QStringLiteral(" · "));
}

QString kcalText(const RecipeSummary *s)
{
    return s && s->hasNutrition ? QStringLiteral("%1 kcal").arg(qRound(s->perServing.kcal)) : QStringLiteral("–");
}

// Tags, die als Thema taugen (Küche, Anlass …), mit Anzahl
QList<QPair<QString, int>> themeTags()
{
    QHash<QString, int> count;
    static const QStringList skip{ QStringLiteral("vegan"), QStringLiteral("vegetarisch"), QStringLiteral("fleischgerichte"), QStringLiteral("fisch"),
                                   QStringLiteral("menüs"), QStringLiteral("vorspeisen"), QStringLiteral("suppen"), QStringLiteral("salate"),
                                   QStringLiteral("zwischengang"), QStringLiteral("hauptgerichte"), QStringLiteral("desserts") };
    for (const RecipeSummary &s : RecipeDb::instance().summaries())
        for (const QString &t : s.tags) {
            const QString k = t.toLower();
            if (skip.contains(k) || MealTypes::mealTags().contains(k) || MealTypes::nutrientTagNames().contains(k)) continue;
            ++count[k];
        }
    QList<QPair<QString, int>> out;
    for (auto it = count.begin(); it != count.end(); ++it)
        if (it.value() >= 2) out << qMakePair(it.key(), it.value());
    std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.first.localeAwareCompare(b.first) < 0; });
    return out;
}

} // namespace

// ---------------------------------------------------------------- Assistent

MenuWizard::MenuWizard(QWidget *parent, const Menu::MenuData *existing) : QWizard(parent)
{
    setObjectName(QStringLiteral("MenuWizard"));
    setWindowTitle(tr("Menüassistent"));
    setWindowIcon(Icons::get(QStringLiteral("menu-card")));
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoBackButtonOnStartPage, true);
    setOption(QWizard::HaveCustomButton1, false);
    setButtonText(QWizard::FinishButton, tr("Speichern und schließen"));
    setButtonText(QWizard::NextButton, tr("Weiter >"));
    setButtonText(QWizard::BackButton, tr("< Zurück"));
    setButtonText(QWizard::CancelButton, tr("Schließen"));
    setPixmap(QWizard::LogoPixmap, Icons::get(QStringLiteral("menu-card")).pixmap(48, 48));
    if (existing) {
        m_menu = *existing;
        m_prefsDirty = false;
    } else {
        m_menu.title = tr("Festliches Menü");
        m_menu.persons = QSettings().value(QStringLiteral("menu/persons"), 4).toInt();
        m_menu.prefs = Menu::Prefs::fromJson(QJsonDocument::fromJson(QSettings().value(QStringLiteral("menu/prefs")).toByteArray()).object());
    }
    setPage(PageFrame, framePage());
    setPage(PagePrefs, prefsPage());
    setPage(PageMenu, menuPage());
    setPage(PageFinish, finishPage());
    updateAvailability();
    if (existing) {
        rebuildComposer();
        m_prefsDirty = false;
        setStartId(PageMenu);
    }
    resize(1100, 720);
}

QWizardPage *MenuWizard::framePage()
{
    auto *p = new QWizardPage;
    p->setTitle(tr("Menü zusammenstellen"));
    p->setSubTitle(tr("Der Assistent stellt aus Ihren Rezepten ein Menü zusammen, das zueinander passt. Jeder Gang lässt sich danach "
                      "austauschen; das fertige Menü kann gespeichert und als Menükarte mit Rezepten exportiert werden."));
    auto *f = new QFormLayout(p);
    m_title = new QLineEdit(m_menu.title);
    f->addRow(tr("&Titel des Menüs:"), m_title);
    m_occasion = new QLineEdit(m_menu.occasion);
    m_occasion->setPlaceholderText(tr("z. B. Weihnachten im Familienkreis, Geburtstag von Anna"));
    f->addRow(tr("&Anlass (optional):"), m_occasion);
    auto *dr = new QHBoxLayout;
    m_useDate = new QCheckBox(tr("am"));
    m_date = new QDateEdit(m_menu.date.isValid() ? m_menu.date : QDate::currentDate());
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat(QStringLiteral("dddd, dd.MM.yyyy"));
    m_date->setLocale(QLocale(QLocale::German, QLocale::Germany));
    m_useDate->setChecked(m_menu.date.isValid());
    m_date->setEnabled(m_menu.date.isValid());
    connect(m_useDate, &QCheckBox::toggled, m_date, &QWidget::setEnabled);
    dr->addWidget(m_useDate);
    dr->addWidget(m_date);
    dr->addStretch();
    f->addRow(tr("Datum:"), dr);
    m_persons = new QSpinBox;
    m_persons->setRange(1, 60);
    m_persons->setValue(m_menu.persons);
    m_persons->setSuffix(tr(" Personen"));
    f->addRow(tr("&Für:"), m_persons);
    auto *box = new QGroupBox(tr("Anzahl der Gänge"));
    auto *bl = new QHBoxLayout(box);
    m_courses = new QButtonGroup(this);
    for (int n : { 3, 4, 5 }) {
        auto *r = new QRadioButton(tr("%1 Gänge").arg(n));
        m_courses->addButton(r, n);
        bl->addWidget(r);
    }
    bl->addStretch();
    m_courses->button(qBound(3, m_menu.prefs.courses, 5))->setChecked(true);
    f->addRow(box);
    m_order = note(QString());
    f->addRow(tr("Folge:"), m_order);
    auto updateOrder = [this] { m_order->setText(QStringLiteral("<b>%1</b>").arg(orderText(m_courses->checkedId()))); };
    connect(m_courses, &QButtonGroup::idClicked, this, [this, updateOrder] {
        updateOrder();
        m_prefsDirty = true;
        updateAvailability();
    });
    updateOrder();
    return p;
}

QWizardPage *MenuWizard::prefsPage()
{
    auto *p = new QWizardPage;
    p->setTitle(tr("Vorlieben"));
    p->setSubTitle(tr("Was soll auf den Tisch? Alle Angaben sind Wünsche – fehlt ein passendes Rezept, schlägt der Assistent das Nächstbeste vor."));
    auto *lay = new QVBoxLayout(p);
    auto *f = new QFormLayout;
    const Menu::Prefs &pr = m_menu.prefs;
    auto combo = [](const QStringList &items, int current) {
        auto *c = new QComboBox;
        c->addItems(items);
        c->setCurrentIndex(qBound(0, current, int(items.size()) - 1));
        return c;
    };
    m_diet = combo({ tr("alles"), tr("vegetarisch"), tr("vegan") }, int(pr.diet));
    f->addRow(tr("&Ernährungsform:"), m_diet);
    m_character = combo({ tr("leicht (etwa %1 kcal je Person)"), tr("ausgewogen (etwa %1 kcal je Person)"), tr("festlich-üppig (etwa %1 kcal je Person)") },
                        int(pr.character));
    f->addRow(tr("&Charakter:"), m_character);
    m_starter = combo({ tr("egal"), tr("Salat"), tr("kalt (Carpaccio, Tatar, Antipasti …)"), tr("warm"), tr("Fisch oder Meeresfrüchte") }, int(pr.starter));
    f->addRow(tr("&Vorspeise:"), m_starter);
    m_main = combo({ tr("egal"), tr("Rind oder Kalb"), tr("Schwein"), tr("Geflügel"), tr("Lamm oder Wild"), tr("Fisch oder Meeresfrüchte"), tr("vegetarisch") },
                   int(pr.main));
    f->addRow(tr("&Hauptgang:"), m_main);
    m_dessert = combo({ tr("egal"), tr("fruchtig"), tr("mit Schokolade"), tr("cremig (Mousse, Panna cotta, Creme …)"), tr("Eis, Parfait oder Sorbet"),
                       tr("Gebäck, Kuchen, Strudel") },
                     int(pr.dessert));
    f->addRow(tr("&Dessert:"), m_dessert);
    m_theme = new QComboBox;
    m_theme->addItem(tr("kein bestimmtes"), QString());
    for (const auto &t : themeTags()) m_theme->addItem(QStringLiteral("%1 (%2)").arg(RecipeHtml::tagLabel(t.first)).arg(t.second), t.first);
    m_theme->setCurrentIndex(qMax(0, m_theme->findData(pr.theme)));
    m_theme->setToolTip(tr("Rezepte mit diesem Tag werden bevorzugt – z. B. eine italienische Menüfolge"));
    f->addRow(tr("&Thema/Küche:"), m_theme);
    m_exclude = new QLineEdit(pr.exclude.join(QStringLiteral(", ")));
    m_exclude->setPlaceholderText(tr("z. B. Pilze, Koriander, Nüsse"));
    m_exclude->setToolTip(tr("Rezepte mit diesen Zutaten (auch Wortteile) werden nicht vorgeschlagen; mehrere durch Komma trennen"));
    f->addRow(tr("&Ohne:"), m_exclude);
    m_favorites = new QCheckBox(tr("Favoriten bevorzugen"));
    m_favorites->setChecked(pr.favorites);
    m_withImage = new QCheckBox(tr("Rezepte mit Foto bevorzugen"));
    m_withImage->setChecked(pr.withImage);
    auto *cb = new QHBoxLayout;
    cb->addWidget(m_favorites);
    cb->addWidget(m_withImage);
    cb->addStretch();
    f->addRow(QString(), cb);
    lay->addLayout(f);
    m_available = note(QString());
    lay->addWidget(m_available);
    m_addSamples = new QPushButton(Icons::get(QStringLiteral("recipe-add")), tr("Festliche Beispielrezepte übernehmen"));
    m_addSamples->setToolTip(tr("16 Rezepte für Vorspeisen, Suppen, Zwischengänge, Hauptgänge und Desserts in die Datenbank übernehmen"));
    lay->addWidget(m_addSamples, 0, Qt::AlignLeft);
    lay->addStretch();
    m_availTimer = new QTimer(this);
    m_availTimer->setSingleShot(true);
    m_availTimer->setInterval(250);
    connect(m_availTimer, &QTimer::timeout, this, &MenuWizard::updateAvailability);
    auto dirty = [this] {
        m_prefsDirty = true;
        m_availTimer->start();
    };
    for (QComboBox *c : { m_diet, m_character, m_starter, m_main, m_dessert, m_theme })
        connect(c, qOverload<int>(&QComboBox::currentIndexChanged), this, dirty);
    connect(m_exclude, &QLineEdit::textEdited, this, dirty);
    connect(m_favorites, &QCheckBox::toggled, this, dirty);
    connect(m_withImage, &QCheckBox::toggled, this, dirty);
    connect(m_addSamples, &QPushButton::clicked, this, [this] {
        RecipeDb &db = RecipeDb::instance();
        int added = 0;
        db.beginBatch();
        for (Recipe r : Importers::importFile(QStringLiteral(":/samples/festlich.rk"), Importers::Format::RezKonv).recipes) {
            if (!db.duplicatesOf(r).isEmpty()) continue;
            r.source = tr("ViviaMeals – festliche Beispielrezepte");
            if (db.save(r)) ++added;
        }
        db.endBatch();
        m_prefsDirty = true;
        updateAvailability();
        QMessageBox::information(this, windowTitle(),
                                 added ? tr("%1 festliche Rezepte wurden übernommen.").arg(added) : tr("Die festlichen Beispielrezepte sind schon vorhanden."));
    });
    return p;
}

QWizardPage *MenuWizard::menuPage()
{
    auto *p = new QWizardPage;
    p->setTitle(tr("Ihr Menü"));
    p->setSubTitle(tr("Gang anklicken, um das Rezept zu sehen. „Anderer Vorschlag“ tauscht den Gang gegen das nächstbeste passende Rezept, "
                      "„Rezept wählen …“ lässt Sie frei wählen. Die Kurzbeschreibung erscheint auf der Menükarte und ist änderbar."));
    auto *lay = new QVBoxLayout(p);
    auto *split = new QSplitter(Qt::Horizontal);
    auto *left = new QWidget;
    auto *ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    m_table = new QTableWidget(0, 4);
    m_table->setHorizontalHeaderLabels({ tr("Gang"), tr("Rezept"), tr("Kurzbeschreibung (Menükarte)"), tr("kcal") });
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->setColumnWidth(1, 230);
    m_table->setWordWrap(true);
    ll->addWidget(m_table, 1);
    auto *bl = new QHBoxLayout;
    auto *other = new QPushButton(Icons::get(QStringLiteral("dice")), tr("&Anderer Vorschlag"));
    other->setToolTip(tr("Den gewählten Gang durch das nächstbeste passende Rezept ersetzen"));
    auto *choose = new QPushButton(Icons::get(QStringLiteral("search")), tr("Rezept &wählen …"));
    auto *descr = new QPushButton(Icons::get(QStringLiteral("refresh")), tr("Beschreibung neu"));
    descr->setToolTip(tr("Kurzbeschreibung des gewählten Gangs aus den Zutaten neu erzeugen"));
    auto *all = new QPushButton(Icons::get(QStringLiteral("refresh")), tr("Ganzes Menü &neu vorschlagen"));
    bl->addWidget(other);
    bl->addWidget(choose);
    bl->addWidget(descr);
    bl->addStretch();
    bl->addWidget(all);
    ll->addLayout(bl);
    m_notes = note(QString());
    ll->addWidget(m_notes);
    split->addWidget(left);
    m_preview = new RecipeView;
    split->addWidget(m_preview);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    split->setSizes({ 620, 440 });
    lay->addWidget(split, 1);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &MenuWizard::showPreview);
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *it) {
        if (m_updatingTable || it->column() != 2 || it->row() >= m_menu.items.size()) return;
        m_menu.items[it->row()].description = it->text().trimmed();
    });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int col) {
        if (col == 1) chooseRecipe();
    });
    connect(other, &QPushButton::clicked, this, [this] {
        const int row = currentRow();
        if (row < 0 || !m_comp) return;
        const QString id = m_comp->alternative(m_menu.items, row);
        if (id.isEmpty()) QMessageBox::information(this, windowTitle(), tr("Für diesen Gang gibt es keinen weiteren passenden Vorschlag."));
        else setRecipe(row, id);
    });
    connect(choose, &QPushButton::clicked, this, &MenuWizard::chooseRecipe);
    connect(descr, &QPushButton::clicked, this, [this] {
        const int row = currentRow();
        if (row < 0) return;
        Recipe r;
        if (RecipeDb::instance().load(m_menu.items[row].recipeId, &r)) m_menu.items[row].description = Menu::describe(r);
        fillTable();
        selectCourse(row);
    });
    connect(all, &QPushButton::clicked, this, [this] {
        proposeNow();
        selectCourse(0);
    });
    return p;
}

QWizardPage *MenuWizard::finishPage()
{
    auto *p = new QWizardPage;
    p->setTitle(tr("Speichern und exportieren"));
    p->setSubTitle(tr("Das Menü wird mit „Speichern und schließen“ in der Rezeptdatenbank gespeichert (Datei → Export → Gespeicherte Menüs). "
                      "Die Menükarte zeigt auf dem Deckblatt mit Schmuckrahmen nur Titel und Kurzbeschreibung der Gänge; die Rezepte folgen auf den nächsten Seiten."));
    auto *lay = new QVBoxLayout(p);
    m_summary = note(QString());
    lay->addWidget(m_summary);
    auto *box = new QGroupBox(tr("Export"));
    auto *f = new QFormLayout(box);
    QSettings s;
    m_withRecipes = new QCheckBox(tr("Rezepte auf den Folgeseiten"));
    m_withRecipes->setChecked(s.value(QStringLiteral("menu/exportRecipes"), true).toBool());
    m_images = new QCheckBox(tr("mit Rezeptbildern"));
    m_images->setChecked(s.value(QStringLiteral("menu/exportImages"), true).toBool());
    m_nutrition = new QCheckBox(tr("mit Nährwerten"));
    m_nutrition->setChecked(s.value(QStringLiteral("menu/exportNutrition"), true).toBool());
    m_scale = new QCheckBox(tr("Mengen auf die Personenzahl umrechnen"));
    m_scale->setChecked(s.value(QStringLiteral("menu/exportScale"), true).toBool());
    f->addRow(m_withRecipes);
    f->addRow(m_images);
    f->addRow(m_nutrition);
    f->addRow(m_scale);
    connect(m_withRecipes, &QCheckBox::toggled, m_images, &QWidget::setEnabled);
    connect(m_withRecipes, &QCheckBox::toggled, m_nutrition, &QWidget::setEnabled);
    connect(m_withRecipes, &QCheckBox::toggled, m_scale, &QWidget::setEnabled);
    m_docClass = new QComboBox;
    m_docClass->addItem(QStringLiteral("scrbook (KOMA-Script)"));
    m_docClass->addItem(QStringLiteral("book"));
    m_docClass->setCurrentIndex(s.value(QStringLiteral("menu/texClass"), 0).toInt());
    f->addRow(tr("LaTeX-Klasse:"), m_docClass);
    auto *bl = new QHBoxLayout;
    m_texButton = new QPushButton(Icons::get(QStringLiteral("tex")), tr("Als &LaTeX exportieren …"));
    auto *odt = new QPushButton(Icons::get(QStringLiteral("odt")), tr("Als Libre&Office-Writer-Datei exportieren …"));
    bl->addWidget(m_texButton);
    bl->addWidget(odt);
    bl->addStretch();
    f->addRow(bl);
    lay->addWidget(box);
    lay->addStretch();
    connect(m_texButton, &QPushButton::clicked, this, &MenuWizard::exportLatex);
    connect(odt, &QPushButton::clicked, this, &MenuWizard::exportOdt);
    return p;
}

Menu::Prefs MenuWizard::prefsFromUi() const
{
    Menu::Prefs p;
    p.courses = m_courses->checkedId();
    p.diet = Menu::Prefs::Diet(m_diet->currentIndex());
    p.character = Menu::Prefs::Character(m_character->currentIndex());
    p.starter = Menu::Prefs::Starter(m_starter->currentIndex());
    p.main = Menu::Prefs::Main(m_main->currentIndex());
    p.dessert = Menu::Prefs::Dessert(m_dessert->currentIndex());
    p.theme = m_theme->currentData().toString();
    for (const QString &x : m_exclude->text().split(QLatin1Char(','), Qt::SkipEmptyParts))
        if (!x.trimmed().isEmpty()) p.exclude << x.trimmed().toLower();
    p.favorites = m_favorites->isChecked();
    p.withImage = m_withImage->isChecked();
    return p;
}

void MenuWizard::readFrame()
{
    m_menu.title = m_title->text().trimmed().isEmpty() ? tr("Menü") : m_title->text().trimmed();
    m_menu.occasion = m_occasion->text().trimmed();
    m_menu.date = m_useDate->isChecked() ? m_date->date() : QDate();
    m_menu.persons = m_persons->value();
}

void MenuWizard::rebuildComposer()
{
    m_comp = std::make_unique<Menu::Composer>(RecipeDb::instance().summaries(), m_menu.prefs);
}

void MenuWizard::updateAvailability()
{
    if (!m_available) return;
    Menu::Prefs p = prefsFromUi();
    // Richtwerte im Charakter anzeigen
    const int idx = m_character->currentIndex();
    const QStringList labels{ tr("leicht (etwa %1 kcal je Person)"), tr("ausgewogen (etwa %1 kcal je Person)"), tr("festlich-üppig (etwa %1 kcal je Person)") };
    for (int i = 0; i < 3; ++i) {
        Menu::Prefs q = p;
        q.character = Menu::Prefs::Character(i);
        m_character->setItemText(i, labels[i].arg(qRound(q.targetKcal() / 50) * 50));
    }
    m_character->setCurrentIndex(idx);
    Menu::Composer comp(RecipeDb::instance().summaries(), p);
    QStringList parts;
    bool missing = false;
    for (Menu::Course c : Menu::courseOrder(p.courses)) {
        const int n = comp.suitableCount(c);
        missing |= n < 2;
        parts << (n < 2 ? QStringLiteral("<span style='color:#c0392b'><b>%1: %2</b></span>") : QStringLiteral("%1: %2")).arg(Menu::courseName(c)).arg(n);
    }
    m_available->setText(tr("Passende Rezepte in der Datenbank – %1").arg(parts.join(QStringLiteral(" · ")))
                         + (missing ? tr("<br>Für manche Gänge gibt es kaum Auswahl. Vorlieben lockern, Rezepte mit Tags wie „Vorspeisen“, „Suppen“, "
                                         "„Desserts“ ergänzen oder die festlichen Beispielrezepte übernehmen.")
                                    : QString()));
    m_addSamples->setVisible(missing);
}

void MenuWizard::initializePage(int id)
{
    if (id == PagePrefs) {
        readFrame();
        updateAvailability();
    } else if (id == PageMenu) {
        readFrame();
        const Menu::Prefs p = prefsFromUi();
        const bool coursesChanged = p.courses != m_menu.items.size();
        m_menu.prefs = p;
        if (m_prefsDirty || m_menu.items.isEmpty() || coursesChanged || !m_comp) {
            rebuildComposer();
            if (m_prefsDirty || m_menu.items.isEmpty() || coursesChanged) proposeNow();
            m_prefsDirty = false;
        }
        fillTable();
        selectCourse(qMax(0, currentRow()));
    } else if (id == PageFinish) {
        QStringList lines;
        lines << QStringLiteral("<b style='font-size:13pt'>%1</b>").arg(m_menu.title.toHtmlEscaped());
        QStringList info;
        if (!m_menu.occasion.isEmpty()) info << m_menu.occasion.toHtmlEscaped();
        if (m_menu.date.isValid()) info << MenuExport::dateText(m_menu.date).toHtmlEscaped();
        info << tr("für %1 Personen").arg(m_menu.persons);
        lines << info.join(QStringLiteral(" · "));
        for (const Menu::Item &it : m_menu.items)
            lines << QStringLiteral("<i>%1:</i> %2").arg(Menu::courseName(it.course).toHtmlEscaped(),
                                                         (it.title.isEmpty() ? tr("(noch offen)") : it.title).toHtmlEscaped());
        if (m_comp) lines << tr("etwa %1 kcal je Person").arg(qRound(m_comp->kcal(m_menu.items) / 10) * 10);
        m_summary->setText(lines.join(QStringLiteral("<br>")));
        const auto inst = LatexExport::findInstallation();
        m_texButton->setEnabled(inst.found());
        m_texButton->setToolTip(inst.found() ? tr("Gefunden: %1").arg(inst.name)
                                             : tr("Keine LaTeX-Installation gefunden (TeX Live, MiKTeX, MacTeX) – bitte installieren und den Assistenten neu öffnen."));
    }
    QWizard::initializePage(id);
}

void MenuWizard::proposeNow(quint32 seed)
{
    if (!m_comp || seed) m_comp = std::make_unique<Menu::Composer>(RecipeDb::instance().summaries(), m_menu.prefs, seed);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QList<Menu::Item> items = m_comp->propose();
    QApplication::restoreOverrideCursor();
    for (Menu::Item &it : items) {
        Recipe r;
        if (!it.recipeId.isEmpty() && RecipeDb::instance().load(it.recipeId, &r)) it.description = Menu::describe(r);
    }
    m_menu.items = items;
    if (m_table) fillTable();
}

void MenuWizard::fillTable()
{
    m_updatingTable = true;
    const int keep = currentRow();
    m_table->setRowCount(int(m_menu.items.size()));
    for (int i = 0; i < m_menu.items.size(); ++i) {
        const Menu::Item &it = m_menu.items[i];
        const RecipeSummary *s = RecipeDb::instance().summary(it.recipeId);
        auto *c0 = new QTableWidgetItem(Menu::courseName(it.course));
        c0->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        QFont bold = c0->font();
        bold.setBold(true);
        c0->setFont(bold);
        auto *c1 = new QTableWidgetItem(s ? s->title : (it.recipeId.isEmpty() ? tr("– bitte wählen –") : tr("%1 (gelöscht)").arg(it.title)));
        c1->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        c1->setToolTip(tr("Doppelklick: anderes Rezept wählen"));
        if (s && s->favorite) c1->setIcon(Icons::get(QStringLiteral("star-filled")));
        auto *c2 = new QTableWidgetItem(it.description);
        c2->setToolTip(tr("Doppelklick zum Ändern – so steht es auf der Menükarte"));
        auto *c3 = new QTableWidgetItem(kcalText(s));
        c3->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        c3->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(i, 0, c0);
        m_table->setItem(i, 1, c1);
        m_table->setItem(i, 2, c2);
        m_table->setItem(i, 3, c3);
    }
    m_table->resizeRowsToContents();
    m_updatingTable = false;
    if (keep >= 0 && keep < m_table->rowCount()) m_table->selectRow(keep);
    updateNotes();
}

void MenuWizard::updateNotes()
{
    if (!m_comp) return;
    QStringList n = m_comp->notes(m_menu.items);
    for (QString &x : n) {
        x = x.toHtmlEscaped();
        if (x.startsWith(QStringLiteral("⚠"))) x = QStringLiteral("<span style='color:#c0392b'>%1</span>").arg(x);
        else if (x.startsWith(QStringLiteral("✓"))) x = QStringLiteral("<span style='color:#2e7d32'>%1</span>").arg(x);
    }
    m_notes->setText(n.join(QStringLiteral("<br>")));
}

int MenuWizard::currentRow() const
{
    if (!m_table) return -1;
    const auto sel = m_table->selectionModel() ? m_table->selectionModel()->selectedRows() : QModelIndexList();
    return sel.isEmpty() ? -1 : sel.first().row();
}

void MenuWizard::selectCourse(int row)
{
    if (row >= 0 && row < m_table->rowCount()) m_table->selectRow(row);
    showPreview();
}

void MenuWizard::showPreview()
{
    const int row = currentRow();
    Recipe r;
    if (row >= 0 && row < m_menu.items.size() && RecipeDb::instance().load(m_menu.items[row].recipeId, &r)) m_preview->setRecipe(r);
    else m_preview->showMessage(tr("Kein Rezept"), tr("Für diesen Gang ist noch kein Rezept gewählt."));
}

void MenuWizard::setRecipe(int row, const QString &id)
{
    if (row < 0 || row >= m_menu.items.size()) return;
    Recipe r;
    if (!RecipeDb::instance().load(id, &r)) return;
    m_menu.items[row].recipeId = id;
    m_menu.items[row].title = r.title;
    m_menu.items[row].description = Menu::describe(r);
    fillTable();
    selectCourse(row);
}

void MenuWizard::chooseRecipe()
{
    const int row = currentRow();
    if (row < 0 || !m_comp) return;
    MenuCandidateDialog d(*m_comp, m_menu.items, row, this);
    if (d.exec() == QDialog::Accepted && !d.recipeId().isEmpty()) setRecipe(row, d.recipeId());
}

bool MenuWizard::loadRecipes(QList<Recipe> *out)
{
    QStringList missing;
    for (Menu::Item &it : m_menu.items) {
        Recipe r;
        if (it.recipeId.isEmpty() || !RecipeDb::instance().load(it.recipeId, &r)) {
            missing << Menu::courseName(it.course);
            r = Recipe();
            r.id.clear();
        } else {
            it.title = r.title;
        }
        *out << r;
    }
    if (!missing.isEmpty()
        && QMessageBox::question(this, windowTitle(),
                                 tr("Für %1 ist kein Rezept vorhanden. Trotzdem exportieren (die Menükarte nennt den Gang als offen)?").arg(missing.join(QStringLiteral(", "))))
               != QMessageBox::Yes)
        return false;
    return true;
}

void MenuWizard::exportLatex()
{
    QList<Recipe> recipes;
    if (!loadRecipes(&recipes)) return;
    QSettings s;
    const QString dir = s.value(QStringLiteral("menu/texDir"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    const QString f = QFileDialog::getSaveFileName(this, tr("Menü als LaTeX exportieren"), QDir(dir).filePath(MenuExport::defaultFileName(m_menu, QStringLiteral("tex"))),
                                                   tr("LaTeX-Dokument (*.tex)"));
    if (f.isEmpty()) return;
    s.setValue(QStringLiteral("menu/texDir"), QFileInfo(f).absolutePath());
    MenuExport::Options o;
    o.docClass = m_docClass->currentIndex() == 1 ? LatexExport::DocClass::Book : LatexExport::DocClass::ScrBook;
    o.recipes = m_withRecipes->isChecked();
    o.images = m_images->isChecked();
    o.nutrition = m_nutrition->isChecked();
    o.scaleToPersons = m_scale->isChecked();
    QString err;
    if (!MenuExport::writeLatex(f, m_menu, recipes, o, &err)) {
        QMessageBox::warning(this, windowTitle(), tr("Die Datei konnte nicht geschrieben werden:\n%1").arg(err));
        return;
    }
    QMessageBox box(QMessageBox::Information, windowTitle(),
                    tr("„%1“ wurde gespeichert (Bilder der Rezepte daneben).<br><br>Übersetzen: <tt>latexmk -pdf %2</tt><br>"
                       "Verzierte Rahmenecken erscheinen, wenn das LaTeX-Paket <i>pgfornament</i> installiert ist; sonst ein schlichterer Schmuckrahmen.")
                        .arg(QDir::toNativeSeparators(f).toHtmlEscaped(), QFileInfo(f).fileName().toHtmlEscaped()),
                    QMessageBox::Ok, this);
    QPushButton *folder = box.addButton(tr("Ordner öffnen"), QMessageBox::ActionRole);
    box.exec();
    if (box.clickedButton() == folder) QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(f).absolutePath()));
}

void MenuWizard::exportOdt()
{
    QList<Recipe> recipes;
    if (!loadRecipes(&recipes)) return;
    QSettings s;
    const QString dir = s.value(QStringLiteral("export/odtDir"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    const QString f = QFileDialog::getSaveFileName(this, tr("Menü als LibreOffice-Writer-Datei exportieren"),
                                                   QDir(dir).filePath(MenuExport::defaultFileName(m_menu, QStringLiteral("odt"))), tr("OpenDocument-Text (*.odt)"));
    if (f.isEmpty()) return;
    s.setValue(QStringLiteral("export/odtDir"), QFileInfo(f).absolutePath());
    MenuExport::Options o;
    o.recipes = m_withRecipes->isChecked();
    o.images = m_images->isChecked();
    o.nutrition = m_nutrition->isChecked();
    o.scaleToPersons = m_scale->isChecked();
    QString err;
    if (!MenuExport::writeOdt(f, m_menu, recipes, o, &err)) {
        QMessageBox::warning(this, windowTitle(), tr("Die Datei konnte nicht geschrieben werden:\n%1").arg(err));
        return;
    }
    QMessageBox box(QMessageBox::Information, windowTitle(), tr("„%1“ wurde gespeichert.").arg(QDir::toNativeSeparators(f)), QMessageBox::Ok, this);
    QPushButton *open = box.addButton(tr("Öffnen"), QMessageBox::ActionRole);
    box.exec();
    if (box.clickedButton() == open) QDesktopServices::openUrl(QUrl::fromLocalFile(f));
}

bool MenuWizard::save(bool quiet)
{
    QString err;
    if (!Menu::saveMenu(RecipeDb::instance().folder(), m_menu, &err)) {
        if (!quiet) QMessageBox::warning(this, windowTitle(), tr("Das Menü konnte nicht gespeichert werden:\n%1").arg(err));
        return false;
    }
    QSettings s;
    s.setValue(QStringLiteral("menu/prefs"), QJsonDocument(m_menu.prefs.toJson()).toJson(QJsonDocument::Compact));
    s.setValue(QStringLiteral("menu/persons"), m_menu.persons);
    return true;
}

void MenuWizard::reject()
{
    if (currentId() >= PageMenu && !m_menu.items.isEmpty()) {
        const auto b = QMessageBox::question(this, windowTitle(), tr("Das Menü vor dem Schließen speichern?"),
                                             QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
        if (b == QMessageBox::Cancel) return;
        if (b == QMessageBox::Save) {
            accept();
            return;
        }
    }
    QWizard::reject();
}

void MenuWizard::accept()
{
    QSettings s;
    s.setValue(QStringLiteral("menu/exportRecipes"), m_withRecipes->isChecked());
    s.setValue(QStringLiteral("menu/exportImages"), m_images->isChecked());
    s.setValue(QStringLiteral("menu/exportNutrition"), m_nutrition->isChecked());
    s.setValue(QStringLiteral("menu/exportScale"), m_scale->isChecked());
    s.setValue(QStringLiteral("menu/texClass"), m_docClass->currentIndex());
    if (!save(false)) return;
    QWizard::accept();
}

// ---------------------------------------------------------------- Rezept wählen

MenuCandidateDialog::MenuCandidateDialog(const Menu::Composer &comp, const QList<Menu::Item> &items, int index, QWidget *parent)
    : QDialog(parent), m_comp(comp), m_items(items), m_index(index)
{
    setObjectName(QStringLiteral("MenuCandidateDialog"));
    setWindowTitle(tr("Rezept für „%1“ wählen").arg(Menu::courseName(items.value(index).course)));
    auto *lay = new QVBoxLayout(this);
    auto *top = new QHBoxLayout;
    m_filter = new QLineEdit;
    m_filter->setPlaceholderText(tr("Filtern (Name, Zutat, Tag) …"));
    m_filter->setClearButtonEnabled(true);
    top->addWidget(m_filter, 1);
    m_all = new QCheckBox(tr("alle Rezepte zeigen"));
    m_all->setToolTip(tr("Auch Rezepte, die der Assistent für diesen Gang nicht vorschlagen würde"));
    top->addWidget(m_all);
    lay->addLayout(top);
    m_list = new QTreeWidget;
    m_list->setHeaderLabels({ tr("Rezept"), tr("Eignung"), tr("kcal"), tr("Hinweise") });
    m_list->setRootIsDecorated(false);
    m_list->setAlternatingRowColors(true);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    lay->addWidget(m_list, 1);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Übernehmen"));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_list, &QTreeWidget::itemDoubleClicked, this, &QDialog::accept);
    connect(m_filter, &QLineEdit::textChanged, this, &MenuCandidateDialog::fill);
    connect(m_all, &QCheckBox::toggled, this, &MenuCandidateDialog::fill);
    fill();
    resize(760, 520);
}

void MenuCandidateDialog::fill()
{
    m_list->clear();
    const QString f = m_filter->text().trimmed().toLower();
    auto matches = [&](const RecipeSummary &s) {
        if (f.isEmpty()) return true;
        if (s.title.toLower().contains(f)) return true;
        for (const QString &x : s.ingredientNames)
            if (x.contains(f)) return true;
        for (const QString &t : s.tags)
            if (t.toLower().contains(f)) return true;
        return false;
    };
    const QList<Menu::Composer::Candidate> cands = m_comp.candidates(m_items, m_index, 100000);
    QSet<QString> shown;
    double best = cands.isEmpty() ? 1 : cands.first().score;
    double worst = cands.isEmpty() ? 0 : cands.last().score;
    for (const auto &c : cands) {
        const RecipeSummary *s = RecipeDb::instance().summary(c.id);
        if (!s || !matches(*s)) continue;
        const double rel = best > worst ? (c.score - worst) / (best - worst) : 1;
        const int stars = 1 + qRound(rel * 4);
        auto *it = new QTreeWidgetItem(m_list, { s->title, QString(stars, QChar(0x2605)) + QString(5 - stars, QChar(0x2606)), kcalText(s), c.notes.join(QStringLiteral("; ")) });
        it->setData(0, Qt::UserRole, c.id);
        if (s->favorite) it->setIcon(0, Icons::get(QStringLiteral("star-filled")));
        shown.insert(c.id);
    }
    if (m_all->isChecked()) {
        QList<const RecipeSummary *> rest;
        for (const RecipeSummary &s : RecipeDb::instance().summaries())
            if (!shown.contains(s.id) && matches(s)) rest << &s;
        std::sort(rest.begin(), rest.end(), [](const RecipeSummary *a, const RecipeSummary *b) { return a->title.localeAwareCompare(b->title) < 0; });
        for (const RecipeSummary *s : rest) {
            auto *it = new QTreeWidgetItem(m_list, { s->title, tr("ungewöhnlich"), kcalText(s), QString() });
            it->setData(0, Qt::UserRole, s->id);
            it->setForeground(1, QBrush(QColor(0x99, 0x99, 0x99)));
        }
    }
    if (m_list->topLevelItemCount() > 0) m_list->setCurrentItem(m_list->topLevelItem(0));
}

QString MenuCandidateDialog::recipeId() const
{
    const QTreeWidgetItem *it = m_list->currentItem();
    return it ? it->data(0, Qt::UserRole).toString() : QString();
}

// ---------------------------------------------------------------- gespeicherte Menüs

MenuListDialog::MenuListDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("MenuListDialog"));
    setWindowTitle(tr("Gespeicherte Menüs"));
    setWindowIcon(Icons::get(QStringLiteral("menu-card")));
    auto *lay = new QVBoxLayout(this);
    m_list = new QTreeWidget;
    m_list->setHeaderLabels({ tr("Menü"), tr("Anlass"), tr("Datum"), tr("Gänge"), tr("Geändert") });
    m_list->setRootIsDecorated(false);
    m_list->setAlternatingRowColors(true);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    lay->addWidget(m_list, 1);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    QPushButton *open = bb->addButton(tr("Öffnen …"), QDialogButtonBox::ActionRole);
    open->setIcon(Icons::get(QStringLiteral("open")));
    QPushButton *neu = bb->addButton(tr("Neues Menü …"), QDialogButtonBox::ActionRole);
    neu->setIcon(Icons::get(QStringLiteral("add")));
    QPushButton *del = bb->addButton(tr("Löschen"), QDialogButtonBox::ActionRole);
    del->setIcon(Icons::get(QStringLiteral("delete")));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto openCurrent = [this] {
        const QTreeWidgetItem *it = m_list->currentItem();
        if (!it) return;
        for (const Menu::MenuData &m : Menu::savedMenus(RecipeDb::instance().folder()))
            if (m.id == it->data(0, Qt::UserRole).toString()) {
                MenuWizard w(this, &m);
                w.exec();
                fill();
                return;
            }
    };
    connect(open, &QPushButton::clicked, this, openCurrent);
    connect(m_list, &QTreeWidget::itemDoubleClicked, this, openCurrent);
    connect(neu, &QPushButton::clicked, this, [this] {
        MenuWizard w(this);
        w.exec();
        fill();
    });
    connect(del, &QPushButton::clicked, this, [this] {
        const QTreeWidgetItem *it = m_list->currentItem();
        if (!it) return;
        if (QMessageBox::question(this, windowTitle(), tr("Das Menü „%1“ löschen? Die Rezepte bleiben erhalten.").arg(it->text(0))) != QMessageBox::Yes) return;
        Menu::removeMenu(RecipeDb::instance().folder(), it->data(0, Qt::UserRole).toString());
        fill();
    });
    fill();
    resize(760, 420);
}

void MenuListDialog::fill()
{
    m_list->clear();
    const QLocale de(QLocale::German);
    for (const Menu::MenuData &m : Menu::savedMenus(RecipeDb::instance().folder())) {
        auto *it = new QTreeWidgetItem(m_list, { m.title, m.occasion, m.date.isValid() ? de.toString(m.date, QStringLiteral("dd.MM.yyyy")) : QString(),
                                                 QString::number(m.items.size()), de.toString(m.modified, QStringLiteral("dd.MM.yyyy HH:mm")) });
        it->setData(0, Qt::UserRole, m.id);
        it->setIcon(0, Icons::get(QStringLiteral("menu-card")));
        QStringList tip;
        for (const Menu::Item &i : m.items) tip << QStringLiteral("%1: %2").arg(Menu::courseName(i.course), i.title);
        it->setToolTip(0, tip.join(QLatin1Char('\n')));
    }
    if (m_list->topLevelItemCount() > 0) m_list->setCurrentItem(m_list->topLevelItem(0));
}
