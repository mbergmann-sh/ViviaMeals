@echo off
rem ---------------------------------------------------------------------------
rem  ViviaMealsCheat - Windows-Installer bauen (Qt Installer Framework)
rem
rem    make_installer.cmd <ViviaMealsCheat.exe> <binarycreator|-> <Arbeitsordner> <Version> <Qt-Hauptversion>
rem
rem  Wird nach jedem Release-Build von installer\deploy.pri aufgerufen (auch: mingw32-make installer).
rem  Der Installer enthaelt NUR ViviaMealsCheat.exe (+ README, Lizenz) und kopiert sie in den Ordner
rem  einer vorhandenen ViviaMeals-Installation (Vorgabe C:\ViviaMeals); dort benutzt sie die Qt-DLLs
rem  von ViviaMeals. Die Verwaltungsdaten des Installers (Deinstallation) liegen in
rem  <ViviaMeals-Ordner>\ViviaMealsCheat, damit sie die Installation von ViviaMeals nicht beruehren.
rem
rem  Ergebnis:  installer\Output\ViviaMealsCheat-<Version>-Setup.exe
rem  Protokoll: installer\Output\ViviaMealsCheat-<Version>-Protokoll.txt
rem ---------------------------------------------------------------------------
setlocal EnableExtensions

set "EXE=%~f1"
set "BC=%~2"
set "WORK=%~f3"
set "VERSION=%~4"
set "QTMAJOR=%~5"
if "%BC%"=="-" set "BC="
if defined VM_BINARYCREATOR if not defined BC set "BC=%VM_BINARYCREATOR%"
if not defined QTMAJOR set "QTMAJOR=6"

set "INST=%~dp0"
if "%INST:~-1%"=="\" set "INST=%INST:~0,-1%"
set "PKGS=%WORK%\cheat-installer-packages"
set "PKG=%PKGS%\de.mbergmann.viviamealscheat"
set "CFG=%WORK%\cheat-installer-config"
set "OUT=%INST%\Output"
set "SETUP=%OUT%\ViviaMealsCheat-%VERSION%-Setup.exe"
set "LOG=%OUT%\ViviaMealsCheat-%VERSION%-Protokoll.txt"

if not exist "%OUT%" mkdir "%OUT%"
> "%LOG%" echo ViviaMealsCheat %VERSION% - Installer-Protokoll %DATE% %TIME%
call :log "ViviaMealsCheat: Installer %VERSION% wird vorbereitet (Qt %QTMAJOR%) ..."
if not exist "%EXE%" (
    call :log "error: ViviaMealsCheat.exe nicht gefunden: %EXE%"
    goto fail
)

rem ---- Paketbaum: meta + data (nur die .exe), config
if exist "%PKGS%" rmdir /s /q "%PKGS%"
if exist "%CFG%" rmdir /s /q "%CFG%"
mkdir "%PKG%\meta" "%PKG%\data" "%CFG%"
if errorlevel 1 goto fail
copy /y "%EXE%" "%PKG%\data\ViviaMealsCheat.exe" >nul || goto fail
copy /y "%INST%\..\README.md" "%PKG%\data\README.md" >nul || goto fail
copy /y "%INST%\..\..\..\LICENSE" "%PKG%\data\LICENSE.txt" >nul
for %%f in (license.txt zielordner.ui) do copy /y "%INST%\qtifw\meta\%%f" "%PKG%\meta\%%f" >nul || goto fail
for %%f in (controlscript.qs viviamealscheat.ico viviamealscheat.png logo.png banner.png watermark.png) do copy /y "%INST%\qtifw\config\%%f" "%CFG%\%%f" >nul || goto fail
rem Platzhalter @VERSION@, @DATE@, @QTMAJOR@ ersetzen
call :subst "%INST%\qtifw\config\config.xml.in" "%CFG%\config.xml" || goto fail
call :subst "%INST%\qtifw\meta\package.xml.in" "%PKG%\meta\package.xml" || goto fail
call :subst "%INST%\qtifw\meta\installscript.qs.in" "%PKG%\meta\installscript.qs" || goto fail

rem ---- binarycreator suchen
if not defined BC if defined QTIFWDIR if exist "%QTIFWDIR%\bin\binarycreator.exe" set "BC=%QTIFWDIR%\bin\binarycreator.exe"
if not defined BC for /d %%d in ("C:\Qt\Tools\QtInstallerFramework\*" "C:\Qt\QtIFW*" "%ProgramFiles%\Qt\QtIFW*") do if exist "%%~fd\bin\binarycreator.exe" set "BC=%%~fd\bin\binarycreator.exe"
if not defined BC for /f "delims=" %%b in ('where binarycreator.exe 2^>nul') do if not defined BC set "BC=%%b"
if not defined BC (
    call :log "warning: Qt Installer Framework (binarycreator.exe) nicht gefunden - es wurde KEIN Installer erstellt."
    call :log "warning: Abhilfe: Umgebungsvariable VM_BINARYCREATOR auf binarycreator.exe setzen."
    exit /b 0
)
if exist "%SETUP%" del /q "%SETUP%"
call :log "ViviaMealsCheat: %BC%"
"%BC%" --offline-only -c "%CFG%\config.xml" -p "%PKGS%" "%SETUP%" >> "%LOG%" 2>&1
if errorlevel 1 (
    call :log "error: binarycreator ist fehlgeschlagen - Einzelheiten im Protokoll."
    goto fail
)
if not exist "%SETUP%" goto fail
call :log "ViviaMealsCheat: Installer erstellt: %SETUP%"
exit /b 0

:fail
call :log "error: ViviaMealsCheat-Installer konnte nicht erstellt werden. Protokoll: %LOG%"
exit /b 1

:subst
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t = [IO.File]::ReadAllText('%~1', [Text.Encoding]::UTF8); $t = $t.Replace('@VERSION@','%VERSION%').Replace('@DATE@',(Get-Date -Format 'yyyy-MM-dd')).Replace('@QTMAJOR@','%QTMAJOR%'); [IO.File]::WriteAllText('%~2', $t, (New-Object Text.UTF8Encoding $false))" >> "%LOG%" 2>&1
if not exist "%~2" (
    call :log "error: %~2 konnte nicht erzeugt werden."
    exit /b 1
)
exit /b 0

:log
echo %~1
>> "%LOG%" echo %~1
exit /b 0
