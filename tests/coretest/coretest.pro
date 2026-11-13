# Test des Kerns: Zutatenparser, Einheiten, Glossar, Nährwerte, Importe
QT += core gui network
CONFIG += c++17 console
CONFIG -= app_bundle
TARGET = coretest
INCLUDEPATH += ../../src
SOURCES += main.cpp \
    ../../src/recipe.cpp ../../src/recipedb.cpp ../../src/units.cpp ../../src/ingredientparser.cpp \
    ../../src/glossary.cpp ../../src/foodtable.cpp ../../src/foodcsv.cpp ../../src/nutrition.cpp ../../src/textcodec.cpp \
    ../../src/importers.cpp ../../src/recipehtml.cpp ../../src/websiteexport.cpp ../../src/latexexport.cpp \
    ../../src/fridge.cpp ../../src/profiles.cpp ../../src/mealplanner.cpp ../../src/umlauts.cpp ../../src/mealtypes.cpp ../../src/coaching.cpp ../../src/diary.cpp ../../src/odtexport.cpp ../../src/deepl.cpp ../../src/ocr.cpp ../../src/recipescan.cpp ../../src/escl.cpp ../../src/duplicates.cpp ../../src/recipepackage.cpp ../../src/openfoodfacts.cpp ../../src/sync.cpp ../../src/menu.cpp ../../src/menuexport.cpp ../../src/syncserver.cpp ../../src/markup.cpp ../../src/season.cpp
HEADERS += ../../src/recipedb.h ../../src/escl.h ../../src/openfoodfacts.h ../../src/syncserver.h
RESOURCES += ../../resources/data.qrc
