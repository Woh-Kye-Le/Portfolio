"""Data-quality checks: nulls, duplicates, orphan IDs, negative amounts, broken logic.

Usage:
    python -m src.data_checks                                  # print a report
    python -m src.data_checks --write db/data_quality_report.md

Every check is a plain SQL query, so the report doubles as a SQL exercise: each
row shows the query that produced it.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from src.db import open_readonly

# (category, table, description, SQL returning one number)
CHECKS: list[tuple[str, str, str, str]] = [
    # --- duplicates ---------------------------------------------------------
    ("duplicates", "orders",
     "Extra copies of a bill with the same restaurant, guest, date, party size and amount",
     "SELECT COALESCE(SUM(n - 1), 0) FROM (SELECT COUNT(*) AS n FROM orders "
     "GROUP BY restaurant_id, guest_id, order_date, party_size, amount HAVING COUNT(*) > 1)"),
    ("duplicates", "bookings",
     "Extra copies of a booking with the same guest, room, dates and rate",
     "SELECT COALESCE(SUM(n - 1), 0) FROM (SELECT COUNT(*) AS n FROM bookings "
     "GROUP BY guest_id, room_type, check_in, check_out, nightly_rate HAVING COUNT(*) > 1)"),
    ("duplicates", "retail_sales",
     "Extra copies of a transaction with identical store, category, date, quantity, amount and guest",
     "SELECT COALESCE(SUM(n - 1), 0) FROM (SELECT COUNT(*) AS n FROM retail_sales "
     "GROUP BY store_name, category, sale_date, quantity, amount, guest_id HAVING COUNT(*) > 1)"),
    ("duplicates", "guests",
     "Guests sharing the same email address",
     "SELECT COALESCE(SUM(n - 1), 0) FROM (SELECT COUNT(*) AS n FROM guests "
     "WHERE email IS NOT NULL GROUP BY email HAVING COUNT(*) > 1)"),
    # --- orphan IDs (foreign key points at a row that does not exist) ---------
    ("orphans", "bookings",
     "bookings.guest_id has no matching guest",
     "SELECT COUNT(*) FROM bookings b LEFT JOIN guests g ON g.guest_id = b.guest_id "
     "WHERE g.guest_id IS NULL"),
    ("orphans", "orders",
     "orders.guest_id is set but has no matching guest",
     "SELECT COUNT(*) FROM orders o LEFT JOIN guests g ON g.guest_id = o.guest_id "
     "WHERE o.guest_id IS NOT NULL AND g.guest_id IS NULL"),
    ("orphans", "orders",
     "orders.restaurant_id has no matching restaurant",
     "SELECT COUNT(*) FROM orders o LEFT JOIN restaurants r ON r.restaurant_id = o.restaurant_id "
     "WHERE r.restaurant_id IS NULL"),
    ("orphans", "retail_sales",
     "retail_sales.guest_id is set but has no matching guest",
     "SELECT COUNT(*) FROM retail_sales s LEFT JOIN guests g ON g.guest_id = s.guest_id "
     "WHERE s.guest_id IS NOT NULL AND g.guest_id IS NULL"),
    # --- negative amounts -----------------------------------------------------
    ("negative amounts", "orders", "orders.amount < 0",
     "SELECT COUNT(*) FROM orders WHERE amount < 0"),
    ("negative amounts", "bookings", "bookings.total_amount < 0 or nightly_rate < 0",
     "SELECT COUNT(*) FROM bookings WHERE total_amount < 0 OR nightly_rate < 0"),
    ("negative amounts", "retail_sales", "retail_sales.amount < 0",
     "SELECT COUNT(*) FROM retail_sales WHERE amount < 0"),
    ("negative amounts", "events", "events.ticket_price < 0",
     "SELECT COUNT(*) FROM events WHERE ticket_price < 0"),
    # --- business-logic consistency --------------------------------------------
    ("logic", "bookings", "check_out is not after check_in",
     "SELECT COUNT(*) FROM bookings WHERE check_out <= check_in"),
    ("logic", "bookings", "total_amount differs from nights x nightly_rate by more than 1 cent",
     "SELECT COUNT(*) FROM bookings WHERE ABS(total_amount - (julianday(check_out) - "
     "julianday(check_in)) * nightly_rate) > 0.01"),
    ("logic", "events", "attendance is higher than capacity",
     "SELECT COUNT(*) FROM events WHERE attendance > capacity"),
]

TABLES = ["guests", "bookings", "restaurants", "orders", "retail_sales", "events"]


@dataclass
class CheckResult:
    category: str
    table: str
    description: str
    count: int
    sql: str
    status: str            # "issue" | "ok" | "info"
    detail: str = ""       # e.g. "3.1% of rows" for null checks


def _null_checks(conn) -> list[CheckResult]:
    """Count NULLs in every column that is allowed to be NULL (nullable by design)."""
    results = []
    for table in TABLES:
        total = conn.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
        for _cid, col, _type, notnull, _default, pk in conn.execute(f"PRAGMA table_info({table})").fetchall():
            if notnull or pk:
                continue
            sql = f"SELECT COUNT(*) FROM {table} WHERE {col} IS NULL"
            n = conn.execute(sql).fetchone()[0]
            pct = f"{100 * n / total:.1f}% of {total:,} rows" if total else ""
            results.append(CheckResult("nulls", table, f"{table}.{col} is NULL", n, sql,
                                       "info" if n else "ok", pct))
    return results


def run_checks(db_path: Path | str | None = None) -> list[CheckResult]:
    conn = open_readonly(db_path, restricted=False)
    try:
        results = _null_checks(conn)
        for category, table, description, sql in CHECKS:
            n = conn.execute(sql).fetchone()[0]
            results.append(CheckResult(category, table, description, int(n), sql,
                                       "issue" if n else "ok"))
        return results
    finally:
        conn.close()


def to_markdown(results: list[CheckResult]) -> str:
    issues = [r for r in results if r.status == "issue"]
    lines = [
        "# Data-quality report",
        "",
        "Generated by `python -m src.data_checks`. The data is synthetic; a few defects were "
        "planted on purpose by `db/seed.py` so these checks have something to find.",
        "",
        f"**{len(issues)} checks found problems** out of {len(results)} run. "
        "NULLs are reported as information: some columns are meant to allow them "
        "(walk-in diners have no guest, an event may have no recorded attendance).",
        "",
        "| Status | Category | Table | What was checked | Rows affected |",
        "|---|---|---|---|---:|",
    ]
    icon = {"issue": "ISSUE", "ok": "ok", "info": "info"}
    for r in sorted(results, key=lambda r: (r.status != "issue", r.status != "info", r.category, r.table)):
        extra = f" ({r.detail})" if r.detail else ""
        lines.append(f"| {icon[r.status]} | {r.category} | {r.table} | {r.description} | {r.count:,}{extra} |")
    lines += ["", "## The SQL behind each check", ""]
    for r in results:
        lines += [f"**{r.category} / {r.table}**: {r.description}", "", "```sql", r.sql + ";", "```", ""]
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--db", default=None, help="database path (default: db/resort.db)")
    parser.add_argument("--write", metavar="PATH", help="write a markdown report to this file")
    args = parser.parse_args()
    results = run_checks(args.db)
    report = to_markdown(results)
    if args.write:
        Path(args.write).write_text(report)
        print(f"Wrote {args.write}")
    for r in results:
        if r.status != "ok":
            print(f"[{r.status.upper():<5}] {r.table}: {r.description}: {r.count}")


if __name__ == "__main__":
    main()
