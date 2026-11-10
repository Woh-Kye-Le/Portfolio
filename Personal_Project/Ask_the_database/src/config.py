"""Central settings. Everything tunable lives here so no other module hardcodes it."""
from __future__ import annotations

import os
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# --- database -------------------------------------------------------------
DEFAULT_DB_PATH = Path(os.getenv("ASK_DB_PATH", ROOT / "db" / "resort.db"))
SCHEMA_PATH = ROOT / "db" / "schema.sql"
SEED = 42

# The synthetic data covers exactly one year. "TODAY" is the dataset's own
# calendar date, fixed on purpose so that questions like "last month" always
# have the same correct answer, no matter when you run the project.
DATA_START = date(2025, 7, 1)
DATA_END = date(2026, 6, 30)
TODAY = date(2026, 7, 1)

# --- safety limits ----------------------------------------------------------
MAX_ROWS = 100            # every query is forced to return at most this many rows
MAX_SQL_CHARS = 4000      # reject absurdly long SQL
QUERY_TIMEOUT_S = 5.0     # abort runaway queries (e.g. accidental cross joins)

# --- LLM --------------------------------------------------------------------
# Three interchangeable providers. Keys are read from environment variables at call
# time (see src/llm.py); Ollama runs locally and needs no key.
PROVIDERS = ("anthropic", "gemini", "ollama")
DEFAULT_PROVIDER = os.getenv("ASK_DB_PROVIDER", "gemini").lower()

# Model names change often. Override per provider with ASK_DB_MODEL_GEMINI etc.,
# or per run with --model. `python -m src.llm --list-models gemini` shows valid names.
PROVIDER_DEFAULT_MODELS = {
    "anthropic": "claude-haiku-5-5",
    "gemini": "gemini-3.5-flash",
    "ollama": "qwen2.5-coder:7b",
}
OLLAMA_HOST = os.getenv("OLLAMA_HOST", "http://localhost:11434")

DEFAULT_PROMPT_VERSION = os.getenv("ASK_DB_PROMPT", "v2")
PROMPTS_DIR = ROOT / "prompts"


def default_model(provider: str) -> str:
    """The model used for a provider unless --model / the UI says otherwise."""
    return os.getenv(f"ASK_DB_MODEL_{provider.upper()}") or PROVIDER_DEFAULT_MODELS[provider]
