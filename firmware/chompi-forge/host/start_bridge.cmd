@echo off
setlocal
cd /d "%~dp0"
python --version >nul 2>&1
if errorlevel 1 (
  echo Install Python 3.10 or newer with Python on PATH, then run this launcher again.
  pause
  exit /b 1
)
if not exist ".bridge-venv\Scripts\python.exe" python -m venv .bridge-venv
if errorlevel 1 goto failed
".bridge-venv\Scripts\python.exe" -c "import mido, rtmidi" >nul 2>&1
if errorlevel 1 ".bridge-venv\Scripts\python.exe" -m pip install --use-feature=truststore -r requirements.txt
if errorlevel 1 goto failed
echo Keep this window open while using the bridge. Ctrl+C stops the server.
if exist "..\build\forge_probe.exe" (
  ".bridge-venv\Scripts\python.exe" forge_web.py --open --probe "..\build\forge_probe.exe"
) else (
  ".bridge-venv\Scripts\python.exe" forge_web.py --open
)
if errorlevel 1 goto failed
exit /b 0
:failed
echo Bridge could not start. Review the error above.
pause
exit /b 1
