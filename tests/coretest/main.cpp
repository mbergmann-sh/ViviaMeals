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

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QTextStream>
#include <cmath>

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
        CHECK(foods.size() == 3 && foods[1].category == QLatin1String("sonstiges"), "ohne Kategorie und Ähnlichkeit: Standardkategorie");
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
    CHECK(db.duplicatesOf(Recipe{ QString(), QStringLiteral("  test ") }).size() == 1, "Doublette");
    db.close();
    CHECK(db.open(tmp.filePath(QStringLiteral("db"))) && db.count() == 1, "wieder öffnen");

    out << "\n" << (g_fail ? QStringLiteral("%1 Fehler").arg(g_fail) : QStringLiteral("Alle Prüfungen bestanden")) << "\n";
    return g_fail ? 1 : 0;
}
