# ---------------------------------------------------------------------------
#  ViviaMeals – Kern ohne Fenster (auch für Tests und eine spätere mobile App)
# ---------------------------------------------------------------------------
INCLUDEPATH += $$PWD/src

SOURCES += \
    $$PWD/src/recipe.cpp \
    $$PWD/src/recipedb.cpp \
    $$PWD/src/units.cpp \
    $$PWD/src/ingredientparser.cpp \
    $$PWD/src/glossary.cpp \
    $$PWD/src/foodtable.cpp \
    $$PWD/src/foodcsv.cpp \
    $$PWD/src/nutrition.cpp \
    $$PWD/src/textcodec.cpp \
    $$PWD/src/importers.cpp \
    $$PWD/src/recipehtml.cpp \
    $$PWD/src/websiteexport.cpp \
    $$PWD/src/latexexport.cpp \
    $$PWD/src/fridge.cpp \
    $$PWD/src/profiles.cpp \
    $$PWD/src/mealplanner.cpp \
    $$PWD/src/umlauts.cpp \
    $$PWD/src/mealtypes.cpp \
    $$PWD/src/coaching.cpp \
    $$PWD/src/diary.cpp \
    $$PWD/src/odtexport.cpp \
    $$PWD/src/deepl.cpp \
    $$PWD/src/ocr.cpp \
    $$PWD/src/recipescan.cpp

HEADERS += \
    $$PWD/src/qtcompat.h \
    $$PWD/src/recipe.h \
    $$PWD/src/recipedb.h \
    $$PWD/src/units.h \
    $$PWD/src/ingredientparser.h \
    $$PWD/src/glossary.h \
    $$PWD/src/foodtable.h \
    $$PWD/src/foodcsv.h \
    $$PWD/src/nutrition.h \
    $$PWD/src/textcodec.h \
    $$PWD/src/importers.h \
    $$PWD/src/recipehtml.h \
    $$PWD/src/websiteexport.h \
    $$PWD/src/latexexport.h \
    $$PWD/src/fridge.h \
    $$PWD/src/profiles.h \
    $$PWD/src/mealplanner.h \
    $$PWD/src/umlauts.h \
    $$PWD/src/mealtypes.h \
    $$PWD/src/coaching.h \
    $$PWD/src/diary.h \
    $$PWD/src/odtexport.h \
    $$PWD/src/deepl.h \
    $$PWD/src/ocr.h \
    $$PWD/src/recipescan.h
