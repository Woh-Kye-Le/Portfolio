"""Week 4 - Continuous distributions, numeric integration and the Poisson process.

Four threads, all checked numerically:

* densities written by hand (Uniform, Normal, Exponential) and integrated with
  the trapezium and Simpson rules to show the total area is 1,
* CDFs built two ways: the analytic formula and ``integral of the PDF up to x``,
* Normal percentiles found by standardising, with the inverse CDF implemented
  from Acklam's rational approximation plus one Newton step,
* Exponential waiting times whose per-interval counts turn out Poisson - the
  link that makes the Poisson distribution the "counting" side of the
  Poisson process.
"""

from __future__ import annotations

import math
from typing import Callable, Iterable, Sequence

import numpy as np

from src.discrete import poisson_pmf
from src.simulate import (
    compare_estimate,
    counts_per_interval,
    exponential as sample_exponential,
    poisson_process_arrivals,
    sample_moments,
)

SQRT_2PI = math.sqrt(2.0 * math.pi)


# ------------------------------------------------------------------- densities
def uniform_pdf(x: float, a: float = 0.0, b: float = 1.0) -> float:
    """1 / (b - a) on [a, b], zero elsewhere."""
    return 1.0 / (b - a) if a <= x <= b else 0.0


def uniform_cdf(x: float, a: float = 0.0, b: float = 1.0) -> float:
    """0 below a, (x - a) / (b - a) inside, 1 above b."""
    if x < a:
        return 0.0
    if x > b:
        return 1.0
    return (x - a) / (b - a)


def normal_pdf(x: float, mu: float = 0.0, sigma: float = 1.0) -> float:
    """The bell curve: exp(-z^2 / 2) / (sigma sqrt(2 pi)), with z = (x - mu) / sigma."""
    z = (x - mu) / sigma
    return math.exp(-0.5 * z * z) / (sigma * SQRT_2PI)


def normal_cdf(x: float, mu: float = 0.0, sigma: float = 1.0) -> float:
    """Phi(z) via the error function, the one special function we allow ourselves."""
    z = (x - mu) / sigma
    return 0.5 * (1.0 + math.erf(z / math.sqrt(2.0)))


def exponential_pdf(x: float, scale: float = 1.0) -> float:
    """exp(-x / scale) / scale on x >= 0; ``scale`` is the mean, i.e. 1 / rate."""
    if x < 0:
        return 0.0
    return math.exp(-x / scale) / scale


def exponential_cdf(x: float, scale: float = 1.0) -> float:
    """1 - exp(-x / scale) on x >= 0."""
    if x < 0:
        return 0.0
    return 1.0 - math.exp(-x / scale)


def exponential_ppf(p: float, scale: float = 1.0) -> float:
    """Inverse CDF: -scale ln(1 - p), the transformation used to sample it."""
    return -scale * math.log1p(-p)


def uniform_mean(a: float = 0.0, b: float = 1.0) -> float:
    """(a + b) / 2."""
    return 0.5 * (a + b)


def uniform_variance(a: float = 0.0, b: float = 1.0) -> float:
    """(b - a)^2 / 12."""
    return (b - a) ** 2 / 12.0


def exponential_mean(scale: float = 1.0) -> float:
    """``scale`` itself."""
    return scale


def exponential_variance(scale: float = 1.0) -> float:
    """scale^2, so the standard deviation equals the mean."""
    return scale**2


# ------------------------------------------------------- standardising and percentiles
def standardize(x: float, mu: float = 0.0, sigma: float = 1.0) -> float:
    """z = (x - mu) / sigma - the transformation behind every Normal table lookup."""
    return (x - mu) / sigma


_A = (
    -3.969683028665376e01,
    2.209460984245205e02,
    -2.759285104469687e02,
    1.383577518672690e02,
    -3.066479806614716e01,
    2.506628277459239e00,
)
_B = (
    -5.447609879822406e01,
    1.615858368580409e02,
    -1.556989798598866e02,
    6.680131188771972e01,
    -1.328068155288572e01,
)
_C = (
    -7.784894002430293e-03,
    -3.223964580411365e-01,
    -2.400758277161838e00,
    -2.549732539343734e00,
    4.374664141464968e00,
    2.938163982698783e00,
)
_D = (
    7.784695709041462e-03,
    3.224671290700398e-01,
    2.445134137142996e00,
    3.754408661907416e00,
)
_P_LOW = 0.02425


def standard_normal_ppf(p: float) -> float:
    """Phi^-1(p): Acklam's approximation, sharpened by one Newton step.

    Starting from the rational approximation, ``x - (Phi(x) - p) / phi(x)`` is
    applied once, which is enough to reach machine precision.
    """
    if not 0.0 < p < 1.0:
        raise ValueError("p must be strictly between 0 and 1")

    if p < _P_LOW:
        q = math.sqrt(-2.0 * math.log(p))
        x = ((((_C[0] * q + _C[1]) * q + _C[2]) * q + _C[3]) * q + _C[4]) * q + _C[5]
        x /= (((_D[0] * q + _D[1]) * q + _D[2]) * q + _D[3]) * q + 1.0
    elif p <= 1.0 - _P_LOW:
        q = p - 0.5
        r = q * q
        x = (((((_A[0] * r + _A[1]) * r + _A[2]) * r + _A[3]) * r + _A[4]) * r + _A[5]) * q
        x /= (((((_B[0] * r + _B[1]) * r + _B[2]) * r + _B[3]) * r + _B[4]) * r + 1.0)
    else:
        q = math.sqrt(-2.0 * math.log1p(-p))
        x = -(((((_C[0] * q + _C[1]) * q + _C[2]) * q + _C[3]) * q + _C[4]) * q + _C[5])
        x /= (((_D[0] * q + _D[1]) * q + _D[2]) * q + _D[3]) * q + 1.0

    error = normal_cdf(x) - p
    step = error * SQRT_2PI * math.exp(0.5 * x * x)
    return x - step / (1.0 + 0.5 * x * step)


def normal_ppf(p: float, mu: float = 0.0, sigma: float = 1.0) -> float:
    """x such that P(X <= x) = p for X ~ Normal(mu, sigma^2)."""
    return mu + sigma * standard_normal_ppf(p)


def normal_percentiles(
    mu: float = 0.0,
    sigma: float = 1.0,
    ps: Sequence[float] = (0.5, 0.9, 0.95, 0.975, 0.99),
) -> list[dict]:
    """Standardise, look the z-value up, then un-standardise.

    ``standardised`` re-applies ``(x - mu) / sigma`` to the answer, so the table
    shows the round trip closing.
    """
    rows = []
    for p in ps:
        z = standard_normal_ppf(p)
        x = mu + sigma * z
        rows.append(
            {
                "p": p,
                "z": z,
                "x": x,
                "standardised": standardize(x, mu, sigma),
                "cdf_check": normal_cdf(x, mu, sigma),
            }
        )
    return rows


# ------------------------------------------------------------- numeric integration
def grid_values(func: Callable[[float], float], xs: Iterable[float]) -> np.ndarray:
    """Evaluate a scalar density/CDF over an array of points (for plotting)."""
    return np.array([func(float(x)) for x in np.asarray(xs, dtype=float)], dtype=float)


def trapezoid(func: Callable[[float], float], a: float, b: float, n: int = 20_000) -> float:
    """Composite trapezium rule on ``n`` equal sub-intervals."""
    xs = np.linspace(a, b, n + 1)
    ys = grid_values(func, xs)
    h = (b - a) / n
    return float(h * (0.5 * ys[0] + np.sum(ys[1:-1]) + 0.5 * ys[-1]))


def simpson(func: Callable[[float], float], a: float, b: float, n: int = 2_000) -> float:
    """Composite Simpson rule; ``n`` is rounded up to an even number if needed."""
    if n % 2:
        n += 1
    xs = np.linspace(a, b, n + 1)
    ys = grid_values(func, xs)
    h = (b - a) / n
    return float(h / 3.0 * (ys[0] + ys[-1] + 4.0 * np.sum(ys[1:-1:2]) + 2.0 * np.sum(ys[2:-1:2])))


def integrate_to_one(
    pdf: Callable[[float], float],
    a: float,
    b: float,
    n: int = 20_000,
) -> dict:
    """Integrate a density over its support with both rules and report the error."""
    trapezoid_area = trapezoid(pdf, a, b, n)
    simpson_area = simpson(pdf, a, b, n)
    return {
        "trapezoid": trapezoid_area,
        "simpson": simpson_area,
        "trapezoid_error": abs(trapezoid_area - 1.0),
        "simpson_error": abs(simpson_area - 1.0),
    }


def integrates_to_one_table(n: int = 20_000) -> list[dict]:
    """One row per density; the support is truncated where the tail is negligible."""
    densities = [
        ("Uniform(0, 1)", uniform_pdf, 0.0, 1.0),
        ("Normal(0, 1), truncated at +-8 sigma", normal_pdf, -8.0, 8.0),
        ("Exponential(mean 1), truncated at 50", exponential_pdf, 0.0, 50.0),
    ]
    rows = []
    for name, pdf, a, b in densities:
        area = integrate_to_one(pdf, a, b, n)
        rows.append(
            {
                "distribution": name,
                "support": f"[{a:g}, {b:g}]",
                "trapezoid": area["trapezoid"],
                "simpson": area["simpson"],
                "trapezoid_error": area["trapezoid_error"],
                "simpson_error": area["simpson_error"],
            }
        )
    return rows


def numeric_cdf(
    pdf: Callable[[float], float],
    a: float,
    x: float,
    n: int = 2_000,
) -> float:
    """F(x) built by integrating the density from the left end of the support."""
    if x <= a:
        return 0.0
    return trapezoid(pdf, a, x, n)


def cdf_comparison(
    pdf: Callable[[float], float],
    analytic_cdf: Callable[[float], float],
    a: float,
    xs: Iterable[float],
    n: int = 2_000,
) -> list[dict]:
    """Analytic CDF against the numerically integrated one, point by point."""
    rows = []
    for x in np.asarray(xs, dtype=float):
        numeric = numeric_cdf(pdf, a, float(x), n)
        analytic = analytic_cdf(float(x))
        rows.append(
            {
                "x": float(x),
                "analytic": analytic,
                "numeric": numeric,
                "difference": abs(analytic - numeric),
            }
        )
    return rows


# --------------------------------------------------- Exponential -> Poisson process
def waiting_time_check(
    rate: float = 1.0,
    n_draws: int = 50_000,
    seed: int | None = None,
) -> dict:
    """Simulated Exponential waiting times against their theoretical mean/variance."""
    scale = 1.0 / rate
    samples = sample_exponential(scale, n_draws, seed)
    moments = sample_moments(samples)
    theoretical_mean = exponential_mean(scale)
    theoretical_variance = exponential_variance(scale)
    return {
        "rate": rate,
        "scale": scale,
        "n_draws": n_draws,
        "sample_mean": moments["mean"],
        "theoretical_mean": theoretical_mean,
        "sample_variance": moments["variance"],
        "theoretical_variance": theoretical_variance,
        "mean_comparison": compare_estimate(
            moments["mean"], theoretical_mean, math.sqrt(theoretical_variance / n_draws)
        ),
        "samples": samples,
    }


def interval_count_table(counts: np.ndarray, lam: float) -> list[dict]:
    """Observed per-interval counts against the Poisson(lam) PMF.

    If arrivals form a rate-``lam / width`` Poisson process, the number of
    arrivals inside one interval of the given width is Poisson(``lam``).
    """
    counts = np.asarray(counts, dtype=float)
    n_intervals = counts.size
    rows = []
    for k in range(int(counts.max()) + 1 if n_intervals else 1):
        pmf = poisson_pmf(k, lam)
        observed = int(np.count_nonzero(counts == k))
        rows.append(
            {
                "count": k,
                "observed": observed / n_intervals if n_intervals else 0.0,
                "poisson": pmf,
                "observed_intervals": observed,
                "expected_intervals": n_intervals * pmf,
            }
        )
    return rows


def simulate_poisson_process(
    rate: float = 1.0,
    horizon: float = 100.0,
    bin_width: float = 1.0,
    seed: int | None = None,
) -> dict:
    """Sample a Poisson process and bin it: counts per interval should be Poisson."""
    arrivals = poisson_process_arrivals(rate, horizon, seed)
    counts = counts_per_interval(arrivals, bin_width, horizon)
    lam = rate * bin_width
    return {
        "rate": rate,
        "horizon": horizon,
        "bin_width": bin_width,
        "lambda_per_interval": lam,
        "n_arrivals": int(arrivals.size),
        "expected_arrivals": rate * horizon,
        "n_intervals": int(counts.size),
        "counts": counts,
        "arrivals": arrivals,
        "mean_counts": float(counts.mean()) if counts.size else float("nan"),
        "counts_comparison": compare_estimate(
            float(counts.mean()) if counts.size else float("nan"),
            lam,
            math.sqrt(lam / counts.size) if counts.size else float("nan"),
        ),
        "table": interval_count_table(counts, lam),
    }
