# Portfolio

A collection of game development coursework completed at DigiPen Institute of Technology.
Each project below lists the libraries used, the languages, my main roles, and a short description of the game.

---

## GAM100 — CaveBound

| | |
|---|---|
| **Language** | C |
| **Libraries** | CProcessing (DigiPen's functional game-development library, built on NanoVG for 2D rendering and FMOD for audio) |
| **My Role** | Physics & map generation |

**About the game**
CaveBound is a 2D platformer in which the player explores a mysterious cave and switches between two unique characters on the fly to solve puzzles, defeat enemies, and escape traps. Each character has distinct abilities, making strategic switching the key to progressing through the levels. The game features platforming physics, collectables, enemy and boss encounters, bullets, and a leaderboard.

---

## GAM200 — OceanBound

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

## GAM250 — Taken (Echoes of Arena)

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
