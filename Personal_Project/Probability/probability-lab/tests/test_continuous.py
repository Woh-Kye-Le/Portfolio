"""Tests for :mod:`src.continuous` - densities, integration, percentiles and the Poisson process."""

from __future__ import annotations

import math

import numpy as np
import pytest
import scipy.stats as stats

from src import continuous, simulate

SEED = 1234
TAIL_P = (0.001, 0.025, 0.25, 0.5, 0.75, 0.975, 0.999)


def test_uniform_pdf_and_cdf_match_scipy() -> None:
    """1 / (b - a) and the straight-line CDF against stats.uniform(loc=a, scale=b - a)."""
    a, b = -1.0, 2.0
    for x in (-2.0, -1.0, -0.5, 0.0, 1.0, 2.0, 3.7):
        assert continuous.uniform_pdf(x, a, b) == pytest.approx(stats.uniform.pdf(x, a, b - a), rel=1e-12)
        assert continuous.uniform_cdf(x, a, b) == pytest.approx(stats.uniform.cdf(x, a, b - a), rel=1e-12)
    assert continuous.uniform_pdf(0.5, a, b) == pytest.approx(1.0 / 3.0)
    assert continuous.uniform_mean(a, b) == pytest.approx(0.5)
    assert continuous.uniform_variance(a, b) == pytest.approx((b - a) ** 2 / 12.0)


def test_normal_pdf_and_cdf_match_scipy() -> None:
    """The bell curve with erf against stats.norm.pdf/cdf, for several (mu, sigma)."""
    for mu, sigma in ((0.0, 1.0), (1.5, 2.0), (-3.0, 0.5)):
        for x in (-4.0, -1.0, 0.0, 1.234, 3.0):
            assert continuous.normal_pdf(x, mu, sigma) == pytest.approx(
                stats.norm.pdf(x, mu, sigma), rel=1e-12
            )
            assert continuous.normal_cdf(x, mu, sigma) == pytest.approx(
                stats.norm.cdf(x, mu, sigma), rel=1e-12, abs=1e-15
            )
    assert continuous.normal_cdf(0.0) == pytest.approx(0.5)


def test_exponential_pdf_cdf_and_ppf_match_scipy() -> None:
    """scale is the mean, so scipy needs stats.expon(scale=scale)."""
    for scale in (0.5, 1.0, 4.0):
        for x in (-0.5, 0.0, 0.2, 1.3, 5.0):
            assert continuous.exponential_pdf(x, scale) == pytest.approx(
                stats.expon.pdf(x, scale=scale), rel=1e-12
            )
            assert continuous.exponential_cdf(x, scale) == pytest.approx(
                stats.expon.cdf(x, scale=scale), rel=1e-12, abs=1e-15
            )
        for p in (0.1, 0.5, 0.9):
            assert continuous.exponential_ppf(p, scale) == pytest.approx(
                stats.expon.ppf(p, scale=scale), rel=1e-12
            )
    assert continuous.exponential_pdf(-1.0) == 0.0
    assert continuous.exponential_cdf(-1.0) == 0.0
    assert continuous.exponential_mean(4.0) == pytest.approx(4.0)
    assert continuous.exponential_variance(4.0) == pytest.approx(16.0)


def test_exponential_ppf_inverts_the_cdf() -> None:
    """The inverse-CDF transform used to sample: F^-1(F(x)) = x.

    Only moderately large x / scale is used: for a very large ratio the CDF rounds to
    1 - tiny and the round trip loses the tail to cancellation in double precision.
    """
    for scale in (0.5, 2.0, 4.0):
        for x in (0.5 * scale, scale, 2.0 * scale):
            assert continuous.exponential_ppf(continuous.exponential_cdf(x, scale), scale) == pytest.approx(x, abs=1e-12)
    assert continuous.exponential_ppf(0.5, 1.0) == pytest.approx(math.log(2.0))


def test_trapezoid_and_simpson_on_known_integrals() -> None:
    """Both rules integrate a polynomial and an exponential on a finite range."""
    assert continuous.simpson(lambda x: x * x, 0.0, 1.0) == pytest.approx(1.0 / 3.0, rel=1e-12)
    assert continuous.trapezoid(lambda x: x * x, 0.0, 1.0) == pytest.approx(1.0 / 3.0, rel=1e-6)
    assert continuous.simpson(lambda x: x**3, 0.0, 2.0) == pytest.approx(4.0, rel=1e-12)
    assert continuous.trapezoid(math.exp, 0.0, 1.0) == pytest.approx(math.e - 1.0, rel=1e-9)
    assert continuous.simpson(lambda x: 2.0 * x + 1.0, 0.0, 3.0) == pytest.approx(12.0)
    grid = continuous.grid_values(lambda x: x * x, [0.0, 1.0, 2.0])
    assert isinstance(grid, np.ndarray) and np.allclose(grid, [0.0, 1.0, 4.0])


def test_every_density_integrates_to_one() -> None:
    for row in continuous.integrates_to_one_table():
        assert row["trapezoid_error"] < 1e-6
        assert row["simpson_error"] < 1e-6
        assert row["trapezoid"] == pytest.approx(1.0, abs=1e-6)
        assert row["simpson"] == pytest.approx(1.0, abs=1e-6)
    area = continuous.integrate_to_one(continuous.normal_pdf, -8.0, 8.0)
    assert area["trapezoid_error"] < 1e-12 and area["simpson_error"] < 1e-12


def test_numeric_cdf_matches_the_analytic_cdf() -> None:
    """Integrating the PDF from the left of the support reproduces the closed form."""
    cases = [
        (continuous.uniform_pdf, continuous.uniform_cdf, 0.0, [0.1, 0.5, 0.9]),
        (continuous.normal_pdf, continuous.normal_cdf, -8.0, [-2.0, 0.0, 1.5, 3.0]),
        (continuous.exponential_pdf, continuous.exponential_cdf, 0.0, [0.5, 1.0, 2.0, 3.0]),
    ]
    for pdf, cdf, a, xs in cases:
        for row in continuous.cdf_comparison(pdf, cdf, a, xs):
            assert row["difference"] < 1e-6
            assert row["numeric"] == pytest.approx(row["analytic"], abs=1e-6)
    assert continuous.numeric_cdf(continuous.normal_pdf, -8.0, -8.0) == 0.0
    assert continuous.numeric_cdf(continuous.normal_pdf, 0.0, -1.0) == 0.0  # left of the support
    assert continuous.numeric_cdf(continuous.normal_pdf, -8.0, -1.0) == pytest.approx(
        continuous.normal_cdf(-1.0), abs=1e-6
    )


@pytest.mark.parametrize("p", TAIL_P)
def test_standard_normal_ppf_matches_scipy(p: float) -> None:
    """Acklam's approximation plus one Newton step reaches machine precision."""
    assert continuous.standard_normal_ppf(p) == pytest.approx(stats.norm.ppf(p), rel=1e-12)


@pytest.mark.parametrize("p", TAIL_P)
def test_normal_ppf_and_percentiles_round_trip(p: float) -> None:
    """Standardise, look up z, un-standardise: the round trip has to close."""
    mu, sigma = 3.0, 2.0
    x = continuous.normal_ppf(p, mu, sigma)
    assert x == pytest.approx(stats.norm.ppf(p, mu, sigma), rel=1e-12)
    assert continuous.standardize(x, mu, sigma) == pytest.approx(continuous.standard_normal_ppf(p), rel=1e-12)

    row = next(r for r in continuous.normal_percentiles(mu, sigma, ps=(p,)) if r["p"] == p)
    assert row["standardised"] == pytest.approx(row["z"], rel=1e-12)
    assert row["x"] == pytest.approx(continuous.normal_ppf(p, mu, sigma), rel=1e-12)
    assert row["cdf_check"] == pytest.approx(p, abs=1e-12)


@pytest.mark.parametrize("p", [0.0, 1.0, -0.5, 1.5])
def test_standard_normal_ppf_rejects_values_outside_zero_to_one(p: float) -> None:
    with pytest.raises(ValueError):
        continuous.standard_normal_ppf(p)


def test_waiting_time_check_agrees_with_the_exponential_mean() -> None:
    result = continuous.waiting_time_check(rate=1.0, n_draws=50_000, seed=SEED)
    assert result["mean_comparison"]["agrees"] is True
    assert result["scale"] == pytest.approx(1.0)
    assert result["theoretical_mean"] == pytest.approx(1.0 / result["rate"])
    assert result["sample_mean"] == pytest.approx(1.0 / result["rate"], abs=0.05)

    faster = continuous.waiting_time_check(rate=2.0, n_draws=50_000, seed=SEED)
    assert faster["mean_comparison"]["agrees"] is True
    assert faster["sample_mean"] == pytest.approx(0.5, abs=0.05)


def test_continuous_samplers_match_their_mean_and_variance() -> None:
    uniform = simulate.uniform(-1.0, 2.0, 50_000, seed=SEED)
    assert float(uniform.mean()) == pytest.approx(continuous.uniform_mean(-1.0, 2.0), abs=0.02)
    assert float(uniform.var(ddof=1)) == pytest.approx(continuous.uniform_variance(-1.0, 2.0), abs=0.02)

    exponential = simulate.exponential(4.0, 50_000, seed=SEED)
    assert float(exponential.mean()) == pytest.approx(continuous.exponential_mean(4.0), abs=0.1)
    assert float(exponential.var(ddof=1)) == pytest.approx(continuous.exponential_variance(4.0), abs=1.5)


def test_simulate_poisson_process_bins_the_arrivals() -> None:
    """100 unit intervals over [0, 100]: the counts must be Poisson(1) on average."""
    result = continuous.simulate_poisson_process(rate=1.0, horizon=100.0, bin_width=1.0, seed=SEED)
    assert result["n_intervals"] == 100
    assert result["counts"].size == result["n_intervals"]
    assert int(result["counts"].sum()) == result["n_arrivals"]
    assert result["n_arrivals"] == result["arrivals"].size
    assert result["lambda_per_interval"] == pytest.approx(result["rate"] * result["bin_width"])
    assert result["counts_comparison"]["agrees"] is True
    assert result["mean_counts"] == pytest.approx(result["lambda_per_interval"], abs=2 * math.sqrt(1.0 / 100))


def test_interval_count_table_is_a_distribution() -> None:
    result = continuous.simulate_poisson_process(rate=1.0, horizon=100.0, bin_width=1.0, seed=SEED)
    rows = continuous.interval_count_table(result["counts"], result["lambda_per_interval"])
    assert [row["count"] for row in rows] == list(range(int(result["counts"].max()) + 1))
    assert math.fsum(row["observed"] for row in rows) == pytest.approx(1.0, abs=1e-12)
    # The table stops at the largest observed count, so it misses ~0.4% of the expected mass.
    assert math.fsum(row["expected_intervals"] for row in rows) == pytest.approx(
        result["n_intervals"], abs=0.5
    )
    for row in rows:
        assert row["poisson"] == pytest.approx(stats.poisson.pmf(row["count"], result["lambda_per_interval"]), rel=1e-9)
        assert row["observed"] == pytest.approx(row["observed_intervals"] / result["n_intervals"])
        assert row["expected_intervals"] == pytest.approx(result["n_intervals"] * row["poisson"])
