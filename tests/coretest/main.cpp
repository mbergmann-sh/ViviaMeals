// VitaMeals – Test des Kerns (ohne Fenster)
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
#include "odtexport.h"
#include "mealtypes.h"
#include "deepl.h"
#include "glossary.h"

#include <QCoreApplication>
#include <QBuffer>
#include <QImage>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
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
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("VitaMealsTest"));
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
            CHECK(r.instructions.contains(QStringLiteral("180 °C")) && r.instructions.contains(QStringLiteral("heißen Brühe"))
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
        qputenv("VITAMEALS_DEEPL_URL", QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
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
            CHECK(r.instructions.contains(QStringLiteral("175 °C")) || r.instructions.contains(QStringLiteral("180 °C")), "°F vor der Übersetzung umgerechnet");
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
        qunsetenv("VITAMEALS_DEEPL_URL");
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
        out << "  Kandidaten ohne gesunde Vorschläge (F/M/A/Z): " << n[0] << "/" << n[1] << "/" << n[2] << "/" << n[3] << "\n";
        for (const QString &x : MealPlanner::notes(o)) out << "  Hinweis: " << x << "\n";
        MealPlan plan = MealPlanner::create(o);
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
        CHECK(n[0] >= 5 && n[3] >= 5, "Frühstück und Zwischenmahlzeiten verfügbar");
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

    out << "\n" << (g_fail ? QStringLiteral("%1 Fehler").arg(g_fail) : QStringLiteral("Alle Prüfungen bestanden")) << "\n";
    return g_fail ? 1 : 0;
}
