# ViviaMealsCheat

Hilfsprogramm zu ViviaMeals (Qt 5.12+ oder Qt 6): ersetzt die Angabe **Quelle** in den Rezepten einer
ViviaMeals-Datenbank. Vorgabe: die Quelle **aller** Rezepte wird zu „Micha B.“.

## Bedienung

1. **Datenbank öffnen …** – im Dateidialog `viviameals.vmdb` im Datenbankordner wählen (ältere Datenbanken:
   `vitameals.vmdb`). Die zuletzt geöffnete Datenbank wird beim nächsten Start wieder geladen.
   Die Liste zeigt alle vorkommenden Quellen mit Anzahl.
2. **Art**
   * *Quelle bei ALLEN Rezepten ersetzen (ohne Suchtext)* – **Vorgabe**; auch Rezepte ohne Quelle
     erhalten die neue Angabe
   * *nur einen bestimmten Text innerhalb der Quelle ersetzen* – z. B. „Gustav Knuth“ → „Micha B.“, auch in
     mehrzeiligen Angaben („Erfasst von Gustav Knuth …“); ein Klick auf eine Quelle in der Liste wählt diese Art
     und übernimmt den Text
   * *nur ersetzen, wenn die ganze Quelle übereinstimmt*
3. **Neue Quelle** (vorbelegt „Micha B.“; leer = Quelle entfernen).
4. Unten steht, wie viele Rezepte betroffen sind, mit Beispiel „alt → neu“.
5. **Ausführen …** → Sicherheitsabfrage („Abbrechen“ ist vorgewählt) → Ausführung.

Die betroffenen Rezeptdateien werden vorher (abschaltbar) nach
`<Datenbank>\sicherung-quelle-<Datum-Uhrzeit>\rezepte\` kopiert; zum Rückgängigmachen zurück nach `rezepte\`
kopieren. Geändert werden nur `source` und `modified` der `.vmr`-Dateien. ViviaMeals sollte dabei geschlossen
sein; geänderte Rezepte erkennt es beim nächsten Öffnen der Datenbank selbst.

## Bauen

    qmake viviamealscheat.pro      (bzw. qmake6)
    make                           (Windows: mingw32-make bzw. nmake)

## Windows-Installer (Qt Installer Framework)

Der Release-Build unter Windows ruft `installer/make_installer.cmd` auf (von Hand: `mingw32-make installer`)
und erzeugt `installer/Output/ViviaMealsCheat-<Version>-Setup.exe` (Protokoll daneben). binarycreator.exe
wird wie bei ViviaMeals gesucht (`VM_BINARYCREATOR`, `QTIFWDIR`, `C:\Qt\Tools\QtInstallerFramework\*` …).

Der Installer
* fragt auf der Seite **ViviaMeals-Ordner** nach dem Ordner von ViviaMeals (Vorgabe `C:\ViviaMeals`) und lässt
  nur weiter, wenn dort `ViviaMeals.exe` liegt,
* kopiert `ViviaMealsCheat.exe` **neben ViviaMeals.exe** und legt im Startmenü-Ordner „ViviaMeals“ eine
  Verknüpfung an,
* legt sein Deinstallationsprogramm in `<ViviaMeals-Ordner>\ViviaMealsCheat\` (`ViviaMealsCheat-Deinstallation.exe`)
  – so bleibt die Installation von ViviaMeals selbst unberührt; eine ältere ViviaMealsCheat-Fassung wird vorher entfernt.

**Wichtig:** Der Installer enthält nur die `.exe`; sie benutzt die Qt-DLLs von ViviaMeals. ViviaMealsCheat
deshalb mit **derselben Qt-Version und demselben Compiler** bauen wie ViviaMeals (der Installer warnt, wenn
z. B. `Qt6Widgets.dll` fehlt). Wird ViviaMeals später neu installiert oder aktualisiert, entfernt dessen
Installer den ganzen Programmordner – ViviaMealsCheat danach einfach erneut installieren.

Programmsymbol: `resources/make_icon.py` (wie ViviaMeals, mit orangem Tausch-Abzeichen).
