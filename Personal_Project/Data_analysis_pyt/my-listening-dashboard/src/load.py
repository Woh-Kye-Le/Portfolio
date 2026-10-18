"""Read raw listening-history exports into a single pandas DataFrame.

Two input shapes are supported:

1. Spotify "Extended streaming history" JSON files. Each file is a list of
   objects whose keys are renamed to the canonical names below.
2. Flat CSV files that already use the canonical header
   ``ts, track, artist, album, ms_played``.
"""

from __future__ import annotations

import json
from pathlib import Path

import pandas as pd

PROJECT_ROOT = Path(__file__).resolve().parents[1]
RAW_DIR = PROJECT_ROOT / "data" / "raw"
SAMPLE_DIR = PROJECT_ROOT / "data" / "sample"

# Spotify extended-history JSON key -> canonical column name.
JSON_KEY_MAP = {
    "ts": "ts",
    "ms_played": "ms_played",
    "master_metadata_track_name": "track",
    "master_metadata_album_artist_name": "artist",
    "master_metadata_album_album_name": "album",
}

CANONICAL_COLUMNS = ["ts", "track", "artist", "album", "ms_played"]


def find_files(directory: Path | str) -> list[Path]:
    """Return every ``.json`` / ``.csv`` file under *directory*, sorted."""
    directory = Path(directory)
    if not directory.is_dir():
        return []

    files: list[Path] = []
    for pattern in ("*.json", "*.csv"):
        files.extend(sorted(directory.rglob(pattern)))
    return files


def load_json_file(path: Path | str) -> pd.DataFrame:
    """Load one Spotify extended-history JSON file."""
    with open(path, "r", encoding="utf-8") as handle:
        records = json.load(handle)

    if isinstance(records, dict):  # a single play in a bare object
        records = [records]

    frame = pd.DataFrame(records)
    return frame.rename(columns=JSON_KEY_MAP)


def load_csv_file(path: Path | str) -> pd.DataFrame:
    """Load one canonical CSV file."""
    return pd.read_csv(path)


def load_raw(directory: Path | str = RAW_DIR) -> pd.DataFrame:
    """Load and concatenate every supported export file in *directory*."""
    files = find_files(directory)
    if not files:
        raise FileNotFoundError(
            f"No .json or .csv export files found in {directory}.\n"
            "Drop your export there (see README) or use the bundled sample."
        )

    frames = []
    for path in files:
        loader = load_json_file if path.suffix.lower() == ".json" else load_csv_file
        frame = loader(path)
        frame["source_file"] = path.name
        frames.append(frame)

    return pd.concat(frames, ignore_index=True)


def load_sample() -> pd.DataFrame:
    """Load the small, anonymised sample dataset committed with the repo."""
    return load_raw(SAMPLE_DIR)
