"""Try to sneak writes past the guard, and prove the layers behind it hold even if it fails."""
import sqlite3

import pytest

from src.db import QueryError, open_readonly, run_query
from src.sql_guard import validate_sql

ATTACKS = [
    # the classics
    "DROP TABLE guests",
    "DROP TABLE guests;",
    "DELETE FROM guests",
    "UPDATE guests SET country = 'X'",
    "INSERT INTO guests VALUES (1, 'a', 'b', 'c', 'd', 'e')",
    "CREATE TABLE t (x)",
    "ALTER TABLE guests ADD COLUMN x",
    "VACUUM",
    # smuggled behind a legitimate SELECT
    "SELECT 1; DROP TABLE guests;",
    "SELECT * FROM guests; DELETE FROM guests",
    "SELECT * FROM guests WHERE full_name = 'x'; DROP TABLE guests; --'",
    "SELECT 1;\nDROP TABLE guests",
    # case games and whitespace games
    "dRoP tAbLe guests",
    "SeLeCt 1; dRoP TABLE guests",
    "SELECT 1;\t\n  DELETE FROM guests",
    # comments used as camouflage
    "SELECT * FROM guests /* ; */; DROP TABLE guests",
    "select * from guests; -- harmless\n drop table guests",
    "/* hi */ DROP TABLE guests",
    # writes hidden inside a WITH
    "WITH x AS (SELECT 1) DELETE FROM guests",
    "WITH x AS (SELECT 1) INSERT INTO guests SELECT * FROM guests",
    "WITH x AS (SELECT 1) REPLACE INTO guests SELECT * FROM guests",
    "WITH x AS (SELECT 1) UPDATE guests SET country = 'X'",
    # admin and internals
    "ATTACH DATABASE '/tmp/evil.db' AS evil",
    "PRAGMA writable_schema = ON",
    "SELECT * FROM pragma_table_info('guests')",
    "SELECT * FROM sqlite_master",
    "SELECT name FROM sqlite_schema",
    "SELECT load_extension('evil.so')",
    "EXPLAIN SELECT 1",
    "BEGIN; SELECT 1",
    # malformed or empty
    "",
    "   ",
    ";",
    "SELECT 'unterminated",
    "SELECT 1 /* unterminated",
    "SELECT (1",
    "SELECT 1)",
    "SELECT 1\x00; DROP TABLE guests",
]


@pytest.mark.parametrize("sql", ATTACKS)
def test_attack_is_blocked(sql):
    verdict = validate_sql(sql)
    assert not verdict.ok, f"guard let this through: {sql!r}"
    assert verdict.reason


VALID = [
    "SELECT COUNT(*) FROM guests",
    "select * from guests;",
    "SELECT * FROM guests;  ",
    "WITH t AS (SELECT guest_id FROM guests) SELECT * FROM t",
    "SELECT replace(full_name, 'a', 'b') FROM guests",                   # replace() is a function
    "SELECT * FROM guests WHERE full_name = 'DROP TABLE guests'",        # keyword inside a string
    "SELECT 'a;b' AS x",                                                  # ';' inside a string
    "SELECT 'it''s' AS x",                                                # escaped quote
    "SELECT 1 AS updated_flag, 2 AS dropoff, 3 AS inserted, 4 AS created_at",  # not whole-word matches
    'SELECT "full_name" FROM guests',
    "SELECT g.country, COUNT(*) FROM guests g GROUP BY g.country -- trailing comment",
    "SELECT ROW_NUMBER() OVER (PARTITION BY country ORDER BY guest_id) FROM guests",
]


@pytest.mark.parametrize("sql", VALID)
def test_valid_queries_pass(sql):
    verdict = validate_sql(sql)
    assert verdict.ok, verdict.reason


# ---- the LIMIT is always enforced -------------------------------------------

def test_limit_is_added_when_missing():
    verdict = validate_sql("SELECT * FROM guests;")
    assert verdict.ok and verdict.limit_added
    assert verdict.sql == "SELECT * FROM guests LIMIT 100"


def test_small_limit_is_kept():
    assert validate_sql("SELECT * FROM guests LIMIT 5").sql == "SELECT * FROM guests LIMIT 5"
    assert validate_sql("SELECT * FROM guests LIMIT 10 OFFSET 5").sql == "SELECT * FROM guests LIMIT 10 OFFSET 5"


@pytest.mark.parametrize("sql", [
    "SELECT * FROM guests LIMIT 5000",
    "SELECT * FROM guests LIMIT 5, 99999",        # in SQLite this means offset 5, count 99999
    "SELECT * FROM guests LIMIT (SELECT 99999)",
])
def test_oversized_limit_is_capped(sql):
    verdict = validate_sql(sql)
    assert verdict.ok and verdict.limit_added
    assert verdict.sql.startswith("SELECT * FROM (") and verdict.sql.endswith("LIMIT 100")


def test_limit_inside_subquery_does_not_count():
    verdict = validate_sql("SELECT * FROM (SELECT * FROM guests LIMIT 5)")
    assert verdict.sql.endswith("LIMIT 100")


def test_comments_are_stripped_from_executed_sql():
    verdict = validate_sql("SELECT 1 /* note */ -- another\n")
    assert "note" not in verdict.sql and "another" not in verdict.sql


def test_forced_limit_actually_limits_rows(db_path):
    for sql in ["SELECT * FROM orders",
                "SELECT * FROM orders LIMIT 100000",
                "WITH t AS (SELECT * FROM orders) SELECT * FROM t LIMIT 5000"]:
        verdict = validate_sql(sql)
        assert len(run_query(verdict.sql, db_path).rows) == 100


# ---- layer 2: even with the guard bypassed, the database refuses ------------

@pytest.mark.parametrize("sql", [
    "INSERT INTO guests VALUES (999999, 'a', NULL, 'c', 'd', 'e')",
    "UPDATE guests SET country = 'X'",
    "DELETE FROM orders",
    "DROP TABLE guests",
    "CREATE TABLE evil (x)",
])
def test_database_refuses_writes_even_without_the_guard(db_path, sql):
    before = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True).execute("SELECT COUNT(*) FROM guests").fetchone()
    with pytest.raises(QueryError):
        run_query(sql, db_path)
    after = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True).execute("SELECT COUNT(*) FROM guests").fetchone()
    assert before == after


def test_raw_readonly_connection_refuses_writes(db_path):
    conn = open_readonly(db_path, restricted=False)   # no authorizer: only mode=ro protects us
    with pytest.raises(sqlite3.OperationalError):
        conn.execute("DELETE FROM guests")
    conn.close()


def test_authorizer_blocks_reading_sqlite_internals(db_path):
    with pytest.raises(QueryError):
        run_query("SELECT * FROM sqlite_master", db_path)


def test_runaway_query_is_stopped(db_path):
    runaway = "WITH RECURSIVE c(x) AS (SELECT 1 UNION ALL SELECT x + 1 FROM c) SELECT COUNT(*) FROM c"
    with pytest.raises(QueryError, match="longer than"):
        run_query(runaway, db_path, timeout_s=0.5)


def test_missing_database_gives_a_helpful_error(tmp_path):
    with pytest.raises(QueryError, match="db.seed"):
        run_query("SELECT 1", tmp_path / "nope.db")
