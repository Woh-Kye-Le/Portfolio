"""Tests for :mod:`src.simulate` - the seeded RNG layer and the error yardsticks."""

from __future__ import annotations

import math

import numpy as np
import pytest

from src import simulate


def test_rng_is_reproducible_and_seed_dependent() -> None:
    """Same seed, same stream; different seeds, different streams."""
    assert simulate.rng(1234).random(5) == pytest.approx(simulate.rng(1234).random(5))
    assert not np.allclose(simulate.rng(1234).random(5), simulate.rng(4321).random(5))


def test_default_seed_keeps_module_helpers_reproducible() -> None:
    """Module-level helpers default to DEFAULT_SEED, so repeated calls agree."""
    assert np.array_equal(simulate.dice_sums(100), simulate.dice_sums(100))
    assert np.array_equal(simulate.dice_sums(100), simulate.dice_sums(100, seed=simulate.DEFAULT_SEED))
    assert not np.array_equal(simulate.dice_sums(100), simulate.dice_sums(100, seed=simulate.DEFAULT_SEED + 1))


def test_standard_error_is_sqrt_p_one_minus_p_over_n() -> None:
    assert simulate.standard_error(0.5, 100) == pytest.approx(0.05)
    assert simulate.standard_error(0.2, 400) == pytest.approx(math.sqrt(0.2 * 0.8 / 400))


def test_sample_mean_error_is_s_over_sqrt_n() -> None:
    sample = np.array([1.0, 2.0, 3.0, 4.0])
    expected = np.std(sample, ddof=1) / np.sqrt(sample.size)
    assert simulate.sample_mean_error(sample) == pytest.approx(expected)


def test_compare_estimate_flags_small_and_large_errors() -> None:
    """The comparison accepts errors inside SIGMA standard errors and rejects larger ones."""
    close = simulate.compare_estimate(0.51, 0.5, 0.1)
    assert close["agrees"] is True
    assert close["z_score"] == pytest.approx(0.1)  # (0.51 - 0.5) / 0.1
    assert close["tolerance"] == pytest.approx(2.0 * 0.1)

    far = simulate.compare_estimate(0.7, 0.5, 0.05)
    assert far["agrees"] is False
    assert far["z_score"] == pytest.approx(4.0)  # positive: the estimate is too high

    low = simulate.compare_estimate(0.35, 0.5, 0.05)
    assert low["agrees"] is False
    assert low["z_score"] == pytest.approx(-3.0)  # negative: the estimate is too low


def test_compare_frequency_uses_the_frequency_standard_error() -> None:
    exact = simulate.compare_frequency(0.5, 0.5, 100)
    assert exact["std_error"] == pytest.approx(simulate.standard_error(0.5, 100))
    assert exact["agrees"] is True

    off = simulate.compare_frequency(0.62, 0.5, 100)
    assert off["agrees"] is False
    assert off["z_score"] == pytest.approx(0.12 / simulate.standard_error(0.5, 100))


def test_relative_frequency_takes_arrays_or_counts() -> None:
    hits = np.array([True, False, True, True])
    assert simulate.relative_frequency(hits, 4) == pytest.approx(0.75)
    assert simulate.relative_frequency(3, 10) == pytest.approx(0.3)


def test_sample_moments_matches_numpy() -> None:
    sample = simulate.normal(2.0, 3.0, 2_000, seed=1234)
    moments = simulate.sample_moments(sample)
    assert moments["n"] == 2_000
    assert moments["mean"] == pytest.approx(float(np.mean(sample)))
    assert moments["second_moment"] == pytest.approx(float(np.mean(sample**2)))
    assert moments["variance"] == pytest.approx(float(np.var(sample, ddof=1)))
    assert moments["variance_biased"] == pytest.approx(float(np.var(sample)))
    assert moments["std"] == pytest.approx(float(np.std(sample, ddof=1)))


def test_running_mean_ends_at_the_sample_mean() -> None:
    sample = simulate.uniform(0.0, 1.0, 500, seed=1234)
    running = simulate.running_mean(sample)
    assert running.shape == sample.shape
    assert running[-1] == pytest.approx(float(np.mean(sample)))


def test_samplers_are_reproducible_with_the_right_shape() -> None:
    assert simulate.roll_dice(10, 3, seed=1234).shape == (10, 3)
    assert simulate.dice_sums(10, 3, seed=1234).shape == (10,)
    assert np.array_equal(simulate.roll_dice(10, 3, seed=1234), simulate.roll_dice(10, 3, seed=1234))
    assert np.array_equal(
        simulate.dice_sums(10, 3, seed=1234),
        simulate.roll_dice(10, 3, seed=1234).sum(axis=1),
    )
    for sample in (
        simulate.bernoulli(0.3, (5, 4), seed=1234),
        simulate.binomial(10, 0.3, 7, seed=1234),
        simulate.geometric(0.2, 7, seed=1234),
        simulate.poisson(3.0, 7, seed=1234),
        simulate.uniform(0.0, 1.0, 7, seed=1234),
        simulate.normal(0.0, 1.0, 7, seed=1234),
        simulate.exponential(2.0, 7, seed=1234),
    ):
        assert sample.shape == (7,) or sample.shape == (5, 4)
    assert simulate.roll_dice(200, 1, seed=1234).min() >= 1
    assert simulate.roll_dice(200, 1, seed=1234).max() <= 6
    assert simulate.geometric(0.2, 500, seed=1234).min() >= 1  # support is k >= 1
    assert np.array_equal(simulate.exponential_waiting_times(2.0, 5, seed=1234), simulate.exponential(2.0, 5, seed=1234))


def test_poisson_process_arrivals_are_sorted_inside_the_horizon() -> None:
    """N ~ Poisson(rate * horizon) and, given N, uniform order statistics on [0, horizon]."""
    rate, horizon = 2.5, 40.0
    arrivals = simulate.poisson_process_arrivals(rate, horizon, seed=1234)
    assert np.all(np.diff(arrivals) >= 0.0)  # sorted
    assert arrivals.min() >= 0.0 and arrivals.max() <= horizon
    expected = rate * horizon
    assert abs(arrivals.size - expected) <= 5.0 * math.sqrt(expected)


def test_counts_per_interval_sums_back_to_the_arrivals() -> None:
    arrivals = simulate.poisson_process_arrivals(1.0, 100.0, seed=1234)
    counts = simulate.counts_per_interval(arrivals, 1.0, 100.0)
    assert counts.size == 100
    assert int(counts.sum()) == int(arrivals.size)
    assert int(counts.sum()) == int(np.histogram(arrivals, bins=np.arange(0.0, 101.0, 1.0))[0].sum())

    auto = simulate.counts_per_interval(arrivals, 10.0)  # horizon inferred from the last arrival
    assert int(auto.sum()) == int(arrivals.size)
