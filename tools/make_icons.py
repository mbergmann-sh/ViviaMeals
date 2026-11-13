#!/usr/bin/env python3
# Erzeugt die SVG-Symbole von ViviaMeals (Programmsymbol + Werkzeugleiste) im Stil von ViviaBreathe:
# Programmsymbol blau mit oranger Akzentfarbe; UI-Symbole als Strichzeichnungen mit den Platzhaltern
# #COLOR (Strich, wird zur Laufzeit an das Theme angepasst) und #ACCENT (Orange).
# Danach: PNG/ICO/ICNS mit rsvg-convert und Pillow (siehe Ende).
import os, subprocess, sys
root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..")
ui = os.path.join(root, "resources/icons/ui"); app = os.path.join(root, "resources/icons/app")
ORANGE = "#E0620D"

APP = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" width="256" height="256">
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#1A82CC"/><stop offset="1" stop-color="#0F67AB"/>
    </linearGradient>
  </defs>
  <rect x="8" y="8" width="240" height="240" rx="48" fill="url(#bg)"/>
  <!-- Gabel -->
  <path d="M40 58 V100 M52 58 V100 M64 58 V100 M40 100 C40 116 64 116 64 100 M52 112 V204" fill="none" stroke="#fff" stroke-width="9" stroke-linecap="round" stroke-linejoin="round"/>
  <!-- Messer -->
  <path d="M206 204 V58 C190 66 186 100 190 132 H206" fill="#fff" stroke="#fff" stroke-width="9" stroke-linecap="round" stroke-linejoin="round"/>
  <!-- Teller -->
  <circle cx="128" cy="138" r="58" fill="#fff"/>
  <circle cx="128" cy="138" r="42" fill="none" stroke="#0F67AB" stroke-width="6"/>
  <!-- Blatt (Akzent) -->
  <path d="M106 160 C102 126 124 104 160 102 C162 138 140 160 106 160 Z" fill="{ORANGE}"/>
  <path d="M110 156 C122 140 134 128 150 114" fill="none" stroke="#fff" stroke-width="5" stroke-linecap="round"/>
</svg>
'''

S = 'fill="none" stroke="#COLOR" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"'
A = 'fill="none" stroke="#ACCENT" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"'
icons = {
 # Datenbank und Datei
 "open": f'<path d="M3 7V5.5A1.5 1.5 0 0 1 4.5 4H9l2 2h8.5A1.5 1.5 0 0 1 21 7.5V9" {S}/><path d="M3 19l2.5-9h17L20 19z" {S}/><path d="M3 7v12" {S}/>',
 "database": f'<ellipse cx="12" cy="5.5" rx="8" ry="3" {S}/><path d="M4 5.5v13c0 1.7 3.6 3 8 3s8-1.3 8-3v-13M4 12c0 1.7 3.6 3 8 3s8-1.3 8-3" {S}/>',
 "database-add": f'<ellipse cx="10" cy="5.5" rx="7" ry="2.7" {S}/><path d="M3 5.5v12c0 1.5 3.1 2.7 7 2.7M17 5.5v5M3 11.5c0 1.5 3.1 2.7 7 2.7" {S}/><path d="M18 14v7M14.5 17.5h7" {A}/>',
 "recipe": f'<path d="M6 3h10l3 3v15H6z" {S}/><path d="M16 3v3h3" {S}/><path d="M9 10h7M9 14h7M9 18h4" {S}/>',
 "recipe-add": f'<path d="M14 21H5V3h9l3 3v6" {S}/><path d="M14 3v3h3" {S}/><path d="M8 9h6M8 13h4" {S}/><path d="M18 14v7M14.5 17.5h7" {A}/>',
 "edit": f'<path d="M4 20h4L19 9l-4-4L4 16z" {S}/><path d="M13.5 6.5l4 4" {A}/>',
 "delete": f'<path d="M4 7h16M10 11v6M14 11v6M6 7l1 14h10l1-14M9 7V4h6v3" {S}/>',
 "search": f'<circle cx="10.5" cy="10.5" r="6.5" {S}/><path d="M15.5 15.5L21 21" {A}/>',
 "import": f'<path d="M14 3H6v18h12V7z" {S}/><path d="M14 3v4h4" {S}/><path d="M12 10v7M8.8 13.8L12 17l3.2-3.2" {A}/>',
 "import-rk": f'<path d="M14 3H6v18h12V7z" {S}/><path d="M14 3v4h4" {S}/><path d="M9 11v6M9 11h2a1.5 1.5 0 0 1 0 3H9M11 14l1.5 3M14 11v6M17 11l-3 3 3 3" {A}/>',
 "import-mm": f'<path d="M14 3H6v18h12V7z" {S}/><path d="M14 3v4h4" {S}/><path d="M8 17v-6l2 3 2-3v6M12.5 17v-6l2 3 2-3v6" {A}/>',
 "web": f'<circle cx="12" cy="12" r="9" {S}/><path d="M3 12h18M12 3c3 3.2 3 14.8 0 18M12 3c-3 3.2-3 14.8 0 18" {S}/><path d="M5.5 7h13M5.5 17h13" {A}/>',
 "odt": f'<path d="M14 3H6v18h12V7z" {S}/><path d="M14 3v4h4" {S}/><path d="M8.5 11h5M8.5 14h7M8.5 17h7" {S}/><rect x="13.5" y="9.5" width="3" height="3" fill="#ACCENT" stroke="none"/>',
 "tex": f'<path d="M14 3H6v18h12V7z" {S}/><path d="M14 3v4h4" {S}/><path d="M8 11h4M10 11v6M12.5 13.5l3 3.5M15.5 13.5l-3 3.5" {A}/>',
 "quit": f'<path d="M15 4h4v16h-4" {S}/><path d="M10 8l-4 4 4 4M6 12h10" {S}/>',
 "scan": f'<path d="M3 14h18v5a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1z" {S}/><path d="M5 14l2-7h10l2 7" {S}/><path d="M2.5 11h19" {A}/><circle cx="17.5" cy="17" r="0.9" fill="#ACCENT" stroke="none"/>',
 "ocr": f'<path d="M3 7V4h3M18 4h3v3M21 17v3h-3M6 20H3v-3" {S}/><path d="M7.5 15.5l2.5-7 2.5 7M8.4 13.2h3.2" {A}/><path d="M14.5 8.5v7h2.2a1.75 1.75 0 0 0 0-3.5H14.5h1.8a1.75 1.75 0 0 0 0-3.5z" {A}/>',
 "image-open": f'<rect x="3" y="4" width="18" height="16" rx="1.5" {S}/><circle cx="8.5" cy="9" r="1.8" {A}/><path d="M3 17l5-5 4 4 3-3 6 6" {S}/>',
 "zoom-in": f'<circle cx="10.5" cy="10.5" r="6.5" {S}/><path d="M15.5 15.5L21 21" {S}/><path d="M10.5 7.5v6M7.5 10.5h6" {A}/>',
 "zoom-out": f'<circle cx="10.5" cy="10.5" r="6.5" {S}/><path d="M15.5 15.5L21 21" {S}/><path d="M7.5 10.5h6" {A}/>',
 "zoom-fit": f'<path d="M4 9V4h5M15 4h5v5M20 15v5h-5M9 20H4v-5" {S}/><rect x="8" y="8" width="8" height="8" rx="1" {A}/>',
 "duplicates": f'<path d="M9 3h8l3 3v11H9z" {S}/><path d="M17 3v3h3" {S}/><path d="M6 7H4v14h11v-2" {S}/><path d="M12 10.5h5M12 13.5h5" {A}/>',
 "check": f'<path d="M4.5 12.5l4.5 4.5L19.5 6.5" {A}/>',
 "close": f'<path d="M6 6l12 12M18 6L6 18" {S}/>',
 # Planung
 "fridge": f'<rect x="5" y="2.5" width="14" height="19" rx="2" {S}/><path d="M5 10h14" {S}/><path d="M8.5 5.5v2M8.5 13v3" {A}/>',
 "scale": f'<path d="M5 21h14a1 1 0 0 0 1-1.2l-2-12A1 1 0 0 0 17 7H7a1 1 0 0 0-1 .8l-2 12A1 1 0 0 0 5 21z" {S}/><circle cx="12" cy="4.5" r="2" {S}/><path d="M9.5 14.5a2.5 2.5 0 0 1 5 0" {A}/><path d="M12 14.5l1.5-2" {A}/>',
 "plan": f'<rect x="3" y="5" width="18" height="16" rx="1.5" {S}/><path d="M3 10h18M8 3v4M16 3v4" {S}/><path d="M7 14h3M14 14h3M7 17.5h3" {A}/>',
 "cart": f'<path d="M3 4h2.5l2.2 10.5a1.5 1.5 0 0 0 1.5 1.2h8.2a1.5 1.5 0 0 0 1.5-1.1L21 8H6.3" {S}/><circle cx="9.5" cy="19.5" r="1.5" {S}/><circle cx="17" cy="19.5" r="1.5" {S}/><path d="M11 11h6" {A}/>',
 "dice": f'<rect x="3.5" y="3.5" width="17" height="17" rx="3" {S}/><circle cx="8.5" cy="8.5" r="1.4" fill="#ACCENT"/><circle cx="15.5" cy="15.5" r="1.4" fill="#ACCENT"/><circle cx="12" cy="12" r="1.4" fill="#ACCENT"/><circle cx="15.5" cy="8.5" r="1.4" fill="#COLOR"/><circle cx="8.5" cy="15.5" r="1.4" fill="#COLOR"/>',
 "nutrition": f'<path d="M12 21c-5 0-8-3.5-8-8 0-4 3-7 6-7 1 0 1.5.4 2 .4s1-.4 2-.4c3 0 6 3 6 7 0 4.5-3 8-8 8z" {S}/><path d="M12 6.4c0-2 1-3.4 3-3.9" {A}/><path d="M8 13a4 4 0 0 1 4-4" {A}/>',
 "user": f'<circle cx="12" cy="8" r="4" {S}/><path d="M4 21c0-4.4 3.6-7 8-7s8 2.6 8 7" {S}/>',
 "diary": f'<path d="M5 3h12a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H5z" {S}/><path d="M8 3v18" {S}/><path d="M11 8h5M11 11.5h5" {S}/><path d="M11.5 16.5c1-1.6 2.2-1.6 3 0" {A}/><circle cx="13" cy="15" r="0.1" {A}/>',
 "user-add": f'<circle cx="10" cy="8" r="4" {S}/><path d="M2 21c0-4.4 3.6-7 8-7 1.6 0 3 .3 4.2.9" {S}/><path d="M19 14v7M15.5 17.5h7" {A}/>',
 "users": f'<circle cx="9" cy="8" r="3.5" {S}/><path d="M2 20c0-3.9 3.1-6 7-6s7 2.1 7 6" {S}/><circle cx="17" cy="7" r="2.8" {S}/><path d="M17 12.5c2.9 0 5 1.8 5 5" {S}/>',
 "chart": f'<path d="M3 3v18h18" {S}/><path d="M6 16l4-5 3 3 6-7" {A}/>',
 # Allgemein
 "settings": f'<path d="M4 6h10M18 6h2M4 12h4M12 12h8M4 18h12M20 18h0" {S}/><circle cx="16" cy="6" r="2" {S}/><circle cx="10" cy="12" r="2" {A}/><circle cx="18" cy="18" r="2" {S}/>',
 "theme": f'<circle cx="12" cy="12" r="9" {S}/><path d="M12 3a9 9 0 0 0 0 18z" fill="#COLOR"/>',
 "help": f'<circle cx="12" cy="12" r="10" {S}/><path d="M9.5 9a2.5 2.5 0 1 1 3.5 2.3c-.7.3-1 .9-1 1.7v.5" {S}/><circle cx="12" cy="17" r="1.1" fill="#COLOR"/>',
 "info": f'<circle cx="12" cy="12" r="10" {S}/><path d="M12 11v6" {S}/><circle cx="12" cy="7.5" r="1.1" fill="#ACCENT"/>',
 "star": f'<path d="M12 3l2.7 5.6 6.1.9-4.4 4.3 1 6.1L12 17l-5.4 2.9 1-6.1-4.4-4.3 6.1-.9z" {S}/>',
 "star-filled": '<path d="M12 3l2.7 5.6 6.1.9-4.4 4.3 1 6.1L12 17l-5.4 2.9 1-6.1-4.4-4.3 6.1-.9z" fill="#ACCENT" stroke="#ACCENT" stroke-width="2" stroke-linejoin="round"/>',
 "print": f'<path d="M6 9V3h12v6" {S}/><rect x="3" y="9" width="18" height="8" rx="1.5" {S}/><path d="M6 14h12v7H6z" {S}/>',
 "pdf": f'<path d="M14 3H6v18h12V7z" {S}/><path d="M14 3v4h4" {S}/><path d="M9 13h6M9 17h4" {A}/>',
 "add": f'<path d="M12 5v14M5 12h14" {S}/>',
 "remove": f'<path d="M5 12h14" {S}/>',
 "image": f'<rect x="3" y="4" width="18" height="16" rx="2" {S}/><circle cx="8.5" cy="9.5" r="1.8" {A}/><path d="M3 17l5-5 4 4 3-3 6 6" {S}/>',
 "translate": f'<path d="M3 5h9M7.5 3v2M5 5c.6 3.2 2.8 6 6 7.5M10 5c-.7 3.5-3 6.4-6.5 8" {S}/><path d="M12.5 21l4-10 4 10M14 17.5h5" {A}/>',
 "umlaut": f'<path d="M6 20V11a4 4 0 0 1 8 0v9M6 15.5h8" {S}/><circle cx="7.5" cy="5" r="1.4" fill="#ACCENT"/><circle cx="12.5" cy="5" r="1.4" fill="#ACCENT"/><path d="M17 13l2 2 3.5-4" {A}/>',
 "eye": f'<path d="M2 12s3.6-7 10-7 10 7 10 7-3.6 7-10 7S2 12 2 12z" {S}/><circle cx="12" cy="12" r="3" {A}/>',
 "tag": f'<path d="M3 12V4h8l10 10-8 8z" {S}/><circle cx="7.5" cy="8.5" r="1.5" fill="#ACCENT"/>',
 "refresh": f'<path d="M20 11a8 8 0 0 0-14.3-4.9L4 8" {S}/><path d="M4 3v5h5" {S}/><path d="M4 13a8 8 0 0 0 14.3 4.9L20 16" {S}/><path d="M20 21v-5h-5" {A}/>',
 "copy": f'<rect x="8" y="8" width="13" height="13" rx="1.5" {S}/><path d="M16 8V4.5A1.5 1.5 0 0 0 14.5 3h-10A1.5 1.5 0 0 0 3 4.5v10A1.5 1.5 0 0 0 4.5 16H8" {S}/>',
 "save": f'<path d="M5 3h11l3 3v15H5z" {S}/><path d="M8 3v5h7V3" {S}/><rect x="8" y="13" width="8" height="6" {A}/>',
 "link": f'<path d="M10 14a4 4 0 0 0 5.7 0l3-3a4 4 0 0 0-5.7-5.7l-1 1" {S}/><path d="M14 10a4 4 0 0 0-5.7 0l-3 3a4 4 0 0 0 5.7 5.7l1-1" {A}/>',
 # Textformat im Editor (0.9.5)
 "format-bold": f'<path d="M7 4h6a4 4 0 0 1 0 8H7zM7 12h7a4 4 0 0 1 0 8H7z" {S.replace('stroke-width="2"', 'stroke-width="2.6"')}/>',
 "format-italic": f'<path d="M10 4h8M6 20h8M15 4L9 20" {S}/>',
 "format-underline": f'<path d="M7 4v7a5 5 0 0 0 10 0V4" {S}/><path d="M5 20h14" {A}/>',
 "list-bullet": f'<path d="M10 6h10M10 12h10M10 18h10" {S}/><circle cx="5" cy="6" r="1.4" fill="#ACCENT"/><circle cx="5" cy="12" r="1.4" fill="#ACCENT"/><circle cx="5" cy="18" r="1.4" fill="#ACCENT"/>',
 "list-number": f'<path d="M10 6h10M10 12h10M10 18h10" {S}/><path d="M4 4.5l1.5-1V8M3.5 10.5a1.5 1.5 0 0 1 3 .5c0 1-3 2.2-3 3.5h3M3.6 16.2h2.7l-1.4 1.6a1.3 1.3 0 1 1-1.3 1.8" {A.replace('stroke-width="2"', 'stroke-width="1.4"')}/>',
 # Saison (Blatt mit Sonne)
 "season": f'<path d="M5 20c0-8 5-13 14-14 0 9-5 14-13 14" {S}/><path d="M5 20c3-4 6-7 10-10" {S}/><circle cx="18.5" cy="17.5" r="2.5" {A}/>',
}

os.makedirs(app, exist_ok=True); os.makedirs(ui, exist_ok=True)
open(os.path.join(app, "viviameals.svg"), "w").write(APP)
for name, body in icons.items():
    open(os.path.join(ui, name + ".svg"), "w").write(
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24">{body}</svg>\n')

# Programmsymbol in PNG, ICO (Windows) und ICNS (macOS)
svg = os.path.join(app, "viviameals.svg")
png = os.path.join(app, "viviameals.png")
subprocess.run(["rsvg-convert", "-w", "256", "-h", "256", svg, "-o", png], check=True)
try:
    from PIL import Image
    big = os.path.join(app, "_1024.png")
    subprocess.run(["rsvg-convert", "-w", "1024", "-h", "1024", svg, "-o", big], check=True)
    im = Image.open(big)
    im.save(os.path.join(app, "viviameals.ico"), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    im.save(os.path.join(app, "viviameals.icns"))
    os.remove(big)
except Exception as e:  # Pillow fehlt – PNG und SVG genügen zum Bauen unter Linux
    print("ICO/ICNS nicht erzeugt:", e)

# Ressourcendatei
qrc = ['<!DOCTYPE RCC>', '<RCC version="1.0">', '    <qresource prefix="/icons">',
       '        <file alias="viviameals.png">icons/app/viviameals.png</file>',
       '        <file alias="viviameals.svg">icons/app/viviameals.svg</file>']
# alle Symbole im Ordner (auch von Hand ergänzte wie sync, network, cloud-down, menu-card)
for name in sorted(f[:-4] for f in os.listdir(ui) if f.endswith(".svg")):
    qrc.append(f'        <file alias="ui/{name}.svg">icons/ui/{name}.svg</file>')
qrc += ['    </qresource>', '</RCC>', '']
open(os.path.join(root, "resources/viviameals.qrc"), "w").write("\n".join(qrc))
print(len(icons), "Symbole")
