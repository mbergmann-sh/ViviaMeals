#!/usr/bin/env python3
# Erzeugt resources/data/lebensmittel.json – die eingebaute Nährwerttabelle von ViviaMeals.
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
wasser|Wasser|getraenk|0 0 0 0 0 0 0 0|1.0|0||wasser,leitungswasser,kaltes wasser,warmes wasser,heißes wasser,mineralwasser,sprudelwasser,eiswasser,eiswürfel,crushed ice,sprudel|water,cold water,warm water,hot water,boiling water,ice water
weizenmehl|Weizenmehl Type 405|getreide|340 10 1 0.2 71 0.4 4 0|0.55|0||weizenmehl,mehl,mehl type 405,type 405,weißmehl,auszugsmehl,mehl type 550,weizenmehl type 550,dinkelmehl,dinkelmehl type 630|flour,all-purpose flour,all purpose flour,plain flour,white flour,bread flour,cake flour,self-rising flour,self raising flour,ap flour
vollkornmehl|Weizenvollkornmehl|getreide|320 12 2 0.3 60 0.4 10 0|0.55|0||vollkornmehl,weizenvollkornmehl,dinkelvollkornmehl,grahammehl|whole wheat flour,wholemeal flour,whole-wheat flour,graham flour
roggenmehl|Roggenmehl|getreide|320 7 1 0.2 68 1 8 0|0.55|0||roggenmehl,roggenvollkornmehl|rye flour
speisestaerke|Speisestärke|getreide|350 0.4 0.1 0 86 0 1 0|0.6|0||speisestärke,stärke,maisstärke,mondamin,kartoffelstärke,stärkemehl,gustin,maizena|cornstarch,corn starch,cornflour,potato starch
backpulver|Backpulver|sonstiges|150 0 0 0 37 0 0 26|0.9|0|Pck.=15|backpulver,backpulve|baking powder
natron|Natron|sonstiges|0 0 0 0 0 0 0 68|1.1|0||natron,kaiser natron,speisenatron,pottasche,hirschhornsalz|baking soda,bicarbonate of soda
hefe|Hefe (frisch)|sonstiges|100 12 1 0.2 8 0 7 0.1|1.0|42|Würfel=42|hefe,frischhefe,frische hefe,bäckerhefe|yeast,fresh yeast,cake yeast
trockenhefe|Trockenhefe|sonstiges|330 40 7 1 13 0 27 0.1|0.8|0|Pck.=7|trockenhefe|dry yeast,active dry yeast,instant yeast,rapid rise yeast
zucker|Zucker|zucker|400 0 0 0 100 100 0 0|0.85|0||zucker,weißer zucker,kristallzucker,feinzucker,haushaltszucker,meringen,baiser,baiserschalen,meringue|sugar,white sugar,granulated sugar,caster sugar,superfine sugar
brauner_zucker|Brauner Zucker|zucker|380 0.1 0 0 98 97 0 0.1|0.9|0||brauner zucker,rohrzucker,vollrohrzucker,muscovado|brown sugar,light brown sugar,dark brown sugar,packed brown sugar,raw sugar
puderzucker|Puderzucker|zucker|400 0 0 0 100 100 0 0|0.55|0||puderzucker,staubzucker,zuckerguss,zuckerguß,fondant|powdered sugar,confectioners sugar,confectioners' sugar,icing sugar
vanillezucker|Vanillezucker|zucker|400 0 0 0 99 99 0 0|0.8|0|Pck.=8|vanillezucker,vanillinzucker,bourbon-vanillezucker|vanilla sugar
honig|Honig|honig|310 0.4 0 0 82 75 0 0|1.4|0||honig,bienenhonig|honey
ahornsirup|Ahornsirup|zucker|260 0 0.1 0 67 60 0 0|1.3|0||ahornsirup|maple syrup
fruchtsirup|Fruchtsirup (Grenadine, Himbeersirup)|zucker|270 0 0 0 67 65 0 0|1.3|0||grenadine,grenadinesirup,himbeersirup,holundersirup,holunderblütensirup,fruchtsirup,erdbeersirup,kirschsirup|grenadine,raspberry syrup,elderflower syrup,fruit syrup
zuckerruebensirup|Zuckerrübensirup|zucker|300 1 0 0 74 64 0 0.2|1.4|0||zuckerrübensirup,rübensirup,melasse,sirup,goldsirup|molasses,golden syrup,corn syrup,light corn syrup,dark corn syrup,treacle
backmalz|Backmalz / Malzextrakt|zucker|330 5 0.5 0.1 72 50 4 0.1|1.3|0||backmalz,malz,gerstenmalz,malzextrakt,gerstenmalz-extrakt,gerstenmalzextrakt,flüssigmalz,malzsirup|malt,malt extract,barley malt,malt syrup,diastatic malt
vanilleschote|Vanilleschote|gewuerz|290 3 0 0 13 13 0 0|0.5|3|Stange=3|vanilleschote,vanille,vanillemark,vanillestange,vanilleschoten,vanillestangen|vanilla bean,vanilla pod
vanilleextrakt|Vanilleextrakt|gewuerz|290 0 0 0 13 13 0 0|0.9|0||vanilleextrakt,vanillearoma,vanilleessenz,bittermandelaroma,aroma,bittermandelöl,zitronenbacköl,citro-back,orangenbacköl,backaroma,rumaroma,butter-vanille-aroma|vanilla extract,vanilla,vanilla essence,almond extract
kakao|Kakaopulver|suess|360 20 21 13 11 1 33 0.1|0.45|0||kakao,kakaopulver,backkakao,kaba,nesquik|cocoa,cocoa powder,unsweetened cocoa
schokolade_zartbitter|Zartbitterschokolade|suess|540 7 33 20 47 42 9 0|0.6|0|Tafel=100|zartbitterschokolade,bitterschokolade,schokolade,schokotropfen,zartbitter-schokotropfen,schokodrops,schokoladentropfen,schokostreusel,kuvertüre,zartbitterkuvertüre,blockschokolade,halbbitterschokolade,kuchenglasur,zartbitter-kuchenglasur,schokoladenglasur,schokoglasur,schokoraspeln,schokoladenraspeln,schokoflocken,schokotröpfchen,raspelschokolade,after eight|chocolate,dark chocolate,semisweet chocolate,semi-sweet chocolate,bittersweet chocolate,chocolate chips,semisweet chocolate chips,baking chocolate,unsweetened chocolate
schokolade_vollmilch|Vollmilchschokolade|suess|540 7 31 19 57 55 2 0.2|0.6|0|Tafel=100|vollmilchschokolade,milchschokolade,vollmilchkuvertüre|milk chocolate,milk chocolate chips
schokolade_weiss|Weiße Schokolade|suess|550 6 32 19 59 59 0 0.2|0.6|0|Tafel=100|weiße schokolade,weisse schokolade|white chocolate,white chocolate chips
marmelade|Konfitüre|suess|250 0.4 0 0 61 58 1 0|1.3|0||marmelade,konfitüre,gelee,aprikosenkonfitüre,erdbeerkonfitüre,johannisbeergelee,wildpreiselbeerkompott,preiselbeerkompott,mangochutney,chutney|jam,jelly,preserves,marmalade,apricot jam
nussnougatcreme|Nuss-Nougat-Creme|suess|540 6 31 10 58 56 3 0.1|1.2|0||nuss-nougat-creme,nutella,nougatcreme,nougat,nougatmasse,nussnougat,nuss-nougat|chocolate hazelnut spread,nutella
marzipan|Marzipanrohmasse|suess|460 9 25 2 49 46 5 0|1.2|0||marzipan,marzipanrohmasse,raffaello,kokos-mandel-konfekt,kokos-mandel-pralinen|marzipan,almond paste
gelatine|Gelatine|gelatine|340 85 0 0 0 0 0 0.3|0.7|0|Blatt=1.7;Scheibe=1.7;Pck.=9|gelatine,blattgelatine,gelatinepulver|gelatin,gelatine,unflavored gelatin
agar|Agar-Agar|sonstiges|300 6 0.3 0 81 0 0 0.3|0.6|0||agar-agar,agar|agar,agar-agar
butter|Butter|milch|740 0.7 82 53 0.6 0.6 0 0|0.95|0||butter,süßrahmbutter,sauerrahmbutter,ungesalzene butter,gesalzene butter,zerlassene butter,weiche butter,kalte butter,bztter|butter,unsalted butter,salted butter,sweet butter,melted butter,softened butter
margarine|Margarine|fett|720 0.2 80 25 0.4 0 0 0.3|0.95|0||margarine,pflanzenmargarine,alsan,fett,fett für die form,fett fürs blech,fett zum ausbacken,fett zum frittieren,rama,sanella,becel,backmargarine|margarine,oleo,shortening,vegetable shortening,crisco
butterschmalz|Butterschmalz|milch|900 0 99.5 65 0 0 0 0|0.9|0||butterschmalz,ghee|ghee,clarified butter
schmalz|Schweineschmalz|fett|900 0 99 39 0 0 0 0|0.9|0||schmalz,schweineschmalz,griebenschmalz|lard
oel|Pflanzenöl|fett|880 0 99.8 9 0 0 0 0|0.92|0||öl,pflanzenöl,speiseöl,rapsöl,sonnenblumenöl,erdnussöl,maiskeimöl,neutrales öl,bratöl,frittieröl,distelöl,chiliöl,knoblauchöl,kräuteröl,walnussöl,traubenkernöl,tomatenöl|oil,vegetable oil,canola oil,salad oil,cooking oil,sunflower oil,peanut oil,corn oil,chili oil,walnut oil,grapeseed oil
olivenoel|Olivenöl|fett|880 0 99.6 14 0 0 0 0|0.92|0||olivenöl,natives olivenöl,olivenöl extra vergine|olive oil,extra virgin olive oil,extra-virgin olive oil
sesamoel|Sesamöl|fett|880 0 99.8 14 0 0 0 0|0.92|0||sesamöl,kürbiskernöl,leinöl|sesame oil,toasted sesame oil
kokosfett|Kokosfett|fett|880 0.8 99 87 0 0 0 0|0.92|0||kokosfett,kokosöl,palmin,pflanzenfett,plattenfett,biskin,frittierfett,palmfett|coconut oil
milch|Milch (3,5 %)|milch|64 3.3 3.5 2.3 4.8 4.8 0 0.1|1.03|0||milch,vollmilch,frischmilch,kuhmilch,warme milch,lauwarme milch,kalte milch|milk,whole milk,warm milk,scalded milk
milch_fettarm|Fettarme Milch (1,5 %)|milch|47 3.4 1.5 1 4.9 4.9 0 0.1|1.03|0||fettarme milch,magermilch,teilentrahmte milch,h-milch|skim milk,skimmed milk,low-fat milk,lowfat milk,nonfat milk
buttermilch|Buttermilch|milch|37 3.5 0.5 0.3 4 4 0 0.1|1.03|0||buttermilch|buttermilk
kondensmilch|Kondensmilch (7,5 %)|milch|130 6.5 7.5 5 10 10 0 0.2|1.07|0|Dose=340|kondensmilch,dosenmilch,kaffeesahne|evaporated milk,half-and-half,half and half
gezuckerte_kondensmilch|Gezuckerte Kondensmilch|milch|320 8 8.5 5.5 55 55 0 0.3|1.3|0|Dose=397|gezuckerte kondensmilch,milchmädchen,kondensmiclh|sweetened condensed milk,condensed milk
sahne|Schlagsahne (30 %)|milch|290 2.4 30 20 3.2 3.2 0 0.1|1.0|0|Becher=200|sahne,schlagsahne,süße sahne,rahm,schlagrahm,konditorsahne,kochsahne|cream,heavy cream,whipping cream,heavy whipping cream,light cream,single cream
creme_double|Crème double (45 %)|milch|420 2 45 29 2.5 2.5 0 0.1|1.0|0|Becher=200|crème double,creme double,doppelrahm,double cream|double cream,clotted cream
saure_sahne|Saure Sahne (10 %)|milch|115 3 10 7 4 4 0 0.1|1.0|0|Becher=200|saure sahne,sauerrahm,schmand,crème fraîche,creme fraiche,creme fraîche,crème fraiche|sour cream,soured cream,creme fraiche,crème fraîche
joghurt|Joghurt (3,5 %)|milch|65 3.5 3.5 2.3 4.5 4.5 0 0.1|1.03|0|Becher=150|joghurt,naturjoghurt,vollmilchjoghurt,joghurt natur,kefir,dickmilch,sauermilch|yogurt,yoghurt,plain yogurt,natural yogurt
rahmjoghurt|Rahmjoghurt (10 %)|milch|125 3 10 6.5 4.5 4.5 0 0.1|1.03|0|Becher=150|rahmjoghurt,sahnejoghurt,griechischer joghurt 10 %,griechischer joghurt|full fat greek yogurt,greek yogurt
quark|Magerquark|milch|67 12 0.3 0.2 4 4 0 0.1|1.05|0|Becher=250;Packung=250|quark,magerquark,speisequark,topfen|quark,farmer's cheese
quark_sahne|Sahnequark (40 %)|milch|160 11 11 7 3 3 0 0.1|1.05|0|Becher=250|sahnequark,schichtkäse|cream quark
frischkaese|Frischkäse (Doppelrahmstufe)|kaese|260 5.5 24 16 3.5 3.5 0 0.8|1.05|0|Packung=200;Becher=200|frischkäse,doppelrahmfrischkäse,philadelphia,rahmfrischkäse|cream cheese,neufchatel cheese
mascarpone|Mascarpone|kaese|440 4.5 46 31 3.5 3.5 0 0.1|1.0|0|Becher=250|mascarpone|mascarpone
ricotta|Ricotta|kaese|150 10 11 7 3 3 0 0.3|1.05|0|Becher=250|ricotta|ricotta,ricotta cheese
huettenkaese|Hüttenkäse|kaese|100 12 4.5 3 2.5 2.5 0 0.8|1.05|0|Becher=200|hüttenkäse,körniger frischkäse|cottage cheese,small curd cottage cheese
gouda|Gouda|kaese|360 25 29 19 0 0 0 2|0.5|25|Scheibe=25|gouda,käse,geriebener käse,reibekäse,gratinkäse,pizzakäse|cheese,shredded cheese,grated cheese,monterey jack cheese,gouda
mozzarella|Mozzarella|kaese|250 18 19 12.5 1 1 0 0.5|1.0|125|Packung=125;Kugel=125|mozzarella,büffelmozzarella|mozzarella,mozzarella cheese,fresh mozzarella
feta|Feta|kaese|260 17 21 14 0.7 0.7 0 2.8|1.0|0|Packung=200|feta,schafskäse,hirtenkäse,fetakäse|feta,feta cheese
# --- Käse nach Gruppen (Käseverordnung: Hart-, Schnitt-, halbfester Schnitt-, Weich-, Frisch-, Sauermilchkäse; dazu Pasta filata, Blauschimmel, Schmelzkäse) ---
emmentaler|Emmentaler / Maasdamer / Leerdammer|kaese|380 28.7 30 18 0 0 0 0.5|0.5|25|Scheibe=25|emmentaler,emmentaler käse,allgäuer emmentaler,schweizer emmentaler,maasdamer,leerdammer,leerdamer,leerdamer im stück,jarlsberg,lochkäse,washington valley swiss,swiss cheese|emmental,emmentaler,swiss cheese,jarlsberg,maasdam,baby swiss
edamer|Edamer / Gouda (30–40 %)|kaese|330 27 25 16 0 0 0 2|0.5|25|Scheibe=25|edamer,leichter gouda,gouda light,leidse kaas,leidener käse,leyden,kümmelkäse,mimolette,fol epi,stirling colby|edam,edam cheese,leyden,mimolette
cheddar|Cheddar|kaese|403 25 33 21 1.3 0.5 0 1.6|0.5|25|Scheibe=25|cheddar,cheddarkäse,chester,chesterkäse,red leicester,double gloucester,cheshire,pinconning,bega brown wax,pyengana cloth cheddar,farmers union vintage,south cape vintage,jumbunna,watsonia,epicure,mainland cheese|cheddar,cheddar cheese,sharp cheddar,mild cheddar,aged cheddar,red leicester,double gloucester,cheshire
schnittkaese|Schnittkäse, halbfest (Butterkäse, Tilsiter, Raclette)|kaese|350 23 28 18 0 0 0 1.8|0.5|25|Scheibe=25|butterkäse,tilsiter,tilsiter käse,havarti,esrom,danbo,steppenkäse,trappistenkäse,trappist,bel paese,bel päse,port salut,saint-paulin,raclette,raclettekäse,raclette-käse,monterey jack,colby,brick-käse,queso andino,nøkkelost,nökkelost,nokkelost,gewürzkäse,schnittkäse,tilsiter vollfett,wilstermarsch,weißlacker,limberger schnittkäse|monterey jack,pepper jack,colby,colby jack,havarti,muenster cheese,brick cheese,raclette,port salut,provel
hartkaese|Hartkäse (Bergkäse, Gruyère, Comté)|kaese|400 28 32 19 0 0 0 1.5|0.5|0|Scheibe=25|hartkäse,bergkäse,greyerzer,gruyère,gruyere,comté,comte,appenzeller,alpkäse,beaufort,manchego,alter manchego,kephalotiri,formaggella,asiago,montasio,bitto,bra-käse,bra duro,canestrato,casciotta,fontina,formai de mut,monte veronese,veronese,raschera,toma piemontese,valle d'aosta fromadzo,valtellina casera,axridda,desmond,gabriel-käse,yak-käse,yakkäse,rajya metok,quillisascut,zamorano,idiazábal,idiazabal,mahón,mahon,tête de moine,vacherin fribourgeois,tilsiter hart,hobelkäse,heumilchkäse,vorarlberger bergkäse|gruyere,gruyere cheese,swiss gruyere,comte,comte cheese,beaufort,manchego,manchego cheese,asiago,fontina,montasio,appenzeller,alpine cheese,hard cheese
parmesan|Parmesan / Grana / Pecorino|kaese|390 35 28 18 0 0 0 1.6|0.4|0||parmesan,parmigiano,parmigiano reggiano,grana padano,grana,pecorino,pecorino romano,pecorino sardo,pecorino toscano,fiore sardo,ragusano,sbrinz,reibkäse hart,parmesanspäne,parmesankäse|parmesan,parmesan cheese,grated parmesan,romano cheese,pecorino,pecorino romano,grana padano,parmigiano-reggiano
camembert|Camembert / Brie (Weißschimmel)|kaese|300 20 24 16 0 0 0 1.5|1.0|125|Packung=125|camembert,brie,brie de meaux,brie de melun,coulommiers,neufchâtel-käse,weißschimmelkäse,weichkäse,cooleeney,sakura cheese,sakura-käse,saint-marcellin,saint-félicien,chaource,ofenkäse,back-camembert|camembert,brie,soft cheese,soft-ripened cheese
dreifachrahm|Dreifachrahm-Weichkäse (Brillat-Savarin, Cambozola)|kaese|420 11 41 27 2 2 0 1.3|1.0|0||brillat-savarin,brillat savarin,saint-andré,saint andré,délice de bourgogne,explorateur,jindi triple cream,triple crème,dreifachrahmkäse,doppelrahm-weichkäse,cambozola,bavaria blu,bavaria blue,manouri|triple cream,triple-cream brie,cambozola,brillat-savarin
rotschmier|Rotschmierkäse (Limburger, Münster, Taleggio)|kaese|300 21 24 15 0 0 0 2.2|1.0|0||limburger,chaumes,romadur,münsterkäse,munster,münster-käse,époisses,epoisses,livarot,pont-l'évêque,pont-l'eveque,maroilles,taleggio,reblochon,vacherin mont d'or,mont d'or,herve,weinkäse,rotschmierkäse,stinkkäse,bergader,ardrahan,durrus,gubbeen,milleens,red hawk,tarrawingee washed rind,mungabareena washed rind,top paddock washed rind,top paddock wine washed,obazda,obatzda,obatzter|limburger,taleggio,reblochon,epoisses,washed-rind cheese,munster
blauschimmel|Blauschimmelkäse (Gorgonzola, Roquefort, Stilton)|kaese|360 21 30 19 0.5 0.5 0 3|1.0|0||blauschimmelkäse,blauschimmel,gorgonzola,gorgonzola dolce,gorgonzola piccante,dolcelatte,roquefort,stilton,blue stilton,bleu d'auvergne,fourme d'ambert,bleu de bresse,saint agur,danablu,dänischer blauschimmelkäse,cabrales,valdeón,valdeon,maytag blue,gippsland blue,chiangrai blauschimmelkäse,edelpilzkäse,bergader edelpilz|blue cheese,gorgonzola,roquefort,stilton,danish blue,crumbled blue cheese,maytag blue
ziegenkaese|Ziegenkäse (Rolle, Taler, Crottin)|kaese|320 20 26 18 1 1 0 1.4|1.0|0|Taler=40;Rolle=150|ziegenkäse,ziegenkäserolle,ziegenkäserollen,ziegenkäsetaler,ziegenweichkäse,ziegenfrischkäse,ziegencamembert,chèvre,chevre,crottin de chavignol,crottin,sainte-maure,valençay,bûcheron,bucheron,chabichou,picodon,banon,pélardon,haystack peak,goat cheese,st. tola,queso de cabra,queso de cabra en aceite,schafsweichkäse,caprino|goat cheese,goat's cheese,chevre,fresh goat cheese,goat cheese log,crottin
halloumi|Halloumi / Grillkäse|kaese|320 21 25 17 2 2 0 2.8|1.0|0|Packung=225|halloumi,hellim,grillkäse,bratkäse,ofenkäse zum grillen,chiangrai halloumi,juustoleipä,queso para freír,queso frito|halloumi,grilling cheese,bread cheese,juustoleipa
paneer|Paneer / Queso fresco (ungesalzener Frischkäse)|kaese|290 18 22 14 3 2 0 0.2|1.0|0|Packung=250|paneer,panir,queso fresco,queso blanco,kesong puti,chhena,bïshtak,bishtak,irmtschuk,bjaslag,frischkäse zum braten|paneer,queso fresco,queso blanco,farmer cheese
burrata|Burrata / Stracciatella|kaese|290 15 25 17 1.5 1.5 0 0.6|1.0|125|Kugel=125|burrata,stracciatella,stracciatella di bufala|burrata,stracciatella
pasta_filata|Provolone / Scamorza / Caciocavallo|kaese|350 25.6 26.6 17 2 0.6 0 2.2|0.5|0||provolone,provolone dolce,provolone piccante,scamorza,scamorza affumicata,caciocavallo,kashkaval,kaschkawal,oaxaca-käse,string cheese,fadenkäse,zopfkäse,pizza-käse|provolone,smoked provolone,scamorza,caciocavallo,string cheese,oaxaca cheese,kashkaval
sauermilchkaese|Harzer / Handkäse (Sauermilchkäse)|kaese|125 28 0.7 0.4 0 0 0 3.5|1.0|0|Rolle=200|harzer,harzer käse,harzer roller,mainzer,mainzer käse,handkäse,handkäs,handkäs mit musik,olmützer quargel,quargel,korbkäse,stangenkäse,nieheimer,nieheimer käse,sauermilchkäse,spitzkäse,bauernhandkäse,graukäse,steirerkas|sour milk cheese,harzer cheese
kochkaese|Kochkäse / Potkäse|kaese|180 18 10 6 3 3 0 2.5|1.05|0|Becher=200|kochkäse,kochkäs,hessischer kochkäse,potkäse,pultost|kochkaese
schmelzkaese|Schmelzkäse / Scheiblettenkäse|kaese|290 14 23 15 6 5 0 2.6|1.0|20|Scheibe=20;Ecke=17|schmelzkäse,schmelzkäsezubereitung,scheibletten,scheiblettenkäse,toastkäse,sandwichkäse,streichkäse,käsecreme,käseecken,dreieckskäse,la vache qui rit,velveeta,cheez whiz,american cheese,amerikanischer käse,cheeseburger-scheiben|processed cheese,american cheese,cheese slices,velveeta,cheez whiz,cheese spread,cheese sauce
brunost|Brunost / Gjetost (Molkenkäse)|kaese|440 10 28 18 40 40 0 0.8|1.0|0||brunost,gjetost,geitost,mysost,braunkäse,molkenkäse,norwegischer braunkäse|brunost,gjetost,mysost,brown cheese
ei|Ei (Größe M)|ei|137 12.5 9.5 2.7 0.7 0.7 0 0.3|1.03|58||ei,eier,hühnerei,hühnereier,ganzes ei,verquirltes ei|egg,eggs,large egg,large eggs,whole egg,beaten egg
eigelb|Eigelb|ei|340 16 32 9 0.3 0.3 0 0.1|1.03|18||eigelb,eidotter,dotter|egg yolk,egg yolks,yolk,yolks
eiweiss|Eiweiß|ei|50 11 0.2 0 0.7 0.7 0 0.4|1.03|35||eiweiß,eiklar,eischnee|egg white,egg whites,whites
haferflocken|Haferflocken|getreide|370 13.5 7 1.3 59 0.7 10 0|0.4|0||haferflocken,zarte haferflocken,kernige haferflocken,porridge|oats,rolled oats,quick oats,oatmeal,old-fashioned oats
muesli|Müsli|getreide|370 9 7 1.5 64 22 8 0.1|0.45|0||müsli,früchtemüsli,granola,crunchy müsli|muesli,granola
cornflakes|Cornflakes|getreide|375 7 1 0.2 84 8 3 1.8|0.12|0||cornflakes,cornflakes ungezuckert|corn flakes,cornflakes
reis|Reis (roh)|getreide|350 7 0.6 0.2 78 0.2 1.4 0|0.85|0|Beutel=125|reis,langkornreis,basmatireis,basmati,jasminreis,risottoreis,arborio,milchreis,rundkornreis,parboiled reis|rice,long grain rice,long-grain rice,white rice,basmati rice,jasmine rice,arborio rice,uncooked rice
reis_gekocht|Reis (gekocht)|getreide|130 2.7 0.3 0.1 28 0 0.4 0|0.8|0||gekochter reis|
vollkornreis|Vollkornreis (roh)|getreide|350 7.5 2.2 0.5 74 0.7 3.5 0|0.85|0||vollkornreis,naturreis,wildreis|brown rice,wild rice
nudeln|Nudeln (roh)|getreide|355 12.5 1.5 0.3 71 3 3 0|0.5|0||nudeln,pasta,spaghetti,spaghettini,makkaroni,maccheroni,penne,fusilli,farfalle,tagliatelle,bandnudeln,hörnchennudeln,rigatoni,lasagneplatten,lasagne-platten,suppennudeln,spätzle,eiernudeln,linguine,fettuccine,tortellini,orecchiette,orecchiete,kritharaki,risoni,orzo,maccharoni,kurze maccharoni|pasta,noodles,spaghetti,macaroni,elbow macaroni,penne,fusilli,lasagna noodles,lasagne noodles,egg noodles,fettuccine,linguine,rigatoni,ziti
vollkornnudeln|Vollkornnudeln (roh)|getreide|340 13 2.5 0.4 64 3 8 0|0.5|0||vollkornnudeln,vollkornspaghetti|whole wheat pasta,whole-wheat pasta
couscous|Couscous|getreide|360 12 1.5 0.3 72 1 5 0|0.8|0||couscous,bulgur,weizengrieß,hartweizengrieß|couscous,bulgur,bulgur wheat
griess|Grieß|getreide|350 10 1 0.2 72 0.5 4 0|0.7|0||grieß,weichweizengrieß,dinkelgrieß|semolina,cream of wheat,farina
polenta|Polenta (Maisgrieß)|getreide|350 8.5 1.5 0.2 75 1 5 0|0.7|0||polenta,maisgrieß,maismehl|polenta,cornmeal,yellow cornmeal,corn meal,grits
quinoa|Quinoa|getreide|370 14 6 0.7 64 0 7 0|0.8|0||quinoa,amaranth,hirse,buchweizen|quinoa,millet,buckwheat,amaranth
gruenkern|Grünkern|getreide|330 11 2.7 0.4 63 1 9 0|0.75|0||grünkern,grünkernschrot,grünkernmehl,grünkerngrieß|green spelt,green spelt grains
brot|Brot (Mischbrot)|backwaren|230 7 1.2 0.2 46 2.5 5 1.3|0.3|50|Scheibe=45|brot,mischbrot,graubrot,bauernbrot,weißbrot,toastbrot,toast,ciabatta,baguette,landbrot|bread,white bread,french bread,italian bread,sourdough bread,loaf,bread slices,toast
vollkornbrot|Vollkornbrot|backwaren|210 7 1.3 0.2 39 3 8 1.1|0.4|50|Scheibe=50|vollkornbrot,schwarzbrot,roggenbrot,pumpernickel|whole wheat bread,wholemeal bread,rye bread,pumpernickel
knaeckebrot|Knäckebrot (Vollkorn)|backwaren|340 10 1.5 0.3 65 2.5 15 1|0.3|10|Scheibe=10;Stück=10|knäckebrot,vollkornknäckebrot,roggenknäckebrot,knäcke|crispbread,rye crispbread
broetchen|Brötchen|backwaren|260 8.5 1.5 0.3 51 2.5 3 1.2|0.2|50||brötchen,semmel,semmeln,schrippe,wecken,laugenbrötchen,laugenbrezen,laugenbrezel,brezen,brezel,laugengebäck|roll,rolls,bun,buns,hamburger buns,hot dog buns,dinner rolls
semmelbroesel|Semmelbrösel|backwaren|360 11 2 0.4 72 4 5 1.2|0.45|0||semmelbrösel,paniermehl,brösel,weckmehl|bread crumbs,breadcrumbs,dry bread crumbs,panko,cracker crumbs
zwieback|Zwieback / Kekse|backwaren|420 9 9 4 72 15 3 0.7|0.4|10||zwieback,biskuits|crackers,graham crackers,graham cracker crumbs,vanilla wafers,saltines
kekse|Kekse / Lebkuchen / Gebäck|backwaren|440 6 16 7 68 35 2 0.5|0.5|10|Packung=200|kekse,keks,butterkekse,spekulatius,lebkuchen,braune lebkuchen,saucenlebkuchen,soßenlebkuchen,printen,hohlhippen,waffelröllchen,oblaten,backoblaten,löffelbiskuits,amarettini,cantuccini,makronen|cookies,biscuits,gingerbread,ladyfingers,wafers,amaretti
tortilla|Weizentortilla|backwaren|310 8.5 7.5 3 51 3 3 1.3|0.3|40||tortilla,tortillas,wrap,wraps,fladenbrot,pitabrot,pita,naan,wan-tan-blätter,wan-tan-teig,wantan-blätter,frühlingsrollenteig,yufka|tortilla,tortillas,flour tortillas,corn tortillas,pita,pita bread,naan
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
tomaten_dose|Tomaten (Dose, gehackt)|gemuese|22 1.2 0.2 0 3.5 3.5 1 0.3|1.0|0|Dose=400|dosentomaten,stückige tomaten,pizzatomaten,passierte tomaten,passata,tomatenpüree,polpa|crushed tomatoes,whole tomatoes,stewed tomatoes,tomato puree,tomato sauce,passata,can tomatoes
tomatenmark|Tomatenmark|gemuese|85 4.5 0.5 0.1 15 11 4 0.4|1.1|0|Dose=70;Tube=200|tomatenmark,tomatenmark zweifach konzentriert|tomato paste
getrocknete_tomaten|Getrocknete Tomaten (in Öl)|gemuese|210 5 14 2 17 10 6 1.5|0.7|0|Glas=280|sonnengetrocknete tomaten|sun-dried tomatoes,sun dried tomatoes,dried tomatoes
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
salat|Blattsalat|gemuese|15 1.3 0.2 0 1.5 1.3 1.5 0|0.2|300|Kopf=300|salat,kopfsalat,eisbergsalat,römersalat,feldsalat,rucola,blattsalat,lollo rosso,endivie,frisée,salatblätter,chicorée,chicoree,chicoreestauden,radicchio,lollo bionda,löwenzahn,romanasalatherzen,romana-salatherzen,salatherzen,romanasalat|lettuce,iceberg lettuce,romaine,romaine lettuce,salad greens,mixed greens,arugula,rocket,lettuce leaves
champignons|Champignons|gemuese|20 3 0.3 0 0.6 0.2 2 0|0.4|20|Dose=230;Schale=250|champignons,pilze,braune champignons,steinpilze,pfifferlinge,austernpilze,shiitake,kräuterseitlinge,egerlinge,morcheln,morchel,trompetenpilze|mushrooms,mushroom,button mushrooms,fresh mushrooms,sliced mushrooms,shiitake mushrooms,cremini mushrooms,portobello mushrooms,morels
mais|Mais (Dose)|gemuese|80 2.7 1.2 0.2 14 4 3 0.5|0.8|0|Dose=285|mais,zuckermais,maiskörner,gemüsemais,maiskolben|corn,sweet corn,corn kernels,whole kernel corn,canned corn,frozen corn,cream-style corn,corn on the cob
erbsen|Erbsen (TK)|gemuese|80 6.5 0.5 0.1 11 4 5 0|0.65|0|Dose=280|erbsen,tk-erbsen,junge erbsen,zuckerschoten,zuckererbsen,kaiserschoten|peas,green peas,frozen peas,snow peas,sugar snap peas
gruene_bohnen|Grüne Bohnen|gemuese|32 2.4 0.2 0 5 2.5 2 0|0.5|0|Dose=220|grüne bohnen,brechbohnen,prinzessbohnen,stangenbohnen,buschbohnen,schnippelbohnen|green beans,string beans,snap beans,french beans,wax beans
spargel|Spargel|gemuese|20 2 0.1 0 2 2 1.5 0|0.5|25||spargel,weißer spargel,grüner spargel,palmherzen,palmitos|asparagus,asparagus spears
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
apfelmus|Apfelmus|obst|75 0.2 0.1 0 18 16 1.4 0|1.05|0|Glas=360|apfelmus,apfelmark,aprikosenkompott,kompott,obstkompott,birnenkompott|applesauce,apple sauce
birne|Birne|obst|55 0.4 0.3 0 12 10 3 0|0.6|170||birne,birnen|pear,pears
banane|Banane|obst|95 1.1 0.2 0.1 21 17 2 0|0.6|120||banane,bananen,reife banane,reife bananen|banana,bananas,ripe banana,ripe bananas,mashed bananas
orange|Orange|obst|47 1 0.2 0 9 8.5 2 0|0.6|180||orange,orangen,apfelsine,apfelsinen,blutorange|orange,oranges,navel orange
zitrone|Zitrone|obst|35 0.7 0.6 0 3.2 2.5 1.3 0|0.6|100||zitrone,zitronen,bio-zitrone,unbehandelte zitrone|lemon,lemons
zitronensaft|Zitronensaft|obst|25 0.4 0.2 0 2.5 2.5 0.3 0|1.03|0||zitronensaft,limettensaft,saft einer zitrone,zitronensaft frisch gepresst|lemon juice,lime juice,fresh lemon juice
zitronenschale|Zitronenschale|gewuerz|50 1.5 0.3 0 5 4 10 0|0.5|0||zitronenschale,abrieb,zitronenabrieb,orangenschale,limettenschale,zitronat,orangeat|lemon zest,lemon peel,grated lemon peel,orange zest,orange peel,lime zest,candied peel
orangensaft|Orangensaft|getraenk|45 0.7 0.2 0 9 9 0.2 0|1.04|0||orangensaft,o-saft,apfelsaft,traubensaft,multivitaminsaft,fruchtsaft,saft,ananassaft,kirschsaft,apfelsaft naturtrüb|orange juice,apple juice,juice,pineapple juice,grape juice,cranberry juice,fruit juice,apple cider
erdbeere|Erdbeeren|obst|32 0.7 0.3 0 5.5 5 2 0|0.6|15|Schale=500|erdbeere,erdbeeren|strawberry,strawberries
beeren|Beeren (gemischt)|obst|40 1 0.4 0 6.5 5.5 4 0|0.6|0|Packung=300|beeren,beerenmischung,tk-beeren,früchte,rote früchte,johannisbeer,himbeer,heidelbeer,brombeer,stachelbeer,preiselbeer,johannisbeer-rispen,johannisbeer-blätter|berries,mixed berries,frozen berries
kirschen|Süßkirschen|obst|60 0.9 0.3 0 13 13 1.5 0|0.65|8|Glas=350|kirschen,süßkirschen,kirsche|cherries,cherry,sweet cherries,maraschino cherries,pie filling,cherry pie filling
pflaume|Pflaumen|obst|47 0.6 0.2 0 10 10 1.6 0|0.65|30||pflaume,pflaumen,zwetschgen,zwetschken|plum,plums,prune plums,damson plums
aprikose|Aprikose / Marille|obst|45 0.9 0.1 0 8.5 8 1.5 0|0.65|40|Dose=480|aprikose,aprikosen,marille,marillen|apricot,apricots
ananas|Ananas|obst|55 0.5 0.2 0 12 12 1.4 0|0.65|1000|Dose=340|ananas,ananasstücke,ananasscheiben|pineapple,crushed pineapple,pineapple chunks,pineapple tidbits
mango|Mango|obst|62 0.6 0.4 0.1 13 13 1.7 0|0.65|350||mango,mangos|mango,mangoes
trauben|Weintrauben|obst|70 0.7 0.3 0 16 16 1.5 0|0.65|5||trauben,weintrauben,weintraube|grapes,green grapes,red grapes
kiwi|Kiwi|obst|60 1 0.6 0 10 9 2 0|0.65|75||kiwi,kiwis,kiwipüree|kiwi,kiwis,kiwifruit
melone|Melone|obst|35 0.6 0.2 0 8 8 0.5 0|0.6|1500||melone|melon
rhabarber|Rhabarber|obst|20 0.6 0.1 0 1.4 1 3.2 0|0.6|100|Stange=100|rhabarber|rhubarb
rosinen|Rosinen|obst|300 2.5 0.6 0.1 68 66 5 0|0.65|0||rosinen,sultaninen,korinthen,trockenfrüchte|raisins,golden raisins,dried fruit
trockenobst|Trockenobst|obst|260 2.5 0.5 0 58 50 9 0|0.65|0||getrocknete aprikosen,softaprikosen,trockenpflaumen,backpflaumen,getrocknete feigen,dörrobst,cranberries getrocknet,backobst,dörrpflaumen,backpflaume|dried apricots,prunes,dried figs,dried cranberries,craisins,dried mixed fruit
# --- Obst: Kern- und Steinobst ---
quitte|Quitte|obst|38 0.4 0.5 0 7.3 7 6 0|0.65|300||quitte,quitten,apfelquitte,birnenquitte|quince,quinces
nashi|Nashi-Birne|obst|42 0.5 0.2 0 10.7 7 3.6 0|0.6|200||nashi,nashibirne,nashi-birne,nashibirnen,asiatische birne|asian pear,nashi pear
mispel|Wollmispel / Mispel|obst|47 0.4 0.2 0 12 10 1.7 0|0.65|30||mispel,mispeln,wollmispel,wollmispeln,japanische wollmispel,nispero,loquat|loquat,loquats,medlar
pfirsich|Pfirsich / Plattpfirsich|obst|41 0.8 0.1 0 9 8.5 1.9 0|0.65|150|Dose=480|pfirsich,pfirsiche,plattpfirsich,plattpfirsiche,weinbergpfirsich,weinbergpfirsiche,bergpfirsich,pfirsichhälften|peach,peaches,canned peaches,peach halves,donut peach,flat peach
nektarine|Nektarine|obst|44 1 0.3 0 10 8 1.7 0|0.65|150||nektarine,nektarinen|nectarine,nectarines
sauerkirschen|Sauerkirschen|obst|53 0.9 0.5 0.1 9.9 9.9 1 0|0.65|5|Glas=350|sauerkirschen,sauerkirsche,schattenmorellen,schattenmorelle,weichseln,weichselkirschen,amarenakirschen,amarena|sour cherries,tart cherries,morello cherries,amarena cherries
mirabellen|Mirabellen / Renekloden|obst|64 0.7 0.2 0 14 14 0.9 0|0.65|10||mirabellen,mirabelle,reneklode,renekloden,reineclauden,reineclaude,ringlotten,kriecherl,kriecherln,spänling|mirabelle plums,mirabelles,greengages,greengage
schlehen|Schlehen|obst|54 0.7 0.3 0 9 9 4 0|0.65|1||schlehen,schlehe,schlehdorn,schwarzdorn|sloes,sloe berries
# --- Beeren ---
himbeeren|Himbeeren|obst|36 1.3 0.3 0 4.8 4.4 6.7 0|0.55|4|Schale=125;Packung=250|himbeeren,himbeere,waldhimbeeren,tk-himbeeren|raspberries,raspberry,frozen raspberries
heidelbeeren|Heidelbeeren / Blaubeeren|obst|42 0.6 0.6 0 7.4 7.4 4.9 0|0.6|1|Schale=125;Packung=250|heidelbeeren,heidelbeere,blaubeeren,blaubeere,kulturheidelbeeren,waldheidelbeeren,bickbeeren,schwarzbeeren|blueberries,blueberry,wild blueberries,bilberries
brombeeren|Brombeeren|obst|43 1.2 1 0 6.2 6 3.2 0|0.6|5|Schale=125|brombeeren,brombeere,taybeeren,loganbeeren,boysenbeeren|blackberries,blackberry,boysenberries,loganberries,tayberries
johannisbeeren|Johannisbeeren, rot / weiß|obst|43 1.1 0.2 0 7.3 4.8 3.5 0|0.6|0|Schale=125|johannisbeeren,rote johannisbeeren,weiße johannisbeeren,johannisbeere,ribisel,träubele,johannisbeerrispen|red currants,redcurrants,white currants,currants
johannisbeeren_schwarz|Schwarze Johannisbeeren / Jostabeeren|obst|63 1.3 0.2 0 10 10 6.8 0|0.6|0|Schale=125|schwarze johannisbeeren,schwarze johannisbeere,jostabeeren,jostabeere|black currants,blackcurrants,jostaberries
stachelbeeren|Stachelbeeren|obst|44 0.8 0.2 0 8.5 7 3 0|0.6|4||stachelbeeren,stachelbeere|gooseberries,gooseberry
preiselbeeren|Preiselbeeren / Cranberries (frisch)|obst|44 0.4 0.4 0 9 5 4 0|0.6|0||preiselbeeren,preiselbeere,cranberries,cranberry,moosbeeren,kranbeeren,kulturpreiselbeeren|cranberries,fresh cranberries,lingonberries
holunderbeeren|Holunderbeeren|obst|55 2.5 1.7 0 7.4 7.4 7 0|0.6|0||holunderbeeren,holunderbeere,holler,fliederbeeren,schwarzer holunder|elderberries,elderberry
sanddorn|Sanddorn|obst|89 1.4 7.1 1 5 3 3 0|0.6|0||sanddorn,sanddornbeeren,sanddornfrüchte|sea buckthorn,sea buckthorn berries
aronia|Aronia / Apfelbeere|obst|47 1.4 0.5 0 7 6 5.3 0|0.6|0||aronia,aroniabeeren,apfelbeere,apfelbeeren,maulbeeren,maulbeere,moltebeeren,multebeeren,felsenbirne|aronia,chokeberries,mulberries,cloudberries,saskatoon berries
hagebutten|Hagebutten|obst|90 1.6 0.6 0 13 13 24 0|0.6|0||hagebutten,hagebutte,hiffen,hagebuttenmark|rose hips,rosehips
physalis|Physalis / Kapstachelbeere|obst|53 1.9 0.7 0 11 11 1 0|0.6|4|Schale=100|physalis,kapstachelbeere,kapstachelbeeren,andenbeere,andenbeeren,lampionfrucht,blasenkirsche,goldbeeren,golden berries|physalis,cape gooseberries,ground cherries,golden berries
goji|Goji-Beeren (getrocknet)|obst|350 14 0.4 0 64 46 13 0.7|0.6|0||goji,gojibeeren,goji-beeren,wolfsbeeren,bocksdornbeeren,berberitzen,getrocknete berberitzen,inkabeeren|goji berries,dried goji berries,wolfberries,barberries,dried barberries
# --- Zitrusfrüchte ---
mandarine|Mandarine / Clementine|obst|50 0.7 0.3 0 10 10 1.8 0|0.6|70|Dose=300|mandarine,mandarinen,clementine,clementinen,satsuma,satsumas,tangerine,tangerinen,mineola,mineolas,tangelo,mandarinenspalten|mandarin,mandarins,mandarin oranges,clementine,clementines,tangerine,tangerines,satsuma
grapefruit|Grapefruit / Pomelo|obst|38 0.6 0.1 0 7.4 7 1.6 0|0.6|350||grapefruit,grapefruits,rosa grapefruit,pampelmuse,pampelmusen,pomelo,pomelos,ugli,sweetie,oroblanco|grapefruit,pink grapefruit,pomelo,ugli fruit
limette|Limette|obst|30 0.7 0.2 0 7.5 1.7 2.8 0|0.6|65||limette,limetten,limone grün,kaffirlimette,kaffir-limette,kalamansi|lime,limes,key lime,kaffir lime,calamansi
kumquat|Kumquat|obst|71 1.9 0.9 0.1 9.4 9 6.5 0|0.6|15||kumquat,kumquats,zwergorange,zwergorangen,limequat|kumquat,kumquats
zitrusfrucht_sonst|Yuzu / Bergamotte / Zedrat|obst|35 0.7 0.3 0 6 2.5 2 0|0.6|100||yuzu,bergamotte,bergamotten,zedrat,zitronatzitrone,cedro,buddhas hand,bitterorange,bitterorangen,pomeranze,pomeranzen,sevilla-orange|yuzu,bergamot,citron,buddha's hand,bitter orange,seville orange
# --- Südfrüchte / Tropen ---
papaya|Papaya|obst|43 0.5 0.3 0.1 9 8 1.7 0|0.65|400||papaya,papayas,papayafrucht,grüne papaya|papaya,papayas,green papaya
maracuja|Maracuja / Passionsfrucht / Grenadilla|obst|97 2.2 0.7 0.1 13 11 10.4 0|0.65|30||maracuja,maracujas,passionsfrucht,passionsfrüchte,grenadilla,grenadillas,granadilla,maracujamark,maracujapüree|passion fruit,passionfruit,granadilla,passion fruit pulp
kaki|Kaki / Sharonfrucht|obst|70 0.6 0.2 0 16 16 2.5 0|0.65|200||kaki,kakis,kakifrucht,kakifrüchte,sharonfrucht,sharonfrüchte,persimone,persimonen,dattelpflaume|persimmon,persimmons,sharon fruit,kaki
granatapfel|Granatapfel (Kerne)|obst|83 1.7 1.2 0.1 17 14 4 0|0.65|150|Stück=150|granatapfel,granatäpfel,granatapfelkerne,granatapfelkernen,granatapfelsamen|pomegranate,pomegranates,pomegranate seeds,pomegranate arils
feige|Feige (frisch)|obst|74 0.8 0.3 0.1 16 16 2.9 0|0.65|50||feige,feigen,frische feigen,frische feige,blaue feigen,grüne feigen|fig,figs,fresh figs
datteln|Datteln (Medjool, Deglet Nour)|obst|282 2.5 0.4 0 64 63 8 0|0.65|8|Packung=200|datteln,dattel,medjool-datteln,medjooldatteln,medjool,deglet nour,entsteinte datteln,softdatteln|dates,date,medjool dates,pitted dates
litschi|Litschi / Rambutan / Longan|obst|66 0.8 0.4 0.1 15 15 1.3 0|0.65|20|Dose=425|litschi,litschis,lychee,lychees,litchi,litchis,rambutan,rambutans,longan,longans,drachenauge,pulasan|lychee,lychees,litchi,rambutan,longan
guave|Guave|obst|68 2.6 1 0.3 8.9 8.9 5.4 0|0.65|100||guave,guaven,guajave,guava,guavas,feijoa,feijoas,ananasguave|guava,guavas,feijoa,pineapple guava
cherimoya|Cherimoya / Annone|obst|75 1.6 0.7 0.2 18 13 3 0|0.65|400||cherimoya,cherimoyas,cherimoja,annone,annonen,rahmapfel,zimtapfel,süßsack,soursop,graviola,stachelannone|cherimoya,custard apple,sugar apple,soursop,guanabana
sternfrucht|Sternfrucht / Karambole|obst|31 1 0.3 0 3.5 4 2.8 0|0.65|90||sternfrucht,sternfrüchte,karambole,karambolen,karambolefrucht,carambola,baumstachelbeere|star fruit,starfruit,carambola
pitahaya|Pitahaya / Drachenfrucht|obst|57 1.2 0.4 0.1 11 8 3 0|0.65|350||pitahaya,pitahayas,pitaya,drachenfrucht,drachenfrüchte,drachenauge-kaktus|dragon fruit,dragonfruit,pitaya,pitahaya
kiwano|Kiwano (Horngurke) / Pepino|obst|30 1.4 0.9 0 5 3 1.5 0|0.65|200||kiwano,kiwanos,horngurke,horngurken,hornmelone,melano,pepino,pepinos,melonenbirne,birnenmelone|kiwano,horned melon,pepino melon
tamarillo|Tamarillo (Baumtomate)|obst|31 2 0.4 0 4 3 3.3 0|0.65|80||tamarillo,tamarillos,baumtomate,baumtomaten,lulo,naranjilla|tamarillo,tree tomato,naranjilla,lulo
kaktusfeige|Kaktusfeige|obst|41 0.7 0.5 0 9.6 9.6 3.6 0|0.65|80||kaktusfeige,kaktusfeigen,feigenkaktusfrucht,tuna-frucht,opuntie|prickly pear,cactus pear,tuna fruit
mangostane|Mangostane / Salak / Sapodilla|obst|73 0.4 0.6 0.1 18 16 1.8 0|0.65|80||mangostane,mangostanen,mangostin,salak,salakfrucht,schlangenhautfrucht,sapodilla,breiapfel,sapote,mamey|mangosteen,salak,snake fruit,sapodilla,sapote,mamey
durian|Durian / Jackfrucht (reif)|obst|120 1.6 3 0.6 25 18 2.5 0|0.65|0||durian,durians,stinkfrucht,jackfrucht,jackfruit,jackfrüchte,reife jackfrucht,cempedak,brotfrucht,brotfrüchte|durian,jackfruit,ripe jackfruit,breadfruit
jackfruit_jung|Junge Jackfruit (Fleischersatz, abgetropft)|obst|30 0.7 0.2 0 4 1 3 0.4|0.8|0|Dose=280|junge jackfruit,grüne jackfruit,jackfruit in lake,jackfruit natur,jackfruit pulled|young jackfruit,green jackfruit,jackfruit in brine
kochbanane|Kochbanane (Plantain)|obst|122 1.3 0.4 0.1 32 15 2.3 0|0.6|250||kochbanane,kochbananen,plantain,plantains,mehlbanane,mehlbananen,plátano macho,platano|plantain,plantains,cooking banana,green plantain
kokosnuss_frisch|Kokosnuss (frisches Fruchtfleisch)|nuss|354 3.3 33 30 6 6 9 0|0.6|0|Stück=400|kokosnuss,kokosnüsse,kokosnussfleisch,kokosfleisch,kokosnussstücke|coconut meat,coconut flesh
kokoswasser|Kokoswasser|getraenk|19 0.7 0.2 0.2 3.7 2.6 1.1 0.25|1.02|0|Packung=330|kokoswasser,kokosnusswasser|coconut water
acerola|Acerola|obst|32 0.4 0.3 0 7.7 0 1.1 0|0.65|5||acerola,acerolakirsche,acerolakirschen,barbados-kirsche|acerola,acerola cherry,barbados cherry
# --- Melonen ---
wassermelone|Wassermelone|obst|30 0.6 0.2 0 7.6 6.2 0.4 0|0.6|3000||wassermelone,wassermelonen,wassermelonenstücke|watermelon,watermelons
honigmelone|Honigmelone / Netzmelone (Cantaloupe, Galia)|obst|35 0.7 0.2 0 8.2 7.9 0.9 0|0.6|1200||honigmelone,honigmelonen,netzmelone,netzmelonen,zuckermelone,zuckermelonen,cantaloupe,cantaloupe-melone,galiamelone,galia-melone,galia,charentais,charentais-melone,piel de sapo,gelbe melone,cavaillon|cantaloupe,honeydew,honeydew melon,galia melon,muskmelon,rock melon
kokosraspel|Kokosraspel|nuss|660 6.5 62 55 7 7 16 0|0.35|0||kokosraspel,kokosflocken|coconut,shredded coconut,flaked coconut,desiccated coconut,sweetened coconut
kokosmilch|Kokosmilch|nuss|180 1.8 18 16 3 2 0 0|1.0|0|Dose=400|kokosmilch,kokoscreme,kokosnussmilch|coconut milk,coconut cream,lite coconut milk
mandeln|Mandeln|nuss|600 22 53 4 5.5 4 11 0|0.55|0|Packung=200|mandeln,gemahlene mandeln,mandelblättchen,mandelstifte,geschälte mandeln,mandelmehl,mandelsplitter,mandelblätter,mandelkerne|almonds,sliced almonds,slivered almonds,ground almonds,almond flour,almond meal,blanched almonds
haselnuesse|Haselnüsse|nuss|650 14 61 4.5 10.5 4 8 0|0.55|0|Packung=200|haselnüsse,haselnuss,gemahlene haselnüsse,haselnusskerne,krokant|hazelnuts,filberts,ground hazelnuts
walnuesse|Walnüsse|nuss|680 15 65 6 10.5 2.6 6 0|0.45|0||walnüsse,walnuss,walnusskerne,pekannüsse,pekannuss|walnuts,chopped walnuts,pecans,pecan halves,chopped pecans
nuesse|Nüsse (gemischt)|nuss|620 18 55 7 12 4 8 0|0.55|0||nüsse,nusskerne,gehackte nüsse,cashewkerne,cashews,pistazien,macadamia,paranüsse,pinienkerne,nussmischung|nuts,chopped nuts,mixed nuts,cashews,pistachios,macadamia nuts,pine nuts,brazil nuts
maronen|Maronen / Esskastanien|nuss|196 1.6 1.3 0.2 44 11 8 0|1.0|10||maronen,esskastanien,kastanien,edelkastanien,gekochte esskastanien|chestnuts,roasted chestnuts
maronenpueree|Maronenpüree, gesüßt|nuss|250 1.5 1 0.2 57 45 3 0|1.2|0|Dose=440|maronenpüree,kastanienpüree,maronencreme,gesüßtes maronenpüree,gesüßtes kastanienpüree|chestnut puree,chestnut spread,chestnut cream
erdnuesse|Erdnüsse|nuss|590 25 49 7 12 4 8 0|0.55|0||erdnüsse,erdnuss,erdnusskerne,gesalzene erdnüsse|peanuts,dry roasted peanuts,salted peanuts
erdnussbutter|Erdnussbutter|nuss|600 25 50 10 15 8 6 1|1.1|0||erdnussbutter,erdnusscreme,erdnussmus,mandelmus,cashewmus,nussmus|peanut butter,creamy peanut butter,crunchy peanut butter,almond butter
samen|Saaten (Sesam, Sonnenblumenkerne …)|nuss|580 21 50 6 11 1 11 0|0.6|0||sesam,sesamsaat,sonnenblumenkerne,kürbiskerne,leinsamen,chiasamen,mohn,blaumohn,saaten,mohnsamen|sesame seeds,sunflower seeds,pumpkin seeds,pepitas,flax seeds,flaxseed,chia seeds,poppy seeds
tahin|Tahin|nuss|600 17 54 7.5 21 0.5 9 0.1|1.1|0||tahin,tahini,sesampaste|tahini,sesame paste
linsen|Linsen (getrocknet)|huelsen|330 24 1.5 0.2 48 2 17 0|0.8|0||linsen,rote linsen,berglinsen,tellerlinsen,belugalinsen,gelbe linsen,beluga linsen|lentils,red lentils,green lentils,brown lentils,dried lentils
kichererbsen|Kichererbsen (Dose, abgetropft)|huelsen|120 7 2.5 0.3 16 1 6 0.5|0.7|0|Dose=240|kichererbsen|chickpeas,garbanzo beans,garbanzos,canned chickpeas
bohnen_dose|Bohnen (Dose, abgetropft)|huelsen|100 7 0.5 0.1 15 1 7 0.5|0.7|0|Dose=250|bohnen,kidneybohnen,weiße bohnen,schwarze bohnen,rote bohnen,cannellini-bohnen,wachtelbohnen,baked beans,dicke bohnen,saubohnen,limabohnen,borlotti|beans,kidney beans,red kidney beans,black beans,white beans,navy beans,pinto beans,cannellini beans,great northern beans,lima beans,baked beans,butter beans,refried beans
bohnen_trocken|Getrocknete Bohnen|huelsen|300 22 1.5 0.3 45 3 20 0|0.8|0||weiße bohnenkerne,schälerbsen,trockenerbsen|dried beans,dry beans,split peas,dried peas
tofu|Tofu|huelsen|130 13 8 1.2 1.5 0.6 1 0|1.0|0|Packung=200|tofu,räuchertofu,seidentofu,naturtofu|tofu,firm tofu,extra-firm tofu,silken tofu
sojadrink|Sojadrink|huelsen|40 3.3 1.8 0.3 2.5 2 0.6 0.1|1.03|0||sojadrink,sojamilch,mandeldrink,haferdrink,hafermilch,mandelmilch,reisdrink,pflanzendrink|soy milk,soymilk,almond milk,oat milk,rice milk
sojasahne|Sojasahne / Hafercuisine|huelsen|170 2.5 16 2 4 1 0.5 0.1|1.0|0|Packung=200|sojasahne,hafersahne,hafercuisine,pflanzensahne,sojacuisine|soy cream,non-dairy creamer,coconut whipped cream
sojaschnetzel|Sojaschnetzel / Fleischersatz|huelsen|340 50 1.5 0.2 18 7 18 0.1|0.3|0||sojaschnetzel,sojagranulat,seitan,veganes hack,vegane hack,fleischersatz,vegetarisches hack|textured vegetable protein,tvp,seitan,meat substitute,veggie crumbles
hackfleisch|Hackfleisch (gemischt)|fleisch|250 19 19.5 8 0 0 0 0.2|1.0|0|Packung=500|hackfleisch,gehacktes,hack,gemischtes hackfleisch,faschiertes|hamburger,ground meat,mince,ground chuck,ground round,ground turkey,sausage meat
rindfleisch|Rindfleisch (mager)|fleisch|130 22 4.5 2 0 0 0 0.1|1.0|0||rindfleisch,rinderbraten,rindergulasch,gulasch,gulaschfleisch,rinderhüfte,rinderfilet,roastbeef,suppenfleisch,tafelspitz,beinscheibe,rinderrouladen,rouladen,schmorfleisch,rinderbrust,ochsenbacke|beef,stew meat,beef stew meat,round steak,chuck roast,pot roast,beef roast,roast,brisket,stewing beef,beef chuck,top round,london broil
kalbfleisch|Kalbfleisch (mager)|fleisch|110 20 3 1 0 0 0 0.2|1.0|0|Medaillon=80;Schnitzel=150|kalbfleisch,kalbsfilet,kalbsnuss,kalbsschnitzel,kalbsbraten,kalbsrücken,kalbsgeschnetzeltes,kalb|veal,veal cutlet,veal loin
kalbsleber|Kalbsleber|fleisch|135 20 4.5 1.5 3 0 0 0.2|1.0|0|Scheibe=120|kalbsleber,kalbslebern,kalbsleberscheiben,kalbsleberstreifen,leber vom kalb|calf liver,calf's liver,calves liver,veal liver
kalbsniere|Kalbsniere|fleisch|100 16 3 1 1 0 0 0.45|1.0|300|Stück=300|kalbsniere,kalbsnieren,kalbsnierchen,nieren vom kalb,kalbsnierle|veal kidney,veal kidneys,calf kidney,calf kidneys
kalbszunge|Kalbszunge|fleisch|180 16 13 5 0.5 0 0 0.2|1.0|400|Stück=400|kalbszunge,kalbszungen,zunge vom kalb|veal tongue,calf tongue,calf's tongue
kalbsbries|Kalbsbries (Milken)|fleisch|100 17 3 1 0 0 0 0.2|1.0|0||kalbsbries,kalbsmilken,kalbsmilke,milken,bries,kalbsbröschen|sweetbreads,veal sweetbreads,calf sweetbreads
kalbshaxe|Kalbshaxe / Beinscheibe (mit Knochen)|fleisch|85 14 3 1.1 0 0 0 0.1|1.0|0|Beinscheibe=300;Haxe=1200|kalbshaxe,kalbshaxen,kalbshachse,kalbshachsen,kalbsbeinscheibe,kalbsbeinscheiben,kalbshaxenscheiben,ossobuco,osso buco|veal shank,veal shanks,osso buco,ossobuco
kalbsbrust|Kalbsbrust|fleisch|160 18.5 9.5 4 0 0 0 0.2|1.0|0||kalbsbrust,gefüllte kalbsbrust,kalbsbrustspitz|veal breast,breast of veal
wild|Wild (Reh, Hirsch)|fleisch|120 23 2.4 1 0 0 0 0.1|1.0|0|Medaillon=80|wild,wildfleisch,reh,rehfleisch,rehrücken,rehkeule,rehblatt,rehblätter,rehmedaillons,rehrückenmedaillons,hirsch,hirschfleisch,hirschmedaillons,hirschkeule,wildschwein,wildgulasch|venison,deer,elk,wild boar
schweinefleisch|Schweinefleisch (mager)|fleisch|140 21 6 2 0 0 0 0.1|1.0|0||schweinefleisch,schweineschnitzel,schnitzel,schweinefilet,schweinelende,kotelett,koteletts,schweinebraten,schweinenacken,schweinebauch,schweineschulter,kasseler,geschnetzeltes,spareribs,schweinegulasch|pork,pork chops,pork loin,pork tenderloin,pork roast,pork shoulder,boneless pork chops,pork butt,spareribs,ribs,pork belly
gefluegel|Hähnchenbrustfilet|fleisch|105 23.5 1 0.3 0 0 0 0.2|1.0|150||hähnchenbrust,hähnchenbrustfilet,hähnchenbrustfilets,hühnerbrust,hühnerbrustfilet,putenbrust,putenbrustfilet,putenschnitzel,hähnchenfilet,hühnchen,geflügel,putenfleisch,hähnchengeschnetzeltes,putengeschnetzeltes,hähnchen|chicken breast,chicken breasts,boneless chicken breasts,boneless skinless chicken breasts,chicken,turkey breast,chicken tenders,turkey,cooked chicken,diced chicken,chicken meat
haehnchenschenkel|Hähnchenkeule / Hähnchenschenkel|fleisch|190 18 13 3.7 0 0 0 0.2|1.0|250||hähnchenkeule,hähnchenschenkel,hühnerschenkel,hähnchenkeulen,hühnerkeulen,hähnchenflügel,chicken wings,freilandhuhn|chicken thighs,chicken wings,chicken pieces
ente|Ente, ganz (mit Knochen)|fleisch|235 8 20 6 0 0 0 0.1|1.0|2200|Stück=2200|ente,enten,ganze ente,flugente,barbarie-ente,pekingente,peking-ente,wildente,hafermastente|duck,whole duck,roast duck
kaninchen|Kaninchen / Hase|fleisch|130 21 5 2 0 0 0 0.1|1.0|0||kaninchen,hase,hasenrücken,hasenrückenfilets,kaninchenrücken,kaninchenkeule,hasenkeule,hasenkeulen,wildhase,kaninchenfleisch|rabbit,hare
gefluegelleber|Geflügelleber|fleisch|135 19 5 1.6 1 0 0 0.2|1.0|0||geflügelleber,hühnerleber,hähnchenleber,entenleber,leber,schweineleber,rinderleber|chicken liver,chicken livers,liver
lamm|Lammfleisch|fleisch|200 18 14 6 0 0 0 0.2|1.0|0||lamm,lammfleisch,lammkeule,lammkoteletts,lammrücken,lammschulter,zicklein,ziegenfleisch,capretto|lamb,leg of lamb,lamb chops,lamb shoulder
speck|Speck (durchwachsen)|fleisch|400 13 38 14 0 0 0 3|1.0|0|Scheibe=10;Packung=125|speck,schinkenspeck,frühstücksspeck,bacon,bauchspeck,speckwürfel,räucherspeck,pancetta,lardons|bacon,bacon slices,bacon strips,salt pork,pancetta,slab bacon
schinken|Kochschinken|fleisch|110 19 3.5 1.2 1 1 0 2.3|1.0|0|Scheibe=25|schinken,kochschinken,gekochter schinken,hinterschinken,schinkenwürfel,putenschinken,lachsschinken|ham,cooked ham,diced ham,ham slices,deli ham
rohschinken|Rohschinken|fleisch|240 25 15 5 0 0 0 5.5|1.0|0|Scheibe=15|rohschinken,parmaschinken,serranoschinken,katenschinken,schwarzwälder schinken,prosciutto,bündnerfleisch|prosciutto,cured ham,smoked ham,country ham
wurst|Wurst (Brühwurst)|fleisch|280 12 25 9 1 0.5 0 2|1.0|100||wurst,würstchen,wiener,wiener würstchen,frankfurter,bockwurst,bratwurst,bratwürste,fleischwurst,lyoner,leberkäse,weißwurst,mettwurst,cabanossi,salami,mortadella,leberwurst|sausage,sausages,hot dogs,frankfurters,bratwurst,kielbasa,polish sausage,italian sausage,pepperoni,salami,smoked sausage,link sausage
# --- Hackfleisch, Steaks, Lamm, Hammel ---
hack_rind|Rinderhackfleisch|fleisch|215 20 15 6.5 0 0 0 0.2|1.0|0|Packung=500|rinderhack,rinderhackfleisch,rindergehacktes,hackfleisch vom rind,hackfleisch rind,beefsteakhack,tatarhack,rinderfaschiertes,hamburgerfleisch,burger patties,carne picada de ternera|ground beef,beef mince,lean ground beef,hamburger meat
hack_schwein|Schweinehackfleisch|fleisch|260 18 21 8 0 0 0 0.2|1.0|0|Packung=500|schweinehack,schweinehackfleisch,hackfleisch vom schwein,hackfleisch schwein,schweinegehacktes,schweinefaschiertes,bratwurstbrät,brät|ground pork,pork mince
mett|Mett / Hackepeter (gewürzt)|fleisch|260 18 21 8 0.5 0.3 0 1.8|1.0|0||mett,hackepeter,zwiebelmett,thüringer mett,schweinemett,gehacktes gewürzt|seasoned pork mince
hack_lamm|Lammhackfleisch|fleisch|280 17 23.4 10.2 0 0 0 0.2|1.0|0|Packung=400|lammhack,lammhackfleisch,hackfleisch vom lamm,lammgehacktes,lammfaschiertes,kofta-fleisch|ground lamb,lamb mince
hack_lamm_rind|Hackfleisch Lamm/Rind (gemischt)|fleisch|245 18.5 19 8.3 0 0 0 0.2|1.0|0|Packung=500|lamm-rind-hack,lamm- und rinderhack,hackfleisch lamm und rind,hackfleisch vom lamm und rind,gemischtes hack lamm rind,rind-lamm-hack|ground lamb and beef,lamb and beef mince
schweinesteak|Schweinesteak (Nacken, Kamm)|fleisch|180 18 12 4.5 0 0 0 0.2|1.0|180|Stück=180|schweinesteak,schweinesteaks,nackensteak,nackensteaks,schweinenackensteak,kammsteak,kammsteaks,halssteak,schweinehalssteak,grillsteak,grillsteaks,schweinerückensteak,minutensteak vom schwein|pork steak,pork steaks,pork shoulder steak,pork neck steak
rindersteak|Rindersteak (Rumpsteak, Entrecôte)|fleisch|155 22 7 3 0 0 0 0.1|1.0|220|Stück=220|rindersteak,rindersteaks,steak,steaks,rumpsteak,rumpsteaks,entrecôte,entrecote,entrecotes,ribeye,rib-eye,rib eye steak,hüftsteak,hüftsteaks,flanksteak,flank steak,flat iron steak,t-bone-steak,t-bone steak,porterhouse,porterhouse-steak,sirloin,sirloin steak,clubsteak,rinderrückensteak,minutensteak|steak,beef steak,sirloin,rib eye,ribeye,strip steak,new york strip,t-bone,porterhouse,flank steak,skirt steak,rump steak
hammel|Hammelfleisch|fleisch|230 17 18 8.5 0 0 0 0.2|1.0|0||hammelfleisch,hammel,hammelkeule,hammelschulter,hammelkotelett,hammelrücken,hammelragout,schaffleisch,schafsfleisch|mutton,mutton leg,mutton shoulder
# --- Geflügelteile ---
huhn_ganz|Huhn / Hähnchen, ganz (mit Knochen)|fleisch|160 13 12 3.4 0 0 0 0.2|1.0|1200|Stück=1200|ganzes hähnchen,ganzes huhn,huhn,brathähnchen,brathuhn,grillhähnchen,maishähnchen,stubenküken,poularde,poulet,freilandpoulet,hendl,brathendl,suppenhuhn,suppenhühner|whole chicken,roasting chicken,broiler,fryer,poussin,stewing hen
haehnchenbein|Hähnchenbein / Unterschenkel|fleisch|160 18.5 9.5 2.6 0 0 0 0.2|1.0|80|Stück=80|hähnchenbein,hähnchenbeine,hähnchenunterschenkel,hähnchenunterkeulen,hühnerbeine,hendlhaxerl,drumsticks,hähnchen-drumsticks,chicken drumsticks|drumstick,drumsticks,chicken drumsticks,chicken legs
entenbrust|Entenbrust (mit Haut)|fleisch|200 18.5 14 4.5 0 0 0 0.2|1.0|300|Stück=300|entenbrust,entenbrüste,entenbrustfilet,entenbrustfilets,entenbrüstchen,barbarie-entenbrust,magret,magret de canard|duck breast,duck breasts,magret
entenkeule|Entenkeule (essbarer Anteil)|fleisch|220 17 17 5 0 0 0 0.2|1.0|200|Stück=200|entenkeule,entenkeulen,entenschenkel,confit de canard,entenconfit,ente confit|duck leg,duck legs,duck confit
gans|Gans, ganz (mit Knochen)|fleisch|240 10 22 6.5 0 0 0 0.1|1.0|4500|Stück=4500|gans,gänse,ganze gans,martinsgans,weihnachtsgans,bauerngans,hafermastgans,mastgans|goose,whole goose
gaensebrust|Gänsebrust (mit Haut)|fleisch|230 18 17 5 0 0 0 0.2|1.0|500|Stück=500|gänsebrust,gänsebrüste,gänsebrustfilet,geräucherte gänsebrust,pommersche gänsebrust|goose breast
gaensekeule|Gänsekeule (essbarer Anteil)|fleisch|250 17 20 6 0 0 0 0.2|1.0|330|Stück=330|gänsekeule,gänsekeulen,gänseschenkel|goose leg,goose legs
# --- französische Fleischpasteten ---
pastete|Pastete / Terrine (Pâté de campagne)|fleisch|320 14 28 10 2 0.5 0 1.8|1.0|0|Dose=125;Glas=180|pastete,pâté,pate,pâté de campagne,landpastete,bauernpastete,terrine,fleischterrine,wildterrine,wildpastete,geflügelterrine,pâté de lapin,kaninchenpastete,entenpastete,pâté de canard,galantine,pâté hénaff,pâté breton,pastete vom schwein|pate,pâté,country pate,terrine,pate de campagne,game terrine
pastete_teig|Pastete im Teig (Pâté en croûte)|fleisch|330 12 22 8 20 1 1 1.6|1.0|0||pâté en croûte,pate en croute,pastete im teig,fleischpastete im teig,pâté lorrain,pate lorrain,schinkenpastete,wildpastete im teig,pastetchen mit fleisch,pork pie|pate en croute,meat pie,pork pie,game pie
leberpastete|Leberpastete / Leberparfait|fleisch|300 12 27 9 3 1 0 1.8|1.0|0|Dose=125|leberpastete,leberparfait,leberterrine,pâté de foie,pate de foie,mousse de foie,geflügelleberpastete,geflügelleberparfait,leberpastete im glas|liver pate,chicken liver pate,liver mousse,liver parfait
rillettes|Rillettes|fleisch|450 15 43 16 0 0 0 1.5|1.0|0|Glas=180|rillettes,rillettes de porc,rillettes de canard,rillettes d'oie,rillettes de tours,schweineschmalzfleisch|rillettes,potted pork,potted duck
foie_gras|Foie gras (Gänse-/Entenstopfleber)|fleisch|460 7 44 15 4 1 0 1.2|1.0|0||foie gras,foie gras de canard,foie gras d'oie,gänsestopfleber,entenstopfleber,gänseleberpastete,stopfleberpastete,bloc de foie gras|foie gras,goose liver pate,duck liver pate
# --- Spanische Tapas (zubereitet, je 100 g) ---
tortilla_espanola|Tortilla de patatas (Kartoffel-Omelett)|ei|145 6 9 1.6 11 1.2 1 0.6|1.0|0|Portion=150;Pincho=120|tortilla de patatas,tortilla española,tortilla espanola,spanische tortilla,kartoffeltortilla,kartoffel-omelett,kartoffelomelett,pincho de tortilla|spanish omelette,spanish tortilla,potato omelette,tortilla de patatas
ensaladilla_rusa|Ensaladilla rusa (Kartoffelsalat mit Thunfisch)|fisch|180 4 14 2 9 1.8 1.5 0.6|1.0|0|Portion=150|ensaladilla rusa,ensalada rusa,russischer salat,spanischer kartoffelsalat|russian salad,olivier salad,ensaladilla rusa
jamon_iberico|Jamón ibérico|fleisch|375 31 28 9 0 0 0 4.5|1.0|0|Scheibe=10|jamón ibérico,jamon iberico,jamón ibérico de bellota,jamón de bellota,pata negra,ibérico-schinken,iberico-schinken,jamón de jabugo,lomo ibérico,paleta ibérica|iberian ham,jamon iberico,iberico ham,pata negra
patatas_bravas|Patatas bravas (mit Soße)|kartoffel|180 2.5 10 1.3 20 2 2.2 0.8|1.0|0|Portion=200|patatas bravas,papas bravas,bravas,salsa brava,bravas-soße|patatas bravas,spicy potatoes,bravas sauce
croquetas|Croquetas (Kroketten mit Schinken)|fleisch|230 8 12 4 22 2 1 1.0|1.0|30|Stück=30|croquetas,croquetas de jamón,croquetas de jamon,croqueta,schinkenkroketten,béchamelkroketten,croquetas de pollo,croquetas de bacalao|croquettes,ham croquettes,croquetas
calamares_romana|Calamares a la romana (frittiert)|fisch|220 12 11 2 18 0.5 0.8 0.8|1.0|0|Portion=150|calamares a la romana,calamares fritos,frittierte tintenfischringe,panierte tintenfischringe,tintenfischringe paniert,backteig-calamari,chipirones a la andaluza,puntillitas,puntillitas fritas,pescadito frito,frittierte fischchen,boquerones fritos|fried calamari,fried squid rings,calamari fritti,fritto misto
gazpacho|Gazpacho / Salmorejo (kalte Suppe)|gemuese|60 1.3 4 0.6 4.5 3.5 1 0.5|1.0|0|Glas=250;Portion=250|gazpacho,gazpacho andaluz,salmorejo,salmorejo cordobés,ajoblanco,kalte gemüsesuppe|gazpacho,salmorejo,cold tomato soup
gambas_ajillo|Gambas al ajillo (in Knoblauchöl)|fisch|220 15 17 2.5 1.5 0.3 0.3 0.9|1.0|0|Portion=150|gambas al ajillo,gambas pil pil,gambas pil-pil,knoblauchgarnelen,garnelen in knoblauchöl,gambas a la plancha|garlic shrimp,gambas al ajillo,garlic prawns
pimientos_padron|Pimientos de Padrón (Bratpaprika)|gemuese|25 1 0.3 0.1 3.5 2.5 1.8 0|0.4|8|Schale=200|pimientos de padrón,pimientos de padron,padrón-paprika,padron-paprika,padrón-paprikaschoten,bratpaprika,pimientos,pimientos fritos|padron peppers,pimientos de padron,shishito peppers
pimientos_piquillo|Pimientos del piquillo (geröstet, im Glas)|gemuese|30 1.2 0.3 0.1 6 4 1.5 0.5|1.0|0|Glas=290|pimientos del piquillo,piquillo-paprika,piquillos,piquillo,eingelegte paprika in knoblauch,pimientos asados|piquillo peppers,roasted piquillo peppers
albondigas|Albóndigas in Tomatensoße|fleisch|170 10 11 4 7 3 1 0.9|1.0|25|Stück=25;Portion=200|albóndigas,albondigas,albóndigas en salsa,albóndigas en salsa de tomate,spanische hackbällchen,hackbällchen in tomatensoße|spanish meatballs,albondigas,meatballs in tomato sauce
chorizo|Chorizo (Paprikawurst)|fleisch|450 24 38 14 2 1 0 3.5|1.0|0|Stück=80|chorizo,chorizos,chorizowürste,chorizo picante,chorizo dulce,chorizo a la riojana,chorizo a la sidra,chistorra,sobrasada,paprikawurst,txistorra|chorizo,spanish chorizo,chorizo sausage
blutwurst|Blutwurst (Morcilla)|fleisch|330 13 29 11 3 0 0 2|1.0|0||blutwurst,morcilla,morcilla de burgos,morcilla frita,rotwurst,flönz,boudin noir,black pudding,blunzn,grützwurst,schwarzwurst|black pudding,blood sausage,morcilla,boudin noir
empanada|Empanada (gefüllte Teigtasche)|fleisch|290 9 15 6 30 2 1.5 1.0|1.0|120|Stück=120|empanada,empanadas,empanadilla,empanadillas,empanada gallega,teigtasche mit fleisch|empanada,empanadas,meat pie turnover
huevos_rellenos|Huevos rellenos (gefüllte Eier)|ei|180 9 15 3 1 0.5 0 0.6|1.0|35|Stück=35|huevos rellenos,gefüllte eier,russische eier,deviled eggs|deviled eggs,stuffed eggs
pan_tomate|Pan con tomate (Pa amb tomàquet)|backwaren|190 5 7 1 27 2 2 1.0|0.4|60|Scheibe=60|pan con tomate,pa amb tomàquet,pa amb tomaquet,pan tumaca,brot mit tomate|pan con tomate,tomato bread
papas_arrugadas|Papas arrugadas (Runzelkartoffeln)|kartoffel|90 2 0.1 0 19 0.5 1.5 2.5|1.0|40|Stück=40|papas arrugadas,runzelkartoffeln,kanarische kartoffeln,salzkartoffeln kanarisch|canarian wrinkled potatoes,papas arrugadas
mojo|Mojo rojo / verde (kanarische Soße)|sosse|350 2 35 5 6 3 2 2.5|0.95|0|Glas=200|mojo,mojo rojo,mojo verde,mojo picón,mojo picon,mojosoße,mojo-soße|mojo,mojo sauce,mojo rojo,mojo verde
patatas_alioli|Patatas alioli|kartoffel|250 2 18 2.5 20 0.5 1.8 0.6|1.0|0|Portion=150|patatas alioli,patatas allioli,patatas ali oli,kartoffeln mit knoblauchmayonnaise|potatoes with aioli
sardellen_essig|Boquerones en vinagre (Sardellen in Essig)|fisch|150 20 7 1.5 0.5 0 0 2.5|1.0|0|Schale=150|boquerones en vinagre,boquerones al vinagre,sardellen in essig,sardinas en escabeche,atún en escabeche,escabeche|marinated anchovies,white anchovies,boquerones,escabeche
ochsenschwanz|Ochsenschwanz (mit Knochen)|fleisch|180 19 11 4.5 0 0 0 0.2|1.0|0||ochsenschwanz,ochsenschwänze,ochsenschwanzstücke,rabo de toro,rabo de buey,coda alla vaccinara|oxtail
churros|Churros (frittiert)|backwaren|400 5 22 5 45 3 1.5 0.5|0.4|25|Stück=25|churros,churro,porras,churros con chocolate|churros
flan|Flan / Crème caramel|milch|130 4 4 2 20 18 0 0.15|1.0|120|Stück=120;Becher=125|flan,flan de huevo,crème caramel,creme caramel,karamellpudding,karamellflan,flan casero|flan,creme caramel,caramel custard
crema_catalana|Crema catalana / Natillas / Crème brûlée|milch|180 4 10 6 18 17 0 0.1|1.0|125|Portion=125|crema catalana,natillas,crème brûlée,creme brulee,crema de naranja,orangencreme,katalanische creme|creme brulee,custard,natillas
leche_frita|Leche frita / Torrijas|ei|250 6 10 3 34 15 1 0.4|1.0|60|Stück=60|leche frita,torrijas,torrija,frittierter pudding,arme ritter|french toast,torrijas,fried milk
arroz_con_leche|Arroz con leche / Milchreis (fertig)|milch|130 3.5 3 2 22 12 0.2 0.1|1.0|0|Becher=200|arroz con leche,riz au lait|rice pudding,arroz con leche
turron|Turrón (Mandelnougat)|ei|480 9 25 2 55 45 4 0.1|1.0|0|Tafel=200|turrón,turron,turrón de jijona,turrón de alicante,turrón blando,turrón duro,mandelnougat,weißer nougat|turron,almond nougat,spanish nougat
fisch_mager|Fischfilet (mager)|fisch|80 18 0.8 0.2 0 0 0 0.2|1.0|150||fisch,fischfilet,fischstäbchen|fish,fish fillets,white fish,orange roughy
stockfisch|Stockfisch / Klippfisch / Bacalhau (getrocknet, gesalzen)|fisch|290 63 2.4 0.5 0 0 0 17.6|1.0|0||stockfisch,klippfisch,bacalhau,bacalao,baccalà,baccala,bacalà,salzfisch,salzkabeljau,salzdorsch,trockenfisch|salt cod,salted cod,dried cod,dried salt cod,stockfish,klipfish,bacalao,bacalhau,baccala
# --- Fisch: Meer, mager (Dorschartige, Plattfische, Barsche) ---
kabeljau|Kabeljau / Dorsch / Skrei|fisch|77 17.7 0.6 0.1 0 0 0 0.2|1.0|150||kabeljau,kabeljaufilet,kabeljaufilets,kabeljaurücken,kabeljaurückenfilet,kabeljauloins,kabeljau-loins,kabeljaukotelett,dorsch,dorschfilet,dorschfilets,dorschrücken,skrei,skreifilet,winterkabeljau,ostseedorsch,leng,lengfisch,lumb,kugelfisch,fugu|cod,cod fillet,cod fillets,atlantic cod,pacific cod,cod loin,skrei,ling,cusk,pufferfish,fugu
seelachs|Seelachs (Köhler) / Alaska-Seelachs|fisch|80 18 0.8 0.2 0 0 0 0.3|1.0|150||seelachs,seelachsfilet,seelachsfilets,köhler,koehler,köhlerfilet,alaska-seelachs,alaska seelachs,alaskaseelachs,alaska-seelachsfilet,alaska-pollack,pollack,pollackfilet,steinköhler|pollock,pollock fillet,pollock fillets,alaska pollock,saithe,coley,pollack
schellfisch|Schellfisch / Wittling / Hoki|fisch|78 17.9 0.6 0.1 0 0 0 0.3|1.0|150||schellfisch,schellfischfilet,wittling,wittlingsfilet,merlan,merlanfilet,merlan-filets,hoki,hokifilet,tk-hokifilet,blauer wittling|haddock,haddock fillet,whiting,hoki,blue whiting
seehecht|Seehecht (Merluza)|fisch|86 17.2 2 0.4 0 0 0 0.2|1.0|150||seehecht,seehechtfilet,seehechtfilets,merluza,merluzza,nasello,hecht vom meer|hake,hake fillet,merluza
rotbarsch|Rotbarsch|fisch|105 18.2 3.6 0.7 0 0 0 0.2|1.0|150||rotbarsch,rotbarschfilet,rotbarschfilets,goldbarsch,drachenkopf,roter drachenkopf,rascasse|redfish,ocean perch,rockfish,rose fish,scorpionfish
seeteufel|Seeteufel (Lotte)|fisch|76 15 1.5 0.3 0 0 0 0.1|1.0|150||seeteufel,seeteufelfilet,seeteufelfilets,seeteufelmedaillons,lotte,anglerfisch,seeteufelbäckchen|monkfish,monkfish fillet,anglerfish,lotte
scholle|Scholle / Flunder / Kliesche|fisch|86 17 1.9 0.4 0 0 0 0.3|1.0|150||scholle,schollen,schollenfilet,schollenfilets,flunder,flundern,flundernfilets,kliesche,klieschen,limande,rotzunge,doggerscharbe,butt,ostseebutt|plaice,flounder,dab,lemon sole,witch flounder,flatfish
seezunge|Seezunge|fisch|83 17.5 1.4 0.3 0 0 0 0.3|1.0|150||seezunge,seezungen,seezungenfilet,seezungenfilets,seezungenfillet,seezungenröllchen|sole,dover sole,sole fillet,common sole
steinbutt|Steinbutt / Glattbutt|fisch|95 16.7 3 0.8 0 0 0 0.3|1.0|150||steinbutt,steinbuttfilet,glattbutt|turbot,brill
heilbutt|Heilbutt (weißer)|fisch|98 20 2.3 0.4 0 0 0 0.2|1.0|150||heilbutt,heilbuttfilet,weißer heilbutt,weisser heilbutt|halibut,halibut fillet,atlantic halibut,pacific halibut
heilbutt_schwarz|Schwarzer Heilbutt|fisch|145 14 10 2 0 0 0 0.2|1.0|150||schwarzer heilbutt,grönland-heilbutt,grönlandheilbutt,heilbutt geräuchert,räucherheilbutt|greenland halibut,greenland turbot
dorade|Dorade / Goldbrasse / Meerbrasse|fisch|120 20 4.5 1.1 0 0 0 0.2|1.0|350||dorade,doraden,dorade royal,dorade rosé,goldbrasse,goldbrassen,meerbrasse,meerbrassen,brasse,streifenbrasse,zahnbrasse,zahnbrassen,rotbrasse,orata,sparus|sea bream,gilt-head bream,gilthead bream,bream,porgy,dorada
wolfsbarsch|Wolfsbarsch / Loup de mer|fisch|115 19 4.3 1 0 0 0 0.2|1.0|350||wolfsbarsch,wolfsbarschfilet,loup de mer,seebarsch,seebarschfilets,branzino,lubina,robalo|sea bass,european sea bass,branzino,seabass,striped bass
snapper|Red Snapper / Zackenbarsch|fisch|98 20 1.3 0.3 0 0 0 0.2|1.0|150||red snapper,roter schnapper,schnapper,snapper,zackenbarsch,brauner zackenbarsch,zackenbarschfilet,barramundi,riesenbarsch,adlerfisch,umberfisch|red snapper,snapper,grouper,barramundi,meagre,croaker,drum
meerbarbe|Rotbarbe / Meerbarbe|fisch|115 19 4.2 1 0 0 0 0.2|1.0|80||rotbarbe,rotbarben,meerbarbe,meerbarben,seebarbe,seebarben,streifenbarbe,triglie,rouget|red mullet,surmullet,goatfish
edelfisch_mager|Petersfisch / Knurrhahn / Steinbeißer|fisch|88 17.5 1.8 0.3 0 0 0 0.2|1.0|150||petersfisch,heringskönig,st. petersfisch,knurrhahn,roter knurrhahn,steinbeißer,steinbeisser,steinbeißerfilet,seewolf,katfisch,goldmakrele,mahi mahi,mahi-mahi,dorado,pomfret,hornhecht,meeräsche,meeraesche,barrakuda,barracuda,blaufisch,blaubarsch|john dory,gurnard,wolffish,mahi mahi,dolphinfish,pomfret,garfish,grey mullet,mullet,barracuda,bluefish
rochen|Rochen|fisch|90 20 0.7 0.1 0 0 0 0.3|1.0|200||rochen,rochenflügel,rochenflossen,gehäuteter rochen,nagelrochen|skate,skate wing,ray
schwertfisch|Schwertfisch|fisch|120 20 4 1.1 0 0 0 0.2|1.0|180||schwertfisch,schwertfischsteak,schwertfischsteaks,schwertfischfilet,gotteslachs,opah,marlin,speerfisch|swordfish,swordfish steak,opah,marlin
thunfisch_frisch|Thunfisch (frisch)|fisch|120 24 2.5 0.7 0 0 0 0.1|1.0|180||thunfischsteak,thunfischsteaks,thunfischfilet,gelbflossenthun,gelbflossen-thunfisch,yellowfin,bonito,echter bonito,weißer thun,roter thun,blauflossenthun,albacore,thunfischtatar,tunfischtatar,sashimi-thunfisch,ahi|tuna steak,tuna steaks,ahi tuna,yellowfin tuna,bluefin tuna,sushi-grade tuna,bonito
thunfisch_oel|Thunfisch in Öl (Dose)|fisch|200 26 10 1.7 0 0 0 0.9|1.0|0|Dose=150|thunfisch in öl,thunfisch in olivenöl,thunfisch in pflanzenöl,thunfischfilets in olivenöl|tuna in oil,oil-packed tuna,tuna in olive oil
haifisch|Dornhai / Schillerlocken|fisch|300 21 24 5 0 0 0 1.8|1.0|0||schillerlocke,schillerlocken,dornhai,haifisch,haifischsteak,seeaal,kalbsfisch|dogfish,spiny dogfish,shark,shark steak
# --- Fisch: Meer, fett ---
makrele|Makrele|fisch|180 18.7 11.9 3 0 0 0 0.2|1.0|300||makrele,makrelen,makrelenfilet,makrelenfilets,frische makrele,stöcker,holzmakrele,spanische makrele,königsmakrele|mackerel,atlantic mackerel,mackerel fillet,spanish mackerel,king mackerel,horse mackerel
makrele_geraeuchert|Makrele (geräuchert)|fisch|230 21 15.5 3.5 0 0 0 2.5|1.0|0||räuchermakrele,räuchermakrelen,pfeffermakrele|smoked mackerel,peppered mackerel
hering|Hering (frisch)|fisch|160 18 9.3 2.2 0 0 0 0.3|1.0|200||hering,heringe,heringsfilet,heringsfilets,grüner hering,grüne heringe,brathering,bratheringe,ostseehering,strömling,strömlinge,maifisch,alse|herring,herring fillet,fresh herring,shad
hering_mariniert|Matjes / Bismarckhering / Rollmops|fisch|230 16 17 4 2 1.5 0 3.5|1.0|0|Stück=60|matjes,matjesfilet,matjesfilets,matjeshering,bismarckhering,bismarckheringe,rollmops,rollmöpse,salzhering,salzheringe,brathering in marinade,heringshappen,heringssalat,hering in sahnesoße|pickled herring,matjes,soused herring,rollmop,rollmops,salted herring
bueckling|Bückling / Kieler Sprotten (geräuchert)|fisch|230 20 16 3.8 0 0 0 2.5|1.0|0||bückling,bueckling,bücklinge,räucherhering,kieler sprotten,sprotten,sprotte,räuchersprotten|kipper,kippers,smoked herring,smoked sprats,sprats,brisling
sardinen|Sardinen (frisch)|fisch|135 19.4 5.6 1.5 0 0 0 0.3|1.0|50||sardine,sardinen,sardinenfilets,pilchard,pilchards,anchoa fresca|sardine,sardines,pilchard
sardinen_oel|Sardinen in Öl (Dose)|fisch|210 24 12 2.5 0 0 0 0.8|1.0|0|Dose=125|sardinen in öl,ölsardinen,sardinen in olivenöl,dosensardinen|sardines in oil
sardellen|Sardellen / Anchovis (gesalzen, in Öl)|fisch|210 29 10 2.2 0 0 0 9|1.0|4|Glas=50|sardellen,sardellenfilets,sardellenfilet,anchovis,anchovisfilets,anchovy,acciughe,alici|anchovies,anchovy,anchovy fillets,anchovy fillet,salted anchovies
aal|Aal|fisch|280 15 24 5 0 0 0 0.2|1.0|0||aal,flussaal,aalstücke,meeraal,conger,grüner aal|eel,conger eel,unagi
aal_geraeuchert|Räucheraal|fisch|330 18 28.6 6 0 0 0 1.5|1.0|0||räucheraal|smoked eel
# --- Lachse, Forellen, Süßwasser ---
wildlachs|Wildlachs (Pazifik)|fisch|140 20.5 6 1.3 0 0 0 0.1|1.0|125||wildlachs,wildlachsfilet,ketalachs,keta-lachs,sockeye,sockeye-lachs,rotlachs,buckellachs,silberlachs,coho,königslachs,pazifischer lachs,pazifiklachs,alaska-wildlachs,alaska-lachs|wild salmon,sockeye salmon,sockeye,pink salmon,chum salmon,keta salmon,coho salmon,king salmon,chinook salmon,pacific salmon
forelle|Forelle / Saibling|fisch|103 19.5 2.7 0.6 0 0 0 0.1|1.0|250||forelle,forellen,forellenfilet,forellenfilets,regenbogenforelle,bachforelle,seeforelle,lachsforelle,lachsforellenfilet,saibling,saiblinge,saiblingsfilet,saiblingfilet,seesaibling,bachsaibling,elsässer saibling,äsche,äschen,aeschen,aschenfilets,huchen,ayu|trout,rainbow trout,brook trout,trout fillet,steelhead,arctic char,char,grayling
forelle_geraeuchert|Räucherforelle / Räucherfisch|fisch|120 22 3.5 0.8 0 0 0 2.5|1.0|0||räucherforelle,räucherforellen,räucherforellenfilet,räucherforellenfilets,räucherfisch,räucherfischfilet,räuchersaibling|smoked trout,smoked fish,smoked whitefish
felchen|Felchen / Renke / Maräne|fisch|100 18 3 0.6 0 0 0 0.1|1.0|200||felchen,felchenfilet,felchenfilets,renke,renken,renkenfilet,maräne,maränen,maraene,große maräne,kleine maräne,coregonus,blaufelchen,balchen,reinanke,bodenseefelchen,weißfisch vom see|whitefish,lake whitefish,vendace,cisco
zander|Zander / Hecht / Flussbarsch|fisch|84 19 0.8 0.2 0 0 0 0.1|1.0|150||zander,zanderfilet,zanderfilets,hecht,hechtfilet,hechte,hechtklößchen,flussbarsch,flussbarschfilet,barsch,barschfilet,barschfilets,egli,eglifilet,eglifilets,kretzer,kretzerfilet,schwarzbarsch,amurbarsch,sander|pike-perch,zander,walleye,pike,northern pike,perch,yellow perch,european perch,pickerel
karpfen|Karpfen|fisch|115 18 4.8 1 0 0 0 0.1|1.0|0||karpfen,spiegelkarpfen,schuppenkarpfen,karpfenfilet,silvesterkarpfen,karpfen blau,graskarpfen,amur,amurkarpfen,silberkarpfen,marmorkarpfen|carp,common carp,grass carp,silver carp
weissfisch|Weißfisch (Schleie, Brachse, Rotauge)|fisch|95 17.7 2.6 0.5 0 0 0 0.1|1.0|200||schleie,schleien,brachse,brachsen,brachsenfilet,blei,rotauge,rotaugen,plötze,plötzen,döbel,barbe,nase,aland,rotfeder,güster|tench,roach,chub,barbel,ide
wels|Wels / Waller|fisch|160 15 11 2.5 0 0 0 0.1|1.0|150||wels,welse,welsfilet,welsfilets,waller,wallerfilet,flusswels|wels catfish,sheatfish,european catfish
pangasius|Pangasius / Tilapia / Katzenwels|fisch|90 17 2.3 0.7 0 0 0 0.2|1.0|150||pangasius,pangasiusfilet,pangasiusfilets,tilapia,tilapiafilet,tilapiafilets,buntbarsch,viktoriabarsch,victoriabarsch,nilbarsch,katzenwels,afrikanischer wels,claridae|pangasius,basa,swai,tilapia,nile perch,catfish,channel catfish,catfish fillets
stoer|Stör|fisch|105 18 3.8 0.9 0 0 0 0.15|1.0|150||stör,störe,störfilet,störfilets,störsteak,stoer,stoerfilet,sterlet,hausen,beluga-stör,sibirischer stör,räucherstör,geräucherter stör|sturgeon,sturgeon fillet,smoked sturgeon,sterlet
# --- Tintenfische, Krebstiere ---
tintenfisch|Tintenfisch (Kalmar, Sepia)|fisch|90 15.6 1.4 0.4 3 0 0 0.1|1.0|0||tintenfisch,tintenfische,tintenfischringe,tintenfischtube,tintenfischtuben,calamari,calamaris,kalmar,kalmare,kalmarringe,sepia,sepien,sepiatinte,sepiola,pijlinktvis,chipirones,seppia,seppie,totani|squid,calamari,squid rings,cuttlefish,baby squid
oktopus|Oktopus / Krake|fisch|82 15 1 0.2 2.2 0 0 0.6|1.0|0||oktopus,oktopusse,krake,kraken,pulpo,polpo,polipo,octopus,pulpo a la gallega|octopus,baby octopus
hummer|Hummer / Languste / Flusskrebs / Krabbe|fisch|90 19 1.5 0.3 0.5 0 0 0.8|1.0|0||hummer,hummerschwänze,hummerfleisch,hummerschwanz,languste,langusten,langustenschwänze,flusskrebse,flusskrebs,flusskrebsfleisch,flusskrebsschwänze,krebse,krebsfleisch,taschenkrebs,königskrabbe,königskrabbenbeine,schneekrabbe,krabbenbeine,seespinne,bärenkrebs|lobster,lobster tail,lobster tails,lobster meat,crawfish,crayfish,crab,crabmeat,crab meat,king crab,snow crab,dungeness crab,blue crab,crab legs,lump crab meat
surimi|Surimi (Krebsfleischimitat)|fisch|100 7.5 0.5 0.1 15 5 0.5 1.8|1.0|0||surimi,surimistäbchen,krebsfleischimitat,crab sticks,kanikama|imitation crab,surimi,crab sticks,krab
# --- Muscheln ---
miesmuscheln|Miesmuscheln (mit Schale gewogen)|fisch|26 3.6 0.7 0.1 1.1 0 0 0.2|1.0|0||miesmuscheln,miesmuschel,pfahlmuscheln,pfahlmuschel,moules,cozze,mejillones,bouchot-muscheln,muscheln,muschel,frische muscheln,blaue muscheln,mittelmeer-miesmuscheln|mussels,blue mussels,fresh mussels,mussel
muschelfleisch|Muschelfleisch (ausgelöst)|fisch|86 11.9 2.2 0.4 3.7 0 0 0.7|1.0|0||muschelfleisch,miesmuschelfleisch,tk-muschelfleisch|mussel meat,clam meat
venusmuscheln|Venus-/Herzmuscheln, Meermandeln (mit Schale)|fisch|16 2.6 0.2 0 0.5 0 0 0.3|1.0|0||venusmuscheln,venusmuschel,vongole,vongole veraci,teppichmuscheln,teppichmuschel,herzmuscheln,herzmuschel,meermandeln,meermandel,samtmuscheln,samtmuschel,englisches pastetchen,sandklaffmuscheln,schwertmuscheln,scheidenmuscheln,messermuscheln,almejas,palourdes,clams|clams,littleneck clams,manila clams,cockles,razor clams,dog cockles,carpet shell clams
austern|Austern|fisch|75 9.5 2.3 0.5 4.5 0 0 0.5|1.0|15|Dutzend=180|auster,austern,felsenaustern,pazifische austern,belon-austern,fines de claire,austernfleisch|oyster,oysters,pacific oysters,raw oysters,shucked oysters
jakobsmuscheln|Jakobsmuscheln (ausgelöst)|fisch|69 12.1 0.5 0.1 3.2 0 0 1|1.0|25||jakobsmuschel,jakobsmuscheln,jakobsmuschelfleisch,jakobsmuschelnüsse,jakobsmuschelnüsschen,muschelnüsse,coquilles st jacques,coquille saint-jacques,kammmuscheln,pilgermuscheln,capesante|scallop,scallops,sea scallops,bay scallops
gruenlippmuscheln|Grünlippmuscheln (halbe Schale)|fisch|34 5.2 0.9 0.2 1.2 0 0 0.3|1.0|30||grünlippmuscheln,grünlippmuschel,grünschalmuscheln,grünschalmuschel,neuseeland-muscheln,neuseeländische grünlippmuscheln,grünlipp-muscheln|green-lipped mussels,green lipped mussels,new zealand mussels,greenshell mussels
entenmuscheln|Entenmuscheln / Percebes (ganz)|fisch|25 5.3 0.4 0.1 0.4 0 0 0.4|1.0|0||entenmuscheln,entenmuschel,percebes,percebe,felsen-entenmuscheln,pouce-pied|goose barnacles,percebes,gooseneck barnacles
lachs|Lachs|fisch|200 20 13.5 3 0 0 0 0.1|1.0|125||lachs,lachsfilet,räucherlachs,graved lachs,stremellachs|salmon,salmon fillet,smoked salmon,lox
thunfisch|Thunfisch (Dose, im eigenen Saft)|fisch|110 25 1 0.3 0 0 0 0.8|1.0|0|Dose=150|thunfisch,thunfisch naturell,thunfischdose|tuna,canned tuna,tuna fish,chunk light tuna,albacore tuna,water-packed tuna
garnelen|Garnelen|fisch|85 18.5 1 0.3 0 0 0 0.6|1.0|10|Packung=200|garnelen,shrimps,krabben,crevetten,riesengarnelen,king prawns,scampi,meeresfrüchte,kaisergranat,langostinos,gambas,garnelenschwänze,nordseekrabben,krabbenfleisch|shrimp,prawns,jumbo shrimp,seafood
bruehe|Brühe (zubereitet)|sosse|5 0.5 0.2 0.1 0.4 0.2 0 0.8|1.0|0|Würfel=10|brühe,gemüsebrühe,fleischbrühe,hühnerbrühe,rinderbrühe,instant-brühe,fond,kalbsfond,wildfond,fischfond,geflügelfond,bouillon,spargelsud,scampisud,wildglace,sud|broth,stock,chicken broth,beef broth,vegetable broth,chicken stock,beef stock,vegetable stock,bouillon,consomme,fish stock
bruehpulver|Brühpulver / Brühwürfel|gewuerz|200 10 12 6 15 5 0 50|0.7|10|Würfel=10;TL=4;EL=12|brühwürfel,brühpulver,gemüsebrühpulver,instant gemüsebrühe,hühnerbrühwürfel,suppenwürfel,gekörnte brühe,instantbrühe|bouillon cube,bouillon cubes,bouillon granules,chicken bouillon granules,beef bouillon cube,stock cube,chicken bouillon,beef bouillon
salz|Salz|gewuerz|0 0 0 0 0 0 0 100|1.2|0||salz,meersalz,jodsalz,kochsalz,steinsalz,fleur de sel,salzwasser,kräutersalz|salt,kosher salt,sea salt,table salt,coarse salt,seasoned salt,garlic salt,onion salt,celery salt
pfeffer|Pfeffer|gewuerz|250 10 3 1 40 0.6 25 0|0.5|0||pfeffer,schwarzer pfeffer,weißer pfeffer,pfeffer aus der mühle,cayennepfeffer,cayenne,pfefferkörner,bunter pfeffer,chilipulver,chiliflocken,peffer|pepper,black pepper,ground pepper,white pepper,cayenne,cayenne pepper,red pepper flakes,crushed red pepper,peppercorns,chili powder,ground black pepper,freshly ground pepper
gewuerze|Gewürze (gemahlen)|gewuerz|300 12 10 1.5 35 3 30 0.1|0.5|0.5||paprikapulver,edelsüß,rosenscharf,geräuchertes paprikapulver,curry,currypulver,kreuzkümmel,cumin,kümmel,zimt,zimtpulver,muskat,muskatnuss,nelken,gewürznelken,kardamom,koriander,koriandersamen,ingwerpulver,kurkuma,gelbwurz,anis,sternanis,lorbeer,lorbeerblatt,lorbeerblätter,wacholderbeeren,senfsaat,senfkörner,lebkuchengewürz,spekulatiusgewürz,garam masala,ras el hanout,gewürzmischung,zimtstange,zimtstangen,safran,bockshornklee,knoblauchpulver,zwiebelpulver,fünf-gewürze-pulver,italienische kräuter,kräuter der provence,majoran,getrockneter majoran,nelkenpulver,piment,pimentkörner,senfpulver,korianderkörner,senfsaat,senfkörner,kardamon,muskatblüte,macis,gulaschgewürz,knoblauchgranulat|smoked paprika,curry powder,cumin,ground cumin,caraway,caraway seeds,cinnamon,ground cinnamon,nutmeg,ground nutmeg,cloves,ground cloves,cardamom,coriander,ground coriander,ground ginger,turmeric,anise,star anise,bay leaf,bay leaves,juniper berries,mustard seed,mustard seeds,pumpkin pie spice,garam masala,italian seasoning,garlic powder,onion powder,dried oregano,dried thyme,dried basil,marjoram,dried marjoram,sage,dried sage,dried rosemary,poultry seasoning,herbes de provence,cinnamon stick,cinnamon sticks,mace,saffron,seasoning,dried parsley,dill weed,celery seed,dry mustard,mustard powder,taco seasoning,cajun seasoning,old bay,allspice,mustard powder,dry mustard
wuerzpaste|Würzpaste (Curry, Sambal, Tandoori, Tamarinde)|gewuerz|120 2 6 0.5 12 6 3 6|1.1|0||currypaste,rote currypaste,grüne currypaste,gelbe currypaste,sambal oelek,sambal olek,tandoori-paste,tandooripaste,harissa,chilipaste,misopaste,miso|curry paste,red curry paste,green curry paste,sambal oelek,harissa,tamarind paste,miso
ingwer|Ingwer (frisch)|gewuerz|80 1.8 0.8 0.2 16 1.7 2 0|0.6|20||ingwer,frischer ingwer,ingwerwurzel,galgant,zitronengras|ginger,fresh ginger,ginger root,gingerroot,grated ginger,lemongrass
kraeuter|Frische Kräuter|kraeuter|40 3 0.6 0.1 4 0.9 4 0|0.2|0|Bund=25;Zweig=1;Handvoll=10;Topf=25|petersilie,glatte petersilie,krause petersilie,schnittlauch,dill,basilikum,schnittlauchröllchen,thymianblättchen,thymianzweige,basilikumblättchen,basilikumblätter,kerbelblättchen,korianderblättchen,minzblättchen,minzeblättchen,petersilienblättchen,rosmarinnadeln,dillspitzen,dillzweig,dillzweige,minzblätter,minzeblätter,essbare blüten,ungespritzte blüten,holunderblüten,holunderblütendolden,koriandergrün,minze,pfefferminze,thymian,rosmarin,salbei,oregano,estragon,kerbel,liebstöckel,bärlauch,zitronenmelisse,kresse,kräuter,frische kräuter,gemischte kräuter,tk-kräuter,thymianzweig,rosmarinzweig,salbeiblätter,basilikumblätter,borretsch,sauerampfer,zitronenmelisse,pimpinelle,rosamarin|parsley,fresh parsley,chopped parsley,flat-leaf parsley,chives,fresh chives,dill,fresh dill,basil,fresh basil,basil leaves,cilantro,fresh cilantro,mint,fresh mint,mint leaves,thyme,fresh thyme,rosemary,fresh rosemary,sage leaves,oregano,fresh oregano,tarragon,herbs,fresh herbs,watercress,sorrel,borage
senf|Senf|sosse|90 6 5 0.3 5 3 2 4|1.1|0||senf,mittelscharfer senf,scharfer senf,dijonsenf,dijon-senf,süßer senf,körniger senf,estragonsenf|mustard,dijon mustard,prepared mustard,yellow mustard,spicy brown mustard,whole grain mustard,dijon
ketchup|Ketchup|sosse|110 1.3 0.2 0 25 22 0.5 2.5|1.15|0||ketchup,tomatenketchup,currysoße,curryketchup,chilisoße,chilisauce,süß-sauer-soße,bbq-soße,barbecuesauce,grillsoße|ketchup,catsup,barbecue sauce,bbq sauce,chili sauce,sweet and sour sauce,cocktail sauce
mayonnaise|Mayonnaise|sosse|700 1.2 77 6 1.5 1.3 0 1.2|0.95|0||mayonnaise,mayo,salatmayonnaise,remoulade,aioli|mayonnaise,mayo,miracle whip,salad dressing,tartar sauce,aioli
salatcreme|Salatcreme / Dressing|sosse|300 1 28 2.5 12 9 0 2|1.0|0||salatcreme,dressing,salatdressing,joghurtdressing,french dressing,caesar dressing|ranch dressing,italian dressing,french dressing,caesar dressing,vinaigrette,thousand island dressing,blue cheese dressing
sojasosse|Sojasoße|sosse|60 8 0.1 0 6 2 0.8 15|1.15|0||sojasoße,sojasauce,soja-soße,teriyakisoße,teriyaki,fischsoße,fischsauce,worcestersoße,worcestershiresauce,maggi,tabasco,sriracha,tamari,teriyakisauce,worcestersauce,maggi-würze|soy sauce,low-sodium soy sauce,light soy sauce,dark soy sauce,tamari,teriyaki sauce,fish sauce,worcestershire sauce,worcestershire,hot sauce,tabasco,tabasco sauce,sriracha,liquid smoke
hoisinsauce|Hoisinsauce|sosse|220 3.3 3.4 0.6 44 27 2.8 4.4|1.2|0||hoisinsauce,hoisin-sauce,hoisinsoße,pflaumensauce,austernsauce,austernsoße|hoisin sauce,plum sauce,oyster sauce
essig|Essig|sosse|20 0 0 0 0.6 0.4 0 0|1.01|0||essig,weinessig,weißweinessig,rotweinessig,apfelessig,kräuteressig,balsamico,balsamessig,aceto balsamico,reisessig,branntweinessig,obstessig,sherryessig|vinegar,white vinegar,cider vinegar,apple cider vinegar,red wine vinegar,white wine vinegar,balsamic vinegar,rice vinegar,wine vinegar
pesto|Pesto|sosse|450 5 45 7 5 3 2 2.5|1.0|0|Glas=190|pesto,pesto genovese,pesto rosso,basilikumpesto,bärlauchpaste,basilikumpaste|pesto,basil pesto
tomatensosse|Tomatensoße (fertig)|sosse|50 1.5 1.5 0.2 7 5 1.5 1|1.03|0|Glas=400|tomatensoße,tomatensauce,pastasoße,pizzasoße,nudelsoße,salsa,arrabiata|marinara sauce,spaghetti sauce,pasta sauce,pizza sauce,salsa,enchilada sauce
sahnesosse|Soße (Fertigprodukt, hell)|sosse|80 1.5 5 3 7 2 0.3 1.2|1.03|0|Pck.=250|bratensoße,soßenbinder,hollandaise,sauce hollandaise,béchamel,bechamelsoße,helle soße,dunkle soße,rahmsoße,jägersoße,soßenpulver,bratensaft|gravy,cream of mushroom soup,cream of chicken soup,cream of celery soup,condensed cream of mushroom soup,condensed soup,hollandaise sauce,white sauce,alfredo sauce,brown gravy
puddingpulver|Puddingpulver|suess|350 0.5 0.1 0 86 0 1 0.5|0.6|0|Pck.=37|puddingpulver,vanillepuddingpulver,schokoladenpuddingpulver,tortenguss,soßenpulver vanille,götterspeise,vanillepudding,schokoladenpudding|pudding mix,instant pudding,vanilla pudding mix,instant vanilla pudding,instant chocolate pudding,gelatin dessert,jell-o,jello
speiseeis|Speiseeis (Vanilleeis)|suess|200 3.5 11 7 23 21 0 0.2|0.55|0|Kugel=50|speiseeis,vanilleeis,eiscreme,sahneeis,schokoladeneis,erdbeereis,milchspeiseeis,softeis,zitroneneis,pecannusseis,walnusseis,sorbet|ice cream,vanilla ice cream,chocolate ice cream
sahnesteif|Sahnesteif|sonstiges|370 0 0 0 92 30 0 0|0.6|0|Pck.=8|sahnesteif,sahnefestiger|whipped cream stabilizer
kaffee|Kaffee|getraenk|2 0.2 0 0 0.3 0 0 0|1.0|0||kaffee,espresso,starker kaffee,filterkaffee,instantkaffee,löslicher kaffee,tee,schwarztee,grüner tee,kräutertee,früchtetee|coffee,brewed coffee,strong coffee,espresso,instant coffee,tea,black tea,green tea
limonade|Limonade / Cola|getraenk|42 0 0 0 10.5 10.5 0 0|1.04|0||limonade,cola,limo,zitronenlimonade,ginger ale,sprite,fanta,tonic water,eistee|soda,cola,lemonade,ginger ale,lemon-lime soda,7-up,sprite,iced tea,tonic water
wein_weiss|Weißwein|alkohol|80 0.1 0 0 2.6 1 0 0|0.99|0|Flasche=750|weißwein,trockener weißwein,wein,riesling,sekt,prosecco,champagner,noilly prat,wermut,trockener wermut,vermouth,frascati,martini dry|white wine,dry white wine,wine,champagne,sparkling wine,sherry,dry sherry,vermouth,mirin,marsala,sake,cooking wine
wein_rot|Rotwein|alkohol|85 0.1 0 0 2.6 0.6 0 0|0.99|0|Flasche=750|rotwein,trockener rotwein,portwein,madeira,glühwein|red wine,dry red wine,port,port wine,burgundy,madeira
bier|Bier|alkohol|43 0.5 0 0 3.6 0 0 0|1.01|0|Flasche=500;Dose=500|bier,helles,dunkles bier,pils,weizenbier,malzbier,starkbier,cidre,cider,apfelwein|beer,ale,lager,dark beer,stout
spirituosen|Spirituosen|alkohol|230 0 0 0 0 0 0 0|0.95|0|Flasche=700|rum,brauner rum,weinbrand,cognac,brandy,kirschwasser,obstler,schnaps,wodka,gin,whisky,calvados,grappa,ouzo,pernod,pastis,campari,cachaca,cachaça,aprikosengeist,himbeergeist,williamsbirne|rum,brandy,cognac,vodka,gin,whiskey,whisky,bourbon,tequila,kirsch,bourbon whiskey
likoer|Likör (Frucht-, Kaffee-, Eierlikör)|alkohol|300 0 0 0 35 35 0 0|1.08|0|Flasche=700|likör,crème de cassis,cassis,cassislikör,passoa,amaretto,grand marnier,cointreau,orangenlikör,eierlikör,kaffeelikör,baileys,limoncello,curaçao,blue curaçao,kirschlikör,schwarz-johannisbeer-likör|liqueur,amaretto,grand marnier,triple sec,kahlua,coffee liqueur,irish cream,limoncello,creme de cassis
ersatz_sonstiges|Kartoffelchips / Snacks|suess|540 6 34 3 50 1 4 1.5|0.1|0|Beutel=175|chips,kartoffelchips,tortillachips,salzstangen,cracker,erdnussflips,popcorn|potato chips,chips,tortilla chips,corn chips,pretzels,popcorn
# ---- ergänzt 0.9.8 (fehlende Lebensmittel aus der Datenbank des Nutzers, Schweizer und asiatische Rezepte)
bambussprossen|Bambussprossen (Dose)|gemuese|19 1.8 0.2 0 1.5 0.3 1.4 0.02|1.0|0||bambussprossen,bambussprosse,bambusschösslinge,bambus,dosen-bambus,bambusspitzen|bamboo shoots
sprossen|Sprossen (Mungbohnen-, Sojasprossen)|gemuese|30 3.0 0.2 0 4.2 4.1 1.8 0.02|0.4|0||sprossen,sojasprossen,sojabohnensprossen,sojakeime,sojabohnenkeime,sojabohnenkeimlinge,soyabohnenkeimlinge,soyabohnensprösslinge,sojabonensprossen,bohnensprossen,bohnenkeime,mungobohnensprossen,mungosprossen,mungbohnensprossen,keimlinge,alfalfasprossen,alfalfa-sprossen,linsensprossen,radieschensprossen,knoblauchsprossen,lauchsprossen|bean sprouts,mung bean sprouts,alfalfa sprouts,sprouts
glutamat|Glutamat (Mononatriumglutamat)|gewuerz|0 0 0 0 0 0 0 31|0.8|0||glutamat,natriumglutamat,mononatriumglutamat,monosodiumglutamat,monosodium glutamat,monos glutamat,glutmat,aji-nomoto,ajinomoto,aji-shio|msg,monosodium glutamate
artischocke|Artischocke|gemuese|47 3.3 0.2 0 5.1 1 5.4 0.24|0.6|250|Stück=250|artischocke,artischocken,artischockenherzen,artischockenböden,artischockenboden,carciofini,artischoken|artichoke,artichokes,artichoke hearts,artichoke bottoms
okra|Okra|gemuese|33 1.9 0.2 0 4.3 1.5 3.2 0.02|0.5|12|Stück=12|okra,okras,okraschoten,ladie's fingers,ladies fingers|okra
reispapier|Reispapier|getreide|335 3 0.3 0 80 0 1.5 0.5|0.5|8|Blatt=8;Scheibe=8|reispapier,reispapierblätter,reispapierblatt,reisblätter|rice paper,rice paper wrappers
tamarinde|Tamarindenmark|obst|239 2.8 0.6 0.3 57 57 5 0.07|1.2|0|Kugel=30|tamarinde,tamarindenmus,tamarindenmark,tamarindenkonzentrat,tamarindenpaste,tamarindenblätter|tamarind,tamarind pulp
garnelenpaste|Garnelenpaste (Trassi, Krabbenpaste)|fisch|190 30 2.5 1 10 1 0 30|1.2|0||garnelenpaste,krabbenpaste,trassi,terasi,belacan,sardellenpaste,fischpaste|shrimp paste,anchovy paste
kaviar|Fischrogen (Kaviar, Forellen-, Lachskaviar)|fisch|250 25 17 4 3 0 0 3.5|1.0|0||kaviar,forellenkaviar,lachskaviar,keta-kaviar,deutscher kaviar,rogen,fischrogen|caviar,roe,salmon roe
algen|Algen (Nori, Wakame, Kombu, getrocknet)|gemuese|190 35 1 0.2 10 1 33 3|0.3|3|Blatt=3;Scheibe=3;Stück=3|nori,noriblätter,noriblatt,nori-algen,wakame,wakamestücke,kombu,konbu,kombu-streifen,algen,seetang,haarseegras,ouessane-algen,streugewürz aus blauer alge|seaweed,nori,wakame,kombu,kelp
lebensmittelfarbe|Lebensmittelfarbe|sonstiges|0 0 0 0 0 0 0 0|1.0|0||lebensmittelfarbe,lebensmittelfarben,speisefarbe,speisefarben,rote speisefarbe,gelbe lebensmittelfarbe,blaue speisefarbe,grüne speisefarbe|food coloring,food colouring
suessstoff|Süßstoff|zucker|0 0 0 0 0 0 0 0|1.0|0||süßstoff,flüssiger süßstoff,diabetikersüße,stevia,stevia-extrakt,streusüße,sucralose,saccharin|sweetener,artificial sweetener,stevia
zitronensaeure|Zitronensäure / Vitamin-C-Pulver|sonstiges|0 0 0 0 0 0 0 0|0.9|0||zitronensäure,vitamin-c-pulver,ascorbinsäure,zitronensalzkristalle,gereinigter weinstein,weinstein,weinsteinbackpulver|citric acid,cream of tartar
trueffel|Trüffel|gemuese|50 6 0.5 0 1 0 17 0.01|0.6|20|Stück=20|trüffel,trüffeln,schwarze trüffel,schwarze trüffeln,ardeche-trüffeln,trüffelscheiben|truffle,truffles
knochen|Knochen (für Brühe, werden nicht gegessen)|sonstiges|0 0 0 0 0 0 0 0|1.0|300||knochen,suppenknochen,markknochen,kalbsknochen,rindsknochen,rinderknochen,schweineknochen,svhweineknochen,gemischte knochen,gemichte knochen,fleischknochen,rinderfleischknochen,rehknochen,wildschweinknochen,hühnerknochen,hühnerknochen und hälse,kalbsfüsse,kalbsfüße,gehackte wildschweinknochen|bones,soup bones,marrow bones
maniok|Maniok / Taro / Yams|kartoffel|160 1.4 0.3 0.1 38 1.7 1.8 0.04|0.7|0||maniok,cassava,yucca,taro,cocoyam,jicama,plantano|cassava,taro,yuca
"""

EXTRA = r"""
moehre: rüebli,rübli,bundrüebli,bund-rüebli,frühlingsrüebli,rüebliwürfel,rüebli-würfel,rüebli mittel,karottenwürfel,möhrchen,arotte
kraeuter: peterli,peterli glatt,peterli-stiele,prezzemolo,prezzomolo,petersilienzweige,petersilienzweigen,petersilienstängel,petersilienstengel,petersilienblätter,bohnenkraut,bohnenkrautzweig,beifuß,beifuss,quendel,lavendel,lavendelstengel,lavendelblüten,verveine,zitronenverbene,geißfuß,giersch,shisoblätter,shiso,portulak,boretsch,pimpernelle,melisse,melisseblättchen,pfefferminzzweige,pfefferminzstengel,minzestängel,kaffirblätter,kaffirzitronenblätter,kaffirlimettenblätter,kaffirlimonenblätter,limettenblätter,europagrasblätter,thai-basilikumblätter,thai-basilikum,basilkum,basilikumsträußchen,korianderkraut,frisches korianderkraut,korianda,dillkraut,fenchelkraut,fenchelgrün,selleriegrün,sellersieblätter,liebstöckelblätter,liebstöckelblatter,nane,orgegano,origano,rosmarinspitze,rosmarinnnadeln,rostnarinnadeln,schnittlauchhalme,bouquet garni,kräutersträusschen,kräutersträußchen,kräuterbündel,käutersträußchen,kräutermischung provençal,brennesseln,brennesselspitzen,junge brennesseln,curryblätter,rosenblüten,essbare rosenblüten
gewuerze: anissamen,anispulver,fenchelsaat,fenchelsamen,kreuzkümmelsaat,cummin samen,cummin körner,cummin,kumin,kimyon,koriandersaat,schwarzkümmelsaat,schwarzkümmel,fünfgewürzpulver,fünf-gewürz-pulver,fünf-gewürzpulver,5-gewürz-pulver,fünferlei-gewürz,fünferlei-gewürzpulver,chinagewürz,china-gewürz,asiagewürz,pfefferkuchengewürz,honigkuchengewürz,stäsz-pfefferkuchengewürz,pastetengewürz,raz-el-hanut,ras-el-hanout,sumak,sumach,berbere,kassie,zimtstengel,zimtstängel,zimtrinde,wacholderbeere,kardamomkapseln,kardamomkapsel,kardamomskapseln,grüne kardamomkapseln,macisblüte,musaktblüte,muskatblüte,curcuma,fenugreek,gyrosgewürz,geflügelgewürz,fischgewürz,bratkartoffelgewürz,fallaffelgewürz,gewürzsalzmischung,lorbeerblätt,lorbeerbaltt,lorbeerbatt,lorberblatt,lorberblätter,lorneerblätter,nelke,nelken,gewürznelke,nelkenköpfe,mußkatnuß,musskatnuss,safranfäden,briefchen safranfäden,kapsel safranfäden,döschen safranfäden,sternanisfrucht,gewürzsäcklein,gewürzsäckchen,ingwerpulver,anis,bockshornkleesamen,sesamsamen,mohnsaat,gewürze
pfeffer: chillipulver,chilli-pulver,chilli pulver,chilliflocken,pul biber,kirmizi biber,fagara,sichuanpfeffer,szechuanpfeffer,chuanjiao,blütenpfeffer,schwarze pfefferkörner,schwarze pfefferkorner,grüne pfefferkörner,pfefffer,feffer,cayennepulver
chili: chilli,chillies,chillis,chillie,chillischote,chillischoten,chilli-schote,chilli-schoten,chilichoten,rote chillies,grüne chillies,rote chilli,roten chilli,grüner chilli,trockene rote chillies,peperoncino,peperonicino,peroncino,peporoni,peproni,ancho-chillis,anaheim-chillies,ancho- oder pasilla-chillies,ajies amarillos,chipotle,pfefferschoten,grüne pefferschote
paprika: peperone,paprikaschotte,papkrikaschoten
weisskohl: wirz,wirzblätter,wirsingkopf,weißkohlkopf,weißkohlköpfe,weißkrautkopf,weisskabis,kabis,rotkabis,kohlkopf,kohlblätter
salat: lattich,lattichblätter,lattich-blätter,salatherz,salatkopf,kopfsalatherz,endivienkopf,rauke,feinblättrige rauke,ruccola,ruccolablätter,lollo bianco,chicoreestaude,chicoreestange,radicchioköpfe
erbsen: kefen,kefe,erbsenschoten,zuckerschoten
rote_bete: randen
zucchini: zucchetti,zuccini,zuchini,patty pans,zucchiniblüten
champignons: champignon,champignonköpfe,eierschwämme,eierschwämmchen,eierschwämmli,steinpilz,parasolpilz,parasolpilz-hüte,trockenpilz,trockenpilze,getrocknete chin pilz,getrocknete chinesische pilze,tonggu,tongu,dhong gü,mu-err,mu err,judasohren,wolkenohren,wolkenohrpilz,baumohren,strohpilze,goldnadeln,lilienblüten
gefluegel: pouletbrust,pouletbrüstchen,pouletbrüste,hühnerbrüstchen,hühnerbrüste,hühnebrüstchen,hähnchenbrüstchen,hähnchnbrust,hänchenbrustfilet,hühnchenbrustfilet,hühnerfilet,hühnerfleisch,gekochtes hühnerfleisch,ausgelöstes hühnerfleisch,perlhuhnbrust,perlhuhnbrüste,perlhuhnbrüstchen,maispoulardenbrust,maispoulardenbrüste,maishuhnbrüste,poulardenbrust,poulardenbrüstchen,truthahnbrust,truthahn brustfilets,hendlbrüste,geflügelklein,gänseklein,hühnerklein,geflügelmagen,hühnerpörkölt
haehnchenschenkel: hähnchenschlegel,hähnchenoberschenkel,hühnerflügel,putenkeulen,putenoberkeulen,hendlkeulen,oberschenkel,hahn,freilandhahn,kapaun,perlhuhn,perlhühner,coquelets,backhendl,hächnchen,truthahn,pute,puter,wachteln,tauben
ente: entenfüße
rindfleisch: rindertatar,tatar beefsteak,beefsteak tatar,rindsfilet,rindsfilet vom spitz,rinderlende,rindsragout,rinderkeule,rindernuss,rinderschulter,rinderquerrippe,rinderbacken,rinds-entrecôte,tournedos,hohrücken,hohrückenbraten,hüfte,rindshuft,rinderschmorbraten,rinderroulade,rinderfiletsteaks,filetsteaks,rostbraten,rindbraten,rind,klärfleisch,rindszunge,rinderkutteln,kutteln,siedfleisch,pferdefleisch,pferdebraten,pferdebratenfleisch,eselsfleisch,tatar
kalbfleisch: kalbsragout,kalbshuft,kalbsplätzchen,kalbsplätzli,kalbsmedaillons,kalbschulter,kalbschulterspitz,kalbsschulterspitz,kalbsbagge,kalbsbäggli,kalbscarrébraten,kalbskopf,kalbsherz,kalbsfleisch für ragout,kalbsmagen
schweinefleisch: schweinsnierstück,schweins-nierstück,schweinsfilet,schweinsfilet-medaillon,schweinshuft,schweinekamm,schweinehals,schweinehalsgrat,schweinsvoressen,schweinskoteletten,schweinskotelett,schweinerippchen,schweinsrippchen,schälrippchen,rippchen,schweinebratenaufschnitt,schweineschmorbraten,schweinenuss,schweinekeule,schweinshaxe,schweinshaxen,eisbein,eisbeine,gepökeltes eisbein,schweinebacke,rippli,kotellets,plätzli,jungschweinernes,schweinsfüße,schweinefüße,schweineherz,schweineschwanz,schweineschwänzchen,nierenbraten,schweinemagen,bratenreste,panierte schnitzelchen
lamm: chops,lammchops,lammhüfte,lammhuftsteak,lammfilets,lammhuft,lammnierstück,lammnierstücke,lammvoressen,lammragout,lammrippen,lammrippenstück,lammkoteletten,milchlammschultern,lammnieren,lammsnieren,lammhirne,lammsmilken
wild: frischlingsfleisch,frischlingsrückenfilet,wildschweinrollbraten,wildschweinfleisch,wildscheinfleisch,wildschwein,rehnuss
kaninchen: kanninchen
wurst: debreziner,debreziner würste,cotechino,kochwürste,schweinswürste,regensburger,regenshurger würste,schinkenmettwürste,salame cotto,casbär-kolbasz,biroldo,grobes bratwurstbrät,kalbsbrät,grobes kalbsbrät,schweinsbrät,wurstbrät,nürnberger bratwürstl,andouille
rohschinken: bündner fleisch,bresaola,serrano,trockenfleisch,rinderrauchfleisch,dörrfleisch,pastirma,lardo
schinken: räucherzunge,kasslerscheiben,kassler,geräuchertes fleisch,geräuchertes rohes fleisch,geräucherte schweinerippe,schinkentranchen
speck: speckwürfeli,bratspeckscheiben,specktranchen,bauernspecktranchen,bratspecktranchen,frühstücksseck,speckschwarte,schweineschwarte,geräucherte schweinebacke,schweinenetz
hackfleisch: durchgedrehtes fleisch
fisch_mager: weißlinge,festfleischige meeresfische,küchenfertige meeresfische,meeresfische,fischabschnitte,poissons de roche,fischreste,karbonadenfisch
lachs: lachsseite,lachsschnitten,lachstranchen,lachsseite ohne haut,lachstatar
stockfisch: bonitoflocken,getrocknete bonitoflocken,kazuobushi
garnelen: krevetten,krevettenschwänze,graue krevetten,tiefgefrorene krevetten,crevette,krabbe,langustinen,schnecken,dutzend schnecken,frutti di mare,seegurke,seegurken
sahne: schlagobers,schlagshne,qimiq
saure_sahne: crème légère,creme legere,crème balance,creme balance,crème frâche,créme fraœche,crème fraœche
joghurt: naturjogurt
mozzarella: mozarella,mozarelline,mozarella-perlen,mini-burrata
gouda: pressato,couda,ceddar,käsereste
hartkaese: 
parmesan: parmäsan,junger peccorino,peccorino
camembert: 
quark: ankenziger,ziger
frischkaese: frischkäsezubereitung
quinoa: dinkel,hafer,gerste,perlgraupen,rollgerste,graupen,hartweizen,weizen,kristallweizen körner
haferflocken: köllnflocken,kölln,kölnflocken,köln instant flocken,kölns echte kernige,blütenzarte köllnflocken,haferfleks,knuprige haferfleks,weizenkeime,vollkornschrot
polenta: bramata,maisgries,maisgrieß
griess: gries
speisestaerke: stärkepuder,sago,tapiokaperlen,tapioka
nudeln: lasagneblätter,weiße lasagneblätter,teigblätter,trenette,tortiglioni,bucatini,pappardelle,pennette,tubettini,trockene tubettini,capelli d'angelo,capelli d`angello,fideli,hörnli,hörnle,hörnchen,maccaroni,makaroni,eiergerstel,gekochte eiergerstel,teigwaren,tagliatelli,barilla fußilini,barilla ondine,bandnudelnester,gelbe bandnudelnester,soba,vermicelli,nüdeli,chinesische nüdeli,ravioli,gefüllte teigtaschen,fusilini,mehlbüdel,gekochte klöße,fritierte mehlklöße
couscous: cous-cous
tortilla: strudelteig,frischer strudelteig,filoteig,phyllo-teig,strudelblätter,tk-strudelblätter,strudelteigblatt,strudelteigblätter,tk-filoteigblätter,frühlingsrollenblätter,frühlingsrollen-teigblätter,frühlingsrollen-teigstücke,won tan hüllen,won-tan-hüllen,wan tan-hüllen,wan-tan,wan tun skins,wan tun skin,teighüllen,hüllen,tiefgekühlte hüllen,tostadas,taco-schalen,weicher tortillateig
blaetterteig: tk-blätterteigplatten,blätterteigplatten,rechteckige tk-blätterteigplatten,blätterteigkissen
muerbeteig: kuchenteig,mürbteig,pizza-teig,grundmischung pizza-teig,pizza-fix,mürbeteigtörtchen
kekse: biskuitboden,schokobiskuitboden,pan di spagna,löffelbiskuit,zitronentörtchen,backopladen,eiswaffeln,hippenröllchen,waffeln,sand- oder zitronenkuchen,elisenkuchen,geriebene elisenkuchen,braune gewürzkuchen,kuchenreste,diabetiker-vollkornplätz-,panettone,christstollen,mandelstuten,muffins
brot: englischbrot,toastkrümel,brotkrumen,frische brotkrumen,weißbrotkrumen,mie de pain,canapes,fertige canapes,reiskuchen
semmelbroesel: croutons,weißbrotcroutons
zwieback: zwiebäcke
puderzucker: gummibärchen,weiße marshmallows,marshmallows,kandierte veilchen,bonbons,liebesperlen,zuckerperlen,grüne zuckerperlen,silberperlen,zuckerstreusel,buntstreusel,silberstreusel,goldende zuckerkügelchen,buntes zuckerwerk,zuckerlösung,dekorschnee,puderzucekr
schokolade_zartbitter: schokoladenstreuseln,schokoladenraspel,schokospäne,schokoladespäne,schokoladetröpfchen,couvertüre,schokoladencouverture,schoko-fettglasur,haselnussglasur,haselnußglasur,kakaoglasur,schokoladenguss,halbbitter,halbbittere,schoko-bits
kakao: kakaopuder,kako,carob
marzipan: marzipanmasse
zitronenschale: sukkade,sukade,citronat,zitronat,zitranat,orangat,orangenat,pomeranzenschale,limonenschale,abgeriebene limonenschale,apfelsinenschale,getrocknete tangerinenschale,tangerinenschale,getrocknete mandarinenschale,mandarinenschale,zitornenschale,feine orangenfrucht,angelikastengel
pflaume: zwetschge,zwetschgenmus,pflaumenmus
aprikose: pfirsische
apfel: gravensteiner,bratapfelpüree,apfelkraut
trockenobst: 
beeren: beerenobst,erdheeren
orange: tropischen fruchtmix
zitrone: limone,limonen,zitronenschnitze,zitronenscheibe,zitronenachteln,zitronenspalten,limonenschnitze
banane: bananenblüte
tomate: tomatillos,dosentomatillos,pellati,pellati gehackt
pastinake: teltower rübchen,weiße rübchen,navetten,junge navetten,rape,petersilienwürzel
blumenkohl: romanesco,romanescoröschen
brokkoli: brocoli
staudensellerie: selleriestängel,karden
lauch: lauchstängel,poree,porreestange,poreestangen,porreblätter,porree
schalotte: echalottes,schaloten,cipollata,gartenzwiebelchen
zwiebel: lauchwiebel
knoblauch: knoblaichzehe,knobauchzehe,knochlauchzehen,koblauchzehen,knobizehe,noblauchzehen
mais: maiskölbchen,babymaiskölbchen,mini-maiskölbchen,junge maiskölbchen
kuerbis: kürbisse
vanilleschote: vanillestängel,vanillepaste,vanilllepulver
ingwer: ingwerknolle,ingwerscheibe,zitronengrasstängel,lemongras
rhabarber: rhabarberstange,rhabarberblüten
senf: wasabi,wasabipaste,wasabipulver,wasabi-pulver,washabi-paste,angerührtes wasabi
spinat: weinblätter,eingelegte weinblätter,krautstiel,krautstiele,krautstielblätter,mangoldstaude
nuesse: paranuß,paranuss,cashew-kerne,gehackte cashew-kerne,pinienkern,erdüsse,nuss
mandeln: mandel,mandelscheiben,mandelhälften,abgezogene mandelhälften,mandel bitter,abgezogene mandeln
walnuesse: baumnuss,baumnüsse,baumnuss-kerne,walnusskernhälften,walnußkerne
kokosraspel: kokosspäne,cocosnusspulver,cocosnuss-pulver,cocosnuss,kokosnusspulver
oel: sojaöl,sojaoel,salatöl,keimöl,palmöl,planzenöl,pflanzeenöl,woköl,chinaöl,chinaoel,haselnussöl,baumnussöl,nussöl,mandelöl,arganöl,limonenöl,rappsöl,paprikaöl,paprikaoel,achioteöl,trüffelöl,blütenpfefferöl,raps- oder nussöl,sonnenblumen- oder maisöl
olivenoel: olio vergine,olivelöl,ölivenöl,olivenööl
sesamoel: sesamsamenöl,seasamöl
sojasosse: soya sauce,soja sauce,soya-sauce,soja-sauce,helle soya sauce,helle soja sauce,dunkle soya sauce,dunkle soja sauce,helle soyasauce,soyasauce,shoyu,ketjab asin,kecap asin,saijasauce,soja sauce pikant,pikante soja-sauce,worcester sauce,suppenwürze,speisewürze,fondor,aromat,würze
ketchup: chilli-sauce,chilli sauce,chillisauce,chinesische chillisauce,pfefferschotensauce,tabascosauce,süß-sauer-scharfe sauce,süß-saure ingwersauce,szechuan-sauce,asia-sauce,okonomiyaki-sauce,braune okonomiyaki-sauce,schaschliksauce,pfefferminz-sauce
hoisinsauce: gelbe bohnensauce,bohnenpaste,süße bohnenpaste,süße bohnensauce,süsze bohnensauce,scharfe bohnensauce,milde bohnenpaste,sojabohnenpaste,sojabohnenmus,braune bean-sauce,osmanthuspaste
mayonnaise: mayonaise,mayonaisesaucen,knoblauchmayonaise
sahnesosse: bechamelsauce,béchamelsauce,braune grundsauce,spanische sauce,demi-glace,braune fleischsauce,kalbsjus,kalbjus,bratenjus,bratensaucewürfel,saucenbinder,heller saucenbinder,dunkler saucenbinder,saucenbinder hell,dunkles saucenbindemittel,käsesahnesauce,dillsahnesauce,trüffelsauce,schokoladensauce,vanillesauce
tomatenmark: ajvar,paprikamark,paprikapaste,paprikakrem,peperonipaste,gulaschpaste,scharfe salca,tomatenpurree,gewürztes tomatenfruchtfleisch
pesto: oliventapenade,tapenade,alexanders würzöl,pistou,olivenpaste,schwarze olivenpaste
wuerzpaste: sambal badjak,sambal,nam prik,nam prik phau,chilli-paste,chilipaste,pfefferpaste,d'lich
tomatensosse: sugo,letscho
bruehe: hühnersuppe,hühnerboullon,gemüsefonds,gemüsesud,kochsud,fleischsud,kochflüssigkeit,gemüsebrülle,dashi,dashigranulat,tomatensuppe,dolmades-flüssigkeit,weinsud
bruehpulver: hühnerbouillonwürfel,gemüsebouillonwürfel,brühwüfel,fleischwürfel,liebig's fleischextrakt,fleischextrakt
suppengruen: suppenbund,suppengrüm
likoer: maraschino,grand manier
spirituosen: armagnac,kognak,aquavit,pisco,williams christ,birnengeist,williamsbirnengeist,marc,rhum,arrak,96%igen alkohol,alkohol,apfelbrand,fernet branca,angostura,angostura-bitter,angostura bitter,angostura aromatic bitters
wein_weiss: muscadet,gewürztraminer,moscato d'asti,weißer martini,vin santo,vinsanto,trockener vinsanto,marc de provende
wein_rot: chianti,barolo,junger barolo,roter burgunder,beaujolais,banyuls
bier: kristallweizen
kaffee: teebeutel,teeblätter,teebeutel grüntee citron,mokka
natron: natriumbikarbonat,bikarbonat,ammonium
agar: apfelpektin,gelierfix,gel-oder gelierfix,biobin,pflanzliches bindemittel,sulzpulver,gelierzauber
backpulver: backin,dr oetkers backin,bachpulver
vanilleextrakt: rosenessenz,orangenblütenessenz,mandelessenz,jasminextrakt,zitronenextrakt,zitronenöl,bittermandel-backöl,rosenöl,orangback,citroback
vanillezucker: vanillin
puddingpulver: backfeste pudding creme,backfeste puddingcreme,dr oetker backfeste pudding creme,pudding,vanillecreme,creme chiboust
tofu: okara,toufu,tauhu,tauhu-pok,weißer tauhu,weicher tauhu,tofuwürfel,tempeh
bohnen_trocken: weiße perlbohnenkerne,getrocknene cannellini,cannellini
gruene_bohnen: böhnchen
ei: wachtelei,wachteleier,dünne omelettes
eiweiss: eiweiá,eiweiáe,eiweisse
hefe: hefewürfel
kartoffel: gschwellti,kartoffelteig,topinambur
ersatz_sonstiges: kartoffelsticks
gemuesemix: verdura mista
speiseeis: vanilleglace,karamellglace,orangenglacé,zimtglace
wasser: eisstücke,eis,viel eis,zerstoßenes eis,zerstoáenes eis
schmalz: rindertalg,talg,knochenmark
butter: butterflöckli,butterflocken
essig: pflaumenbalsam,alter pflaumenbalsam
margarine: schmelzmagarine,schmelzmargarine
marmelade: hägemark,hagebuttenmus,zwetschgensauce,süße zwetschgensauce
radieschen: radiesli
orangensaft: apfelgarflüssigkeit,aprikosennektar

sahnesteif: sahnetorten-hilfe
samen: mohnfüllung,tulsi-samen
kohlrabi: kohlkrabiknolle,kohlrabiknolle
mango: mangospalten
gefluegelleber: pouletlebern,pouletleber,gänselebermousse,gänseleber
kirschen: sauerkirschglas
maronen: marroni,geschälte marroni
spargel: spargelreste
"""

def extra_aliases():
    """Zusätzliche Namen je Schlüssel (Schweizerdeutsch, Schreibvarianten, asiatische Zutaten …)."""
    out = {}
    for line in EXTRA.strip().splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        key, names = line.split(':', 1)
        out.setdefault(key.strip(), []).extend(n.strip().lower() for n in names.split(',') if n.strip())
    return out

def parse():
    foods = []
    extra = extra_aliases()
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
        aliases = [a.strip().lower() for a in (de + ',' + en).split(',') if a.strip()] + extra.pop(key, [])
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
    if extra:
        sys.exit("Zusatznamen für unbekannte Schlüssel: " + ", ".join(sorted(extra)))
    return foods

if __name__ == "__main__":
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..")
    foods = parse()
    keys = [f["key"] for f in foods]
    assert len(keys) == len(set(keys)), "doppelte Schlüssel"
    out = os.path.join(root, "resources", "data", "lebensmittel.json")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump({"format": "ViviaMeals-Foods", "version": 1,
                   "note": "Richtwerte je 100 g – Schätzwerte für die Rezeptplanung, keine Analysewerte.",
                   "foods": foods}, f, ensure_ascii=False, indent=1)
    print(len(foods), "Lebensmittel ->", out)
