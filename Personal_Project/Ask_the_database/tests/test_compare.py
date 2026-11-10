import pytest

from src.compare import results_match


def match(expected, actual, ordered=False):
    return results_match(expected, actual, ordered)[0]


def test_exact_match():
    assert match([("Singapore",)], [("Singapore",)])


def test_extra_columns_are_allowed():
    assert match([("Harbour Grill",)], [("Harbour Grill", 39317.81)])


def test_missing_columns_fail():
    assert not match([("Harbour Grill", 39317.81)], [("Harbour Grill",)])


def test_column_order_and_names_do_not_matter():
    assert match([("Gold", 76)], [(76, "Gold")])


def test_wrong_value_fails():
    assert not match([("Harbour Grill",)], [("Tide & Tonic",)])


def test_row_count_must_match():
    ok, reason = results_match([(1,), (2,)], [(1,)])
    assert not ok and "1 rows" in reason


def test_unordered_ignores_row_order():
    assert match([("a", 1), ("b", 2)], [("b", 2), ("a", 1)])


def test_ordered_checks_row_order():
    assert not match([("a", 1), ("b", 2)], [("b", 2), ("a", 1)], ordered=True)
    assert match([("a", 1), ("b", 2)], [("a", 1), ("b", 2)], ordered=True)


def test_rows_must_be_paired_correctly():
    # same columns individually, but the pairing is wrong
    assert not match([("a", 1), ("b", 2)], [("a", 2), ("b", 1)])


@pytest.mark.parametrize("actual", [34.52, 34.5, 35, 35.0, 34.520000001])
def test_float_rounding_is_accepted(actual):
    assert match([(34.52,)], [(actual,)])


@pytest.mark.parametrize("actual", [34, 34.4, 36, 34.6])
def test_wrong_float_is_rejected(actual):
    assert not match([(34.52,)], [(actual,)])


def test_integer_division_bug_is_caught():
    # expected 35.47 percent; an integer-division bug returns 35 or 0 -> 35 would
    # be accepted as rounding, 0 must not be
    assert not match([(35.47,)], [(0,)])


def test_integers_must_match_exactly():
    assert match([(241,)], [(241,)])
    assert match([(241,)], [(241.0,)])
    assert not match([(241,)], [(241.4,)])
    assert not match([(241,)], [(240,)])


def test_none_only_matches_none():
    assert match([(None,)], [(None,)])
    assert not match([(None,)], [(0,)])


def test_strings_are_not_numbers():
    assert not match([("2026-03",)], [(3,)])


def test_both_empty_is_a_match():
    assert match([], [])
