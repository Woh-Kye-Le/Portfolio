"""Decide whether the AI's result matches the known-correct result.

The rules are deliberately explicit, because "how do you score it?" is the first
question anyone asks about an eval:

  * Column NAMES are ignored; VALUES must match.
  * Extra columns are fine (asked "which restaurant?", returning name + revenue is ok).
    Missing columns are not.
  * Column order does not matter.
  * Row order matters only when the question says `ordered: true` (rankings).
  * Integers must match exactly. Floats may differ only by rounding to 0, 1 or 2
    decimals ("34.5" or "35" both match 34.52, but "34" does not).
"""
from __future__ import annotations

from typing import Sequence

Row = Sequence


def _values_equal(expected, actual) -> bool:
    if expected is None or actual is None:
        return expected is None and actual is None
    if isinstance(expected, bool):
        expected = int(expected)
    if isinstance(actual, bool):
        actual = int(actual)
    if isinstance(expected, (int, float)) and isinstance(actual, (int, float)):
        if isinstance(expected, int):
            return actual == expected
        if abs(actual - expected) <= 1e-9 * max(1.0, abs(expected)):
            return True
        return any(
            abs(actual - round(expected, k)) < 1e-9 or abs(expected - round(actual, k)) < 1e-9
            for k in (0, 1, 2)
        )
    if isinstance(expected, str) and isinstance(actual, str):
        return expected.strip() == actual.strip()
    return False


def _rows_equal(expected: Row, actual: Row) -> bool:
    return len(expected) == len(actual) and all(_values_equal(e, a) for e, a in zip(expected, actual))


def _same_multiset(expected: list, actual: list, eq) -> bool:
    remaining = list(actual)
    for item in expected:
        for i, candidate in enumerate(remaining):
            if eq(item, candidate):
                del remaining[i]
                break
        else:
            return False
    return not remaining


def _column(rows: list[Row], idx: int) -> list:
    return [r[idx] for r in rows]


def _map_columns(expected: list[Row], actual: list[Row], ordered: bool) -> list[int] | None:
    """For each expected column, find an unused actual column holding the same values."""
    used: set[int] = set()
    mapping = []
    for j in range(len(expected[0])):
        e_col = _column(expected, j)
        for k in range(len(actual[0])):
            if k in used:
                continue
            a_col = _column(actual, k)
            same = (
                all(_values_equal(e, a) for e, a in zip(e_col, a_col))
                if ordered else _same_multiset(e_col, a_col, _values_equal)
            )
            if same:
                used.add(k)
                mapping.append(k)
                break
        else:
            return None
    return mapping


def results_match(expected: list[Row], actual: list[Row], ordered: bool = False) -> tuple[bool, str]:
    """Return (matched, reason). The reason explains a mismatch in plain words."""
    if len(expected) != len(actual):
        return False, f"returned {len(actual)} rows, expected {len(expected)}"
    if not expected:
        return True, "both empty"
    if len(actual[0]) < len(expected[0]):
        return False, f"returned {len(actual[0])} columns, expected at least {len(expected[0])}"

    mapping = _map_columns(expected, actual, ordered)
    if mapping is None:
        return False, "values differ from the expected result"

    projected = [tuple(row[i] for i in mapping) for row in actual]
    if ordered:
        ok = all(_rows_equal(e, a) for e, a in zip(expected, projected))
        return (True, "match") if ok else (False, "right values but wrong row order or pairing")
    ok = _same_multiset(expected, projected, _rows_equal)
    return (True, "match") if ok else (False, "columns match individually but rows are paired wrongly")


def format_rows(rows: list[Row], limit: int = 5) -> str:
    """Compact text for reports: first few rows, one per line."""
    shown = [" | ".join("NULL" if v is None else str(v) for v in r) for r in rows[:limit]]
    if len(rows) > limit:
        shown.append(f"... ({len(rows) - limit} more rows)")
    return "\n".join(shown) if shown else "(no rows)"
