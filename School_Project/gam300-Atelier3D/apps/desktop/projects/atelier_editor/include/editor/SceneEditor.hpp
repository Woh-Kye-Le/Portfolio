/* !
@file    	    SceneEditor.hpp
@author  	    sanni.go@digipen.edu [4%] 
         	    w.kyele@digipen.edu [78%]
         	    c.kojat@digipen.edu [13%]
         	    zhihenghenrey.hu@digipen.edu [3%]
         	    k.thantwin@digipen.edu [2%]
@date    		30/09/2026
@brief
    Declares SceneEditor, the main editor UI: menu bar, Play/Pause/Stop, the
    docked windows (Viewport, Hierarchy, Inspector, Assets, Console, ...), the
    Modes page, scene files, undo/redo and the game runtime shown in the
    Viewport.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// GameRuntime first: its graphics headers must come before Input.hpp's GLFW header (which includes GL/gl.h)
#include "engine/game/GameRuntime.hpp"
#include "engine/graphics/OpenGL/FramebufferGL.hpp"

#include "editor/camera/EditorCamera.hpp"
#include "editor/camera/View2D.hpp"
#include "editor/panels/assets/AssetBrowserPanel.hpp"
#include "editor/panels/assets/AssetsPanel.hpp"
#include "editor/panels/audio/AudioPanel.hpp"
#include "editor/panels/blocks/BlocksPanel.hpp"
#include "editor/panels/blocks/LogicPanel.hpp"
#include "editor/panels/blocks/VariableInspectorPanel.hpp"
#include "editor/panels/console/ConsolePanel.hpp"
#include "editor/panels/controls/ControlsPanel.hpp"
#include "editor/panels/diagnostics/DiagnosticsPanel.hpp"
#include "editor/runtime/GameHud.hpp"

#include "engine/editor/EditorScene.hpp"
#include "engine/editor/TutorialNotes.hpp"
#include "engine/game/PrefabManager.hpp"
#include "engine/graphics/DebugLines.hpp"
#include "engine/history/UndoRedoManager.hpp"
#include "engine/input/Input.hpp"
#include "engine/logic/BreakpointFile.hpp"
#include "engine/ui/ImGuiLayer.hpp"

#include <glm/glm.hpp>
#include <imgui/imgui.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Engine::Core
{
    class MemorySink;
}

namespace Engine::Platform
{
    class Window;
}

namespace Editor::Panels
{
    struct DroppedAsset;
}

namespace Editor
{
    // Home = the Modes page with the tutorial cards, Workspace = the docked editor windows
    enum class EditorView : std::uint8_t
    {
        Home,
        Workspace
    };

    // Two interfaces over the same editor, in the same look:
    //   Simple    for learners: a top bar with Run / Pause / Stop, and one fixed screen of Objects (or Assets, or the
    //             open lesson), the Viewport with the selected object's bar, and the Logic (blocks) editor
    //   Advanced  for people who know commercial engines: the menus and every docked window (Hierarchy, Console,
    //             Diagnostics, Audio, Controls, ...)
    // The choice is remembered in imgui.ini.
    enum class InterfaceMode : std::uint8_t
    {
        Simple,
        Advanced
    };

    // Drag and drop of a prefab file (its path relative to the game folder, with its terminating '\0'): from the
    // Hierarchy's Prefabs list onto the Hierarchy or the Viewport, to place an instance there
    inline constexpr const char* kPrefabPayload = "ATELIER_PREFAB";

    // The Viewport's transform gizmo (W / E / R; Q hides it)
    enum class GizmoTool : std::uint8_t
    {
        None,
        Move,
        Rotate,
        Scale
    };

    // The editor window: a top bar with Run / Pause / Stop (under the menus in the Advanced interface),
    // the Home page (Learn: the lessons; Create: games), and the workspace, which the Simple interface shows as one
    // fixed screen (see InterfaceMode) and the Advanced interface docks:
    //   Viewport        the game in 3D with a ground grid. Edit mode: click to select, move / rotate / scale gizmos
    //                   (W / E / R), a view cube (top right) to look from the Front / Top / ..., and a scene-view
    //                   camera (right mouse + WASD fly, Alt + left orbit, middle pan, scroll zoom, F frame). Play
    //                   runs the game (the move keys drive the player). A 2D view shows the scene from the front
    //                   over a grid (drag to move, middle / right drag to pan, wheel to zoom, Ctrl snaps to the grid).
    //   Hierarchy       the scene's game objects as a tree (drag one onto another to put it under it); Create Game
    //                   Object (empty), Add Player, rename / duplicate / delete, Save as Prefab. Prefabs lists the
    //                   game's prefabs, like Unity's: drag an object onto the list to save it as a prefab, drag a
    //                   prefab into the tree or the Viewport (or Place) to add an instance, double-click one to open
    //                   it in Prefab Mode, right-click to make a variant of it, select its instances or delete it;
    //                   instances are blue
    //   Inspector       the selected game object's components, only those it has: Transform, Renderable (shape,
    //                   spin, texture, mesh, shader, audio, image import), Player, Health, Rigid Body; Add Component adds one.
    //                   For part of a prefab instance: Open Prefab, its overrides (each can be applied to the prefab
    //                   or reverted), Apply to Prefab (the whole instance), Unpack
    // Prefab Mode edits one prefab on its own, in place of the scene (which is kept aside, with its undo history):
    // the Hierarchy shows the prefab's root and everything under it, Save (Ctrl+S) writes the prefab file, and
    // < Scene goes back to the scene, whose instances then take every change they did not override. A variant opens
    // as an instance of its base: what it changed shows as overrides (Apply sends one to the base, Revert drops it).
    // Placing a prefab in Prefab Mode nests it: an instance under the root, whose overrides belong to the open prefab.
    // A prefab that already contains the open one cannot be nested in it.
    //   Assets          the game's asset files (click an image or model to use it on the selected object, or drag
    //                   any asset onto an object or an Inspector field); Loaded Assets lists what the scene has loaded
    //   Scenes          the game's scenes
    //   Blocks, Logic   the selected object's blocks; Lua Scripts its script; Variable Inspector its live values
    //   Console         everything logged; Controls the game's input_actions.json; Diagnostics sound and input
    //                   checks; Audio the sound bank; Physics the game's gravity and the ray and distance tests
    // Menus: File (scenes), Edit (undo, copy / paste), Project (new game, refresh, build, controls), Window (views,
    // windows, fullscreen). Shortcuts: F5 Play / Stop, Ctrl+N / O / S scene, Ctrl+Z / Y undo / redo, Ctrl+C / V,
    // Delete, Ctrl+B build, Ctrl+R refresh games, F11 fullscreen, Shift+Space maximize the Viewport.
    // The UI follows the display's scale (DPI) and its text is drawn at the exact size shown, so it stays sharp at
    // any window size or in fullscreen.
    class SceneEditor
    {
    public:
        // The docked workspace. A new name gives everyone the default layout again, in place of the one they saved.
        static constexpr const char* kWorkspaceDock = "AtelierWorkspaceDockV9";

        // consoleSink: where the Console reads log messages from; logDirectory: the log file's folder
        SceneEditor(std::shared_ptr<Engine::Core::MemorySink> consoleSink, std::filesystem::path logDirectory);

        // requestClose: File > Exit
        void Init(Engine::Platform::Window& window, Engine::Audio::AudioSystem& audio, Engine::Core::EventBus& events,
                  std::function<void()> requestClose);
        void Shutdown(Engine::Platform::Window& window);
        void Update(Engine::Platform::Window& window, double deltaSeconds);
        // Playing with the mouse, like a game: a click in the playing view hides the pointer and holds it in the
        // window, so the mouse turns the player with no button held; Esc, Pause, Stop or leaving the window give it
        // back. While the game has it, the editor's UI does not see the mouse.
        void UpdatePointerLock(Engine::Platform::Window& window);

    private:
        // ---- Frame
        void HandleShortcuts();
        void SyncRuntime();
        void UpdateViewportCamera(double deltaSeconds);
        // True while the game places the camera (Play mode, or the Viewport's Game Camera view in edit mode)
        bool UsesGameCamera() const;
        void RenderGame(Engine::Platform::Window& window, double deltaSeconds);

        void DrawLuaPanel();

        std::array<char, 65536> luaDraft_{};
        std::uint32_t luaEditingObject_ = 0;
        std::string luaAppliedSource_;
        std::array<char, 1024> luaFileDraft_{};
        std::string luaAppliedFile_;
        bool focusLua_ = false;

        // Retained after Stop, cleared when changing editable scenes.
        std::set<std::uint64_t> consoleSessions_;

        // ---- Menu bar and popups
        void DrawMenuBar();
        void DrawEditState();
        void DrawScenePathPopup();
        void DrawNewGamePopup();
        void ApplyTheme();
        // The bar under the menus (both interfaces): back to Home, the game's name and whether it is saved, undo /
        // redo, Run / Pause / Stop, the Simple / Advanced switch and Export
        void DrawTopBar();
        void SetInterface(InterfaceMode mode);
        // Remembers the interface in imgui.ini
        void RegisterSettings();

        // ---- Layout
        void DrawMain();
        // Home: Learn (the lessons) or Create (a new game, or one of the games already made)
        void DrawHome();
        void DrawWorkspace();
        // The Simple interface's one screen (see InterfaceMode)
        void DrawSimpleWorkspace();
        void DrawStatusBar();
        void EnsureDefaultDockLayout(unsigned int dockspaceId, float width, float height);

        // ---- Windows
        void DrawViewport();
        void DrawViewportToolbar();
        // While editing: the Screen Elements over the view, as the game shows them (hidden ones faintly). A click
        // selects one, a drag moves it and its corner handle resizes it. True while the mouse is on one (or dragging
        // one), so the view does not pick or turn.
        bool EditScreenElements(const ImVec2& position, const ImVec2& size);
        void DrawViewport3D();
        void DrawViewport2D();
        // The orientation gizmo: a labelled cube with +/- X, Y, Z handles. Clicking a face or handle turns the camera
        // to look from that side, dragging it orbits, Home returns to the default angle. Returns true while the mouse
        // is on it (so the click does not also select in the scene).
        bool DrawViewCube(const glm::mat4& view, const ImVec2& center, float halfSize);
        // Move / rotate / scale gizmo on the selected object; returns true while the mouse is over it or dragging it
        bool DrawTransformGizmo(const glm::mat4& view, const glm::mat4& projection, const ImVec2& position,
                                const ImVec2& size);
        void DrawSelectionOutline(const glm::mat4& viewProjection, const ImVec2& position, const ImVec2& size);
        // The Physics window's test ray, distance check and ground check, drawn over the view
        // (SceneEditorColliders.cpp)
        void DrawPhysicsTests(const glm::mat4& viewProjection, const ImVec2& position, const ImVec2& size);
        void DrawTestRay(ImDrawList& drawList, const glm::mat4& viewProjection, const ImVec2& position,
                         const ImVec2& size) const;
        void DrawTestDistance(ImDrawList& drawList, const glm::mat4& viewProjection, const ImVec2& position,
                              const ImVec2& size) const;
        void DrawTestGround(ImDrawList& drawList, const glm::mat4& viewProjection, const ImVec2& position,
                            const ImVec2& size) const;
        // A badge over every object with block logic: what starts its scripts and, while playing, which run now, its
        // variables and problems; a click opens its blocks. Returns true while the mouse is on one
        // (SceneEditorLogic.cpp)
        bool DrawLogicBadges(const glm::mat4& viewProjection, const ImVec2& position, const ImVec2& size);
        // The object under a point of the viewport (0 if none)
        std::uint32_t PickObject(const glm::mat4& viewProjection, const ImVec2& position, const ImVec2& size,
                                 const ImVec2& mouse) const;
        void FocusSelected();
        void DrawHierarchy();
        // What the Create menu (Hierarchy toolbar, right-click) can add
        enum class CreateKind
        {
            None,
            Empty,
            Group,
            Cube,
            Sphere,
            Pyramid,
            Plane,
            DirectionalLight,
            PointLight,
            Camera,
            Player,
            ScreenText,
            ScreenPicture,
            ScreenBar,
            ScreenButton
        };
        // The Create menu's items (inside a popup or menu); returns the one picked
        CreateKind DrawCreateMenuItems();
        // Adds the object picked in the Create menu, under parent when it is not 0, and selects it
        void CreateObjectOf(CreateKind kind, std::uint32_t parent);
        // The prefab list (in the Assets panel)
        void DrawPrefabs();
        void DrawSavePrefabPopup();
        void DrawDeletePrefabPopup();
        void DrawCreateVariantPopup();
        void DrawPrefabModeBar();
        void DrawClosePrefabPopup();
        void DrawInspector();
        // What the Inspector's components are drawn for: Edit, to change the object, or Live, the Variable Inspector's
        // read-only copy of them that shows the game's values (it has no menus to reset, remove or add, no Import row
        // and no Logic section, starts open, and records where each field is, see FieldBox)
        enum class InspectMode
        {
            Edit,
            Live
        };
        // The object's components and Add Component. simple: the Simple interface's names (Looks, Sound, ...), folded,
        // without the Transform and Logic it shows its own way. In Live mode fields collects where each field was
        // drawn.
        void DrawInspectorComponents(Engine::Editor::GameObject& object, bool simple,
                                     InspectMode mode = InspectMode::Edit,
                                     std::vector<Panels::VariableInspectorPanel::FieldBox>* fields = nullptr);
        // A field's label, and the label to give the field itself: ImGui::DragFloat(Field("Speed").c_str(), ...).
        // AssetField is a text field for an asset path plus, when files is not empty, a picker of the game's files of
        // that kind; with dropKind an asset of that kind can be dropped on it. Returns true if value changed.
        void FieldLabel(const char* label, bool changed = false);
        std::string Field(const char* label, bool changed = false);
        bool AssetField(const char* label, std::string& value, const std::vector<std::string>& files,
                        bool marked = false, std::optional<Panels::AssetKind> dropKind = std::nullopt);
        // One component in the Inspector: a header that folds its fields away, with a "..." menu (not in Live mode) to
        // reset it or remove it
        template <class Component, class DrawFields>
        void ComponentSection(const char* label, bool changed, std::optional<Component>& component,
                              DrawFields&& drawFields);
        // What the Inspector shows of a model (or mesh) file of the game, read again when the file changes
        struct ModelFacts
        {
            std::filesystem::file_time_type written;
            std::vector<std::string> animations;               // its clips' names ("Walk", "Run")
            std::optional<Engine::Assets::BoundingBox> bounds; // the box around it at rest, for its collider
            std::vector<glm::vec3> triangles;                  // its triangles at rest, three corners each: for clicks
        };
        const ModelFacts& ModelFactsOf(const std::string& modelPath) const;
        // The box around what an object draws, in its own units: a model or mesh file's own box (its centre may be off
        // the origin: a figure standing on it), or a built-in shape's
        struct LocalBox
        {
            glm::vec3 center{0.0f};
            glm::vec3 half{0.5f};
        };
        LocalBox BoxOf(const Engine::Scene::RenderableDesc& renderable) const;
        // The Material window: opens a material file of the game to edit; draws it each frame while open
        void OpenMaterialEditor(const std::string& path);
        void DrawMaterialEditor();
        // The Physics window (Project > Physics...): the game's gravity, saved to configs/game.cfg, and while
        // playing the body count and the latest collisions
        void DrawPhysicsSettings();
        void DrawRecentHits();
        // The Physics window's ray test while playing: a ray cast from the selected object every frame, and what it hits
        void DrawRayTest();
        // ...and its distance check: how far the selected object is from another one, and which objects are near it
        void DrawDistanceTest();
        // ...and its ground check: whether the selected object stands on something, with a Jump button to test it
        void DrawGroundTest();
        // The Simple interface's panels: the object list and the open lesson. It has no Inspector: the selected
        // object's bar over the view (its colour, its parts) and its menu (right-click it in the list or the view);
        // what adds or removes objects goes in deferred, run once the list or the view is drawn.
        void DrawSimpleObjects();
        void DrawSimpleObjectMenu(Engine::Editor::GameObject& object, std::function<void()>& deferred);
        void DrawSimpleObjectBar(const ImVec2& position, const ImVec2& size);
        // The Simple interface's Assets tab: the saved objects (prefabs, in a learner's words) to add copies of,
        // then the game's files by kind
        void DrawSimpleStuff();
        void DrawSimpleSavedObjects();
        // While a saved object is being changed (Prefab Mode): the way back to the scene, and Save
        void DrawSimpleSavedObjectBar();
        void DrawLesson();

        // The first-time tour (SceneEditorTour.cpp): the screen darkened round one thing at a time, with words by it
        void DrawTour();
        void StartTour();
        // The tour of the Advanced interface's windows: it stays in the open game and the workspace
        void StartAdvancedTour();
        void EndTour();
        void GoToTourStep(int step);
        // Says where a target of the tour is this frame (Tour.hpp names them)
        void TourMark(const char* target, ImVec2 min, ImVec2 max);

        // The game every shipped lesson happens in, made when first needed: one scene that grows lesson by lesson
        std::filesystem::path LessonGameDir() const;
        bool InLessonGame() const;
        bool IsLessonGame(const std::filesystem::path& dir) const;
        std::filesystem::path FirstOwnGame() const;
        bool OpenLessonGame();
        void OpenLesson(int mode, int index);
        // Back to the first lesson: the editor's own lessons lose the lesson game too (its scene would tick their steps
        // off again); a teacher's only lose the progress, as their game is the learner's own
        void ResetLessons();
        // Ticks off the open lesson's steps from the scene and the running game. Returns whether all are done.
        bool UpdateLesson();
        // The prefab bar at the top of the Inspector. Returns what was clicked (Apply, Open, Unpack; empty if
        // nothing): those can add objects or replace the scene, so the caller runs it once the Inspector is drawn
        std::function<void()> DrawPrefabInstance(const Engine::Editor::GameObject& object);
        // Whether the prefab instance being inspected changed any of these fields of the component being drawn
        bool Changed(std::initializer_list<std::string_view> fields);
        void DrawTransform(Engine::Scene::TransformComponent& transform);
        void DrawRenderable(Engine::Editor::GameObject& object);
        void DrawAudioSource(Engine::Editor::GameObject& object);
        void DrawAssets();
        // Tells the Assets window the game, the selected object and the running scene's assets, for this frame
        void PrepareAssetBrowser();
        // An asset dragged from the Assets window onto an object (Hierarchy row or Viewport): a model becomes its
        // mesh, an image its texture and a sound its Audio Source clip. The component is added when the object has
        // none (with a Transform for a Renderable, so it shows), and the object is selected.
        void UseAsset(Engine::Editor::GameObject& object, const Panels::DroppedAsset& asset);
        void DrawEcsStress();

        // ---- Scenes
        void NewScene();
        void OpenScene();
        void SaveScene();
        void SaveSceneAs();
        bool LoadSceneFile(const std::filesystem::path& path);
        bool SaveSceneFile(const std::filesystem::path& path);
        void LoadProjectScene();
        std::filesystem::path StartScenePath() const;
        std::filesystem::path ResolvePath(const std::string& path) const;
        bool HasUnsavedChanges() const;
        // Runs then (switching games, closing the editor) once unsaved changes are dealt with: straight away if there
        // are none, otherwise after asking Save / Don't save / Cancel (Cancel drops then)
        void AskToSave(std::function<void()> then);
        void DrawUnsavedPopup();
        // Reset lessons (the Home page's learning path): asks, then ResetLessons
        void DrawResetLessonsPopup();

        // ---- Play
        void StartPlay();
        void StopPlay();
        void TogglePause();
        // While paused: Step runs one block of one script, then pauses again (F8)
        void Step();
        // The same, with a My Block run as one step (F10)
        void StepOver();
        // Where the open scene's breakpoints are kept between sessions; empty for a scene with no file
        std::filesystem::path BreakpointFilePath() const;
        void SaveBreakpointFile() const;
        // A breakpoint on or off (the Blocks window's right-click; F04's red dot calls this too)
        void ToggleBreakpoint(std::uint32_t objectId, std::uint32_t blockId);
        std::set<std::uint32_t> BreakpointsOf(std::uint32_t objectId) const;
        // Switched off, or a condition (the Blocks window's right-click)
        void SetBreakpointRule(std::uint32_t objectId, std::uint32_t blockId, Engine::Logic::BreakpointRule rule);
        // Each frame: Pause follows the scripts (a breakpoint pauses them), and a stop is shown in the Blocks window
        void SyncDebugger();
        // The selected object's values as the game runs (F03), drawn in the Inspector while playing and in the
        // Variable Inspector window
        void DrawLiveValues();
        // The Inspector's eye: shows or hides the Variable Inspector window. While it is shown and the game runs, the
        // Inspector shows the saved values (locked) instead of the live ones.
        void DrawFloatingToggle(float size);
        // The Variable Inspector (F03): the same values in a window of their own, floating over the workspace in both
        // interfaces
        void DrawVariableInspector();
        // Selects the object and shows its Blocks window, at the block when blockId is not 0 (the Logic window, the
        // Variable Inspector and the Viewport's logic badges open blocks this way)
        void OpenBlock(std::uint32_t objectId, std::uint32_t blockId);

        // ---- Scenes and games (SceneEditorScenes.cpp)
        // The scene manager: the Scenes window (Advanced) and the Simple interface's Scenes tab. The game's scenes,
        // which one it starts with (START), open one, make, rename, copy and delete them; while playing, which scene
        // the game is in ("go to scene" blocks move it).
        void DrawScenes();
        void DrawScenePopups();
        // Delete Game... (the Home page's game tiles, Project menu): asks, then the folder goes to the Recycle Bin
        void DrawDeleteGamePopup();
        void DeleteGame(const std::filesystem::path& dir);
        enum class SceneNaming : std::uint8_t
        {
            New,
            Rename,
            Duplicate
        };
        SceneNaming sceneNaming_ = SceneNaming::New;
        std::filesystem::path sceneTarget_; // the scene Rename, Duplicate or Delete is for
        std::array<char, 64> sceneName_{};
        std::string sceneError_;
        bool openSceneNaming_ = false;
        bool openSceneDelete_ = false;
        std::filesystem::path deleteGame_; // the game Delete Game... is for
        bool openDeleteGame_ = false;
        bool showScenes_ = true;

        // ---- Projects
        // A game's picture on the Home page: the view, saved small in data/thumbnail.editor.bmp when the scene is
        // saved, on the way Home and when the editor closes (in the workspace, so the view shows the open game)
        struct Thumbnail
        {
            unsigned int texture = 0; // GL texture, 0 when the game has no picture yet
            int width = 0;
            int height = 0;
        };
        void SaveThumbnail();
        const Thumbnail& ThumbnailOf(const std::filesystem::path& project);
        void FreeThumbnails();
        void RefreshProjects();
        void OpenProject(const std::filesystem::path& dir);
        // Opening a game reads its scene's files a few per frame behind a loading screen (StepProjectLoad, from
        // SyncRuntime), then starts its runtime, and keeps the screen up while the first frame sends it to the GPU
        void StepProjectLoad();
        void DrawProjectLoad();
        void CreateGame();
        // Export game: says so, then BuildGameNow builds it a couple of frames later (the editor stops while it does)
        void BuildGame();
        void BuildGameNow();
        void ReloadGame();

        // ---- Modes (tutorials), history, clipboard
        void SaveModes();
        void LoadModes();
        void Undo();
        void Redo();
        // True while the left mouse button is held (a drag in progress)
        bool IsDragging() const;
        // Records the edits made since the last call as one undo step
        void CommitHistory(bool force = false);
        void ResetHistory();
        // Records the object just created as one undo step
        void RecordCreated(std::uint32_t id);
        // Records the gizmo drag that just ended as one undo step
        void FinishGizmoDrag();
        void CopySelected();
        void PasteClipboard();

        // ---- Objects and prefabs
        // Deleting, duplicating and reparenting refuse parts of a prefab instance (the prefab decides those), with a
        // message saying why
        bool CanRestructure(std::uint32_t id, const char* action);
        // Deletes an object and everything under it as one undo step
        void DeleteObject(std::uint32_t id);
        void DuplicateObject(std::uint32_t id);
        void ReparentObject(std::uint32_t id, std::uint32_t parent);
        // Groups: an object to keep others under, so they move (and their blocks act) together. GroupObject puts the
        // object in a new group where it is; UngroupObject moves what is in a group out to the group's own parent,
        // and removes the group if it was only a group (nothing to draw, hear or run).
        void GroupObject(std::uint32_t id);
        // Files dropped on the editor from the desktop's file manager: copied into the Assets window's open folder
        void ImportDropped(const std::vector<std::filesystem::path>& files);
        void UngroupObject(std::uint32_t id);
        bool IsGroup(const Engine::Editor::GameObject& object) const;
        // Groups folded shut in the Simple interface's Objects list
        std::vector<std::uint32_t> collapsedGroups_;
        // Adds an instance of the prefab: with its root at (x, z) of at (keeping the prefab's own height) when given,
        // otherwise next to the instances already placed; under parent (where parent is) when it is not 0
        void PlacePrefab(const std::string& path, std::optional<glm::vec3> at = std::nullopt, std::uint32_t parent = 0);
        // Asks for a name for a new prefab made of the object and everything under it (Save as Prefab)
        void BeginSavePrefab(std::uint32_t id);
        void ApplyPrefab(std::uint32_t id);
        void ApplyPrefabProperty(std::uint32_t id, const std::string& property);
        void RefreshPrefabs();

        // ---- Prefab Mode (see the class comment)
        bool InPrefabMode() const { return prefabMode_.has_value(); }
        // Opens the prefab at path (relative to the game folder) on its own in place of the scene
        void OpenPrefab(const std::string& path);
        bool SavePrefabMode();
        // Back to the scene: asks first when the prefab has unsaved changes
        void ClosePrefab();
        // Back to the scene straight away (unsaved prefab changes are dropped); its instances are updated
        void LeavePrefabMode();
        // A prefab has one root: objects that end up at the top in Prefab Mode (Create, Paste, ...) go under it
        void KeepPrefabUnderRoot();

        // Shows message in the status bar, and for a few seconds over the workspace, and writes it to the log (errors
        // as errors)
        void SetStatus(std::string message, bool error = false);
        // That message over the bottom of the workspace while it is new
        void DrawToast();

        // ---- Services
        Engine::ImGuiLayer::ImGuiLayer imgui_;
        Engine::Input::InputSystem input_;
        Camera::EditorCamera editorCamera_;
        std::shared_ptr<Engine::Core::MemorySink> consoleSink_;
        std::filesystem::path logDirectory_;
        std::function<void()> requestClose_;
        Engine::Platform::Window* window_ = nullptr;

        // ---- Panels hosted as docked windows
        std::unique_ptr<Panels::ConsolePanel> console_;
        std::unique_ptr<Panels::ControlsPanel> controls_;
        std::unique_ptr<Panels::DiagnosticsPanel> diagnostics_;
        std::unique_ptr<Panels::AudioPanel> audio_;
        std::unique_ptr<Panels::BlocksPanel> blocks_; // the selected object's block logic
        std::unique_ptr<Panels::LogicPanel> logic_;   // every object's logic at a glance
        // The selected object's values, live (the Variable Inspector)
        std::unique_ptr<Panels::VariableInspectorPanel> variables_;
        // The game's block library (data/library.blocks.json), shared by every object and scene of the game
        Engine::Logic::Library library_;
        std::filesystem::path libraryProject_; // the game library_ was read from
        // Reads the library when another game is opened
        void SyncLibrary();
        // After a library change: saves it, renames the blocks that run a renamed one, and the game uses it
        void LibraryChanged(std::string_view from, std::string_view to);
        // The engine audio system, handed to GameRuntime each tick so scene
        // objects with an audio asset become positioned emitters.
        Engine::Audio::AudioSystem* audioSystem_ = nullptr;
        // The app's event bus, handed to the Play mode GameRuntime
        Engine::Core::EventBus* events_ = nullptr;
        // The editor's own sounds (SceneEditorSounds.cpp), loaded once from assets/sounds next to the exe
        void LoadEditorSounds();
        // A click for every mouse click on the editor; not on the game while it plays
        void PlayClickSounds();
        void PlayEditorSound(Engine::Audio::SoundId sound) const;
        Engine::Audio::SoundId clickSound_;   // any mouse click on the editor
        Engine::Audio::SoundId attachSound_;  // blocks snapping onto others
        Engine::Audio::SoundId pluckSound_;   // a block pulled out of the palette (Simple interface)
        Engine::Audio::SoundId successSound_; // a step of the open lesson done
        // After UpdateLesson has ticked off newlyDone more steps: plays the success sound when the learner did them,
        // not when a lesson that just opened (or was just reset) finds them already done
        void LessonStepsDone(std::size_t newlyDone);
        std::optional<std::pair<int, int>> heardLesson_; // the lesson (track, index) checked last frame
        std::size_t heardTicks_ = 0;                     // how many of its steps were done then
        Panels::AssetBrowserPanel assetBrowser_;
        Panels::AssetsPanel loadedAssets_;

        // ---- Scene being edited
        Engine::Editor::EditorScene scene_;
        Engine::History::UndoRedoManager history_;          // last 100 operations
        Engine::Editor::EditorScene::State committed_;      // the scene as of the last recorded operation
        std::uint32_t gizmoObject_ = 0;                     // object being dragged with the gizmo
        Engine::Scene::TransformComponent gizmoBefore_;     // its Transform when the drag started
        Engine::Editor::EditorScene::State saved_;          // what the scene file holds, for "unsaved changes"
        std::vector<Engine::Editor::GameObject> clipboard_; // an object and everything under it (EditorScene::CopyOf)
        std::filesystem::path scenePath_;

        // ---- Prefabs of the selected game (prefabList_ is read again after one is saved, and on Refresh)
        Engine::Game::PrefabManager prefabs_;
        std::vector<std::string> prefabList_;
        std::uint32_t savePrefabObject_ = 0; // the object Save as Prefab was picked for
        bool showSavePrefabPopup_ = false;
        std::array<char, 65> prefabName_{};
        std::string prefabError_;
        std::string deletePrefabPath_; // the prefab Delete Prefab... was picked for
        bool showDeletePrefabPopup_ = false;
        std::string variantBase_; // the prefab Create Variant... was picked for
        bool showVariantPopup_ = false;
        std::array<char, 65> variantName_{};
        std::string variantError_;

        // ---- Hierarchy and Inspector
        std::array<char, 64> hierarchySearch_{};
        std::uint32_t renamingId_ = 0; // the object being renamed in the Hierarchy (F2), 0 if none
        std::array<char, 128> renameBuffer_{};
        bool focusRename_ = false;
        std::array<char, 64> addComponentSearch_{};
        // While the Inspector draws a component of a prefab instance: its overrides and the component's key, so the
        // fields it changed are marked (see Changed)
        const std::vector<std::string>* inspectedOverrides_ = nullptr;
        std::string inspectedComponent_;
        // While DrawInspectorComponents draws: what for, and in Live mode where the fields are drawn (liveFields_) and
        // which saved fields the next one shows (set by Changed, taken by FieldLabel)
        InspectMode inspectMode_ = InspectMode::Edit;
        std::vector<Panels::VariableInspectorPanel::FieldBox>* liveFields_ = nullptr;
        std::vector<std::string> nextFields_;

        // ---- Prefab Mode: the open prefab, and the scene it was opened from (restored by LeavePrefabMode)
        struct PrefabModeContext
        {
            std::string path; // the prefab file, relative to the game folder
            std::string name; // the prefab's name (Prefab::name)
            Engine::Editor::EditorScene::State scene;
            std::uint32_t selected = 0;
            Engine::History::UndoRedoManager history;
            Engine::Editor::EditorScene::State committed;
            Engine::Editor::EditorScene::State saved;
            std::filesystem::path scenePath;
        };
        std::optional<PrefabModeContext> prefabMode_;
        bool showClosePrefabPopup_ = false;

        // ---- The selected game: shown in edit mode (not simulating) unless playing
        std::unique_ptr<Engine::Game::GameRuntime> runtime_;
        std::filesystem::path runtimeProject_;         // game runtime_ was made for (empty: make it again)
        Engine::Scene::SceneDescription runtimeScene_; // scene last given to the edit-mode runtime
        struct ProjectLoad
        {
            std::unique_ptr<Engine::Game::GameRuntime> runtime; // the runtime being filled, runtime_ once started
            std::size_t next = 0;                               // the object whose files are read next
            int frames = 0;                                     // frames shown in the current stage
            bool started = false;                               // read and started: waiting for its first frame
        };
        std::optional<ProjectLoad> projectLoad_;
        std::vector<std::filesystem::path> projects_;
        std::filesystem::path projectDir_;
        bool playing_ = false;
        bool paused_ = false; // Play mode, stopped by Pause, a breakpoint or a step (the debugger is not Running)
        // Breakpoints by object, then block: kept through Play and Stop, saved per scene (not in the history)
        std::map<std::uint32_t, std::set<std::uint32_t>> breakpoints_;
        Engine::Logic::BreakpointRules breakpointRules_;
        std::uint32_t shownPause_ = 0;                  // DebugState::pauses when the last stop was shown
        Engine::Systems::DebugLineSettings debugLines_; // the viewport's Debug menu
        bool traceBlocks_ = false;                      // the Debug menu's "Log each block as it runs"

        // ---- Viewport (the Viewport window's area last frame, in window pixels)
        Engine::Graphics::OpenGL::FramebufferGL viewportFbo_;   // offscreen render target for the 3D/2D view
        std::map<std::filesystem::path, Thumbnail> thumbnails_; // the Home page's pictures, loaded once each
        mutable std::map<std::string, ModelFacts> modelFacts_;  // ModelFactsOf's, by full path
        std::string materialPath_;                   // the material in the Material window; empty: the window is shut
        Engine::Assets::MaterialFile materialDraft_; // what it holds, saved as it is changed
        std::filesystem::file_time_type materialWritten_{}; // when it was last read or saved here
        std::string materialError_;
        std::array<char, 64> materialValueName_{}; // a value being added
        std::array<char, 64> newShaderName_{};
        bool focusMaterialEditor_ = false;
        bool viewportVisible_ = false;
        bool viewportOwnsMouse_ = false; // pointer over it, or a drag that started in it
        bool viewportHovered_ = false;   // pointer over the 3D view last frame (for its shortcuts)
        bool viewport2D_ = false;
        bool gameCameraView_ = false; // edit mode: look through the game camera instead of the editor camera
        bool viewportMaximized_ = false;
        // The playing game has the mouse (UpdatePointerLock); what can be clicked on its screen (last frame)
        bool pointerLocked_ = false;
        std::vector<Runtime::ScreenTarget> screenTargets_;
        // Editing the game's screen (the toolbar's Screen): its elements take the view's clicks instead of the 3D
        // objects. It follows the selection (an element turns it on, a 3D object off); screenEditingFor_ is the
        // selection it last followed.
        bool screenEditing_ = false;
        std::uint32_t screenEditingFor_ = 0;
        // A Screen Element being dragged in the view (0: none), whether by its corner, and where it started
        std::uint32_t screenDragId_ = 0;
        bool screenDragResize_ = false;
        ImVec2 screenDragStart_;
        Engine::Scene::ScreenElementComponent screenDragFrom_;
        bool viewportMouseDrag_ = false; // a mouse button went down in the view and is still held
        bool viewCubeDrag_ = false;      // the mouse went down on the view cube and is still held
        bool viewCubeMoved_ = false;     // ...and has moved enough to orbit rather than click
        bool showGrid_ = true;
        bool showWireframe_ = false; // objects drawn as the edges of their triangles (see level of detail switch)
        bool showSky_ = true;
        bool showStats_ = true; // FPS + entity count in the Viewport corner
        // The logic visualiser's badges over the objects with logic
        bool showLogicBadges_ = true;
        std::uint32_t logicBadgeHovered_ = 0; // the object whose badge the mouse was on last frame (0: none)
        GizmoTool gizmoTool_ = GizmoTool::Move;
        bool gizmoLocal_ = true; // local or world axes
        bool gizmoSnap_ = false; // Ctrl snaps too
        float appliedDpiScale_ = 0.0f;
        float viewportX_ = 0.0f;
        float viewportY_ = 0.0f;
        float viewportWidth_ = 0.0f;
        float viewportHeight_ = 0.0f;
        // ---- 2D view: its camera, and the object being dragged (with the grab point's offset from its position)
        Camera::View2D view2D_;
        std::uint32_t dragObject_ = 0;
        glm::vec2 dragOffset_{0.0f};

        // ---- Modes / layout
        std::vector<Engine::Editor::TutorialMode> modes_ = Engine::Editor::TutorialNotes::Defaults();
        EditorView view_ = EditorView::Home;
        InterfaceMode interface_ = InterfaceMode::Simple;
        int simpleLeftTab_ = 0;        // the Simple interface's panel: 0 Objects, 1 Assets, 2 Scenes, 3 the open lesson
        bool simplePartsOpen_ = false; // the object menu's "Change its parts...": the object bar opens its Parts
        float topBarBottom_ = 0.0f;    // where the menus and the top bar end, in window pixels
        float homeCardHeight_ = 0.0f;  // what the Home page's Create card needs, measured as it was drawn last frame
        int activeMode_ = 0;
        int openTutorial_ = -1;
        bool modesShipped_ = false;    // modes_ are the editor's own lessons, not a game's own notes
        std::vector<bool> lessonMet_;  // the open lesson's steps, done or not
        double lessonRunStart_ = -1.0; // when this run of the game started; below 0: it is not running
        std::optional<std::pair<int, int>>
            pendingLesson_;                  // the track and lesson to open at the start of the next frame
        std::set<std::size_t> lessonTicked_; // those done: they stay done until the lesson changes
        int tourStep_ = -1;                  // the tour's stop; -1: not touring
        bool tourAdvanced_ = false;          // the running tour is the Advanced interface's
        bool advancedTourDone_ = false;      // as tourDone_, for that tour (in imgui.ini)
        bool tourDone_ = false;              // seen or skipped: it does not start by itself again (in imgui.ini)
        float tourHeight_ = 200.0f;          // of its words, as drawn last frame
        std::map<std::string, std::pair<ImVec2, ImVec2>> tourTargets_;
        bool modesDirty_ = false;
        float uiScale_ = 1.0f;
        bool showViewport_ = true;
        bool showConsole_ = true;
        bool showHierarchy_ = true;
        bool showInspector_ = true;
        bool showAssets_ = true;
        bool showLoadedAssets_ = true;
        bool showControls_ = true;
        bool showDiagnostics_ = true;
        bool showAudio_ = true;
        bool showPhysics_ = true;
        bool showLua_ = true;
        bool showBlocks_ = true;
        bool showLogic_ = true;
        bool showVariables_ = false;      // the floating live values, off until asked for (kept in imgui.ini)
        bool savedShowVariables_ = false; // showVariables_ as imgui.ini was last told
        bool focusInspector_ = false;     // a breakpoint was hit: bring the Inspector to the front
        bool focusControls_ = false;
        bool focusPhysics_ = false;
        // The gravity the Physics window shows, read from the game it was read from (read again when that changes)
        std::array<float, 3> gravity_{0.0f, -9.81f, 0.0f};
        std::filesystem::path gravityProject_;
        // The Physics window's latest collisions while playing, newest first, and the step count they were seen at
        // (it starts again from 0 when Play starts again, which clears them)
        std::vector<std::string> recentHits_;
        std::uint64_t hitsStep_ = 0;
        // The Physics window's ray test. The last cast is kept with the ImGui frame it was made in, so the Viewport
        // stops drawing it once the window closes or Play stops.
        struct RayTest
        {
            bool on = false;
            std::array<float, 3> direction{0.0f, -1.0f, 0.0f};
            float length = 10.0f;
            int frame = -1;
            glm::vec3 from{0.0f};
            glm::vec3 to{0.0f}; // the hit point, or the ray's end when it hits nothing
            std::optional<Engine::Systems::RaycastHit> hit;
        };
        RayTest rayTest_;
        // The Physics window's distance check, kept with the ImGui frame it was measured in like the ray test
        struct DistanceTest
        {
            bool on = false;
            std::uint32_t target = 0; // the object measured to (its id); 0 for none
            float range = 3.0f;       // how far around the selected object to look for others
            int frame = -1;
            glm::vec3 from{0.0f}; // where the selected object is
            std::optional<Engine::Physics::Gap> gap;
        };
        DistanceTest distanceTest_;
        // The Physics window's ground check, kept with the ImGui frame it was made in like the ray test
        struct GroundTest
        {
            bool on = false;
            int frame = -1;
            std::optional<Engine::Systems::GroundHit> ground;
        };
        GroundTest groundTest_;
        bool focusBlocks_ = false; // the Logic window opened a block: bring the Blocks window to the front
        bool resetLayout_ = false;
        bool selectDefaultTabs_ = false;

        // ---- Popups and messages
        std::string pathBuffer_;
        bool showPathPopup_ = false;
        bool saveAsMode_ = false;
        bool showUnsavedPopup_ = false;
        bool showResetLessonsPopup_ = false;
        std::function<void()> afterSaving_; // what AskToSave runs once the learner has chosen
        bool showNewGamePopup_ = false;
        std::array<char, 65> newGameName_{};
        int newGameTemplate_ = 0; // index into Engine::Game::kGameTemplates: empty
        std::string newGameError_;
        std::array<char, 512> importPath_{};
        std::string status_;
        bool statusError_ = false;
        double statusTime_ = -100.0; // ImGui time it was set (for the toast)
        int exportCountdown_ = 0;    // frames until BuildGameNow (0: no export waiting)
        bool initialized_ = false;
        double deltaSeconds_ = 0.0;
    };
}
