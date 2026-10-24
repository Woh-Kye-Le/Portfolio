"""Week 8 - Joint distributions.

Build the joint PMF table first, then read everything else out of it: marginals
(row/column sums), conditional PMFs (a row divided by its marginal),
conditional expectation, covariance and correlation.

Two examples:

* ``(X, Y) = (first die, sum of the dice)`` - counted with :mod:`itertools`, so
  the joint PMF is exact and the covariance has a value to check against,
* the lecture's trap ``Y = X^2`` for a symmetric ``X``: the correlation is
  exactly zero, yet ``Y`` is a deterministic function of ``X``, so ``X`` and
  ``Y`` are as dependent as two variables can be.
"""

from __future__ import annotations

import math
from itertools import product
from typing import Callable, Sequence

import numpy as np

from src.moments import expectation, variance_from_definition
from src.simulate import compare_estimate, rng

DICE_FACES: tuple[int, ...] = (1, 2, 3, 4, 5, 6)


# ------------------------------------------------------------- building the table
def joint_pmf(weights: np.ndarray) -> np.ndarray:
    """Normalise a table of counts (or weights) into a joint PMF."""
    weights = np.asarray(weights, dtype=float)
    return weights / weights.sum()


def joint_of_transform(
    values_x: Sequence[float],
    probs_x: Sequence[float],
    transform: Callable[[float], float],
    values_y: Sequence[float],
) -> np.ndarray:
    """Joint PMF of ``(X, g(X))``: a single non-zero entry per row of X."""
    index_y = {value: i for i, value in enumerate(values_y)}
    joint = np.zeros((len(values_x), len(values_y)))
    for i, (x, p) in enumerate(zip(values_x, probs_x)):
        joint[i, index_y[transform(x)]] = p
    return joint


def dice_joint(n_dice: int = 2) -> dict:
    """``(X, Y) = (first die, sum)`` counted straight from the sample space."""
    values_x = list(DICE_FACES)
    values_y = list(range(n_dice, 6 * n_dice + 1))
    counts = np.zeros((len(values_x), len(values_y)))

    for outcome in product(DICE_FACES, repeat=n_dice):
        counts[outcome[0] - 1, sum(outcome) - n_dice] += 1

    return {
        "values_x": values_x,
        "values_y": values_y,
        "counts": counts,
        "total": int(counts.sum()),
        "joint": joint_pmf(counts),
    }


# --------------------------------------------------------------- derived objects
def marginal_x(joint: np.ndarray) -> np.ndarray:
    """P(X = x): sum each row."""
    return np.asarray(joint, dtype=float).sum(axis=1)


def marginal_y(joint: np.ndarray) -> np.ndarray:
    """P(Y = y): sum each column."""
    return np.asarray(joint, dtype=float).sum(axis=0)


def conditional_pmf(joint: np.ndarray, row: int) -> np.ndarray:
    """P(Y = y | X = values_x[row]): one row, divided by its marginal."""
    joint = np.asarray(joint, dtype=float)
    marginal = joint[row].sum()
    return joint[row] / marginal


def conditional_expectations(joint: np.ndarray, values_y: Sequence[float]) -> np.ndarray:
    """E[Y | X = x] for every row - the regression of Y on X."""
    joint = np.asarray(joint, dtype=float)
    values_y = np.asarray(values_y, dtype=float)
    return np.array(
        [float(np.sum(conditional_pmf(joint, row) * values_y)) for row in range(joint.shape[0])]
    )


def conditional_variances(joint: np.ndarray, values_y: Sequence[float]) -> np.ndarray:
    """Var(Y | X = x) for every row."""
    joint = np.asarray(joint, dtype=float)
    values_y = np.asarray(values_y, dtype=float)
    means = conditional_expectations(joint, values_y)
    return np.array(
        [
            float(np.sum(conditional_pmf(joint, row) * (values_y - means[row]) ** 2))
            for row in range(joint.shape[0])
        ]
    )


def conditional_table(
    joint: np.ndarray,
    values_x: Sequence[float],
    values_y: Sequence[float],
) -> list[dict]:
    """One row per x: P(X = x), E[Y | X = x] and Var(Y | X = x)."""
    probs_x = marginal_x(joint)
    means = conditional_expectations(joint, values_y)
    variances = conditional_variances(joint, values_y)
    return [
        {
            "x": float(values_x[i]),
            "P(X = x)": float(probs_x[i]),
            "E[Y | X = x]": float(means[i]),
            "Var(Y | X = x)": float(variances[i]),
        }
        for i in range(len(values_x))
    ]


def expectation_xy(
    joint: np.ndarray,
    values_x: Sequence[float],
    values_y: Sequence[float],
) -> float:
    """E[XY] = sum sum x y p(x, y) - over the whole table, not the marginals."""
    joint = np.asarray(joint, dtype=float)
    return float(np.sum(np.outer(np.asarray(values_x, dtype=float), np.asarray(values_y, dtype=float)) * joint))


def covariance(
    joint: np.ndarray,
    values_x: Sequence[float],
    values_y: Sequence[float],
) -> float:
    """Cov(X, Y) = E[XY] - E[X] E[Y]."""
    return expectation_xy(joint, values_x, values_y) - expectation(values_x, marginal_x(joint)) * expectation(
        values_y, marginal_y(joint)
    )


def correlation(
    joint: np.ndarray,
    values_x: Sequence[float],
    values_y: Sequence[float],
) -> float:
    """rho = Cov(X, Y) / (sd(X) sd(Y))."""
    std_x = math.sqrt(variance_from_definition(values_x, marginal_x(joint)))
    std_y = math.sqrt(variance_from_definition(values_y, marginal_y(joint)))
    return covariance(joint, values_x, values_y) / (std_x * std_y)


def independence_check(
    joint: np.ndarray,
    values_x: Sequence[float],
    values_y: Sequence[float],
    tol: float = 1e-12,
) -> dict:
    """Independent means every cell equals the product of its marginals."""
    joint = np.asarray(joint, dtype=float)
    product_table = np.outer(marginal_x(joint), marginal_y(joint))
    deviations = np.abs(joint - product_table)
    worst = np.unravel_index(int(np.argmax(deviations)), joint.shape)
    return {
        "independent": bool(deviations.max() <= tol),
        "max_deviation": float(deviations.max()),
        "worst_cell": (float(values_x[worst[0]]), float(values_y[worst[1]])),
        "joint_at_worst_cell": float(joint[worst]),
        "product_at_worst_cell": float(product_table[worst]),
    }


def summary(
    joint: np.ndarray,
    values_x: Sequence[float],
    values_y: Sequence[float],
) -> dict:
    """Every derived quantity in one dict, including the iterated-expectation check."""
    probs_x = marginal_x(joint)
    probs_y = marginal_y(joint)
    mean_x = expectation(values_x, probs_x)
    mean_y = expectation(values_y, probs_y)
    conditional_means = conditional_expectations(joint, values_y)
    iterated = float(np.sum(probs_x * conditional_means))
    return {
        "E_X": mean_x,
        "E_Y": mean_y,
        "Var_X": variance_from_definition(values_x, probs_x),
        "Var_Y": variance_from_definition(values_y, probs_y),
        "E_XY": expectation_xy(joint, values_x, values_y),
        "E_X_E_Y": mean_x * mean_y,
        "Cov": covariance(joint, values_x, values_y),
        "Corr": correlation(joint, values_x, values_y),
        "E_conditional_Y_given_X": iterated,
        "iterated_expectation_error": abs(iterated - mean_y),
        "independent": independence_check(joint, values_x, values_y)["independent"],
    }


def joint_table(
    joint: np.ndarray,
    values_x: Sequence[float],
    values_y: Sequence[float],
) -> list[dict]:
    """Long-format table of the non-zero cells, for display."""
    joint = np.asarray(joint, dtype=float)
    return [
        {"x": float(values_x[i]), "y": float(values_y[j]), "P(X = x, Y = y)": float(joint[i, j])}
        for i in range(len(values_x))
        for j in range(len(values_y))
        if joint[i, j] > 0.0
    ]


# ------------------------------------------------------------------- simulations
def simulate_dice_joint(
    n_rolls: int = 100_000,
    n_dice: int = 2,
    seed: int | None = None,
) -> dict:
    """Sample ``(first die, sum)`` and check covariance and correlation."""
    exact = dice_joint(n_dice)
    roll = rng(seed).integers(1, 7, size=(n_rolls, n_dice))
    x = roll[:, 0].astype(float)
    y = roll.sum(axis=1).astype(float)

    covariance_estimate = float(np.cov(x, y, ddof=1)[0, 1])
    correlation_estimate = float(np.corrcoef(x, y)[0, 1])
    exact_covariance = covariance(exact["joint"], exact["values_x"], exact["values_y"])
    exact_correlation = correlation(exact["joint"], exact["values_x"], exact["values_y"])

    return {
        "n_rolls": n_rolls,
        "covariance_estimate": covariance_estimate,
        "covariance_exact": exact_covariance,
        "correlation_estimate": correlation_estimate,
        "correlation_exact": exact_correlation,
        "correlation_comparison": compare_estimate(
            correlation_estimate,
            exact_correlation,
            (1.0 - exact_correlation**2) / math.sqrt(n_rolls),
        ),
        "sample_means": (float(x.mean()), float(y.mean())),
    }


def x_squared_trap(values: Sequence[float] = (-2, -1, 0, 1, 2)) -> dict:
    """``Y = X^2`` with a symmetric ``X``: zero correlation, total dependence.

    ``E[X] = 0`` and ``E[XY] = E[X^3] = 0`` by symmetry, so the covariance - and
    with it the correlation - vanishes.  Yet ``P(Y = 4 | X = 2) = 1`` while
    ``P(Y = 4) = 1/5``: knowing X tells you exactly what Y is.
    """
    values = list(values)
    probs = [1.0 / len(values)] * len(values)
    values_y = sorted({float(v) ** 2 for v in values})
    joint = joint_of_transform(values, probs, lambda x: x * x, values_y)

    witness_x = float(max(values))
    witness_y = witness_x**2
    row = conditional_pmf(joint, values.index(max(values)))
    column = marginal_y(joint)
    return {
        "values_x": values,
        "values_y": values_y,
        "joint": joint,
        "probs_x": probs,
        "summary": summary(joint, values, values_y),
        "witness": {
            "x": witness_x,
            "y": witness_y,
            "P(Y = y | X = x)": float(row[values_y.index(witness_y)]),
            "P(Y = y)": float(column[values_y.index(witness_y)]),
        },
    }


def simulate_x_squared_trap(
    n_draws: int = 100_000,
    values: Sequence[float] = (-2, -1, 0, 1, 2),
    seed: int | None = None,
) -> dict:
    """Sample the trap and count the two conditional frequencies directly."""
    values = np.asarray(values, dtype=float)
    x = rng(seed).choice(values, size=n_draws)
    y = x**2

    correlation_estimate = float(np.corrcoef(x, y)[0, 1])
    witness_x = float(values.max())
    witness_y = witness_x**2
    at_witness = x == witness_x
    return {
        "n_draws": n_draws,
        "correlation_estimate": correlation_estimate,
        "correlation_exact": 0.0,
        "correlation_comparison": compare_estimate(
            correlation_estimate, 0.0, 1.0 / math.sqrt(n_draws)
        ),
        "P_y_at_witness": float(np.mean(y == witness_y)),
        "P_y_given_x_at_witness": float(np.mean(y[at_witness] == witness_y)),
        "witness": {"x": witness_x, "y": witness_y},
    }
