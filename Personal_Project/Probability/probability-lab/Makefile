# ============================================================================
#  probability-lab - Makefile
# ----------------------------------------------------------------------------
#  Tasks for the Streamlit lab.  Written so that it works with BOTH GNU Make
#  and Microsoft NMake (nmake.exe ships with Visual Studio), so a Visual Studio
#  "Makefile Project" can build and run everything from the IDE.
#
#  Targets
#    all      (default) create .venv and install dependencies
#    setup    same as all
#    run      launch the Streamlit app
#    test     run the pytest suite
#    clean    delete .venv and the Python caches
#    rebuild  clean, then all
#    help     list the targets
#
#  Visual Studio
#    File > New > Project > Makefile Project, then set
#        Build command line   :  nmake all
#        Rebuild command line :  nmake rebuild
#        Clean command line   :  nmake clean
#    Ctrl+Shift+B builds, and "nmake run" / "nmake test" work from the
#    integrated terminal.
#
#  Interpreter overrides: this Makefile assumes the Windows "py" launcher.
#        nmake PYTHON=python all        (python.exe already on PATH)
#        make VENV_PY=.venv/bin/python all   (POSIX venv layout)
# ============================================================================

PYTHON  = py
VENV    = .venv
VENV_PY = $(VENV)\Scripts\python.exe
PIP     = $(VENV_PY) -m pip
APP     = app.py

# Recreated whenever requirements.txt is newer, so dependencies stay in sync
# without reinstalling on every single invocation.
STAMP   = $(VENV)\.installed

# 'all' is the first target, so it is the default goal for make and nmake.
all: $(STAMP)

setup: $(STAMP)

$(VENV_PY):
	$(PYTHON) -m venv $(VENV)

$(STAMP): requirements.txt $(VENV_PY)
	$(PIP) install --upgrade pip
	$(PIP) install -r requirements.txt
	$(VENV_PY) -c "import pathlib; pathlib.Path(r'$(STAMP)').write_text('ok')"
	@echo Environment ready. Run "make run" to start the lab.

run: $(STAMP)
	$(VENV_PY) -m streamlit run $(APP)

test: $(STAMP)
	$(VENV_PY) -m pytest -q

clean:
	$(PYTHON) -c "import pathlib,shutil; [shutil.rmtree(p, ignore_errors=True) for p in list(pathlib.Path('.').rglob('__pycache__')) + list(pathlib.Path('.').rglob('.ipynb_checkpoints')) + [pathlib.Path('$(VENV)'), pathlib.Path('.pytest_cache')]]"
	@echo Removed $(VENV) and Python caches.

rebuild: clean all

help:
	@echo Targets: all setup run test clean rebuild help
