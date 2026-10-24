"""Tests for :mod:`src.counting` - exact counts over the 6^n sample space."""

from __future__ import annotations

import math
from fractions import Fraction

import numpy as np
import pytest

from src import counting, simulate

SEED = 1234


def test_sample_space_and_total_outcomes() -> None:
    """Three dice: 6^3 = 216 equally likely ordered outcomes."""
    assert counting.total_outcomes(3) == 216
    assert counting.total_outcomes(1) == 6
    assert len(counting.sample_space(3)) == 216
    assert len(set(counting.sample_space(3))) == 216
    assert counting.sample_space(1)[0] == (1,)
    assert counting.sample_space(2)[-1] == (6, 6)


def test_sum_at_least_17_is_one_over_54() -> None:
    """P(sum >= 17) = 4/216 = 1/54, kept exact as a Fraction."""
    assert counting.sum_at_least(17, 3) == Fraction(4, 216) == Fraction(1, 54)
    assert counting.exact_probability(lambda o: counting.dice_sum(o) >= 17, 3) == Fraction(1, 54)
    assert counting.count_where(lambda o: counting.dice_sum(o) >= 17, 3) == 4


def test_sum_distribution_counts() -> None:
    """The 216 outcomes split into the textbook sum counts; 17 and 18 are rare."""
    distribution = counting.sum_distribution(3)
    assert sum(distribution.values()) == 216
    assert distribution[17] == 3
    assert distribution[18] == 1
    assert distribution[3] == 1 and distribution[10] == 27 and distribution[11] == 27
    assert sorted(distribution) == list(range(3, 19))


def test_simulate_sum_at_least_agrees_with_the_count() -> None:
    """The seeded dice simulation reproduces P(sum >= 17) = 1/54 within 2 standard errors."""
    result = counting.simulate_sum_at_least(17, n_rolls=50_000, n_dice=3, seed=SEED)
    assert result["agrees"] is True
    assert result["exact"] == pytest.approx(float(Fraction(1, 54)))
    assert result["estimate"] == pytest.approx(result["hits"] / result["trials"])
    assert abs(result["estimate"] - float(Fraction(1, 54))) <= 3 * simulate.standard_error(float(Fraction(1, 54)), 50_000)

    default = counting.simulate_sum_at_least(17, 50_000, 3, simulate.DEFAULT_SEED)
    assert default["agrees"] is True


def test_sum_distribution_table_lines_up_with_the_simulation() -> None:
    rows = counting.sum_distribution_table(3, n_rolls=20_000, seed=SEED)
    assert [row["sum"] for row in rows] == list(range(3, 19))
    assert sum(row["count"] for row in rows) == 216
    for row in rows:
        assert row["exact"] == pytest.approx(row["count"] / 216)
        assert abs(row["simulated"] - row["exact"]) <= 0.005
    seventeen = next(row for row in rows if row["sum"] == 17)
    assert seventeen["count"] == 3
    assert seventeen["exact"] == pytest.approx(3 / 216)  # P(sum == 17), not P(sum >= 17) = 1/54


def test_union_two_and_union_three_by_hand() -> None:
    """P(A u B) = 0.5 + 0.4 - 0.2 and the three-event alternating sum."""
    assert counting.union_two(0.5, 0.4, 0.2) == pytest.approx(0.7)
    assert counting.union_two(0.3, 0.6, 0.3 * 0.6) == pytest.approx(0.72)  # independent events
    assert counting.union_three(0.5, 0.4, 0.3, 0.2, 0.15, 0.12, 0.05) == pytest.approx(
        0.5 + 0.4 + 0.3 - 0.2 - 0.15 - 0.12 + 0.05
    )
    assert counting.union_inclusion_exclusion([[0.1, 0.2], [0.05]]) == pytest.approx(0.1 + 0.2 - 0.05)


def test_at_least_one_six_equals_91_over_216() -> None:
    """Three independent sixes: inclusion-exclusion collapses to 1 - (5/6)^3."""
    assert counting.at_least_one_six(3) == pytest.approx(91 / 216)
    assert counting.at_least_one_six(3) == pytest.approx(1.0 - (5.0 / 6.0) ** 3)
    assert counting.at_least_one_six(1) == pytest.approx(1 / 6)
    assert counting.at_least_one_six(2) == pytest.approx(1.0 - (5.0 / 6.0) ** 2)


def test_at_least_one_complement_shortcut() -> None:
    assert counting.at_least_one(1 / 6, 3) == pytest.approx(91 / 216)
    assert counting.at_least_one(0.5, 10) == pytest.approx(1.0 - 0.5**10)
    assert counting.at_least_one(0.9, 1) == pytest.approx(0.9)


def test_combinations_and_permutations() -> None:
    assert counting.combinations_count(10, 3) == 120
    assert counting.permutations_count(10, 3) == 720
    assert counting.combinations_count(10, 0) == 1
    assert counting.combinations_count(10, 10) == 1
    assert counting.permutations_count(6, 6) == math.factorial(6)
    assert counting.permutations_count(6, 3) == counting.combinations_count(6, 3) * math.factorial(3)


@pytest.mark.parametrize("n_people", [1, 2, 10, 23, 50, 100])
def test_birthday_probability_matches_the_independent_product(n_people: int) -> None:
    """Cross-check the ordered counting formula against 1 - prod (365 - i) / 365."""
    independent = 1.0 - math.prod((365 - i) / 365 for i in range(n_people))
    assert counting.birthday_collision_probability(n_people) == pytest.approx(independent, rel=1e-12)
    assert counting.birthday_collision_probability(n_people) == pytest.approx(
        1.0 - counting.permutations_count(365, n_people) / 365**n_people, rel=1e-12
    )


def test_birthday_probability_is_certain_past_365_people() -> None:
    assert counting.birthday_collision_probability(366) == 1.0
    assert counting.birthday_collision_probability(400) == 1.0
    assert counting.birthday_collision_probability(23) == pytest.approx(0.5072972343239854, rel=1e-12)


def test_birthday_probability_respects_the_number_of_days() -> None:
    assert counting.birthday_collision_probability(1, n_days=30) == 0.0
    assert counting.birthday_collision_probability(31, n_days=30) == 1.0
    assert counting.birthday_collision_probability(2, n_days=30) == pytest.approx(1 / 30)
