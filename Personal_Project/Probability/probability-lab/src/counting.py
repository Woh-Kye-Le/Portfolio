"""Week 1 - Counting and events.

Everything here is *exact*: three dice all 6^3 = 216 outcomes are small enough
to write down with :mod:`itertools`, so a probability is just
``favourable / total``.  The simulation only exists to confirm the count, which
is the pattern the rest of the lab repeats with formulas instead of counting.

Example used throughout: ``P(sum >= 17) = 4/216 = 1/54``, i.e. about 1.85%.
"""

from __future__ import annotations

from fractions import Fraction
from itertools import product
from math import comb, perm
from typing import Callable, Sequence

import numpy as np

from src.simulate import bernoulli, compare_frequency, dice_sums

DICE_FACES: tuple[int, ...] = (1, 2, 3, 4, 5, 6)


# ------------------------------------------------------------- the sample space
def sample_space(n_dice: int = 3) -> list[tuple[int, ...]]:
    """All 6^n equally likely outcomes, from (1, 1, 1) to (6, 6, 6)."""
    return list(product(DICE_FACES, repeat=n_dice))


def total_outcomes(n_dice: int = 3) -> int:
    """|Omega| = 6^n."""
    return 6**n_dice


def count_where(predicate: Callable[[tuple[int, ...]], bool], n_dice: int = 3) -> int:
    """|A|: how many outcomes satisfy the event."""
    return sum(1 for outcome in sample_space(n_dice) if predicate(outcome))


def exact_probability(
    predicate: Callable[[tuple[int, ...]], bool],
    n_dice: int = 3,
) -> Fraction:
    """P(A) = |A| / |Omega|, kept exact as a Fraction so nothing hides in floats."""
    return Fraction(count_where(predicate, n_dice), total_outcomes(n_dice))


def dice_sum(outcome: Sequence[int]) -> int:
    """The statistic most Week-1 events are defined on."""
    return int(sum(outcome))


def sum_distribution(n_dice: int = 3) -> dict[int, int]:
    """Outcome counts per attainable sum - the PMF before normalising."""
    counts: dict[int, int] = {}
    for outcome in sample_space(n_dice):
        total = dice_sum(outcome)
        counts[total] = counts.get(total, 0) + 1
    return dict(sorted(counts.items()))


# ------------------------------------------------------ the week-1 running example
def sum_at_least(threshold: int = 17, n_dice: int = 3) -> Fraction:
    """P(sum >= threshold) - 1/54 when threshold is 17 and there are three dice."""
    return exact_probability(lambda outcome: dice_sum(outcome) >= threshold, n_dice)


def simulate_sum_at_least(
    threshold: int = 17,
    n_rolls: int = 50_000,
    n_dice: int = 3,
    seed: int | None = None,
) -> dict:
    """Roll the dice ``n_rolls`` times and count how often the sum reaches ``threshold``."""
    sums = dice_sums(n_rolls, n_dice, seed)
    hits = int(np.count_nonzero(sums >= threshold))
    return {
        "hits": hits,
        "trials": n_rolls,
        **compare_frequency(hits / n_rolls, float(sum_at_least(threshold, n_dice)), n_rolls),
    }


def sum_distribution_table(
    n_dice: int = 3,
    n_rolls: int = 50_000,
    seed: int | None = None,
) -> list[dict]:
    """Exact count / probability per sum next to the simulated frequency."""
    counts = sum_distribution(n_dice)
    total = total_outcomes(n_dice)
    sums = dice_sums(n_rolls, n_dice, seed)

    rows = []
    for total_sum, count in counts.items():
        rows.append(
            {
                "sum": total_sum,
                "count": count,
                "exact": count / total,
                "simulated": float(np.count_nonzero(sums == total_sum)) / n_rolls,
            }
        )
    return rows


# ------------------------------------------------------------- inclusion-exclusion
def union_inclusion_exclusion(intersections: Sequence[Sequence[float]]) -> float:
    """P(A1 u ... u An) from a nested list of intersection probabilities.

    ``intersections[order - 1]`` holds the probabilities of every order-``order``
    intersection, so ``[[0.1, 0.2], [0.05]]`` computes P(A) + P(B) - P(A n B).
    """
    total = 0.0
    for order, probs in enumerate(intersections, start=1):
        total += (-1.0) ** (order + 1) * float(sum(probs))
    return total


def union_two(p_a: float, p_b: float, p_both: float) -> float:
    """P(A u B) = P(A) + P(B) - P(A n B)."""
    return union_inclusion_exclusion([[p_a, p_b], [p_both]])


def union_three(
    p_a: float,
    p_b: float,
    p_c: float,
    p_ab: float,
    p_ac: float,
    p_bc: float,
    p_abc: float,
) -> float:
    """The three-event version, with the +P(A n B n C) correction term."""
    return union_inclusion_exclusion([[p_a, p_b, p_c], [p_ab, p_ac, p_bc], [p_abc]])


def at_least_one_six(n_dice: int = 3) -> float:
    """P(at least one six) by inclusion-exclusion: 91/216 for three dice.

    The events "die i shows a six" are independent, so every order-k intersection
    is (1/6)^k and the alternating sum collapses.
    """
    intersections = [
        [float(Fraction(1, 6) ** order)] * comb(n_dice, order) for order in range(1, n_dice + 1)
    ]
    return union_inclusion_exclusion(intersections)


def at_least_one(p: float, n_trials: int) -> float:
    """The complement shortcut: 1 - (1 - p)^n for independent trials."""
    return 1.0 - (1.0 - p) ** n_trials


def at_least_one_six_table(
    n_values: Sequence[int] = (1, 2, 3, 4, 5, 6),
    n_experiments: int = 50_000,
    seed: int | None = None,
) -> list[dict]:
    """Exact ``1 - (5/6)^n`` against a simulation of ``n`` dice per experiment."""
    rows = []
    for n in n_values:
        draws = bernoulli(1 / 6, (n_experiments, n), seed)
        estimate = float(np.mean(draws.any(axis=1)))
        rows.append(
            {
                "n": int(n),
                "exact": at_least_one(1 / 6, n),
                "simulated": estimate,
                "abs_error": abs(estimate - at_least_one(1 / 6, n)),
            }
        )
    return rows


# ------------------------------------------------------- combinations / permutations
def combinations_count(n: int, k: int) -> int:
    """C(n, k) - subsets of size k (order ignored)."""
    return comb(n, k)


def permutations_count(n: int, k: int) -> int:
    """P(n, k) - ordered arrangements of size k."""
    return perm(n, k)


def birthday_collision_probability(n_people: int, n_days: int = 365) -> float:
    """Classic counting exercise: 1 - P(365, n) / 365^n, ordered outcomes counted directly."""
    if n_people > n_days:
        return 1.0
    distinct = perm(n_days, n_people)
    return 1.0 - distinct / n_days**n_people
