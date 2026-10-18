"""Turn a raw export into a tidy, typed DataFrame that is ready for analysis.

Cleaning rules
--------------
* column names are lower-cased and stripped
* ``ts`` is parsed as UTC and converted to a local timezone
* blank / missing track, artist and album names become ``"Unknown"``
* rows without a usable timestamp or with ``ms_played <= 0`` are dropped
* exact replays of the same track at the same instant are de-duplicated
* derived columns (minutes, date, hour, weekday, month) are added
"""

from __future__ import annotations

from datetime import datetime

import pandas as pd

CLEAN_COLUMNS = [
    "ts",
    "track",
    "artist",
    "album",
    "ms_played",
    "minutes",
    "date",
    "hour",
    "weekday",
    "month",
]

TEXT_COLUMNS = ("track", "artist", "album")
UNKNOWN = "Unknown"


def clean(df: pd.DataFrame, tz: str | None = None) -> pd.DataFrame:
    """Return a cleaned copy of a raw listening-history frame.

    Parameters
    ----------
    df:
        Raw frame, typically from :func:`src.load.load_raw`.
    tz:
        Timezone used to derive the local ``hour`` / ``weekday`` / ``month``
        columns. ``None`` uses the timezone of the machine running the app.
    """
    df = df.copy()
    df.columns = [str(column).strip().lower() for column in df.columns]

    missing = {"ts", "ms_played"} - set(df.columns)
    if missing:
        raise ValueError(f"Missing required column(s): {sorted(missing)}")

    df = _fill_text_columns(df)
    df["ts"] = _to_local_datetime(df["ts"], tz)
    df["ms_played"] = pd.to_numeric(df["ms_played"], errors="coerce")

    # Keep only rows we can reason about.
    df = df.dropna(subset=["ts", "ms_played"])
    df = df[df["ms_played"] > 0]
    df = df[df["track"].ne(UNKNOWN) | df["artist"].ne(UNKNOWN)]
    df = df.drop_duplicates(subset=["ts", "track", "artist"])

    # Derived columns used by the analysis functions.
    df["minutes"] = df["ms_played"] / 60_000
    df["date"] = df["ts"].dt.date
    df["hour"] = df["ts"].dt.hour.astype("int16")
    df["weekday"] = df["ts"].dt.day_name()
    df["month"] = df["ts"].dt.strftime("%Y-%m")

    df = df.sort_values("ts").reset_index(drop=True)
    return df[CLEAN_COLUMNS]


def _fill_text_columns(df: pd.DataFrame) -> pd.DataFrame:
    for column in TEXT_COLUMNS:
        if column not in df.columns:
            df[column] = UNKNOWN
        values = df[column].fillna(UNKNOWN).astype(str).str.strip()
        df[column] = values.mask(values.eq(""), UNKNOWN)
    return df


def _to_local_datetime(series: pd.Series, tz: str | None) -> pd.Series:
    timestamps = pd.to_datetime(series, errors="coerce", utc=True)
    if tz is None:
        tz = datetime.now().astimezone().tzinfo
    return timestamps.dt.tz_convert(tz)
