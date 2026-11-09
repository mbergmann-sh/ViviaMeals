# ---------------------------------------------------------------------------
#  VitaMeals – Windows: DLLs bereitstellen und Qt-Installer-Framework-Installer bauen
#  (Aufbau wie in VitaParms und VitaBreathe)
#
#  Nach jedem erfolgreichen Windows-Build (Qt Creator oder Konsole, MinGW oder MSVC):
#    1. windeployqt kopiert die nötigen Qt-DLLs, Plugins und die deutschen Qt-Übersetzungen
#       neben VitaMeals.exe – das Programm startet direkt aus dem Build-Ordner.
#  Zusätzlich beim Release-Build:
#    2. installer/qtifw/data wird neu befüllt: VitaMeals.exe, alle DLLs und windeployqt-
#       Unterordner, LICENSE.txt, README.md
#    3. Paketbaum installer/qtifw/packages/de.mbergmann.vitameals (meta + data)
#    4. Ist das Qt Installer Framework installiert:
#         installer/Output/VitaMeals-<Version>-Setup.exe     (Zielordner-Vorgabe C:\VitaMeals)
#
#  binarycreator wird beim qmake-Lauf gesucht; abweichender Ort über die Umgebungsvariable
#    VM_BINARYCREATOR   Pfad zu binarycreator.exe   (Vorgabe: <Qt>/Tools/QtInstallerFramework/<neueste>/bin
#                                                     bzw. %QTIFWDIR%/bin)
#  Installer nicht automatisch bauen:   qmake CONFIG+=vm_no_installer
#  Alles wie von windeployqt vorgesehen bereitstellen:   qmake CONFIG+=vm_full_deploy
#
#  Alle Pfade mit Schrägstrich und in Anführungszeichen – das funktioniert unverändert in
#  cmd.exe und in einer sh.exe. Dateien kopiert qmake selbst (qmake -install qinstall).
# ---------------------------------------------------------------------------

defineReplace(vmQuote) {
    return("\"$$clean_path($$1)\"")
}

VM_INST     = $$clean_path($$PWD)
VM_IFW      = $$VM_INST/qtifw
VM_DATA     = $$VM_IFW/data
VM_PKG      = de.mbergmann.vitameals
VM_PKGDIR   = $$VM_IFW/packages/$$VM_PKG
VM_OUTPUT   = $$VM_INST/Output

VM_WINDEPLOYQT = $$vmQuote($$[QT_INSTALL_BINS]/windeployqt.exe) --verbose 0

# Schlank ausliefern: reine Widgets-Anwendung (kein OpenGL/Direct3D/Qt Quick, kein Multimedia).
# Druck (printsupport) und SVG-Symbole (svg + imageformats) bringt windeployqt selbst mit.
# Qt-Übersetzungen: Deutsch (Standarddialoge „Speichern“, „Abbrechen“ …).
!vm_full_deploy {
    VM_WINDEPLOYQT += --no-opengl-sw --no-system-d3d-compiler --translations de
    versionAtLeast(QT_VERSION, 6.8.0): VM_WINDEPLOYQT += --no-system-dxc-compiler
}
VM_QINSTALL    = $$vmQuote($$QMAKE_QMAKE) -install qinstall
VM_EXE         = \"$(DESTDIR_TARGET)\"
VM_NL          = $$escape_expand(\\n\\t)

# 1. Qt-DLLs neben das Programm im Build-Ordner
QMAKE_POST_LINK += $$VM_WINDEPLOYQT $$VM_EXE

CONFIG(release, debug|release) {
    # 2. Programmdateien für den Installer frisch aufbauen (führendes "-": Fehler ignorieren)
    QMAKE_POST_LINK += $$VM_NL-$$QMAKE_DEL_TREE $$shell_quote($$shell_path($$VM_DATA))
    QMAKE_POST_LINK += $$VM_NL$$VM_WINDEPLOYQT --dir $$vmQuote($$VM_DATA) $$VM_EXE
    QMAKE_POST_LINK += $$VM_NL$$VM_QINSTALL -exe $$VM_EXE $$vmQuote($$VM_DATA/VitaMeals.exe)
    QMAKE_POST_LINK += $$VM_NL$$VM_QINSTALL $$vmQuote($$PWD/../LICENSE) $$vmQuote($$VM_DATA/LICENSE.txt)
    QMAKE_POST_LINK += $$VM_NL$$VM_QINSTALL $$vmQuote($$PWD/../README.md) $$vmQuote($$VM_DATA/README.md)

    # 3. Paketbaum
    QMAKE_POST_LINK += $$VM_NL-$$QMAKE_DEL_TREE $$shell_quote($$shell_path($$VM_IFW/packages))
    QMAKE_POST_LINK += $$VM_NL$$VM_QINSTALL $$vmQuote($$VM_IFW/meta/package.xml) $$vmQuote($$VM_PKGDIR/meta/package.xml)
    QMAKE_POST_LINK += $$VM_NL$$VM_QINSTALL $$vmQuote($$VM_IFW/meta/license.txt) $$vmQuote($$VM_PKGDIR/meta/license.txt)
    QMAKE_POST_LINK += $$VM_NL$$VM_QINSTALL $$vmQuote($$VM_IFW/meta/installscript.qs) $$vmQuote($$VM_PKGDIR/meta/installscript.qs)
    QMAKE_POST_LINK += $$VM_NL$$VM_QINSTALL $$vmQuote($$VM_DATA) $$vmQuote($$VM_PKGDIR/data)

    # 4. Installer bauen, sofern das Qt Installer Framework vorhanden ist
    !vm_no_installer {
        VM_BINARYCREATOR = $$(VM_BINARYCREATOR)
        isEmpty(VM_BINARYCREATOR) {
            VM_IFW_ROOT = $$(QTIFWDIR)
            !isEmpty(VM_IFW_ROOT): exists($$VM_IFW_ROOT/bin/binarycreator.exe): VM_BINARYCREATOR = $$VM_IFW_ROOT/bin/binarycreator.exe
        }
        isEmpty(VM_BINARYCREATOR) {
            # Qt-Online-Installer: <Qt>/<Version>/<Kit> → <Qt>/Tools/QtInstallerFramework/<Version>/bin
            VM_IFW_VERSIONS = $$files($$clean_path($$[QT_INSTALL_PREFIX]/../../Tools/QtInstallerFramework/*))
            for(d, VM_IFW_VERSIONS): exists($$d/bin/binarycreator.exe): VM_BINARYCREATOR = $$d/bin/binarycreator.exe
        }
        !isEmpty(VM_BINARYCREATOR) {
            message("VitaMeals: Qt Installer Framework gefunden – $$VM_BINARYCREATOR")
            QMAKE_POST_LINK += $$VM_NL-$$QMAKE_MKDIR $$shell_quote($$shell_path($$VM_OUTPUT))
            QMAKE_POST_LINK += $$VM_NL-$$vmQuote($$VM_BINARYCREATOR) --offline-only --compression 9 \
                -c $$vmQuote($$VM_IFW/config/config.xml) -p $$vmQuote($$VM_IFW/packages) \
                $$vmQuote($$VM_OUTPUT/VitaMeals-$${VERSION}-Setup.exe)
        } else {
            message("VitaMeals: Qt Installer Framework nicht gefunden – kein Installer (siehe installer/README.md)")
        }
    }
}
