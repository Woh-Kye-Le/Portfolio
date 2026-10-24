# Portfolio

A collection of my personal and DigiPen Institute of Technology coursework projects.
Each project below lists the languages used, the libraries/frameworks, my main roles, and a short description.

---

## Personal Projects

### My Listening Dashboard

| | |
|---|---|
| **Folder** | `Personal_Project/Data_analysis_pyt/my-listening-dashboard` |
| **Language** | Python |
| **Libraries** | pandas (data wrangling), Streamlit (UI), Plotly (charts), pytest (testing) |
| **My Role** | Solo project — data pipeline, analysis, and UI |

**About the project**
A small pandas + Streamlit app that answers questions about my own listening habits, built from a Spotify *Extended streaming history* export I already had. Instead of a one-off notebook, the data wrangling lives in a tested `src/` package, the UI is a thin Streamlit layer on top of it, and a `Makefile` sets everything up in one command.

It answers:
- **What time of day do I listen the most?**
- **Which days do I listen the most?**
- **How has my listening shifted month to month?**
- **What do I replay most?** (tracks and artists)

Cleaning is centralised in `src/clean.py` (timezone conversion, dropping junk rows, de-duplication, blank name handling, derived columns), so `analysis.py`, `charts.py`, and `app.py` only ever see a clean, canonical frame. The app opens with a bundled anonymous sample dataset out of the box, and supports any export mapped onto the same `ts, track, artist, album, ms_played` schema.

---

### Probability Lab — "Exact Answer First, Simulation Second"

| | |
|---|---|
| **Folder** | `Personal_Project/Probability/probability-lab` |
| **Language** | Python |
| **Libraries** | NumPy (Monte-Carlo simulation), Streamlit (UI), Matplotlib (plots); SciPy and pytest appear only in the tests, to check my own formulas |
| **My Role** | Solo project — formulas, simulations, test suite, and the Streamlit app |

**About the project**
A Streamlit lab for the probability course, with one tab per topic (counting, Bayes' rule, the Binomial/Geometric/Poisson triple, continuous distributions, expectation and variance, and joint distributions). Every topic does the same thing twice: compute the answer from the formula, estimate the same quantity with a seeded NumPy simulation, then compare the two **in units of the simulation's own noise** — an estimate counts as agreeing only if it lands within 2 standard errors of the exact value, so each comparison reports the exact value, the estimate, the tolerance and the z-score rather than a vague "close enough".

The PMFs, PDFs, CDFs and quantiles are written from scratch, so `scipy` stays out of `src/` and only shows up in the tests, where the lab's own implementations are checked against `scipy.stats` to `1e-9`. Randomness enters through a single seeded module, so the app, the README and the tests all quote the same numbers.

Highlights:
- **Week 1, done exactly** — three dice give 216 outcomes you can enumerate, so `P(sum >= 17) = 1/54` is a count, not an estimate. Everything else follows this worksheet.
- **Bayes' rule three ways** — the formula, the lecture's "natural frequencies", and simulating 10,000 people; a rare disease keeps a positive test unimpressive.
- **The Poisson approximation, watched** — fixing `lambda = np` and growing `n` from 5 to 1,000 (the 1,000-component defects example) shows the Binomial law sliding onto the Poisson.
- **The trap** — `Y = X²` with a symmetric `X`: covariance and correlation are exactly zero, yet the two variables are completely dependent.

---

### Assembly Benchmark — "Can I Beat the Compiler?"

| | |
|---|---|
| **Folder** | `Personal_Project/Assembly_benchmark` |
| **Language** | x86-64 Assembly (MASM) + C++17 |
| **Libraries** | None — MSVC toolchain (`ml64.exe` assembler), C++ built with `/O2 /GL /arch:AVX2` |
| **My Role** | Solo project — hand-written x64 assembly, C++ reference implementations, benchmark harness, correctness tests |

**About the project**
After learning the basics of x64 assembly, I hand-wrote five functions in MASM — `strlen`, summing an array, counting set bits (popcount), reversing an array, and converting a 1080p RGBA image to grayscale — and raced them against the optimized C++ versions to work out *why* each one went the way it did.

Results (Release / x64, `/O2 /arch:AVX2`):

| Benchmark | Winner | By how much |
|---|---|---|
| `strlen` | asm | 1.64x |
| `sum` (1M int64) | C++ | 3.30x |
| `popcount` | tie | 1.08x |
| `reverse` | tie | 1.04x |
| `grayscale` (1080p RGBA) | asm | 4.17x |

The takeaway: for a simple loop over memory the compiler already emits the vector code you would write by hand (it auto-vectorized `sum` into AVX2 and won 3.3x), but for awkward data — interleaved bytes, widening multiplies, packing — the auto-vectorizer often gives up, which is where hand-written SSE2 (`pmaddwd`, shuffles, packs) paid off for the grayscale pass. The project also covers the Microsoft x64 calling convention, the C++/assembly boundary, `extern "C"` linkage, and a correctness test suite that checks the assembly output matches C++.

---

## School Projects

### GAM100 — CaveBound

| | |
|---|---|
| **Language** | C |
| **Libraries** | CProcessing (DigiPen's functional game-development library, built on NanoVG for 2D rendering and FMOD for audio) |
| **My Role** | Physics & map generation |

**About the game**
CaveBound is a 2D platformer in which the player explores a mysterious cave and switches between two unique characters on the fly to solve puzzles, defeat enemies, and escape traps. Each character has distinct abilities, making strategic switching the key to progressing through the levels. The game features platforming physics, collectables, enemy and boss encounters, bullets, and a leaderboard.

---

### GAM200 — OceanBound

| | |
|---|---|
| **Language** | C++17 |
| **Libraries** | Alpha Engine (DigiPen's Alpha Engine — provides `AEEngine`, `AEGraphics`, `AEInput`, `AEMath`, `AEAudio` powered by FMOD, and FreeType for font rendering) |
| **My Role** | Physics, map generation & special abilities |

**About the game**
OceanBound is a 2D platformer set in an underwater-themed world. The player navigates 10 hand-crafted levels using movement, jumping, crouching, rope-swinging, and lever mechanics while managing an oxygen meter in water zones. Traps and enemies add challenge, and three special abilities are available as power-ups:

- **Shield** — temporary invulnerability
- **Speed Boost** — increased movement speed
- **Oxygen Replenishment** — restores oxygen for water navigation

The game also includes a health system, respawns, and a leaderboard that logs player times.

---

### GAM250 — Taken (Echoes of Arena)

| | |
|---|---|
| **Language** | C++20 (EngineCore) / C++17 (Game) |
| **Libraries** | Custom `EngineCore` + OpenGL (GLFW, GLAD), GLM (math), ImGui + ImGuizmo (editor UI), FMOD (audio), Lua (scripting), FreeType (fonts), nlohmann JSON (serialization), stb_image (texture loading) |
| **My Role** | Graphics & UI menu page |

**About the game**
Taken is a 2D top-down labyrinth action game built on a custom Entity-Component-System (ECS) engine. The player explores procedurally generated rooms connected by doors, fighting enemies and solving puzzles to unlock new areas.

Core gameplay features:
- **Player abilities** — Burrow (dig through soft walls / go underground), Heal (restore HP), and Projectile (ranged attack). Abilities are also used as keys to open level-locked doors.
- **Enemies** — basic, ranged, burrow, heal, and boss variants, with AI behaviour driven by Lua scripts.
- **World systems** — procedural map generation, door/room transitions, puzzle objects (breakable walls, levers), dynamic lighting, and particle effects.
- **Editor tools** — a full ImGui-based editor with a Room Editor, UI Editor, prefab system, scene save/load to JSON, and undo/redo.

The game runs on a fixed-timestep simulation with separate variable-rate rendering, and ships with an installer build.
