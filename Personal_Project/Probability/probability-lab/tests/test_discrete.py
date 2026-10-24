"""Tests for :mod:`src.discrete` - the hand-written Binomial, Geometric and Poisson."""

from __future__ import annotations

import math

import pytest
import scipy.stats as stats

from src import discrete

SEED = 1234
BINOMIAL_CASES = [(5, 0.1), (10, 0.3), (10, 0.5), (50, 0.9), (200, 0.4)]
POISSON_LAMBDAS = (0.5, 1.0, 3.0, 10.0, 25.0)
GEOMETRIC_PS = (0.05, 0.2, 0.5, 0.9)


def test_log_factorial_and_log_comb_building_blocks() -> None:
    assert discrete.log_factorial(0) == pytest.approx(0.0)
    assert discrete.log_factorial(5) == pytest.approx(math.log(120.0))
    assert discrete.log_comb(5, 2) == pytest.approx(math.log(10.0))
    assert discrete.log_comb(5, 0) == pytest.approx(0.0)
    assert discrete.log_comb(5, -1) == -math.inf
    assert discrete.log_comb(5, 6) == -math.inf


@pytest.mark.parametrize("n,p", BINOMIAL_CASES)
def test_binomial_pmf_matches_scipy(n: int, p: float) -> None:
    """The lecture formula equals stats.binom.pmf over the whole support k = 0..n."""
    for k in range(n + 1):
        assert discrete.binomial_pmf(k, n, p) == pytest.approx(stats.binom.pmf(k, n, p), rel=1e-9)


@pytest.mark.parametrize("n,p", BINOMIAL_CASES)
def test_binomial_cdf_matches_scipy(n: int, p: float) -> None:
    """The explicit sum of the PMFs equals stats.binom.cdf."""
    for k in range(n + 1):
        assert discrete.binomial_cdf(k, n, p) == pytest.approx(stats.binom.cdf(k, n, p), rel=1e-9)
    assert discrete.binomial_cdf(-1, n, p) == 0.0
    assert discrete.binomial_cdf(n + 5, n, p) == 1.0


def test_binomial_tail_is_zero_outside_the_support() -> None:
    assert discrete.binomial_pmf(-1, 10, 0.3) == 0.0
    assert discrete.binomial_pmf(11, 10, 0.3) == 0.0
    assert discrete.binomial_pmf(0, 10, 1.0) == 0.0
    assert discrete.binomial_pmf(10, 10, 1.0) == 1.0
    assert discrete.binomial_pmf(0, 10, 0.0) == 1.0


def test_binomial_pmf_with_n_1000_does_not_overflow() -> None:
    """log-space binomial coefficient: n = 1000, p = 0.001 behaves like n = 10."""
    n, p = 1000, 0.001
    for k in (0, 1, 2, 5, 999, 1000):
        assert discrete.binomial_pmf(k, n, p) == pytest.approx(stats.binom.pmf(k, n, p), rel=1e-9)
    for k in (0, 1, 2, 5, 100, 999):
        assert discrete.binomial_cdf(k, n, p) == pytest.approx(stats.binom.cdf(k, n, p), rel=1e-9)


def test_binomial_pmf_sums_to_one() -> None:
    for n, p in BINOMIAL_CASES:
        assert math.fsum(discrete.binomial_pmf(k, n, p) for k in range(n + 1)) == pytest.approx(1.0, rel=1e-12)


def test_binomial_mean_and_variance_formulas() -> None:
    assert discrete.binomial_mean(10, 0.3) == pytest.approx(3.0)
    assert discrete.binomial_variance(10, 0.3) == pytest.approx(2.1)
    n, p = 50, 0.9
    assert discrete.binomial_mean(n, p) == pytest.approx(float(stats.binom.mean(n, p)))
    assert discrete.binomial_variance(n, p) == pytest.approx(float(stats.binom.var(n, p)))


@pytest.mark.parametrize("p", GEOMETRIC_PS)
def test_geometric_pmf_and_cdf_match_scipy(p: float) -> None:
    """scipy's geom is supported on k >= 1, the same convention as this project."""
    for k in range(1, 61):
        assert discrete.geometric_pmf(k, p) == pytest.approx(stats.geom.pmf(k, p), rel=1e-9)
        assert discrete.geometric_cdf(k, p) == pytest.approx(stats.geom.cdf(k, p), rel=1e-9)
    assert discrete.geometric_pmf(0, p) == 0.0
    assert discrete.geometric_cdf(0, p) == 0.0


@pytest.mark.parametrize("p", GEOMETRIC_PS)
def test_geometric_tail_and_moments(p: float) -> None:
    # k_cover is where (1 - p)^k drops below 1e-9, so the sum of the PMF is exhausted.
    k_cover = math.ceil(math.log(1e-9) / math.log1p(-p)) + 1
    assert discrete.geometric_tail(0, p) == pytest.approx(1.0)
    assert discrete.geometric_tail(k_cover, p) < 1e-9
    assert discrete.geometric_tail(200, 0.2) < 1e-9  # the tail the PMF-sum check relies on
    assert discrete.geometric_pmf(1, p) + discrete.geometric_tail(1, p) == pytest.approx(1.0)
    assert discrete.geometric_cdf(k_cover, p) == pytest.approx(1.0, abs=1e-9)
    assert math.fsum(discrete.geometric_pmf(k, p) for k in range(1, k_cover + 1)) == pytest.approx(1.0, abs=1e-9)
    assert discrete.geometric_mean(p) == pytest.approx(1.0 / p)
    assert discrete.geometric_variance(p) == pytest.approx((1.0 - p) / p**2)
    assert discrete.geometric_mean(p) == pytest.approx(float(stats.geom.mean(p)))
    assert discrete.geometric_variance(p) == pytest.approx(float(stats.geom.var(p)))


@pytest.mark.parametrize("lam", POISSON_LAMBDAS)
def test_poisson_pmf_and_cdf_match_scipy(lam: float) -> None:
    """The log-space formula equals stats.poisson.pmf/cdf over k = 0..80."""
    for k in range(81):
        assert discrete.poisson_pmf(k, lam) == pytest.approx(stats.poisson.pmf(k, lam), rel=1e-9)
        assert discrete.poisson_cdf(k, lam) == pytest.approx(stats.poisson.cdf(k, lam), rel=1e-9)
    assert discrete.poisson_pmf(-1, lam) == 0.0
    assert discrete.poisson_cdf(-1, lam) == 0.0


@pytest.mark.parametrize("lam", POISSON_LAMBDAS)
def test_poisson_pmf_sums_to_one_and_mean_equals_variance(lam: float) -> None:
    k_max = discrete.poisson_k_max(lam)
    # k_max covers all but ~1e-6 of the mass, so the truncated sum is a 1e-5 match.
    assert math.fsum(discrete.poisson_pmf(k, lam) for k in range(k_max + 1)) == pytest.approx(1.0, abs=1e-5)
    assert discrete.poisson_cdf(k_max, lam) > 1.0 - 1e-5
    assert discrete.poisson_mean(lam) == lam
    assert discrete.poisson_variance(lam) == lam


def test_poisson_approximation_improves_as_n_grows() -> None:
    """Binomial(n, lam / n) -> Poisson(lam): the total-variation gap shrinks."""
    rows = discrete.poisson_approximation(lam=1.0)
    assert [row["n"] for row in rows] == [5, 10, 20, 50, 100, 500, 1000]
    variations = [row["total_variation"] for row in rows]
    assert variations == sorted(variations, reverse=True)
    assert all(a > b for a, b in zip(variations, variations[1:]))
    assert variations[-1] < 0.01
    assert variations[-1] < variations[0] / 10
    for row in rows:
        assert row["p"] == pytest.approx(1.0 / row["n"])
        assert math.fsum(row["binomial"]) == pytest.approx(1.0, abs=1e-5)
        assert math.fsum(row["poisson"]) == pytest.approx(1.0, abs=1e-5)


def test_defect_example_binomial_and_poisson_columns_agree() -> None:
    """1 000 components at p = 0.001 against Poisson(1): the approximation error is small."""
    rows = discrete.defect_example()
    assert [row["k"] for row in rows] == [0, 1, 2, 3, 4, 5]
    for row in rows:
        assert row["binomial_pmf"] == pytest.approx(discrete.binomial_pmf(row["k"], 1000, 0.001), rel=1e-12)
        assert row["poisson_pmf"] == pytest.approx(discrete.poisson_pmf(row["k"], 1.0), rel=1e-12)
        assert row["pmf_difference"] == pytest.approx(abs(row["binomial_pmf"] - row["poisson_pmf"]), rel=1e-12)
        assert abs(row["binomial_pmf"] - row["poisson_pmf"]) < 1e-3
        assert abs(row["binomial_cdf"] - row["poisson_cdf"]) < 1e-3


def test_geometric_is_memoryless_and_the_die_is_not() -> None:
    """P(X > s + t | X > s) = P(X > t) for the Geometric, but not for a fair die."""
    check = discrete.memoryless_check(0.3)
    assert check["memoryless"] is True
    assert check["difference"] < 1e-12
    assert check["conditional"] == pytest.approx(check["unconditional"])

    counterexample = discrete.memoryless_counterexample()
    assert counterexample["memoryless"] is False
    # P(D > 4 | D > 2) = 2/4 = 1/2, while P(D > 2) = 4/6: ageing changes the odds.
    assert counterexample["conditional"] == pytest.approx(0.5)
    assert counterexample["unconditional"] == pytest.approx(4 / 6)
    assert counterexample["difference"] > 0.1
    assert discrete.conditional_tail(discrete.die_tail, 4, 0) == pytest.approx(1.0)


def test_simulate_binomial_check_agrees_with_the_theory() -> None:
    result = discrete.simulate_binomial_check(n=10, p=0.3, trials=50_000, seed=SEED)
    assert result["mean_comparison"]["agrees"] is True
    assert result["theoretical_mean"] == pytest.approx(3.0)
    assert result["theoretical_variance"] == pytest.approx(2.1)
    assert result["sample_mean"] == pytest.approx(3.0, abs=0.05)
    assert result["sample_variance"] == pytest.approx(2.1, abs=0.1)
    assert {row["k"] for row in result["rows"]}.issubset(set(range(11)))


def test_simulate_geometric_check_agrees_with_the_theory() -> None:
    result = discrete.simulate_geometric_check(p=0.2, trials=50_000, seed=SEED)
    assert result["mean_comparison"]["agrees"] is True
    assert result["theoretical_mean"] == pytest.approx(5.0)
    assert result["theoretical_variance"] == pytest.approx(20.0)
    assert result["sample_mean"] == pytest.approx(5.0, abs=0.2)


def test_simulate_poisson_check_agrees_with_the_theory() -> None:
    result = discrete.simulate_poisson_check(lam=3.0, trials=50_000, seed=SEED)
    assert result["mean_comparison"]["agrees"] is True
    assert result["theoretical_mean"] == pytest.approx(3.0)
    assert result["theoretical_variance"] == pytest.approx(3.0)
    assert result["sample_mean"] == pytest.approx(3.0, abs=0.1)
    assert result["sample_variance"] == pytest.approx(3.0, abs=0.2)
