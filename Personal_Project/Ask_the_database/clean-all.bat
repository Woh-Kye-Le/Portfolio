@echo off
rem Ask the Database: remove EVERYTHING generated, for a completely fresh start.
rem Removes: Python caches, db\resort.db, eval\runs (saved eval reports) and the .venv folder.
rem Never touches: your code, prompts, eval\results.md, README or the data-quality report.
rem Asks for confirmation first. Equivalent to: clean.bat all
rem Stop the web app and run "deactivate" first if your prompt shows (.venv).
rem After this, run build.bat to set everything up again.
call "%~dp0clean.bat" all
exit /b %errorlevel%
