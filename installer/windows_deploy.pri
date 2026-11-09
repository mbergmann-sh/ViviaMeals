# ---------------------------------------------------------------------------
#  VitaMeals – Windows: DLLs bereitstellen und Qt-Installer-Framework-Installer bauen
#  (Aufbau wie in VitaParms und VitaBreathe)
#
#  Nach jedem erfolgreichen Windows-Build (Qt Creator oder Konsole, MinGW oder MSVC):
#    1. windeployqt kopiert die nötigen Qt-DLLs, Plugins und die deutschen Qt-Übersetzungen
#       neben VitaMeals.exe – das Programm startet direkt aus dem Build-Ordner.
#  Zusätzlich beim Release-Build (installer/make_installer.cmd; von Hand: mingw32-make installer):
#    2. Bereitstellungsordner <Build>/installer-staging: NUR VitaMeals.exe, die von windeployqt
#       ermittelten DLLs/Plugins, translations\qt_de.qm, LICENSE.txt, README.md –
#       keine Quelltexte, Objekt- oder Debugdateien (werden zur Sicherheit zusätzlich gelöscht)
#    3. Paketbaum <Build>/installer-packages/de.mbergmann.vitameals (meta + data)
#    4. installer/Output/VitaMeals-<Version>-Setup.exe     (Zielordner-Vorgabe C:\VitaMeals),
#       installer/Output/VitaMeals-<Version>-Inhalt.txt (Liste aller eingepackten Dateien) und
#       installer/Output/VitaMeals-<Version>-Protokoll.txt (Ablauf, Fehlermeldungen)
#
#  binarycreator.exe wird gesucht über
#    VM_BINARYCREATOR (Umgebungsvariable, Pfad zu binarycreator.exe), QTIFWDIR,
#    <Qt>/Tools/QtInstallerFramework/<Version>/bin (Qt Maintenance Tool),
#    <Qt>/QtIFW-<Version>/bin bzw. C:/Qt/QtIFW-<Version>/bin (einzelner IFW-Installer)
#    und beim Build erneut (dieselben Orte, C:\Qt\Tools\QtInstallerFramework\*, Suchpfad).
#  Installer nicht automatisch bauen:                    qmake CONFIG+=vm_no_installer
#  Alles wie von windeployqt vorgesehen bereitstellen:   qmake CONFIG+=vm_full_deploy
# ---------------------------------------------------------------------------

defineReplace(vmQuote) {
    return("\"$$shell_path($$clean_path($$1))\"")
}

VM_INST = $$clean_path($$PWD)
VM_NL   = $$escape_expand(\\n\\t)
VM_EXE  = \"$(DESTDIR_TARGET)\"

# 1. Qt-DLLs neben das Programm im Build-Ordner (zum Starten aus Qt Creator)
VM_WINDEPLOYQT = $$vmQuote($$[QT_INSTALL_BINS]/windeployqt.exe) --verbose 0 --translations de
!vm_full_deploy {
    VM_WINDEPLOYQT += --no-opengl-sw --no-system-d3d-compiler
    versionAtLeast(QT_VERSION, 6.8.0): VM_WINDEPLOYQT += --no-system-dxc-compiler
}
QMAKE_POST_LINK += $$VM_WINDEPLOYQT $$VM_EXE

# 2.–4. Installer (nur Release)
VM_QT68 = qt5
versionAtLeast(QT_VERSION, 6.8.0): VM_QT68 = qt68
VM_FULL = slim
vm_full_deploy: VM_FULL = full

# binarycreator.exe schon beim qmake-Lauf suchen (make_installer.cmd sucht beim Build noch einmal –
# falls das Qt Installer Framework erst nach dem qmake-Lauf installiert wurde)
VM_BINARYCREATOR = $$(VM_BINARYCREATOR)
isEmpty(VM_BINARYCREATOR) {
    VM_IFW_ROOT = $$(QTIFWDIR)
    !isEmpty(VM_IFW_ROOT):exists($$VM_IFW_ROOT/bin/binarycreator.exe): VM_BINARYCREATOR = $$VM_IFW_ROOT/bin/binarycreator.exe
}
isEmpty(VM_BINARYCREATOR) {
    # Qt-Installation <Qt>/<Version>/<Kit> → <Qt>/Tools/QtInstallerFramework/<Version>/bin,
    # einzelner IFW-Installer → <Qt>/QtIFW-<Version>/bin (Vorgabe C:\Qt\QtIFW-4.x.x)
    VM_QT_ROOT = $$clean_path($$[QT_INSTALL_PREFIX]/../..)
    VM_IFW_CANDIDATES = $$files($$VM_QT_ROOT/Tools/QtInstallerFramework/*) \
                        $$files($$VM_QT_ROOT/QtIFW*) $$files(C:/Qt/QtIFW*) \
                        $$files(C:/Qt/Tools/QtInstallerFramework/*)
    for(d, VM_IFW_CANDIDATES): exists($$d/bin/binarycreator.exe): VM_BINARYCREATOR = $$d/bin/binarycreator.exe
}
isEmpty(VM_BINARYCREATOR) {
    VM_BC_ARG = \"-\"
} else {
    VM_BC_ARG = $$vmQuote($$VM_BINARYCREATOR)
}
VM_MAKE_INSTALLER = cmd /c call $$vmQuote($$VM_INST/make_installer.cmd) $$VM_EXE \
    $$vmQuote($$[QT_INSTALL_BINS]) $$vmQuote($$[QT_INSTALL_TRANSLATIONS]) $$VM_BC_ARG \
    $$vmQuote($$OUT_PWD) $$VERSION $$VM_QT68 $$VM_FULL

# von Hand: mingw32-make installer  bzw.  nmake installer  (im Build-Ordner, baut immer die Release-Fassung)
debug_and_release:!build_pass {
    installer.commands = $(MAKE) -f $(MAKEFILE).Release installer
} else: CONFIG(release, debug|release) {
    installer.commands = $$VM_MAKE_INSTALLER
    installer.depends = $(DESTDIR_TARGET)
} else {
    installer.commands = @echo VitaMeals: Der Installer wird aus dem Release-Build erstellt - bitte Release waehlen.
}
QMAKE_EXTRA_TARGETS += installer

CONFIG(release, debug|release) {
    vm_no_installer {
        QMAKE_POST_LINK += $${VM_NL}@echo VitaMeals: Installer abgeschaltet (CONFIG+=vm_no_installer) - von Hand: mingw32-make installer
    } else {
        isEmpty(VM_BINARYCREATOR): message("VitaMeals: binarycreator.exe beim qmake-Lauf nicht gefunden – make_installer.cmd sucht beim Build erneut (siehe installer/README.md)")
        else: message("VitaMeals: Qt Installer Framework gefunden – $$VM_BINARYCREATOR")
        QMAKE_POST_LINK += $${VM_NL}$$VM_MAKE_INSTALLER
    }
} else {
    QMAKE_POST_LINK += $${VM_NL}@echo VitaMeals: Debug-Build - der Installer wird nur beim Release-Build erstellt.
}
