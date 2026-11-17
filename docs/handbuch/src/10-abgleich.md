# Abgleich zwischen Geräten {#abgleich}

Mit dem Abgleich bleibt eine Rezeptdatenbank auf mehreren Geräten gleich – etwa auf dem Rechner in der
Küche und dem Laptop. Abgeglichen werden Rezepte, gespeicherte Menüs, Profile mit Gewichtsverlauf und
Wochenplänen, das Ernährungstagebuch, eigene Lebensmittel, Doubletten-Ausnahmen sowie die Standard-Quelle
und die Liste der „Kein Lebensmittel“-Zutaten.

Jedes Gerät behält eine vollständige Kopie und kann auch ohne Verbindung arbeiten; beim nächsten Abgleich
werden nur die geänderten Dateien übertragen. Nicht abgeglichen werden Dinge, die zum jeweiligen Gerät
gehören: Fenstergrößen, Darstellung, Scanner- und Tesseract-Einstellungen, der DeepL-Schlüssel, der
Kühlschrank-Bestand und Sicherungskopien.

Alle Befehle stehen unter *Datei → Abgleich*.

## Zwei Wege {#abgleich-wege}

### Über die eigene Website {#abgleich-website}

Sie brauchen Webspace mit PHP (Version 7.4 oder neuer, z. B. 8.3) – bei den meisten Hostern
selbstverständlich.

![Abgleich über die eigene Website einrichten](bilder/abgleich-website.png)

1. *Datei → Abgleich → Abgleich einrichten …* öffnen und „die eigene Website“ wählen.
2. **Neu** erzeugt einen zufälligen **Zugangsschlüssel**. Er ist verdeckt; **Anzeigen** zeigt ihn im
   Klartext (zum Notieren), **Verbergen** verdeckt ihn wieder.
3. **Sync-Skript speichern …** legt die Datei `viviameals-sync.php` in einem Ordner Ihrer Wahl ab.
4. Diese Datei per FTP oder SFTP in einen Ordner Ihrer Website hochladen, z. B. `/viviameals/`.
5. Die Adresse eintragen, z. B. `https://www.meine-seite.de/viviameals/viviameals-sync.php`, und
   **Verbindung prüfen**.
6. **OK** – der erste Abgleich lädt die Datenbank hoch.
7. Für jedes weitere Gerät **Einladungscode kopieren** und den Code dort einfügen (er enthält Adresse und
   Schlüssel – bitte nur an eigene Geräte weitergeben).

Das Skript legt neben sich den Ordner `vm-sync-daten` an und sperrt ihn mit einer `.htaccess`-Datei gegen
Zugriffe von außen. Der Zugangsschlüssel steht nur als Prüfsumme im Skript. Wird ein neuer Schlüssel
erzeugt, muss das Skript erneut gespeichert und hochgeladen werden.

> **Wichtig:** Die Daten selbst werden nicht verschlüsselt gespeichert. Verwenden Sie deshalb möglichst
> eine Adresse mit **https://**. Läuft Ihr Webserver mit nginx statt Apache, wirkt die
> `.htaccess`-Sperre nicht – sperren Sie den Ordner `vm-sync-daten` dann in der Serverkonfiguration.

### Über das lokale Netz {#abgleich-lan}

Ein Rechner gibt die Datenbank frei, die anderen gleichen mit ihm ab. Ein Webserver ist nicht nötig.

![Freigabe im lokalen Netz mit Kopplungscode](bilder/abgleich-freigabe.png)

1. Auf dem freigebenden Rechner: *Abgleich einrichten …* → „Freigabe: dieser Rechner stellt die Datenbank
   im lokalen Netz bereit“ → **OK**. Angezeigt werden ein sechsstelliger **Kopplungscode** und die Adresse.
2. Auf dem anderen Gerät: *Abgleich einrichten …* (bzw. *Datenbank aus dem Abgleich laden …*) → „ein
   anderes Gerät im lokalen Netz“. Die Freigabe erscheint in der Liste; den Code eingeben und **Koppeln**,
   dann **OK**.
3. Jeder Code gilt für eine Kopplung; nach fünf Fehlversuchen entsteht ein neuer. Die Kopplung bleibt
   dauerhaft bestehen – auch wenn sich die IP-Adresse des freigebenden Rechners ändert, findet ViviaMeals
   ihn wieder.

Die Freigabe ist aktiv, solange ViviaMeals auf diesem Rechner läuft. Gleicht ein anderes Gerät ab,
übernimmt der freigebende Rechner die Änderungen nach wenigen Sekunden selbst.

> **Firewall:** Beim ersten Start der Freigabe fragt die Windows-Firewall nach – „Private Netzwerke“
> zulassen. ViviaMeals nutzt TCP-Port 47420 und für die Suche UDP-Port 47421.

Bewusst nicht vorgesehen sind Cloud-Dienste und Netzwerk- bzw. NAS-Ordner: dort können gleichzeitige
Änderungen Daten zerstören.

## Wann abgeglichen wird {#abgleich-wann}

- **automatisch** beim Öffnen und Schließen der Datenbank (im Dialog abschaltbar; Vorgabe: an),
- jederzeit mit *Datei → Abgleich → Jetzt abgleichen* (**Strg+Umschalt+Y**) oder „Abgleich“ in der
  Werkzeugleiste.

Die Statuszeile zeigt Weg und Zeitpunkt des letzten Abgleichs, z. B. „⇅ Website · abgeglichen um 14:32“.
Nach dem Abgleich meldet ViviaMeals kurz, was geholt, gesendet, gelöscht oder zusammengeführt wurde.

## Konflikte {#abgleich-konflikte}

Wurde dasselbe Rezept seit dem letzten Abgleich auf zwei Geräten geändert, bleiben **beide Fassungen**
erhalten: die eigene unter dem bisherigen Rezept, die andere als zweites Rezept „Titel (Konflikt: Gerät,
Datum)“. ViviaMeals nennt die betroffenen Rezepte; die überzählige Fassung löschen Sie von Hand – die
Löschung wird ebenfalls abgeglichen.

Profile, Gewichtsverlauf, Tagebuch und eigene Lebensmittel werden **eintragsweise zusammengeführt**:
Einträge beider Geräte bleiben erhalten.

Gelöschte Rezepte werden auf den anderen Geräten ebenfalls gelöscht. Gleichen zwei Geräte genau
gleichzeitig ab, wartet das zweite („Gerade gleicht … ab – bitte in einer Minute erneut versuchen“).

## Ein neues Gerät einrichten {#abgleich-neues-geraet}

Ein frisch installiertes ViviaMeals holt eine schon abgeglichene Datenbank vollständig auf das Gerät:

![Datenbank aus dem Abgleich laden](bilder/abgleich-laden.png)

1. Im Willkommensfenster **Aus dem Abgleich laden …** wählen – oder später *Datei → Abgleich → Datenbank
   aus dem Abgleich laden …*.
2. Weg wählen: eigene Website (Einladungscode einfügen) oder Gerät im lokalen Netz (Freigabe wählen und
   koppeln).
3. Speicherort und Ordnername bestätigen und **Laden**.

Die Datenbank wird mit Profilen, Tagebuch und Lebensmitteln geholt, geöffnet und künftig automatisch
abgeglichen.


## Große Datenbanken und Probleme mit dem Webhoster {#abgleich-gross}

Beim ersten Abgleich oder nach einem großen Import werden viele Dateien übertragen – bei 3000 Rezepten
rund 3000 einzelne Anfragen an das Sync-Skript. Viele Webhoster begrenzen, wie viele Anfragen über eine
Verbindung laufen dürfen, oder brechen bei Last einzelne Anfragen ab. Früher scheiterte der Abgleich dann
mit Meldungen wie „Keine Verbindung: Server stopped accepting new streams …“ und musste von vorn beginnen.

Seit Version 0.9.8 gilt:

- ViviaMeals verwendet für den Abgleich **HTTP/1.1** statt HTTP/2 (die häufigste Ursache dieser Meldung).
- Eine Anfrage, die an der Verbindung scheitert oder mit einem vorübergehenden Serverfehler (429, 500,
  502, 503, 504) beantwortet wird, wird **bis zu viermal wiederholt** – jeweils mit neuer Verbindung und
  kurzer Pause (0,5 bis 4 Sekunden).
- Alle 150 Dateien bzw. jede Minute wird ein **Zwischenstand** eingetragen. Bricht der Abgleich doch ab,
  macht der nächste Versuch dort weiter, statt alles erneut zu senden. Dabei wird auch die Sperre des
  Speichers verlängert, damit kein anderes Gerät dazwischenfunkt.

Das Sync-Skript auf der Website muss dafür **nicht** neu hochgeladen werden.

Wenn es trotzdem hakt:

- **Abgleich einfach erneut starten** – er setzt beim letzten Zwischenstand fort.
- Beim Hoster nachsehen, ob ein **Schutz gegen viele Anfragen** (Rate-Limit, „Bot-Schutz“, Web Application
  Firewall, z. B. ModSecurity) aktiv ist, und die Adresse des Skripts davon ausnehmen.
- Große Importe möglichst **auf einem Gerät** abschließen und erst danach abgleichen; die übrigen Geräte
  holen die Rezepte anschließend in einem Zug.
- Im **lokalen Netz** (anderes Gerät gibt frei) gibt es diese Grenzen nicht – für den allerersten Abgleich
  einer großen Datenbank ist das der schnellste Weg.
