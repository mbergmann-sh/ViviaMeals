# ViviaMeals Mobil (Android)

Kostenlose App zu ViviaMeals für Telefone und Tablets ab Android 9 (API 28), gebaut für API 36,
nur 64-Bit-ARM (arm64-v8a). Sie nutzt denselben Kern (`../core.pri`) wie das Desktop-Programm.

## Bauen
Ausführlich in `../docs/ViviaMeals-Android-Anleitung.pdf`. Kurz:
1. Qt 6.12 LTS mit Android-Paket, Qt Creator 20.0.2, JDK 21, Android-SDK/NDK (über Qt Creator).
2. `ViviaMealsMobile.pro` öffnen, Kit „Android Qt 6.12.x Clang arm64-v8a“ wählen.
3. Gerät per USB anschließen (USB-Debugging an), Ausführen.

## Am PC ausprobieren
Mit einem Desktop-Kit (Qt ≥ 6.4) läuft die App im Fenster:

    ViviaMeals --size phone        # 400×860 dp, auch: tablet oder BxH
    ViviaMeals --selftest          # Kern und Schnittstelle ohne Oberfläche prüfen
    ViviaMeals --screenshots ordner  # Bildschirmfotos aller Seiten mit Demo-Daten

## Aufbau
- `src/mobileapp.*` – Schnittstelle zwischen Kern und QML (Objekt `app`)
- `src/recipelistmodel.*` – Rezeptliste mit Suche und Themen
- `src/recipeimageprovider.*` – Rezeptfotos (`image://recipe/<id>`)
- `src/platform.*` – Android-Teile: Bildschirm anlassen, Teilen
- `qml/` – Oberfläche (Material-Stil, Vivia-Farben), `Main.qml` ist der Einstieg
- `android/` – Manifest und Startsymbole; alles Übrige ergänzt Qt beim Bauen
- `data/beispielrezepte.vmpack` – 100 Beispielrezepte, erzeugt mit
  `python3 tools/samples/make_samples.py <pfad>/coretest` aus `tools/samples/teil*.py`
