// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "mainwindow.h"
#include "foodwizard.h"
#include "importdialogs.h"
#include "mealtypes.h"
#include "nutrition.h"
#include "packagedialog.h"
#include "scandialog.h"
#include "duplicatesdialog.h"
#include "assistantdialog.h"
#include "editordialog.h"
#include "exportdialogs.h"
#include "fooddialog.h"
#include "fridgedialog.h"
#include "icons.h"
#include "importdialogs.h"
#include "mealplandialog.h"
#include "nutrition.h"
#include "profiles.h"
#include "recipedb.h"
#include "recipehtml.h"
#include "recipeview.h"
#include "searchdialog.h"
#include "settingsdialog.h"
#include "themes.h"
#include "umlauts.h"
#include "weighindialog.h"
#include "diarydialog.h"
#include "mealtypes.h"
#include "odtexport.h"
#include "deepl.h"

#include <QAction>
#include <QActionGroup>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QApplication>
#include <QCloseEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextDocument>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("ViviaMeals"));
    setWindowIcon(Icons::app());

    auto *central = new QWidget;
    auto *lay = new QVBoxLayout(central);
    lay->setContentsMargins(6, 6, 6, 2);

    // Kopfbereich (wie das Stammblatt in ViviaParms bzw. der Kopf in ViviaBreathe)
    auto *head = new QFrame;
    head->setFrameShape(QFrame::StyledPanel);
    head->setFrameShadow(QFrame::Sunken);
    auto *hl = new QHBoxLayout(head);
    hl->setContentsMargins(8, 4, 8, 4);
    auto *logo = new QLabel;
    logo->setPixmap(Icons::app().pixmap(44, 44));
    hl->addWidget(logo, 0, Qt::AlignTop);
    auto *ht = new QVBoxLayout;
    ht->setSpacing(0);
    m_headName = new QLabel;
    QFont nf = m_headName->font();
    nf.setPointSizeF(nf.pointSizeF() * 1.45);
    nf.setBold(true);
    m_headName->setFont(nf);
    ht->addWidget(m_headName);
    m_headInfo = new QLabel;
    m_headInfo->setTextFormat(Qt::RichText);
    m_headInfo->setWordWrap(true);
    ht->addWidget(m_headInfo);
    hl->addLayout(ht, 1);
    lay->addWidget(head);

    // Erinnerung an den Wiegetag (Assistent Gewichtskontrolle)
    m_reminder = new QFrame;
    m_reminder->setObjectName(QStringLiteral("weighReminder"));
    m_reminder->setFrameShape(QFrame::StyledPanel);
    m_reminder->setStyleSheet(QStringLiteral("QFrame#weighReminder { background: rgba(224, 98, 13, 40); border: 1px solid #e0620d; border-radius: 4px; }"));
    auto *rl = new QHBoxLayout(m_reminder);
    rl->setContentsMargins(8, 4, 8, 4);
    auto *ricon = new QLabel;
    ricon->setPixmap(Icons::get(QStringLiteral("scale")).pixmap(22, 22));
    rl->addWidget(ricon);
    m_reminderText = new QLabel;
    m_reminderText->setWordWrap(true);
    rl->addWidget(m_reminderText, 1);
    auto *weigh = new QPushButton(Icons::get(QStringLiteral("add")), tr("Gewicht eintragen …"));
    auto *done = new QPushButton(tr("Erledigt"));
    done->setToolTip(tr("Erinnerung bis zum nächsten Wiegetag ausblenden"));
    rl->addWidget(weigh);
    rl->addWidget(done);
    lay->addWidget(m_reminder);
    m_reminder->hide();
    connect(weigh, &QPushButton::clicked, this, &MainWindow::openWeighIn);
    connect(done, &QPushButton::clicked, this, [this] {
        for (Profile &p : Profiles::instance().all())
            if (p.weighReminderDue()) p.markWeighDone();
        Profiles::instance().save();
        updateReminder();
    });

    m_split = new QSplitter(Qt::Horizontal);
    auto *left = new QWidget;
    auto *ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    m_filter = new QLineEdit;
    m_filter->setPlaceholderText(tr("Rezepte filtern (Name, Zutat, Tag) …"));
    m_filter->setClearButtonEnabled(true);
    m_filter->addAction(Icons::get(QStringLiteral("search")), QLineEdit::LeadingPosition);
    ll->addWidget(m_filter);
    m_tagFilter = new QComboBox;
    ll->addWidget(m_tagFilter);
    m_list = new QListWidget;
    m_list->setUniformItemSizes(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    ll->addWidget(m_list, 1);
    m_split->addWidget(left);
    m_view = new RecipeView;
    m_split->addWidget(m_view);
    m_split->setStretchFactor(0, 1);
    m_split->setStretchFactor(1, 3);
    m_split->setSizes({ 280, 820 });
    lay->addWidget(m_split, 1);
    setCentralWidget(central);

    m_statusDb = new QLabel;
    statusBar()->addPermanentWidget(m_statusDb);

    buildMenus();
    buildToolBar();

    auto *filterTimer = new QTimer(this);
    filterTimer->setSingleShot(true);
    filterTimer->setInterval(150);
    connect(filterTimer, &QTimer::timeout, this, &MainWindow::refreshList);
    connect(m_filter, &QLineEdit::textChanged, filterTimer, qOverload<>(&QTimer::start));
    connect(m_tagFilter, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::refreshList);
    connect(m_list, &QListWidget::currentRowChanged, this, &MainWindow::showCurrent);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &MainWindow::editRecipe);
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        if (!m_list->itemAt(pos)) return;
        QMenu menu(this);
        menu.addAction(Icons::get(QStringLiteral("edit")), tr("Bearbeiten …"), this, &MainWindow::editRecipe);
        menu.addAction(Icons::get(QStringLiteral("tex")), tr("Als LaTeX-Datei exportieren …"), this, &MainWindow::exportLatex);
        menu.addAction(Icons::get(QStringLiteral("odt")), tr("Als LibreOffice-Writer-Datei (.odt) exportieren …"), this, &MainWindow::exportOdt);
        menu.addSeparator();
        menu.addAction(Icons::get(QStringLiteral("diary")), tr("Ins Ernährungstagebuch eintragen …"), this, [this] { openDiary(currentId()); });
        menu.addAction(Icons::get(QStringLiteral("save")), tr("Als .vmr-Datei speichern …"), this, &MainWindow::exportVmr);
        menu.addAction(Icons::get(QStringLiteral("print")), tr("Drucken …"), this, &MainWindow::printRecipe);
        menu.addSeparator();
        menu.addAction(Icons::get(QStringLiteral("delete")), tr("Löschen …"), this, &MainWindow::deleteRecipe);
        menu.exec(m_list->viewport()->mapToGlobal(pos));
    });
    connect(m_view, &RecipeView::editRequested, this, &MainWindow::editRecipe);
    connect(m_view, &RecipeView::favoriteToggled, this, [this](bool on) {
        Recipe r;
        if (!RecipeDb::instance().load(currentId(), &r)) return;
        r.favorite = on;
        m_pendingSelect = r.id;
        RecipeDb::instance().save(r);
    });
    connect(&RecipeDb::instance(), &RecipeDb::changed, this, [this] {
        refreshTags();
        refreshList();
        refreshHeader();
        updateActions();
    });

    QSettings s;
    if (!restoreGeometry(s.value(QStringLiteral("ui/geometry")).toByteArray())) resize(1180, 780);
    restoreState(s.value(QStringLiteral("ui/state")).toByteArray());
    m_split->restoreState(s.value(QStringLiteral("ui/split")).toByteArray());
    refreshHeader();
    updateActions();
    updateReminder();
    m_view->showMessage(tr("Willkommen bei ViviaMeals"), tr("Keine Rezeptdatenbank geöffnet."));
}

// ---------------------------------------------------------------- Menüs und Werkzeugleiste

void MainWindow::buildMenus()
{
    QMenu *file = menuBar()->addMenu(tr("&Datei"));
    QAction *a = file->addAction(Icons::get(QStringLiteral("database-add")), tr("&Neue Rezeptdatenbank …"), this, &MainWindow::newDatabase);
    a->setObjectName(QStringLiteral("actNewDb"));
    a = file->addAction(Icons::get(QStringLiteral("open")), tr("Rezeptdatenbank ö&ffnen …"), this, &MainWindow::openDatabaseDialog);
    a->setShortcut(QKeySequence::Open);
    a->setObjectName(QStringLiteral("actOpenDb"));
    m_recent = file->addMenu(Icons::get(QStringLiteral("database")), tr("&Zuletzt geöffnet"));
    file->addSeparator();
    a = file->addAction(Icons::get(QStringLiteral("recipe-add")), tr("Neues &Rezept …"), this, &MainWindow::newRecipe);
    a->setShortcut(QKeySequence::New);
    a->setObjectName(QStringLiteral("actNewRecipe"));
    m_needDb << a;
    a = file->addAction(Icons::get(QStringLiteral("edit")), tr("Rezept &bearbeiten …"), this, &MainWindow::editRecipe);
    a->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    a->setObjectName(QStringLiteral("actEdit"));
    m_needRecipe << a;
    a = file->addAction(Icons::get(QStringLiteral("delete")), tr("Rezept &löschen …"), this, &MainWindow::deleteRecipe);
    a->setShortcut(QKeySequence::Delete);
    a->setShortcutContext(Qt::WidgetShortcut);
    m_list->addAction(a);
    m_needRecipe << a;
    a = file->addAction(Icons::get(QStringLiteral("search")), tr("&Suchen …"), this, &MainWindow::search);
    a->setShortcut(QKeySequence::Find);
    a->setObjectName(QStringLiteral("actSearch"));
    m_needDb << a;
    file->addSeparator();
    QMenu *imp = file->addMenu(Icons::get(QStringLiteral("import")), tr("&Import"));
    a = imp->addAction(Icons::get(QStringLiteral("import-rk")), tr("&RezKonv-Rezepte importieren …"), this,
                       [this] { importFormat(Importers::Format::RezKonv); });
    a->setObjectName(QStringLiteral("actImportRk"));
    m_needDb << a;
    a = imp->addAction(Icons::get(QStringLiteral("import-mm")), tr("&MealMaster-Rezepte importieren …"), this,
                       [this] { importFormat(Importers::Format::MealMaster); });
    a->setObjectName(QStringLiteral("actImportMm"));
    m_needDb << a;
    imp->addSeparator();
    a = imp->addAction(Icons::get(QStringLiteral("scan")), tr("Rezept &scannen …"), this, [this] { scanRecipe(false); });
    a->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    a->setObjectName(QStringLiteral("actScan"));
    a->setStatusTip(tr("Rezeptseite scannen, Text erkennen und als neues Rezept übernehmen"));
    m_needDb << a;
    a = imp->addAction(Icons::get(QStringLiteral("ocr")), tr("Rezept aus &Bild oder Foto erkennen …"), this, [this] { scanRecipe(true); });
    a->setObjectName(QStringLiteral("actScanImage"));
    m_needDb << a;
    imp->addSeparator();
    a = imp->addAction(Icons::get(QStringLiteral("import")), tr("Alte ViviaMeals-&Liste (.vtm) importieren …"), this,
                       [this] { importFormat(Importers::Format::Vtm); });
    m_needDb << a;
    a = imp->addAction(Icons::get(QStringLiteral("recipe")), tr("ViviaMeals-Rezept (.vmr) importieren …"), this, [this] {
        const QStringList files = QFileDialog::getOpenFileNames(this, tr("ViviaMeals-Rezepte importieren"), QString(), tr("ViviaMeals-Rezepte (*.vmr)"));
        QString first;
        for (const QString &f : files) {
            Recipe r;
            QString err;
            if (!Recipe::load(f, &r, &err)) {
                QMessageBox::warning(this, tr("Import"), QStringLiteral("%1: %2").arg(QFileInfo(f).fileName(), err));
                continue;
            }
            if (RecipeDb::instance().summary(r.id)) r.id = Recipe::newId();
            RecipeDb::instance().save(r);
            if (first.isEmpty()) first = r.id;
        }
        if (!first.isEmpty()) selectRecipe(first);
    });
    m_needDb << a;
    imp->addSeparator();
    a = imp->addAction(Icons::get(QStringLiteral("database")), tr("Rezept&paket (.vmpack) übernehmen …"), this, [this] { importPackage(); });
    a->setStatusTip(tr("Eine von anderen ViviaMeals-Anwendern weitergegebene Rezeptsammlung übernehmen oder als neue Datenbank anlegen"));
    QMenu *exp = file->addMenu(Icons::get(QStringLiteral("web")), tr("&Export"));
    a = exp->addAction(Icons::get(QStringLiteral("web")), tr("Als &Website exportieren …"), this, &MainWindow::exportWebsite);
    a->setObjectName(QStringLiteral("actWeb"));
    m_needDb << a;
    a = exp->addAction(Icons::get(QStringLiteral("tex")), tr("Als &LaTeX-Datei exportieren …"), this, &MainWindow::exportLatex);
    a->setObjectName(QStringLiteral("actTex"));
    m_needRecipe << a;
    a = exp->addAction(Icons::get(QStringLiteral("odt")), tr("Als Libre&Office-Writer-Datei (.odt) exportieren …"), this, &MainWindow::exportOdt);
    a->setObjectName(QStringLiteral("actOdt"));
    m_needRecipe << a;
    a = exp->addAction(Icons::get(QStringLiteral("save")), tr("Als ViviaMeals-Rezept (.&vmr) speichern …"), this, &MainWindow::exportVmr);
    m_needRecipe << a;
    exp->addSeparator();
    a = exp->addAction(Icons::get(QStringLiteral("database")), tr("Rezept&datenbank weitergeben (.vmpack) …"), this, &MainWindow::exportPackage);
    a->setStatusTip(tr("Rezeptsammlung als Paket für andere ViviaMeals-Anwender speichern – ohne Benutzer, Ziele, Tagebuch und Pläne"));
    a->setObjectName(QStringLiteral("actPackage"));
    m_needDb << a;
    file->addSeparator();
    a = file->addAction(Icons::get(QStringLiteral("delete")), tr("Datenbank &leeren …"), this, &MainWindow::clearDatabase);
    a->setStatusTip(tr("Alle Rezepte der geöffneten Datenbank löschen (mit Sicherheitsabfrage und Sicherungskopie)"));
    m_needDb << a;
    file->addSeparator();
    a = file->addAction(Icons::get(QStringLiteral("print")), tr("&Drucken …"), this, &MainWindow::printRecipe);
    a->setShortcut(QKeySequence::Print);
    a->setObjectName(QStringLiteral("actPrint"));
    m_needRecipe << a;
    file->addSeparator();
    a = file->addAction(Icons::get(QStringLiteral("quit")), tr("Programm &beenden"), this, &QWidget::close);
    a->setShortcut(QKeySequence::Quit);

    QMenu *plan = menuBar()->addMenu(tr("&Planen"));
    a = plan->addAction(Icons::get(QStringLiteral("fridge")), tr("&Kühlschrank – was koche ich heute? …"), this, &MainWindow::openFridge);
    a->setShortcut(QKeySequence(Qt::Key_F5));
    a->setObjectName(QStringLiteral("actFridge"));
    m_needDb << a;
    a = plan->addAction(Icons::get(QStringLiteral("scale")), tr("&Assistent Gewichtskontrolle …"), this, &MainWindow::openAssistant);
    a->setShortcut(QKeySequence(Qt::Key_F6));
    a->setObjectName(QStringLiteral("actAssistant"));
    a = plan->addAction(Icons::get(QStringLiteral("diary")), tr("&Ernährungstagebuch …"), this, [this] { openDiary(); });
    a->setShortcut(QKeySequence(Qt::Key_F9));
    a->setObjectName(QStringLiteral("actDiary"));
    a->setStatusTip(tr("Gegessene Mahlzeiten je Person mit Uhrzeit, Menge und Kalorien erfassen – mit Restkalorien für den Tag"));
    m_diaryAction = a;
    a = plan->addAction(Icons::get(QStringLiteral("chart")), tr("&Gewichtskontrolle …"), this, &MainWindow::openWeighIn);
    a->setShortcut(QKeySequence(Qt::Key_F8));
    a->setObjectName(QStringLiteral("actWeighIn"));
    a->setStatusTip(tr("Gewicht für eine Person eintragen – es wird in ihren Gewichtsverlauf übernommen"));
    a = plan->addAction(Icons::get(QStringLiteral("plan")), tr("&Wochenmenü-Plan …"), this, [this] { openMealPlan(); });
    a->setShortcut(QKeySequence(Qt::Key_F7));
    a->setObjectName(QStringLiteral("actPlan"));
    m_needDb << a;
    plan->addSeparator();
    a = plan->addAction(Icons::get(QStringLiteral("dice")), tr("&Zufälliges Rezept"), this, &MainWindow::showRandom);
    a->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    a->setObjectName(QStringLiteral("actRandom"));
    m_needDb << a;

    QMenu *extras = menuBar()->addMenu(tr("E&xtras"));
    a = extras->addAction(Icons::get(QStringLiteral("nutrition")), tr("&Nährwerttabelle …"), this, &MainWindow::openFoodTable);
    a = extras->addAction(Icons::get(QStringLiteral("add")), tr("Eigenes &Lebensmittel anlegen …"), this, &MainWindow::newFood);
    a->setStatusTip(tr("Assistent für Fertigprodukte und andere Lebensmittel mit den Nährwerten von der Verpackung"));
    a = extras->addAction(Icons::get(QStringLiteral("nutrition")), tr("Zutaten &ohne Nährwerte …"), this, &MainWindow::missingFoods);
    a->setStatusTip(tr("Alle Zutaten der Datenbank ohne Eintrag in der Nährwerttabelle auflisten, anlegen oder zuordnen"));
    m_needDb << a;
    a = extras->addAction(Icons::get(QStringLiteral("duplicates")), tr("&Doubletten suchen …"), this, &MainWindow::findDuplicates);
    a->setObjectName(QStringLiteral("actDuplicates"));
    a->setStatusTip(tr("Rezepte mit gleichem oder ähnlichem Titel bzw. gleichen Zutaten finden, vergleichen, zusammenführen und löschen"));
    m_needDb << a;
    a = extras->addAction(Icons::get(QStringLiteral("refresh")), tr("Nährwerte aller Rezepte neu &berechnen"), this, &MainWindow::recalcAll);
    m_needDb << a;
    a = extras->addAction(Icons::get(QStringLiteral("tag")), tr("&Mahlzeit- und Nährwert-Tags für alle Rezepte ergänzen …"), this, &MainWindow::mealTagsAll);
    a->setStatusTip(tr("Frühstück, Hauptgericht, Zwischenmahlzeit, Dessert, Süßspeise, Backen sowie eiweißreich, kalorienarm, fettarm, "
                       "hochkalorisch, vitaminreich und reduktionsdiät aus Titel, Zutaten und Nährwerten ableiten"));
    m_needDb << a;
    a = extras->addAction(Icons::get(QStringLiteral("umlaut")), tr("&Umlaute in allen Rezepten korrigieren …"), this, &MainWindow::fixUmlautsAll);
    a->setStatusTip(tr("Rezepte ohne ä, ö, ü und ß (z. B. „Kaese“, „heiss“) in der ganzen Datenbank korrigieren"));
    m_needDb << a;
    a = extras->addAction(Icons::get(QStringLiteral("translate")), tr("Fremdsprachige Rezepte mit DeepL ü&bersetzen …"), this, &MainWindow::translateAll);
    m_needDb << a;
    extras->addSeparator();
    QMenu *themes = extras->addMenu(Icons::get(QStringLiteral("theme")), tr("&Darstellung"));
    auto *group = new QActionGroup(this);
    for (const QString &id : Themes::available()) {
        QAction *t = themes->addAction(Themes::displayName(id));
        t->setCheckable(true);
        t->setChecked(id == Themes::current());
        group->addAction(t);
        connect(t, &QAction::triggered, this, [this, id] {
            Themes::apply(id);
            appearanceChanged();
        });
    }
    a = extras->addAction(Icons::get(QStringLiteral("settings")), tr("&Einstellungen …"), this, &MainWindow::openSettings);
    a->setShortcut(QKeySequence::Preferences);

    QMenu *help = menuBar()->addMenu(tr("&Hilfe"));
    help->addAction(Icons::get(QStringLiteral("info")), tr("&Über ViviaMeals …"), this, &MainWindow::about);
    help->addAction(tr("Über &Qt …"), qApp, &QApplication::aboutQt);
    updateRecentMenu();
}

void MainWindow::buildToolBar()
{
    QToolBar *tb = addToolBar(tr("Werkzeugleiste"));
    tb->setObjectName(QStringLiteral("mainToolBar"));
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    tb->setIconSize(QSize(24, 24));
    auto add = [&](const char *name, const QString &text, bool important) {
        QAction *a = findChild<QAction *>(QLatin1String(name));
        if (!a) return;
        a->setIconText(text);
        a->setPriority(important ? QAction::NormalPriority : QAction::LowPriority);   // LowPriority: nur Symbol
        tb->addAction(a);
    };
    add("actOpenDb", tr("Öffnen"), false);
    add("actNewRecipe", tr("Neues Rezept"), true);
    add("actEdit", tr("Bearbeiten"), true);
    add("actSearch", tr("Suchen"), true);
    tb->addSeparator();
    add("actImportRk", tr("RezKonv"), false);
    add("actImportMm", tr("MealMaster"), false);
    add("actScan", tr("Scannen"), true);
    add("actWeb", tr("Website"), false);
    add("actTex", tr("LaTeX"), false);
    tb->addSeparator();
    add("actFridge", tr("Kühlschrank"), true);
    add("actAssistant", tr("Assistent"), true);
    add("actPlan", tr("Wochenplan"), true);
    add("actDiary", tr("Tagebuch"), true);
    tb->addSeparator();
    add("actRandom", tr("Zufall"), false);
    add("actPrint", tr("Drucken"), false);
}

void MainWindow::updateActions()
{
    const bool db = RecipeDb::instance().isOpen();
    const bool rec = db && m_view->hasRecipe();
    for (QAction *a : m_needDb) a->setEnabled(db);
    for (QAction *a : m_needRecipe) a->setEnabled(rec);
}

// ---------------------------------------------------------------- Datenbank

void MainWindow::rememberDatabase(const QString &folder)
{
    QSettings s;
    s.setValue(QStringLiteral("db/last"), folder);
    QStringList recent = s.value(QStringLiteral("db/recent")).toStringList();
    recent.removeAll(folder);
    recent.prepend(folder);
    while (recent.size() > 8) recent.removeLast();
    s.setValue(QStringLiteral("db/recent"), recent);
    updateRecentMenu();
}

void MainWindow::updateRecentMenu()
{
    m_recent->clear();
    const QStringList recent = QSettings().value(QStringLiteral("db/recent")).toStringList();
    for (const QString &f : recent) {
        if (!RecipeDb::isDatabaseFolder(f)) continue;
        m_recent->addAction(QDir::toNativeSeparators(f), this, [this, f] { openDatabase(f); });
    }
    m_recent->setEnabled(!m_recent->isEmpty());
}

bool MainWindow::openDatabase(const QString &path, bool quiet)
{
    QString err;
    if (!RecipeDb::instance().open(path, &err)) {
        if (!quiet) QMessageBox::warning(this, tr("Rezeptdatenbank öffnen"), err);
        return false;
    }
    rememberDatabase(RecipeDb::instance().folder());
    m_filter->clear();
    if (QSettings().value(QStringLiteral("ui/randomStart"), true).toBool() || RecipeDb::instance().count() > 0) showRandom();
    if (RecipeDb::instance().count() == 0)
        m_view->showMessage(tr("Die Datenbank „%1“ ist leer").arg(RecipeDb::instance().name()),
                            tr("Legen Sie mit <b>Datei → Neues Rezept</b> ein Rezept an oder importieren Sie Rezepte im "
                               "RezKonv- oder MealMaster-Format (<b>Datei → Import</b>)."));
    updateActions();
    return true;
}

bool MainWindow::ensureDatabase()
{
    QSettings s;
    const QString last = s.value(QStringLiteral("db/last")).toString();
    if (!last.isEmpty() && RecipeDb::isDatabaseFolder(last) && openDatabase(last, true)) return true;
    // keine Datenbank: zum Anlegen auffordern
    QMessageBox box(this);
    box.setWindowTitle(tr("Willkommen bei ViviaMeals"));
    box.setIconPixmap(Icons::app().pixmap(64, 64));
    box.setText(tr("<b>Es ist noch keine Rezeptdatenbank vorhanden.</b>"));
    box.setInformativeText(tr("Eine ViviaMeals-Rezeptdatenbank ist ein Ordner, in dem jedes Rezept als eigene .vmr-Datei liegt. "
                              "Bitte legen Sie eine neue Datenbank an oder öffnen Sie eine vorhandene."));
    QPushButton *create = box.addButton(tr("Neue Datenbank anlegen …"), QMessageBox::AcceptRole);
    QPushButton *samples = box.addButton(tr("Mit Beispielrezepten anlegen …"), QMessageBox::AcceptRole);
    QPushButton *open = box.addButton(tr("Vorhandene öffnen …"), QMessageBox::AcceptRole);
    box.addButton(tr("Später"), QMessageBox::RejectRole);
    box.setDefaultButton(samples);
    box.exec();
    if (box.clickedButton() == create || box.clickedButton() == samples) {
        newDatabase();
        if (RecipeDb::instance().isOpen() && box.clickedButton() == samples) {
            // Beispielrezepte aus den Ressourcen importieren (ohne Rückfragen)
            RecipeDb::instance().beginBatch();
            for (const QString &f : { QStringLiteral(":/samples/beispiele.rk"), QStringLiteral(":/samples/beispiele.mmf") }) {
                const Importers::Result res = Importers::importFile(f, Importers::Format::Unknown);
                for (Recipe r : res.recipes) RecipeDb::instance().save(r);
            }
            RecipeDb::instance().endBatch();
            showRandom();
        }
    } else if (box.clickedButton() == open) {
        openDatabaseDialog();
    }
    return RecipeDb::instance().isOpen();
}

void MainWindow::newDatabase()
{
    createDatabase(tr("Meine Rezepte"));
}

bool MainWindow::createDatabase(const QString &defaultName)
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Neue Rezeptdatenbank"), tr("Name der Datenbank:"), QLineEdit::Normal,
                                               defaultName, &ok);
    if (!ok || name.trimmed().isEmpty()) return false;
    const QString parentDir = QFileDialog::getExistingDirectory(this, tr("Ordner wählen, in dem die Datenbank angelegt wird"), base);
    if (parentDir.isEmpty()) return false;
    QString folderName = name.trimmed();
    folderName.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    const QString folder = QDir(parentDir).filePath(folderName);
    if (QDir(folder).exists() && !QDir(folder).isEmpty() && !RecipeDb::isDatabaseFolder(folder)) {
        QMessageBox::warning(this, tr("Neue Rezeptdatenbank"), tr("Der Ordner %1 existiert bereits und ist nicht leer.").arg(QDir::toNativeSeparators(folder)));
        return false;
    }
    QString err;
    if (!RecipeDb::instance().create(folder, name.trimmed(), &err)) {
        QMessageBox::warning(this, tr("Neue Rezeptdatenbank"), err);
        return false;
    }
    return openDatabase(folder);
}

void MainWindow::openDatabaseDialog()
{
    const QString start = RecipeDb::instance().isOpen() ? QFileInfo(RecipeDb::instance().folder()).absolutePath()
                                                         : QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString file = QFileDialog::getOpenFileName(this, tr("Rezeptdatenbank öffnen"), start,
                                                      tr("ViviaMeals-Datenbank (%1);;Alle Dateien (*)").arg(RecipeDb::markerName()));
    if (!file.isEmpty()) openDatabase(file);
}

// ---------------------------------------------------------------- Liste und Anzeige

void MainWindow::refreshHeader()
{
    updateReminder();
    const RecipeDb &db = RecipeDb::instance();
    if (!db.isOpen()) {
        m_headName->setText(QStringLiteral("ViviaMeals"));
        m_headInfo->setText(tr("Rezeptverwaltung mit Nährwertanalyse und Gewichtskontrolle – keine Datenbank geöffnet"));
        m_statusDb->clear();
        setWindowTitle(QStringLiteral("ViviaMeals"));
        return;
    }
    int withNut = 0, fav = 0, veg = 0;
    for (const RecipeSummary &s : db.summaries()) {
        if (s.hasNutrition) ++withNut;
        if (s.favorite) ++fav;
        if (s.tags.contains(QStringLiteral("vegetarisch")) || s.tags.contains(QStringLiteral("vegan"))) ++veg;
    }
    m_headName->setText(db.name());
    m_headInfo->setText(tr("<b>%1</b> Rezepte · %2 mit Nährwerten · %3 vegetarisch/vegan · %4 Favoriten")
                            .arg(db.count())
                            .arg(withNut)
                            .arg(veg)
                            .arg(fav));
    m_statusDb->setText(tr("Datenbank: %1").arg(QDir::toNativeSeparators(db.folder())));
    setWindowTitle(QStringLiteral("%1 – ViviaMeals").arg(db.name()));
}

void MainWindow::refreshTags()
{
    const QString cur = m_tagFilter->currentData().toString();
    m_tagFilter->blockSignals(true);
    m_tagFilter->clear();
    m_tagFilter->addItem(Icons::get(QStringLiteral("tag")), tr("Alle Themen"), QString());
    m_tagFilter->addItem(Icons::get(QStringLiteral("star-filled")), tr("Favoriten"), QStringLiteral(":fav"));
    QStringList tags = RecipeDb::instance().allTags();
    std::sort(tags.begin(), tags.end(), [](const QString &a, const QString &b) { return QString::localeAwareCompare(a, b) < 0; });
    for (const QString &t : tags) m_tagFilter->addItem(RecipeHtml::tagLabel(t), t);
    const int i = m_tagFilter->findData(cur);
    m_tagFilter->setCurrentIndex(i >= 0 ? i : 0);
    m_tagFilter->blockSignals(false);
}

void MainWindow::refreshList()
{
    const QString keep = m_pendingSelect.isEmpty() ? currentId() : m_pendingSelect;
    m_pendingSelect.clear();
    const QString text = m_filter->text().trimmed();
    const QString tag = m_tagFilter->currentData().toString();
    m_list->blockSignals(true);
    m_list->clear();
    const QIcon star = Icons::get(QStringLiteral("star-filled"));
    const QIcon plain = Icons::get(QStringLiteral("recipe"));
    int select = -1;
    for (const RecipeSummary &s : RecipeDb::instance().summaries()) {
        if (!text.isEmpty() && !SearchDialog::matches(s, text, true)) continue;
        if (tag == QLatin1String(":fav") && !s.favorite) continue;
        if (!tag.isEmpty() && !tag.startsWith(QLatin1Char(':')) && !s.tags.contains(tag, Qt::CaseInsensitive)) continue;
        auto *it = new QListWidgetItem(s.favorite ? star : plain, s.title);
        it->setData(Qt::UserRole, s.id);
        it->setToolTip(s.hasNutrition ? tr("%1 kcal je Portion · %2").arg(qRound(s.perServing.kcal)).arg(s.tags.join(QStringLiteral(", ")))
                                      : s.tags.join(QStringLiteral(", ")));
        m_list->addItem(it);
        if (s.id == keep) select = m_list->count() - 1;
    }
    m_list->blockSignals(false);
    statusBar()->showMessage(tr("%1 von %2 Rezepten angezeigt").arg(m_list->count()).arg(RecipeDb::instance().count()), 4000);
    if (select >= 0) {
        m_list->setCurrentRow(select);
        m_list->scrollToItem(m_list->item(select));
        if (m_view->hasRecipe() && m_view->recipe().id == keep) {
            // gleiches Rezept – Ansicht nur bei Änderung neu laden
            const RecipeSummary *s = RecipeDb::instance().summary(keep);
            if (s && s->modified != m_view->recipe().modified) showCurrent();
        } else {
            showCurrent();
        }
    }
}

QString MainWindow::currentId() const
{
    const QListWidgetItem *it = m_list->currentItem();
    return it ? it->data(Qt::UserRole).toString() : QString();
}

QStringList MainWindow::visibleIds() const
{
    QStringList l;
    for (int i = 0; i < m_list->count(); ++i) l << m_list->item(i)->data(Qt::UserRole).toString();
    return l;
}

void MainWindow::showCurrent()
{
    const QString id = currentId();
    if (id.isEmpty()) return;
    Recipe r;
    QString err;
    if (!RecipeDb::instance().load(id, &r, &err)) {
        m_view->showMessage(tr("Rezept kann nicht geladen werden"), err.toHtmlEscaped());
        return;
    }
    m_view->setRecipe(r);
    updateActions();
}

void MainWindow::selectRecipe(const QString &id)
{
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < m_list->count(); ++i)
            if (m_list->item(i)->data(Qt::UserRole).toString() == id) {
                m_list->setCurrentRow(i);
                m_list->scrollToItem(m_list->item(i), QAbstractItemView::PositionAtCenter);
                showCurrent();
                return;
            }
        // durch Filter verborgen: Filter zurücksetzen
        m_filter->blockSignals(true);
        m_filter->clear();
        m_filter->blockSignals(false);
        m_tagFilter->setCurrentIndex(0);
        refreshList();
    }
}

void MainWindow::openFiles(const QStringList &files)
{
    QStringList vmr, rk, mm, vtm, other;
    for (const QString &f : files) {
        const QFileInfo fi(f);
        const QString suf = fi.suffix().toLower();
        if (fi.isDir() || fi.fileName() == RecipeDb::markerName()) {
            openDatabase(fi.absoluteFilePath());
            continue;
        }
        if (!fi.exists()) {
            QMessageBox::warning(this, tr("Datei öffnen"), tr("Die Datei %1 gibt es nicht.").arg(QDir::toNativeSeparators(f)));
            continue;
        }
        if (suf == RecipePackage::suffix()) {
            importPackage(fi.absoluteFilePath());   // auch ohne geöffnete Datenbank: als neue Datenbank anlegen
            continue;
        }
        if (suf == QLatin1String("vmr")) vmr << fi.absoluteFilePath();
        else if (suf == QLatin1String("rk") || suf == QLatin1String("rkv")) rk << fi.absoluteFilePath();
        else if (suf == QLatin1String("mmf") || suf == QLatin1String("mm") || suf == QLatin1String("mxp")) mm << fi.absoluteFilePath();
        else if (suf == QLatin1String("vtm")) vtm << fi.absoluteFilePath();
        else other << fi.absoluteFilePath();
    }
    if (vmr.isEmpty() && rk.isEmpty() && mm.isEmpty() && vtm.isEmpty() && other.isEmpty()) return;
    if (!RecipeDb::instance().isOpen() && !ensureDatabase()) return;
    // Textdateien unbekannter Endung: Format am Inhalt erkennen
    for (const QString &f : other) {
        const Importers::Result probe = Importers::importFile(f, Importers::Format::Unknown);
        if (probe.recipes.isEmpty()) {
            QMessageBox::warning(this, tr("Datei öffnen"), tr("%1 enthält keine erkannten Rezepte.").arg(QFileInfo(f).fileName()));
            continue;
        }
        (probe.recipes.first().sourceFormat == QLatin1String("MealMaster") ? mm : rk) << f;
    }
    QString first;
    auto take = [&](const QString &id) { if (first.isEmpty()) first = id; };
    if (!rk.isEmpty()) take(ImportFlow::run(this, Importers::Format::RezKonv, rk));
    if (!mm.isEmpty()) take(ImportFlow::run(this, Importers::Format::MealMaster, mm));
    if (!vtm.isEmpty()) take(ImportFlow::run(this, Importers::Format::Vtm, vtm));
    for (const QString &f : vmr) {
        Recipe r;
        QString err;
        if (!Recipe::load(f, &r, &err)) {
            QMessageBox::warning(this, tr("Rezept öffnen"), QStringLiteral("%1: %2").arg(QFileInfo(f).fileName(), err));
            continue;
        }
        RecipeDb &db = RecipeDb::instance();
        if (const RecipeSummary *s = db.summary(r.id)) {
            // dasselbe Rezept liegt schon in der Datenbank
            if (s->modified >= r.modified) {
                take(r.id);
                continue;
            }
        } else if (!db.duplicatesOf(r).isEmpty()) {
            Recipe existing;
            db.load(db.duplicatesOf(r).first(), &existing);
            DuplicateDialog d(r, existing, this);
            d.exec();
            if (d.choice() == DuplicateDialog::Skip) {
                take(existing.id);
                continue;
            }
            if (d.choice() == DuplicateDialog::Replace) {
                r.id = existing.id;
                r.created = existing.created;
            }
        }
        if (r.id.isEmpty()) r.id = Recipe::newId();
        if (!db.save(r, &err)) QMessageBox::warning(this, tr("Rezept öffnen"), err);
        else take(r.id);
    }
    if (!first.isEmpty()) selectRecipe(first);
}

void MainWindow::showRandom()
{
    const QStringList ids = visibleIds();
    if (ids.isEmpty()) {
        const QString id = RecipeDb::instance().randomId();
        if (!id.isEmpty()) selectRecipe(id);
        return;
    }
    QString id = ids.at(QRandomGenerator::global()->bounded(int(ids.size())));
    if (ids.size() > 1 && id == currentId()) id = ids.at((ids.indexOf(id) + 1) % ids.size());
    selectRecipe(id);
}

// ---------------------------------------------------------------- Rezepte

void MainWindow::newRecipe()
{
    Recipe r;
    r.servings = 4;
    EditorDialog d(r, this);
    if (d.exec() != QDialog::Accepted) return;
    saveNewRecipe(d.recipe(), tr("Neues Rezept"));
}

void MainWindow::saveNewRecipe(const Recipe &r, const QString &title)
{
    Recipe nr = r;
    QString err;
    if (!RecipeDb::instance().duplicatesOf(nr).isEmpty()
        && QMessageBox::question(this, title, tr("Ein Rezept „%1“ gibt es bereits. Trotzdem speichern?").arg(nr.title)) != QMessageBox::Yes)
        return;
    m_pendingSelect = nr.id.isEmpty() ? QString() : nr.id;
    if (!RecipeDb::instance().save(nr, &err)) QMessageBox::warning(this, tr("Speichern"), err);
    else selectRecipe(nr.id);
}

void MainWindow::scanRecipe(bool fromFile)
{
    if (!RecipeDb::instance().isOpen()) return;
    ScanDialog scan(fromFile ? ScanDialog::StartOpen : ScanDialog::StartScan, this);
    if (scan.exec() != QDialog::Accepted) return;
    EditorDialog d(scan.recipe(), this);
    d.setReference(scan.pages());
    if (d.exec() != QDialog::Accepted) return;
    saveNewRecipe(d.recipe(), tr("Rezept scannen"));
}

void MainWindow::editRecipe()
{
    const QString id = currentId();
    Recipe r;
    if (id.isEmpty() || !RecipeDb::instance().load(id, &r)) return;
    EditorDialog d(r, this);
    if (d.exec() != QDialog::Accepted) return;
    Recipe nr = d.recipe();
    QString err;
    m_pendingSelect = nr.id;
    if (!RecipeDb::instance().save(nr, &err)) QMessageBox::warning(this, tr("Speichern"), err);
    showCurrent();
}

void MainWindow::deleteRecipe()
{
    const QString id = currentId();
    const RecipeSummary *s = RecipeDb::instance().summary(id);
    if (!s) return;
    if (QMessageBox::question(this, tr("Rezept löschen"), tr("Das Rezept „%1“ endgültig löschen?").arg(s->title)) != QMessageBox::Yes) return;
    const int row = m_list->currentRow();
    RecipeDb::instance().remove(id);
    if (m_list->count() > 0) {
        m_list->setCurrentRow(qMin(row, m_list->count() - 1));
        showCurrent();
    } else {
        m_view->showMessage(tr("Keine Rezepte"), QString());
        updateActions();
    }
}

void MainWindow::search()
{
    SearchDialog d(this);
    if (d.exec() == QDialog::Accepted && !d.selectedId().isEmpty()) selectRecipe(d.selectedId());
}

void MainWindow::importFormat(Importers::Format f)
{
    const QString first = ImportFlow::run(this, f);
    if (!first.isEmpty()) selectRecipe(first);
}

void MainWindow::exportWebsite()
{
    WebsiteExportDialog d(visibleIds(), this);
    d.exec();
}

void MainWindow::exportLatex()
{
    if (!m_view->hasRecipe()) return;
    LatexExportDialog d(m_view->recipe(), this);
    d.exec();
}

void MainWindow::exportOdt()
{
    if (!m_view->hasRecipe()) return;
    const Recipe &r = m_view->recipe();
    QSettings s;
    const QString dir = s.value(QStringLiteral("export/odtDir"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    const QString f = QFileDialog::getSaveFileName(this, tr("Als LibreOffice-Writer-Datei exportieren"), QDir(dir).filePath(OdtExport::defaultFileName(r)),
                                                   tr("OpenDocument-Text (*.odt)"));
    if (f.isEmpty()) return;
    s.setValue(QStringLiteral("export/odtDir"), QFileInfo(f).absolutePath());
    OdtExport::Options o;
    o.servings = m_view->servings();
    QString err;
    if (!OdtExport::write(r, f, o, &err)) {
        QMessageBox::warning(this, tr("Export"), tr("Die Datei konnte nicht geschrieben werden:\n%1").arg(err));
        return;
    }
    QMessageBox box(QMessageBox::Information, tr("Export"), tr("„%1“ wurde gespeichert.").arg(QDir::toNativeSeparators(f)), QMessageBox::Ok, this);
    QPushButton *open = box.addButton(tr("Öffnen"), QMessageBox::ActionRole);
    box.exec();
    if (box.clickedButton() == open) QDesktopServices::openUrl(QUrl::fromLocalFile(f));
}

void MainWindow::clearDatabase()
{
    RecipeDb &db = RecipeDb::instance();
    if (!db.isOpen()) return;
    QDialog d(this);
    d.setWindowTitle(tr("Datenbank leeren"));
    auto *lay = new QVBoxLayout(&d);
    auto *warn = new QLabel(tr("<p><b>Achtung:</b> Alle <b>%1 Rezepte</b> der Datenbank „%2“ werden gelöscht.</p>"
                               "<p>Zur Bestätigung bitte den Namen der Datenbank eingeben:</p>")
                                .arg(db.count())
                                .arg(db.name().toHtmlEscaped()));
    warn->setWordWrap(true);
    lay->addWidget(warn);
    auto *name = new QLineEdit;
    name->setPlaceholderText(db.name());
    lay->addWidget(name);
    auto *backup = new QCheckBox(tr("Vorher Sicherungskopie anlegen (Unterordner „sicherung-…“ im Datenbankordner)"));
    backup->setChecked(true);
    lay->addWidget(backup);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Cancel);
    QPushButton *go = bb->addButton(tr("Alle Rezepte löschen"), QDialogButtonBox::DestructiveRole);
    go->setIcon(Icons::get(QStringLiteral("delete")));
    go->setEnabled(false);
    lay->addWidget(bb);
    connect(name, &QLineEdit::textChanged, &d, [&](const QString &t) { go->setEnabled(t.trimmed() == db.name()); });
    connect(go, &QPushButton::clicked, &d, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    bb->button(QDialogButtonBox::Cancel)->setDefault(true);
    if (d.exec() != QDialog::Accepted) return;
    QString folder, err;
    const int n = db.clear(backup->isChecked(), &folder, &err);
    if (n < 0) {
        QMessageBox::warning(this, tr("Datenbank leeren"), err);
        return;
    }
    QString msg = tr("%1 Rezepte gelöscht.").arg(n);
    if (!folder.isEmpty()) msg += QLatin1Char('\n') + tr("Sicherung: %1 – zum Wiederherstellen die .vmr-Dateien über "
                                                         "Datei → Import → ViviaMeals-Rezept (.vmr) importieren.")
                                                          .arg(QDir::toNativeSeparators(folder));
    QMessageBox::information(this, tr("Datenbank leeren"), msg);
    showCurrent();
}

void MainWindow::mealTagsAll()
{
    RecipeDb &db = RecipeDb::instance();
    QList<Recipe> todo;
    QHash<QString, int> added, removed;
    for (const RecipeSummary &s : db.summaries()) {
        Recipe r;
        if (!db.load(s.id, &r) || Nutrition::isMenu(r)) continue;
        const QStringList before = r.tags;
        MealTypes::applyTags(r);
        MealTypes::applyNutrientTags(r);
        if (r.tags == before) continue;
        for (const QString &t : r.tags)
            if (!before.contains(t)) added[t]++;
        for (const QString &t : before)
            if (!r.tags.contains(t)) removed[t]++;
        todo << r;
    }
    if (todo.isEmpty()) {
        QMessageBox::information(this, tr("Tags ergänzen"), tr("Alle Rezepte haben bereits passende Mahlzeit- und Nährwert-Tags."));
        return;
    }
    QStringList lines;
    for (const QString &t : MealTypes::mealTags() + MealTypes::nutrientTagNames()) {
        if (added.value(t)) lines << tr("  + %1: %2").arg(RecipeHtml::tagLabel(t)).arg(added.value(t));
        if (removed.value(t)) lines << tr("  − %1: %2 (trifft nach den Nährwerten nicht mehr zu)").arg(RecipeHtml::tagLabel(t)).arg(removed.value(t));
    }
    if (QMessageBox::question(this, tr("Tags ergänzen"),
                              tr("Für %1 Rezepte ändern sich die Tags:\n\n%2\n\nAndere Tags bleiben erhalten. Fortfahren?")
                                  .arg(todo.size())
                                  .arg(lines.join(QLatin1Char('\n'))))
        != QMessageBox::Yes)
        return;
    db.beginBatch();
    for (Recipe &r : todo) db.save(r);
    db.endBatch();
    statusBar()->showMessage(tr("Tags für %1 Rezepte aktualisiert.").arg(todo.size()), 5000);
    showCurrent();
}

void MainWindow::exportVmr()
{
    if (!m_view->hasRecipe()) return;
    const Recipe &r = m_view->recipe();
    QString base = r.title;
    base.replace(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]+")), QStringLiteral("_"));
    const QString f = QFileDialog::getSaveFileName(this, tr("Rezept speichern"),
                                                   QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(base + QStringLiteral(".vmr")),
                                                   tr("ViviaMeals-Rezept (*.vmr)"));
    if (f.isEmpty()) return;
    QString err;
    if (!r.save(f, &err)) QMessageBox::warning(this, tr("Speichern"), err);
}

void MainWindow::printRecipe()
{
    if (!m_view->hasRecipe()) return;
    QPrinter printer(QPrinter::HighResolution);
    QPrintPreviewDialog pd(&printer, this);
    connect(&pd, &QPrintPreviewDialog::paintRequested, this, [this](QPrinter *p) {
        QTextDocument doc;
        m_view->fillDocument(&doc);
        doc.print(p);
    });
    pd.exec();
}

void MainWindow::findDuplicates()
{
    if (!RecipeDb::instance().isOpen()) return;
    DuplicatesDialog d(this);
    d.exec();
    if (d.changes() > 0) {
        statusBar()->showMessage(tr("Doubletten bereinigt."), 5000);
        if (!RecipeDb::instance().summary(currentId())) showRandom();
        else showCurrent();
    }
}

void MainWindow::recalcAll()
{
    RecipeDb &db = RecipeDb::instance();
    QStringList ids;
    for (const RecipeSummary &s : db.summaries()) ids << s.id;
    QProgressDialog prog(tr("Nährwerte werden neu berechnet …"), tr("Abbrechen"), 0, int(ids.size()), this);
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(300);
    db.beginBatch();
    int n = 0;
    for (const QString &id : ids) {
        prog.setValue(n++);
        if (prog.wasCanceled()) break;
        Recipe r;
        if (!db.load(id, &r)) continue;
        Nutrition::analyze(r);
        MealTypes::applyNutrientTags(r);   // abgeleitete Tags passend zu den neuen Werten
        db.save(r);
    }
    db.endBatch();
    prog.setValue(int(ids.size()));
    statusBar()->showMessage(tr("Nährwerte von %1 Rezepten neu berechnet.").arg(n), 5000);
    showCurrent();
}

void MainWindow::fixUmlautsAll()
{
    RecipeDb &db = RecipeDb::instance();
    // erst zählen und eine Vorschau zeigen
    QList<Recipe> todo;
    QStringList examples;
    for (const RecipeSummary &s : db.summaries()) {
        Recipe r;
        if (!db.load(s.id, &r) || r.language != QLatin1String("de")) continue;
        if (!Umlauts::looksTranscribed(Umlauts::allRecipeText(r))) continue;
        Recipe fixed = r;
        if (Umlauts::fixRecipe(fixed) == 0) continue;
        if (examples.size() < 6) examples << QStringLiteral("„%1“ → „%2“").arg(r.title, fixed.title);
        todo << fixed;
    }
    if (todo.isEmpty()) {
        QMessageBox::information(this, tr("Umlaute korrigieren"), tr("Es wurden keine Rezepte ohne Umlaute gefunden."));
        return;
    }
    if (QMessageBox::question(this, tr("Umlaute korrigieren"),
                              tr("%1 Rezepte schreiben ae, oe, ue und ss statt ä, ö, ü und ß, zum Beispiel:\n\n%2\n\nJetzt korrigieren?")
                                  .arg(todo.size())
                                  .arg(examples.join(QLatin1Char('\n'))))
        != QMessageBox::Yes)
        return;
    db.beginBatch();
    for (Recipe &r : todo) {
        Nutrition::analyze(r);   // „Kaese“ → „Käse“ findet jetzt den Eintrag in der Nährwerttabelle
        Nutrition::applyTagSuggestions(r);
        db.save(r);
    }
    db.endBatch();
    statusBar()->showMessage(tr("Umlaute in %1 Rezepten korrigiert.").arg(todo.size()), 5000);
    showCurrent();
}

void MainWindow::translateAll()
{
    const DeepL::Settings ds = DeepL::Settings::load();
    if (!ds.usable()) {
        if (QMessageBox::question(this, tr("Mit DeepL übersetzen"),
                                  tr("Dafür wird ein eigener DeepL-API-Schlüssel benötigt (kostenlos: „DeepL API Free“). "
                                     "Jetzt die Einstellungen öffnen?"))
            == QMessageBox::Yes)
            openSettings();
        return;
    }
    RecipeDb &db = RecipeDb::instance();
    QStringList ids;
    int chars = 0;
    for (const RecipeSummary &s : db.summaries()) {
        if (s.language == QLatin1String("de")) continue;
        Recipe r;
        if (!db.load(s.id, &r)) continue;
        ids << s.id;
        chars += DeepL::characterCount(r);
    }
    if (ids.isEmpty()) {
        QMessageBox::information(this, tr("Mit DeepL übersetzen"), tr("In der Datenbank gibt es keine fremdsprachigen Rezepte."));
        return;
    }
    if (QMessageBox::question(this, tr("Mit DeepL übersetzen"),
                              tr("%1 fremdsprachige Rezepte gefunden. Titel, Zutaten und Zubereitung (etwa %2 Zeichen) werden an DeepL "
                                 "gesendet und durch die deutsche Übersetzung ersetzt. Fortfahren?")
                                  .arg(ids.size())
                                  .arg(QLocale(QLocale::German).toString(chars)))
        != QMessageBox::Yes)
        return;
    QProgressDialog prog(tr("Rezepte werden übersetzt …"), tr("Abbrechen"), 0, int(ids.size()), this);
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(0);
    const auto translator = ImportFlow::deeplTranslator(&prog);
    int done = 0, failed = 0;
    QString lastError;
    db.beginBatch();
    for (int i = 0; i < ids.size(); ++i) {
        prog.setValue(i);
        if (prog.wasCanceled()) break;
        Recipe r;
        if (!db.load(ids[i], &r)) continue;
        if (r.originalLanguage.isEmpty()) r.originalLanguage = r.language;
        QString err;
        if (!translator(r, &err)) {
            ++failed;
            lastError = err;
            continue;
        }
        Nutrition::analyze(r);
        Nutrition::applyTagSuggestions(r);
        db.save(r);
        ++done;
    }
    db.endBatch();
    prog.setValue(int(ids.size()));
    if (failed > 0)
        QMessageBox::warning(this, tr("Mit DeepL übersetzen"),
                             tr("%1 Rezepte übersetzt, %2 nicht übersetzt.\nLetzter Fehler: %3").arg(done).arg(failed).arg(lastError));
    else
        statusBar()->showMessage(tr("%1 Rezepte mit DeepL übersetzt.").arg(done), 5000);
    showCurrent();
}

// ---------------------------------------------------------------- Planung und Extras

void MainWindow::openFridge()
{
    FridgeDialog d(this);
    connect(&d, &FridgeDialog::showRecipe, this, &MainWindow::selectRecipe);
    d.exec();
}

void MainWindow::openAssistant()
{
    AssistantDialog d(this);
    connect(&d, &AssistantDialog::planRequested, this, [this](const QString &id) { openMealPlan(id); });
    d.exec();
    updateReminder();
}

void MainWindow::openDiary(const QString &recipeId)
{
    if (Profiles::instance().all().isEmpty()) {
        if (QMessageBox::question(this, tr("Ernährungstagebuch"),
                                  tr("Das Ernährungstagebuch wird je Person geführt. Es ist noch keine Person angelegt – jetzt den "
                                     "Assistenten Gewichtskontrolle öffnen und eine Person anlegen?"))
            == QMessageBox::Yes)
            openAssistant();
        return;
    }
    DiaryDialog d(this);
    if (!recipeId.isEmpty()) d.addRecipe(recipeId);
    d.exec();
    refreshList();   // eigene Rezepte können in die Datenbank übernommen worden sein
}

void MainWindow::openWeighIn()
{
    if (!WeighInDialog::hasProfiles()) {
        if (QMessageBox::question(this, tr("Gewichtskontrolle"),
                                  tr("Es ist noch keine Person angelegt. Jetzt den Assistenten Gewichtskontrolle öffnen und eine Person anlegen?"))
            == QMessageBox::Yes)
            openAssistant();
        return;
    }
    WeighInDialog d(QString(), this);
    if (d.exec() == QDialog::Accepted) {
        if (const Profile *p = Profiles::instance().find(d.profileId()))
            statusBar()->showMessage(tr("Gewicht für %1 in den Gewichtsverlauf eingetragen.").arg(p->name), 6000);
    }
    updateReminder();
}

void MainWindow::updateReminder()
{
    QStringList who;
    QString day;
    for (const Profile &p : Profiles::instance().all())
        if (p.weighReminderDue()) {
            who << p.name.toHtmlEscaped();
            const QDate d = p.lastWeighDay();
            day = d == QDate::currentDate() ? tr("Heute (%1) ist Wiegetag").arg(Profile::dayName(d.dayOfWeek()))
                                           : tr("Der Wiegetag am %1, %2 steht noch aus").arg(Profile::dayName(d.dayOfWeek()), d.toString(QStringLiteral("dd.MM.")));
        }
    if (who.isEmpty()) {
        m_reminder->hide();
        return;
    }
    m_reminderText->setText(tr("<b>%1</b> für %2 – am besten morgens vor dem Frühstück wiegen und das Gewicht eintragen (Planen → Gewichtskontrolle, F8).")
                                .arg(day, who.join(QStringLiteral(", "))));
    m_reminder->show();
}

void MainWindow::openMealPlan(const QString &profileId)
{
    if (!RecipeDb::instance().isOpen()) return;
    MealPlanDialog d(profileId, this);
    connect(&d, &MealPlanDialog::showRecipe, this, &MainWindow::selectRecipe);
    d.exec();
}

void MainWindow::exportPackage()
{
    if (!RecipeDb::instance().isOpen()) return;
    PackageExportDialog d(visibleIds(), this);
    d.exec();
}

void MainWindow::importPackage(const QString &given)
{
    QSettings s;
    QString file = given;
    if (file.isEmpty()) {
        file = QFileDialog::getOpenFileName(this, tr("Rezeptpaket übernehmen"),
                                            s.value(QStringLiteral("package/dir"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString(),
                                            tr("ViviaMeals-Rezeptpaket (*.vmpack);;Alle Dateien (*)"));
        if (file.isEmpty()) return;
        s.setValue(QStringLiteral("package/dir"), QFileInfo(file).absolutePath());
    }
    RecipePackage::Content c;
    QString err;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = RecipePackage::read(file, &c, &err);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, tr("Rezeptpaket"), tr("„%1“ kann nicht gelesen werden:\n%2").arg(QFileInfo(file).fileName(), err));
        return;
    }
    PackageImportDialog d(c, file, RecipeDb::instance().isOpen(), this);
    if (d.exec() != QDialog::Accepted) return;
    if (d.target() == PackageImportDialog::NewDatabase && !createDatabase(c.name.isEmpty() ? QFileInfo(file).completeBaseName() : c.name)) return;
    int foods = 0, learned = 0;
    if (d.importFoods()) foods = RecipePackage::importFoods(c, &learned);
    // Nährwerte mit der eigenen Tabelle neu berechnen; manuelle Zuordnungen bleiben erhalten
    RecipeDb &db = RecipeDb::instance();
    for (Recipe &r : c.recipes) {
        Nutrition::analyze(r);
        if (QSettings().value(QStringLiteral("import/nutrientTags"), true).toBool()) MealTypes::applyNutrientTags(r);
        // gleiche ID, aber anderes Rezept (z. B. umbenannt): eigene ID vergeben statt überschreiben
        if (db.summary(r.id) && db.duplicatesOf(r).isEmpty()) r.id.clear();
    }
    QString title = tr("Rezeptpaket übernommen");
    if (foods > 0 || learned > 0) title = tr("Rezeptpaket übernommen (%1 Lebensmittel, %2 Zuordnungen)").arg(foods).arg(learned);
    const QString first = ImportFlow::store(this, c.recipes, c.warnings, title);
    if (!first.isEmpty()) selectRecipe(first);
}

void MainWindow::newFood()
{
    FoodWizard w(QString(), this);
    if (w.exec() != QDialog::Accepted || w.foodKey().isEmpty()) return;
    if (RecipeDb::instance().isOpen()
        && QMessageBox::question(this, tr("Nährwerttabelle"), tr("Das Lebensmittel wurde angelegt. Nährwerte aller Rezepte jetzt neu berechnen, "
                                                                 "damit es überall berücksichtigt wird?"))
               == QMessageBox::Yes)
        recalcAll();
}

void MainWindow::missingFoods()
{
    RecipeDb &db = RecipeDb::instance();
    if (!db.isOpen()) return;
    QProgressDialog prog(tr("Zutaten werden geprüft …"), tr("Abbrechen"), 0, db.count(), this);
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(400);
    QList<Recipe> list;
    int i = 0;
    for (const RecipeSummary &s : db.summaries()) {
        prog.setValue(i++);
        if (prog.wasCanceled()) return;
        if (s.coverage >= 0.999) continue;
        Recipe r;
        if (db.load(s.id, &r)) list << r;
    }
    prog.setValue(db.count());
    const auto missing = Nutrition::missingFoods(list, MissingFoodsDialog::ignoredNames());
    if (missing.isEmpty()) {
        QMessageBox::information(this, tr("Zutaten ohne Nährwerte"), tr("Alle Zutaten mit Mengenangabe haben einen Eintrag in der Nährwerttabelle."));
        return;
    }
    MissingFoodsDialog d(missing, (missing.size() == 1 ? tr("Eine Zutat der Datenbank hat noch keine Nährwerte:") : tr("%1 Zutaten der Datenbank haben noch keine Nährwerte (häufigste zuerst):").arg(missing.size())), this);
    d.exec();
    if (d.changed()
        && QMessageBox::question(this, tr("Nährwerttabelle"), tr("Die Nährwerttabelle wurde geändert. Nährwerte aller Rezepte jetzt neu berechnen?"))
               == QMessageBox::Yes)
        recalcAll();
}

void MainWindow::openFoodTable()
{
    FoodTableDialog d(this);
    d.exec();
    if (d.changed() && RecipeDb::instance().isOpen()
        && QMessageBox::question(this, tr("Nährwerttabelle"), tr("Die Nährwerttabelle wurde geändert. Nährwerte aller Rezepte jetzt neu berechnen?"))
               == QMessageBox::Yes)
        recalcAll();
}

void MainWindow::openSettings()
{
    SettingsDialog d(this);
    connect(&d, &SettingsDialog::appearanceChanged, this, &MainWindow::appearanceChanged);
    d.exec();
}

void MainWindow::about()
{
    QMessageBox box(this);
    box.setWindowTitle(tr("Über ViviaMeals"));
    box.setIconPixmap(Icons::app().pixmap(72, 72));
    box.setText(tr("<h2>ViviaMeals %1</h2><p>Rezeptverwaltung mit Nährwertanalyse, Kühlschrank-Vorschlägen, Wochenplänen "
                   "und Assistenten für die Gewichtskontrolle.</p>")
                    .arg(QApplication::applicationVersion()));
    box.setInformativeText(tr("<p><b>ViviaMeals ist kein Medizinprodukt.</b> Nährwerte und Energiebedarf sind Schätzwerte "
                              "und ersetzen keine ärztliche oder Ernährungsberatung.</p>"
                              "<p>Importiert RezKonv und MealMaster, exportiert Websites (HTML5/PHP), LaTeX (book, scrbook) "
                              "und LibreOffice Writer (.odt). Rezepte scannen mit Texterkennung.<br/>"
                              "Optik und Bausteine aus ViviaParms und ViviaBreathe (bis 2026: „Vita“-Reihe).</p>"
                              "<p>© 2026 Michael Bergmann<br/>"
                              "Quelloffen unter der <b>PolyForm Noncommercial License 1.0.0</b>: kostenlos für jede "
                              "<b>nicht-kommerzielle</b> Nutzung – privat sowie durch gemeinnützige Organisationen, Schulen und "
                              "Behörden –, einschließlich Verändern und Weitergeben. <b>Kommerzielle Nutzung und Verkauf</b> nur mit "
                              "Erlaubnis des Autors.<br/>"
                              "<a href=\"https://polyformproject.org/licenses/noncommercial/1.0.0\">polyformproject.org/licenses/noncommercial/1.0.0</a>"
                              "<br/>Verwendet Qt (LGPL-3.0); die Texterkennung nutzt das eigenständige Programm "
                              "<a href=\"https://github.com/tesseract-ocr/tesseract\">Tesseract</a> (Apache-2.0).</p>"));
    box.setTextFormat(Qt::RichText);
    box.setTextInteractionFlags(Qt::TextBrowserInteraction);
    QFile lic(QStringLiteral(":/doc/LICENSE"));
    if (lic.open(QIODevice::ReadOnly)) box.setDetailedText(QString::fromUtf8(lic.readAll()));   // „Details einblenden …“ zeigt den Lizenztext
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

void MainWindow::appearanceChanged()
{
    Icons::refreshWidgets();
    refreshTags();
    refreshList();
    m_view->refresh();
    // Häkchen im Menü Darstellung nachführen
    for (QAction *a : findChildren<QAction *>())
        if (a->isCheckable() && a->actionGroup()) a->setChecked(a->text() == Themes::displayName(Themes::current()));
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    QSettings s;
    if (!isFullScreen() && !isMinimized()) s.setValue(QStringLiteral("ui/geometry"), saveGeometry());
    s.setValue(QStringLiteral("ui/state"), saveState());
    s.setValue(QStringLiteral("ui/split"), m_split->saveState());
    e->accept();
}
