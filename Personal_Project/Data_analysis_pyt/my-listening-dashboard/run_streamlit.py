"""Visual Studio-friendly launcher for the Streamlit dashboard."""

from __future__ import annotations

import sys
from pathlib import Path


def main() -> int:
    app_path = Path(__file__).with_name("app.py")
    sys.argv = ["streamlit", "run", str(app_path)]

    try:
        from streamlit.web.cli import main as streamlit_main
    except ImportError:
        from streamlit.cli import main as streamlit_main

    return streamlit_main()


if __name__ == "__main__":
    raise SystemExit(main())
