# ViviaMeals – Windows-Installer (Qt Installer Framework)

Ein **Release-Build unter Windows** (Qt Creator oder Konsole, MinGW oder MSVC) erledigt alles
automatisch – gesteuert von `installer/windows_deploy.pri` (von `ViviaMeals.pro` unter Windows
eingebunden) und `installer/make_installer.cmd`:

| Schritt | Ergebnis |
|---|---|
| `windeployqt` nach **jedem** Build | Qt-DLLs, Plugins und deutsche Qt-Übersetzungen neben `ViviaMeals.exe` im Build-Ordner (Start aus Qt Creator) |
| 1. Bereitstellung (nur Release) | `<Build-Ordner>\installer-staging`: **nur** `ViviaMeals.exe` + die von `windeployqt` ermittelten DLLs und Plugins |
| 2. Übersetzungsschritt | `windeployqt --translations de` → `translations\qt_de.qm`; fehlt sie, wird `qt_de.qm` bzw. `qtbase_de.qm` aus der Qt-Installation kopiert (deutsche Knöpfe „Speichern“, „Abbrechen“ …) |
| 3. Aufräumen | Sicherheitsnetz: `*.h`, `*.cpp`, `*.o`, `*.obj`, `*.a`, `*.lib`, `*.pdb`, `*.pro`, `*.qrc` … und Ordner `debug`, `release`, `.moc`, `.obj` werden entfernt; dazu `LICENSE.txt`, `README.md` |
| 4. Paketbaum | `<Build-Ordner>\installer-packages\de.mbergmann.viviameals` (meta + data) |
| 5. Qt Installer Framework | **`installer\Output\ViviaMeals-<Version>-Setup.exe`** |

Außerdem in `installer\Output`:

* `ViviaMeals-<Version>-Inhalt.txt` – Liste aller Dateien im Installer (zur Kontrolle: keine Quelltexte)
* `ViviaMeals-<Version>-Protokoll.txt` – **Ablauf mit allen Meldungen** von windeployqt und binarycreator.
  Entsteht bei jedem Release-Build – auch wenn kein Installer erstellt werden konnte, steht hier, warum.

## Wenn kein Installer entsteht

1. **Release gewählt?** Der Installer wird nur beim Release-Build erstellt. Ein Debug-Build meldet
   „ViviaMeals: Debug-Build – der Installer wird nur beim Release-Build erstellt.“
2. **Protokoll lesen:** `installer\Output\ViviaMeals-<Version>-Protokoll.txt`.
3. **Qt Installer Framework installiert?** Qt Maintenance Tool → *Developer and Designer Tools* →
   *Qt Installer Framework*. Gesucht wird beim qmake-Lauf **und bei jedem Build** in
   `VM_BINARYCREATOR` (Umgebungsvariable mit vollem Pfad zu `binarycreator.exe`), `QTIFWDIR`,
   `<Qt>\Tools\QtInstallerFramework\*\bin`, `C:\Qt\Tools\QtInstallerFramework\*\bin`, `C:\Qt\QtIFW*\bin`
   und im Suchpfad.
4. **Von Hand nachholen:** im Build-Ordner `mingw32-make installer` (MSVC: `nmake installer`) –
   baut bei Bedarf die Release-Fassung und dann den Installer. In Qt Creator: *Projekte → Erstellen →
   Schritt hinzufügen → Make* mit dem Argument `installer`.

Nicht bei jedem Release-Build erzeugen: `qmake CONFIG+=vm_no_installer` (dann nur von Hand wie oben).
Alles wie von windeployqt vorgesehen bereitstellen (Software-OpenGL, D3D-Compiler): `qmake CONFIG+=vm_full_deploy`.

## Handbuch

`make_installer.cmd` legt `docs\handbuch\ViviaMeals-Handbuch.chm`, das PDF und den Ordner `html` nach
`handbuch\` in den Installer (Startmenü: „ViviaMeals – Handbuch“; im Programm F1). Fehlt die CHM-Datei, steht eine
Warnung im Protokoll – erzeugen mit `python tools\make_handbuch.py` (siehe Kopf des Skripts).

## Eigenschaften des Installers

* Zielordner Vorgabe **`C:\ViviaMeals`** (änderbar); Lizenz (PolyForm Noncommercial 1.0.0) und Haftungshinweis
  („kein Medizinprodukt“) zum Annehmen
* Installation auf **Deutsch** (fest über `<Translations>` in `config.xml`)
* Startmenü: ViviaMeals, ViviaMeals – Demo (`--demo`), „ViviaMeals aktualisieren oder entfernen“;
  Desktop-Verknüpfung; am Ende „ViviaMeals starten“
* **Dateizuordnung:** `.vmr` (Rezept übernehmen) und `viviameals.vmdb` (Datenbank öffnen); wird bei der
  Deinstallation entfernt
* Wartungsprogramm `ViviaMeals-Wartung.exe`
* **Update mit einem neueren Installer:** vorhandene Installation im Zielordner wird nach Rückfrage
  entfernt (`purge`) und die neue Version in denselben Ordner installiert. Rezeptdatenbanken, Profile,
  eigene Nährwerte und Einstellungen (`%APPDATA%\mbergmann-sh\ViviaMeals`, Registrierung) bleiben erhalten.
* DeepL (HTTPS): Qt 6 nutzt unter Windows Schannel; `windeployqt` legt dafür `tls\qschannelbackend.dll` bei.
* **Alte Installation „VitaMeals“** (bis 0.5, `C:\VitaMeals`): wird erkannt, der Installer bietet an, sie zu
  entfernen. Daten werden beim ersten Start von ViviaMeals übernommen (die alten bleiben unverändert).
* Rezept scannen: der Windows-Scandialog (WIA) ist Teil von Windows, dafür wird nichts mitgeliefert.

## Texterkennung (Tesseract) mitliefern – optional

Für *Rezept scannen* braucht ViviaMeals das freie Programm **Tesseract** (Apache-2.0). Ohne Weiteres
installiert man es selbst (Installer der [UB Mannheim](https://github.com/UB-Mannheim/tesseract/wiki),
Sprache *German* ankreuzen) – ViviaMeals findet es in `C:\Program Files\Tesseract-OCR`.

Soll der ViviaMeals-Installer es gleich mitbringen, vor dem Release-Build (bzw. vor `mingw32-make installer`)
die Umgebungsvariable setzen – z. B. in Qt Creator unter *Projekte → Build-Umgebung*:

```
VIVIA_TESSERACT_DIR=C:\Program Files\Tesseract-OCR
VIVIA_TESSERACT_LANGS=deu eng osd          (Vorgabe, optional)
```

`make_installer.cmd` kopiert dann nur `tesseract.exe`, die DLLs daneben, die gewählten Sprachdaten
(`tessdata\*.traineddata`), `tessdata\configs` (enthält `tsv`, wird gebraucht) und einen Lizenzhinweis
(`THIRD-PARTY-Tesseract.txt` → `tesseract\LIZENZ-Tesseract.txt`) nach `<Installationsordner>\tesseract\`.
ViviaMeals sucht dort zuerst. Das Protokoll nennt, ob und mit welchen Sprachen Tesseract mitgeliefert wurde.
Der Installer wird dadurch etwa 40–60 MB größer (deu+eng in der „fast“-Fassung deutlich weniger).

## Dateien

```
make_installer.cmd                    Schritte 1–5 (auch von Hand aufrufbar, Aufruf siehe Kopf der Datei)
windows_deploy.pri                    qmake-Teil: windeployqt nach dem Build, Aufruf von make_installer.cmd,
                                      Ziel „installer“
qtifw/config/config.xml.in            Vorlage (qmake trägt die Version ein → config.xml)
qtifw/config/controlscript.qs         Update-Erkennung
qtifw/config/*.png, viviameals.ico     Bilder des Assistenten (tools/make_installer_images.py)
qtifw/meta/package.xml.in             Paketbeschreibung (→ package.xml)
qtifw/meta/license.txt                Lizenz und Haftungshinweis
qtifw/meta/installscript.qs           Verknüpfungen und Dateizuordnungen
THIRD-PARTY-Tesseract.txt             Lizenzhinweis, falls Tesseract mitgeliefert wird
```

**Änderungen bitte in den `.in`-Dateien vornehmen** – qmake schreibt Versionsnummer und Datum aus
`version.pri` bei jedem Lauf in `config.xml` und `package.xml`.

## Stand der Prüfung

`config.xml` und die Skripte sind auf gültiges XML bzw. JavaScript geprüft, die erzeugten Makefiles
mit der Spezifikation `win32-g++` kontrolliert (Release: windeployqt + Installer; Debug: Hinweis;
Ziel `installer`). Ein Lauf auf einem echten Windows-Rechner steht noch aus – bitte bei Problemen
das Protokoll mitschicken.
