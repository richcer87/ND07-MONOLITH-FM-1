@echo off
rem ND-07 // MONOLITH: install on the FM-1 with a double click (Windows).
rem   Double click: downloads the latest build of main (GitHub Pages) and installs it.
rem   Drag a .fwsc onto this file: installs that one.
rem Needs Python 3 (python.org); the first time it installs mido and python-rtmidi. Keep it next to fm1_install.py.
setlocal
cd /d "%~dp0"
title ND-07 // MONOLITH - instalador
set "URL=https://richcer87.github.io/ND07-MONOLITH-FM-1/fw/nd07.fwsc"
set "PKG=%~1"
if not "%PKG%"=="" goto have_pkg
echo Descargando la ultima compilacion...
set "PKG=%TEMP%\nd07-latest.fwsc"
powershell -NoProfile -Command "try { Invoke-WebRequest -UseBasicParsing '%URL%' -OutFile '%PKG%' } catch { exit 1 }"
if errorlevel 1 goto no_download
:have_pkg
set "PY="
where py >nul 2>nul && set "PY=py"
if not defined PY where python >nul 2>nul && set "PY=python"
if not defined PY goto no_python
%PY% -c "import mido, rtmidi" >nul 2>nul
if errorlevel 1 (
  echo Instalando mido y python-rtmidi ^(solo la primera vez^)...
  %PY% -m pip install --user --quiet mido python-rtmidi
)
echo.
echo Conecta el FM-1 por USB y cierra REAPER u otros programas MIDI.
echo.
%PY% "%~dp0fm1_install.py" "%PKG%"
echo.
pause
exit /b
:no_download
echo No pude descargar la ultima compilacion (%URL%).
echo Arrastra un archivo .fwsc sobre este instalador, o usa la pagina install.html.
pause
exit /b 1
:no_python
echo No encontre Python. Instalalo desde https://www.python.org (marca "Add python.exe to PATH") y vuelve a intentar.
echo O usa la pagina del instalador en Chrome: https://richcer87.github.io/ND07-MONOLITH-FM-1/install.html
pause
exit /b 1
