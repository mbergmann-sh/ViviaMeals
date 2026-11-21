// ViviaMeals Mobil – Schnittstelle zwischen Kern und QML (siehe mobileapp.h)
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "mobileapp.h"

#include "basenfasten.h"
#include "diary.h"
#include "foodtable.h"
#include "fridge.h"
#include "ingredientparser.h"
#include "kitchenglossary.h"
#include "markup.h"
#include "mealplanner.h"
#include "nutrition.h"
#include "platform.h"
#include "profiles.h"
#include "recipe.h"
#include "recipedb.h"
#include "recipehtml.h"
#include "recipepackage.h"
#include "season.h"
#include "sync.h"
#include "syncserver.h"
#include "units.h"
#include "viviameals_version.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QTime>
#include <memory>

namespace {

const char *kProductUrl = "https://portfolio.mbergmann-sh.de/article/viviameals-rezeptverwaltung-fuer-windows-download";

QString appDataDir()
{
    const QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(d);
    return d;
}

QString shoppingFile()
{
    return QDir(appDataDir()).filePath(QStringLiteral("einkaufsliste.json"));
}

QString fmt(double v, int decimals = 1)
{
    return Fmt::number(v, decimals);
}

QVariantMap nutrientsMap(const Nutrients &n)
{
    return { { QStringLiteral("kcal"), qRound(n.kcal) },       { QStringLiteral("kJ"), qRound(n.kJ()) },
             { QStringLiteral("protein"), fmt(n.protein) },   { QStringLiteral("fat"), fmt(n.fat) },
             { QStringLiteral("satFat"), fmt(n.satFat) },     { QStringLiteral("carbs"), fmt(n.carbs) },
             { QStringLiteral("sugar"), fmt(n.sugar) },       { QStringLiteral("fiber"), fmt(n.fiber) },
             { QStringLiteral("salt"), fmt(n.salt, 2) } };
}

QString localPath(const QUrl &url)
{
    // Android liefert content://-Adressen, die QFile direkt öffnen kann
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

// Hintergrundarbeit: work() im eigenen Thread, done() danach im Haupt-Thread
template<typename Result>
void runInThread(QObject *ctx, std::function<Result()> work, std::function<void(const Result &)> done)
{
    auto result = std::make_shared<Result>();
    QThread *t = QThread::create([work, result]() { *result = work(); });
    QPointer<QObject> guard(ctx);
    QObject::connect(t, &QThread::finished, ctx, [t, result, done, guard]() {
        if (guard) done(*result);
        t->deleteLater();
    });
    t->start();
}

QStringList splitTags(const QString &text)
{
    QStringList out;
    for (QString t : text.split(QRegularExpression(QStringLiteral("[,;]")), Qt::SkipEmptyParts)) {
        t = t.trimmed().toLower();
        if (!t.isEmpty() && !out.contains(t)) out << t;
    }
    return out;
}

} // namespace

MobileApp::MobileApp(QObject *parent) : QObject(parent), m_recipes(new RecipeListModel(this))
{
    connect(&RecipeDb::instance(), &RecipeDb::changed, m_recipes, &RecipeListModel::refresh);
}

MobileApp::~MobileApp() = default;

QString MobileApp::sampleSource()
{
    return QStringLiteral("ViviaMeals-Beispielrezepte");
}

QString MobileApp::databaseFolder()
{
    return QDir(appDataDir()).filePath(QStringLiteral("Rezepte"));
}

QString MobileApp::version() const
{
    return QStringLiteral(VIVIAMEALS_VERSION);
}

QString MobileApp::productUrl() const
{
    return QString::fromLatin1(kProductUrl);
}

bool MobileApp::isAndroid() const
{
    return Platform::isAndroid();
}

QString MobileApp::seasonLabel() const
{
    return RecipeHtml::tagLabel(Season::seasonTagFor(QDate::currentDate()));
}

void MobileApp::setBusy(bool on, const QString &text, int done, int total)
{
    m_busy = on;
    m_busyText = text;
    m_busyDone = done;
    m_busyTotal = total;
    emit busyChanged();
}

// ----------------------------------------------------------------------------------------------- Start

bool MobileApp::start(QString *error)
{
    const QString folder = databaseFolder();
    RecipeDb &db = RecipeDb::instance();
    if (RecipeDb::isDatabaseFolder(folder)) {
        if (!db.open(folder, error)) return false;
    } else {
        QDir().mkpath(folder);
        if (!db.create(folder, QStringLiteral("Meine Rezepte"), error)) return false;
        if (!installSamples(error)) return false;
    }
    ensureProfile();
    m_recipes->refresh();
    Platform::setKeepScreenOn(false);
    return true;
}

bool MobileApp::installSamples(QString *error)
{
    RecipePackage::Content c;
    if (!RecipePackage::read(QStringLiteral(":/data/beispielrezepte.vmpack"), &c, error)) return false;
    RecipeDb &db = RecipeDb::instance();
    db.beginBatch();
    for (Recipe r : c.recipes) {
        r.source = sampleSource();
        db.save(r);
    }
    db.endBatch();
    RecipePackage::importFoods(c);
    return true;
}

int MobileApp::sampleCount() const
{
    int n = 0;
    const QString src = sampleSource();
    // Quelle steht nicht in der Zusammenfassung; die IDs der Beispiele merken wir uns über die Datei
    for (const RecipeSummary &s : RecipeDb::instance().summaries()) {
        Recipe r;
        if (RecipeDb::instance().loadLight(s.id, &r) && r.source == src) ++n;
    }
    return n;
}

void MobileApp::reopenDatabase()
{
    QString err;
    RecipeDb::instance().open(databaseFolder(), &err);
    m_recipes->refresh();
    emit databaseChanged();
}

// ----------------------------------------------------------------------------------------------- Einstellungen

int MobileApp::themeMode() const
{
    return QSettings().value(QStringLiteral("mobile/theme"), 0).toInt();
}

void MobileApp::setThemeMode(int m)
{
    QSettings().setValue(QStringLiteral("mobile/theme"), qBound(0, m, 2));
    emit settingsChanged();
}

bool MobileApp::keepScreenOn() const
{
    return QSettings().value(QStringLiteral("mobile/keepScreenOn"), true).toBool();
}

void MobileApp::setKeepScreenOn(bool on)
{
    QSettings().setValue(QStringLiteral("mobile/keepScreenOn"), on);
    emit settingsChanged();
}

bool MobileApp::firstRun() const
{
    return !QSettings().value(QStringLiteral("mobile/firstRunDone"), false).toBool();
}

QString MobileApp::licenseText() const
{
    QFile f(QStringLiteral(":/doc/LICENSE"));
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

void MobileApp::finishFirstRun()
{
    QSettings().setValue(QStringLiteral("mobile/firstRunDone"), true);
    emit settingsChanged();
}

bool MobileApp::showEditorHint()
{
    QSettings s;
    if (s.value(QStringLiteral("mobile/editorHintOff"), false).toBool()) return false;
    const int n = s.value(QStringLiteral("mobile/editorHintCount"), 0).toInt();
    s.setValue(QStringLiteral("mobile/editorHintCount"), n + 1);
    return n % 5 == 0;
}

void MobileApp::disableEditorHint()
{
    QSettings().setValue(QStringLiteral("mobile/editorHintOff"), true);
}

void MobileApp::setCooking(bool on)
{
    Platform::setKeepScreenOn(on && keepScreenOn());
}

void MobileApp::share(const QString &title, const QString &text)
{
    if (!Platform::shareText(title, text)) emit message(tr("In die Zwischenablage kopiert."));
}

// ----------------------------------------------------------------------------------------------- Rezepte

QVariantMap MobileApp::recipe(const QString &id, double servings) const
{
    Recipe r;
    if (!RecipeDb::instance().loadLight(id, &r)) return {};
    const RecipeSummary *sum = RecipeDb::instance().summary(id);
    const double base = r.servings > 0 ? r.servings : 1;
    const double shown = servings > 0 ? servings : base;
    const double f = shown / base;
    QVariantList ings;
    for (Ingredient i : r.ingredients) {
        if (!i.heading) {
            i.amount *= f;
            i.amountMax *= f;
        }
        const QString line = Units::normalizeCelsius(i.displayLine());
        ings << QVariantMap{ { QStringLiteral("heading"), i.heading },
                             { QStringLiteral("text"), i.heading ? i.name : line } };
    }
    QVariantList tags;
    for (const QString &t : r.tags) tags << QVariantMap{ { QStringLiteral("tag"), t }, { QStringLiteral("label"), RecipeHtml::tagLabel(t) } };
    const NutritionInfo &n = r.nutrition;
    QVariantMap nut = nutrientsMap(n.perServing);
    nut.insert(QStringLiteral("valid"), n.valid() && n.perServing.kcal > 0);
    nut.insert(QStringLiteral("coverage"), qRound(n.coverage() * 100));
    nut.insert(QStringLiteral("unmatched"), n.unmatched);
    const QString unit = r.yieldUnit.isEmpty() ? (shown == 1 ? tr("Portion") : tr("Portionen")) : (shown == 1 ? RecipeListModel::perUnit(r.yieldUnit) : r.yieldUnit);
    return { { QStringLiteral("id"), r.id },
             { QStringLiteral("title"), r.title },
             { QStringLiteral("servings"), shown },
             { QStringLiteral("baseServings"), base },
             { QStringLiteral("servingsLabel"), unit },
             { QStringLiteral("perLabel"), RecipeListModel::perUnit(r.yieldUnit) },
             { QStringLiteral("tags"), tags },
             { QStringLiteral("ingredients"), ings },
             { QStringLiteral("instructionsHtml"), Markup::toHtml(r.instructions) },
             { QStringLiteral("prepMinutes"), r.prepMinutes },
             { QStringLiteral("source"), r.source == sampleSource() ? tr("ViviaMeals-Beispielrezept") : r.source },
             { QStringLiteral("hasImage"), sum ? sum->hasImage : false },
             { QStringLiteral("favorite"), r.favorite },
             { QStringLiteral("nutrition"), nut },
             { QStringLiteral("season"), Season::monthsText(Season::recipeMonths(r)) } };
}

QVariantList MobileApp::themes() const
{
    const auto &all = RecipeDb::instance().summaries();
    QHash<QString, int> counts;
    int fav = 0;
    const QString season = Season::seasonTagFor(QDate::currentDate());
    int seasonal = 0;
    for (const RecipeSummary &s : all) {
        for (const QString &t : s.tags) counts[t]++;
        if (s.favorite) ++fav;
        if (s.tags.contains(season)) ++seasonal;
    }
    QVariantList out;
    out << QVariantMap{ { QStringLiteral("tag"), QString() }, { QStringLiteral("label"), tr("Alle Rezepte") }, { QStringLiteral("count"), int(all.size()) } };
    out << QVariantMap{ { QStringLiteral("tag"), QStringLiteral(":fav") }, { QStringLiteral("label"), tr("Favoriten") }, { QStringLiteral("count"), fav } };
    out << QVariantMap{ { QStringLiteral("tag"), QStringLiteral(":season") },
                        { QStringLiteral("label"), tr("Saisonal (%1)").arg(seasonLabel()) },
                        { QStringLiteral("count"), seasonal } };
    for (const QString &t : RecipeDb::instance().allTags())
        out << QVariantMap{ { QStringLiteral("tag"), t }, { QStringLiteral("label"), RecipeHtml::tagLabel(t) }, { QStringLiteral("count"), counts.value(t) } };
    return out;
}

bool MobileApp::toggleFavorite(const QString &id)
{
    Recipe r;
    RecipeDb &db = RecipeDb::instance();
    if (!db.load(id, &r)) return false;
    r.favorite = !r.favorite;
    db.save(r);
    m_recipes->refresh();
    return r.favorite;
}

QString MobileApp::randomRecipe() const
{
    return RecipeDb::instance().randomId();
}

QVariantMap MobileApp::recipeForEdit(const QString &id) const
{
    Recipe r;
    if (!id.isEmpty() && !RecipeDb::instance().loadLight(id, &r)) return {};
    if (id.isEmpty()) r.servings = 4;
    QStringList tags = r.tags;
    return { { QStringLiteral("id"), r.id },
             { QStringLiteral("title"), r.title },
             { QStringLiteral("servings"), r.servings },
             { QStringLiteral("yieldUnit"), r.yieldUnit },
             { QStringLiteral("prepMinutes"), r.prepMinutes },
             { QStringLiteral("tags"), tags.join(QStringLiteral(", ")) },
             { QStringLiteral("ingredients"), Units::normalizeCelsius(r.ingredientsText()) },
             { QStringLiteral("instructions"), r.instructions },
             { QStringLiteral("hasImage"), !id.isEmpty() && RecipeDb::instance().summary(id) && RecipeDb::instance().summary(id)->hasImage } };
}

QVariantMap MobileApp::previewNutrition(const QString &ingredients, double servings) const
{
    Recipe r;
    r.servings = servings > 0 ? servings : 1;
    r.ingredients = IngredientParser::parseText(Units::normalizeCelsius(ingredients));
    Nutrition::analyze(r);
    QVariantMap m = nutrientsMap(r.nutrition.perServing);
    m.insert(QStringLiteral("coverage"), qRound(r.nutrition.coverage() * 100));
    m.insert(QStringLiteral("unmatched"), r.nutrition.unmatched);
    m.insert(QStringLiteral("count"), r.nutrition.countable);
    return m;
}

QString MobileApp::saveRecipe(const QVariantMap &d)
{
    RecipeDb &db = RecipeDb::instance();
    Recipe r;
    const QString id = d.value(QStringLiteral("id")).toString();
    if (!id.isEmpty() && !db.load(id, &r)) return {};
    const QString title = d.value(QStringLiteral("title")).toString().trimmed();
    if (title.isEmpty()) {
        emit message(tr("Bitte einen Titel eingeben."));
        return {};
    }
    r.title = title;
    r.servings = qMax(0.5, d.value(QStringLiteral("servings")).toDouble());
    r.yieldUnit = d.value(QStringLiteral("yieldUnit")).toString().trimmed();
    r.prepMinutes = d.value(QStringLiteral("prepMinutes")).toInt();
    r.instructions = d.value(QStringLiteral("instructions")).toString().trimmed();
    // Zutaten: manuelle Zuordnungen gleichnamiger Zutaten behalten
    QHash<QString, QString> manual;
    for (const Ingredient &i : r.ingredients)
        if (i.foodManual) manual.insert(i.name.toLower(), i.food);
    r.ingredients = IngredientParser::parseText(Units::normalizeCelsius(d.value(QStringLiteral("ingredients")).toString()));
    for (Ingredient &i : r.ingredients) {
        auto it = manual.constFind(i.name.toLower());
        if (it != manual.constEnd()) {
            i.food = *it;
            i.foodManual = true;
        }
    }
    r.tags = splitTags(d.value(QStringLiteral("tags")).toString());
    // Foto: neue Datei, entfernen oder unverändert
    const QUrl image = d.value(QStringLiteral("imageUrl")).toUrl();
    if (d.value(QStringLiteral("removeImage")).toBool()) r.image.clear();
    if (image.isValid() && !image.isEmpty()) {
        QFile f(localPath(image));
        QImage img;
        if (f.open(QIODevice::ReadOnly)) img = QImage::fromData(f.readAll());
        if (img.isNull()) {
            emit message(tr("Das Bild konnte nicht gelesen werden."));
        } else {
            if (img.width() > 1200 || img.height() > 1200) img = img.scaled(1200, 1200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QByteArray jpg;
            QBuffer buf(&jpg);
            buf.open(QIODevice::WriteOnly);
            img.convertToFormat(QImage::Format_RGB888).save(&buf, "JPEG", 85);
            r.image = QString::fromLatin1(jpg.toBase64());
        }
    }
    if (r.id.isEmpty()) {
        r.id = Recipe::newId();
        r.sourceFormat = QStringLiteral("ViviaMeals");
        r.language = QStringLiteral("de");
    }
    Nutrition::analyze(r);
    if (!Nutrition::isMenu(r)) Season::applyTags(r);
    Basenfasten::applyTag(r);
    for (const QString &t : Nutrition::suggestTags(r))
        if (!r.hasTag(t)) r.addTag(t);
    QString err;
    if (!db.save(r, &err)) {
        emit message(tr("Speichern fehlgeschlagen: %1").arg(err));
        return {};
    }
    m_recipes->refresh();
    emit databaseChanged();
    return r.id;
}

bool MobileApp::deleteRecipe(const QString &id)
{
    const bool ok = RecipeDb::instance().remove(id);
    if (ok) {
        m_recipes->refresh();
        emit databaseChanged();
    }
    return ok;
}

QString MobileApp::recipeText(const QString &id) const
{
    Recipe r;
    if (!RecipeDb::instance().loadLight(id, &r)) return {};
    QString t = r.title + QStringLiteral("\n") + r.servingsText() + QStringLiteral("\n\n") + tr("Zutaten:") + QStringLiteral("\n");
    for (const Ingredient &i : r.ingredients) t += (i.heading ? QStringLiteral("\n") + i.name + QStringLiteral(":") : QStringLiteral("• ") + i.displayLine()) + QStringLiteral("\n");
    t += QStringLiteral("\n") + tr("Zubereitung:") + QStringLiteral("\n") + Markup::toPlain(r.instructions) + QStringLiteral("\n\n");
    t += tr("Aus ViviaMeals – %1").arg(productUrl());
    return t;
}

// ----------------------------------------------------------------------------------------------- Einkaufsliste

QJsonArray MobileApp::loadShopping() const
{
    QFile f(shoppingFile());
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).array();
}

void MobileApp::saveShopping(const QJsonArray &a)
{
    QSaveFile f(shoppingFile());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(a).toJson(QJsonDocument::Compact));
        f.commit();
    }
    emit shoppingChanged();
}

int MobileApp::shoppingOpen() const
{
    int n = 0;
    for (const QJsonValue &v : loadShopping())
        if (!v.toObject().value(QStringLiteral("checked")).toBool()) ++n;
    return n;
}

QVariantList MobileApp::shoppingItems() const
{
    QVariantList out;
    const QJsonArray a = loadShopping();
    for (int i = 0; i < a.size(); ++i) {
        const QJsonObject o = a.at(i).toObject();
        out << QVariantMap{ { QStringLiteral("index"), i },
                            { QStringLiteral("text"), o.value(QStringLiteral("text")).toString() },
                            { QStringLiteral("category"), o.value(QStringLiteral("category")).toString().isEmpty()
                                                              ? tr("Sonstiges") : FoodTable::categoryName(o.value(QStringLiteral("category")).toString()) },
                            { QStringLiteral("recipes"), o.value(QStringLiteral("recipes")).toString() },
                            { QStringLiteral("checked"), o.value(QStringLiteral("checked")).toBool() } };
    }
    return out;
}

// kg → g, l → ml, damit gespeicherte und neue Mengen zusammenpassen (Shopping::sort rechnet zurück)
static void baseUnit(ShoppingItem &it)
{
    if (it.unit == QLatin1String("kg")) { it.unit = QStringLiteral("g"); it.amount *= 1000; }
    else if (it.unit == QLatin1String("l")) { it.unit = QStringLiteral("ml"); it.amount *= 1000; }
}

static QJsonArray mergeShopping(const QJsonArray &existing, QList<ShoppingItem> items)
{
    // vorhandene, noch offene Einträge mit Mengen zusammenführen
    QList<ShoppingItem> open;
    QJsonArray done;
    for (const QJsonValue &v : existing) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("checked")).toBool() || !o.contains(QStringLiteral("name"))) {
            done.append(o);
            continue;
        }
        ShoppingItem it;
        it.name = o.value(QStringLiteral("name")).toString();
        it.amount = o.value(QStringLiteral("amount")).toDouble();
        it.unit = o.value(QStringLiteral("unit")).toString();
        it.category = o.value(QStringLiteral("category")).toString();
        it.recipes = o.value(QStringLiteral("recipes")).toString().split(QStringLiteral(", "), Qt::SkipEmptyParts);
        baseUnit(it);
        open << it;
    }
    for (ShoppingItem n : items) {
        baseUnit(n);
        bool merged = false;
        for (ShoppingItem &o : open)
            if (o.name.compare(n.name, Qt::CaseInsensitive) == 0 && (o.unit == n.unit || (o.amount <= 0 && n.amount <= 0))) {
                o.amount += n.amount;
                for (const QString &r : n.recipes)
                    if (!o.recipes.contains(r)) o.recipes << r;
                merged = true;
                break;
            }
        if (!merged) open << n;
    }
    Shopping::sort(open);
    QJsonArray out;
    for (const ShoppingItem &it : open)
        out.append(QJsonObject{ { QStringLiteral("name"), it.name },
                                { QStringLiteral("amount"), it.amount },
                                { QStringLiteral("unit"), it.unit },
                                { QStringLiteral("category"), it.category },
                                { QStringLiteral("recipes"), it.recipes.join(QStringLiteral(", ")) },
                                { QStringLiteral("text"), it.text() },
                                { QStringLiteral("checked"), false } });
    for (const QJsonValue &v : done) out.append(v);
    return out;
}

int MobileApp::addRecipeToShopping(const QString &id, double servings, bool onlyMissing)
{
    Recipe r;
    if (!RecipeDb::instance().loadLight(id, &r)) return 0;
    const double f = servings > 0 && r.servings > 0 ? servings / r.servings : 1.0;
    QList<ShoppingItem> items;
    if (onlyMissing) {
        QStringList st = Fridge::stock();
        st << Fridge::basics();
        items = Fridge::missingFor(r, st, f);
    } else {
        for (const Ingredient &i : r.ingredients) Shopping::add(items, i, f, r.title);
    }
    saveShopping(mergeShopping(loadShopping(), items));
    return int(items.size());
}

void MobileApp::addShoppingText(const QString &text)
{
    QList<ShoppingItem> items;
    for (const QString &line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const Ingredient i = IngredientParser::parseLine(line.trimmed());
        if (!i.name.isEmpty()) Shopping::add(items, i, 1.0, QString());
    }
    saveShopping(mergeShopping(loadShopping(), items));
}

void MobileApp::setShoppingChecked(int index, bool checked)
{
    QJsonArray a = loadShopping();
    if (index < 0 || index >= a.size()) return;
    QJsonObject o = a.at(index).toObject();
    o.insert(QStringLiteral("checked"), checked);
    a.replace(index, o);
    saveShopping(a);
}

void MobileApp::removeShoppingItem(int index)
{
    QJsonArray a = loadShopping();
    if (index < 0 || index >= a.size()) return;
    a.removeAt(index);
    saveShopping(a);
}

int MobileApp::removeCheckedShopping()
{
    QJsonArray a = loadShopping(), keep;
    for (const QJsonValue &v : a)
        if (!v.toObject().value(QStringLiteral("checked")).toBool()) keep.append(v);
    const int removed = int(a.size() - keep.size());
    saveShopping(keep);
    return removed;
}

void MobileApp::clearShopping()
{
    saveShopping({});
}

QString MobileApp::shoppingText() const
{
    QStringList lines;
    lines << tr("Einkaufsliste");
    for (const QJsonValue &v : loadShopping()) {
        const QJsonObject o = v.toObject();
        if (!o.value(QStringLiteral("checked")).toBool()) lines << QStringLiteral("☐ ") + o.value(QStringLiteral("text")).toString();
    }
    return lines.join(QLatin1Char('\n'));
}

QStringList MobileApp::stock() const
{
    return Fridge::stock();
}

void MobileApp::setStock(const QStringList &items)
{
    Fridge::setStock(items);
}

void MobileApp::addStock(const QString &item)
{
    QStringList s = Fridge::stock();
    for (QString t : item.split(QRegularExpression(QStringLiteral("[,\\n]")), Qt::SkipEmptyParts)) {
        t = t.trimmed();
        bool have = false;
        for (const QString &x : s)
            if (x.compare(t, Qt::CaseInsensitive) == 0) have = true;
        if (!t.isEmpty() && !have) s << t;
    }
    Fridge::setStock(s);
}

void MobileApp::removeStock(const QString &item)
{
    QStringList s = Fridge::stock();
    s.removeAll(item);
    Fridge::setStock(s);
}

QStringList MobileApp::basics() const
{
    return Fridge::basics();
}

QVariantList MobileApp::fridgeSuggestions(int maxMissing) const
{
    QStringList st = Fridge::stock();
    if (st.isEmpty()) return {};
    st << Fridge::basics();
    QVariantList out;
    const QList<Fridge::Suggestion> list = Fridge::suggest(st, maxMissing);
    for (const Fridge::Suggestion &s : list) {
        if (out.size() >= 60) break;
        out << QVariantMap{ { QStringLiteral("id"), s.id },
                            { QStringLiteral("title"), s.title },
                            { QStringLiteral("have"), s.have },
                            { QStringLiteral("total"), s.total },
                            { QStringLiteral("missing"), s.missing.join(QStringLiteral(", ")) } };
    }
    return out;
}

// ----------------------------------------------------------------------------------------------- Wochenplan

QVariantMap MobileApp::plan() const
{
    Profile *p = Profiles::instance().find(profileId());
    if (!p || p->lastPlan.isEmpty()) return {};
    const MealPlan plan = MealPlan::fromJson(p->lastPlan);
    if (plan.isEmpty()) return {};
    QLocale de(QLocale::German, QLocale::Germany);
    QVariantList days;
    for (int d = 0; d < plan.days.size(); ++d) {
        QVariantList meals;
        for (int m = 0; m < plan.days.at(d).size() && m < plan.meals.size(); ++m) {
            const PlanEntry &e = plan.days.at(d).at(m);
            if (e.recipeId.isEmpty() && e.title.isEmpty()) continue;
            meals << QVariantMap{ { QStringLiteral("meal"), plan.meals.at(m) },
                                  { QStringLiteral("title"), e.title },
                                  { QStringLiteral("id"), RecipeDb::instance().summary(e.recipeId) ? e.recipeId : QString() },
                                  { QStringLiteral("portions"), MealPlanner::portionText(e) },
                                  { QStringLiteral("kcal"), qRound(e.kcal) } };
        }
        const QDate date = plan.start.addDays(d);
        days << QVariantMap{ { QStringLiteral("date"), date.toString(Qt::ISODate) },
                             { QStringLiteral("label"), de.toString(date, QStringLiteral("dddd, d. MMMM")) },
                             { QStringLiteral("today"), date == QDate::currentDate() },
                             { QStringLiteral("kcal"), qRound(plan.dayKcal(d)) },
                             { QStringLiteral("meals"), meals } };
    }
    return { { QStringLiteral("person"), p->name },
             { QStringLiteral("start"), de.toString(plan.start, QStringLiteral("d. MMMM yyyy")) },
             { QStringLiteral("target"), qRound(plan.targetKcal) },
             { QStringLiteral("days"), days } };
}

int MobileApp::planToShopping()
{
    Profile *p = Profiles::instance().find(profileId());
    if (!p || p->lastPlan.isEmpty()) return 0;
    QStringList st = Fridge::stock();
    st << Fridge::basics();
    const QList<ShoppingItem> items = MealPlanner::shoppingList(MealPlan::fromJson(p->lastPlan), st);
    saveShopping(mergeShopping(loadShopping(), items));
    return int(items.size());
}

// ----------------------------------------------------------------------------------------------- Person, Tagebuch, Gewicht

QString MobileApp::profileId() const
{
    return QSettings().value(QStringLiteral("mobile/profileId")).toString();
}

void MobileApp::ensureProfile()
{
    Profiles &ps = Profiles::instance();
    if (ps.find(profileId())) return;
    if (ps.all().size() == 1) {
        QSettings().setValue(QStringLiteral("mobile/profileId"), ps.all().first().id);
        return;
    }
    if (!ps.all().isEmpty()) return;   // mehrere Personen vom PC: Auswahl auf der Seite „Ich“
    Profile p;
    p.name = tr("Ich");
    const QString id = ps.add(p);
    ps.save();
    QSettings().setValue(QStringLiteral("mobile/profileId"), id);
}

QVariantList MobileApp::profileChoices() const
{
    QVariantList out;
    for (const Profile &p : Profiles::instance().all())
        out << QVariantMap{ { QStringLiteral("id"), p.id }, { QStringLiteral("name"), p.name }, { QStringLiteral("current"), p.id == profileId() } };
    return out;
}

void MobileApp::chooseProfile(const QString &id)
{
    if (!Profiles::instance().find(id)) return;
    QSettings().setValue(QStringLiteral("mobile/profileId"), id);
    emit profileChanged();
    emit diaryChanged();
}

QVariantMap MobileApp::profile() const
{
    Profile *p = Profiles::instance().find(profileId());
    if (!p) return { { QStringLiteral("missing"), true } };
    QString hint;
    const double target = p->targetKcal(&hint);
    const double kg = p->currentKg();
    return { { QStringLiteral("id"), p->id },
             { QStringLiteral("name"), p->name },
             { QStringLiteral("female"), p->female },
             { QStringLiteral("birthYear"), p->birthYear },
             { QStringLiteral("heightCm"), p->heightCm },
             { QStringLiteral("pal"), p->pal },
             { QStringLiteral("goal"), int(p->goal) },
             { QStringLiteral("targetKg"), p->targetKg },
             { QStringLiteral("ratePerWeek"), p->ratePerWeek },
             { QStringLiteral("currentKg"), kg },
             { QStringLiteral("bmi"), kg > 0 ? fmt(p->bmi()) : QString() },
             { QStringLiteral("bmiClass"), kg > 0 ? Profile::bmiClass(p->bmi()) : QString() },
             { QStringLiteral("tdee"), kg > 0 ? qRound(p->tdee()) : 0 },
             { QStringLiteral("targetKcal"), kg > 0 ? qRound(target) : 0 },
             { QStringLiteral("hint"), hint },
             { QStringLiteral("hasPlan"), !p->lastPlan.isEmpty() } };
}

void MobileApp::saveProfile(const QVariantMap &d)
{
    ensureProfile();
    Profile *p = Profiles::instance().find(profileId());
    if (!p) return;
    Profile q = *p;
    q.name = d.value(QStringLiteral("name"), q.name).toString().trimmed();
    if (q.name.isEmpty()) q.name = tr("Ich");
    q.female = d.value(QStringLiteral("female"), q.female).toBool();
    q.birthYear = qBound(1900, d.value(QStringLiteral("birthYear"), q.birthYear).toInt(), QDate::currentDate().year());
    q.heightCm = qBound(80.0, d.value(QStringLiteral("heightCm"), q.heightCm).toDouble(), 250.0);
    q.pal = qBound(1.2, d.value(QStringLiteral("pal"), q.pal).toDouble(), 2.4);
    q.goal = Profile::Goal(qBound(0, d.value(QStringLiteral("goal"), int(q.goal)).toInt(), 2));
    q.targetKg = qMax(0.0, d.value(QStringLiteral("targetKg"), q.targetKg).toDouble());
    q.ratePerWeek = qBound(0.1, d.value(QStringLiteral("ratePerWeek"), q.ratePerWeek).toDouble(), 1.0);
    Profiles::instance().update(q);
    Profiles::instance().save();
    emit profileChanged();
}

QVariantList MobileApp::palLevels() const
{
    QVariantList out;
    for (const auto &l : Profile::palLevels()) out << QVariantMap{ { QStringLiteral("value"), l.first }, { QStringLiteral("text"), l.second } };
    return out;
}

QStringList MobileApp::mealNames() const
{
    return Diary::mealNames();
}

QString MobileApp::mealForNow() const
{
    return Diary::mealForTime(QTime::currentTime());
}

QVariantMap MobileApp::diaryDay(const QString &isoDate) const
{
    const QDate d = isoDate.isEmpty() ? QDate::currentDate() : QDate::fromString(isoDate, Qt::ISODate);
    Diary::Book book(profileId());
    QVariantList entries;
    for (const Diary::Entry &e : book.day(d))
        entries << QVariantMap{ { QStringLiteral("id"), e.id },
                                { QStringLiteral("time"), e.when.time().toString(QStringLiteral("HH:mm")) },
                                { QStringLiteral("meal"), e.meal },
                                { QStringLiteral("title"), e.title },
                                { QStringLiteral("recipeId"), e.recipeId },
                                { QStringLiteral("amount"), e.amountText() },
                                { QStringLiteral("kcal"), qRound(e.eaten().kcal) } };
    const Nutrients total = book.dayTotal(d);
    Profile *p = Profiles::instance().find(profileId());
    const int target = p && p->currentKg() > 0 ? qRound(p->targetKcal()) : 0;
    QLocale de(QLocale::German, QLocale::Germany);
    QVariantMap m = nutrientsMap(total);
    m.insert(QStringLiteral("date"), d.toString(Qt::ISODate));
    m.insert(QStringLiteral("label"), d == QDate::currentDate() ? tr("Heute") : de.toString(d, QStringLiteral("dddd, d. MMMM")));
    m.insert(QStringLiteral("entries"), entries);
    m.insert(QStringLiteral("target"), target);
    return m;
}

bool MobileApp::addDiaryRecipe(const QString &recipeId, double portions, const QString &meal, const QString &isoDate)
{
    ensureProfile();
    Recipe r;
    if (!RecipeDb::instance().loadLight(recipeId, &r)) return false;
    Diary::Book book(profileId());
    Diary::Entry e = Diary::Entry::fromRecipe(r, false);
    e.setPortions(portions > 0 ? portions : 1);
    e.meal = meal.isEmpty() ? mealForNow() : meal;
    const QDate d = isoDate.isEmpty() ? QDate::currentDate() : QDate::fromString(isoDate, Qt::ISODate);
    e.when = QDateTime(d, d == QDate::currentDate() ? QTime::currentTime() : Diary::defaultTime(e.meal));
    book.add(e);
    const bool ok = book.save();
    emit diaryChanged();
    return ok;
}

bool MobileApp::addDiaryFree(const QString &title, double kcal, const QString &meal, const QString &isoDate)
{
    ensureProfile();
    if (title.trimmed().isEmpty()) return false;
    Diary::Book book(profileId());
    Diary::Entry e;
    e.title = title.trimmed();
    e.meal = meal.isEmpty() ? mealForNow() : meal;
    e.perServing.kcal = qMax(0.0, kcal);
    e.setPortions(1);
    const QDate d = isoDate.isEmpty() ? QDate::currentDate() : QDate::fromString(isoDate, Qt::ISODate);
    e.when = QDateTime(d, d == QDate::currentDate() ? QTime::currentTime() : Diary::defaultTime(e.meal));
    book.add(e);
    const bool ok = book.save();
    emit diaryChanged();
    return ok;
}

void MobileApp::removeDiaryEntry(const QString &id)
{
    Diary::Book book(profileId());
    book.remove(id);
    book.save();
    emit diaryChanged();
}

QVariantList MobileApp::weights() const
{
    QVariantList out;
    Profile *p = Profiles::instance().find(profileId());
    if (!p) return out;
    QLocale de(QLocale::German, QLocale::Germany);
    for (int i = int(p->weights.size()) - 1; i >= 0; --i) {
        const WeightEntry &w = p->weights.at(i);
        out << QVariantMap{ { QStringLiteral("date"), w.date.toString(Qt::ISODate) },
                            { QStringLiteral("label"), de.toString(w.date, QStringLiteral("ddd, d. MMM yyyy")) },
                            { QStringLiteral("kg"), w.kg },
                            { QStringLiteral("text"), fmt(w.kg) + QStringLiteral(" kg") } };
    }
    return out;
}

void MobileApp::addWeight(const QString &isoDate, double kg)
{
    ensureProfile();
    Profile *p = Profiles::instance().find(profileId());
    if (!p || kg < 20 || kg > 400) return;
    Profile q = *p;
    q.addWeight(isoDate.isEmpty() ? QDate::currentDate() : QDate::fromString(isoDate, Qt::ISODate), kg);
    Profiles::instance().update(q);
    Profiles::instance().save();
    emit profileChanged();
}

void MobileApp::removeWeight(const QString &isoDate)
{
    Profile *p = Profiles::instance().find(profileId());
    if (!p) return;
    Profile q = *p;
    const QDate d = QDate::fromString(isoDate, Qt::ISODate);
    for (int i = int(q.weights.size()) - 1; i >= 0; --i)
        if (q.weights.at(i).date == d) q.weights.removeAt(i);
    Profiles::instance().update(q);
    Profiles::instance().save();
    emit profileChanged();
}

// ----------------------------------------------------------------------------------------------- Glossar

QVariantList MobileApp::glossary(const QString &query) const
{
    QVariantList out;
    const auto &all = KitchenGlossary::entries();
    const QList<int> hits = KitchenGlossary::search(query);
    for (int i : hits) {
        if (out.size() >= 200) break;
        const auto &e = all.at(i);
        out << QVariantMap{ { QStringLiteral("term"), e.term },
                            { QStringLiteral("category"), e.category },
                            { QStringLiteral("text"), e.text },
                            { QStringLiteral("seeAlso"), e.seeAlso.join(QStringLiteral(", ")) } };
    }
    return out;
}

// ----------------------------------------------------------------------------------------------- Import

void MobileApp::importFile(const QUrl &url)
{
    if (m_busy) return;
    const QString path = localPath(url);
    setBusy(true, tr("Lese Datei …"));
    struct Res {
        bool ok = false;
        QString error;
        QList<Recipe> recipes;
        RecipePackage::Content content;
        bool package = false;
    };
    runInThread<Res>(
        this,
        [path]() {
            Res r;
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly)) {
                r.error = tr("Die Datei kann nicht geöffnet werden.");
                return r;
            }
            const QByteArray data = f.readAll();
            if (data.startsWith("PK")) {
                r.package = true;
                r.ok = RecipePackage::parse(data, &r.content, &r.error);
                r.recipes = r.content.recipes;
            } else {
                const QJsonObject o = QJsonDocument::fromJson(data).object();
                if (o.value(QStringLiteral("format")).toString().endsWith(QLatin1String("-Recipe"))) {
                    r.recipes << Recipe::fromJson(o);
                    r.ok = true;
                } else {
                    r.error = tr("Das ist weder ein ViviaMeals-Rezeptpaket (.vmpack) noch ein ViviaMeals-Rezept (.vmr).");
                }
            }
            return r;
        },
        [this](const Res &r) {
            if (!r.ok) {
                setBusy(false);
                emit importFinished(false, r.error);
                return;
            }
            RecipeDb &db = RecipeDb::instance();
            int added = 0, skipped = 0;
            db.beginBatch();
            for (Recipe rec : r.recipes) {
                if (rec.title.trimmed().isEmpty()) continue;
                if ((!rec.id.isEmpty() && db.summary(rec.id)) || !db.duplicatesOf(rec).isEmpty()) {
                    ++skipped;
                    continue;
                }
                if (rec.id.isEmpty()) rec.id = Recipe::newId();
                if (!rec.nutrition.valid()) Nutrition::analyze(rec);
                if (db.save(rec)) ++added;
            }
            db.endBatch();
            int foods = 0;
            if (r.package) foods = RecipePackage::importFoods(r.content);
            m_recipes->refresh();
            setBusy(false);
            QString text = tr("%n Rezept(e) übernommen.", nullptr, added);
            if (skipped) text += QLatin1Char(' ') + tr("%n schon vorhanden und übersprungen.", nullptr, skipped);
            if (foods) text += QLatin1Char(' ') + tr("%n eigene Lebensmittel übernommen.", nullptr, foods);
            emit databaseChanged();
            emit importFinished(true, text);
        });
}

// ----------------------------------------------------------------------------------------------- Abgleich

QVariantMap MobileApp::syncConfig() const
{
    const Sync::Connection c = Sync::Connection::load(databaseFolder());
    QString kind;
    switch (c.kind) {
    case Sync::Connection::Kind::Website: kind = QStringLiteral("website"); break;
    case Sync::Connection::Kind::Lan: kind = QStringLiteral("lan"); break;
    default: kind = QStringLiteral("none"); break;
    }
    return { { QStringLiteral("kind"), kind },
             { QStringLiteral("url"), c.url },
             { QStringLiteral("device"), c.deviceLabel },
             { QStringLiteral("auto"), c.autoSync },
             { QStringLiteral("deviceName"), Sync::deviceName() },
             { QStringLiteral("last"), m_lastSync } };
}

bool MobileApp::syncConfigured() const
{
    const Sync::Connection c = Sync::Connection::load(databaseFolder());
    return c.valid() && c.kind != Sync::Connection::Kind::Host;
}

QString MobileApp::syncStatus() const
{
    if (!syncConfigured()) return tr("Nicht eingerichtet");
    const QVariantMap c = syncConfig();
    const QString where = c.value(QStringLiteral("kind")).toString() == QLatin1String("website") ? tr("Website") : tr("Heimnetz");
    return m_lastSync.isEmpty() ? where : tr("%1 · %2").arg(where, m_lastSync);
}

QString MobileApp::setSyncWebsite(const QString &url, const QString &key)
{
    QString u = url.trimmed();
    if (!u.startsWith(QLatin1String("http"))) u.prepend(QStringLiteral("https://"));
    if (!u.contains(QLatin1String(".php"))) {
        if (!u.endsWith(QLatin1Char('/'))) u += QLatin1Char('/');
        u += QStringLiteral("viviameals-sync.php");
    }
    if (key.trimmed().isEmpty()) return tr("Bitte den Zugangsschlüssel eintragen.");
    Sync::Connection c;
    c.kind = Sync::Connection::Kind::Website;
    c.url = u;
    c.token = key.trimmed();
    c.autoSync = true;
    if (!c.save(databaseFolder())) return tr("Die Einstellung konnte nicht gespeichert werden.");
    emit syncConfigChanged();
    return {};
}

QString MobileApp::setSyncInvitation(const QString &code)
{
    QString name;
    Sync::Connection c = Sync::Connection::fromInvitation(code.trimmed(), &name);
    if (!c.valid()) return tr("Das ist kein gültiger Einladungscode. Er beginnt mit „vmsync1:“ und steht am PC unter Datei → Abgleich → Abgleich einrichten … → „Einladungscode kopieren“.");
    if (c.kind == Sync::Connection::Kind::Lan && c.deviceLabel.isEmpty()) c.deviceLabel = name;
    c.autoSync = true;
    if (!c.save(databaseFolder())) return tr("Die Einstellung konnte nicht gespeichert werden.");
    emit syncConfigChanged();
    return {};
}

void MobileApp::discoverLan()
{
    runInThread<QList<Sync::Found>>(
        this, []() { return Sync::discover(2000); },
        [this](const QList<Sync::Found> &found) {
            QVariantList out;
            for (const Sync::Found &f : found)
                out << QVariantMap{ { QStringLiteral("name"), f.name }, { QStringLiteral("device"), f.device }, { QStringLiteral("url"), f.url } };
            emit lanFound(out);
        });
}

void MobileApp::pairLan(const QString &url, const QString &code, const QString &name)
{
    struct Res {
        bool ok = false;
        QString token, error;
    };
    const QString u = url.trimmed();
    const QString cd = code.trimmed();
    setBusy(true, tr("Koppeln …"));
    runInThread<Res>(
        this,
        [u, cd]() {
            Res r;
            r.ok = Sync::HttpTransport::pair(u, cd, &r.token, &r.error);
            return r;
        },
        [this, u, name](const Res &r) {
            setBusy(false);
            if (!r.ok) {
                emit lanPaired(false, r.error.isEmpty() ? tr("Koppeln fehlgeschlagen.") : r.error);
                return;
            }
            Sync::Connection c;
            c.kind = Sync::Connection::Kind::Lan;
            c.url = u;
            c.token = r.token;
            c.deviceLabel = name;
            c.autoSync = true;
            c.save(databaseFolder());
            emit syncConfigChanged();
            emit lanPaired(true, tr("Mit „%1“ gekoppelt.").arg(name.isEmpty() ? u : name));
        });
}

void MobileApp::removeSync()
{
    Sync::Connection c;
    c.save(databaseFolder());
    m_lastSync.clear();
    emit syncConfigChanged();
}

void MobileApp::setSyncAuto(bool on)
{
    Sync::Connection c = Sync::Connection::load(databaseFolder());
    c.autoSync = on;
    c.save(databaseFolder());
    emit syncConfigChanged();
}

void MobileApp::autoSync()
{
    const Sync::Connection c = Sync::Connection::load(databaseFolder());
    if (c.valid() && c.autoSync && c.kind != Sync::Connection::Kind::Host && !m_busy) syncNow(false);
}

void MobileApp::syncNow(bool removeSamples)
{
    if (m_busy) return;
    const Sync::Connection c = Sync::Connection::load(databaseFolder());
    if (!c.valid()) {
        emit syncFinished(false, tr("Der Abgleich ist noch nicht eingerichtet."));
        return;
    }
    if (removeSamples) {
        // Beispielrezepte nicht in die eigene Sammlung hochladen
        RecipeDb &db = RecipeDb::instance();
        QStringList ids;
        for (const RecipeSummary &s : db.summaries()) {
            Recipe r;
            if (db.loadLight(s.id, &r) && r.source == sampleSource() && !r.favorite) ids << s.id;
        }
        db.beginBatch();
        for (const QString &id : ids) db.remove(id);
        db.endBatch();
    }
    setBusy(true, tr("Abgleich …"));
    const QString folder = databaseFolder();
    QPointer<MobileApp> self(this);
    runInThread<Sync::Result>(
        this,
        [c, folder, self]() {
            std::unique_ptr<Sync::Transport> t(Sync::makeTransport(c, folder));
            Sync::Result res;
            if (!t) {
                res.error = tr("Keine Verbindung eingerichtet.");
                return res;
            }
            Sync::Engine engine(t.get(), folder);
            res = engine.run([self](int done, int total, const QString &text) {
                if (self)
                    QMetaObject::invokeMethod(self, [self, done, total, text]() {
                        if (self) self->setBusy(true, text, done, total);
                    }, Qt::QueuedConnection);
                return true;
            });
            return res;
        },
        [this](const Sync::Result &res) {
            setBusy(false);
            if (res.ok) m_lastSync = tr("abgeglichen um %1").arg(QTime::currentTime().toString(QStringLiteral("HH:mm")));
            if (res.dbChanged) reopenDatabase();
            if (res.appChanged) {
                Profiles::instance().reload();
                FoodTable::instance().reload();
                ensureProfile();
                emit profileChanged();
                emit diaryChanged();
            }
            emit syncConfigChanged();
            emit syncFinished(res.ok, res.summary());
        });
}

// ----------------------------------------------------------------------------------------------- Selbsttest

int MobileApp::selfTest(QTextStream &out)
{
    int fails = 0;
    auto check = [&](bool ok, const QString &what) {
        out << (ok ? "  ok   " : "  FEHL ") << what << "\n";
        if (!ok) ++fails;
    };
    RecipeDb &db = RecipeDb::instance();
    out << "=== ViviaMeals Mobil – Selbsttest " << version() << "\n";
    check(db.isOpen() && db.count() >= 90, QStringLiteral("Datenbank mit Beispielrezepten: %1").arg(db.count()));
    check(sampleCount() == db.count(), QStringLiteral("alle als Beispielrezepte markiert"));
    int withNutrition = 0, lowCoverage = 0;
    QStringList low;
    for (const RecipeSummary &s : db.summaries()) {
        if (s.hasNutrition && s.perServing.kcal > 0) ++withNutrition;
        if (s.coverage < 0.85) {
            ++lowCoverage;
            if (low.size() < 12) low << QStringLiteral("%1 (%2 %)").arg(s.title).arg(qRound(s.coverage * 100));
        }
    }
    check(withNutrition == db.count(), QStringLiteral("alle mit Nährwerten: %1").arg(withNutrition));
    check(lowCoverage == 0, QStringLiteral("Zutaten zugeordnet (≥ 85 %): %1 darunter %2").arg(lowCoverage).arg(low.join(QStringLiteral(", "))));
    const QVariantList th = themes();
    check(th.size() > 8, QStringLiteral("Themen: %1").arg(th.size()));
    m_recipes->setQuery(QStringLiteral("kartoffel"));
    check(m_recipes->count() > 0 && m_recipes->count() < db.count(), QStringLiteral("Suche „kartoffel“: %1").arg(m_recipes->count()));
    m_recipes->setQuery(QString());
    m_recipes->setTheme(QStringLiteral("vegan"));
    check(m_recipes->count() >= 10, QStringLiteral("vegane Rezepte: %1").arg(m_recipes->count()));
    m_recipes->setTheme(QString());
    const QString first = m_recipes->idAt(0);
    QVariantMap r = recipe(first);
    check(!r.value(QStringLiteral("title")).toString().isEmpty() && r.value(QStringLiteral("ingredients")).toList().size() > 2, QStringLiteral("Rezeptansicht: %1").arg(r.value(QStringLiteral("title")).toString()));
    QVariantMap r2 = recipe(first, r.value(QStringLiteral("baseServings")).toDouble() * 2);
    check(r2.value(QStringLiteral("ingredients")).toList() != r.value(QStringLiteral("ingredients")).toList(), QStringLiteral("Portionen umrechnen"));
    // neues Rezept
    const QString id = saveRecipe({ { QStringLiteral("title"), QStringLiteral("Testsuppe") },
                                   { QStringLiteral("servings"), 2 },
                                   { QStringLiteral("ingredients"), QStringLiteral("500 g Kartoffeln\n1 Zwiebel\n750 ml Gemüsebrühe\n1 EL Olivenöl") },
                                   { QStringLiteral("instructions"), QStringLiteral("Alles kochen und **pürieren**.") },
                                   { QStringLiteral("tags"), QStringLiteral("Suppen, schnell") } });
    QVariantMap t = recipe(id);
    check(!id.isEmpty() && t.value(QStringLiteral("nutrition")).toMap().value(QStringLiteral("kcal")).toInt() > 100,
          QStringLiteral("neues Rezept mit Nährwerten: %1 kcal").arg(t.value(QStringLiteral("nutrition")).toMap().value(QStringLiteral("kcal")).toInt()));
    check(t.value(QStringLiteral("instructionsHtml")).toString().contains(QStringLiteral("<b>")), QStringLiteral("Auszeichnung in der Zubereitung"));
    bool tagged = false;
    for (const QVariant &v : t.value(QStringLiteral("tags")).toList()) tagged |= v.toMap().value(QStringLiteral("tag")).toString() == QLatin1String("vegan");
    check(tagged, QStringLiteral("Tag vegan ergänzt"));
    check(toggleFavorite(id) && recipe(id).value(QStringLiteral("favorite")).toBool(), QStringLiteral("Favorit"));
    // Einkaufsliste
    clearShopping();
    check(addRecipeToShopping(id, 4, false) >= 3 && shoppingOpen() >= 3, QStringLiteral("Einkaufsliste aus Rezept: %1").arg(shoppingOpen()));
    addRecipeToShopping(id, 2, false);
    {
        QStringList texts;
        for (const QVariant &v : shoppingItems()) texts << v.toMap().value(QStringLiteral("text")).toString();
        check(texts.size() == 4 || texts.size() == 3, QStringLiteral("gleiche Zutaten zusammengefasst: %1").arg(texts.join(QStringLiteral(" | "))));
    }
    setShoppingChecked(0, true);
    check(removeCheckedShopping() == 1, QStringLiteral("abgehakte entfernen"));
    // Kühlschrank
    setStock({ QStringLiteral("Kartoffeln"), QStringLiteral("Zwiebeln"), QStringLiteral("Eier"), QStringLiteral("Möhren") });
    const QVariantList sug = fridgeSuggestions(3);
    check(!sug.isEmpty(), QStringLiteral("Kühlschrank-Vorschläge: %1").arg(sug.size()));
    // Person, Tagebuch, Gewicht
    saveProfile({ { QStringLiteral("name"), QStringLiteral("Erika") }, { QStringLiteral("female"), true }, { QStringLiteral("birthYear"), 1975 },
                  { QStringLiteral("heightCm"), 168 }, { QStringLiteral("goal"), 0 } });
    addWeight(QDate::currentDate().toString(Qt::ISODate), 72.5);
    check(profile().value(QStringLiteral("targetKcal")).toInt() > 1200 && weights().size() == 1, QStringLiteral("Person und Gewicht: Ziel %1 kcal").arg(profile().value(QStringLiteral("targetKcal")).toInt()));
    check(addDiaryRecipe(id, 1.5, QStringLiteral("Mittagessen"), QString()) && addDiaryFree(QStringLiteral("Apfel"), 80, QString(), QString()), QStringLiteral("Tagebuch-Einträge"));
    const QVariantMap day = diaryDay(QString());
    check(day.value(QStringLiteral("entries")).toList().size() == 2 && day.value(QStringLiteral("kcal")).toInt() > 80, QStringLiteral("Tagessumme: %1 kcal").arg(day.value(QStringLiteral("kcal")).toInt()));
    // Glossar
    check(!glossary(QStringLiteral("blanchieren")).isEmpty(), QStringLiteral("Glossar"));
    // Rezeptpaket schreiben und wieder einlesen (Doubletten werden übersprungen)
    QList<Recipe> pack;
    Recipe rr;
    db.load(id, &rr);
    rr.id = Recipe::newId();
    rr.title = QStringLiteral("Importierte Suppe");
    pack << rr;
    Recipe dup;
    db.load(first, &dup);
    pack << dup;
    const QString file = QDir(appDataDir()).filePath(QStringLiteral("test.vmpack"));
    RecipePackage::Options po;
    po.name = QStringLiteral("Test");
    RecipePackage::write(file, pack, po);
    bool done = false, ok = false;
    QString text;
    QMetaObject::Connection conn = connect(this, &MobileApp::importFinished, this, [&](bool o, const QString &t2) {
        done = true;
        ok = o;
        text = t2;
    });
    const int before = db.count();
    importFile(QUrl::fromLocalFile(file));
    QElapsedTimer timer;
    timer.start();
    while (!done && timer.elapsed() < 10000) QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    disconnect(conn);
    check(ok && db.count() == before + 1, QStringLiteral("Rezeptpaket importieren: %1").arg(text));
    check(deleteRecipe(id), QStringLiteral("Rezept löschen"));
    out << (fails ? QStringLiteral("%1 Fehler\n").arg(fails) : QStringLiteral("Alle Prüfungen bestanden\n"));
    return fails ? 1 : 0;
}
