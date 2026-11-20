# Atelier3D — Project Proposal

## 1. Project Overview

**Project:** Atelier3D  
**Team:** SayReal  
**Module:** GAM300 Project 5 (UXG3400 / CSD3401), Fall 2026  
**Product:** Modular 3D Programming Software  
**Repository:** https://github.com/L1nceLin/Atelier3D  
**Version:** 1.0 — Milestone 1 Proposal

Atelier3D is a custom 3D engine and visual programming environment designed to help beginners learn how games and programming work by making program execution visible.

The application combines a lightweight 3D editor, Scratch-inspired block programming, a custom runtime, live execution visualisation, beginner-oriented debugging, and a gradual path towards C++. The goal is not to compete with professional engines in rendering quality or production features. Instead, Atelier3D is intended to be a learning environment that can also produce small playable 3D games.

---

## 2. Problem Statement

Learning programming can be difficult because program execution is mostly invisible to beginners.

A learner may understand what a block says but still have difficulty understanding:

- Which instruction is currently running.
- Why a condition did or did not execute.
- Which object is controlled by a piece of logic.
- What caused a variable to change.
- Why an object behaved differently from what was expected.

Traditional block-programming tools make syntax easier to learn, but the transition to traditional programming can still feel disconnected. Professional game engines provide powerful 3D tools, but their complexity can make them difficult for beginners.

Atelier3D addresses this gap by connecting programming logic directly to visible 3D behaviour.

---

## 3. Target Users

### Primary Users

Secondary-school students, approximately ages 13–17, who are interested in games, technology, digital art, or creative projects and may have little or no traditional programming experience.

They need:

- Visual programming.
- Understandable terminology.
- Beginner-friendly tutorials.
- Immediate visual feedback.
- Safe experimentation.
- Clear error recovery.

### Secondary Users

Parents, guardians, and educators who support learners exploring programming and game development.

---

## 4. Product Vision

The core design principle is that the learner should be able to answer three questions without leaving the 3D view:

1. **What is controlling this object?**
2. **What is happening right now?**
3. **What changed?**

Atelier3D therefore connects:

**Code → Logic → Object → Execution → Result**

The application is structured around two modes.

### Teaching Mode

Teaching Mode provides prepared scenes, stated learning objectives, restricted block palettes, and automatic validation. Lessons introduce programming concepts progressively.

The intended lesson sequence covers concepts such as:

- Objects and transforms
- Events
- Variables
- Conditions
- Loops
- Functions
- Object interaction
- Collision and physics
- Small game systems

### Sandbox Mode

Sandbox Mode removes the teaching constraints. Learners can start from an empty project or template, import assets, build scenes, create block logic, test and debug their projects, and export them.

---

## 5. Core Features

The proposed MVP includes:

- 3D scene editing.
- Object creation, selection, transformation, duplication, and deletion.
- Lighting and camera controls.
- Textured materials.
- Block-based programming.
- Runtime execution inside the editor.
- Execution highlighting.
- In-world debugging.
- Pause, step execution, and breakpoints.
- Beginner-friendly error messages.
- Object inspection.
- Asset importing.
- Teaching Mode with validated lessons.
- Sandbox Mode.
- Project save/load.
- Game executable export for the platform the editor runs on (Windows or Linux).

The MVP supports OBJ and FBX models, PNG/JPG textures, and WAV/OGG audio. The project deliberately keeps the initial asset and rendering scope small to control development complexity.

---

## 6. Milestone 1 Technical Deliverables

Milestone 1 focuses primarily on the engine foundations represented by **T12–T19**.

### T12 — Basic 3D Rendering Pipeline

The rendering pipeline initializes the graphics API and renders into the editor viewport.

It supports:

- Built-in meshes.
- Imported models.
- Materials with base colour and diffuse textures.
- Directional and point lighting.
- Blinn-Phong lighting.
- Separate editor and game cameras.
- Main-camera selection for Play mode.

The renderer provides the visual foundation required by the scene editor and asset workflow.

### T13 — Serializer / Deserializer

The serializer provides reliable project persistence.

It saves:

- Entities.
- Components.
- Property values.
- Asset references.
- A format version.

Scenes can be loaded back and tested through a save → load → save round trip. Missing or corrupt files should generate errors without crashing the editor.

### T14 — Resource Manager

The resource manager handles asset loading and reuse.

Supported resources include:

- OBJ/FBX models.
- PNG/JPG textures.
- Shaders.
- WAV/OGG audio.

The system caches resources, uses reference counting, handles missing assets with placeholders, and provides the asset list used by the editor.

### T15 — Engine Logging System

The logging system provides a common diagnostic interface for the engine.

It supports:

- Debug.
- Info.
- Warning.
- Error.

Each entry contains a timestamp, level, and category. Logs are available through the editor console, terminal, and per-session log files.

### T16 — Entity Component System

The ECS manages game objects and their components.

It supports entities with components including:

- Transform.
- MeshRenderer.
- Light.
- Camera.
- AudioSource.
- PlayerController.

The ECS provides the foundation for rendering, audio, and player-control systems.

### T17 — Basic 3D Audio System

The audio system provides playback for WAV and OGG clips.

It supports:

- Play.
- Pause.
- Stop.
- Looping.
- Per-source volume.
- Master volume.
- 3D positional audio.
- Background music.

The active camera acts as the listener.

### T18 — Scene Editor Interface

The editor provides the main user-facing workspace using an ImGui-based interface.

The interface includes:

- Viewport.
- Hierarchy.
- Inspector.
- Assets.
- Console.

The editor supports scene creation, object selection, renaming, duplication, deletion, component editing, save/load, and Play/Stop controls.

### T19 — Keyboard / Mouse Input System

The input system handles:

- Key pressed.
- Key held.
- Key released.
- Mouse position.
- Mouse movement.
- Mouse scroll.

Input actions are mapped through `input_actions.json`, allowing controls to be changed without recompiling.

Editor input and game input are separated so that typing into Inspector fields does not accidentally control the camera or player.

---

## 7. Milestone 1 Product Features

The technical deliverables support the following product features.

| Feature | Priority | Main Technical Support |
|---|---|---|
| F01 — Scene Viewing | MUST | T12, T18, T19 |
| F02 — Object Editing | MUST | T12, T16, T18 |
| F03 — Asset Import | MUST | T12, T14 |
| F04 — Save and Load | MUST | T13, T14 |
| F05 — Play Mode | MUST | T16, T19 |
| F06 — Sound and Music | MUST | T14, T17 |
| F07 — Editor Console | MUST | T15, T18 |
| F08 — Editor Windows | MUST | T18 |
| F09 — Add / Remove Components | SHOULD | T16, T18 |
| F10 — Control Remapping | COULD | T19 |
| F11 — Missing-Asset Recovery | COULD | T13, T14, T15 |
| F12 — Performance Overlay | COULD | T12, T16 |
| F13 — Sample Scene | COULD | T12, T13, T14, T17 |

---

## 8. Proposed Technical Architecture

Atelier3D is proposed as a single desktop executable containing an editor layer, runtime layer, and project layer. The editor and runtime operate in the same process so the editor can observe runtime state efficiently.

### Main Components

- **Scene Editor** — object placement, selection, transforms, camera control, viewport rendering, and debugging overlay.
- **Block Editor** — block palette, canvas, connection validation, compilation, and execution highlighting.
- **Object System** — entity/component model for transforms, rendering, collision, audio, variables, and attached logic.
- **Event System** — handles start, update, input, collision, and timer events.
- **Block Virtual Machine** — executes compiled block instructions.
- **Debug Bridge** — sends execution state, variables, events, and errors to the editor.
- **Physics System** — collision and simple rigid-body behaviour for the MVP.
- **Renderer** — forward renderer with textured materials and a small number of lights.
- **Asset Manager** — import, validation, caching, and reference counting.
- **Teaching System** — lessons, palette restrictions, validation, and local completion data.
- **Project System** — versioned project files and save/load functionality.

---

## 9. Technology Stack

The table lists the technology originally proposed and the technology used in the Milestone 1 implementation.

| Area | Originally Proposed | Used in Milestone 1 |
|---|---|---|
| Language | C++17 | C++20 |
| Window/Input | GLFW | Win32 on Windows, GLFW on Linux |
| Graphics API | OpenGL 3.3 Core | OpenGL 3.3 Core (GLEW loader) |
| Editor UI | Dear ImGui | Dear ImGui, ImGuizmo |
| Model Loading | tinyobjloader, later cgltf | Built-in OBJ loader, OpenFBX |
| Image Loading | stb_image | stb_image |
| Audio | miniaudio | FMOD Core (Windows and 64-bit Linux) |
| Mathematics | GLM | GLM |
| Entity Component System | — | EnTT |
| Serialization | JSON | JSON (nlohmann/json) |
| Build System | CMake | CMake |
| Testing | Catch2 or doctest | GoogleTest |

Established libraries are used for windowing, model loading, image decoding, audio, and interface rendering. This reduces unnecessary implementation risk while keeping the engine architecture custom to the project.

---

## 10. Visual Programming Approach

The block language is designed around concepts that a beginner can understand.

### Block Categories

- Events
- Movement
- Physics
- Variables
- Conditions
- Loops
- Objects
- Audio
- Visuals
- Functions
- Game systems

Blocks are attached to objects rather than being placed in one global script area.

Block shapes communicate their roles:

- Event blocks are entry points.
- Statement blocks are stackable.
- Conditions and values fit into appropriate slots.

Illegal connections are prevented during drag-and-drop.

The runtime compiles block graphs into a compact intermediate representation. The virtual machine executes this representation and reports the identifier of the block currently executing. This provides the foundation for execution highlighting, breakpoints, and step debugging.

---

## 11. Visual Debugging

Visual debugging is a central part of the proposal.

### Execution Highlighting

The currently executing block is highlighted while the program runs.

### In-World State Display

A watched object can display:

- Current variables.
- Current event.
- Current block.
- Collision state.

### Step Debugging

Learners can:

- Pause execution.
- Resume execution.
- Execute one block at a time.
- Set breakpoints.

The 3D world updates after each step so the learner can connect an instruction directly to a visible change.

### Beginner Error Messages

Runtime errors are reported with:

- The relevant block.
- The affected object.
- A plain-language explanation.
- The technical error where appropriate.

---

## 12. MVP Scope

The MVP is defined around one central test:

> A learner with no programming experience can complete a short sequence of lessons, then build and export a small original game.

The MVP includes:

- Scene creation and editing.
- Primitive objects.
- Directional lighting.
- A camera.
- Textured materials.
- Basic collision and physics.
- Events, movement, variables, conditions, loops, objects, audio, visuals, and functions.
- Run/Stop runtime.
- Block-level execution reporting.
- Execution highlighting.
- Floating debug panel.
- Pause, stepping, and breakpoints.
- Plain-language errors.
- Object inspector.
- OBJ/FBX, PNG/JPG, and WAV/OGG assets.
- Eight to ten validated Teaching Mode lessons.
- Sandbox Mode.
- Save/load.
- Game executable export for the platform the editor runs on (Windows or Linux).
- Undo/redo.
- Autosave and crash-safe project saving.
- Automated tests for the virtual machine and file format.

---

## 13. Out of Scope for the MVP

The following features are deferred to later development:

- User-authored C++.
- Generated C++ view, which the learner can swap with the block view to see the same logic as C++.
- Logic dependency map.
- Logic visualiser showing the logic attached to each object.
- Level visualiser giving an overview of a game's levels.
- Execution history.
- Progressive hints.
- Reusable logic components that can be shared between multiple levels.
- Shader authoring.
- Animation.
- In-game UI widgets.
- Cross-platform export (building a game for a different operating system than the editor runs on), Android, and web export.
- Classroom assignment/submission systems.
- Learning analytics.
- AI tutor.
- Multiplayer.
- Online project sharing.

These exclusions are intended to protect the core learning and engine objectives from scope expansion.

---

## 14. Development Roadmap

| Phase | Activity | Approx. Duration | Deliverable |
|---|---|---:|---|
| 1 | Requirements, research, system design | 4 weeks | Design document, block specification, project format |
| 2 | Core engine and scene editor | 8 weeks | Rendering scene with objects, camera, lighting, transforms |
| 3 | Object system and asset pipeline | 5 weeks | Asset import, Inspector, project load/save |
| 4 | Block programming system | 8 weeks | Palette, canvas, connection rules, IR compilation |
| 5 | Runtime and execution visualisation | 6 weeks | In-editor runtime and block highlighting |
| 6 | Debugging and inspection | 5 weeks | Debugger, stepping, breakpoints, error messages |
| 7 | Teaching Mode | 5 weeks | Eight to ten validated lessons |
| 8 | Sandbox Mode and export | 4 weeks | Free creation and game export (Windows and Linux) |
| 9 | C++ view | 4 weeks | Conditional; read-only generated C++, swappable with the block view |
| 10 | Testing, user trials, optimisation, documentation | 5 weeks | Test report, trial findings, user guide, final build |

The PDD estimates approximately 54 weeks including the conditional C++ phase and describes the schedule as tight but workable if the scope is maintained.

---

## 15. Feasibility

### Technical Feasibility

The main technical components are standard areas of undergraduate C++ development:

- Forward rendering.
- Entity-component architecture.
- Collision detection.
- Virtual-machine execution.
- Serialization.
- Desktop UI.

The proposal reduces risk by using established libraries rather than implementing loaders, audio, and UI infrastructure from scratch.

The block system also compiles into an intermediate representation rather than generating and compiling C++ at runtime. This avoids requiring a compiler on the user's machine and simplifies debugging.

### Resource Requirements

- Standard laptop or desktop with OpenGL 3.3 support.
- C++ toolchain.
- CMake.
- IDE and version control.
- Two or three test machines with different specifications.
- Six to ten beginner usability participants.
- One or two teachers for review.
- Freely licensed sample assets.

The proposed budget is effectively zero beyond existing hardware.

---

## 16. Risks and Mitigation

| Risk | Impact | Likelihood | Mitigation |
|---|---|---|---|
| Engine development consumes schedule | High | High | Fix visual target early and use established libraries |
| Scope creep | High | High | Maintain a fixed MVP and require additions to replace existing scope |
| VM defects cause confusing behaviour | High | Medium | Automated tests and reference projects |
| Debugging reduces performance | Medium | Medium | Throttle reporting and sample state |
| C++ integration takes longer than expected | Medium | Medium | Keep the C++ phase conditional |
| Asset import failures | Medium | High | Validate imports and handle bad input without crashing |
| Editor becomes intimidating | High | Medium | Usability testing and progressive disclosure |
| Teaching content takes too long | Medium | High | Define lesson format early and author lessons in parallel |
| Validation rejects correct solutions | High | Medium | Validate observable behaviour with tolerances and multiple correct solutions |
| Data loss | High | Medium | Atomic saves, autosave, versioned project format |
| Team member unavailability | High | Low | Frequent commits, documentation, and contingency |
| Learners do not find visualisation useful | High | Low | Test the hypothesis early with prototypes and usability trials |

---

## 17. Success Criteria

The project will evaluate success through measurable functional and educational criteria.

### Educational Criteria

1. A beginner with no programming background can create an interactive 3D scene within one session.
2. A learner can complete the first five lessons without researcher assistance.
3. A learner can identify the highlighted block and explain what the program is doing at that moment.
4. A learner can use the debugger to find the cause of a deliberately introduced fault.

### Technical Criteria

5. A Sandbox project can be exported to a standalone executable (Windows or Linux) that runs without the editor.
6. The editor maintains an interactive frame rate with debugging enabled on a scene containing at least 50 objects.
7. Save, load, crash, and recovery tests do not result in data loss.

### Evaluation

The PDD proposes usability sessions with approximately six to ten beginners and review by one or two teachers.

The educational value of visible execution is treated as a hypothesis to test rather than a pre-existing measured result.

---

## 18. Expected Deliverables

The project is expected to produce:

### Software

- Working Atelier3D application for Windows and Linux.
- 3D scene editor.
- Block programming system.
- Instrumented runtime.
- Beginner debugger.
- Teaching Mode.
- Sandbox Mode.
- Game executable export for the platform the editor runs on (Windows or Linux).

### Documentation

- Product Design Document.
- Block language specification.
- Technical architecture documentation.
- Test report.
- User guide.
- Usability trial findings.

### Milestone 1 Focus

For the current milestone, the primary implementation focus is **T12–T19** and the associated **F01–F13** product features. These establish the rendering, serialization, resources, logging, ECS, audio, editor UI, input, and core user-facing editor workflows required for the project foundation.

---

## 19. Proposed Demonstration Flow

A practical demonstration of the current system should follow the user's workflow:

1. Launch Atelier3D.
2. Open the main editor windows.
3. Create a 3D scene.
4. Add and transform objects.
5. Add a camera and lighting.
6. Import a model and texture.
7. Assign assets to an object.
8. Demonstrate the Inspector.
9. Save the scene.
10. Reload the scene and verify the state.
11. Demonstrate the Console and logging system.
12. Add and configure an audio source.
13. Demonstrate 3D audio.
14. Press Play.
15. Demonstrate player/game input.
16. Press Stop and return to the editor.
17. Demonstrate component add/remove where available.
18. Demonstrate input remapping where available.
19. Demonstrate missing-asset recovery.
20. Show the performance overlay and sample scene where available.

This flow demonstrates the connection between the technical engine systems and the product features without requiring later-stage features such as user-authored C++.

---

## 20. Conclusion

Atelier3D proposes a focused learning environment that combines 3D creation with visual programming and visible program execution.

The project addresses a specific learning problem: beginners can see what their blocks say, but may struggle to understand what the program is doing at runtime. By connecting blocks, objects, execution state, and visible 3D results in one environment, Atelier3D aims to make programming behaviour easier to observe and reason about.

The MVP therefore prioritises the core engine, editor, block runtime, visual debugging, Teaching Mode, Sandbox Mode, project persistence, and game export for Windows and Linux. More complex capabilities such as C++ authoring, shaders, analytics, multiplayer, and Android and web export are deliberately deferred.

The proposal's central hypothesis is testable: if learners can see program execution directly in the 3D world, they may be better able to understand and debug the logic they create. The project's usability trials will provide evidence for or against that hypothesis rather than assuming the outcome in advance.

