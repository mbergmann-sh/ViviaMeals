# VitaMeals – Windows-Installer (Qt Installer Framework)

Ein **Release-Build unter Windows** (Qt Creator oder Konsole, MinGW oder MSVC) erledigt alles
automatisch – gesteuert von `installer/windows_deploy.pri`, das `VitaMeals.pro` unter Windows
einbindet (Aufbau wie bei VitaParms und VitaBreathe):

| Schritt | Ergebnis |
|---|---|
| `windeployqt` nach **jedem** Build | Qt-DLLs, Plugins (Druck, SVG, Bildformate) und die deutschen Qt-Übersetzungen neben `VitaMeals.exe` |
| Programmdateien (nur Release) | `installer/qtifw/data`: `VitaMeals.exe`, DLLs, Plugins, `LICENSE.txt`, `README.md` |
| Paketbaum | `installer/qtifw/packages/de.mbergmann.vitameals` (meta + data) |
| Qt Installer Framework (falls installiert) | `installer/Output/VitaMeals-<Version>-Setup.exe` |

## Eigenschaften

* Zielordner Vorgabe **`C:\VitaMeals`** (änderbar), Lizenz und Haftungshinweis („kein Medizinprodukt“) zum Annehmen
* Installation auf **Deutsch** (fest über `<Translations>` in `config.xml`, unabhängig von der Windows-Sprache)
* Startmenü: VitaMeals, VitaMeals – Demo (`--demo`), „VitaMeals aktualisieren oder entfernen“;
  Desktop-Verknüpfung; am Ende „VitaMeals starten“
* **Dateizuordnung:** Doppelklick auf eine `.vmr`-Datei übernimmt das Rezept in die geöffnete
  Rezeptdatenbank (mit Doubletten-Abfrage), auf `vitameals.vmdb` öffnet die Datenbank.
  Die Zuordnungen werden bei der Deinstallation entfernt.
* Wartungsprogramm `VitaMeals-Wartung.exe`
* **Update mit einem neueren Installer:** Das Steuerskript erkennt eine vorhandene Installation im
  Zielordner, entfernt sie nach Rückfrage über ihr Wartungsprogramm (`purge`) und installiert die
  neue Version in denselben Ordner. Rezeptdatenbanken (eigene Ordner) sowie Profile, eigene
  Nährwerte und Einstellungen (`%APPDATA%\mbergmann-sh\VitaMeals`, Registrierung) bleiben erhalten.

## Werkzeuge

* **Qt Installer Framework 4.x** – im Qt Maintenance Tool unter *Developer and Designer Tools*.
  Gesucht in `<Qt>\Tools\QtInstallerFramework\<neueste Version>\bin` und über `QTIFWDIR`;
  sonst Umgebungsvariable `VM_BINARYCREATOR` auf `binarycreator.exe` setzen.
* Gesucht wird beim **qmake-Lauf** (Qt Creator: *Erstellen → qmake ausführen*); die Meldung
  „Qt Installer Framework gefunden …“ steht in der Ausgabe.
* Installer nicht bei jedem Release-Build erzeugen: `qmake CONFIG+=vm_no_installer`, dann von Hand:
  ```
  binarycreator --offline-only -c qtifw\config\config.xml -p qtifw\packages Output\VitaMeals-Setup.exe
  ```

## Dateien

```
qtifw/config/config.xml.in            Vorlage (qmake trägt die Version ein → config.xml)
qtifw/config/controlscript.qs         Update-Erkennung
qtifw/config/*.png, vitameals.ico     Bilder des Assistenten (tools/make_installer_images.py)
qtifw/meta/package.xml.in             Paketbeschreibung (→ package.xml)
qtifw/meta/license.txt                Lizenz und Haftungshinweis
qtifw/meta/installscript.qs           Verknüpfungen und Dateizuordnungen
```

**Änderungen bitte immer in den `.in`-Dateien vornehmen** – qmake schreibt die Versionsnummer und
das Datum aus `version.pri` bei jedem Lauf in `config.xml` und `package.xml`.

## Hinweise

* `windeployqt` lässt Software-OpenGL und den Direct3D-Compiler weg (reine Widgets-Anwendung).
  Vollständige Bereitstellung: `qmake CONFIG+=vm_full_deploy`.
* VitaMeals braucht keine Netzwerk- oder TLS-Bibliotheken.
* `config.xml` und die Skripte sind auf gültiges XML bzw. JavaScript geprüft, die qmake-Logik mit der
  Spezifikation `win32-g++` durchgespielt. Ein Testlauf auf einem echten Windows-Rechner steht noch aus.
