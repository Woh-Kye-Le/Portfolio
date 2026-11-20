@echo off
REM ===============================================
REM CI Build Script (used by Jenkins, runnable locally)
REM Usage: tools\ci\build.bat [Debug|Release|Package]
REM ===============================================
setlocal

REM Release unless a config is given (Jenkins passes Debug for the test stage)
set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Release

REM %~dp0 is this script's folder (tools\ci\), so two levels up is the repo root
set ROOT=%~dp0..\..
set BUILD_DIR=%ROOT%\apps\desktop\build\ci-%CONFIG%

echo ===============================================
echo   Atelier3D CI build - %CONFIG%
echo ===============================================
REM Prints the version for the log, and stops early if CMake isn't installed at all
cmake --version || exit /b 1

REM Always start from a clean build directory so every run is repeatable
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

echo [1/2] Configuring...
REM No -G: CMake picks the newest installed Visual Studio generator
REM -A x64: 64-bit only (FMOD and the other prebuilt libraries in libraries\ are x64)
cmake -S "%ROOT%\apps\desktop" -B "%BUILD_DIR%" -A x64 -DATELIER_BUILD_TESTS=ON
if errorlevel 1 (
    echo ERROR: CMake configuration failed
    exit /b 1
)

echo [2/2] Building...
REM Visual Studio is multi-config, so the config is picked here instead of at configure time.
REM The part after -- goes to MSBuild: minimal output keeps the Jenkins log readable.
cmake --build "%BUILD_DIR%" --config %CONFIG% --parallel -- /nologo /verbosity:minimal
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

echo Build succeeded: %BUILD_DIR%
exit /b 0
