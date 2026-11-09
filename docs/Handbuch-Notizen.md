# VitaMeals – Notizen für das Handbuch

Stichpunkte und fertige Textbausteine für die spätere Anleitung. Stand: Version 0.3.0.

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
