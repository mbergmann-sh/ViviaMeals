// ViviaMeals Mobil – Einstieg
//
//   ViviaMeals                         normaler Start
//   ViviaMeals --size phone|tablet|WxH Fenstergröße zum Ausprobieren am PC (dp)
//   ViviaMeals --screenshots <ordner>  Bildschirmfotos aller Seiten in Telefon- und Tablet-Größe (Demo-Daten)
//   ViviaMeals --selftest              Kern und Schnittstelle ohne Oberfläche prüfen
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "mobileapp.h"
#include "recipeimageprovider.h"
#include "viviameals_version.h"

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

static void takeScreenshots(QQmlApplicationEngine &engine, QGuiApplication &app, const QString &dir);

int main(int argc, char *argv[])
{
    QGuiApplication::setOrganizationName(QStringLiteral("mbergmann-sh"));
    QGuiApplication::setApplicationName(QStringLiteral("ViviaMeals"));
    QGuiApplication::setApplicationVersion(QStringLiteral(VIVIAMEALS_VERSION));
    QGuiApplication app(argc, argv);
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));

    QCommandLineParser p;
    p.addVersionOption();
    QCommandLineOption size(QStringLiteral("size"), QStringLiteral("Fenstergröße: phone, tablet oder BxH"), QStringLiteral("größe"));
    QCommandLineOption shots(QStringLiteral("screenshots"), QStringLiteral("Bildschirmfotos erzeugen"), QStringLiteral("ordner"));
    QCommandLineOption self(QStringLiteral("selftest"), QStringLiteral("Prüfung ohne Oberfläche"));
    p.addOptions({ size, shots, self });
    p.process(app);

    // Demo- und Testläufe in einem eigenen, leeren Datenordner
    std::unique_ptr<QTemporaryDir> tmp;
    if (p.isSet(shots) || p.isSet(self)) {
        tmp = std::make_unique<QTemporaryDir>();
        qputenv("XDG_DATA_HOME", tmp->path().toUtf8());
        qputenv("XDG_CONFIG_HOME", (tmp->path() + QStringLiteral("/config")).toUtf8());
        QGuiApplication::setApplicationName(QStringLiteral("ViviaMeals-Mobil-Demo"));
    }

    MobileApp vm;
    QString error;
    if (!vm.start(&error)) {
        QTextStream(stderr) << "Datenbank: " << error << "\n";
        if (!p.isSet(self)) return 1;
    }
    if (p.isSet(self)) {
        QTextStream out(stdout);
        return vm.selfTest(out);
    }

    QQuickStyle::setStyle(QStringLiteral("Material"));
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("recipe"), new RecipeImageProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &vm);
    QString sz = p.value(size);
    if (p.isSet(shots)) sz = QStringLiteral("phone");
    QSize win;
    if (sz == QLatin1String("phone")) win = QSize(400, 860);
    else if (sz == QLatin1String("tablet")) win = QSize(1280, 800);
    else if (sz.contains(QLatin1Char('x'))) win = QSize(sz.section(QLatin1Char('x'), 0, 0).toInt(), sz.section(QLatin1Char('x'), 1, 1).toInt());
    engine.rootContext()->setContextProperty(QStringLiteral("startWidth"), win.isValid() ? win.width() : 0);
    engine.rootContext()->setContextProperty(QStringLiteral("startHeight"), win.isValid() ? win.height() : 0);
    engine.rootContext()->setContextProperty(QStringLiteral("screenshotMode"), p.isSet(shots));
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 2;

    // Abgleich beim Start und beim Zurückkehren in die App (falls eingerichtet)
    if (!p.isSet(shots)) {
        QTimer::singleShot(1500, &vm, &MobileApp::autoSync);
        QObject::connect(&app, &QGuiApplication::applicationStateChanged, &vm, [&vm](Qt::ApplicationState s) {
            static QDateTime hidden;
            if (s == Qt::ApplicationSuspended || s == Qt::ApplicationHidden) hidden = QDateTime::currentDateTime();
            if (s == Qt::ApplicationActive && hidden.isValid() && hidden.secsTo(QDateTime::currentDateTime()) > 300) {
                hidden = QDateTime();
                vm.autoSync();
            }
        });
    }
    if (p.isSet(shots)) takeScreenshots(engine, app, p.value(shots));
    return app.exec();
}

// Bildschirmfotos: Main.qml stellt showScreen(name, wide) bereit
static void takeScreenshots(QQmlApplicationEngine &engine, QGuiApplication &app, const QString &dir)
{
    QDir().mkpath(dir);
    auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    struct Shot { const char *name; const char *screen; int w, h; };
    static const Shot list[] = {
        { "telefon-rezepte", "recipes", 400, 860 },     { "telefon-rezept", "recipe", 400, 860 },
        { "telefon-editor", "editor", 400, 860 },       { "telefon-einkauf", "shopping", 400, 860 },
        { "telefon-kuehlschrank", "fridge", 400, 860 }, { "telefon-tagebuch", "diary", 400, 860 },
        { "telefon-mehr", "more", 400, 860 },           { "telefon-pc-version", "desktop", 400, 860 },
        { "telefon-abgleich", "sync", 400, 860 },       { "telefon-glossar", "glossary", 400, 860 },
        { "telefon-quer", "recipe", 860, 400 },         { "tablet-rezepte", "recipe", 1280, 800 },
        { "tablet-hoch", "recipe", 800, 1280 },         { "tablet-plan", "plan", 1280, 800 },
        { "tablet-ich", "me", 1280, 800 },              { "telefon-start", "welcome", 400, 860 },
    };
    auto *i = new int(0);
    auto *timer = new QTimer(&app);
    timer->setInterval(900);
    QObject::connect(timer, &QTimer::timeout, &app, [=, &app]() {
        static bool shown = false;
        const int n = int(sizeof(list) / sizeof(list[0]));
        if (*i >= n) {
            timer->stop();
            app.quit();
            return;
        }
        const Shot &s = list[*i];
        if (!shown) {
            win->resize(s.w, s.h);
            QMetaObject::invokeMethod(win, "showScreen", Q_ARG(QVariant, QString::fromLatin1(s.screen)));
            shown = true;
            return;
        }
        win->grabWindow().save(QDir(dir).filePath(QString::fromLatin1(s.name) + QStringLiteral(".png")));
        shown = false;
        ++*i;
    });
    timer->start();
}
