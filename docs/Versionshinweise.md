# ViviaMeals – Versionshinweise

## Version 1.1.0 (20. November 2026)

### Neu: ViviaMeals Mobil für Android
- Eigene, kostenlose App für Telefone und Tablets (Ordner `mobile/`, Qt Quick, gleicher Kern wie das
  Desktop-Programm). Bau- und Einrichtungsanleitung: `docs/ViviaMeals-Android-Anleitung.pdf`.
- Rezeptliste mit Suche, Themen, Favoriten und „Saisonal“; Rezeptansicht mit Portionen umrechnen,
  Nährwerten und „Bildschirm beim Kochen anlassen“; einfacher Editor mit laufender Nährwertberechnung
- Einkaufsliste (nach Abteilungen, Teilen), Kühlschrank mit Rezeptvorschlägen, Wochenplan vom PC,
  Tagebuch und Gewicht für eine Person, Küchen-Glossar
- Import von Rezeptpaketen (.vmpack) und Rezepten (.vmr); Abgleich mit dem PC über die eigene Website,
  per Einladungscode oder im Heimnetz
- 100 mitgelieferte, eigens geschriebene Beispielrezepte (ohne Fotos), alle mit vollständig berechneten
  Nährwerten; sie werden vor dem ersten Abgleich entfernt (Favoriten bleiben)
- Telefon hochkant: Navigation unten; quer und auf Tablets: Navigation am Rand, Liste und Rezept nebeneinander

### Nährwerte genauer (betrifft auch das Desktop-Programm)
- Einige Zutaten wurden bisher einem falschen Eintrag der Nährwerttabelle zugeordnet, z. B. „Tomaten“ den
  getrockneten Tomaten, „Erbsen“ den getrockneten Erbsen, „Reis“ dem gekochten Reis, „Joghurt“ dem
  griechischen Joghurt. Die Zuordnung prüft solche Zusätze („getrocknet“, „aus der Dose“, „gekocht“ …)
  jetzt gezielt. Beim nächsten Start bietet ViviaMeals an, die Nährwerte aller Rezepte neu zu berechnen –
  bitte zustimmen; Rezepte mit diesen Zutaten erhalten dabei korrigierte Werte.
- Säure-Basen-Fasten: Zutaten aus der Dose werden jetzt auch erkannt, wenn „Dose“ als Einheit steht.

## Version 1.0.1 (18. November 2026)

- Website-Export: „ViviaMeals“ im linken Menü und in der Fußzeile verlinkt auf die Produktseite
  (portfolio.mbergmann-sh.de/article/viviameals-rezeptverwaltung-fuer-windows-download), auch auf der Suchseite.

## Version 1.0.0 (18. November 2026)

Die erste stabile Version. ViviaMeals ist eine deutsche Rezeptverwaltung mit Nährwertanalyse für
Windows, Linux und macOS (Qt 6, auch Qt 5.15). Alle Daten bleiben auf dem eigenen Rechner.

> **Kein Medizinprodukt.** Nährwerte, Energiebedarf und Wochenpläne sind Schätzwerte und ersetzen keine
> ärztliche oder Ernährungsberatung.

### Rezepte verwalten
- Rezeptfenster mit Liste (Schnellfilter, Themen, Favoriten, „Saisonal“) und Rezeptansicht mit Foto,
  Portionen, Kalorien/Kilojoule, Zutaten und Zubereitung; zufälliges Rezept beim Start
- Editor mit Rezeptbild (Zuschneiden), Zutatenliste, formatierbarer Zubereitung und Live-Nährwertanalyse
- Suche nach Titel (auch Teilwörtern), Zutaten und Tags
- Tags: Ernährung (vegan, vegetarisch, Fleisch, Fisch), Mahlzeit, Nährwerte, Saison, Säure-Basen-Fasten
- Doubletten suchen, vergleichen, zusammenführen oder aufräumen; Datenbank prüfen; Wartungslisten für
  Rezepte ohne Nährwerte bzw. Zutaten

### Importieren
- RezKonv und MealMaster (einzeln und als Sammlung, mit Doublettenprüfung), Ambrosia-Datenbanken,
  Rezepte von Webseiten per Adresse, ViviaMeals-Rezeptpakete
- Englische Rezepte: Maße (cups, oz, lb, °F …) werden automatisch in deutsche Küchenmaße umgerechnet,
  Zutaten übersetzt; optional Übersetzung der Zubereitung mit DeepL
- Automatische Umlaut-Korrektur („Kaese“ → „Käse“)

### Nährwerte
- Kalorien, Kilojoule, Eiweiß, Fett, gesättigte Fettsäuren, Kohlenhydrate, Zucker, Ballaststoffe und
  Salz je Rezept und je Portion – automatisch aus der Zutatenliste
- Nährwerttabelle mit über 400 Lebensmittelgruppen (Obst und Südfrüchte, Fisch und Meeresfrüchte,
  Käse, Fleisch und Innereien, spanische Spezialitäten …)
- Eigene Lebensmittel per Assistent, mit Strichcode-Scanner (Kamera oder Foto) und Open Food Facts;
  Nährwerte aus CSV importieren; Abgleich aller Rezepte mit der Lebensmitteldatenbank

### Planen und Gewicht
- Kühlschrank: Rezeptvorschläge aus dem Vorrat mit Einkaufsliste für fehlende Zutaten
- Assistent für Gewichtsverlust und Gewichtszunahme nach eigenen Vorlieben, mit Tipps und Motivation
- Wochenmenü-Plan mit Einkaufsliste, Ernährungstagebuch, Gewichtskontrolle
- Menüassistent für Gäste mit druckfertiger Menükarte

### Weitergeben
- Drucken, Rezeptpakete, Abgleich zwischen mehreren Geräten (eigene Website oder Heimnetz)

### Komfortfunktionen (mit Registrierungsschlüssel)
- Rezepte scannen per Scanner (auch Netzwerkscanner über eSCL/AirScan) oder aus Bildern und Fotos –
  Texterkennung auf dem eigenen Rechner
- Export als Website (HTML5/PHP mit Themenmenü, Suche, Impressum und Datenschutzseite)
- Export als LaTeX-Datei (book und scrbook) und als LibreOffice-Writer-Datei

Registrierung unter *Hilfe → Registrierung eingeben …*; die Prüfung erfolgt offline. Eine
Registrierung bleibt bei Updates erhalten.

### Außerdem
- Küchen-Glossar mit rund 350 Fachbegriffen, Tipp des Tages
- Themen der Vivia-Reihe (hell, dunkel u. a.), passt sich kleinen Bildschirmen an und merkt sich
  Fenstergröße und -position
- Handbuch als Hilfedatei (Windows), HTML und PDF; Demo-Modus zum Ausprobieren
- Windows-Installer (Qt Installer Framework); übernimmt Daten der früheren Fassung „VitaMeals“
