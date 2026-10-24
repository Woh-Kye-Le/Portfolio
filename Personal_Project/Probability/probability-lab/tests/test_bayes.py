"""Tests for :mod:`src.bayes` - the formula, natural frequencies and the simulations."""

from __future__ import annotations

import math

import pytest

from src import bayes

SEED = 1234
# Defaults used by the app: 1% prevalence, 99% sensitivity, 95% specificity.
PREVALENCE, SENSITIVITY, SPECIFICITY = 0.01, 0.99, 0.95


def test_posterior_formula() -> None:
    """P(D | +) = 0.99 * 0.01 / (0.99 * 0.01 + 0.05 * 0.99) = 1/6."""
    value = bayes.posterior(PREVALENCE, SENSITIVITY, SPECIFICITY)
    assert value == pytest.approx(0.1666666, rel=1e-6)
    assert value == pytest.approx(0.0099 / (0.0099 + 0.05 * 0.99))
    assert value == pytest.approx(bayes.posterior(*bayes.SCREENING.values()))


def test_posterior_breakdown_is_consistent() -> None:
    """p_positive is the sum of the two shares and the posterior is their ratio."""
    breakdown = bayes.posterior_breakdown(PREVALENCE, SENSITIVITY, SPECIFICITY)
    assert breakdown["false_positive_rate"] == pytest.approx(1.0 - SPECIFICITY)
    assert breakdown["true_positive_share"] == pytest.approx(SENSITIVITY * PREVALENCE)
    assert breakdown["false_positive_share"] == pytest.approx((1.0 - SPECIFICITY) * (1.0 - PREVALENCE))
    assert breakdown["p_positive"] == pytest.approx(
        breakdown["true_positive_share"] + breakdown["false_positive_share"]
    )
    assert breakdown["posterior"] == pytest.approx(
        breakdown["true_positive_share"] / breakdown["p_positive"]
    )
    assert breakdown["posterior"] == pytest.approx(bayes.posterior(PREVALENCE, SENSITIVITY, SPECIFICITY))
    assert breakdown["true_positive_fraction_of_positives"] == pytest.approx(breakdown["posterior"])


def test_natural_frequencies_are_integer_consistent() -> None:
    """Out of 10 000 people: 100 diseased, 99 true positives, 495 false positives."""
    table = bayes.natural_frequencies(PREVALENCE, SENSITIVITY, SPECIFICITY, population=10_000)
    assert table["population"] == 10_000
    assert (table["diseased"], table["healthy"]) == (100.0, 9_900.0)
    assert table["diseased"] + table["healthy"] == pytest.approx(table["population"])
    assert table["true_positives"] == pytest.approx(99.0)
    assert table["false_positives"] == pytest.approx(495.0)
    assert table["test_positives"] == pytest.approx(
        table["true_positives"] + table["false_positives"]
    )
    assert table["false_negatives"] + table["true_positives"] == pytest.approx(table["diseased"])
    assert table["true_negatives"] + table["false_positives"] == pytest.approx(table["healthy"])
    assert table["posterior"] == pytest.approx(table["true_positives"] / table["test_positives"])
    assert table["posterior"] == pytest.approx(bayes.posterior(PREVALENCE, SENSITIVITY, SPECIFICITY))


def test_simulate_screening_agrees_with_the_formula() -> None:
    """10 000 simulated people: P(D | +) lands within 2 standard errors of 1/6."""
    result = bayes.simulate_screening(PREVALENCE, SENSITIVITY, SPECIFICITY, 10_000, seed=SEED)
    assert result["comparison"]["agrees"] is True
    assert result["exact_posterior"] == pytest.approx(0.1666666, rel=1e-6)
    assert result["posterior_estimate"] == pytest.approx(
        result["true_positives"] / result["test_positives"]
    )
    assert result["true_positives"] + result["false_positives"] == result["test_positives"]
    assert result["empirical_prevalence"] == pytest.approx(PREVALENCE, abs=0.01)


def test_sequential_and_direct_two_positive_tests_agree() -> None:
    """Feeding the posterior back in gives the same answer as the squared likelihood."""
    result = bayes.two_positive_tests(PREVALENCE, SENSITIVITY, SPECIFICITY)
    assert result["after_second_sequential"] == pytest.approx(result["after_second_direct"])
    assert result["after_first"] == pytest.approx(bayes.posterior(PREVALENCE, SENSITIVITY, SPECIFICITY))
    assert result["after_second_sequential"] > result["after_first"]
    assert result["positive_twice_given_disease"] == pytest.approx(SENSITIVITY**2)
    assert result["positive_twice_given_healthy"] == pytest.approx((1.0 - SPECIFICITY) ** 2)
    assert result["p_two_positives"] == pytest.approx(
        SENSITIVITY**2 * PREVALENCE + (1.0 - SPECIFICITY) ** 2 * (1.0 - PREVALENCE)
    )

    steps = bayes.sequential_update(PREVALENCE, SENSITIVITY, SPECIFICITY, n_tests=3)
    assert [step["test"] for step in steps] == [1, 2, 3]
    assert steps[0]["prior"] == pytest.approx(PREVALENCE)
    assert steps[1]["prior"] == pytest.approx(steps[0]["posterior"])  # each posterior is the next prior


def test_simulate_two_tests_agrees() -> None:
    result = bayes.simulate_two_tests(PREVALENCE, SENSITIVITY, SPECIFICITY, 100_000, seed=SEED)
    assert result["comparison"]["agrees"] is True
    assert result["exact_posterior"] == pytest.approx(
        bayes.two_positive_tests(PREVALENCE, SENSITIVITY, SPECIFICITY)["after_second_direct"]
    )
    assert result["true_positives"] + result["false_positives"] == result["positive_twice"]


def test_naive_bayes_flags_the_spammy_message() -> None:
    result = bayes.naive_bayes_spam(
        bayes.SPAM_WORD_GIVEN_SPAM, bayes.SPAM_WORD_GIVEN_HAM, {"free", "winner", "click"}
    )
    assert result["posterior_spam"] > 0.9
    assert result["posterior_spam"] == pytest.approx(1.0 / (1.0 + result["ham_score"] / result["spam_score"]))
    assert result["posterior_spam"] == pytest.approx(1.0 / (1.0 + math.exp(-result["log_likelihood_ratio"])))


def test_naive_bayes_keeps_a_neutral_message_below_half() -> None:
    result = bayes.naive_bayes_spam(
        bayes.SPAM_WORD_GIVEN_SPAM, bayes.SPAM_WORD_GIVEN_HAM, {"meeting", "project"}
    )
    assert result["posterior_spam"] < 0.5
    assert result["posterior_spam"] < bayes.SPAM_PRIOR


def test_naive_bayes_handles_absent_words_and_reports_every_vocabulary_word() -> None:
    """An empty message still carries the "these words are missing" evidence."""
    result = bayes.naive_bayes_spam(bayes.SPAM_WORD_GIVEN_SPAM, bayes.SPAM_WORD_GIVEN_HAM, set())
    vocabulary = set(bayes.SPAM_WORD_GIVEN_SPAM) | set(bayes.SPAM_WORD_GIVEN_HAM)
    assert 0.0 < result["posterior_spam"] < 1.0
    assert len(result["contributions"]) == len(vocabulary) == 5
    assert {row["word"] for row in result["contributions"]} == vocabulary
    assert all(row["in_message"] is False for row in result["contributions"])
    assert all(row["evidence"] == "absent" for row in result["contributions"])


def test_naive_bayes_contribution_log_ratios_sum_to_the_total() -> None:
    words = {"free", "meeting"}
    result = bayes.naive_bayes_spam(bayes.SPAM_WORD_GIVEN_SPAM, bayes.SPAM_WORD_GIVEN_HAM, words)
    prior_ratio = bayes.SPAM_PRIOR / (1.0 - bayes.SPAM_PRIOR)
    total = sum(row["log_likelihood_ratio"] for row in result["contributions"]) + math.log(prior_ratio)
    assert result["log_likelihood_ratio"] == pytest.approx(total, rel=1e-12)
    assert {row["word"] for row in result["contributions"] if row["in_message"]} == words


def test_simulate_spam_filter_agrees() -> None:
    result = bayes.simulate_spam_filter(
        bayes.SPAM_WORD_GIVEN_SPAM,
        bayes.SPAM_WORD_GIVEN_HAM,
        {"free", "winner", "click"},
        n_messages=200_000,
        seed=SEED,
    )
    assert result["comparison"]["agrees"] is True
    assert result["exact_posterior"] > 0.9
    assert result["spam_matches"] <= result["matches"]
    assert result["posterior_estimate"] == pytest.approx(result["spam_matches"] / result["matches"])


def test_fraud_detection_reports_consistent_numbers() -> None:
    result = bayes.fraud_detection(seed=SEED)
    posterior = result["posterior"]
    assert 0.0 < posterior < 1.0
    assert result["breakdown"]["posterior"] == pytest.approx(posterior)
    assert result["frequencies"]["posterior"] == pytest.approx(posterior)
    assert result["frequencies"]["posterior"] == pytest.approx(
        result["frequencies"]["true_positives"] / result["frequencies"]["test_positives"]
    )
    assert posterior == pytest.approx(
        bayes.posterior(result["prior_fraud"], result["detection_rate"], 1.0 - result["false_alarm_rate"])
    )


def test_fraud_detection_simulation_agrees() -> None:
    result = bayes.fraud_detection(seed=SEED)
    assert result["simulation"]["comparison"]["agrees"] is True
    assert result["simulation"]["exact_posterior"] == pytest.approx(result["posterior"])
