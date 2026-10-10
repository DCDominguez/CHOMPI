@echo off
setlocal
cd /d "%~dp0"
rem Development kit: Python, MIDI and audio packages are bundled. Nothing to install.
set "PY=%~dp0..\python\python.exe"
if exist "%PY%" goto run

rem Source checkout: an isolated environment from Python 3.10-3.12
rem (python-rtmidi has no Windows build for Python 3.13 or newer).
set "PY=.bridge-venv\Scripts\python.exe"
if exist "%PY%" goto packages
set "BASE="
for %%V in (3.12 3.11 3.10) do if not defined BASE (py -%%V -c "pass" >nul 2>&1 && set "BASE=py -%%V")
if not defined BASE (python -c "import sys; sys.exit(not (3,10) <= sys.version_info[:2] <= (3,12))" >nul 2>&1 && set "BASE=python")
if not defined BASE (
  echo This needs Python 3.10, 3.11 or 3.12 ^(the MIDI library has no Windows build for 3.13+^).
  echo Easiest: use the Forge development kit, which includes Python. Otherwise install Python 3.12 from python.org.
  pause
  exit /b 1
)
%BASE% -m venv .bridge-venv
if errorlevel 1 goto failed
:packages
"%PY%" -c "import mido, rtmidi, numpy, sounddevice" >nul 2>&1
if errorlevel 1 "%PY%" -m pip install --use-feature=truststore -r requirements.txt -r bridge-requirements.txt
if errorlevel 1 goto failed

:run
echo Keep this window open while using the bridge. Ctrl+C stops the server.
if exist "..\build\forge_probe.exe" (
  "%PY%" forge_web.py --open --probe "..\build\forge_probe.exe"
) else (
  "%PY%" forge_web.py --open
)
if errorlevel 1 goto failed
exit /b 0
:failed
echo Bridge could not start. Review the error above.
pause
exit /b 1
