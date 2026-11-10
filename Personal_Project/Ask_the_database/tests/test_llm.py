"""Provider layer tests. HTTP is replaced by a fake, so no keys, network or Ollama needed."""
import sys
import types

import pytest

import src.llm as llm
from src.llm import MissingAPIKey, ProviderError, generate_sql, provider_status


class FakeHTTP:
    """Records the request and returns a canned response (or raises)."""

    def __init__(self, response=None, error=None):
        self.response, self.error, self.calls = response, error, []

    def __call__(self, method, url, payload=None, headers=None, timeout=120.0, retries=0):
        self.calls.append({"method": method, "url": url, "payload": payload,
                           "headers": headers or {}, "retries": retries})
        if self.error:
            raise self.error
        return self.response


@pytest.fixture(autouse=True)
def clean_env(monkeypatch):
    for name in ("ANTHROPIC_API_KEY", "GEMINI_API_KEY", "GOOGLE_API_KEY", "ASK_DB_TEMPERATURE",
                 "ASK_DB_MODEL_GEMINI", "ASK_DB_MODEL_OLLAMA", "ASK_DB_MODEL_ANTHROPIC"):
        monkeypatch.delenv(name, raising=False)


# ---------------------------------------------------------------------- gemini

GEMINI_OK = {
    "candidates": [{"content": {"parts": [{"text": "```sql\nSELECT COUNT(*) FROM guests;\n```"}]}}],
    "usageMetadata": {"promptTokenCount": 120, "candidatesTokenCount": 9},
}


def test_gemini_request_shape_and_parsing(monkeypatch):
    monkeypatch.setenv("GEMINI_API_KEY", "test-key-123")
    fake = FakeHTTP(GEMINI_OK)
    monkeypatch.setattr(llm, "_request_json", fake)

    result = generate_sql("How many guests?", "v1", provider="gemini", model="gemini-test")

    call = fake.calls[0]
    assert call["url"].endswith("/models/gemini-test:generateContent")
    assert "test-key-123" not in call["url"]                       # key goes in a header, never the URL
    assert call["headers"]["x-goog-api-key"] == "test-key-123"
    assert call["payload"]["contents"][0]["parts"][0]["text"] == "How many guests?"
    assert "CREATE TABLE guests" in call["payload"]["systemInstruction"]["parts"][0]["text"]
    assert call["retries"] > 0                                      # free tier rate limits are retried
    assert result.sql == "SELECT COUNT(*) FROM guests;"             # fence stripped
    assert (result.provider, result.model) == ("gemini", "gemini-test")
    assert (result.input_tokens, result.output_tokens) == (120, 9)


def test_gemini_thinking_parts_are_ignored(monkeypatch):
    monkeypatch.setenv("GEMINI_API_KEY", "k")
    data = {"candidates": [{"content": {"parts": [
        {"text": "let me think about joins", "thought": True}, {"text": "SELECT 1"}]}}]}
    monkeypatch.setattr(llm, "_request_json", FakeHTTP(data))
    assert generate_sql("q", "v1", provider="gemini").sql == "SELECT 1"


def test_gemini_accepts_google_api_key_name_and_models_prefix(monkeypatch):
    monkeypatch.setenv("GOOGLE_API_KEY", "alt")
    fake = FakeHTTP(GEMINI_OK)
    monkeypatch.setattr(llm, "_request_json", fake)
    generate_sql("q", "v1", provider="gemini", model="models/gemini-test")
    assert fake.calls[0]["url"].endswith("/models/gemini-test:generateContent")
    assert fake.calls[0]["headers"]["x-goog-api-key"] == "alt"


def test_gemini_missing_key_is_a_clear_error():
    with pytest.raises(MissingAPIKey, match="aistudio.google.com"):
        generate_sql("q", "v1", provider="gemini")


def test_gemini_blocked_prompt_is_reported(monkeypatch):
    monkeypatch.setenv("GEMINI_API_KEY", "k")
    monkeypatch.setattr(llm, "_request_json", FakeHTTP({"promptFeedback": {"blockReason": "SAFETY"}}))
    with pytest.raises(ProviderError, match="SAFETY"):
        generate_sql("q", "v1", provider="gemini")


def test_gemini_unknown_model_points_to_list_models(monkeypatch):
    monkeypatch.setenv("GEMINI_API_KEY", "k")
    monkeypatch.setattr(llm, "_request_json", FakeHTTP(error=ProviderError("HTTP 404: not found", status=404)))
    with pytest.raises(ProviderError, match="--list-models gemini"):
        generate_sql("q", "v1", provider="gemini", model="gemini-old")


def test_temperature_env_is_passed_through(monkeypatch):
    monkeypatch.setenv("GEMINI_API_KEY", "k")
    monkeypatch.setenv("ASK_DB_TEMPERATURE", "0")
    fake = FakeHTTP(GEMINI_OK)
    monkeypatch.setattr(llm, "_request_json", fake)
    generate_sql("q", "v1", provider="gemini")
    assert fake.calls[0]["payload"]["generationConfig"]["temperature"] == 0.0


# ---------------------------------------------------------------------- ollama

OLLAMA_OK = {"message": {"role": "assistant", "content": "SELECT COUNT(*) FROM guests"},
             "prompt_eval_count": 800, "eval_count": 12}


def test_ollama_request_shape_and_parsing(monkeypatch):
    fake = FakeHTTP(OLLAMA_OK)
    monkeypatch.setattr(llm, "_request_json", fake)

    result = generate_sql("How many guests?", "v1", provider="ollama", model="tiny:1b")

    call = fake.calls[0]
    assert call["url"].endswith("/api/chat")
    assert call["payload"]["model"] == "tiny:1b" and call["payload"]["stream"] is False
    assert call["payload"]["messages"][0]["role"] == "system"
    assert call["payload"]["messages"][1] == {"role": "user", "content": "How many guests?"}
    assert call["payload"]["options"]["temperature"] == 0.0          # deterministic by default
    assert result.sql == "SELECT COUNT(*) FROM guests"
    assert (result.input_tokens, result.output_tokens) == (800, 12)


def test_ollama_not_running_explains_how_to_start_it(monkeypatch):
    monkeypatch.setattr(llm, "_request_json",
                        FakeHTTP(error=ProviderError("Could not connect to http://localhost:11434/api/chat (refused)")))
    with pytest.raises(ProviderError, match="ollama pull"):
        generate_sql("q", "v1", provider="ollama", model="qwen2.5-coder:7b")


def test_ollama_missing_model_says_to_pull_it(monkeypatch):
    monkeypatch.setattr(llm, "_request_json", FakeHTTP(error=ProviderError("HTTP 404: model not found", status=404)))
    with pytest.raises(ProviderError, match="ollama pull tiny:1b"):
        generate_sql("q", "v1", provider="ollama", model="tiny:1b")


# ------------------------------------------------------------------- anthropic

def test_anthropic_missing_key_is_a_clear_error():
    with pytest.raises(MissingAPIKey, match="platform.claude.com"):
        generate_sql("q", "v1", provider="anthropic")


def test_anthropic_call_uses_sdk(monkeypatch):
    monkeypatch.setenv("ANTHROPIC_API_KEY", "sk-test")
    seen = {}

    class FakeMessages:
        def create(self, **kwargs):
            seen.update(kwargs)
            block = types.SimpleNamespace(type="text", text="SELECT 1")
            return types.SimpleNamespace(content=[block], usage=types.SimpleNamespace(input_tokens=5, output_tokens=2))

    class FakeClient:
        def __init__(self, api_key):
            seen["api_key"] = api_key
            self.messages = FakeMessages()

    monkeypatch.setitem(sys.modules, "anthropic", types.SimpleNamespace(Anthropic=FakeClient))
    result = generate_sql("q", "v1", provider="anthropic", model="claude-test")
    assert seen["api_key"] == "sk-test" and seen["model"] == "claude-test" and "CREATE TABLE" in seen["system"]
    assert result.sql == "SELECT 1" and result.provider == "anthropic"


# ---------------------------------------------------------------------- general

def test_unknown_provider_is_rejected():
    with pytest.raises(ValueError, match="Unknown provider"):
        generate_sql("q", "v1", provider="skynet")


def test_each_provider_has_its_own_default_model(monkeypatch):
    from src.config import default_model
    assert len({default_model(p) for p in ("anthropic", "gemini", "ollama")}) == 3
    monkeypatch.setenv("ASK_DB_MODEL_GEMINI", "my-model")
    assert default_model("gemini") == "my-model" and default_model("ollama") != "my-model"


def test_provider_status_messages(monkeypatch):
    assert provider_status("gemini")[0] is False and "GEMINI_API_KEY" in provider_status("gemini")[1]
    monkeypatch.setenv("GEMINI_API_KEY", "k")
    assert provider_status("gemini")[0] is True
    assert provider_status("anthropic")[0] is False
    monkeypatch.setattr(llm, "_request_json", FakeHTTP(error=ProviderError("refused")))
    ok, message = provider_status("ollama")
    assert ok is False and "not running" in message
    monkeypatch.setattr(llm, "_request_json", FakeHTTP({"models": [{"name": "a"}, {"name": "b"}]}))
    assert provider_status("ollama") == (True, "Ollama is running (2 models installed)")


def test_list_models(monkeypatch):
    monkeypatch.setenv("GEMINI_API_KEY", "k")
    monkeypatch.setattr(llm, "_request_json", FakeHTTP({"models": [
        {"name": "models/gemini-b", "supportedGenerationMethods": ["generateContent"]},
        {"name": "models/embed-x", "supportedGenerationMethods": ["embedContent"]},
        {"name": "models/gemini-a", "supportedGenerationMethods": ["generateContent"]}]}))
    assert llm.list_models("gemini") == ["gemini-a", "gemini-b"]
    monkeypatch.setattr(llm, "_request_json", FakeHTTP({"models": [{"name": "qwen2.5-coder:7b"}]}))
    assert llm.list_models("ollama") == ["qwen2.5-coder:7b"]


# ------------------------------------------------------- the real HTTP helper

def test_rate_limit_is_retried_then_succeeds(monkeypatch):
    import io
    import json
    import urllib.error

    attempts = {"n": 0}
    sleeps = []

    class FakeResponse(io.BytesIO):
        def __enter__(self): return self
        def __exit__(self, *a): return False

    def fake_urlopen(request, timeout=None):
        attempts["n"] += 1
        if attempts["n"] < 3:
            raise urllib.error.HTTPError(request.full_url, 429, "Too Many Requests", {},
                                         io.BytesIO(b'{"error": {"message": "quota"}}'))
        return FakeResponse(json.dumps({"ok": True}).encode())

    monkeypatch.setattr(llm.urllib.request, "urlopen", fake_urlopen)
    monkeypatch.setattr(llm.time, "sleep", sleeps.append)
    assert llm._request_json("POST", "https://x.test/y", {"a": 1}, retries=4) == {"ok": True}
    assert attempts["n"] == 3 and sleeps == [5, 10]


def test_http_error_message_never_contains_the_key(monkeypatch):
    import io
    import urllib.error

    def fake_urlopen(request, timeout=None):
        raise urllib.error.HTTPError(request.full_url, 403, "Forbidden", {},
                                     io.BytesIO(b'{"error": {"message": "API key not valid"}}'))

    monkeypatch.setattr(llm.urllib.request, "urlopen", fake_urlopen)
    with pytest.raises(ProviderError) as info:
        llm._request_json("POST", "https://x.test/y", {}, headers={"x-goog-api-key": "SECRET-KEY-VALUE"})
    assert "SECRET-KEY-VALUE" not in str(info.value) and info.value.status == 403
