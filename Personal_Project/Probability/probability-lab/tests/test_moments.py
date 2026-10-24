"""Tests for :mod:`src.moments` - expectation and variance four ways."""

from __future__ import annotations

import math

import numpy as np
import pytest
import scipy.stats as stats

from src import discrete, moments, simulate

SEED = 1234
DIE_VALUES = [1, 2, 3, 4, 5, 6]
DIE_PROBS = [1.0 / 6.0] * 6
DIE_MEAN = 3.5
DIE_VARIANCE = 35.0 / 12.0
SMALL_VALUES = [0.0, 1.0, 2.0]
SMALL_PROBS = [0.1, 0.6, 0.3]
SMALL_MEAN = 1.2
SMALL_VARIANCE = 0.36


def test_die_distribution_helper() -> None:
    values, probs = moments.die_distribution()
    assert values == DIE_VALUES
    assert probs == pytest.approx(DIE_PROBS)
    assert math.fsum(probs) == pytest.approx(1.0)


def test_all_routes_agree_on_the_die() -> None:
    """E[X] = 3.5 and Var(X) = 35/12 from the definition, the shortcut and the MGF."""
    assert moments.expectation(DIE_VALUES, DIE_PROBS) == pytest.approx(DIE_MEAN)
    assert moments.variance_from_definition(DIE_VALUES, DIE_PROBS) == pytest.approx(DIE_VARIANCE)
    assert moments.variance_shortcut(DIE_VALUES, DIE_PROBS) == pytest.approx(DIE_VARIANCE)
    assert moments.second_moment(DIE_VALUES, DIE_PROBS) == pytest.approx(91.0 / 6.0)
    assert moments.standard_deviation(DIE_VALUES, DIE_PROBS) == pytest.approx(math.sqrt(DIE_VARIANCE))
    assert moments.central_moment(DIE_VALUES, DIE_PROBS, 4) == pytest.approx(88.375 / 6.0)
    assert moments.expectation_of(lambda x: x * x, DIE_VALUES, DIE_PROBS) == pytest.approx(91.0 / 6.0)


def test_all_routes_agree_on_a_small_hand_made_distribution() -> None:
    assert moments.expectation(SMALL_VALUES, SMALL_PROBS) == pytest.approx(SMALL_MEAN)
    assert moments.second_moment(SMALL_VALUES, SMALL_PROBS) == pytest.approx(1.8)
    assert moments.variance_from_definition(SMALL_VALUES, SMALL_PROBS) == pytest.approx(SMALL_VARIANCE)
    assert moments.variance_shortcut(SMALL_VALUES, SMALL_PROBS) == pytest.approx(SMALL_VARIANCE)
    assert moments.central_moment(SMALL_VALUES, SMALL_PROBS, 1) == pytest.approx(0.0, abs=1e-15)
    assert moments.expectation_of(lambda x: 3.0 * x + 1.0, SMALL_VALUES, SMALL_PROBS) == pytest.approx(4.6)


def test_variance_from_definition_anchors_to_scipy() -> None:
    """The hand-written variance of a Binomial sample law equals the scipy PMF sum."""
    n, p = 10, 0.3
    values, probs = moments.binomial_distribution(n, p)
    mean = float(stats.binom.mean(n, p))
    reference = math.fsum((k - mean) ** 2 * stats.binom.pmf(k, n, p) for k in values)
    assert moments.variance_from_definition(values, probs) == pytest.approx(reference, rel=1e-12)
    assert moments.variance_from_definition(values, probs) == pytest.approx(float(stats.binom.var(n, p)), rel=1e-12)
    assert moments.expectation(values, probs) == pytest.approx(mean, rel=1e-12)
    assert moments.variance_shortcut(values, probs) == pytest.approx(reference, rel=1e-12)


def test_mgf_at_zero_and_derivatives() -> None:
    """M(0) = 1 and M'(0) = E[X] for any valid PMF."""
    assert moments.mgf(DIE_VALUES, DIE_PROBS, 0.0) == pytest.approx(1.0)
    assert moments.mgf(DIE_VALUES, DIE_PROBS, 0.0) == pytest.approx(math.fsum(DIE_PROBS))
    assert moments.mgf_derivative(DIE_VALUES, DIE_PROBS, 0.0, 1) == pytest.approx(DIE_MEAN)
    assert moments.mgf_derivative(DIE_VALUES, DIE_PROBS, 0.0, 2) == pytest.approx(91.0 / 6.0)
    assert moments.mgf_derivative(DIE_VALUES, DIE_PROBS, 0.5, 1) == pytest.approx(
        math.fsum(x * math.exp(0.5 * x) / 6.0 for x in DIE_VALUES)
    )


def test_moments_from_mgf_recover_the_die() -> None:
    recovered = moments.moments_from_mgf(DIE_VALUES, DIE_PROBS)
    assert recovered["M_0"] == pytest.approx(1.0)
    assert recovered["E_X"] == pytest.approx(DIE_MEAN)
    assert recovered["E_X2"] == pytest.approx(91.0 / 6.0)
    assert recovered["Var"] == pytest.approx(DIE_VARIANCE)


def test_moments_from_mgf_recover_binomial_mean_and_variance() -> None:
    """E[X] = np = 3 and Var(X) = np(1 - p) = 2.1 for Binomial(10, 0.3)."""
    values, probs = moments.binomial_distribution(10, 0.3)
    recovered = moments.moments_from_mgf(values, probs)
    assert recovered["E_X"] == pytest.approx(discrete.binomial_mean(10, 0.3), rel=1e-12)
    assert recovered["Var"] == pytest.approx(discrete.binomial_variance(10, 0.3), rel=1e-12)
    assert recovered["E_X"] == pytest.approx(3.0, rel=1e-12)
    assert recovered["Var"] == pytest.approx(2.1, rel=1e-12)


def test_mgf_moment_table_errors_are_tiny() -> None:
    rows = moments.mgf_moment_table()
    assert [row["distribution"] for row in rows] == [
        "Fair die",
        "Binomial(10, 0.3)",
        "Poisson(3) (truncated, renormalised)",
        "Geometric(0.2) (truncated, renormalised)",
    ]
    for row in rows:
        assert row["M(0)"] == pytest.approx(1.0, abs=1e-12)
        assert row["mean_error"] < 1e-9
        assert row["variance_error"] < 1e-9
        assert row["E[X] from M'(0)"] == pytest.approx(row["analytic E[X]"], abs=1e-9)
        assert row["Var from M''(0)"] == pytest.approx(row["analytic Var(X)"], abs=1e-9)


def test_truncated_distribution_helpers_are_renormalised() -> None:
    for values, probs in (moments.poisson_distribution(3.0), moments.geometric_distribution(0.2)):
        assert math.fsum(probs) == pytest.approx(1.0, abs=1e-15)
        assert all(prob > 0.0 for prob in probs)
    assert moments.poisson_distribution(3.0)[1][0] == pytest.approx(discrete.poisson_pmf(0, 3.0), rel=1e-12)
    assert moments.geometric_distribution(0.2)[0][0] == 1


def test_indicator_decomposition_reaches_the_binomial_mean() -> None:
    """Linearity of expectation: 10 indicators each with mean 0.3 give E[X] = 3 = np."""
    result = moments.indicator_decomposition(10, 0.3)
    assert result["terms"] == 10
    assert result["each_indicator_mean"] == pytest.approx(0.3)
    assert result["sum_of_indicator_means"] == pytest.approx(10 * 0.3)
    assert result["sum_of_indicator_means"] == pytest.approx(discrete.binomial_mean(10, 0.3))
    assert result["binomial_mean"] == pytest.approx(result["sum_of_indicator_means"])
    assert result["binomial_variance"] == pytest.approx(discrete.binomial_variance(10, 0.3))


def test_simulate_linearity_agrees() -> None:
    result = moments.simulate_linearity(n=10, p=0.3, trials=50_000, seed=SEED)
    assert result["indicator_comparison"]["agrees"] is True
    assert result["count_comparison"]["agrees"] is True
    assert result["mean_of_counts"] == pytest.approx(result["summed_indicator_means"], abs=1e-12)
    assert result["summed_indicator_means"] == pytest.approx(result["exact_mean"], abs=0.05)
    assert result["indicator_means"].shape == (10,)
    assert result["sample_variance"] == pytest.approx(result["exact_variance"], abs=0.1)


def test_moment_report_die_agrees_with_a_seeded_sample() -> None:
    """50 000 seeded rolls: the simulated mean and variance match 3.5 and 35/12."""
    sample = simulate.rng(SEED).integers(1, 7, size=50_000)
    report = moments.moment_report(DIE_VALUES, DIE_PROBS, sample)
    assert report["sample_size"] == 50_000
    assert report["mean_comparison"]["agrees"] is True
    assert report["variance_comparison"]["agrees"] is True
    assert report["exact_mean"] == pytest.approx(DIE_MEAN)
    assert report["exact_variance"] == pytest.approx(DIE_VARIANCE)
    assert report["mean_definition"] == pytest.approx(report["mean_shortcut"])
    assert report["variance_definition"] == pytest.approx(report["variance_shortcut"])
    assert report["sample_mean"] == pytest.approx(DIE_MEAN, abs=0.05)
    assert report["sample_variance"] == pytest.approx(DIE_VARIANCE, abs=0.1)


def test_moment_report_table_has_the_three_routes() -> None:
    sample = simulate.rng(SEED).integers(1, 7, size=5_000)
    rows = moments.moment_report_table(moments.moment_report(DIE_VALUES, DIE_PROBS, sample))
    assert [row["route"] for row in rows] == ["definition", "shortcut formula", "simulation (n = 5,000)"]
    assert all(row["E[X]"] == pytest.approx(DIE_MEAN, abs=0.1) for row in rows)
    assert all(row["Var(X)"] == pytest.approx(DIE_VARIANCE, abs=0.2) for row in rows)


def test_sample_variance_error_is_positive_and_shrinks_with_n() -> None:
    small = moments.sample_variance_error(DIE_VALUES, DIE_PROBS, 1_000)
    large = moments.sample_variance_error(DIE_VALUES, DIE_PROBS, 100_000)
    assert 0.0 < large < small
    assert large == pytest.approx(small / math.sqrt(100.0), rel=1e-12)
    assert np.isfinite(moments.expectation_of(lambda x: x, DIE_VALUES, DIE_PROBS))
