"""question -> LLM -> guard -> read-only execution -> result.

Both the Streamlit app and the evaluation harness call ask(), so what you evaluate is
exactly what the UI runs. Failures are returned as data (stage + error), never raised,
because an evaluation run needs to record a failure, not crash on it.
"""
from __future__ import annotations

import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable

from src.config import DEFAULT_PROMPT_VERSION
from src.db import QueryError, run_query
from src.llm import LLMResult, generate_sql
from src.sql_guard import validate_sql

MAX_QUESTION_CHARS = 500


@dataclass
class AskResult:
    question: str
    prompt_version: str
    stage: str = "pending"          # ok | invalid | llm_error | blocked | sql_error
    provider: str | None = None
    model: str | None = None
    generated_sql: str | None = None  # exactly what the model wrote
    executed_sql: str | None = None   # what actually ran (comments stripped, LIMIT enforced)
    columns: list[str] = field(default_factory=list)
    rows: list[tuple] = field(default_factory=list)
    error: str | None = None
    latency_s: float = 0.0
    input_tokens: int | None = None
    output_tokens: int | None = None

    @property
    def ok(self) -> bool:
        return self.stage == "ok"


def ask(
    question: str,
    prompt_version: str = DEFAULT_PROMPT_VERSION,
    model: str | None = None,
    db_path: Path | str | None = None,
    generate: Callable[..., LLMResult] = generate_sql,
    provider: str | None = None,
) -> AskResult:
    result = AskResult(question=question, prompt_version=prompt_version)
    started = time.perf_counter()

    if not question or not question.strip():
        result.stage, result.error = "invalid", "Please type a question."
        return result
    if len(question) > MAX_QUESTION_CHARS:
        result.stage, result.error = "invalid", f"Question is too long (max {MAX_QUESTION_CHARS} characters)."
        return result

    try:
        llm = generate(question, prompt_version=prompt_version, model=model, provider=provider)
    except Exception as exc:  # network, auth, rate limit, missing key...
        result.stage, result.error = "llm_error", f"{type(exc).__name__}: {exc}"
        result.latency_s = time.perf_counter() - started
        return result

    result.model, result.provider = llm.model, llm.provider
    result.generated_sql = llm.sql
    result.input_tokens, result.output_tokens = llm.input_tokens, llm.output_tokens

    verdict = validate_sql(llm.sql)
    if not verdict.ok:
        result.stage, result.error = "blocked", verdict.reason
        result.latency_s = time.perf_counter() - started
        return result
    result.executed_sql = verdict.sql

    try:
        data = run_query(verdict.sql, db_path)
    except QueryError as exc:
        result.stage, result.error = "sql_error", str(exc)
    else:
        result.stage, result.columns, result.rows = "ok", data.columns, data.rows
    result.latency_s = time.perf_counter() - started
    return result
