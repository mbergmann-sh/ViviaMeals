# ---------------------------------------------------------------------------
#  ViviaMeals – Desktop-Oberfläche (Qt Widgets)
# ---------------------------------------------------------------------------
SOURCES += \
    $$PWD/src/main.cpp \
    $$PWD/src/mainwindow.cpp \
    $$PWD/src/foodwizard.cpp \
    $$PWD/src/busyprogress.cpp \
    $$PWD/src/packagedialog.cpp \
    $$PWD/src/recipeview.cpp \
    $$PWD/src/imagecropdialog.cpp \
    $$PWD/src/weighindialog.cpp \
    $$PWD/src/diarydialog.cpp \
    $$PWD/src/editordialog.cpp \
    $$PWD/src/searchdialog.cpp \
    $$PWD/src/importdialogs.cpp \
    $$PWD/src/exportdialogs.cpp \
    $$PWD/src/fridgedialog.cpp \
    $$PWD/src/profiledialog.cpp \
    $$PWD/src/weightchart.cpp \
    $$PWD/src/assistantdialog.cpp \
    $$PWD/src/mealplandialog.cpp \
    $$PWD/src/fooddialog.cpp \
    $$PWD/src/settingsdialog.cpp \
    $$PWD/src/syncdialog.cpp \
    $$PWD/src/menudialog.cpp \
    $$PWD/src/manual.cpp \
    $$PWD/src/splash.cpp \
    $$PWD/src/instructionsedit.cpp \
    $$PWD/src/symbols.cpp \
    $$PWD/src/uiscale.cpp \
    $$PWD/src/themes.cpp \
    $$PWD/src/icons.cpp \
    $$PWD/src/screenfit.cpp \
    $$PWD/src/windowstate.cpp \
    $$PWD/src/scanner.cpp \
    $$PWD/src/scandialog.cpp \
    $$PWD/src/duplicatesdialog.cpp

HEADERS += \
    $$PWD/src/mainwindow.h \
    $$PWD/src/foodwizard.h \
    $$PWD/src/busyprogress.h \
    $$PWD/src/packagedialog.h \
    $$PWD/src/recipeview.h \
    $$PWD/src/imagecropdialog.h \
    $$PWD/src/weighindialog.h \
    $$PWD/src/diarydialog.h \
    $$PWD/src/editordialog.h \
    $$PWD/src/searchdialog.h \
    $$PWD/src/importdialogs.h \
    $$PWD/src/exportdialogs.h \
    $$PWD/src/fridgedialog.h \
    $$PWD/src/profiledialog.h \
    $$PWD/src/weightchart.h \
    $$PWD/src/assistantdialog.h \
    $$PWD/src/mealplandialog.h \
    $$PWD/src/fooddialog.h \
    $$PWD/src/settingsdialog.h \
    $$PWD/src/syncdialog.h \
    $$PWD/src/menudialog.h \
    $$PWD/src/manual.h \
    $$PWD/src/splash.h \
    $$PWD/src/instructionsedit.h \
    $$PWD/src/symbols.h \
    $$PWD/src/uiscale.h \
    $$PWD/src/themes.h \
    $$PWD/src/icons.h \
    $$PWD/src/screenfit.h \
    $$PWD/src/windowstate.h \
    $$PWD/src/scanner.h \
    $$PWD/src/scandialog.h \
    $$PWD/src/duplicatesdialog.h

# Scannen unter Windows: WIA-Automatisierung über COM (nur ole32/oleaut32, keine weiteren Bibliotheken)
win32 {
    SOURCES += $$PWD/src/wiascan_win.cpp
    HEADERS += $$PWD/src/wiascan_win.h
    LIBS += -lole32 -loleaut32 -luuid
}
