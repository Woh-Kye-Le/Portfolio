@echo off
rem Ask the Database: remove generated files. Never touches your code, prompts or eval\results.md.
rem   clean.bat         Python caches and the generated database (both come back with build.bat)
rem   clean.bat runs    ...also the saved evaluation reports in eval\runs   (asks first)
rem   clean.bat all     ...also eval\runs and the .venv folder              (asks first)
rem Stop the web app and close other windows using this project before running it.
setlocal
cd /d "%~dp0"
set "MODE=%~1"
echo === Ask the Database: clean ===

for %%D in (. src tests eval db) do (
    if exist "%%D\__pycache__" rd /s /q "%%D\__pycache__"
)
if exist ".pytest_cache" rd /s /q ".pytest_cache"
echo Removed Python caches.

if exist "db\resort.db" (
    del /q "db\resort.db"
    echo Removed db\resort.db. Rebuild it with build.bat
)

if /i "%MODE%"=="runs" goto :runs
if /i "%MODE%"=="all" goto :all
echo.
echo Done. For more: clean.bat runs   or   clean.bat all
exit /b 0

:runs
call :confirm "Delete the saved evaluation reports in eval\runs? Copy any numbers you need into eval\results.md first." || exit /b 0
call :delruns
exit /b 0

:all
call :confirm "Delete eval\runs AND the .venv folder? You will run build.bat to set up again." || exit /b 0
call :delruns
if exist ".venv" (
    rd /s /q ".venv"
    echo Removed .venv. If your prompt still shows it, type: deactivate
)
exit /b 0

:delruns
if exist "eval\runs" (
    rd /s /q "eval\runs"
    echo Removed eval\runs
) else (
    echo No eval\runs folder to remove.
)
exit /b 0

:confirm
choice /c YN /n /m "%~1 [Y/N] "
if errorlevel 2 (
    echo Cancelled. Nothing more was deleted.
    exit /b 1
)
exit /b 0
