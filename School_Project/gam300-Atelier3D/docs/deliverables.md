# Milestone 1 Deliverables

This document lists the project-specific technical deliverables (T12–T19) and product features (F01–F13) for Milestone 1.

## Technical Deliverables

### T12 — Basic 3D Rendering Pipeline

**Owner:** Kye Le WOH  
**Jira:** ATE3D-99 (Story: ATE3D-35)  
**Status:** Done (verification pending: 60 FPS with 100 textured objects on a lab workstation not yet measured)  
**Supports:** F01, F03

#### Acceptance Criteria
- Initializes the graphics API and renders into the editor Viewport.
- Renders built-in meshes:
  - Cube
  - Sphere
  - Plane
- Renders imported models.
- Supports materials with:
  - Base colour
  - Diffuse texture
- Supports:
  - 1 directional light
  - Up to 8 point lights
- Uses Blinn-Phong lighting.
- Provides separate editor and game cameras.
- The camera marked as `main` is used in Play mode.
- Maintains 60 FPS with 100 textured objects on a school lab workstation.

#### Demonstration
- Display 100 textured objects.
- Use 1 directional light and 4 point lights.
- Switch between the editor and game cameras.
- Show the FPS counter.

---

### T13 — Serializer / Deserializer

**Owner:** Ko Jat CHONG  
**Jira:** ATE3D-99 (Story: ATE3D-15)  
**Status:** Done  
**Supports:** F04

#### Acceptance Criteria
- Saves a scene containing:
  - Every entity
  - All components
  - All property values
  - Asset references as file paths
- Saves the scene to a readable `.json` file.
- Includes a format version number.
- Loads the saved scene back identically.
- Passes the round-trip check:

`save → load → save`

The resulting file should be equivalent to the original file.

- Missing files, corrupt files, and missing fields:
  - Are logged as errors.
  - Do not crash the editor.
  - Use appropriate default values where necessary.

#### Demonstration
- Build a scene.
- Save it.
- Close and reopen the editor.
- Load the saved scene.
- Open a corrupted scene file and demonstrate error handling.
- Show the automated round-trip test.

---

### T14 — Resource Manager

**Owner:** Julius Junshen CAI  
**Jira:** ATE3D-99 (Story: ATE3D-14)  
**Status:** Done  
**Supports:** F03, F04, F06

#### Acceptance Criteria
- Loads:
  - `.obj` / `.fbx` models
  - `.png` / `.jpg` textures
  - Shaders
  - `.wav` / `.ogg` audio clips
- Loads assets using file paths.
- Caches assets so the same file is loaded from disk only once.
- Uses reference counting.
- Releases assets when no objects use them.
- Replaces missing assets with placeholders:
  - Magenta texture
  - Default cube
- Logs a warning when an asset is missing.
- Provides the asset list shown in the Assets window.

#### Demonstration
- Import one model and texture.
- Assign them to 10 objects.
- Show that the asset is loaded once with a reference count of 10.
- Rename or move the texture file.
- Show the placeholder and warning message.

---

### T15 — Engine Logging System

**Owner:** Henrey HU  
**Jira:** ATE3D-99 (Story: ATE3D-57)  
**Status:** Done  
**Supports:** F07

#### Acceptance Criteria
- Supports:
  - Debug
  - Info
  - Warning
  - Error
- Every log entry includes:
  - Timestamp
  - Level
  - Category
- Supported categories include:
  - Render
  - Audio
  - IO
  - ECS
  - Editor
  - Input
- Engine modules can log through common macros:
  - `LOG_INFO`
  - `LOG_WARN`
  - `LOG_ERROR`
- Logs are output to:
  - Editor Console
  - Terminal
  - Per-session log file in `logs/`
- Minimum log level can be changed at runtime.

#### Demonstration
- Trigger messages from multiple systems.
- Filter messages by level in the Console.
- Show the same entries in the session log file.

---

### T16 — Entity Component System

**Owner:** Hong Chieh Jethro SUNG  
**Jira:** ATE3D-99 (Story: ATE3D-59)  
**Status:** Done (verification pending: 1,000 entities at 60 FPS not yet measured)  
**Supports:** F02, F05

#### Acceptance Criteria
- Creates and destroys entities with unique IDs.
- Supports adding, removing, and retrieving components.
- Provides these component types:
  - Transform
  - MeshRenderer
  - Light
  - Camera
  - AudioSource
  - PlayerController
- Provides these systems:
  - RenderSystem
  - AudioSystem
  - PlayerControllerSystem
- Handles 1,000 entities at 60 FPS.
- Unit tests cover:
  - Entity creation/destruction
  - Component addition/removal

#### Demonstration
- Spawn 1,000 entities at runtime.
- Add and remove components through the Inspector.
- Show the relevant system reacting immediately.

---

### T17 — Basic 3D Audio System

**Owner:** Thant Win KAUNG  
**Jira:** ATE3D-99 (Story: ATE3D-89)  
**Status:** Done  
**Supports:** F06

#### Acceptance Criteria
- Built on FMOD.
- Loads and plays `.wav` and `.ogg` clips.
- Supports:
  - Play
  - Pause
  - Stop
  - Loop
- Supports per-source volume.
- Supports master volume.
- 3D audio:
  - Becomes quieter with distance.
  - Pans left/right relative to the listener.
- The active camera acts as the listener.
- Background music uses a separate non-3D channel.
- Missing audio clips are logged without crashing.

#### Demonstration
- Add a looping 3D sound to an object.
- Move the camera around the object.
- Demonstrate changes in volume and stereo panning.
- Play looping background music.
- Change the audio volume live in the Inspector.

---

### T18 — Scene Editor Interface

**Owner:** San Ni GO  
**Jira:** ATE3D-99 (Story: ATE3D-60)  
**Status:** Done (verification pending: editor-only demonstration not yet recorded)  
**Supports:** F01, F02, F07, F08

#### Acceptance Criteria
- Uses an ImGui-based editor.
- Provides dockable windows:
  - Viewport
  - Hierarchy
  - Inspector
  - Assets
  - Console
- Windows can be opened/closed from a Window menu.
- Editor layout is remembered between sessions.
- Hierarchy supports:
  - Create
  - Select
  - Rename
  - Duplicate
  - Delete
- Inspector:
  - Displays components of the selected entity.
  - Allows component properties to be edited.
  - Supports Add Component.
  - Supports Remove Component.
- Menu bar provides:
  - File > New
  - File > Open
  - File > Save
  - File > Save As
- Toolbar provides:
  - Play
  - Stop

#### Demonstration
- Start from an empty scene.
- Build a small level using only the editor.
- Edit objects and components through the Inspector.
- Save the scene.
- Press Play.

---

### T19 — Keyboard / Mouse Input System

**Owner:** Shi Wei BEH  
**Jira:** ATE3D-99 (Story: ATE3D-10)  
**Status:** Done  
**Supports:** F01, F05

#### Acceptance Criteria
- Reads keyboard and mouse input as:
  - Pressed
  - Held
  - Released
- Reads:
  - Mouse position
  - Mouse movement
  - Mouse scroll
- Supports named actions such as `MoveForward = W`.
- Loads input mappings from `input_actions.json`.
- Allows keys to be remapped without recompiling.
- Separates editor input from game input.
- Typing in an Inspector field must not move the camera or player.

#### Demonstration
- Fly the editor camera.
- Move the player in Play mode.
- Change a key binding in `input_actions.json`.
- Press Play again (the file is re-read each time Play starts).
- Demonstrate the new key mapping.

---

# Product Features

## F01 — Scene Viewing

**Priority:** MUST  
**Owner:** Kye Le WOH  
**Jira:** ATE3D-61 (Story: ATE3D-90)  
**Status:** Done  
**Supported by:** T12, T18, T19

### User Capability
Open the editor and fly the camera around the 3D scene using right-click + WASD, with scroll-to-zoom and objects lit by a directional light.

### Demonstration
- Open the editor.
- Load or create a 3D scene.
- Hold right-click and use WASD to move the editor camera.
- Use the mouse wheel to zoom.
- Show objects receiving directional lighting.

---

## F02 — Object Editing

**Priority:** MUST  
**Owner:** Ko Jat CHONG  
**Jira:** ATE3D-61 (Story: ATE3D-91)  
**Status:** Done  
**Supported by:** T12, T16, T18

### User Capability
Create objects from the Hierarchy panel and edit their properties through the Inspector.

Supported objects:
- Empty
- Cube
- Sphere
- Plane
- Light
- Camera

Supported operations:
- Select
- Rename
- Duplicate
- Delete

Supported component properties:
- **Transform:** Position, rotation, scale
- **Renderable** (the ECS `MeshRenderer`): Mesh, material colour, texture
- **Light:** Type, colour, intensity, range
- **Camera:** Field of view, near/far clip, main camera toggle
- **Audio Source:** Clip, volume, loop, 3D on/off, play on start

### Demonstration
- Create multiple objects.
- Select objects through the Hierarchy.
- Edit their properties in the Inspector.
- Duplicate, rename, and delete objects.

---

## F03 — Asset Import

**Priority:** MUST  
**Owner:** Julius Junshen CAI  
**Jira:** ATE3D-61 (Story: ATE3D-92)  
**Status:** Done  
**Supported by:** T12, T14

### User Capability
Import `.obj` / `.fbx` models and `.png` / `.jpg` textures into the Assets panel and assign them to an object's mesh and material.

### Demonstration
- Import a model.
- Import a texture.
- Display both in the Assets panel.
- Assign the model and texture to an object.
- Show the textured object in the Viewport.

---

## F04 — Save and Load

**Priority:** MUST  
**Owner:** Ko Jat CHONG  
**Jira:** ATE3D-61 (Story: ATE3D-93)  
**Status:** Done  
**Supported by:** T13, T14

### User Capability
Save a scene to a `.json` file and reopen it with all objects, property values, and asset links restored.

Broken or missing files should display an error instead of crashing.

### Demonstration
- Create a scene.
- Save it as JSON.
- Close the editor.
- Reopen the editor.
- Load the scene.
- Verify that objects and properties are restored.
- Test a broken or missing file.

---

## F05 — Play Mode

**Priority:** MUST  
**Owner:** Hong Chieh Jethro SUNG  
**Jira:** ATE3D-61 (Story: ATE3D-94)  
**Status:** Done  
**Supported by:** T16, T19

### User Capability
Press Play to run the scene from the game camera, move a player using WASD and the mouse, and press Stop to return to the editor with the scene reset.

### Demonstration
- Set a main/game camera.
- Press Play.
- Move the player using keyboard and mouse.
- Press Stop.
- Verify that the editor returns to its pre-Play state.

---

## F06 — Sound and Music

**Priority:** MUST  
**Owner:** Thant Win KAUNG  
**Jira:** ATE3D-61 (Story: ATE3D-95)  
**Status:** Done  
**Supported by:** T14, T17

### User Capability
Add an Audio Source to an object, select a `.wav` / `.ogg` clip, and configure:
- Volume
- Loop
- 3D on/off

In Play mode:
- 3D sounds become quieter with distance.
- 3D sounds pan as the camera moves.
- Background music loops.

### Demonstration
- Add an Audio Source.
- Select an audio clip.
- Enable looping.
- Enable 3D audio.
- Move the camera around the sound source.
- Demonstrate volume and panning changes.
- Play looping background music.

---

## F07 — Editor Console

**Priority:** MUST  
**Owner:** Henrey HU  
**Jira:** ATE3D-61 (Story: ATE3D-96)  
**Status:** Done  
**Supported by:** T15, T18

### User Capability
View timestamped Info, Warning, and Error messages in the Console panel.

The user can:
- Filter messages by level.
- Clear the Console.
- Find the same messages in a log file.

### Demonstration
- Trigger Info, Warning, and Error messages.
- Show timestamps and categories.
- Filter the Console.
- Clear the Console.
- Open the corresponding session log file.

---

## F08 — Editor Windows

**Priority:** MUST  
**Owner:** San Ni GO  
**Jira:** ATE3D-61 (Story: ATE3D-97)  
**Status:** Done  
**Supported by:** T18

### User Capability
Open, close, resize, and dock:
- Viewport
- Hierarchy
- Inspector
- Assets
- Console

The layout is remembered the next time the editor opens.

### Demonstration
- Open the Window menu.
- Toggle editor windows.
- Resize and dock windows.
- Close and reopen the editor.
- Verify that the previous layout is restored.

---

## F09 — Add / Remove Components

**Priority:** SHOULD  
**Owner:** Hong Chieh Jethro SUNG  
**Jira:** ATE3D-61 (Story: ATE3D-102)  
**Status:** Done  
**Supported by:** T16, T18

### User Capability
Add or remove the following components from any object through the Inspector:
- Renderable (Mesh Renderer)
- Light
- Camera
- Audio Source
- Player (Player Controller)

The object should react immediately to component changes.

### Demonstration
- Select an object.
- Add a component.
- Configure it.
- Remove the component.
- Show the corresponding behaviour changing immediately.

---

## F10 — Control Remapping

**Priority:** COULD  
**Owner:** Shi Wei BEH  
**Jira:** ATE3D-61 (Story: ATE3D-103)  
**Status:** Done  
**Supported by:** T19

### User Capability
Change the keys used to move the camera and player by editing `input_actions.json`.

The new mappings take effect the next time Play is pressed; no restart is needed.

Typing into an Inspector field must not move the camera or player.

### Demonstration
- Open `input_actions.json`.
- Change a camera or player key.
- Press Play.
- Demonstrate the new key.
- Type in an Inspector field and verify that movement does not occur.

---

## F11 — Missing-Asset Recovery

**Priority:** COULD  
**Owner:** Julius Junshen CAI  
**Jira:** ATE3D-61 (Story: ATE3D-104)  
**Status:** Done  
**Supported by:** T13, T14, T15

### User Capability
If a model, texture, or audio clip is moved or renamed, the scene still opens.

A missing asset is replaced with:
- A magenta texture, or
- A default cube

The Console identifies the missing file.

### Demonstration
- Create a scene using an external asset.
- Save the scene.
- Move or rename the asset.
- Reopen the scene.
- Show the placeholder.
- Show the corresponding Console warning/error.

---

## F12 — Performance Overlay

**Priority:** COULD  
**Owner:** Kye Le WOH  
**Jira:** ATE3D-61 (Story: ATE3D-105)  
**Status:** Done  
**Supported by:** T12, T16

### User Capability
Turn on an on-screen performance overlay showing:
- FPS
- Entity count

### Demonstration
- Enable the overlay.
- Show FPS while the scene is running.
- Show the current entity count.
- Demonstrate the overlay with a larger scene.

---

## F13 — Sample Scene

**Priority:** COULD  
**Owner:** Ko Jat CHONG  
**Jira:** ATE3D-61 (Story: ATE3D-106)  
**Status:** Done  
**Supported by:** T13, T14, T12, T17

### User Capability
Open a bundled sample level containing:
- Textured props
- Lights
- Looping music
- A player

The user can press Play without building the scene first.

### Demonstration
- Open the bundled sample scene.
- Verify that assets and lighting are already configured.
- Verify that background music is playing/looping.
- Press Play.
- Move the player.

---

# M1 Demonstration Flow

The recommended demonstration should follow the main user journey:

1. Launch the Atelier3D editor.
2. Open the Viewport, Hierarchy, Inspector, Assets, and Console windows.
3. Create a small 3D scene.
4. Add cubes, spheres, planes, lights, and a camera.
5. Edit object properties in the Inspector.
6. Import a model and texture.
7. Assign the imported assets to an object.
8. Save the scene to a `.json` file.
9. Reload the scene and verify that the scene is restored.
10. Demonstrate the Console and logging system.
11. Add an Audio Source and demonstrate 3D audio/background music.
12. Press Play.
13. Demonstrate the game camera and player movement.
14. Press Stop and verify the scene returns to its editor state.
15. Demonstrate optional features:
    - Component add/remove
    - Input remapping
    - Missing-asset recovery
    - Performance overlay
    - Sample scene

# Traceability Summary

| Technical Deliverable | Supported Features |
|---|---|
| T12 — Basic 3D Rendering Pipeline | F01, F03 |
| T13 — Serializer / Deserializer | F04 |
| T14 — Resource Manager | F03, F04, F06 |
| T15 — Engine Logging System | F07 |
| T16 — Entity Component System | F02, F05 |
| T17 — Basic 3D Audio System | F06 |
| T18 — Scene Editor Interface | F01, F02, F07, F08 |
| T19 — Keyboard / Mouse Input System | F01, F05 |

| Product Feature | Priority | Supported Technical Deliverables |
|---|---|---|
| F01 — Scene viewing | MUST | T12, T18, T19 |
| F02 — Object editing | MUST | T12, T16, T18 |
| F03 — Asset import | MUST | T12, T14 |
| F04 — Save and load | MUST | T13, T14 |
| F05 — Play mode | MUST | T16, T19 |
| F06 — Sound and music | MUST | T14, T17 |
| F07 — Editor console | MUST | T15, T18 |
| F08 — Editor windows | MUST | T18 |
| F09 — Add / remove components | SHOULD | T16, T18 |
| F10 — Control remapping | COULD | T19 |
| F11 — Missing-asset recovery | COULD | T13, T14, T15 |
| F12 — Performance overlay | COULD | T12, T16 |
| F13 — Sample scene | COULD | T13, T14, T12, T17 |
