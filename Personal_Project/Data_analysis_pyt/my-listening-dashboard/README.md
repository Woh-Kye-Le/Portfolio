# my-listening-dashboard

A small **pandas + Streamlit** app that answers a few questions about my own
listening habits, using an export I already had lying around (Spotify
*Extended streaming history*).

Instead of a notebook that runs once and rots, this is a tiny project: the data
wrangling lives in a tested `src/` package, the UI is a thin Streamlit layer on
top of it, and a `Makefile` sets everything up in one command.

## The questions it answers

| Question | Where it comes from |
| --- | --- |
| **What time of day do I listen the most?** | `analysis.listening_by_hour` + `charts.hourly_bar` |
| **Which days do I listen the most?** | `analysis.listening_by_weekday` + `charts.weekday_bar` |
| **How has my listening shifted month to month?** | `analysis.listening_by_month` + `charts.monthly_line` |
| **What do I replay most?** (tracks and artists) | `analysis.top_tracks` / `top_artists` + `charts.top_bar` |

## Screenshots

> _TODO: run `make run`, take a screenshot of the app, drop it in `docs/`, and
> reference it here — e.g._
> `![Dashboard overview](docs/overview.png)`

## Quickstart

```powershell
nmake all       # create .venv and install dependencies
nmake run       # launch the Streamlit app
nmake test      # run the test suite
nmake clean     # delete .venv and the Python caches
```

The app opens with the **bundled sample dataset**, so it works before you have
any data of your own.

If you have GNU Make on your `PATH` (Git for Windows, MSYS2, Chocolatey), the
same targets are available as `make all`, `make run`, `make test`, and
`make clean`.

Prefer to do it by hand?

```powershell
python -m venv .venv
.venv\Scripts\python.exe -m pip install -r requirements.txt   # Windows
# .venv/bin/python -m pip install -r requirements.txt         # macOS / Linux
.venv\Scripts\python.exe -m streamlit run app.py
```

### If `python` is not found

On Windows, a bare `python` sometimes hits the Microsoft Store stub instead of a
real interpreter. Point the `Makefile` at the one you want:

```powershell
nmake PYTHON=py all
make PYTHON=py all
make PYTHON=C:/Python312/python.exe all
```

In Visual Studio, the *Python Environments* window (**View → Other Windows →
Python Environments**) tells you the exact path of the interpreter the Python
workload installed.

## Running from Visual Studio

Open the folder directly in Visual Studio:

1. **File → Open → Folder** and select `my-listening-dashboard`.
2. Press `Ctrl+Shift+B` to run the configured **build** task (`nmake all`).
3. In Solution Explorer, right-click and use:
   - `Run Streamlit app` to launch the dashboard
   - `Run pytest suite` to run tests
4. `F5` uses the Visual Studio launch configuration and starts the app through
   [run_streamlit.py](file:///c:/Users/User/source/repos/Personal/Data_analysis_pyt/my-listening-dashboard/run_streamlit.py),
   which in turn runs `streamlit run app.py` inside `.venv`.

The Visual Studio-specific files are:

| File | Purpose |
| --- | --- |
| [tasks.vs.json](file:///c:/Users/User/source/repos/Personal/Data_analysis_pyt/my-listening-dashboard/tasks.vs.json) | Build / rebuild / clean / run / test tasks for Open Folder |
| [launch.vs.json](file:///c:/Users/User/source/repos/Personal/Data_analysis_pyt/my-listening-dashboard/launch.vs.json) | `F5` startup target |
| [run_streamlit.py](file:///c:/Users/User/source/repos/Personal/Data_analysis_pyt/my-listening-dashboard/run_streamlit.py) | Python launcher used by `F5` |
| [run.cmd](file:///c:/Users/User/source/repos/Personal/Data_analysis_pyt/my-listening-dashboard/run.cmd) | Simple Windows wrapper that still works from a shell |

On macOS/Linux, override the interpreter path when using GNU Make:
`make VENV_PY=.venv/bin/python all`.

## Using your own export

1. Request your data from Spotify (**Account → Privacy → Download your data**)
   and pick *Extended streaming history*. You get
   `Streaming_History_Audio_*.json` files.
2. Drop them into `data/raw/`. **This folder is gitignored on purpose — your
   listening history is personal and should never be committed.**
3. In the app sidebar, switch the data source to *My export (data/raw)*.

Canonical CSV files also work. Either shape is accepted, and both are
concatenated if you throw in several:

```text
ts,track,artist,album,ms_played
2024-01-05T10:45:00Z,Blue Hour,Faye Vector,Vector Fields,214000
```

| Column | Meaning |
| --- | --- |
| `ts` | ISO-8601 timestamp, UTC (Spotify's `ts`) |
| `track` | track name (Spotify's `master_metadata_track_name`) |
| `artist` | album artist (Spotify's `master_metadata_album_artist_name`) |
| `album` | album name (Spotify's `master_metadata_album_album_name`) |
| `ms_played` | play length in milliseconds; `0` marks a skip |

Not a Spotify user? Map whatever export you have onto that schema (Netflix
viewing history, YouTube watch history, a bank CSV…) and everything downstream
keeps working.

## Project layout

```
my-listening-dashboard/
├── app.py                  # Streamlit entry point (the UI)
├── requirements.txt        # pandas, streamlit, plotly, pytest
├── README.md
├── Makefile                # setup / run / test / clean (GNU Make + NMake)
├── run.cmd                 # simple Windows launcher for make / nmake
├── run_streamlit.py        # Visual Studio-friendly Python launcher for F5
├── launch.vs.json          # Visual Studio startup config
├── tasks.vs.json           # Visual Studio Open Folder tasks
├── .gitignore              # ignores raw data
├── data/
│   ├── raw/                # your original export (NOT committed)
│   └── sample/             # small anonymised sample (committed, for demo)
├── src/
│   ├── __init__.py
│   ├── load.py             # read the raw files into a DataFrame
│   ├── clean.py            # types, timestamps, drop junk, rename columns
│   ├── analysis.py         # functions that answer each question
│   └── charts.py           # functions that turn results into Plotly charts
├── notebooks/
│   └── exploration.ipynb   # messy first look at the data
└── tests/
    └── test_clean.py       # tests for cleaning and analysis
```

## How cleaning works

`src/clean.py` is the only place that knows about messy input:

* column names are lower-cased and stripped
* `ts` is parsed as UTC and converted to a local timezone (sidebar override),
  so "evening" means *your* evening
* blank / missing track, artist and album names become `"Unknown"`
* rows with no timestamp or `ms_played <= 0` are dropped
* exact replays of the same track at the same instant are de-duplicated
* derived columns are added: `minutes`, `date`, `hour`, `weekday`, `month`

Everything downstream (`analysis.py`, `charts.py`, `app.py`) only ever sees the
clean, canonical frame.

## Tests

```powershell
nmake test
```

The suite covers the cleaning rules (timezone conversion, dropping junk,
de-duplication, blank names) and the analysis helpers (hourly buckets, monthly
ordering, replay counts), plus a smoke test over the committed sample. GNU Make
users can run `make test` instead.
