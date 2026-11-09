#!/usr/bin/env python3
# Erzeugt die Bilder des Qt-IFW-Installers aus dem Programmsymbol (Stil wie VitaBreathe):
#   installer/qtifw/config/banner.png (600×90), watermark.png (164×314), logo.png (200×200),
#   vitameals.png (64×64), vitameals.ico
# Benötigt rsvg-convert; Schrift: DejaVu Sans (bzw. jede serifenlose Schrift).
import os, re, shutil, subprocess, sys, tempfile

root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..")
app_svg = open(os.path.join(root, "resources/icons/app/vitameals.svg"), encoding="utf-8").read()
out = os.path.join(root, "installer/qtifw/config")
os.makedirs(out, exist_ok=True)

# Inhalt des Programmsymbols zum Einbetten (defs-IDs eindeutig machen)
inner = re.sub(r"^.*?<svg[^>]*>|</svg>\s*$", "", app_svg, flags=re.S)
def icon(x, y, size, suffix):
    body = inner.replace('id="bg"', f'id="bg{suffix}"').replace("url(#bg)", f"url(#bg{suffix})")
    return f'<svg x="{x}" y="{y}" width="{size}" height="{size}" viewBox="0 0 256 256">{body}</svg>'

GRAD = '''<linearGradient id="g" x1="0" y1="0" x2="1" y2="{y2}"><stop offset="0" stop-color="#1A82CC"/><stop offset="1" stop-color="#0F67AB"/></linearGradient>'''
FONT = 'font-family="DejaVu Sans, Segoe UI, Arial, sans-serif"'

banner = f'''<svg xmlns="http://www.w3.org/2000/svg" width="600" height="90" viewBox="0 0 600 90">
<defs>{GRAD.format(y2="0")}</defs><rect width="600" height="90" fill="url(#g)"/>
{icon(12, 9, 72, "a")}
<text x="98" y="46" {FONT} font-size="32" font-weight="bold" fill="#fff">VitaMeals</text>
<text x="99" y="72" {FONT} font-size="14" fill="#fff" fill-opacity="0.9">Rezepte · Nährwerte · Gewichtskontrolle</text>
</svg>'''

watermark = f'''<svg xmlns="http://www.w3.org/2000/svg" width="164" height="314" viewBox="0 0 164 314">
<defs>{GRAD.format(y2="1")}</defs><rect width="164" height="314" fill="url(#g)"/>
{icon(22, 36, 120, "b")}
<text x="82" y="196" {FONT} font-size="19" font-weight="bold" fill="#fff" text-anchor="middle">VitaMeals</text>
<path d="M30 292 C26 258 52 236 92 234 C94 270 70 292 30 292 Z" fill="#E0620D"/>
<path d="M36 286 C50 270 64 258 82 246" fill="none" stroke="#fff" stroke-width="4" stroke-linecap="round"/>
<path d="M98 300 C96 278 112 264 138 262 C139 286 124 300 98 300 Z" fill="#E0620D" fill-opacity="0.75"/>
</svg>'''

def render(svg, name, w, h):
    with tempfile.NamedTemporaryFile("w", suffix=".svg", delete=False, encoding="utf-8") as f:
        f.write(svg)
        tmp = f.name
    subprocess.run(["rsvg-convert", "-w", str(w), "-h", str(h), tmp, "-o", os.path.join(out, name)], check=True)
    os.remove(tmp)

render(banner, "banner.png", 600, 90)
render(watermark, "watermark.png", 164, 314)
render(app_svg, "logo.png", 200, 200)
render(app_svg, "vitameals.png", 64, 64)
shutil.copy(os.path.join(root, "resources/icons/app/vitameals.ico"), os.path.join(out, "vitameals.ico"))
print("Installer-Bilder ->", out)
