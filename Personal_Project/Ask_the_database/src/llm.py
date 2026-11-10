"""The only file that talks to an AI model. Three providers, one function: generate_sql().

  anthropic  Claude API.        Key: ANTHROPIC_API_KEY (paid, prepaid credit)
  gemini     Google Gemini API. Key: GEMINI_API_KEY    (has a free tier)
  ollama     A model on your own computer. No key, no cost.

Pick one with ASK_DB_PROVIDER, the --provider flag of the eval, or the UI sidebar.

Security notes: keys are read from environment variables at call time and are never
written to disk by this project. They are sent in request headers, never in URLs, so
they cannot leak into error messages or logs.

    python -m src.llm --list-models gemini     # model names your key can use
    python -m src.llm --list-models ollama     # models installed on this computer
"""
from __future__ import annotations

import argparse
import json
import os
import re
import time
import urllib.error
import urllib.request
from dataclasses import dataclass

from src.config import (
    DEFAULT_PROMPT_VERSION, DEFAULT_PROVIDER, OLLAMA_HOST, PROVIDERS, default_model,
)
from src.prompt import build_system_prompt

MAX_OUTPUT_TOKENS = 1000
GEMINI_MAX_OUTPUT_TOKENS = 4096     # headroom: "thinking" models spend output tokens thinking
GEMINI_BASE = "https://generativelanguage.googleapis.com/v1beta"
RETRY_STATUSES = {429, 503}         # rate limited / overloaded: wait and try again
GEMINI_RETRIES = 4


class MissingAPIKey(RuntimeError):
    pass


class ProviderError(RuntimeError):
    """The provider could not be reached or rejected the request. Message is safe to show."""

    def __init__(self, message: str, status: int | None = None):
        super().__init__(message)
        self.status = status


@dataclass
class LLMResult:
    sql: str
    raw: str
    model: str
    provider: str = ""
    input_tokens: int | None = None
    output_tokens: int | None = None
    latency_s: float = 0.0


_FENCE = re.compile(r"```(?:sql)?\s*(.*?)```", re.DOTALL | re.IGNORECASE)


def extract_sql(text: str) -> str:
    """Models sometimes wrap SQL in a markdown fence despite being told not to."""
    match = _FENCE.search(text)
    return (match.group(1) if match else text).strip()


# --------------------------------------------------------------------------- HTTP

def _error_text(body: str) -> str:
    try:
        data = json.loads(body)
        err = data.get("error", data)
        if isinstance(err, dict):
            return str(err.get("message") or err)[:400]
        return str(err)[:400]
    except (ValueError, AttributeError):
        return body.strip()[:300] or "(empty response)"


def _request_json(method, url, payload=None, headers=None, timeout=120.0, retries=0) -> dict:
    """One JSON request using only the standard library. Retries rate-limit errors."""
    body = None if payload is None else json.dumps(payload).encode("utf-8")
    all_headers = {"Content-Type": "application/json", **(headers or {})}
    attempt = 0
    while True:
        request = urllib.request.Request(url, data=body, headers=all_headers, method=method)
        try:
            with urllib.request.urlopen(request, timeout=timeout) as response:
                return json.loads(response.read().decode("utf-8"))
        except urllib.error.HTTPError as exc:
            text = exc.read().decode("utf-8", errors="replace")
            if exc.code in RETRY_STATUSES and attempt < retries:
                time.sleep(5 * 2 ** attempt)
                attempt += 1
                continue
            raise ProviderError(f"HTTP {exc.code}: {_error_text(text)}", status=exc.code) from exc
        except urllib.error.URLError as exc:
            raise ProviderError(f"Could not connect to {url.split('?')[0]} ({exc.reason})") from exc
        except TimeoutError as exc:
            raise ProviderError(f"No answer from {url.split('?')[0]} after {timeout:g}s") from exc
        except ValueError as exc:
            raise ProviderError("The server sent a response that was not valid JSON") from exc


# --------------------------------------------------------------------- providers
# Each returns (raw_text, input_tokens, output_tokens).

def _call_anthropic(system: str, question: str, model: str, temperature):
    key = os.environ.get("ANTHROPIC_API_KEY")
    if not key:
        raise MissingAPIKey("ANTHROPIC_API_KEY is not set. Create a key at platform.claude.com "
                            "and set it in your shell (see .env.example).")
    import anthropic  # imported here so the other providers work without the SDK

    kwargs = dict(model=model, max_tokens=MAX_OUTPUT_TOKENS, system=system,
                  messages=[{"role": "user", "content": question}])
    if temperature is not None:
        kwargs["temperature"] = temperature
    response = anthropic.Anthropic(api_key=key).messages.create(**kwargs)
    raw = "".join(block.text for block in response.content if block.type == "text")
    return raw, getattr(response.usage, "input_tokens", None), getattr(response.usage, "output_tokens", None)


def _gemini_key() -> str | None:
    return os.environ.get("GEMINI_API_KEY") or os.environ.get("GOOGLE_API_KEY")


def _call_gemini(system: str, question: str, model: str, temperature):
    key = _gemini_key()
    if not key:
        raise MissingAPIKey("GEMINI_API_KEY is not set. Get a free key at "
                            "https://aistudio.google.com/apikey and set it in your shell.")
    model = model.removeprefix("models/")
    config = {"maxOutputTokens": GEMINI_MAX_OUTPUT_TOKENS}
    if temperature is not None:
        config["temperature"] = temperature
    payload = {
        "systemInstruction": {"parts": [{"text": system}]},
        "contents": [{"role": "user", "parts": [{"text": question}]}],
        "generationConfig": config,
    }
    try:
        data = _request_json("POST", f"{GEMINI_BASE}/models/{model}:generateContent", payload,
                             headers={"x-goog-api-key": key}, retries=GEMINI_RETRIES)
    except ProviderError as exc:
        if exc.status == 404:
            raise ProviderError(f"{exc} -- model '{model}' was not found. "
                                "Run: python -m src.llm --list-models gemini", status=404) from exc
        if exc.status in (400, 403) and "api key" in str(exc).lower():
            raise ProviderError(f"{exc} -- check GEMINI_API_KEY", status=exc.status) from exc
        raise
    candidates = data.get("candidates") or []
    if not candidates:
        reason = (data.get("promptFeedback") or {}).get("blockReason") or "no candidates returned"
        raise ProviderError(f"Gemini gave no answer ({reason})")
    parts = (candidates[0].get("content") or {}).get("parts") or []
    raw = "".join(p.get("text", "") for p in parts if not p.get("thought"))
    usage = data.get("usageMetadata") or {}
    return raw, usage.get("promptTokenCount"), usage.get("candidatesTokenCount")


def _call_ollama(system: str, question: str, model: str, temperature):
    options = {"num_ctx": 8192, "num_predict": MAX_OUTPUT_TOKENS,
               "temperature": 0.0 if temperature is None else temperature}
    payload = {"model": model, "stream": False, "options": options,
               "messages": [{"role": "system", "content": system},
                            {"role": "user", "content": question}]}
    try:
        data = _request_json("POST", f"{OLLAMA_HOST.rstrip('/')}/api/chat", payload, timeout=600.0)
    except ProviderError as exc:
        if exc.status == 404:
            raise ProviderError(f"{exc} -- model not installed. Run: ollama pull {model}", status=404) from exc
        if exc.status is None and "connect" in str(exc).lower():
            raise ProviderError(f"{exc} -- is Ollama running? Install it from ollama.com, "
                                f"start it, then run: ollama pull {model}") from exc
        raise
    raw = (data.get("message") or {}).get("content", "")
    return raw, data.get("prompt_eval_count"), data.get("eval_count")


_CALLERS = {"anthropic": _call_anthropic, "gemini": _call_gemini, "ollama": _call_ollama}


# --------------------------------------------------------------------- public API

def _temperature():
    value = os.environ.get("ASK_DB_TEMPERATURE")
    return float(value) if value else None


def generate_sql(
    question: str,
    prompt_version: str = DEFAULT_PROMPT_VERSION,
    model: str | None = None,
    provider: str | None = None,
) -> LLMResult:
    provider = (provider or DEFAULT_PROVIDER).lower()
    if provider not in PROVIDERS:
        raise ValueError(f"Unknown provider '{provider}'. Choose one of: {', '.join(PROVIDERS)}")
    model = model or default_model(provider)
    system = build_system_prompt(prompt_version)

    started = time.perf_counter()
    raw, tokens_in, tokens_out = _CALLERS[provider](system, question.strip(), model, _temperature())
    return LLMResult(
        sql=extract_sql(raw), raw=raw, model=model, provider=provider,
        input_tokens=tokens_in, output_tokens=tokens_out,
        latency_s=time.perf_counter() - started,
    )


def provider_status(provider: str) -> tuple[bool, str]:
    """Is this provider ready to use? Returns (ready, message for the user). Never raises."""
    if provider == "anthropic":
        if os.environ.get("ANTHROPIC_API_KEY"):
            return True, "ANTHROPIC_API_KEY found in environment"
        return False, "ANTHROPIC_API_KEY is not set (create one at platform.claude.com)"
    if provider == "gemini":
        if _gemini_key():
            return True, "GEMINI_API_KEY found in environment"
        return False, "GEMINI_API_KEY is not set (free key: aistudio.google.com/apikey)"
    if provider == "ollama":
        try:
            data = _request_json("GET", f"{OLLAMA_HOST.rstrip('/')}/api/tags", timeout=1.5)
        except ProviderError:
            return False, f"Ollama is not running at {OLLAMA_HOST} (install from ollama.com and start it)"
        n = len(data.get("models") or [])
        return True, f"Ollama is running ({n} model{'s' if n != 1 else ''} installed)"
    return False, f"Unknown provider '{provider}'"


def list_models(provider: str) -> list[str]:
    if provider == "gemini":
        key = _gemini_key()
        if not key:
            raise MissingAPIKey("GEMINI_API_KEY is not set.")
        data = _request_json("GET", f"{GEMINI_BASE}/models?pageSize=200", headers={"x-goog-api-key": key})
        return sorted(m["name"].removeprefix("models/") for m in data.get("models", [])
                      if "generateContent" in m.get("supportedGenerationMethods", []))
    if provider == "ollama":
        data = _request_json("GET", f"{OLLAMA_HOST.rstrip('/')}/api/tags", timeout=5.0)
        return sorted(m["name"] for m in data.get("models", []))
    raise ValueError("--list-models supports: gemini, ollama")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--list-models", choices=["gemini", "ollama"], metavar="PROVIDER",
                        help="print the model names available to you (gemini or ollama)")
    args = parser.parse_args()
    if not args.list_models:
        parser.print_help()
        return
    try:
        for name in list_models(args.list_models):
            print(name)
    except (MissingAPIKey, ProviderError) as exc:
        raise SystemExit(str(exc))


if __name__ == "__main__":
    main()
