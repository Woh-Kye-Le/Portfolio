"""Tests for the cleaning and analysis helpers.

Run them with::

    make test
    # or
    python -m pytest -q
"""

from __future__ import annotations

import pandas as pd
import pytest

from src.analysis import (
    listening_by_hour,
    listening_by_month,
    peak_hour,
    summary_stats,
    top_tracks,
)
from src.clean import clean
from src.load import load_sample

TZ = "Asia/Singapore"


@pytest.fixture()
def raw() -> pd.DataFrame:
    """A tiny, deliberately messy frame with one duplicate and one junk row."""
    return pd.DataFrame(
        {
            "ts": [
                "2024-01-01T00:15:00Z",  # 08:15 local
                "2024-01-01T10:45:00Z",  # 18:45 local
                "2024-01-02T10:50:00Z",  # 18:50 local
                "2024-02-03T13:00:00Z",  # 21:00 local
                "2024-02-03T13:00:00Z",  # exact duplicate of the row above
                "2024-02-04T13:00:00Z",  # missing track name
                "2024-02-05T13:00:00Z",  # zero length play -> junk
            ],
            "track": ["Song A", "Song B", "Song B", "Song A", "Song A", "", "Song C"],
            "artist": [
                "Artist 1",
                "Artist 2",
                "Artist 2",
                "Artist 1",
                "Artist 1",
                "Artist 3",
                "Artist 4",
            ],
            "album": ["Album X", "Album Y", "Album Y", "Album X", "Album X", "Album Z", "Album W"],
            "ms_played": [180000, 200000, 240000, 300000, 300000, 120000, 0],
        }
    )


# --------------------------------------------------------------------------- #
# clean
# --------------------------------------------------------------------------- #
def test_clean_adds_derived_columns(raw: pd.DataFrame) -> None:
    df = clean(raw, tz=TZ)
    assert {"minutes", "date", "hour", "weekday", "month"} <= set(df.columns)
    assert df["minutes"].iloc[0] == pytest.approx(3.0)


def test_clean_drops_duplicates_and_zero_length_plays(raw: pd.DataFrame) -> None:
    df = clean(raw, tz=TZ)
    # 7 raw rows, minus 1 exact duplicate, minus 1 zero-length play
    assert len(df) == 5


def test_clean_converts_to_the_requested_timezone(raw: pd.DataFrame) -> None:
    utc = clean(raw, tz="UTC")
    local = clean(raw, tz=TZ)
    first_song_b = utc["track"].eq("Song B")
    assert utc.loc[first_song_b, "hour"].iloc[0] == 10
    assert local.loc[first_song_b, "hour"].iloc[0] == 18


def test_clean_replaces_blank_names_with_unknown(raw: pd.DataFrame) -> None:
    df = clean(raw, tz=TZ)
    assert df.loc[df["artist"].eq("Artist 3"), "track"].iloc[0] == "Unknown"


def test_clean_is_sorted_by_timestamp(raw: pd.DataFrame) -> None:
    df = clean(raw, tz=TZ)
    assert df["ts"].is_monotonic_increasing
    assert df["ts"].is_unique


def test_clean_requires_timestamp_and_duration() -> None:
    with pytest.raises(ValueError):
        clean(pd.DataFrame({"track": ["Song A"]}))


# --------------------------------------------------------------------------- #
# analysis
# --------------------------------------------------------------------------- #
def test_summary_stats_counts(raw: pd.DataFrame) -> None:
    stats = summary_stats(clean(raw, tz=TZ))
    assert stats["total_plays"] == 5
    assert stats["unique_artists"] == 3


def test_listening_by_hour_covers_every_hour(raw: pd.DataFrame) -> None:
    hourly = listening_by_hour(clean(raw, tz=TZ))
    assert len(hourly) == 24
    assert hourly["plays"].sum() == 5


def test_listening_by_month_is_sorted(raw: pd.DataFrame) -> None:
    monthly = listening_by_month(clean(raw, tz=TZ))
    assert monthly["month"].tolist() == ["2024-01", "2024-02"]


def test_peak_hour(raw: pd.DataFrame) -> None:
    # 200000 + 240000 ms at 18:00 beats 300000 + 120000 ms at 21:00
    assert peak_hour(clean(raw, tz=TZ)) == 18


def test_top_tracks_counts_replays(raw: pd.DataFrame) -> None:
    top = top_tracks(clean(raw, tz=TZ), n=5)
    assert top["plays"].max() == 2
    first = top.iloc[0]
    assert first["label"] == f"{first['track']} - {first['artist']}"


# --------------------------------------------------------------------------- #
# the committed sample dataset
# --------------------------------------------------------------------------- #
def test_sample_dataset_loads_and_cleans() -> None:
    df = clean(load_sample(), tz=TZ)
    assert not df.empty
    assert {"ts", "track", "artist", "ms_played", "minutes", "hour"} <= set(df.columns)
    assert df["minutes"].gt(0).all()
