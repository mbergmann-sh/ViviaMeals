#!/usr/bin/env python3
# Erzeugt resources/data/lebensmittel.json – die eingebaute Nährwerttabelle von VitaMeals.
#
# Werte: Richtwerte je 100 g essbarer Anteil (roh, sofern nicht anders angegeben), gerundet,
# zusammengestellt aus allgemein zugänglichen Durchschnittswerten. Es sind Schätzwerte für die
# Rezeptplanung – keine Analysewerte. Eigene/abweichende Werte trägt man im Programm unter
# Extras → Nährwerttabelle ein (eigene Einträge haben Vorrang).
#
# Spalten:  key | Name | Kategorie | kcal Eiweiß Fett gesFS KH Zucker Ballast Salz | Dichte g/ml |
#           Stückgewicht g | Einheitengewichte (Einheit=g,…) | deutsche Namen | englische Namen
# Kategorien: gemuese obst kartoffel getreide backwaren huelsen nuss milch kaese ei fleisch fisch
#             fett zucker suess gewuerz kraeuter getraenk alkohol sosse honig gelatine sonstiges
import json, os, sys

ROWS = r"""
wasser|Wasser|getraenk|0 0 0 0 0 0 0 0|1.0|0||wasser,leitungswasser,kaltes wasser,warmes wasser,heißes wasser,mineralwasser,sprudelwasser,eiswasser|water,cold water,warm water,hot water,boiling water,ice water
weizenmehl|Weizenmehl Type 405|getreide|340 10 1 0.2 71 0.4 4 0|0.55|0||weizenmehl,mehl,mehl type 405,type 405,weißmehl,auszugsmehl,mehl type 550,weizenmehl type 550,dinkelmehl,dinkelmehl type 630|flour,all-purpose flour,all purpose flour,plain flour,white flour,bread flour,cake flour,self-rising flour,self raising flour,ap flour
vollkornmehl|Weizenvollkornmehl|getreide|320 12 2 0.3 60 0.4 10 0|0.55|0||vollkornmehl,weizenvollkornmehl,dinkelvollkornmehl,grahammehl|whole wheat flour,wholemeal flour,whole-wheat flour,graham flour
roggenmehl|Roggenmehl|getreide|320 7 1 0.2 68 1 8 0|0.55|0||roggenmehl,roggenvollkornmehl|rye flour
speisestaerke|Speisestärke|getreide|350 0.4 0.1 0 86 0 1 0|0.6|0||speisestärke,stärke,maisstärke,mondamin,kartoffelstärke,stärkemehl|cornstarch,corn starch,cornflour,potato starch
backpulver|Backpulver|sonstiges|150 0 0 0 37 0 0 26|0.9|0|Pck.=15|backpulver|baking powder
natron|Natron|sonstiges|0 0 0 0 0 0 0 68|1.1|0||natron,kaiser natron,speisenatron|baking soda,bicarbonate of soda
hefe|Hefe (frisch)|sonstiges|100 12 1 0.2 8 0 7 0.1|1.0|42|Würfel=42|hefe,frischhefe,frische hefe,bäckerhefe|yeast,fresh yeast,cake yeast
trockenhefe|Trockenhefe|sonstiges|330 40 7 1 13 0 27 0.1|0.8|0|Pck.=7|trockenhefe|dry yeast,active dry yeast,instant yeast,rapid rise yeast
zucker|Zucker|zucker|400 0 0 0 100 100 0 0|0.85|0||zucker,weißer zucker,kristallzucker,feinzucker,haushaltszucker|sugar,white sugar,granulated sugar,caster sugar,superfine sugar
brauner_zucker|Brauner Zucker|zucker|380 0.1 0 0 98 97 0 0.1|0.9|0||brauner zucker,rohrzucker,vollrohrzucker,muscovado|brown sugar,light brown sugar,dark brown sugar,packed brown sugar,raw sugar
puderzucker|Puderzucker|zucker|400 0 0 0 100 100 0 0|0.55|0||puderzucker,staubzucker|powdered sugar,confectioners sugar,confectioners' sugar,icing sugar
vanillezucker|Vanillezucker|zucker|400 0 0 0 99 99 0 0|0.8|0|Pck.=8|vanillezucker,vanillinzucker,bourbon-vanillezucker|vanilla sugar
honig|Honig|honig|310 0.4 0 0 82 75 0 0|1.4|0||honig,bienenhonig|honey
ahornsirup|Ahornsirup|zucker|260 0 0.1 0 67 60 0 0|1.3|0||ahornsirup|maple syrup
fruchtsirup|Fruchtsirup (Grenadine, Himbeersirup)|zucker|270 0 0 0 67 65 0 0|1.3|0||grenadine,grenadinesirup,himbeersirup,holundersirup,holunderblütensirup,fruchtsirup,erdbeersirup,kirschsirup|grenadine,raspberry syrup,elderflower syrup,fruit syrup
zuckerruebensirup|Zuckerrübensirup|zucker|300 1 0 0 74 64 0 0.2|1.4|0||zuckerrübensirup,rübensirup,melasse,sirup,goldsirup|molasses,golden syrup,corn syrup,light corn syrup,dark corn syrup,treacle
vanilleschote|Vanilleschote|gewuerz|290 3 0 0 13 13 0 0|0.5|3|Stange=3|vanilleschote,vanille,vanillemark|vanilla bean,vanilla pod
vanilleextrakt|Vanilleextrakt|gewuerz|290 0 0 0 13 13 0 0|0.9|0||vanilleextrakt,vanillearoma,vanilleessenz,bittermandelaroma,aroma|vanilla extract,vanilla,vanilla essence,almond extract
kakao|Kakaopulver|suess|360 20 21 13 11 1 33 0.1|0.45|0||kakao,kakaopulver,backkakao|cocoa,cocoa powder,unsweetened cocoa
schokolade_zartbitter|Zartbitterschokolade|suess|540 7 33 20 47 42 9 0|0.6|0|Tafel=100|zartbitterschokolade,bitterschokolade,schokolade,schokotropfen,zartbitter-schokotropfen,schokodrops,schokoladentropfen,schokostreusel,kuvertüre,zartbitterkuvertüre,blockschokolade,halbbitterschokolade|chocolate,dark chocolate,semisweet chocolate,semi-sweet chocolate,bittersweet chocolate,chocolate chips,semisweet chocolate chips,baking chocolate,unsweetened chocolate
schokolade_vollmilch|Vollmilchschokolade|suess|540 7 31 19 57 55 2 0.2|0.6|0|Tafel=100|vollmilchschokolade,milchschokolade,vollmilchkuvertüre|milk chocolate,milk chocolate chips
schokolade_weiss|Weiße Schokolade|suess|550 6 32 19 59 59 0 0.2|0.6|0|Tafel=100|weiße schokolade,weisse schokolade|white chocolate,white chocolate chips
marmelade|Konfitüre|suess|250 0.4 0 0 61 58 1 0|1.3|0||marmelade,konfitüre,gelee,aprikosenkonfitüre,erdbeerkonfitüre,johannisbeergelee|jam,jelly,preserves,marmalade,apricot jam
nussnougatcreme|Nuss-Nougat-Creme|suess|540 6 31 10 58 56 3 0.1|1.2|0||nuss-nougat-creme,nutella,nougatcreme|chocolate hazelnut spread,nutella
marzipan|Marzipanrohmasse|suess|460 9 25 2 49 46 5 0|1.2|0||marzipan,marzipanrohmasse|marzipan,almond paste
gelatine|Gelatine|gelatine|340 85 0 0 0 0 0 0.3|0.7|0|Blatt=1.7;Scheibe=1.7;Pck.=9|gelatine,blattgelatine,gelatinepulver|gelatin,gelatine,unflavored gelatin
agar|Agar-Agar|sonstiges|300 6 0.3 0 81 0 0 0.3|0.6|0||agar-agar,agar|agar,agar-agar
butter|Butter|milch|740 0.7 82 53 0.6 0.6 0 0|0.95|0||butter,süßrahmbutter,sauerrahmbutter,ungesalzene butter,gesalzene butter,zerlassene butter,weiche butter,kalte butter|butter,unsalted butter,salted butter,sweet butter,melted butter,softened butter
margarine|Margarine|fett|720 0.2 80 25 0.4 0 0 0.3|0.95|0||margarine,pflanzenmargarine,alsan|margarine,oleo,shortening,vegetable shortening,crisco
butterschmalz|Butterschmalz|milch|900 0 99.5 65 0 0 0 0|0.9|0||butterschmalz,ghee|ghee,clarified butter
schmalz|Schweineschmalz|fett|900 0 99 39 0 0 0 0|0.9|0||schmalz,schweineschmalz,griebenschmalz|lard
oel|Pflanzenöl|fett|880 0 99.8 9 0 0 0 0|0.92|0||öl,pflanzenöl,speiseöl,rapsöl,sonnenblumenöl,erdnussöl,maiskeimöl,neutrales öl,bratöl,frittieröl,distelöl,chiliöl,knoblauchöl,kräuteröl,walnussöl,traubenkernöl|oil,vegetable oil,canola oil,salad oil,cooking oil,sunflower oil,peanut oil,corn oil,chili oil,walnut oil,grapeseed oil
olivenoel|Olivenöl|fett|880 0 99.6 14 0 0 0 0|0.92|0||olivenöl,natives olivenöl,olivenöl extra vergine|olive oil,extra virgin olive oil,extra-virgin olive oil
sesamoel|Sesamöl|fett|880 0 99.8 14 0 0 0 0|0.92|0||sesamöl,walnussöl,kürbiskernöl,leinöl|sesame oil,toasted sesame oil,walnut oil
kokosfett|Kokosfett|fett|880 0.8 99 87 0 0 0 0|0.92|0||kokosfett,kokosöl,palmin|coconut oil
milch|Milch (3,5 %)|milch|64 3.3 3.5 2.3 4.8 4.8 0 0.1|1.03|0||milch,vollmilch,frischmilch,kuhmilch,warme milch,lauwarme milch,kalte milch|milk,whole milk,warm milk,scalded milk
milch_fettarm|Fettarme Milch (1,5 %)|milch|47 3.4 1.5 1 4.9 4.9 0 0.1|1.03|0||fettarme milch,magermilch,teilentrahmte milch,h-milch|skim milk,skimmed milk,low-fat milk,lowfat milk,2% milk,nonfat milk
buttermilch|Buttermilch|milch|37 3.5 0.5 0.3 4 4 0 0.1|1.03|0||buttermilch|buttermilk
kondensmilch|Kondensmilch (7,5 %)|milch|130 6.5 7.5 5 10 10 0 0.2|1.07|0|Dose=340|kondensmilch,dosenmilch,kaffeesahne|evaporated milk,half-and-half,half and half
gezuckerte_kondensmilch|Gezuckerte Kondensmilch|milch|320 8 8.5 5.5 55 55 0 0.3|1.3|0|Dose=397|gezuckerte kondensmilch,milchmädchen|sweetened condensed milk,condensed milk
sahne|Schlagsahne (30 %)|milch|290 2.4 30 20 3.2 3.2 0 0.1|1.0|0|Becher=200|sahne,schlagsahne,süße sahne,rahm,schlagrahm,konditorsahne,kochsahne|cream,heavy cream,whipping cream,heavy whipping cream,double cream,light cream,single cream
saure_sahne|Saure Sahne (10 %)|milch|115 3 10 7 4 4 0 0.1|1.0|0|Becher=200|saure sahne,sauerrahm,schmand,crème fraîche,creme fraiche,creme fraîche,crème fraiche|sour cream,soured cream,creme fraiche,crème fraîche
joghurt|Joghurt (3,5 %)|milch|65 3.5 3.5 2.3 4.5 4.5 0 0.1|1.03|0|Becher=150|joghurt,naturjoghurt,vollmilchjoghurt,griechischer joghurt,joghurt natur|yogurt,yoghurt,plain yogurt,natural yogurt,greek yogurt
rahmjoghurt|Rahmjoghurt (10 %)|milch|125 3 10 6.5 4.5 4.5 0 0.1|1.03|0|Becher=150|rahmjoghurt,sahnejoghurt,griechischer joghurt 10 %|full fat greek yogurt
quark|Magerquark|milch|67 12 0.3 0.2 4 4 0 0.1|1.05|0|Becher=250;Packung=250|quark,magerquark,speisequark,topfen|quark,farmer cheese,farmer's cheese
quark_sahne|Sahnequark (40 %)|milch|160 11 11 7 3 3 0 0.1|1.05|0|Becher=250|sahnequark,quark 40 %,quark 20 %,schichtkäse|cream quark
frischkaese|Frischkäse (Doppelrahmstufe)|kaese|260 5.5 24 16 3.5 3.5 0 0.8|1.05|0|Packung=200;Becher=200|frischkäse,doppelrahmfrischkäse,philadelphia,rahmfrischkäse|cream cheese,neufchatel cheese
mascarpone|Mascarpone|kaese|440 4.5 46 31 3.5 3.5 0 0.1|1.0|0|Becher=250|mascarpone|mascarpone
ricotta|Ricotta|kaese|150 10 11 7 3 3 0 0.3|1.05|0|Becher=250|ricotta|ricotta,ricotta cheese
huettenkaese|Hüttenkäse|kaese|100 12 4.5 3 2.5 2.5 0 0.8|1.05|0|Becher=200|hüttenkäse,körniger frischkäse|cottage cheese,small curd cottage cheese
gouda|Gouda|kaese|360 25 29 19 0 0 0 2|0.5|25|Scheibe=25|gouda,käse,geriebener käse,reibekäse,butterkäse,edamer,tilsiter,emmentaler,emmentaler käse,gratinkäse,pizzakäse,leerdammer|cheese,cheddar,cheddar cheese,sharp cheddar,shredded cheese,grated cheese,swiss cheese,monterey jack,monterey jack cheese,colby,american cheese,jarlsberg,gouda,edam
hartkaese|Hartkäse (Bergkäse, Greyerzer)|kaese|400 28 32 19 0 0 0 1.5|0.5|0|Scheibe=25|hartkäse,bergkäse,greyerzer,gruyère,gruyere,comté,comte,appenzeller,alpkäse,sbrinz,beaufort|gruyere,gruyere cheese,swiss gruyere,comte,comte cheese,beaufort
parmesan|Parmesan|kaese|390 35 28 18 0 0 0 1.6|0.4|0||parmesan,parmigiano,parmigiano reggiano,grana padano,pecorino|parmesan,parmesan cheese,grated parmesan,romano cheese,pecorino,pecorino romano
mozzarella|Mozzarella|kaese|250 18 19 12.5 1 1 0 0.5|1.0|125|Packung=125;Kugel=125|mozzarella,büffelmozzarella|mozzarella,mozzarella cheese,fresh mozzarella
feta|Feta|kaese|260 17 21 14 0.7 0.7 0 2.8|1.0|0|Packung=200|feta,schafskäse,hirtenkäse,fetakäse|feta,feta cheese
camembert|Camembert|kaese|300 20 24 16 0 0 0 1.5|1.0|125|Packung=125|camembert,brie,blauschimmelkäse,gorgonzola,roquefort|camembert,brie,blue cheese,gorgonzola,roquefort
ei|Ei (Größe M)|ei|137 12.5 9.5 2.7 0.7 0.7 0 0.3|1.03|58||ei,eier,hühnerei,hühnereier,ganzes ei,verquirltes ei|egg,eggs,large egg,large eggs,whole egg,beaten egg
eigelb|Eigelb|ei|340 16 32 9 0.3 0.3 0 0.1|1.03|18||eigelb,eidotter,dotter|egg yolk,egg yolks,yolk,yolks
eiweiss|Eiweiß|ei|50 11 0.2 0 0.7 0.7 0 0.4|1.03|35||eiweiß,eiklar,eischnee|egg white,egg whites,whites
haferflocken|Haferflocken|getreide|370 13.5 7 1.3 59 0.7 10 0|0.4|0||haferflocken,zarte haferflocken,kernige haferflocken,porridge|oats,rolled oats,quick oats,oatmeal,old-fashioned oats
muesli|Müsli|getreide|370 9 7 1.5 64 22 8 0.1|0.45|0||müsli,früchtemüsli,granola,crunchy müsli|muesli,granola
cornflakes|Cornflakes|getreide|375 7 1 0.2 84 8 3 1.8|0.12|0||cornflakes,cornflakes ungezuckert|corn flakes,cornflakes
reis|Reis (roh)|getreide|350 7 0.6 0.2 78 0.2 1.4 0|0.85|0|Beutel=125|reis,langkornreis,basmatireis,basmati,jasminreis,risottoreis,arborio,milchreis,rundkornreis,parboiled reis|rice,long grain rice,long-grain rice,white rice,basmati rice,jasmine rice,arborio rice,uncooked rice
reis_gekocht|Reis (gekocht)|getreide|130 2.7 0.3 0.1 28 0 0.4 0|0.8|0||gekochter reis,reis gekocht|cooked rice
vollkornreis|Vollkornreis (roh)|getreide|350 7.5 2.2 0.5 74 0.7 3.5 0|0.85|0||vollkornreis,naturreis,wildreis|brown rice,wild rice
nudeln|Nudeln (roh)|getreide|355 12.5 1.5 0.3 71 3 3 0|0.5|0||nudeln,pasta,spaghetti,spaghettini,makkaroni,maccheroni,penne,fusilli,farfalle,tagliatelle,bandnudeln,hörnchennudeln,rigatoni,lasagneplatten,lasagne-platten,suppennudeln,spätzle,eiernudeln,linguine,fettuccine,tortellini|pasta,noodles,spaghetti,macaroni,elbow macaroni,penne,fusilli,lasagna noodles,lasagne noodles,egg noodles,fettuccine,linguine,rigatoni,ziti
vollkornnudeln|Vollkornnudeln (roh)|getreide|340 13 2.5 0.4 64 3 8 0|0.5|0||vollkornnudeln,vollkornspaghetti|whole wheat pasta,whole-wheat pasta
couscous|Couscous|getreide|360 12 1.5 0.3 72 1 5 0|0.8|0||couscous,bulgur,weizengrieß,hartweizengrieß|couscous,bulgur,bulgur wheat
griess|Grieß|getreide|350 10 1 0.2 72 0.5 4 0|0.7|0||grieß,weichweizengrieß,dinkelgrieß|semolina,cream of wheat,farina
polenta|Polenta (Maisgrieß)|getreide|350 8.5 1.5 0.2 75 1 5 0|0.7|0||polenta,maisgrieß,maismehl|polenta,cornmeal,yellow cornmeal,corn meal,grits
quinoa|Quinoa|getreide|370 14 6 0.7 64 0 7 0|0.8|0||quinoa,amaranth,hirse,buchweizen|quinoa,millet,buckwheat,amaranth
gruenkern|Grünkern|getreide|330 11 2.7 0.4 63 1 9 0|0.75|0||grünkern,grünkernschrot,grünkernmehl,grünkerngrieß|green spelt,green spelt grains
brot|Brot (Mischbrot)|backwaren|230 7 1.2 0.2 46 2.5 5 1.3|0.3|50|Scheibe=45|brot,mischbrot,graubrot,bauernbrot,weißbrot,toastbrot,toast,ciabatta,baguette,landbrot|bread,white bread,french bread,italian bread,sourdough bread,loaf,bread slices,toast
vollkornbrot|Vollkornbrot|backwaren|210 7 1.3 0.2 39 3 8 1.1|0.4|50|Scheibe=50|vollkornbrot,schwarzbrot,roggenbrot,pumpernickel|whole wheat bread,wholemeal bread,rye bread,pumpernickel
broetchen|Brötchen|backwaren|260 8.5 1.5 0.3 51 2.5 3 1.2|0.2|50||brötchen,semmel,semmeln,schrippe,wecken,laugenbrötchen|roll,rolls,bun,buns,hamburger buns,hot dog buns,dinner rolls
semmelbroesel|Semmelbrösel|backwaren|360 11 2 0.4 72 4 5 1.2|0.45|0||semmelbrösel,paniermehl,brösel,weckmehl|bread crumbs,breadcrumbs,dry bread crumbs,panko,cracker crumbs
zwieback|Zwieback / Kekse|backwaren|420 9 9 4 72 15 3 0.7|0.4|10||zwieback,butterkekse,löffelbiskuits,biskuits,kekse,cantuccini,amarettini|crackers,graham crackers,graham cracker crumbs,cookies,ladyfingers,vanilla wafers,saltines
tortilla|Weizentortilla|backwaren|310 8.5 7.5 3 51 3 3 1.3|0.3|40||tortilla,tortillas,wrap,wraps,fladenbrot,pitabrot,pita,naan|tortilla,tortillas,flour tortillas,corn tortillas,pita,pita bread,naan
blaetterteig|Blätterteig|backwaren|400 6 26 12 35 1 2 1|0.5|0|Packung=275|blätterteig,blätterteig tk|puff pastry,frozen puff pastry
muerbeteig|Mürbeteig (Fertigteig)|backwaren|460 6 23 11 56 17 2 0.7|0.5|0|Packung=275|mürbeteig,tortenboden,tarteteig,pizzateig,hefeteig|pie crust,pastry,pie shell,pie pastry,pizza dough
kartoffel|Kartoffeln|kartoffel|75 2 0.1 0 15.5 0.8 2 0|0.7|100||kartoffel,kartoffeln,festkochende kartoffeln,mehligkochende kartoffeln,vorwiegend festkochende kartoffeln,pellkartoffeln,salzkartoffeln,frühkartoffeln|potato,potatoes,russet potatoes,red potatoes,new potatoes,yukon gold potatoes,baking potatoes
suesskartoffel|Süßkartoffel|kartoffel|86 1.6 0.1 0 20 4 3 0.1|0.7|250||süßkartoffel,süßkartoffeln,batate|sweet potato,sweet potatoes,yam,yams
kartoffelpueree|Kartoffelpüree (Pulver)|kartoffel|350 8 1 0.3 75 3 7 2|0.4|0||kartoffelpüreepulver,püreepulver|instant mashed potatoes,potato flakes
pommes|Pommes frites (TK)|kartoffel|150 2.5 5 0.5 23 0.5 2.5 0.4|0.4|0||pommes,pommes frites,kroketten,kartoffelkroketten,rösti,reibekuchen|french fries,fries,hash browns,tater tots
zwiebel|Zwiebel|gemuese|40 1.2 0.1 0 8 5 1.8 0|0.6|100||zwiebel,zwiebeln,gemüsezwiebel,gemüsezwiebeln,rote zwiebel,rote zwiebeln,weiße zwiebel,zwiebelwürfel|onion,onions,yellow onion,white onion,red onion,sweet onion,chopped onion,diced onion
schalotte|Schalotte|gemuese|72 2.5 0.1 0 15 8 3 0|0.6|25||schalotte,schalotten|shallot,shallots
fruehlingszwiebel|Frühlingszwiebel|gemuese|32 1.8 0.2 0 5 2.3 2.6 0|0.4|15|Bund=100|frühlingszwiebel,frühlingszwiebeln,lauchzwiebel,lauchzwiebeln,schnittlauchzwiebel|green onion,green onions,scallion,scallions,spring onion,spring onions
knoblauch|Knoblauch|gemuese|150 6.4 0.5 0.1 30 1 2 0|0.6|4|Zehe=4;Knolle=50|knoblauch,knoblauchzehe,knoblauchzehen,knoblauchknolle|garlic,garlic clove,garlic cloves,minced garlic
lauch|Lauch|gemuese|31 1.5 0.3 0 5 3 2.3 0|0.4|200|Stange=200|lauch,porree,lauchstange|leek,leeks
moehre|Möhre|gemuese|36 0.9 0.2 0 7 5 3 0.1|0.6|80|Bund=500|möhre,möhren,karotte,karotten,mohrrübe,mohrrüben,gelbe rübe,gelbe rüben,wurzel,wurzeln|carrot,carrots,baby carrots,shredded carrots
sellerie|Knollensellerie|gemuese|25 1.5 0.3 0 2.5 2 4 0.2|0.6|600|Knolle=600|sellerie,knollensellerie,sellerieknolle|celeriac,celery root
staudensellerie|Staudensellerie|gemuese|17 1 0.2 0 2 2 1.6 0.2|0.5|40|Stange=40|staudensellerie,stangensellerie,selleriestange,selleriestangen,bleichsellerie|celery,celery stalk,celery stalks,celery rib,celery ribs,chopped celery
paprika|Paprikaschote|gemuese|30 1 0.3 0 5 4.5 3.5 0|0.5|160||paprika,paprikaschote,paprikaschoten,rote paprika,gelbe paprika,grüne paprika,spitzpaprika|bell pepper,bell peppers,green pepper,green peppers,red pepper,red peppers,sweet pepper,red bell pepper,green bell pepper
chili|Chilischote|gemuese|40 2 0.4 0 7 5 1.5 0|0.5|10||chili,chilischote,chilischoten,peperoni,peperoncini,jalapeño,jalapeno,pfefferschote|chili,chili pepper,chile pepper,jalapeno,jalapeno pepper,jalapenos,serrano pepper,hot pepper,green chilies,green chiles
tomate|Tomate|gemuese|19 1 0.2 0 3 3 1.3 0|0.6|80||tomate,tomaten,fleischtomate,fleischtomaten,strauchtomaten,eiertomaten,rispentomaten|tomato,tomatoes,ripe tomatoes,roma tomatoes,plum tomatoes,beefsteak tomato
kirschtomate|Kirschtomaten|gemuese|25 1 0.3 0 4 4 1.5 0|0.6|15|Schale=250|kirschtomaten,cherrytomaten,cocktailtomaten,kirschtomate,cherrytomate|cherry tomatoes,grape tomatoes,cherry tomato
tomaten_dose|Tomaten (Dose, gehackt)|gemuese|22 1.2 0.2 0 3.5 3.5 1 0.3|1.0|0|Dose=400|tomaten aus der dose,dosentomaten,gehackte tomaten,geschälte tomaten,stückige tomaten,pizzatomaten,passierte tomaten,passata,tomatenpüree,polpa|canned tomatoes,diced tomatoes,crushed tomatoes,whole tomatoes,stewed tomatoes,peeled tomatoes,tomato puree,tomato sauce,passata,can tomatoes
tomatenmark|Tomatenmark|gemuese|85 4.5 0.5 0.1 15 11 4 0.4|1.1|0|Dose=70;Tube=200|tomatenmark,tomatenmark zweifach konzentriert|tomato paste
getrocknete_tomaten|Getrocknete Tomaten (in Öl)|gemuese|210 5 14 2 17 10 6 1.5|0.7|0|Glas=280|getrocknete tomaten,sonnengetrocknete tomaten|sun-dried tomatoes,sun dried tomatoes,dried tomatoes
gurke|Salatgurke|gemuese|13 0.6 0.2 0 2 2 0.5 0|0.6|400||gurke,gurken,salatgurke,salatgurken,schlangengurke|cucumber,cucumbers,english cucumber
gewuerzgurke|Gewürzgurke|gemuese|20 0.4 0.2 0 3 3 1 2|0.7|40|Glas=360|gewürzgurke,gewürzgurken,cornichons,essiggurke,essiggurken,saure gurken|pickle,pickles,dill pickle,dill pickles,gherkins,sweet pickle relish,relish
zucchini|Zucchini|gemuese|20 1.6 0.4 0.1 2 2 1 0|0.6|250||zucchini,zucchinis,zucchino|zucchini,zucchinis,courgette,courgettes,summer squash,yellow squash
aubergine|Aubergine|gemuese|22 1.2 0.2 0 2.5 2.5 2.8 0|0.4|300||aubergine,auberginen,melanzani|eggplant,eggplants,aubergine
kuerbis|Kürbis (Hokkaido)|gemuese|40 1.7 0.6 0.1 7 5 2.5 0|0.6|1200||kürbis,hokkaido,hokkaidokürbis,butternut,butternutkürbis,kürbisfleisch,kürbispüree|pumpkin,squash,butternut squash,winter squash,pumpkin puree,canned pumpkin,acorn squash
brokkoli|Brokkoli|gemuese|34 3.8 0.2 0 2.7 1.7 3 0|0.4|400|Kopf=400|brokkoli,broccoli,brokkoliröschen|broccoli,broccoli florets
blumenkohl|Blumenkohl|gemuese|25 2.5 0.3 0 2.3 2.3 2.9 0|0.4|800|Kopf=800|blumenkohl,blumenkohlröschen,karfiol|cauliflower,cauliflower florets
weisskohl|Weißkohl|gemuese|25 1.4 0.2 0 4 4 3 0|0.4|1500|Kopf=1500|weißkohl,kohl,weißkraut,spitzkohl,wirsing,wirsingkohl,chinakohl,sauerkraut,rotkohl,rotkraut,blaukraut|cabbage,green cabbage,red cabbage,savoy cabbage,napa cabbage,chinese cabbage,sauerkraut,coleslaw mix
rosenkohl|Rosenkohl|gemuese|43 4.5 0.3 0.1 3.3 2.3 4.4 0|0.5|10||rosenkohl|brussels sprouts
gruenkohl|Grünkohl|gemuese|37 4.3 0.9 0.1 2.5 2.4 4.2 0.1|0.3|0||grünkohl,braunkohl|kale
spinat|Spinat|gemuese|17 2.7 0.3 0 0.6 0.4 2.6 0.2|0.3|0|Packung=450|spinat,blattspinat,babyspinat,rahmspinat,tk-spinat,mangold|spinach,baby spinach,frozen spinach,chopped spinach,swiss chard,chard
salat|Blattsalat|gemuese|15 1.3 0.2 0 1.5 1.3 1.5 0|0.2|300|Kopf=300|salat,kopfsalat,eisbergsalat,römersalat,feldsalat,rucola,blattsalat,lollo rosso,endivie,frisée,salatblätter|lettuce,iceberg lettuce,romaine,romaine lettuce,salad greens,mixed greens,arugula,rocket,lettuce leaves
champignons|Champignons|gemuese|20 3 0.3 0 0.6 0.2 2 0|0.4|20|Dose=230;Schale=250|champignons,pilze,braune champignons,steinpilze,pfifferlinge,austernpilze,shiitake,kräuterseitlinge,egerlinge,morcheln,morchel,trompetenpilze|mushrooms,mushroom,button mushrooms,fresh mushrooms,sliced mushrooms,shiitake mushrooms,cremini mushrooms,portobello mushrooms,morels
mais|Mais (Dose)|gemuese|80 2.7 1.2 0.2 14 4 3 0.5|0.8|0|Dose=285|mais,zuckermais,maiskörner,gemüsemais,maiskolben|corn,sweet corn,corn kernels,whole kernel corn,canned corn,frozen corn,cream-style corn,corn on the cob
erbsen|Erbsen (TK)|gemuese|80 6.5 0.5 0.1 11 4 5 0|0.65|0|Dose=280|erbsen,tk-erbsen,junge erbsen,zuckerschoten,zuckererbsen,kaiserschoten|peas,green peas,frozen peas,snow peas,sugar snap peas
gruene_bohnen|Grüne Bohnen|gemuese|32 2.4 0.2 0 5 2.5 2 0|0.5|0|Dose=220|grüne bohnen,brechbohnen,prinzessbohnen,stangenbohnen,buschbohnen,schnippelbohnen|green beans,string beans,snap beans,french beans,wax beans
spargel|Spargel|gemuese|20 2 0.1 0 2 2 1.5 0|0.5|25||spargel,weißer spargel,grüner spargel|asparagus,asparagus spears
rote_bete|Rote Bete|gemuese|43 1.6 0.1 0 8.5 8 2.5 0.2|0.6|150||rote bete,rote beete,rote rübe,rote rüben|beet,beets,beetroot
radieschen|Radieschen|gemuese|16 1 0.1 0 2.4 2 1.6 0|0.6|10|Bund=150|radieschen,rettich|radish,radishes,daikon
fenchel|Fenchel|gemuese|25 1.2 0.2 0 3 3 3.3 0.1|0.4|250|Knolle=250|fenchel,fenchelknolle|fennel,fennel bulb
kohlrabi|Kohlrabi|gemuese|27 2 0.1 0 3.7 3.6 1.4 0|0.6|300||kohlrabi|kohlrabi
pastinake|Pastinake|gemuese|60 1.3 0.4 0 12 5 4 0|0.6|150||pastinake,pastinaken,petersilienwurzel,steckrübe,rübe|parsnip,parsnips,turnip,turnips,rutabaga
avocado|Avocado|obst|200 2 20 3 3.5 0.6 6.5 0|0.9|150||avocado,avocados|avocado,avocados
oliven|Oliven|gemuese|145 1 15 2 0 0 3.3 3|0.7|4|Glas=150|oliven,grüne oliven,schwarze oliven,kalamata|olives,black olives,green olives,ripe olives,kalamata olives,pitted olives
kapern|Kapern|gemuese|25 2.4 0.9 0.2 2 0.4 3 7|0.7|0||kapern|capers
suppengruen|Suppengrün|gemuese|30 1.3 0.2 0 5 4 3 0.1|0.5|400|Bund=400|suppengrün,suppengemüse,wurzelwerk|soup greens,mirepoix
gemuesemix|Gemüsemischung (TK)|gemuese|45 2.5 0.4 0.1 6 3 3.5 0.1|0.5|0|Packung=450|gemüse,gemischtes gemüse,tk-gemüse,gemüsemischung,mischgemüse,ratatouille-gemüse,wokgemüse|mixed vegetables,frozen mixed vegetables,vegetables,stir-fry vegetables
apfel|Apfel|obst|54 0.3 0.2 0 12 10 2.3 0|0.6|150||apfel,äpfel,boskop,säuerliche äpfel,kochäpfel,apfelstücke|apple,apples,granny smith apples,tart apples,cooking apples,green apples
apfelmus|Apfelmus|obst|75 0.2 0.1 0 18 16 1.4 0|1.05|0|Glas=360|apfelmus,apfelmark|applesauce,apple sauce
birne|Birne|obst|55 0.4 0.3 0 12 10 3 0|0.6|170||birne,birnen|pear,pears
banane|Banane|obst|95 1.1 0.2 0.1 21 17 2 0|0.6|120||banane,bananen,reife banane,reife bananen|banana,bananas,ripe banana,ripe bananas,mashed bananas
orange|Orange|obst|47 1 0.2 0 9 8.5 2 0|0.6|180||orange,orangen,apfelsine,apfelsinen,blutorange,mandarine,mandarinen,clementine,clementinen|orange,oranges,navel orange,tangerine,tangerines,mandarin,mandarins,clementine
zitrone|Zitrone|obst|35 0.7 0.6 0 3.2 2.5 1.3 0|0.6|100||zitrone,zitronen,limette,limetten,bio-zitrone,unbehandelte zitrone|lemon,lemons,lime,limes
zitronensaft|Zitronensaft|obst|25 0.4 0.2 0 2.5 2.5 0.3 0|1.03|0||zitronensaft,limettensaft,saft einer zitrone,zitronensaft frisch gepresst|lemon juice,lime juice,fresh lemon juice
zitronenschale|Zitronenschale|gewuerz|50 1.5 0.3 0 5 4 10 0|0.5|0||zitronenschale,abrieb,zitronenabrieb,orangenschale,limettenschale,zitronat,orangeat|lemon zest,lemon peel,grated lemon peel,orange zest,orange peel,lime zest,candied peel
orangensaft|Orangensaft|getraenk|45 0.7 0.2 0 9 9 0.2 0|1.04|0||orangensaft,o-saft,apfelsaft,traubensaft,multivitaminsaft,fruchtsaft,saft,ananassaft,kirschsaft,apfelsaft naturtrüb|orange juice,apple juice,juice,pineapple juice,grape juice,cranberry juice,fruit juice,apple cider
erdbeere|Erdbeeren|obst|32 0.7 0.3 0 5.5 5 2 0|0.6|15|Schale=500|erdbeere,erdbeeren|strawberry,strawberries
beeren|Beeren (gemischt)|obst|40 1 0.4 0 6.5 5.5 4 0|0.6|0|Packung=300|beeren,himbeeren,heidelbeeren,blaubeeren,brombeeren,johannisbeeren,stachelbeeren,preiselbeeren,cranberries,beerenmischung,tk-beeren|berries,raspberries,blueberries,blackberries,cranberries,mixed berries,currants,gooseberries,frozen berries
kirschen|Kirschen|obst|60 0.9 0.3 0 13 13 1.5 0|0.65|8|Glas=350|kirschen,sauerkirschen,süßkirschen,schattenmorellen,kirsche|cherries,cherry,sour cherries,sweet cherries,tart cherries,maraschino cherries,pie filling,cherry pie filling
pflaume|Pflaumen|obst|47 0.6 0.2 0 10 10 1.6 0|0.65|30||pflaume,pflaumen,zwetschgen,zwetschken,mirabellen,reneklode|plum,plums,prune plums,damson plums
aprikose|Aprikosen|obst|45 0.9 0.1 0 8.5 8 1.5 0|0.65|40|Dose=480|aprikose,aprikosen,marille,marillen,pfirsich,pfirsiche,nektarine,nektarinen|apricot,apricots,peach,peaches,nectarine,nectarines,canned peaches
ananas|Ananas|obst|55 0.5 0.2 0 12 12 1.4 0|0.65|1000|Dose=340|ananas,ananasstücke,ananasscheiben|pineapple,crushed pineapple,pineapple chunks,pineapple tidbits
mango|Mango|obst|62 0.6 0.4 0.1 13 13 1.7 0|0.65|350||mango,mangos,papaya,maracuja,passionsfrucht|mango,mangoes,papaya,passion fruit
trauben|Weintrauben|obst|70 0.7 0.3 0 16 16 1.5 0|0.65|5||trauben,weintrauben,weintraube|grapes,green grapes,red grapes
kiwi|Kiwi|obst|60 1 0.6 0 10 9 2 0|0.65|75||kiwi,kiwis|kiwi,kiwis,kiwifruit
melone|Melone|obst|35 0.6 0.2 0 8 8 0.5 0|0.6|1500||melone,wassermelone,honigmelone,cantaloupe|melon,watermelon,cantaloupe,honeydew
rhabarber|Rhabarber|obst|20 0.6 0.1 0 1.4 1 3.2 0|0.6|100|Stange=100|rhabarber|rhubarb
rosinen|Rosinen|obst|300 2.5 0.6 0.1 68 66 5 0|0.65|0||rosinen,sultaninen,korinthen,trockenfrüchte|raisins,golden raisins,dried fruit
trockenobst|Trockenobst|obst|260 2.5 0.5 0 58 50 9 0|0.65|0||datteln,feigen,getrocknete aprikosen,softaprikosen,trockenpflaumen,backpflaumen,getrocknete feigen,dörrobst,cranberries getrocknet,backobst,dörrpflaumen,backpflaume|dates,figs,dried apricots,prunes,dried figs,dried cranberries,craisins,dried mixed fruit
kokosraspel|Kokosraspel|nuss|660 6.5 62 55 7 7 16 0|0.35|0||kokosraspel,kokosflocken,kokosnuss|coconut,shredded coconut,flaked coconut,desiccated coconut,sweetened coconut
kokosmilch|Kokosmilch|nuss|180 1.8 18 16 3 2 0 0|1.0|0|Dose=400|kokosmilch,kokoscreme,kokosnussmilch|coconut milk,coconut cream,lite coconut milk
mandeln|Mandeln|nuss|600 22 53 4 5.5 4 11 0|0.55|0|Packung=200|mandeln,gemahlene mandeln,mandelblättchen,mandelstifte,geschälte mandeln,mandelmehl|almonds,sliced almonds,slivered almonds,ground almonds,almond flour,almond meal,blanched almonds
haselnuesse|Haselnüsse|nuss|650 14 61 4.5 10.5 4 8 0|0.55|0|Packung=200|haselnüsse,haselnuss,gemahlene haselnüsse,haselnusskerne,krokant|hazelnuts,filberts,ground hazelnuts
walnuesse|Walnüsse|nuss|680 15 65 6 10.5 2.6 6 0|0.45|0||walnüsse,walnuss,walnusskerne,pekannüsse,pekannuss|walnuts,chopped walnuts,pecans,pecan halves,chopped pecans
nuesse|Nüsse (gemischt)|nuss|620 18 55 7 12 4 8 0|0.55|0||nüsse,nusskerne,gehackte nüsse,cashewkerne,cashews,pistazien,macadamia,paranüsse,pinienkerne,nussmischung|nuts,chopped nuts,mixed nuts,cashews,pistachios,macadamia nuts,pine nuts,brazil nuts
erdnuesse|Erdnüsse|nuss|590 25 49 7 12 4 8 0|0.55|0||erdnüsse,erdnuss,erdnusskerne,gesalzene erdnüsse|peanuts,dry roasted peanuts,salted peanuts
erdnussbutter|Erdnussbutter|nuss|600 25 50 10 15 8 6 1|1.1|0||erdnussbutter,erdnusscreme,erdnussmus,mandelmus,cashewmus,nussmus|peanut butter,creamy peanut butter,crunchy peanut butter,almond butter
samen|Saaten (Sesam, Sonnenblumenkerne …)|nuss|580 21 50 6 11 1 11 0|0.6|0||sesam,sesamsaat,sonnenblumenkerne,kürbiskerne,leinsamen,chiasamen,mohn,blaumohn,saaten|sesame seeds,sunflower seeds,pumpkin seeds,pepitas,flax seeds,flaxseed,chia seeds,poppy seeds
tahin|Tahin|nuss|600 17 54 7.5 21 0.5 9 0.1|1.1|0||tahin,tahini,sesampaste|tahini,sesame paste
linsen|Linsen (getrocknet)|huelsen|330 24 1.5 0.2 48 2 17 0|0.8|0||linsen,rote linsen,berglinsen,tellerlinsen,belugalinsen,gelbe linsen,beluga linsen|lentils,red lentils,green lentils,brown lentils,dried lentils
kichererbsen|Kichererbsen (Dose, abgetropft)|huelsen|120 7 2.5 0.3 16 1 6 0.5|0.7|0|Dose=240|kichererbsen|chickpeas,garbanzo beans,garbanzos,canned chickpeas
bohnen_dose|Bohnen (Dose, abgetropft)|huelsen|100 7 0.5 0.1 15 1 7 0.5|0.7|0|Dose=250|bohnen,kidneybohnen,weiße bohnen,schwarze bohnen,rote bohnen,cannellini-bohnen,wachtelbohnen,baked beans,dicke bohnen,saubohnen,limabohnen,borlotti|beans,kidney beans,red kidney beans,black beans,white beans,navy beans,pinto beans,cannellini beans,great northern beans,lima beans,baked beans,butter beans,refried beans
bohnen_trocken|Getrocknete Bohnen|huelsen|300 22 1.5 0.3 45 3 20 0|0.8|0||getrocknete bohnen,weiße bohnenkerne,getrocknete erbsen,schälerbsen,trockenerbsen,erbsen getrocknet|dried beans,dry beans,split peas,dried peas
tofu|Tofu|huelsen|130 13 8 1.2 1.5 0.6 1 0|1.0|0|Packung=200|tofu,räuchertofu,seidentofu,naturtofu|tofu,firm tofu,extra-firm tofu,silken tofu
sojadrink|Sojadrink|huelsen|40 3.3 1.8 0.3 2.5 2 0.6 0.1|1.03|0||sojadrink,sojamilch,mandeldrink,haferdrink,hafermilch,mandelmilch,reisdrink,pflanzendrink|soy milk,soymilk,almond milk,oat milk,rice milk
sojasahne|Sojasahne / Hafercuisine|huelsen|170 2.5 16 2 4 1 0.5 0.1|1.0|0|Packung=200|sojasahne,hafersahne,hafercuisine,pflanzensahne,sojacuisine|soy cream,non-dairy creamer,coconut whipped cream
sojaschnetzel|Sojaschnetzel / Fleischersatz|huelsen|340 50 1.5 0.2 18 7 18 0.1|0.3|0||sojaschnetzel,sojagranulat,seitan,tempeh,veganes hack,vegane hack,fleischersatz,vegetarisches hack|textured vegetable protein,tvp,seitan,tempeh,meat substitute,veggie crumbles
hackfleisch|Hackfleisch (gemischt)|fleisch|250 19 19.5 8 0 0 0 0.2|1.0|0|Packung=500|hackfleisch,gehacktes,hack,gemischtes hackfleisch,rinderhack,rinderhackfleisch,schweinehack,schweinehackfleisch,mett,faschiertes,lammhack|ground beef,hamburger,ground meat,ground pork,lean ground beef,minced beef,mince,ground chuck,ground round,ground lamb,ground turkey,sausage meat
rindfleisch|Rindfleisch (mager)|fleisch|130 22 4.5 2 0 0 0 0.1|1.0|0||rindfleisch,rinderbraten,rindergulasch,gulasch,gulaschfleisch,rinderhüfte,rumpsteak,steak,rinderfilet,roastbeef,suppenfleisch,tafelspitz,beinscheibe,rinderrouladen,rouladen,schmorfleisch,rinderbrust,kalbfleisch,kalbsschnitzel|beef,stew meat,beef stew meat,steak,sirloin,round steak,flank steak,chuck roast,pot roast,beef roast,roast,sirloin steak,ribeye,brisket,stewing beef,veal,beef chuck,top round,london broil
schweinefleisch|Schweinefleisch (mager)|fleisch|140 21 6 2 0 0 0 0.1|1.0|0||schweinefleisch,schweineschnitzel,schnitzel,schweinefilet,schweinelende,kotelett,koteletts,schweinebraten,schweinenacken,nackensteak,schweinebauch,schweineschulter,kasseler,geschnetzeltes,spareribs,schweinegulasch|pork,pork chops,pork loin,pork tenderloin,pork roast,pork shoulder,boneless pork chops,pork butt,spareribs,ribs,pork belly
gefluegel|Hähnchenbrustfilet|fleisch|105 23.5 1 0.3 0 0 0 0.2|1.0|150||hähnchenbrust,hähnchenbrustfilet,hähnchenbrustfilets,hühnerbrust,hühnerbrustfilet,putenbrust,putenbrustfilet,putenschnitzel,hähnchenfilet,hühnchen,geflügel,putenfleisch,hähnchengeschnetzeltes,putengeschnetzeltes,hähnchen|chicken breast,chicken breasts,boneless chicken breasts,boneless skinless chicken breasts,chicken,turkey breast,chicken tenders,turkey,cooked chicken,diced chicken,chicken meat
haehnchenschenkel|Hähnchenschenkel|fleisch|190 18 13 3.7 0 0 0 0.2|1.0|250||hähnchenschenkel,hühnerschenkel,hähnchenkeulen,hühnerkeulen,hähnchenflügel,chicken wings,brathähnchen,suppenhuhn,ganzes hähnchen,poularde,ente,entenbrust,gans|chicken thighs,chicken legs,drumsticks,chicken wings,whole chicken,broiler,fryer,chicken pieces,duck,duck breast
ente|Ente / Entenbrust|fleisch|200 19 14 4 0 0 0 0.2|1.0|300||ente,entenbrust,entenbrüste,entenbrustfilet,entenkeule,entenkeulen,flugente,barbarie-ente,gans,gänsebrust,gänsekeule|duck,duck breast,duck legs,goose
kaninchen|Kaninchen / Hase|fleisch|130 21 5 2 0 0 0 0.1|1.0|0||kaninchen,hase,hasenrücken,hasenrückenfilets,kaninchenrücken,kaninchenkeule,hasenkeule,hasenkeulen,wildhase,kaninchenfleisch,reh,rehrücken,hirsch,wild|rabbit,hare,venison
gefluegelleber|Geflügelleber|fleisch|135 19 5 1.6 1 0 0 0.2|1.0|0||geflügelleber,hühnerleber,hähnchenleber,entenleber,leber,kalbsleber,schweineleber,rinderleber|chicken liver,chicken livers,liver,calf liver
lamm|Lammfleisch|fleisch|200 18 14 6 0 0 0 0.2|1.0|0||lamm,lammfleisch,lammkeule,lammkoteletts,lammrücken,lammschulter,wild,reh,hirsch,wildschwein|lamb,leg of lamb,lamb chops,lamb shoulder,venison
speck|Speck (durchwachsen)|fleisch|400 13 38 14 0 0 0 3|1.0|0|Scheibe=10;Packung=125|speck,schinkenspeck,frühstücksspeck,bacon,bauchspeck,speckwürfel,räucherspeck,pancetta,lardons|bacon,bacon slices,bacon strips,salt pork,pancetta,slab bacon
schinken|Kochschinken|fleisch|110 19 3.5 1.2 1 1 0 2.3|1.0|0|Scheibe=25|schinken,kochschinken,gekochter schinken,hinterschinken,schinkenwürfel,putenschinken,lachsschinken|ham,cooked ham,diced ham,ham slices,deli ham
rohschinken|Rohschinken|fleisch|240 25 15 5 0 0 0 5.5|1.0|0|Scheibe=15|rohschinken,parmaschinken,serranoschinken,katenschinken,schwarzwälder schinken,prosciutto,bündnerfleisch|prosciutto,cured ham,smoked ham,country ham
wurst|Wurst (Brühwurst)|fleisch|280 12 25 9 1 0.5 0 2|1.0|100||wurst,würstchen,wiener,wiener würstchen,frankfurter,bockwurst,bratwurst,bratwürste,fleischwurst,lyoner,leberkäse,weißwurst,mettwurst,chorizo,cabanossi,salami,mortadella,leberwurst|sausage,sausages,hot dogs,frankfurters,bratwurst,kielbasa,polish sausage,italian sausage,chorizo,pepperoni,salami,smoked sausage,link sausage
fisch_mager|Fischfilet (mager)|fisch|80 18 0.8 0.2 0 0 0 0.2|1.0|150||fisch,fischfilet,seelachs,seelachsfilet,kabeljau,kabeljaufilet,dorsch,rotbarsch,rotbarschfilet,scholle,zander,pangasius,tilapia,schellfisch,seehecht,forelle,forellenfilet,barsch,heilbutt,steinbutt,dorade,wolfsbarsch,fischstäbchen|fish,fish fillets,white fish,cod,haddock,pollock,sole,flounder,tilapia,halibut,trout,snapper,perch,catfish,sea bass,orange roughy,whitefish
lachs|Lachs|fisch|200 20 13.5 3 0 0 0 0.1|1.0|125||lachs,lachsfilet,räucherlachs,graved lachs,stremellachs,makrele,hering,matjes,thunfischsteak,sardinen,sardellen,anchovis,karpfen,aal|salmon,salmon fillet,smoked salmon,lox,mackerel,herring,sardines,anchovies,anchovy fillets,anchovy,tuna steak
thunfisch|Thunfisch (Dose, im eigenen Saft)|fisch|110 25 1 0.3 0 0 0 0.8|1.0|0|Dose=150|thunfisch,thunfisch in öl,thunfisch naturell,thunfischdose|tuna,canned tuna,tuna fish,chunk light tuna,albacore tuna,water-packed tuna
garnelen|Garnelen|fisch|85 18.5 1 0.3 0 0 0 0.6|1.0|10|Packung=200|garnelen,shrimps,krabben,crevetten,riesengarnelen,king prawns,scampi,meeresfrüchte,muscheln,miesmuscheln,tintenfisch,calamari,krebse,flusskrebse,hummer,surimi|shrimp,prawns,jumbo shrimp,crab,crabmeat,crab meat,lobster,scallops,clams,mussels,oysters,squid,calamari,seafood,crawfish,imitation crab
bruehe|Brühe (zubereitet)|sosse|5 0.5 0.2 0.1 0.4 0.2 0 0.8|1.0|0|Würfel=10|brühe,gemüsebrühe,fleischbrühe,hühnerbrühe,rinderbrühe,instant-brühe,fond,kalbsfond,wildfond,fischfond,geflügelfond,bouillon|broth,stock,chicken broth,beef broth,vegetable broth,chicken stock,beef stock,vegetable stock,bouillon,consomme,fish stock
bruehpulver|Brühpulver / Brühwürfel|gewuerz|200 10 12 6 15 5 0 50|0.7|10|Würfel=10;TL=4;EL=12|brühwürfel,brühpulver,gemüsebrühpulver,instant gemüsebrühe,hühnerbrühwürfel,suppenwürfel,gekörnte brühe,instantbrühe|bouillon cube,bouillon cubes,bouillon granules,chicken bouillon granules,beef bouillon cube,stock cube,chicken bouillon,beef bouillon
salz|Salz|gewuerz|0 0 0 0 0 0 0 100|1.2|0||salz,meersalz,jodsalz,kochsalz,steinsalz,fleur de sel,salzwasser,kräutersalz|salt,kosher salt,sea salt,table salt,coarse salt,seasoned salt,garlic salt,onion salt,celery salt
pfeffer|Pfeffer|gewuerz|250 10 3 1 40 0.6 25 0|0.5|0||pfeffer,schwarzer pfeffer,weißer pfeffer,pfeffer aus der mühle,cayennepfeffer,cayenne,pfefferkörner,bunter pfeffer,chilipulver,chiliflocken,piment|pepper,black pepper,ground pepper,white pepper,cayenne,cayenne pepper,red pepper flakes,crushed red pepper,peppercorns,chili powder,ground black pepper,freshly ground pepper,allspice
gewuerze|Gewürze (gemahlen)|gewuerz|300 12 10 1.5 35 3 30 0.1|0.5|0.5||paprikapulver,edelsüß,rosenscharf,geräuchertes paprikapulver,curry,currypulver,kreuzkümmel,cumin,kümmel,zimt,zimtpulver,muskat,muskatnuss,nelken,gewürznelken,kardamom,koriander,koriandersamen,ingwerpulver,kurkuma,gelbwurz,anis,sternanis,lorbeer,lorbeerblatt,lorbeerblätter,wacholderbeeren,senfsaat,senfkörner,lebkuchengewürz,spekulatiusgewürz,garam masala,ras el hanout,gewürzmischung,zimtstange,zimtstangen,safran,bockshornklee,knoblauchpulver,zwiebelpulver,fünf-gewürze-pulver,italienische kräuter,kräuter der provence,oregano getrocknet,thymian getrocknet,majoran,getrockneter majoran,nelkenpulver,piment,pimentkörner,senfpulver,korianderkörner,senfsaat,senfkörner|smoked paprika,curry powder,cumin,ground cumin,caraway,caraway seeds,cinnamon,ground cinnamon,nutmeg,ground nutmeg,cloves,ground cloves,cardamom,coriander,ground coriander,ground ginger,turmeric,anise,star anise,bay leaf,bay leaves,juniper berries,mustard seed,mustard seeds,pumpkin pie spice,garam masala,italian seasoning,garlic powder,onion powder,dried oregano,dried thyme,dried basil,marjoram,dried marjoram,sage,dried sage,dried rosemary,poultry seasoning,herbes de provence,cinnamon stick,cinnamon sticks,mace,saffron,seasoning,dried parsley,dill weed,celery seed,dry mustard,mustard powder,cream of tartar,msg,taco seasoning,cajun seasoning,old bay,allspice,mustard powder,dry mustard
ingwer|Ingwer (frisch)|gewuerz|80 1.8 0.8 0.2 16 1.7 2 0|0.6|20||ingwer,frischer ingwer,ingwerwurzel,galgant,zitronengras|ginger,fresh ginger,ginger root,gingerroot,grated ginger,lemongrass
kraeuter|Frische Kräuter|kraeuter|40 3 0.6 0.1 4 0.9 4 0|0.2|0|Bund=25;Zweig=1;Handvoll=10;Topf=25|petersilie,glatte petersilie,krause petersilie,schnittlauch,dill,basilikum,schnittlauchröllchen,koriandergrün,minze,pfefferminze,thymian,rosmarin,salbei,oregano,estragon,kerbel,liebstöckel,bärlauch,zitronenmelisse,kresse,kräuter,frische kräuter,gemischte kräuter,tk-kräuter,thymianzweig,rosmarinzweig,salbeiblätter,basilikumblätter,borretsch,sauerampfer,zitronenmelisse,pimpinelle|parsley,fresh parsley,chopped parsley,flat-leaf parsley,chives,fresh chives,dill,fresh dill,basil,fresh basil,basil leaves,cilantro,fresh cilantro,mint,fresh mint,mint leaves,thyme,fresh thyme,rosemary,fresh rosemary,sage leaves,oregano,fresh oregano,tarragon,herbs,fresh herbs,watercress,sorrel,borage
senf|Senf|sosse|90 6 5 0.3 5 3 2 4|1.1|0||senf,mittelscharfer senf,scharfer senf,dijonsenf,dijon-senf,süßer senf,körniger senf,estragonsenf|mustard,dijon mustard,prepared mustard,yellow mustard,spicy brown mustard,whole grain mustard,dijon
ketchup|Ketchup|sosse|110 1.3 0.2 0 25 22 0.5 2.5|1.15|0||ketchup,tomatenketchup,currysoße,curryketchup,chilisoße,chilisauce,süß-sauer-soße,bbq-soße,barbecuesauce,grillsoße|ketchup,catsup,barbecue sauce,bbq sauce,chili sauce,sweet and sour sauce,cocktail sauce
mayonnaise|Mayonnaise|sosse|700 1.2 77 6 1.5 1.3 0 1.2|0.95|0||mayonnaise,mayo,salatmayonnaise,remoulade,aioli|mayonnaise,mayo,miracle whip,salad dressing,tartar sauce,aioli
salatcreme|Salatcreme / Dressing|sosse|300 1 28 2.5 12 9 0 2|1.0|0||salatcreme,dressing,salatdressing,joghurtdressing,french dressing,caesar dressing|ranch dressing,italian dressing,french dressing,caesar dressing,vinaigrette,thousand island dressing,blue cheese dressing
sojasosse|Sojasoße|sosse|60 8 0.1 0 6 2 0.8 15|1.15|0||sojasoße,sojasauce,soja-soße,teriyakisoße,teriyaki,fischsoße,fischsauce,austernsoße,austernsauce,hoisinsoße,worcestersoße,worcestershiresauce,maggi,würze,tabasco,sambal oelek,sriracha,tamari|soy sauce,low-sodium soy sauce,light soy sauce,dark soy sauce,tamari,teriyaki sauce,fish sauce,oyster sauce,hoisin sauce,worcestershire sauce,worcestershire,hot sauce,tabasco,tabasco sauce,sriracha,liquid smoke
essig|Essig|sosse|20 0 0 0 0.6 0.4 0 0|1.01|0||essig,weinessig,weißweinessig,rotweinessig,apfelessig,kräuteressig,balsamico,balsamessig,aceto balsamico,reisessig,branntweinessig,obstessig,sherryessig|vinegar,white vinegar,cider vinegar,apple cider vinegar,red wine vinegar,white wine vinegar,balsamic vinegar,rice vinegar,wine vinegar
pesto|Pesto|sosse|450 5 45 7 5 3 2 2.5|1.0|0|Glas=190|pesto,pesto genovese,pesto rosso,basilikumpesto|pesto,basil pesto
tomatensosse|Tomatensoße (fertig)|sosse|50 1.5 1.5 0.2 7 5 1.5 1|1.03|0|Glas=400|tomatensoße,tomatensauce,pastasoße,pizzasoße,nudelsoße,salsa,arrabiata|marinara sauce,spaghetti sauce,pasta sauce,pizza sauce,salsa,enchilada sauce
sahnesosse|Soße (Fertigprodukt, hell)|sosse|80 1.5 5 3 7 2 0.3 1.2|1.03|0|Pck.=250|bratensoße,soßenbinder,hollandaise,sauce hollandaise,béchamel,bechamelsoße,helle soße,dunkle soße,rahmsoße,jägersoße,soßenpulver,bratensaft|gravy,cream of mushroom soup,cream of chicken soup,cream of celery soup,condensed cream of mushroom soup,condensed soup,hollandaise sauce,white sauce,alfredo sauce,brown gravy
puddingpulver|Puddingpulver|suess|350 0.5 0.1 0 86 0 1 0.5|0.6|0|Pck.=37|puddingpulver,vanillepuddingpulver,schokoladenpuddingpulver,tortenguss,soßenpulver vanille,götterspeise|pudding mix,instant pudding,vanilla pudding mix,instant vanilla pudding,instant chocolate pudding,gelatin dessert,jell-o,jello
speiseeis|Speiseeis (Vanilleeis)|suess|200 3.5 11 7 23 21 0 0.2|0.55|0|Kugel=50|speiseeis,vanilleeis,eiscreme,sahneeis,schokoladeneis,erdbeereis,milchspeiseeis,softeis|ice cream,vanilla ice cream,chocolate ice cream
sahnesteif|Sahnesteif|sonstiges|370 0 0 0 92 30 0 0|0.6|0|Pck.=8|sahnesteif,sahnefestiger|whipped cream stabilizer
kaffee|Kaffee|getraenk|2 0.2 0 0 0.3 0 0 0|1.0|0||kaffee,espresso,starker kaffee,filterkaffee,instantkaffee,löslicher kaffee,tee,schwarztee,grüner tee,kräutertee,früchtetee|coffee,brewed coffee,strong coffee,espresso,instant coffee,tea,black tea,green tea
limonade|Limonade / Cola|getraenk|42 0 0 0 10.5 10.5 0 0|1.04|0||limonade,cola,limo,zitronenlimonade,ginger ale,sprite,fanta,tonic water,eistee|soda,cola,lemonade,ginger ale,lemon-lime soda,7-up,sprite,iced tea,tonic water
wein_weiss|Weißwein|alkohol|80 0.1 0 0 2.6 1 0 0|0.99|0|Flasche=750|weißwein,trockener weißwein,wein,riesling,sekt,prosecco,champagner|white wine,dry white wine,wine,champagne,sparkling wine,sherry,dry sherry,vermouth,mirin,marsala,sake,cooking wine
wein_rot|Rotwein|alkohol|85 0.1 0 0 2.6 0.6 0 0|0.99|0|Flasche=750|rotwein,trockener rotwein,portwein,madeira,glühwein|red wine,dry red wine,port,port wine,burgundy,madeira
bier|Bier|alkohol|43 0.5 0 0 3.6 0 0 0|1.01|0|Flasche=500;Dose=500|bier,helles,dunkles bier,pils,weizenbier,malzbier,starkbier|beer,ale,lager,dark beer,stout
spirituosen|Spirituosen|alkohol|230 0 0 0 0 0 0 0|0.95|0|Flasche=700|rum,brauner rum,weinbrand,cognac,brandy,kirschwasser,obstler,schnaps,wodka,gin,whisky,amaretto,grand marnier,cointreau,orangenlikör,likör,eierlikör,calvados,grappa,ouzo,kaffeelikör|rum,brandy,cognac,vodka,gin,whiskey,whisky,bourbon,tequila,liqueur,amaretto,grand marnier,triple sec,kahlua,coffee liqueur,kirsch,bourbon whiskey
ersatz_sonstiges|Kartoffelchips / Snacks|suess|540 6 34 3 50 1 4 1.5|0.1|0|Beutel=175|chips,kartoffelchips,tortillachips,salzstangen,cracker,erdnussflips,popcorn|potato chips,chips,tortilla chips,corn chips,pretzels,popcorn
"""

def parse():
    foods = []
    for line in ROWS.strip().splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        p = line.split('|')
        if len(p) != 9:
            sys.exit("Fehler in Zeile: " + line)
        key, name, cat, nut, dens, piece, units, de, en = p
        v = [float(x) for x in nut.split()]
        if len(v) != 8:
            sys.exit("Nährwerte unvollständig: " + key)
        unit_map = {}
        if units:
            for u in units.split(';'):
                k, g = u.split('=')
                unit_map[k.strip()] = float(g)
        aliases = [a.strip().lower() for a in (de + ',' + en).split(',') if a.strip()]
        # Duplikate entfernen, Reihenfolge behalten
        seen = set(); al = []
        for a in aliases:
            if a not in seen:
                seen.add(a); al.append(a)
        foods.append({
            "key": key, "name": name, "category": cat,
            "kcal": v[0], "protein": v[1], "fat": v[2], "saturatedFat": v[3],
            "carbohydrates": v[4], "sugar": v[5], "fiber": v[6], "salt": v[7],
            "density": float(dens), "piece": float(piece), "units": unit_map, "aliases": al,
        })
    return foods

if __name__ == "__main__":
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..")
    foods = parse()
    keys = [f["key"] for f in foods]
    assert len(keys) == len(set(keys)), "doppelte Schlüssel"
    out = os.path.join(root, "resources", "data", "lebensmittel.json")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump({"format": "VitaMeals-Foods", "version": 1,
                   "note": "Richtwerte je 100 g – Schätzwerte für die Rezeptplanung, keine Analysewerte.",
                   "foods": foods}, f, ensure_ascii=False, indent=1)
    print(len(foods), "Lebensmittel ->", out)
