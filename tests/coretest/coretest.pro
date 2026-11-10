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
    ../../src/fridge.cpp ../../src/profiles.cpp ../../src/mealplanner.cpp ../../src/umlauts.cpp ../../src/mealtypes.cpp ../../src/coaching.cpp ../../src/diary.cpp ../../src/odtexport.cpp ../../src/deepl.cpp
HEADERS += ../../src/recipedb.h
RESOURCES += ../../resources/data.qrc
