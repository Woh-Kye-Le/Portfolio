"""Generate the synthetic resort database (fake data only).

Usage:
    python -m db.seed              # builds db/resort.db
    python -m db.seed --clean      # same, but without the deliberate data defects

The generator is deterministic (fixed random seed), so everyone gets the same
data. A handful of defects are injected ON PURPOSE (duplicates, orphan IDs,
negative amounts) so that src/data_checks.py has something real to find.
"""
from __future__ import annotations

import argparse
import random
import sqlite3
from datetime import date, timedelta
from pathlib import Path

from src.config import DATA_END, DATA_START, DEFAULT_DB_PATH, SCHEMA_PATH, SEED

N_GUESTS = 500
N_BOOKINGS = 3000
N_ORDERS = 12000
N_SALES = 6000

COUNTRIES = [
    ("Singapore", 25), ("China", 18), ("Indonesia", 12), ("Malaysia", 10),
    ("India", 9), ("Australia", 6), ("Japan", 5), ("United Kingdom", 5),
    ("United States", 5), ("South Korea", 3), ("Philippines", 2),
]
TIERS = [("Member", 55), ("Silver", 25), ("Gold", 15), ("Platinum", 5)]

FIRST_NAMES = [
    "Aiden", "Aisha", "Alicia", "Amir", "Anya", "Arjun", "Bella", "Benjamin",
    "Chloe", "Daniel", "Elena", "Ethan", "Farah", "Gabriel", "Hana", "Isabelle",
    "Ivan", "Jasmine", "Kenji", "Lara", "Liam", "Mei", "Nadia", "Noah",
    "Olivia", "Priya", "Rahul", "Sofia", "Takumi", "Wei", "Yuna", "Zara",
    "Marcus", "Nina", "Omar", "Rachel", "Samuel", "Tessa", "Victor", "Yusuf",
]
LAST_NAMES = [
    "Tan", "Lim", "Lee", "Ng", "Wong", "Chen", "Goh", "Teo", "Koh", "Ong",
    "Patel", "Singh", "Kumar", "Sharma", "Nair", "Rahman", "Hassan", "Ibrahim",
    "Santos", "Reyes", "Cruz", "Smith", "Brown", "Taylor", "Wilson", "Clark",
    "Walker", "Young", "Kim", "Park", "Choi", "Tanaka", "Sato", "Suzuki",
    "Yamamoto", "Nguyen", "Wijaya", "Santoso", "Hartono", "Lopez",
]

# name, cuisine, location, seats, avg spend per head (SGD), popularity weight
RESTAURANTS = [
    ("Harbour Grill", "Steakhouse", "Level 1", 120, 80, 12),
    ("Lotus Court", "Chinese", "Level 2", 160, 55, 12),
    ("Saffron Table", "Indian", "Level 1", 90, 40, 10),
    ("Tide & Tonic", "Seafood", "Waterfront", 110, 72, 13),
    ("Sakura Counter", "Japanese", "Level 2", 40, 65, 10),
    ("Nonna's Oven", "Italian", "Level 1", 100, 45, 11),
    ("Garden Kopi", "Cafe", "Lobby", 80, 18, 20),
    ("Skyline Lounge", "Bar & Grill", "Rooftop", 70, 50, 12),
]

ROOM_TYPES = [  # type, base nightly rate, popularity weight
    ("Deluxe", 420, 34), ("Premier", 520, 30), ("Club", 680, 20), ("Suite", 1100, 16),
]

STORES = {  # store -> the categories it sells
    "Harbour Boutique": ["Fashion", "Cosmetics", "Souvenirs"],
    "Gem & Gold": ["Jewellery", "Fashion", "Cosmetics"],
    "Tech Corner": ["Electronics", "Souvenirs", "Fashion"],
    "Beauty Bar": ["Cosmetics", "Fashion", "Souvenirs"],
    "Lion City Gifts": ["Souvenirs", "Fashion", "Cosmetics"],
    "Duty Free Hub": ["Electronics", "Jewellery", "Cosmetics"],
}
CATEGORY_PRICE = {"Fashion": 120, "Jewellery": 600, "Electronics": 350,
                  "Cosmetics": 60, "Souvenirs": 25}

EVENT_THEMES = [
    "Lantern", "Aurora", "Harbour", "Silk Road", "Neon", "Monsoon", "Golden Hour",
    "Tidal", "Starlight", "Orchid", "Skyline", "Coral", "Midnight", "Jade", "Horizon",
]
EVENT_TYPES = {  # type -> (capacity, base ticket price, possible venues)
    "Concert": (6000, 150, ["Harbour Arena", "Grand Ballroom"]),
    "Conference": (800, 400, ["Expo Hall A", "Expo Hall B", "Grand Ballroom"]),
    "Exhibition": (1500, 30, ["Expo Hall A", "Expo Hall B"]),
    "Gala": (400, 600, ["Grand Ballroom", "Sky Terrace"]),
}


def daterange(start: date, end: date) -> list[date]:
    return [start + timedelta(days=i) for i in range((end - start).days + 1)]


def day_weights(days: list[date]) -> list[float]:
    """Busier Fri-Sun and in the June/December holiday months, quieter Feb/Sep."""
    weights = []
    for d in days:
        w = 1.0
        if d.weekday() >= 4:
            w *= 1.35
        if d.month in (12, 6):
            w *= 1.25
        if d.month in (2, 9):
            w *= 0.85
        weights.append(w)
    return weights


def make_guests(rng: random.Random) -> list[tuple]:
    combos = [(f, l) for f in FIRST_NAMES for l in LAST_NAMES]
    names = rng.sample(combos, N_GUESTS)  # unique full names
    countries, c_weights = zip(*COUNTRIES)
    tiers, t_weights = zip(*TIERS)
    rows = []
    for gid, (first, last) in enumerate(names, start=1):
        joined = date(2022, 1, 1) + timedelta(days=rng.randint(0, (date(2025, 6, 30) - date(2022, 1, 1)).days))
        rows.append((
            gid, f"{first} {last}", f"{first}.{last}{gid}@example.com".lower(),
            rng.choices(countries, c_weights)[0], rng.choices(tiers, t_weights)[0],
            joined.isoformat(),
        ))
    return rows


def pick_guest(rng: random.Random) -> int:
    """Skewed pick so some guests are much more active than others."""
    return int((rng.random() ** 1.8) * N_GUESTS) + 1


def make_bookings(rng: random.Random, days: list[date], dw: list[float]) -> list[tuple]:
    types, rates, weights = zip(*ROOM_TYPES)
    drafts = []
    for check_in in sorted(rng.choices(days, dw, k=N_BOOKINGS)):
        nights = rng.choices([1, 2, 3, 4, 5, 6], [15, 35, 25, 12, 8, 5])[0]
        nights = max(1, min(nights, (DATA_END - check_in).days))
        idx = rng.choices(range(len(types)), weights)[0]
        rate = round(rates[idx] * rng.uniform(0.85, 1.15), 2)
        status = rng.choices(["completed", "cancelled", "no_show"], [88, 8, 4])[0]
        drafts.append((pick_guest(rng), types[idx], check_in, nights, rate, status))
    rows = []
    for bid, (gid, room, check_in, nights, rate, status) in enumerate(drafts, start=1):
        check_out = check_in + timedelta(days=nights)
        rows.append((bid, gid, room, check_in.isoformat(), check_out.isoformat(),
                     rate, round(nights * rate, 2), status))
    return rows


def make_restaurants() -> list[tuple]:
    return [(i, n, c, loc, seats) for i, (n, c, loc, seats, _, _) in enumerate(RESTAURANTS, start=1)]


def make_orders(rng: random.Random, days: list[date], dw: list[float]) -> list[list]:
    # Each restaurant's popularity wobbles month to month, so the "top
    # restaurant" is not the same every month.
    months = sorted({d.strftime("%Y-%m") for d in days})
    factor = {(i, m): rng.uniform(0.85, 1.2) for i in range(len(RESTAURANTS)) for m in months}
    rows = []
    for oid, d in enumerate(sorted(rng.choices(days, dw, k=N_ORDERS)), start=1):
        ym = d.strftime("%Y-%m")
        w = [RESTAURANTS[i][5] * factor[(i, ym)] for i in range(len(RESTAURANTS))]
        idx = rng.choices(range(len(RESTAURANTS)), w)[0]
        party = rng.choices([1, 2, 3, 4, 5, 6], [10, 35, 20, 20, 10, 5])[0]
        amount = round(party * RESTAURANTS[idx][4] * rng.uniform(0.7, 1.4), 2)
        guest = pick_guest(rng) if rng.random() < 0.65 else None   # ~35% walk-ins
        rows.append([oid, idx + 1, guest, d.isoformat(), party, amount])
    return rows


def make_sales(rng: random.Random, days: list[date], dw: list[float]) -> list[list]:
    stores = list(STORES)
    rows = []
    for sid, d in enumerate(sorted(rng.choices(days, dw, k=N_SALES)), start=1):
        store = rng.choice(stores)
        category = rng.choice(STORES[store])
        qty = rng.choices([1, 2, 3], [70, 22, 8])[0]
        amount = round(qty * CATEGORY_PRICE[category] * rng.uniform(0.6, 1.6), 2)
        guest = pick_guest(rng) if rng.random() < 0.30 else None
        rows.append([sid, store, category, d.isoformat(), qty, amount, guest])
    return rows


def make_events(rng: random.Random, days: list[date]) -> list[list]:
    combos = [(t, theme) for t in EVENT_TYPES for theme in EVENT_THEMES]  # 60 unique events
    rng.shuffle(combos)
    drafts = []
    for etype, theme in combos:
        capacity, price, venues = EVENT_TYPES[etype]
        drafts.append((rng.choice(days), f"{theme} {etype}", etype, rng.choice(venues),
                       capacity, int(capacity * rng.uniform(0.45, 1.0)),
                       round(price * rng.uniform(0.7, 1.3), 2)))
    return [[eid, name, etype, venue, d.isoformat(), cap, att, price]
            for eid, (d, name, etype, venue, cap, att, price) in enumerate(sorted(drafts), start=1)]


def inject_defects(rng, guests, orders, sales, events) -> dict[str, int]:
    """Plant known problems for the data-quality checks to find."""
    guests_l = [list(g) for g in guests]
    for i in rng.sample(range(len(guests_l)), 15):
        guests_l[i][2] = None                                   # missing emails
    next_id = max(o[0] for o in orders) + 1
    for i in rng.sample(range(len(orders)), 3):                 # duplicate bills
        dup = list(orders[i]); dup[0] = next_id; next_id += 1
        orders.append(dup)
    for n, i in enumerate(rng.sample(range(len(orders) - 3), 5), start=1):
        orders[i][2] = 9000 + n                                 # orphan guest IDs
    for i in rng.sample(range(len(sales)), 4):
        sales[i][5] = -abs(sales[i][5])                         # negative amounts
    for i in rng.sample(range(len(events)), 2):
        events[i][6] = None                                     # attendance not recorded
    guests[:] = [tuple(g) for g in guests_l]
    return {"null_emails": 15, "duplicate_orders": 3, "orphan_order_guest_ids": 5,
            "negative_retail_amounts": 4, "null_event_attendance": 2}


def build_database(path: Path | str = DEFAULT_DB_PATH, clean: bool = False, seed: int = SEED) -> dict:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        path.unlink()

    rng = random.Random(seed)
    days = daterange(DATA_START, DATA_END)
    dw = day_weights(days)

    guests = make_guests(rng)
    bookings = make_bookings(rng, days, dw)
    restaurants = make_restaurants()
    orders = make_orders(rng, days, dw)
    sales = make_sales(rng, days, dw)
    events = make_events(rng, days)
    defects = {} if clean else inject_defects(rng, guests, orders, sales, events)

    conn = sqlite3.connect(path)
    try:
        conn.executescript(Path(SCHEMA_PATH).read_text())
        conn.executemany("INSERT INTO guests VALUES (?,?,?,?,?,?)", guests)
        conn.executemany("INSERT INTO bookings VALUES (?,?,?,?,?,?,?,?)", bookings)
        conn.executemany("INSERT INTO restaurants VALUES (?,?,?,?,?)", restaurants)
        conn.executemany("INSERT INTO orders VALUES (?,?,?,?,?,?)", orders)
        conn.executemany("INSERT INTO retail_sales VALUES (?,?,?,?,?,?,?)", sales)
        conn.executemany("INSERT INTO events VALUES (?,?,?,?,?,?,?,?)", events)
        conn.commit()
        counts = {t: conn.execute(f"SELECT COUNT(*) FROM {t}").fetchone()[0]
                  for t in ("guests", "bookings", "restaurants", "orders", "retail_sales", "events")}
    finally:
        conn.close()
    return {"path": str(path), "rows": counts, "defects": defects}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--db", default=str(DEFAULT_DB_PATH), help="output path")
    parser.add_argument("--clean", action="store_true", help="skip the deliberate data defects")
    args = parser.parse_args()
    info = build_database(args.db, clean=args.clean)
    print(f"Created {info['path']}")
    for table, n in info["rows"].items():
        print(f"  {table:<13}{n:>7,}")
    if info["defects"]:
        print("Injected defects (so the data-quality checks have something to find):")
        for name, n in info["defects"].items():
            print(f"  {name:<26}{n}")


if __name__ == "__main__":
    main()
