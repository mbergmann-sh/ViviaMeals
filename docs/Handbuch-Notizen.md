# VitaMeals – Notizen für das Handbuch

Stichpunkte und fertige Textbausteine für die spätere Anleitung. Stand: Version 0.4.0.

---

## Fremdsprachige Rezepte mit DeepL übersetzen

### Worum es geht

VitaMeals versteht englische Rezepte auch ohne Internet: Maße werden umgerechnet (cups, oz, °F …)
und die Zutaten über ein eingebautes Wörterbuch (Glossar) übersetzt. Die Zubereitung bleibt dabei
englisch. Wer Rezepte **vollständig** auf Deutsch haben möchte – Titel, Zutaten und Zubereitung –
oder Rezepte in anderen Sprachen (Französisch, Italienisch, Spanisch, Niederländisch …) importiert,
kann die Übersetzung durch **DeepL** einschalten.

* **Nur fremdsprachige Rezepte** werden übersetzt. Deutsche Rezepte werden nie an DeepL gesendet.
* Die Übersetzung ist **freiwillig** und standardmäßig ausgeschaltet.
* Jeder Benutzer verwendet **seinen eigenen DeepL-Schlüssel**. VitaMeals enthält keinen Schlüssel.

### Kosten und Lizenz

* Der Tarif **DeepL API Free** ist kostenlos und erlaubt **500.000 Zeichen im Monat**.
  Ein durchschnittliches Rezept hat 1.000–2.500 Zeichen, also reichen 500.000 Zeichen für etwa
  200–400 Rezepte im Monat. Ist das Kontingent aufgebraucht, übersetzt VitaMeals bis zum Monatsende
  wieder mit dem Glossar.
* Für die Anmeldung erfasst DeepL Zahlungsdaten (zum Schutz vor Missbrauch); der Free-Tarif kostet
  trotzdem nichts.
* Wer mehr braucht, kann *DeepL API Pro* buchen (Grundgebühr plus Preis je Zeichen). VitaMeals
  erkennt den Tarif am Schlüssel selbst: Free-Schlüssel enden auf `:fx`.

### Einrichten – Schritt für Schritt

1. Auf <https://www.deepl.com/pro-api> den Tarif **DeepL API Free** wählen und ein Konto anlegen.
2. Im DeepL-Konto unter **API Keys** den Schlüssel kopieren (Form `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx:fx`).
3. In VitaMeals **Extras → Einstellungen** öffnen.
4. Im Abschnitt **Übersetzung fremdsprachiger Rezepte (DeepL)** das Häkchen setzen und den
   Schlüssel einfügen (das Auge-Symbol zeigt den Schlüssel im Klartext).
5. **Verbindung testen** – angezeigt werden Tarif und Verbrauch des laufenden Monats,
   z. B. „✓ Verbunden (DeepL API Free). Diesen Monat verbraucht: 12.345 von 500.000 Zeichen.“
6. Mit **OK** speichern.

*Bildschirmfoto: `docs/images/einstellungen.png`*

### Beim Import

* Nach dem Einlesen prüft VitaMeals die Sprache jedes Rezepts. Sind fremdsprachige darunter, fragt
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
  Nach einem Schlüssel- oder Verbindungsfehler versucht VitaMeals es bei den übrigen Rezepten des
  Imports nicht erneut.

### Bereits vorhandene Rezepte übersetzen

* **Einzeln:** im Editor auf **Mit DeepL übersetzen** klicken. Das Ergebnis erscheint sofort in den
  Feldern und wird erst mit **Speichern** übernommen.
* **Alle auf einmal:** **Extras → Fremdsprachige Rezepte mit DeepL übersetzen …** – VitaMeals
  nennt Anzahl und Zeichenmenge und fragt vor dem Senden nach. Nährwerte und Tags werden danach neu
  berechnet.

### Datenschutz (Textbaustein)

> Wenn die DeepL-Übersetzung eingeschaltet ist, sendet VitaMeals Titel, Zutaten und Zubereitung
> **fremdsprachiger** Rezepte verschlüsselt (HTTPS) an die DeepL SE, Köln. Deutsche Rezepte,
> Fotos, Profile, Gewichtsverlauf und alle übrigen Daten verlassen den Rechner nie. Vor jedem
> Senden fragt VitaMeals nach. Der API-Schlüssel wird nur in den Einstellungen dieses Rechners
> gespeichert (Windows: Registrierung, `HKEY_CURRENT_USER\Software\mbergmann-sh\VitaMeals`).
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
geschrieben: „Kaese“, „Gemuesebruehe“, „heiss“, „Schuessel“. VitaMeals erkennt solche Rezepte beim
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
  über **Datei → Import → VitaMeals-Rezept (.vmr)** importieren.

## Lizenz (Textbaustein)

> VitaMeals steht unter der PolyForm Noncommercial License 1.0.0. Die Nutzung ist für alle
> nicht-kommerziellen Zwecke kostenlos erlaubt – privat sowie durch gemeinnützige Organisationen,
> Schulen und Behörden –, einschließlich Verändern und Weitergeben mit dem Lizenztext. Kommerzielle
> Nutzung und Verkauf sind nur mit Erlaubnis des Autors gestattet.
