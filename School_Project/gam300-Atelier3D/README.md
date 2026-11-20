# Atelier3D

**A Scratch-inspired 3D programming and game development environment.**

Built by team **SayReal** (GAM300).

**Quick links:** [Quick Start](#quick-start) · [Running the Tests](#running-the-tests) · [Milestone 1 Deliverables and Jira](#milestone-1-deliverables-and-jira) · [Documentation](#documentation)

Atelier3D is a custom 3D engine and visual programming environment designed to help beginners learn how games and programming work by making code execution visible.

Inspired by the simplicity of Scratch, Atelier3D brings block-based programming into a real-time 3D environment. Users can create 3D scenes, attach logic to objects, run their programs and directly see how their blocks affect the world.

The core idea is:

**Code → Logic → Object → Execution → Result**

Rather than hiding program execution behind the engine, Atelier3D makes it visible.

---

## The problem

Learning programming can be difficult because program execution is mostly invisible.

A beginner may understand what a block says, but not necessarily understand:

- Which instruction is currently running
- Why a condition did or did not execute
- Which object is controlled by a piece of logic
- What caused a variable to change
- Why an object behaved differently from what they expected

Traditional block programming tools make syntax easier to learn, but the transition from blocks to traditional programming can still feel disconnected.

Professional game engines provide powerful 3D tools, but their complexity can make them difficult for beginners to understand.

Atelier3D sits between these two approaches.

It combines:

- A lightweight 3D editor
- Scratch-inspired visual programming
- A custom runtime
- Live execution visualisation
- Beginner-oriented debugging
- A gradual path towards C++

The goal is not to compete with professional game engines in rendering quality or production features. Atelier3D is designed as a learning environment that happens to produce small playable 3D games.

---

## Who it is for

### Primary users: secondary school students (13–17)

| Group | Who they are | What they need |
|---|---|---|
| **1** | Students interested in games, technology, digital art or creative projects, with little or no knowledge of C++ or traditional programming syntax. Curious about how games work and willing to experiment, often by trial and error. | Visual programming, understandable terminology, beginner-friendly tutorials, and seeing immediately how their code affects the 3D environment. |
| **2** | Students with varied academic backgrounds and limited formal programming experience, perhaps with some exposure to games, Scratch, Roblox or other creative software. They learn through experimentation and visual feedback, prefer hands-on activities to long explanations, and get frustrated when outcomes are unclear. | Simple and approachable tools, clear visual feedback, immediate cause and effect, guided learning objectives, error recovery, and room to experiment safely. |

### Secondary users: parents, guardians and educators

| Group | Who they are | What they need |
|---|---|---|
| **Parents / guardians (35–60)** | Encourage and support a teenager's learning, but may have limited programming knowledge themselves. | Simple onboarding, understandable terminology, safe experimentation, and clear indicators of learning progress. |
| **Teachers / educators** | Teach programming concepts to students. | A controlled 3D environment with structured lessons (Teaching Mode). |

**Target persona:** Ethan, 16, a Secondary 4 student in Singapore and an "explorer". He enjoys games and experimenting with technology, and uses Scratch, Construct 3 and Minecraft, but has little experience with traditional programming or 3D game engines. Programming syntax confuses him, errors are hard to understand, and he cannot tell which part of the code caused an action. He wants to learn basic programming logic by block-programming a simple game, with immediate visual feedback and clear explanations of errors.

### User stories

1. **Learning programming.** As a secondary school student with little programming experience, I want to use visual blocks to control objects in a 3D environment, so that I can understand how programming logic affects the game world.
2. **Understanding and debugging.** As a beginner learning programming, I want to see which blocks are currently executing and how they change the 3D scene, so that I can identify what my code is doing and understand why something happened.

---

## How it works

1. **Create the scene:** Build a 3D environment by creating objects, positioning them and editing their properties.

2. **Add logic:** Attach visual blocks to objects and build behaviour using events, variables, conditions and loops.

3. **Run:** Start the game directly inside the editor.

4. **See execution:** The currently executing block is highlighted while the corresponding object changes in the 3D world.

5. **Debug:** Pause execution, inspect variables and step through the program to understand what is happening.

6. **Learn:** Use structured lessons in Teaching Mode or freely experiment in Sandbox Mode.

The learner should always be able to answer three questions without leaving the 3D view:

- **What is controlling this object?**
- **What is happening right now?**
- **What changed?**

### Core user journeys

**Guided learning (Teaching Mode):**

```text
Open Atelier3D → Select Teaching / Tutorial Mode → Choose learning objective → Read / view instructions
→ Create or open 3D scene → Add / select object → Create block logic → Run program
→ Observe blocks executing + 3D result → Identify and fix errors if needed → Automatic objective check
→ Complete objective / continue to next lesson
```

**Create and experiment (Sandbox Mode):**

```text
Open Atelier3D → Select Sandbox Mode → Create new project → Build 3D scene → Add and modify objects
→ Attach block logic → Run program → Observe real-time behaviour → Modify blocks / variables
→ Run and test again → Save / export project
```

---

## See the code. See the logic. See the result.

The central feature of Atelier3D is the connection between visual programming and the 3D world.

For example:

```text
[ When W is held ]
        |
        v
[ If Player is on ground ]
        |
        v
[ Move Player forward ]
        |
        v
[ Play footstep sound ]
```

When the scene is running, Atelier3D can show the execution state:

```text
[ When W is held ]          Waiting
        |
[ If Player is on ground ]  Completed
        |
[ Move Player forward ]     EXECUTING
        |
[ Play footstep sound ]     Waiting
```

At the same time, the Player object visibly moves in the 3D viewport.

This creates a direct relationship between:

```text
Input
  ↓
Event
  ↓
Block
  ↓
Object State
  ↓
3D Result
```

---

## Core Features

The application has an editor and a runtime that run together: edits made in the editor are reflected directly in the runtime, showing changes, effects and real-time updates.

Features are labelled by intended release, as in the [Product Design Document](docs/design/product-design.md): **MVP** is the first working prototype, **V2** is the second iteration, and **Future** is work that is realistic only beyond the initial project.

| Feature | Description | Release |
|---|---|---|
| 3D scene editor | Create, select, transform, duplicate and delete objects; lighting; camera; materials; collision volumes; simple physics | MVP |
| Block programming | Drag-and-drop blocks grouped into events, movement, variables, conditions, loops, objects, audio, visuals and functions | MVP |
| Real-time execution | Run and stop the scene inside the editor without a build step | MVP |
| Execution highlighting | The block being executed is highlighted while the game runs | MVP |
| In-world debugger | A floating panel beside the selected object showing its variables, current event and current block | MVP |
| Step debugging | Pause, resume, step one block at a time, and set breakpoints on blocks | MVP |
| Object inspector | Transform, model, material, physics, collision, audio, variables and attached logic for the selected object | MVP |
| Beginner error messages | Plain-language explanations linked to the block and object that caused the problem | MVP |
| Asset import | OBJ and FBX models, PNG and JPG textures, WAV and OGG audio | MVP |
| Teaching Mode | Prepared scenes with an objective and an automatic check, introduced in a fixed order | MVP |
| Sandbox Mode | An empty project with full editor access | MVP |
| Game export | Package a project into a standalone executable for the platform the editor runs on (Windows or Linux) | MVP |
| C++ view | Read-only generated C++ for the learner's block logic, kept in sync with the blocks; the learner can swap the block editor between the block view and the C++ view of the same logic | V2 |
| Reusable logic components | Save a block group as a named component and attach it to several objects, or share it between levels | V2 |
| Logic visualiser | Shows all the logic attached to the selected object | After MVP (release not set) |
| Level visualiser | An overview of a game's levels | After MVP (release not set) |
| Logic dependency map | A generated graph of objects, events, variables and functions, with click-through navigation | V2 |
| Execution history | A recorded timeline of variable changes and events for the last run | V2 |
| Hint system | Progressive hints attached to Teaching Mode challenges | V2 |
| User-authored C++ | Writing behaviour components in C++ against a documented engine API | V2 / Future |
| Custom shaders and materials | Importing or editing shader code for advanced users | Future |
| Additional export targets | Android and web, and cross-platform builds (building a game for a different operating system than the editor runs on) | Future |
| Classroom features | Assignment distribution, submission and review | Future |
| Learning analytics | Progress and error pattern reporting | Future |
| AI tutor | Optional assistant that explains errors and gives hints rather than answers | Future |

What is built so far (the engine and editor foundations of Milestone 1) is listed under [Milestone 1 Deliverables and Jira](#milestone-1-deliverables-and-jira).

### 3D Scene Editor

Create and edit interactive 3D scenes directly inside Atelier3D.

- Create, select, duplicate and delete objects
- Translate, rotate and scale objects with interactive gizmos
- Camera controls
- Lighting
- Materials
- Collision volumes
- Simple physics
- Object hierarchy
- Object inspector: transform, model, material, physics, collision, audio, variables and attached logic
- Real-time 3D viewport

---

### Visual Block Programming

Build behaviour using drag-and-drop blocks instead of writing code.

The block system covers core programming concepts including:

- Events
- Movement
- Variables
- Conditions
- Loops
- Functions
- Objects and object lifecycle
- Audio
- Visuals

Example:

```text
WHEN KEY W IS PRESSED
        |
        v
IF PLAYER IS ON GROUND
        |
        v
MOVE PLAYER FORWARD
```

Blocks are associated with objects so that users can understand which logic controls an object. A planned logic visualiser will show all the logic attached to a selected object, and reusable logic components will let the same logic be shared between multiple levels.

---

### Live Execution Visualisation

While a project is running, Atelier3D can communicate runtime information back to the editor.

This allows the editor to show:

- Currently executing block
- Waiting blocks
- Runtime events
- Variable values
- Object state
- Runtime errors
- Execution progress

The purpose is to make program execution visible rather than treating the program as a black box.

---

### In-World Debugger

Atelier3D provides debugging information directly alongside the 3D world.

A watched object can expose information such as:

```text
Player
--------------------
Position
X: 10.0
Y: 0.0
Z: 5.0

Health: 80
Speed: 5.0

State: Running
Event: W Held
Current Block:
Move Player Forward
```

This allows learners to connect a variable or execution state with the object they can see in the scene.

---

### Step Debugging

Users can pause a running project and inspect execution one block at a time.

Debugging features in the MVP:

- Pause and resume
- Step one block at a time
- Breakpoints on blocks
- Variable and runtime event inspection
- Beginner error messages: plain-language explanations linked to the block and object that caused the problem

Instead of debugging low-level engine instructions, Atelier3D focuses on meaningful block-level execution.

---

## Teaching Mode

Teaching Mode provides structured programming challenges.

A lesson can contain:

1. A prepared 3D scene
2. A programming objective
3. A limited block palette
4. A programming workspace
5. An automatic check of the objective
6. Progressive hints (V2)

Lessons are introduced in a fixed order.

Example:

```text
Challenge:

Make the player move forward
when the W key is held.
```

The learner builds the behaviour using blocks and runs the scene to test their solution.

Teaching Mode is intended to progressively introduce:

```text
Events
   ↓
Sequence
   ↓
Variables
   ↓
Conditions
   ↓
Loops
   ↓
Functions
   ↓
Game Systems
```

**Current status:** the editor opens on a *Modes* page with tutorial cards, and the Simple interface's Lesson tab shows the open tutorial. Lessons with objectives and automatic checks are not implemented yet.

---

## Sandbox Mode

Sandbox Mode is an empty project with full editor access, so users can experiment freely without lesson restrictions.

Users can:

- Create their own scenes
- Add objects
- Build custom block logic
- Import assets
- Experiment with game mechanics
- Debug their projects
- Create small playable games
- Export their projects as a standalone executable (Windows or Linux)

The same engine and editor are used for both Teaching Mode and Sandbox Mode.

**Current status:** creating games (empty or from a demo template), building scenes, importing assets, Play mode and standalone builds work today (see [Creating a New Game](#creating-a-new-game)). Block logic works too, with execution highlighting, stepping and breakpoints.

---

## Block Execution Model

Blocks do not execute directly inside the editor.

Instead, the block graph is compiled into an intermediate representation which is executed by the runtime.

```text
Block Graph
     |
     v
Block Compiler
     |
     v
Intermediate Representation
     |
     v
Block Virtual Machine
     |
     +------> Runtime State
     |
     +------> Current Block
     |
     +------> Variables
     |
     +------> Events
     |
     +------> Errors
             |
             v
        Editor Debugger
```

Each instruction retains information about the block it originated from.

This allows the runtime to report the currently executing block back to the editor and enables:

- Execution highlighting
- Breakpoints
- Step debugging
- Runtime inspection

Long-running block handlers yield instead of freezing the editor, allowing the editor and runtime to remain responsive during execution.

---

## Block-to-C++ Learning Path

Atelier3D is designed to provide a gradual transition from visual programming to traditional programming.

The intended progression is:

```text
Beginner
   |
   v
Blocks
   |
   v
Blocks + Generated C++
   |
   v
C++ Extension Points
   |
   v
Engine / Graphics Programming
```

The C++ system is intended as a **learning bridge**, not an arbitrary C++-to-block converter.

The architecture uses a shared intermediate representation:

```text
                    +------------------+
                    |                  |
Block Graph ------> | Intermediate     | ------> Runtime VM
                    | Representation   |
                    |                  |
                    +--------+---------+
                             |
                             v
                    C++ Code Generator
                             |
                             v
                       C++ View
```

The initial MVP focuses on the block programming system and runtime. The generated C++ view is planned as a later feature: learners will be able to swap the block editor between the block view and the C++ view of the same logic.

---

## Propose, Execute, Debug

The system is designed around a clear separation between creating logic and executing logic.

| Layer | Responsibility |
|---|---|
| **Block Editor** | Create and edit visual programs |
| **Compiler** | Convert block graphs into an intermediate representation |
| **Runtime / VM** | Execute the intermediate representation |
| **Object System** | Store objects, components, variables and attached logic |
| **Event System** | Dispatch input, update, collision and other events |
| **Debug Bridge** | Send runtime state back to the editor |
| **Debugger** | Display execution, variables, events and errors |

This separation allows the editor to inspect the runtime without being responsible for executing the game logic itself.

---

## Custom 3D Engine

Atelier3D is built using a **custom 3D engine** rather than Unity, Unreal Engine, Godot or another complete game engine.

The engine is custom in the areas that are central to the project:

- Object system
- Scene system
- Runtime architecture
- Block execution model
- Editor
- Debugging system
- Project system
- Engine/editor communication

Established libraries are used for supporting functionality instead of reimplementing every low-level system.

---

## Editor

The Atelier3D editor is built around a custom 3D viewport and immediate-mode editor interface.

### Editor UI

The editor uses:

- **Dear ImGui**
- **ImGuizmo**
- Custom editor panels
- Custom 3D viewport
- Scene hierarchy
- Inspector
- Asset browser
- Block editor
- Debugging interface

Example editor layout:

```text
+----------------------------------------------------------+
| Atelier3D                                                |
+----------------+-------------------------+---------------+
| Scene          |                         | Inspector     |
|                |                         |               |
| Player         |       3D VIEWPORT       | Transform     |
| Floor          |                         | Material      |
| Enemy          |       Gizmos            | Physics       |
| Camera         |       Objects           | Variables     |
|                |       Debug Info        | Logic         |
+----------------+-------------------------+---------------+
| Asset Browser  | BLOCK EDITOR / DEBUGGER                 |
+----------------+-----------------------------------------+
```

The Viewport, Hierarchy, Inspector, Assets, Console, Controls, Diagnostics and Audio windows exist today; "The editor window" under [Creating a New Game](#creating-a-new-game) describes them. The block editor and the debugger are planned.

---

## Architecture

Atelier3D is divided into three major layers.

```text
+------------------------------------------------------+
|                    EDITOR LAYER                     |
|------------------------------------------------------|
| Scene Editor | Block Editor | Inspector | Debugger  |
| Teaching     | Asset Browser| Gizmos    | C++ View* |
+---------------------------+--------------------------+
                            |
                     Commands / State
                            |
+---------------------------v--------------------------+
|                    RUNTIME LAYER                    |
|------------------------------------------------------|
| Object System | Event System | Block VM             |
| Renderer      | Physics      | Audio                |
| Asset Manager | Debug Bridge                       |
+---------------------------+--------------------------+
                            |
                       Load / Save
                            |
+---------------------------v--------------------------+
|                    PROJECT LAYER                    |
|------------------------------------------------------|
| Scenes | Block Graphs | Models | Textures | Audio   |
| Settings | Project Metadata                          |
+------------------------------------------------------+

* V2 feature
```

The editor and runtime operate together so that runtime information can be reported directly back to the editor.

This architecture is what enables live execution highlighting and in-world debugging.

This is the target architecture. The parts implemented so far (rendering, the entity component system, scenes, assets, audio, input, logging, the serializer and the editor) are Architecture v0.1, drawn in [docs/architecture.md](docs/architecture.md) with their owners, the data stores, the external libraries and the Jenkins pipeline; the block editor, physics, stepping and breakpoints are built since. The block VM (`LogicRunner` runs the block tree directly) and the in-world debugger are not implemented yet.

---

## Technology

| Concern | Choice |
|---|---|
| Language | C++20 |
| Engine | Custom 3D Engine |
| Entity Component System | EnTT |
| Graphics | OpenGL 3.3 (GLEW) |
| Editor UI | Dear ImGui (docking) |
| Gizmos | ImGuizmo |
| Window / Input | Win32 on Windows, GLFW + X11 on Linux |
| Mathematics | GLM |
| Model Loading | The engine's `.amodel` format, built-in OBJ loader, OpenFBX (FBX); assimp in the asset tool only |
| Image Loading | stb_image (PNG, JPG) |
| Audio | FMOD Core |
| Physics | Jolt Physics |
| Scripting | Lua |
| Project Format | JSON scenes and input actions (nlohmann/json), INI-style `game.cfg` |
| Build | CMake |
| Tests | GoogleTest |
| Formatting | clang-format |
| Static Analysis | clang-tidy, cppcheck |
| CI | Jenkins |

---

## Supported Platforms

| Platform | Status |
|---|---|
| Windows 10/11 (x64) | Supported, with sound (FMOD): the primary target for the editor and for exported games (MVP) |
| Ubuntu 24.04+ / WSL2 | Supported for development and used by CI; the editor builds Linux games (MVP); sound through FMOD's Linux library |
| macOS | Not supported |

A game is exported for the platform the editor runs on: Windows or Linux (MVP). Android, web and cross-platform builds are **Future** export targets in the Product Design Document.

---

## Project Structure

```text
Atelier3D/
├── README.md
├── Jenkinsfile                     # CI pipeline
│
├── docs/
│   ├── proposal.md                 # Project proposal
│   ├── design/
│   │   ├── product-design.md       # Product Design Document
│   │   └── ui-ux/                  # Wireframes, user flows, mockups
│   ├── architecture.md             # System architecture + technology stack
│   └── deliverables.md             # M1 technical deliverables (T12-T19) and product features (F01-F13)
│
├── apps/
│   ├── desktop/                    # The Atelier3D application (C++ source)
│   │   ├── CMakeLists.txt          # CMake build
│   │   ├── CMakePresets.json
│   │   ├── run.bat / clean.bat     # Build/run helper scripts (Windows)
│   │   ├── cmake/                  # Toolchain files (MinGW cross-compile)
│   │   ├── libraries/              # Third-party dependencies
│   │   │   ├── bin/                # DLLs (GLEW, Lua)
│   │   │   ├── include/            # entt, FMOD, GL, GLFW, glm, imgui, ImGuizmo, json, lua, OpenFBX, stb, ...
│   │   │   └── lib/
│   │   └── projects/
│   │       ├── atelier_engine/     # Engine library
│   │       │   ├── include/engine/ # assets, audio, core, editor, game, graphics, history, input, logic, messenger, physics, platform, scene, scripting, serializer, ui
│   │       │   └── src/            # Mirrors include/engine/, plus the built-in shaders (assets/Shaders/Builtin)
│   │       ├── atelier_asset_tool/ # AtelierAssetTool: converts models to .amodel (assimp)
│   │       ├── atelier_player/     # AtelierPlayer: the runtime exported games copy
│   │       └── atelier_editor/     # Editor: create, edit, play and build games
│   │           ├── include/editor/ # SceneEditor, camera, panels, runtime
│   │           ├── src/
│   │           └── configs/ data/ assets/   # The sample game AtelierDemos (also the templates for new games)
│   ├── web/                        # Not used (desktop-only product)
│   ├── mobile/                     # Not used
│   └── server/                     # Not used
│
├── packages/                       # Shared packages (none yet)
├── tools/
│   └── ci/                         # Build, test, lint, analysis and packaging scripts
├── tests/
│   ├── unit/                       # GoogleTest unit tests
│   └── tool/                       # Tests of the asset tool
├── .github/
│   └── workflows/                  # (CI runs on Jenkins, see Jenkinsfile)
├── .gitignore
└── .editorconfig
```

Games are created in a `projects` folder next to the editor executable (see "Creating a New Game"), and every game has the same layout:

```text
<folder of AtelierEditor.exe>/projects/<game>/
├── configs/game.cfg                # Name, window size, start scene (marks the folder as a game)
├── data/scenes/main.json           # Start scene (JSON): game objects and their components
├── assets/                         # models, textures, materials, shaders, audio
└── scripts/
```

---

## Quick Start

This section gets the editor built and running. The full step-by-step guide (IDE setup, WSL2, the checks to run before pushing, troubleshooting) is the wiki's **[Development Setup](https://github.com/L1nceLin/Atelier3D/wiki/Development-Setup)** page.

### Prerequisites

| Platform | Needed |
|---|---|
| Windows 10/11 (x64) | Git, and Visual Studio 2022 or 2026 with the **Desktop development with C++** workload (it includes MSVC and CMake; `apps/desktop/.vsconfig` lists the components) |
| Ubuntu 24.04+ / WSL2 | `git build-essential cmake ninja-build libgl1-mesa-dev xorg-dev libpulse0`. `bash tools/ci/install-deps.sh` also installs what the CI checks need (clang-format, clang-tidy, cppcheck, mingw-w64) |

CMake 3.22 or newer is required. Third-party libraries are committed under `apps/desktop/libraries/`; CMake downloads the rest on the first configure (GoogleTest, and on Linux GLFW and the GLEW source), so the first build needs an internet connection.

### Windows

Run from **Developer PowerShell for VS**:

```powershell
git clone https://github.com/L1nceLin/Atelier3D.git
cd Atelier3D\apps\desktop
.\run.bat                      # build Debug and open the editor
.\run.bat new my_game audio    # create a game from a template without opening the editor
.\run.bat build my_game        # build it into a standalone my_game.exe (see "Building a Game")
.\run.bat release              # the same commands with a Release build
```

`clean.bat` removes all build output. To open the code in Visual Studio, use **File → Open → Folder** on `apps\desktop`. For a clean build like CI's, run `tools\ci\build.bat Debug` from the repository root; it writes executables to `apps\desktop\build\ci-Debug\bin\Debug\`. All build output goes to `apps\desktop\build\`.

### Ubuntu

```bash
sudo apt install -y git build-essential cmake ninja-build libgl1-mesa-dev xorg-dev libpulse0 liblua5.4-dev
git clone https://github.com/L1nceLin/Atelier3D.git
cd Atelier3D
bash tools/ci/build.sh Debug linux
./apps/desktop/build/ci-Debug/bin/AtelierEditor                                    # open the editor
./apps/desktop/build/ci-Debug/bin/AtelierEditor --new-game my_game --template input # create a game
```

---

## Running the Tests

Unit tests use **GoogleTest** and live in [`tests/unit/`](tests/unit). They are built into the `AtelierTests` executable when the CMake option `ATELIER_BUILD_TESTS` is on (off by default; the CI scripts turn it on): about 900 tests, all of which must pass.

**Windows** (from `apps\desktop`; turn the tests on once, and `run.bat` keeps building them until you configure with `-DATELIER_BUILD_TESTS=OFF`):

```powershell
cmake -S . -B build\dev -DATELIER_BUILD_TESTS=ON                        # turn the tests on
cmake --build build\dev --config Debug --parallel                       # build them
.\build\dev\bin\Debug\AtelierTests.exe                                  # run every test
.\build\dev\bin\Debug\AtelierTests.exe --gtest_filter=SceneFileTest.*   # run one suite
ctest --test-dir build\dev -C Debug --output-on-failure                 # or through CTest
ctest --test-dir build\dev -C Debug --output-on-failure -j 8            # CTest, 8 tests at a time
```

**Ubuntu** (from the repository root; these are the scripts Jenkins runs):

```bash
bash tools/ci/build.sh Debug linux        # clean build into apps/desktop/build/ci-Debug
bash tools/ci/test.sh Debug               # unit tests in parallel, then the asset tool tests; reports in apps/desktop/build/reports/tests-*.xml
bash tools/ci/lint.sh Debug               # clang-format + clang-tidy
bash tools/ci/static-analysis.sh          # cppcheck
```

| Test files | What they cover |
|---|---|
| `EcsSceneTests.cpp`, `EcsSpawnTests.cpp`, `EcsSystemsTests.cpp`, `EcsPlayerControllerSystemTests.cpp` | The entity component system: creating and destroying entities, adding and removing components, spawning 1,000 entities, and the render, audio, spin and player systems |
| `SceneFileTests.cpp`, `SerializerTests.cpp` | The versioned JSON scene format (save → load → save gives the same file; missing, corrupt and incomplete files are reported), and the CFG and JSON serializers |
| `ResourceManagerTests.cpp` | Asset loading, caching and reference counting, placeholders for missing files, and the OBJ / WAV / OGG loaders |
| `GameProjectTests.cpp`, `GameBuildTests.cpp`, `GameRuntimeTests.cpp`, `SampleGameTests.cpp` | Creating games from templates, building standalone games, turning a scene into entities and applying live edits, and every demo scene of the sample game |
| `InputTests.cpp`, `Win32KeyMapTests.cpp` | Key and mouse state, named actions and `input_actions.json`, keeping editor input apart from game input, and Win32 key translation |
| `LogTests.cpp` | Log levels and categories, the sinks, the per-session log file, and stdout / stderr capture |
| `AudioSystemTests.cpp` | Playing a sound file through FMOD (at volume 0), or the silent backend where FMOD is off |
| `EditorSceneTests.cpp`, `EditorCameraTests.cpp`, `View2DTests.cpp` | The scene being edited (objects, copies, undo snapshots, tutorial notes), the editor camera, and the 2D view |
| `AssetBrowserTests.cpp` | The Assets window: listing a game's assets, search, type filters, favorites and their file, and the "crate" demo |
| `BlockLogicTests.cpp`, `BlockEventTests.cpp`, `BlockPreviewTests.cpp`, `DebuggerTests.cpp`, `VariableInspectorTests.cpp` | Block logic, its events and previews, breakpoints and stepping, and the variable inspector |
| `PhysicsWorldTests.cpp`, `PhysicsSystemTests.cpp`, `CollisionResponseTests.cpp`, `ColliderLinesTests.cpp`, `ForceTests.cpp`, `GravityTests.cpp`, `QueryTests.cpp` | Physics: bodies, collisions, forces, gravity and queries |
| `EventBusTests.cpp`, `EventRegistryTests.cpp`, `TimerSystemTests.cpp` | The event system and timers |
| `PrefabTests.cpp`, `NestedPrefabTests.cpp` | Prefabs, their instances and overrides, and nested prefabs |
| `AtelierModelTests.cpp`, `ModelImporterTests.cpp`, `ModelLoaderTests.cpp`, `GltfLoaderTests.cpp`, `MaterialFileTests.cpp`, `AssetLayoutTests.cpp`, `LodTests.cpp`, `AnimationTests.cpp`, `BuiltInShapeTests.cpp` | Models, materials, the asset folders, level of detail, animation and the built-in shapes |
| `LuaScriptTests.cpp`, `SceneCommandTests.cpp`, `TutorialLessonTests.cpp` | Lua scripts, scene commands and tutorial lessons |
| `CoreTests.cpp` | Frame timing |

Two frame-time benchmarks in `EcsSpawnTests.cpp` are disabled by default because their result depends on the machine; add `--gtest_also_run_disabled_tests` to run them. Test data is in `tests/unit/data/`, and the tests also load the sample game in `apps/desktop/projects/atelier_editor/`. The asset tool has its own tests in `tests/tool/`. Any failing test fails the Jenkins build.

---

## Creating a New Game

Games are folders of data, so a new game needs no C++ code and no CMake changes. The repository ships no games; the engine creates them, so the editor starts with an empty scene until a game is created or opened.

**In the editor:** choose **Project > New Game...** (or **New Game...** in the game list at the top right), type a name, pick a template and click **Create**. The new game opens in the workspace; press **Play (F5)** to run it.

New Game offers an empty world and three genre games, each starting at its own main menu (Play goes to the game), played with blocks and the physics (W A S D move, Shift is faster). Floors, walls and buildings are Static bodies, props are Dynamic ones you knock about, and the player is a Dynamic body that walls stop:

| Template | The new game starts with |
|---|---|
| `empty` (the default) | An empty scene |
| `rpg` | *Adventure*: a walled village (houses, a well, lantern posts, trees), a *Villager* who asks for 3 lost gems on their pedestals, crates and hay bales to push, and 3 hopping slimes that chase and hurt you (with health bars over them); Space jumps, F swings the sword (knocking slimes back). Crates, hay bales, gems and slimes are prefabs; a gem picture, a quest bar and the quest text are made with Screen blocks |
| `fps` | *Arena shooter*: a lit arena with pillars, a platform up a ramp, and crate stacks and barrels as cover; 5 hovering drones (a prefab, with health bars) chase you; the left mouse button (or F) fires bolts that fly the way you face and knock back what they hit, Space jumps; a title, a *drones down* bar and the win are made with Screen blocks |
| `racing` | *Racing*: a circuit with barriers, a grandstand, floodlights, cones and tyre stacks to knock flying, and 3 gates and a finish line to drive through in order for 3 laps; A / D steer while driving, Shift boosts, and *Race Control* counts the laps on a bar; a 3-2-1-GO countdown is made with Screen blocks, and cones and tyres are prefabs |

The milestone demos are not offered in New Game, but `--template` still takes them (and they stay in the sample game for the demonstrations):

| Demo template | The new game starts with |
|---|---|
| `rendering` | 100 crates, 1 directional + 4 point lights and a Main camera |
| `resources` | 10 crates and a few more objects, all built-in shapes |
| `ecs` | An object to spawn 1,000 copies of, and objects to add and remove components on |
| `audio` | A looping 3D sound, background music, and a player the camera follows |
| `input` | A player the camera follows, and waypoints marking the directions |
| `prefabs` | A player with three child objects, saved as the *Enemy* prefab, and five Enemy instances (one with less health) |

The templates are the scenes of the sample game (see "Sample Game" below): the new game's start scene is a copy of that demo scene, together with the prefabs and My Blocks it uses. The demos ship no models, textures, pictures or sounds: every object is a built-in shape (a box the size of the model it stood in for, in its colour), so a game made from one starts with an empty `assets/` to fill. The build copies them to `templates/demos/` next to the editor, and release packages include that folder.

**From the command line:**

```bash
AtelierEditor --new-game my_game --template audio    # or, in apps/desktop: run.bat new my_game audio
```

This creates `projects/my_game/` **next to the editor executable** (for example `apps\desktop\build\dev\bin\Debug\projects\my_game\` after `run.bat`); the `projects` folder is made with the first game. The editor lists every game in that folder. Add `--projects-dir <dir>` to create a game somewhere else. Edit `data/scenes/main.json` to change what the game shows:

```json
{
  "version": 8,
  "objects": [
    {
      "id": 1, "name": "Hero",
      "transform": { "x": -1.2, "y": 0, "z": 0, "rotation_x": 0, "rotation": 0, "rotation_z": 0,
                     "size": 1, "scale_x": 1, "scale_y": 1, "scale_z": 1 },
      "renderable": { "shape": "cube", "rotation_speed": 45, "texture": "assets/art/textures/crate/crate.png" },
      "player": { "speed": 3, "fast_multiplier": 2, "camera_offset_y": 1, "camera_offset_z": 4, "follow_camera": true }
    },
    {
      "id": 2, "name": "Sun",
      "transform": { "x": 0, "y": 5, "z": 0, "rotation_x": -45, "rotation": 0, "rotation_z": 0,
                     "size": 1, "scale_x": 1, "scale_y": 1, "scale_z": 1 },
      "light": { "type": "directional", "enabled": true, "color_r": 1, "color_g": 1, "color_b": 1,
                 "intensity": 1, "range": 10, "spot_inner": 25, "spot_outer": 35 }
    }
  ]
}
```

`version` is the scene format version (currently 8; version 7 files, from before parents and prefabs, still open); any other version, or a file without one, is refused with an error naming the problem (versions 1 to 6 used an older, flat layout). Every object has an `id` (unique, above 0) and a `name`, and `parent` (the id of the object it is under) when it is a child: its `transform` is then relative to the parent, so moving the parent moves it too. A parent that does not exist, or would make a loop, is dropped with an error. Each object lists only the components it has: `transform` (position, rotation in degrees around X / Y / Z, and scale: `size` above 0 times `scale_x` / `scale_y` / `scale_z`), `renderable` (`shape` is one of `quad`, `circle`, `cube`, `pyramid`, `plane`, `wirebox`, `sphere` or `line`; `rotation_speed` turns it in degrees per second; optional `mesh`, `texture` and `shader` paths, `albedo_r` / `_g` / `_b` / `_a` tint, and `shape_size_x` / `_y` / `_z` and `shape_center_x` / `_y` / `_z`: the built-in shape's own size and middle inside the object, before its Transform), `audio_source` (`clip`, `volume`, `loop`, `play_on_start`, `spatial`, `min_distance`, `max_distance`, `bus`), `player` (`speed`, `fast_multiplier`, `look_sensitivity`; with `follow_camera` the camera stays `camera_offset_z` behind it, looking at a point `camera_offset_y` above it, tilted by `camera_pitch`), `camera` (`field_of_view`, `near_plane`, `far_plane`, and `primary`: the Main camera Play mode renders with) `light` (`type` `directional`, `point` or `spot`, colour, intensity, range and spot cone angles), `health` (`health` and `max_health`) and `prefab` (on objects placed from a prefab: the prefab file's `path`, which `object` of it this one copies, and the `overrides` changed on this copy only; see Prefabs below). An object is drawn only when it has both a `transform` and a `renderable`. Asset paths are relative to the game folder. Unknown components are skipped. A missing, corrupt or unreadable scene, and any missing or invalid field, is logged as an error; the game still starts, using an empty scene or the field's default (position and rotation 0, `rotation_speed` 60, `size` 1). The editor saves this same file, and saving then loading then saving gives an identical file. `configs/game.cfg` is an INI-style file (`key = value`) with the game's name, window size and start scene.

`configs/input_actions.json` maps named actions to keys and mouse buttons, so controls can be remapped without recompiling (the file is read each time the game starts):

```json
{
  "version": 1,
  "actions": {
    "MoveForward": ["W", "Up"], "MoveBack": ["S"], "MoveLeft": ["A"], "MoveRight": ["D"],
    "MoveUp": ["E"], "MoveDown": ["Q"], "MoveFast": ["LeftShift", "RightShift"], "Look": ["MouseRight"],
    "Jump": ["Space"]
  }
}
```

An action lists one or more keys (any of them triggers it) or `[]` for none; games can add their own actions. Key names are letters, digits, `F1`–`F12`, `Space`, `Enter`, `Escape`, `Tab`, `Up`/`Down`/`Left`/`Right`, `LeftShift`, `LeftCtrl`, `LeftAlt` (and `Right…`), `NumPad0`–`NumPad9`, punctuation such as `Comma` or `Slash`, and `MouseLeft`, `MouseRight`, `MouseMiddle`; case does not matter. New games get the defaults above (without `Up` and `Jump`). A missing file is logged as a warning and the defaults are used; a corrupt file, an unknown key name or a missing action is logged as an error and the default binding is used. While you type in an editor field or the mouse is over an editor panel, the game receives no keyboard or mouse input respectively.

**Edit mode and Play mode.** The editor camera and the game camera are separate. When the editor is not playing, the selected game's scene is shown frozen in the **Viewport** and the move keys (WASD, Q/E, Shift, right mouse to look, scroll for speed) fly the editor camera around it. In Play mode (F5) the game camera takes over: the scene is drawn from the camera marked **Main Camera**, and the editor camera's controls do nothing until Stop, which returns to the editor's view. If the scene has a player, WASD moves it the way it faces (Q / E down / up, Shift faster), holding the right mouse button turns it, the wheel sets the camera distance, and the Main camera follows behind it (with no Main camera, the view itself follows the player). The toolbar's **Game Camera** switch shows the game camera in edit mode too. While playing, Inspector edits reach the running game straight away (add or remove a component and its system reacts on the next frame) while everything else keeps its live state. To remap a key: stop, edit `configs/input_actions.json` (for example `"MoveForward": ["Up"]`), and press Play again; the new key works and the old one no longer does. Built games read the same file.

**The editor window.** A menu bar (**File** scenes, **Edit** undo / copy / paste, **Project** games / build / controls, **Window** windows) with **Play / Pause / Stop** in the middle and the selected game on the right. The editor opens on the **Modes** page (tutorial cards); **Open Workspace** (or **Window > Workspace**) shows the docked windows:
- *Viewport*: the game in 3D over a ground grid, drawn at the display's full resolution. In edit mode it works like a commercial engine's scene view:
  - **Camera**: hold the right mouse button and drag to look, with WASD / Q E to fly (Shift faster, wheel = fly speed); **Alt + left drag** orbits, **middle drag** pans, the **wheel** zooms, **F** (or double-clicking an entity in the Hierarchy) frames the selection. Movement is eased, so it glides.
  - **Selection and gizmos**: click an object to select it (it is outlined); **W** move, **E** rotate, **R** scale, **Q** no gizmo, or the toolbar buttons. **Local / World** switches the gizmo's axes, **Snap** (or holding Ctrl) snaps to 0.5 units / 15 degrees / 0.1 scale. Delete removes the selection, Escape deselects.
  - **View cube** (top right): a labelled cube with X / Y / Z handles (solid for +, rings for -). Click a face or a handle to look from the Front, Back, Left, Right, Top or Bottom, drag it to orbit, and **Home** returns to a three-quarter view. It stays in the corner, sized to the display, at any window size.
  - **Sky and lighting**: scenes are drawn under a sky with a sun, soft light from the sky and ground, and haze that fades distance into the horizon (games too). The toolbar's **Sky** switch turns it off for flat colours.
  - **2D view** (toolbar **2D**): the scene from the front over a grid. Drag an object to move it (**Snap** or Ctrl snaps it to the grid), **middle or right drag** pans, the **wheel** zooms at the pointer. The grid, the X / Y axis lines and the objects share one world-to-screen conversion, so they stay lined up at any zoom, pan or window size; the grid spacing adapts to the zoom (shown at the bottom).
  - Toolbar: 3D / 2D, Grid on / off, Sky on / off, Stats (FPS and entity count in the corner, also in the Window menu), Game Camera (look through the Main camera), Maximize (Shift+Space shows only the Viewport). **F11** makes the window fullscreen.
  In Play mode the move keys drive the player and the Main camera follows it. The game only gets the mouse while it is over the Viewport.
- *Hierarchy*: the scene's game objects, as a tree. **Create** adds an Empty object (no components, not even a Transform), a Cube, Sphere or Plane, a Directional or Point Light, or a Camera (the first camera becomes the Main camera); **Add Player** adds a cube the move keys drive (one per scene); right-click to rename, duplicate, delete, **Create Child**, **Move to Top** or **Save as Prefab...**. Drag an object onto another to put it under it (it stays where it is in the world), or onto the empty space below the list to take it out again. Deleting, duplicating or copying an object takes its children with it. Prefab instances are shown in blue; the **Prefabs** list above the tree places more of them.
- *Inspector*: the selected game object's name and the components it has, each with **Remove**; **Add Component** adds **Transform** (position, starting rotation, scale), **Renderable** (shape, rotation speed, and the **texture**, mesh and shader: pick one of the game's files from the list next to each, or type the path of an image or an .obj/.fbx model anywhere on disk and click **Import**: images are copied into `assets/art/textures/` and become the texture, models are converted into `assets/art/models/<name>/` with their original files, their images in `assets/art/textures/<name>/`, and become the model), **Audio Source** (a sound clip, volume and range), **Player** (speed, look sensitivity and the follow camera's distance, height and pitch), **Camera** (field of view, near / far planes, Main Camera), **Light** or **Health** (health and maximum health). For part of a prefab instance, the top of the Inspector shows its prefab and each property changed on this copy, with **Revert**, plus **Apply to Prefab** and **Unpack** (see Prefabs below). An object is drawn only when it has both a Transform and a Renderable. A texture shows on a `cube`, or on any object with a mesh.
- *Assets*: the game's models, textures and sounds as tiles. Search by name, filter with **Models** / **Textures** / **Audio** / **Favorites**, hover a tile for a bigger preview, press the play button on a sound to hear it, and click the star to keep it in Favorites. Click a tile to use it on the selected object (a model becomes its mesh, an image its texture, a sound its Audio Source clip), or drag it onto an object in the Viewport or Hierarchy, or onto an Inspector field. *Loaded Assets* lists what the scene has loaded.
- *Controls* (also **Project > Controls...**): every action in `configs/input_actions.json` with its keys. Click a key to remove it, use **+** to add one from the list, or **Press key** and press the key to bind. **Add action** creates a new action for game code; the engine's own actions cannot be removed, only left without keys. **Save controls** writes the file and the new keys work straight away; **Reset to defaults** restores the default bindings.
- *Console* (every log message), *Diagnostics* (frame rate, the ECS demo's **Spawn 1,000 copies** of the selected object, sound test, camera values, everything the input system reports), *Audio* (the sound bank) and *Notes* (the open tutorial).

**File > Save Scene** (Ctrl+S) writes the open scene (for a new game, its start scene `data/scenes/main.json`): every game object and its components, in the format above. **File > Game Scenes** lists the game's scene files to open one with a click. The game and built games play the same file. **Play** saves first and plays the scene as it is in the editor. Shortcuts: F5 Play / Stop, Ctrl+N / O / S scene, Ctrl+Z / Y undo / redo, Ctrl+C / V copy / paste, Delete, Ctrl+B build, Ctrl+R refresh the game list.

Game names use letters, digits, `_` and `-`.

**Prefabs.** A prefab is a group of objects saved to be placed again and again: an object and everything under it, kept in the game's `assets/prefabs/<name>.prefab` (the same layout as a scene file's objects). Right-click an object in the Hierarchy and choose **Save as Prefab...**: the objects become the prefab's first instance. **Place** in the Hierarchy's **Prefabs** list adds another instance, next to the ones already placed. Each instance can be moved, renamed and changed on its own: a property you change on one instance (its health, say) is an *override*, listed in the Inspector with **Revert**, and only that instance has it. **Apply to Prefab** saves an instance's changes into the prefab, and every other instance takes them, except the properties it changed itself; the same happens to the instances in a scene when it is opened after its prefab file changed (or after **Refresh** in the Prefabs list). The parts of an instance stay as the prefab has them, so deleting or moving one is refused: change the prefab and apply it, or **Unpack** the instance into ordinary objects. Scenes keep all of each instance's data, so a game plays the same without its prefab files.

**Nested prefabs.** A prefab can hold instances of other prefabs: a *Squad* prefab made of two *Enemy* instances, say. Open a prefab (double-click it in the Prefabs list) and **Place** another prefab, or drag it onto an object in the Hierarchy or into the Viewport: the instance goes under the prefab's root, and **Save** keeps it as a nested prefab. Saving a group that holds whole instances as a prefab (**Save as Prefab...**), or using **Apply to Prefab** on an instance with another prefab's instance added under it, nests them too. A change to the Enemy prefab then reaches the enemies in every Squad, except what the Squad (or one Squad instance) changed on them. A prefab cannot be nested in itself, or in a prefab it contains.

> **Games inside `build\` are deleted by `clean.bat` and by the CI build scripts**, which start from an empty build folder, and `build\` is not committed to Git. Copy games you want to keep, or share with the team, somewhere else.

---

## Sample Game

`apps/desktop/projects/atelier_editor/` is a complete sample game, **AtelierDemos**, kept in the repository: the three genre games new games start from (`rpg.json`, `fps.json`, `racing.json`, see [Creating a New Game](#creating-a-new-game)), and one demo scene per engine feature, each built from objects that show that feature. Its scenes are also the templates for new games (above): to demo a feature in the editor, create a game from its template, or open the sample game itself. The unit tests load every scene, check it still holds what its demo needs, and check that loading and saving it gives back the same file. A game made from a template opens straight on that scene; the sample game's scenes open with **File > Game Scenes**. Play one in its own window with `AtelierEditor --play apps/desktop/projects/atelier_editor --scene data/scenes/<name>.json` (without `--scene`, the start scene, `rendering.json`).

| Scene | What it shows | How to demo it |
|---|---|---|
| `rendering.json` | 100 crates (built-in cubes) on a floor, lit by 1 directional and 4 coloured point lights; a Main camera | The FPS counter is at the top left of the Viewport. Edit mode shows the editor camera (fly around); **Game Camera** or **Play** switches to the Main camera, and the crates start turning. |
| `resources.json` | 10 crates and a few other objects, all built-in shapes, so nothing is read from disk | *Loaded Assets* is empty. Give a crate a model or texture from the Assets window to see it loaded once and shared, and point one at a file that does not exist to see the placeholder (magenta texture, default cube) and the Console warning. |
| `ecs.json` | A *Spawn Template* crate, a *Spinner*, a *Lamp* (a light), and a *Player* whose camera does not follow, so the Main camera keeps the overview | Press Play, select *Spawn Template*, and click **Diagnostics > ECS > Spawn 1,000 copies**: 1,000 entities with its Transform, MeshRenderer and Spin fill a block above it, and the frame rate shows beside them. Then, still playing, use the Inspector: remove the Spinner's Renderable (it disappears) or change its rotation speed, remove the Lamp's Light (its glow goes), remove the Player component (WASD stops moving it) or add one to another object. |
| `audio.json` | A *Radio* crate playing a looping 3D sound, *Background Music* on the Music bus (not 3D), and a *Player* the Main camera follows | Press Play and walk around the radio with WASD and the right mouse: the camera is the listener, so the sound fades with distance and pans left / right as you turn. Change either volume in the Inspector while it plays. |
| `prefabs.json` | A *Player* with three child objects (Head, Left Arm, Right Arm) and 100 health, the *Enemy* prefab made from it (`assets/prefabs/Enemy.prefab`: the same body in red, without the move keys), and five Enemy instances in a row; *Enemy 3* has 25 health | Select *Enemy 3*: the Inspector lists **Health > Health** as changed on this instance, while the others have 100. Change another enemy's Max Health and click **Apply to Prefab**: every enemy takes it except Enemy 3's own health. **Revert** gives Enemy 3 the prefab's health again. Move an enemy and its head and arms go with it; **Place** adds a sixth. |
| `input.json` | A *Player* the Main camera follows, and four coloured waypoints marking the directions | Edit mode: fly the editor camera (right mouse + WASD, Q / E, Shift, wheel). Play: WASD moves the player the way it faces, right mouse turns it, the wheel zooms the camera. Then stop, change a key in `configs/input_actions.json` (or **Project > Controls**), press Play again, and the new key works. Typing in an Inspector field moves nothing. |

The game's other files: `configs/game.cfg` (name, window size, start scene), `configs/input_actions.json` (the named actions and their keys), and `assets/` (the prefabs, one material and an empty sound bank: no models, textures or sounds); objects with no shader of their own use the engine's built-in lit shader.

## Building a Game

The editor turns a game project into a standalone program that runs without the editor.

**In the editor:** select the game (top right) and choose **Project > Build Game** (Ctrl+B); the scene is saved first.

**From the command line:**

```bash
AtelierEditor --build-game my_game                   # or, in apps/desktop: run.bat build my_game
AtelierEditor --build-game my_game --output D:\Games # choose where the build goes
```

The build goes to `builds/my_game/` next to the editor (or `--output`):

```text
builds/my_game/
├── my_game.exe        # run this to play (my_game on Linux)
├── *.dll              # Windows: the runtime libraries next to the editor (GLEW, FMOD, Lua)
└── game/              # the game's configs, data, assets and scripts
```

Copy or zip the whole `my_game` folder to share it; it does not need the editor or the source code. Building again replaces the previous build.

How it works: games are data, so there is no game code to compile. `my_game.exe` is a copy of the editor program, and when it starts it finds `game/` next to itself and plays that game instead of opening the editor. `AtelierEditor --play my_game` plays a game the same way without building it.

> Windows builds made with Visual Studio need the Microsoft Visual C++ Redistributable on the player's PC (most PCs already have it). Share **Release** builds: a Debug build only runs on PCs with Visual Studio installed.

---

## Continuous Integration

Jenkins automatically checks commits on `main`. The Jenkinsfile always checks out `main` and builds as soon as GitHub reports a push; if GitHub cannot reach Jenkins, it checks for new commits every 5 minutes instead. For builds on push, add a webhook in the GitHub repository (Settings > Webhooks) pointing to `<jenkins>/github-webhook/`, with content type `application/json` and only the push event.

The CI pipeline currently covers:

- Dependency installation (`tools/ci/install-deps.sh`: installs missing build tools with apt; the first run on a new node needs root or passwordless sudo)
- Linux build
- Windows build
- Unit tests
- clang-format
- clang-tidy (a push checks only what changed since the last successful build and the files that include it; a nightly build, which stops after the lint and static analysis stages, and manual builds check everything)
- cppcheck
- Packaging, plus a public download and build history page at `<jenkins>/userContent/atelier3d/`. Guests can use it without access to the Jenkins job itself.

Builds stay clean, but compile results (ccache) and downloaded libraries are kept between builds in `~/.cache/atelier3d-ci` on the build node. Set `ATELIER_CI_CACHE` to move that folder and `CCACHE_MAXSIZE` to change the cache limit (default 20G).

The Jenkins pipeline is defined in:

**[Jenkinsfile](Jenkinsfile)**

The same checks can be run locally using the scripts under:

```text
tools/ci/
```

See [Running the Tests](#running-the-tests) for the commands, and the wiki's **[Development Setup](https://github.com/L1nceLin/Atelier3D/wiki/Development-Setup)** page for the full setup guide and what each check covers.

---

## MVP

The initial MVP focuses on the core learning experience: it is the set of features marked **MVP** under [Core Features](#core-features). The status of each one at Milestone 1 is in the [Product Design Document](docs/design/product-design.md#minimum-viable-product-mvp).

---

## Development Roadmap

| Phase | Activity | Deliverable |
|---|---|---|
| 1 | Requirements & system design | Architecture, block language and project format |
| 2 | Core engine & scene editor | 3D scene with objects, camera, lighting and gizmos |
| 3 | Object & asset pipeline | Asset import, inspector and project save/load |
| 4 | Block programming system | Block palette, canvas, rules and compilation |
| 5 | Runtime & execution visualisation | Running games with block execution highlighting |
| 6 | Debugging & inspection | Step execution, breakpoints and readable errors |
| 7 | Teaching Mode | Structured validated lessons |
| 8 | Sandbox Mode & export | Free project creation and game export (Windows and Linux) |
| 9 | C++ View | Generated C++ representation, swappable with the block view |
| 10 | Testing & optimisation | Final testing, usability trials and documentation |

The C++ view is a **V2** feature in the Product Design Document. It is treated as a conditional later-stage feature so that the core engine, block system and runtime remain the priority.

---

## Team SayReal

| Member | Role |
|---|---|
| Ko Jat Chong | Tech Lead, Engine Champion, Backend Lead |
| Sheng Lin | Product Manager |
| Kye Le Woh | Graphics Champion, API Champion |
| Hong Chieh Jethro Sung | Physics Champion, Scripting Champion |
| Julius Junshen Cai | Animation Champion, Database Champion |
| Shi Wei Beh | Tools Champion |
| Thant Win Kaung | Audio Champion |
| San Ni Go | UI Design Champion, Auth Champion |
| Henrey Hu | Debugging Tools Champion |

---

## Milestones

| Milestone | Week | Weight | Focus |
|---|---:|---:|---|
| M1 | Week 5 | 25% | Tool / pipeline MVP: architecture, build pipeline, data pipeline and usability |
| M2 | Week 10 | 25% | Prototype-capable tool: integration quality, documentation and testing |
| M3 | Week 13–14 | 30% | First app / product MVP: core experience and Semester 2 feasibility |

---

## Milestone 1 Deliverables and Jira

Work is tracked in the Jira space **Atelier3D-DEV** (project key `ATE3D`): [backlog](https://digipen-team-apps-1.atlassian.net/jira/software/projects/ATE3D/boards/1/backlog). Each ticket is developed on a Git branch named after its key (for example `ATE3D-59`) and merged into the `merge` integration branch, which goes into `main`, the branch Jenkins builds. Commit messages start with the ticket key or a tag such as `[Feat]`, `[Fix]`, `[Chore]` or `[Doc]`.

M1 is the tool / pipeline MVP (see [Milestones](#milestones)). The approved project-specific deliverables are the technical deliverables **T12–T19** and the product features **F01–F13**. Their acceptance criteria, demonstration steps and status (all marked Done) are in **[docs/deliverables.md](docs/deliverables.md)**; the tables below link every M1 item to its Jira ticket and to where it is in this repository.

### Project setup and documentation (T1–T11)

| ID | Deliverable | Jira | Evidence in this repository |
|---|---|---|---|
| T1 | GitHub repository and workflow | [ATE3D-67] | [github.com/L1nceLin/Atelier3D](https://github.com/L1nceLin/Atelier3D); ticket branches → `merge` → `main` (described above); `.gitignore`, `.gitattributes`, `.editorconfig` |
| T2 | Jira project and backlog | [ATE3D-68] | [ATE3D backlog](https://digipen-team-apps-1.atlassian.net/jira/software/projects/ATE3D/boards/1/backlog); Jira setup in [ATE3D-7]; ticket keys in branch names, commit messages and `docs/deliverables.md` |
| T3 | Architecture V0.1 | [ATE3D-69] | [docs/architecture.md](docs/architecture.md); [Architecture](#architecture) and [Technology](#technology) above |
| T4 | Automated build | [ATE3D-70] | [Jenkinsfile](Jenkinsfile): Linux Debug and Release builds, Windows Release build (MinGW-w64), packaging and public downloads; `tools/ci/build.sh`, `tools/ci/build.bat`, `apps/desktop/run.bat` |
| T5 | Automated checks / tests | [ATE3D-71] | GoogleTest suite in `tests/unit/`; clang-format, clang-tidy and cppcheck in `tools/ci/`; all run on every Jenkins build ([Running the Tests](#running-the-tests), [Continuous Integration](#continuous-integration)) |
| T6 | Reproducible development setup | [ATE3D-72] | [Quick Start](#quick-start) and the wiki's [Development Setup](https://github.com/L1nceLin/Atelier3D/wiki/Development-Setup) page; `tools/ci/install-deps.sh`, `apps/desktop/.vsconfig`, `CMakePresets.json`; third-party code committed in `apps/desktop/libraries/` or downloaded by CMake at a fixed version; `.clang-format`, `.clang-tidy`, `.editorconfig` |
| T7 | Working technical POC | [ATE3D-73] | The editor creates, edits, plays and builds games ([Creating a New Game](#creating-a-new-game), [Building a Game](#building-a-game)) |
| T8 | Basic error handling | [ATE3D-74] | Missing or corrupt scene files, input files and fields are logged and fall back to defaults; missing assets get placeholders; every error reaches the Console and the session log file (T13, T14, T15); covered by `SceneFileTests.cpp`, `ResourceManagerTests.cpp`, `InputTests.cpp` and `LogTests.cpp` |
| T9 | Working product demonstration | [ATE3D-75] | "M1 Demonstration Flow" in [docs/deliverables.md](docs/deliverables.md); the [Sample Game](#sample-game) and its demo templates |
| T10 | Project documentation | [ATE3D-76] | This README (setup, build, run, test, Jira links) and the documents listed under [Documentation](#documentation) |
| T11 | Individual contributions evidence | [ATE3D-77] | The owner of each deliverable and feature in the tables below; `@author` lines with contribution percentages in the source file headers; Git history per author (`git shortlog -sn --all`) |

### Technical deliverables (T12–T19)

Parent ticket: [ATE3D-99]. The demo template is the game template that shows the deliverable (see [Creating a New Game](#creating-a-new-game)).

| ID | Deliverable | Jira | Owner | Code | Tests | Demo template |
|---|---|---|---|---|---|---|
| T12 | Basic 3D rendering pipeline: OpenGL 3.3, built-in shapes and imported models, base colour and diffuse texture, Blinn-Phong lighting with 1 directional and up to 8 point lights, separate editor and game cameras | [ATE3D-35] | Kye Le Woh | `engine/graphics`, `engine/scene/systems/RenderSystem` | `EcsSystemsTests.cpp`, `EditorCameraTests.cpp` | `rendering` |
| T13 | Serializer / deserializer: scenes saved as versioned JSON with every entity, component, property value and asset path; save → load → save round trip; errors logged without crashing | [ATE3D-15] | Ko Jat Chong | `engine/serializer`, `engine/scene/SceneFile` | `SerializerTests.cpp`, `SceneFileTests.cpp`, `SampleGameTests.cpp` | any |
| T14 | Resource Manager: models (`.obj` / `.fbx`), textures (`.png` / `.jpg`), shaders and audio (`.wav` / `.ogg`) loaded by path, cached, reference-counted, with placeholders for missing files | [ATE3D-14] | Julius Junshen Cai | `engine/assets` | `ResourceManagerTests.cpp` | `resources` |
| T15 | Engine logging system: Debug / Info / Warning / Error with timestamp and category, `LOG_*` macros, output to the Console, the terminal and a per-session file in `logs/`, minimum level changeable at runtime | [ATE3D-57] | Henrey Hu | `engine/core` (`Log`, `LogSinks`, `StdCapture`), editor Console panel | `LogTests.cpp` | any |
| T16 | Entity Component System (EnTT): entities with unique IDs; Transform, MeshRenderer, Light, Camera, AudioSource and PlayerController components; render, audio and player controller systems; 1,000 entities | [ATE3D-59] | Hong Chieh Jethro Sung | `engine/scene` | `EcsSceneTests.cpp`, `EcsSpawnTests.cpp`, `EcsSystemsTests.cpp`, `EcsPlayerControllerSystemTests.cpp`, `GameRuntimeTests.cpp` | `ecs` |
| T17 | Basic 3D audio system (FMOD): play / pause / stop / loop, per-source and master volume, 3D sound that fades with distance and pans, the camera as the listener, a separate music bus | [ATE3D-89] | Thant Win Kaung | `engine/audio`, `engine/scene/systems/AudioSystem`, editor Audio panel | `AudioSystemTests.cpp`, `EcsSystemsTests.cpp` | `audio` |
| T18 | Scene editor interface (Dear ImGui): dockable Viewport, Hierarchy, Inspector, Assets and Console windows, a Window menu, a layout remembered between sessions, File > New / Open / Save / Save As, Play / Stop | [ATE3D-60] | San Ni Go | `atelier_editor` (`SceneEditor`, panels), `engine/editor`, `engine/ui` | `EditorSceneTests.cpp`, `EditorCameraTests.cpp`, `View2DTests.cpp` | any |
| T19 | Keyboard / mouse input system: pressed / held / released, mouse position, movement and scroll, named actions from `input_actions.json`, editor input kept apart from game input | [ATE3D-10] | Shi Wei Beh | `engine/input`, editor Controls panel | `InputTests.cpp`, `Win32KeyMapTests.cpp` | `input` |

Related tickets: for rendering, [ATE3D-66] (3D / 2D camera system) and [ATE3D-58] (3D model pipeline) on the Jira board; for audio, [ATE3D-46] (FMOD Core integration), [ATE3D-47] (audio manager and sound bank), [ATE3D-48] (Music and SFX buses) and [ATE3D-49] (3D positional audio), named in the branches and commit messages.

### Product features (F01–F13)

Parent ticket: [ATE3D-61]. Priorities are MUST, SHOULD or COULD.

| ID | Feature | Priority | Jira | Owner | Built on | Where to see it |
|---|---|---|---|---|---|---|
| F01 | Scene viewing: fly the editor camera (right mouse + WASD, wheel to zoom) around a lit 3D scene | MUST | [ATE3D-90] | Kye Le Woh | T12, T18, T19 | Viewport |
| F02 | Object editing: create Empty, Cube, Sphere, Plane, Light and Camera objects; select, rename, duplicate and delete them; edit their component properties | MUST | [ATE3D-91] | Ko Jat Chong | T12, T16, T18 | Hierarchy and Inspector |
| F03 | Asset import: `.obj` / `.fbx` models and `.png` / `.jpg` textures, assigned to an object's mesh and material | MUST | [ATE3D-92] | Julius Junshen Cai | T12, T14 | Inspector **Import**, Assets window |
| F04 | Save and load: scenes saved to `.json` and reopened with every object, value and asset link; a broken or missing file shows an error instead of crashing | MUST | [ATE3D-93] | Ko Jat Chong | T13, T14 | File menu |
| F05 | Play mode: run the scene from the game camera, move a player with WASD and the mouse, Stop returns to the editor with the scene reset | MUST | [ATE3D-94] | Hong Chieh Jethro Sung | T16, T19 | Play / Pause / Stop (F5) |
| F06 | Sound and music: an Audio Source with clip, volume, loop and 3D on / off; 3D sounds fade and pan with the camera, music loops | MUST | [ATE3D-95] | Thant Win Kaung | T14, T17 | Inspector Audio Source, `audio` template |
| F07 | Editor console: timestamped Info, Warning and Error messages, filtered by level, cleared, and also written to a log file | MUST | [ATE3D-96] | Henrey Hu | T15, T18 | Console window |
| F08 | Editor windows: open, close, resize and dock the windows; the layout is remembered next time | MUST | [ATE3D-97] | San Ni Go | T18 | Window menu |
| F09 | Add / remove components (Mesh Renderer, Light, Camera, Audio Source, Player Controller); the object reacts immediately | SHOULD | [ATE3D-102] | Hong Chieh Jethro Sung | T16, T18 | Inspector **Add Component** / **Remove** |
| F10 | Control remapping through `input_actions.json`; typing in a field never moves the camera or player | COULD | [ATE3D-103] | Shi Wei Beh | T19 | Controls window, `input` template |
| F11 | Missing-asset recovery: the scene still opens, with a magenta texture or default cube and a Console warning naming the file | COULD | [ATE3D-104] | Julius Junshen Cai | T13, T14, T15 | `resources` template |
| F12 | Performance overlay: FPS and entity count | COULD | [ATE3D-105] | Kye Le Woh | T12, T16 | Viewport toolbar **Stats** |
| F13 | Sample scene: a bundled level with textured props, lights, looping music and a player, ready to play | COULD | [ATE3D-106] | Ko Jat Chong | T12, T13, T14, T17 | [Sample Game](#sample-game) and the templates |

[ATE3D-7]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-7
[ATE3D-10]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-10
[ATE3D-14]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-14
[ATE3D-15]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-15
[ATE3D-35]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-35
[ATE3D-46]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-46
[ATE3D-47]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-47
[ATE3D-48]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-48
[ATE3D-49]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-49
[ATE3D-57]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-57
[ATE3D-58]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-58
[ATE3D-59]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-59
[ATE3D-60]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-60
[ATE3D-61]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-61
[ATE3D-66]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-66
[ATE3D-67]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-67
[ATE3D-68]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-68
[ATE3D-69]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-69
[ATE3D-70]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-70
[ATE3D-71]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-71
[ATE3D-72]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-72
[ATE3D-73]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-73
[ATE3D-74]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-74
[ATE3D-75]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-75
[ATE3D-76]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-76
[ATE3D-77]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-77
[ATE3D-89]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-89
[ATE3D-90]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-90
[ATE3D-91]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-91
[ATE3D-92]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-92
[ATE3D-93]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-93
[ATE3D-94]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-94
[ATE3D-95]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-95
[ATE3D-96]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-96
[ATE3D-97]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-97
[ATE3D-99]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-99
[ATE3D-102]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-102
[ATE3D-103]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-103
[ATE3D-104]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-104
[ATE3D-105]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-105
[ATE3D-106]: https://digipen-team-apps-1.atlassian.net/browse/ATE3D-106

---

## Project Scope

Atelier3D is intentionally focused on the core learning experience.

The initial project does **not** focus on:

- Online services
- User accounts
- Monetisation
- Asset marketplaces
- Multiplayer
- Large-scale commercial game development
- High-end production rendering

Classroom features, learning analytics, an AI tutor, custom shaders and additional export targets are **Future** features in the Product Design Document (see [Core Features](#core-features)): outside the MVP, but planned.

---

## Design Philosophy

### Make execution visible

A program should not feel like a black box.

### Connect logic to objects

Users should be able to understand which logic controls an object in the scene.

### Learn progressively

The system should introduce programming concepts gradually.

### Debug by understanding

Errors should help learners understand what went wrong rather than simply exposing technical engine errors.

### Build real things

Programming concepts should be learned through creating interactive 3D projects.

### Keep the engine understandable

The engine should remain small enough that its architecture can be understood by the people learning from it.

---

## What Atelier3D Is Not

Atelier3D is **not intended to replace Unity, Unreal Engine or other professional game engines.**

It is not designed for:

- AAA game development
- Large open-world games
- Advanced multiplayer infrastructure
- Console development
- Production-scale rendering
- Large studio pipelines

Instead, Atelier3D focuses on providing a small and understandable 3D environment where beginners can learn programming and game development.

---

## Competitors

| Product | What it is | How Atelier3D differs |
|---|---|---|
| **Scratch** | Beginner-friendly, block-based programming platform, mainly 2D, with a large educational community | Real-time 3D instead of 2D; block coding **plus** visible execution; runtime visualisation and variable tracking instead of limited debugging; a read-only C++ equivalent; designed specifically for learning how games and programming work |
| **Construct 3** | Browser-based engine for fast, mostly 2D game creation with event sheets | Focus on learning programming rather than making games quickly; live execution visualisation; visual blocks instead of event sheets; Teaching Mode with objectives; a C++ representation |
| **Roblox Studio** | Full 3D creator platform with Lua / Luau scripting, a large asset ecosystem and multiplayer | An educational platform rather than a professional creator platform; block programming instead of scripting; learning by seeing execution; a focused learning environment with Teaching and Sandbox modes |

The full analysis (strengths, weaknesses, opportunities and threats of each) is in the [Product Design Document](docs/design/product-design.md#competitor-analysis).

---

## Vision

Atelier3D aims to make programming concepts visible.

Instead of:

```text
Code
  ↓
???
  ↓
Result
```

the learner should see:

```text
Code
  ↓
Logic
  ↓
Execution
  ↓
Object State
  ↓
3D Result
```

A learner should be able to start with simple blocks, understand how programs execute, transition toward C++, and eventually understand the engine and graphics systems underneath the tools they are using.

> **See the code. See the logic. See the result.**

---

## Documentation

| Document | Contents |
|---|---|
| [Development Setup](https://github.com/L1nceLin/Atelier3D/wiki/Development-Setup) (wiki) | Step-by-step setup on Windows and Ubuntu / WSL2, IDE use, the checks to run before pushing, troubleshooting |
| [Product Design Document](docs/design/product-design.md) | Users and research, competitors, features by release, screens, MVP status, data model, risks |
| [Proposal](docs/proposal.md) | Project proposal: problem, vision, scope, roadmap, feasibility, risks and success criteria |
| [Architecture](docs/architecture.md) | The system as implemented: engine modules, editor, runtime, file formats, tests, technology stack |
| [Deliverables](docs/deliverables.md) | Milestone 1 technical deliverables (T12–T19) and product features (F01–F13): acceptance criteria, demonstrations and Jira keys |
| [Jira backlog](https://digipen-team-apps-1.atlassian.net/jira/software/projects/ATE3D/boards/1/backlog) | Live status of every ticket |
