@echo off
REM ---------------------------------------------------------------------------
REM  Visual Studio startup target (see launch.vs.json -> "Set as Startup Item").
REM
REM  Creates/updates .venv through the Makefile, then launches the Streamlit
REM  dashboard, so pressing F5 in Visual Studio is all you need - even on a
REM  fresh clone with no virtual environment yet.
REM ---------------------------------------------------------------------------
setlocal
cd /d "%~dp0"

where nmake >nul 2>nul
if not errorlevel 1 goto :nmake

where make >nul 2>nul
if not errorlevel 1 goto :make

echo Neither nmake nor make was found on PATH.
echo From a Developer Command Prompt run:  nmake run
pause
exit /b 1

:nmake
nmake run
exit /b %ERRORLEVEL%

:make
make run
exit /b %ERRORLEVEL%
