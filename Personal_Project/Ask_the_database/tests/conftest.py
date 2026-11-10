import pytest

from db.seed import build_database


@pytest.fixture(scope="session")
def db_path(tmp_path_factory):
    """A freshly seeded database, built once per test session."""
    path = tmp_path_factory.mktemp("db") / "test.db"
    build_database(path)
    return path
