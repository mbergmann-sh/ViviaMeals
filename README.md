# ViviaMeals

Rezeptverwaltung mit **Nährwertanalyse**, **Kühlschrank-Vorschlägen**, **Wochenmenü-Plänen** und
**Assistenten für Gewichtsverlust und Gewichtszunahme** – Deutsch, Qt 6 (bzw. Qt 5.15) / C++17 / qmake – PolyForm Noncommercial 1.0.0 (frei für nicht-kommerzielle Nutzung).
Technisch und optisch an [ViviaParms](https://github.com/mbergmann-sh/ViviaParms) und
[ViviaBreathe](https://github.com/mbergmann-sh/ViviaBreathe) angelehnt (Themes, Symbole, `screenfit`, `windowstate`).

> **Kein Medizinprodukt.** ViviaMeals ist eine persönliche Rezept- und Ernährungsverwaltung. Nährwerte,
> Energiebedarf und Wochenpläne sind Schätzwerte und ersetzen keine ärztliche oder Ernährungsberatung.

![ViviaMeals – Hauptfenster (Demo-Daten)](docs/images/hauptfenster.png)

| Rezepteditor mit Nährwertanalyse | Englisches MealMaster-Rezept, umgerechnet | Dark-Theme (VS Code) |
|---|---|---|
| ![Editor](docs/images/editor.png) | ![Englisch](docs/images/hauptfenster-englisch.png) | ![Dark](docs/images/hauptfenster-dark.png) |
| **Rezept scannen (Texterkennung)** | **Gescanntes Rezept im Editor, Scan als Vorlage** | **Einstellungen: Scannen** |
| ![Scannen](docs/images/scannen.png) | ![Scan im Editor](docs/images/scannen-editor.png) | ![Einstellungen Scannen](docs/images/einstellungen-scannen.png) |
| **Kühlschrank & Einkaufsliste** | **Assistent Gewichtskontrolle** | **Wochenmenü-Plan** |
| ![Kühlschrank](docs/images/kuehlschrank.png) | ![Assistent](docs/images/assistent.png) | ![Wochenplan](docs/images/wochenplan.png) |
| **Ernährungstagebuch** | **Tagebuch: Mahlzeit eintragen** | **Gewichtskontrolle** |
| ![Tagebuch](docs/images/tagebuch.png) | ![Tagebuch-Eintrag](docs/images/tagebuch-eintrag.png) | ![Gewichtskontrolle](docs/images/gewichtskontrolle.png) |
| **Suche** | **Import-Vorschau mit Doubletten** | **Kleiner Bildschirm (1024 × 640)** |
| ![Suche](docs/images/suche.png) | ![Import](docs/images/import-vorschau.png) | ![Klein](docs/images/hauptfenster-klein.png) |
| **Nährwerte aus CSV importieren** | **Import mit Umlaut-Korrektur** | **Einstellungen** |
| ![Nährwert-Import](docs/images/naehrwerte-import.png) | ![Umlaute](docs/images/import-umlaute.png) | ![Einstellungen](docs/images/einstellungen.png) |
| **Assistent: Tipps und Motivation** | **Rezeptbild zuschneiden** | |
| ![Tipps](docs/images/assistent-tipps.png) | ![Zuschneiden](docs/images/bild-zuschneiden.png) | |

> **Umbenennung:** Bis Version 0.5 hieß das Programm **VitaMeals**. ViviaMeals übernimmt beim ersten Start
> Einstellungen, Profile, Tagebücher und eigene Lebensmittel der alten Fassung (die alten Daten bleiben
> unverändert), öffnet bestehende Rezeptdatenbanken (`vitameals.vmdb` → `viviameals.vmdb`) und `.vmr`-Dateien
> weiterhin; der Windows-Installer bietet an, die alte Installation in `C:\VitaMeals` zu entfernen.

## Funktionen

| Bereich | Umsetzung |
|---|---|
| **Rezeptfenster** | links die Rezeptliste (Schnellfilter nach Name/Zutat/Tag, Themenauswahl, Favoriten), rechts die HTML-Ansicht: Foto rechts oben, daneben Titel, Portionen, kcal/kJ je Portion, Tags; darunter Zutaten in deutschen Küchenmaßen, Zubereitung und **Nährwerttabelle**. Portionenzahl umstellbar (Mengen werden umgerechnet). Beim Start wird die zuletzt geöffnete Datenbank geöffnet und ein **zufälliges Rezept** gezeigt; ohne Datenbank fordert ViviaMeals zum Anlegen auf (auf Wunsch mit Beispielrezepten) |
| **Rezeptdatenbank** | ein Ordner mit `viviameals.vmdb` (Kennung) und je Rezept einer Datei `rezepte/<id>.vmr` (Eigenformat, JSON, Foto eingebettet) – sicherbar, synchronisierbar (Nextcloud/Dropbox), einzelne Rezepte weitergebbar; `index.json` hält Kurzfassungen für schnelle Listen und Suche. *Datei → Datenbank leeren …* löscht alle Rezepte nach Eingabe des Datenbanknamens, wahlweise mit Sicherungskopie |
| **Editor** | Titel, Rezeptbild (Dateiauswahl mit **Zuschneiden**: Ausschnitt 4:3, 3:2, 16:9, 1:1, 3:4 oder frei, drehen; gespeichert wird nur der Ausschnitt als PNG, höchstens 800 px breit), Portionen/Einheit, Zubereitungszeit, Tags (mit Vorschlägen), Quelle, Favorit, Zutatenliste (eine je Zeile, `== Abschnitt ==`), Zubereitung. **Live-Nährwertanalyse** während der Eingabe; Zuordnung einer Zutat zur Nährwerttabelle per Doppelklick änderbar und für alle Rezepte lernbar; Tags vegan/vegetarisch/fleischgerichte/fisch und Mahlzeit-Tags aus Titel, Zutaten und Nährwerten ergänzen |
| **Nährwertanalyse** | Energie (kcal **und** kJ), Fett, gesättigte Fettsäuren, Kohlenhydrate, Zucker, Ballaststoffe, Eiweiß, Salz – gesamt, je Portion und in % der Referenzmenge (LMIV). Mengen werden in Gramm umgerechnet (Gewicht direkt, Volumen über die Dichte, Stück/Zehe/Dose/Pck. über Stückgewichte, „1 Süßkartoffel (ca. 400 g)“ über die Angabe). Ergebnis wird **im Rezept gespeichert**; nicht zuordenbare Zutaten werden genannt |
| **Nährwerttabelle** | eingebaut: rund 210 Lebensmittelgruppen mit Richtwerten je 100 g, Dichte, Stückgewichten und deutschen/englischen Namen (`tools/make_foods.py`). Unter *Extras → Nährwerttabelle* eigene Lebensmittel anlegen und Werte überschreiben (`lebensmittel_eigene.json`) |
| **Nährwerte importieren (CSV)** | *Extras → Nährwerttabelle → Importieren / Exportieren*: eigene Tabellen aus Excel/LibreOffice, die mitgelieferte **CSV-Vorlage** oder **Open-Food-Facts**-Exporte – Trennzeichen, Zeichensatz (UTF-8, Windows-1252, CP850), Dezimalkomma, „<0,1“ und kJ/Natrium werden erkannt bzw. umgerechnet, Spalten anhand der Überschriften zugeordnet (änderbar), mit Vorschau. Außerdem der entpackte CSV-Ordner von **USDA FoodData Central** (Foundation Foods, SR Legacy – gemeinfrei, CC0). Jeder Import erhält eine Herkunft und lässt sich wieder entfernen; wahlweise ersetzen eigene Werte die eingebauten gleichen Namens. Eigene Lebensmittel lassen sich als CSV exportieren |
| **Rezept scannen** | *Datei → Import → Rezept scannen …* (Strg+Umschalt+S, Werkzeugleiste) bzw. *Rezept aus Bild oder Foto erkennen …*: Seite **scannen** – unter **Windows** mit dem Scandialog des Systems (WIA, jeder Scanner mit WIA-Treiber), unter **Linux** mit **XSane** oder `scanimage` (Bild wird automatisch übernommen) bzw. GNOME-Dokumentenscanner, Skanpage, Skanlite, unter **macOS** mit „Digitale Bilder“/Vorschau, überall auch mit eigenem Befehl – oder **Bild/Handyfoto öffnen** (EXIF-Drehung). **Fotos auf der Seite werden erkannt**, orange markiert und vor der Texterkennung ausgeblendet (Klick entfernt, Ziehen fügt hinzu); das größte wird als **Rezeptbild** vorgeschlagen (zuschneidbar). Texterkennung mit **[Tesseract](https://github.com/tesseract-ocr/tesseract)** (Deutsch + Englisch, läuft lokal), danach Aufteilung in **Titel** (größte Schrift), **Portionen** („für 4 Personen“), **Zubereitungszeit**, **Zutaten** (Mengenangaben, Überschrift „Zutaten“, zweispaltige Listen, umbrochene Zeilen) und **Zubereitung** („Zubereitung“, „So wird’s gemacht“, Silbentrennung); typische Lesefehler werden korrigiert (l/2 → ½, 1OO g → 100 g, El → EL, 180 *C → 180 °C, Aufzählungszeichen, fehlende Umlaut-Punkte wie „Krauterquark“ → „Kräuterquark“). Mehrseitige Rezepte: weitere Seiten hinzufügen. Alles bleibt editierbar; „Im Editor öffnen“ übernimmt das Rezept mit Nährwerten und Tags wie beim Import, der Editor zeigt den **Scan als Vorlage** daneben |
| **Import** | **RezKonv** und **MealMaster** – einzelne Dateien und Sammlungen, mehrere Dateien auf einmal; Zeichensatz-Erkennung (UTF-8, Windows-1252, DOS-CP850); Abschnitte, Fortsetzungszeilen, zweispaltige MealMaster-Zutaten, Quelle; winREZKONV-Besonderheiten (gekürzte Titel über „O-Titel“, „180GradC“, „Prise/n“, „Kugel(n)“, „Zitrone: Saft“, „à 125 g“). Menüfolgen werden als Menü angezeigt (ohne Nährwerte). **Vorschau** mit Auswahl und **Doubletten-Abfrage** (Überspringen / Ersetzen / Beide behalten, „für alle übernehmen“). Außerdem `.vmr` und alte Listen `.vtm` des Prototyps |
| **Umlaut-Korrektur** | ältere RezKonv-/MealMaster-Sammlungen in ASCII-Umschrift („Kaese“, „Gemuesebruehe“, „heiss“) werden beim Import korrigiert: ae/oe/ue → ä/ö/ü mit Ausnahmen (Feuer, Sauerbraten, Quelle, zuerst, aktuell, Baguette, Fondue, Oetker, Balsamicoessig …), ss → ß nur in bekannten Wortstämmen (heiß, weiß, gießen, süß, Grieß, gleichmäßig …), sodass Wasser, Schüssel, Masse und Essig bleiben. Großgeschriebene Abschnitte werden normal geschrieben. Abschaltbar; außerdem im Editor („Umlaute korrigieren“) und für die ganze Datenbank (*Extras*) |
| **Übersetzung mit DeepL (optional)** | fremdsprachige Rezepte (Englisch, Französisch, Italienisch, Spanisch, Niederländisch …) werden beim Import nach Rückfrage mit **DeepL** vollständig ins Deutsche übersetzt – Titel, Zutaten und Zubereitung; deutsche Rezepte werden nie gesendet. Jeder trägt seinen **eigenen API-Schlüssel** ein (kostenloser Tarif *DeepL API Free*, 500.000 Zeichen im Monat), Verbindungstest zeigt den Verbrauch. Ohne Schlüssel oder bei Fehlern übernimmt das Glossar. Auch im Editor und für alle vorhandenen Rezepte (*Extras*) |
| **Englische Rezepte** | Einheiten werden automatisch umgerechnet (cups → ml bzw. g über die Dichte, oz/lb → g, fl oz/pt/qt → ml, tbsp/tsp → EL/TL, stick butter → g, „1 (16 oz) can“ → g), Zutaten und Zusätze über ein **Offline-Glossar** übersetzt („chopped walnuts“ → „Walnüsse, gehackt“), Temperaturen (°F → °C) und Zoll in der Anleitung umgerechnet. Ohne DeepL bleibt die Anleitung selbst englisch (Hinweis in der Ansicht) |
| **Mahlzeit-Tags** | beim Import (abschaltbar), im Editor und unter *Extras* für alle Rezepte: **frühstück**, **hauptgerichte**, **zwischenmahlzeit**, **desserts**, **süßspeise**, **backen** – geschätzt aus Kategorien, Titel, Zutaten und Nährwerten (z. B. „Flammeri“ → Dessert, „Porridge“ → Frühstück, Vorspeisen mit wenig Energie → Zwischenmahlzeit) |
| **Nährwert-Tags** | ebenso automatisch aus den berechneten Nährwerten: **eiweißreich** (≥ 20 % der Energie aus Eiweiß und ≥ 15 g je Portion), **fettarm** (≤ 3 g Fett je 100 g), **kalorienarm** (≤ 80 kcal je 100 g und ≤ 400 kcal je Portion), **hochkalorisch** (≥ 250 kcal je 100 g oder ≥ 800 kcal je Portion), **vitaminreich** (≥ 200 g Gemüse, Obst und Kräuter je Portion bzw. ≥ 50 % des Portionsgewichts; nicht bei Gebäck), **reduktionsdiät** (≤ 600 kcal und ≤ 150 kcal/100 g, ≤ 35 % Fett-, ≤ 20 % Zucker-, ≥ 15 % Eiweißenergie, kein Dessert). Grenzwerte für „eiweißreich“ und „fettarm“ nach der EU-Verordnung 1924/2006, die übrigen an Mahlzeiten angepasst. Nicht mehr zutreffende Nährwert-Tags werden beim Neuberechnen entfernt; Rezepte mit zu wenigen bekannten Zutaten bleiben unverändert |
| **Suche** | Rezeptnamen (auch Teilnamen), Zutaten, Tags (UND/ODER), höchstens kcal je Portion, Favoriten, mit Foto |
| **Kühlschrank** | Bestandsliste und Grundvorrat pflegen → passende Rezepte mit „vorhanden x/y“ und fehlenden Zutaten, **Einkaufsliste** (nach Warengruppen, kopieren/drucken/speichern) |
| **Assistent Gewichtskontrolle** | beliebig viele Personen mit Größe, Alter, Aktivität (PAL), Ziel (verlieren/halten/zunehmen), Zielgewicht, Tempo, Ernährungsform, ausgeschlossenen Zutaten und bevorzugten Themen; BMI, Grund- und Gesamtumsatz (Mifflin-St Jeor), Zielzufuhr mit Schutzgrenzen, Dauer bis zum Zielgewicht; **Gewichtsverlauf** mit Grafik; **Wiegetag** mit Erinnerung im Hauptfenster („Erledigt“ blendet sie bis zur nächsten Woche aus, ein Eintrag am Wiegetag ebenso); **Motivation** mit Fortschritt seit dem ersten Eintrag, kurze Erklärung zum Abnehmen/Halten/Zunehmen und Reiter **Tipps** („statt Butter lieber Tomatenmark …“, gute Gewohnheiten nach DGE „Gut essen und trinken“) |
| **Ernährungstagebuch** | *Planen → Ernährungstagebuch* (F9, auch im Kontextmenü der Rezeptliste): für **jede Person ein eigenes Tagebuch**. Mahlzeiten mit Datum und **Uhrzeit** (änderbar), Mahlzeit (Frühstück … Spätmahlzeit, nach der Uhrzeit vorgeschlagen), **gegessener Menge** in Portionen oder Gramm und den daraus berechneten Nährwerten. Speisen **aus einem Rezept der Datenbank** (Suche nach Name, Zutat, Tag), als **eigenes Rezept** im Editor erfasst (auf Wunsch in die Rezeptdatenbank übernommen) oder **aus dem gespeicherten Wochenplan**. Anzeige von Ziel, gegessen und **verfügbaren Restkalorien** mit Balken, Tagesnährwerten und den letzten 7 Tagen; Einträge bearbeiten, löschen, „auf heute kopieren“. Gespeichert unter `<AppData>/tagebuch/<Person>.json`; Nährwerte werden beim Eintragen kopiert, das Tagebuch bleibt also auch nach Änderungen an Rezepten stimmig |
| **Gewichtskontrolle** | *Planen → Gewichtskontrolle* (F8): Datum und Person wählen, Gewicht eintragen – es kommt in den Gewichtsverlauf (ein Eintrag desselben Tages wird ersetzt). Zeigt letzten Eintrag, Veränderung, BMI und Abstand zum Zielgewicht; erledigt die Wiegetag-Erinnerung |
| **Wochenmenü-Plan** | Frühstück/Mittag/Abend (+ Zwischenmahlzeit) über 1–14 Tage passend zum Kalorienziel (Anteile 25/35/30/10 % bzw. 30/40/30 %). **Frühstück nur aus Frühstücksrezepten**, Zwischenmahlzeiten aus Snacks bzw. leichten Frühstücksrezepten, Mittag-/Abendessen aus Hauptgerichten; beim **Abnehmen** keine Desserts/Süßspeisen und Vorrang für leichte, eiweiß- und ballaststoffreiche Rezepte mit wenig Zucker. Portionen mit **Gramm** („1 ¼ Portionen (ca. 375 g)“), sinnvolle Höchstmengen je Mahlzeit, Tagessummen mit Eiweiß und Ballaststoffen, Wochendurchschnitt mit Hinweisen nach DGE-Richtwerten. Fehlen Frühstücks- oder Snack-Rezepte, weist der Plan darauf hin und bietet **zehn mitgelieferte gesunde Rezepte** an. Einzelne Mahlzeiten neu würfeln oder gezielt wählen; **Einkaufsliste** (Vorrat abziehbar), Druck, Speichern im Profil |
| **Website-Export** | themenbasierte **HTML5/PHP-Website**: Startseite, Themenseiten (Tags), Rezeptseiten mit Foto und Nährwerten, Menü, responsives Layout; Suche serverseitig (`suche.php`, PHP 7.4+) und sofort im Browser (JavaScript) – Ordner nur noch hochladen |
| **LaTeX-Export** | einzelnes Rezept als `.tex` für **book** oder **scrbook** – vollständiges Dokument oder Kapitel zum `\input`, mit Foto (JPEG daneben), Zutatentabelle und Nährwerttabelle (booktabs). Mit pdflatex geprüft |
| **LibreOffice-Writer-Export** | einzelnes Rezept als `.odt` (A4): Titel, darunter **rechts das Rezeptbild, vom Text umflossen**, Portionen mit Gramm, Energie, Tags, Zutaten (Mengen an einem Tabulator ausgerichtet), Zubereitung, Nährwerttabelle; **Fußzeile mit Trennlinie** (Rezepttitel, „Seite x von y“). Ohne Fremdbibliothek erzeugt, mit LibreOffice geprüft |
| **Darstellung** | alle Qt-Styles sowie „Dark“ und „Visual Studio Code Dark“ (aus ViviaParms/ViviaBreathe), sofort umschaltbar; Symbole passen sich hell/dunkel an |
| **Fenster und Bildschirm** | Lage und Größe des Hauptfensters, der Aufteilung und aller Dialoge werden gemerkt (`windowstate`); auf kleinen Bildschirmen werden Dialoge rollbar und in den Bildschirm geholt (`screenfit`); **Schriftgröße automatisch** nach Bildschirmgröße oder fest (85–140 %) |
| **Demo** | `ViviaMeals --demo`: temporäre Datenbank mit Beispielrezepten, Bildern, Profil und Vorrat – eigene Daten bleiben unberührt |
| **Windows-Installer** | Qt Installer Framework, Zielordner-Vorgabe `C:\ViviaMeals`, Startmenü und Desktop, Dateizuordnung `.vmr`/`.vmdb`, Update über neueren Installer – wird beim Release-Build automatisch erzeugt (siehe [installer/README.md](installer/README.md)) |
| **Kommandozeile / Doppelklick** | `ViviaMeals rezept.vmr`, `ViviaMeals sammlung.mmf`, `ViviaMeals <ordner>/viviameals.vmdb` – Rezepte übernehmen, Sammlungen importieren, Datenbank öffnen |

## Bauen

Benötigt Qt 6.4+ (oder Qt 5.15) mit den Modulen **widgets, printsupport, svg, network** (network nur für DeepL).
Für *Rezept scannen* wird zur Laufzeit das freie Programm **Tesseract** genutzt (nicht eingelinkt, kein Build-Bestandteil):
Windows – Installer der [UB Mannheim](https://github.com/UB-Mannheim/tesseract/wiki) mit der Sprache *German*
(oder mit dem ViviaMeals-Installer mitliefern, siehe [installer/README.md](installer/README.md)); Linux –
`sudo apt install tesseract-ocr tesseract-ocr-deu` (Fedora: `tesseract tesseract-langpack-deu`); macOS –
`brew install tesseract tesseract-lang`. Der Windows-Scandialog braucht nur `ole32`/`oleaut32` (Systembestandteil).
Für die HTTPS-Verbindung zu DeepL nutzt Qt 6 unter Windows das systemeigene Schannel (Plugin `tls/qschannelbackend.dll`,
kopiert `windeployqt` automatisch); unter Qt 5.15 müssen zusätzlich die OpenSSL-1.1-DLLs neben `ViviaMeals.exe` liegen.

```sh
qmake6 ViviaMeals.pro      # bzw. qmake (Qt 5.15)
make -j8                  # Windows: mingw32-make (MinGW) bzw. nmake/jom (MSVC)
```

Getestet mit Qt 6.4.2 unter Linux (GCC 13). Für den MinGW-Build in Qt Creator einfach `ViviaMeals.pro` öffnen.
**Windows:** Jeder Build ruft `windeployqt` auf; ein **Release-Build erzeugt zusätzlich den Installer**
`installer/Output/ViviaMeals-<Version>-Setup.exe`, sofern das Qt Installer Framework installiert ist –
Einzelheiten in [installer/README.md](installer/README.md).
Weitergabe: `windeployqt ViviaMeals.exe` (kopiert auch die deutschen Qt-Übersetzungen, damit Standarddialoge
„Speichern/Abbrechen“ zeigen), macOS `macdeployqt`. Das Programmsymbol liegt als `.ico`, `.icns`, `.png` und `.svg`
in `resources/icons/app/`.

Test des Kerns (Zutatenparser, Einheiten, Glossar, Umlaute, Spracherkennung, Nährwerte, Rezept- und CSV-Importe, USDA, Datenbank,
DeepL gegen einen lokalen Testserver, Scan-Aufbereitung und – falls installiert – Texterkennung einer gerenderten Seite; optional LaTeX-Ausgabe).
`./coretest --dump <datei>` zeigt, wie eine Rezeptdatei importiert wird, `./coretest --scan <bild>` die Texterkennung eines Scans:

```sh
cd tests/coretest && qmake6 && make && ./coretest [ordner-für-tex-dateien]
```

Bildschirmfotos neu erzeugen: `ViviaMeals --demo --screenshots docs/images`.

## Aufbau

```
ViviaMeals.pro   version.pri   core.pri (Kern ohne Fenster)   desktop.pri (Qt-Widgets-Oberfläche)
resources/      icons/ (SVG → zur Laufzeit eingefärbt), data/lebensmittel.json, samples/ (Beispielrezepte)
src/
  recipe.*            Rezeptmodell, Nährwerte, Eigenformat .vmr (JSON)
  recipedb.*          Rezeptdatenbank als Ordner (Index, Doubletten, Tags)
  units.*             Küchenmaßeinheiten deutsch/englisch/MealMaster/RezKonv, °F→°C, Zoll→cm
  ingredientparser.*  Zutatenzeilen zerlegen (Brüche, Bereiche, Packungsgrößen, Abschnitte)
  glossary.*          Offline-Glossar Englisch → Deutsch, Spracherkennung, Kategorien → Tags
  umlauts.*           Umlaut-Korrektur für ASCII-Umschrift (ae/oe/ue/ss)
  mealtypes.*         Mahlzeit-Arten (Frühstück, Hauptgericht, Zwischenmahlzeit …), Portionsgewicht, Kennzahlen
  coaching.*          Motivation, Erklärungen und Tipps für den Assistenten
  diary.*             Ernährungstagebuch je Person (Einträge, Tagessummen, Speicherung)
  odtexport.*         LibreOffice-Writer-Export (.odt, eigener ZIP-Schreiber)
  deepl.*             optionale Übersetzung mit der DeepL-API (eigener Schlüssel)
  ocr.*               Texterkennung mit Tesseract (Aufruf, TSV-Ausgabe → Zeilen mit Lage und Schrifthöhe)
  recipescan.*        Fotos auf Scans finden, Lesefehler korrigieren, Text → Titel/Portionen/Zutaten/Zubereitung
  scanner.*, wiascan_win.*   Scanner ansprechen (Windows: WIA über COM; Linux/macOS: Scanprogramm starten)
  foodtable.*         Nährwerttabelle (eingebaut + eigene + gelernte Zuordnungen), Namensabgleich
  foodcsv.*           CSV-Import/-Export eigener Nährwerte, USDA FoodData Central
  nutrition.*         Nährwertanalyse, Referenzmengen, automatische Tags
  textcodec.*         Zeichensatz-Erkennung (UTF-8, CP1252, CP850)
  importers.*         RezKonv, MealMaster, .vtm
  recipehtml.*        Rezept als HTML (Anzeige und Website)
  websiteexport.*     Website (HTML5, CSS, JavaScript-Sofortsuche, suche.php)
  latexexport.*       LaTeX für book/scrbook
  fridge.*            Bestand, Vorschläge, Einkaufslisten
  profiles.*          Personen, Gewichtsverlauf, Energiebedarf
  mealplanner.*       Wochenmenü-Plan
  mainwindow.*, recipeview.*, editordialog.*, scandialog.*, imagecropdialog.*, weighindialog.*, diarydialog.*, searchdialog.*, importdialogs.*, exportdialogs.*,
  fridgedialog.*, assistantdialog.*, profiledialog.*, weightchart.*, mealplandialog.*, fooddialog.*,
  settingsdialog.*, uiscale.*             Oberfläche
  themes.*, icons.*, screenfit.*, windowstate.*, qtcompat.h   aus ViviaBreathe/ViviaParms übernommen
tools/          make_icons.py (Symbole, ICO/ICNS), make_foods.py (Nährwerttabelle), make_installer_images.py
installer/      Qt-Installer-Framework-Installer (windows_deploy.pri, qtifw/config, qtifw/meta)
tests/coretest/ Test des Kerns
```

### Speicherorte

* **Rezeptdatenbank:** frei wählbarer Ordner (zuletzt geöffnete Datenbanken unter *Datei → Zuletzt geöffnet*).
* **Profile, Gewichtsverlauf, eigene Nährwerte:** `profile.json` und `lebensmittel_eigene.json` im
  Anwendungsdatenordner (`QStandardPaths::AppDataLocation`) – getrennt von den Rezepten, damit eine
  Rezeptsammlung weitergegeben werden kann, ohne persönliche Daten mitzugeben.
* **Fensterlagen, Theme, Kühlschrank, Scanner/Tesseract:** Einstellungen dieses Geräts (`QSettings`).
* **Scans:** werden nicht gespeichert – nur der zugeschnittene Rezeptbild-Ausschnitt landet im Rezept.

### Format `.vmr`

```json
{ "format": "ViviaMeals-Recipe", "formatVersion": 1, "id": "…", "title": "…", "servings": 4,
  "tags": ["vegetarisch"], "ingredients": [ {"amount": 200, "unit": "g", "name": "Mehl", "food": "weizenmehl", "grams": 200},
  {"heading": "Für die Füllung"} ], "instructions": "…", "image": "<Base64-PNG/JPEG>",
  "nutrition": { "total": {…}, "perServing": {"kcal": 450, "kJ": 1883, "protein": 12.5, "fat": 18, "saturatedFat": 9,
  "carbohydrates": 55, "sugar": 20, "fiber": 4, "salt": 0.8}, "matched": 9, "countable": 10, "unmatched": ["…"] } }
```

## Lizenz

© 2026 Michael Bergmann. ViviaMeals steht unter der **PolyForm Noncommercial License 1.0.0**
(Wortlaut in [LICENSE](LICENSE), <https://polyformproject.org/licenses/noncommercial/1.0.0>):

* **erlaubt** – kostenlos – ist jede **nicht-kommerzielle** Nutzung: privat (Hobby, Studium, eigene
  Gesundheitsplanung, Forschung ohne kommerzielle Absicht) und durch gemeinnützige Organisationen,
  Schulen, Hochschulen und Behörden. Der Quelltext darf dafür gelesen, verändert und – zusammen mit
  dem Lizenztext und der Zeile `Required Notice: …` – weitergegeben werden.
* **nicht erlaubt** ist jede **kommerzielle** Nutzung, insbesondere der Verkauf von ViviaMeals oder
  von veränderten Fassungen und der Einsatz in kostenpflichtigen Produkten oder Dienstleistungen.
  Dafür ist eine gesonderte Erlaubnis des Autors nötig.

ViviaMeals ist damit *quelloffen*, aber keine „Open Source“-Software im Sinne der Open Source
Initiative. Die mit dem Windows-Installer ausgelieferten Qt-Bibliotheken stehen weiterhin unter der
LGPL-3.0; ein optional mitgeliefertes Tesseract steht unter der Apache License 2.0
(`tesseract/LIZENZ-Tesseract.txt`). Kein Medizinprodukt; ohne Gewähr.
