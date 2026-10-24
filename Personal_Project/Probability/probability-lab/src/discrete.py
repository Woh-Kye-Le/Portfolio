"""Week 3 - Binomial, Geometric and Poisson, written from scratch.

Nothing here calls ``scipy``: the PMFs are the lecture formulas, the CDFs are
explicit sums of the PMFs, and the binomial coefficient is computed in log space
(``lgamma``) so that ``n = 1000`` behaves exactly like ``n = 10``.

Checks in this module:

* the simulated frequency of each value against the PMF,
* the memoryless property of the Geometric (and the die as a counter-example),
* the Poisson approximation to the Binomial as ``n`` grows, i.e. the
  1 000-component defect example with ``p = 0.001`` and ``lam = 1``.
"""

from __future__ import annotations

import math
from typing import Callable, Iterable, Sequence

import numpy as np

from src.simulate import (
    binomial as sample_binomial,
    compare_estimate,
    geometric as sample_geometric,
    poisson as sample_poisson,
    sample_moments,
)

#: Terms smaller than this are dropped from the printed PMF tables.
PMF_FLOOR = 1e-6


# --------------------------------------------------------------- building blocks
def log_factorial(n: float) -> float:
    """ln(n!) through ``lgamma``, so 1000! needs no special handling."""
    return math.lgamma(n + 1.0)


def log_comb(n: int, k: int) -> float:
    """ln C(n, k), the log-space binomial coefficient."""
    if k < 0 or k > n:
        return -math.inf
    return log_factorial(n) - log_factorial(k) - log_factorial(n - k)


# -------------------------------------------------------------------- Binomial
def binomial_pmf(k: int, n: int, p: float) -> float:
    """P(X = k) = C(n, k) p^k (1 - p)^(n - k) for k = 0, ..., n."""
    if k < 0 or k > n:
        return 0.0
    if p <= 0.0:
        return 1.0 if k == 0 else 0.0
    if p >= 1.0:
        return 1.0 if k == n else 0.0
    return math.exp(log_comb(n, k) + k * math.log(p) + (n - k) * math.log1p(-p))


def binomial_cdf(k: int, n: int, p: float) -> float:
    """P(X <= k), summed term by term."""
    if k < 0:
        return 0.0
    if k >= n:
        return 1.0
    return math.fsum(binomial_pmf(i, n, p) for i in range(int(k) + 1))


def binomial_mean(n: int, p: float) -> float:
    """E[X] = np - proved with indicators in :mod:`src.moments`."""
    return n * p


def binomial_variance(n: int, p: float) -> float:
    """Var(X) = np(1 - p)."""
    return n * p * (1.0 - p)


# ------------------------------------------------------------------ Geometric
def geometric_pmf(k: int, p: float) -> float:
    """P(X = k) = (1 - p)^(k - 1) p, where X counts trials up to the first success."""
    if k < 1:
        return 0.0
    return (1.0 - p) ** (k - 1) * p


def geometric_cdf(k: int, p: float) -> float:
    """P(X <= k) = 1 - (1 - p)^k - the sum telescopes."""
    if k < 1:
        return 0.0
    return 1.0 - (1.0 - p) ** k


def geometric_tail(k: int, p: float) -> float:
    """P(X > k) = (1 - p)^k, the survival function the memoryless test uses."""
    if k < 0:
        return 1.0
    return (1.0 - p) ** k


def geometric_mean(p: float) -> float:
    """E[X] = 1 / p."""
    return 1.0 / p


def geometric_variance(p: float) -> float:
    """Var(X) = (1 - p) / p^2."""
    return (1.0 - p) / p**2


# --------------------------------------------------------------------- Poisson
def poisson_pmf(k: int, lam: float) -> float:
    """P(X = k) = lam^k e^-lam / k!, evaluated in log space."""
    if k < 0:
        return 0.0
    if lam <= 0.0:
        return 1.0 if k == 0 else 0.0
    return math.exp(-lam + k * math.log(lam) - log_factorial(k))


def poisson_cdf(k: int, lam: float) -> float:
    """P(X <= k) as an explicit sum of the PMF."""
    if k < 0:
        return 0.0
    return math.fsum(poisson_pmf(i, lam) for i in range(int(k) + 1))


def poisson_mean(lam: float) -> float:
    """E[X] = lam."""
    return lam


def poisson_variance(lam: float) -> float:
    """Var(X) = lam, the property that names the distribution."""
    return lam


# ---------------------------------------------------------------- pmf/cdf tables
def _table(pmf: Callable[[int], float], cdf: Callable[[int], float], ks: Iterable[int]) -> list[dict]:
    """Rows of ``k``, PMF and CDF, with negligible tails left out."""
    return [
        {"k": int(k), "pmf": pmf(int(k)), "cdf": cdf(int(k))}
        for k in ks
        if pmf(int(k)) >= PMF_FLOOR
    ]


def binomial_table(n: int, p: float) -> list[dict]:
    """PMF/CDF columns for every k = 0, ..., n."""
    return _table(lambda k: binomial_pmf(k, n, p), lambda k: binomial_cdf(k, n, p), range(n + 1))


def geometric_table(p: float, k_max: int = 20) -> list[dict]:
    """PMF/CDF columns for k = 1, ..., k_max."""
    return _table(lambda k: geometric_pmf(k, p), lambda k: geometric_cdf(k, p), range(1, k_max + 1))


def poisson_table(lam: float, k_max: int = 20) -> list[dict]:
    """PMF/CDF columns for k = 0, ..., k_max."""
    return _table(lambda k: poisson_pmf(k, lam), lambda k: poisson_cdf(k, lam), range(k_max + 1))


def poisson_k_max(lam: float, sigmas: float = 6.0) -> int:
    """A k range that covers the mass of Poisson(lam): lam + 6 sqrt(lam)."""
    return int(math.ceil(lam + sigmas * math.sqrt(lam))) + 1


# --------------------------------------------------------------- memorylessness
def conditional_tail(tail: Callable[[int], float], s: int, t: int) -> float:
    """P(X > s + t | X > s) for any survival function ``tail``."""
    return tail(s + t) / tail(s)


def memoryless_check(p: float, s: int = 3, t: int = 2) -> dict:
    """The Geometric satisfies P(X > s + t | X > s) = P(X > t) exactly."""
    conditional = conditional_tail(lambda k: geometric_tail(k, p), s, t)
    unconditional = geometric_tail(t, p)
    return {
        "distribution": "Geometric",
        "p": p,
        "s": s,
        "t": t,
        "conditional": conditional,
        "unconditional": unconditional,
        "difference": abs(conditional - unconditional),
        "memoryless": abs(conditional - unconditional) < 1e-12,
    }


def die_tail(k: int) -> float:
    """P(D > k) for one fair die: the discrete uniform is *not* memoryless."""
    return max(6 - k, 0) / 6.0


def memoryless_counterexample(s: int = 2, t: int = 2) -> dict:
    """The same test with a die, where "how long it has lasted" changes the odds."""
    conditional = conditional_tail(die_tail, s, t)
    unconditional = die_tail(t)
    return {
        "distribution": "Discrete uniform (die)",
        "s": s,
        "t": t,
        "conditional": conditional,
        "unconditional": unconditional,
        "difference": abs(conditional - unconditional),
        "memoryless": abs(conditional - unconditional) < 1e-12,
    }


# ------------------------------------------------ Poisson approximation to Binomial
def poisson_approximation(
    n_values: Sequence[int] = (5, 10, 20, 50, 100, 500, 1000),
    lam: float = 1.0,
    k_max: int | None = None,
) -> list[dict]:
    """Binomial(``n``, ``lam / n``) against Poisson(``lam``) as ``n`` grows.

    Each row carries both PMFs (plain lists, ready to plot), the pointwise maximum
    error and the total-variation distance ``0.5 * sum |b(k) - pois(k)|`` - the
    number that keeps shrinking towards zero.
    """
    if k_max is None:
        k_max = poisson_k_max(lam)

    rows = []
    for n in n_values:
        p = lam / n
        ks = list(range(k_max + 1))
        binomial_terms = [binomial_pmf(k, n, p) for k in ks]
        poisson_terms = [poisson_pmf(k, lam) for k in ks]
        differences = [abs(a - b) for a, b in zip(binomial_terms, poisson_terms)]
        rows.append(
            {
                "n": int(n),
                "p": p,
                "k": ks,
                "binomial": binomial_terms,
                "poisson": poisson_terms,
                "max_abs_error": max(differences),
                "total_variation": 0.5 * math.fsum(differences),
            }
        )
    return rows


def defect_example(
    n: int = 1000,
    p: float = 0.001,
    k_values: Sequence[int] = (0, 1, 2, 3, 4, 5),
) -> list[dict]:
    """1000 components, each defective with probability 0.001, i.e. lam = 1.

    Side by side: the exact binomial numbers and the Poisson stand-in.
    """
    lam = n * p
    return [
        {
            "k": int(k),
            "binomial_pmf": binomial_pmf(k, n, p),
            "poisson_pmf": poisson_pmf(k, lam),
            "pmf_difference": abs(binomial_pmf(k, n, p) - poisson_pmf(k, lam)),
            "binomial_cdf": binomial_cdf(k, n, p),
            "poisson_cdf": poisson_cdf(k, lam),
        }
        for k in k_values
    ]


# ------------------------------------------------------------------- simulations
def _frequency_rows(samples: np.ndarray, pmf: Callable[[int], float], k_max: int) -> list[dict]:
    """Exact PMF next to the simulated relative frequency for each k."""
    trials = samples.size
    rows = []
    for k in range(k_max + 1):
        exact = pmf(k)
        if exact < PMF_FLOOR and np.count_nonzero(samples == k) == 0:
            continue
        rows.append(
            {
                "k": k,
                "exact": exact,
                "simulated": float(np.count_nonzero(samples == k)) / trials,
            }
        )
    return rows


def simulate_binomial_check(
    n: int = 10,
    p: float = 0.3,
    trials: int = 50_000,
    seed: int | None = None,
) -> dict:
    """PMF table plus mean/variance for a simulated Binomial sample."""
    samples = sample_binomial(n, p, trials, seed)
    moments = sample_moments(samples)
    theoretical_mean = binomial_mean(n, p)
    theoretical_variance = binomial_variance(n, p)
    return {
        "rows": _frequency_rows(samples, lambda k: binomial_pmf(k, n, p), n),
        "trials": trials,
        "sample_mean": moments["mean"],
        "theoretical_mean": theoretical_mean,
        "sample_variance": moments["variance"],
        "theoretical_variance": theoretical_variance,
        "mean_comparison": compare_estimate(
            moments["mean"], theoretical_mean, math.sqrt(theoretical_variance / trials)
        ),
    }


def simulate_geometric_check(
    p: float = 0.2,
    trials: int = 50_000,
    seed: int | None = None,
) -> dict:
    """PMF table plus mean/variance for a simulated Geometric sample."""
    samples = sample_geometric(p, trials, seed)
    moments = sample_moments(samples)
    theoretical_mean = geometric_mean(p)
    theoretical_variance = geometric_variance(p)
    return {
        "rows": _frequency_rows(samples, lambda k: geometric_pmf(k, p), int(np.max(samples))),
        "trials": trials,
        "sample_mean": moments["mean"],
        "theoretical_mean": theoretical_mean,
        "sample_variance": moments["variance"],
        "theoretical_variance": theoretical_variance,
        "mean_comparison": compare_estimate(
            moments["mean"], theoretical_mean, math.sqrt(theoretical_variance / trials)
        ),
    }


def simulate_poisson_check(
    lam: float = 3.0,
    trials: int = 50_000,
    seed: int | None = None,
) -> dict:
    """PMF table plus mean/variance for a simulated Poisson sample."""
    samples = sample_poisson(lam, trials, seed)
    moments = sample_moments(samples)
    return {
        "rows": _frequency_rows(samples, lambda k: poisson_pmf(k, lam), poisson_k_max(lam)),
        "trials": trials,
        "sample_mean": moments["mean"],
        "theoretical_mean": poisson_mean(lam),
        "sample_variance": moments["variance"],
        "theoretical_variance": poisson_variance(lam),
        "mean_comparison": compare_estimate(
            moments["mean"], poisson_mean(lam), math.sqrt(poisson_variance(lam) / trials)
        ),
    }
