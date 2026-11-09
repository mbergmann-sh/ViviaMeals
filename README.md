# VitaMeals

Rezeptverwaltung mit **Nährwertanalyse**, **Kühlschrank-Vorschlägen**, **Wochenmenü-Plänen** und
**Assistenten für Gewichtsverlust und Gewichtszunahme** – Deutsch, Qt 6 (bzw. Qt 5.15) / C++17 / qmake – MIT-Lizenz mit „Commons Clause“ (kein Verkauf).
Technisch und optisch an [VitaParms](https://github.com/mbergmann-sh/VitaParms) und
[VitaBreathe](https://github.com/mbergmann-sh/VitaBreathe) angelehnt (Themes, Symbole, `screenfit`, `windowstate`).

> **Kein Medizinprodukt.** VitaMeals ist eine persönliche Rezept- und Ernährungsverwaltung. Nährwerte,
> Energiebedarf und Wochenpläne sind Schätzwerte und ersetzen keine ärztliche oder Ernährungsberatung.

![VitaMeals – Hauptfenster (Demo-Daten)](docs/images/hauptfenster.png)

| Rezepteditor mit Nährwertanalyse | Englisches MealMaster-Rezept, umgerechnet | Dark-Theme (VS Code) |
|---|---|---|
| ![Editor](docs/images/editor.png) | ![Englisch](docs/images/hauptfenster-englisch.png) | ![Dark](docs/images/hauptfenster-dark.png) |
| **Kühlschrank & Einkaufsliste** | **Assistent Gewichtskontrolle** | **Wochenmenü-Plan** |
| ![Kühlschrank](docs/images/kuehlschrank.png) | ![Assistent](docs/images/assistent.png) | ![Wochenplan](docs/images/wochenplan.png) |
| **Suche** | **Import-Vorschau mit Doubletten** | **Kleiner Bildschirm (1024 × 640)** |
| ![Suche](docs/images/suche.png) | ![Import](docs/images/import-vorschau.png) | ![Klein](docs/images/hauptfenster-klein.png) |
| **Nährwerte aus CSV importieren** | **Import mit Umlaut-Korrektur** | **Einstellungen (DeepL)** |
| ![Nährwert-Import](docs/images/naehrwerte-import.png) | ![Umlaute](docs/images/import-umlaute.png) | ![Einstellungen](docs/images/einstellungen.png) |

## Funktionen

| Bereich | Umsetzung |
|---|---|
| **Rezeptfenster** | links die Rezeptliste (Schnellfilter nach Name/Zutat/Tag, Themenauswahl, Favoriten), rechts die HTML-Ansicht: Foto rechts oben, daneben Titel, Portionen, kcal/kJ je Portion, Tags; darunter Zutaten in deutschen Küchenmaßen, Zubereitung und **Nährwerttabelle**. Portionenzahl umstellbar (Mengen werden umgerechnet). Beim Start wird die zuletzt geöffnete Datenbank geöffnet und ein **zufälliges Rezept** gezeigt; ohne Datenbank fordert VitaMeals zum Anlegen auf (auf Wunsch mit Beispielrezepten) |
| **Rezeptdatenbank** | ein Ordner mit `vitameals.vmdb` (Kennung) und je Rezept einer Datei `rezepte/<id>.vmr` (Eigenformat, JSON, Foto eingebettet) – sicherbar, synchronisierbar (Nextcloud/Dropbox), einzelne Rezepte weitergebbar; `index.json` hält Kurzfassungen für schnelle Listen und Suche |
| **Editor** | Titel, Rezeptbild (Dateiauswahl, auf 800 px verkleinert), Portionen/Einheit, Zubereitungszeit, Tags (mit Vorschlägen), Quelle, Favorit, Zutatenliste (eine je Zeile, `== Abschnitt ==`), Zubereitung. **Live-Nährwertanalyse** während der Eingabe; Zuordnung einer Zutat zur Nährwerttabelle per Doppelklick änderbar und für alle Rezepte lernbar; Tags vegan/vegetarisch/fleischgerichte/fisch aus den Zutaten ergänzen |
| **Nährwertanalyse** | Energie (kcal **und** kJ), Fett, gesättigte Fettsäuren, Kohlenhydrate, Zucker, Ballaststoffe, Eiweiß, Salz – gesamt, je Portion und in % der Referenzmenge (LMIV). Mengen werden in Gramm umgerechnet (Gewicht direkt, Volumen über die Dichte, Stück/Zehe/Dose/Pck. über Stückgewichte, „1 Süßkartoffel (ca. 400 g)“ über die Angabe). Ergebnis wird **im Rezept gespeichert**; nicht zuordenbare Zutaten werden genannt |
| **Nährwerttabelle** | eingebaut: rund 210 Lebensmittelgruppen mit Richtwerten je 100 g, Dichte, Stückgewichten und deutschen/englischen Namen (`tools/make_foods.py`). Unter *Extras → Nährwerttabelle* eigene Lebensmittel anlegen und Werte überschreiben (`lebensmittel_eigene.json`) |
| **Nährwerte importieren (CSV)** | *Extras → Nährwerttabelle → Importieren / Exportieren*: eigene Tabellen aus Excel/LibreOffice, die mitgelieferte **CSV-Vorlage** oder **Open-Food-Facts**-Exporte – Trennzeichen, Zeichensatz (UTF-8, Windows-1252, CP850), Dezimalkomma, „<0,1“ und kJ/Natrium werden erkannt bzw. umgerechnet, Spalten anhand der Überschriften zugeordnet (änderbar), mit Vorschau. Außerdem der entpackte CSV-Ordner von **USDA FoodData Central** (Foundation Foods, SR Legacy – gemeinfrei, CC0). Jeder Import erhält eine Herkunft und lässt sich wieder entfernen; wahlweise ersetzen eigene Werte die eingebauten gleichen Namens. Eigene Lebensmittel lassen sich als CSV exportieren |
| **Import** | **RezKonv** und **MealMaster** – einzelne Dateien und Sammlungen, mehrere Dateien auf einmal; Zeichensatz-Erkennung (UTF-8, Windows-1252, DOS-CP850); Abschnitte, Fortsetzungszeilen, zweispaltige MealMaster-Zutaten, Quelle; winREZKONV-Besonderheiten (gekürzte Titel über „O-Titel“, „180GradC“, „Prise/n“, „Kugel(n)“, „Zitrone: Saft“, „à 125 g“). Menüfolgen werden als Menü angezeigt (ohne Nährwerte). **Vorschau** mit Auswahl und **Doubletten-Abfrage** (Überspringen / Ersetzen / Beide behalten, „für alle übernehmen“). Außerdem `.vmr` und alte Listen `.vtm` des Prototyps |
| **Umlaut-Korrektur** | ältere RezKonv-/MealMaster-Sammlungen in ASCII-Umschrift („Kaese“, „Gemuesebruehe“, „heiss“) werden beim Import korrigiert: ae/oe/ue → ä/ö/ü mit Ausnahmen (Feuer, Sauerbraten, Quelle, zuerst, aktuell, Baguette, Fondue, Oetker, Balsamicoessig …), ss → ß nur in bekannten Wortstämmen (heiß, weiß, gießen, süß, Grieß, gleichmäßig …), sodass Wasser, Schüssel, Masse und Essig bleiben. Großgeschriebene Abschnitte werden normal geschrieben. Abschaltbar; außerdem im Editor („Umlaute korrigieren“) und für die ganze Datenbank (*Extras*) |
| **Übersetzung mit DeepL (optional)** | fremdsprachige Rezepte (Englisch, Französisch, Italienisch, Spanisch, Niederländisch …) werden beim Import nach Rückfrage mit **DeepL** vollständig ins Deutsche übersetzt – Titel, Zutaten und Zubereitung; deutsche Rezepte werden nie gesendet. Jeder trägt seinen **eigenen API-Schlüssel** ein (kostenloser Tarif *DeepL API Free*, 500.000 Zeichen im Monat), Verbindungstest zeigt den Verbrauch. Ohne Schlüssel oder bei Fehlern übernimmt das Glossar. Auch im Editor und für alle vorhandenen Rezepte (*Extras*) |
| **Englische Rezepte** | Einheiten werden automatisch umgerechnet (cups → ml bzw. g über die Dichte, oz/lb → g, fl oz/pt/qt → ml, tbsp/tsp → EL/TL, stick butter → g, „1 (16 oz) can“ → g), Zutaten und Zusätze über ein **Offline-Glossar** übersetzt („chopped walnuts“ → „Walnüsse, gehackt“), Temperaturen (°F → °C) und Zoll in der Anleitung umgerechnet. Ohne DeepL bleibt die Anleitung selbst englisch (Hinweis in der Ansicht) |
| **Suche** | Rezeptnamen (auch Teilnamen), Zutaten, Tags (UND/ODER), höchstens kcal je Portion, Favoriten, mit Foto |
| **Kühlschrank** | Bestandsliste und Grundvorrat pflegen → passende Rezepte mit „vorhanden x/y“ und fehlenden Zutaten, **Einkaufsliste** (nach Warengruppen, kopieren/drucken/speichern) |
| **Assistent Gewichtskontrolle** | beliebig viele Personen mit Größe, Alter, Aktivität (PAL), Ziel (verlieren/halten/zunehmen), Zielgewicht, Tempo, Ernährungsform, ausgeschlossenen Zutaten und bevorzugten Themen; BMI, Grund- und Gesamtumsatz (Mifflin-St Jeor), Zielzufuhr mit Schutzgrenzen, Dauer bis zum Zielgewicht; **Gewichtsverlauf** mit Grafik |
| **Wochenmenü-Plan** | Frühstück/Mittag/Abend (+ Zwischenmahlzeit) über 1–14 Tage passend zum Kalorienziel, Portionen in Vierteln bemessen, Abwechslung, Vorlieben; einzelne Mahlzeiten neu würfeln oder gezielt wählen; **Einkaufsliste** für die Woche (Vorrat abziehbar), Druck, Speichern im Profil |
| **Website-Export** | themenbasierte **HTML5/PHP-Website**: Startseite, Themenseiten (Tags), Rezeptseiten mit Foto und Nährwerten, Menü, responsives Layout; Suche serverseitig (`suche.php`, PHP 7.4+) und sofort im Browser (JavaScript) – Ordner nur noch hochladen |
| **LaTeX-Export** | einzelnes Rezept als `.tex` für **book** oder **scrbook** – vollständiges Dokument oder Kapitel zum `\input`, mit Foto (JPEG daneben), Zutatentabelle und Nährwerttabelle (booktabs). Mit pdflatex geprüft |
| **Darstellung** | alle Qt-Styles sowie „Dark“ und „Visual Studio Code Dark“ (aus VitaParms/VitaBreathe), sofort umschaltbar; Symbole passen sich hell/dunkel an |
| **Fenster und Bildschirm** | Lage und Größe des Hauptfensters, der Aufteilung und aller Dialoge werden gemerkt (`windowstate`); auf kleinen Bildschirmen werden Dialoge rollbar und in den Bildschirm geholt (`screenfit`); **Schriftgröße automatisch** nach Bildschirmgröße oder fest (85–140 %) |
| **Demo** | `VitaMeals --demo`: temporäre Datenbank mit Beispielrezepten, Bildern, Profil und Vorrat – eigene Daten bleiben unberührt |
| **Windows-Installer** | Qt Installer Framework, Zielordner-Vorgabe `C:\VitaMeals`, Startmenü und Desktop, Dateizuordnung `.vmr`/`.vmdb`, Update über neueren Installer – wird beim Release-Build automatisch erzeugt (siehe [installer/README.md](installer/README.md)) |
| **Kommandozeile / Doppelklick** | `VitaMeals rezept.vmr`, `VitaMeals sammlung.mmf`, `VitaMeals <ordner>/vitameals.vmdb` – Rezepte übernehmen, Sammlungen importieren, Datenbank öffnen |

## Bauen

Benötigt Qt 6.4+ (oder Qt 5.15) mit den Modulen **widgets, printsupport, svg, network** (network nur für DeepL).
Für die HTTPS-Verbindung zu DeepL nutzt Qt 6 unter Windows das systemeigene Schannel (Plugin `tls/qschannelbackend.dll`,
kopiert `windeployqt` automatisch); unter Qt 5.15 müssen zusätzlich die OpenSSL-1.1-DLLs neben `VitaMeals.exe` liegen.

```sh
qmake6 VitaMeals.pro      # bzw. qmake (Qt 5.15)
make -j8                  # Windows: mingw32-make (MinGW) bzw. nmake/jom (MSVC)
```

Getestet mit Qt 6.4.2 unter Linux (GCC 13). Für den MinGW-Build in Qt Creator einfach `VitaMeals.pro` öffnen.
**Windows:** Jeder Build ruft `windeployqt` auf; ein **Release-Build erzeugt zusätzlich den Installer**
`installer/Output/VitaMeals-<Version>-Setup.exe`, sofern das Qt Installer Framework installiert ist –
Einzelheiten in [installer/README.md](installer/README.md).
Weitergabe: `windeployqt VitaMeals.exe` (kopiert auch die deutschen Qt-Übersetzungen, damit Standarddialoge
„Speichern/Abbrechen“ zeigen), macOS `macdeployqt`. Das Programmsymbol liegt als `.ico`, `.icns`, `.png` und `.svg`
in `resources/icons/app/`.

Test des Kerns (Zutatenparser, Einheiten, Glossar, Umlaute, Spracherkennung, Nährwerte, Rezept- und CSV-Importe, USDA, Datenbank,
DeepL gegen einen lokalen Testserver; optional LaTeX-Ausgabe). `./coretest --dump <datei>` zeigt, wie eine Rezeptdatei importiert wird:

```sh
cd tests/coretest && qmake6 && make && ./coretest [ordner-für-tex-dateien]
```

Bildschirmfotos neu erzeugen: `VitaMeals --demo --screenshots docs/images`.

## Aufbau

```
VitaMeals.pro   version.pri   core.pri (Kern ohne Fenster)   desktop.pri (Qt-Widgets-Oberfläche)
resources/      icons/ (SVG → zur Laufzeit eingefärbt), data/lebensmittel.json, samples/ (Beispielrezepte)
src/
  recipe.*            Rezeptmodell, Nährwerte, Eigenformat .vmr (JSON)
  recipedb.*          Rezeptdatenbank als Ordner (Index, Doubletten, Tags)
  units.*             Küchenmaßeinheiten deutsch/englisch/MealMaster/RezKonv, °F→°C, Zoll→cm
  ingredientparser.*  Zutatenzeilen zerlegen (Brüche, Bereiche, Packungsgrößen, Abschnitte)
  glossary.*          Offline-Glossar Englisch → Deutsch, Spracherkennung, Kategorien → Tags
  umlauts.*           Umlaut-Korrektur für ASCII-Umschrift (ae/oe/ue/ss)
  deepl.*             optionale Übersetzung mit der DeepL-API (eigener Schlüssel)
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
  mainwindow.*, recipeview.*, editordialog.*, searchdialog.*, importdialogs.*, exportdialogs.*,
  fridgedialog.*, assistantdialog.*, profiledialog.*, weightchart.*, mealplandialog.*, fooddialog.*,
  settingsdialog.*, uiscale.*             Oberfläche
  themes.*, icons.*, screenfit.*, windowstate.*, qtcompat.h   aus VitaBreathe/VitaParms übernommen
tools/          make_icons.py (Symbole, ICO/ICNS), make_foods.py (Nährwerttabelle), make_installer_images.py
installer/      Qt-Installer-Framework-Installer (windows_deploy.pri, qtifw/config, qtifw/meta)
tests/coretest/ Test des Kerns
```

### Speicherorte

* **Rezeptdatenbank:** frei wählbarer Ordner (zuletzt geöffnete Datenbanken unter *Datei → Zuletzt geöffnet*).
* **Profile, Gewichtsverlauf, eigene Nährwerte:** `profile.json` und `lebensmittel_eigene.json` im
  Anwendungsdatenordner (`QStandardPaths::AppDataLocation`) – getrennt von den Rezepten, damit eine
  Rezeptsammlung weitergegeben werden kann, ohne persönliche Daten mitzugeben.
* **Fensterlagen, Theme, Kühlschrank:** Einstellungen dieses Geräts (`QSettings`).

### Format `.vmr`

```json
{ "format": "VitaMeals-Recipe", "formatVersion": 1, "id": "…", "title": "…", "servings": 4,
  "tags": ["vegetarisch"], "ingredients": [ {"amount": 200, "unit": "g", "name": "Mehl", "food": "weizenmehl", "grams": 200},
  {"heading": "Für die Füllung"} ], "instructions": "…", "image": "<Base64-JPEG>",
  "nutrition": { "total": {…}, "perServing": {"kcal": 450, "kJ": 1883, "protein": 12.5, "fat": 18, "saturatedFat": 9,
  "carbohydrates": 55, "sugar": 20, "fiber": 4, "salt": 0.8}, "matched": 9, "countable": 10, "unmatched": ["…"] } }
```

## Lizenz

© 2026 Michael Bergmann. VitaMeals steht unter der **MIT-Lizenz mit „Commons Clause“ v1.0**
(Wortlaut in [LICENSE](LICENSE)):

* **erlaubt** – kostenlos: das Programm nutzen (privat, im Verein, im Büro), den Quelltext lesen,
  verändern und Veränderungen weitergeben; Weitergabe nur mit diesem Lizenztext.
* **nicht erlaubt:** VitaMeals zu **verkaufen** – also das Programm oder ein Produkt bzw. eine
  Dienstleistung, deren Wert ganz oder im Wesentlichen auf VitaMeals beruht, gegen Entgelt anzubieten
  (auch als kostenpflichtiges Hosting oder kostenpflichtigen Support).

Durch die Commons Clause ist VitaMeals *quelloffen*, aber keine „Open Source“-Software im Sinne der
Open Source Initiative. Die mit dem Windows-Installer ausgelieferten Qt-Bibliotheken stehen weiterhin
unter der LGPL-3.0 und fallen nicht unter die Commons Clause.
