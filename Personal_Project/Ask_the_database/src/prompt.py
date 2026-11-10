"""Builds the system prompt from a versioned template in prompts/.

Prompts are plain markdown files (prompts/v1.md, prompts/v2.md, ...). Never edit a
version you have already evaluated: copy it to a new version, change it, and re-run
the eval. That is what makes before/after accuracy comparisons meaningful.

Placeholders like {{schema}} are filled in here. The schema comes straight from
db/schema.sql so there is one source of truth.
"""
from __future__ import annotations

import re
from datetime import date, timedelta

from src.config import DATA_END, DATA_START, MAX_ROWS, PROMPTS_DIR, SCHEMA_PATH, TODAY


def available_versions() -> list[str]:
    return sorted(p.stem for p in PROMPTS_DIR.glob("v*.md"))


def last_month_range(today: date) -> tuple[date, date]:
    end = today.replace(day=1) - timedelta(days=1)
    return end.replace(day=1), end


def last_quarter_range(today: date) -> tuple[date, date]:
    this_q_start = date(today.year, 3 * ((today.month - 1) // 3) + 1, 1)
    end = this_q_start - timedelta(days=1)
    return date(end.year, 3 * ((end.month - 1) // 3) + 1, 1), end


def placeholder_values(today: date = TODAY) -> dict[str, str]:
    lm_start, lm_end = last_month_range(today)
    lq_start, lq_end = last_quarter_range(today)
    return {
        "schema": SCHEMA_PATH.read_text().strip(),
        "today": today.isoformat(),
        "this_year": str(today.year),
        "data_start": DATA_START.isoformat(),
        "data_end": DATA_END.isoformat(),
        "last_month_start": lm_start.isoformat(),
        "last_month_end": lm_end.isoformat(),
        "last_quarter_start": lq_start.isoformat(),
        "last_quarter_end": lq_end.isoformat(),
        "max_rows": str(MAX_ROWS),
    }


def load_template(version: str) -> str:
    path = PROMPTS_DIR / f"{version}.md"
    if not path.exists():
        raise ValueError(f"Unknown prompt version '{version}'. Available: {', '.join(available_versions())}")
    return path.read_text()


def build_system_prompt(version: str, today: date = TODAY) -> str:
    text = load_template(version)
    for key, value in placeholder_values(today).items():
        text = text.replace("{{" + key + "}}", value)
    leftover = re.findall(r"\{\{\w+\}\}", text)
    if leftover:
        raise ValueError(f"Prompt {version} has unknown placeholders: {sorted(set(leftover))}")
    return text
