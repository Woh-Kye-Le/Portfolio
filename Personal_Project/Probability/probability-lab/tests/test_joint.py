"""Tests for :mod:`src.joint` - joint PMF tables, marginals, covariance and dependence."""

from __future__ import annotations

import math

import numpy as np
import pytest

from src import joint

SEED = 1234
DICE_VALUES_X = [1, 2, 3, 4, 5, 6]
DICE_VALUES_Y = list(range(2, 13))
TWO_DICE_SUM_PROBS = [1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1]


def test_joint_pmf_normalises_weights() -> None:
    joint_table = joint.joint_pmf(np.array([[1.0, 3.0], [5.0, 7.0]]))
    assert joint_table.sum() == pytest.approx(1.0)
    assert joint_table[0, 0] == pytest.approx(1.0 / 16.0)


def test_dice_joint_counts_the_sample_space() -> None:
    """(first die, sum of two dice) counted from the 36 equally likely outcomes."""
    table = joint.dice_joint(2)
    assert table["values_x"] == DICE_VALUES_X
    assert table["values_y"] == DICE_VALUES_Y
    assert table["counts"].shape == (6, 11)
    assert table["total"] == 36
    assert table["counts"].sum() == 36
    assert table["joint"].sum() == pytest.approx(1.0)
    assert table["counts"][0, 0] == 1.0  # x = 1, y = 2: only (1, 1)
    assert table["counts"][5, 10] == 1.0  # x = 6, y = 12: only (6, 6)
    assert table["counts"][0, 5] == 1.0  # x = 1, y = 7: exactly one second die


def test_marginals_are_valid_pmfs() -> None:
    table = joint.dice_joint(2)
    probs_x = joint.marginal_x(table["joint"])
    probs_y = joint.marginal_y(table["joint"])
    for marginal in (probs_x, probs_y):
        assert np.all(marginal >= 0.0)
        assert math.fsum(marginal) == pytest.approx(1.0)
        assert marginal.sum() == pytest.approx(1.0)
    assert probs_x == pytest.approx([1.0 / 6.0] * 6)
    assert probs_y == pytest.approx([count / 36.0 for count in TWO_DICE_SUM_PROBS])
    assert np.outer(probs_x, probs_y).sum() == pytest.approx(1.0)


def test_covariance_of_first_die_and_sum() -> None:
    """Cov(X, X + Z) = Var(X) = 35/12 because the second die is independent of the first."""
    table = joint.dice_joint(2)
    covariance = joint.covariance(table["joint"], table["values_x"], table["values_y"])
    assert covariance == pytest.approx(35.0 / 12.0, rel=1e-9)
    # E[X(X + Z)] = E[X^2] + E[X] E[Z] = 91/6 + 3.5^2
    assert joint.expectation_xy(table["joint"], table["values_x"], table["values_y"]) == pytest.approx(
        91.0 / 6.0 + 3.5 * 3.5
    )
    assert joint.correlation(table["joint"], table["values_x"], table["values_y"]) == pytest.approx(
        1.0 / math.sqrt(2.0), rel=1e-9
    )


def test_independence_check_rejects_the_dice_joint() -> None:
    """X and X + Z are dependent even though the covariance is positive, not zero."""
    table = joint.dice_joint(2)
    check = joint.independence_check(table["joint"], table["values_x"], table["values_y"])
    assert check["independent"] is False
    assert check["max_deviation"] > 1e-12
    assert abs(check["joint_at_worst_cell"] - check["product_at_worst_cell"]) > 1e-12


def test_independence_check_accepts_a_product_joint() -> None:
    """A factorised table really is independent: every cell equals the product of marginals."""
    values_x, values_y = [0.0, 1.0], [0.0, 1.0]
    product = joint.joint_pmf(np.outer([2.0, 8.0], [3.0, 7.0]))
    check = joint.independence_check(product, values_x, values_y)
    assert check["independent"] is True
    assert check["max_deviation"] <= 1e-12
    assert joint.covariance(product, values_x, values_y) == pytest.approx(0.0, abs=1e-15)
    assert joint.correlation(product, values_x, values_y) == pytest.approx(0.0, abs=1e-15)
    assert joint.summary(product, values_x, values_y)["independent"] is True


def test_iterated_expectation_recovers_the_marginal_mean() -> None:
    """E[E[Y | X]] = E[Y], the law of total expectation read off the joint table."""
    table = joint.dice_joint(2)
    summary = joint.summary(table["joint"], table["values_x"], table["values_y"])
    assert summary["iterated_expectation_error"] < 1e-12
    assert summary["E_conditional_Y_given_X"] == pytest.approx(summary["E_Y"])
    assert summary["E_Y"] == pytest.approx(7.0)
    assert summary["E_X"] == pytest.approx(3.5)
    assert summary["Var_Y"] == pytest.approx(2 * 35.0 / 12.0)
    assert summary["Cov"] == pytest.approx(summary["Var_X"])
    assert summary["E_XY"] == pytest.approx(summary["E_X"] * summary["E_Y"] + summary["Cov"])


def test_conditional_tables_condition_on_the_first_die() -> None:
    table = joint.dice_joint(2)
    for row, x in enumerate(table["values_x"]):
        conditional = joint.conditional_pmf(table["joint"], row)
        assert conditional.sum() == pytest.approx(1.0)
    means = joint.conditional_expectations(table["joint"], table["values_y"])
    variances = joint.conditional_variances(table["joint"], table["values_y"])
    assert means == pytest.approx([x + 3.5 for x in table["values_x"]])
    assert variances == pytest.approx([35.0 / 12.0] * 6)

    rows = joint.conditional_table(table["joint"], table["values_x"], table["values_y"])
    assert len(rows) == 6
    for row, x in enumerate(table["values_x"]):
        assert rows[row]["x"] == x
        assert rows[row]["P(X = x)"] == pytest.approx(1.0 / 6.0)
        assert rows[row]["E[Y | X = x]"] == pytest.approx(x + 3.5)
        assert rows[row]["Var(Y | X = x)"] == pytest.approx(35.0 / 12.0)


def test_joint_table_lists_the_non_zero_cells() -> None:
    table = joint.dice_joint(2)
    rows = joint.joint_table(table["joint"], table["values_x"], table["values_y"])
    assert len(rows) == 36  # every (die, sum) pair with sum >= die + 1 occurs
    assert math.fsum(row["P(X = x, Y = y)"] for row in rows) == pytest.approx(1.0)


def test_simulate_dice_joint_agrees() -> None:
    result = joint.simulate_dice_joint(n_rolls=50_000, n_dice=2, seed=SEED)
    assert result["correlation_comparison"]["agrees"] is True
    assert result["correlation_exact"] == pytest.approx(1.0 / math.sqrt(2.0), rel=1e-9)
    assert result["covariance_exact"] == pytest.approx(35.0 / 12.0, rel=1e-9)
    assert result["correlation_estimate"] == pytest.approx(result["correlation_exact"], abs=0.01)
    assert result["covariance_estimate"] == pytest.approx(result["covariance_exact"], abs=0.1)
    assert result["sample_means"][0] == pytest.approx(3.5, abs=0.05)
    assert result["sample_means"][1] == pytest.approx(7.0, abs=0.05)


def test_x_squared_trap_has_zero_correlation_but_total_dependence() -> None:
    """Y = X^2 for a symmetric X: rho = 0, yet knowing X pins down Y exactly."""
    trap = joint.x_squared_trap()
    summary = trap["summary"]
    assert summary["Cov"] == pytest.approx(0.0, abs=1e-12)
    assert summary["Corr"] == pytest.approx(0.0, abs=1e-12)
    assert summary["independent"] is False

    witness = trap["witness"]
    assert (witness["x"], witness["y"]) == (2.0, 4.0)
    assert witness["P(Y = y | X = x)"] == 1.0
    # Y = 4 happens for X = 2 and X = -2, so the marginal probability is 2/5, not 1/5.
    assert witness["P(Y = y)"] == pytest.approx(0.4)
    assert trap["joint"].sum() == pytest.approx(1.0)

    conditional = joint.conditional_expectations(trap["joint"], trap["values_y"])
    assert conditional == pytest.approx([x * x for x in trap["values_x"]])


def test_simulate_x_squared_trap_agrees() -> None:
    result = joint.simulate_x_squared_trap(n_draws=50_000, values=(-2, -1, 0, 1, 2), seed=SEED)
    assert result["correlation_comparison"]["agrees"] is True
    assert result["correlation_exact"] == 0.0
    assert abs(result["correlation_estimate"]) < 0.02
    assert result["P_y_given_x_at_witness"] == 1.0
    assert result["P_y_at_witness"] == pytest.approx(0.4, abs=0.02)
    assert (result["witness"]["x"], result["witness"]["y"]) == (2.0, 4.0)
