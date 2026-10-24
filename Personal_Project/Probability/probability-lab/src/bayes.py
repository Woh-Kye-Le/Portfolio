"""Week 2 - Bayes' rule, three ways of seeing the same arithmetic.

1. *By formula*: ``P(D | +) = P(+ | D) P(D) / P(+)`` with the denominator split
   into the true-positive and false-positive contributions.
2. *By natural frequencies*: the lecture's "imagine 10 000 people" table, where
   the same numbers are whole people you can point at.
3. *By simulation*: draw each person's disease status and test result, then count
   - which is exactly the 10 000-person simulation, run by NumPy.

Applications: medical screening, a naive-Bayes spam filter, fraud detection,
and the two-independent-positive-tests case from the midterm review.
"""

from __future__ import annotations

import math
from typing import Iterable, Mapping

import numpy as np

from src.simulate import compare_estimate, rng

# Defaults used by the app: 1% prevalence, 99% sensitivity, 95% specificity.
SCREENING = {"prevalence": 0.01, "sensitivity": 0.99, "specificity": 0.95}

# Fraud screening: a rare event, a good detector, a non-trivial false alarm rate.
FRAUD = {"prior_fraud": 0.005, "detection_rate": 0.90, "false_alarm_rate": 0.03}

# Spam filter: P(word appears | class) for a five-word vocabulary.
SPAM_WORD_GIVEN_SPAM = {"free": 0.35, "winner": 0.25, "click": 0.20, "meeting": 0.05, "project": 0.10}
SPAM_WORD_GIVEN_HAM = {"free": 0.02, "winner": 0.01, "click": 0.05, "meeting": 0.30, "project": 0.25}
SPAM_PRIOR = 0.30


# ------------------------------------------------------------------ the formula
def posterior(prior: float, sensitivity: float, specificity: float) -> float:
    """P(disease | positive test) for a single test.

    ``sensitivity`` is P(+ | D) and ``specificity`` is P(- | healthy), so the
    false-positive rate is ``1 - specificity``.
    """
    hit = sensitivity * prior
    false_alarm = (1.0 - specificity) * (1.0 - prior)
    return hit / (hit + false_alarm)


def posterior_breakdown(prior: float, sensitivity: float, specificity: float) -> dict:
    """Every ingredient of the formula, ready to be printed next to it."""
    false_positive_rate = 1.0 - specificity
    true_positive_share = sensitivity * prior
    false_positive_share = false_positive_rate * (1.0 - prior)
    total_positive = true_positive_share + false_positive_share
    return {
        "prior": prior,
        "sensitivity": sensitivity,
        "specificity": specificity,
        "false_positive_rate": false_positive_rate,
        "p_positive": total_positive,
        "true_positive_share": true_positive_share,
        "false_positive_share": false_positive_share,
        "true_positive_fraction_of_positives": true_positive_share / total_positive,
        "posterior": true_positive_share / total_positive,
    }


# ------------------------------------------------------- natural frequencies
def natural_frequencies(
    prevalence: float,
    sensitivity: float,
    specificity: float,
    population: int = 10_000,
) -> dict:
    """The "out of ``population`` people" table: counts, not fractions."""
    diseased = population * prevalence
    healthy = population - diseased
    true_positives = diseased * sensitivity
    false_negatives = diseased - true_positives
    false_positives = healthy * (1.0 - specificity)
    true_negatives = healthy - false_positives
    test_positives = true_positives + false_positives
    return {
        "population": population,
        "diseased": diseased,
        "healthy": healthy,
        "true_positives": true_positives,
        "false_positives": false_positives,
        "false_negatives": false_negatives,
        "true_negatives": true_negatives,
        "test_positives": test_positives,
        "posterior": true_positives / test_positives,
    }


# --------------------------------------------------------------- the simulation
def simulate_screening(
    prevalence: float,
    sensitivity: float,
    specificity: float,
    population: int = 10_000,
    seed: int | None = None,
) -> dict:
    """Draw ``population`` people, test them, and count the 2x2 table.

    Each person is diseased with probability ``prevalence``; the test then fires
    with probability ``sensitivity`` for a diseased person and ``1 - specificity``
    for a healthy one.  The empirical P(D | +) is compared with the formula.
    """
    gen = rng(seed)
    diseased = gen.random(population) < prevalence
    detect = gen.random(population) < np.where(diseased, sensitivity, 1.0 - specificity)

    true_positives = int(np.count_nonzero(diseased & detect))
    false_positives = int(np.count_nonzero(~diseased & detect))
    test_positives = true_positives + false_positives
    estimate = true_positives / test_positives if test_positives else float("nan")
    exact = posterior(prevalence, sensitivity, specificity)

    return {
        "population": population,
        "diseased": int(np.count_nonzero(diseased)),
        "test_positives": test_positives,
        "true_positives": true_positives,
        "false_positives": false_positives,
        "false_negatives": int(np.count_nonzero(diseased & ~detect)),
        "empirical_prevalence": float(np.mean(diseased)),
        "empirical_sensitivity": (
            float(np.mean(detect[diseased])) if np.any(diseased) else float("nan")
        ),
        "posterior_estimate": estimate,
        "exact_posterior": exact,
        "comparison": compare_estimate(
            estimate,
            exact,
            math.sqrt(max(exact * (1.0 - exact), 0.0) / max(test_positives, 1)),
        ),
    }


# ----------------------------------------------------- two tests, midterm review
def sequential_update(
    prior: float,
    sensitivity: float,
    specificity: float,
    n_tests: int = 2,
) -> list[dict]:
    """Feed each positive test result back in as the next prior.

    Assumes the two tests are conditionally independent given the disease state.
    """
    steps = []
    current = prior
    for test in range(1, n_tests + 1):
        updated = posterior(current, sensitivity, specificity)
        steps.append({"test": test, "prior": current, "posterior": updated})
        current = updated
    return steps


def two_positive_tests(prior: float, sensitivity: float, specificity: float) -> dict:
    """P(D | +, +) - the sequential update and the one-shot likelihood agree.

    The one-shot version uses P(+, + | D) = sensitivity^2 and
    P(+, + | healthy) = (1 - specificity)^2.
    """
    steps = sequential_update(prior, sensitivity, specificity, n_tests=2)
    hit = sensitivity**2 * prior
    false_alarm = (1.0 - specificity) ** 2 * (1.0 - prior)
    return {
        "after_first": steps[0]["posterior"],
        "after_second_sequential": steps[1]["posterior"],
        "after_second_direct": hit / (hit + false_alarm),
        "positive_twice_given_disease": sensitivity**2,
        "positive_twice_given_healthy": (1.0 - specificity) ** 2,
        "p_two_positives": hit + false_alarm,
    }


def simulate_two_tests(
    prior: float,
    sensitivity: float,
    specificity: float,
    population: int = 100_000,
    seed: int | None = None,
) -> dict:
    """Same population, but everyone is tested twice; count P(D | +, +)."""
    gen = rng(seed)
    diseased = gen.random(population) < prior
    probability = np.where(diseased, sensitivity, 1.0 - specificity)
    positive_twice = (gen.random(population) < probability) & (gen.random(population) < probability)

    true_positives = int(np.count_nonzero(diseased & positive_twice))
    false_positives = int(np.count_nonzero(~diseased & positive_twice))
    positives = true_positives + false_positives
    exact = two_positive_tests(prior, sensitivity, specificity)["after_second_direct"]
    estimate = true_positives / positives if positives else float("nan")

    return {
        "population": population,
        "positive_twice": positives,
        "true_positives": true_positives,
        "false_positives": false_positives,
        "posterior_estimate": estimate,
        "exact_posterior": exact,
        "comparison": compare_estimate(
            estimate,
            exact,
            math.sqrt(max(exact * (1.0 - exact), 0.0) / max(positives, 1)),
        ),
    }


# ------------------------------------------------------------------ spam filter
def naive_bayes_spam(
    word_given_spam: Mapping[str, float],
    word_given_ham: Mapping[str, float],
    message_words: Iterable[str],
    prior_spam: float = SPAM_PRIOR,
) -> dict:
    """Bernoulli naive Bayes over word *presence* in the message.

    For every word in the vocabulary the likelihood is ``P(word | class)`` when
    the word appears in the message and ``1 - P(word | class)`` when it does not,
    so words that are missing also carry evidence.  The class scores are summed
    in log space; every probability in the tables must sit strictly between 0 and 1.
    """
    present = {word.lower() for word in message_words}
    vocabulary = sorted(set(word_given_spam) | set(word_given_ham))

    log_spam = math.log(prior_spam)
    log_ham = math.log1p(-prior_spam)
    contributions = []

    for word in vocabulary:
        p_given_spam = word_given_spam.get(word, 0.0)
        p_given_ham = word_given_ham.get(word, 0.0)
        in_message = word in present
        likelihood_spam = p_given_spam if in_message else 1.0 - p_given_spam
        likelihood_ham = p_given_ham if in_message else 1.0 - p_given_ham

        log_spam += math.log(likelihood_spam)
        log_ham += math.log(likelihood_ham)
        contributions.append(
            {
                "word": word,
                "in_message": in_message,
                "p_given_spam": p_given_spam,
                "p_given_ham": p_given_ham,
                "evidence": "present" if in_message else "absent",
                "log_likelihood_ratio": math.log(likelihood_spam) - math.log(likelihood_ham),
            }
        )

    log_ratio = log_spam - log_ham
    return {
        "prior_spam": prior_spam,
        "spam_score": math.exp(log_spam),
        "ham_score": math.exp(log_ham),
        "log_likelihood_ratio": log_ratio,
        "posterior_spam": 1.0 / (1.0 + math.exp(-log_ratio)),
        "contributions": contributions,
    }


def simulate_spam_filter(
    word_given_spam: Mapping[str, float],
    word_given_ham: Mapping[str, float],
    message_words: Iterable[str],
    prior_spam: float = SPAM_PRIOR,
    n_messages: int = 300_000,
    seed: int | None = None,
) -> dict:
    """Generate messages from the same model and check the posterior by counting.

    Messages are sampled from the model, the ones whose word-presence pattern
    matches the message under study are kept, and the spam share among those is
    the simulated posterior - a Monte-Carlo version of the naive-Bayes formula.
    """
    present = {word.lower() for word in message_words}
    vocabulary = sorted(set(word_given_spam) | set(word_given_ham))

    gen = rng(seed)
    is_spam = gen.random(n_messages) < prior_spam
    pattern = np.ones(n_messages, dtype=bool)
    for word in vocabulary:
        p_spam = word_given_spam.get(word, 0.0)
        p_ham = word_given_ham.get(word, 0.0)
        probability = np.where(is_spam, p_spam, p_ham)
        appears = gen.random(n_messages) < probability
        pattern &= appears == (word in present)

    matches = int(np.count_nonzero(pattern))
    spam_matches = int(np.count_nonzero(pattern & is_spam))
    estimate = spam_matches / matches if matches else float("nan")
    exact = naive_bayes_spam(word_given_spam, word_given_ham, present, prior_spam)["posterior_spam"]

    return {
        "n_messages": n_messages,
        "matches": matches,
        "spam_matches": spam_matches,
        "posterior_estimate": estimate,
        "exact_posterior": exact,
        "comparison": compare_estimate(
            estimate,
            exact,
            math.sqrt(max(exact * (1.0 - exact), 0.0) / max(matches, 1)),
        ),
    }


# --------------------------------------------------------------- fraud detection
def fraud_detection(
    prior_fraud: float = FRAUD["prior_fraud"],
    detection_rate: float = FRAUD["detection_rate"],
    false_alarm_rate: float = FRAUD["false_alarm_rate"],
    population: int = 100_000,
    seed: int | None = None,
) -> dict:
    """Fraud screening is the medical-screening problem with different labels."""
    specificity = 1.0 - false_alarm_rate
    return {
        "prior_fraud": prior_fraud,
        "detection_rate": detection_rate,
        "false_alarm_rate": false_alarm_rate,
        "posterior": posterior(prior_fraud, detection_rate, specificity),
        "breakdown": posterior_breakdown(prior_fraud, detection_rate, specificity),
        "frequencies": natural_frequencies(prior_fraud, detection_rate, specificity, population),
        "simulation": simulate_screening(prior_fraud, detection_rate, specificity, population, seed),
    }
