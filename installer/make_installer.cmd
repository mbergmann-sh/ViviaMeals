@echo off
rem ---------------------------------------------------------------------------
rem  ViviaMeals - Windows-Installer bauen (Qt Installer Framework)
rem
rem  Wird nach jedem Release-Build von installer/windows_deploy.pri aufgerufen
rem  (auch: mingw32-make installer / nmake installer), kann aber auch von Hand
rem  gestartet werden:
rem
rem    make_installer.cmd <ViviaMeals.exe> <Qt-bin> <Qt-translations> <binarycreator|-> <Arbeitsordner> <Version> [qt68] [full]
rem
rem  1. Bereitstellungsordner <Arbeitsordner>\installer-staging frisch anlegen:
rem     NUR ViviaMeals.exe und die von windeployqt ermittelten DLLs/Plugins
rem  2. Uebersetzungsschritt: deutsche Qt-Uebersetzungen nach translations\
rem     (windeployqt --translations de, sonst qtbase_de.qm/qt_de.qm aus Qt)
rem  3. Sicherheitsnetz: Quelltext- und Zwischendateien (*.h, *.cpp, *.o, *.obj,
rem     *.pdb ...) entfernen, LICENSE.txt und README.md dazu
rem  4. Paketbaum (meta + data) und installer\Output\ViviaMeals-<Version>-Setup.exe
rem
rem  Optional: Texterkennung Tesseract mitliefern (fuer "Rezept scannen")
rem    set VIVIA_TESSERACT_DIR=C:\Program Files\Tesseract-OCR
rem    set VIVIA_TESSERACT_LANGS=deu eng osd       (Vorgabe)
rem  Kopiert tesseract.exe, die DLLs, die Sprachdaten und tessdata\configs nach
rem  <Installationsordner>\tesseract\ - ViviaMeals findet es dort von selbst.
rem
rem  Protokoll (immer):   installer\Output\ViviaMeals-<Version>-Protokoll.txt
rem  Inhaltsliste:        installer\Output\ViviaMeals-<Version>-Inhalt.txt
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
set "PKG=%PKGS%\de.mbergmann.viviameals"
set "OUT=%INST%\Output"
set "SETUP=%OUT%\ViviaMeals-%VERSION%-Setup.exe"
set "LIST=%OUT%\ViviaMeals-%VERSION%-Inhalt.txt"
set "LOG=%OUT%\ViviaMeals-%VERSION%-Protokoll.txt"
rem windeployqt braucht Qt (und bei MinGW den Compiler) im Suchpfad
set "PATH=%QTBIN%;%PATH%"

if not exist "%OUT%" mkdir "%OUT%"
> "%LOG%" echo ViviaMeals %VERSION% - Installer-Protokoll %DATE% %TIME%
call :log "ViviaMeals: Installer %VERSION% wird vorbereitet ..."
call :log "  Programm:  %EXE%"
call :log "  Qt:        %QTBIN%"
if not exist "%EXE%" (
    call :log "error: ViviaMeals.exe nicht gefunden: %EXE%"
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
copy /y "%EXE%" "%STAGE%\ViviaMeals.exe" >nul
if errorlevel 1 goto fail

rem ---- DLLs, Plugins und Uebersetzungen ermitteln
set "OPTS=--verbose 1 --translations de"
if /i not "%FULL%"=="full" set "OPTS=%OPTS% --no-opengl-sw --no-system-d3d-compiler"
if /i not "%FULL%"=="full" if "%QT68%"=="qt68" set "OPTS=%OPTS% --no-system-dxc-compiler"
call :log "ViviaMeals: windeployqt %OPTS%"
"%QTBIN%\windeployqt.exe" %OPTS% "%STAGE%\ViviaMeals.exe" >> "%LOG%" 2>&1
if errorlevel 1 (
    call :log "error: windeployqt ist fehlgeschlagen - Einzelheiten im Protokoll."
    goto fail
)

rem ---- 2. Uebersetzungsschritt (Standarddialoge "Speichern", "Abbrechen" ... auf Deutsch)
if not exist "%STAGE%\translations" mkdir "%STAGE%\translations"
if not exist "%STAGE%\translations\qt_de.qm" if exist "%QTTR%\qt_de.qm" copy /y "%QTTR%\qt_de.qm" "%STAGE%\translations\qt_de.qm" >nul
if not exist "%STAGE%\translations\qt_de.qm" if exist "%QTTR%\qtbase_de.qm" copy /y "%QTTR%\qtbase_de.qm" "%STAGE%\translations\qtbase_de.qm" >nul
if exist "%STAGE%\translations\qt_de.qm" (
    call :log "ViviaMeals: Qt-Uebersetzung translations\qt_de.qm"
) else if exist "%STAGE%\translations\qtbase_de.qm" (
    call :log "ViviaMeals: Qt-Uebersetzung translations\qtbase_de.qm"
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
rem ---- 3a. Handbuch (CHM fuer F1, dazu PDF und HTML) - erzeugt mit tools\make_handbuch.py
set "HB=%INST%\..\docs\handbuch"
if exist "%HB%\ViviaMeals-Handbuch.chm" (
    if not exist "%STAGE%\handbuch" mkdir "%STAGE%\handbuch"
    copy /y "%HB%\ViviaMeals-Handbuch.chm" "%STAGE%\handbuch\" >nul
    if exist "%HB%\ViviaMeals-Handbuch.pdf" copy /y "%HB%\ViviaMeals-Handbuch.pdf" "%STAGE%\handbuch\" >nul
    if exist "%HB%\html\index.html" xcopy "%HB%\html" "%STAGE%\handbuch\html\" /e /i /q /y >nul
    call :log "Handbuch beigelegt: handbuch\ViviaMeals-Handbuch.chm"
) else (
    call :log "warning: Handbuch fehlt (docs\handbuch\ViviaMeals-Handbuch.chm) - F1 zeigt dann nichts an. Erzeugen: python tools\make_handbuch.py"
)

rem ---- 3b. optional: Tesseract (Texterkennung) mitliefern
if not defined VIVIA_TESSERACT_DIR goto notess
if not exist "%VIVIA_TESSERACT_DIR%\tesseract.exe" (
    call :log "warning: VIVIA_TESSERACT_DIR gesetzt, aber dort fehlt tesseract.exe: %VIVIA_TESSERACT_DIR%"
    goto notess
)
call :bundletess
if errorlevel 1 goto fail
goto tessdone
:notess
call :log "ViviaMeals: Tesseract wird nicht mitgeliefert (optional: VIVIA_TESSERACT_DIR setzen)."
:tessdone

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
call :log "ViviaMeals: Inhalt des Installers: %LIST%"

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
call :log "ViviaMeals: %BC%"
"%BC%" --offline-only -c "%INST%\qtifw\config\config.xml" -p "%PKGS%" "%SETUP%" >> "%LOG%" 2>&1
if errorlevel 1 (
    call :log "error: binarycreator ist fehlgeschlagen - Einzelheiten im Protokoll."
    goto fail
)
if not exist "%SETUP%" goto fail
call :log "ViviaMeals: Installer erstellt: %SETUP%"
exit /b 0

:fail
call :log "error: ViviaMeals-Installer konnte nicht erstellt werden. Protokoll: %LOG%"
exit /b 1

:bundletess
set "TS=%VIVIA_TESSERACT_DIR%"
set "TD=%STAGE%\tesseract"
if not defined VIVIA_TESSERACT_LANGS set "VIVIA_TESSERACT_LANGS=deu eng osd"
mkdir "%TD%\tessdata" >nul 2>&1
copy /y "%TS%\tesseract.exe" "%TD%\tesseract.exe" >nul
if errorlevel 1 exit /b 1
copy /y "%TS%\*.dll" "%TD%\" >nul
if errorlevel 1 (
    call :log "error: Keine DLLs neben tesseract.exe gefunden: %TS%"
    exit /b 1
)
for %%l in (%VIVIA_TESSERACT_LANGS%) do call :tesslang %%l
if exist "%TS%\tessdata\configs" xcopy "%TS%\tessdata\configs" "%TD%\tessdata\configs\" /e /i /q /y >nul
if exist "%TS%\tessdata\tessconfigs" xcopy "%TS%\tessdata\tessconfigs" "%TD%\tessdata\tessconfigs\" /e /i /q /y >nul
if not exist "%TD%\tessdata\configs\tsv" call :log "warning: tessdata\configs\tsv fehlt - die Texterkennung braucht diese Datei."
copy /y "%INST%\THIRD-PARTY-Tesseract.txt" "%TD%\LIZENZ-Tesseract.txt" >nul
call :log "ViviaMeals: Tesseract mitgeliefert aus %TS% (Sprachen: %VIVIA_TESSERACT_LANGS%)"
exit /b 0

:tesslang
if exist "%TS%\tessdata\%~1.traineddata" (
    copy /y "%TS%\tessdata\%~1.traineddata" "%TD%\tessdata\%~1.traineddata" >nul
) else (
    call :log "warning: Sprachdaten %~1.traineddata fehlen in %TS%\tessdata"
)
exit /b 0

:log
echo %~1
>> "%LOG%" echo %~1
exit /b 0
