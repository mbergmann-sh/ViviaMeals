// VitaMeals – Programmstart
//
//   VitaMeals                       zuletzt geöffnete Rezeptdatenbank öffnen (sonst zum Anlegen auffordern)
//   VitaMeals --database <ordner>   bestimmte Datenbank öffnen
//   VitaMeals --demo                Demo mit Beispielrezepten, Bildern und einem Profil in einem temporären
//                                   Ordner, mit eigenen Einstellungen – berührt die eigenen Daten nicht
//   VitaMeals --demo --screenshots <ordner>   Bildschirmfotos für Handbuch/README erzeugen
// SPDX-License-Identifier: MIT
#include "assistantdialog.h"
#include "editordialog.h"
#include "fridge.h"
#include "fridgedialog.h"
#include "icons.h"
#include "importdialogs.h"
#include "importers.h"
#include "mainwindow.h"
#include "mealplandialog.h"
#include "profiles.h"
#include "qtcompat.h"
#include "recipedb.h"
#include "recipeview.h"
#include "screenfit.h"
#include "searchdialog.h"
#include "themes.h"
#include "uiscale.h"
#include "websiteexport.h"
#include "windowstate.h"

#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QCommandLineParser>
#include <QDir>
#include <QEventLoop>
#include <QLinearGradient>
#include <QLibraryInfo>
#include <QListWidget>
#include <QLocale>
#include <QTranslator>
#include <QMessageBox>
#include <QPainter>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

// Beispielbild für Demo-Rezepte: Teller von oben, Farben nach Thema
QString demoImage(const Recipe &r)
{
    const uint h = qHash(r.title);
    QImage img(640, 480, QImage::Format_RGB32);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient bg(0, 0, 640, 480);
    const int hue = int(h % 40) + 20;   // warme Holztöne
    bg.setColorAt(0, QColor::fromHsv(hue, 90, 200));
    bg.setColorAt(1, QColor::fromHsv(hue, 120, 140));
    p.fillRect(img.rect(), bg);
    // Tischtuch-Streifen
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 28));
    for (int x = -480; x < 640; x += 60) p.drawPolygon(QPolygon({ QPoint(x, 480), QPoint(x + 30, 480), QPoint(x + 510, 0), QPoint(x + 480, 0) }));
    // Teller
    p.setBrush(QColor(0, 0, 0, 50));
    p.drawEllipse(QPointF(330, 252), 190, 190);
    p.setBrush(QColor(250, 250, 248));
    p.drawEllipse(QPointF(320, 240), 190, 190);
    p.setBrush(QColor(236, 238, 240));
    p.drawEllipse(QPointF(320, 240), 140, 140);
    // Essen
    QColor food(200, 120, 50);
    if (r.hasTag(QStringLiteral("suppen")) || r.hasTag(QStringLiteral("eintöpfe"))) food = QColor(170, 110, 50);
    if (r.hasTag(QStringLiteral("vegan")) || r.hasTag(QStringLiteral("indisch"))) food = QColor(225, 160, 40);
    if (r.hasTag(QStringLiteral("backen"))) food = QColor(205, 150, 80);
    if (r.hasTag(QStringLiteral("desserts"))) food = QColor(190, 50, 70);
    if (r.hasTag(QStringLiteral("frühstück"))) food = QColor(230, 210, 170);
    auto *rng = QRandomGenerator::global();
    p.setBrush(food);
    p.drawEllipse(QPointF(320, 240), 110, 104);
    p.setBrush(food.lighter(125));
    for (int i = 0; i < 14; ++i) {
        const double a = rng->bounded(360) * kPi / 180.0, d = rng->bounded(80);
        p.drawEllipse(QPointF(320 + std::cos(a) * d, 240 + std::sin(a) * d), 12 + rng->bounded(14), 10 + rng->bounded(12));
    }
    // Garnitur
    const QColor garnish[] = { QColor(70, 150, 60), QColor(200, 40, 40), QColor(250, 230, 120) };
    for (int i = 0; i < 18; ++i) {
        p.setBrush(garnish[rng->bounded(3)]);
        const double a = rng->bounded(360) * kPi / 180.0, d = rng->bounded(95);
        p.drawEllipse(QPointF(320 + std::cos(a) * d, 240 + std::sin(a) * d), 5, 4);
    }
    // Besteck
    p.setBrush(QColor(210, 214, 220));
    p.drawRoundedRect(QRectF(560, 70, 16, 340), 8, 8);
    p.drawRoundedRect(QRectF(64, 70, 16, 340), 8, 8);
    p.end();
    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPG", 82);
    return QString::fromLatin1(ba.toBase64());
}

void createDemo(const QString &folder)
{
    QDir(folder).removeRecursively();
    RecipeDb &db = RecipeDb::instance();
    db.create(folder, QObject::tr("Demo-Rezepte"));
    db.beginBatch();
    for (const QString &f : { QStringLiteral(":/samples/beispiele.rk"), QStringLiteral(":/samples/beispiele.mmf") }) {
        const Importers::Result res = Importers::importFile(f, Importers::Format::Unknown);
        for (Recipe r : res.recipes) {
            r.image = demoImage(r);
            if (r.title.contains(QStringLiteral("Curry")) || r.title.contains(QStringLiteral("Apfelkuchen"))) r.favorite = true;
            db.save(r);
        }
    }
    db.endBatch();
    // Demo-Profil mit Gewichtsverlauf
    Profiles &pr = Profiles::instance();
    pr.all().clear();
    Profile p;
    p.name = QObject::tr("Erika Mustermann");
    p.female = true;
    p.birthYear = 1972;
    p.heightCm = 168;
    p.pal = 1.6;
    p.goal = Profile::Goal::Lose;
    p.targetKg = 68;
    p.ratePerWeek = 0.5;
    p.diet = Profile::Diet::All;
    p.likedTags = { QStringLiteral("suppen"), QStringLiteral("vegetarisch") };
    p.excluded = { QStringLiteral("Walnüsse") };
    double kg = 79.4;
    for (int d = 84; d >= 0; d -= 7) {
        p.weights << WeightEntry{ QDate::currentDate().addDays(-d), std::round(kg * 10) / 10.0, {} };
        kg -= 0.35 + QRandomGenerator::global()->bounded(30) / 100.0;
    }
    pr.add(p);
    Fridge::setStock({ QObject::tr("Eier"), QObject::tr("Milch"), QObject::tr("Kartoffeln"), QObject::tr("Zwiebeln"), QObject::tr("Möhren"),
                       QObject::tr("Linsen"), QObject::tr("Butter"), QObject::tr("Mehl"), QObject::tr("Äpfel"),
                       QObject::tr("Würstchen"), QObject::tr("Haferflocken"), QObject::tr("Joghurt"), QObject::tr("Quark") });
}

void wait(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

void shot(QWidget *w, const QString &dir, const QString &name)
{
    QApplication::processEvents();
    wait(150);
    w->grab().save(dir + QLatin1Char('/') + name + QStringLiteral(".png"));
}

void takeScreenshots(MainWindow *w, const QString &dir)
{
    QDir().mkpath(dir);
    w->resize(1280, 800);
    // Hauptfenster mit einem Rezept
    for (const RecipeSummary &s : RecipeDb::instance().summaries())
        if (s.title.contains(QStringLiteral("Curry"))) w->selectRecipe(s.id);
    shot(w, dir, QStringLiteral("hauptfenster"));
    for (const RecipeSummary &s : RecipeDb::instance().summaries())
        if (s.title.contains(QStringLiteral("Cookies"))) w->selectRecipe(s.id);
    shot(w, dir, QStringLiteral("hauptfenster-englisch"));
    // Editor
    {
        EditorDialog d(w->view()->recipe(), w);
        d.resize(1180, 760);
        d.show();
        shot(&d, dir, QStringLiteral("editor"));
        d.reject();
    }
    {
        SearchDialog d(w);
        d.show();
        shot(&d, dir, QStringLiteral("suche"));
        d.reject();
    }
    {
        FridgeDialog d(w);
        d.show();
        shot(&d, dir, QStringLiteral("kuehlschrank"));
        d.reject();
    }
    {
        AssistantDialog d(w);
        d.show();
        shot(&d, dir, QStringLiteral("assistent"));
        d.reject();
    }
    {
        MealPlanDialog d(Profiles::instance().all().value(0).id, w);
        d.show();
        shot(&d, dir, QStringLiteral("wochenplan"));
        d.reject();
    }
    {
        ImportPreviewDialog d(Importers::importFile(QStringLiteral(":/samples/beispiele.mmf"), Importers::Format::Unknown).recipes,
                              { QStringLiteral("beispiele.mmf"), QStringLiteral("beispiele.mmf"), QStringLiteral("beispiele.mmf") }, {}, w);
        d.show();
        shot(&d, dir, QStringLiteral("import-vorschau"));
        d.reject();
    }
    // Dark-Theme
    Themes::apply(Themes::vscodeId());
    w->appearanceChanged();
    shot(w, dir, QStringLiteral("hauptfenster-dark"));
    Themes::apply(Themes::systemId());
    w->appearanceChanged();
    // kleiner Bildschirm
    w->resize(1024, 640);
    shot(w, dir, QStringLiteral("hauptfenster-klein"));
    // Website
    WebsiteExport::Options wo;
    wo.title = QObject::tr("Demo-Rezepte");
    wo.subtitle = QObject::tr("Rezepte mit Nährwerten");
    WebsiteExport::exportSite(dir + QStringLiteral("/website"), wo);
}

} // namespace

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("mbergmann-sh"));
    QApplication::setApplicationName(QStringLiteral("VitaMeals"));
    QApplication::setApplicationVersion(QStringLiteral(VITAMEALS_VERSION));
    QApplication::setWindowIcon(Icons::app());

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("VitaMeals – Rezeptverwaltung mit Nährwertanalyse und Gewichtskontrolle"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption demo(QStringLiteral("demo"), QStringLiteral("Demo mit Beispielrezepten in einem temporären Ordner"));
    QCommandLineOption shots(QStringLiteral("screenshots"), QStringLiteral("Bildschirmfotos erzeugen (mit --demo)"), QStringLiteral("ordner"));
    QCommandLineOption dbOpt(QStringLiteral("database"), QStringLiteral("Rezeptdatenbank (Ordner)"), QStringLiteral("ordner"));
    parser.addOptions({ demo, shots, dbOpt });
    parser.process(app);
    const bool isDemo = parser.isSet(demo);
    if (isDemo) QApplication::setApplicationName(QStringLiteral("VitaMeals-Demo"));   // eigene Einstellungen und Profile

    // Programm ist deutschsprachig: Zahlen/Datumsformate und Qt-eigene Texte (Schaltflächen, Dialoge) auf Deutsch
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
    auto *qtTr = new QTranslator(&app);
    // Linux: Qt-Übersetzungen des Systems; Windows/macOS: von windeployqt/macdeployqt neben das Programm kopiert
    const QString appTr = QApplication::applicationDirPath() + QStringLiteral("/translations");
    if (qtTr->load(QLocale(QLocale::German), QStringLiteral("qtbase"), QStringLiteral("_"), vpLibraryPath(VpLibraryPath::TranslationsPath))
        || qtTr->load(QStringLiteral("qt_de"), appTr) || qtTr->load(QStringLiteral("qtbase_de"), appTr))
        QApplication::installTranslator(qtTr);

    UiScale::apply(app);
    Themes::init();
    ScreenFit::install(app);
    WindowState::install(app);   // nach ScreenFit: erst wiederherstellen, dann an den Bildschirm anpassen

    MainWindow w;
    if (isDemo) {
        const QString folder = QDir::temp().filePath(QStringLiteral("vitameals-demo"));
        createDemo(folder);
        w.openDatabase(folder);
    }
    w.show();

    if (isDemo && parser.isSet(shots)) {
        QTimer::singleShot(300, &w, [&w, &parser, &shots] {
            takeScreenshots(&w, parser.value(shots));
            QApplication::quit();
        });
        return app.exec();
    }
    if (!isDemo) {
        QTimer::singleShot(0, &w, [&w, &parser, &dbOpt] {
            const QString db = parser.value(dbOpt);
            if (!db.isEmpty() && w.openDatabase(db)) return;
            w.ensureDatabase();
        });
    }
    if (QSettings().value(QStringLiteral("ui/startNotice"), true).toBool()) {
        QTimer::singleShot(600, &w, [&w] {
            QMessageBox box(&w);
            box.setWindowTitle(QObject::tr("Hinweis"));
            box.setIcon(QMessageBox::Information);
            box.setText(QObject::tr("<b>VitaMeals ist kein Medizinprodukt.</b><br/>Nährwerte, Energiebedarf und Wochenpläne sind "
                                    "Schätzwerte für die persönliche Planung und ersetzen keine ärztliche oder Ernährungsberatung."));
            auto *dont = new QCheckBox(QObject::tr("Nicht mehr anzeigen"));
            box.setCheckBox(dont);
            box.exec();
            if (dont->isChecked()) QSettings().setValue(QStringLiteral("ui/startNotice"), false);
        });
    }
    return app.exec();
}
