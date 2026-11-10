"""Streamlit UI: question in, table + the SQL that produced it out.

Run with:  streamlit run app.py
"""
from __future__ import annotations

import pandas as pd
import streamlit as st

from eval.run_eval import load_questions
from src.config import DEFAULT_DB_PATH, DEFAULT_PROMPT_VERSION, DEFAULT_PROVIDER, PROVIDERS, default_model
from src.data_checks import run_checks
from src.llm import provider_status
from src.pipeline import ask
from src.prompt import available_versions

PROVIDER_LABELS = {
    "gemini": "Google Gemini (free tier)",
    "ollama": "Ollama (local, free)",
    "anthropic": "Claude API (paid)",
}

st.set_page_config(page_title="Ask the Database", layout="wide")
st.title("Ask the Database")
st.caption("Ask a question in plain English. An AI model writes the SQL, a safety guard checks it, "
           "and it runs read-only on a synthetic resort database. No real company data.")

if not DEFAULT_DB_PATH.exists():
    st.error("The database has not been built yet. Run `python -m db.seed` and refresh.")
    st.stop()

with st.sidebar:
    st.header("Settings")
    provider = st.selectbox(
        "AI provider", PROVIDERS,
        index=PROVIDERS.index(DEFAULT_PROVIDER) if DEFAULT_PROVIDER in PROVIDERS else 0,
        format_func=lambda p: PROVIDER_LABELS[p],
    )
    # One model box per provider, so switching provider never carries over a wrong model name.
    model = st.text_input("Model", value=default_model(provider), key=f"model_{provider}")
    ready, status_message = provider_status(provider)
    (st.success if ready else st.warning)(status_message)

    versions = available_versions()
    default_idx = versions.index(DEFAULT_PROMPT_VERSION) if DEFAULT_PROMPT_VERSION in versions else len(versions) - 1
    prompt_version = st.selectbox("Prompt version", versions, index=default_idx,
                                  help="Files in prompts/. Compare versions with the eval harness.")
    st.markdown("**Safety:** one SELECT only, writes blocked, `LIMIT 100` forced, "
                "database opened read-only.")

tab_ask, tab_quality = st.tabs(["Ask", "Data quality"])


def show_result(result) -> None:
    if result.stage == "invalid":
        st.warning(result.error)
        return
    if result.stage == "llm_error":
        st.error(f"Could not get SQL from the AI. {result.error}")
        return

    if result.stage == "blocked":
        st.error(f"Blocked by the safety guard: {result.error}")
    elif result.stage == "sql_error":
        st.error(f"The database could not run that SQL: {result.error}")
    else:
        st.subheader("Answer")
        if result.rows:
            st.dataframe(pd.DataFrame(result.rows, columns=result.columns), hide_index=True)
        else:
            st.info("The query ran fine but returned no rows.")
        st.caption(f"{len(result.rows)} row(s) · {result.latency_s:.1f}s · "
                   f"{result.provider} / {result.model} · prompt {result.prompt_version}")

    st.subheader("SQL the AI wrote")
    st.code(result.generated_sql, language="sql")
    if result.executed_sql and result.executed_sql != result.generated_sql:
        with st.expander("SQL that actually ran (after the safety guard)"):
            st.code(result.executed_sql, language="sql")


with tab_ask:
    examples = [""] + [q["question"] for q in load_questions()]
    example = st.selectbox("Try an example", examples,
                           format_func=lambda s: s or "Pick an example, or type your own below")
    with st.form("ask_form"):
        question = st.text_input("Your question", value=example,
                                 placeholder="Which restaurant had the highest revenue last month?")
        submitted = st.form_submit_button("Ask")
    st.caption("In this dataset today is 2026-07-01, so \"last month\" means June 2026.")
    if submitted:
        with st.spinner("Writing and running the SQL..."):
            show_result(ask(question, prompt_version, model.strip() or None, provider=provider))


@st.cache_data(show_spinner=False)
def cached_checks():
    return run_checks()


with tab_quality:
    st.write("Checks for nulls, duplicates, orphan IDs, negative amounts and broken logic. "
             "A few defects were planted by the seed script on purpose.")
    checks = cached_checks()
    st.dataframe(
        pd.DataFrame([{"Status": c.status, "Category": c.category, "Table": c.table,
                       "Check": c.description, "Rows": c.count} for c in checks]),
        hide_index=True,
    )
    with st.expander("The SQL behind each check"):
        for c in checks:
            st.markdown(f"**{c.table}**: {c.description}")
            st.code(c.sql, language="sql")
