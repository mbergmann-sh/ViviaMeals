# Anhang {#anhang}

## Tastenkürzel {#tastenkuerzel}

| Taste | Befehl |
|---|---|
| **F1** | Handbuch öffnen |
| **F2** | Als LaTeX-Datei exportieren |
| **F5** | Kühlschrank – was koche ich heute? |
| **F6** | Assistent Gewichtskontrolle |
| **F7** | Wochenmenü-Plan |
| **F8** | Gewicht eintragen |
| **F9** | Ernährungstagebuch |
| **Strg+N** | Neues Rezept |
| **Strg+E** | Rezept bearbeiten |
| **Entf** | Rezept löschen (in der Rezeptliste) |
| **Strg+O** | Rezeptdatenbank öffnen |
| **Strg+F** | Suchen |
| **Strg+P** | Drucken |
| **Strg+R** | Zufälliges Rezept |
| **Strg+Umschalt+S** | Rezept scannen |
| **Strg+Umschalt+M** | Menüassistent |
| **Strg+Umschalt+Y** | Jetzt abgleichen |
| **Strg+,** | Einstellungen |
| **Strg+Q** | Programm beenden |

Unter macOS steht **⌘** für **Strg**.

## Dateien und Speicherorte {#dateien}

**Eine Rezeptdatenbank** ist ein Ordner:

| Datei | Inhalt |
|---|---|
| `viviameals.vmdb` | Kennung und Name der Datenbank |
| `rezepte/<id>.vmr` | je Rezept eine Datei (JSON, Foto eingebettet) |
| `menues/<id>.vmmenu` | gespeicherte Menüs |
| `index.json` | Zwischenspeicher für schnelle Listen – wird bei Bedarf neu erzeugt |
| `doubletten-ignoriert.json` | als „keine Doublette“ markierte Rezepte |
| `sicherung-…/` | Sicherungskopien vor dem Leeren der Datenbank |
| `.sync/` | Stand und Verbindung des Abgleichs (bleibt auf diesem Gerät) |

**Persönliche Daten** liegen nicht im Datenbankordner, sondern im Benutzerprofil:

| System | Ordner |
|---|---|
| Windows | `%APPDATA%\mbergmann-sh\ViviaMeals` |
| Linux | `~/.local/share/mbergmann-sh/ViviaMeals` |
| macOS | `~/Library/Application Support/mbergmann-sh/ViviaMeals` |

Dort stehen `profile.json` (Personen, Gewichtsverlauf, Wochenpläne), `tagebuch/<Person>.json` und
`lebensmittel_eigene.json` (eigene Lebensmittel und Zuordnungen). Die Einstellungen speichert Windows in
der Registrierung (`HKEY_CURRENT_USER\Software\mbergmann-sh\ViviaMeals`), Linux in
`~/.config/mbergmann-sh/ViviaMeals.conf`.

> **Sichern:** Kopieren Sie regelmäßig den Datenbankordner und den Ordner mit den persönlichen Daten.

## Kommandozeile und Doppelklick {#kommandozeile}

| Aufruf | Wirkung |
|---|---|
| `ViviaMeals rezept.vmr` | Rezept übernehmen |
| `ViviaMeals sammlung.mmf` bzw. `sammlung.rk` | Sammlung importieren (mit Vorschau) |
| `ViviaMeals paket.vmpack` | Rezeptpaket übernehmen |
| `ViviaMeals <ordner>/viviameals.vmdb` | Datenbank öffnen |
| `ViviaMeals --demo` | Demo mit Beispieldaten in einem eigenen Bereich |

Unter Windows richtet der Installer die Dateizuordnungen ein – ein Doppelklick auf die Datei genügt.

## Häufige Fragen {#faq}

**Die Nährwerte eines Rezepts sind offensichtlich falsch.**
Öffnen Sie das Rezept im Editor und sehen Sie in der Nährwertanalyse nach, welche Zutat ein unerwartetes
Gewicht oder Lebensmittel hat. Oft hilft eine Gewichtsangabe in Klammern („1 Kürbis (800 g)“) oder eine
andere Zuordnung per Doppelklick ([Nährwerte im Editor](rezepte.md#editor-naehrwerte)).

**Der Wochenplan findet kaum passende Rezepte.**
Prüfen Sie, ob die Rezepte Nährwerte und Mahlzeit-Tags haben (*Extras → Mahlzeit- und Nährwert-Tags für
alle Rezepte ergänzen …*) und ob die Vorlieben der Person (Ernährung, „Nicht verwenden“) zu streng sind.
Die Schaltfläche **Gesunde Vorschläge übernehmen** ergänzt Frühstücks- und leichte Rezepte.

**Das Handbuch (.chm) zeigt unter Windows nur leere Seiten.**
Windows sperrt Hilfedateien, die aus dem Internet oder von einem Netzlaufwerk stammen. Rechte Maustaste
auf `ViviaMeals-Handbuch.chm` → *Eigenschaften* → **Zulassen** ankreuzen. Mit dem Installer
eingerichtete Handbücher sind davon nicht betroffen.

**Wie bringe ich meine Rezepte auf einen neuen Rechner?**
Am bequemsten mit dem [Abgleich](abgleich.md#abgleich-neues-geraet). Ohne Abgleich kopieren Sie den
Datenbankordner und den Ordner mit den persönlichen Daten ([Dateien und Speicherorte](#dateien)) und
öffnen die Datenbank mit *Datei → Rezeptdatenbank öffnen …*.

**ViviaMeals hieß früher VitaMeals – sind meine alten Daten noch da?**
Ja. Beim ersten Start übernimmt ViviaMeals Einstellungen, Profile, Tagebücher und eigene Lebensmittel
der alten Fassung; alte Rezeptdatenbanken öffnet es weiterhin.

## Datenschutz {#datenschutz}

ViviaMeals arbeitet vollständig auf Ihrem Rechner. Daten verlassen ihn nur, wenn Sie eine der folgenden
Funktionen ausdrücklich nutzen:

- **DeepL-Übersetzung:** Titel, Zutaten und Zubereitung fremdsprachiger Rezepte gehen an die DeepL SE
  ([Übersetzen mit DeepL](import.md#deepl)).
- **Open Food Facts:** nur der eingegebene Barcode bzw. Suchbegriff.
- **Abgleich:** die abgeglichenen Daten an Ihre eigene Website bzw. an Ihre eigenen Geräte im lokalen Netz.
- **Netzwerkscanner:** die Scan-Anfrage an Ihr eigenes Gerät.

Es gibt keine Werbung, keine Nutzungsstatistik und keine Verbindung zu Servern des Autors.

## Lizenz {#lizenz}

ViviaMeals © 2026 Michael Bergmann steht unter der **PolyForm Noncommercial License 1.0.0**. Die Nutzung
ist für alle nicht-kommerziellen Zwecke kostenlos erlaubt – privat sowie durch gemeinnützige
Organisationen, Schulen und Behörden –, einschließlich Verändern und Weitergeben mit dem Lizenztext.
Kommerzielle Nutzung und Verkauf sind nur mit Erlaubnis des Autors gestattet. Den vollständigen
Lizenztext zeigt *Hilfe → Über ViviaMeals → Details einblenden …*.

ViviaMeals verwendet Qt (LGPL-3.0). Die Texterkennung nutzt das eigenständige Programm Tesseract
(Apache-2.0). Nährwerte stammen überwiegend aus USDA FoodData Central (gemeinfrei); Daten von Open Food
Facts stehen unter der Open Database License (ODbL).
