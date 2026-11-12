// ViviaMeals – Programmstart
//
//   ViviaMeals                       zuletzt geöffnete Rezeptdatenbank öffnen (sonst zum Anlegen auffordern)
//   ViviaMeals --database <ordner>   bestimmte Datenbank öffnen
//   ViviaMeals --demo                Demo mit Beispielrezepten, Bildern und einem Profil in einem temporären
//                                   Ordner, mit eigenen Einstellungen – berührt die eigenen Daten nicht
//   ViviaMeals --demo --screenshots <ordner>   Bildschirmfotos für Handbuch/README erzeugen
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "assistantdialog.h"
#include "editordialog.h"
#include "foodcsv.h"
#include "fooddialog.h"
#include "foodwizard.h"
#include "busyprogress.h"
#include "exportdialogs.h"
#include "packagedialog.h"
#include "fridge.h"
#include "fridgedialog.h"
#include "icons.h"
#include "importdialogs.h"
#include "importers.h"
#include "mainwindow.h"
#include "scandialog.h"
#include "duplicatesdialog.h"
#include "settingsdialog.h"
#include "imagecropdialog.h"
#include "weighindialog.h"
#include "diarydialog.h"
#include "diary.h"
#include "nutrition.h"
#include "ingredientparser.h"
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QListWidget>
#include <QTabWidget>
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
#include <QDirIterator>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QEventLoop>
#include <QLinearGradient>
#include <QLibraryInfo>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QTranslator>
#include <QMessageBox>
#include <QPainter>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QTemporaryDir>
#include "sync.h"
#include "syncdialog.h"
#include "menudialog.h"
#include "menu.h"
#include "syncserver.h"
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
    for (const QString &f : { QStringLiteral(":/samples/beispiele.rk"), QStringLiteral(":/samples/beispiele.mmf"), QStringLiteral(":/samples/umlaute.rk"),
                            QStringLiteral(":/samples/gesund.rk"), QStringLiteral(":/samples/festlich.rk") }) {
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
    p.snack = true;
    p.weighDay = QDate::currentDate().dayOfWeek();   // heute ist Wiegetag → Erinnerung im Hauptfenster
    for (int d = 91; d >= 7; d -= 7) {
        p.weights << WeightEntry{ QDate::currentDate().addDays(-d), std::round(kg * 10) / 10.0, {} };
        kg -= 0.35 + QRandomGenerator::global()->bounded(30) / 100.0;
    }
    const QString pid = pr.add(p);
    // Ernährungstagebuch: heute Frühstück, Zwischenmahlzeit, Mittagessen; Vortage mit Tagessummen
    {
        Diary::Book book;
        book.load(pid);
        auto byTitle = [&](const QString &t) -> Recipe {
            for (const RecipeSummary &s : db.summaries())
                if (s.title.startsWith(t)) {
                    Recipe r;
                    db.load(s.id, &r);
                    return r;
                }
            return Recipe();
        };
        struct D { int day; const char *title; int h, m; double portions; double grams; };
        const D items[] = {
            { 0, "Haferflocken-Porridge", 7, 20, 1, 0 },        { 0, "Apfel mit Mandeln", 10, 15, 1, 0 },
            { 0, "Gemüse-Curry", 12, 45, 0.75, 0 },             { 1, "Vollkornbrot mit Kräuterquark", 7, 30, 1, 0 },
            { 1, "Linsensuppe", 12, 30, 0, 450 },               { 1, "Magerquark mit Beeren", 15, 30, 1, 0 },
            { 1, "Lauch-Käse-Suppe", 18, 45, 1.25, 0 },         { 2, "Rührei mit Spinat", 8, 0, 1, 0 },
            { 2, "Käsespätzle", 13, 0, 1, 0 },                  { 2, "Gemüsesticks", 16, 0, 1, 0 },
            { 2, "Chicken Vegetable Soup", 19, 0, 1.5, 0 },     { 3, "Bircher-Müsli", 7, 45, 1, 0 },
            { 3, "Gemüse-Curry", 12, 30, 1, 0 },                { 3, "Knäckebrot", 18, 30, 2, 0 },
        };
        for (const D &it : items) {
            const Recipe r = byTitle(QString::fromUtf8(it.title));
            if (r.title.isEmpty()) continue;
            Diary::Entry e = Diary::Entry::fromRecipe(r, false);
            e.when = QDateTime(QDate::currentDate().addDays(-it.day), QTime(it.h, it.m));
            e.meal = Diary::mealForTime(e.when.time());
            if (it.grams > 0) e.setGrams(it.grams);
            else e.setPortions(it.portions);
            book.add(e);
        }
        // eigenes Rezept (im Editor erfasst)
        Recipe own;
        own.id = Recipe::newId();
        own.title = QObject::tr("Belegtes Vollkornbrötchen");
        own.servings = 1;
        own.ingredients = IngredientParser::parseText(QObject::tr("1 Vollkornbrötchen\n20 g Frischkäse\n30 g Putenbrust\n50 g Tomate"));
        Nutrition::analyze(own);
        Diary::Entry o = Diary::Entry::fromRecipe(own, true);
        o.when = QDateTime(QDate::currentDate().addDays(-3), QTime(10, 0));
        o.meal = Diary::mealForTime(o.when.time());
        o.note = QObject::tr("Bäckerei");
        book.add(o);
        book.save();
    }
    Fridge::setStock({ QObject::tr("Eier"), QObject::tr("Milch"), QObject::tr("Kartoffeln"), QObject::tr("Zwiebeln"), QObject::tr("Möhren"),
                       QObject::tr("Linsen"), QObject::tr("Butter"), QObject::tr("Mehl"), QObject::tr("Äpfel"),
                       QObject::tr("Würstchen"), QObject::tr("Haferflocken"), QObject::tr("Joghurt"), QObject::tr("Quark") });
}

// Rezeptseite wie aus dem Scanner (Screenshots des Scandialogs)
QImage demoScanPage()
{
    QImage page(1654, 2339, QImage::Format_RGB32);   // A4 mit 200 dpi
    page.fill(QColor(250, 248, 242));
    QPainter p(&page);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    Recipe demo;
    demo.title = QStringLiteral("Gemüse-Curry");
    QImage photo;
    photo.loadFromData(QByteArray::fromBase64(demoImage(demo).toLatin1()));
    if (!photo.isNull()) p.drawImage(QRect(900, 330, 620, 465), photo);
    auto font = [](int px, bool bold) {
        QFont f(QStringLiteral("DejaVu Serif"));
        f.setPixelSize(px);
        f.setBold(bold);
        return f;
    };
    p.setPen(QColor(150, 40, 30));
    p.setFont(font(76, true));
    p.drawText(130, 200, QStringLiteral("Ofengemüse mit Kräuterquark"));
    p.setPen(QColor(50, 50, 50));
    p.setFont(font(30, false));
    p.drawText(130, 270, QStringLiteral("Bunt, sättigend und schnell vorbereitet – den Rest erledigt der Ofen."));
    p.drawText(130, 320, QStringLiteral("Zubereitungszeit: 45 Min."));
    p.setPen(QColor(150, 40, 30));
    p.setFont(font(40, true));
    p.drawText(130, 420, QStringLiteral("Zutaten für 4 Personen"));
    p.setPen(QColor(25, 25, 25));
    p.setFont(font(32, false));
    const QStringList ing{ QStringLiteral("800 g Kartoffeln"), QStringLiteral("2 rote Paprikaschoten"), QStringLiteral("1 Zucchini"),
                           QStringLiteral("2 rote Zwiebeln"), QStringLiteral("3 EL Olivenöl"), QStringLiteral("1 TL Paprikapulver"),
                           QStringLiteral("Salz, Pfeffer"), QStringLiteral("500 g Magerquark"), QStringLiteral("100 ml Milch"),
                           QStringLiteral("1 Bund Schnittlauch"), QStringLiteral("½ Bund Petersilie") };
    int y = 490;
    for (const QString &t : ing) {
        p.drawText(130, y, QStringLiteral("• ") + t);
        y += 50;
    }
    y += 60;
    p.setPen(QColor(150, 40, 30));
    p.setFont(font(40, true));
    p.drawText(130, y, QStringLiteral("So wird's gemacht"));
    p.setPen(QColor(25, 25, 25));
    p.setFont(font(32, false));
    y += 70;
    const QStringList dir{ QStringLiteral("1. Den Backofen auf 200° C vorheizen. Kartoffeln waschen und vierteln, Paprika,"),
                           QStringLiteral("Zucchini und Zwiebeln in grobe Stücke schneiden."), QString(),
                           QStringLiteral("2. Gemüse mit Öl, Paprikapulver, Salz und Pfeffer mischen, auf einem Blech verteilen"),
                           QStringLiteral("und etwa 35 Minuten backen, dabei einmal wenden."), QString(),
                           QStringLiteral("3. Quark mit Milch glatt rühren, Kräuter fein schneiden und unterrühren, mit Salz"),
                           QStringLiteral("und Pfeffer abschmecken. Zum Ofengemüse servieren.") };
    for (const QString &t : dir) {
        if (!t.isEmpty()) p.drawText(130, y, t);
        y += t.isEmpty() ? 30 : 50;
    }
    p.setPen(QColor(110, 110, 110));
    p.setFont(font(26, false));
    p.drawText(130, 2200, QStringLiteral("Pro Portion: ca. 380 kcal"));
    p.end();
    return page;
}

// Das Programm hieß bis Version 0.5 „VitaMeals“. Beim ersten Start von ViviaMeals werden Einstellungen
// (Registrierung bzw. .conf) und Benutzerdaten (Profile, Tagebücher, eigene Lebensmittel) übernommen.
// Die alten Daten bleiben unverändert erhalten.
void migrateFromVitaMeals()
{
    QSettings now;
    if (now.value(QStringLiteral("migration/fromVitaMeals")).toBool()) return;
    QSettings old(QStringLiteral("mbergmann-sh"), QStringLiteral("VitaMeals"));
    if (now.allKeys().isEmpty())
        for (const QString &k : old.allKeys()) now.setValue(k, old.value(k));
    const QString newDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString oldDir = newDir;
    oldDir.replace(QStringLiteral("ViviaMeals"), QStringLiteral("VitaMeals"));
    if (oldDir != newDir && QFileInfo(oldDir).isDir()) {
        // rekursiv kopieren, vorhandene Dateien nicht überschreiben
        QDirIterator it(oldDir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString src = it.next();
            const QString dst = QDir(newDir).filePath(QDir(oldDir).relativeFilePath(src));
            if (QFileInfo::exists(dst)) continue;
            QDir().mkpath(QFileInfo(dst).absolutePath());
            QFile::copy(src, dst);
        }
        Profiles::instance().reload();
    }
    now.setValue(QStringLiteral("migration/fromVitaMeals"), true);
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
        if (auto *tabs = d.findChild<QTabWidget *>()) {
            tabs->setCurrentIndex(2);
            shot(&d, dir, QStringLiteral("assistent-tipps"));
        }
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
    {
        // ASCII-Umschrift (Kaese, heiss) wird beim Import korrigiert
        const auto res = Importers::importFile(QStringLiteral(":/samples/umlaute.rk"), Importers::Format::Unknown);
        ImportPreviewDialog d(res.recipes, { QStringLiteral("umlaute.rk"), QStringLiteral("umlaute.rk") },
                              { ImportFlow::umlautNote(res.umlautsFixed) },
                              w);
        d.show();
        shot(&d, dir, QStringLiteral("import-umlaute"));
        d.reject();
    }
    {
        QImage img(1200, 800, QImage::Format_RGB32);
        Recipe demo;
        demo.title = QStringLiteral("Gemüse-Curry");
        img.loadFromData(QByteArray::fromBase64(demoImage(demo).toLatin1()));
        if (img.isNull()) img.fill(QColor(200, 180, 120));
        ImageCropDialog d(img.scaled(1200, 900), w);
        d.show();
        QApplication::processEvents();
        // Ausschnitt per Maus verkleinern (prüft zugleich die Mausbedienung)
        if (auto *cw = d.findChild<CropWidget *>()) {
            const QRect before = cw->selection();
            auto send = [cw](QEvent::Type t, QPointF p, Qt::MouseButtons b) {
                QMouseEvent e(t, p, cw->mapToGlobal(p), t == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, b, Qt::NoModifier);
                QApplication::sendEvent(cw, &e);
            };
            const QRectF v(cw->rect());
            // untere rechte Ecke liegt am Rand des Bildbereichs – etwas nach innen ziehen
            QPointF corner(v.width() - 8 - (v.width() - 16 - (v.height() - 16) * 1200.0 / 900.0) / 2.0, v.height() - 8);
            send(QEvent::MouseButtonPress, corner, Qt::LeftButton);
            send(QEvent::MouseMove, corner - QPointF(200, 150), Qt::LeftButton);
            send(QEvent::MouseButtonRelease, corner - QPointF(200, 150), Qt::NoButton);
            send(QEvent::MouseButtonPress, QPointF(v.width() * 0.3, v.height() * 0.3), Qt::LeftButton);
            send(QEvent::MouseMove, QPointF(v.width() * 0.3 + 80, v.height() * 0.3 + 60), Qt::LeftButton);
            send(QEvent::MouseButtonRelease, QPointF(v.width() * 0.3 + 80, v.height() * 0.3 + 60), Qt::NoButton);
            const QRect after = cw->selection();
            const QImage res = d.result();
            qInfo("Zuschneiden: %dx%d -> %dx%d bei %d,%d, Ergebnis %dx%d", before.width(), before.height(), after.width(), after.height(),
                  after.x(), after.y(), res.width(), res.height());
        }
        shot(&d, dir, QStringLiteral("bild-zuschneiden"));
        d.reject();
    }
    {
        DiaryDialog d(w);
        d.show();
        shot(&d, dir, QStringLiteral("tagebuch"));
        Diary::Entry e;
        e.when = QDateTime(QDate::currentDate(), QTime(18, 30));
        DiaryEntryDialog ed(DiaryEntryDialog::Mode::Recipe, e, 1100, 1700, w);
        if (auto *f = ed.findChild<QLineEdit *>()) f->setText(QStringLiteral("suppe"));
        if (auto *l = ed.findChild<QListWidget *>()) l->setCurrentRow(0);
        if (auto *sb = ed.findChild<QDoubleSpinBox *>()) sb->setValue(1.5);
        ed.show();
        shot(&ed, dir, QStringLiteral("tagebuch-eintrag"));
        ed.reject();
        d.reject();
    }
    {
        WeighInDialog d(QString(), w);
        d.show();
        shot(&d, dir, QStringLiteral("gewichtskontrolle"));
        d.reject();
    }
    {
        SettingsDialog d(w);
        d.show();
        shot(&d, dir, QStringLiteral("einstellungen"));
        if (auto *tabs = d.findChild<QTabWidget *>()) {
            tabs->setCurrentIndex(2);
            wait(1500);   // Tesseract prüfen
            shot(&d, dir, QStringLiteral("einstellungen-scannen"));
        }
        static_cast<QDialog &>(d).reject();
    }
    {
        // Doubletten: zwei Kopien aus der Demo-Datenbank (danach wieder entfernt)
        RecipeDb &db = RecipeDb::instance();
        QStringList added;
        for (const RecipeSummary &s : QList<RecipeSummary>(db.summaries())) {
            Recipe r;
            if (!db.load(s.id, &r)) continue;
            if (s.title.contains(QStringLiteral("Curry"))) {
                r.id.clear();
                r.title = QStringLiteral("GEMUESE-CURRY MIT KICHERERBSEN");
                r.image.clear();
                r.favorite = false;
            } else if (s.title.startsWith(QStringLiteral("Apfelkuchen"))) {
                r.id.clear();
                r.title = QStringLiteral("Omas ") + r.title;
                r.image.clear();
            } else {
                continue;
            }
            r.id = Recipe::newId();
            db.save(r);
            added << r.id;
        }
        DuplicatesDialog d(w);
        d.show();
        shot(&d, dir, QStringLiteral("doubletten"));
        d.reject();
        for (const QString &id : added) db.remove(id);
    }
    {
        // Rezept scannen: gerenderte Rezeptseite mit Foto
        ScanDialog d(ScanDialog::StartNothing, w);
        d.show();
        d.addImage(demoScanPage(), QStringLiteral("demo"));
        for (int i = 0; i < 300 && d.busy(); ++i) wait(100);
        shot(&d, dir, QStringLiteral("scannen"));
        if (!d.pages().isEmpty()) {
            static_cast<QDialog &>(d).accept();
            EditorDialog e(d.recipe(), w);
            e.setReference(d.pages());
            e.resize(1500, 800);
            e.show();
            shot(&e, dir, QStringLiteral("scannen-editor"));
            e.reject();
        }
    }
    {
        // Nährwert-Import mit einer Tabelle im Stil einer Excel-Datei
        const QString csv = dir + QStringLiteral("/naehrwerte-beispiel.csv");
        QFile f(csv);
        if (f.open(QIODevice::WriteOnly)) {
            f.write("Bezeichnung;Warengruppe;Brennwert (kJ);Energie (kcal);Fett (g);davon ges\xe4ttigte Fetts\xe4uren (g);Kohlenhydrate (g);"
                    "davon Zucker (g);Ballaststoffe (g);Eiwei\xdf (g);Salz (g)\r\n"
                    "Dinkelvollkornmehl;Getreide;1402;335;2,5;0,4;61;1;10;14;0,01\r\nGr\xfcnkern geschrotet;Getreide;1440;344;2,7;0,5;63;1,5;8,8;11;<0,1\r\n"
                    "Hafer-Joghurt natur;Milch;259;62;2,8;0,3;7,6;2,4;1,2;1,2;0,1\r\nBergk\xe4se 50 %;K\xe4se;1650;398;32;20;0;0;0;28;1,5\r\n"
                    "Bio-Tofu geräuchert;Hülsenfrüchte;690;165;9,5;1,5;1,5;0,5;1;18;1,4\r\n");
            f.close();
        }
        FoodImportDialog d(w);
        d.show();
        if (auto *le = d.findChild<QLineEdit *>(QStringLiteral("foodImportPath"))) {
            le->setText(csv);
            emit le->editingFinished();
        }
        shot(&d, dir, QStringLiteral("naehrwerte-import"));
        d.reject();
        QFile::remove(csv);
    }
    {
        // Lebensmittel-Assistent: Fertigprodukt mit den Angaben der Verpackung
        FoodWizard wz(QStringLiteral("Buttaris"), w);
        Nutrients n;
        n.kcal = 3690; n.fat = 99.8; n.satFat = 69; n.salt = 0;
        wz.setEntered(n, QStringLiteral("Tegut"));
        if (auto *cb = wz.findChildren<QComboBox *>().value(2)) cb->setCurrentIndex(1);   // kJ
        wz.show();
        wz.next();
        shot(&wz, dir, QStringLiteral("lebensmittel-assistent"));
        wz.next();
        wz.next();
        shot(&wz, dir, QStringLiteral("lebensmittel-assistent-uebersicht"));
        wz.reject();
        // Open Food Facts: Produkt per Barcode übernommen (Antwort wie vom Server)
        FoodWizard wo(QStringLiteral("Nuss-Nougat-Creme"), w);
        OpenFoodFacts::Product op;
        OpenFoodFacts::parseProductReply(R"({"status":1,"product":{"code":"3017620422003","product_name_de":"Nutella","brands":"Ferrero",
            "quantity":"400 g","categories_tags":["en:sweet-spreads"],"nutriments":{"energy-kcal_100g":539,"fat_100g":30.9,
            "saturated-fat_100g":10.6,"carbohydrates_100g":57.5,"sugars_100g":56.3,"fiber_100g":0,"proteins_100g":6.3,"salt_100g":0.107}}})", &op);
        wo.applyProduct(op);
        wo.show();
        shot(&wo, dir, QStringLiteral("lebensmittel-openfoodfacts"));
        wo.reject();
        QList<Nutrition::MissingFood> miss;
        auto add = [&](const QString &name, int count, const QString &ex, const QString &recipe) {
            Nutrition::MissingFood m;
            m.name = name; m.key = FoodTable::normalize(name); m.count = count; m.examples = { ex }; m.recipes = { recipe };
            miss << m;
        };
        add(QStringLiteral("Buttaris"), 14, QStringLiteral("2 EL Buttaris"), QStringLiteral("Bratkartoffeln"));
        add(QStringLiteral("Fix für Spaghetti Bolognese"), 3, QStringLiteral("1 Pck. Fix für Spaghetti Bolognese"), QStringLiteral("Schnelle Bolognese"));
        add(QStringLiteral("Bratensoße, hell"), 2, QStringLiteral("1/4 l Bratensoße, hell"), QStringLiteral("Kalbsröllchen"));
        add(QStringLiteral("Kuchenglasur Zartbitter"), 2, QStringLiteral("1 Beutel Kuchenglasur Zartbitter"), QStringLiteral("Marmorkuchen"));
        MissingFoodsDialog md(miss, QObject::tr("Diese Zutaten der importierten Rezepte haben noch keine Nährwerte (häufigste zuerst):"), w);
        md.show();
        shot(&md, dir, QStringLiteral("lebensmittel-fehlend"));
        md.reject();
    }
    {
        PackageExportDialog d(w->property("visibleIds").toStringList(), w);
        d.show();
        shot(&d, dir, QStringLiteral("paket-export"));
        d.reject();
        QList<Recipe> list;
        for (const RecipeSummary &s : RecipeDb::instance().summaries()) {
            Recipe r;
            if (list.size() < 12 && RecipeDb::instance().load(s.id, &r)) list << r;
        }
        RecipePackage::Options o;
        o.name = QStringLiteral("Familienrezepte");
        o.author = QStringLiteral("Michael");
        o.description = QStringLiteral("Unsere liebsten Alltagsrezepte – viel Spaß beim Nachkochen!");
        RecipePackage::Content c;
        RecipePackage::parse(RecipePackage::build(list, o), &c);
        PackageImportDialog pd(c, QStringLiteral("Familienrezepte.vmpack"), true, w);
        pd.show();
        shot(&pd, dir, QStringLiteral("paket-import"));
        pd.reject();
    }
    {
        // LaTeX-Export: Vorgabe Kapitel, dann „Neues Master-Dokument“
        QList<RecipeSummary> all = RecipeDb::instance().summaries();
        Recipe r;
        if (!all.isEmpty() && RecipeDb::instance().load(all.first().id, &r)) {
            // Beispiel-Hintergrundbild (Rezeptfoto) frei positioniert auf der Titelseite
            QSettings st;
            const QString bg = QDir(dir).filePath(QStringLiteral("_hintergrund.png"));
            for (const RecipeSummary &x : all) {
                Recipe pr;
                if (RecipeDb::instance().load(x.id, &pr) && !pr.image.isEmpty()) {
                    QImage::fromData(QByteArray::fromBase64(pr.image.toLatin1())).save(bg);
                    break;
                }
            }
            st.setValue(QStringLiteral("tex/bgImage"), bg);
            st.setValue(QStringLiteral("tex/bgTarget"), 0);
            st.setValue(QStringLiteral("tex/bgPlacement"), 2);
            st.setValue(QStringLiteral("tex/bgWidth"), 139.9);
            st.setValue(QStringLiteral("tex/bgLeft"), 46.4);
            st.setValue(QStringLiteral("tex/bgBottom"), 60.5);
            st.setValue(QStringLiteral("tex/author"), QStringLiteral("Micha B."));
            st.setValue(QStringLiteral("tex/heading"), 1);
            LatexExportDialog d(r, w);
            d.show();
            QApplication::processEvents();
            shot(&d, dir, QStringLiteral("latex-export"));
            const auto radios = d.findChildren<QRadioButton *>();
            if (radios.size() >= 2) radios[1]->setChecked(true);
            QApplication::processEvents();
            shot(&d, dir, QStringLiteral("latex-kochbuch"));
            d.reject();
            QFile::remove(bg);
            for (const char *k : { "tex/bgImage", "tex/bgTarget", "tex/bgPlacement", "tex/bgWidth", "tex/bgLeft", "tex/bgBottom", "tex/author", "tex/heading" })
                st.remove(QLatin1String(k));
        }
    }
    {
        // Menüassistent: Rahmen, Vorlieben, Menü, Abschluss; Rezeptwahl für einen Gang; gespeicherte Menüs
        MenuWizard wz(w);
        wz.show();
        if (auto *r = wz.findChildren<QRadioButton *>().value(2)) r->click();   // 5 Gänge
        if (auto *t = wz.findChildren<QLineEdit *>().value(0)) t->setText(QObject::tr("Festtagsmenü"));
        if (auto *o = wz.findChildren<QLineEdit *>().value(1)) o->setText(QObject::tr("Weihnachten im Familienkreis"));
        QApplication::processEvents();
        shot(&wz, dir, QStringLiteral("menue-1-rahmen"));
        wz.next();
        QApplication::processEvents();
        shot(&wz, dir, QStringLiteral("menue-2-vorlieben"));
        wz.next();
        wz.proposeNow(11);
        wz.selectCourse(3);
        QApplication::processEvents();
        shot(&wz, dir, QStringLiteral("menue-3-menue"));
        {
            Menu::Composer comp(RecipeDb::instance().summaries(), wz.menu().prefs, 11);
            MenuCandidateDialog cd(comp, wz.menu().items, 4, &wz);
            cd.show();
            QApplication::processEvents();
            shot(&cd, dir, QStringLiteral("menue-gang-waehlen"));
        }
        wz.next();
        QApplication::processEvents();
        shot(&wz, dir, QStringLiteral("menue-4-export"));
        Menu::MenuData saved = wz.menu();
        saved.date = QDate(2026, 12, 24);
        Menu::saveMenu(RecipeDb::instance().folder(), saved);
        {
            MenuListDialog ld(w);
            ld.show();
            QApplication::processEvents();
            shot(&ld, dir, QStringLiteral("menue-gespeichert"));
        }
        wz.hide();
    }
    {
        // Abgleich einrichten: eigene Website, Freigabe im LAN; Laden auf einem neuen Gerät
        const QString folder = RecipeDb::instance().folder();
        Sync::Connection c;
        c.kind = Sync::Connection::Kind::Website;
        c.url = QStringLiteral("https://www.meine-seite.de/viviameals/viviameals-sync.php");
        c.token = Sync::newToken();
        c.save(folder);
        {
            SyncSetupDialog d(folder, RecipeDb::instance().name(), nullptr, w);
            d.show();
            QApplication::processEvents();
            shot(&d, dir, QStringLiteral("abgleich-website"));
        }
        Sync::Server server;
        server.setPersistTokens(false);
        QTemporaryDir store;
        server.start(store.path(), RecipeDb::instance().name(), 0);
        c.kind = Sync::Connection::Kind::Host;
        c.save(folder);
        {
            SyncSetupDialog d(folder, RecipeDb::instance().name(), &server, w);
            d.show();
            QApplication::processEvents();
            shot(&d, dir, QStringLiteral("abgleich-freigabe"));
        }
        {
            SyncLoadDialog d(w);
            d.show();
            if (auto *r = d.findChildren<QRadioButton *>().value(1)) r->click();
            QApplication::processEvents();
            shot(&d, dir, QStringLiteral("abgleich-laden"));
        }
        server.stop();
        QFile::remove(QDir(folder).filePath(QStringLiteral(".sync/verbindung.json")));
    }
    {
        BusyProgress prog(w, QObject::tr("Kalorien und Nährwerte neu berechnen"), 3127);
        prog.step(1412, QObject::tr("Rezept 1413 von 3127: Kartoffel-Lauch-Suppe"));
        QApplication::processEvents();
        if (QWidget *d = QApplication::activeModalWidget()) shot(d, dir, QStringLiteral("fortschritt"));
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
    QApplication::setApplicationName(QStringLiteral("ViviaMeals"));
    QApplication::setApplicationVersion(QStringLiteral(VIVIAMEALS_VERSION));
    QApplication::setWindowIcon(Icons::app());

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("ViviaMeals – Rezeptverwaltung mit Nährwertanalyse und Gewichtskontrolle"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption demo(QStringLiteral("demo"), QStringLiteral("Demo mit Beispielrezepten in einem temporären Ordner"));
    QCommandLineOption shots(QStringLiteral("screenshots"), QStringLiteral("Bildschirmfotos erzeugen (mit --demo)"), QStringLiteral("ordner"));
    QCommandLineOption dbOpt(QStringLiteral("database"), QStringLiteral("Rezeptdatenbank (Ordner)"), QStringLiteral("ordner"));
    parser.addOptions({ demo, shots, dbOpt });
    parser.addPositionalArgument(QStringLiteral("dateien"), QStringLiteral("Rezepte (.vmr), Datenbank (viviameals.vmdb), RezKonv- oder MealMaster-Dateien"),
                                 QStringLiteral("[dateien…]"));
    parser.process(app);
    const bool isDemo = parser.isSet(demo);
    if (isDemo) QApplication::setApplicationName(QStringLiteral("ViviaMeals-Demo"));   // eigene Einstellungen und Profile
    else migrateFromVitaMeals();

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
        const QString folder = QDir::temp().filePath(QStringLiteral("viviameals-demo"));
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
            QStringList files = parser.positionalArguments();
            QString db = parser.value(dbOpt);
            // per Doppelklick geöffnete Datenbank hat Vorrang vor der zuletzt geöffneten
            for (const QString &f : files)
                if (QFileInfo(f).fileName() == RecipeDb::markerName() || QFileInfo(f).isDir()) db = f;
            files.removeAll(db);
            if (db.isEmpty() || !w.openDatabase(db)) w.ensureDatabase();
            if (!files.isEmpty()) w.openFiles(files);
        });
    }
    if (QSettings().value(QStringLiteral("ui/startNotice"), true).toBool()) {
        QTimer::singleShot(600, &w, [&w] {
            QMessageBox box(&w);
            box.setWindowTitle(QObject::tr("Hinweis"));
            box.setIcon(QMessageBox::Information);
            box.setText(QObject::tr("<b>ViviaMeals ist kein Medizinprodukt.</b><br/>Nährwerte, Energiebedarf und Wochenpläne sind "
                                    "Schätzwerte für die persönliche Planung und ersetzen keine ärztliche oder Ernährungsberatung."));
            auto *dont = new QCheckBox(QObject::tr("Nicht mehr anzeigen"));
            box.setCheckBox(dont);
            box.exec();
            if (dont->isChecked()) QSettings().setValue(QStringLiteral("ui/startNotice"), false);
        });
    }
    return app.exec();
}
