# Evaluation results

This is the write-up. The numbers and causes below are **yours to fill in** after real runs;
nothing here has been run against an LLM yet.

How to produce them:

```bash
# first choose a provider and set its key (see README)
python -m eval.run_eval --prompt v1     # baseline
python -m eval.run_eval --prompt v2     # after adding rules + examples
python -m eval.run_eval --history       # accuracy of every saved run
```

Each run saves `eval/runs/<time>_<prompt>_<model>.md` with every failure laid out
(generated SQL, reference SQL, expected vs. got). Copy the interesting ones below and
write the cause in your own words. LLM output varies a little from run to run, so for
numbers you quote, run each prompt at least 3 times (or set `ASK_DB_TEMPERATURE=0`).

## Accuracy by prompt version

| Run | Prompt | Provider / model | Correct | Accuracy | What changed |
|---|---|---|---:|---:|---|
| 1 | v1 | _fill in_ | _/20_ | _%_ | Baseline: schema + "return only the SQL" |
| 2 | v2 | _fill in_ | _/20_ | _%_ | Added dates, business rules, style rules, 4 examples |
| 3 | v3 | _fill in_ | _/20_ | _%_ | _your next change, based on the failures below_ |

q20 is ambiguous on purpose; when you quote a headline number, say whether it includes it
(e.g. "17/19 on unambiguous questions").

## Failure causes

Use the same labels every time so you can count them:

| Label | Meaning |
|---|---|
| wrong join | joined the wrong tables or keys, or double-counted through a join |
| wrong date range | misread "last month", "last quarter", "this year", or an off-by-one on a boundary |
| missing business rule | did something sensible the schema can't tell it (e.g. which bookings count as revenue) |
| wrong aggregation | summed vs. counted, distinct vs. not, NULL handling, integer division |
| ambiguous question | more than one reasonable reading; the model picked a different one |
| bad output shape | right idea, wrong columns or format (e.g. month as `3` instead of `2026-03`) |
| invalid SQL | syntax error or hallucinated column; the database rejected it |
| blocked by guard | the guard rejected it (extra text around the SQL, a forbidden keyword) |

## Failure log

Copy one block per failure per run.

| Prompt | Q | What went wrong | Cause label | Fix tried | Fixed in |
|---|---|---|---|---|---|
| v1 | _q__ | _one line_ | _label_ | _what you changed in the prompt_ | _v2_ |

## Hypotheses written before the first run

These are guesses to test, not results. Replace them with your own, then see who was right.

- **q04, q05, q16 (relative dates).** v1 never says what today's date is, so "last month" and "last quarter" should fail or be inconsistent. v2 resolves the dates explicitly.
- **q08 (room revenue).** Whether cancelled and no-show bookings count is not in the schema. v1 will probably sum everything. v2 states the rule.
- **q19 (ticket revenue).** The formula is attendance x ticket_price. Models will probably get this right even in v1.
- **q13, q14 (top N per group).** A capable model should handle window functions; failures here are more likely tie-breaking or column-shape issues than logic.
- **q20 (ambiguous).** Expect disagreement however good the prompt is. That is the point.

## What I learned

_Three or four sentences once you have the data: what moved the accuracy most, what the
prompt cannot fix, and what you would do next (schema comments? a clarifying-question step?
a retry when the database rejects the SQL?)._
