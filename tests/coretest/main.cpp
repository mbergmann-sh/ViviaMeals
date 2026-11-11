// ViviaMeals – Test des Kerns (ohne Fenster)
//   coretest            alle Prüfungen, Ausgabe der importierten Beispielrezepte
#include "foodcsv.h"
#include "foodtable.h"
#include "importers.h"
#include "latexexport.h"
#include "ingredientparser.h"
#include "nutrition.h"
#include "recipedb.h"
#include "textcodec.h"
#include "units.h"
#include "recipehtml.h"
#include "umlauts.h"
#include "mealplanner.h"
#include "coaching.h"
#include "diary.h"
#include "odtexport.h"
#include "mealtypes.h"
#include "deepl.h"
#include "glossary.h"
#include "ocr.h"
#include "recipescan.h"
#include "escl.h"
#include "duplicates.h"
#include "recipepackage.h"
#include "openfoodfacts.h"

#include <QGuiApplication>
#include <QPainter>
#include <QFont>
#include <QRandomGenerator>
#include <QBuffer>
#include <QImage>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QElapsedTimer>
#include <QTimer>
#include <QEventLoop>
#include <QTcpSocket>
#include <QProcess>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <cmath>
#include <memory>

static int g_fail = 0;
static QTextStream out(stdout);

#define CHECK(cond, msg)                                                         \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++g_fail;                                                            \
            out << "FEHLER: " << msg << "  [" #cond "]\n";                       \
        }                                                                        \
    } while (0)

static void line(const QString &l, const IngredientParser::Options &o, double amount, const QString &unit, const QString &name)
{
    const Ingredient i = IngredientParser::parseLine(l, o);
    out << QStringLiteral("  %1  →  [%2|%3|%4|%5] food=%6\n").arg(l, Fmt::number(i.amount, 2), i.unit, i.name, i.note,
                                                              FoodTable::instance().match(i.name) ? FoodTable::instance().match(i.name)->key : QStringLiteral("-"));
    CHECK(std::fabs(i.amount - amount) < 0.6, l + " Menge " + QString::number(i.amount));
    CHECK(i.unit == unit, l + " Einheit " + i.unit);
    if (!name.isEmpty()) CHECK(i.name == name, l + " Name " + i.name);
}

int main(int argc, char **argv)
{
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ViviaMealsTest"));
    // coretest --scan <bild>: Fotoerkennung, Texterkennung, Aufteilung anzeigen
    if (argc >= 3 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--scan")) {
        QImage img = RecipeScan::normalize(QImage(QString::fromLocal8Bit(argv[2])));
        const QList<QRect> photos = RecipeScan::findPhotos(img);
        for (const QRect &r : photos) out << "Foto: " << r.x() << "," << r.y() << " " << r.width() << "x" << r.height() << "\n";
        QList<Ocr::Line> lines;
        QString err;
        if (!Ocr::recognize(RecipeScan::blank(img, photos), Ocr::findTesseract(), QStringLiteral("deu+eng"), &lines, &err)) {
            out << err << "\n";
            return 1;
        }
        out << "--- Text ---\n" << Ocr::plainText(lines) << "\n";
        const RecipeScan::Parts p = RecipeScan::split(lines);
        out << "--- Titel: " << p.text.title << "\n--- Portionen: " << p.text.yield << "  Zeit: " << p.prepMinutes
            << "\n--- Zutaten:\n  " << p.text.ingredientLines.join(QStringLiteral("\n  ")) << "\n--- Zubereitung:\n"
            << p.text.directionLines.join(QStringLiteral("\n")) << "\n--- nicht zugeordnet:\n  " << p.unassigned.join(QStringLiteral("\n  ")) << "\n";
        const Recipe r = RecipeScan::toRecipe(p);
        out << "--- Rezept: " << r.title << " / " << r.servingsText() << " / " << qRound(r.nutrition.perServing.kcal) << " kcal je Portion / Tags: "
            << r.tags.join(QStringLiteral(", ")) << "\n";
        return 0;
    }
    // coretest --unmatched <dateien…>: Zutaten ohne Nährwert-Zuordnung zählen (TSV: Anzahl, Name, Beispiele)
    if (argc >= 3 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--unmatched")) {
        QHash<QString, int> count;
        QHash<QString, QStringList> examples;
        int recipes = 0, lines = 0, unmatched = 0;
        for (int a = 2; a < argc; ++a) {
            for (const Recipe &r : Importers::importFile(QString::fromLocal8Bit(argv[a]), Importers::Format::Unknown).recipes) {
                if (Nutrition::isMenu(r)) continue;   // Menüfolgen: „Zutaten“ sind Gerichte
                ++recipes;
                for (const Ingredient &i : r.ingredients) {
                    if (i.heading) continue;
                    ++lines;
                    if (!i.food.isEmpty()) continue;
                    ++unmatched;
                    const QString key = FoodTable::normalize(i.name);
                    count[key]++;
                    if (examples[key].size() < 3 && !examples[key].contains(i.displayLine())) examples[key] << i.displayLine();
                }
            }
        }
        QStringList keys = count.keys();
        std::sort(keys.begin(), keys.end(), [&](const QString &x, const QString &y) { return count[x] != count[y] ? count[x] > count[y] : x < y; });
        out << "# " << recipes << " Rezepte, " << lines << " Zutatenzeilen, " << unmatched << " ohne Zuordnung, " << keys.size() << " verschiedene\n";
        for (const QString &k : keys) out << count[k] << "\t" << k << "\t" << examples[k].join(QStringLiteral(" | ")) << "\n";
        return 0;
    }
    // coretest --dups <rezeptdatei>: Sammlung in eine Testdatenbank laden und Doubletten anzeigen
    if (argc >= 3 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--dups")) {
        QTemporaryDir tmp;
        RecipeDb &db = RecipeDb::instance();
        db.create(tmp.filePath(QStringLiteral("d")), QStringLiteral("d"));
        db.beginBatch();
        for (Recipe r : Importers::importFile(QString::fromLocal8Bit(argv[2]), Importers::Format::Unknown).recipes) db.save(r);
        db.endBatch();
        QElapsedTimer t;
        t.start();
        for (auto st : { Duplicates::Strictness::TitleOnly, Duplicates::Strictness::Normal, Duplicates::Strictness::Generous }) {
            const QList<Duplicates::Group> g = Duplicates::find(db.summaries(), {}, st);
            out << "Stufe " << int(st) << ": " << g.size() << " Gruppen bei " << db.count() << " Rezepten (" << t.restart() << " ms)\n";
            if (st == Duplicates::Strictness::TitleOnly) continue;
            for (const Duplicates::Group &x : g) {
                if (x.reasons == Duplicates::SameTitle) continue;
                QStringList titles;
                for (const QString &id : x.ids) titles << db.summary(id)->title;
                out << "  [" << Duplicates::reasonText(x.reasons) << " " << qRound(x.score * 100) << "%] " << titles.join(QStringLiteral(" | ")) << "\n";
            }
        }
        return 0;
    }
    // coretest --odt <rezeptdatei> <ordner>: alle Rezepte als .odt (mit Testbild) schreiben
    if (argc >= 4 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--odt")) {
        const auto res = Importers::importFile(QString::fromLocal8Bit(argv[2]), Importers::Format::Unknown);
        QImage img(800, 600, QImage::Format_RGB32);
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) img.setPixel(x, y, qRgb(60 + x / 5, 120 + y / 6, 200 - x / 8));
        QByteArray png;
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
        int n = 0;
        for (Recipe r : res.recipes) {
            if (n % 2 == 0) r.image = QString::fromLatin1(png.toBase64());
            QString err;
            const QString f = QDir(QString::fromLocal8Bit(argv[3])).filePath(QStringLiteral("rezept-%1.odt").arg(++n));
            if (!OdtExport::write(r, f, {}, &err)) out << "FEHLER " << err << "\n";
            else out << f << "\n";
        }
        return 0;
    }
    // coretest --dump <datei>: Import anzeigen (Titel, Zutaten mit Zuordnung, Anleitung)
    if (argc >= 3 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--dump")) {
        const auto res = Importers::importFile(QString::fromLocal8Bit(argv[2]), Importers::Format::Unknown);
        for (const Recipe &r : res.recipes) {
            out << "### " << r.title << "  [" << r.language << "] " << r.servingsText() << " " << r.tags.join(QLatin1Char(','))
                << QStringLiteral("  | %1 kcal, %2 g/Portion %3").arg(qRound(r.nutrition.perServing.kcal)).arg(qRound(MealTypes::servingGrams(r)))
                       .arg(MealTypes::healthNotes(MealTypes::from(r)).join(QLatin1Char(','))) << "\n";
            for (const Ingredient &i : r.ingredients)
                out << "  " << (i.heading ? QStringLiteral("== ") : QString()) << i.displayLine() << "   -> " << (i.food.isEmpty() ? QStringLiteral("??") : i.food) << "\n";
            if (!r.source.isEmpty()) out << "  [Quelle] " << r.source << "\n";
            out << r.instructions << "\n\n";
        }
        for (const QString &w : res.warnings) out << "WARN " << w << "\n";
        return 0;
    }
    out << "Lebensmittel: " << FoodTable::instance().all().size() << "\n";

    IngredientParser::Options de, en;
    en.translate = true;
    out << "Zutatenzeilen:\n";
    line(QStringLiteral("200 g Mehl"), de, 200, QStringLiteral("g"), QStringLiteral("Mehl"));
    line(QStringLiteral("1 1/2 EL Olivenöl"), de, 1.5, QStringLiteral("EL"), QStringLiteral("Olivenöl"));
    line(QStringLiteral("2-3 Zehen Knoblauch, fein gehackt"), de, 2, QStringLiteral("Zehe"), QStringLiteral("Knoblauch"));
    line(QStringLiteral("½ TL Salz"), de, 0.5, QStringLiteral("TL"), QStringLiteral("Salz"));
    line(QStringLiteral("4 Eier"), de, 4, QString(), QStringLiteral("Eier"));
    line(QStringLiteral("2 gr. Zwiebeln"), de, 2, QString(), QStringLiteral("Zwiebeln"));
    line(QStringLiteral("1 c all-purpose flour, sifted"), en, 130, QStringLiteral("g"), QStringLiteral("Weizenmehl"));
    line(QStringLiteral("2 cups milk"), en, 475, QStringLiteral("ml"), QStringLiteral("Milch"));
    line(QStringLiteral("1 lb ground beef"), en, 455, QStringLiteral("g"), QStringLiteral("Rinderhackfleisch"));
    line(QStringLiteral("2 T butter"), en, 2, QStringLiteral("EL"), QStringLiteral("Butter"));
    line(QStringLiteral("1 t salt"), en, 1, QStringLiteral("TL"), QStringLiteral("Salz"));
    line(QStringLiteral("1 (16 oz) can diced tomatoes"), en, 455, QStringLiteral("g"), QString());
    line(QStringLiteral("3 large eggs"), en, 3, QString(), QStringLiteral("Eier"));
    CHECK(IngredientParser::isHeading(QStringLiteral("== Für den Teig ==")), "Überschrift");
    CHECK(IngredientParser::isHeading(QStringLiteral("Für die Soße:")), "Überschrift mit Doppelpunkt");

    out << "Text: " << Units::convertTextMeasures(QStringLiteral("Bake at 350 degrees F in a 9x13 inch pan, cut into 1-inch cubes.")) << "\n";

    // Nährwerte eines einfachen Rezepts
    Recipe r;
    r.title = QStringLiteral("Test");
    r.servings = 2;
    r.ingredients = IngredientParser::parseText(QStringLiteral("100 g Zucker\n100 g Butter\n2 Eier\nSalz"));
    Nutrition::analyze(r);
    out << "Nährwerte Test: " << r.nutrition.total.kcal << " kcal, je Portion " << r.nutrition.perServing.kcal << "\n";
    CHECK(std::fabs(r.nutrition.total.kcal - (400 + 740 + 2 * 58 * 1.37)) < 5, "kcal Testrezept");
    CHECK(r.nutrition.matched == 3 && r.nutrition.countable == 3, "Zählung");

    // Importe
    for (const QString &file : { QStringLiteral(":/samples/beispiele.rk"), QStringLiteral(":/samples/beispiele.mmf") }) {
        QFile f(file);
        f.open(QIODevice::ReadOnly);
        const QString text = TextCodec::decode(f.readAll());
        const Importers::Format fmt = Importers::detect(text);
        const Importers::Result res = fmt == Importers::Format::RezKonv ? Importers::parseRezKonv(text) : Importers::parseMealMaster(text);
        out << "\n=== " << file << " (" << Importers::formatName(fmt) << "): " << res.recipes.size() << " Rezepte\n";
        for (const QString &w : res.warnings) out << "  Warnung: " << w << "\n";
        for (const Recipe &rec : res.recipes) {
            out << "--- " << rec.title << " | " << rec.servingsText() << " | Sprache " << rec.language << " | Tags " << rec.tags.join(", ")
                << "\n";
            for (const Ingredient &i : rec.ingredients)
                out << "    " << (i.heading ? "## " : "") << i.displayLine() << "   {" << i.food << " " << Fmt::number(i.grams, 0) << " g}\n";
            out << "    Quelle: " << rec.source << "\n";
            out << "    Nährwerte/Portion: " << Fmt::kcal(rec.nutrition.perServing.kcal) << " kcal, E " << Fmt::number(rec.nutrition.perServing.protein)
                << " F " << Fmt::number(rec.nutrition.perServing.fat) << " KH " << Fmt::number(rec.nutrition.perServing.carbs)
                << " | zugeordnet " << rec.nutrition.matched << "/" << rec.nutrition.countable << " ohne: " << rec.nutrition.unmatched.join(", ") << "\n";
            out << "    Anleitung: " << rec.instructions.left(160).replace('\n', " / ") << " …\n";
        }
        CHECK(res.recipes.size() >= 3, "Anzahl Rezepte " + file);
        // LaTeX-Export (Prüfung der Übersetzbarkeit: siehe tests/README bzw. pdflatex)
        if (argc > 1)
            for (int k = 0; k < res.recipes.size(); ++k)
                for (int cls = 0; cls < 2; ++cls) {
                    LatexExport::Options lo;
                    lo.docClass = cls ? LatexExport::DocClass::Book : LatexExport::DocClass::ScrBook;
                    const QString f = QStringLiteral("%1/%2_%3_%4.tex").arg(QString::fromLocal8Bit(argv[1])).arg(QFileInfo(file).completeBaseName()).arg(k).arg(cls ? "book" : "scrbook");
                    CHECK(LatexExport::exportRecipe(res.recipes[k], f, lo), "LaTeX " + f);
                }
    }

    // ---- CSV-Import eigener Nährwerte
    {
        QTemporaryDir ct;
        auto writeBytes = [&](const QString &name, const QByteArray &b) {
            QFile f(ct.filePath(name));
            f.open(QIODevice::WriteOnly);
            f.write(b);
            return ct.filePath(name);
        };
        // 1. deutsche Excel-Tabelle: Semikolon, Dezimalkomma, Windows-1252, Feld in Anführungszeichen
        QByteArray de = "Bezeichnung;Kategorie;Brennwert (kJ);Energie (kcal);Fett (g);davon ges\xe4ttigte Fetts\xe4uren (g);"
                        "Kohlenhydrate (g);davon Zucker (g);Ballaststoffe (g);Eiwei\xdf (g);Salz (g)\r\n"
                        "Dinkelvollkornmehl;Getreide;1402;335;2,5;0,4;61;1;10;14;0,01\r\n"
                        "\"Gr\xfcnkern; geschrotet\";;1440;344;2,7;0,5;63;1,5;8,8;11;<0,1\r\n"
                        "Hafer-Joghurt natur;Milch;259;;2,8;0,3;7,6;2,4;1,2;1,2;0,1\r\n"
                        ";;;;;;;;;;\r\n";
        FoodCsv::Table t;
        QString err;
        CHECK(FoodCsv::readTable(writeBytes(QStringLiteral("de.csv"), de), &t, &err), "CSV lesen: " + err);
        CHECK(t.delimiter == QLatin1Char(';') && t.encoding == QLatin1String("Windows-1252"), "Trennzeichen/Zeichensatz " + t.encoding);
        const FoodCsv::Mapping m = FoodCsv::autoMap(t.header);
        QStringList map;
        for (int f = 0; f < FoodCsv::FieldCount; ++f)
            map << QStringLiteral("%1=%2").arg(FoodCsv::fieldName(FoodCsv::Field(f))).arg(m.col[f] >= 0 ? t.header[m.col[f]] : QStringLiteral("-"));
        out << "CSV-Zuordnung: " << map.join(" | ") << "\n";
        CHECK(m.col[FoodCsv::Name] == 0 && m.col[FoodCsv::KJ] == 2 && m.col[FoodCsv::Kcal] == 3 && m.col[FoodCsv::SatFat] == 5
                  && m.col[FoodCsv::Fat] == 4 && m.col[FoodCsv::Sugar] == 7 && m.col[FoodCsv::Carbs] == 6 && m.col[FoodCsv::Protein] == 9
                  && m.col[FoodCsv::Salt] == 10 && m.col[FoodCsv::Category] == 1,
              "automatische Zuordnung (deutsch)");
        FoodCsv::Options o;
        o.source = QStringLiteral("Test-CSV");
        QStringList warn;
        const QList<Food> foods = FoodCsv::convert(t, m, o, &warn);
        for (const Food &f : foods)
            out << "  " << f.name << " [" << f.category << "] " << f.per100.kcal << " kcal, Fett " << f.per100.fat << ", Salz " << f.per100.salt << "\n";
        CHECK(foods.size() == 3, "3 Lebensmittel");
        CHECK(foods.size() == 3 && foods[1].name == QStringLiteral("Grünkern; geschrotet") && std::fabs(foods[1].per100.salt - 0.05) < 1e-9,
              "Anführungszeichen, Umlaute, <-Wert");
        CHECK(foods.size() == 3 && std::fabs(foods[2].per100.kcal - 259 / 4.184) < 0.1 && foods[2].category == QLatin1String("milch"), "kJ→kcal, Kategorie");
        CHECK(foods.size() == 3 && foods[1].category == QLatin1String("getreide"), "ohne Kategorie: vom ähnlichen eingebauten Eintrag (Grünkern)");
        // 2. Open-Food-Facts-Export: Tabulator, energy_100g in kJ, Natrium in g
        QByteArray off = "code\tproduct_name\tgeneric_name\tcategories\tenergy_100g\tenergy-kcal_100g\tfat_100g\tsaturated-fat_100g\t"
                         "carbohydrates_100g\tsugars_100g\tfiber_100g\tproteins_100g\tsodium_100g\n"
                         "400\tKichererbsen-Aufstrich\tHummus\ten:spreads\t1100\t\t18\t2\t12\t1\t5\t7\t0.4\n";
        FoodCsv::Table t2;
        CHECK(FoodCsv::readTable(writeBytes(QStringLiteral("off.csv"), off), &t2, &err), "OFF lesen");
        const FoodCsv::Mapping m2 = FoodCsv::autoMap(t2.header);
        CHECK(m2.col[FoodCsv::Name] == 1 && m2.col[FoodCsv::KJ] == 4 && m2.col[FoodCsv::Kcal] == 5 && m2.col[FoodCsv::Sodium] == 12
                  && m2.col[FoodCsv::Fat] == 6 && m2.col[FoodCsv::SatFat] == 7,
              "automatische Zuordnung (Open Food Facts)");
        const QList<Food> f2 = FoodCsv::convert(t2, m2, o);
        CHECK(f2.size() == 1 && std::fabs(f2[0].per100.kcal - 1100 / 4.184) < 0.1 && std::fabs(f2[0].per100.salt - 1.0) < 1e-6,
              "OFF: leere kcal → kJ, Natrium → Salz");
        // 3. USDA FoodData Central (Aufbau wie Foundation Foods/SR Legacy)
        QDir(ct.path()).mkpath(QStringLiteral("fdc"));
        writeBytes(QStringLiteral("fdc/food.csv"), "\"fdc_id\",\"data_type\",\"description\",\"food_category_id\",\"publication_date\"\n"
                                                    "\"1001\",\"sr_legacy_food\",\"Butter, salted\",\"1\",\"2019-04-01\"\n"
                                                    "\"1002\",\"foundation_food\",\"Lentils, dry\",\"16\",\"2020-10-30\"\n"
                                                    "\"1003\",\"sub_sample_food\",\"Lentils, dry (Probe)\",\"16\",\"2020-10-30\"\n");
        writeBytes(QStringLiteral("fdc/food_category.csv"), "\"id\",\"code\",\"description\"\n\"1\",\"0100\",\"Dairy and Egg Products\"\n"
                                                             "\"16\",\"1600\",\"Legumes and Legume Products\"\n");
        writeBytes(QStringLiteral("fdc/nutrient.csv"), "\"id\",\"name\",\"unit_name\",\"nutrient_nbr\",\"rank\"\n"
                                                        "\"1003\",\"Protein\",\"G\",\"203\",\"600\"\n\"1004\",\"Total lipid (fat)\",\"G\",\"204\",\"800\"\n"
                                                        "\"1005\",\"Carbohydrate, by difference\",\"G\",\"205\",\"1110\"\n\"1008\",\"Energy\",\"KCAL\",\"208\",\"300\"\n"
                                                        "\"2047\",\"Energy (Atwater General Factors)\",\"KCAL\",\"957\",\"280\"\n"
                                                        "\"1093\",\"Sodium, Na\",\"MG\",\"307\",\"5800\"\n\"1258\",\"Fatty acids, total saturated\",\"G\",\"606\",\"9700\"\n");
        writeBytes(QStringLiteral("fdc/food_nutrient.csv"), "\"id\",\"fdc_id\",\"nutrient_id\",\"amount\",\"data_points\"\n"
                                                             "\"1\",\"1001\",\"1008\",\"717\",\"\"\n\"2\",\"1001\",\"1004\",\"81.11\",\"\"\n"
                                                             "\"3\",\"1001\",\"1093\",\"643\",\"\"\n\"4\",\"1001\",\"1258\",\"51.37\",\"\"\n"
                                                             "\"5\",\"1002\",\"2047\",\"360\",\"\"\n\"6\",\"1002\",\"1003\",\"23.6\",\"\"\n"
                                                             "\"7\",\"1003\",\"2047\",\"999\",\"\"\n");
        QList<Food> usda;
        CHECK(FoodCsv::isUsdaFolder(ct.filePath(QStringLiteral("fdc"))), "USDA-Ordner erkannt");
        CHECK(FoodCsv::readUsda(ct.filePath(QStringLiteral("fdc")), FoodCsv::Options(), &usda, &err), "USDA lesen: " + err);
        for (const Food &f : usda)
            out << "  USDA " << f.key << " " << f.name << " [" << f.category << "] " << f.per100.kcal << " kcal, Salz " << f.per100.salt << "\n";
        CHECK(usda.size() == 2, "USDA: Einzelprobe ausgelassen");
        CHECK(usda.size() == 2 && usda[0].name == QLatin1String("Butter, salted") && std::fabs(usda[0].per100.salt - 1.6075) < 1e-3
                  && usda[0].category == QLatin1String("milch") && std::fabs(usda[0].per100.satFat - 51.37) < 1e-6,
              "USDA: Butter (Natrium mg → Salz, Kategorie)");
        CHECK(usda.size() == 2 && std::fabs(usda[1].per100.kcal - 360) < 1e-6 && usda[1].category == QLatin1String("huelsen"), "USDA: Atwater-Energie");
        // 4. Übernehmen, eingebaute Werte überschreiben, wieder entfernen, Export/Vorlage einlesen
        FoodCsv::Options ov;
        ov.source = QStringLiteral("Test-Überschreiben");
        ov.overrideBuiltin = true;
        Food ownButter;
        FoodCsv::Table tb;
        writeBytes(QStringLiteral("butter.csv"), "Name;kcal;Fett\nButter;700;77\n");
        FoodCsv::readTable(ct.filePath(QStringLiteral("butter.csv")), &tb);
        const QList<Food> ob = FoodCsv::convert(tb, FoodCsv::autoMap(tb.header), ov);
        CHECK(ob.size() == 1 && ob[0].key == QLatin1String("butter") && ob[0].aliases.contains(QStringLiteral("unsalted butter")), "eingebaute Butter überschreiben");
        FoodTable &ft = FoodTable::instance();
        CHECK(ft.saveUserFoods(ob, true) == 1 && std::fabs(ft.match(QStringLiteral("Butter"))->per100.kcal - 700) < 1e-6, "eigener Wert wirkt");
        CHECK(ft.removeUserFoodsFrom(QStringLiteral("Test-Überschreiben")) == 1 && std::fabs(ft.match(QStringLiteral("Butter"))->per100.kcal - 740) < 1e-6,
              "Herkunft entfernen → eingebauter Wert");
        CHECK(ft.saveUserFoods(foods, true) == 3 && ft.match(QStringLiteral("Dinkelvollkornmehl"))->key == QLatin1String("csv_dinkelvollkornmehl"),
              "importierte Zutat wird zugeordnet");
        const QString exp = ct.filePath(QStringLiteral("export.csv"));
        CHECK(FoodCsv::exportUserFoods(exp, &err), "Export");
        FoodCsv::Table te;
        CHECK(FoodCsv::readTable(exp, &te) && te.rows.size() == 3, "Export wieder einlesen");
        const QList<Food> back = FoodCsv::convert(te, FoodCsv::autoMap(te.header), o);
        CHECK(back.size() == 3 && std::fabs(back[0].per100.kcal - foods[0].per100.kcal) < 1 && back[0].category == foods[0].category,
              "Export/Import verlustfrei");
        CHECK(FoodCsv::writeTemplate(ct.filePath(QStringLiteral("vorlage.csv"))), "Vorlage");
        FoodCsv::Table tv;
        FoodCsv::readTable(ct.filePath(QStringLiteral("vorlage.csv")), &tv);
        CHECK(FoodCsv::convert(tv, FoodCsv::autoMap(tv.header), o).size() == 2, "Vorlage einlesbar");
        ft.removeUserFoodsFrom(QStringLiteral("Test-CSV"));
        CHECK(FoodCsv::number(QStringLiteral("1.234,5")) == 1234.5 && FoodCsv::number(QStringLiteral("1,234.5")) == 1234.5, "Zahlenformate");
    }

    // Datenbank
    QTemporaryDir tmp;
    RecipeDb &db = RecipeDb::instance();
    CHECK(db.create(tmp.filePath(QStringLiteral("db")), QStringLiteral("Test")), "Datenbank anlegen");
    Recipe a = r;
    CHECK(db.save(a), "speichern");
    Recipe b;
    CHECK(db.load(a.id, &b) && b.title == a.title && b.ingredients.size() == a.ingredients.size(), "laden");
    CHECK(std::fabs(b.nutrition.perServing.kcal - a.nutrition.perServing.kcal) < 1, "Nährwerte gespeichert");
    Recipe probe;
    probe.title = QStringLiteral("  test ");
    CHECK(db.duplicatesOf(probe).size() == 1, "Doublette");
    db.close();
    CHECK(db.open(tmp.filePath(QStringLiteral("db"))) && db.count() == 1, "wieder öffnen");


    // ---------------------------------------------------------------- Umlaute
    out << "\nUmlaute:\n";
    {
        struct U { const char *in, *want; };
        const U cases[] = {
            { "Gemuesebruehe", "Gemüsebrühe" }, { "Kaese", "Käse" }, { "Weisswein", "Weißwein" }, { "heiss", "heiß" },
            { "Suessspeise", "Süßspeise" }, { "Schuessel", "Schüssel" }, { "Sauerbraten", "Sauerbraten" }, { "Feuer", "Feuer" },
            { "neue", "neue" }, { "Quelle", "Quelle" }, { "zuerst", "zuerst" }, { "eventuell", "eventuell" },
            { "Baguette", "Baguette" }, { "Masse", "Masse" }, { "Wasser", "Wasser" }, { "giessen", "gießen" },
            { "gleichmaessig", "gleichmäßig" }, { "Griess", "Grieß" }, { "Reissalat", "Reissalat" }, { "UEBERBACKEN", "ÜBERBACKEN" },
            { "Balsamicoessig", "Balsamicoessig" }, { "eingeoelt", "eingeölt" }, { "Chilioel", "Chiliöl" }, { "Kakaoeis", "Kakaoeis" },
            { "Menue", "Menü" }, { "Fondue", "Fondue" }, { "Oetker", "Oetker" }, { "Paella", "Paella" }, { "aussen", "außen" },
            { "fuellen", "füllen" }, { "Kloesse", "Klöße" }, { "Muskatnuss", "Muskatnuss" }, { "Essig", "Essig" },
        };
        for (const U &c : cases) {
            const QString got = Umlauts::fixWord(QString::fromUtf8(c.in));
            CHECK(got == QString::fromUtf8(c.want), QStringLiteral("%1 → %2 (erhalten: %3)").arg(QString::fromUtf8(c.in), QString::fromUtf8(c.want), got));
        }
        CHECK(Umlauts::headingCase(QStringLiteral("FÜR DEN TEIG")) == QStringLiteral("Für den Teig"), "Überschrift Groß → normal");
        CHECK(!Umlauts::looksTranscribed(QStringLiteral("Käse und Gemuese")), "Text mit Umlauten bleibt");
        CHECK(Umlauts::looksTranscribed(QStringLiteral("Den Kaese reiben")), "Umschrift erkannt");
        CHECK(Glossary::detectLanguage(QStringLiteral("Den Lauch in Ringe schneiden und mit der Butter dünsten, dann die Brühe zugeben.")) == QLatin1String("de"), "Sprache de");
        CHECK(Glossary::detectLanguage(QStringLiteral("Add the flour and stir until smooth, then bake for 20 minutes.")) == QLatin1String("en"), "Sprache en");
        CHECK(Glossary::detectLanguage(QStringLiteral("Faire cuire les pommes de terre dans l'eau avec du sel pendant 20 minutes, puis ajouter le beurre.")) == QLatin1String("fr"), "Sprache fr");
        CHECK(Glossary::detectLanguage(QStringLiteral("Cuocere la pasta in acqua con sale per 10 minuti, poi aggiungere il sugo e il pepe.")) == QLatin1String("it"), "Sprache it");
    }

    // ---------------------------------------------------------------- winREZKONV (ASCII-Umschrift)
    out << "\nwinREZKONV:\n";
    {
        const QString rk = QStringLiteral(
            "========== REZKONV-Rezept - winREZKONV v1.6.100\n\n"
            "     Titel: Gruenkernbratlinge mit Kaesesosse und gedünstetem Lauchgemue ...\n"
            "Kategorien: Getreide, Kaese\n"
            "     Menge: 4 Personen\n\n"
            "====================BRATLINGE====================\n"
            "    250     Gramm  Gruenkern, geschrotet\n"
            "    0,5     Liter  Gemuesebruehe\n"
            "      1      Prise/n Muskat\n"
            "      2      Essl. Greyerzer; fein\n"
            "                   -gerieben\n"
            "      1            Zitrone: Saft\n"
            "      4      Kugel(n) Vanilleeis\n"
            "      4            Zanderfilets a 125 g\n\n"
            "Den Gruenkern in der heissen Bruehe quellen lassen, in eine Schuessel geben. Bei 180GradC\n"
            "backen und mit Weisswein abloeschen.\n\n"
            ": O-Titel   : Gruenkernbratlinge mit Kaesesosse und gedünstetem\n"
            ": >           Lauchgemuese\n\n"
            "=====\n");
        const auto res = Importers::parseRezKonv(rk);
        CHECK(res.recipes.size() == 1, "ein Rezept");
        if (!res.recipes.isEmpty()) {
            const Recipe &r = res.recipes.first();
            out << "  " << r.title << "\n";
            for (const Ingredient &i : r.ingredients) out << "    " << i.displayLine() << " -> " << i.food << " (" << i.grams << " g)\n";
            out << "  " << r.instructions << "\n";
            // „gedünstetem“ enthält ein echtes ü → Datei nicht reine Umschrift? Titel kommt aus O-Titel
            CHECK(r.title.startsWith(QStringLiteral("Grünkernbratlinge mit Käsesoße")), "voller Titel aus O-Titel");
            CHECK(r.ingredients.size() == 8 && r.ingredients[1].heading == false, "Zutaten ohne Fortsetzungszeilen");
            auto ing = [&](int k) { return k < r.ingredients.size() ? r.ingredients[k] : Ingredient(); };
            CHECK(ing(1).name == QStringLiteral("Grünkern") && ing(1).food == QLatin1String("gruenkern"), "Grünkern erkannt");
            CHECK(ing(2).food == QLatin1String("bruehe"), "Gemüsebrühe erkannt");
            CHECK(ing(3).unit == QLatin1String("Prise"), "Prise/n");
            CHECK(ing(4).food == QLatin1String("hartkaese") && ing(4).note == QStringLiteral("fein gerieben"), "Greyerzer + Fortsetzung");
            CHECK(ing(5).name == QStringLiteral("Zitrone") && ing(5).food == QLatin1String("zitronensaft"), "Zitrone: Saft");
            CHECK(ing(6).unit == QLatin1String("Kugel") && ing(6).food == QLatin1String("speiseeis") && std::fabs(ing(6).grams - 200) < 1, "Kugel(n) Vanilleeis");
            CHECK(ing(7).food == QLatin1String("fisch_mager") && std::fabs(ing(7).grams - 500) < 1, "Zanderfilets à 125 g");
            CHECK(r.instructions.contains(QStringLiteral("180° C")) && r.instructions.contains(QStringLiteral("heißen Brühe"))
                      && r.instructions.contains(QStringLiteral("Schüssel")) && r.instructions.contains(QStringLiteral("ablöschen"))
                      && !r.instructions.contains(QStringLiteral("O-Titel")),
                  "Anleitung korrigiert");
        }
        CHECK(FoodTable::instance().match(QStringLiteral("Gruyère"))->key == QLatin1String("hartkaese"), "Gruyère");
        CHECK(FoodTable::instance().match(QStringLiteral("Gemuesebruehe"))->key == QLatin1String("bruehe"), "Gemuesebruehe ohne Umlaute");
        CHECK(FoodTable::instance().match(QStringLiteral("Nelkenpulver"))->key == QLatin1String("gewuerze"), "Nelkenpulver (Wortteil)");
    }

    // ---------------------------------------------------------------- DeepL (Testserver)
    out << "\nDeepL (Testserver):\n";
    {
        QTcpServer server;
        server.listen(QHostAddress::LocalHost);
        QByteArray lastAuth;
        QObject::connect(&server, &QTcpServer::newConnection, [&] {
            QTcpSocket *sock = server.nextPendingConnection();
            auto buf = std::make_shared<QByteArray>();
            QObject::connect(sock, &QTcpSocket::readyRead, sock, [sock, buf, &lastAuth] {
                *buf += sock->readAll();
                const int he = int(buf->indexOf("\r\n\r\n"));
                if (he < 0) return;
                const QByteArray head = buf->left(he);
                int len = 0;
                for (const QByteArray &l : head.split('\n')) {
                    if (l.toLower().startsWith("content-length:")) len = l.mid(15).trimmed().toInt();
                    if (l.toLower().startsWith("authorization:")) lastAuth = l.mid(14).trimmed();
                }
                if (buf->size() < he + 4 + len) return;
                const QByteArray body = buf->mid(he + 4, len);
                QByteArray answer;
                int status = 200;
                if (lastAuth != "DeepL-Auth-Key test:fx") {
                    status = 403;
                } else if (head.startsWith("GET /v2/usage")) {
                    answer = R"({"character_count":1234,"character_limit":500000})";
                } else {
                    const QJsonArray in = QJsonDocument::fromJson(body).object().value(QStringLiteral("text")).toArray();
                    QJsonArray tr;
                    for (const auto &v : in) {
                        static const QHash<QString, QString> dict{ { QStringLiteral("flour"), QStringLiteral("Mehl") },
                                                                   { QStringLiteral("butter"), QStringLiteral("Butter") },
                                                                   { QStringLiteral("melted"), QStringLiteral("geschmolzen") } };
                        const QString t = v.toString();
                        tr.append(QJsonObject{ { QStringLiteral("detected_source_language"), QStringLiteral("EN") },
                                               { QStringLiteral("text"), dict.value(t, QStringLiteral("DE:") + t) } });
                    }
                    answer = QJsonDocument(QJsonObject{ { QStringLiteral("translations"), tr } }).toJson(QJsonDocument::Compact);
                }
                sock->write(QByteArray("HTTP/1.1 ") + QByteArray::number(status) + (status == 200 ? " OK" : " Forbidden")
                            + "\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "
                            + QByteArray::number(answer.size()) + "\r\n\r\n" + answer);
                sock->disconnectFromHost();
            });
        });
        qputenv("VIVIAMEALS_DEEPL_URL", QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        DeepL::Usage u;
        QString err;
        CHECK(DeepL::usage(QStringLiteral("test:fx"), &u, &err) && u.characterCount == 1234 && u.characterLimit == 500000, "Kontingent: " + err);
        CHECK(!DeepL::usage(QStringLiteral("falsch"), &u, &err) && err.contains(QStringLiteral("abgelehnt")), "falscher Schlüssel abgelehnt");
        const QString mm = QStringLiteral(
            "MMMMM----- Recipe via Meal-Master (tm) v8.05\n\n"
            "      Title: Simple Cake\n"
            " Categories: Cakes\n"
            "      Yield: 8 servings\n\n"
            "      2 c  flour\n"
            "    1/2 c  butter; melted\n\n"
            "  Mix the flour and the butter until smooth, then bake at 350 F for 30 minutes.\n\n"
            "MMMMM\n");
        Importers::Options opt;
        int calls = 0;
        opt.translator = [&](Recipe &r, QString *e) {
            ++calls;
            return DeepL::translateRecipe(r, QStringLiteral("test:fx"), e);
        };
        auto res = Importers::parseMealMaster(mm, opt);
        CHECK(res.recipes.size() == 1 && calls == 1, "englisches Rezept an DeepL übergeben");
        if (!res.recipes.isEmpty()) {
            const Recipe &r = res.recipes.first();
            out << "  " << r.title << " | " << r.instructions << "\n";
            for (const Ingredient &i : r.ingredients) out << "    " << i.displayLine() << " -> " << i.food << "\n";
            CHECK(r.language == QLatin1String("de") && r.originalLanguage == QLatin1String("en") && r.translatedBy == QLatin1String("DeepL"),
                  "Sprache nach Übersetzung");
            CHECK(r.ingredients.size() == 2 && r.ingredients[0].name == QStringLiteral("Mehl") && r.ingredients[0].unit == QLatin1String("g")
                      && r.ingredients[0].food == QLatin1String("weizenmehl") && r.ingredients[1].note == QStringLiteral("geschmolzen"),
                  "Zutaten übersetzt und zugeordnet");
            CHECK(r.instructions.contains(QStringLiteral("175° C")) || r.instructions.contains(QStringLiteral("180° C")), "°F vor der Übersetzung umgerechnet");
            CHECK(RecipeHtml::languageNote(r).contains(QStringLiteral("Englischen")), "Hinweis „aus dem Englischen übersetzt“");
        }
        // deutsches Rezept geht nie an DeepL
        calls = 0;
        res = Importers::parseRezKonv(QStringLiteral("========== REZKONV-Rezept - winREZKONV v1.6.100\n\n     Titel: Test\n     Menge: 2 Personen\n\n"
                                                     "    200     Gramm  Mehl\n\nDas Mehl mit der Butter und dem Zucker verrühren und backen.\n\n=====\n"),
                                      opt);
        CHECK(res.recipes.size() == 1 && calls == 0 && res.recipes.first().translatedBy.isEmpty(), "deutsches Rezept nicht übersetzt");
        // Fehler → Glossar
        opt.translator = [&](Recipe &r, QString *e) { return DeepL::translateRecipe(r, QStringLiteral("falsch"), e); };
        res = Importers::parseMealMaster(mm, opt);
        if (!res.recipes.isEmpty()) out << "  Glossar: " << res.recipes.first().ingredients.value(0).displayLine() << " | " << res.warnings.join(QStringLiteral("; ")) << "\n";
        CHECK(res.recipes.size() == 1 && res.recipes.first().ingredients.value(0).food == QLatin1String("weizenmehl")
                  && !res.recipes.first().ingredients.value(0).name.startsWith(QLatin1String("DE:")) && res.warnings.size() == 1,
              "bei Fehler Glossar + Warnung");
        qunsetenv("VIVIAMEALS_DEEPL_URL");
    }


    // ---------------------------------------------------------------- Mahlzeit-Arten und Wochenplan
    out << "\nWochenplan:\n";
    {
        QTemporaryDir tmp2;
        RecipeDb &db = RecipeDb::instance();
        CHECK(db.create(tmp2.filePath(QStringLiteral("plan")), QStringLiteral("Plan")), "Plan-Datenbank");
        auto importAll = [&](const QString &f) {
            for (Recipe r : Importers::importFile(f, Importers::Format::Unknown).recipes) db.save(r);
        };
        importAll(QStringLiteral(":/samples/beispiele.rk"));
        importAll(QStringLiteral(":/samples/beispiele.mmf"));
        importAll(QStringLiteral(":/samples/umlaute.rk"));
        MealPlanner::Options o;
        o.goal = Profile::Goal::Lose;
        o.snack = true;
        o.targetKcal = 1700;
        QList<int> n = MealPlanner::slotCounts(o);
        out << "  Kandidaten ohne gesunde Vorschläge (Frühstück/warm/leicht/Zwischenmahlzeit): " << n[0] << "/" << n[1] << "/" << n[2] << "/" << n[3] << "\n";
        for (const QString &x : MealPlanner::notes(o)) out << "  Hinweis: " << x << "\n";
        // Fortschritt: steigend bis zum Ende; Abbruch liefert einen leeren Plan
        int lastDone = -1, calls = 0, lastTotal = 0;
        bool monotone = true;
        MealPlan plan = MealPlanner::create(o, [&](int done, int total, const QString &) {
            if (done < lastDone) monotone = false;
            lastDone = done;
            lastTotal = total;
            ++calls;
            return true;
        });
        CHECK(calls >= 8 && monotone && lastDone == lastTotal && !plan.isEmpty(), "Wochenplan-Fortschritt " + QString::number(calls));
        CHECK(MealPlanner::create(o, [](int done, int, const QString &) { return done < 300; }).isEmpty(), "Wochenplan abgebrochen");
        bool breakfastOk = true, noDessert = true;
        for (const auto &day : plan.days) {
            const RecipeSummary *b = db.summary(day.value(0).recipeId);
            if (b && !b->tags.contains(QStringLiteral("frühstück"))) breakfastOk = false;
            for (const PlanEntry &e : day)
                if (const RecipeSummary *s = db.summary(e.recipeId))
                    if (s->tags.contains(QStringLiteral("desserts")) || s->tags.contains(QStringLiteral("süßspeise"))) noDessert = false;
        }
        CHECK(breakfastOk, "Frühstück nur aus Frühstücksrezepten");
        CHECK(noDessert, "beim Abnehmen keine Desserts/Süßspeisen");
        importAll(QStringLiteral(":/samples/gesund.rk"));
        n = MealPlanner::slotCounts(o);
        out << "  mit gesunden Vorschlägen: " << n[0] << "/" << n[1] << "/" << n[2] << "/" << n[3] << "\n";
        CHECK(n[0] >= 5 && n[3] >= 5 && n[2] >= 10, "Frühstück, Zwischenmahlzeiten und leichte Mahlzeiten verfügbar");
        plan = MealPlanner::create(o);
        int empty = 0;
        bool grams = true;
        for (const auto &day : plan.days)
            for (const PlanEntry &e : day) {
                if (e.recipeId.isEmpty()) ++empty;
                else if (e.grams <= 0) grams = false;
            }
        CHECK(empty == 0, "Plan vollständig");
        CHECK(grams, "Gramm je Mahlzeit bekannt");
        for (int m = 0; m < plan.meals.size(); ++m)
            out << "  " << plan.meals[m] << ": " << plan.days[0][m].title << " – " << MealPlanner::portionText(plan.days[0][m]) << ", "
                << qRound(plan.days[0][m].kcal) << " kcal\n";
        out << "  " << MealPlanner::summary(plan) << "\n";
        // Tagesaufbau beim Abnehmen: Frühstück, Mittag (warm), Zwischenmahlzeit, Abend (leicht)
        CHECK(plan.roles.size() == 4 && plan.roles[1] == MealPlanner::Role::Warm && plan.roles[3] == MealPlanner::Role::Light
                  && plan.meals[3] == QStringLiteral("Abendessen"),
              "Abnehmen: mittags warm, abends leicht");
        auto checkDays = [&](const MealPlan &pl, const QString &what) {
            bool lightOk = true, oneWarm = true, noDouble = true;
            for (const auto &day : pl.days) {
                int warm = 0, meatMeals = 0, fishMeals = 0;
                QStringList starches;
                for (int m = 0; m < day.size(); ++m) {
                    const RecipeSummary *s = db.summary(day[m].recipeId);
                    if (!s) continue;
                    const MealPlanner::Role r = pl.roles[m];
                    if (r == MealPlanner::Role::Warm) ++warm;
                    if (r == MealPlanner::Role::Light && !(MealTypes::classify(MealTypes::from(*s)) & MealTypes::Light)) lightOk = false;
                    if (r == MealPlanner::Role::Warm || r == MealPlanner::Role::Light) {
                        bool meat = false, fish = false;
                        for (const QString &k : s->foods)
                            if (const Food *f = FoodTable::instance().find(k)) {
                                meat |= f->category == QLatin1String("fleisch");
                                fish |= f->category == QLatin1String("fisch");
                            }
                        meatMeals += meat;
                        fishMeals += fish;
                    }
                }
                if (warm != 1) oneWarm = false;
                if (meatMeals > 1 || fishMeals > 1) noDouble = false;
            }
            CHECK(oneWarm, what + ": genau eine warme Hauptmahlzeit je Tag");
            CHECK(lightOk, what + ": leichte Mahlzeit aus leichten Rezepten");
            CHECK(noDouble, what + ": nicht zweimal Fleisch bzw. Fisch am Tag");
        };
        checkDays(plan, QStringLiteral("Abnehmen"));
        const MealPlanner::WeekCheck wc = MealPlanner::weekCheck(plan);
        out << "  Woche: Fisch " << wc.fish << "×, Hülsenfrüchte " << wc.legumes << "×, Fleisch " << wc.meat << "×\n";
        CHECK(wc.fish >= wc.fishMin && wc.fish <= wc.fishMax + 1 && wc.legumes >= wc.legumesMin && wc.meat <= wc.meatMax,
              "Wochenrichtwerte Fisch/Hülsenfrüchte/Fleisch");
        // warm abends
        o.warmEvening = true;
        plan = MealPlanner::create(o);
        CHECK(plan.roles[1] == MealPlanner::Role::Light && plan.roles[3] == MealPlanner::Role::Warm, "warm abends: mittags leicht");
        checkDays(plan, QStringLiteral("warm abends"));
        // gespeicherter Plan behält die Rollen
        const MealPlan again = MealPlan::fromJson(plan.toJson());
        CHECK(again.roles == plan.roles && again.goal == plan.goal, "Plan speichern/laden mit Rollen");
        // Zunehmen: fünf bzw. sechs Mahlzeiten
        o.goal = Profile::Goal::Gain;
        o.warmEvening = false;
        o.targetKcal = 2900;
        plan = MealPlanner::create(o);
        CHECK(plan.meals.size() == 6 && plan.meals[5] == QStringLiteral("Spätmahlzeit") && plan.roles[4] == MealPlanner::Role::Warm,
              "Zunehmen: 6 Mahlzeiten, abends darf warm sein");
        double dayAvg = 0;
        for (int d = 0; d < plan.days.size(); ++d) dayAvg += plan.dayKcal(d) / plan.days.size();
        out << "  Zunehmen: " << plan.meals.join(QStringLiteral(", ")) << " – Ø " << qRound(dayAvg) << " kcal\n";
        o.snack = false;
        CHECK(MealPlanner::layout(o).names.size() == 5, "Zunehmen ohne Spätmahlzeit: 5 Mahlzeiten");
        // alter Plan (vor 0.6.1) ohne Rollen
        QJsonObject old = plan.toJson();
        old.remove(QStringLiteral("roles"));
        old[QStringLiteral("meals")] = QJsonArray{ QStringLiteral("Frühstück"), QStringLiteral("Mittagessen"), QStringLiteral("Abendessen") };
        CHECK(MealPlan::fromJson(old).roles.value(2) == MealPlanner::Role::Warm, "alter Plan: Rollen ergänzt");
        Recipe pud;
        pud.title = QStringLiteral("Grießflammeri mit Kirschsoße");
        CHECK(MealTypes::classify(MealTypes::from(pud)) & MealTypes::Dessert, "Flammeri = Dessert");
        Profile pr;
        pr.weighDay = QDate(2026, 11, 9).dayOfWeek();   // Montag
        CHECK(pr.weighReminderDue(QDate(2026, 11, 9)) && pr.weighReminderDue(QDate(2026, 11, 11)), "Wiegetag-Erinnerung fällig");
        pr.markWeighDone(QDate(2026, 11, 10));
        CHECK(!pr.weighReminderDue(QDate(2026, 11, 12)) && pr.weighReminderDue(QDate(2026, 11, 16)), "erledigt bis zum nächsten Wiegetag");
        pr.weights << WeightEntry{ QDate(2026, 11, 16), 80, {} };
        CHECK(!pr.weighReminderDue(QDate(2026, 11, 17)), "Eintrag am Wiegetag erledigt die Erinnerung");
        CHECK(!Coaching::tipsHtml(pr, QStringLiteral("#000"), QStringLiteral("#f00")).isEmpty(), "Tipps");
        db.close();
    }


    // ---------------------------------------------------------------- Nährwert-Tags
    out << "\nNährwert-Tags:\n";
    {
        const auto res = Importers::importFile(QStringLiteral(":/samples/gesund.rk"), Importers::Format::Unknown);
        auto find = [&](const QString &t) -> Recipe {
            for (const Recipe &r : res.recipes)
                if (r.title.startsWith(t)) return r;
            return Recipe();
        };
        const Recipe quark = find(QStringLiteral("Magerquark"));
        CHECK(quark.hasTag(QStringLiteral("eiweißreich")) && quark.hasTag(QStringLiteral("fettarm")) && !quark.hasTag(QStringLiteral("hochkalorisch")),
              "Magerquark mit Beeren: eiweißreich, fettarm");
        const Recipe sticks = find(QStringLiteral("Gemüsesticks"));
        CHECK(sticks.hasTag(QStringLiteral("vitaminreich")) && sticks.hasTag(QStringLiteral("kalorienarm")), "Gemüsesticks: vitaminreich, kalorienarm");
        const Recipe porridge = find(QStringLiteral("Haferflocken"));
        CHECK(porridge.hasTag(QStringLiteral("reduktionsdiät")), "Porridge: reduktionsdiät");
        const auto rk = Importers::importFile(QStringLiteral(":/samples/umlaute.rk"), Importers::Format::Unknown);
        CHECK(!rk.recipes.isEmpty() && rk.recipes.first().hasTag(QStringLiteral("hochkalorisch"))
                  && !rk.recipes.first().hasTag(QStringLiteral("reduktionsdiät")),
              "Käsespätzle: hochkalorisch, keine Reduktionsdiät");
        const auto bk = Importers::importFile(QStringLiteral(":/samples/beispiele.rk"), Importers::Format::Unknown);
        CHECK(!bk.recipes.isEmpty() && !bk.recipes.first().hasTag(QStringLiteral("vitaminreich")), "Apfelkuchen nicht vitaminreich");
        // abgeleitete Tags werden entfernt, wenn sie nicht mehr passen; ohne Nährwerte bleibt alles
        Recipe r = quark;
        r.addTag(QStringLiteral("hochkalorisch"));
        CHECK(MealTypes::applyNutrientTags(r) == 1 && !r.hasTag(QStringLiteral("hochkalorisch")), "unpassendes Tag entfernt");
        Recipe empty;
        empty.title = QStringLiteral("ohne Nährwerte");
        empty.addTag(QStringLiteral("kalorienarm"));
        CHECK(MealTypes::applyNutrientTags(empty) == 0 && empty.hasTag(QStringLiteral("kalorienarm")), "ohne Nährwerte unverändert");
        Profile w;
        w.weighDay = QDate::currentDate().dayOfWeek();
        w.addWeight(QDate::currentDate(), 72.5);
        w.addWeight(QDate::currentDate().addDays(-7), 73.4);
        w.addWeight(QDate::currentDate(), 72.3);
        CHECK(w.weights.size() == 2 && w.weights.last().kg == 72.3 && w.weights.first().kg == 73.4 && !w.weighReminderDue(),
              "Gewicht eintragen: sortiert, gleicher Tag ersetzt, Erinnerung erledigt");
    }


    // ---------------------------------------------------------------- Ernährungstagebuch
    out << "\nErnährungstagebuch:\n";
    {
        const auto res = Importers::importFile(QStringLiteral(":/samples/gesund.rk"), Importers::Format::Unknown);
        CHECK(res.recipes.size() >= 2, "Rezepte für das Tagebuch");
        const Recipe porridge = res.recipes.value(0);
        const QString pid = QStringLiteral("test-%1").arg(QDateTime::currentMSecsSinceEpoch());
        Diary::Book book(pid);
        Diary::Entry e = Diary::Entry::fromRecipe(porridge, false);
        e.when = QDateTime(QDate(2026, 11, 10), QTime(7, 45));
        e.meal = Diary::mealForTime(e.when.time());
        e.setPortions(1.5);
        CHECK(std::fabs(e.eaten().kcal - porridge.nutrition.perServing.kcal * 1.5) < 0.01 && e.grams > 0, "Portionen → kcal und Gramm");
        const double perGram = porridge.nutrition.perServing.kcal / e.servingGrams;
        e.setGrams(200);
        CHECK(std::fabs(e.eaten().kcal - perGram * 200) < 0.5 && e.byGrams, "Gramm → kcal");
        CHECK(e.meal == QStringLiteral("Frühstück"), "Mahlzeit nach Uhrzeit");
        book.add(e);
        // eigenes Rezept
        Recipe own;
        own.title = QStringLiteral("Mein Salat");
        own.servings = 2;
        own.ingredients = IngredientParser::parseText(QStringLiteral("200 g Feldsalat\n2 EL Rapsöl\n100 g Feta"));
        Nutrition::analyze(own);
        Diary::Entry o = Diary::Entry::fromRecipe(own, true);
        o.when = QDateTime(QDate(2026, 11, 10), QTime(12, 40));
        o.meal = Diary::mealForTime(o.when.time());
        book.add(o);
        Diary::Entry other = o;
        other.id.clear();
        other.when = QDateTime(QDate(2026, 11, 9), QTime(19, 0));
        book.add(other);
        CHECK(book.save(), "Tagebuch speichern");
        Diary::Book again(pid);
        const QList<Diary::Entry> day = again.day(QDate(2026, 11, 10));
        CHECK(day.size() == 2 && day[0].title == porridge.title && day[1].own && day[1].recipe().ingredients.size() == 3, "Tagebuch laden, eigenes Rezept erhalten");
        const double total = again.dayTotal(QDate(2026, 11, 10)).kcal;
        CHECK(std::fabs(total - (perGram * 200 + own.nutrition.perServing.kcal)) < 1, "Tagessumme");
        out << "  10.11.: " << qRound(total) << " kcal (" << day[0].amountText() << " " << day[0].title << ", " << day[1].amountText() << " "
            << day[1].title << ")\n";
        CHECK(again.daysWithEntries().size() == 2, "Tage mit Einträgen");
        again.remove(day[0].id);
        CHECK(again.day(QDate(2026, 11, 10)).size() == 1, "Eintrag löschen");
        Diary::Book::removeFor(pid);
        CHECK(!QFile::exists(Diary::Book::fileFor(pid)), "Tagebuch einer gelöschten Person entfernt");
    }

    out << "\nRezept scannen:\n";
    {
        // Erkennungsfehler korrigieren
        CHECK(RecipeScan::fixLine(QStringLiteral("l/2 TL Salz")) == QStringLiteral("½ TL Salz"), "l/2 → ½");
        CHECK(RecipeScan::fixLine(QStringLiteral("1OO g Mehl")) == QStringLiteral("100 g Mehl"), "1OO → 100");
        CHECK(RecipeScan::fixLine(QStringLiteral("1l Milch")) == QStringLiteral("1 l Milch"), "1l → 1 l");
        CHECK(RecipeScan::fixLine(QStringLiteral("75g Mehl")) == QStringLiteral("75 g Mehl") && RecipeScan::fixLine(QStringLiteral("6EL Sahne")) == QStringLiteral("6 EL Sahne"),
              "Menge und Einheit trennen");
        CHECK(RecipeScan::fixLine(QStringLiteral("11 Fleischbrühe vom Tafelspitz")) == QStringLiteral("1 l Fleischbrühe vom Tafelspitz")
                  && RecipeScan::fixLine(QStringLiteral("11 Eier")) == QStringLiteral("11 Eier"),
              "„11“ vor Flüssigkeit = 1 l");
        CHECK(RecipeScan::fixLine(QStringLiteral("2 El Öl")) == QStringLiteral("2 EL Öl"), "El → EL");
        CHECK(RecipeScan::fixLine(QStringLiteral("Va Bund Petersilie")) == QStringLiteral("½ Bund Petersilie"), "Va → ½");
        CHECK(RecipeScan::fixLine(QStringLiteral("bei 180 *C backen")) == QStringLiteral("bei 180° C backen"), "°C");
        CHECK(RecipeScan::fixLine(QStringLiteral("| ﬁnden")) == QStringLiteral("finden"), "Ligatur, Rahmen");

        // Aufteilung aus Zeilen (wie Tesseract sie liefert: Block, Absatz, Höhe)
        auto L = [](const QString &t, int block, int par, int h) {
            Ocr::Line l;
            l.text = t;
            l.block = block;
            l.par = par;
            l.box = QRect(0, 0, 400, h);
            l.conf = 90;
            return l;
        };
        QList<Ocr::Line> page{
            L(QStringLiteral("Kartoffel-Lauch-"), 1, 1, 60), L(QStringLiteral("Suppe"), 1, 1, 60),
            L(QStringLiteral("Eine wärmende Suppe für kalte Tage, schnell gemacht und sättigend."), 2, 1, 22),
            L(QStringLiteral("Zubereitungszeit: 35 Min."), 3, 1, 22),
            L(QStringLiteral("Zutaten für 4 Personen"), 4, 1, 26),
            L(QStringLiteral("• 600 g Kartoffeln"), 4, 2, 22), L(QStringLiteral("2 Stangen Lauch"), 4, 2, 22),
            L(QStringLiteral("1 Zwiebel, fein"), 4, 2, 22), L(QStringLiteral("gewürfelt"), 4, 2, 22),
            L(QStringLiteral("2 El Rapsöl 1 l Gemüsebrühe"), 4, 2, 22),
            L(QStringLiteral("Salz"), 4, 2, 22), L(QStringLiteral("Pfeffer"), 4, 2, 22),
            L(QStringLiteral("So wird's gemacht"), 5, 1, 26),
            L(QStringLiteral("1. Kartoffeln schälen und würfeln. Lauch putzen, in Ringe schnei-"), 5, 2, 22),
            L(QStringLiteral("den und waschen."), 5, 2, 22),
            L(QStringLiteral("2. Zwiebel im Öl andünsten, Gemüse zugeben und mit der Brühe"), 5, 3, 22),
            L(QStringLiteral("aufgießen. 20 Minuten köcheln lassen, pürieren und abschmecken."), 5, 3, 22),
            L(QStringLiteral("Pro Portion: 210 kcal"), 6, 1, 20),
        };
        const RecipeScan::Parts p = RecipeScan::split(page);
        CHECK(p.text.title == QStringLiteral("Kartoffel-Lauch-Suppe"), "Titel zweizeilig: " + p.text.title);
        CHECK(p.text.yield == QStringLiteral("4 Portionen"), "Portionen: " + p.text.yield);
        CHECK(p.prepMinutes == 35, "Zubereitungszeit");
        CHECK(p.text.ingredientLines.size() == 7, "Zutaten: " + p.text.ingredientLines.join(QStringLiteral(" | ")));
        CHECK(p.text.ingredientLines.value(2) == QStringLiteral("1 Zwiebel, fein gewürfelt"), "Fortsetzungszeile");
        CHECK(p.text.ingredientLines.value(3) == QStringLiteral("2 EL Rapsöl") && p.text.ingredientLines.value(4) == QStringLiteral("1 l Gemüsebrühe"),
              "zwei Spalten in einer Zeile");
        const QString dirText = p.text.directionLines.join(QLatin1Char('\n'));
        CHECK(dirText.contains(QStringLiteral("schnei-")) && dirText.contains(QStringLiteral("aufgießen")), "Zubereitung");
        CHECK(p.unassigned.size() == 3, "Einleitung und Nährwertzeile nicht zugeordnet: " + p.unassigned.join(QStringLiteral(" | ")));
        const Recipe r = RecipeScan::toRecipe(p);
        CHECK(r.servings == 4 && r.ingredients.size() == 7 && r.instructions.contains(QStringLiteral("schneiden und waschen")),
              "Rezept aus Scan (Silbentrennung zusammengefügt)");
        CHECK(r.nutrition.perServing.kcal > 100 && r.prepMinutes == 35 && r.hasTag(QStringLiteral("vegan")), "Nährwerte und Tags beim Scan");
        out << "  " << r.title << ": " << r.servingsText() << ", " << qRound(r.nutrition.perServing.kcal) << " kcal je Portion, Tags "
            << r.tags.join(QStringLiteral(", ")) << "\n";

        // Mengen in eigener Spalte (getrennte Tesseract-Blöcke), Abschnittsüberschrift links davor, Kopf mit
        // Nährwerten und Kategorien, Fußzeile mit Seitenzahl und wiederholtem Titel
        auto B = [](const QString &t, int block, int x, int y, int w, int h) {
            Ocr::Line l;
            l.text = t;
            l.block = block;
            l.par = 1;
            l.box = QRect(x, y, w, h);
            l.conf = 90;
            return l;
        };
        QList<Ocr::Line> cols{
            B(QStringLiteral("Fleischbrühe mit Kräuterflädle"), 1, 230, 250, 1000, 64),
            B(QStringLiteral("200 kcal / 837 kJ je Portion"), 2, 230, 420, 500, 30),
            B(QStringLiteral("Eiweiß 7,4 g - Fett 11,4 g - Kohlenhydrate 17 g"), 2, 230, 480, 900, 28),
            B(QStringLiteral("Suppen - Einlage - Pfannkuchen - Vegetarisch"), 3, 230, 530, 700, 24),
            B(QStringLiteral("Zutaten"), 4, 230, 650, 160, 40),
            // linke Spalte: Überschrift und Mengen
            B(QStringLiteral("Kräuterflädle"), 5, 230, 740, 200, 30),
            B(QStringLiteral("75 g"), 5, 380, 800, 70, 30), B(QStringLiteral("1"), 5, 430, 852, 20, 30),
            B(QStringLiteral("6 EL"), 5, 370, 904, 80, 30),
            // rechte Spalte: Zutaten
            B(QStringLiteral("Mehl"), 6, 480, 800, 80, 30), B(QStringLiteral("Ei, klein"), 6, 480, 852, 120, 30),
            B(QStringLiteral("Sahne"), 6, 480, 904, 90, 30),
            B(QStringLiteral("Zubereitung"), 7, 230, 1100, 220, 40),
            B(QStringLiteral("Mehl, Ei und Sahne verrühren, ruhen lassen und dünne Pfannkuchen backen."), 8, 230, 1180, 1500, 28),
            B(QStringLiteral("Fleischbrühe mit Krauterfladle"), 9, 230, 3300, 400, 24),
            B(QStringLiteral("Seite 1 von 2"), 10, 2000, 3300, 160, 24),
        };
        const RecipeScan::Parts c = RecipeScan::split(cols);
        CHECK(c.text.ingredientLines == QStringList({ QStringLiteral("== Kräuterflädle =="), QStringLiteral("75 g Mehl"), QStringLiteral("1 Ei, klein"),
                                                      QStringLiteral("6 EL Sahne") }),
              "Mengenspalte und Überschrift: " + c.text.ingredientLines.join(QStringLiteral(" | ")));
        CHECK(c.text.directionLines.size() == 1, "Fußzeile nicht in der Zubereitung: " + c.text.directionLines.join(QStringLiteral(" | ")));
        CHECK(c.text.categories.size() == 4 && c.unassigned.size() == 2, "Kategorien und Nährwerte im Kopf");

        // Ohne Überschriften: Zutatenblock und Anleitung aus der Form erkennen
        QList<Ocr::Line> plain{
            L(QStringLiteral("Quarkspeise"), 1, 1, 40),
            L(QStringLiteral("500 g Magerquark"), 2, 1, 20), L(QStringLiteral("250 g Erdbeeren"), 2, 1, 20), L(QStringLiteral("2 EL Honig"), 2, 1, 20),
            L(QStringLiteral("Die Erdbeeren waschen und halbieren. Quark mit Honig glatt rühren und"), 3, 1, 20),
            L(QStringLiteral("die Früchte unterheben. Kalt servieren."), 3, 1, 20),
        };
        const RecipeScan::Parts q = RecipeScan::split(plain);
        CHECK(q.text.title == QStringLiteral("Quarkspeise") && q.text.ingredientLines.size() == 3 && q.text.directionLines.size() == 2,
              "ohne Überschriften: " + q.text.ingredientLines.join(QStringLiteral(" | ")) + " // " + q.text.directionLines.join(QStringLiteral(" | ")));

        // Seite rendern: Text links, Foto rechts oben
        QImage pageImg(1700, 2200, QImage::Format_RGB32);
        pageImg.fill(Qt::white);
        const QRect photoRect(1000, 150, 560, 420);
        {
            QPainter pt(&pageImg);
            QLinearGradient grad(photoRect.topLeft(), photoRect.bottomRight());
            grad.setColorAt(0, QColor(200, 120, 40));
            grad.setColorAt(1, QColor(60, 140, 60));
            pt.fillRect(photoRect, grad);
            QRandomGenerator rng(7);
            for (int k = 0; k < 4000; ++k)
                pt.fillRect(photoRect.x() + int(rng.bounded(photoRect.width() - 4)), photoRect.y() + int(rng.bounded(photoRect.height() - 4)), 4, 4,
                            QColor::fromHsv(int(rng.bounded(360)), 180, 120 + int(rng.bounded(100))));
            pt.setPen(Qt::black);
            QFont f(QStringLiteral("DejaVu Sans"));
            f.setPixelSize(64);
            f.setBold(true);
            pt.setFont(f);
            pt.drawText(120, 230, QStringLiteral("Linsensalat"));
            f.setPixelSize(36);
            pt.setFont(f);
            pt.drawText(120, 700, QStringLiteral("Zutaten für 2 Personen"));
            f.setBold(false);
            pt.setFont(f);
            const QStringList ingr{ QStringLiteral("150 g Berglinsen"), QStringLiteral("1 rote Paprikaschote"), QStringLiteral("2 Frühlingszwiebeln"),
                                    QStringLiteral("3 EL Olivenöl"), QStringLiteral("2 EL Apfelessig"), QStringLiteral("1 TL Senf") };
            int y = 770;
            for (const QString &t : ingr) {
                pt.drawText(120, y, t);
                y += 56;
            }
            f.setBold(true);
            pt.setFont(f);
            y += 50;
            pt.drawText(120, y, QStringLiteral("Zubereitung"));
            f.setBold(false);
            pt.setFont(f);
            const QStringList dirs{ QStringLiteral("Die Linsen in Wasser etwa 25 Minuten garen und abtropfen"),
                                    QStringLiteral("lassen. Paprika und Frühlingszwiebeln klein schneiden."),
                                    QStringLiteral("Öl, Essig und Senf verrühren und alles mischen.") };
            y += 70;
            for (const QString &t : dirs) {
                pt.drawText(120, y, t);
                y += 56;
            }
        }
        const QImage norm = RecipeScan::normalize(pageImg);
        const QList<QRect> photos = RecipeScan::findPhotos(norm);
        CHECK(photos.size() == 1, "genau ein Foto gefunden: " + QString::number(photos.size()));
        if (!photos.isEmpty()) {
            const QRect inter = photos.first().intersected(photoRect);
            CHECK(inter.width() * inter.height() > 0.85 * photoRect.width() * photoRect.height()
                      && photos.first().width() * photos.first().height() < 1.4 * photoRect.width() * photoRect.height(),
                  "Fotobereich passt");
        }
        const QString exe = Ocr::findTesseract();
        if (exe.isEmpty()) {
            out << "  (Tesseract nicht installiert – Texterkennung übersprungen)\n";
        } else {
            out << "  Tesseract " << Ocr::version(exe) << ", Sprachen " << Ocr::languages(exe).join(QLatin1Char(',')) << "\n";
            QList<Ocr::Line> lines;
            QString err;
            const bool ok = Ocr::recognize(RecipeScan::blank(norm, photos), exe, Ocr::defaultLanguages(Ocr::languages(exe)), &lines, &err);
            CHECK(ok, "Texterkennung: " + err);
            const RecipeScan::Parts s = RecipeScan::split(lines);
            CHECK(s.text.title == QStringLiteral("Linsensalat"), "OCR Titel: " + s.text.title);
            CHECK(s.text.yield == QStringLiteral("2 Portionen"), "OCR Portionen: " + s.text.yield);
            CHECK(s.text.ingredientLines.size() == 6, "OCR Zutaten: " + s.text.ingredientLines.join(QStringLiteral(" | ")));
            CHECK(s.text.directionLines.join(QLatin1Char(' ')).contains(QStringLiteral("Senf verrühren")), "OCR Zubereitung");
            const Recipe sr = RecipeScan::toRecipe(s);
            CHECK(sr.nutrition.coverage() > 0.8, "OCR Nährwerte zugeordnet");
            out << "  OCR: " << sr.title << ", " << qRound(sr.nutrition.perServing.kcal) << " kcal je Portion: " << s.text.ingredientLines.join(QStringLiteral(" | "))
                << "\n";
        }
    }

    out << "\nNährwerttabelle (Erweiterung aus den Rezeptarchiven):\n";
    {
        const QList<QPair<QString, QString>> cases{
            { QStringLiteral("Fett für die Form"), QStringLiteral("margarine") }, { QStringLiteral("Plattenfett"), QStringLiteral("kokosfett") },
            { QStringLiteral("Kokos-Mandel-Konfekt"), QStringLiteral("marzipan") }, { QStringLiteral("Raffaello"), QStringLiteral("marzipan") },
            { QStringLiteral("Rehrückenmedaillons"), QStringLiteral("wild") }, { QStringLiteral("Kalbsfilet"), QStringLiteral("kalbfleisch") },
            { QStringLiteral("Lammkeule"), QStringLiteral("lamm") }, { QStringLiteral("Creme double"), QStringLiteral("creme_double") },
            { QStringLiteral("Amaretto"), QStringLiteral("likoer") }, { QStringLiteral("Rum"), QStringLiteral("spirituosen") },
            { QStringLiteral("Spekulatius"), QStringLiteral("kekse") }, { QStringLiteral("Esskastanien"), QStringLiteral("maronen") },
            { QStringLiteral("Rote Currypaste"), QStringLiteral("wuerzpaste") }, { QStringLiteral("Hoisinsauce"), QStringLiteral("hoisinsauce") },
            { QStringLiteral("Johannisbeergelee"), QStringLiteral("marmelade") }, { QStringLiteral("Johannisbeer-Rispen"), QStringLiteral("beeren") },
            { QStringLiteral("Wildente"), QStringLiteral("ente") }, { QStringLiteral("Poularde"), QStringLiteral("haehnchenschenkel") },
            { QStringLiteral("Cidre"), QStringLiteral("bier") }, { QStringLiteral("Thymianblättchen"), QStringLiteral("kraeuter") },
            { QStringLiteral("Mandelsplitter"), QStringLiteral("mandeln") }, { QStringLiteral("Kuchenglasur"), QStringLiteral("schokolade_zartbitter") },
        };
        QStringList wrong;
        for (const auto &c : cases) {
            const Food *f = FoodTable::instance().match(c.first);
            if (!f || f->key != c.second) wrong << c.first + QStringLiteral(" → ") + (f ? f->key : QStringLiteral("—"));
        }
        CHECK(wrong.isEmpty(), "Zuordnungen: " + wrong.join(QStringLiteral(", ")));
        out << "  " << FoodTable::instance().all().size() << " Lebensmittel, " << cases.size() - wrong.size() << "/" << cases.size() << " Stichproben richtig\n";
    }

    out << "\nMealMaster aus RezKonv (Kochkiste):\n";
    {
        const QString mm = QStringLiteral(
            "*REZKONV* (iau) v0.9i - *DER* Rezeptkonverter\r\n\r\n"
            "MMMMM----- *REZKONV* (iau) v0.9i nach Meal-Master\r\n\r\n"
            "     Title: Leinsamenbrot #1\r\nCategories: Brot, Leinsamen\r\n     Yield: 1 Brot\r\n\r\n"
            "    125 g  Leinsamen\r\n    500 g  Roggenmehl Type 1370;\r\n           -ersatzweise 997\r\n"
            "  1 1/2 tb ;Salz\r\n    200 g  Kefir\r\n\r\n"
            "MMMMM-----------SAUERTEIG (500 G)----------------\r\n     15 g  Hefe\r\n    200 g  Mehl\r\n\r\n"
            "MMMMM-----------ERFASST *RK* AM 24.11.00 VON-----------\r\n           Petra Holzapfel\r\n\r\n"
            "Den Leinsamen mit kochendem Wasser übergießen.\r\n\r\nMMMMM\r\n");
        CHECK(Importers::detect(mm) == Importers::Format::MealMaster, "RezKonv-Ausgabe im MealMaster-Format erkannt");
        const auto res = Importers::parseMealMaster(mm);
        const Recipe r = res.recipes.value(0);
        bool salt = false, kefir = false, holz = false;
        for (const Ingredient &i : r.ingredients) {
            if (i.name == QStringLiteral("Salz")) salt = true;
            if (i.food == QStringLiteral("joghurt")) kefir = true;
            if (i.name.contains(QStringLiteral("Holzapfel"))) holz = true;
        }
        CHECK(res.recipes.size() == 1 && r.title == QStringLiteral("Leinsamenbrot #1") && salt && kefir && !holz
                  && r.source.contains(QStringLiteral("Petra Holzapfel")),
              "Titel, „;Salz“, Kefir, Erfasser als Quelle: " + r.title + " / " + r.source);
        out << "  " << r.title << " – " << r.ingredients.size() << " Zeilen, Quelle: " << r.source << "\n";
    }

    out << "\nDoubletten:\n";
    {
        QTemporaryDir tmpd;
        RecipeDb &db = RecipeDb::instance();
        CHECK(db.create(tmpd.filePath(QStringLiteral("dup")), QStringLiteral("Dup")), "Doubletten-Datenbank");
        auto make = [&](const QString &title, const QString &ings, bool fav = false, bool image = false) {
            Recipe r;
            r.title = title;
            r.servings = 4;
            r.ingredients = IngredientParser::parseText(ings);
            r.instructions = QStringLiteral("Alles verrühren und backen.");
            r.favorite = fav;
            if (image) r.image = QStringLiteral("iVBORw0KGgo=");
            Nutrition::analyze(r);
            db.save(r);
            return r.id;
        };
        const QString cake = QStringLiteral("250 g Magerquark\n100 g Zucker\n3 Eier\n1 Zitrone\n200 g Mehl\n100 g Butter");
        const QString k1 = make(QStringLiteral("Käsekuchen"), cake);
        const QString k2 = make(QStringLiteral("KAESEKUCHEN!"), cake, true);
        const QString k3 = make(QStringLiteral("Käsekuchen"), cake, false, true);
        const QString a1 = make(QStringLiteral("Apfelkuchen vom Blech"), QStringLiteral("1 kg Äpfel\n300 g Mehl\n150 g Butter\n150 g Zucker\n2 Eier\n1 TL Zimt"));
        const QString a2 = make(QStringLiteral("Omas Apfelkuchen vom Blech"), QStringLiteral("1 kg Äpfel\n300 g Mehl\n150 g Butter\n150 g Zucker\n2 Eier"));
        const QString p1 = make(QStringLiteral("Pfannkuchen"), QStringLiteral("250 g Mehl\n500 ml Milch\n3 Eier\n1 Prise Salz\n2 EL Zucker\n2 EL Butter\n1 Päckchen Vanillezucker"));
        const QString p2 = make(QStringLiteral("Eierkuchen nach Art des Hauses"), QStringLiteral("250 g Mehl\n500 ml Milch\n3 Eier\n1 Prise Salz\n2 EL Zucker\n2 EL Butter\n1 Päckchen Vanillezucker"));
        make(QStringLiteral("Linsensuppe"), QStringLiteral("250 g Linsen\n1 Zwiebel\n2 Möhren\n1 l Gemüsebrühe"));
        make(QStringLiteral("Linsensalat"), QStringLiteral("250 g Linsen\n1 Paprikaschote\n2 EL Essig\n3 EL Öl"));
        CHECK(Duplicates::titleKey(QStringLiteral("Käsekuchen")) == Duplicates::titleKey(QStringLiteral("KAESEKUCHEN!")), "Titel vereinheitlicht");
        QList<Duplicates::Group> g = Duplicates::find(db.summaries());
        auto groupOf = [&](const QString &id) -> const Duplicates::Group * {
            for (const Duplicates::Group &x : g)
                if (x.ids.contains(id)) return &x;
            return nullptr;
        };
        const Duplicates::Group *gk = groupOf(k1);
        CHECK(gk && gk->ids.size() == 3 && gk->ids.contains(k2) && gk->ids.contains(k3) && (gk->reasons & Duplicates::SameTitle),
              "drei Käsekuchen in einer Gruppe");
        CHECK(gk && gk->ids.first() != k1, "vollständigstes Rezept (Favorit/Bild) zuerst");
        const Duplicates::Group *ga = groupOf(a1);
        CHECK(ga && ga->ids.contains(a2) && (ga->reasons & Duplicates::SimilarTitle), "„Omas Apfelkuchen vom Blech“ ähnlich");
        const Duplicates::Group *gp = groupOf(p1);
        CHECK(gp && gp->ids.contains(p2) && (gp->reasons & Duplicates::SameIngredients), "Pfannkuchen/Eierkuchen: gleiche Zutaten");
        CHECK(g.size() == 3, "keine Fehltreffer (Linsensuppe/Linsensalat): " + QString::number(g.size()));
        CHECK(Duplicates::find(db.summaries(), {}, Duplicates::Strictness::TitleOnly).size() == 1, "streng: nur gleiche Titel");
        // als „keine Doubletten“ markieren
        QSet<QString> ign{ Duplicates::pairKey(p1, p2) };
        CHECK(Duplicates::saveIgnored(ign) && Duplicates::loadIgnored() == ign, "Ausnahmen speichern");
        g = Duplicates::find(db.summaries(), Duplicates::loadIgnored());
        CHECK(g.size() == 2, "markiertes Paar wird nicht mehr gemeldet");
        // zusammenführen
        Recipe keep, o1, o2;
        db.load(k1, &keep);
        db.load(k2, &o1);
        db.load(k3, &o2);
        keep.tags.clear();
        o1.tags = { QStringLiteral("backen"), QStringLiteral("omas rezepte") };
        const QStringList merged = Duplicates::mergeInto(keep, { o1, o2 });
        CHECK(keep.favorite && !keep.image.isEmpty() && keep.hasTag(QStringLiteral("omas rezepte")) && merged.size() >= 3,
              "Zusammenführen: Favorit, Bild, Tags übernommen");
        out << "  Gruppen: " << g.size() << ", übernommen: " << merged.join(QStringLiteral("; ")) << "\n";
        db.close();
    }

    out << "\nNetzwerkscanner (eSCL):\n";
    {
        CHECK(Escl::baseUrl(QStringLiteral("192.168.178.101")).toString() == QStringLiteral("http://192.168.178.101/eSCL"), "eSCL-Adresse aus IP");
        CHECK(Escl::baseUrl(QStringLiteral("https://drucker.fritz.box/eSCL/")).toString() == QStringLiteral("https://drucker.fritz.box/eSCL"),
              "eSCL-Adresse aus URL");
        const QByteArray capsXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?><scan:ScannerCapabilities xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\" "
            "xmlns:pwg=\"http://www.pwg.org/schemas/2010/12/sm\"><pwg:Version>2.63</pwg:Version><pwg:MakeAndModel>EPSON ET-2870 Series</pwg:MakeAndModel>"
            "<scan:Platen><scan:PlatenInputCaps><scan:MaxWidth>2550</scan:MaxWidth><scan:MaxHeight>3508</scan:MaxHeight>"
            "<scan:SettingProfiles><scan:SettingProfile><scan:ColorModes><scan:ColorMode>BlackAndWhite1</scan:ColorMode><scan:ColorMode>Grayscale8</scan:ColorMode>"
            "<scan:ColorMode>RGB24</scan:ColorMode></scan:ColorModes><scan:DocumentFormats><pwg:DocumentFormat>application/pdf</pwg:DocumentFormat>"
            "<pwg:DocumentFormat>image/jpeg</pwg:DocumentFormat><scan:DocumentFormatExt>image/jpeg</scan:DocumentFormatExt></scan:DocumentFormats>"
            "<scan:SupportedResolutions><scan:DiscreteResolutions><scan:DiscreteResolution><scan:XResolution>100</scan:XResolution><scan:YResolution>100</scan:YResolution></scan:DiscreteResolution>"
            "<scan:DiscreteResolution><scan:XResolution>200</scan:XResolution><scan:YResolution>200</scan:YResolution></scan:DiscreteResolution>"
            "<scan:DiscreteResolution><scan:XResolution>300</scan:XResolution><scan:YResolution>300</scan:YResolution></scan:DiscreteResolution>"
            "<scan:DiscreteResolution><scan:XResolution>600</scan:XResolution><scan:YResolution>600</scan:YResolution></scan:DiscreteResolution>"
            "</scan:DiscreteResolutions></scan:SupportedResolutions></scan:SettingProfile></scan:SettingProfiles></scan:PlatenInputCaps></scan:Platen>"
            "</scan:ScannerCapabilities>";
        const Escl::Capabilities caps = Escl::parseCapabilities(capsXml);
        CHECK(caps.makeAndModel == QStringLiteral("EPSON ET-2870 Series") && caps.version == QStringLiteral("2.63") && caps.resolutions.size() == 4
                  && caps.colorModes.contains(QStringLiteral("RGB24")) && Escl::chooseFormat(caps) == QStringLiteral("image/jpeg"),
              "eSCL-Fähigkeiten lesen");
        CHECK(Escl::chooseResolution(caps, 300) == 300 && Escl::chooseResolution(caps, 250) == 300, "Auflösung wählen");
        const QByteArray settings = Escl::scanSettings(caps, true, 300);
        CHECK(settings.contains("<scan:ColorMode>RGB24</scan:ColorMode>") && settings.contains("<pwg:DocumentFormat>image/jpeg")
                  && settings.contains("<pwg:InputSource>Platen</pwg:InputSource>"),
              "ScanSettings");

        // kleiner eSCL-Server: Fähigkeiten, Auftrag, einmal „noch nicht fertig“ (503), dann das Bild
        QImage page(850, 1170, QImage::Format_RGB32);
        page.fill(Qt::white);
        QByteArray jpeg;
        {
            QBuffer b(&jpeg);
            b.open(QIODevice::WriteOnly);
            page.save(&b, "JPEG");
        }
        QTcpServer server;
        server.listen(QHostAddress::LocalHost);
        int busyOnce = 1;
        QByteArray postedBody;
        QObject::connect(&server, &QTcpServer::newConnection, [&] {
            QTcpSocket *sock = server.nextPendingConnection();
            auto buf = std::make_shared<QByteArray>();
            QObject::connect(sock, &QTcpSocket::readyRead, sock, [&, sock, buf] {
                *buf += sock->readAll();
                const int he = int(buf->indexOf("\r\n\r\n"));
                if (he < 0) return;
                const QByteArray head = buf->left(he);
                int len = 0;
                for (const QByteArray &l : head.split('\n'))
                    if (l.toLower().startsWith("content-length:")) len = l.mid(15).trimmed().toInt();
                if (buf->size() < he + 4 + len) return;
                const QByteArray body = buf->mid(he + 4, len);
                buf->remove(0, he + 4 + len);
                QByteArray status = "200 OK", type = "text/xml", answer, extra;
                if (head.startsWith("GET /eSCL/ScannerCapabilities")) {
                    answer = capsXml;
                } else if (head.startsWith("POST /eSCL/ScanJobs")) {
                    postedBody = body;
                    status = "201 Created";
                    extra = "Location: http://127.0.0.1:" + QByteArray::number(server.serverPort()) + "/eSCL/ScanJobs/42\r\n";
                } else if (head.startsWith("GET /eSCL/ScanJobs/42/NextDocument")) {
                    if (busyOnce-- > 0) status = "503 Service Unavailable";
                    else {
                        type = "image/jpeg";
                        answer = jpeg;
                    }
                } else {
                    status = "404 Not Found";
                }
                sock->write("HTTP/1.1 " + status + "\r\nContent-Type: " + type + "\r\n" + extra + "Content-Length: " + QByteArray::number(answer.size())
                            + "\r\n\r\n" + answer);
            });
        });
        const QString addr = QStringLiteral("127.0.0.1:%1").arg(server.serverPort());
        Escl::Scanner sc;
        QEventLoop loop;
        QString model, err;
        bool ok = false;
        QObject::connect(&sc, &Escl::Scanner::checked, &loop, [&](bool o, const QString &m, const QString &e) {
            ok = o;
            model = m;
            err = e;
            loop.quit();
        });
        sc.check(addr);
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        loop.exec();
        CHECK(ok && model == QStringLiteral("EPSON ET-2870 Series"), "eSCL: Verbindung prüfen " + err);
        QImage got;
        QObject::connect(&sc, &Escl::Scanner::finished, &loop, [&](const QImage &img) {
            got = img;
            loop.quit();
        });
        QObject::connect(&sc, &Escl::Scanner::failed, &loop, [&](const QString &e) {
            err = e;
            loop.quit();
        });
        sc.scan(addr, true, 300);
        QTimer::singleShot(15000, &loop, &QEventLoop::quit);
        loop.exec();
        CHECK(got.size() == page.size(), "eSCL: Seite gescannt (mit 503-Wartezeit) " + err);
        CHECK(postedBody.contains("<scan:XResolution>300</scan:XResolution>") && postedBody.contains("<pwg:Version>2.63</pwg:Version>"),
              "eSCL: Auftrag mit 300 dpi");
        // nicht erreichbare Adresse
        bool failedOk = false;
        Escl::Scanner bad;
        QObject::connect(&bad, &Escl::Scanner::checked, &loop, [&](bool o, const QString &, const QString &e) {
            failedOk = !o && !e.isEmpty();
            loop.quit();
        });
        bad.check(QStringLiteral("127.0.0.1:1"));
        QTimer::singleShot(15000, &loop, &QEventLoop::quit);
        loop.exec();
        CHECK(failedOk, "eSCL: Fehlermeldung bei falscher Adresse");
        out << "  eSCL-Testserver: " << model << ", Bild " << got.width() << "×" << got.height() << "\n";
    }

    // ---- eigene Lebensmittel: fehlende Zutaten, Energie aus Nährstoffen
    out << "\n== Eigene Lebensmittel / fehlende Zutaten ==\n";
    {
        Nutrients n;
        n.fat = 10; n.carbs = 20; n.protein = 5; n.fiber = 2;
        CHECK(std::fabs(Nutrition::energyFromMacros(n) - 194) < 1e-9, "Energie aus Nährstoffen");
        FoodTable &ft = FoodTable::instance();
        ft.removeUserFood(QStringLiteral("u_buttaris_test"));
        Recipe r;
        r.title = QStringLiteral("Testkuchen");
        r.servings = 4;
        r.ingredients = IngredientParser::parseText(QStringLiteral("200 g Mehl\n100 g Buttaris Test\n50 g Buttaris Test, zerlassen\n1 Pck. Zauberpulver Xyz"));
        Nutrition::analyze(r);
        auto miss = Nutrition::missingFoods({ r });
        CHECK(miss.size() == 2 && miss.first().count == 2 && miss.first().name.startsWith(QStringLiteral("Buttaris")), "fehlende Zutaten gruppiert");
        const QString normXyz = FoodTable::normalize(QStringLiteral("Zauberpulver Xyz"));
        CHECK(Nutrition::missingFoods({ r }, { normXyz }).size() == 1, "ausgeblendete Zutat");
        Food f;
        f.key = QStringLiteral("u_buttaris_test");
        f.name = QStringLiteral("Buttaris Test");
        f.category = QStringLiteral("fett");
        f.per100.kcal = 900; f.per100.fat = 99.8;
        f.aliases = { QStringLiteral("buttaris test") };
        f.source = QStringLiteral("eigene Eingabe");
        const QString key = ft.saveUserFood(f);
        CHECK(key == QLatin1String("u_buttaris_test"), "saveUserFood gibt Schlüssel zurück");
        Nutrition::analyze(r);
        CHECK(Nutrition::missingFoods({ r }).size() == 1, "nach dem Anlegen nur noch eine fehlende Zutat");
        CHECK(std::fabs(r.nutrition.total.kcal - (200 * 3.4 + 150 * 9.0)) < 40, "kcal mit eigenem Lebensmittel " + QString::number(r.nutrition.total.kcal));

        // ---- Rezeptpaket: Hin und zurück
        out << "\n== Rezeptpaket (.vmpack) ==\n";
        r.id = Recipe::newId();
        r.favorite = true;
        r.image = QStringLiteral("iVBORw0KGgo=");
        ft.learn(QStringLiteral("Zauberpulver Xyz"), QStringLiteral("zucker"));
        Nutrition::analyze(r);
        Recipe r2;
        r2.id = Recipe::newId();
        r2.title = QStringLiteral("Zweites Rezept – Ümlaut");
        r2.ingredients = IngredientParser::parseText(QStringLiteral("1 l Milch"));
        Nutrition::analyze(r2);
        RecipePackage::Options o;
        o.name = QStringLiteral("Testsammlung");
        o.author = QStringLiteral("Tester");
        int foods = 0;
        const QByteArray zip = RecipePackage::build({ r, r2 }, o, &foods);
        CHECK(foods == 1, "Paket: ein eigenes Lebensmittel");
        RecipePackage::Content c;
        QString err;
        CHECK(RecipePackage::parse(zip, &c, &err), "Paket lesen: " + err);
        CHECK(c.recipes.size() == 2 && c.name == QLatin1String("Testsammlung") && c.author == QLatin1String("Tester"), "Paket: Kopfdaten und Rezepte");
        bool found = false;
        for (const Recipe &x : c.recipes)
            if (x.title == r.title) {
                found = true;
                CHECK(!x.favorite && x.image == r.image && x.ingredients.size() == 4, "Paket: Favorit entfernt, Foto und Zutaten erhalten");
            }
        CHECK(found, "Paket: Rezept wiedergefunden");
        CHECK(c.foods.size() == 1 && c.foods.first().key == QLatin1String("u_buttaris_test") && c.foods.first().source.contains(QStringLiteral("Rezeptpaket")),
              "Paket: Lebensmittel mit Herkunft");
        CHECK(c.learned.value(normXyz) == QLatin1String("zucker"), "Paket: gelernte Zuordnung");
        const auto files = RecipePackage::unzip(zip, &err);
        CHECK(files.contains(QStringLiteral("viviameals-paket.json")) && files.size() == 4, "Paket: Dateien im Archiv");
        for (auto it = files.begin(); it != files.end(); ++it)
            CHECK(!it.key().contains(QStringLiteral("profil")) && !it.key().contains(QStringLiteral("tagebuch")) && !it.key().contains(QStringLiteral("plan")),
                  "Paket ohne Benutzerdaten: " + it.key());
        // ohne Fotos, ohne Lebensmittel
        o.images = false;
        o.foods = false;
        RecipePackage::parse(RecipePackage::build({ r }, o), &c, &err);
        CHECK(c.recipes.size() == 1 && c.recipes.first().image.isEmpty() && c.foods.isEmpty() && c.learned.isEmpty(), "Paket ohne Fotos/Lebensmittel");
        // beschädigtes Paket
        QByteArray broken = zip;
        broken[200] = char(broken[200] ^ 0x55);
        CHECK(!RecipePackage::parse(broken, &c, &err) && !err.isEmpty(), "beschädigtes Paket erkannt: " + err);
        CHECK(!RecipePackage::parse(QByteArray("kein zip"), &c, &err), "kein ZIP erkannt");
        // Empfänger: Lebensmittel übernehmen, vorhandene nicht überschreiben
        RecipePackage::parse(zip, &c, &err);
        ft.removeUserFood(QStringLiteral("u_buttaris_test"));
        ft.learn(QStringLiteral("Zauberpulver Xyz"), QString());
        int learned = 0;
        CHECK(RecipePackage::importFoods(c, &learned) == 1 && learned == 1, "Lebensmittel und Zuordnung übernommen");
        CHECK(RecipePackage::importFoods(c, &learned) == 0 && learned == 0, "zweites Mal nichts doppelt");
        // mit Unzip-Programm lesbar?
        QTemporaryDir td;
        QFile zf(td.filePath(QStringLiteral("t.vmpack")));
        zf.open(QIODevice::WriteOnly);
        zf.write(zip);
        zf.close();
        if (!QStandardPaths::findExecutable(QStringLiteral("unzip")).isEmpty()) {
            const int rc = QProcess::execute(QStringLiteral("unzip"), { QStringLiteral("-tq"), zf.fileName() });
            CHECK(rc == 0, "unzip -t akzeptiert das Paket");
        }
        ft.removeUserFood(QStringLiteral("u_buttaris_test"));
        ft.learn(QStringLiteral("Zauberpulver Xyz"), QString());
        out << "  Paket: " << zip.size() << " Bytes, " << files.size() << " Dateien\n";
    }

    // ---- LaTeX: Kapitel, Master-Dokument, Eintragen, Stichwörter, Quelle
    out << "\nLaTeX:\n";
    {
        Recipe r;
        r.id = Recipe::newId();
        r.title = QStringLiteral("Äpfel im Schlafrock");
        r.servings = 4;
        r.source = QStringLiteral("Gustav Knuth");
        {
            QImage photo(320, 240, QImage::Format_RGB32);
            photo.fill(QColor(QStringLiteral("#1474BC")));
            QByteArray b;
            QBuffer buf(&b);
            buf.open(QIODevice::WriteOnly);
            photo.save(&buf, "PNG");
            r.image = QString::fromLatin1(b.toBase64());
        }
        r.ingredients = IngredientParser::parseText(QStringLiteral("4 Äpfel\n1 Pck. Blätterteig\n50 g Rosinen \"Sultaninen\"\nSalz"));
        r.instructions = QStringLiteral("Backofen auf 200° C vorheizen. Äpfel schälen & entkernen.\n\nIm Teig 25 Minuten backen – fertig!");
        Nutrition::analyze(r);
        LatexExport::Options o;
        QString ch = LatexExport::toLatex(r, o);
        CHECK(o.mode == LatexExport::Mode::Chapter && !ch.contains(QStringLiteral("\\documentclass")) && ch.contains(QStringLiteral("\\chapter{Äpfel im Schlafrock}")),
              "Kapitel ist Vorgabe");
        CHECK(!ch.contains(QStringLiteral("Gustav Knuth")), "Quelle standardmäßig unterdrückt");
        CHECK(ch.contains(QStringLiteral("\\index{apfel im schlafrock@Äpfel im Schlafrock}")) && ch.contains(QStringLiteral("\\index{blatterteig@Blätterteig!apfel im schlafrock@Äpfel im Schlafrock}"))
                  && !ch.contains(QStringLiteral("\\index{salz")),
              "Stichwörter mit deutscher Sortierung");
        CHECK(ch.contains(QStringLiteral("\\markright{Zutaten}")), "Abschnitt für die Kopfzeile");
        o.source = true;
        CHECK(LatexExport::toLatex(r, o).contains(QStringLiteral("Gustav Knuth")), "Quelle auf Wunsch");
        CHECK(LatexExport::indexEntry(QStringLiteral("Mehl \"Typ 405\" | fein!")).contains(QStringLiteral("\"|")), "makeindex-Zeichen maskiert");
        QTemporaryDir td;
        if (qEnvironmentVariableIsSet("VIVIA_TEX_KEEP")) td.setAutoRemove(false);
        const QString dir = td.path();
        if (qEnvironmentVariableIsSet("VIVIA_TEX_KEEP")) out << "  LaTeX-Testordner: " << dir << "\n";
        QDir().mkpath(dir + QStringLiteral("/rezepte"));
        o.source = false;
        for (int cls = 0; cls < 2; ++cls) {
            LatexExport::MasterOptions m;
            m.docClass = cls ? LatexExport::DocClass::Book : LatexExport::DocClass::ScrBook;
            o.docClass = m.docClass;
            m.title = QStringLiteral("Omas Küche");
            m.subtitle = QStringLiteral("Süßes & Herzhaftes");
            m.author = QStringLiteral("Micha B.");
            QImage bg(400, 560, QImage::Format_RGB32);
            bg.fill(QColor(QStringLiteral("#E0620D")));
            bg.save(dir + QStringLiteral("/bg.png"));
            m.halfTitleImage = dir + QStringLiteral("/bg.png");
            if (cls == 1) {
                // wie im Beispiel des Nutzers: Bild frei auf der Titelseite, 139,9 mm breit, links 46,4 mm, 60,5 mm hoch
                m.bgTarget = LatexExport::MasterOptions::BgTarget::TitlePage;
                m.bgPlacement = LatexExport::MasterOptions::BgPlacement::Free;
                m.bgWidthMm = 139.9;
                m.bgLeftMm = 46.4;
                m.bgBottomMm = 60.5;
                QImage shot(500, 300, QImage::Format_RGB32);
                shot.fill(Qt::white);
                QPainter pt(&shot);
                pt.fillRect(40, 30, 400, 200, QColor(QStringLiteral("#1474BC")));
                pt.end();
                shot.save(dir + QStringLiteral("/shot.png"));
                m.halfTitleImage = dir + QStringLiteral("/shot.png");
                const QImage trimmed = LatexExport::trimWhite(shot);
                CHECK(trimmed.size() == QSize(400, 200), "weißen Rand abschneiden");
            }
            const QString master = QStringLiteral("%1/kochbuch%2.tex").arg(dir).arg(cls);
            // scrbook: Rezept als \section im Themen-Kapitel „Nachtisch“, Hintergrundbild zugeschnitten
            o.heading = cls == 0 ? LatexExport::Heading::Section : LatexExport::Heading::Chapter;
            if (cls == 0) {
                m.firstChapter = QStringLiteral("Nachtisch");
                m.bgCrop = QRectF(0.1, 0.1, 0.8, 0.8);
            }
            const QString recipeFile = dir + QStringLiteral("/rezepte/aepfel.tex");
            o.imagePrefix = QStringLiteral("rezepte/");
            CHECK(LatexExport::exportRecipe(r, recipeFile, o), "Kapitel schreiben");
            QString err;
            CHECK(LatexExport::writeMaster(master, m, {}, &err), "Master schreiben " + err);
            const QString in = LatexExport::relativeInput(master, recipeFile);
            CHECK(in == QLatin1String("rezepte/aepfel"), "relativer Pfad " + in);
            bool already = false;
            CHECK(LatexExport::addToMaster(master, in, &err, &already) && !already, "eintragen " + err);
            CHECK(LatexExport::addToMaster(master, in, &err, &already) && already, "nicht doppelt eintragen");
            QFile mf(master);
            mf.open(QIODevice::ReadOnly);
            const QString mt = QString::fromUtf8(mf.readAll());
            CHECK(mt.indexOf(QStringLiteral("\\input{rezepte/aepfel}")) < mt.indexOf(LatexExport::masterMarker()) && mt.count(QStringLiteral("\\input{rezepte/aepfel}")) == 1,
                  "Eintrag vor der Marke");
            if (cls == 1)
                CHECK(mt.contains(QStringLiteral("\\BindungsHSpace{46.4mm}")) && mt.contains(QStringLiteral("\\raisebox{60.5mm}"))
                          && mt.contains(QStringLiteral("width=139.9mm]{titelseite-hintergrund}")) && mt.contains(QStringLiteral("\\setlength{\\vmBindekorrektur}{12mm}"))
                          && mt.indexOf(QStringLiteral("titelseite-hintergrund")) > mt.indexOf(QStringLiteral("Titelseite")),
                      "frei positioniertes Bild auf der Titelseite");
            if (cls == 0) CHECK(mt.contains(QStringLiteral("\\chapter{Nachtisch}\n\\input{rezepte/aepfel}")), "Themen-Kapitel im neuen Master");
            CHECK(mt.contains(QStringLiteral("\\date{}")) && mt.contains(cls ? QStringLiteral("titelseite-hintergrund") : QStringLiteral("schmutztitel-hintergrund")) && mt.contains(QStringLiteral("\\printindex"))
                      && mt.contains(QStringLiteral("\\input{glossar}")) && mt.contains(cls ? QStringLiteral("bindingoffset=12mm") : QStringLiteral("BCOR=12mm")),
                  "Master-Inhalt");
            // optional übersetzen (latexmk vorhanden und VIVIA_TEX_TEST gesetzt)
            if (qEnvironmentVariableIsSet("VIVIA_TEX_TEST") && !QStandardPaths::findExecutable(QStringLiteral("latexmk")).isEmpty()) {
                QStringList engines{ QStringLiteral("-pdf") };
                if (qEnvironmentVariableIsSet("VIVIA_TEX_LUA")) engines << QStringLiteral("-lualatex");   // braucht OpenType-Latin-Modern
                for (const QString &engine : engines) {
                    QProcess p;
                    p.setWorkingDirectory(dir);
                    p.start(QStringLiteral("latexmk"), { engine, QStringLiteral("-interaction=nonstopmode"), QStringLiteral("-halt-on-error"), QFileInfo(master).fileName() });
                    p.waitForFinished(240000);
                    CHECK(p.exitCode() == 0, QStringLiteral("latexmk %1 %2").arg(engine, master) + QString::fromLocal8Bit(p.readAllStandardOutput().right(1500)));
                    const QString pdf = QFileInfo(master).completeBaseName() + QStringLiteral(".pdf");
                    if (engine == QLatin1String("-pdf")) {
                        QFile::remove(QStringLiteral("/tmp/kochbuch-test%1.pdf").arg(cls));
                        QFile::copy(QDir(dir).filePath(pdf), QStringLiteral("/tmp/kochbuch-test%1.pdf").arg(cls));
                    }
                    QProcess::execute(QStringLiteral("latexmk"), { QStringLiteral("-C"), QStringLiteral("-cd"), master });
                }
                out << "  " << QFileInfo(master).fileName() << " übersetzt (" << engines.join(QStringLiteral(", ")) << ")\n";
            }
        }
    }

    // ---- LaTeX: Rezepte als \section in Themen-Kapiteln, Hintergrundbild-Zuschnitt
    out << "\nLaTeX-Themenkapitel:\n";
    {
        QTemporaryDir td;
        const QString master = td.filePath(QStringLiteral("buch.tex"));
        LatexExport::MasterOptions m;
        m.title = QStringLiteral("Test");
        m.firstChapter = QStringLiteral("Suppen & Eintöpfe");
        QString err;
        CHECK(LatexExport::writeMaster(master, m, { QStringLiteral("rezepte/linsensuppe") }, &err), "Master " + err);
        bool already = false, created = false;
        CHECK(LatexExport::addToMaster(master, QStringLiteral("rezepte/gulasch"), &err, &already, QStringLiteral("Hauptgerichte"), &created) && created,
              "neues Themen-Kapitel");
        CHECK(LatexExport::addToMaster(master, QStringLiteral("rezepte/erbsensuppe"), &err, &already, QStringLiteral("suppen & eintöpfe"), &created) && !created,
              "vorhandenes Kapitel (Groß/klein egal)");
        CHECK(LatexExport::addToMaster(master, QStringLiteral("rezepte/rouladen"), &err, &already, QStringLiteral("Hauptgerichte"), &created) && !created,
              "zweites Rezept im Kapitel");
        QFile f(master);
        f.open(QIODevice::ReadOnly);
        const QString t = QString::fromUtf8(f.readAll());
        const int suppen = t.indexOf(QStringLiteral("\\chapter{Suppen \\& Eintöpfe}")), linsen = t.indexOf(QStringLiteral("rezepte/linsensuppe")),
                  erbsen = t.indexOf(QStringLiteral("rezepte/erbsensuppe")), haupt = t.indexOf(QStringLiteral("\\chapter{Hauptgerichte}")),
                  gulasch = t.indexOf(QStringLiteral("rezepte/gulasch")), rouladen = t.indexOf(QStringLiteral("rezepte/rouladen")),
                  marker = t.indexOf(LatexExport::masterMarker());
        CHECK(suppen >= 0 && suppen < linsen && linsen < erbsen && erbsen < haupt && haupt < gulasch && gulasch < rouladen && rouladen < marker,
              "Reihenfolge im Master");
        CHECK(LatexExport::masterChapters(t) == QStringList({ QStringLiteral("Suppen & Eintöpfe"), QStringLiteral("Hauptgerichte") }), "Kapitel lesen");
        Recipe r;
        r.title = QStringLiteral("Kartoffelsuppe");
        r.tags = { QStringLiteral("vegetarisch") };
        CHECK(LatexExport::suggestChapter(r) == QStringLiteral("Suppen"), "Kapitelvorschlag Suppe");
        r.title = QStringLiteral("Mousse au Chocolat");
        r.tags = { QStringLiteral("desserts") };
        CHECK(LatexExport::suggestChapter(r) == QStringLiteral("Nachtisch"), "Kapitelvorschlag Nachtisch");
        LatexExport::Options o;
        o.heading = LatexExport::Heading::Section;
        const QString sec = LatexExport::toLatex(r, o);
        CHECK(sec.contains(QStringLiteral("\\ifnum\\value{section}>0 \\clearpage\\fi\n\\section{Mousse au Chocolat}")) && sec.contains(QStringLiteral("\\subsection*{Zubereitung}"))
                  && !sec.contains(QStringLiteral("\\chapter")) && !sec.contains(QStringLiteral("\\markright")),
              "Rezept als \\section");
        // Hintergrundbild: drehen, zuschneiden, Breite begrenzen
        QImage img(400, 200, QImage::Format_RGB32);
        img.fill(Qt::red);
        LatexExport::MasterOptions bm;
        bm.bgPlacement = LatexExport::MasterOptions::BgPlacement::Free;
        bm.bgTrim = false;
        bm.bgRotation = 90;
        bm.bgCrop = QRectF(0, 0.5, 1, 0.5);
        const QImage pb = LatexExport::prepareBackground(img, bm);
        CHECK(pb.size() == QSize(200, 200), "drehen und zuschneiden " + QString::number(pb.width()) + "x" + QString::number(pb.height()));
        bm.bgLeftMm = 40;
        bm.bindingMm = 12;
        CHECK(std::fabs(LatexExport::maxBackgroundWidth(bm, 0.5) - 158) < 0.01, "maximale Breite (frei)");
        bm.bgBottomMm = 250;   // nur noch 47 mm Höhe frei
        CHECK(std::fabs(LatexExport::maxBackgroundWidth(bm, 1.0) - 47) < 0.01, "maximale Breite durch Höhe begrenzt");
    }

    // ---- Celsius-Schreibweise
    out << "\nCelsius:\n";
    {
        const QList<QPair<QString, QString>> cases = {
            { QStringLiteral("bei 200GradC backen"), QStringLiteral("bei 200° C backen") },
            { QStringLiteral("200 Grad C"), QStringLiteral("200° C") },
            { QStringLiteral("auf 180-200 GRADC."), QStringLiteral("auf 180-200° C.") },
            { QStringLiteral("175 Grad Celsius (Umluft 160GradC)"), QStringLiteral("175° C (Umluft 160° C)") },
            { QStringLiteral("200 °C, 220°C, 90ºC"), QStringLiteral("200° C, 220° C, 90° C") },
            { QStringLiteral("200° C bleibt"), QStringLiteral("200° C bleibt") },
            { QStringLiteral("um 90 Grad drehen, 3 Grad Celsiustemperatur"), QStringLiteral("um 90 Grad drehen, 3 Grad Celsiustemperatur") },
            { QStringLiteral("2 Gradcakes"), QStringLiteral("2 Gradcakes") },
        };
        for (const auto &c : cases) {
            const QString got = Units::normalizeCelsius(c.first);
            CHECK(got == c.second, c.first + " → " + got);
        }
        CHECK(Units::convertTextMeasures(QStringLiteral("Bake at 350 F")) == QStringLiteral("Bake at 175° C"), "Fahrenheit → ° C");
        const Importers::Result res = Importers::parseRezKonv(QStringLiteral(
            "=========== REZKONV-Rezept - RezkonvSuite v1.4\n\n     Titel: Testauflauf\n Kategorien: Auflauf\n     Menge: 2 Portionen\n\n"
            "      500 g  Kartoffeln\n\nIm Ofen bei 200GradC 30 Minuten backen.\n\n=====\n"));
        CHECK(res.recipes.size() == 1 && res.recipes.first().instructions.contains(QStringLiteral("bei 200° C 30 Minuten")),
              "RezKonv-Import: " + (res.recipes.isEmpty() ? QString() : res.recipes.first().instructions));
    }

    // ---- Open Food Facts
    out << "\nOpen Food Facts:\n";
    {
        using namespace OpenFoodFacts;
        CHECK(validGtin(QStringLiteral("3017620422003")) && validGtin(QStringLiteral("96385074")) && validGtin(QStringLiteral("036000291452")),
              "GTIN gültig (EAN-13, EAN-8, UPC-A)");
        CHECK(!validGtin(QStringLiteral("3017620422004")), "falsche Prüfziffer");
        QString err;
        CHECK(normalizeBarcode(QStringLiteral(" 3 017620 422003 "), &err) == QLatin1String("3017620422003"), "Barcode mit Leerzeichen");
        CHECK(normalizeBarcode(QStringLiteral("12345"), &err).isEmpty() && err.contains(QStringLiteral("Ziffern")), "zu kurz: " + err);
        CHECK(normalizeBarcode(QStringLiteral("3017620422004"), &err).isEmpty() && err.contains(QStringLiteral("Prüfziffer")), "Prüfziffer: " + err);
        bool ml = false;
        CHECK(parseQuantity(QStringLiteral("6 x 125 g"), &ml) == 750 && !ml, "6 x 125 g");
        CHECK(parseQuantity(QStringLiteral("1,5 l"), &ml) == 1500 && ml, "1,5 l");
        CHECK(parseQuantity(QStringLiteral("0.5 kg")) == 500, "0.5 kg");
        CHECK(categoryFromTags({ QStringLiteral("en:spreads"), QStringLiteral("en:fats"), QStringLiteral("en:margarines") }) == QLatin1String("fett"), "Kategorie Fett");
        const QByteArray nutella = R"({"code":"3017620422003","status":1,"product":{"code":"3017620422003","product_name":"Nutella","product_name_de":"Nutella",
            "brands":"Ferrero,Nutella","quantity":"400 g","product_quantity":"400","product_quantity_unit":"g","serving_size":"15 g","serving_quantity":15,
            "categories_tags":["en:breakfasts","en:spreads","en:sweet-spreads","en:cocoa-and-hazelnuts-spreads","en:chocolate-spreads"],
            "nutriments":{"energy-kcal_100g":539,"energy-kj_100g":2252,"fat_100g":30.9,"saturated-fat_100g":10.6,"carbohydrates_100g":57.5,
            "sugars_100g":56.3,"fiber_100g":0,"proteins_100g":6.3,"salt_100g":0.107}}})";
        Product p;
        CHECK(parseProductReply(nutella, &p, &err), "Nutella lesen: " + err);
        CHECK(p.name == QLatin1String("Nutella") && p.brand == QLatin1String("Ferrero") && p.packageAmount == 400 && !p.perMl && p.servingGrams == 15,
              "Produktdaten");
        CHECK(std::fabs(p.per100.kcal - 539) < 1e-9 && std::fabs(p.per100.sugar - 56.3) < 1e-9 && std::fabs(p.per100.salt - 0.107) < 1e-9 && p.missing.isEmpty(),
              "Nährwerte je 100 g");
        // nur kJ und Natrium, Getränk in Litern, fehlende Ballaststoffe
        const QByteArray drink = R"({"status":1,"product":{"code":"4006381333931","product_name":"Apfelschorle","brands":"Test","quantity":"1 l",
            "categories_tags":["en:beverages"],"nutriments":{"energy_100g":105,"fat_100g":0,"carbohydrates_100g":"6,1","sugars_100g":6,"proteins_100g":0.1,"sodium_100g":0.004}}})";
        CHECK(parseProductReply(drink, &p, &err), "Schorle lesen");
        CHECK(p.perMl && p.packageAmount == 1000 && std::fabs(p.per100.kcal - 105 / 4.184) < 0.01 && std::fabs(p.per100.salt - 0.01) < 1e-9
                  && std::fabs(p.per100.carbs - 6.1) < 1e-9 && p.missing.contains(QStringLiteral("Ballaststoffe")) && p.category == QLatin1String("getraenk"),
              "kJ, Natrium, ml, Dezimalkomma, fehlende Werte");
        CHECK(!parseProductReply(R"({"code":"0000000000000","status":0,"status_verbose":"product not found"})", &p, &err) && err.contains(QStringLiteral("nicht erfasst")),
              "unbekanntes Produkt");
        const QList<Product> found = parseSearchReply(R"({"count":2,"products":[{"code":"1","product_name":"Buttaris","brands":"Tegut","quantity":"250 g",
            "nutriments":{"energy-kcal_100g":898,"fat_100g":99.8}},{"code":"","product_name":"ohne Code"}]})", &err);
        CHECK(found.size() == 1 && found.first().name == QLatin1String("Buttaris") && found.first().missing.contains(QStringLiteral("Salz")), "Suchergebnis");
        CHECK(productUrl(QStringLiteral("3017620422003")).toString().contains(QStringLiteral("/api/v2/product/3017620422003.json?fields=")), "Produkt-URL");
        CHECK(userAgent().startsWith("ViviaMeals/"), "User-Agent");

        // Abfrage gegen einen lokalen Testserver
        QTcpServer server;
        server.listen(QHostAddress::LocalHost);
        QByteArray lastPath, lastAgent;
        QObject::connect(&server, &QTcpServer::newConnection, [&] {
            QTcpSocket *sock = server.nextPendingConnection();
            auto buf = std::make_shared<QByteArray>();
            QObject::connect(sock, &QTcpSocket::readyRead, sock, [sock, buf, &lastPath, &lastAgent, &nutella] {
                *buf += sock->readAll();
                const int he = int(buf->indexOf("\r\n\r\n"));
                if (he < 0) return;
                const QList<QByteArray> lines = buf->left(he).split('\n');
                lastPath = lines.first().split(' ').value(1);
                for (const QByteArray &l : lines)
                    if (l.toLower().startsWith("user-agent:")) lastAgent = l.mid(11).trimmed();
                QByteArray status = "200 OK", body;
                if (lastPath.startsWith("/api/v2/product/3017620422003")) body = nutella;
                else if (lastPath.startsWith("/api/v2/product/")) {
                    status = "404 Not Found";
                    body = R"({"status":0,"status_verbose":"product not found"})";
                } else if (lastPath.startsWith("/cgi/search.pl")) body = R"({"products":[{"code":"3017620422003","product_name":"Nutella","nutriments":{"energy-kcal_100g":539}}]})";
                sock->write("HTTP/1.1 " + status + "\r\nContent-Type: application/json\r\nContent-Length: " + QByteArray::number(body.size())
                            + "\r\nConnection: close\r\n\r\n" + body);
                sock->disconnectFromHost();
            });
        });
        qputenv("VIVIA_OFF_URL", QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toLatin1());
        Client client;
        QEventLoop loop;
        Product got;
        QString failure;
        QList<Product> results;
        QObject::connect(&client, &Client::productFound, &loop, [&](const Product &x) { got = x; loop.quit(); });
        QObject::connect(&client, &Client::failed, &loop, [&](const QString &e) { failure = e; loop.quit(); });
        QObject::connect(&client, &Client::searchFinished, &loop, [&](const QList<Product> &l) { results = l; loop.quit(); });
        auto run = [&] { QTimer::singleShot(10000, &loop, &QEventLoop::quit); loop.exec(); };
        client.lookup(QStringLiteral("3017620422003"));
        run();
        CHECK(got.name == QLatin1String("Nutella") && lastAgent.startsWith("ViviaMeals/"), "Abfrage über HTTP: " + failure);
        client.lookup(QStringLiteral("4006381333931"));
        run();
        CHECK(failure.contains(QStringLiteral("nicht erfasst")), "404 → nicht erfasst: " + failure);
        client.search(QStringLiteral("Nutella Ferrero"));
        run();
        CHECK(results.size() == 1 && lastPath.contains("search_terms=Nutella"), "Namenssuche über HTTP");
        qunsetenv("VIVIA_OFF_URL");
        out << "  " << got.name << " (" << got.brand << "), " << got.quantity << ": " << qRound(got.per100.kcal) << " kcal/100 g\n";
    }

    out << "\n" << (g_fail ? QStringLiteral("%1 Fehler").arg(g_fail) : QStringLiteral("Alle Prüfungen bestanden")) << "\n";
    return g_fail ? 1 : 0;
}
