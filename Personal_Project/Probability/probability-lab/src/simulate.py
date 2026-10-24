"""Shared Monte-Carlo layer for the whole lab.

Every topic module computes an exact answer first and then estimates the same
quantity from random samples; this file is the only place randomness enters,
and every generator is seeded, so the Streamlit app and the test-suite agree on
the numbers from run to run.

Conventions used everywhere:

* a *sample* is a 1-D ``numpy`` array of i.i.d. draws,
* a *frequency* estimate of a probability ``p`` has standard error
  ``sqrt(p(1-p)/n)`` and is accepted when it is within
  :data:`SIGMA` standard errors of the exact value,
* a *mean* estimate of ``E[X]`` has standard error ``s/sqrt(n)``.
"""

from __future__ import annotations

import numpy as np

# Fixed so that the app, the README and the tests all quote the same numbers.
DEFAULT_SEED = 20260221

#: How many estimated standard errors the simulations are allowed to drift.
SIGMA = 2.0


def rng(seed: int | None = DEFAULT_SEED) -> np.random.Generator:
    """A fresh generator, so callers never share hidden global state."""
    return np.random.default_rng(seed)


# --------------------------------------------------------------- dice (week 1)
def roll_dice(n_rolls: int, n_dice: int = 3, seed: int | None = DEFAULT_SEED) -> np.ndarray:
    """``n_rolls`` independent rolls of ``n_dice`` fair dice, shape (n_rolls, n_dice)."""
    return rng(seed).integers(1, 7, size=(n_rolls, n_dice))


def dice_sums(n_rolls: int, n_dice: int = 3, seed: int | None = DEFAULT_SEED) -> np.ndarray:
    """The sum of each roll - the statistic Week 1 is built around."""
    return roll_dice(n_rolls, n_dice, seed).sum(axis=1)


# ------------------------------------------------------- discrete samplers
def bernoulli(p: float, size, seed: int | None = DEFAULT_SEED) -> np.ndarray:
    """Boolean array, True with probability ``p``."""
    return rng(seed).random(size) < p


def binomial(n: int, p: float, size, seed: int | None = DEFAULT_SEED) -> np.ndarray:
    """Number of successes in ``n`` independent trials each succeeding with ``p``."""
    return rng(seed).binomial(n, p, size)


def geometric(p: float, size, seed: int | None = DEFAULT_SEED) -> np.ndarray:
    """Trials up to and including the first success: support {1, 2, 3, ...}."""
    return rng(seed).geometric(p, size)


def poisson(lam: float, size, seed: int | None = DEFAULT_SEED) -> np.ndarray:
    """Counts with rate ``lam``."""
    return rng(seed).poisson(lam, size)


# ---------------------------------------------------- continuous samplers
def uniform(a: float = 0.0, b: float = 1.0, size=1, seed: int | None = DEFAULT_SEED) -> np.ndarray:
    """Uniform draws on the interval (a, b)."""
    return rng(seed).uniform(a, b, size)


def normal(mu: float = 0.0, sigma: float = 1.0, size=1, seed: int | None = DEFAULT_SEED) -> np.ndarray:
    """Normal draws with mean ``mu`` and standard deviation ``sigma``."""
    return rng(seed).normal(mu, sigma, size)


def exponential(scale: float = 1.0, size=1, seed: int | None = DEFAULT_SEED) -> np.ndarray:
    """Exp(scale) draws; ``scale`` is the mean waiting time, i.e. 1 / rate."""
    return rng(seed).exponential(scale, size)


# ------------------------------------------------------ Poisson process (week 4)
def exponential_waiting_times(scale: float, size: int, seed: int | None = DEFAULT_SEED) -> np.ndarray:
    """Independent waiting times with mean ``scale`` (same as :func:`exponential`)."""
    return exponential(scale, size, seed)


def poisson_process_arrivals(rate: float, horizon: float, seed: int | None = DEFAULT_SEED) -> np.ndarray:
    """Arrival times of a rate-``rate`` Poisson process on ``[0, horizon]``.

    The number of arrivals is N ~ Poisson(rate * horizon) and, conditioned on N,
    the arrivals are the order statistics of N uniform points on the interval.
    That is the definition of the process, so this is exact rather than an
    approximation built from a fixed number of exponential gaps.
    """
    gen = rng(seed)
    n_arrivals = int(gen.poisson(rate * horizon))
    return np.sort(gen.uniform(0.0, horizon, n_arrivals))


def counts_per_interval(arrivals: np.ndarray, bin_width: float, horizon: float | None = None) -> np.ndarray:
    """Histogram arrivals into equal bins of width ``bin_width`` -> counts per bin."""
    arrivals = np.asarray(arrivals, dtype=float)
    if horizon is None:
        horizon = float(arrivals.max()) if arrivals.size else 0.0
    edges = np.arange(0.0, horizon + bin_width, bin_width)
    counts, _ = np.histogram(arrivals, bins=edges)
    return counts


# ------------------------------------------------------------ error yardsticks
def relative_frequency(hits, trials: int) -> float:
    """hits / trials, accepting either a boolean array or a count."""
    if np.isscalar(hits):
        return float(hits) / float(trials)
    return float(np.count_nonzero(hits)) / float(trials)


def standard_error(p: float, trials: int) -> float:
    """sqrt(p(1-p)/n): the binomial standard error of a frequency estimate."""
    return float(np.sqrt(max(p * (1.0 - p), 0.0) / trials))


def sample_mean_error(sample: np.ndarray) -> float:
    """s / sqrt(n): the standard error of a sample mean."""
    x = np.asarray(sample, dtype=float)
    return float(np.std(x, ddof=1) / np.sqrt(x.size))


def compare_estimate(
    estimate: float,
    exact: float,
    std_error: float,
    sigma: float = SIGMA,
) -> dict:
    """Line the simulation up against the theory.

    Returns the estimate, the exact value, the absolute error, the z-score of
    that error and whether it stays inside ``sigma`` standard errors.
    """
    error = float(estimate) - float(exact)
    tolerance = sigma * std_error
    return {
        "estimate": float(estimate),
        "exact": float(exact),
        "abs_error": abs(error),
        "std_error": float(std_error),
        "z_score": error / std_error if std_error > 0 else 0.0,
        "tolerance": tolerance,
        "agrees": abs(error) <= tolerance,
    }


def compare_frequency(
    estimate: float,
    exact: float,
    trials: int,
    sigma: float = SIGMA,
) -> dict:
    """Same as :func:`compare_estimate`, with the standard error of a frequency."""
    return compare_estimate(estimate, exact, standard_error(exact, trials), sigma)


# ------------------------------------------------------------------ sample stats
def sample_moments(sample: np.ndarray) -> dict:
    """Empirical moments of a sample; ``variance`` is the unbiased (ddof=1) one."""
    x = np.asarray(sample, dtype=float)
    return {
        "n": int(x.size),
        "mean": float(np.mean(x)),
        "second_moment": float(np.mean(x**2)),
        "variance": float(np.var(x, ddof=1)),
        "variance_biased": float(np.var(x)),
        "std": float(np.std(x, ddof=1)),
    }


def running_mean(sample: np.ndarray) -> np.ndarray:
    """Cumulative average, for the "does the estimate settle down?" plot."""
    x = np.asarray(sample, dtype=float)
    return np.cumsum(x) / np.arange(1, x.size + 1)
