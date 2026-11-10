"""The pipeline end to end, with a fake LLM so no API key or network is needed."""
import sqlite3

from src.llm import LLMResult
from src.pipeline import ask


def fake_llm(sql):
    def generate(question, prompt_version=None, model=None, provider=None):
        return LLMResult(sql=sql, raw=sql, model="fake", input_tokens=10, output_tokens=5)
    return generate


def test_happy_path(db_path):
    res = ask("How many guests?", db_path=db_path, generate=fake_llm("SELECT COUNT(*) AS n FROM guests"))
    assert res.ok and res.rows == [(500,)] and res.columns == ["n"]
    assert res.generated_sql == "SELECT COUNT(*) AS n FROM guests"
    assert res.executed_sql.endswith("LIMIT 100")          # what ran differs from what the model wrote


def test_destructive_sql_is_blocked_and_data_survives(db_path):
    res = ask("Delete everyone", db_path=db_path, generate=fake_llm("DELETE FROM guests"))
    assert res.stage == "blocked" and "SELECT" in res.error and res.executed_sql is None
    count = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True).execute("SELECT COUNT(*) FROM guests").fetchone()[0]
    assert count == 500


def test_hallucinated_column_is_a_sql_error(db_path):
    res = ask("Revenue?", db_path=db_path, generate=fake_llm("SELECT revenue FROM guests"))
    assert res.stage == "sql_error" and "revenue" in res.error


def test_llm_failure_is_returned_not_raised(db_path):
    def broken(question, prompt_version=None, model=None, provider=None):
        raise RuntimeError("rate limited")
    res = ask("Anything", db_path=db_path, generate=broken)
    assert res.stage == "llm_error" and "rate limited" in res.error


def test_blank_and_oversized_questions_are_rejected(db_path):
    assert ask("   ", db_path=db_path, generate=fake_llm("SELECT 1")).stage == "invalid"
    assert ask("x" * 501, db_path=db_path, generate=fake_llm("SELECT 1")).stage == "invalid"
