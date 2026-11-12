# ---------------------------------------------------------------------------
#  ViviaMeals – Rezeptverwaltung mit Nährwertanalyse und Gewichtskontrolle
#  Copyright (c) 2026 Michael Bergmann – PolyForm Noncommercial License 1.0.0 (siehe LICENSE)
#
#  KEIN Medizinprodukt – ViviaMeals ist eine persönliche Rezept- und Ernährungsverwaltung.
#
#  Build:   qmake6 ViviaMeals.pro && make        (Linux/macOS)
#           qmake6 ViviaMeals.pro && nmake/jom   (Windows, MSVC) bzw. mingw32-make (MinGW)
#  Benötigt Qt 6.4 oder neuer bzw. Qt 5.15 mit den Modulen: widgets printsupport svg
#  Technisch und optisch an ViviaParms und ViviaBreathe angelehnt.
# ---------------------------------------------------------------------------

lessThan(QT_MAJOR_VERSION, 5) | equals(QT_MAJOR_VERSION, 5):lessThan(QT_MINOR_VERSION, 15) {
    error("ViviaMeals braucht Qt 5.15 oder Qt 6.4 und neuer (gefunden: $$[QT_VERSION])")
}

QT       += core gui widgets printsupport svg network
CONFIG   += c++17
TEMPLATE  = app
TARGET    = ViviaMeals

include(version.pri)
DEFINES  += VIVIAMEALS_VERSION=\\\"$$VERSION\\\"

# Gemeinsamer Kern (Rezeptmodell, Datenbank, Importe, Exporte, Nährwerte, Planung)
include(core.pri)
# Desktop-Oberfläche (Qt Widgets)
include(desktop.pri)

RESOURCES += resources/viviameals.qrc resources/data.qrc

# Handbuch (F1): wird im Quellbaum gefunden, solange das Programm aus dem Build-Ordner läuft;
# erzeugt mit tools/make_handbuch.py (docs/handbuch: html/, .pdf, .chm). Siehe src/manual.cpp.
# (nur ohne Leerzeichen im Pfad – qmake trennt DEFINES an Leerzeichen; sonst greifen die übrigen Suchorte)
equals(PWD, $$replace(PWD, " ", "")): DEFINES += VIVIAMEALS_SOURCE_DIR=\\\"$$PWD\\\"
VM_HANDBUCH = $$PWD/docs/handbuch
unix:!macx {
    # make install: Programm nach <PREFIX>/bin, Handbuch nach <PREFIX>/share/viviameals/handbuch
    isEmpty(PREFIX): PREFIX = /usr/local
    target.path = $$PREFIX/bin
    handbuch.path = $$PREFIX/share/viviameals/handbuch
    handbuch.files = $$VM_HANDBUCH/html $$VM_HANDBUCH/ViviaMeals-Handbuch.pdf
    INSTALLS += target handbuch
}
macx {
    # im App-Bundle unter Contents/Resources/handbuch
    vm_handbuch.files = $$VM_HANDBUCH/html $$VM_HANDBUCH/ViviaMeals-Handbuch.pdf
    vm_handbuch.path = Contents/Resources/handbuch
    QMAKE_BUNDLE_DATA += vm_handbuch
}

win32: RC_ICONS = resources/icons/app/viviameals.ico
macx:  ICON     = resources/icons/app/viviameals.icns

# Programminformationen für Windows (Eigenschaften der .exe)
QMAKE_TARGET_COMPANY     = Michael Bergmann
QMAKE_TARGET_PRODUCT     = ViviaMeals
QMAKE_TARGET_DESCRIPTION = ViviaMeals – Rezeptverwaltung mit Nährwertanalyse
QMAKE_TARGET_COPYRIGHT   = (c) 2026 Michael Bergmann, PolyForm Noncommercial 1.0.0 (nicht kommerziell)

# Windows-Installer: Versionsnummer und Datum in die Installer-Dateien übernehmen
# (*.in → Datei ohne .in; @VM_VERSION@, @VM_RELEASE_DATE@). Läuft auf allen Plattformen,
# damit die erzeugten Dateien im Quellbaum immer zur Programmversion passen (wie in ViviaBreathe).
VM_TEMPLATES = installer/qtifw/config/config.xml.in \
               installer/qtifw/meta/package.xml.in
for(t, VM_TEMPLATES) {
    vm_in = $$cat($$PWD/$$t, blob)
    vm_in = $$replace(vm_in, @VM_VERSION@, $$VERSION)
    vm_in = $$replace(vm_in, @VM_RELEASE_DATE@, $$VM_RELEASE_DATE)
    vm_out = $$PWD/$$replace(t, \.in$, )
    vm_cur = $$cat($$vm_out, blob)
    !equals(vm_cur, $$vm_in$$escape_expand(\\n)): write_file($$vm_out, vm_in)
}

# Windows: DLLs bereitstellen und Qt-Installer-Framework-Installer (C:\ViviaMeals)
win32: include(installer/windows_deploy.pri)

OTHER_FILES += tools/make_handbuch.py installer/README.md installer/windows_deploy.pri \
    installer/qtifw/config/config.xml.in installer/qtifw/config/controlscript.qs \
    installer/qtifw/meta/package.xml.in installer/qtifw/meta/installscript.qs installer/qtifw/meta/license.txt
