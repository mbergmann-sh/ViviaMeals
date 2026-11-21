#!/usr/bin/env python3
# ViviaMeals – Beispielrezepte für die mobile App erzeugen
#
#   python3 tools/samples/make_samples.py [coretest]
#
# Liest teil*.py (eigene Rezepte), schreibt tools/samples/beispielrezepte.json und erzeugt mit
# „coretest --make-samples“ das Rezeptpaket mobile/data/beispielrezepte.vmpack (Nährwerte, Saison- und
# Ernährungs-Tags rechnet der Kern von ViviaMeals). Zeigt Zutaten ohne Zuordnung an.
import glob, json, os, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
import common
for f in sorted(glob.glob(os.path.join(here, "teil*.py"))):
    exec(open(f, encoding="utf-8").read(), {"R": common.R, "__name__": "teil"})
titles = [r["title"] for r in common.RECIPES]
dups = {t for t in titles if titles.count(t) > 1}
if dups:
    sys.exit("doppelte Titel: %s" % ", ".join(sorted(dups)))
out = os.path.join(here, "beispielrezepte.json")
json.dump(common.RECIPES, open(out, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
print("%d Rezepte → %s" % (len(common.RECIPES), out))
coretest = sys.argv[1] if len(sys.argv) > 1 else None
if coretest:
    pack = os.path.join(here, "..", "..", "mobile", "data", "beispielrezepte.vmpack")
    sys.exit(subprocess.call([coretest, "--make-samples", out, os.path.abspath(pack)]))
