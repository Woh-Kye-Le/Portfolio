"""Guards the evaluation set itself: a broken reference query would silently corrupt every score."""
import pytest

from eval.run_eval import expected_result, load_questions, run_eval
from src.config import MAX_ROWS
from src.prompt import available_versions, build_system_prompt

QUESTIONS = load_questions()


def test_there_are_at_least_20_questions_with_unique_text():
    assert len(QUESTIONS) >= 20
    texts = [q["question"] for q in QUESTIONS]
    assert len(texts) == len(set(texts))


@pytest.mark.parametrize("q", QUESTIONS, ids=[q["id"] for q in QUESTIONS])
def test_reference_query_runs_and_returns_rows(q, db_path):
    result = expected_result(q, db_path)                 # raises if the guard rejects it
    assert result.rows, f"{q['id']} returns nothing, so it can't tell a right answer from a wrong one"
    assert len(result.rows) < MAX_ROWS, f"{q['id']} hits the row cap, so the expected result is truncated"


def test_ranked_questions_have_no_ties_at_the_cut(db_path):
    """A tie at the cut-off would make 'the right answer' arbitrary."""
    from src.db import run_query
    checks = {
        "q04": "SELECT SUM(o.amount) FROM orders o WHERE o.order_date >= '2026-06-01' AND o.order_date < '2026-07-01' GROUP BY o.restaurant_id ORDER BY 1 DESC LIMIT 2",
        "q09": "SELECT COUNT(*) FROM guests GROUP BY country ORDER BY 1 DESC LIMIT 2",
        "q11": "SELECT SUM(amount) FROM orders WHERE guest_id IN (SELECT guest_id FROM guests) GROUP BY guest_id ORDER BY 1 DESC LIMIT 6",
        "q19": "SELECT SUM(attendance * ticket_price) FROM events GROUP BY event_type ORDER BY 1 DESC LIMIT 2",
        "q20": "SELECT COUNT(*) FROM bookings GROUP BY room_type ORDER BY 1 DESC LIMIT 2",
    }
    for qid, sql in checks.items():
        values = [r[0] for r in run_query(sql, db_path).rows]
        assert len(set(values)) == len(values), f"{qid}: tie in {values}"


def test_harness_scores_100_percent_when_the_ai_gives_the_reference_answer(db_path):
    """Self-test of the scoring pipeline, with no LLM involved."""
    run = run_eval("v2", "n/a", db_path, dry_run=True)
    failed = [r["id"] for r in run["results"] if not r["passed"]]
    assert not failed, f"harness bug, these failed against themselves: {failed}"
    assert run["accuracy"] == 100.0


# ---- prompts ------------------------------------------------------------------

@pytest.mark.parametrize("version", available_versions())
def test_prompt_builds_with_no_unfilled_placeholders(version):
    prompt = build_system_prompt(version)
    assert "{{" not in prompt and "CREATE TABLE guests" in prompt


@pytest.mark.parametrize("version", available_versions())
def test_prompt_examples_do_not_leak_eval_questions(version):
    prompt = build_system_prompt(version).lower()
    leaked = [q["id"] for q in QUESTIONS if q["question"].lower() in prompt]
    assert not leaked, f"{version} contains eval questions verbatim: {leaked}. That inflates accuracy."
