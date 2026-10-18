"""Turn analysis results into Plotly figures."""

from __future__ import annotations

import pandas as pd
import plotly.express as px
import plotly.graph_objects as go

from src.analysis import WEEKDAY_ORDER

ACCENT = "#1DB954"  # a familiar shade of green

_LAYOUT = dict(
    template="plotly_white",
    margin=dict(t=60, b=20, l=10, r=20),
    title_x=0.01,
    font=dict(size=14),
)


def hourly_bar(hourly: pd.DataFrame) -> go.Figure:
    """Bars for each hour of the day."""
    figure = px.bar(
        hourly,
        x="hour",
        y="hours",
        color="hours",
        color_continuous_scale="Greens",
        labels={"hour": "Hour of day", "hours": "Hours listened"},
        title="What time of day do I listen the most?",
    )
    figure.update_xaxes(dtick=1, title="Hour of day")
    figure.update_layout(coloraxis_showscale=False, **_LAYOUT)
    return figure


def weekday_bar(weekday: pd.DataFrame) -> go.Figure:
    """Bars for each day of the week."""
    ordered = weekday.set_index("weekday").reindex(WEEKDAY_ORDER).reset_index()
    figure = px.bar(
        ordered,
        x="weekday",
        y="hours",
        labels={"weekday": "", "hours": "Hours listened"},
        title="Which days do I listen the most?",
    )
    figure.update_traces(marker_color=ACCENT)
    figure.update_layout(**_LAYOUT)
    return figure


def monthly_line(monthly: pd.DataFrame) -> go.Figure:
    """Line chart of the month-by-month trend."""
    figure = px.line(
        monthly,
        x="month",
        y="hours",
        markers=True,
        labels={"month": "", "hours": "Hours listened"},
        title="How has my listening shifted month to month?",
    )
    figure.update_traces(line=dict(color=ACCENT, width=3))
    figure.update_layout(**_LAYOUT)
    return figure


def top_bar(top: pd.DataFrame, title: str) -> go.Figure:
    """Horizontal bars for a ``top_tracks`` / ``top_artists`` frame."""
    figure = px.bar(
        top.sort_values("plays"),
        x="plays",
        y="label",
        orientation="h",
        labels={"plays": "Plays", "label": ""},
        title=title,
        hover_data={"minutes": ":.0f"},
    )
    figure.update_traces(marker_color=ACCENT)
    figure.update_layout(**_LAYOUT, height=max(320, 28 * len(top) + 120))
    return figure
