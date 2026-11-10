"""Validate LLM-generated SQL before it goes anywhere near the database.

This is layer 1 of 3 (see README, "Security"):

  1. sql_guard.py  - text-level checks (this file): exactly one statement, must be
                     SELECT/WITH, no write/admin keywords, a LIMIT is always enforced.
  2. db.py         - the database is opened read-only, with an authorizer that only
                     permits reads, a query timeout, and a cap on blob/string size.
  3. the tests     - tests/test_sql_guard.py tries to sneak DROP/UPDATE/DELETE in.

No single layer is trusted on its own. The guard assumes the model can be tricked
(prompt injection, or just a bad day) and that its own parsing can have gaps, which
is why layer 2 exists.

The guard works on a small tokenizer rather than regexes over the raw text, so that
a keyword inside a string literal ('DROP TABLE') is not a false alarm, and a
semicolon inside a string literal is not mistaken for a second statement.
"""
from __future__ import annotations

from dataclasses import dataclass

from src.config import MAX_ROWS, MAX_SQL_CHARS

FORBIDDEN_WORDS = {
    "INSERT", "UPDATE", "DELETE", "DROP", "ALTER", "CREATE", "ATTACH", "DETACH",
    "PRAGMA", "VACUUM", "REINDEX", "ANALYZE", "BEGIN", "COMMIT", "ROLLBACK",
    "SAVEPOINT", "RELEASE", "TRUNCATE", "GRANT", "REVOKE", "EXEC", "EXECUTE",
    "LOAD_EXTENSION",
}
# REPLACE is a legitimate SQL function, replace(x, 'a', 'b'), but also the start
# of "REPLACE INTO ...". It is only blocked when it is NOT followed by "(".
ALLOWED_FIRST_WORDS = {"SELECT", "WITH"}


@dataclass
class GuardResult:
    ok: bool
    sql: str = ""            # the SQL that is safe to execute (comments stripped, LIMIT enforced)
    reason: str = ""         # why it was rejected, written for a human
    limit_added: bool = False


class _ScanError(ValueError):
    pass


def _scan(sql: str) -> tuple[list[tuple[str, str]], str]:
    """Split SQL into (kind, text) tokens and return a comment-free copy of it.

    kinds: word, string, ident, number, punct. Comments are dropped.
    """
    tokens: list[tuple[str, str]] = []
    out: list[str] = []
    i, n = 0, len(sql)
    while i < n:
        c = sql[i]
        if c.isspace():
            out.append(c)
            i += 1
        elif sql.startswith("--", i):
            j = sql.find("\n", i)
            i = n if j == -1 else j
            out.append(" ")
        elif sql.startswith("/*", i):
            j = sql.find("*/", i + 2)
            if j == -1:
                raise _ScanError("Unterminated /* comment")
            i = j + 2
            out.append(" ")
        elif c in "'\"`":
            j = i + 1
            while True:
                if j >= n:
                    raise _ScanError("Unterminated quoted string or identifier")
                if sql[j] == c:
                    if j + 1 < n and sql[j + 1] == c:   # doubled quote = escaped quote
                        j += 2
                        continue
                    break
                j += 1
            tokens.append(("string" if c == "'" else "ident", sql[i:j + 1]))
            out.append(sql[i:j + 1])
            i = j + 1
        elif c == "[":
            j = sql.find("]", i + 1)
            if j == -1:
                raise _ScanError("Unterminated [identifier]")
            tokens.append(("ident", sql[i:j + 1]))
            out.append(sql[i:j + 1])
            i = j + 1
        elif c.isalpha() or c == "_":
            j = i + 1
            while j < n and (sql[j].isalnum() or sql[j] in "_$"):
                j += 1
            tokens.append(("word", sql[i:j]))
            out.append(sql[i:j])
            i = j
        elif c.isdigit() or (c == "." and i + 1 < n and sql[i + 1].isdigit()):
            j = i + 1
            while j < n and (sql[j].isalnum() or sql[j] == "."):
                j += 1
            tokens.append(("number", sql[i:j]))
            out.append(sql[i:j])
            i = j
        else:
            tokens.append(("punct", c))
            out.append(c)
            i += 1
    return tokens, "".join(out)


def _reject(reason: str) -> GuardResult:
    return GuardResult(ok=False, reason=reason)


def validate_sql(sql: str, max_rows: int = MAX_ROWS) -> GuardResult:
    """Return a GuardResult. Never raises; a rejected query has ok=False and a reason."""
    if sql is None or not sql.strip():
        return _reject("Empty query.")
    if len(sql) > MAX_SQL_CHARS:
        return _reject(f"Query is too long (over {MAX_SQL_CHARS} characters).")
    if any(ord(ch) < 32 and ch not in "\t\n\r" for ch in sql):
        return _reject("Query contains control characters.")

    try:
        tokens, cleaned = _scan(sql)
    except _ScanError as exc:
        return _reject(str(exc))

    # Allow one trailing semicolon, nothing after it.
    if tokens and tokens[-1] == ("punct", ";"):
        tokens = tokens[:-1]
        cleaned = cleaned.rstrip()[:-1].rstrip()
    if not tokens:
        return _reject("Empty query.")
    if ("punct", ";") in tokens:
        return _reject("Only a single SQL statement is allowed (found a ';' in the middle).")

    first_kind, first = tokens[0]
    if first_kind != "word" or first.upper() not in ALLOWED_FIRST_WORDS:
        return _reject("Only SELECT queries are allowed (the query must start with SELECT or WITH).")

    for idx, (kind, text) in enumerate(tokens):
        if kind != "word":
            continue
        upper = text.upper()
        if upper in FORBIDDEN_WORDS:
            return _reject(f"Forbidden keyword: {upper}. Only read-only SELECT queries are allowed.")
        if upper.startswith("SQLITE_") or upper.startswith("PRAGMA"):
            return _reject(f"Access to SQLite internals ({text}) is not allowed.")
        if upper == "REPLACE":
            nxt = tokens[idx + 1] if idx + 1 < len(tokens) else None
            if nxt != ("punct", "("):
                return _reject("Forbidden keyword: REPLACE. Only read-only SELECT queries are allowed.")

    # Parentheses must balance; track depth so we can find the TOP-LEVEL limit clause.
    depth = 0
    limit_idx = None
    for idx, (kind, text) in enumerate(tokens):
        if kind == "punct" and text == "(":
            depth += 1
        elif kind == "punct" and text == ")":
            depth -= 1
            if depth < 0:
                return _reject("Unbalanced parentheses.")
        elif kind == "word" and depth == 0 and text.upper() == "LIMIT" and limit_idx is None:
            limit_idx = idx
    if depth != 0:
        return _reject("Unbalanced parentheses.")

    if limit_idx is None:
        return GuardResult(ok=True, sql=f"{cleaned} LIMIT {max_rows}", limit_added=True)

    # An existing LIMIT is kept only if it is a plain number <= max_rows, optionally
    # followed by OFFSET <number>. Anything else ("LIMIT 5, 99999", LIMIT 100000,
    # LIMIT (SELECT ...)) is wrapped in an outer query that enforces the cap.
    rest = tokens[limit_idx + 1:]
    simple = (
        len(rest) in (1, 3)
        and rest[0][0] == "number" and rest[0][1].isdigit() and int(rest[0][1]) <= max_rows
        and (len(rest) == 1 or (rest[1][1].upper() == "OFFSET" and rest[2][0] == "number" and rest[2][1].isdigit()))
    )
    if simple:
        return GuardResult(ok=True, sql=cleaned)
    return GuardResult(ok=True, sql=f"SELECT * FROM ({cleaned}) LIMIT {max_rows}", limit_added=True)
