"""Streamlit entry point for the My Listening Dashboard.

Run it with::

    streamlit run app.py

or simply ``make run``.
"""

from __future__ import annotations

import pandas as pd
import streamlit as st

from src import analysis, charts
from src.clean import clean
from src.load import RAW_DIR, find_files, load_raw, load_sample

st.set_page_config(page_title="My Listening Dashboard", page_icon="🎧", layout="wide")


# --------------------------------------------------------------------------- #
# Data loading (cached so the app stays snappy while you tweak the layout)
# --------------------------------------------------------------------------- #
@st.cache_data(show_spinner="Crunching your listening history...")
def load_dashboard_data(source: str, tz: str | None) -> pd.DataFrame:
    """Load the chosen source and return a cleaned DataFrame."""
    raw = load_sample() if source == "sample" else load_raw(RAW_DIR)
    return clean(raw, tz=tz)


def format_hours(minutes: float) -> str:
    return f"{minutes / 60:,.0f} h"


def main() -> None:
    st.title("🎧 My Listening Dashboard")
    st.caption(
        "A pandas + Plotly look at my own streaming history. "
        "Drop your export into `data/raw/`, or explore the bundled sample."
    )

    # ----------------------------------------------------------------- content
    st.sidebar.header("Settings")
    source = st.sidebar.radio(
        "Data source",
        options=["sample", "raw"],
        format_func=lambda value: (
            "Bundled sample" if value == "sample" else "My export (data/raw)"
        ),
    )

    if source == "raw" and not find_files(RAW_DIR):
        st.error(
            f"No export files found in `{RAW_DIR}`.\n\n"
            "Copy your Spotify *Extended streaming history* JSON (or a canonical CSV) "
            "into that folder and press **R** to reload."
        )
        st.stop()

    tz_input = st.sidebar.text_input("Timezone (blank = this machine)", value="")
    tz = tz_input.strip() or None

    top_n = st.sidebar.slider("Top N tracks / artists", 5, 25, 10, step=5)

    df = load_dashboard_data(source, tz)
    if df.empty:
        st.warning("The cleaned dataset is empty - nothing to show.")
        st.stop()

    stats = analysis.summary_stats(df)
    peak_hour = analysis.peak_hour(df)
    peak_day = analysis.peak_weekday(df)

    # ------------------------------------------------------------------ metrics
    columns = st.columns(5)
    columns[0].metric("Hours listened", format_hours(stats["total_minutes"]))
    columns[1].metric("Plays", f"{stats['total_plays']:,}")
    columns[2].metric("Unique tracks", f"{stats['unique_tracks']:,}")
    columns[3].metric("Unique artists", f"{stats['unique_artists']:,}")
    columns[4].metric("Days covered", f"{stats['days']:,}")

    st.info(
        f"You listen the most around **{peak_hour:02d}:00** "
        f"and on **{peak_day}s**. "
        f"Data spans {stats['first_play']:%d %b %Y} → {stats['last_play']:%d %b %Y}."
    )

    # --------------------------------------------------------------------- tabs
    tab_overview, tab_time, tab_repeat = st.tabs(
        ["⏰ Time of day", "📈 Over time", "🔁 What I replay"]
    )

    hourly = analysis.listening_by_hour(df)
    weekday = analysis.listening_by_weekday(df)
    monthly = analysis.listening_by_month(df)
    tracks = analysis.top_tracks(df, top_n)
    artists = analysis.top_artists(df, top_n)

    with tab_overview:
        left, right = st.columns(2)
        left.plotly_chart(charts.hourly_bar(hourly), use_container_width=True)
        right.plotly_chart(charts.weekday_bar(weekday), use_container_width=True)
        with st.expander("Hourly numbers"):
            st.dataframe(
                hourly[["hour", "plays", "hours"]].style.format({"hours": "{:.1f}"}),
                use_container_width=True,
                hide_index=True,
            )

    with tab_time:
        st.plotly_chart(charts.monthly_line(monthly), use_container_width=True)
        with st.expander("Monthly numbers"):
            st.dataframe(
                monthly[["month", "plays", "hours"]].style.format({"hours": "{:.1f}"}),
                use_container_width=True,
                hide_index=True,
            )

    with tab_repeat:
        left, right = st.columns(2)
        left.plotly_chart(
            charts.top_bar(tracks, "Most-replayed tracks"), use_container_width=True
        )
        right.plotly_chart(
            charts.top_bar(artists, "Most-replayed artists"), use_container_width=True
        )


if __name__ == "__main__":
    main()
