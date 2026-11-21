# ---------------------------------------------------------------------------
#  ViviaMealsCheat – Windows: Qt-DLLs bereitstellen und Installer bauen (wie bei ViviaMeals)
#
#  Nach jedem Build: windeployqt kopiert die Qt-DLLs neben ViviaMealsCheat.exe im Build-Ordner,
#  damit das Programm direkt aus Qt Creator startet.
#  Release-Build: installer/make_installer.cmd baut installer/Output/ViviaMealsCheat-<Version>-Setup.exe.
#  Der Installer enthält NUR ViviaMealsCheat.exe – das Programm benutzt die Qt-DLLs der vorhandenen
#  ViviaMeals-Installation. Deshalb mit DERSELBEN Qt-Version (und demselben Compiler) bauen wie ViviaMeals.
#
#  binarycreator.exe wird gesucht über VM_BINARYCREATOR, QTIFWDIR, <Qt>/Tools/QtInstallerFramework/*,
#  C:/Qt/QtIFW*, den Suchpfad.  Installer abschalten: qmake CONFIG+=vm_no_installer
# ---------------------------------------------------------------------------
defineReplace(vmcQuote) {
    return("\"$$shell_path($$clean_path($$1))\"")
}
VMC_INST = $$clean_path($$PWD)
VMC_NL   = $$escape_expand(\\n\\t)
VMC_EXE  = \"$(DESTDIR_TARGET)\"

QMAKE_POST_LINK += $$vmcQuote($$[QT_INSTALL_BINS]/windeployqt.exe) --verbose 0 --no-translations --no-opengl-sw --no-system-d3d-compiler $$VMC_EXE

VMC_BC = $$(VM_BINARYCREATOR)
isEmpty(VMC_BC) {
    VMC_IFW_ROOT = $$(QTIFWDIR)
    !isEmpty(VMC_IFW_ROOT):exists($$VMC_IFW_ROOT/bin/binarycreator.exe): VMC_BC = $$VMC_IFW_ROOT/bin/binarycreator.exe
}
isEmpty(VMC_BC) {
    VMC_QT_ROOT = $$clean_path($$[QT_INSTALL_PREFIX]/../..)
    VMC_CAND = $$files($$VMC_QT_ROOT/Tools/QtInstallerFramework/*) $$files($$VMC_QT_ROOT/QtIFW*) \
               $$files(C:/Qt/QtIFW*) $$files(C:/Qt/Tools/QtInstallerFramework/*)
    for(d, VMC_CAND): exists($$d/bin/binarycreator.exe): VMC_BC = $$d/bin/binarycreator.exe
}
isEmpty(VMC_BC): VMC_BC_ARG = \"-\"
else: VMC_BC_ARG = $$vmcQuote($$VMC_BC)

VMC_MAKE_INSTALLER = cmd /c call $$vmcQuote($$VMC_INST/make_installer.cmd) $$VMC_EXE $$VMC_BC_ARG \
    $$vmcQuote($$OUT_PWD) $$VERSION $$QT_MAJOR_VERSION

debug_and_release:!build_pass {
    installer.commands = $(MAKE) -f $(MAKEFILE).Release installer
} else: CONFIG(release, debug|release) {
    installer.commands = $$VMC_MAKE_INSTALLER
    installer.depends = $(DESTDIR_TARGET)
} else {
    installer.commands = @echo ViviaMealsCheat: Der Installer wird aus dem Release-Build erstellt.
}
QMAKE_EXTRA_TARGETS += installer

CONFIG(release, debug|release):!vm_no_installer: QMAKE_POST_LINK += $${VMC_NL}$$VMC_MAKE_INSTALLER
