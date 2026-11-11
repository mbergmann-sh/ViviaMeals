#!/usr/bin/env python3
# Programmsymbol von ViviaMealsCheat: wie ViviaMeals (blaue Kachel, Gabel, Messer, Teller),
# statt des Blatts ein oranges Abzeichen mit zwei Tauschpfeilen („Quelle ersetzen“).
# Erzeugt viviamealscheat.svg/.png/.ico (rsvg-convert + Pillow).
import os, subprocess
from PIL import Image
here = os.path.dirname(os.path.abspath(__file__))
ORANGE = "#E0620D"
SVG = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" width="256" height="256">
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#1A82CC"/><stop offset="1" stop-color="#0F67AB"/>
    </linearGradient>
  </defs>
  <rect x="8" y="8" width="240" height="240" rx="48" fill="url(#bg)"/>
  <path d="M40 58 V100 M52 58 V100 M64 58 V100 M40 100 C40 116 64 116 64 100 M52 112 V204" fill="none" stroke="#fff" stroke-width="9" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M206 204 V58 C190 66 186 100 190 132 H206" fill="#fff" stroke="#fff" stroke-width="9" stroke-linecap="round" stroke-linejoin="round"/>
  <circle cx="128" cy="128" r="58" fill="#fff"/>
  <circle cx="128" cy="128" r="42" fill="none" stroke="#0F67AB" stroke-width="6"/>
  <!-- Abzeichen: Tauschpfeile -->
  <circle cx="176" cy="180" r="50" fill="{ORANGE}" stroke="#fff" stroke-width="8"/>
  <path d="M152 170 C156 152 176 146 192 156" fill="none" stroke="#fff" stroke-width="9" stroke-linecap="round"/>
  <path d="M184 144 L196 158 L178 164" fill="none" stroke="#fff" stroke-width="9" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M200 190 C196 208 176 214 160 204" fill="none" stroke="#fff" stroke-width="9" stroke-linecap="round"/>
  <path d="M168 216 L156 202 L174 196" fill="none" stroke="#fff" stroke-width="9" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
'''
svg = os.path.join(here, "viviamealscheat.svg")
open(svg, "w").write(SVG)
png = os.path.join(here, "viviamealscheat.png")
subprocess.run(["rsvg-convert", "-w", "256", "-h", "256", svg, "-o", png], check=True)
Image.open(png).save(os.path.join(here, "viviamealscheat.ico"),
                     sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
