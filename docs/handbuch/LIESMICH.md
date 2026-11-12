# ViviaMeals – Handbuch

- `src/NN-<kapitel>.md` – Quelltext (Pandoc-Markdown), ein Kapitel je Datei; Bilder aus `docs/images`
  (`![Text](bilder/<datei>.png)`), Querverweise als `[Text](<kapitel>.md#anker)`.
- Erzeugt (nicht von Hand ändern): `html/` (Webseiten, F1 unter Linux/macOS), `ViviaMeals-Handbuch.pdf`,
  `ViviaMeals-Handbuch.chm` (F1 unter Windows).

Neu erzeugen nach Änderungen an Text, Bildschirmfotos oder Version:

    python3 tools/make_handbuch.py            # alles
    python3 tools/make_handbuch.py --html     # nur HTML (schnell)

Benötigt: pandoc; für das PDF XeLaTeX (TeX Live/MiKTeX mit KOMA-Script) und die DejaVu-Schriften;
für die CHM `chmcmd` (Linux: Paket fp-utils) oder `hhc.exe` (Microsoft HTML Help Workshop, Windows).
Das Skript prüft alle Querverweise und bricht bei fehlenden Ankern oder Bildern ab.
