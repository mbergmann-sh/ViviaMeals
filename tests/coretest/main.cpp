// VitaMeals – Test des Kerns (ohne Fenster)
//   coretest            alle Prüfungen, Ausgabe der importierten Beispielrezepte
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
