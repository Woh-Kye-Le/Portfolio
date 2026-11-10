@echo off
rem Ask the Database: one-command setup. Safe to run again and again.
rem   build.bat         create .venv, install packages, build the database, run the tests
rem   build.bat app     ...then open the web app
rem   build.bat eval    ...then run the evaluation for prompts v1 and v2 and show the history
rem From PowerShell run it as:  .\build.bat app
rem Your API key is NOT stored here. Set it in the same window before using app or eval.
setlocal
cd /d "%~dp0"
echo === Ask the Database: build ===

where python >nul 2>nul
if errorlevel 1 (
    echo ERROR: Python was not found. Install Python 3.10 or newer from python.org and tick Add to PATH.
    exit /b 1
)
python -c "import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)" >nul 2>nul
if errorlevel 1 (
    echo ERROR: Python 3.10 or newer is required.
    exit /b 1
)

if not exist ".venv\Scripts\python.exe" (
    echo [1/4] Creating virtual environment...
    python -m venv .venv || goto :fail
) else (
    echo [1/4] Virtual environment found.
)
set "PY=.venv\Scripts\python.exe"

echo [2/4] Installing packages...
"%PY%" -m pip install -q -r requirements.txt || goto :fail

if not exist "db\resort.db" (
    echo [3/4] Building the synthetic database...
    "%PY%" -m db.seed || goto :fail
) else (
    echo [3/4] Database found.
)

echo [4/4] Running the tests...
"%PY%" -m pytest -q || goto :fail
"%PY%" -m eval.run_eval --dry-run --no-save | findstr /b "Accuracy"

echo.
echo Build OK.
if /i "%~1"=="app" goto :app
if /i "%~1"=="eval" goto :eval
echo.
echo Next:  .\build.bat app    opens the web app
echo        .\build.bat eval   runs the evaluation
exit /b 0

:app
call :checkkey || exit /b 1
echo.
echo Starting the app. Press Ctrl+C in this window to stop it.
"%PY%" -m streamlit run app.py
exit /b 0

:eval
call :checkkey || exit /b 1
echo.
echo === Evaluation: prompt v1 ===
"%PY%" -m eval.run_eval --prompt v1 || goto :fail
echo.
echo === Evaluation: prompt v2 ===
"%PY%" -m eval.run_eval --prompt v2 || goto :fail
echo.
"%PY%" -m eval.run_eval --history
exit /b 0

:checkkey
set "PROVIDER=%ASK_DB_PROVIDER%"
if not defined PROVIDER set "PROVIDER=gemini"
if /i "%PROVIDER%"=="ollama" exit /b 0
if /i "%PROVIDER%"=="anthropic" goto :key_anthropic
if defined GEMINI_API_KEY exit /b 0
if defined GOOGLE_API_KEY exit /b 0
echo.
echo No Gemini key found in this window. Set it first, then run this again:
echo     PowerShell:  $env:GEMINI_API_KEY="your-key"
echo     cmd.exe:     set GEMINI_API_KEY=your-key
exit /b 1
:key_anthropic
if defined ANTHROPIC_API_KEY exit /b 0
echo.
echo No Anthropic key found in this window. Set it first, then run this again:
echo     PowerShell:  $env:ANTHROPIC_API_KEY="your-key"
echo     cmd.exe:     set ANTHROPIC_API_KEY=your-key
exit /b 1

:fail
echo.
echo Build FAILED. Read the message above. Nothing was deleted.
exit /b 1
