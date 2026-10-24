"""Visual Studio-friendly launcher for the Probability Lab app."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path


def ensure_environment(script_dir: Path) -> None:
    """Match the Makefile's setup step before launching Streamlit."""
    env = os.environ.copy()
    env.setdefault("MPLCONFIGDIR", str(script_dir / ".mplconfig"))

    for builder in ("nmake", "make"):
        if shutil.which(builder):
            subprocess.check_call([builder, "all"], cwd=script_dir, env=env)
            return

    venv_dir = script_dir / ".venv"
    venv_python = venv_dir / "Scripts" / "python.exe"
    stamp = venv_dir / ".installed"
    requirements = script_dir / "requirements.txt"

    if not venv_python.exists():
        subprocess.check_call([sys.executable, "-m", "venv", str(venv_dir)], cwd=script_dir, env=env)

    if not stamp.exists() or stamp.stat().st_mtime < requirements.stat().st_mtime:
        subprocess.check_call([str(venv_python), "-m", "pip", "install", "--upgrade", "pip"], cwd=script_dir, env=env)
        subprocess.check_call([str(venv_python), "-m", "pip", "install", "-r", str(requirements)], cwd=script_dir, env=env)
        stamp.write_text("ok", encoding="utf-8")


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    venv_python = script_dir / ".venv" / "Scripts" / "python.exe"
    env = os.environ.copy()
    env.setdefault("MPLCONFIGDIR", str(script_dir / ".mplconfig"))

    ensure_environment(script_dir)

    # If Visual Studio launches this with a global interpreter, hop into the
    # project venv so the Start button behaves like "nmake run".
    if venv_python.exists() and Path(sys.executable).resolve() != venv_python.resolve():
        return subprocess.call([str(venv_python), str(Path(__file__).resolve())], cwd=script_dir, env=env)

    app_path = script_dir / "app.py"
    sys.argv = ["streamlit", "run", str(app_path)]

    try:
        from streamlit.web.cli import main as streamlit_main
    except ImportError:
        from streamlit.cli import main as streamlit_main

    return streamlit_main()


if __name__ == "__main__":
    raise SystemExit(main())
