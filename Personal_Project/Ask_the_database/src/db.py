"""Read-only query execution. This is layer 2 of the security model.

Even if a malicious statement slipped past sql_guard.py, this layer would still stop it:
  - the file is opened with mode=ro, so SQLite itself refuses writes
  - PRAGMA query_only = ON as a second switch
  - an authorizer callback allows only SELECT/READ/FUNCTION (+ recursive CTEs) and
    blocks reading SQLite's internal tables
  - a progress handler aborts queries that run longer than QUERY_TIMEOUT_S
  - string/blob size is capped so something like randomblob(1e9) can't eat memory
"""
from __future__ import annotations

import sqlite3
import time
from dataclasses import dataclass
from pathlib import Path

from src.config import DEFAULT_DB_PATH, MAX_ROWS, QUERY_TIMEOUT_S

_SQLITE_RECURSIVE = getattr(sqlite3, "SQLITE_RECURSIVE", 33)
_ALLOWED_ACTIONS = {
    sqlite3.SQLITE_SELECT,
    sqlite3.SQLITE_READ,
    sqlite3.SQLITE_FUNCTION,
    _SQLITE_RECURSIVE,
}


class QueryError(Exception):
    """A query failed to run. The message is safe to show to the user."""


@dataclass
class QueryResult:
    columns: list[str]
    rows: list[tuple]


def _authorizer(action, arg1, arg2, db_name, source):
    if action not in _ALLOWED_ACTIONS:
        return sqlite3.SQLITE_DENY
    if action == sqlite3.SQLITE_READ and arg1 and str(arg1).lower().startswith("sqlite_"):
        return sqlite3.SQLITE_DENY
    if action == sqlite3.SQLITE_FUNCTION and arg2 and str(arg2).lower() == "load_extension":
        return sqlite3.SQLITE_DENY
    return sqlite3.SQLITE_OK


def open_readonly(path: Path | str | None = None, restricted: bool = True) -> sqlite3.Connection:
    """Open the database read-only.

    restricted=True (default) also installs the authorizer. The data-quality checks
    pass restricted=False because they need PRAGMA table_info; they are still read-only.
    """
    path = Path(path or DEFAULT_DB_PATH)
    if not path.exists():
        raise QueryError(f"Database not found at {path}. Build it with: python -m db.seed")
    conn = sqlite3.connect(f"{path.resolve().as_uri()}?mode=ro", uri=True)
    conn.execute("PRAGMA query_only = ON")
    if hasattr(conn, "setlimit"):  # Python 3.11+
        conn.setlimit(sqlite3.SQLITE_LIMIT_LENGTH, 1_000_000)
    if restricted:
        conn.set_authorizer(_authorizer)  # set last: it would block the PRAGMA above
    return conn


def run_query(
    sql: str,
    db_path: Path | str | None = None,
    max_rows: int = MAX_ROWS,
    timeout_s: float = QUERY_TIMEOUT_S,
) -> QueryResult:
    """Run one already-validated query and return at most max_rows rows."""
    conn = open_readonly(db_path)
    deadline = time.monotonic() + timeout_s
    conn.set_progress_handler(lambda: 1 if time.monotonic() > deadline else 0, 10_000)
    try:
        cur = conn.execute(sql)
        rows = cur.fetchmany(max_rows)
        columns = [d[0] for d in cur.description] if cur.description else []
        return QueryResult(columns=columns, rows=[tuple(r) for r in rows])
    except sqlite3.Error as exc:
        if "interrupted" in str(exc).lower():
            raise QueryError(f"Query ran longer than {timeout_s:g}s and was stopped.") from exc
        raise QueryError(str(exc)) from exc
    finally:
        conn.close()
