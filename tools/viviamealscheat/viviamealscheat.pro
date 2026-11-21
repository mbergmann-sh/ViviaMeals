# ViviaMealsCheat – ersetzt die Angabe „Quelle“ in allen Rezepten einer ViviaMeals-Datenbank
# Bauen:  qmake viviamealscheat.pro && make        (Qt 5.12+ oder Qt 6; Windows: mingw32-make bzw. nmake)
# Windows-Release-Build erzeugt zusätzlich installer/Output/ViviaMealsCheat-<Version>-Setup.exe
# (Qt Installer Framework, siehe installer/make_installer.cmd).
QT += widgets
CONFIG += c++17
TARGET = ViviaMealsCheat
VERSION = 1.0.0
SOURCES += main.cpp
RESOURCES += resources/resources.qrc
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

win32 {
    CONFIG -= console
    RC_ICONS = resources/viviamealscheat.ico
    QMAKE_TARGET_DESCRIPTION = ViviaMealsCheat
    QMAKE_TARGET_PRODUCT = ViviaMealsCheat
    QMAKE_TARGET_COPYRIGHT = Michael Bergmann
    include(installer/deploy.pri)
}
