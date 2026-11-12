#!/usr/bin/env python3
# ViviaMeals – Handbuch erzeugen: HTML (mit Navigation), PDF (XeLaTeX) und Windows-Hilfe (.chm)
#
# Quelle:   docs/handbuch/src/NN-<name>.md   (Pandoc-Markdown, ein Kapitel je Datei)
#           Querverweise als  [Text](<name>.md#anker), Bilder als  ![Text](bilder/<datei>.png)
#           (Bilder liegen in docs/images)
# Ausgabe:  docs/handbuch/html/index.html …         Webseiten, im Programm unter Linux/macOS (F1)
#           docs/handbuch/ViviaMeals-Handbuch.pdf     zum Drucken
#           docs/handbuch/ViviaMeals-Handbuch.chm     Windows-Hilfe, im Programm unter Windows (F1)
#
# Benötigt: Python 3, pandoc (≥ 2.11); für das PDF XeLaTeX mit KOMA-Script und den DejaVu-Schriften;
#           für die CHM chmcmd (Free Pascal, Paket fp-utils) oder hhc.exe (Microsoft HTML Help Workshop).
# Aufruf:   python3 tools/make_handbuch.py [--html] [--pdf] [--chm]   (ohne Angabe: alles)
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)

import argparse
import html
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'docs', 'handbuch', 'src')
OUT = os.path.join(ROOT, 'docs', 'handbuch')
IMAGES = os.path.join(ROOT, 'docs', 'images')
APP_ICON = os.path.join(ROOT, 'resources', 'icons', 'app', 'viviameals.png')
TITLE_IMAGE = os.path.join(ROOT, 'resources', 'images', 'about-koch.jpg')
TITLE = 'ViviaMeals – Handbuch'


def version():
    with open(os.path.join(ROOT, 'version.pri'), encoding='utf-8') as f:
        m = re.search(r'^VERSION\s*=\s*(\S+)', f.read(), re.M)
    return m.group(1) if m else '?'


def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode != 0:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit('Fehler bei: ' + ' '.join(cmd[:3]) + ' …')
    return r.stdout


class Chapter:
    def __init__(self, path):
        self.path = path
        base = os.path.splitext(os.path.basename(path))[0]
        self.name = re.sub(r'^\d+-', '', base)            # 03-rezepte → rezepte
        with open(path, encoding='utf-8') as f:
            self.md = f.read()
        m = re.search(r'^#\s+(.+?)(?:\s+\{#([\w-]+)\})?\s*$', self.md, re.M)
        self.title = m.group(1).strip() if m else self.name
        self.anchor = m.group(2) if m and m.group(2) else self.name
        self.html = ''
        self.sections = []                                   # (Ebene, id, Text)


def chapters():
    files = sorted(f for f in os.listdir(SRC) if re.match(r'^\d+-.+\.md$', f))
    return [Chapter(os.path.join(SRC, f)) for f in files]


def images_used(chs):
    used = set()
    for c in chs:
        used.update(re.findall(r'\]\(bilder/([^)\s]+)\)', c.md))
    return sorted(used)


# ---------------------------------------------------------------- HTML

CSS = r"""
:root { --blau: #1474bc; --orange: #e0620d; --text: #222; --grau: #666; --linie: #d9dee3; --hell: #f4f7fa; }
* { box-sizing: border-box; }
body { margin: 0; font-family: "Segoe UI", "Noto Sans", "DejaVu Sans", Arial, sans-serif; color: var(--text); line-height: 1.55; background: #fff; }
a { color: var(--blau); }
header.top { background: var(--blau); color: #fff; padding: 10px 20px; }
header.top a { color: #fff; text-decoration: none; font-size: 1.25em; font-weight: 600; }
header.top img { height: 32px; width: 32px; vertical-align: middle; margin-right: 10px; }
header.top .version { float: right; margin-top: 6px; opacity: .85; font-size: .9em; }
.wrap { display: flex; max-width: 1280px; margin: 0 auto; }
nav.side { flex: 0 0 270px; padding: 18px 14px 40px 20px; border-right: 1px solid var(--linie); background: var(--hell); font-size: .92em; }
nav.side ol { list-style: none; margin: 0; padding: 0; }
nav.side li { margin: 2px 0; }
nav.side li li { margin-left: 14px; font-size: .95em; }
nav.side a { text-decoration: none; color: var(--text); display: block; padding: 2px 6px; border-radius: 3px; }
nav.side a:hover { background: #e3ecf5; }
nav.side li.aktiv > a { background: var(--blau); color: #fff; }
nav.side .such { width: 100%; padding: 5px 8px; margin-bottom: 12px; border: 1px solid var(--linie); border-radius: 4px; }
main { flex: 1; min-width: 0; padding: 10px 36px 40px 36px; max-width: 980px; }
h1 { color: var(--blau); border-bottom: 3px solid var(--orange); padding-bottom: 6px; margin-top: 18px; }
h2 { color: var(--blau); margin-top: 1.8em; border-bottom: 1px solid var(--linie); padding-bottom: 3px; }
h3 { color: #0f5a92; margin-top: 1.4em; }
img { max-width: 100%; height: auto; }
figure { margin: 1.2em 0; text-align: center; }
figure img { border: 1px solid var(--linie); box-shadow: 0 2px 8px rgba(0,0,0,.12); }
figcaption { color: var(--grau); font-size: .9em; margin-top: 6px; font-style: italic; }
table { border-collapse: collapse; margin: 1em 0; width: 100%; font-size: .95em; }
th, td { border: 1px solid var(--linie); padding: 5px 9px; text-align: left; vertical-align: top; }
th { background: var(--hell); }
tr:nth-child(even) td { background: #fafbfc; }
code { background: #f1f3f5; padding: 1px 4px; border-radius: 3px; font-size: .92em; }
pre { background: #f1f3f5; padding: 10px 14px; border-left: 3px solid var(--blau); overflow-x: auto; }
pre code { background: none; padding: 0; }
blockquote { margin: 1.2em 0; padding: 8px 16px; background: #fff4ec; border-left: 4px solid var(--orange); }
blockquote p { margin: .4em 0; }
nav.pager { display: flex; justify-content: space-between; margin-top: 40px; padding-top: 12px; border-top: 1px solid var(--linie); }
nav.pager a { text-decoration: none; }
footer { text-align: center; color: var(--grau); font-size: .85em; padding: 16px; border-top: 1px solid var(--linie); }
.start { overflow: hidden; }
.start img.koch { float: right; width: 230px; margin: 0 0 16px 24px; border: 1px solid var(--linie); }
.inhalt ol { padding-left: 1.4em; }
.inhalt > ol > li { margin: 6px 0; font-weight: 600; }
.inhalt li li { font-weight: normal; }
#treffer li { margin: 4px 0; }
@media (max-width: 820px) {
  .wrap { display: block; }
  nav.side { border-right: none; border-bottom: 1px solid var(--linie); }
  main { padding: 10px 16px 30px 16px; }
  .start img.koch { float: none; display: block; margin: 0 auto 16px auto; }
}
@media print { nav.side, header.top, nav.pager, footer { display: none; } main { max-width: none; } }
/* Windows-Hilfe (CHM, Internet-Explorer-Darstellung): ohne Seitenleiste, ohne Flexbox */
body.chm .wrap { display: block; }
body.chm main { max-width: none; padding: 4px 18px 24px 18px; }
body.chm nav.pager { display: block; overflow: hidden; }
body.chm nav.pager .weiter { float: right; }
"""

SEARCH_JS = r"""
(function () {
  var feld = document.getElementById('suche');
  if (!feld || !window.VM_INDEX) return;
  var main = document.querySelector('main');
  var original = null;
  feld.addEventListener('input', function () {
    var q = feld.value.trim().toLowerCase();
    if (!q) { if (original !== null) { main.innerHTML = original; original = null; } return; }
    if (original === null) original = main.innerHTML;
    var woerter = q.split(/\s+/), treffer = [];
    VM_INDEX.forEach(function (e) {
      var t = (e.t + ' ' + e.x).toLowerCase(), ok = true;
      woerter.forEach(function (w) { if (t.indexOf(w) < 0) ok = false; });
      if (ok) treffer.push(e);
    });
    var h = '<h1>Suche: „' + feld.value.replace(/[<&]/g, '') + '“</h1>';
    if (!treffer.length) h += '<p>Keine Treffer.</p>';
    else h += '<ol id="treffer">' + treffer.slice(0, 60).map(function (e) {
      return '<li><a href="' + e.u + '">' + e.t + '</a> <small>– ' + e.k + '</small></li>'; }).join('') + '</ol>';
    main.innerHTML = h;
  });
})();
"""


def pandoc_html(md):
    return run(['pandoc', '-f', 'markdown+smart', '-t', 'html5', '--wrap=none'], input=md)


def fix_links_html(text):
    # [x](kapitel.md#anker) → kapitel.html#anker
    return re.sub(r'href="([\w-]+)\.md(#[\w-]+)?"', lambda m: 'href="%s.html%s"' % (m.group(1), m.group(2) or ''), text)


def collect_sections(c):
    c.sections = []
    for m in re.finditer(r'<h([23]) id="([^"]+)"[^>]*>(.*?)</h\1>', c.html, re.S):
        c.sections.append((int(m.group(1)), m.group(2), re.sub(r'<[^>]+>', '', m.group(3)).strip()))


def side_nav(chs, current, home=False):
    out = ['<nav class="side"><input id="suche" class="such" type="search" placeholder="Im Handbuch suchen …"><ol>']
    out.append('<li%s><a href="index.html">Start und Inhalt</a></li>' % (' class="aktiv"' if home else ''))
    for c in chs:
        active = c is current
        out.append('<li%s><a href="%s.html">%s</a>' % (' class="aktiv"' if active else '', c.name, html.escape(c.title)))
        if active:
            subs = [s for s in c.sections if s[0] == 2]
            if subs:
                out.append('<ol>' + ''.join('<li><a href="#%s">%s</a></li>' % (sid, html.escape(t)) for _, sid, t in subs) + '</ol>')
        out.append('</li>')
    out.append('</ol></nav>')
    return '\n'.join(out)


def page(title, body, chs, current=None, prev=None, nxt=None, chm=False, home=False):
    ver = version()
    pager = ''
    if prev or nxt:
        pager = '<nav class="pager"><span>%s</span><span class="weiter">%s</span></nav>' % (
            '<a href="%s">← %s</a>' % prev if prev else '', '<a href="%s">%s →</a>' % nxt if nxt else '')
    head = ('<!DOCTYPE html>\n<html lang="de"><head><meta charset="utf-8">'
            + ('<meta http-equiv="X-UA-Compatible" content="IE=edge">' if chm else '<meta name="viewport" content="width=device-width, initial-scale=1">')
            + '<title>%s</title><link rel="stylesheet" href="handbuch.css"></head>\n' % html.escape(title))
    top = ('<header class="top"><span class="version">Version %s</span><a href="index.html"><img src="bilder/viviameals-logo.png" alt="">%s</a></header>\n'
           % (ver, html.escape(TITLE)))
    nav = '' if chm else side_nav(chs, current, home)
    script = '' if chm else '<script src="suchindex.js"></script><script src="suche.js"></script>'
    return (head + '<body class="%s">\n' % ('chm' if chm else 'web') + ('' if chm else top) + '<div class="wrap">' + nav
            + '<main>\n' + body + pager + '</main></div>\n<footer>ViviaMeals %s · © 2026 Michael Bergmann · PolyForm Noncommercial 1.0.0 · kein Medizinprodukt</footer>%s\n</body></html>\n'
            % (ver, script))


def index_body(chs, chm=False):
    toc = ['<div class="inhalt"><h2>Inhalt</h2><ol>']
    for c in chs:
        toc.append('<li><a href="%s.html">%s</a><ol>' % (c.name, html.escape(c.title)))
        toc.extend('<li><a href="%s.html#%s">%s</a></li>' % (c.name, sid, html.escape(t)) for lvl, sid, t in c.sections if lvl == 2)
        toc.append('</ol></li>')
    toc.append('</ol></div>')
    intro = ('<div class="start"><img class="koch" src="bilder/titelbild.jpg" alt="">'
             '<h1>ViviaMeals – Handbuch</h1>'
             '<p><b>Version %s</b></p>'
             '<p>ViviaMeals verwaltet Ihre Rezepte, berechnet Kalorien und Nährwerte, schlägt Gerichte für den Kühlschrankinhalt vor, '
             'plant Wochen und Menüs und hilft beim Abnehmen, Halten oder Zunehmen.</p>'
             '<p>Neu hier? Beginnen Sie mit <a href="willkommen.html">Willkommen bei ViviaMeals</a> und '
             '<a href="hauptfenster.html">Das Hauptfenster</a>. Alle Tastenkürzel stehen im '
             '<a href="anhang.html#tastenkuerzel">Anhang</a>.</p>'
             '<blockquote><p><b>Kein Medizinprodukt.</b> Nährwerte, Energiebedarf und Pläne sind Schätzwerte und ersetzen keine '
             'ärztliche oder Ernährungsberatung.</p></blockquote>%s</div>' % (version(), '' if chm else
             '<p>Dieses Handbuch gibt es auch als <a href="../ViviaMeals-Handbuch.pdf">PDF zum Drucken</a>.</p>'))
    return intro + '\n'.join(toc)


def build_pages(chs, target, chm=False):
    os.makedirs(os.path.join(target, 'bilder'), exist_ok=True)
    for c in chs:
        if not c.html:
            c.html = fix_links_html(pandoc_html(c.md))
            collect_sections(c)
    for i, c in enumerate(chs):
        prev = ('%s.html' % chs[i - 1].name, chs[i - 1].title) if i > 0 else ('index.html', 'Start und Inhalt')
        nxt = ('%s.html' % chs[i + 1].name, chs[i + 1].title) if i + 1 < len(chs) else None
        with open(os.path.join(target, c.name + '.html'), 'w', encoding='utf-8') as f:
            f.write(page('%s – %s' % (c.title, TITLE), c.html, chs, c, prev, nxt, chm))
    with open(os.path.join(target, 'index.html'), 'w', encoding='utf-8') as f:
        f.write(page(TITLE, index_body(chs, chm), chs, None, None, ('%s.html' % chs[0].name, chs[0].title), chm, home=True))
    # CSS-Variablen auflösen: die Windows-Hilfe (Internet-Explorer-Darstellung) kennt keine var(…)
    css = CSS
    for name, value in re.findall(r'(--[\w-]+):\s*([^;]+);', CSS.split('}')[0]):
        css = css.replace('var(%s)' % name, value.strip())
    css = re.sub(r':root\s*\{[^}]*\}\n?', '', css) + 'main, figure, figcaption, nav, header, footer { display: block; }\n'
    with open(os.path.join(target, 'handbuch.css'), 'w', encoding='utf-8') as f:
        f.write(css)
    for img in images_used(chs):
        src = os.path.join(IMAGES, img)
        if not os.path.exists(src):
            raise SystemExit('Bild fehlt: docs/images/' + img)
        shutil.copy2(src, os.path.join(target, 'bilder', img))
    shutil.copy2(APP_ICON, os.path.join(target, 'bilder', 'viviameals-logo.png'))
    shutil.copy2(TITLE_IMAGE, os.path.join(target, 'bilder', 'titelbild.jpg'))
    if not chm:
        # Suchindex: Abschnitte mit ihrem Text
        import json
        entries = []
        for c in chs:
            parts = re.split(r'(<h[123] id="[^"]+"[^>]*>.*?</h[123]>)', c.html, flags=re.S)
            cur_id, cur_t = c.anchor, c.title
            buf = ''
            def flush():
                if cur_t:
                    entries.append({'u': '%s.html#%s' % (c.name, cur_id), 't': cur_t, 'k': c.title,
                                    'x': re.sub(r'\s+', ' ', html.unescape(re.sub(r'<[^>]+>', ' ', buf)))[:4000]})
            for p in parts:
                m = re.match(r'<h[123] id="([^"]+)"[^>]*>(.*?)</h[123]>', p, re.S)
                if m:
                    flush()
                    cur_id, cur_t, buf = m.group(1), re.sub(r'<[^>]+>', '', m.group(2)).strip(), ''
                else:
                    buf += p
            flush()
        with open(os.path.join(target, 'suchindex.js'), 'w', encoding='utf-8') as f:
            f.write('window.VM_INDEX = ' + json.dumps(entries, ensure_ascii=False) + ';\n')
        with open(os.path.join(target, 'suche.js'), 'w', encoding='utf-8') as f:
            f.write(SEARCH_JS)


def check_links(chs):
    ids = {}
    for c in chs:
        ids[c.name] = set(re.findall(r'id="([^"]+)"', c.html))
    problems = []
    for c in chs:
        for m in re.finditer(r'href="([\w-]+)\.html(?:#([\w-]+))?"', c.html):
            page_, anchor = m.group(1), m.group(2)
            if page_ not in ids:
                problems.append('%s: Seite %s fehlt' % (c.name, page_))
            elif anchor and anchor not in ids[page_]:
                problems.append('%s: Anker %s#%s fehlt' % (c.name, page_, anchor))
        for m in re.finditer(r'href="#([\w-]+)"', c.html):
            if m.group(1) not in ids[c.name]:
                problems.append('%s: Anker #%s fehlt' % (c.name, m.group(1)))
    return problems


def build_html(chs):
    target = os.path.join(OUT, 'html')
    if os.path.isdir(target):
        shutil.rmtree(target)
    build_pages(chs, target)
    print('HTML:', os.path.relpath(os.path.join(target, 'index.html'), ROOT))


# ---------------------------------------------------------------- CHM

def cp1252(s):
    return s.encode('cp1252', errors='replace')


def build_chm(chs):
    tool = shutil.which('chmcmd') or shutil.which('hhc') or shutil.which('hhc.exe')
    if not tool:
        for p in (r'C:\Program Files (x86)\HTML Help Workshop\hhc.exe', r'C:\Program Files\HTML Help Workshop\hhc.exe'):
            if os.path.exists(p):
                tool = p
    if not tool:
        print('CHM: übersprungen – weder chmcmd (fp-utils) noch hhc.exe (HTML Help Workshop) gefunden', file=sys.stderr)
        return
    work = tempfile.mkdtemp(prefix='vm-chm-')
    try:
        build_pages(chs, work, chm=True)
        def esc(s):
            return html.escape(s, quote=True)
        def obj(name, local):
            return ('<LI> <OBJECT type="text/sitemap">\n<param name="Name" value="%s">\n<param name="Local" value="%s">\n</OBJECT>\n'
                    % (esc(name), local))
        hhc = ['<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">\n<HTML><HEAD></HEAD><BODY>\n<OBJECT type="text/site properties">\n'
               '<param name="ImageType" value="Folder">\n</OBJECT>\n<UL>\n', obj('Start und Inhalt', 'index.html')]
        for c in chs:
            hhc.append(obj(c.title, c.name + '.html'))
            subs = [s for s in c.sections if s[0] == 2]
            if subs:
                hhc.append('<UL>\n')
                for _, sid, t in subs:
                    hhc.append(obj(t, '%s.html#%s' % (c.name, sid)))
                hhc.append('</UL>\n')
        hhc.append('</UL>\n</BODY></HTML>\n')
        keys = {}
        for c in chs:
            keys.setdefault(c.title, '%s.html' % c.name)
            for _, sid, t in c.sections:
                keys.setdefault(t, '%s.html#%s' % (c.name, sid))
        hhk = ['<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">\n<HTML><HEAD></HEAD><BODY>\n<UL>\n']
        for k in sorted(keys, key=lambda s: s.lower().translate(str.maketrans('äöüß', 'aous'))):
            hhk.append(obj(k, keys[k]))
        hhk.append('</UL>\n</BODY></HTML>\n')
        files = []
        for root, _, names in os.walk(work):
            for n in names:
                if not n.endswith(('.hhp', '.hhc', '.hhk', '.chm')):
                    files.append(os.path.relpath(os.path.join(root, n), work).replace(os.sep, '\\'))
        hhp = ('[OPTIONS]\r\nCompatibility=1.1 or later\r\nCompiled file=ViviaMeals-Handbuch.chm\r\nContents file=toc.hhc\r\n'
               'Index file=index.hhk\r\nDefault topic=index.html\r\nDefault Window=main\r\nDisplay compile progress=No\r\n'
               'Full-text search=Yes\r\nLanguage=0x407 Deutsch (Deutschland)\r\nTitle=%s\r\n\r\n[WINDOWS]\r\n'
               'main="%s","toc.hhc","index.hhk","index.html","index.html",,,,,0x63520,260,0x10184e,[60,40,1180,860],,,,,,,0\r\n\r\n'
               '[FILES]\r\n%s\r\n' % (TITLE, TITLE, '\r\n'.join(sorted(files))))
        with open(os.path.join(work, 'toc.hhc'), 'wb') as f:
            f.write(cp1252(''.join(hhc)))
        with open(os.path.join(work, 'index.hhk'), 'wb') as f:
            f.write(cp1252(''.join(hhk)))
        with open(os.path.join(work, 'handbuch.hhp'), 'wb') as f:
            f.write(cp1252(hhp))
        r = subprocess.run([tool, 'handbuch.hhp'], cwd=work, capture_output=True, text=True, errors='replace')
        chm = os.path.join(work, 'ViviaMeals-Handbuch.chm')
        if not os.path.exists(chm):   # hhc.exe meldet Erfolg mit Rückgabewert 1
            sys.stderr.write(r.stdout + r.stderr)
            raise SystemExit('CHM konnte nicht erzeugt werden')
        shutil.copy2(chm, os.path.join(OUT, 'ViviaMeals-Handbuch.chm'))
        print('CHM: docs/handbuch/ViviaMeals-Handbuch.chm (%d KB)' % (os.path.getsize(chm) // 1024))
    finally:
        shutil.rmtree(work, ignore_errors=True)


# ---------------------------------------------------------------- PDF

LATEX_HEADER = r"""
\usepackage{fontspec}
\setmainfont{DejaVu Serif}[Scale=0.95]
\setsansfont{DejaVu Sans}[Scale=0.95]
\setmonofont{DejaVu Sans Mono}[Scale=0.85]
\newfontfamily\vmsymbol{DejaVu Sans}
\usepackage{newunicodechar}
\newunicodechar{✓}{{\vmsymbol ✓}}
\newunicodechar{⚠}{{\vmsymbol ⚠}}
\newunicodechar{⇅}{{\vmsymbol ⇅}}
\newunicodechar{⌘}{{\vmsymbol ⌘}}
\newunicodechar{ℹ}{{\vmsymbol ℹ}}
\usepackage{xcolor}
\definecolor{vmblau}{HTML}{1474BC}
\definecolor{vmorange}{HTML}{E0620D}
\addtokomafont{disposition}{\color{vmblau}}
\usepackage{float}
\floatplacement{figure}{H}
\usepackage[headsepline,footsepline]{scrlayer-scrpage}
\clearpairofpagestyles
\ihead{\leftmark}
\ohead{ViviaMeals – Handbuch}
\cfoot*{\pagemark}
\automark{chapter}
\usepackage{etoolbox}
\AtBeginEnvironment{quote}{\small}
\setlength{\emergencystretch}{3em}
\RedeclareSectionCommand[tocnumwidth=1.9em]{chapter}
\RedeclareSectionCommand[tocindent=1.9em,tocnumwidth=2.7em]{section}
"""

TITLEPAGE = r"""
\begin{titlepage}
\centering
\vspace*{0mm}
\includegraphics[width=24mm]{%(logo)s}\par\vspace{6mm}
{\sffamily\fontsize{34}{40}\selectfont\color{vmblau} ViviaMeals\par}
\vspace{4mm}
{\sffamily\LARGE Handbuch\par}
\vspace{4mm}
{\color{vmorange}\rule{60mm}{1.2pt}\par}
\vspace{4mm}
{\large Version %(version)s\par}
\vspace{10mm}
\includegraphics[width=64mm]{%(koch)s}\par
\vfill
{\small Rezeptverwaltung mit Nährwertanalyse, Kühlschrank-Vorschlägen, Wochenplänen, Menüassistent\\
und Assistenten für die Gewichtskontrolle -- kein Medizinprodukt.\par}
\vspace{6mm}
{\small © 2026 Michael Bergmann · PolyForm Noncommercial License 1.0.0\par}
\end{titlepage}
"""


def build_pdf(chs):
    engine = shutil.which('xelatex')
    if not engine:
        print('PDF: übersprungen – xelatex nicht gefunden', file=sys.stderr)
        return
    work = tempfile.mkdtemp(prefix='vm-pdf-')
    try:
        parts = []
        for c in chs:
            md = c.md
            md = re.sub(r'\]\([\w-]+\.md#([\w-]+)\)', r'](#\1)', md)            # kapitel.md#anker → #anker
            md = re.sub(r'\]\(([\w-]+)\.md\)', lambda m: '](#%s)' % next((x.anchor for x in chs if x.name == m.group(1)), m.group(1)), md)
            md = md.replace('](bilder/', '](' + IMAGES.replace('\\', '/') + '/')
            parts.append(md)
        with open(os.path.join(work, 'handbuch.md'), 'w', encoding='utf-8') as f:
            f.write('\n\n'.join(parts))
        with open(os.path.join(work, 'kopf.tex'), 'w', encoding='utf-8') as f:
            f.write(LATEX_HEADER)
        with open(os.path.join(work, 'titel.tex'), 'w', encoding='utf-8') as f:
            f.write(TITLEPAGE % {'logo': APP_ICON.replace('\\', '/'), 'koch': TITLE_IMAGE.replace('\\', '/'), 'version': version()})
        run(['pandoc', 'handbuch.md', '-f', 'markdown+smart', '-o', 'handbuch.tex', '-s',
             '--top-level-division=chapter', '--toc', '--toc-depth=2', '-H', 'kopf.tex', '-B', 'titel.tex',
             '-V', 'documentclass=scrreprt', '-V', 'classoption=paper=a4,fontsize=11pt,parskip=half,DIV=12,headings=big', '-N',
             '-V', 'lang=de-DE', '-V', 'colorlinks=true', '-V', 'linkcolor=vmblau', '-V', 'urlcolor=vmblau', '-V', 'toccolor=black',
             '-M', 'title=' + TITLE, '-M', 'pagetitle=' + TITLE, '-V', 'title=', '-V', 'fontfamily='], cwd=work)
        tex = os.path.join(work, 'handbuch.tex')
        with open(tex, encoding='utf-8') as f:
            t = f.read()
        t = t.replace('\\maketitle', '')                                       # eigene Titelseite (titel.tex)
        t = t.replace('\\begin{document}', '\\hypersetup{pdftitle={%s},pdfauthor={Michael Bergmann}}\n\\begin{document}' % TITLE)
        with open(tex, 'w', encoding='utf-8') as f:
            f.write(t)
        for _ in range(3):
            r = subprocess.run([engine, '-interaction=nonstopmode', '-halt-on-error', 'handbuch.tex'], cwd=work, capture_output=True, text=True, errors='replace')
            if r.returncode != 0:
                sys.stderr.write(r.stdout[-4000:])
                raise SystemExit('PDF konnte nicht erzeugt werden (xelatex)')
        shutil.copy2(os.path.join(work, 'handbuch.pdf'), os.path.join(OUT, 'ViviaMeals-Handbuch.pdf'))
        print('PDF: docs/handbuch/ViviaMeals-Handbuch.pdf (%d KB)' % (os.path.getsize(os.path.join(work, 'handbuch.pdf')) // 1024))
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(description='ViviaMeals-Handbuch erzeugen (HTML, PDF, CHM)')
    ap.add_argument('--html', action='store_true')
    ap.add_argument('--pdf', action='store_true')
    ap.add_argument('--chm', action='store_true')
    a = ap.parse_args()
    every = not (a.html or a.pdf or a.chm)
    if not shutil.which('pandoc'):
        raise SystemExit('pandoc wird benötigt (https://pandoc.org)')
    chs = chapters()
    for c in chs:
        c.html = fix_links_html(pandoc_html(c.md))
        collect_sections(c)
    problems = check_links(chs)
    for p in problems:
        print('Verweis:', p, file=sys.stderr)
    if problems:
        raise SystemExit('Bitte die Verweise korrigieren.')
    if every or a.html:
        build_html(chs)
    if every or a.chm:
        build_chm(chs)
    if every or a.pdf:
        build_pdf(chs)


if __name__ == '__main__':
    main()
