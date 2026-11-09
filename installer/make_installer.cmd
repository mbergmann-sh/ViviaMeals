@echo off
rem ---------------------------------------------------------------------------
rem  VitaMeals - Windows-Installer bauen (Qt Installer Framework)
rem
rem  Wird nach jedem Release-Build von installer/windows_deploy.pri aufgerufen
rem  (auch: mingw32-make installer / nmake installer), kann aber auch von Hand
rem  gestartet werden:
rem
rem    make_installer.cmd <VitaMeals.exe> <Qt-bin> <Qt-translations> <binarycreator|-> <Arbeitsordner> <Version> [qt68] [full]
rem
rem  1. Bereitstellungsordner <Arbeitsordner>\installer-staging frisch anlegen:
rem     NUR VitaMeals.exe und die von windeployqt ermittelten DLLs/Plugins
rem  2. Uebersetzungsschritt: deutsche Qt-Uebersetzungen nach translations\
rem     (windeployqt --translations de, sonst qtbase_de.qm/qt_de.qm aus Qt)
rem  3. Sicherheitsnetz: Quelltext- und Zwischendateien (*.h, *.cpp, *.o, *.obj,
rem     *.pdb ...) entfernen, LICENSE.txt und README.md dazu
rem  4. Paketbaum (meta + data) und installer\Output\VitaMeals-<Version>-Setup.exe
rem
rem  Protokoll (immer):   installer\Output\VitaMeals-<Version>-Protokoll.txt
rem  Inhaltsliste:        installer\Output\VitaMeals-<Version>-Inhalt.txt
rem ---------------------------------------------------------------------------
setlocal EnableExtensions

set "EXE=%~f1"
set "QTBIN=%~f2"
set "QTTR=%~f3"
set "BC=%~4"
set "WORK=%~f5"
set "VERSION=%~6"
set "QT68=%~7"
set "FULL=%~8"
if "%BC%"=="-" set "BC="
if defined VM_BINARYCREATOR if not defined BC set "BC=%VM_BINARYCREATOR%"

set "INST=%~dp0"
if "%INST:~-1%"=="\" set "INST=%INST:~0,-1%"
set "STAGE=%WORK%\installer-staging"
set "PKGS=%WORK%\installer-packages"
set "PKG=%PKGS%\de.mbergmann.vitameals"
set "OUT=%INST%\Output"
set "SETUP=%OUT%\VitaMeals-%VERSION%-Setup.exe"
set "LIST=%OUT%\VitaMeals-%VERSION%-Inhalt.txt"
set "LOG=%OUT%\VitaMeals-%VERSION%-Protokoll.txt"
rem windeployqt braucht Qt (und bei MinGW den Compiler) im Suchpfad
set "PATH=%QTBIN%;%PATH%"

if not exist "%OUT%" mkdir "%OUT%"
> "%LOG%" echo VitaMeals %VERSION% - Installer-Protokoll %DATE% %TIME%
call :log "VitaMeals: Installer %VERSION% wird vorbereitet ..."
call :log "  Programm:  %EXE%"
call :log "  Qt:        %QTBIN%"
if not exist "%EXE%" (
    call :log "error: VitaMeals.exe nicht gefunden: %EXE%"
    goto fail
)
if not exist "%QTBIN%\windeployqt.exe" (
    call :log "error: windeployqt.exe nicht gefunden in %QTBIN%"
    goto fail
)

rem ---- 1. Bereitstellungsordner: nur die .exe
if exist "%STAGE%" rmdir /s /q "%STAGE%"
if exist "%PKGS%" rmdir /s /q "%PKGS%"
mkdir "%STAGE%"
if errorlevel 1 goto fail
copy /y "%EXE%" "%STAGE%\VitaMeals.exe" >nul
if errorlevel 1 goto fail

rem ---- DLLs, Plugins und Uebersetzungen ermitteln
set "OPTS=--verbose 1 --translations de"
if /i not "%FULL%"=="full" set "OPTS=%OPTS% --no-opengl-sw --no-system-d3d-compiler"
if /i not "%FULL%"=="full" if "%QT68%"=="qt68" set "OPTS=%OPTS% --no-system-dxc-compiler"
call :log "VitaMeals: windeployqt %OPTS%"
"%QTBIN%\windeployqt.exe" %OPTS% "%STAGE%\VitaMeals.exe" >> "%LOG%" 2>&1
if errorlevel 1 (
    call :log "error: windeployqt ist fehlgeschlagen - Einzelheiten im Protokoll."
    goto fail
)

rem ---- 2. Uebersetzungsschritt (Standarddialoge "Speichern", "Abbrechen" ... auf Deutsch)
if not exist "%STAGE%\translations" mkdir "%STAGE%\translations"
if not exist "%STAGE%\translations\qt_de.qm" if exist "%QTTR%\qt_de.qm" copy /y "%QTTR%\qt_de.qm" "%STAGE%\translations\qt_de.qm" >nul
if not exist "%STAGE%\translations\qt_de.qm" if exist "%QTTR%\qtbase_de.qm" copy /y "%QTTR%\qtbase_de.qm" "%STAGE%\translations\qtbase_de.qm" >nul
if exist "%STAGE%\translations\qt_de.qm" (
    call :log "VitaMeals: Qt-Uebersetzung translations\qt_de.qm"
) else if exist "%STAGE%\translations\qtbase_de.qm" (
    call :log "VitaMeals: Qt-Uebersetzung translations\qtbase_de.qm"
) else (
    call :log "warning: Keine deutschen Qt-Uebersetzungen gefunden - Standarddialoge erscheinen englisch."
    call :log "warning: Abhilfe: im Qt Maintenance Tool die Komponente Qt Translations installieren."
)

rem ---- 3. nur Programmdateien: Quelltexte, Objekt- und Debugdateien entfernen
for %%x in (h hpp hxx cpp cxx cc c o obj a lib pdb ilk exp map pro pri prl qrc ui ts moc qmake.stash) do del /s /q "%STAGE%\*.%%x" >nul 2>&1
for /d /r "%STAGE%" %%d in (debug release .moc .obj .rcc .ui) do if exist "%%d" rmdir /s /q "%%d" >nul 2>&1
copy /y "%INST%\..\LICENSE" "%STAGE%\LICENSE.txt" >nul
if errorlevel 1 goto fail
copy /y "%INST%\..\README.md" "%STAGE%\README.md" >nul

rem ---- 4. Paketbaum
mkdir "%PKG%\meta"
mkdir "%PKG%\data"
for %%f in (package.xml license.txt installscript.qs) do (
    if not exist "%INST%\qtifw\meta\%%f" (
        call :log "error: %INST%\qtifw\meta\%%f fehlt - qmake erneut ausfuehren."
        goto fail
    )
    copy /y "%INST%\qtifw\meta\%%f" "%PKG%\meta\%%f" >nul
)
if not exist "%INST%\qtifw\config\config.xml" (
    call :log "error: %INST%\qtifw\config\config.xml fehlt - qmake erneut ausfuehren."
    goto fail
)
xcopy "%STAGE%\*" "%PKG%\data\" /e /i /q /y >nul
if errorlevel 1 goto fail
dir /s /b /a-d "%STAGE%" > "%LIST%"
call :log "VitaMeals: Inhalt des Installers: %LIST%"

rem ---- 5. Installer: binarycreator suchen (auch, wenn das IFW erst nach dem qmake-Lauf installiert wurde)
if not defined BC if defined QTIFWDIR if exist "%QTIFWDIR%\bin\binarycreator.exe" set "BC=%QTIFWDIR%\bin\binarycreator.exe"
if not defined BC for /d %%d in ("%QTBIN%\..\..\..\Tools\QtInstallerFramework\*") do if exist "%%~fd\bin\binarycreator.exe" set "BC=%%~fd\bin\binarycreator.exe"
if not defined BC for /d %%d in ("C:\Qt\Tools\QtInstallerFramework\*" "C:\Qt\QtIFW*" "%ProgramFiles%\Qt\QtIFW*") do if exist "%%~fd\bin\binarycreator.exe" set "BC=%%~fd\bin\binarycreator.exe"
if not defined BC for /f "delims=" %%b in ('where binarycreator.exe 2^>nul') do if not defined BC set "BC=%%b"
if not defined BC (
    call :log "warning: Qt Installer Framework (binarycreator.exe) nicht gefunden - es wurde KEIN Installer erstellt."
    call :log "warning: Abhilfe: Qt Maintenance Tool - Developer and Designer Tools - Qt Installer Framework installieren"
    call :log "warning: oder die Umgebungsvariable VM_BINARYCREATOR auf binarycreator.exe setzen."
    call :log "warning: Die Programmdateien liegen bereit in: %STAGE%"
    exit /b 0
)
if exist "%SETUP%" del /q "%SETUP%"
call :log "VitaMeals: %BC%"
"%BC%" --offline-only -c "%INST%\qtifw\config\config.xml" -p "%PKGS%" "%SETUP%" >> "%LOG%" 2>&1
if errorlevel 1 (
    call :log "error: binarycreator ist fehlgeschlagen - Einzelheiten im Protokoll."
    goto fail
)
if not exist "%SETUP%" goto fail
call :log "VitaMeals: Installer erstellt: %SETUP%"
exit /b 0

:fail
call :log "error: VitaMeals-Installer konnte nicht erstellt werden. Protokoll: %LOG%"
exit /b 1

:log
echo %~1
>> "%LOG%" echo %~1
exit /b 0
