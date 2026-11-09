// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "glossary.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace Glossary {

namespace {

// Zutaten (englisch klein → deutsch). Mehrwortausdrücke vor Einzelwörtern.
const QHash<QString, QString> &ingredients()
{
    static const QHash<QString, QString> d = [] {
        QHash<QString, QString> h;
        const char *pairs[][2] = {
            { "all-purpose flour", "Weizenmehl" }, { "all purpose flour", "Weizenmehl" }, { "plain flour", "Weizenmehl" },
            { "bread flour", "Weizenmehl Type 550" }, { "cake flour", "Weizenmehl Type 405" }, { "self-rising flour", "Mehl mit Backpulver" },
            { "whole wheat flour", "Weizenvollkornmehl" }, { "rye flour", "Roggenmehl" }, { "flour", "Mehl" },
            { "cornstarch", "Speisestärke" }, { "corn starch", "Speisestärke" }, { "baking powder", "Backpulver" },
            { "baking soda", "Natron" }, { "active dry yeast", "Trockenhefe" }, { "dry yeast", "Trockenhefe" }, { "yeast", "Hefe" },
            { "granulated sugar", "Zucker" }, { "white sugar", "Zucker" }, { "brown sugar", "brauner Zucker" },
            { "light brown sugar", "brauner Zucker" }, { "dark brown sugar", "dunkler brauner Zucker" },
            { "powdered sugar", "Puderzucker" }, { "confectioners sugar", "Puderzucker" }, { "confectioners' sugar", "Puderzucker" },
            { "icing sugar", "Puderzucker" }, { "sugar", "Zucker" }, { "honey", "Honig" }, { "maple syrup", "Ahornsirup" },
            { "corn syrup", "Glukosesirup" }, { "molasses", "Melasse" }, { "vanilla extract", "Vanilleextrakt" },
            { "vanilla", "Vanilleextrakt" }, { "almond extract", "Bittermandelaroma" }, { "cocoa powder", "Kakaopulver" },
            { "unsweetened cocoa", "Kakaopulver" }, { "cocoa", "Kakaopulver" }, { "chocolate chips", "Schokotropfen" },
            { "semisweet chocolate chips", "Zartbitter-Schokotropfen" }, { "semisweet chocolate", "Zartbitterschokolade" },
            { "semi-sweet chocolate", "Zartbitterschokolade" }, { "bittersweet chocolate", "Zartbitterschokolade" },
            { "unsweetened chocolate", "Bitterschokolade (ungesüßt)" }, { "milk chocolate", "Vollmilchschokolade" },
            { "white chocolate", "weiße Schokolade" }, { "chocolate", "Schokolade" }, { "gelatin", "Gelatine" },
            { "unsalted butter", "ungesalzene Butter" }, { "salted butter", "gesalzene Butter" }, { "butter", "Butter" },
            { "margarine", "Margarine" }, { "shortening", "Pflanzenfett" }, { "vegetable shortening", "Pflanzenfett" },
            { "lard", "Schweineschmalz" }, { "vegetable oil", "Pflanzenöl" }, { "canola oil", "Rapsöl" },
            { "olive oil", "Olivenöl" }, { "extra virgin olive oil", "natives Olivenöl extra" }, { "sesame oil", "Sesamöl" },
            { "peanut oil", "Erdnussöl" }, { "salad oil", "Speiseöl" }, { "oil", "Öl" }, { "whole milk", "Vollmilch" },
            { "skim milk", "Magermilch" }, { "milk", "Milch" }, { "buttermilk", "Buttermilch" },
            { "evaporated milk", "Kondensmilch" }, { "sweetened condensed milk", "gezuckerte Kondensmilch" },
            { "heavy cream", "Sahne" }, { "whipping cream", "Schlagsahne" }, { "heavy whipping cream", "Schlagsahne" },
            { "light cream", "Kaffeesahne" }, { "half-and-half", "Kaffeesahne" }, { "half and half", "Kaffeesahne" },
            { "sour cream", "saure Sahne" }, { "cream cheese", "Frischkäse" }, { "cream", "Sahne" }, { "yogurt", "Joghurt" },
            { "plain yogurt", "Naturjoghurt" }, { "greek yogurt", "griechischer Joghurt" }, { "cottage cheese", "Hüttenkäse" },
            { "ricotta cheese", "Ricotta" }, { "cheddar cheese", "Cheddar" }, { "sharp cheddar cheese", "würziger Cheddar" },
            { "cheddar", "Cheddar" }, { "parmesan cheese", "Parmesan" }, { "parmesan", "Parmesan" },
            { "mozzarella cheese", "Mozzarella" }, { "swiss cheese", "Emmentaler" }, { "monterey jack cheese", "Monterey Jack (Käse)" },
            { "feta cheese", "Feta" }, { "blue cheese", "Blauschimmelkäse" }, { "cheese", "Käse" }, { "eggs", "Eier" },
            { "egg", "Ei" }, { "egg yolks", "Eigelb" }, { "egg yolk", "Eigelb" }, { "egg whites", "Eiweiß" },
            { "egg white", "Eiweiß" }, { "rolled oats", "Haferflocken" }, { "quick oats", "zarte Haferflocken" },
            { "oats", "Haferflocken" }, { "oatmeal", "Haferflocken" }, { "rice", "Reis" }, { "long grain rice", "Langkornreis" },
            { "brown rice", "Naturreis" }, { "wild rice", "Wildreis" }, { "pasta", "Nudeln" }, { "noodles", "Nudeln" },
            { "egg noodles", "Eiernudeln" }, { "spaghetti", "Spaghetti" }, { "macaroni", "Makkaroni" },
            { "elbow macaroni", "Hörnchennudeln" }, { "lasagna noodles", "Lasagneplatten" }, { "bread crumbs", "Semmelbrösel" },
            { "breadcrumbs", "Semmelbrösel" }, { "bread", "Brot" }, { "white bread", "Weißbrot" }, { "french bread", "Baguette" },
            { "cornmeal", "Maisgrieß" }, { "tortillas", "Tortillas" }, { "flour tortillas", "Weizentortillas" },
            { "graham cracker crumbs", "Butterkeksbrösel" }, { "graham crackers", "Butterkekse" }, { "crackers", "Cracker" },
            { "pie crust", "Mürbeteigboden" }, { "puff pastry", "Blätterteig" }, { "potatoes", "Kartoffeln" },
            { "potato", "Kartoffel" }, { "sweet potatoes", "Süßkartoffeln" }, { "sweet potato", "Süßkartoffel" },
            { "onions", "Zwiebeln" }, { "onion", "Zwiebel" }, { "red onion", "rote Zwiebel" }, { "green onions", "Frühlingszwiebeln" },
            { "green onion", "Frühlingszwiebel" }, { "scallions", "Frühlingszwiebeln" }, { "shallots", "Schalotten" },
            { "shallot", "Schalotte" }, { "garlic cloves", "Knoblauchzehen" }, { "garlic clove", "Knoblauchzehe" },
            { "garlic", "Knoblauch" }, { "leeks", "Lauch" }, { "leek", "Lauch" }, { "carrots", "Möhren" }, { "carrot", "Möhre" },
            { "celery", "Staudensellerie" }, { "celery ribs", "Selleriestangen" }, { "celery stalks", "Selleriestangen" },
            { "green bell pepper", "grüne Paprikaschote" }, { "red bell pepper", "rote Paprikaschote" },
            { "bell pepper", "Paprikaschote" }, { "bell peppers", "Paprikaschoten" }, { "green pepper", "grüne Paprikaschote" },
            { "red pepper", "rote Paprikaschote" }, { "jalapeno", "Jalapeño" }, { "jalapeno pepper", "Jalapeño" },
            { "chili powder", "Chilipulver" }, { "tomatoes", "Tomaten" }, { "tomato", "Tomate" },
            { "cherry tomatoes", "Kirschtomaten" }, { "diced tomatoes", "gehackte Tomaten (Dose)" },
            { "crushed tomatoes", "passierte Tomaten" }, { "tomato paste", "Tomatenmark" }, { "tomato sauce", "Tomatensoße" },
            { "cucumber", "Salatgurke" }, { "cucumbers", "Salatgurken" }, { "pickles", "Gewürzgurken" }, { "zucchini", "Zucchini" },
            { "eggplant", "Aubergine" }, { "pumpkin", "Kürbis" }, { "squash", "Kürbis" }, { "broccoli", "Brokkoli" },
            { "cauliflower", "Blumenkohl" }, { "cabbage", "Weißkohl" }, { "red cabbage", "Rotkohl" }, { "spinach", "Spinat" },
            { "lettuce", "Kopfsalat" }, { "mushrooms", "Champignons" }, { "mushroom", "Champignon" }, { "corn", "Mais" },
            { "peas", "Erbsen" }, { "green beans", "grüne Bohnen" }, { "asparagus", "Spargel" }, { "beets", "Rote Bete" },
            { "radishes", "Radieschen" }, { "avocado", "Avocado" }, { "olives", "Oliven" }, { "black olives", "schwarze Oliven" },
            { "capers", "Kapern" }, { "apples", "Äpfel" }, { "apple", "Apfel" }, { "applesauce", "Apfelmus" },
            { "pears", "Birnen" }, { "bananas", "Bananen" }, { "banana", "Banane" }, { "oranges", "Orangen" },
            { "orange", "Orange" }, { "lemons", "Zitronen" }, { "lemon", "Zitrone" }, { "limes", "Limetten" }, { "lime", "Limette" },
            { "lemon juice", "Zitronensaft" }, { "lime juice", "Limettensaft" }, { "lemon zest", "Zitronenschale" },
            { "lemon peel", "Zitronenschale" }, { "grated lemon peel", "abgeriebene Zitronenschale" },
            { "orange juice", "Orangensaft" }, { "orange zest", "Orangenschale" }, { "apple juice", "Apfelsaft" },
            { "strawberries", "Erdbeeren" }, { "raspberries", "Himbeeren" }, { "blueberries", "Heidelbeeren" },
            { "blackberries", "Brombeeren" }, { "cranberries", "Cranberries" }, { "cherries", "Kirschen" },
            { "peaches", "Pfirsiche" }, { "apricots", "Aprikosen" }, { "pineapple", "Ananas" }, { "crushed pineapple", "Ananasstückchen" },
            { "grapes", "Weintrauben" }, { "raisins", "Rosinen" }, { "dates", "Datteln" }, { "coconut", "Kokosraspel" },
            { "shredded coconut", "Kokosraspel" }, { "flaked coconut", "Kokosflocken" }, { "coconut milk", "Kokosmilch" },
            { "almonds", "Mandeln" }, { "sliced almonds", "Mandelblättchen" }, { "ground almonds", "gemahlene Mandeln" },
            { "walnuts", "Walnüsse" }, { "pecans", "Pekannüsse" }, { "hazelnuts", "Haselnüsse" }, { "peanuts", "Erdnüsse" },
            { "nuts", "Nüsse" }, { "chopped nuts", "gehackte Nüsse" }, { "pine nuts", "Pinienkerne" }, { "cashews", "Cashewkerne" },
            { "peanut butter", "Erdnussbutter" }, { "sesame seeds", "Sesam" }, { "sunflower seeds", "Sonnenblumenkerne" },
            { "poppy seeds", "Mohn" }, { "lentils", "Linsen" }, { "chickpeas", "Kichererbsen" }, { "garbanzo beans", "Kichererbsen" },
            { "kidney beans", "Kidneybohnen" }, { "black beans", "schwarze Bohnen" }, { "white beans", "weiße Bohnen" },
            { "beans", "Bohnen" }, { "tofu", "Tofu" }, { "ground beef", "Rinderhackfleisch" }, { "hamburger", "Rinderhackfleisch" },
            { "ground pork", "Schweinehackfleisch" }, { "ground turkey", "Putenhackfleisch" }, { "beef", "Rindfleisch" },
            { "stew meat", "Gulaschfleisch" }, { "steak", "Steak" }, { "pork chops", "Schweinekoteletts" },
            { "pork loin", "Schweinelende" }, { "pork tenderloin", "Schweinefilet" }, { "pork", "Schweinefleisch" },
            { "chicken breasts", "Hähnchenbrustfilets" }, { "chicken breast", "Hähnchenbrustfilet" },
            { "boneless skinless chicken breasts", "Hähnchenbrustfilets" }, { "chicken thighs", "Hähnchenschenkel" },
            { "chicken wings", "Hähnchenflügel" }, { "chicken", "Hähnchen" }, { "turkey", "Pute" }, { "lamb", "Lammfleisch" },
            { "bacon", "Frühstücksspeck" }, { "ham", "Kochschinken" }, { "sausage", "Wurst" }, { "italian sausage", "italienische Bratwurst" },
            { "hot dogs", "Würstchen" }, { "pepperoni", "Peperoni-Salami" }, { "fish", "Fisch" }, { "fish fillets", "Fischfilets" },
            { "salmon", "Lachs" }, { "tuna", "Thunfisch" }, { "cod", "Kabeljau" }, { "shrimp", "Garnelen" }, { "prawns", "Garnelen" },
            { "crab meat", "Krebsfleisch" }, { "crabmeat", "Krebsfleisch" }, { "chicken broth", "Hühnerbrühe" },
            { "beef broth", "Rinderbrühe" }, { "vegetable broth", "Gemüsebrühe" }, { "chicken stock", "Hühnerbrühe" },
            { "beef stock", "Rinderbrühe" }, { "broth", "Brühe" }, { "stock", "Brühe" }, { "bouillon cube", "Brühwürfel" },
            { "bouillon cubes", "Brühwürfel" }, { "chicken bouillon granules", "Hühnerbrühpulver" }, { "water", "Wasser" },
            { "salt", "Salz" }, { "kosher salt", "grobes Salz" }, { "sea salt", "Meersalz" }, { "pepper", "Pfeffer" },
            { "black pepper", "schwarzer Pfeffer" }, { "white pepper", "weißer Pfeffer" }, { "cayenne pepper", "Cayennepfeffer" },
            { "cayenne", "Cayennepfeffer" }, { "red pepper flakes", "Chiliflocken" }, { "crushed red pepper", "Chiliflocken" },
            { "paprika", "Paprikapulver" }, { "smoked paprika", "geräuchertes Paprikapulver" }, { "curry powder", "Currypulver" },
            { "cumin", "Kreuzkümmel" }, { "ground cumin", "gemahlener Kreuzkümmel" }, { "cinnamon", "Zimt" },
            { "ground cinnamon", "Zimt" }, { "cinnamon stick", "Zimtstange" }, { "nutmeg", "Muskatnuss" },
            { "ground nutmeg", "gemahlene Muskatnuss" }, { "cloves", "Gewürznelken" }, { "ground cloves", "gemahlene Nelken" },
            { "allspice", "Piment" }, { "ginger", "Ingwer" }, { "ground ginger", "Ingwerpulver" }, { "fresh ginger", "frischer Ingwer" },
            { "turmeric", "Kurkuma" }, { "cardamom", "Kardamom" }, { "coriander", "Koriander" }, { "bay leaf", "Lorbeerblatt" },
            { "bay leaves", "Lorbeerblätter" }, { "oregano", "Oregano" }, { "dried oregano", "getrockneter Oregano" },
            { "basil", "Basilikum" }, { "fresh basil", "frisches Basilikum" }, { "dried basil", "getrocknetes Basilikum" },
            { "thyme", "Thymian" }, { "dried thyme", "getrockneter Thymian" }, { "rosemary", "Rosmarin" }, { "sage", "Salbei" },
            { "parsley", "Petersilie" }, { "fresh parsley", "frische Petersilie" }, { "dried parsley", "getrocknete Petersilie" },
            { "cilantro", "Koriandergrün" }, { "dill", "Dill" }, { "dill weed", "Dill" }, { "chives", "Schnittlauch" },
            { "mint", "Minze" }, { "marjoram", "Majoran" }, { "tarragon", "Estragon" }, { "italian seasoning", "italienische Kräuter" },
            { "garlic powder", "Knoblauchpulver" }, { "onion powder", "Zwiebelpulver" }, { "garlic salt", "Knoblauchsalz" },
            { "dry mustard", "Senfpulver" }, { "mustard", "Senf" }, { "dijon mustard", "Dijon-Senf" }, { "prepared mustard", "Senf" },
            { "ketchup", "Ketchup" }, { "catsup", "Ketchup" }, { "mayonnaise", "Mayonnaise" }, { "soy sauce", "Sojasoße" },
            { "worcestershire sauce", "Worcestershiresoße" }, { "hot sauce", "scharfe Soße" }, { "tabasco sauce", "Tabasco" },
            { "barbecue sauce", "Barbecuesoße" }, { "vinegar", "Essig" }, { "white vinegar", "Branntweinessig" },
            { "cider vinegar", "Apfelessig" }, { "apple cider vinegar", "Apfelessig" }, { "red wine vinegar", "Rotweinessig" },
            { "balsamic vinegar", "Balsamico-Essig" }, { "wine vinegar", "Weinessig" }, { "white wine", "Weißwein" },
            { "dry white wine", "trockener Weißwein" }, { "red wine", "Rotwein" }, { "dry red wine", "trockener Rotwein" },
            { "wine", "Wein" }, { "sherry", "Sherry" }, { "beer", "Bier" }, { "rum", "Rum" }, { "brandy", "Weinbrand" },
            { "coffee", "Kaffee" }, { "tea", "Tee" }, { "salsa", "Salsa" }, { "pesto", "Pesto" }, { "marinara sauce", "Tomatensoße" },
            { "cream of mushroom soup", "Champignoncremesuppe (Konzentrat)" }, { "cream of chicken soup", "Hühnercremesuppe (Konzentrat)" },
            { "jam", "Konfitüre" }, { "jelly", "Gelee" }, { "pudding mix", "Puddingpulver" }, { "instant pudding", "Instant-Puddingpulver" },
            { "marshmallows", "Marshmallows" }, { "miniature marshmallows", "Mini-Marshmallows" },
            { "nonstick cooking spray", "Backtrennspray" }, { "cooking spray", "Backtrennspray" }, { "ice", "Eis" },
            { "ice cream", "Speiseeis" }, { "vanilla ice cream", "Vanilleeis" }, { "whipped topping", "Sahne (geschlagen)" },
            { "frozen", "TK-" },
        };
        for (auto &p : pairs) h.insert(QString::fromUtf8(p[0]), QString::fromUtf8(p[1]));
        return h;
    }();
    return d;
}

const QHash<QString, QString> &words()
{
    // Zusätze, Eigenschaften und Zubereitungsarten
    static const QHash<QString, QString> d = [] {
        QHash<QString, QString> h;
        const char *pairs[][2] = {
            { "finely chopped", "fein gehackt" }, { "coarsely chopped", "grob gehackt" }, { "chopped", "gehackt" },
            { "minced", "fein gehackt" }, { "diced", "gewürfelt" }, { "cubed", "gewürfelt" }, { "sliced", "in Scheiben" },
            { "thinly sliced", "in dünnen Scheiben" }, { "grated", "gerieben" }, { "shredded", "geraspelt" },
            { "peeled", "geschält" }, { "seeded", "entkernt" }, { "pitted", "entsteint" }, { "cored", "entkernt" },
            { "halved", "halbiert" }, { "quartered", "geviertelt" }, { "crushed", "zerdrückt" }, { "mashed", "zerdrückt" },
            { "melted", "geschmolzen" }, { "softened", "weich" }, { "room temperature", "zimmerwarm" },
            { "at room temperature", "zimmerwarm" }, { "beaten", "verquirlt" }, { "lightly beaten", "leicht verquirlt" },
            { "well beaten", "gut verquirlt" }, { "separated", "getrennt" }, { "sifted", "gesiebt" }, { "packed", "fest gedrückt" },
            { "firmly packed", "fest gedrückt" }, { "drained", "abgetropft" }, { "rinsed", "abgespült" },
            { "rinsed and drained", "abgespült und abgetropft" }, { "undrained", "mit Flüssigkeit" }, { "cooked", "gekocht" },
            { "uncooked", "ungekocht" }, { "boiled", "gekocht" }, { "fried", "gebraten" }, { "toasted", "geröstet" },
            { "roasted", "geröstet" }, { "frozen", "tiefgekühlt" }, { "thawed", "aufgetaut" }, { "fresh", "frisch" },
            { "dried", "getrocknet" }, { "ground", "gemahlen" }, { "whole", "ganz" }, { "large", "groß" }, { "medium", "mittelgroß" },
            { "small", "klein" }, { "cold", "kalt" }, { "warm", "warm" }, { "hot", "heiß" }, { "boiling", "kochend" },
            { "lukewarm", "lauwarm" }, { "cut up", "zerkleinert" }, { "cut into pieces", "in Stücke geschnitten" },
            { "in pieces", "in Stücken" }, { "to taste", "nach Geschmack" }, { "optional", "nach Belieben" },
            { "divided", "aufgeteilt" }, { "plus more", "plus etwas mehr" }, { "for garnish", "zum Garnieren" },
            { "for serving", "zum Servieren" }, { "unsalted", "ungesalzen" }, { "salted", "gesalzen" }, { "sweetened", "gesüßt" },
            { "unsweetened", "ungesüßt" }, { "lean", "mager" }, { "boneless", "ohne Knochen" }, { "skinless", "ohne Haut" },
            { "trimmed", "pariert" }, { "julienned", "in feinen Streifen" }, { "strips", "Streifen" }, { "pieces", "Stücke" },
            { "or", "oder" }, { "and", "und" }, { "with", "mit" }, { "without", "ohne" }, { "about", "etwa" }, { "each", "je" },
            { "if desired", "nach Belieben" }, { "more or less", "mehr oder weniger" }, { "inch", "Zoll" },
        };
        for (auto &p : pairs) h.insert(QString::fromUtf8(p[0]), QString::fromUtf8(p[1]));
        return h;
    }();
    return d;
}

const QHash<QString, QString> &categories()
{
    static const QHash<QString, QString> d = [] {
        QHash<QString, QString> h;
        const char *pairs[][2] = {
            { "desserts", "desserts" }, { "dessert", "desserts" }, { "cakes", "backen" }, { "cake", "backen" },
            { "cookies", "backen" }, { "breads", "backen" }, { "bread", "backen" }, { "baking", "backen" }, { "pies", "backen" },
            { "pastry", "backen" }, { "muffins", "backen" }, { "meats", "fleischgerichte" }, { "meat", "fleischgerichte" },
            { "beef", "fleischgerichte" }, { "pork", "fleischgerichte" }, { "poultry", "fleischgerichte" },
            { "chicken", "fleischgerichte" }, { "lamb", "fleischgerichte" }, { "game", "fleischgerichte" },
            { "fish", "fisch" }, { "seafood", "fisch" }, { "vegetarian", "vegetarisch" }, { "vegan", "vegan" },
            { "vegetables", "gemüse" }, { "salads", "salate" }, { "salad", "salate" }, { "soups", "suppen" }, { "soup", "suppen" },
            { "stews", "eintöpfe" }, { "main dish", "hauptgerichte" }, { "main dishes", "hauptgerichte" },
            { "entrees", "hauptgerichte" }, { "side dishes", "beilagen" }, { "side dish", "beilagen" },
            { "appetizers", "vorspeisen" }, { "snacks", "snacks" }, { "beverages", "getränke" }, { "drinks", "getränke" },
            { "breakfast", "frühstück" }, { "brunch", "frühstück" }, { "sauces", "soßen" }, { "pasta", "nudeln" },
            { "rice", "reis" }, { "candy", "süßigkeiten" }, { "chocolate", "schokolade" }, { "low-fat", "fettarm" },
            { "low fat", "fettarm" }, { "diabetic", "zuckerarm" }, { "holiday", "festtage" }, { "christmas", "weihnachten" },
            { "italian", "italienisch" }, { "mexican", "mexikanisch" }, { "chinese", "chinesisch" }, { "german", "deutsch" },
            { "french", "französisch" }, { "indian", "indisch" }, { "casseroles", "aufläufe" }, { "casserole", "aufläufe" },
            { "menus", "menüs" }, { "menu", "menüs" }, { "eggs", "eierspeisen" }, { "cheese", "käse" }, { "fruits", "obst" },
            { "fruit", "obst" }, { "potatoes", "kartoffeln" }, { "grilling", "grillen" }, { "barbecue", "grillen" },
            { "microwave", "mikrowelle" }, { "crockpot", "schongarer" }, { "slow cooker", "schongarer" },
        };
        for (auto &p : pairs) h.insert(QString::fromUtf8(p[0]), QString::fromUtf8(p[1]));
        return h;
    }();
    return d;
}

QStringList tokens(const QString &s)
{
    QString t = s;
    t.replace(QRegularExpression(QStringLiteral("([,;()])")), QStringLiteral(" \\1 "));
    return t.split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

// Phrasenweise übersetzen: längste bekannte Wortfolge zuerst
QString translatePhrases(const QString &text, const QHash<QString, QString> &primary, const QHash<QString, QString> *secondary,
                         int *knownWords, int *allWords)
{
    const QStringList t = tokens(text);
    QStringList out;
    int known = 0, all = 0;
    for (int i = 0; i < t.size();) {
        if (t[i].size() == 1 && QStringLiteral(",;()").contains(t[i])) {
            out << t[i];
            ++i;
            continue;
        }
        bool found = false;
        for (int n = qMin(5, int(t.size()) - i); n >= 1 && !found; --n) {
            QString phrase = t.mid(i, n).join(QLatin1Char(' ')).toLower();
            for (int pass = 0; pass < 3 && !found; ++pass) {
                QString p = phrase;
                if (pass == 1) {            // Mehrzahl → Einzahl
                    if (p.endsWith(QLatin1String("ies"))) p = p.left(p.size() - 3) + QLatin1Char('y');
                    else if (p.endsWith(QLatin1Char('s'))) p.chop(1);
                    else continue;
                }
                if (pass == 2) {
                    if (p.endsWith(QLatin1String("es"))) p.chop(2);
                    else continue;
                }
                auto it = primary.constFind(p);
                if (it == primary.constEnd() && secondary) {
                    auto s2 = secondary->constFind(p);
                    if (s2 != secondary->constEnd()) it = s2;
                    else continue;
                } else if (it == primary.constEnd()) {
                    continue;
                }
                out << *it;
                known += n;
                all += n;
                i += n;
                found = true;
            }
        }
        if (!found) {
            out << t[i];
            ++all;
            ++i;
        }
    }
    if (knownWords) *knownWords = known;
    if (allWords) *allWords = all;
    QString r = out.join(QLatin1Char(' '));
    r.replace(QStringLiteral(" ,"), QStringLiteral(","));
    r.replace(QStringLiteral(" ;"), QStringLiteral(";"));
    r.replace(QStringLiteral("( "), QStringLiteral("("));
    r.replace(QStringLiteral(" )"), QStringLiteral(")"));
    r.replace(QStringLiteral("TK- "), QStringLiteral("TK-"));
    return r.simplified();
}

} // namespace

bool looksEnglish(const QString &text)
{
    static const QSet<QString> en{ QStringLiteral("the"), QStringLiteral("and"), QStringLiteral("with"),
                                   QStringLiteral("until"), QStringLiteral("minutes"), QStringLiteral("add"),
                                   QStringLiteral("into"), QStringLiteral("bake"), QStringLiteral("stir"),
                                   QStringLiteral("mix"), QStringLiteral("in"), QStringLiteral("to"),
                                   QStringLiteral("of"), QStringLiteral("cup"), QStringLiteral("place"),
                                   QStringLiteral("serve"), QStringLiteral("combine"), QStringLiteral("heat") };
    static const QSet<QString> de{ QStringLiteral("und"), QStringLiteral("die"), QStringLiteral("der"),
                                   QStringLiteral("mit"), QStringLiteral("den"), QStringLiteral("das"),
                                   QStringLiteral("minuten"), QStringLiteral("dazu"), QStringLiteral("geben"),
                                   QStringLiteral("bis"), QStringLiteral("im"), QStringLiteral("ein"),
                                   QStringLiteral("eine"), QStringLiteral("zugeben"), QStringLiteral("backen"),
                                   QStringLiteral("auf"), QStringLiteral("zu"), QStringLiteral("mit") };
    int e = 0, d = 0;
    for (const QString &w : text.toLower().split(QRegularExpression(QStringLiteral("[^\\p{L}]+")), Qt::SkipEmptyParts)) {
        if (en.contains(w)) ++e;
        if (de.contains(w)) ++d;
    }
    return e > d && e >= 2;
}

QString detectLanguage(const QString &text)
{
    struct L { const char *code; QSet<QString> words; };
    auto set = [](std::initializer_list<const char *> l) {
        QSet<QString> s;
        for (const char *w : l) s.insert(QString::fromUtf8(w));
        return s;
    };
    static const QList<L> langs{
        { "de", set({ "und", "die", "der", "mit", "den", "das", "minuten", "dazu", "geben", "bis", "im", "ein", "eine", "zugeben",
                      "backen", "auf", "zu", "nicht", "etwas", "dann", "darin", "abschmecken", "kochen", "unter", "wenig", "mehl" }) },
        { "en", set({ "the", "and", "with", "until", "minutes", "add", "into", "bake", "stir", "mix", "to", "of", "cup", "place",
                      "serve", "combine", "heat", "cook", "remove", "pour", "about" }) },
        { "fr", set({ "le", "la", "les", "et", "avec", "dans", "pour", "des", "du", "une", "un", "faire", "cuire", "ajouter",
                      "minutes", "puis", "sur", "au", "jusqu", "sel", "poivre" }) },
        { "it", set({ "il", "la", "le", "e", "con", "di", "del", "della", "per", "un", "una", "aggiungere", "cuocere", "minuti",
                      "sale", "pepe", "fino", "nel", "poi", "qb" }) },
        { "es", set({ "el", "la", "los", "las", "y", "con", "de", "del", "para", "un", "una", "añadir", "cocinar", "minutos",
                      "sal", "pimienta", "hasta", "en", "luego" }) },
        { "nl", set({ "de", "het", "en", "met", "een", "van", "in", "tot", "minuten", "toevoegen", "bakken", "zout", "peper",
                      "daarna", "op", "voor" }) },
    };
    QHash<QString, int> score;
    const auto words = text.toLower().split(QRegularExpression(QStringLiteral("[^\\p{L}]+")), Qt::SkipEmptyParts);
    for (const QString &w : words)
        for (const L &l : langs)
            if (l.words.contains(w)) ++score[QString::fromLatin1(l.code)];
    QString best = QStringLiteral("de");
    int bestScore = score.value(best);
    for (const L &l : langs) {
        const QString c = QString::fromLatin1(l.code);
        if (score.value(c) > bestScore) {
            best = c;
            bestScore = score.value(c);
        }
    }
    // knapp oder kaum Text: lieber deutsch annehmen (keine unnötige Übersetzung)
    if (best != QLatin1String("de") && (bestScore < 3 || bestScore < score.value(QStringLiteral("de")) * 1.3)) return QStringLiteral("de");
    return best;
}

QString ingredient(const QString &englishName, QString *leadNote)
{
    QString name = englishName.trimmed();
    if (leadNote) {
        leadNote->clear();
        // „chopped walnuts“ → Walnüsse, gehackt – nur, wenn der ganze Ausdruck kein bekannter Zutatenname ist
        auto known = [](const QString &n) {
            const QString l = n.toLower();
            return ingredients().contains(l) || (l.endsWith(QLatin1Char('s')) && ingredients().contains(l.left(l.size() - 1)));
        };
        QStringList leads;
        while (!known(name)) {
            QStringList t = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (t.size() < 2) break;
            bool stripped = false;
            for (int n = qMin(3, int(t.size()) - 1); n >= 1; --n) {
                const QString p = t.mid(0, n).join(QLatin1Char(' ')).toLower();
                auto it = words().constFind(p);
                if (it != words().constEnd() && p != QLatin1String("or") && p != QLatin1String("and")) {
                    leads << *it;
                    name = t.mid(n).join(QLatin1Char(' '));
                    stripped = true;
                    break;
                }
            }
            if (!stripped) break;
        }
        *leadNote = leads.join(QStringLiteral(", "));
    }
    QString r = translatePhrases(name, ingredients(), &words(), nullptr, nullptr);
    if (!r.isEmpty()) r[0] = r[0].toUpper();
    return r;
}

QString note(const QString &englishNote)
{
    return translatePhrases(englishNote, words(), &ingredients(), nullptr, nullptr);
}

double coverage(const QString &englishName)
{
    int k = 0, a = 0;
    translatePhrases(englishName, ingredients(), &words(), &k, &a);
    return a > 0 ? double(k) / a : 0.0;
}

QString category(const QString &englishCategory)
{
    const QString c = englishCategory.trimmed().toLower();
    auto it = categories().constFind(c);
    if (it != categories().constEnd()) return *it;
    return c;
}

QString germanCategory(const QString &cat)
{
    QString c = cat.trimmed().toLower();
    static const QHash<QString, QString> map{
        { QStringLiteral("fleisch"), QStringLiteral("fleischgerichte") }, { QStringLiteral("rind"), QStringLiteral("fleischgerichte") },
        { QStringLiteral("schwein"), QStringLiteral("fleischgerichte") }, { QStringLiteral("geflügel"), QStringLiteral("fleischgerichte") },
        { QStringLiteral("lamm"), QStringLiteral("fleischgerichte") }, { QStringLiteral("wild"), QStringLiteral("fleischgerichte") },
        { QStringLiteral("kuchen"), QStringLiteral("backen") }, { QStringLiteral("gebäck"), QStringLiteral("backen") },
        { QStringLiteral("kekse"), QStringLiteral("backen") }, { QStringLiteral("plätzchen"), QStringLiteral("backen") },
        { QStringLiteral("brot"), QStringLiteral("backen") }, { QStringLiteral("torte"), QStringLiteral("backen") },
        { QStringLiteral("torten"), QStringLiteral("backen") }, { QStringLiteral("dessert"), QStringLiteral("desserts") },
        { QStringLiteral("nachspeise"), QStringLiteral("desserts") }, { QStringLiteral("nachtisch"), QStringLiteral("desserts") },
        { QStringLiteral("suppe"), QStringLiteral("suppen") }, { QStringLiteral("salat"), QStringLiteral("salate") },
        { QStringLiteral("menü"), QStringLiteral("menüs") }, { QStringLiteral("menue"), QStringLiteral("menüs") },
        { QStringLiteral("vegetarisch"), QStringLiteral("vegetarisch") }, { QStringLiteral("vegan"), QStringLiteral("vegan") },
        { QStringLiteral("fisch"), QStringLiteral("fisch") }, { QStringLiteral("meeresfrüchte"), QStringLiteral("fisch") },
        { QStringLiteral("hauptgericht"), QStringLiteral("hauptgerichte") }, { QStringLiteral("beilage"), QStringLiteral("beilagen") },
        { QStringLiteral("vorspeise"), QStringLiteral("vorspeisen") }, { QStringLiteral("getränk"), QStringLiteral("getränke") },
        { QStringLiteral("auflauf"), QStringLiteral("aufläufe") }, { QStringLiteral("eintopf"), QStringLiteral("eintöpfe") },
    };
    auto it = map.constFind(c);
    return it != map.constEnd() ? *it : c;
}

} // namespace Glossary
