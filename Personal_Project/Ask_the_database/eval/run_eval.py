"""Run the evaluation set through the AI and report how many answers are correct.

Usage:
    python -m eval.run_eval --prompt v1                       # uses ASK_DB_PROVIDER (default: gemini)
    python -m eval.run_eval --prompt v1 --provider ollama     # free, runs on your computer
    python -m eval.run_eval --prompt v2 --provider anthropic --model claude-sonnet-5-5
    python -m eval.run_eval --dry-run              # harness self-test: the "AI" is the reference SQL
    python -m eval.run_eval --history              # accuracy of every saved run, side by side

Each real run writes two files to eval/runs/: a .json with every detail, and a .md
with the failures laid out so you can fill in the cause of each one.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
import time
from collections import defaultdict
from datetime import datetime
from pathlib import Path

import yaml

from src.compare import format_rows, results_match
from src.config import DEFAULT_DB_PATH, DEFAULT_PROMPT_VERSION, DEFAULT_PROVIDER, PROVIDERS, default_model
from src.db import run_query
from src.llm import LLMResult, generate_sql, provider_status
from src.pipeline import ask
from src.sql_guard import validate_sql

QUESTIONS_PATH = Path(__file__).with_name("questions.yaml")
RUNS_DIR = Path(__file__).with_name("runs")
STORED_ROWS = 20  # rows kept per result in the JSON file


def load_questions(path: Path = QUESTIONS_PATH) -> list[dict]:
    questions = yaml.safe_load(path.read_text())
    ids = [q["id"] for q in questions]
    if len(ids) != len(set(ids)):
        raise ValueError("Duplicate question ids in questions.yaml")
    for q in questions:
        for key in ("id", "category", "question", "reference_sql"):
            if key not in q:
                raise ValueError(f"{q.get('id', '?')}: missing '{key}'")
    return questions


def expected_result(question: dict, db_path):
    verdict = validate_sql(question["reference_sql"])
    if not verdict.ok:
        raise ValueError(f"{question['id']}: reference SQL rejected by the guard: {verdict.reason}")
    return run_query(verdict.sql, db_path)


def _reference_generator(questions: list[dict]):
    by_question = {q["question"]: q["reference_sql"] for q in questions}

    def generate(question, prompt_version=None, model=None, provider=None):
        sql = by_question[question]
        return LLMResult(sql=sql, raw=sql, model="reference-sql", provider="none")
    return generate


def run_eval(prompt_version, model, db_path, dry_run=False, only=None, pause=0.0, provider=None) -> dict:
    provider = provider or DEFAULT_PROVIDER
    model = model or default_model(provider)
    questions = load_questions()
    if only:
        questions = [q for q in questions if q["id"] in only]
    generate = _reference_generator(questions) if dry_run else generate_sql

    records = []
    for q in questions:
        expected = expected_result(q, db_path)
        got = ask(q["question"], prompt_version, model, db_path, generate=generate, provider=provider)

        if not got.ok:
            passed, failure_type, detail = False, got.stage, got.error
        else:
            passed, detail = results_match(expected.rows, got.rows, ordered=q.get("ordered", False))
            failure_type = None if passed else "wrong_result"

        records.append({
            "id": q["id"], "category": q["category"], "question": q["question"],
            "passed": passed, "failure_type": failure_type, "detail": detail,
            "generated_sql": got.generated_sql, "executed_sql": got.executed_sql,
            "reference_sql": q["reference_sql"].strip(), "note": q.get("note"),
            "expected_columns": expected.columns, "expected_rows": expected.rows[:STORED_ROWS],
            "actual_columns": got.columns, "actual_rows": got.rows[:STORED_ROWS],
            "latency_s": round(got.latency_s, 2),
            "input_tokens": got.input_tokens, "output_tokens": got.output_tokens,
        })
        mark = "PASS" if passed else "FAIL"
        extra = "" if passed else f"  <- {failure_type}: {str(detail)[:90]}"
        print(f"[{mark}] {q['id']} {q['category']:<13} {q['question'][:62]}{extra}", flush=True)
        if pause:
            time.sleep(pause)

    by_category: dict[str, list[int]] = defaultdict(lambda: [0, 0])
    for r in records:
        by_category[r["category"]][1] += 1
        by_category[r["category"]][0] += r["passed"]
    correct = sum(r["passed"] for r in records)
    return {
        "timestamp": datetime.now().isoformat(timespec="seconds"),
        "prompt_version": prompt_version,
        "provider": "none" if dry_run else provider,
        "model": "reference-sql" if dry_run else model,
        "dry_run": dry_run,
        "correct": correct,
        "total": len(records),
        "accuracy": round(100 * correct / len(records), 1) if records else 0.0,
        "by_category": {k: {"correct": v[0], "total": v[1]} for k, v in sorted(by_category.items())},
        "results": records,
    }


def to_markdown(run: dict) -> str:
    lines = [
        f"# Eval run: prompt {run['prompt_version']}, {run.get('provider', '?')} / {run['model']}",
        "",
        f"- Run at: {run['timestamp']}",
        f"- **Accuracy: {run['correct']}/{run['total']} = {run['accuracy']}%**",
        "",
        "| Category | Correct | Total |", "|---|---:|---:|",
    ]
    for cat, v in run["by_category"].items():
        lines.append(f"| {cat} | {v['correct']} | {v['total']} |")
    failures = [r for r in run["results"] if not r["passed"]]
    lines += ["", f"## Failures ({len(failures)})", ""]
    if not failures:
        lines.append("None.")
    for r in failures:
        lines += [
            f"### {r['id']} ({r['category']}): {r['question']}", "",
            f"- What happened: **{r['failure_type']}**, {r['detail']}",
        ]
        if r.get("note"):
            lines.append(f"- Question note: {r['note']}")
        lines += [
            "- Cause: _TODO: wrong join / wrong date range / missing business rule / ambiguous question / wrong aggregation / other_",
            "- Fix: _TODO_", "",
            "Generated SQL:", "```sql", (r["generated_sql"] or "(none)"), "```", "",
            "Reference SQL:", "```sql", r["reference_sql"], "```", "",
            "Expected (first rows):", "```", format_rows(r["expected_rows"]), "```", "",
            "Got (first rows):", "```", format_rows(r["actual_rows"]), "```", "",
        ]
    return "\n".join(lines)


def save_run(run: dict) -> Path:
    RUNS_DIR.mkdir(exist_ok=True)
    stamp = datetime.fromisoformat(run["timestamp"]).strftime("%Y%m%d-%H%M%S")
    label = re.sub(r"[^A-Za-z0-9._-]", "-", f"{run.get('provider', 'x')}_{run['model']}")  # Windows-safe
    stem = f"{stamp}_{run['prompt_version']}_{label}"
    (RUNS_DIR / f"{stem}.json").write_text(json.dumps(run, indent=2))
    (RUNS_DIR / f"{stem}.md").write_text(to_markdown(run))
    return RUNS_DIR / f"{stem}.md"


def print_history() -> None:
    runs = [json.loads(p.read_text()) for p in sorted(RUNS_DIR.glob("*.json"))]
    if not runs:
        print("No saved runs yet. Run: python -m eval.run_eval --prompt v1")
        return
    print(f"{'run at':<20} {'prompt':<7} {'provider':<10} {'model':<24} {'correct':>8} {'accuracy':>9}")
    for r in runs:
        print(f"{r['timestamp']:<20} {r['prompt_version']:<7} {r.get('provider', '?'):<10} {r['model']:<24} "
              f"{r['correct']:>3}/{r['total']:<4} {r['accuracy']:>8}%")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--prompt", default=DEFAULT_PROMPT_VERSION, help="prompt version, e.g. v1 (default: %(default)s)")
    parser.add_argument("--provider", default=DEFAULT_PROVIDER, choices=PROVIDERS, help="anthropic | gemini | ollama (default: %(default)s)")
    parser.add_argument("--model", default=None, help="model id (default: the provider's default model)")
    parser.add_argument("--db", default=str(DEFAULT_DB_PATH))
    parser.add_argument("--only", nargs="*", help="run just these ids, e.g. --only q04 q08")
    parser.add_argument("--pause", type=float, default=None, help="seconds between questions (default: 4 for gemini's free tier, else 0)")
    parser.add_argument("--dry-run", action="store_true", help="use the reference SQL as the AI's answer (harness self-test)")
    parser.add_argument("--no-save", action="store_true", help="do not write files to eval/runs/")
    parser.add_argument("--history", action="store_true", help="show accuracy of all saved runs and exit")
    args = parser.parse_args()

    if args.history:
        print_history()
        return
    if not args.dry_run:
        ready, message = provider_status(args.provider)
        if not ready:
            sys.exit(f"{message}\nFix that, pick another --provider, or use --dry-run to self-test the harness.")
    pause = args.pause if args.pause is not None else (4.0 if args.provider == "gemini" else 0.0)

    run = run_eval(args.prompt, args.model, args.db, dry_run=args.dry_run, only=args.only,
                   pause=pause, provider=args.provider)
    print(f"\nAccuracy: {run['correct']}/{run['total']} = {run['accuracy']}%  "
          f"(prompt {run['prompt_version']}, {run['provider']} / {run['model']})")
    for cat, v in run["by_category"].items():
        print(f"  {cat:<14}{v['correct']}/{v['total']}")
    if not args.dry_run and not args.no_save:
        print(f"\nFailure report: {save_run(run)}")


if __name__ == "__main__":
    main()
