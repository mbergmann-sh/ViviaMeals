# Exportieren {#export}

Alle Exporte finden Sie unter *Datei → Export*. Einzelne Rezepte lassen sich außerdem über das
Kontextmenü der Rezeptliste (rechte Maustaste) exportieren.

> **Komfortfunktionen:** Der Export als Website, als LaTeX-Datei und als LibreOffice-Writer-Datei steht
> nach der [Registrierung](extras.md#registrierung) zur Verfügung. Rezeptpakete, `.vmr`-Dateien, Drucken
> und der Menüassistent sind immer frei.

## Als Website {#website}

*Datei → Export → Als Website exportieren …* erzeugt aus Ihrer Sammlung eine themenbasierte Website
(HTML5 mit PHP-Suche), die Sie nur noch auf Ihren Webspace hochladen müssen.

- **Titel der Website**, **Untertitel** und **Autor/Betreiber** erscheinen im Kopf und im Impressum-Hinweis.
- **Umfang:** alle Rezepte oder nur die Rezepte der aktuell angezeigten (gefilterten) Liste.
- **Nährwerte anzeigen**, **Zielordner**, **Nach dem Export im Browser öffnen**.

![Website-Export: Reiter „Website“](bilder/website-export.png)

### Impressum und Datenschutz {#website-impressum}

Auf dem Reiter **Impressum & Datenschutz** erfassen Sie die Angaben, die eine deutsche Website braucht:
Name bzw. Betreiber, Straße und Hausnummer, PLZ und Ort, E-Mail-Adresse (Pflicht, mit * markiert), dazu
optional Land, Telefon, eine abweichende verantwortliche Person nach § 18 Abs. 2 MStV (leer = Sie selbst)
und den Webhoster mit Anschrift. Fehlt eine Pflichtangabe, weist ViviaMeals vor dem Export darauf hin.

![Website-Export: Reiter „Impressum & Datenschutz“](bilder/website-impressum.png)

ViviaMeals erzeugt daraus zwei Seiten und verlinkt sie **in der Kopfzeile** (weiß auf Blau, rechts neben
der Suche) und in der Fußzeile jeder Seite:

- **impressum.html** – Angaben nach § 5 DDG, Kontakt, Verantwortlicher nach § 18 Abs. 2 MStV, Hinweise
  zur Haftung für Inhalte (auch: Nährwerte sind Schätzwerte) und Links sowie zum Urheberrecht.
- **datenschutz.html** – Datenschutzhinweise nach DSGVO, zugeschnitten auf diese Website: Sie ist
  statisch, setzt keine Cookies, nutzt kein Tracking und bindet keine fremden Inhalte ein. Beschrieben
  werden die Server-Logdateien beim Hoster, die Suche (im Browser bzw. über `suche.php`), Kontakt per
  E-Mail, Links zu anderen Websites und Ihre Rechte als Betroffene.

Die E-Mail-Adresse steht auf der Website verschleiert im Quelltext, damit einfache Adresssammler sie
nicht lesen. Ihre Angaben speichert ViviaMeals nur auf Ihrem Rechner. Wer die Seiten nicht möchte, nimmt
das Häkchen *Impressum und Datenschutzhinweise erzeugen* heraus.

> **Hinweis:** Die Texte sind Muster nach deutschem Recht und **keine Rechtsberatung**. Prüfen Sie sie
> vor dem Veröffentlichen, besonders wenn Sie die Website erweitern (Kontaktformular, Statistik,
> eingebundene Videos …). Mit Ihrem Webhoster sollte ein Vertrag zur Auftragsverarbeitung
> (Art. 28 DSGVO) bestehen – die meisten Anbieter stellen ihn im Kundenbereich bereit.

Die Website enthält eine Startseite, eine Seite je Thema (Tag) und je Rezept – mit Foto und Nährwerten –,
ein Menü und eine Suche: sofort im Browser (JavaScript) und auf dem Server (`suche.php`, PHP 7.4 oder
neuer). Oben in der Themenspalte stehen auf jeder Seite das ViviaMeals-Bild aus dem Über-Dialog,
„Erstellt mit ViviaMeals“ und ein Link zu portfolio.mbergmann-sh.de; „ViviaMeals“ verweist hier und in der Fußzeile auf die Produktseite von ViviaMeals. Das Layout passt sich Handys und
Tablets an; dort steht das Bild klein neben dem Text.

Bei **sehr vielen Themen** (mehr als 60) zeigen Rezept- und Themenseiten links nur eine kurze Liste: *Alle
Rezepte*, die Themen des Rezepts bzw. das aktuelle Thema und den Link *Alle … Themen*, der zur Übersicht
`themen.html` führt. Die volle Liste steht auf der Startseite und in der Übersicht. Das hält große
Websites klein – bei 4000 Rezepten und 600 Themen etwa 30 statt fast 300 MB – und spart Zeit beim
Hochladen. Der Fortschrittsbalken begleitet den ganzen Export bis zur letzten Seite.

Zum Veröffentlichen laden Sie den Inhalt des
Zielordners per FTP oder SFTP auf Ihren Webspace.

## Als LaTeX-Datei und Kochbuch {#latex}

*Datei → Export → Als LaTeX-Datei exportieren …* (F2) erzeugt aus dem ausgewählten Rezept eine
LaTeX-Datei – für ein eigenes, gesetztes Kochbuch. Der Befehl ist nur aktiv, wenn eine LaTeX-Installation
gefunden wird (TeX Live, MiKTeX oder MacTeX); der Tooltip nennt die gefundene Installation.

![Der LaTeX-Export mit neuem Master-Dokument](bilder/latex-kochbuch.png)

### Ausgabe {#latex-ausgabe}

1. **Kapitel zum Einbinden in ein Master-Dokument** (Vorgabe). Mit „eintragen in“ schreibt ViviaMeals
   `\input{rezepte/<datei>}` in das gewählte Master-Dokument – vor die Zeile
   `% ViviaMeals: weitere Rezepte hier`, sonst vor `\backmatter` bzw. `\end{document}`. Steht das Rezept
   schon darin, wird nichts doppelt eingetragen.
2. **Neues Master-Dokument (Kochbuch) erstellen** und das Rezept eintragen – siehe unten.
3. **Vollständiges Einzeldokument** – nur dieses Rezept.

**Rezept-Optionen:** Foto, Nährwerttabelle, Quelle angeben (Vorgabe aus), Stichwörter für das
Stichwortverzeichnis. **Einbinden als** eigenes Kapitel (`\chapter`) oder als **Abschnitt in einem
Themen-Kapitel** (`\section`) – etwa „Vorspeisen“, „Hauptgerichte“, „Süßspeisen“. ViviaMeals schlägt ein
passendes Kapitel vor und legt es bei Bedarf im Master-Dokument an.

### Ein neues Kochbuch {#latex-kochbuch}

Für ein neues Master-Dokument (`kochbuch.tex`) wählen Sie:

- **Dokumentklasse** scrbook (KOMA-Script) oder book,
- **Buchtitel**, **Untertitel** und **Autor** (Vorgabe: Standard-Quelle), auf Wunsch ohne Datum,
- **Schriftfarbe** für Titel und Untertitel (Vorgabe Schwarz) und einen Schatten,
- ein **Hintergrundbild** für Schmutztitel oder Titelseite – ganze Seite, waagerecht mittig oder frei
  positioniert, zuschneidbar und stufenlos skalierbar; auf Wunsch zusätzlich auf der Titelseite zwischen
  Überschrift und Autor,
- **Inhaltsverzeichnis**, **Glossar**, **Stichwortverzeichnis** und die **Bindekorrektur**
  (Vorgabe 12 mm für Ringbindung).

Das Buch ist zweiseitig gesetzt, mit Kopfzeile (Kapitel und Abschnitt, Linie darunter) und Fußzeile
(Seitenzahl, Linie darüber); Titelseiten bleiben ohne beides. Vor dem Hauptteil steht ein **Vorwort**
(`vorwort.tex`), das ViviaMeals einmal anlegt und danach nie überschreibt – schreiben Sie dort Ihren
eigenen Text hinein. Danach merkt sich ViviaMeals das Master-Dokument; weitere Rezepte werden dort
eingetragen.

![Seiten eines Kochbuchs: Titelseite, Inhaltsverzeichnis und ein Rezept](bilder/latex-kochbuch-seiten.png)

**Übersetzen:** `latexmk -pdf kochbuch.tex` im Ordner des Master-Dokuments (oder dreimal `pdflatex`).
Glossar und Stichwortverzeichnis brauchen keine Zusatzprogramme.

## Als LibreOffice-Writer-Datei {#odt}

*Datei → Export → Als LibreOffice-Writer-Datei (.odt) exportieren …* speichert das ausgewählte Rezept als
Textdokument (A4) – mit der gerade eingestellten Portionenzahl. Titel, rechts daneben das Foto (der Text
fließt links vorbei), Portionen, Energie, Tags, Zutaten, Zubereitung, Nährwerttabelle und Quelle; in der
Fußzeile Rezepttitel und „Seite x von y“. Die Datei lässt sich in LibreOffice, OpenOffice und Microsoft
Word öffnen und weiterbearbeiten.

Menükarten mit mehreren Rezepten erstellt der [Menüassistent](menue.md#menue).

## Rezeptpakete weitergeben {#rezeptpaket}

*Datei → Export → Rezeptdatenbank weitergeben (.vmpack) …* packt Rezepte zum Weitergeben an andere
ViviaMeals-Nutzer – etwa die Familienrezepte für die Kinder.

![Rezeptsammlung weitergeben](bilder/paket-export.png)

- **Welche Rezepte?** alle Rezepte, die angezeigte Liste, nur Favoriten oder Rezepte mit einem Tag.
- **Name der Sammlung**, **Beschreibung** und **Zusammengestellt von** (optional).
- **Mitgeben:** Rezeptfotos (Vorgabe ja), eigene Lebensmittel und Zuordnungen, die in den Rezepten
  vorkommen – damit beim Empfänger dieselben Nährwerte herauskommen (ja), Favoriten-Markierungen (nein).

**Nie enthalten** sind Profile, Ziele, Gewichtsverlauf, Tagebuch, Wochenpläne, Einkaufslisten und
Einstellungen (auch nicht der DeepL-Schlüssel).

**Ein Paket übernehmen:** *Datei → Import → Rezeptpaket (.vmpack) übernehmen …* oder Doppelklick auf die
Datei. Die Vorschau zeigt Name, Beschreibung, Absender und Anzahl der Rezepte. Das Paket lässt sich **in
die geöffnete Datenbank übernehmen** (mit Doubletten-Prüfung) oder **als neue Rezeptdatenbank anlegen**.
Eigene Lebensmittel des Pakets werden ergänzt, ohne vorhandene zu verändern.

![Ein Rezeptpaket übernehmen](bilder/paket-import.png)

> Ein Rezeptpaket ist ein gewöhnliches ZIP-Archiv – es lässt sich auch mit jedem Entpackprogramm öffnen.
