"""Answer the dashboard's questions about a cleaned listening-history frame.

Every function takes the DataFrame returned by :func:`src.clean.clean` and
returns either a plain DataFrame (ready for a chart) or a value.
"""

from __future__ import annotations

import pandas as pd

WEEKDAY_ORDER = [
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday",
    "Sunday",
]

HOURS_PER_DAY = list(range(24))


def summary_stats(df: pd.DataFrame) -> dict:
    """Headline numbers shown as metric cards."""
    return {
        "total_plays": int(len(df)),
        "total_minutes": float(df["minutes"].sum()),
        "unique_tracks": int(df["track"].nunique()),
        "unique_artists": int(df["artist"].nunique()),
        "first_play": df["ts"].min(),
        "last_play": df["ts"].max(),
        "days": int(df["date"].nunique()),
    }


def listening_by_hour(df: pd.DataFrame) -> pd.DataFrame:
    """How much was played during each hour of the day (local time)."""
    grouped = (
        df.groupby("hour")
        .agg(plays=("track", "size"), minutes=("minutes", "sum"))
        .reindex(HOURS_PER_DAY, fill_value=0)
    )
    grouped["hours"] = grouped["minutes"] / 60
    return grouped.reset_index().astype({"hour": int, "plays": int})


def listening_by_weekday(df: pd.DataFrame) -> pd.DataFrame:
    """How much was played on each day of the week."""
    grouped = (
        df.groupby("weekday")
        .agg(plays=("track", "size"), minutes=("minutes", "sum"))
        .reindex(WEEKDAY_ORDER, fill_value=0)
    )
    grouped["hours"] = grouped["minutes"] / 60
    return grouped.reset_index().astype({"plays": int})


def listening_by_month(df: pd.DataFrame) -> pd.DataFrame:
    """Month-by-month listening trend."""
    grouped = (
        df.groupby("month")
        .agg(plays=("track", "size"), minutes=("minutes", "sum"))
        .sort_index()
    )
    grouped["hours"] = grouped["minutes"] / 60
    return grouped.reset_index().astype({"plays": int})


def top_tracks(df: pd.DataFrame, n: int = 10) -> pd.DataFrame:
    """Most-replayed tracks, with a ready-to-plot ``label`` column."""
    top = (
        df.groupby(["track", "artist"])
        .agg(plays=("track", "size"), minutes=("minutes", "sum"))
        .sort_values(["plays", "minutes"], ascending=False)
        .head(n)
        .reset_index()
    )
    top["label"] = top["track"] + " - " + top["artist"]
    return top.astype({"plays": int})


def top_artists(df: pd.DataFrame, n: int = 10) -> pd.DataFrame:
    """Most-replayed artists, with a ready-to-plot ``label`` column."""
    top = (
        df.groupby("artist")
        .agg(plays=("track", "size"), minutes=("minutes", "sum"))
        .sort_values(["plays", "minutes"], ascending=False)
        .head(n)
        .reset_index()
    )
    top["label"] = top["artist"]
    return top.astype({"plays": int})


def peak_hour(df: pd.DataFrame) -> int:
    """The hour of the day with the most listening (0-23)."""
    hourly = listening_by_hour(df)
    return int(hourly.loc[hourly["minutes"].idxmax(), "hour"])


def peak_weekday(df: pd.DataFrame) -> str:
    """The weekday with the most listening."""
    weekly = listening_by_weekday(df)
    return str(weekly.loc[weekly["minutes"].idxmax(), "weekday"])
