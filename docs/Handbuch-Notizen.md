# ViviaMeals – Notizen für das Handbuch

Stichpunkte und fertige Textbausteine für die spätere Anleitung. Stand: Version 0.6.0.

> **Name:** Das Programm heißt seit Version 0.6 **ViviaMeals** (vorher VitaMeals; die ganze Reihe heißt jetzt
> Vivia – ViviaParms, ViviaBreathe). Alte Einstellungen, Profile, Tagebücher, eigene Lebensmittel und
> Rezeptdatenbanken werden beim ersten Start automatisch übernommen bzw. weiter geöffnet.

---

## Fremdsprachige Rezepte mit DeepL übersetzen

### Worum es geht

ViviaMeals versteht englische Rezepte auch ohne Internet: Maße werden umgerechnet (cups, oz, °F …)
und die Zutaten über ein eingebautes Wörterbuch (Glossar) übersetzt. Die Zubereitung bleibt dabei
englisch. Wer Rezepte **vollständig** auf Deutsch haben möchte – Titel, Zutaten und Zubereitung –
oder Rezepte in anderen Sprachen (Französisch, Italienisch, Spanisch, Niederländisch …) importiert,
kann die Übersetzung durch **DeepL** einschalten.

* **Nur fremdsprachige Rezepte** werden übersetzt. Deutsche Rezepte werden nie an DeepL gesendet.
* Die Übersetzung ist **freiwillig** und standardmäßig ausgeschaltet.
* Jeder Benutzer verwendet **seinen eigenen DeepL-Schlüssel**. ViviaMeals enthält keinen Schlüssel.

### Kosten und Lizenz

* Der Tarif **DeepL API Free** ist kostenlos und erlaubt **500.000 Zeichen im Monat**.
  Ein durchschnittliches Rezept hat 1.000–2.500 Zeichen, also reichen 500.000 Zeichen für etwa
  200–400 Rezepte im Monat. Ist das Kontingent aufgebraucht, übersetzt ViviaMeals bis zum Monatsende
  wieder mit dem Glossar.
* Für die Anmeldung erfasst DeepL Zahlungsdaten (zum Schutz vor Missbrauch); der Free-Tarif kostet
  trotzdem nichts.
* Wer mehr braucht, kann *DeepL API Pro* buchen (Grundgebühr plus Preis je Zeichen). ViviaMeals
  erkennt den Tarif am Schlüssel selbst: Free-Schlüssel enden auf `:fx`.

### Einrichten – Schritt für Schritt

1. Auf <https://www.deepl.com/pro-api> den Tarif **DeepL API Free** wählen und ein Konto anlegen.
2. Im DeepL-Konto unter **API Keys** den Schlüssel kopieren (Form `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx:fx`).
3. In ViviaMeals **Extras → Einstellungen** öffnen.
4. Im Abschnitt **Übersetzung fremdsprachiger Rezepte (DeepL)** das Häkchen setzen und den
   Schlüssel einfügen (das Auge-Symbol zeigt den Schlüssel im Klartext).
5. **Verbindung testen** – angezeigt werden Tarif und Verbrauch des laufenden Monats,
   z. B. „✓ Verbunden (DeepL API Free). Diesen Monat verbraucht: 12.345 von 500.000 Zeichen.“
6. Mit **OK** speichern.

*Bildschirmfoto: `docs/images/einstellungen.png`*

### Beim Import

* Nach dem Einlesen prüft ViviaMeals die Sprache jedes Rezepts. Sind fremdsprachige darunter, fragt
  es nach: „3 der 25 Rezepte sind nicht deutschsprachig. Sollen sie mit DeepL ins Deutsche übersetzt
  werden? Dazu werden … Zeichen an DeepL gesendet.“
  * **Ja:** Titel, Zutaten, Zwischenüberschriften und Zubereitung werden übersetzt. Maße und
    Temperaturen werden **vorher** umgerechnet (aus „350 °F“ wird „175 °C“, aus „2 cups flour“ „260 g Mehl“).
  * **Nein:** wie ohne DeepL – Zutaten über das Glossar, Anleitung im Original.
* In der Vorschau zeigt die Spalte **Sprache** z. B. „englisch → DeepL“, „englisch (Glossar)“ oder
  „französisch (nicht übersetzt)“.
* Unter der Zubereitung steht später „Aus dem Englischen übersetzt (DeepL) – Temperaturen und Maße
  in °C und cm.“
* Klappt die Übersetzung nicht (falscher Schlüssel, Kontingent erschöpft, keine Verbindung), wird
  das Rezept trotzdem importiert – mit dem Glossar – und in der Vorschau ein Hinweis angezeigt.
  Nach einem Schlüssel- oder Verbindungsfehler versucht ViviaMeals es bei den übrigen Rezepten des
  Imports nicht erneut.

### Bereits vorhandene Rezepte übersetzen

* **Einzeln:** im Editor auf **Mit DeepL übersetzen** klicken. Das Ergebnis erscheint sofort in den
  Feldern und wird erst mit **Speichern** übernommen.
* **Alle auf einmal:** **Extras → Fremdsprachige Rezepte mit DeepL übersetzen …** – ViviaMeals
  nennt Anzahl und Zeichenmenge und fragt vor dem Senden nach. Nährwerte und Tags werden danach neu
  berechnet.

### Datenschutz (Textbaustein)

> Wenn die DeepL-Übersetzung eingeschaltet ist, sendet ViviaMeals Titel, Zutaten und Zubereitung
> **fremdsprachiger** Rezepte verschlüsselt (HTTPS) an die DeepL SE, Köln. Deutsche Rezepte,
> Fotos, Profile, Gewichtsverlauf und alle übrigen Daten verlassen den Rechner nie. Vor jedem
> Senden fragt ViviaMeals nach. Der API-Schlüssel wird nur in den Einstellungen dieses Rechners
> gespeichert (Windows: Registrierung, `HKEY_CURRENT_USER\Software\mbergmann-sh\ViviaMeals`).
> Beim kostenlosen Tarif **DeepL API Free** behält sich DeepL vor, die Texte für begrenzte Zeit
> zu speichern und zum Verbessern seiner Übersetzungen zu verwenden; nur bei **DeepL API Pro**
> werden sie nach der Übersetzung gelöscht. Für Rezepte ist das in aller Regel unbedenklich –
> persönliche Angaben (Namen, Adressen, Gesundheitsnotizen) gehören dann aber nicht in fremdsprachige
> Rezepte, die übersetzt werden sollen. Maßgeblich sind die Datenschutzhinweise von DeepL.

### Häufige Fragen

* **„Der API-Schlüssel wurde abgelehnt“** – Schlüssel vollständig kopiert, inklusive `:fx`?
  Ein Schlüssel für die DeepL-*App* (Abo „DeepL Pro“ für Privatnutzer) ist kein API-Schlüssel.
* **„Das monatliche Zeichenkontingent ist aufgebraucht“** – bis zum Monatswechsel warten oder Glossar nutzen.
* **„Keine Verbindung“** – Internetverbindung bzw. Firewall/Proxy prüfen.
* **Wird die Übersetzung schlechter, wenn das Rezept schon mit dem Glossar übersetzt wurde?** Nein,
  DeepL erkennt die Sprache selbst; bereits deutsche Zutatennamen bleiben erhalten.

---

## Umlaut-Korrektur

### Worum es geht

Viele ältere Rezeptsammlungen (RezKonv, MealMaster, Mailinglisten der 1990er) sind ohne Umlaute
geschrieben: „Kaese“, „Gemuesebruehe“, „heiss“, „Schuessel“. ViviaMeals erkennt solche Rezepte beim
Import und schreibt sie richtig: „Käse“, „Gemüsebrühe“, „heiß“, „Schüssel“. Dadurch findet auch die
Nährwerttabelle die Zutaten wieder.

### Regeln (für Interessierte)

* **ae, oe, ue → ä, ö, ü** – außer
  * nach einem Selbstlaut bzw. bei „ue“ nach q: Feuer, neue, Sauerbraten, Quelle, Kakaoeis,
  * in bekannten Ausnahmen: zuerst, aktuell, eventuell, Baguette, Duett, Fondue, Oetker, Goethe,
    Paella, Israel, Michael, Aloe, Balsamicoessig, Bioeier …
* **ss → ß** nur in bekannten Wortstämmen: heiß, weiß, groß, gießen, schließen, außen, Fuß, Kloß,
  -mäßig, Soße, Spieß, süß, Spaß, bloß, Grieß, Gefäß, stoßen, beißen … Alles andere bleibt: Wasser,
  Schüssel, Masse, Essig, Nuss, lassen, muss.
* Großgeschriebene Zwischenüberschriften („FUER DEN TEIG“) werden zu „Für den Teig“.
* Ein Rezept wird nur korrigiert, wenn es (fast) keine echten Umlaute enthält – bereits richtig
  geschriebene Rezepte bleiben unangetastet.

### Bedienung

* **Beim Import automatisch** (Einstellung *„Deutsche Rezepte ohne Umlaute korrigieren“*, voreingestellt
  an). Die Vorschau meldet „In 12 Rezepten wurden fehlende Umlaute ergänzt …“.
* **Im Editor:** Schaltfläche **Umlaute korrigieren** – zeigt, wie viele Wörter geändert wurden; erst
  **Speichern** übernimmt die Änderung.
* **Für alle vorhandenen Rezepte:** **Extras → Umlaute in allen Rezepten korrigieren …** – zeigt
  Beispiele („Kaesespaetzle“ → „Käsespätzle“) und fragt vor dem Ändern nach. Nährwerte werden danach
  neu berechnet.
* Seltene Fehlgriffe (z. B. ein Eigenname) lassen sich im Editor von Hand zurücknehmen.

*Bildschirmfoto: `docs/images/import-umlaute.png`*

---

## Nährwerttabelle erweitern

* Neu eingebaut (0.3.0): **Hartkäse** (Bergkäse, Greyerzer/Gruyère, Comté, Appenzeller), Grünkern,
  Rahmjoghurt, Speiseeis (mit Einheit „Kugel“ ≈ 50 g), Ente/Gans, Kaninchen/Hase/Wild, Geflügelleber,
  Sahnesteif, Fruchtsirup (Grenadine), dazu weitere Namen (Morcheln, Nelkenpulver, Piment,
  Borretsch, Sauerampfer, Backobst …).
* Namen werden ohne Rücksicht auf Umlaute und Akzente verglichen: „Gemuesebruehe“ = „Gemüsebrühe“,
  „Gruyere“ = „Gruyère“.
* Zusammengesetzte Wörter mit typischen Endungen werden erkannt: „Zanderfilets“, „Nelkenpulver“,
  „Hasenrücken“, „Korianderkörner“.
* **Eigene Lebensmittel:** *Extras → Nährwerttabelle → Neu …* – oder direkt im Editor eine nicht
  erkannte Zutat doppelt anklicken, Lebensmittel wählen und „für alle Rezepte merken“ ankreuzen.
* **Viele Werte auf einmal:** *Extras → Nährwerttabelle → Importieren / Exportieren* (CSV-Vorlage,
  Open Food Facts, USDA FoodData Central).

---

## Bekannte Grenzen

* Menüfolgen (Kategorie „Menü“, nur Gerichtnamen ohne Mengen) werden als **Menüfolge** angezeigt und
  nicht als Rezept berechnet.
* Die Spracherkennung arbeitet mit häufigen Wörtern; sehr kurze Rezepte gelten im Zweifel als deutsch
  (sie werden also nicht unnötig an DeepL gesendet).

---

## Wochenmenü-Plan (ab 0.4.0)

* **Welche Rezepte wohin?** Frühstück kommt nur aus Rezepten mit dem Tag „frühstück“, die
  Zwischenmahlzeit aus „zwischenmahlzeit“ (oder leichten Frühstücksrezepten bis ca. 320 kcal),
  Mittag- und Abendessen aus Hauptgerichten. Desserts, Kuchen und Grundrezepte (Soßen, Getränke)
  werden nicht als Hauptmahlzeit verplant.
* **Beim Abnehmen** werden Desserts und Süßspeisen gar nicht verplant; bevorzugt werden Rezepte mit
  geringer Energiedichte (kcal je 100 g), wenig Zucker und viel Eiweiß bzw. Ballaststoffen – sie
  machen satter. Beim Zunehmen ist es umgekehrt (energiereichere Rezepte, viel Eiweiß).
* **Aufteilung des Tagesbedarfs:** mit Zwischenmahlzeit 25 % / 35 % / 30 % / 10 %, ohne 30 % / 40 % / 30 %.
* **Portionen in Gramm:** „1 ¼ Portionen (ca. 375 g)“. Das Gewicht ergibt sich aus den Zutaten des
  Rezepts. Höchstmengen je Mahlzeit: etwa 450 g zum Frühstück, 350 g als Zwischenmahlzeit, 800 g mittags
  und abends – damit nicht „1,3 kg Suppe“ eingeplant werden.
* **Zusammenfassung unter dem Plan:** Tagesdurchschnitt von Energie, Eiweiß, Fett, Kohlenhydraten,
  Zucker und Ballaststoffen mit Hinweisen (z. B. „Ballaststoffe unter dem Richtwert von 30 g“) und
  dem frei bleibenden Rest („Platz für ein Stück Obst …“).
* **Fehlen Frühstücks- oder Snack-Rezepte**, steht ein Hinweis unter dem Plan; die Schaltfläche
  **Gesunde Vorschläge übernehmen** fügt zehn mitgelieferte Rezepte hinzu (Porridge mit Beeren,
  Vollkornbrot mit Kräuterquark, Rührei mit Spinat, Joghurt mit Apfel, Vollkornbrot mit
  Tomatenmark-Aufstrich, Apfel mit Mandeln, Gemüsesticks mit Dip, Magerquark mit Beeren, Knäckebrot
  mit Hüttenkäse, gekochtes Ei mit Paprika). Doppelte werden übersprungen.

## Mahlzeit-Tags

* Beim Import (Einstellung „Mahlzeit-Tags ergänzen“), im Editor über **Ergänzen** und für die ganze
  Datenbank über **Extras → Mahlzeit-Tags für alle Rezepte ergänzen …** (mit Vorschau der Anzahl).
* Erkannt werden **frühstück**, **hauptgerichte**, **zwischenmahlzeit**, **desserts**, **süßspeise**
  und **backen** – aus Kategorien, Titel („Porridge“, „Flammeri“, „Gulasch“, „Bauernbrot“),
  Zutaten (Fleisch/Fisch) und Nährwerten (Zuckeranteil, Energie je Portion). Vorspeisen und Salate
  mit wenig Energie gelten als Zwischenmahlzeit.
* Es werden nur Tags **ergänzt**, nie entfernt. Fehlgriffe im Editor einfach löschen.

## Wiegetag und Erinnerung

* Im Assistenten unter **Bearbeiten …** den **Wiegetag** wählen (z. B. Montag).
* Am Wiegetag erscheint im Hauptfenster ein orangefarbener Hinweis „Heute (Montag) ist Wiegetag für …“
  mit **Gewicht eintragen …** und **Erledigt**. Wird der Tag verpasst, bleibt der Hinweis bis zum
  Eintrag stehen („Der Wiegetag am Montag, 09.11. steht noch aus“).
* **Erledigt** blendet die Erinnerung bis zum nächsten Wiegetag aus; ein Gewichtseintrag am oder nach
  dem Wiegetag erledigt sie automatisch.
* Tipp für die Anleitung: immer am selben Wochentag, morgens nach dem Aufstehen, vor dem Frühstück wiegen.

## Motivation und Tipps

* In der **Übersicht** des Assistenten: Fortschritt seit dem ersten Eintrag („Seit dem 10.08. haben
  Sie 5,9 kg abgenommen – weiter so! Bis zum Ziel sind es noch 5,5 kg.“) und eine kurze Erklärung, wie
  Abnehmen, Halten bzw. Zunehmen funktioniert.
* Reiter **Tipps**: Tabelle „statt … lieber …“ mit Effekt (z. B. Butter → Tomatenmark, Sahnesoße →
  Milch mit Speisestärke, Limonade → Schorle 1 : 3) und gute Gewohnheiten nach den DGE-Empfehlungen
  „Gut essen und trinken“ (2024) sowie der WHO-Bewegungsempfehlung. Für Zunehmen eigene Tipps
  (mehrere kleine Mahlzeiten, Nüsse, Krafttraining).

## Rezeptbild zuschneiden

* Nach **Bild wählen …** öffnet sich der Zuschnitt: Rahmen verschieben, an den Ecken die Größe ändern
  oder außerhalb einen neuen Rahmen aufziehen; Seitenverhältnis 4 : 3, 3 : 2, 16 : 9, 1 : 1, 3 : 4
  oder frei; drehen in 90°-Schritten; „Größter Ausschnitt“ setzt zurück.
* Gespeichert wird **nur der Ausschnitt** als PNG (höchstens 800 Pixel breit) im Rezept – das
  Originalbild wird nicht übernommen und bleibt unverändert auf der Festplatte.

*Bildschirmfoto: `docs/images/bild-zuschneiden.png`*

## Export als LibreOffice-Writer-Datei (.odt)

* **Datei → Export → Als LibreOffice-Writer-Datei (.odt) exportieren …** oder im Kontextmenü der
  Rezeptliste. Die eingestellte Portionenzahl wird übernommen.
* Aufbau: Titel, darunter rechts das Rezeptbild (der Text fließt links daran vorbei), Portionen mit
  Gramm, Energie, Tags, Zutaten, Zubereitung, Nährwerttabelle, Quelle. Fußzeile mit Trennlinie:
  Rezepttitel links, „Seite x von y“ rechts. Nach dem Speichern kann die Datei direkt geöffnet werden.
* Die Datei lässt sich in LibreOffice, OpenOffice und Microsoft Word öffnen und weiterbearbeiten.

## Datenbank leeren

* **Datei → Datenbank leeren …** löscht alle Rezepte der geöffneten Datenbank.
* Sicherheitsabfrage: Der Name der Datenbank muss eingetippt werden, erst dann wird die Schaltfläche
  „Alle Rezepte löschen“ aktiv. Voreingestellt wird vorher eine **Sicherungskopie** angelegt
  (Unterordner `sicherung-JJJJ-MM-TT-hhmmss` im Datenbankordner). Wiederherstellen: die `.vmr`-Dateien
  über **Datei → Import → ViviaMeals-Rezept (.vmr)** importieren.

## Lizenz (Textbaustein)

> ViviaMeals steht unter der PolyForm Noncommercial License 1.0.0. Die Nutzung ist für alle
> nicht-kommerziellen Zwecke kostenlos erlaubt – privat sowie durch gemeinnützige Organisationen,
> Schulen und Behörden –, einschließlich Verändern und Weitergeben mit dem Lizenztext. Kommerzielle
> Nutzung und Verkauf sind nur mit Erlaubnis des Autors gestattet.

## Nährwert-Tags (ab 0.4.1)

Beim Import (Einstellung „Nährwert-Tags vergeben“), im Editor über **Ergänzen**, bei *Extras → Nährwerte
aller Rezepte neu berechnen* und bei *Extras → Mahlzeit- und Nährwert-Tags für alle Rezepte ergänzen …*
werden diese Tags aus den berechneten Nährwerten bestimmt:

| Tag | Bedingung (je Portion bzw. je 100 g) |
|---|---|
| eiweißreich | mind. 20 % der Energie aus Eiweiß und mind. 15 g Eiweiß je Portion |
| fettarm | höchstens 3 g Fett je 100 g |
| kalorienarm | höchstens 80 kcal je 100 g und höchstens 400 kcal je Portion |
| hochkalorisch | mind. 250 kcal je 100 g oder mind. 800 kcal je Portion |
| vitaminreich | mind. 200 g Gemüse, Obst und Kräuter je Portion – oder mind. die Hälfte des Portionsgewichts (nicht bei Kuchen/Gebäck; Desserts nur mit mind. 200 g Obst) |
| reduktionsdiät | höchstens 600 kcal je Portion und 150 kcal je 100 g, höchstens 35 % der Energie aus Fett und 20 % aus Zucker, mind. 15 % aus Eiweiß, kein Dessert |

* „eiweißreich“ und „fettarm“ folgen den Grenzwerten der EU-Verordnung über nährwertbezogene Angaben
  (1924/2006). Die übrigen Werte sind an ganze Mahlzeiten angepasst (die EU-Angabe „energiearm“ mit
  40 kcal je 100 g erreichen fast nur Salate und Brühen).
* **vitaminreich** ist ein Näherungswert: Die Nährwerttabelle enthält keine Vitamine; Gemüse und Obst
  sind deren wichtigste Quelle.
* Die Tags gelten nur bei Rezepten, für die mindestens 80 % der Zutaten Nährwerte haben. Nach einer
  Änderung der Zutaten werden nicht mehr zutreffende Nährwert-Tags beim Neuberechnen entfernt.

## Gewicht eintragen (Planen → Gewichtskontrolle, F8)

* Datum (Vorgabe: heute, keine Zukunft) und Person wählen, Gewicht eingeben, **Eintragen** – oder **Abbrechen**.
* Das Gewicht kommt in den Gewichtsverlauf der Person; gibt es für das Datum schon einen Eintrag, wird er
  ersetzt (Hinweis im Dialog).
* Angezeigt werden der letzte Eintrag mit Veränderung (grün in Zielrichtung, orange dagegen), BMI und
  der Abstand zum Zielgewicht.
* Ein Eintrag am oder nach dem Wiegetag erledigt die Wiegetag-Erinnerung; „Gewicht eintragen …“ in der
  Erinnerung öffnet diesen Dialog.
* Ist noch keine Person angelegt, bietet ViviaMeals an, den Assistenten Gewichtskontrolle zu öffnen.

## Ernährungstagebuch (ab 0.5.0)

*Planen → Ernährungstagebuch* (F9), Symbolleiste „Tagebuch“ oder Rechtsklick auf ein Rezept →
„Ins Ernährungstagebuch eintragen …“.

* **Je Person ein eigenes Tagebuch** – die Personen sind die im Assistenten Gewichtskontrolle
  angelegten. Gibt es noch keine, bietet ViviaMeals an, den Assistenten zu öffnen.
* Oben: Person, Tag (◀ ▶, Kalender, „Heute“). Darunter **Ziel · gegessen · noch verfügbar**
  (Restkalorien) mit Balken – orange, wenn das Ziel überschritten ist –, die Nährwerte des Tages
  (Eiweiß, Fett, Kohlenhydrate mit Energieanteil, Zucker, Ballaststoffe, Salz) und unten die letzten
  7 Tage mit Durchschnitt. Das Ziel ist die Zielzufuhr aus dem Assistenten.
* **Aus Rezept …**: Rezept suchen (Name, Zutat, Tag), Datum und Uhrzeit, Mahlzeit (wird nach der
  Uhrzeit vorgeschlagen und lässt sich frei ändern), **gegessene Menge** in Portionen oder Gramm.
  Der Dialog zeigt sofort die Nährwerte und wie viele Kalorien danach noch übrig sind.
* **Eigenes Rezept …**: öffnet den Rezepteditor (Titel, Portionen, Zutaten mit Mengen). Danach wie
  oben Uhrzeit und Menge angeben. **„In die Rezeptdatenbank übernehmen“** speichert die Speise
  zusätzlich als Rezept (mit Mahlzeit- und Nährwert-Tags); sonst bleibt sie nur im Tagebuch und kann
  dort jederzeit über „Rezept bearbeiten …“ geändert werden.
* **Aus Wochenplan**: trägt die für den Tag geplanten Mahlzeiten mit typischen Uhrzeiten ein
  (Frühstück 7:30, Mittag 12:30, Zwischenmahlzeit 15:30, Abend 18:30) – danach anpassen.
* **Bearbeiten** (auch Doppelklick), **Auf heute kopieren** (z. B. das tägliche Frühstück), **Löschen**.
* Beim Eintragen werden Titel, Nährwerte je Portion und Portionsgewicht ins Tagebuch kopiert. Spätere
  Änderungen am Rezept oder das Löschen der Datenbank verändern alte Einträge daher nicht.
* Gespeichert wird in `%APPDATA%\mbergmann-sh\ViviaMeals\tagebuch\<Person>.json`. Beim Löschen einer
  Person im Assistenten wird ihr Tagebuch mit gelöscht.

## Rezept scannen (ab 0.6.0)

### Worum es geht

Rezepte aus Kochbüchern, Zeitschriften oder von Karteikarten lassen sich einscannen oder mit dem Handy
fotografieren. ViviaMeals liest den Text (Texterkennung mit dem freien Programm **Tesseract**, läuft
vollständig auf dem eigenen Rechner), teilt ihn in Titel, Portionen, Zutaten und Zubereitung auf und
öffnet das Ergebnis im Rezepteditor – mit Nährwerten und Tags wie bei einem Import.

### Was man braucht

* **Tesseract** mit deutschen Sprachdaten:
  * Windows: Installer der UB Mannheim (https://github.com/UB-Mannheim/tesseract/wiki), bei
    „Additional language data“ **German** ankreuzen. Wird ggf. auch mit dem ViviaMeals-Installer mitgeliefert.
  * Linux: `sudo apt install tesseract-ocr tesseract-ocr-deu` (Fedora: `tesseract tesseract-langpack-deu`).
  * macOS: `brew install tesseract tesseract-lang`.
  * *Extras → Einstellungen → Scannen → Prüfen* zeigt Version und installierte Sprachen.
* **Zum Scannen** (sonst einfach ein Bild oder Foto öffnen):
  * Windows: nichts – der Scandialog von Windows (WIA) wird benutzt. Jeder Scanner bzw. jedes
    Multifunktionsgerät mit WIA-Treiber funktioniert (bei Windows 10/11 meist automatisch installiert).
  * Linux: **XSane** (`sudo apt install xsane`) – das gescannte Bild wird automatisch übernommen, für
    weitere Seiten einfach erneut scannen. Ohne Oberfläche: `scanimage` (SANE, nimmt den ersten Scanner).
    Mit GNOME-Dokumentenscanner (simple-scan), Skanpage oder Skanlite (KDE) speichert man das Bild selbst
    und lädt es mit „Bild öffnen …“.
  * macOS: „Digitale Bilder“ (Image Capture) oder Vorschau („Ablage → Von Scanner importieren“) –
    Bild speichern und mit „Bild öffnen …“ laden.
  * Überall möglich: **eigener Befehl** mit `%f` als Platzhalter für die Zieldatei (z. B. NAPS2:
    `naps2.console -o %f`).

### Ablauf

1. *Datei → Import → Rezept scannen …* (Strg+Umschalt+S, Werkzeugleiste „Scannen“) – oder
   *Rezept aus Bild oder Foto erkennen …*.
2. Seite scannen bzw. Bild öffnen. **Fotos auf der Seite** werden orange markiert und bei der
   Texterkennung ausgelassen (Bilder erzeugen sonst Buchstabensalat). Falsch markiert? Anklicken
   entfernt die Markierung; ein nicht erkanntes Foto mit der Maus aufziehen. Danach „Text erkennen“.
3. Das größte Foto wird als **Rezeptbild** vorgeschlagen; „Aus der Seite zuschneiden …“ wählt einen
   anderen Ausschnitt, „Kein Rezeptbild“ verwirft es.
4. Rechts Titel, Portionen, Zutaten und Zubereitung prüfen und korrigieren. Was nicht zugeordnet werden
   konnte (Einleitung, Nährwertangaben, Schwierigkeit), steht im Reiter „Nicht zugeordnet“, der
   komplette erkannte Text unter „Erkannter Text“.
5. **Mehrseitige Rezepte:** weitere Seiten scannen oder öffnen – sie werden angehängt.
6. „Im Editor öffnen“: Der Rezepteditor zeigt den Scan rechts als **Vorlage** (zoombar, Strg+Mausrad);
   nach „Speichern“ steht das Rezept in der Datenbank. Der Scan selbst wird nicht gespeichert.

### Tipps für gute Ergebnisse (Textbaustein)

* Mit **300 dpi** scannen; Farbe nur, wenn das Foto mit übernommen werden soll.
* Handyfotos: Seite flach hinlegen, gerade von oben, ohne Schatten und bei gutem Licht.
* Die Aufteilung richtet sich nach Überschriften („Zutaten“, „Zubereitung“, „So wird’s gemacht“),
  Mengenangaben am Zeilenanfang und der Schriftgröße des Titels. Typische Lesefehler werden
  verbessert (l/2 → ½, 1OO g → 100 g, El → EL, 180 *C → 180 °C, fehlende Umlaut-Punkte wie
  „Krauterquark“ → „Kräuterquark“). Handschrift erkennt Tesseract kaum.

### Grenzen

* Zwei Rezepte auf einer Seite werden als eines erkannt – die überzähligen Zeilen im Editor löschen
  oder die Seite für das zweite Rezept erneut öffnen.
* Stark gestaltete Seiten (Text auf Fotos, farbige Hintergründe) erkennt Tesseract schlechter.
* Der Windows-Scandialog ist auf Windows beschränkt; unter Linux/macOS wird ein Scanprogramm gestartet.
