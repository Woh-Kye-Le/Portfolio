@echo off
rem ===============================================
rem Packages a Windows build of Atelier3D into dist\Atelier3D-win64.zip (the file the portfolio links to).
rem Windows counterpart of package.sh. Run from anywhere; it works out the project root itself.
rem
rem   1) Build first, in "Developer PowerShell for VS" at the project root:   tools\ci\build.bat Release
rem   2) Then:                                                              tools\ci\package.bat
rem
rem Optional argument: the folder that holds AtelierEditor.exe, if the build put it somewhere else, e.g.
rem   tools\ci\package.bat apps\desktop\build\dev\bin\Release
rem Output: apps\desktop\build\dist\Atelier3D-win64.zip
rem ===============================================
setlocal EnableExtensions

pushd "%~dp0..\.." || exit /b 1
set "ROOT=%CD%"
set "DIST=%ROOT%\apps\desktop\build\dist"
set "PKG=%DIST%\Atelier3D"
set "ZIP=%DIST%\Atelier3D-win64.zip"

set "BIN=%~1"
if "%BIN%"=="" set "BIN=%ROOT%\apps\desktop\build\ci-Release\bin\Release"

if not exist "%BIN%\AtelierEditor.exe" (
    echo ERROR: %BIN%\AtelierEditor.exe not found. Build first with: tools\ci\build.bat Release
    echo        If the build is somewhere else, pass that folder: tools\ci\package.bat ^<folder with AtelierEditor.exe^>
    popd & exit /b 1
)
if not exist "%BIN%\AtelierPlayer.exe" (
    echo ERROR: %BIN%\AtelierPlayer.exe not found. Without it the editor cannot export games.
    popd & exit /b 1
)
if not exist "%BIN%\assets\shaders\Builtin\Standard3D.vert" (
    echo ERROR: %BIN%\assets\shaders\Builtin\Standard3D.vert not found. Without assets\ nothing is drawn.
    popd & exit /b 1
)
if not exist "%BIN%\templates\demos\data\scenes\rendering.json" (
    echo ERROR: %BIN%\templates\demos\data\scenes\rendering.json not found. New Game needs the templates folder.
    popd & exit /b 1
)
if not exist "%BIN%\AtelierAssetTool.exe" echo WARNING: AtelierAssetTool.exe not found; glTF, zip and most model imports will not work.
if not exist "%BIN%\fmod.dll" echo WARNING: fmod.dll not found; the editor will have no sound.

rem Start from a clean package folder
if exist "%PKG%" rmdir /s /q "%PKG%"
if exist "%ZIP%" del /q "%ZIP%"
mkdir "%PKG%" || (popd & exit /b 1)

rem Only the three shipped programs; AtelierTests.exe (unit tests) is not part of the package
for %%E in (AtelierEditor AtelierPlayer AtelierAssetTool) do (
    if exist "%BIN%\%%E.exe" copy /y "%BIN%\%%E.exe" "%PKG%\" >nul
)
copy /y "%BIN%\*.dll" "%PKG%\" >nul
xcopy "%BIN%\assets" "%PKG%\assets\" /E /I /Q /Y >nul || (echo ERROR: copying assets failed & popd & exit /b 1)
xcopy "%BIN%\templates" "%PKG%\templates\" /E /I /Q /Y >nul || (echo ERROR: copying templates failed & popd & exit /b 1)

rem tar ships with Windows 10/11 and writes a normal .zip with -a
tar -a -c -f "%ZIP%" -C "%DIST%" Atelier3D || (echo ERROR: could not create the zip & popd & exit /b 1)

echo.
echo Package created: %ZIP%
dir "%ZIP%" | find "Atelier3D-win64.zip"
echo Test it: unzip it into a new folder, run AtelierEditor.exe, then upload the zip to the GitHub release.
popd
endlocal
