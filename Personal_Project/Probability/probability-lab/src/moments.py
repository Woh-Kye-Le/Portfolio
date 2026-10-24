"""Week 5 - Expectation and variance, computed four ways and then checked.

* from the definition: ``E[X] = sum x p(x)`` and ``Var(X) = E[(X - E[X])^2]``,
* from the shortcut: ``Var(X) = E[X^2] - E[X]^2``,
* from a simulated sample,
* from the MGF: ``E[X^r] = M^(r)(0)``.

Plus linearity of expectation with indicators, which is why a Binomial mean is
``np`` without ever writing down the Binomial PMF.
"""

from __future__ import annotations

import math
from typing import Callable, Sequence

import numpy as np

from src.simulate import bernoulli, compare_estimate, sample_mean_error, sample_moments


# --------------------------------------------------------------- the definitions
def expectation(values: Sequence[float], probs: Sequence[float]) -> float:
    """E[X] = sum x p(x) - straight from the definition."""
    return math.fsum(x * p for x, p in zip(values, probs))


def expectation_of(
    g: Callable[[float], float],
    values: Sequence[float],
    probs: Sequence[float],
) -> float:
    """E[g(X)] = sum g(x) p(x) - the law of the unconscious statistician."""
    return math.fsum(g(x) * p for x, p in zip(values, probs))


def second_moment(values: Sequence[float], probs: Sequence[float]) -> float:
    """E[X^2], the ingredient the shortcut formula needs."""
    return expectation_of(lambda x: x * x, values, probs)


def central_moment(values: Sequence[float], probs: Sequence[float], order: int = 2) -> float:
    """E[(X - E[X])^r]; the fourth one gives the error bar of a sample variance."""
    mu = expectation(values, probs)
    return expectation_of(lambda x: (x - mu) ** order, values, probs)


def variance_from_definition(values: Sequence[float], probs: Sequence[float]) -> float:
    """Var(X) = E[(X - E[X])^2], averaging over the deviations themselves."""
    return central_moment(values, probs, 2)


def variance_shortcut(values: Sequence[float], probs: Sequence[float]) -> float:
    """Var(X) = E[X^2] - (E[X])^2, algebraically identical and one pass cheaper."""
    mu = expectation(values, probs)
    return second_moment(values, probs) - mu * mu


def standard_deviation(values: Sequence[float], probs: Sequence[float]) -> float:
    """sqrt(Var(X))."""
    return math.sqrt(variance_from_definition(values, probs))


def sample_variance_error(values: Sequence[float], probs: Sequence[float], n: int) -> float:
    """Approximate standard error of a simulated variance: sqrt((mu4 - sigma^4) / n)."""
    variance = variance_from_definition(values, probs)
    fourth = central_moment(values, probs, 4)
    return math.sqrt(max(fourth - variance**2, 0.0) / n)


# ------------------------------------------------------------- the moment reports
def moment_report(
    values: Sequence[float],
    probs: Sequence[float],
    sample: np.ndarray,
) -> dict:
    """Definition, shortcut and simulation side by side for one distribution."""
    exact_mean = expectation(values, probs)
    exact_variance = variance_from_definition(values, probs)
    moments = sample_moments(sample)
    return {
        "exact_mean": exact_mean,
        "exact_variance": exact_variance,
        "exact_std": math.sqrt(exact_variance),
        "mean_definition": exact_mean,
        "mean_shortcut": expectation_of(lambda x: x, values, probs),
        "variance_definition": exact_variance,
        "variance_shortcut": variance_shortcut(values, probs),
        "sample_mean": moments["mean"],
        "sample_variance": moments["variance"],
        "sample_size": moments["n"],
        "mean_comparison": compare_estimate(
            moments["mean"], exact_mean, sample_mean_error(sample)
        ),
        "variance_comparison": compare_estimate(
            moments["variance"], exact_variance, sample_variance_error(values, probs, sample.size)
        ),
    }


def moment_report_table(report: dict) -> list[dict]:
    """The same numbers as rows, so the app can render one small table."""
    return [
        {
            "route": "definition",
            "E[X]": report["mean_definition"],
            "Var(X)": report["variance_definition"],
        },
        {
            "route": "shortcut formula",
            "E[X]": report["mean_shortcut"],
            "Var(X)": report["variance_shortcut"],
        },
        {
            "route": f"simulation (n = {report['sample_size']:,})",
            "E[X]": report["sample_mean"],
            "Var(X)": report["sample_variance"],
        },
    ]


# ------------------------------------------------------------------ the MGF route
def mgf(values: Sequence[float], probs: Sequence[float], t: float) -> float:
    """M_X(t) = E[e^{tX}] = sum e^{t x} p(x)."""
    return math.fsum(math.exp(t * x) * p for x, p in zip(values, probs))


def mgf_derivative(
    values: Sequence[float],
    probs: Sequence[float],
    t: float,
    order: int = 1,
) -> float:
    """M^(r)(t) = sum x^r e^{t x} p(x) - differentiating inside the sum."""
    return math.fsum(x**order * math.exp(t * x) * p for x, p in zip(values, probs))


def moments_from_mgf(values: Sequence[float], probs: Sequence[float]) -> dict:
    """Recover the moments from the MGF: E[X] = M'(0), E[X^2] = M''(0)."""
    first = mgf_derivative(values, probs, 0.0, 1)
    second = mgf_derivative(values, probs, 0.0, 2)
    return {
        "M_0": mgf(values, probs, 0.0),
        "E_X": first,
        "E_X2": second,
        "Var": second - first * first,
    }


# ----------------------------------------------- reference distributions on hand
def die_distribution() -> tuple[list[int], list[float]]:
    """One fair die: values 1..6, each with probability 1/6."""
    values = [1, 2, 3, 4, 5, 6]
    return values, [1.0 / 6.0] * 6


def binomial_distribution(n: int, p: float) -> tuple[list[int], list[float]]:
    """Binomial PMF as plain value/probability lists (from :mod:`src.discrete`)."""
    from src.discrete import binomial_pmf

    values = list(range(n + 1))
    return values, [binomial_pmf(k, n, p) for k in values]


def poisson_distribution(lam: float, k_max: int = 30) -> tuple[list[int], list[float]]:
    """Poisson PMF truncated at ``k_max`` and renormalised, for finite sums."""
    from src.discrete import poisson_pmf

    values = list(range(k_max + 1))
    probs = [poisson_pmf(k, lam) for k in values]
    total = math.fsum(probs)
    return values, [p / total for p in probs]


def geometric_distribution(p: float, k_max: int = 400) -> tuple[list[int], list[float]]:
    """Geometric PMF truncated at ``k_max`` and renormalised, for finite sums."""
    from src.discrete import geometric_pmf

    values = list(range(1, k_max + 1))
    probs = [geometric_pmf(k, p) for k in values]
    total = math.fsum(probs)
    return values, [q / total for q in probs]


def mgf_moment_table() -> list[dict]:
    """MGF-derived moments against the analytic ones, one row per distribution."""
    from src.discrete import (
        binomial_mean,
        binomial_variance,
        geometric_mean,
        geometric_variance,
        poisson_mean,
        poisson_variance,
    )

    cases = [
        ("Fair die", *die_distribution(), 3.5, 35.0 / 12.0),
        (
            "Binomial(10, 0.3)",
            *binomial_distribution(10, 0.3),
            binomial_mean(10, 0.3),
            binomial_variance(10, 0.3),
        ),
        (
            "Poisson(3) (truncated, renormalised)",
            *poisson_distribution(3.0),
            poisson_mean(3.0),
            poisson_variance(3.0),
        ),
        (
            "Geometric(0.2) (truncated, renormalised)",
            *geometric_distribution(0.2),
            geometric_mean(0.2),
            geometric_variance(0.2),
        ),
    ]

    rows = []
    for name, values, probs, analytic_mean, analytic_variance in cases:
        recovered = moments_from_mgf(values, probs)
        rows.append(
            {
                "distribution": name,
                "M(0)": recovered["M_0"],
                "E[X] from M'(0)": recovered["E_X"],
                "Var from M''(0)": recovered["Var"],
                "analytic E[X]": analytic_mean,
                "analytic Var(X)": analytic_variance,
                "mean_error": abs(recovered["E_X"] - analytic_mean),
                "variance_error": abs(recovered["Var"] - analytic_variance),
            }
        )
    return rows


# ------------------------------------------------- linearity of expectation, np
def indicator_decomposition(n: int, p: float) -> dict:
    """X = I1 + ... + In, with I_i the indicator that trial i succeeded.

    Linearity needs no independence at all: ``E[X] = sum E[I_i] = np``.
    """
    return {
        "n": n,
        "p": p,
        "terms": n,
        "each_indicator_mean": p,
        "sum_of_indicator_means": n * p,
        "binomial_mean": n * p,
        "binomial_variance": n * p * (1.0 - p),
        "expected_successes": n * p,
    }


def simulate_linearity(
    n: int = 10,
    p: float = 0.3,
    trials: int = 50_000,
    seed: int | None = None,
) -> dict:
    """Simulate the indicators, the sum and their means.

    Every indicator is just a Bernoulli variable, and the mean of the count is
    the mean of the sum - the simulation cannot tell the difference, which is
    the practical content of linearity.
    """
    flags = bernoulli(p, (trials, n), seed)
    counts = flags.sum(axis=1)
    indicator_means = flags.mean(axis=0)
    exact_mean = n * p
    return {
        "n": n,
        "p": p,
        "trials": trials,
        "indicator_means": indicator_means,
        "average_indicator_mean": float(np.mean(indicator_means)),
        "mean_of_counts": float(np.mean(counts)),
        "exact_mean": exact_mean,
        "summed_indicator_means": float(n * np.mean(indicator_means)),
        "sample_variance": float(np.var(counts, ddof=1)),
        "exact_variance": n * p * (1.0 - p),
        "indicator_comparison": compare_estimate(
            float(np.mean(indicator_means)), p, math.sqrt(p * (1.0 - p) / (trials * n))
        ),
        "count_comparison": compare_estimate(
            float(np.mean(counts)),
            exact_mean,
            math.sqrt(n * p * (1.0 - p) / trials),
        ),
        "counts": counts,
    }
