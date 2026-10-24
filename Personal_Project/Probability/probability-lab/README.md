# probability-lab

**Exact answer first, simulation second.**

I built this while working through my probability course, mainly because I
wanted to stop taking results on faith. Every topic does the same job twice: the
answer comes out of the formula, then the same quantity gets estimated by
simulating it in NumPy, and the two are compared in units of the simulation's
own noise.

That comparison is the part I care about. A simulation never lands exactly on
the formula, so "they agree" has to mean something concrete. Here it means the
estimate is within **2 standard errors** of the exact value, using the standard
error that belongs to the estimator doing the work:

| Quantity | Standard error used |
| --- | --- |
| A relative frequency (a proportion) | `sqrt(p(1-p)/n)` |
| A sample mean | `s / sqrt(n)` |

Every comparison hands back the exact value, the simulated value, the tolerance
and the z-score, so you can see *how* close it landed instead of just being told
it was close. Week 1 of the course already does this with the dice exercise
(`P(sum >= 17) = 1/54 ~ 1.85%`, then a simulated estimate on top), so the rest of
this lab is just that same idea applied to the other five topics.

## What's in it

| Module | Week | What it does |
| --- | --- | --- |
| [counting.py](src/counting.py) | 1 | Enumerates three dice with `itertools` (216 outcomes) and checks `P(sum >= 17)` against a simulation. Also inclusion–exclusion for `P(at least one six) = 91/216`, the complement trick `1-(5/6)^n`, combinations/permutations, and the birthday problem |
| [bayes.py](src/bayes.py) | 2 | Medical screening, a spam filter and fraud detection. Each posterior comes three ways: the formula, the lecture's "natural frequencies" (out of 10,000 people), and simulating 10,000 people and counting. Plus the two-positive-tests case from the midterm review |
| [discrete.py](src/discrete.py) | 3 | Binomial, Geometric and Poisson PMFs and CDFs written from scratch (no `scipy`). Memorylessness of the Geometric, next to a counterexample that is *not* memoryless. The Poisson approximation to the Binomial as `n` grows, including the 1,000-component defects example |
| [continuous.py](src/continuous.py) | 4 | PDFs integrated numerically to check they come to 1 (trapezium and Simpson), CDFs from the formula against the numeric integral, Normal percentiles by standardising. Exponential waiting times simulated, then the counts per interval shown to look Poisson — the Poisson-process link |
| [moments.py](src/moments.py) | 5 | `E[X]` and `Var(X)` three ways: the definition, the shortcut `E[X^2] - E[X]^2`, and a simulated sample. Linearity of expectation with indicators (why a Binomial mean is `np`), and moments recovered by differentiating an MGF |
| [joint.py](src/joint.py) | 8 | Joint PMF table, marginals, conditionals, conditional expectation, covariance and correlation, and iterated expectation `E[E[Y|X]] = E[Y]`. Ends on the lecture's trap `Y = X^2`: zero correlation, still obviously dependent |

All the randomness goes through [simulate.py](src/simulate.py), which is the only
module that touches a random generator. Keep the seed and anything simulated here
can be reproduced exactly (see *Determinism* below).

## Quickstart

```powershell
nmake all       # create .venv and install dependencies
nmake run       # launch the Streamlit app
nmake test      # run the test suite
nmake clean     # delete .venv and the Python caches
```

If you have GNU Make on your `PATH` (Git for Windows, MSYS2, Chocolatey), then
`make all` / `make run` / `make test` / `make clean` do the same thing.

Prefer to do it by hand?

```powershell
py -m venv .venv
.venv\Scripts\python.exe -m pip install -r requirements.txt   # Windows
# .venv/bin/python -m pip install -r requirements.txt         # macOS / Linux
.venv\Scripts\python.exe -m streamlit run app.py
```

### If `python` is not found

A bare `python` on Windows sometimes hits the Microsoft Store stub instead of a
real interpreter, so the `Makefile` defaults to the `py` launcher. Point it
somewhere else if you need to:

```powershell
nmake PYTHON=python all                        # python.exe already on PATH
make VENV_PY=.venv/bin/python all              # POSIX venv layout
```

## Running from Visual Studio

Open the folder directly in Visual Studio:

1. **File → Open → Folder** and select `probability-lab`.
2. `Ctrl+Shift+B` runs the configured **build** task (`nmake all`).
3. In Solution Explorer, right-click and use `Streamlit lab` to launch the app,
   or `pytest suite` to run the tests.
4. `F5` uses the Visual Studio launch configuration and starts the app through
   [run_streamlit.py](run_streamlit.py), which runs `streamlit run app.py` inside
   `.venv` and opens `http://localhost:8501`.

The Visual Studio plumbing is:

| File | Purpose |
| --- | --- |
| [tasks.vs.json](tasks.vs.json) | Build / rebuild / clean / run / test tasks for Open Folder |
| [launch.vs.json](launch.vs.json) | `F5` startup target |
| [run_streamlit.py](run_streamlit.py) | Python launcher used by `F5` |
| [run.cmd](run.cmd) | Plain Windows wrapper, still works from a shell |

## Determinism

Everything simulated is seeded from one constant in [simulate.py](src/simulate.py):

```python
DEFAULT_SEED = 20260221
SIGMA = 2.0          # the agreement band, in standard errors
```

Pass `seed=None` to any simulation helper and you get fresh randomness instead.
Leave the default alone and the app, this README and the tests all quote the same
numbers.

## Tests

```powershell
nmake test
```

There are two kinds of assertion in the suite, and both have to hold:

* **formula vs theory** — the hand-written PMFs, PDFs, CDFs and quantiles are
  compared against `scipy.stats` (relative tolerance `1e-9`), and the moment
  formulas against their closed forms. `scipy` shows up *only* here, never in
  `src/`, because the point is to test my own implementations rather than the
  library's.
* **simulation vs formula** — a seeded simulation has to satisfy
  `comparison["agrees"] is True`, meaning it lands inside the 2-standard-error
  band.

So the suite is exact about formulas and statistical about simulations: it
asserts agreement, not equality.

## Project layout

```
probability-lab/
├── app.py                 # Streamlit, one tab per module
├── src/
│   ├── counting.py        # events, inclusion-exclusion, combinations
│   ├── bayes.py
│   ├── discrete.py        # Binomial, Geometric, Poisson from scratch
│   ├── continuous.py      # Uniform, Normal, Exponential, numeric integration
│   ├── moments.py         # E[X], Var(X), MGF
│   ├── joint.py           # joint PMF, marginals, covariance, correlation
│   └── simulate.py        # the only place randomness enters
├── tests/                 # exact vs simulated, and this code vs scipy.stats
├── requirements.txt       # numpy, matplotlib, streamlit
└── README.md
```

The house-keeping files alongside those are `Makefile`, `run.cmd`,
`run_streamlit.py`, `launch.vs.json` and `tasks.vs.json`, plus `conftest.py`
(which makes the project root importable so `from src.x import y` works under
pytest) and `.gitignore`.

## A worked example

`P(sum >= 17)` on three dice — the Week 1 exercise the rest of the lab is
modelled on:

```python
from src.counting import sum_at_least, simulate_sum_at_least

sum_at_least(17)              # Fraction(1, 54)
simulate_sum_at_least(17)     # exact, simulated, standard error, z-score, agrees
```

By hand, only `(6,6,5)`, `(6,5,6)`, `(5,6,6)` and `(6,6,6)` qualify, so the
probability is `4/216 = 1/54 ~ 0.0185`. Roll 50,000 times and you should land
within roughly `2 * sqrt(0.0185 * 0.9815 / 50000) ~ 0.0012` of it.
