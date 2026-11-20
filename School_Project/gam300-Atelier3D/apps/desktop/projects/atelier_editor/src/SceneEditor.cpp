/* !
@file    	    SceneEditor.cpp
@author  	    sanni.go@digipen.edu [23%] 
         	    w.kyele@digipen.edu [59%]
         	    c.kojat@digipen.edu [13%]
         	    zhihenghenrey.hu@digipen.edu [3%]
         	    k.thantwin@digipen.edu [2%]
@date    		30/09/2026
@brief
    Implements SceneEditor: the frame loop, shortcuts, menus, project and scene
    management, the Hierarchy and Inspector windows, Play mode, undo/redo, game
    creation/building and the Modes page.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/SceneEditor.hpp"

#include "editor/OpenFile.hpp"
#include "editor/Theme.hpp"
#include "editor/panels/assets/AssetDragDrop.hpp"

#include "engine/assets/loaders/ModelLoader.hpp"
#include "engine/core/Log.hpp"
#include "engine/core/LogSinks.hpp"
#include "engine/game/AssetLayout.hpp"
#include "engine/game/GameBuild.hpp"
#include "engine/game/GameProject.hpp"
#include "engine/game/ModelImporter.hpp"
#include "engine/history/SceneCommands.hpp"
#include "engine/platform/Paths.hpp"
#include "engine/platform/Window.hpp"
#include "engine/scene/Hierarchy.hpp"
#include "engine/scene/systems/PhysicsSystem.hpp"
#include "engine/scene/systems/PlayerControllerSystem.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// glfw3.h (through engine/input/Input.hpp) defines its own APIENTRY when windows.h has not been included yet;
// windows.h defines the same __stdcall unconditionally, so drop GLFW's copy first
#ifdef GLFW_APIENTRY_DEFINED
#undef APIENTRY
#undef GLFW_APIENTRY_DEFINED
#endif
#include <windows.h>
#ifdef CreateWindow
#undef CreateWindow
#endif
#endif

#include <GL/gl.h>
#include <GL/glew.h>

#include <ImGuizmo/ImGuizmo.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace Editor
{
    namespace
    {
        using Engine::Editor::EditorScene;
        using Engine::Editor::GameObject;
        using Engine::Editor::Tutorial;
        using Engine::Editor::TutorialMode;
        using Engine::Editor::TutorialNotes;
        using Engine::Game::Shape;

        constexpr std::array kShapes{Shape::Quad,  Shape::Circle,  Shape::Cube,   Shape::Pyramid,
                                     Shape::Plane, Shape::Wirebox, Shape::Sphere, Shape::Line};

        constexpr const auto& kTemplates = Engine::Game::kGameTemplates;

        const ImVec4 kGreen = Theme::kSuccessText;
        const ImVec4 kRed = Theme::kErrorText;
        const ImVec4 kYellow = Theme::kWarningText;
        const ImVec4 kPrefabBlue = Theme::kSelect; // prefab instances in the Hierarchy and Inspector

        // Drag and drop of a game object id within the Hierarchy
        constexpr const char* kObjectPayload = "ATELIER_OBJECT";

        // "health/max_health" -> "Health > Max Health", for the Inspector's list of overrides
        std::string PropertyLabel(const std::string& property)
        {
            std::string label;
            bool startOfWord = true;
            for (const char c : property)
            {
                if (c == '/')
                {
                    label += " > ";
                    startOfWord = true;
                }
                else if (c == '_')
                {
                    label += ' ';
                    startOfWord = true;
                }
                else
                {
                    label += startOfWord ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
                    startOfWord = false;
                }
            }
            return label;
        }

        // A game's tutorial notes (the Modes page)
        constexpr const char* kTutorialNotesFile = "data/tutorials.editor.modes";

        template <std::size_t Size> void CopyToBuffer(std::array<char, Size>& buffer, const std::string& value)
        {
            std::fill(buffer.begin(), buffer.end(), '\0');
            std::strncpy(buffer.data(), value.c_str(), buffer.size() - 1);
        }

        std::string DisplayName(Shape shape)
        {
            std::string name = Engine::Game::ToString(shape);
            name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
            return name;
        }

        // Whether text contains part, ignoring case (the Hierarchy and Add Component searches)
        bool ContainsNoCase(std::string_view text, std::string_view part)
        {
            const auto lower = [](char c) { return std::tolower(static_cast<unsigned char>(c)); };
            return std::search(text.begin(), text.end(), part.begin(), part.end(),
                               [&lower](char a, char b) { return lower(a) == lower(b); }) != text.end();
        }
    }

    // An Inspector row as other editors lay it out: the label on the left, then the next item fills the rest of
    // the row. changed marks a value this prefab instance changed, with a blue bar at the left edge (like Unity).
    void SceneEditor::FieldLabel(const char* label, bool changed)
    {
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float startX = ImGui::GetCursorPosX();
        const float labelWidth =
            std::max(ImGui::GetContentRegionAvail().x * 0.38f, 80.0f * ImGui::GetStyle().FontScaleMain);
        const float height = ImGui::GetFrameHeight();
        if (liveFields_ && !nextFields_.empty())
        {
            // The live view: where this field is, and which saved fields it shows (named by Changed just before)
            liveFields_->push_back({.component = inspectedComponent_,
                                    .fields = std::move(nextFields_),
                                    .label = label,
                                    .x0 = start.x,
                                    .xValue = start.x + labelWidth,
                                    .x1 = start.x + ImGui::GetContentRegionAvail().x,
                                    .y0 = start.y,
                                    .y1 = start.y + height});
            nextFields_.clear();
        }
        if (changed)
        {
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(start.x - 5.0f, start.y),
                                                      ImVec2(start.x - 2.0f, start.y + height),
                                                      ImGui::GetColorU32(kPrefabBlue));
        }
        ImGui::AlignTextToFramePadding();
        // Long labels are cut at the field rather than drawn under it
        ImGui::PushClipRect(start, ImVec2(start.x + labelWidth - ImGui::GetStyle().ItemSpacing.x, start.y + height),
                            true);
        ImGui::TextUnformatted(label);
        ImGui::PopClipRect();
        ImGui::SameLine(startX + labelWidth);
        ImGui::SetNextItemWidth(-FLT_MIN);
    }

    // FieldLabel, then the hidden label to give the field: ImGui::DragFloat(Field("Speed").c_str(), ...)
    std::string SceneEditor::Field(const char* label, bool changed)
    {
        FieldLabel(label, changed);
        return std::string("##") + label;
    }

    // A text field for an asset path plus, when files is not empty, a picker of the game's files of that kind.
    // With dropKind, an asset of that kind dragged from the Assets window can be dropped on the text field.
    // Returns true if value changed.
    bool SceneEditor::AssetField(const char* label, std::string& value, const std::vector<std::string>& files,
                                 bool marked, std::optional<Panels::AssetKind> dropKind)
    {
        bool changed = false;
        ImGui::PushID(label);
        FieldLabel(label, marked);

        if (!files.empty())
        {
            ImGui::SetNextItemWidth(ImGui::GetFrameHeight());
            if (ImGui::BeginCombo("##pick", "", ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft))
            {
                if (ImGui::Selectable("(none)", value.empty()))
                {
                    changed = !value.empty();
                    value.clear();
                }
                for (const std::string& file : files)
                {
                    if (ImGui::Selectable(file.c_str(), file == value))
                    {
                        changed |= file != value;
                        value = file;
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Pick one of the game's files");
            }
            ImGui::SameLine();
        }

        std::array<char, 512> text{};
        CopyToBuffer(text, value);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputTextWithHint("##path", "(none)", text.data(), text.size()))
        {
            value = text.data();
            changed = true;
        }
        if (dropKind)
        {
            if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
            {
                ImGui::SetTooltip("Type a path, or drag %s here from the Assets window",
                                  *dropKind == Panels::AssetKind::Audio ? "a sound" : "an asset of this kind");
            }
            if (const auto dropped = Panels::AcceptAssetDrop(*dropKind))
            {
                changed |= *dropped != value;
                value = *dropped;
            }
        }
        ImGui::PopID();
        return changed;
    }

    // One component in the Inspector, as other editors show it: a full-width header that folds its fields away,
    // with a "..." menu at its right end to reset the component to its defaults or remove it (done after its
    // fields are drawn). changed marks a component this prefab instance changed.
    template <class Component, class DrawFields>
    void SceneEditor::ComponentSection(const char* label, bool changed, std::optional<Component>& component,
                                       DrawFields&& drawFields)
    {
        ImGui::PushID(label);
        const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
        const std::string title =
            changed ? std::format("{} (changed)###header", label) : std::format("{}###header", label);
        const bool open =
            ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        bool reset = false;
        bool remove = false;
        if (inspectMode_ == InspectMode::Edit) // the live view only shows, so it has no menu to reset or remove
        {
            const float buttonWidth = ImGui::GetFrameHeight() * 1.4f;
            ImGui::SameLine(right - buttonWidth);
            // Locked while the game runs, like the fields and Add Component: the scene is the game's now
            ImGui::BeginDisabled(playing_);
            if (ImGui::Button("...", ImVec2(buttonWidth, 0.0f)))
            {
                ImGui::OpenPopup("##ComponentMenu");
            }
            ImGui::EndDisabled();
            if (ImGui::BeginPopup("##ComponentMenu"))
            {
                reset = ImGui::MenuItem("Reset");
                remove = ImGui::MenuItem("Remove Component");
                ImGui::EndPopup();
            }
        }
        if (open)
        {
            // Only the fields are locked while the game runs (or in the live view): the header still folds
            ImGui::BeginDisabled(playing_ || inspectMode_ == InspectMode::Live);
            drawFields();
            ImGui::EndDisabled();
            ImGui::Spacing();
        }
        ImGui::PopID();
        if (reset)
        {
            component.emplace();
        }
        if (remove)
        {
            component.reset();
        }
    }

    SceneEditor::SceneEditor(std::shared_ptr<Engine::Core::MemorySink> consoleSink, std::filesystem::path logDirectory)
        : consoleSink_(std::move(consoleSink)), logDirectory_(std::move(logDirectory))
    {
    }

    // Runs once when the editor opens: input, panels, ImGui, then the first game in projects/ (or an empty
    // scene if there are no games yet)
    void SceneEditor::Init(Engine::Platform::Window& window, Engine::Audio::AudioSystem& audio,
                           Engine::Core::EventBus& events, std::function<void()> requestClose)
    {
        requestClose_ = std::move(requestClose);
        window_ = &window;
        events_ = &events;
        // Closing the window (X, Alt+F4) with unsaved changes asks first
        window.SetCloseCallback(
            [this]
            {
                if (!HasUnsavedChanges() || !requestClose_)
                {
                    return true;
                }
                AskToSave(requestClose_);
                return false;
            });

        // Keyboard / mouse / text from the window into input_ (before ImGui starts, so on GLFW ImGui's callbacks
        // chain onto these)
        window.AttachInput(&input_);

        console_ = std::make_unique<Panels::ConsolePanel>(consoleSink_, logDirectory_);
        console_->SetOnOpen(
            [this](const Engine::Core::LogTarget& target)
            {
                if (!consoleSessions_.contains(target.session) || InPrefabMode())
                {
                    SetStatus("This error belongs to another scene. "
                              "Return to its scene and run it again.",
                              true);
                    return;
                }

                const auto* object = scene_.Find(target.object);

                if (!object ||
                    (target.block != 0 && (!object->logic || !Engine::Logic::FindBlock(*object->logic, target.block))))
                {
                    SetStatus("The object or block for this error "
                              "no longer exists.",
                              true);
                    return;
                }

                if (target.script)
                {
                    scene_.Select(target.object);
                    SetInterface(InterfaceMode::Advanced);
                    view_ = EditorView::Workspace;
                    viewportMaximized_ = false;
                    focusLua_ = true;
                }
                else if (target.block != 0)
                {
                    viewportMaximized_ = false;
                    OpenBlock(target.object, target.block);
                }
                else
                {
                    scene_.Select(target.object);
                    SetInterface(InterfaceMode::Advanced);
                    viewportMaximized_ = false;
                    showInspector_ = true;
                    focusInspector_ = true;
                }
            });

        controls_ =
            std::make_unique<Panels::ControlsPanel>(input_, [this] { return projectDir_; }, [this] { ReloadGame(); });
        diagnostics_ = std::make_unique<Panels::DiagnosticsPanel>(input_, editorCamera_);
        diagnostics_->ConnectEvents(events);
        audio_ = std::make_unique<Panels::AudioPanel>(audio);
        blocks_ = std::make_unique<Panels::BlocksPanel>();
        logic_ = std::make_unique<Panels::LogicPanel>();
        blocks_->SetLibrary(&library_,
                            [this](std::string_view from, std::string_view to) { LibraryChanged(from, to); });
        // A click in the Logic window or the Variable Inspector selects the object and shows the block in the Blocks
        // window
        logic_->SetOnOpen([this](std::uint32_t objectId, std::uint32_t blockId) { OpenBlock(objectId, blockId); });
        variables_ = std::make_unique<Panels::VariableInspectorPanel>();
        variables_->SetOnOpen([this](std::uint32_t objectId, std::uint32_t blockId) { OpenBlock(objectId, blockId); });
        // The live view is the Inspector's own fields, only shown: locked but not dimmed
        variables_->SetDrawFields(
            [this](GameObject& object, std::vector<Panels::VariableInspectorPanel::FieldBox>& boxes)
            {
                boxes.clear();
                ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
                // With the interface's names
                DrawInspectorComponents(object, interface_ == InterfaceMode::Simple, InspectMode::Live, &boxes);
                ImGui::PopStyleVar();
            });
        blocks_->SetOnToggleBreakpoint([this](std::uint32_t objectId, std::uint32_t blockId)
                                       { ToggleBreakpoint(objectId, blockId); });
        blocks_->SetOnBreakpointRule(
            [this](std::uint32_t objectId, std::uint32_t blockId, Engine::Logic::BreakpointRule rule)
            { SetBreakpointRule(objectId, blockId, std::move(rule)); });
        // While paused, a click on a block picks its script for Step
        blocks_->SetOnClickBlock(
            [this](std::uint32_t objectId, std::uint32_t blockId)
            {
                if (playing_ && runtime_)
                {
                    runtime_->Debugger().FocusScript(objectId, blockId);
                }
            });
        audioSystem_ = &audio;
        LoadEditorSounds();
        blocks_->SetOnAttach([this] { PlayEditorSound(attachSound_); });
        blocks_->SetOnPluck(
            [this]
            {
                if (interface_ == InterfaceMode::Simple)
                {
                    PlayEditorSound(pluckSound_);
                }
            });
        assetBrowser_.SetAudio(&audio);
        // A click on an asset uses it on the selected object, the same way as dropping it there
        assetBrowser_.SetOnUse(
            [this](const Panels::AssetEntry& asset)
            {
                if (GameObject* selected = scene_.Find(scene_.Selected()))
                {
                    UseAsset(*selected, {asset.kind, asset.path});
                }
            });
        assetBrowser_.SetOnEditMaterial([this](const std::string& path) { OpenMaterialEditor(path); });
        // Files picked in the Assets' Open dialog come in the same way as files dropped on the editor
        assetBrowser_.SetOnImportFiles([this](const std::vector<std::filesystem::path>& files)
                                       { ImportDropped(files); });

        imgui_.Init(window);
        // Windows move by their title bar only, so dragging in the Viewport never moves it
        ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
        RegisterSettings(); // before the first frame, when imgui.ini is read
        Theme::LoadFonts();
        ApplyTheme();
        RefreshProjects(); // opens the first game, or an empty scene when there are none
        initialized_ = true;
        LOG_INFO(Editor, "Editor initialized");
    }

    // Save the tutorial notes, stop Play, and shut ImGui down before the window goes away
    void SceneEditor::Shutdown(Engine::Platform::Window& window)
    {
        window.SetCursorCaptured(false);
        SaveModes();
        SaveThumbnail();
        StopPlay();
        runtime_.reset();
        assetBrowser_.Shutdown(); // frees its GPU previews while the GL context is alive
        if (blocks_)
        {
            blocks_->Shutdown(); // and the block previews theirs
        }
        FreeThumbnails();
        imgui_.Shutdown();
        window.AttachInput(nullptr); // input_ is destroyed before the window
        window.SetCloseCallback(nullptr);
        initialized_ = false;
        LOG_INFO(Editor, "Editor shutdown");
    }

    void SceneEditor::UpdatePointerLock(Engine::Platform::Window& window)
    {
        ImGuiIO& io = ImGui::GetIO();
        bool locked = window.IsCursorCaptured();
        const bool canLock = playing_ && !paused_ && runtime_ && view_ == EditorView::Workspace;
        // The blocks' "lock the mouse" / "free the mouse", and a click on something the game shows on its screen
        // (a button): that goes to the object, rather than taking the mouse
        const std::optional<bool> asked = canLock ? runtime_->TakeMouseRequest() : std::nullopt;
        std::optional<std::uint32_t> clickedItem;
        if (canLock && !locked && io.MouseClicked[ImGuiMouseButton_Left])
        {
            for (const Runtime::ScreenTarget& target : screenTargets_)
            {
                if (io.MousePos.x >= target.min.x && io.MousePos.x <= target.max.x && io.MousePos.y >= target.min.y &&
                    io.MousePos.y <= target.max.y)
                {
                    clickedItem = target.objectId;
                }
            }
        }
        if (clickedItem)
        {
            runtime_->ScreenClicked(*clickedItem);
        }
        if (!canLock)
        {
            screenTargets_.clear();
        }
        if (locked && (!canLock || asked == false || ImGui::IsKeyPressed(ImGuiKey_Escape, false)))
        {
            window.SetCursorCaptured(false);
            locked = false;
        }
        else if (!locked && canLock &&
                 (asked == true || (!clickedItem && viewportHovered_ && io.MouseClicked[ImGuiMouseButton_Left])))
        {
            window.SetCursorCaptured(true);
            locked = window.IsCursorCaptured(); // not while another window has the focus
        }
        // The editor's UI leaves the mouse alone (and does not show its own cursor) while the game has it
        constexpr ImGuiConfigFlags kGameHasMouse = ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange;
        if (locked)
        {
            io.ConfigFlags |= kGameHasMouse;
        }
        else
        {
            io.ConfigFlags &= ~kGameHasMouse;
        }
        pointerLocked_ = locked;
        input_.SetPointerLocked(locked);
    }

    // One editor frame. The order matters: input capture -> shortcuts -> menus -> sync the game -> draw -> render
    void SceneEditor::Update(Engine::Platform::Window& window, double deltaSeconds)
    {
        if (!initialized_)
        {
            return;
        }

        deltaSeconds_ = deltaSeconds;

        // Clear the whole window to the editor's background colour
        {
            const ImVec4& bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
            glDisable(GL_SCISSOR_TEST);
            glViewport(0, 0, static_cast<GLsizei>(window.Width()), static_cast<GLsizei>(window.Height()));
            glClearColor(bg.x, bg.y, bg.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        // Scale the UI with the display (DPI), not the window: a larger or fullscreen window shows more at the same
        // size, like any desktop app. Text is rasterized at the size it is shown (see ImGuiLayer), so it stays sharp.
        const float dpiScale = std::clamp(window.ContentScale(), 0.5f, 4.0f);
        if (dpiScale != appliedDpiScale_)
        {
            static const ImGuiStyle baseStyle = ImGui::GetStyle();
            ImGuiStyle& style = ImGui::GetStyle();
            style = baseStyle;
            style.ScaleAllSizes(dpiScale);
            style.FontScaleDpi = dpiScale;
            appliedDpiScale_ = dpiScale;
            uiScale_ = dpiScale;
        }

        imgui_.BeginFrame(window, deltaSeconds);
        ImGuizmo::BeginFrame();
        UpdatePointerLock(window);

        // While a text field is being typed in (or Controls waits for a key to bind), keys belong to the UI, not to
        // shortcuts or the game. The mouse belongs to the game only over the Viewport (or in a drag started there):
        // everywhere else is an editor window. Game-side code reads input through a GameInput, which reports
        // nothing for a device the UI has.
        const ImGuiIO& io = ImGui::GetIO();
        input_.SetCaptureInput(io.WantTextInput || controls_->IsListeningForKey());
        input_.SetCaptureMouse(!pointerLocked_ && io.WantCaptureMouse && !viewportOwnsMouse_);
        diagnostics_->RecordInputEvents();
        PlayClickSounds();
        HandleShortcuts();
        if (exportCountdown_ > 0)
        {
            --exportCountdown_;
            if (exportCountdown_ == 0)
            {
                BuildGameNow();
            }
        }

        if (interface_ == InterfaceMode::Advanced)
        {
            DrawMenuBar();
        }
        SyncDebugger();
        // A lesson clicked last frame: opened here, where nothing is drawing from the lessons
        if (pendingLesson_)
        {
            const auto [mode, index] = *pendingLesson_;
            pendingLesson_.reset();
            OpenLesson(mode, index);
        }
        DrawTopBar();
        DrawScenePathPopup();
        DrawNewGamePopup();
        DrawSavePrefabPopup();
        DrawDeletePrefabPopup();
        DrawCreateVariantPopup();
        DrawClosePrefabPopup();
        DrawUnsavedPopup();
        DrawDeleteGamePopup();
        DrawResetLessonsPopup();
        DrawMaterialEditor();
        UpdateLesson();

        SyncLibrary();
        if (const auto dropped = window.TakeDroppedFiles(); !dropped.empty())
        {
            ImportDropped(dropped);
        }
        SyncRuntime();
        UpdateViewportCamera(deltaSeconds);

        // The layout measures the Viewport this frame; the game is drawn into that same area straight after (ImGui
        // draws on top at EndFrame), so the view, its gizmos and the view cube always line up, even mid-resize
        DrawMain();
        DrawVariableInspector();
        Theme::DrawCelebration(); // over everything, when something worth marking just happened
        DrawProjectLoad();
        RenderGame(window, deltaSeconds);
        KeepPrefabUnderRoot(); // part of the same undo step as the edit that added the object
        CommitHistory();
        if (modesDirty_ && !ImGui::IsAnyItemActive())
        {
            SaveModes();
        }
        imgui_.EndFrame();

        // This frame's input has been read: it becomes "previous" before the next window events arrive
        input_.Update();
    }

    // ---------------------------------------------------------------- Frame

    // Keyboard shortcuts. Skipped while typing in a text box so e.g. "W" goes into the name field, not the gizmo
    void SceneEditor::HandleShortcuts()
    {
        using namespace Engine::Input;
        if (input_.IsInputCaptured())
        {
            return; // typing: the keys are text
        }

        if (input_.IsKeyPressed(Key_F5))
        {
            playing_ ? StopPlay() : StartPlay();
        }
        if (input_.IsKeyPressed(Key_F8))
        {
            Step();
        }
        if (input_.IsKeyPressed(Key_F10))
        {
            StepOver();
        }
        if (input_.IsKeyPressed(Key_F11) && window_)
        {
            window_->SetFullscreen(!window_->IsFullscreen());
        }
        if (input_.IsShiftDown() && input_.IsKeyPressed(Key_Space))
        {
            viewportMaximized_ = !viewportMaximized_;
        }

        // Viewport tools, while the pointer is over the 3D view. Not while flying: then W / E / Q move the camera.
        if (viewportHovered_ && !editorCamera_.IsFlying() && !input_.IsCtrlDown() && !input_.IsAltDown() && !playing_)
        {
            if (input_.IsKeyPressed(Key_Q))
            {
                gizmoTool_ = GizmoTool::None;
            }
            if (input_.IsKeyPressed(Key_W))
            {
                gizmoTool_ = GizmoTool::Move;
            }
            if (input_.IsKeyPressed(Key_E))
            {
                gizmoTool_ = GizmoTool::Rotate;
            }
            if (input_.IsKeyPressed(Key_R))
            {
                gizmoTool_ = GizmoTool::Scale;
            }
            if (input_.IsKeyPressed(Key_F))
            {
                FocusSelected();
            }
            if (input_.IsKeyPressed(Key_Escape))
            {
                scene_.Select(0);
            }
            if (input_.IsKeyPressed(Key_Delete) && scene_.Selected() != 0)
            {
                DeleteObject(scene_.Selected());
            }
        }

        if (!input_.IsCtrlDown())
        {
            return;
        }
        if (input_.IsKeyPressed(Key_S))
        {
            SaveScene();
        }
        if (input_.IsKeyPressed(Key_N))
        {
            NewScene();
        }
        if (input_.IsKeyPressed(Key_O))
        {
            OpenScene();
        }
        if (input_.IsKeyPressed(Key_B))
        {
            BuildGame();
        }
        if (input_.IsKeyPressed(Key_R))
        {
            RefreshProjects();
        }
        // Not while typing (the text field undoes its own text) or in a My Block's window (it has its own undo: the scene
        // behind it stays as it is)
        const bool canUndoScene = !ImGui::GetIO().WantTextInput && !(blocks_ && blocks_->IsEditingMyBlock());
        if (input_.IsKeyPressed(Key_Z) && canUndoScene)
        {
            input_.IsShiftDown() ? Redo() : Undo();
        }
        if (input_.IsKeyPressed(Key_Y) && canUndoScene)
        {
            Redo();
        }
        if (input_.IsKeyPressed(Key_C))
        {
            CopySelected();
        }
        if (input_.IsKeyPressed(Key_V))
        {
            PasteClipboard();
        }
    }

    // The selected game's runtime shows the scene being edited (frozen in edit mode, running in Play mode) and
    // takes every edit: only what changed is applied, so in Play mode the game keeps its live state and the system
    // for an added or removed component reacts straight away
    void SceneEditor::SyncRuntime()
    {
        if (projectLoad_)
        {
            StepProjectLoad();
            return;
        }
        if (playing_)
        {
            if (runtime_ && scene_.Description() != runtimeScene_)
            {
                runtime_->UpdateScene(scene_.Description());
                runtimeScene_ = scene_.Description();
            }
            return;
        }
        if (projectDir_.empty())
        {
            runtime_.reset();
            runtimeProject_.clear();
            return;
        }

        const Engine::Scene::SceneDescription& scene = scene_.Description();
        if (!runtime_ || runtimeProject_ != projectDir_)
        {
            runtime_.reset();
            runtimeProject_ = projectDir_;
            try
            {
                auto runtime = std::make_unique<Engine::Game::GameRuntime>();
                runtime->SetSimulating(false);
                runtime->LoadProject(Engine::Game::GameProject(projectDir_));
                if (runtime->Start(scene))
                {
                    runtime_ = std::move(runtime);
                    runtimeScene_ = scene;
                }
            }
            catch (const std::exception& e)
            {
                SetStatus("Could not show " + projectDir_.filename().string() + ": " + e.what(), true);
            }
            return;
        }
        if (scene != runtimeScene_)
        {
            runtime_->UpdateScene(scene);
            runtimeScene_ = scene;
        }
    }

    // Edit mode: the editor camera moves from this frame's input (keys from the game's input_actions.json) before
    // the Viewport (gizmos, view cube) is drawn with it. Play mode, or edit mode looking through the game camera:
    // the game places its own camera (the Main camera, behind the player when it follows) and the editor camera is
    // left untouched, so Stop returns to the editor's view. The camera exists once the first frame is drawn.
    bool SceneEditor::UsesGameCamera() const
    {
        return playing_ || gameCameraView_;
    }

    // (see UsesGameCamera above) the editor camera only moves when the game isn't controlling it
    void SceneEditor::UpdateViewportCamera(double deltaSeconds)
    {
        if (!runtime_ || !viewportVisible_)
        {
            return;
        }
        runtime_->SetUseGameCamera(UsesGameCamera());
        if (auto* camera = runtime_->Camera(); camera && !UsesGameCamera())
        {
            editorCamera_.Update(deltaSeconds, Engine::Input::GameInput(input_, runtime_->Actions()), *camera);
        }
    }

    // Renders the game into an offscreen framebuffer the size of the Viewport, which DrawViewport3D shows
    void SceneEditor::RenderGame(Engine::Platform::Window& window, double deltaSeconds)
    {
        if (!runtime_ || !viewportVisible_ || viewportWidth_ < 1.0f || viewportHeight_ < 1.0f)
        {
            return;
        }

        const int fboW = static_cast<int>(std::lround(viewportWidth_));
        const int fboH = static_cast<int>(std::lround(viewportHeight_));
        if (viewportFbo_.Width() != fboW || viewportFbo_.Height() != fboH || !viewportFbo_.IsValid())
        {
            viewportFbo_.Resize(fboW, fboH);
            LOG_INFO(Editor, "Viewport picture made at {} x {}", fboW, fboH);
        }

        runtime_->SetViewport(0, 0, fboW, fboH);
        runtime_->SetShowGrid(showGrid_ && !playing_);
        runtime_->SetWireframe(showWireframe_);
        runtime_->SetDebugLines(debugLines_);
        runtime_->Debugger().SetTrace(traceBlocks_);
        runtime_->SetEnvironmentEnabled(showSky_);
        editorCamera_.SetViewportHeight(static_cast<float>(fboH));

        runtime_->SetUseGameCamera(UsesGameCamera());
        if (auto* camera = runtime_->Camera(); camera && !UsesGameCamera())
        {
            editorCamera_.Update(0.0, Engine::Input::GameInput(input_, runtime_->Actions()), *camera);
        }

        const Engine::Input::GameInput gameInput(input_, runtime_->Actions());
        // "quit the game" stops playing it here; the window is the editor's, so fullscreen and its size stay as they are
        const auto quitIfAsked = [this]
        {
            const auto requests = runtime_->TakeGameRequests();
            const bool quit =
                std::any_of(requests.begin(), requests.end(), [](const auto& request)
                            { return request.kind == Engine::Logic::LogicRunner::GameRequest::Kind::Quit; });
            if (quit && playing_)
            {
                StopPlay();
            }
        };

        // Preferred path: render off-screen, then blit to the viewport ImGui window as an image (allows compositing
        // editor UI overlays on top of a pixel-replace scene blit).
        if (viewportFbo_.IsValid())
        {
            runtime_->SetViewport(0, 0, fboW, fboH);
            viewportFbo_.Bind();
            // Paused, the player stands still anyway; a block being stepped still reads the keys
            runtime_->Tick(deltaSeconds, window, playing_ ? &gameInput : nullptr, audioSystem_);
            viewportFbo_.Unbind();
            quitIfAsked();
            return;
        }

        {
            const int fbH = window.Height();
            const int x = static_cast<int>(std::lround(viewportX_));
            const int y = fbH - static_cast<int>(std::lround(viewportY_)) - fboH;
            runtime_->SetViewport(x, y, fboW, fboH);
            runtime_->Tick(deltaSeconds, window, playing_ ? &gameInput : nullptr, audioSystem_);
            quitIfAsked();
        }
    }

    // ---------------------------------------------------------------- Menu bar and popups

    // The Atelier3D UI design's colours and shapes for the whole editor, the same in both interfaces (Theme.hpp)
    void SceneEditor::ApplyTheme()
    {
        Theme::Apply(ImGui::GetStyle());
    }

    // File / Edit / Project / Window menus along the top
    void SceneEditor::DrawMenuBar()
    {
        if (!ImGui::BeginMainMenuBar())
        {
            return;
        }
        if (ImGui::BeginMenu("File"))
        {
            // Prefab Mode edits the prefab in place of the scene: scenes are opened and saved once it is closed
            const bool scenes = !InPrefabMode();
            if (ImGui::MenuItem("New Scene", "Ctrl+N", false, scenes))
            {
                NewScene();
            }
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O", false, scenes))
            {
                OpenScene();
            }
            // The game's scene files (e.g. the sample game's demo scenes), one click to open
            if (ImGui::BeginMenu("Game Scenes", !projectDir_.empty() && !playing_ && scenes))
            {
                static constexpr std::array<std::string_view, 1> kSceneExtensions{".json"};
                const auto scenes = Engine::Game::GameProject(projectDir_).FindAssets("data/scenes", kSceneExtensions);
                if (scenes.empty())
                {
                    ImGui::MenuItem("(no scenes in data/scenes)", nullptr, false, false);
                }
                for (const std::string& relative : scenes)
                {
                    const std::filesystem::path path = projectDir_ / relative;
                    if (ImGui::MenuItem(relative.c_str(), nullptr, path == scenePath_))
                    {
                        LoadSceneFile(path);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem(scenes ? "Save Scene" : "Save Prefab", "Ctrl+S"))
            {
                SaveScene(); // the prefab, in Prefab Mode
            }
            if (ImGui::MenuItem("Save Scene As...", nullptr, false, scenes))
            {
                SaveSceneAs();
            }
            if (!scenes && ImGui::MenuItem("Close Prefab"))
            {
                ClosePrefab();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit") && requestClose_)
            {
                AskToSave(requestClose_);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            const bool canUndo = history_.CanUndo() || scene_.CaptureState() != committed_;
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo))
            {
                Undo();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, history_.CanRedo()))
            {
                Redo();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, scene_.Selected() != 0))
            {
                CopySelected();
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, !clipboard_.empty()))
            {
                PasteClipboard();
            }
            if (ImGui::MenuItem("Duplicate", nullptr, false, scene_.Selected() != 0))
            {
                DuplicateObject(scene_.Selected());
            }
            if (ImGui::MenuItem("Delete", "Del", false, scene_.Selected() != 0))
            {
                DeleteObject(scene_.Selected());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Deselect", nullptr, false, scene_.Selected() != 0))
            {
                scene_.Select(0);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Project"))
        {
            for (const auto& candidate : projects_)
            {
                if (ImGui::MenuItem(candidate.filename().string().c_str(), nullptr, candidate == projectDir_))
                {
                    OpenProject(candidate);
                }
            }
            if (projects_.empty())
            {
                ImGui::MenuItem("(no games yet)", nullptr, false, false);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("New Game..."))
            {
                showNewGamePopup_ = true;
            }
            if (ImGui::MenuItem("Refresh Games", "Ctrl+R"))
            {
                RefreshProjects();
            }
            if (ImGui::MenuItem("Build Game", "Ctrl+B", false, !projectDir_.empty()))
            {
                BuildGame();
            }
            if (ImGui::MenuItem("Delete Game...", nullptr, false, !projectDir_.empty()))
            {
                deleteGame_ = projectDir_;
                openDeleteGame_ = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Controls...", nullptr, false, !projectDir_.empty()))
            {
                view_ = EditorView::Workspace;
                showControls_ = true;
                focusControls_ = true;
            }
            if (ImGui::MenuItem("Physics...", nullptr, false, !projectDir_.empty()))
            {
                view_ = EditorView::Workspace;
                showPhysics_ = true;
                focusPhysics_ = true;
            }
            ImGui::EndMenu();
        }
        const bool windowMenu = ImGui::BeginMenu("Window");
        TourMark("window-menu", ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        if (windowMenu)
        {
            if (ImGui::MenuItem("Home", nullptr, view_ == EditorView::Home))
            {
                SaveThumbnail();
                StopPlay();
                view_ = EditorView::Home;
            }
            if (ImGui::MenuItem("Workspace", nullptr, view_ == EditorView::Workspace))
            {
                view_ = EditorView::Workspace;
            }
            if (ImGui::MenuItem("Simple Interface"))
            {
                SetInterface(InterfaceMode::Simple);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("The learner's screen: objects, scene and blocks");
            }
            if (ImGui::MenuItem("Show Guide", nullptr, false, !playing_))
            {
                StartAdvancedTour();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("A short tour of what each window is for");
            }
            ImGui::Separator();
            ImGui::MenuItem("Viewport", nullptr, &showViewport_);
            ImGui::MenuItem("Hierarchy", nullptr, &showHierarchy_);
            ImGui::MenuItem("Inspector", nullptr, &showInspector_);
            ImGui::MenuItem("Assets", nullptr, &showAssets_);
            ImGui::MenuItem("Blocks", nullptr, &showBlocks_);
            ImGui::MenuItem("Logic", nullptr, &showLogic_);
            ImGui::MenuItem("Lua Scripts", nullptr, &showLua_);
            ImGui::MenuItem("Variable Inspector", nullptr, &showVariables_);
            ImGui::MenuItem("Loaded Assets", nullptr, &showLoadedAssets_);
            ImGui::MenuItem("Console", nullptr, &showConsole_);
            ImGui::MenuItem("Controls", nullptr, &showControls_);
            ImGui::MenuItem("Physics", nullptr, &showPhysics_);
            ImGui::MenuItem("Scenes", nullptr, &showScenes_);
            ImGui::MenuItem("Diagnostics", nullptr, &showDiagnostics_);
            ImGui::MenuItem("Audio", nullptr, &showAudio_);
            ImGui::Separator();
            ImGui::MenuItem("Show Grid", nullptr, &showGrid_);
            ImGui::MenuItem("Show Wireframe", nullptr, &showWireframe_);
            ImGui::MenuItem("Show Stats (FPS, entities)", nullptr, &showStats_);
            ImGui::MenuItem("Maximize Viewport", "Shift+Space", &viewportMaximized_);
            if (ImGui::MenuItem("Fullscreen", "F11", window_ && window_->IsFullscreen()) && window_)
            {
                window_->SetFullscreen(!window_->IsFullscreen());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout"))
            {
                resetLayout_ = true;
            }
            ImGui::EndMenu();
        }

        DrawEditState();
        ImGui::EndMainMenuBar();
    }

    // On the right of the menu bar: what the editor is doing, and whether there is anything to save
    void SceneEditor::DrawEditState()
    {
        std::string mode = playing_ ? (paused_ ? "PAUSED" : "RUNNING") : InPrefabMode() ? "PREFAB MODE" : "EDIT MODE";
        if (HasUnsavedChanges())
        {
            mode += "  *";
        }
        const float labelWidth = ImGui::CalcTextSize(mode.c_str()).x;
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetIO().DisplaySize.x - labelWidth - ImGui::GetStyle().ItemSpacing.x * 2.0f);
        ImGui::TextDisabled("%s", mode.c_str());
        if (ImGui::IsItemHovered() && HasUnsavedChanges())
        {
            ImGui::SetTooltip(InPrefabMode() ? "The prefab has unsaved changes (Ctrl+S)"
                                             : "The scene has unsaved changes (Ctrl+S)");
        }
    }

    // The little Open / Save As box where you type a scene path
    void SceneEditor::DrawScenePathPopup()
    {
        const char* title = saveAsMode_ ? "Save Scene As" : "Open Scene";
        if (showPathPopup_)
        {
            ImGui::OpenPopup(title);
            showPathPopup_ = false;
        }
        if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }

        std::array<char, 512> path{};
        CopyToBuffer(path, pathBuffer_);
        ImGui::SetNextItemWidth(420.0f * uiScale_);
        ImGui::InputText("Path", path.data(), path.size());
        pathBuffer_ = path.data();
        ImGui::TextDisabled("Relative to the game folder, e.g. data/scenes/level.json (every game object and its");
        ImGui::TextDisabled("components; the game plays the same file).");
        if (ImGui::Button(saveAsMode_ ? "Save" : "Open"))
        {
            const std::filesystem::path target = ResolvePath(pathBuffer_);
            if (saveAsMode_ ? SaveSceneFile(target) : LoadSceneFile(target))
            {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Project > New Game: pick a name and a template (empty or one of the demos)
    // ---------------------------------------------------------------- Layout

    // Either the Modes page (tutorial cards) or the docked workspace, depending on the view
    void SceneEditor::DrawMain()
    {
        viewportVisible_ = false; // set again if the Viewport window is drawn in 3D this frame

        const float top = topBarBottom_;
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(0.0f, top));
        ImGui::SetNextWindowSize(ImVec2(display.x, std::max(0.0f, display.y - top)));
        // No background: the game shows through the Viewport window (every other window draws its own)
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                           ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings |
                                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                           ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("##EditorMain", nullptr, flags);
        ImGui::PopStyleVar(2);
        if (view_ == EditorView::Home)
        {
            viewportOwnsMouse_ = false;
            DrawHome();
        }
        else if (interface_ == InterfaceMode::Simple)
        {
            DrawSimpleWorkspace();
        }
        else
        {
            DrawWorkspace();
        }
        ImGui::End();
        DrawToast();
        DrawTour();
    }

    // Builds the default window layout the first time (or after "Reset layout"). After that ImGui remembers
    // whatever the user changed in imgui.ini.
    void SceneEditor::EnsureDefaultDockLayout(unsigned int dockspaceId, float width, float height)
    {
        ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspaceId);
        if (node != nullptr && !node->IsEmpty() && !resetLayout_)
        {
            return;
        }
        resetLayout_ = false;

        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImVec2(width, height));

        // Like other editors, so the Hierarchy, the Viewport and the Inspector are all always in view:
        // left: Hierarchy over Scenes
        // centre: Viewport over the wide windows (Blocks, Assets, Console, Lua Scripts, Controls, ...)
        // right: Inspector, and the narrow windows about one object or the game's physics
        // Across: 18% left, 22% right (0.27 of the rest), so the middle gets 60%. Down: the Hierarchy takes 60% of the
        // left column, the Viewport 60% of the middle (the Blocks window needs room too).
        ImGuiID left = 0;
        ImGuiID rest = 0;
        ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.18f, &left, &rest);
        ImGuiID center = 0;
        ImGuiID right = 0;
        ImGui::DockBuilderSplitNode(rest, ImGuiDir_Right, 0.27f, &right, &center);
        ImGuiID leftTop = 0;
        ImGuiID leftBottom = 0;
        ImGui::DockBuilderSplitNode(left, ImGuiDir_Up, 0.6f, &leftTop, &leftBottom);
        ImGuiID centerTop = 0;
        ImGuiID centerBottom = 0;
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Up, 0.6f, &centerTop, &centerBottom);

        ImGui::DockBuilderDockWindow("Hierarchy", leftTop);
        ImGui::DockBuilderDockWindow("Scenes", leftBottom);
        ImGui::DockBuilderDockWindow("Viewport", centerTop);
        ImGui::DockBuilderDockWindow("Blocks", centerBottom);
        ImGui::DockBuilderDockWindow("Assets", centerBottom);
        ImGui::DockBuilderDockWindow("Loaded Assets", centerBottom);
        ImGui::DockBuilderDockWindow("Console", centerBottom);
        ImGui::DockBuilderDockWindow("Diagnostics", centerBottom);
        ImGui::DockBuilderDockWindow("Audio", centerBottom);
        ImGui::DockBuilderDockWindow("Controls", centerBottom);
        ImGui::DockBuilderDockWindow("Lua Scripts", centerBottom);
        ImGui::DockBuilderDockWindow("Inspector", right);
        ImGui::DockBuilderDockWindow("Logic", right);
        ImGui::DockBuilderDockWindow("Variables", right);
        ImGui::DockBuilderDockWindow("Physics", right);
        ImGui::DockBuilderFinish(dockspaceId);
        selectDefaultTabs_ = true;
    }

    // The dockspace with all the editor windows (Viewport, Hierarchy, Inspector, ...)
    void SceneEditor::DrawWorkspace()
    {
        const float statusHeight = ImGui::GetFrameHeight();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 area(avail.x, std::max(1.0f, avail.y - statusHeight));
        // The same room round the cards as the Simple interface
        const float gap = 10.0f * uiScale_;
        const ImVec2 dockSize(std::max(1.0f, area.x - gap * 2.0f), std::max(1.0f, area.y - gap * 2.0f));
        const ImGuiID dockspaceId = ImHashStr(kWorkspaceDock);
        if (viewportMaximized_)
        {
            // Keep the docked layout alive (hidden) and show only the Viewport, filling the workspace
            ImGui::DockSpace(dockspaceId, dockSize, ImGuiDockNodeFlags_KeepAliveOnly);
            viewportOwnsMouse_ = false;
            ImGui::BeginChild("##MaximizedViewport", area, ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse);
            DrawViewport();
            ImGui::EndChild();
            DrawStatusBar();
            controls_->CancelListening();
            return;
        }
        ImGui::SetCursorScreenPos(ImVec2(origin.x + gap, origin.y + gap));
        // The dock's splitters take these two colours: the gaps between the cards show the ground, as in Simple
        ImGui::PushStyleColor(ImGuiCol_Border, Theme::kGround);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::kGround);
        // Each tab has its own close button: the node's would be a second one, closing only the tab on top
        ImGui::DockSpace(dockspaceId, dockSize,
                         ImGuiDockNodeFlags_NoCloseButton | ImGuiDockNodeFlags_NoWindowMenuButton);
        ImGui::PopStyleColor(2);
        EnsureDefaultDockLayout(dockspaceId, dockSize.x, dockSize.y);
        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + area.y));
        DrawStatusBar();

        auto panel = [this](const char* name, bool& open, ImGuiWindowFlags flags, bool flush, bool clearOnEmptyClick,
                            auto&& draw)
        {
            if (!open)
            {
                return false;
            }
            if (flush)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            }
            const bool visible = ImGui::Begin(name, &open, flags);
            if (flush)
            {
                ImGui::PopStyleVar();
            }
            if (visible)
            {
                const ImVec2 at = ImGui::GetWindowPos();
                const ImVec2 windowSize = ImGui::GetWindowSize();
                TourMark(name, at, ImVec2(at.x + windowSize.x, at.y + windowSize.y));
                draw();
                if (clearOnEmptyClick && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                    !ImGui::IsAnyItemHovered())
                {
                    scene_.Select(0);
                }
                Theme::DockedCard();
            }
            ImGui::End();
            return visible;
        };

        // The 3D / 2D scene is rendered into an offscreen FBO first (RenderGame) then blitted as an ImGui image.
        // 2D view draws its own grey canvas with the grid; 3D view shows the FBO. Neither needs the window to be
        // see-through any more.
        viewportOwnsMouse_ = false;
        panel("Viewport", showViewport_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse, true, false,
              [this] { DrawViewport(); });
        // After the layout is (re)built: the tab each group starts on
        const bool selectDefaultTabs = selectDefaultTabs_;
        selectDefaultTabs_ = false;
        const auto defaultTab = [selectDefaultTabs]
        {
            if (selectDefaultTabs)
            {
                ImGui::SetNextWindowFocus();
            }
        };
        defaultTab();
        if (focusInspector_)
        {
            ImGui::SetNextWindowFocus();
            focusInspector_ = false;
        }
        panel("Inspector", showInspector_, 0, false, false, [this] { DrawInspector(); });
        defaultTab();
        panel("Assets", showAssets_, 0, false, true, [this] { DrawAssets(); });
        defaultTab();
        panel("Hierarchy", showHierarchy_, 0, false, false, [this] { DrawHierarchy(); });
        panel("Loaded Assets", showLoadedAssets_, 0, false, false,
              [this]
              {
                  loadedAssets_.SetSource(runtime_ ? &runtime_->Resources() : nullptr);
                  loadedAssets_.DrawContents();
              });
        defaultTab();
        if (focusBlocks_)
        {
            ImGui::SetNextWindowFocus();
            focusBlocks_ = false;
        }
        panel("Blocks", showBlocks_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse, false, false,
              [this]
              {
                  // Blocks glow only in Play mode: the edit-mode runtime does not run them
                  blocks_->SetScale(uiScale_);
                  GameObject* selected = scene_.Find(scene_.Selected());
                  blocks_->SetTarget(selected, playing_ && runtime_ ? &runtime_->Blocks() : nullptr);
                  blocks_->SetBreakpoints(BreakpointsOf(scene_.Selected()));
                  blocks_->SetBreakpointRules(breakpointRules_);
                  blocks_->SetPreviewLook(projectDir_,
                                          selected ? Panels::LookOf(scene_.Description(), *selected) : nullptr,
                                          selected ? Panels::AnimatorOf(scene_.Description(), *selected) : nullptr);
                  blocks_->DrawContents();
              });
        panel("Console", showConsole_, 0, false, false, [this] { console_->DrawContents(); });
        panel("Diagnostics", showDiagnostics_, 0, false, false,
              [this]
              {
                  DrawEcsStress();
                  diagnostics_->DrawContents();
              });
        panel("Audio", showAudio_, 0, false, false, [this] { audio_->DrawContents(); });
        if (focusLua_)
        {
            showLua_ = true;
            ImGui::SetNextWindowFocus();
            focusLua_ = false;
        }
        panel("Lua Scripts", showLua_, 0, false, false, [this] { DrawLuaPanel(); });
        panel("Logic", showLogic_, 0, false, false,
              [this]
              {
                  logic_->SetScene(&scene_.Description(), scene_.Selected(),
                                   playing_ && runtime_ ? &runtime_->Blocks() : nullptr);
                  logic_->DrawContents();
              });

        if (focusControls_)
        {
            ImGui::SetNextWindowFocus();
            focusControls_ = false;
        }
        if (!panel("Controls", showControls_, 0, false, false, [this] { controls_->DrawContents(); }))
        {
            controls_->CancelListening(); // a hidden panel must not hold on to the keyboard
        }
        if (focusPhysics_)
        {
            ImGui::SetNextWindowFocus();
            focusPhysics_ = false;
        }
        panel("Physics", showPhysics_, 0, false, false, [this] { DrawPhysicsSettings(); });
        panel("Variables", showVariables_, 0, false, false, [this] { DrawLiveValues(); });
        panel("Scenes", showScenes_, 0, false, false, [this] { DrawScenes(); });
    }

    // The line at the bottom that shows the last status message (errors in red)
    void SceneEditor::DrawStatusBar()
    {
        ImGui::SetCursorPosX(ImGui::GetStyle().ItemSpacing.x);
        ImGui::AlignTextToFramePadding();
        if (status_.empty())
        {
            ImGui::TextDisabled("%s", projectDir_.empty() ? "No game open - New game on the Home page makes one"
                                                          : projectDir_.string().c_str());
            return;
        }
        ImGui::TextColored(statusError_ ? Theme::kErrorText : Theme::kTextSoft, "%s", status_.c_str());
    }

    // A pill over the bottom of the workspace: a green (done) or red (problem) dot and the message, for a few seconds
    void SceneEditor::DrawToast()
    {
        const double age = ImGui::GetTime() - statusTime_;
        const double shown = statusError_ ? 6.0 : 3.5;
        if (status_.empty() || age > shown)
        {
            return;
        }
        const float s = uiScale_;
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y - ImGui::GetFrameHeight() - 16.0f * s),
                                ImGuiCond_Always, ImVec2(0.5f, 1.0f));
        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, static_cast<float>(std::clamp((shown - age) / 0.4, 0.0, 1.0)));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * s, 10.0f * s));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f * s);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::kRaisedHigh);
        ImGui::PushStyleColor(ImGuiCol_Border, statusError_ ? Theme::kError : Theme::kBorderStrong);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                           ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                           ImGuiWindowFlags_NoDocking;
        if (ImGui::Begin("##Toast", nullptr, flags))
        {
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow()); // over the docked windows clicked since
            const float dot = 10.0f * s;
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const float line = ImGui::GetTextLineHeight();
            ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(at.x + dot * 0.5f, at.y + line * 0.5f), dot * 0.5f,
                                                        ImGui::GetColorU32(statusError_ ? Theme::kError : Theme::kRun));
            ImGui::Dummy(ImVec2(dot, line));
            ImGui::SameLine(0.0f, 10.0f * s);
            ImGui::PushTextWrapPos(std::max(display.x * 0.6f, 200.0f * s));
            ImGui::TextUnformatted(status_.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(4);
    }

    // ---------------------------------------------------------------- Windows

    // List of game objects: click to select, double-click to frame it, right-click to rename/duplicate/delete
    // The Create menu's items: an empty object, or one that already has the components a shape / light / camera /
    // player needs (they can still be added or removed in the Inspector afterwards)
    SceneEditor::CreateKind SceneEditor::DrawCreateMenuItems()
    {
        CreateKind picked = CreateKind::None;
        if (ImGui::MenuItem("Empty"))
        {
            picked = CreateKind::Empty;
        }
        if (ImGui::MenuItem("Group"))
        {
            picked = CreateKind::Group;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("An object to put others in, so they move together (drag objects onto it)");
        }
        ImGui::Separator();
        for (const auto& [label, kind] :
             {std::pair{"Cube", CreateKind::Cube}, std::pair{"Sphere", CreateKind::Sphere},
              std::pair{"Pyramid", CreateKind::Pyramid}, std::pair{"Plane", CreateKind::Plane}})
        {
            if (ImGui::MenuItem(label))
            {
                picked = kind;
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Directional Light"))
        {
            picked = CreateKind::DirectionalLight;
        }
        if (ImGui::MenuItem("Point Light"))
        {
            picked = CreateKind::PointLight;
        }
        if (ImGui::MenuItem("Camera"))
        {
            picked = CreateKind::Camera;
        }
        // One player per scene: it is what the move keys drive in Play mode and what the camera follows
        ImGui::Separator();
        const bool hasPlayer = std::any_of(scene_.Objects().begin(), scene_.Objects().end(),
                                           [](const GameObject& object) { return object.player.has_value(); });
        if (ImGui::MenuItem("Player", nullptr, false, !hasPlayer))
        {
            picked = CreateKind::Player;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip(hasPlayer ? "This scene already has a player"
                                        : "A cube the move keys drive while the game runs (the camera follows it)");
        }
        // The game's screen: 2D things over the view (drag them in the view to place them)
        ImGui::Separator();
        if (ImGui::BeginMenu("Screen"))
        {
            for (const auto& [label, kind] :
                 {std::pair{"Text", CreateKind::ScreenText}, std::pair{"Picture", CreateKind::ScreenPicture},
                  std::pair{"Bar", CreateKind::ScreenBar}, std::pair{"Button", CreateKind::ScreenButton}})
            {
                if (ImGui::MenuItem(label))
                {
                    picked = kind;
                }
            }
            ImGui::EndMenu();
        }
        return picked;
    }

    void SceneEditor::CreateObjectOf(CreateKind kind, std::uint32_t parent)
    {
        if (kind == CreateKind::None)
        {
            return;
        }
        GameObject* created = nullptr;
        switch (kind)
        {
        case CreateKind::Empty:
            created = &scene_.CreateObject(); // no components at all, not even a Transform
            break;
        case CreateKind::Group:
            created = &scene_.CreateObject("Group");
            created->transform.emplace(); // its place is its members' place
            break;
        case CreateKind::Cube:
        case CreateKind::Sphere:
        case CreateKind::Pyramid:
        case CreateKind::Plane:
        {
            const auto [label, shape] = kind == CreateKind::Cube      ? std::pair{"Cube", Shape::Cube}
                                        : kind == CreateKind::Sphere  ? std::pair{"Sphere", Shape::Sphere}
                                        : kind == CreateKind::Pyramid ? std::pair{"Pyramid", Shape::Pyramid}
                                                                      : std::pair{"Plane", Shape::Plane};
            created = &scene_.CreateObject(label);
            created->transform.emplace();
            // the built-in plane mesh is 10 x 10, so start it smaller; the sphere mesh has radius 1
            if (shape == Shape::Plane || shape == Shape::Sphere)
            {
                created->transform->scale = 0.5f;
            }
            created->renderable = Engine::Scene::RenderableDesc{.shape = shape};
            break;
        }
        case CreateKind::DirectionalLight:
            created = &scene_.CreateObject("Directional Light");
            // pointing down at an angle, like the sun; its direction comes from the Transform rotation
            created->transform = Engine::Scene::TransformComponent{.y = 5.0f, .rotation = -50.0f, .rotationX = -55.0f};
            created->light = Engine::Scene::LightComponent{.type = Engine::Graphics::LightType::Directional};
            break;
        case CreateKind::PointLight:
            created = &scene_.CreateObject("Point Light");
            created->transform = Engine::Scene::TransformComponent{.y = 2.0f};
            created->light = Engine::Scene::LightComponent{.type = Engine::Graphics::LightType::Point};
            break;
        case CreateKind::Camera:
        {
            // the first camera in a scene becomes the Main camera, so Play uses it straight away
            const bool hasMain =
                std::any_of(scene_.Objects().begin(), scene_.Objects().end(),
                            [](const GameObject& other) { return other.camera && other.camera->primary; });
            created = &scene_.CreateObject("Camera");
            created->transform = Engine::Scene::TransformComponent{.y = 2.0f, .z = 6.0f, .rotationX = -15.0f};
            created->camera = Engine::Scene::CameraComponent{.primary = !hasMain};
            break;
        }
        case CreateKind::Player:
            created = &scene_.CreateObject("Player");
            created->transform.emplace();
            created->renderable.emplace();
            created->player.emplace();
            break;
        case CreateKind::ScreenText:
            created = &scene_.CreateObject("Text");
            created->screen = Engine::Scene::ScreenElementComponent{.text = "Hello!", .height = 7.0f};
            break;
        case CreateKind::ScreenPicture:
            created = &scene_.CreateObject("Picture");
            created->screen = Engine::Scene::ScreenElementComponent{
                .kind = Engine::Scene::ScreenElementKind::Picture, .text = "", .height = 14.0f};
            break;
        case CreateKind::ScreenBar:
            created = &scene_.CreateObject("Bar");
            created->screen = Engine::Scene::ScreenElementComponent{
                .kind = Engine::Scene::ScreenElementKind::Bar, .text = "Bar", .height = 4.0f};
            break;
        case CreateKind::ScreenButton:
            created = &scene_.CreateObject("Button");
            created->screen = Engine::Scene::ScreenElementComponent{.kind = Engine::Scene::ScreenElementKind::Button,
                                                                    .text = "Play",
                                                                    .height = 9.0f,
                                                                    .colorR = 0.98f,
                                                                    .colorG = 0.78f,
                                                                    .colorB = 0.2f};
            break;
        case CreateKind::None: return;
        }
        created->parent = parent; // a Transform is relative to its parent, so it starts at (or near) the parent
        scene_.Select(created->id);
        RecordCreated(created->id);
    }

    void SceneEditor::DrawHierarchy()
    {
        if (InPrefabMode())
        {
            DrawPrefabModeBar();
        }

        // Toolbar: Create, and a search that keeps the objects whose name matches (with the objects above them)
        if (ImGui::Button("+ Create"))
        {
            ImGui::OpenPopup("##create");
        }
        if (ImGui::BeginPopup("##create"))
        {
            CreateObjectOf(DrawCreateMenuItems(), 0);
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##search", "Search", hierarchySearch_.data(), hierarchySearch_.size());
        const std::string_view search(hierarchySearch_.data());
        ImGui::Separator();

        // Changes to the tree wait until it has been drawn: each one changes the list being walked
        std::function<void()> deferred;
        const auto later = [&deferred](std::function<void()> action) { deferred = std::move(action); };
        const Engine::Scene::SceneDescription& description = scene_.Description();

        // While searching: an object is shown if its name matches or one under it does
        const auto shown = [&](const auto& self, std::uint32_t id) -> bool
        {
            const GameObject* object = scene_.Find(id);
            if (!object || search.empty() || ContainsNoCase(object->name, search))
            {
                return object != nullptr;
            }
            const auto children = Engine::Scene::ChildrenOf(description, id);
            return std::any_of(children.begin(), children.end(),
                               [&](std::uint32_t child) { return self(self, child); });
        };

        const auto drawNode = [&](const auto& self, std::uint32_t id) -> void
        {
            if (!shown(shown, id))
            {
                return;
            }
            GameObject& object = *scene_.Find(id);
            const auto children = Engine::Scene::ChildrenOf(description, id);
            const std::uint32_t instanceRoot = Engine::Scene::InstanceRootOf(description, id);
            const bool part = instanceRoot != 0 && instanceRoot != id; // the prefab decides where these go
            const bool renaming = renamingId_ == id;

            ImGui::PushID(static_cast<int>(id));
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                                       ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap;
            if (children.empty())
            {
                flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            }
            if (scene_.Selected() == id)
            {
                flags |= ImGuiTreeNodeFlags_Selected;
            }
            if (!search.empty())
            {
                ImGui::SetNextItemOpen(true); // show the matches under their parents
            }
            // Prefab instances are blue, like Unity's
            if (instanceRoot != 0)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, kPrefabBlue);
            }
            const bool open = ImGui::TreeNodeEx("##node", flags, "%s", renaming ? "" : object.name.c_str());
            if (instanceRoot != 0)
            {
                ImGui::PopStyleColor();
            }
            const ImVec2 rowMin = ImGui::GetItemRectMin();

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
            {
                scene_.Select(id);
                showBlocks_ = true;
                focusBlocks_ = true;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    FocusSelected();
                }
            }
            ImGui::OpenPopupOnItemClick("##ObjectMenu", ImGuiPopupFlags_MouseButtonRight);
            if (const auto dropped = Panels::AcceptAnyAssetDrop())
            {
                UseAsset(object, *dropped);
            }
            if (instanceRoot != 0 && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && !ImGui::IsDragDropActive())
            {
                ImGui::SetTooltip("Prefab instance: %s", scene_.Find(instanceRoot)->prefabInstance->prefab.c_str());
            }

            // Drag onto another object to put it under that one (it stays where it is in the world)
            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload(kObjectPayload, &id, sizeof(id));
                ImGui::Text("Put %s under...", object.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kObjectPayload))
                {
                    std::uint32_t dragged = 0;
                    std::memcpy(&dragged, payload->Data, sizeof(dragged));
                    later([this, dragged, id] { ReparentObject(dragged, id); });
                }
                // A prefab dragged from the Prefabs list: a new instance of it, under this object
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPrefabPayload))
                {
                    const std::string path(static_cast<const char*>(payload->Data));
                    later([this, path, id] { PlacePrefab(path, std::nullopt, id); });
                }
                ImGui::EndDragDropTarget();
            }

            if (renaming)
            {
                // F2 / Rename: typed over the name; Enter or clicking away keeps it, Esc puts the old one back
                ImGui::SameLine();
                ImGui::SetCursorScreenPos(ImVec2(rowMin.x + ImGui::GetTreeNodeToLabelSpacing(), rowMin.y));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0.0f));
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (focusRename_)
                {
                    ImGui::SetKeyboardFocusHere();
                    focusRename_ = false;
                }
                ImGui::InputText("##rename", renameBuffer_.data(), renameBuffer_.size(),
                                 ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::PopStyleVar();
                if (ImGui::IsItemDeactivated())
                {
                    if (renameBuffer_[0] != '\0')
                    {
                        object.name = renameBuffer_.data();
                    }
                    renamingId_ = 0;
                }
            }
            else
            {
                if (object.player)
                {
                    ImGui::SameLine();
                    ImGui::TextColored(kGreen, "(player)");
                }
                if (object.camera && object.camera->primary)
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(main camera)");
                }
                if (object.prefabInstance && !object.prefabInstance->overrides.empty())
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%zu changed)", object.prefabInstance->overrides.size());
                }
            }

            if (ImGui::BeginPopup("##ObjectMenu"))
            {
                scene_.Select(id);
                if (ImGui::MenuItem("Rename", "F2"))
                {
                    renamingId_ = id;
                    CopyToBuffer(renameBuffer_, object.name);
                    focusRename_ = true;
                }
                if (ImGui::BeginMenu("Create Child"))
                {
                    const CreateKind kind = DrawCreateMenuItems();
                    if (kind != CreateKind::None)
                    {
                        later([this, kind, id] { CreateObjectOf(kind, id); });
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Duplicate", nullptr, false, !part))
                {
                    later([this, id] { DuplicateObject(id); });
                }
                if (ImGui::MenuItem("Copy"))
                {
                    CopySelected();
                }
                if (object.parent != 0 && ImGui::MenuItem("Move to Top", nullptr, false, !part))
                {
                    later([this, id] { ReparentObject(id, 0); });
                }
                if (ImGui::MenuItem("Group", nullptr, false, !part))
                {
                    later([this, id] { GroupObject(id); });
                }
                if (!children.empty() && ImGui::MenuItem("Ungroup", nullptr, false, !part))
                {
                    later([this, id] { UngroupObject(id); });
                }
                if (ImGui::MenuItem("Delete", "Del", false, !part))
                {
                    later([this, id] { DeleteObject(id); });
                }
                ImGui::Separator();
                if (instanceRoot == 0)
                {
                    if (ImGui::MenuItem("Save as Prefab...", nullptr, false, !projectDir_.empty() && !InPrefabMode()))
                    {
                        BeginSavePrefab(id);
                    }
                }
                else if (ImGui::BeginMenu("Prefab"))
                {
                    if (ImGui::MenuItem("Open Prefab"))
                    {
                        const std::string path = scene_.Find(instanceRoot)->prefabInstance->prefab;
                        later([this, path] { OpenPrefab(path); });
                    }
                    if (part && ImGui::MenuItem("Select Prefab Root"))
                    {
                        scene_.Select(instanceRoot);
                    }
                    // In Prefab Mode the root's instance is the variant's base; any other is a nested prefab
                    const bool toBase = InPrefabMode() && instanceRoot == Engine::Scene::kPrefabRootId;
                    if (ImGui::MenuItem(toBase ? "Apply All to Base" : "Apply All to Prefab"))
                    {
                        later([this, id] { ApplyPrefab(id); });
                    }
                    if (ImGui::MenuItem("Unpack Prefab"))
                    {
                        later(
                            [this, id]
                            {
                                Engine::Game::PrefabManager::Unpack(scene_.Description(), id);
                                SetStatus("Unpacked: the objects are no longer linked to the prefab");
                            });
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }

            if (open && !children.empty())
            {
                for (const std::uint32_t child : children)
                {
                    self(self, child);
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        };
        for (const std::uint32_t id : Engine::Scene::ChildrenOf(description, 0))
        {
            drawNode(drawNode, id);
        }
        if (scene_.Objects().empty())
        {
            ImGui::TextDisabled("Empty scene: + Create, or right-click here.");
        }

        // The rest of the window: drop an object here to take it out of its parent, or a prefab to place it;
        // right-click for the Create menu
        // (at least 1 px wide: ImGui asserts on a 0-size button, e.g. the first frame a docked layout is shown)
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImVec2 rest(std::max(available.x, 1.0f), std::max(available.y, ImGui::GetFrameHeight()));
        ImGui::InvisibleButton("##empty", rest);
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kObjectPayload))
            {
                std::uint32_t dragged = 0;
                std::memcpy(&dragged, payload->Data, sizeof(dragged));
                later([this, dragged] { ReparentObject(dragged, 0); });
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPrefabPayload))
            {
                const std::string path(static_cast<const char*>(payload->Data));
                later([this, path] { PlacePrefab(path); });
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            scene_.Select(0);
        }
        if (ImGui::BeginPopupContextItem("##EmptyMenu"))
        {
            const CreateKind kind = DrawCreateMenuItems();
            if (kind != CreateKind::None)
            {
                later([this, kind] { CreateObjectOf(kind, 0); });
            }
            ImGui::EndPopup();
        }

        const ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsWindowFocused() && !io.WantTextInput && scene_.Selected() != 0)
        {
            const std::uint32_t selected = scene_.Selected();
            if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            {
                later([this, selected] { DeleteObject(selected); });
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
            {
                renamingId_ = selected;
                CopyToBuffer(renameBuffer_, scene_.Find(selected)->name);
                focusRename_ = true;
            }
        }
        if (deferred)
        {
            deferred();
        }
    }

    // The game's prefabs, a view of the Assets panel, used like Unity's prefab assets: drag one into the Hierarchy
    // or the Viewport (or Place) to add an instance, double-click to open it in Prefab Mode, right-click for the rest;
    // drop an object from the Hierarchy onto the list to save it as a new prefab
    void SceneEditor::DrawPrefabs()
    {
        ImGui::PushID("Prefabs");
        if (prefabList_.empty())
        {
            ImGui::TextDisabled(projectDir_.empty() ? "Open a game to use prefabs."
                                                    : "None yet: drag an object here from the Hierarchy.");
        }
        if (InPrefabMode())
        {
            ImGui::TextDisabled("Place or drag one in to nest it in %s.", prefabMode_->name.c_str());
        }

        // Actions wait until the list is drawn: each one changes the scene (or replaces it, for Open)
        std::string place;
        std::string open;
        std::uint32_t selectInstance = 0;
        const float placeWidth = ImGui::CalcTextSize("Place").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        for (const std::string& path : prefabList_)
        {
            ImGui::PushID(path.c_str());
            const std::string name = std::filesystem::path(path).stem().string();
            const bool isOpen = InPrefabMode() && prefabMode_->path == path;
            const auto instances = Engine::Game::PrefabManager::InstancesOf(scene_.Description(), path);
            const Engine::Scene::Prefab* prefab = prefabs_.Find(path);
            const std::string variantOf =
                prefab && prefab->IsVariant() ? std::filesystem::path(prefab->base).stem().string() : std::string{};
            std::string label = name;
            if (!variantOf.empty())
            {
                label += "  (variant of " + variantOf + ")";
            }
            if (!instances.empty())
            {
                label += std::format("  ({} in scene)", instances.size());
            }

            // A file, not an object in the scene: a prefab icon (a blue box, a hollow one for a variant) and plain text
            const float iconSize = ImGui::GetTextLineHeight() * 0.8f;
            const ImVec2 iconAt = ImGui::GetCursorScreenPos();
            const float iconTop = iconAt.y + (ImGui::GetTextLineHeight() - iconSize) * 0.5f;
            if (variantOf.empty())
            {
                ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(iconAt.x, iconTop),
                                                          ImVec2(iconAt.x + iconSize, iconTop + iconSize),
                                                          ImGui::GetColorU32(kPrefabBlue), 2.0f);
            }
            else
            {
                ImGui::GetWindowDrawList()->AddRect(ImVec2(iconAt.x, iconTop),
                                                    ImVec2(iconAt.x + iconSize, iconTop + iconSize),
                                                    ImGui::GetColorU32(kPrefabBlue), 2.0f, 0, 2.0f);
            }
            ImGui::Dummy(ImVec2(iconSize, ImGui::GetTextLineHeight()));
            ImGui::SameLine();
            const float width =
                std::max(ImGui::GetContentRegionAvail().x - placeWidth - ImGui::GetStyle().ItemSpacing.x, 1.0f);
            ImGui::Selectable(label.c_str(), isOpen, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(width, 0.0f));
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                open = path;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && !ImGui::IsDragDropActive())
            {
                ImGui::SetTooltip("%s\nDouble-click to open it (Prefab Mode); drag it into the Hierarchy or the "
                                  "Viewport to place it; right-click for more",
                                  path.c_str());
            }
            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload(kPrefabPayload, path.c_str(), path.size() + 1);
                ImGui::Text("Place %s", name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginPopupContextItem("##PrefabMenu"))
            {
                if (ImGui::MenuItem("Open Prefab", nullptr, false, !isOpen))
                {
                    open = path;
                }
                if (ImGui::MenuItem(InPrefabMode() ? "Place in Prefab" : "Place in Scene"))
                {
                    place = path;
                }
                // Like Unity's Create > Prefab Variant: a new prefab that follows this one, except what it changes
                if (ImGui::MenuItem("Create Variant...", nullptr, false, prefab != nullptr))
                {
                    variantBase_ = path;
                    CopyToBuffer(variantName_, name + "_Variant");
                    showVariantPopup_ = true;
                }
                // Each click selects the next instance in the scene (and frames it), like Unity's Select All
                if (ImGui::MenuItem("Select Next Instance", nullptr, false, !instances.empty()))
                {
                    const auto current = std::find(instances.begin(), instances.end(), scene_.Selected());
                    selectInstance = current == instances.end() || std::next(current) == instances.end()
                                         ? instances.front()
                                         : *std::next(current);
                }
                ImGui::Separator();
                // Not while a prefab is open: it could be the base of the variant being edited
                if (ImGui::MenuItem("Delete Prefab...", nullptr, false, !InPrefabMode()))
                {
                    deletePrefabPath_ = path;
                    showDeletePrefabPopup_ = true;
                }
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Place"))
            {
                place = path;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(InPrefabMode() ? "Nest an instance of it in the open prefab, under its root (or "
                                                   "drag it onto an object in it)"
                                                 : "Add an instance of it to the scene, next to the ones already "
                                                   "placed (or drag it to where it should go)");
            }
            ImGui::PopID();
        }

        // Drop an object from the tree here to save it as a prefab (like dragging it into Unity's Project window)
        if (!projectDir_.empty() && !InPrefabMode())
        {
            const ImGuiPayload* dragged = ImGui::GetDragDropPayload();
            const bool draggingObject = dragged && dragged->IsDataType(kObjectPayload);
            if (draggingObject)
            {
                ImGui::TextColored(kPrefabBlue, "Drop here to save it as a prefab");
            }
            else
            {
                ImGui::TextDisabled("Drag an object here from the Hierarchy to save it as a prefab");
            }
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kObjectPayload))
                {
                    std::uint32_t id = 0;
                    std::memcpy(&id, payload->Data, sizeof(id));
                    BeginSavePrefab(id);
                }
                ImGui::EndDragDropTarget();
            }
        }

        if (!projectDir_.empty() && ImGui::SmallButton("Refresh"))
        {
            RefreshPrefabs();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Read the prefab files again (e.g. after editing one by hand) and update the instances");
        }
        ImGui::PopID();
        if (selectInstance != 0)
        {
            scene_.Select(selectInstance);
            FocusSelected();
        }
        if (!place.empty())
        {
            PlacePrefab(place);
        }
        if (!open.empty())
        {
            OpenPrefab(open);
        }
    }

    // Right-click > Save as Prefab, or an object dropped on the Prefabs list: asks for the new prefab's name. Part of
    // an instance is already linked to a prefab (Apply to Prefab changes that one instead).
    void SceneEditor::BeginSavePrefab(std::uint32_t id)
    {
        const GameObject* object = scene_.Find(id);
        if (!object || projectDir_.empty() || InPrefabMode())
        {
            return;
        }
        if (const std::uint32_t root = Engine::Scene::InstanceRootOf(scene_.Description(), id); root != 0)
        {
            const GameObject* rootObject = scene_.Find(root);
            const std::string prefab =
                rootObject && rootObject->prefabInstance ? rootObject->prefabInstance->prefab : std::string{};
            SetStatus(std::format("'{}' is already part of an instance of {}: use Apply to Prefab, or Unpack it first",
                                  object->name, prefab),
                      true);
            return;
        }
        savePrefabObject_ = id;
        CopyToBuffer(prefabName_, object->name);
        std::replace(prefabName_.begin(), prefabName_.end(), ' ', '_');
        showSavePrefabPopup_ = true;
    }

    // Right-click a prefab > Delete Prefab...: its instances in this scene are unpacked, then the file goes
    void SceneEditor::DrawDeletePrefabPopup()
    {
        if (showDeletePrefabPopup_)
        {
            ImGui::OpenPopup("Delete Prefab");
            showDeletePrefabPopup_ = false;
        }
        if (!ImGui::BeginPopupModal("Delete Prefab", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }
        const auto instances = Engine::Game::PrefabManager::InstancesOf(scene_.Description(), deletePrefabPath_);
        const auto variants = prefabs_.VariantsOf(deletePrefabPath_);
        if (interface_ == InterfaceMode::Simple)
        {
            ImGui::Text("Delete the saved object %s?",
                        std::filesystem::path(deletePrefabPath_).stem().string().c_str());
            ImGui::TextDisabled("Its %zu copies in this scene stay, as ordinary objects.", instances.size());
        }
        else
        {
            ImGui::Text("Delete %s?", deletePrefabPath_.c_str());
            ImGui::TextDisabled("Its %zu instances in this scene become ordinary objects (they keep everything they "
                                "have).",
                                instances.size());
            ImGui::TextDisabled("Instances in other scenes keep their objects too, but are no longer updated.");
        }
        if (!variants.empty() && interface_ == InterfaceMode::Advanced)
        {
            ImGui::TextDisabled("Its %zu variants become ordinary prefabs, keeping what they have:", variants.size());
            for (const std::string& variant : variants)
            {
                ImGui::BulletText("%s", variant.c_str());
            }
        }
        ImGui::TextColored(kYellow, "The file is deleted: this cannot be undone.");
        if (ImGui::Button("Delete"))
        {
            try
            {
                const auto deleted = prefabs_.Delete(scene_.Description(), deletePrefabPath_);
                prefabList_ = prefabs_.List();
                SetStatus(std::format("Deleted {} ({} instances unpacked, {} variants made ordinary prefabs, {} "
                                      "prefabs it was nested in keep its objects)",
                                      deletePrefabPath_, deleted.instances, deleted.variants, deleted.containers));
            }
            catch (const std::exception& e)
            {
                SetStatus(e.what(), true);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Right-click a prefab > Create Variant...: a name for the new variant, which then opens in Prefab Mode
    void SceneEditor::DrawCreateVariantPopup()
    {
        if (showVariantPopup_)
        {
            ImGui::OpenPopup("Create Variant");
            showVariantPopup_ = false;
            variantError_.clear();
        }
        if (!ImGui::BeginPopupModal("Create Variant", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }
        ImGui::Text("A variant of %s", variantBase_.c_str());
        ImGui::TextDisabled("It follows %s, except for what you change on it.",
                            std::filesystem::path(variantBase_).stem().string().c_str());
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(240.0f * uiScale_);
        const bool enter =
            ImGui::InputText("Name", variantName_.data(), variantName_.size(), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::TextDisabled("Saved as %s", Engine::Game::PrefabManager::PathFor(variantName_.data()).c_str());
        if (!variantError_.empty())
        {
            ImGui::TextColored(kRed, "%s", variantError_.c_str());
        }
        std::string created;
        if (ImGui::Button("Create") || enter)
        {
            try
            {
                created = prefabs_.CreateVariant(variantBase_, variantName_.data());
                prefabList_ = prefabs_.List();
                ImGui::CloseCurrentPopup();
            }
            catch (const std::exception& e)
            {
                variantError_ = e.what();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        // Like Unity, the new variant opens straight away to be changed
        if (!created.empty())
        {
            OpenPrefab(created);
        }
    }

    // Top of the Hierarchy in Prefab Mode: which prefab is open, Save, and the way back to the scene
    void SceneEditor::DrawPrefabModeBar()
    {
        const bool back = ImGui::Button("< Scene");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Back to the scene: its instances of this prefab take the saved changes");
        }
        if (back)
        {
            ClosePrefab();
        }
        if (!prefabMode_)
        {
            return; // the scene is back: there is no prefab bar to draw any more
        }
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        const GameObject* root = scene_.Find(Engine::Scene::kPrefabRootId);
        const std::string variantOf =
            root && root->prefabInstance
                ? " (variant of " + std::filesystem::path(root->prefabInstance->prefab).stem().string() + ")"
                : std::string{};
        ImGui::TextColored(kPrefabBlue, "Prefab: %s%s%s", prefabMode_->name.c_str(), variantOf.c_str(),
                           HasUnsavedChanges() ? " *" : "");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", prefabMode_->path.c_str());
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!HasUnsavedChanges());
        if (ImGui::Button("Save"))
        {
            SavePrefabMode();
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("Write the prefab file (Ctrl+S)");
        }
        ImGui::TextDisabled("Editing the prefab on its own: every instance takes these changes, except what it changed "
                            "itself.");
        ImGui::Separator();
    }

    // < Scene with unsaved prefab changes: save them, drop them, or stay
    void SceneEditor::DrawClosePrefabPopup()
    {
        if (showClosePrefabPopup_)
        {
            ImGui::OpenPopup("Close Prefab");
            showClosePrefabPopup_ = false;
        }
        if (!ImGui::BeginPopupModal("Close Prefab", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }
        if (!prefabMode_)
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ImGui::Text("Save the changes to the prefab %s?", prefabMode_->name.c_str());
        ImGui::TextDisabled("Its instances take the saved changes.");
        if (ImGui::Button("Save"))
        {
            if (SavePrefabMode())
            {
                LeavePrefabMode();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save"))
        {
            LeavePrefabMode();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Right-click > Save as Prefab: a name for the new prefab file
    void SceneEditor::DrawSavePrefabPopup()
    {
        if (showSavePrefabPopup_)
        {
            ImGui::OpenPopup("Save as Prefab");
            showSavePrefabPopup_ = false;
            prefabError_.clear();
        }
        if (!ImGui::BeginPopupModal("Save as Prefab", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }
        const GameObject* object = scene_.Find(savePrefabObject_);
        if (!object)
        {
            ImGui::CloseCurrentPopup(); // removed meanwhile (e.g. undone)
            ImGui::EndPopup();
            return;
        }
        ImGui::Text("%s and the %zu objects under it", object->name.c_str(),
                    Engine::Scene::SubtreeOf(scene_.Description(), object->id).size() - 1);
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(240.0f * uiScale_);
        const bool enter =
            ImGui::InputText("Name", prefabName_.data(), prefabName_.size(), ImGuiInputTextFlags_EnterReturnsTrue);
        if (interface_ == InterfaceMode::Simple)
        {
            ImGui::TextDisabled("Add copies of it from Assets. Change the saved one, and every copy changes too.");
            ImGui::TextDisabled("Letters, digits, '_' and '-'. This object becomes its first copy.");
        }
        else
        {
            ImGui::TextDisabled("Saved as %s", Engine::Game::PrefabManager::PathFor(prefabName_.data()).c_str());
            ImGui::TextDisabled("Letters, digits, '_' and '-'. The objects become its first instance.");
        }
        if (!prefabError_.empty())
        {
            ImGui::TextColored(kRed, "%s", prefabError_.c_str());
        }
        if (ImGui::Button("Save") || enter)
        {
            try
            {
                const std::string path =
                    prefabs_.CreatePrefab(scene_.Description(), savePrefabObject_, prefabName_.data());
                RefreshPrefabs();
                SetStatus("Saved prefab " + path + " - Place adds more of it");
                ImGui::CloseCurrentPopup();
            }
            catch (const std::exception& e)
            {
                prefabError_ = e.what();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Position / rotation / scale fields in the Inspector
    void SceneEditor::DrawTransform(Engine::Scene::TransformComponent& transform)
    {
        float position[3] = {transform.x, transform.y, transform.z};
        if (ImGui::DragFloat3(Field("Position", Changed({"x", "y", "z"})).c_str(), position, 0.05f, 0.0f, 0.0f, "%.2f"))
        {
            transform.x = position[0];
            transform.y = position[1];
            transform.z = position[2];
        }
        float rotation[3] = {transform.rotationX, transform.rotation, transform.rotationZ};
        if (ImGui::DragFloat3(Field("Rotation", Changed({"rotation_x", "rotation", "rotation_z"})).c_str(), rotation,
                              1.0f, 0.0f, 0.0f, "%.1f"))
        {
            transform.rotationX = rotation[0];
            transform.rotation = rotation[1];
            transform.rotationZ = rotation[2];
        }
        float scale[3] = {transform.scaleX, transform.scaleY, transform.scaleZ};
        if (ImGui::DragFloat3(Field("Scale", Changed({"scale_x", "scale_y", "scale_z"})).c_str(), scale, 0.01f, 0.0f,
                              0.0f, "%.2f"))
        {
            transform.scaleX = scale[0];
            transform.scaleY = scale[1];
            transform.scaleZ = scale[2];
        }
        ImGui::DragFloat(Field("Size", Changed({"size"})).c_str(), &transform.scale, 0.05f, 0.01f, 100.0f, "%.2f",
                         ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Uniform size; the Scale above multiplies it on each axis");
        }
    }

    // Shape, spin speed, tint and the asset pickers (mesh, texture, shader) for a Renderable
    void SceneEditor::DrawRenderable(GameObject& object)
    {
        if (!object.renderable)
        {
            return; // only called for objects that have one
        }
        Engine::Scene::RenderableDesc& renderable = *object.renderable;
        if (!object.transform)
        {
            ImGui::TextColored(kYellow, "Not drawn: it has no Transform to place it in the world.");
        }
        int shape = static_cast<int>(std::find(kShapes.begin(), kShapes.end(), renderable.shape) - kShapes.begin());
        if (ImGui::BeginCombo(Field("Shape", Changed({"shape"})).c_str(), DisplayName(renderable.shape).c_str()))
        {
            for (int i = 0; i < static_cast<int>(kShapes.size()); ++i)
            {
                if (ImGui::Selectable(DisplayName(kShapes[i]).c_str(), i == shape))
                {
                    renderable.shape = kShapes[i];
                }
            }
            ImGui::EndCombo();
        }
        ImGui::DragFloat(Field("Rotation Speed", Changed({"rotation_speed"})).c_str(), &renderable.rotationSpeed, 1.0f);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("Degrees per second it turns around Y while the game runs (0: it stays still)");
        }

        // Assets: paths relative to the game folder, with pickers of the game's own files
        const bool hasProject = !projectDir_.empty();
        std::vector<std::string> textures;
        std::vector<std::string> meshes;
        if (hasProject)
        {
            const Engine::Game::GameProject project(projectDir_);
            // The whole assets folder, like the Assets window: a file counts wherever it was put
            textures = project.FindAssets("assets", Engine::Game::kTextureExtensions);
            meshes = project.FindModels(); // not the originals kept beside a .amodel
        }
        AssetField("Texture", renderable.texture, textures, Changed({"texture"}), Panels::AssetKind::Texture);
        AssetField("Mesh", renderable.mesh, meshes, Changed({"mesh"}), Panels::AssetKind::Model);
        AssetField("Model", renderable.model, meshes, Changed({"model"}), Panels::AssetKind::Model);
        AssetField("Shader", renderable.shader, {}, Changed({"shader"}));

        // Its material: shader, colour, texture and the shader's values in one file, shared by every object using it
        std::vector<std::string> materials;
        if (hasProject)
        {
            materials = Engine::Game::GameProject(projectDir_).FindAssets("assets", Engine::Game::kMaterialExtensions);
        }
        AssetField("Material", renderable.material, materials, Changed({"material"}), Panels::AssetKind::Material);
        FieldLabel("");
        ImGui::BeginDisabled(!hasProject);
        if (ImGui::SmallButton("New Material"))
        {
            try
            {
                renderable.material = Engine::Game::GameProject(projectDir_).CreateMaterial(object.name);
                OpenMaterialEditor(renderable.material);
                assetBrowser_.RequestRescan();
            }
            catch (const std::exception& e)
            {
                SetStatus(e.what());
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("Makes a material for it (in assets/art/materials) and opens it to edit");
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(renderable.material.empty());
        if (ImGui::SmallButton("Edit Material"))
        {
            OpenMaterialEditor(renderable.material);
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();

        // Level of detail is automatic: the engine made simpler versions of the model when it loaded it, and draws
        // the one that suits its size on screen. This only says which one is drawn now.
        if (!renderable.model.empty() && runtime_)
        {
            if (const auto lod = runtime_->LodOf(object.id))
            {
                if (lod->levels > 1)
                {
                    ImGui::TextDisabled("Level of detail: LOD %d of %d, %zu triangles, %.0f%% of the view", lod->level,
                                        lod->levels - 1, lod->triangles, std::min(lod->screenSize, 1.0f) * 100.0f);
                }
                else
                {
                    ImGui::TextDisabled("Level of detail: none needed (%zu triangles)", lod->triangles);
                }
            }
        }

        // A model made facing the other way (or sideways) is turned inside the object: the object, and where it
        // walks, keep their direction
        if (!renderable.model.empty() || !renderable.mesh.empty())
        {
            if (object.parent != 0)
            {
                std::array<char, 128> bone{};
                renderable.holdBone.copy(bone.data(), bone.size() - 1);
                if (ImGui::InputTextWithHint(Field("Held in Bone", Changed({"hold_bone"})).c_str(), "e.g. handslot.r",
                                             bone.data(), bone.size()))
                {
                    renderable.holdBone = bone.data();
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    ImGui::SetTooltip("The name of a bone of its parent's model: it stays in that bone as the parent\n"
                                      "animates (a sword in a hand). Its Transform then places it from the bone.");
                }
            }
            ImGui::Checkbox(Field("View Model", Changed({"view_model"})).c_str(), &renderable.viewModel);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("First-person arms and gun: its Transform is from the camera (x right, y up,\n"
                                  "-z ahead), it moves with the view, and it is drawn over everything, so it never\n"
                                  "goes into a wall. Give what it holds View Model too.");
            }
            ImGui::DragFloat(Field("Model Turn", Changed({"model_turn"})).c_str(), &renderable.modelTurn, 1.0f, -360.0f,
                             360.0f, "%.0f deg");
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip(
                    "Turns the model round inside the object, so it faces the way the object walks\n"
                    "(forward is the way Motion blocks move it). 180 turns a model that faces backwards.");
            }
            FieldLabel("");
            for (const float degrees : {0.0f, 90.0f, 180.0f, 270.0f})
            {
                if (degrees != 0.0f)
                {
                    ImGui::SameLine();
                }
                if (ImGui::SmallButton(std::format("{:.0f}", degrees).c_str()))
                {
                    renderable.modelTurn = degrees;
                }
            }
        }

        // A built-in shape's own size and centre inside the object (a stand-in for a model keeps the model's box)
        if (renderable.mesh.empty() && renderable.model.empty())
        {
            float shapeSize[3] = {renderable.shapeSizeX, renderable.shapeSizeY, renderable.shapeSizeZ};
            if (ImGui::DragFloat3(
                    Field("Shape Size", Changed({"shape_size_x", "shape_size_y", "shape_size_z"})).c_str(), shapeSize,
                    0.01f, 0.01f, 1000.0f, "%.2f"))
            {
                renderable.shapeSizeX = std::max(shapeSize[0], 0.01f);
                renderable.shapeSizeY = std::max(shapeSize[1], 0.01f);
                renderable.shapeSizeZ = std::max(shapeSize[2], 0.01f);
            }
            float shapeCenter[3] = {renderable.shapeCenterX, renderable.shapeCenterY, renderable.shapeCenterZ};
            if (ImGui::DragFloat3(
                    Field("Shape Centre", Changed({"shape_center_x", "shape_center_y", "shape_center_z"})).c_str(),
                    shapeCenter, 0.01f, 0.0f, 0.0f, "%.2f"))
            {
                renderable.shapeCenterX = shapeCenter[0];
                renderable.shapeCenterY = shapeCenter[1];
                renderable.shapeCenterZ = shapeCenter[2];
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("The shape's size and middle inside the object, before its Transform scales it.");
            }
        }

        {
            float rgba[4] = {renderable.albedoR, renderable.albedoG, renderable.albedoB, renderable.albedoA};
            if (ImGui::ColorEdit4(Field("Tint", Changed({"albedo_r", "albedo_g", "albedo_b", "albedo_a"})).c_str(),
                                  rgba, ImGuiColorEditFlags_Float))
            {
                renderable.albedoR = rgba[0];
                renderable.albedoG = rgba[1];
                renderable.albedoB = rgba[2];
                renderable.albedoA = rgba[3];
            }
        }

        if (!renderable.texture.empty() && renderable.mesh.empty() && renderable.model.empty() &&
            renderable.shape != Shape::Cube)
        {
            ImGui::TextColored(kYellow, "Textures show on cubes and on meshes: set Shape to Cube or pick a mesh.");
        }

        if (inspectMode_ == InspectMode::Live)
        {
            return; // the live view only shows the values
        }
        // Bring an image or a model from anywhere on disk into the game and use it: an image is a texture for this 3D
        // object, so it goes to assets/art/textures and becomes the texture; a model (.obj, .fbx, .gltf, ...) is
        // converted by the asset tool into assets/art/models/<name>/ (the .amodel and the original files) with its
        // images in assets/art/textures/<name>/, and becomes the model. A .zip or a folder with one model in it (a
        // downloaded model pack) is converted the same way, the tool picking the model inside and keeping the pack's
        // images
        ImGui::BeginDisabled(!hasProject);
        ImGui::SetNextItemWidth(-ImGui::CalcTextSize("Import").x - ImGui::GetStyle().FramePadding.x * 4.0f);
        ImGui::InputTextWithHint("##import", "C:/path/to/image.png, model.fbx or model_pack.zip", importPath_.data(),
                                 importPath_.size());
        ImGui::SameLine();
        if (ImGui::Button("Import"))
        {
            try
            {
                std::string source = importPath_.data();
                std::erase(source, '"'); // pasted paths are often quoted
                std::string extension = std::filesystem::path(source).extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                const auto isOneOf = [&extension](const auto& list)
                { return std::find(list.begin(), list.end(), extension) != list.end(); };

                const Engine::Game::GameProject project(projectDir_);
                // A .zip or a folder with one model in it: the model pack is converted in one go
                const bool package = Engine::Game::IsModelPackage(source);
                std::string* target = nullptr;
                if (package || Engine::Game::IsModelFile(source))
                {
                    target = &renderable.model;
                }
                else if (isOneOf(Engine::Game::kTextureExtensions))
                {
                    target = &renderable.texture;
                }
                else
                {
                    throw std::runtime_error("expected an image (.png, .jpg, ...), a model (.fbx, .obj, .gltf, .glb, "
                                             ".dae, ...), or a folder or .zip with one model in it");
                }

                std::string note;
                if (target == &renderable.model)
                {
                    // Converted by the asset tool: the .amodel and originals in art/models, the images in art/textures
                    const Engine::Game::ModelImportResult imported = Engine::Game::ImportModel(project, source);
                    *target = imported.path;
                    for (const std::string& warning : imported.warnings)
                    {
                        LOG_WARN(Editor, "Import {}: {}", imported.path, warning);
                    }
                    if (!imported.warnings.empty())
                    {
                        note =
                            " (" + imported.warnings.front() + (imported.warnings.size() > 1 ? ", see Console)" : ")");
                    }
                }
                else
                {
                    *target = project.ImportImage(source, Engine::Game::ImageUse::Texture3D);
                }
                assetBrowser_.RequestRescan(); // the new file shows in the Assets window straight away
                importPath_.fill('\0');
                SetStatus("Imported " + *target + note + " - save the scene to keep it");
            }
            catch (const std::exception& e)
            {
                SetStatus(std::string("Import failed: ") + e.what(), true);
            }
        }
        ImGui::EndDisabled();
    }

    // An asset from the Assets window used on an object (clicked while the object is selected, or dragged onto it); the
    // undo history picks the change up like any other edit
    void SceneEditor::UseAsset(GameObject& object, const Panels::DroppedAsset& asset)
    {
        using Panels::AssetKind;
        const std::string file = std::filesystem::path(asset.path).filename().string();
        if (asset.kind == AssetKind::Audio)
        {
            if (!object.audioSource)
            {
                object.audioSource.emplace();
            }
            object.audioSource->clip = asset.path;
            SetStatus(std::format("{} now plays {}", object.name, file));
        }
        else if (asset.kind == AssetKind::Shader)
        {
            SetStatus("A shader is used through a material: make a material and pick the shader in it");
            return;
        }
        else
        {
            // An object is drawn only once it has a Transform and a Renderable
            if (!object.transform)
            {
                object.transform.emplace();
            }
            if (!object.renderable)
            {
                object.renderable = Engine::Scene::RenderableDesc{.rotationSpeed = 0.0f};
            }
            const bool model = asset.kind == AssetKind::Model;
            const bool material = asset.kind == AssetKind::Material;
            (model      ? object.renderable->model
             : material ? object.renderable->material
                        : object.renderable->texture) = asset.path;
            SetStatus(std::format("Used {} as {}'s {}", file, object.name,
                                  model      ? "model"
                                  : material ? "material"
                                             : "texture"));
        }
        scene_.Select(object.id);
    }

    // The sound an object emits. Volume and the distances are read by the runtime every frame, so dragging them while
    // the game runs is audible straight away.
    void SceneEditor::DrawAudioSource(Engine::Editor::GameObject& object)
    {
        if (!object.audioSource)
        {
            return; // only called for objects that have one
        }
        Engine::Scene::AudioSourceDesc& audio = *object.audioSource;

        std::vector<std::string> clips;
        if (!projectDir_.empty())
        {
            clips = Engine::Game::GameProject(projectDir_)
                        .FindAssets(Engine::Game::kAudioFolder, Engine::Game::kAudioExtensions);
        }
        AssetField("Clip", audio.clip, clips, Changed({"clip"}), Panels::AssetKind::Audio);

        ImGui::SliderFloat(Field("Volume", Changed({"volume"})).c_str(), &audio.volume, 0.0f, 1.0f, "%.2f",
                           ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Takes effect while the game is running");
        }

        // What is actually heard right now: the Volume above after the distance fade. Read back from FMOD rather than
        // worked out here, so it can't disagree with the headphones. Play mode only; it is not saved.
        if (playing_ && runtime_ && audioSystem_)
        {
            const Engine::Audio::VoiceId voice = runtime_->EmitterVoice(object.id);
            if (audioSystem_->IsPlaying(voice))
            {
                const float heard = audioSystem_->GetVoiceAudibility(voice);
                char label[16];
                std::snprintf(label, sizeof(label), "%.0f%%", heard * 100.0f);
                FieldLabel("Heard At");
                ImGui::ProgressBar(heard, ImVec2(-FLT_MIN, 0.0f), label);
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("How loud it is where the camera is right now: Volume after the distance fade");
                }
            }
            else
            {
                FieldLabel("Heard At");
                ImGui::TextDisabled("not playing");
            }
        }

        const char* buses[] = {"sfx", "music"};
        int bus = audio.bus == "music" ? 1 : 0;
        if (ImGui::Combo(Field("Bus", Changed({"bus"})).c_str(), &bus, buses, 2))
        {
            audio.bus = buses[bus];
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Which group's volume controls this sound (Audio window)");
        }

        ImGui::Checkbox(Field("Loop", Changed({"loop"})).c_str(), &audio.loop);
        ImGui::Checkbox(Field("Play On Start", Changed({"play_on_start"})).c_str(), &audio.playOnStart);

        ImGui::Checkbox(Field("3D", Changed({"spatial"})).c_str(), &audio.spatial);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Off: plays at the same volume wherever the camera is (background music)");
        }
        if (audio.spatial)
        {
            ImGui::DragFloat(Field("Min Distance", Changed({"min_distance"})).c_str(), &audio.minDistance, 0.1f, 0.1f,
                             1000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Full volume within this many units of the object");
            }
            ImGui::DragFloat(Field("Max Distance", Changed({"max_distance"})).c_str(), &audio.maxDistance, 0.5f, 0.2f,
                             10000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Silent from this many units away; it fades evenly from Min Distance to here");
            }
            if (audio.maxDistance <= audio.minDistance)
            {
                ImGui::TextColored(kYellow, "Max Distance must be above Min Distance, or nothing fades.");
            }
        }

        if (!object.transform)
        {
            ImGui::TextColored(kYellow, "Needs a Transform to be in the game.");
        }
        if (audio.clip.empty())
        {
            ImGui::TextDisabled("No clip: silent. Put a file in %s to pick one.", Engine::Game::kAudioFolder);
        }
        else if (audio.spatial)
        {
            ImGui::TextDisabled("Only mono files can be positioned; a stereo one plays flat.");
        }
    }

    // Only the components the selected object has are shown; Add Component adds the others
    void SceneEditor::DrawInspector()
    {
        GameObject* object = scene_.Find(scene_.Selected());
        if (!object)
        {
            ImGui::TextWrapped("Select a game object in the Hierarchy (or the Viewport) to view and edit it.");
            return;
        }

        // Header: the name across the row (while the game runs, only shown: values are changed when it stops), then
        // the eye that moves the live values to a floating window
        const float eye = ImGui::GetFrameHeight();
        const float nameWidth = ImGui::GetContentRegionAvail().x - eye - ImGui::GetStyle().ItemSpacing.x;
        if (playing_)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(object->name.c_str());
        }
        else
        {
            std::array<char, 128> name{};
            CopyToBuffer(name, object->name);
            ImGui::SetNextItemWidth(nameWidth);
            if (ImGui::InputTextWithHint("##Name", "Name", name.data(), name.size()))
            {
                object->name = name.data();
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("Game Object %u", object->id);
        }
        ImGui::SameLine(ImGui::GetCursorStartPos().x + nameWidth + ImGui::GetStyle().ItemSpacing.x);
        DrawFloatingToggle(eye);
        if (const GameObject* parent = scene_.Find(object->parent))
        {
            ImGui::TextDisabled("Under %s: the Transform is relative to it", parent->name.c_str());
        }
        if (playing_ && !showVariables_)
        {
            // The values as the game has them now, with what changed them; the editing fields are back after Stop
            ImGui::Spacing();
            DrawLiveValues();
            return;
        }

        // Part of a prefab instance: the prefab bar. What it does (Apply, Open, Unpack) can add objects or replace the
        // scene, so it waits until the Inspector is drawn.
        std::function<void()> prefabAction;
        if (object->prefabInstance && !playing_)
        {
            prefabAction = DrawPrefabInstance(*object);
        }
        ImGui::Spacing();

        // While the game runs with the live values floating: the saved values, locked
        DrawInspectorComponents(*object, false);
        if (playing_)
        {
            ImGui::TextWrapped("Live values are in the floating window. Stop the game to change values.");
        }

        if (prefabAction)
        {
            prefabAction();
        }
    }

    void SceneEditor::DrawFloatingToggle(float size)
    {
        if (Theme::IconButton("##variables", Theme::Icon::Watch, size,
                              showVariables_ ? "Hide live values" : "Show live values", showVariables_))
        {
            showVariables_ = !showVariables_;
        }
    }

    const SceneEditor::ModelFacts& SceneEditor::ModelFactsOf(const std::string& modelPath) const
    {
        const std::filesystem::path file = (projectDir_ / modelPath).lexically_normal();
        std::error_code ec;
        const auto written = std::filesystem::last_write_time(file, ec);
        ModelFacts& facts = modelFacts_[file.generic_string()];
        if (facts.written != written || written == std::filesystem::file_time_type{})
        {
            // A new file, or one imported again since it was read
            facts = ModelFacts{.written = written};
            if (const auto model = Engine::Assets::LoadModelFromFile(file))
            {
                for (const Engine::Assets::AnimationClip& clip : model->animations)
                {
                    facts.animations.push_back(clip.name);
                }
                facts.bounds = Engine::Assets::RestBounds(*model);
                facts.triangles = Engine::Assets::RestTriangles(*model);
            }
        }
        return facts;
    }

    void SceneEditor::OpenMaterialEditor(const std::string& path)
    {
        std::string error;
        const std::filesystem::path file = projectDir_ / path;
        const auto read = Engine::Assets::ReadMaterialFile(file, &error);
        if (!read)
        {
            SetStatus("Could not open the material: " + error);
            return;
        }
        materialPath_ = path;
        materialDraft_ = *read;
        materialError_.clear();
        std::error_code ec;
        materialWritten_ = std::filesystem::last_write_time(file, ec);
        focusMaterialEditor_ = true;
    }

    // The Material window: what the open material file holds, saved as it is changed (the running scene reads it
    // again within a second, so every object using it changes)
    void SceneEditor::DrawMaterialEditor()
    {
        if (materialPath_.empty() || projectDir_.empty())
        {
            return;
        }
        const std::filesystem::path file = projectDir_ / materialPath_;
        std::error_code ec;
        // Saved somewhere else since (in a code editor, say): read it again
        if (const auto written = std::filesystem::last_write_time(file, ec); !ec && written != materialWritten_)
        {
            if (const auto read = Engine::Assets::ReadMaterialFile(file))
            {
                materialDraft_ = *read;
            }
            materialWritten_ = written;
        }

        bool open = true;
        ImGui::SetNextWindowSize(ImVec2(380.0f * uiScale_, 0.0f), ImGuiCond_FirstUseEver);
        if (focusMaterialEditor_)
        {
            ImGui::SetNextWindowFocus(); // just opened: in front
            focusMaterialEditor_ = false;
        }
        const std::string title = std::format("Material: {}###MaterialEditor", file.stem().string());
        if (!ImGui::Begin(title.c_str(), &open))
        {
            ImGui::End();
            if (!open)
            {
                materialPath_.clear();
            }
            return;
        }

        Engine::Assets::MaterialFile& m = materialDraft_;
        const Engine::Assets::MaterialFile before = m;
        const Engine::Game::GameProject project(projectDir_);
        ImGui::TextDisabled("%s", materialPath_.c_str());

        // Shader: the standard lit one, or one of the game's own (not the engine's copies in shaders/Builtin)
        std::vector<std::string> shaders = project.FindAssets("assets", Engine::Game::kShaderExtensions);
        std::erase_if(shaders, [](const std::string& path) { return path.find("/Builtin/") != std::string::npos; });
        const std::string shown = m.shader.empty() ? "Standard (lit)" : std::filesystem::path(m.shader).stem().string();
        if (ImGui::BeginCombo(Field("Shader").c_str(), shown.c_str()))
        {
            if (ImGui::Selectable("Standard (lit)", m.shader.empty()))
            {
                m.shader.clear();
            }
            for (const std::string& shader : shaders)
            {
                if (ImGui::Selectable(shader.c_str(), shader == m.shader))
                {
                    m.shader = shader;
                }
            }
            ImGui::EndCombo();
        }
        FieldLabel("");
        if (ImGui::SmallButton("New Shader..."))
        {
            CopyToBuffer(newShaderName_, file.stem().string());
            ImGui::OpenPopup("##newShader");
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("Makes a shader of the game's own from the standard one, uses it here, and opens it");
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(m.shader.empty());
        if (ImGui::SmallButton("Open in Code Editor"))
        {
            std::string error;
            if (!OpenWithDefaultApp(projectDir_ / std::filesystem::path(m.shader).replace_extension(".frag"), error))
            {
                SetStatus("Could not open the shader: " + error);
            }
        }
        ImGui::EndDisabled();
        if (ImGui::BeginPopup("##newShader"))
        {
            ImGui::InputTextWithHint("##name", "Shader name", newShaderName_.data(), newShaderName_.size());
            if (ImGui::Button("Make"))
            {
                try
                {
                    m.shader = project.CreateShader(newShaderName_.data());
                    if (std::none_of(m.values.begin(), m.values.end(),
                                     [](const auto& value) { return value.first == "u_Brightness"; }))
                    {
                        m.values.emplace_back("u_Brightness", 1.0f); // the value the new shader reads
                    }
                    std::string error;
                    OpenWithDefaultApp(projectDir_ / std::filesystem::path(m.shader).replace_extension(".frag"), error);
                    assetBrowser_.RequestRescan();
                    ImGui::CloseCurrentPopup();
                }
                catch (const std::exception& e)
                {
                    materialError_ = e.what();
                }
            }
            ImGui::EndPopup();
        }

        ImGui::ColorEdit4(Field("Colour").c_str(), &m.color[0], ImGuiColorEditFlags_Float);
        AssetField("Texture", m.texture, project.FindAssets("assets", Engine::Game::kTextureExtensions), false,
                   Panels::AssetKind::Texture);
        ImGui::ColorEdit3(Field("Glow").c_str(), &m.emissive[0], ImGuiColorEditFlags_Float);
        ImGui::DragFloat(Field("Glow Strength").c_str(), &m.emissiveStrength, 0.01f, 0.0f, 20.0f, "%.2f",
                         ImGuiSliderFlags_AlwaysClamp);

        // What a custom shader reads: its values by name, and the metal and roughness the engine passes on
        if (!m.shader.empty())
        {
            ImGui::SeparatorText("Values for the shader");
            ImGui::TextDisabled("It reads each as: uniform float <name>;");
            for (std::size_t i = 0; i < m.values.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                ImGui::DragFloat(Field(m.values[i].first.c_str()).c_str(), &m.values[i].second, 0.01f);
                ImGui::SameLine();
                const bool remove = ImGui::SmallButton("x");
                ImGui::PopID();
                if (remove)
                {
                    m.values.erase(m.values.begin() + static_cast<std::ptrdiff_t>(i));
                    break;
                }
            }
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
            ImGui::InputTextWithHint("##valueName", "u_Name", materialValueName_.data(), materialValueName_.size());
            ImGui::SameLine();
            const std::string name = materialValueName_.data();
            ImGui::BeginDisabled(name.empty() ||
                                 std::any_of(m.values.begin(), m.values.end(),
                                             [&name](const auto& value) { return value.first == name; }));
            if (ImGui::Button("Add Value"))
            {
                m.values.emplace_back(name, 0.0f);
                materialValueName_.fill('\0');
            }
            ImGui::EndDisabled();
            ImGui::DragFloat(Field("Metal (u_Metallic)").c_str(), &m.metallic, 0.01f, 0.0f, 1.0f, "%.2f",
                             ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat(Field("Roughness (u_Roughness)").c_str(), &m.roughness, 0.01f, 0.0f, 1.0f, "%.2f",
                             ImGuiSliderFlags_AlwaysClamp);
        }

        if (!materialError_.empty())
        {
            ImGui::TextColored(kYellow, "%s", materialError_.c_str());
        }
        ImGui::Spacing();
        ImGui::TextDisabled("Saved as you change it: every object using it changes too.");

        if (m != before)
        {
            std::string error;
            if (Engine::Assets::WriteMaterialFile(file, m, &error))
            {
                materialWritten_ = std::filesystem::last_write_time(file, ec);
                materialError_.clear();
            }
            else
            {
                materialError_ = error;
            }
        }
        ImGui::End();
        if (!open)
        {
            materialPath_.clear();
        }
    }

    void SceneEditor::DrawInspectorComponents(GameObject& target, bool simple, InspectMode mode,
                                              std::vector<Panels::VariableInspectorPanel::FieldBox>* fields)
    {
        GameObject* const object = &target;

        // What the helpers below draw for, until this returns
        const struct Restore
        {
            SceneEditor& editor;
            InspectMode mode;
            std::vector<Panels::VariableInspectorPanel::FieldBox>* fields;
            ~Restore()
            {
                editor.inspectMode_ = mode;
                editor.liveFields_ = fields;
                editor.nextFields_.clear();
            }
        } restore{*this, inspectMode_, liveFields_};
        inspectMode_ = mode;
        liveFields_ = fields;
        const bool live = mode == InspectMode::Live;

        // Each component: its header says whether this prefab instance changed it, and its fields that were changed
        // are marked (see Changed). The Simple interface names them for learners and starts them folded, as a list of
        // the object's parts.
        const auto changed = [&](const char* key)
        {
            inspectedOverrides_ = object->prefabInstance ? &object->prefabInstance->overrides : nullptr;
            inspectedComponent_ = key;
            if (simple && !live) // the live view starts open: it is there to be watched
            {
                ImGui::SetNextItemOpen(false, ImGuiCond_Once);
            }
            return inspectedOverrides_ &&
                   std::any_of(inspectedOverrides_->begin(), inspectedOverrides_->end(),
                               [key](const std::string& property)
                               { return property == key || property.starts_with(std::string(key) + "/"); });
        };
        const auto name = [simple](const char* advanced, const char* learner) { return simple ? learner : advanced; };

        if (object->transform)
        {
            const bool transformChanged = changed("transform");
            if (simple)
            {
                ImGui::SetNextItemOpen(true, ImGuiCond_Once); // the part a learner opens this list for most
            }
            ComponentSection(name("Transform", "Position & Size"), transformChanged, object->transform,
                             [&] { DrawTransform(*object->transform); });
        }
        if (object->renderable)
        {
            ComponentSection(name("Renderable", "Looks"), changed("renderable"), object->renderable,
                             [&] { DrawRenderable(*object); });
        }
        if (object->audioSource)
        {
            ComponentSection(name("Audio Source", "Sound"), changed("audio_source"), object->audioSource,
                             [&] { DrawAudioSource(*object); });
        }
        const auto drawPlayer = [&]
        {
            Engine::Scene::PlayerControllerComponent& player = *object->player;
            ImGui::TextWrapped(
                "While the game runs, the move keys drive this object the way it faces, and holding Look "
                "(right mouse) turns it. Keys: Project > Controls.");
            ImGui::DragFloat(Field("Speed", Changed({"speed"})).c_str(), &player.speed, 0.1f, 0.0f, 100.0f, "%.1f",
                             ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat(Field("Fast Multiplier", Changed({"fast_multiplier"})).c_str(), &player.fastMultiplier,
                             0.05f, 1.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat(Field("Look Sensitivity", Changed({"look_sensitivity"})).c_str(), &player.lookSensitivity,
                             0.01f, 0.01f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::Checkbox(Field("Camera Follows", Changed({"follow_camera"})).c_str(), &player.followCamera);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("The Main camera (or the game view, without one) stays behind this object");
            }
            ImGui::BeginDisabled(!player.followCamera);
            ImGui::DragFloat(Field("Camera Distance", Changed({"camera_offset_z"})).c_str(), &player.cameraOffsetZ,
                             0.05f, Engine::Systems::PlayerControllerSystem::kMinDistance,
                             Engine::Systems::PlayerControllerSystem::kMaxDistance, "%.2f",
                             ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat(Field("Camera Height", Changed({"camera_offset_y"})).c_str(), &player.cameraOffsetY, 0.05f,
                             -10.0f, 10.0f, "%.2f");
            ImGui::DragFloat(Field("Camera Pitch", Changed({"camera_pitch"})).c_str(), &player.cameraPitch, 0.5f,
                             Engine::Systems::PlayerControllerSystem::kMinPitch,
                             Engine::Systems::PlayerControllerSystem::kMaxPitch, "%.0f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::EndDisabled();
            if (!object->transform)
            {
                ImGui::TextColored(kYellow, "Needs a Transform to move.");
            }
            else if (!object->renderable)
            {
                ImGui::TextColored(kYellow, "Moves, but is not drawn without a Renderable.");
            }
        };
        if (object->player)
        {
            ComponentSection(name("Player", "Player Controls"), changed("player"), object->player, drawPlayer);
        }
        const auto drawCamera = [&]
        {
            Engine::Scene::CameraComponent& camera = *object->camera;
            ImGui::DragFloat(Field("Field of View", Changed({"field_of_view"})).c_str(), &camera.fieldOfView, 1.0f,
                             1.0f, 179.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat(Field("Near Plane", Changed({"near_plane"})).c_str(), &camera.nearPlane, 0.01f, 0.001f,
                             camera.farPlane, "%.3f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat(Field("Far Plane", Changed({"far_plane"})).c_str(), &camera.farPlane, 1.0f,
                             camera.nearPlane, 100000.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::Checkbox(Field("Main Camera", Changed({"primary"})).c_str(), &camera.primary) && camera.primary)
            {
                // One Main camera per scene: the one Play mode renders with
                // cppcheck-suppress constVariableReference ; not const: other.camera->primary is changed below
                for (GameObject& other : scene_.Objects())
                {
                    if (other.id != object->id && other.camera)
                    {
                        other.camera->primary = false;
                    }
                }
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(
                    "The running game is seen through the Main camera, from this object's Transform (behind the "
                    "player when the player's camera follows)");
            }
            if (!object->transform)
            {
                ImGui::TextColored(kYellow, "Needs a Transform to be in the game.");
            }
        };
        if (object->camera)
        {
            ComponentSection("Camera", changed("camera"), object->camera, drawCamera);
        }

        const auto drawLight = [&]
        {
            auto& light = *object->light;

            ImGui::Checkbox(Field("Enabled", Changed({"enabled"})).c_str(), &light.enabled);

            using Engine::Graphics::LightType;
            int typeIndex = light.type == LightType::Directional ? 0 : light.type == LightType::Spot ? 2 : 1;

            if (ImGui::Combo(Field("Type", Changed({"type"})).c_str(), &typeIndex, "Directional\0Point\0Spot\0"))
            {
                light.type = typeIndex == 0   ? LightType::Directional
                             : typeIndex == 2 ? LightType::Spot
                                              : LightType::Point;
            }

            float color[3] = {light.colorR, light.colorG, light.colorB};

            if (ImGui::ColorEdit3(Field("Color", Changed({"color_r", "color_g", "color_b"})).c_str(), color))
            {
                light.colorR = color[0];
                light.colorG = color[1];
                light.colorB = color[2];
            }

            ImGui::DragFloat(Field("Intensity", Changed({"intensity"})).c_str(), &light.intensity, 0.05f, 0.0f, 100.0f,
                             "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SliderFloat(Field("Flicker", Changed({"flicker"})).c_str(), &light.flicker, 0.0f, 1.0f, "%.2f",
                               ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("How much it flickers while the game runs, like a flame: 0 steady, 1 dips to dark");
            }

            if (light.type != LightType::Directional)
            {
                ImGui::DragFloat(Field("Range", Changed({"range"})).c_str(), &light.range, 0.1f, 0.01f, 1000.0f, "%.2f",
                                 ImGuiSliderFlags_AlwaysClamp);
            }

            if (light.type == LightType::Spot)
            {
                ImGui::DragFloat(Field("Inner Cone", Changed({"spot_inner"})).c_str(), &light.spotInnerConeDeg, 0.5f,
                                 0.0f, 89.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);

                ImGui::DragFloat(Field("Outer Cone", Changed({"spot_outer"})).c_str(), &light.spotOuterConeDeg, 0.5f,
                                 0.0f, 89.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);

                light.spotOuterConeDeg = std::max(light.spotOuterConeDeg, light.spotInnerConeDeg);

                ImGui::TextDisabled("Spot lights are not rendered yet.");
            }

            if (!object->transform)
            {
                ImGui::TextColored(kYellow, "Needs a Transform to be in the game.");
            }

            ImGui::TextDisabled("The Transform sets the light's position and direction.");
        };

        if (object->light)
        {
            ComponentSection("Light", changed("light"), object->light, drawLight);
        }

        // On the game's screen: what it shows, where (drag it in the view), how big, its colour
        const auto drawScreen = [&]
        {
            using Engine::Scene::ScreenElementKind;
            Engine::Scene::ScreenElementComponent& e = *object->screen;
            const auto textField = [](const std::string& label, std::string& value, const char* hint)
            {
                std::array<char, 256> buffer{};
                value.copy(buffer.data(), buffer.size() - 1);
                if (ImGui::InputTextWithHint(label.c_str(), hint, buffer.data(), buffer.size()))
                {
                    value = buffer.data();
                }
            };
            int kind = static_cast<int>(e.kind);
            if (ImGui::Combo(Field("Kind", Changed({"kind"})).c_str(), &kind, "Text\0Picture\0Bar\0Button\0"))
            {
                e.kind = static_cast<ScreenElementKind>(kind);
            }
            if (e.kind != ScreenElementKind::Picture)
            {
                textField(Field(e.kind == ScreenElementKind::Button ? "Label"
                                : e.kind == ScreenElementKind::Bar  ? "Name"
                                                                    : "Text",
                                Changed({"text"})),
                          e.text, "What it says");
            }
            if (e.kind != ScreenElementKind::Text)
            {
                std::vector<std::string> pictures;
                if (!projectDir_.empty())
                {
                    for (const std::string& each :
                         Engine::Game::GameProject(projectDir_).FindAssets("assets", Engine::Game::kTextureExtensions))
                    {
                        pictures.push_back(std::filesystem::path(each).stem().string());
                    }
                }
                const bool needsPicture = e.kind == ScreenElementKind::Picture;
                const char* const label = needsPicture                       ? "Picture"
                                          : e.kind == ScreenElementKind::Bar ? "Fill Picture"
                                                                             : "Background Picture";
                if (ImGui::BeginCombo(Field(label, Changed({"picture"})).c_str(), !e.picture.empty() ? e.picture.c_str()
                                                                                  : needsPicture     ? "(choose one)"
                                                                                                     : "(none)"))
                {
                    if (!needsPicture && ImGui::Selectable("(none)", e.picture.empty()))
                    {
                        e.picture.clear();
                    }
                    for (const std::string& each : pictures)
                    {
                        if (ImGui::Selectable(each.c_str(), each == e.picture))
                        {
                            e.picture = each;
                        }
                    }
                    if (pictures.empty())
                    {
                        ImGui::TextDisabled("No pictures: import some into assets/art/sprites");
                    }
                    ImGui::EndCombo();
                }
                if (!needsPicture && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    ImGui::SetTooltip(e.kind == ScreenElementKind::Bar
                                          ? "A picture of your own for the bar's filled part, tinted by the Colour"
                                          : "A picture of your own for the button, tinted by the Colour (white: as "
                                            "it is)");
                }
            }
            ImGui::Checkbox(Field("Above This Object", Changed({"above_object"})).c_str(), &e.aboveObject);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Shown over this object in the world and following it (a health bar, a name), "
                                  "instead of at a place on the screen.");
            }
            if (e.kind == ScreenElementKind::Bar)
            {
                // Filled by a variable of an object with blocks
                if (ImGui::BeginCombo(Field("Variable of", Changed({"bar_object"})).c_str(),
                                      e.barObject.empty() ? "(this object)" : e.barObject.c_str()))
                {
                    if (ImGui::Selectable("(this object)", e.barObject.empty()))
                    {
                        e.barObject.clear();
                    }
                    for (const GameObject& other : scene_.Objects())
                    {
                        if (other.logic && !other.logic->variables.empty() &&
                            ImGui::Selectable(other.name.c_str(), other.name == e.barObject))
                        {
                            e.barObject = other.name;
                            e.barVariable = other.logic->variables.front().name;
                        }
                    }
                    ImGui::EndCombo();
                }
                const auto owner = std::find_if(
                    scene_.Objects().begin(), scene_.Objects().end(), [&e, &object](const GameObject& other)
                    { return e.barObject.empty() ? other.id == object->id : other.name == e.barObject; });
                if (owner != scene_.Objects().end() && owner->logic &&
                    ImGui::BeginCombo(Field("Variable", Changed({"bar_variable"})).c_str(), e.barVariable.c_str()))
                {
                    for (const Engine::Logic::Variable& variable : owner->logic->variables)
                    {
                        if (ImGui::Selectable(variable.name.c_str(), variable.name == e.barVariable))
                        {
                            e.barVariable = variable.name;
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::DragFloat(Field("Full at", Changed({"bar_maximum"})).c_str(), &e.barMaximum, 0.5f, 0.0f, 1e6f,
                                 "%g");
            }
            int anchor = static_cast<int>(e.anchor);
            if (ImGui::Combo(Field("Placed from", Changed({"anchor"})).c_str(), &anchor,
                             "Top left\0Top\0Top right\0Left\0Middle\0Right\0Bottom left\0Bottom\0Bottom right\0"))
            {
                e.anchor = static_cast<Engine::Scene::ScreenAnchor>(anchor);
            }
            ImGui::DragFloat(Field("Across", Changed({"x"})).c_str(), &e.x, 0.2f, -100.0f, 100.0f, "%.1f %%");
            ImGui::DragFloat(Field("Down", Changed({"y"})).c_str(), &e.y, 0.2f, -100.0f, 100.0f, "%.1f %%");
            ImGui::DragFloat(Field("Height", Changed({"height"})).c_str(), &e.height, 0.1f, 0.5f, 100.0f,
                             "%.1f %% of the screen", ImGuiSliderFlags_AlwaysClamp);
            if (e.kind == ScreenElementKind::Bar || e.kind == ScreenElementKind::Button)
            {
                ImGui::DragFloat(Field("Width", Changed({"width"})).c_str(), &e.width, 0.2f, 0.0f, 300.0f,
                                 e.width <= 0.0f ? "fitted" : "%.1f %%", ImGuiSliderFlags_AlwaysClamp);
            }
            float rgb[3] = {e.colorR, e.colorG, e.colorB};
            if (ImGui::ColorEdit3(Field("Colour", Changed({"color_r", "color_g", "color_b"})).c_str(), rgb))
            {
                e.colorR = rgb[0];
                e.colorG = rgb[1];
                e.colorB = rgb[2];
            }
            ImGui::DragInt(Field("Layer", Changed({"layer"})).c_str(), &e.layer, 0.1f, -100, 100);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("Higher layers are drawn over lower ones");
            }
            ImGui::Checkbox(Field("Shown at the start", Changed({"visible"})).c_str(), &e.visible);
            ImGui::TextDisabled("Drag it in the view to move it, or its corner to resize it.");
        };
        if (object->screen)
        {
            ComponentSection(name("Screen Element", "On Screen"), changed("screen"), object->screen, drawScreen);
        }

        const auto drawAnimator = [&]
        {
            Engine::Scene::AnimatorComponent& a = *object->animator;
            const std::string model = object->renderable ? object->renderable->model : std::string{};
            if (model.empty())
            {
                ImGui::TextDisabled("Give it a model with animations (Mesh Renderer > Model) to play one.");
                return;
            }
            // Its model's own clips, then those of its animation files (a character pack's separate files)
            std::vector<std::string> clips = ModelFactsOf(model).animations;
            for (const std::string& source : a.sources)
            {
                for (const std::string& clip : ModelFactsOf(source).animations)
                {
                    if (std::find(clips.begin(), clips.end(), clip) == clips.end())
                    {
                        clips.push_back(clip);
                    }
                }
            }
            if (clips.empty())
            {
                ImGui::TextDisabled("No animations: add an animation file below, or use a model that has some.");
            }
            const std::string shown = a.clip.empty() ? (clips.empty() ? "(none)" : clips.front() + " (first)") : a.clip;
            if (ImGui::BeginCombo(Field("Animation", Changed({"clip"})).c_str(), shown.c_str()))
            {
                for (const std::string& clip : clips)
                {
                    if (ImGui::Selectable(clip.c_str(), clip == a.clip))
                    {
                        a.clip = clip;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::Checkbox(Field("Playing", Changed({"playing"})).c_str(), &a.playing);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("Whether it moves when the game starts; the Animation blocks start and stop it");
            }
            ImGui::Checkbox(Field("Loop", Changed({"loop"})).c_str(), &a.loop);
            ImGui::DragFloat(Field("Speed", Changed({"speed"})).c_str(), &a.speed, 0.01f, -10.0f, 10.0f, "%.2f");

            // Animation files: models made for the same skeleton whose clips it can play too (matched by bone name)
            ImGui::SeparatorText("Animation files");
            for (std::size_t i = 0; i < a.sources.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                const std::size_t count = ModelFactsOf(a.sources[i]).animations.size();
                ImGui::TextUnformatted(std::filesystem::path(a.sources[i]).filename().string().c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("%zu clip%s", count, count == 1 ? "" : "s");
                ImGui::SameLine();
                const bool remove = ImGui::SmallButton("x");
                ImGui::PopID();
                if (remove)
                {
                    a.sources.erase(a.sources.begin() + static_cast<std::ptrdiff_t>(i));
                    break;
                }
            }
            // Picked from the game's models, or dropped from the Assets window (a half-typed path is not added)
            const std::vector<std::string> models =
                projectDir_.empty() ? std::vector<std::string>{} : Engine::Game::GameProject(projectDir_).FindModels();
            std::string adding;
            if (AssetField("Add file", adding, models, Changed({"sources"}), Panels::AssetKind::Model) &&
                adding != model && std::find(models.begin(), models.end(), adding) != models.end() &&
                std::find(a.sources.begin(), a.sources.end(), adding) == a.sources.end())
            {
                a.sources.push_back(adding);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("A file of animations made for the same skeleton (a character pack's\n"
                                  "Animations folder): its clips are added to the Animation list");
            }
            ImGui::TextDisabled("It shows the animation's first moment here, and moves in Play.");
        };
        if (object->animator)
        {
            ComponentSection(name("Animator", "Animation"), changed("animator"), object->animator, drawAnimator);
        }

        const auto drawSky = [&]
        {
            Engine::Scene::SkyComponent& s = *object->sky;
            const auto colour =
                [&](const char* label, float& r, float& g, float& b, std::initializer_list<std::string_view> keys)
            {
                float rgb[3] = {r, g, b};
                if (ImGui::ColorEdit3(Field(label, Changed(keys)).c_str(), rgb))
                {
                    r = rgb[0];
                    g = rgb[1];
                    b = rgb[2];
                }
            };
            // A picture of the whole sky, from the game's pictures or dropped from the Assets window
            const std::vector<std::string> pictures =
                projectDir_.empty()
                    ? std::vector<std::string>{}
                    : Engine::Game::GameProject(projectDir_).FindAssets("assets", Engine::Game::kTextureExtensions);
            AssetField("Picture", s.picture, pictures, Changed({"picture"}), Panels::AssetKind::Texture);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("A panorama of the whole sky (twice as wide as it is high, the horizon across its\n"
                                  "middle), wrapped all round the scene. Empty: the colours below");
            }
            if (!s.picture.empty())
            {
                ImGui::DragFloat(Field("Turn", Changed({"turn"})).c_str(), &s.turn, 1.0f, -360.0f, 360.0f,
                                 "%.0f degrees");
                if (ImGui::SmallButton("Use the colours instead"))
                {
                    s.picture.clear();
                }
            }
            else
            {
                colour("Top", s.topR, s.topG, s.topB, {"top_r", "top_g", "top_b"});
                colour("Horizon", s.horizonR, s.horizonG, s.horizonB, {"horizon_r", "horizon_g", "horizon_b"});
                colour("Ground", s.groundR, s.groundG, s.groundB, {"ground_r", "ground_g", "ground_b"});
            }
            ImGui::DragFloat(Field("Haze", Changed({"haze"})).c_str(), &s.haze, 0.0005f, 0.0f, 0.2f, "%.4f",
                             ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("How quickly far things fade into the horizon colour: 0 none, 0.05 thick fog");
            }
            ImGui::SliderFloat(Field("Ambient Light", Changed({"ambient"})).c_str(), &s.ambient, 0.0f, 1.0f, "%.2f",
                               ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("Light from all around, reaching what no light shines on. Low for a dark night.");
            }
            ImGui::TextDisabled("The scene's sky (the first in the scene, if several). The Sky blocks change it.");
        };
        if (object->sky)
        {
            ComponentSection("Sky", changed("sky"), object->sky, drawSky);
        }

        const auto drawRigidBody = [&]
        {
            using Engine::Scene::BodyType;
            using Engine::Scene::ColliderShape;
            Engine::Scene::RigidBodyComponent& body = *object->rigidBody;
            // In BodyType's order
            int type = static_cast<int>(body.type);
            if (ImGui::Combo(Field("Type", Changed({"type"})).c_str(), &type, "Static\0Dynamic\0Kinematic\0"))
            {
                body.type = static_cast<BodyType>(type);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Static: never moves (floors, walls)\nDynamic: falls and is pushed by the physics\n"
                                  "Kinematic: follows its Transform and pushes dynamic bodies away");
            }
            ImGui::BeginDisabled(body.type != BodyType::Dynamic);
            ImGui::DragFloat(Field("Mass", Changed({"mass"})).c_str(), &body.mass, 0.1f, 0.01f, 10000.0f, "%.2f kg",
                             ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat(Field("Gravity Scale", Changed({"gravity_scale"})).c_str(), &body.gravityScale, 0.01f,
                             -10.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("How much of the game's gravity (Project > Physics) pulls it: 1 normal, 0 floats, 2 "
                                  "falls twice as fast, below 0 rises");
            }
            float startVelocity[3] = {body.startVelocityX, body.startVelocityY, body.startVelocityZ};
            const std::string startVelocityLabel =
                Field("Start Velocity", Changed({"start_velocity_x", "start_velocity_y", "start_velocity_z"}));
            if (ImGui::DragFloat3(startVelocityLabel.c_str(), startVelocity, 0.1f, -1000.0f, 1000.0f, "%.1f",
                                  ImGuiSliderFlags_AlwaysClamp))
            {
                body.startVelocityX = startVelocity[0];
                body.startVelocityY = startVelocity[1];
                body.startVelocityZ = startVelocity[2];
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("How fast it is already moving when Play starts, in units per second on X, Y and Z: "
                                  "throw a ball, fire a shot. Up is +Y.");
            }
            ImGui::EndDisabled();

            // The surface: how it slides and bounces against other bodies
            ImGui::DragFloat(Field("Friction", Changed({"friction"})).c_str(), &body.friction, 0.01f, 0.0f, 2.0f,
                             "%.2f", ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("How much it grips what it slides on: 0 ice, 0.5 most things, 1 rubber. Of two "
                                  "touching objects the slipperier one counts.");
            }
            ImGui::DragFloat(Field("Bounciness", Changed({"bounciness"})).c_str(), &body.bounciness, 0.01f, 0.0f, 1.0f,
                             "%.2f", ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("How much of its speed it bounces back with: 0 none, 1 back as high as it fell. Of "
                                  "two touching objects the bouncier one counts.");
            }
            FieldLabel("");
            for (const auto& [label, friction, bounciness] :
                 {std::tuple{"Ice", 0.02f, 0.0f}, std::tuple{"Normal", 0.5f, 0.0f}, std::tuple{"Bouncy", 0.5f, 0.8f}})
            {
                if (label != std::string_view("Ice"))
                {
                    ImGui::SameLine();
                }
                if (ImGui::SmallButton(label))
                {
                    body.friction = friction;
                    body.bounciness = bounciness;
                }
            }
            // What it is made of: the footsteps on it ("play footstep")
            const std::string surfaceLabel = Field("Surface", Changed({"surface"}));
            if (ImGui::BeginCombo(surfaceLabel.c_str(), body.surface.empty() ? "(none)" : body.surface.c_str()))
            {
                for (const std::string_view surface : Engine::Scene::kSurfaces)
                {
                    const std::string name(surface);
                    if (ImGui::Selectable(name.empty() ? "(none)" : name.c_str(), body.surface == name))
                    {
                        body.surface = name;
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("What it is made of, for what walks on it: \"play footstep\" on top of it plays the "
                                  "game's step_<surface> sounds (stone: step_stone_1, step_stone_2, ...).");
            }

            if (ImGui::TreeNode("Constraints"))
            {
                ImGui::Checkbox(Field("Lock Move X", Changed({"lock_move_x"})).c_str(), &body.lockMoveX);
                ImGui::Checkbox(Field("Lock Move Y", Changed({"lock_move_y"})).c_str(), &body.lockMoveY);
                ImGui::Checkbox(Field("Lock Move Z", Changed({"lock_move_z"})).c_str(), &body.lockMoveZ);
                ImGui::Checkbox(Field("Lock Turn X", Changed({"lock_turn_x"})).c_str(), &body.lockTurnX);
                ImGui::Checkbox(Field("Lock Turn Y", Changed({"lock_turn_y"})).c_str(), &body.lockTurnY);
                ImGui::Checkbox(Field("Lock Turn Z", Changed({"lock_turn_z"})).c_str(), &body.lockTurnZ);
                ImGui::DragInt(Field("Layers", Changed({"layers"})).c_str(), &body.layers, 1.0f, 0, 1 << 30);
                ImGui::TreePop();
            }
            ImGui::Checkbox(Field("Keep Upright", Changed({"keep_upright"})).c_str(), &body.keepUpright);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("It stays standing whatever hits it, like a player or something walking.");
            }

            // The collider. Picking a shape fits it to what the object draws; its sizes can be changed after.
            // A model (or mesh) file is fitted by the box around it, a built-in shape by its own
            const std::string drawnFile = !object->renderable                  ? std::string{}
                                          : !object->renderable->model.empty() ? object->renderable->model
                                                                               : object->renderable->mesh;
            const std::optional<Shape> drawn = object->renderable && drawnFile.empty()
                                                   ? std::optional<Shape>(object->renderable->shape)
                                                   : std::nullopt;
            const auto drawnBox = [&]() -> std::optional<Engine::Assets::BoundingBox>
            {
                if (drawn && object->renderable->shape == Shape::Cube)
                {
                    // A cube at its own size and centre inside the object (a stand-in for a model): that box
                    const Engine::Scene::RenderableDesc& r = *object->renderable;
                    const glm::vec3 center(r.shapeCenterX, r.shapeCenterY, r.shapeCenterZ);
                    const glm::vec3 half = 0.5f * glm::vec3(r.shapeSizeX, r.shapeSizeY, r.shapeSizeZ);
                    return Engine::Assets::BoundingBox{center - half, center + half};
                }
                const auto& bounds = drawnFile.empty() ? std::nullopt : ModelFactsOf(drawnFile).bounds;
                return bounds ? std::optional(Engine::Assets::Turned(*bounds, object->renderable->modelTurn))
                              : std::nullopt;
            };
            int shape = static_cast<int>(body.shape); // in ColliderShape's order
            if (ImGui::Combo(Field("Collider", Changed({"shape"})).c_str(), &shape,
                             "Auto\0Box\0Sphere\0Capsule\0Mesh\0Compound\0") &&
                shape != static_cast<int>(body.shape))
            {
                body.shape = static_cast<ColliderShape>(shape);
                Engine::Systems::PhysicsSystem::FitCollider(body, drawn, drawnBox());
            }
            if (body.shape == ColliderShape::Mesh)
            {
                ImGui::TextDisabled(drawnFile.empty()
                                        ? "Needs a model or mesh: until then it fits the object like Auto"
                                        : "The model's own triangles (the hull around them when dynamic)");
            }
            else if (body.shape == ColliderShape::Compound)
            {
                std::size_t remove = body.parts.size();
                for (std::size_t i = 0; i < body.parts.size(); ++i)
                {
                    Engine::Scene::ColliderPart& part = body.parts[i];
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::SeparatorText(std::format("Part {}", i + 1).c_str());
                    const bool changed = Changed({"parts"});
                    int partShape = static_cast<int>(part.shape) - 1; // no Auto
                    if (ImGui::Combo(Field("Shape", changed).c_str(), &partShape, "Box\0Sphere\0Capsule\0"))
                    {
                        part.shape = static_cast<ColliderShape>(partShape + 1);
                    }
                    float center[3] = {part.centerX, part.centerY, part.centerZ};
                    if (ImGui::DragFloat3(Field("Center", changed).c_str(), center, 0.01f))
                    {
                        part.centerX = center[0];
                        part.centerY = center[1];
                        part.centerZ = center[2];
                    }
                    if (part.shape == ColliderShape::Box)
                    {
                        float size[3] = {part.sizeX, part.sizeY, part.sizeZ};
                        if (ImGui::DragFloat3(Field("Size", changed).c_str(), size, 0.01f, 0.01f, 10000.0f))
                        {
                            part.sizeX = size[0];
                            part.sizeY = size[1];
                            part.sizeZ = size[2];
                        }
                    }
                    else
                    {
                        ImGui::DragFloat(Field("Radius", changed).c_str(), &part.radius, 0.01f, 0.01f, 10000.0f);
                    }
                    if (part.shape == ColliderShape::Capsule)
                    {
                        ImGui::DragFloat(Field("Height", changed).c_str(), &part.height, 0.01f, 0.01f, 10000.0f);
                    }
                    if (ImGui::SmallButton("Remove Part"))
                    {
                        remove = i;
                    }
                    ImGui::PopID();
                }
                if (remove < body.parts.size())
                {
                    body.parts.erase(body.parts.begin() + static_cast<std::ptrdiff_t>(remove));
                }
                if (ImGui::Button("Add Part"))
                {
                    body.parts.emplace_back();
                }
            }
            else if (body.shape == ColliderShape::Auto)
            {
                ImGui::TextDisabled(drawnFile.empty()
                                        ? "Fits the object: a sphere for a sphere, a box for anything else"
                                        : "Fits the object: a box around its model");
            }
            else
            {
                float center[3] = {body.centerX, body.centerY, body.centerZ};
                if (ImGui::DragFloat3(Field("Center", Changed({"center_x", "center_y", "center_z"})).c_str(), center,
                                      0.01f, 0.0f, 0.0f, "%.2f"))
                {
                    body.centerX = center[0];
                    body.centerY = center[1];
                    body.centerZ = center[2];
                }
                if (body.shape == ColliderShape::Box)
                {
                    float size[3] = {body.sizeX, body.sizeY, body.sizeZ};
                    if (ImGui::DragFloat3(Field("Size", Changed({"size_x", "size_y", "size_z"})).c_str(), size, 0.01f,
                                          0.01f, 10000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp))
                    {
                        body.sizeX = size[0];
                        body.sizeY = size[1];
                        body.sizeZ = size[2];
                    }
                }
                else
                {
                    ImGui::DragFloat(Field("Radius", Changed({"radius"})).c_str(), &body.radius, 0.01f, 0.01f, 10000.0f,
                                     "%.2f", ImGuiSliderFlags_AlwaysClamp);
                }
                if (body.shape == ColliderShape::Capsule)
                {
                    ImGui::DragFloat(Field("Height", Changed({"height"})).c_str(), &body.height, 0.01f, 0.01f, 10000.0f,
                                     "%.2f", ImGuiSliderFlags_AlwaysClamp);
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("From end to end, rounded ends included. Below twice the radius it is a "
                                          "sphere.");
                    }
                }
                FieldLabel("");
                if (ImGui::Button("Fit to Shape"))
                {
                    Engine::Systems::PhysicsSystem::FitCollider(body, drawn, drawnBox());
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Sets the centre and sizes to fit what the object draws");
                }
            }

            if (!object->transform)
            {
                ImGui::TextColored(kYellow, "Needs a Transform to be in the physics.");
            }
            else if (object->player && body.type == BodyType::Dynamic)
            {
                ImGui::TextColored(kYellow, "The physics moves a dynamic body, not the move keys: use Kinematic.");
            }
        };
        if (object->rigidBody)
        {
            ComponentSection(name("Rigid Body", "Physics"), changed("rigid_body"), object->rigidBody, drawRigidBody);
        }
        // Block logic is built in the Blocks window; here it is summed up
        const auto drawLogic = [&]
        {
            const Engine::Logic::LogicDesc& logic = *object->logic;
            ImGui::TextWrapped("%zu scripts, %zu blocks. Build them in the Blocks window; they run when you press Run.",
                               logic.scripts.size(), Engine::Logic::CountBlocks(logic));
            if (ImGui::Button("Edit Blocks", ImVec2(-FLT_MIN, 0.0f)))
            {
                showBlocks_ = true;
                focusBlocks_ = true;
            }
        };
        if (object->logic && !simple && !live) // the live view has its own Logic rows (what each script runs)
        {
            ComponentSection("Logic", changed("logic"), object->logic, drawLogic);
        }
        inspectedOverrides_ = nullptr;

        if (!object->transform && !object->renderable && !object->audioSource && !object->player && !object->camera &&
            !object->light && !object->rigidBody && !object->logic && !object->screen)
        {
            ImGui::TextDisabled(simple ? "No parts yet." : "No components.");
        }
        if (live)
        {
            return; // nothing is added in the live view
        }

        // Add Component: across the whole row, with a search like other editors'
        ImGui::Spacing();
        ImGui::BeginDisabled(playing_);
        if (simple)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::kLink);
            ImGui::PushStyleColor(ImGuiCol_Border, Theme::kBorderStrong);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        }
        const bool addClicked = ImGui::Button(simple ? "+ Add a part" : "Add Component", ImVec2(-FLT_MIN, 0.0f));
        if (simple)
        {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }
        if (addClicked)
        {
            addComponentSearch_.fill('\0');
            ImGui::OpenPopup("AddComponentPopup");
        }
        ImGui::SetNextWindowSizeConstraints(ImVec2(ImGui::GetItemRectSize().x, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##search", "Search", addComponentSearch_.data(), addComponentSearch_.size());
            ImGui::Separator();
            const std::string_view search(addComponentSearch_.data());
            const auto item = [&](const char* label, bool enabled)
            { return ContainsNoCase(label, search) && ImGui::MenuItem(label, nullptr, false, enabled); };

            const bool otherPlayer =
                std::any_of(scene_.Objects().begin(), scene_.Objects().end(),
                            [object](const GameObject& other) { return other.player && other.id != object->id; });
            if (item(name("Transform", "Position & Size"), !object->transform))
            {
                object->transform.emplace();
            }
            if (item(name("Renderable", "Looks"), !object->renderable))
            {
                object->renderable.emplace();
            }
            if (item(name("Audio Source", "Sound"), !object->audioSource))
            {
                object->audioSource.emplace();
            }
            if (item(name("Player", "Player Controls"), !object->player && !otherPlayer))
            {
                object->player.emplace();
            }
            if (otherPlayer && ContainsNoCase("Player", search) &&
                ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("Another game object is already the player");
            }
            if (item("Camera", !object->camera))
            {
                // The scene's first camera becomes the primary one
                const bool anyCamera = std::any_of(scene_.Objects().begin(), scene_.Objects().end(),
                                                   [](const GameObject& other) { return other.camera.has_value(); });
                object->camera = Engine::Scene::CameraComponent{.primary = !anyCamera};
            }
            if (item("Light", !object->light))
            {
                object->light.emplace();
            }
            if (item(name("Screen Element", "On Screen"), !object->screen))
            {
                object->screen.emplace();
            }
            if (item(name("Animator", "Animation"), !object->animator))
            {
                object->animator.emplace();
            }
            if (item("Sky", !object->sky))
            {
                object->sky.emplace();
            }
            if (item(name("Rigid Body", "Physics"), !object->rigidBody))
            {
                object->rigidBody.emplace();
            }
            // The Simple interface's Logic editor is always on screen: blocks dragged there add the logic
            if (!simple && item("Logic", !object->logic))
            {
                object->logic.emplace();
                showBlocks_ = true;
                focusBlocks_ = true;
            }
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
    }

    // The game's gravity, from configs/game.cfg. A change reaches the running game straight away and is saved when the
    // drag ends (or a preset is picked).
    void SceneEditor::DrawPhysicsSettings()
    {
        if (projectDir_.empty())
        {
            ImGui::TextDisabled("Open a game to set its physics.");
            return;
        }
        if (gravityProject_ != projectDir_)
        {
            const Engine::Game::GameSettings settings = Engine::Game::GameProject(projectDir_).Settings();
            gravity_ = {settings.gravityX, settings.gravityY, settings.gravityZ};
            gravityProject_ = projectDir_;
        }

        ImGui::TextWrapped(
            "Gravity pulls every dynamic Rigid Body in Play mode. A body's Gravity Scale multiplies it.");
        ImGui::Spacing();
        bool changed = ImGui::DragFloat3(Field("Gravity").c_str(), gravity_.data(), 0.05f, -100.0f, 100.0f, "%.2f",
                                         ImGuiSliderFlags_AlwaysClamp);
        bool save = ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Units per second squared, on the X, Y and Z axes. Down is -Y.");
        }

        FieldLabel("");
        for (const auto& [label, down] :
             {std::pair{"Earth", -9.81f}, std::pair{"Moon", -1.62f}, std::pair{"None", 0.0f}})
        {
            if (label != std::string_view("Earth"))
            {
                ImGui::SameLine();
            }
            if (ImGui::Button(label))
            {
                gravity_ = {0.0f, down, 0.0f};
                changed = true;
                save = true;
            }
        }

        if (changed && runtime_)
        {
            runtime_->Physics().SetGravity({gravity_[0], gravity_[1], gravity_[2]});
        }
        if (save)
        {
            try
            {
                Engine::Game::GameProject project(projectDir_);
                Engine::Game::GameSettings settings = project.Settings();
                settings.gravityX = gravity_[0];
                settings.gravityY = gravity_[1];
                settings.gravityZ = gravity_[2];
                project.SaveSettings(settings);
                SetStatus("Saved the gravity to configs/game.cfg");
            }
            catch (const std::exception& e)
            {
                SetStatus(std::string("Could not save the gravity: ") + e.what(), true);
            }
        }

        if (playing_ && runtime_)
        {
            const Engine::Physics::PhysicsWorld& physics = runtime_->Physics();
            ImGui::Spacing();
            ImGui::TextDisabled("Playing: %u bodies, %u touching, %llu steps", physics.BodyCount(),
                                static_cast<unsigned>(physics.ContactCount()),
                                static_cast<unsigned long long>(physics.StepCount()));
            DrawRecentHits();
            DrawRayTest();
            DrawDistanceTest();
            DrawGroundTest();
        }
    }

    // The last few times two objects hit each other in Play mode, newest first
    void SceneEditor::DrawRecentHits()
    {
        constexpr std::size_t kMaxHits = 8;
        const std::uint64_t steps = runtime_->Physics().StepCount();
        if (steps < hitsStep_)
        {
            recentHits_.clear(); // Play started again
        }
        hitsStep_ = steps;

        const auto nameOf = [this](Engine::Scene::Entity entity)
        {
            const GameObject* object = scene_.Find(runtime_->ObjectIdOf(entity));
            return object != nullptr ? object->name : std::string("a copy");
        };
        for (const Engine::Systems::Collision& collision : runtime_->Collisions())
        {
            if (collision.began)
            {
                recentHits_.insert(recentHits_.begin(), std::format("{} hit {} (speed {:.1f})", nameOf(collision.a),
                                                                    nameOf(collision.b), collision.impactSpeed));
            }
        }
        if (recentHits_.size() > kMaxHits)
        {
            recentHits_.resize(kMaxHits);
        }

        if (!recentHits_.empty())
        {
            ImGui::SeparatorText("Recent collisions");
            for (const std::string& hit : recentHits_)
            {
                ImGui::TextUnformatted(hit.c_str());
            }
        }
    }

    // A ray cast every frame from where the selected object is now (passing through its own collider), to see what it
    // hits. The Viewport draws it (SceneEditorColliders.cpp).
    void SceneEditor::DrawRayTest()
    {
        ImGui::SeparatorText("Ray test");
        ImGui::Checkbox("Cast a ray from the selected object", &rayTest_.on);
        if (!rayTest_.on)
        {
            return;
        }

        ImGui::DragFloat3(Field("Direction").c_str(), rayTest_.direction.data(), 0.02f, -1.0f, 1.0f, "%.2f",
                          ImGuiSliderFlags_AlwaysClamp);
        FieldLabel("");
        for (const auto& [label, axis] :
             {std::pair{"Down", glm::vec3(0.0f, -1.0f, 0.0f)}, std::pair{"+X", glm::vec3(1.0f, 0.0f, 0.0f)},
              std::pair{"-X", glm::vec3(-1.0f, 0.0f, 0.0f)}, std::pair{"+Z", glm::vec3(0.0f, 0.0f, 1.0f)},
              std::pair{"-Z", glm::vec3(0.0f, 0.0f, -1.0f)}})
        {
            if (label != std::string_view("Down"))
            {
                ImGui::SameLine();
            }
            if (ImGui::SmallButton(label))
            {
                rayTest_.direction = {axis.x, axis.y, axis.z};
            }
        }
        ImGui::DragFloat(Field("Length").c_str(), &rayTest_.length, 0.1f, 0.1f, 1000.0f, "%.1f",
                         ImGuiSliderFlags_AlwaysClamp);

        const GameObject* selected = scene_.Find(scene_.Selected());
        const Engine::Scene::Entity entity =
            selected != nullptr ? runtime_->EntityOf(selected->id) : Engine::Scene::Entity{};
        Engine::Scene::Scene* ecs = runtime_->Ecs();
        const auto transform = ecs != nullptr ? ecs->GetTransform(entity) : std::nullopt;
        const glm::vec3 direction(rayTest_.direction[0], rayTest_.direction[1], rayTest_.direction[2]);
        if (!transform)
        {
            ImGui::TextDisabled("Select an object to cast the ray from.");
            return;
        }
        if (glm::length(direction) <= 0.0f)
        {
            ImGui::TextDisabled("Give the ray a direction.");
            return;
        }

        rayTest_.from = {transform->x, transform->y, transform->z};
        rayTest_.hit = runtime_->Raycast(rayTest_.from, direction, rayTest_.length, entity);
        rayTest_.to = rayTest_.hit ? rayTest_.hit->point : rayTest_.from + glm::normalize(direction) * rayTest_.length;
        rayTest_.frame = ImGui::GetFrameCount();

        if (const auto& hit = rayTest_.hit)
        {
            const GameObject* object = scene_.Find(runtime_->ObjectIdOf(hit->entity));
            ImGui::TextWrapped("Hits %s %.2f units away, at (%.2f, %.2f, %.2f), its surface facing (%.2f, %.2f, %.2f)",
                               object != nullptr ? object->name.c_str() : "a copy", hit->distance, hit->point.x,
                               hit->point.y, hit->point.z, hit->normal.x, hit->normal.y, hit->normal.z);
        }
        else
        {
            ImGui::TextWrapped("Hits nothing within %.1f units", rayTest_.length);
        }
    }

    // How far the selected object is from another one, collider to collider, and which objects are within range of
    // where it is. The Viewport draws the gap and the range (SceneEditorColliders.cpp).
    void SceneEditor::DrawDistanceTest()
    {
        ImGui::SeparatorText("Distance check");
        ImGui::Checkbox("Measure from the selected object", &distanceTest_.on);
        if (!distanceTest_.on)
        {
            return;
        }

        const GameObject* selected = scene_.Find(scene_.Selected());
        const GameObject* target = scene_.Find(distanceTest_.target);
        if (ImGui::BeginCombo(Field("To").c_str(), target != nullptr ? target->name.c_str() : "(pick an object)"))
        {
            for (const GameObject& object : scene_.Objects())
            {
                if (!object.rigidBody || (selected != nullptr && object.id == selected->id))
                {
                    continue; // only objects with a body can be measured to
                }
                ImGui::PushID(static_cast<int>(object.id));
                if (ImGui::Selectable(object.name.c_str(), object.id == distanceTest_.target))
                {
                    distanceTest_.target = object.id;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::DragFloat(Field("Range").c_str(), &distanceTest_.range, 0.1f, 0.1f, 1000.0f, "%.1f",
                         ImGuiSliderFlags_AlwaysClamp);

        const Engine::Scene::Entity entity =
            selected != nullptr ? runtime_->EntityOf(selected->id) : Engine::Scene::Entity{};
        Engine::Scene::Scene* ecs = runtime_->Ecs();
        const auto transform = ecs != nullptr ? ecs->GetTransform(entity) : std::nullopt;
        if (!transform)
        {
            ImGui::TextDisabled("Select an object to measure from.");
            return;
        }
        distanceTest_.from = {transform->x, transform->y, transform->z};
        distanceTest_.frame = ImGui::GetFrameCount();

        const Engine::Scene::Entity targetEntity =
            target != nullptr ? runtime_->EntityOf(target->id) : Engine::Scene::Entity{};
        distanceTest_.gap = runtime_->DistanceBetween(entity, targetEntity);
        if (const auto& gap = distanceTest_.gap)
        {
            const auto there = ecs->GetTransform(targetEntity).value_or(*transform);
            const float centres = glm::length(glm::vec3(there.x, there.y, there.z) - distanceTest_.from);
            ImGui::Text("Gap to %s: %.2f units", target->name.c_str(), gap->distance);
            ImGui::TextDisabled("Their centres are %.2f apart", centres);
        }
        else if (target != nullptr)
        {
            ImGui::TextWrapped("No gap to %s: it has no body, or is more than %.0f units away", target->name.c_str(),
                               Engine::Physics::DefaultMaxGap);
        }

        const std::vector<Engine::Systems::NearbyEntity> nearby =
            runtime_->EntitiesWithin(distanceTest_.from, distanceTest_.range, entity);
        if (nearby.empty())
        {
            ImGui::TextWrapped("Nothing within %.1f units", distanceTest_.range);
            return;
        }
        ImGui::Text("Within %.1f units:", distanceTest_.range);
        for (const Engine::Systems::NearbyEntity& found : nearby)
        {
            const GameObject* object = scene_.Find(runtime_->ObjectIdOf(found.entity));
            ImGui::BulletText("%s, %.2f away", object != nullptr ? object->name.c_str() : "a copy", found.distance);
        }
    }

    // Whether the selected object stands on the ground, and on what. Jump throws it up to see the answer change. The
    // Viewport marks where it stands (SceneEditorColliders.cpp).
    void SceneEditor::DrawGroundTest()
    {
        constexpr float kJumpSpeed = 5.0f; // units per second, added upwards

        ImGui::SeparatorText("Ground check");
        ImGui::Checkbox("Check if the selected object is on the ground", &groundTest_.on);
        if (!groundTest_.on)
        {
            return;
        }

        const GameObject* selected = scene_.Find(scene_.Selected());
        const Engine::Scene::Entity entity =
            selected != nullptr ? runtime_->EntityOf(selected->id) : Engine::Scene::Entity{};
        Engine::Scene::Scene* ecs = runtime_->Ecs();
        if (ecs == nullptr || !ecs->HasComponent<Engine::Scene::PhysicsBodyComponent>(entity))
        {
            ImGui::TextDisabled("Select an object with a Rigid Body.");
            return;
        }
        groundTest_.ground = runtime_->GroundUnder(entity);
        groundTest_.frame = ImGui::GetFrameCount();

        if (const auto& ground = groundTest_.ground)
        {
            const GameObject* under = scene_.Find(runtime_->ObjectIdOf(ground->entity));
            ImGui::TextWrapped("On the ground: %s, its surface facing (%.2f, %.2f, %.2f)",
                               under != nullptr ? under->name.c_str() : "a copy", ground->normal.x, ground->normal.y,
                               ground->normal.z);
        }
        else
        {
            ImGui::TextUnformatted("In the air");
        }

        // Up is against gravity, as the ground check sees it
        const glm::vec3 gravity = runtime_->Physics().Gravity();
        const glm::vec3 up = glm::length(gravity) > 0.0f ? -glm::normalize(gravity) : glm::vec3(0.0f, 1.0f, 0.0f);
        if (ImGui::SmallButton("Jump"))
        {
            const glm::vec3 velocity =
                Engine::Systems::PhysicsSystem::VelocityOf(*ecs, runtime_->Physics(), entity).value_or(glm::vec3(0.0f));
            Engine::Systems::PhysicsSystem::SetVelocity(*ecs, runtime_->Physics(), entity, velocity + up * kJumpSpeed);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Throws the object up (a dynamic Rigid Body only)");
        }
    }

    bool SceneEditor::Changed(std::initializer_list<std::string_view> fields)
    {
        if (liveFields_)
        {
            nextFields_.assign(fields.begin(), fields.end()); // the field about to be drawn (see FieldLabel)
        }
        if (!inspectedOverrides_)
        {
            return false;
        }
        return std::any_of(fields.begin(), fields.end(),
                           [this](std::string_view field)
                           {
                               const std::string property = inspectedComponent_ + "/" + std::string(field);
                               return std::find(inspectedOverrides_->begin(), inspectedOverrides_->end(), property) !=
                                      inspectedOverrides_->end();
                           });
    }

    // Top of the Inspector for part of a prefab instance, one row like Unity's: the prefab, Open, and an Overrides
    // menu with what this object changed (each with Apply, into the prefab for every instance, and Revert, back to the
    // prefab's value); "..." has the rest
    std::function<void()> SceneEditor::DrawPrefabInstance(const GameObject& object)
    {
        if (!object.prefabInstance)
        {
            return {};
        }
        const Engine::Scene::PrefabInstance& link = *object.prefabInstance;
        const std::uint32_t id = object.id;
        const std::uint32_t root = Engine::Scene::InstanceRootOf(scene_.Description(), id);
        const bool readable = prefabs_.PrefabObjectOf(object) != nullptr;
        std::function<void()> action;

        const ImGuiStyle& style = ImGui::GetStyle();
        const auto buttonWidth = [&style](const char* text)
        { return ImGui::CalcTextSize(text, nullptr, true).x + style.FramePadding.x * 2.0f; };
        const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
        const std::string overridesLabel =
            link.overrides.empty() ? std::string("No Overrides") : std::format("Overrides ({})", link.overrides.size());

        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(kPrefabBlue, "Prefab: %s", std::filesystem::path(link.prefab).stem().string().c_str());
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("%s%s", link.prefab.c_str(),
                              root != id ? std::format("\npart of {}", scene_.Find(root)->name).c_str() : "");
        }

        // The buttons sit at the right end of the row
        const float buttons =
            buttonWidth("Open") + buttonWidth(overridesLabel.c_str()) + buttonWidth("...") + style.ItemSpacing.x * 2.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), right - buttons));
        if (ImGui::Button("Open"))
        {
            const std::string path = link.prefab;
            action = [this, path] { OpenPrefab(path); };
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("Edit the prefab on its own (Prefab Mode); every instance takes the changes");
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(link.overrides.empty() || !readable);
        if (ImGui::Button(overridesLabel.c_str()))
        {
            ImGui::OpenPopup("##Overrides");
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip(link.overrides.empty() ? "Same as the prefab (apart from its name and place)"
                                                     : "What this instance changed from the prefab");
        }
        ImGui::SameLine();
        if (ImGui::Button("..."))
        {
            ImGui::OpenPopup("##PrefabMenu");
        }

        // In Prefab Mode the root's instance is the variant's base; any other is a nested prefab
        const char* target = InPrefabMode() && root == Engine::Scene::kPrefabRootId ? "base" : "prefab";
        if (ImGui::BeginPopup("##Overrides"))
        {
            ImGui::TextDisabled("Changed on this instance");
            ImGui::Separator();
            std::string revert;
            if (ImGui::BeginTable("##list", 2, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupColumn("property", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("actions");
                for (const std::string& property : link.overrides)
                {
                    ImGui::PushID(property.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(PropertyLabel(property).c_str());
                    ImGui::TableNextColumn();
                    if (ImGui::SmallButton("Revert"))
                    {
                        revert = property;
                    }
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                    {
                        ImGui::SetTooltip("Back to the %s's value", target);
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Apply"))
                    {
                        action = [this, id, property] { ApplyPrefabProperty(id, property); };
                        ImGui::CloseCurrentPopup();
                    }
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                    {
                        ImGui::SetTooltip("Make this the %s's value: every instance that did not change it takes it",
                                          target);
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::Separator();
            if (ImGui::Button("Revert All"))
            {
                revert = "*";
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Apply All"))
            {
                action = [this, id] { ApplyPrefab(id); };
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("Save every change into the %s: every other instance takes them, except what it "
                                  "changed itself",
                                  target);
            }
            ImGui::EndPopup();
            if (!revert.empty())
            {
                prefabs_.Revert(scene_.Description(), id, revert == "*" ? std::string{} : revert);
            }
        }

        if (ImGui::BeginPopup("##PrefabMenu"))
        {
            if (root != id && ImGui::MenuItem("Select Prefab Root"))
            {
                scene_.Select(root);
            }
            if (ImGui::MenuItem("Unpack Prefab"))
            {
                action = [this, id]
                {
                    Engine::Game::PrefabManager::Unpack(scene_.Description(), id);
                    SetStatus("Unpacked: the objects are no longer linked to the prefab");
                };
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("Make the instance ordinary objects again (they keep everything they have)");
            }
            ImGui::EndPopup();
        }

        if (!readable)
        {
            ImGui::TextColored(kYellow, "The prefab file is missing or unreadable (see Console): this object keeps "
                                        "its values but is not updated from it.");
        }
        ImGui::Separator();
        return action;
    }

    // Diagnostics > ECS: entity counts, FPS, and the "spawn 1,000 copies" button for the ECS demo
    void SceneEditor::DrawEcsStress()
    {
        if (!ImGui::CollapsingHeader("ECS", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        if (!runtime_ || !runtime_->IsRunning())
        {
            ImGui::TextDisabled("Select a game to spawn entities.");
            ImGui::Separator();
            return;
        }

        const float fps = ImGui::GetIO().Framerate;
        const std::size_t spawnedCount = runtime_->SpawnedEntityCount();

        ImGui::Text("Frame rate: %.1f FPS (%.2f ms)", fps, fps > 0.0f ? 1000.0f / fps : 0.0f);

        ImGui::Text("Entities: %zu live (%zu spawned)", runtime_->EntityCount(), spawnedCount);

        if (spawnedCount >= 1000 && playing_ && !paused_)
        {
            if (fps >= 60.0f)
            {
                ImGui::TextColored(kGreen, "Current average meets 60 FPS");
            }
            else
            {
                ImGui::TextColored(kYellow, "Current average is below 60 FPS");
            }
        }
        else
        {
            ImGui::TextDisabled("Run the game and spawn 1,000 entities to check performance.");
        }

        // Copies of the selected object: the same mesh, texture, tint and spin on 1,000 new entities
        const GameObject* selected = scene_.Find(scene_.Selected());
        const bool canCopy = selected && selected->transform && selected->renderable;
        const std::string spawnLabel =
            canCopy ? "Spawn 1,000 copies of " + selected->name : std::string("Spawn 1,000 copies");
        ImGui::BeginDisabled(!canCopy);
        if (ImGui::Button(spawnLabel.c_str()))
        {
            try
            {
                const std::size_t spawned = runtime_->SpawnEntities(1000, selected->id);

                SetStatus("Spawned " + std::to_string(spawned) + " entities (" +
                          std::to_string(runtime_->EntityCount()) + " live)");
            }
            catch (const std::exception& e)
            {
                SetStatus(std::string("Spawn failed: ") + e.what(), true);
            }
        }
        ImGui::EndDisabled();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip(canCopy ? "1,000 new entities, each with a Transform, the selected object's "
                                        "MeshRenderer and its Spin,\nin a 10 x 10 block above it. Press again for "
                                        "another layer. Not saved or shown in the Hierarchy."
                                      : "Select an object with a Transform and a Renderable to copy");
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(spawnedCount == 0);

        if (ImGui::Button("Clear spawned"))
        {
            const std::size_t destroyed = runtime_->ClearSpawnedEntities();

            SetStatus("Destroyed " + std::to_string(destroyed) + " spawned entities");
        }

        ImGui::EndDisabled();
        ImGui::Separator();
    }

    // The prefab list, then the game's models, textures and sounds (the asset browser)
    void SceneEditor::DrawAssets()
    {
        PrepareAssetBrowser();
        assetBrowser_.SetPrefabsView([this] { DrawPrefabs(); });
        assetBrowser_.DrawContents();
    }

    void SceneEditor::PrepareAssetBrowser()
    {
        // The selected object (what a click uses an asset on) and the scene's loaded assets
        std::optional<Panels::AssetBrowserPanel::Target> target;
        if (const GameObject* selected = scene_.Find(scene_.Selected()))
        {
            target.emplace();
            target->objectName = selected->name;
            if (selected->renderable)
            {
                target->mesh = selected->renderable->mesh;
                target->model = selected->renderable->model;
                target->texture = selected->renderable->texture;
                target->material = selected->renderable->material;
            }
            if (selected->audioSource)
            {
                target->clip = selected->audioSource->clip;
            }
        }
        assetBrowser_.SetProject(projectDir_);
        assetBrowser_.SetTarget(std::move(target));
        assetBrowser_.SetSceneResources(runtime_ ? &runtime_->Resources() : nullptr);
    }

    // ---------------------------------------------------------------- Scenes

    std::filesystem::path SceneEditor::StartScenePath() const
    {
        if (projectDir_.empty())
        {
            return {};
        }
        return projectDir_ / Engine::Game::GameProject(projectDir_).Settings().startScene;
    }

    // Paths typed in the popup are relative to the game folder unless they're already absolute
    std::filesystem::path SceneEditor::ResolvePath(const std::string& path) const
    {
        const std::filesystem::path value = path;
        return value.is_absolute() || projectDir_.empty() ? value : projectDir_ / value;
    }

    // Compares the scene with what was last saved (the * next to EDIT MODE, and Build saves first if true)
    bool SceneEditor::HasUnsavedChanges() const
    {
        return scene_.CaptureState() != saved_;
    }

    // Opens the selected game's start scene (an empty scene if it is missing or unreadable; why is in the Console)
    void SceneEditor::LoadProjectScene()
    {
        const auto path = StartScenePath();
        if (LoadSceneFile(path))
        {
            return;
        }
        scene_.Clear();
        breakpoints_.clear();
        scenePath_ = path; // Save writes the start scene, replacing the unreadable file
        ResetHistory();
        saved_ = scene_.CaptureState();
    }

    // Opens a scene file into the editor. On failure the current scene stays as it was
    bool SceneEditor::LoadSceneFile(const std::filesystem::path& path)
    {
        if (InPrefabMode())
        {
            SetStatus("Close the prefab (< Scene in the Hierarchy) before opening a scene", true);
            return false;
        }
        Engine::Scene::SceneDescription scene;
        const auto report = Engine::Scene::LoadSceneFile(path, scene); // problems are logged
        if (!report.ok)
        {
            SetStatus("Could not open " + path.filename().string() + ": " + report.error, true);
            return false;
        }
        prefabs_.RelinkMoved(scene);
        scene_.SetDescription(std::move(scene));
        scenePath_ = path;
        // Kept by object and block number, which another scene may share: each scene has its own file
        breakpointRules_.clear();
        breakpoints_ = Engine::Logic::LoadBreakpoints(BreakpointFilePath(), &breakpointRules_);
        saved_ = scene_.CaptureState(); // as the file has it: an update below shows as unsaved changes
        // A prefab may have changed since the scene was saved: its instances take what they did not change themselves
        const bool updated = prefabs_.UpdateInstances(scene_.Description(), scene_.NextId());
        prefabs_.RefreshOverrides(scene_.Description());
        ResetHistory();
        // Simple names the game rather than its scene file, and has no Console to point at
        const bool simple = interface_ == InterfaceMode::Simple;
        const std::string opened =
            simple && path == StartScenePath() ? projectDir_.filename().string() : path.filename().string();
        std::string message =
            report.warnings.empty() ? "Opened " + opened
            : simple                ? std::format("Opened {}, but some of it could not be read", opened)
                     : std::format("Opened {} ({} problems - see Console)", opened, report.warnings.size());
        if (updated)
        {
            message += " - prefab instances updated from their prefabs";
        }
        SetStatus(message, !report.warnings.empty());
        return true;
    }

    // Writes the scene and remembers it as "saved" (so the * goes away)
    bool SceneEditor::SaveSceneFile(const std::filesystem::path& path)
    {
        try
        {
            Engine::Scene::SaveSceneFile(path, scene_.Description());
            scenePath_ = path;
            saved_ = scene_.CaptureState();
            SaveThumbnail();
            SetStatus("Saved " + path.filename().string());
            return true;
        }
        catch (const std::exception& e)
        {
            SetStatus(std::string("Save failed: ") + e.what(), true);
            return false;
        }
    }

    // Empty, unsaved scene for the current game
    void SceneEditor::NewScene()
    {
        if (InPrefabMode())
        {
            SetStatus("Close the prefab (< Scene in the Hierarchy) before making a new scene", true);
            return;
        }
        scene_.Clear();
        breakpoints_.clear();
        scenePath_.clear();
        ResetHistory();
        saved_ = scene_.CaptureState();
        SetStatus("New scene");
    }

    // Ctrl+O just opens the path popup, the actual loading happens when you press Open
    void SceneEditor::OpenScene()
    {
        if (InPrefabMode())
        {
            SetStatus("Close the prefab (< Scene in the Hierarchy) before opening a scene", true);
            return;
        }
        saveAsMode_ = false;
        pathBuffer_ = scenePath_.empty() ? StartScenePath().string() : scenePath_.string();
        showPathPopup_ = true;
    }

    // Ctrl+S: save to the current file, or ask for a path if the scene was never saved (the prefab, in Prefab Mode)
    void SceneEditor::SaveScene()
    {
        if (InPrefabMode())
        {
            SavePrefabMode();
            return;
        }
        if (scenePath_.empty())
        {
            SaveSceneAs();
            return;
        }
        SaveSceneFile(scenePath_);
    }

    // Always asks for a path first
    void SceneEditor::SaveSceneAs()
    {
        if (InPrefabMode())
        {
            SavePrefabMode(); // a prefab is saved in place
            return;
        }
        saveAsMode_ = true;
        pathBuffer_ = scenePath_.empty() ? StartScenePath().string() : scenePath_.string();
        showPathPopup_ = true;
    }

    // ---------------------------------------------------------------- Play

    // Saves, then starts a fresh GameRuntime for Play mode. Making a new one each time means Stop always
    // goes back to exactly the scene you had before pressing Play.
    void SceneEditor::StartPlay()
    {
        if (playing_ || projectLoad_)
        {
            return;
        }
        if (projectDir_.empty())
        {
            SetStatus("Open a game, or make one on the Home page, before running it", true);
            return;
        }
        if (InPrefabMode())
        {
            SetStatus("Go back to the scene (the arrow at the top left) before running it", true);
            return;
        }
        if (!scenePath_.empty())
        {
            SaveScene();
        }

        try
        {
            // The runtime showing the scene already has its files read and on the GPU: Play restarts that one, so
            // pressing Play does not load everything again
            std::unique_ptr<Engine::Game::GameRuntime> runtime;
            if (runtime_ && runtimeProject_ == projectDir_)
            {
                runtime = std::move(runtime_);
            }
            else
            {
                runtime = std::make_unique<Engine::Game::GameRuntime>();
            }
            runtimeProject_.clear();
            runtime->LoadProject(Engine::Game::GameProject(projectDir_)); // the game's settings, as saved now
            runtime->SetSimulating(true);
            runtime->SetEventBus(events_);

            // Start re-reads input_actions.json, so remapped keys apply now
            if (!runtime->Start(scene_.Description()))
            {
                // The Console, with the reason, is an Advanced window
                SetStatus(interface_ == InterfaceMode::Advanced ? "The game could not start (see Console)."
                                                                : "The game could not start.",
                          true);
                return;
            }
            runtime_ = std::move(runtime);
            runtimeProject_ = projectDir_;
            runtimeScene_ = scene_.Description();
            // The breakpoints stop this game too; those whose block or object is gone are dropped. A reused runtime
            // still holds the ones removed while stopped, so it starts from none
            runtime_->Debugger().ClearBreakpoints();
            std::erase_if(breakpoints_,
                          [this](auto& entry)
                          {
                              const GameObject* object = scene_.Find(entry.first);
                              std::erase_if(entry.second,
                                            [object](std::uint32_t id) {
                                                return !object || !object->logic ||
                                                       !Engine::Logic::FindBlock(*object->logic, id);
                                            });
                              return entry.second.empty();
                          });
            for (const auto& [objectId, blocks] : breakpoints_)
            {
                runtime_->Debugger().SetBreakpoints(objectId, blocks);
            }
            for (const auto& [place, rule] : breakpointRules_)
            {
                runtime_->Debugger().SetBreakpointRule(place.first, place.second, rule);
            }
            shownPause_ = 0;
            playing_ = true;
            consoleSessions_.insert(runtime_->Blocks().DiagnosticSession());
            paused_ = false;
            view_ = EditorView::Workspace;
            showViewport_ = true;

            const auto& objects = runtimeScene_.objects;
            const bool hasMainCamera =
                std::any_of(objects.begin(), objects.end(), [](const GameObject& object)
                            { return object.camera && object.camera->primary && object.transform; });
            if (runtime_->HasPlayer())
            {
                SetStatus("Running - WASD moves the player, hold right mouse to look around");
            }
            else if (hasMainCamera)
            {
                SetStatus("Running - seen through the Main camera");
            }
            else
            {
                SetStatus("Running - this scene has no Main camera or player, so the view stays where the editor "
                          "left it");
            }
        }
        catch (const std::exception& e)
        {
            SetStatus(std::string("The game could not start: ") + e.what(), true);
        }
    }

    // Throw away the Play mode runtime; SyncRuntime makes a frozen edit-mode one again next frame
    void SceneEditor::StopPlay()
    {
        if (!playing_)
        {
            return;
        }
        // Back to the scene as it is edited, in the same runtime: what the game read stays loaded
        if (runtime_)
        {
            runtime_->SetSimulating(false);
            if (runtime_->Start(scene_.Description()))
            {
                runtimeScene_ = scene_.Description();
            }
            else
            {
                runtime_.reset();
                runtimeProject_.clear(); // SyncRuntime makes a new one
            }
        }
        playing_ = false;
        paused_ = false;
        SetStatus("Stopped");
    }

    // Pause stops the scripts before their next block, and the world with them (it still draws); again: Resume
    void SceneEditor::TogglePause()
    {
        if (!playing_ || !runtime_)
        {
            return;
        }
        Engine::Logic::LogicRunner& debugger = runtime_->Debugger();
        if (paused_)
        {
            debugger.Resume();
            SetStatus("Playing");
        }
        else
        {
            debugger.Pause(scene_.Selected());
        }
        paused_ = debugger.Debug().mode != Engine::Logic::RunMode::Running;
    }

    void SceneEditor::Step()
    {
        if (playing_ && runtime_ && runtime_->Blocks().IsPaused())
        {
            runtime_->Debugger().StepOnce();
        }
    }

    void SceneEditor::StepOver()
    {
        if (playing_ && runtime_ && runtime_->Blocks().IsPaused())
        {
            runtime_->Debugger().StepOver();
        }
    }

    std::filesystem::path SceneEditor::BreakpointFilePath() const
    {
        return projectDir_.empty() || scenePath_.empty()
                   ? std::filesystem::path{}
                   : projectDir_ / "data" / "breakpoints" / (scenePath_.stem().string() + ".txt");
    }

    void SceneEditor::SaveBreakpointFile() const
    {
        if (const auto path = BreakpointFilePath(); !path.empty())
        {
            Engine::Logic::SaveBreakpoints(path, breakpoints_, breakpointRules_);
        }
    }

    void SceneEditor::ToggleBreakpoint(std::uint32_t objectId, std::uint32_t blockId)
    {
        std::set<std::uint32_t>& blocks = breakpoints_[objectId];
        if (blocks.erase(blockId) == 0)
        {
            blocks.insert(blockId);
        }
        if (playing_ && runtime_)
        {
            runtime_->Debugger().SetBreakpoints(objectId, blocks);
        }
        if (blocks.empty())
        {
            breakpoints_.erase(objectId);
        }
        if (!breakpoints_.contains(objectId) || !breakpoints_.at(objectId).contains(blockId))
        {
            breakpointRules_.erase({objectId, blockId});
        }
        SaveBreakpointFile();
    }

    void SceneEditor::SetBreakpointRule(std::uint32_t objectId, std::uint32_t blockId,
                                        Engine::Logic::BreakpointRule rule)
    {
        if (playing_ && runtime_)
        {
            runtime_->Debugger().SetBreakpointRule(objectId, blockId, rule);
        }
        breakpointRules_[{objectId, blockId}] = std::move(rule);
        SaveBreakpointFile();
    }

    std::set<std::uint32_t> SceneEditor::BreakpointsOf(std::uint32_t objectId) const
    {
        const auto found = breakpoints_.find(objectId);
        return found != breakpoints_.end() ? found->second : std::set<std::uint32_t>{};
    }

    void SceneEditor::SyncDebugger()
    {
        if (!playing_ || !runtime_)
        {
            return;
        }
        using Engine::Logic::PauseReason;
        const Engine::Logic::DebugState& debug = runtime_->Blocks().Debug();
        paused_ = debug.mode != Engine::Logic::RunMode::Running;
        if (debug.mode != Engine::Logic::RunMode::Paused || debug.pauses == shownPause_)
        {
            return;
        }
        shownPause_ = debug.pauses;

        const GameObject* object = scene_.Find(debug.pausedObject);
        const Engine::Logic::Block* block =
            object && object->logic ? Engine::Logic::FindBlock(*object->logic, debug.pausedBlock) : nullptr;
        if (object && debug.reason != PauseReason::User)
        {
            // Show where it stopped: the object, its Blocks window, the block
            scene_.Select(debug.pausedObject);
            showBlocks_ = true;
            focusBlocks_ = debug.reason == PauseReason::Breakpoint || debug.reason == PauseReason::InfiniteLoop;
            if (debug.reason == PauseReason::Breakpoint)
            {
                showInspector_ = true; // the values as the breakpoint stopped them
                focusInspector_ = true;
            }
            if (block)
            {
                blocks_->Reveal(block->id, true); // the canvas moves only if the block is out of view
            }
        }
        const std::string where = block    ? "'" + Engine::Logic::Describe(*block) + "' on " + object->name
                                  : object ? object->name
                                           : std::string();
        switch (debug.reason)
        {
        case PauseReason::Breakpoint:
            SetStatus("Paused at a breakpoint: " + where + ". Step (F8) runs one block, Run goes on.");
            break;
        case PauseReason::Step:
            SetStatus(block ? "Stepped. Next: " + where : "Stepped. The script of " + where + " has ended.");
            break;
        case PauseReason::StepLast:
            SetStatus("Stepped. That was the last block of " + where + ". Step (F8) again to end the script.");
            break;
        case PauseReason::Focus: SetStatus("Step (F8) runs this script. Next: " + where); break;
        case PauseReason::InfiniteLoop:
            showConsole_ = true;
            SetStatus("Possible infinite loop detected", true);
            break;
        default: SetStatus("Paused. Step (F8) runs one block, Run goes on."); break;
        }
    }

    void SceneEditor::OpenBlock(std::uint32_t objectId, std::uint32_t blockId)
    {
        scene_.Select(objectId);
        showBlocks_ = true;
        focusBlocks_ = true;
        if (blockId != 0)
        {
            blocks_->Reveal(blockId);
        }
    }

    void SceneEditor::DrawLiveValues()
    {
        variables_->SetScale(uiScale_);
        variables_->SetTarget(&scene_.Description(), scene_.Selected(), playing_ ? runtime_.get() : nullptr);
        variables_->DrawContents();
    }

    // The Inspector's values as the game runs, in a window of its own that floats over the workspace, never docked: the
    // Simple interface's dockspace only keeps windows alive, so a docked one would vanish there. It starts at the top
    // right of the Viewport; imgui.ini remembers where it is moved to. Off until the eye button or the Window menu
    // asks for it: while the game runs the Inspector shows the same values.
    void SceneEditor::DrawVariableInspector()
    {
        // Opened or closed (eye button, Window menu, its x, a breakpoint): imgui.ini remembers it
        if (showVariables_ != savedShowVariables_)
        {
            savedShowVariables_ = showVariables_;
            ImGui::MarkIniSettingsDirty();
        }
        // Advanced docks it with the other windows (DrawWorkspace); Simple has no dock to put it in
        if (!showVariables_ || view_ != EditorView::Workspace || interface_ == InterfaceMode::Advanced)
        {
            return;
        }
        const float s = uiScale_;
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const ImVec2 corner = viewportWidth_ > 0.0f
                                  ? ImVec2(viewportX_ + viewportWidth_ - 12.0f * s, viewportY_ + 12.0f * s)
                                  : ImVec2(display.x - 12.0f * s, topBarBottom_ + 12.0f * s);
        ImGui::SetNextWindowPos(corner, ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(340.0f * s, 380.0f * s), ImGuiCond_FirstUseEver);
        const GameObject* object = scene_.Find(scene_.Selected());
        const std::string title = std::string("Variable Inspector") + (object ? " - " + object->name : std::string()) +
                                  "###VariableInspector";
        // Not focused when it appears, so the window being worked in (and its keys) stays in front
        if (ImGui::Begin(title.c_str(), &showVariables_,
                         ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing))
        {
            DrawLiveValues();
        }
        ImGui::End();
    }

    // ---------------------------------------------------------------- Projects

    // Re-scan the projects folder (Ctrl+R), e.g. after copying a game in by hand
    void SceneEditor::RefreshProjects()
    {
        projects_ = Engine::Game::GameProject::FindAll(Engine::Game::GameProject::ProjectsRoot());
        FreeThumbnails();              // pictures changed outside the editor are read again
        assetBrowser_.RequestRescan(); // files added to the game outside the editor show up too
        if (std::find(projects_.begin(), projects_.end(), projectDir_) == projects_.end())
        {
            OpenProject(FirstOwnGame());
        }
    }

    // Switch to another game and open its start scene
    void SceneEditor::OpenProject(const std::filesystem::path& dir)
    {
        if (dir == projectDir_ && !dir.empty())
        {
            return;
        }
        if (InPrefabMode())
        {
            // Unsaved prefab changes are only dropped when the game itself is gone (e.g. Refresh after deleting it)
            std::error_code ec;
            if (HasUnsavedChanges() && std::filesystem::exists(projectDir_, ec))
            {
                SetStatus("Save or close the open prefab before switching games", true);
                return;
            }
            LeavePrefabMode();
        }
        // Unsaved scene changes: ask first (unless the game itself is gone, e.g. Refresh after deleting it)
        if (std::error_code ec; HasUnsavedChanges() && std::filesystem::exists(projectDir_, ec))
        {
            AskToSave([this, dir] { OpenProject(dir); });
            return;
        }
        StopPlay();
        SaveModes();
        projectDir_ = dir;
        prefabs_.SetProjectDir(dir);
        prefabList_ = prefabs_.List();
        LoadModes();
        runtime_.reset();
        runtimeProject_.clear();
        editorCamera_.Detach(); // a different game: start from its camera, not the last game's view
        if (projectDir_.empty())
        {
            NewScene();
            return;
        }
        // Games made before models had folders of their own (assets/textures, models loose in assets/models) are moved
        // to the current layout once, and their scenes and prefabs updated to match
        if (Engine::Game::NeedsLayoutMigration(projectDir_))
        {
            const Engine::Game::LayoutMigration migration = Engine::Game::MigrateAssetLayout(projectDir_);
            for (const std::string& warning : migration.warnings)
            {
                LOG_WARN(Editor, "Asset layout: {}", warning);
            }
            LOG_INFO(Editor, "Moved {} asset files to the current folder layout ({} files updated)",
                     migration.moved.size(), migration.rewritten);
            prefabList_ = prefabs_.List();
        }
        LoadProjectScene();
        // Its runtime is filled behind the loading screen rather than all at once in one long frozen frame
        projectLoad_.emplace(ProjectLoad{});
        projectLoad_->runtime = std::make_unique<Engine::Game::GameRuntime>();
        projectLoad_->runtime->SetSimulating(false);
        projectLoad_->runtime->LoadProject(Engine::Game::GameProject(projectDir_));
    }

    void SceneEditor::StepProjectLoad()
    {
        if (!projectLoad_)
        {
            return;
        }
        ProjectLoad& load = *projectLoad_;
        ++load.frames;
        if (load.started)
        {
            // The first frame drawn sends the models and pictures to the GPU: still behind the screen
            if (load.frames > 2)
            {
                projectLoad_.reset();
            }
            return;
        }
        if (load.frames == 1)
        {
            return; // the screen is drawn once before the reading starts
        }
        try
        {
            // As many objects as fit in a short slice of the frame, so the bar keeps moving
            const auto& objects = scene_.Description().objects;
            const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(30);
            while (load.next < objects.size() && std::chrono::steady_clock::now() < until)
            {
                load.runtime->Preload(objects[load.next++]);
            }
            if (load.next < objects.size())
            {
                return;
            }
            if (load.runtime->Start(scene_.Description()))
            {
                runtime_ = std::move(load.runtime);
                runtimeProject_ = projectDir_;
                runtimeScene_ = scene_.Description();
            }
            load.started = true;
            load.frames = 0;
        }
        catch (const std::exception& e)
        {
            SetStatus("Could not show " + projectDir_.filename().string() + ": " + e.what(), true);
            projectLoad_.reset(); // SyncRuntime tries again the usual way
        }
    }

    // A screen over the whole editor while a game opens: its name and how much of it has been read
    void SceneEditor::DrawProjectLoad()
    {
        if (!projectLoad_)
        {
            return;
        }
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float s = uiScale_;
        // An empty window over everything takes the clicks and keys meant for the editor underneath
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowFocus();
        ImGui::Begin("##OpeningGame", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav);
        ImGui::End();

        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        const ImVec2 min = viewport->Pos;
        const ImVec2 max(min.x + viewport->Size.x, min.y + viewport->Size.y);
        drawList->AddRectFilled(min, max, ImGui::GetColorU32(Theme::kGround));

        const std::size_t total = scene_.Description().objects.size();
        const ProjectLoad& load = *projectLoad_;
        const float done =
            load.started || total == 0 ? 1.0f : static_cast<float>(load.next) / static_cast<float>(total);
        const ImVec2 size(460.0f * s, 150.0f * s);
        const ImVec2 cardMin(min.x + (viewport->Size.x - size.x) * 0.5f, min.y + (viewport->Size.y - size.y) * 0.5f);
        const ImVec2 cardMax(cardMin.x + size.x, cardMin.y + size.y);
        const float round = 14.0f * s;
        // The cards' look: white, an ink outline and a hard shadow
        drawList->AddRectFilled(ImVec2(cardMin.x + 5.0f * s, cardMin.y + 5.0f * s),
                                ImVec2(cardMax.x + 5.0f * s, cardMax.y + 5.0f * s), ImGui::GetColorU32(Theme::kInk),
                                round);
        drawList->AddRectFilled(cardMin, cardMax, ImGui::GetColorU32(Theme::kPanel), round);
        drawList->AddRect(cardMin, cardMax, ImGui::GetColorU32(Theme::kBorder), round, ImDrawFlags_None, 2.0f * s);

        const float pad = 28.0f * s;
        ImGui::PushFont(Theme::BoldFont(), 24.0f);
        drawList->AddText(ImVec2(cardMin.x + pad, cardMin.y + pad), ImGui::GetColorU32(Theme::kText),
                          ("Opening " + projectDir_.filename().string()).c_str());
        ImGui::PopFont();
        ImGui::PushFont(nullptr, 14.0f);
        const std::string detail =
            load.started ? std::string("Getting the scene ready...")
                         : std::format("Loading models, pictures and sounds ({} of {} objects)", load.next, total);
        drawList->AddText(ImVec2(cardMin.x + pad, cardMin.y + pad + 38.0f * s), ImGui::GetColorU32(Theme::kTextMuted),
                          detail.c_str());
        ImGui::PopFont();

        const ImVec2 barMin(cardMin.x + pad, cardMax.y - pad - 16.0f * s);
        const ImVec2 barMax(cardMax.x - pad, cardMax.y - pad);
        drawList->AddRectFilled(barMin, barMax, ImGui::GetColorU32(Theme::kRaised), 8.0f * s);
        if (done > 0.0f)
        {
            drawList->AddRectFilled(barMin, ImVec2(barMin.x + (barMax.x - barMin.x) * done, barMax.y),
                                    ImGui::GetColorU32(Theme::kRun), 8.0f * s);
        }
        drawList->AddRect(barMin, barMax, ImGui::GetColorU32(Theme::kBorder), 8.0f * s, ImDrawFlags_None, 1.5f * s);
    }

    // Makes the game folder from the chosen template and switches to it
    void SceneEditor::CreateGame()
    {
        try
        {
            // A name is a folder name: spaces ("My game") become '_'
            std::replace(newGameName_.begin(), newGameName_.end(), ' ', '_');
            const auto project = Engine::Game::GameProject::Create(Engine::Game::GameProject::ProjectsRoot(),
                                                                   newGameName_.data(), kTemplates[newGameTemplate_]);
            newGameError_.clear();
            newGameName_.fill('\0');
            projects_ = Engine::Game::GameProject::FindAll(Engine::Game::GameProject::ProjectsRoot());
            OpenProject(project.ProjectDir());
            view_ = EditorView::Workspace;
            SetStatus("Created " + project.ProjectDir().string());
        }
        catch (const std::exception& e)
        {
            newGameError_ = e.what();
            LOG_ERROR(Editor, "Could not create game: {}", newGameError_);
        }
    }

    // Export game: building takes a few seconds and the editor stops while it does, so it starts two frames later,
    // once a frame saying so has been shown (Update counts exportCountdown_ down)
    void SceneEditor::BuildGame()
    {
        if (projectDir_.empty())
        {
            SetStatus("Open a game first, then export it", true);
            return;
        }
        if (exportCountdown_ == 0)
        {
            exportCountdown_ = 2;
            SetStatus("Exporting " + projectDir_.filename().string() + "... this takes a few seconds");
        }
    }

    // Builds the selected game into builds/<game>/ next to the editor: <game>[.exe] plus the game's data
    void SceneEditor::BuildGameNow()
    {
        if (projectDir_.empty())
        {
            return;
        }
        if (scenePath_ == StartScenePath() && HasUnsavedChanges())
        {
            SaveScene(); // the build copies the saved scene
        }
        try
        {
            // The player (the game without the editor) of a Release build if there is one; the editor only when
            // there is no player at all
            std::filesystem::path player = Engine::Game::FindPlayerExecutable(Engine::Platform::ExecutablePath());
            if (player.empty())
            {
                player = Engine::Platform::ExecutablePath();
                LOG_WARN(Editor, "No AtelierPlayer next to the editor: exporting with the editor itself");
            }
            const auto result = Engine::Game::BuildGame(Engine::Game::GameProject(projectDir_), player,
                                                        Engine::Platform::ExecutableDir() / "builds");
            Theme::Celebrate("Game exported!", "It is in builds/" +
                                                   result.executable.parent_path().filename().string() +
                                                   ", ready to share.");
            if (Engine::Game::IsDebugBuildOutput(player))
            {
                SetStatus("Exported to " + result.executable.parent_path().string() +
                              " - from a Debug build, so it runs slower and needs Visual Studio. Build the Release "
                              "configuration once for a fast game to share.",
                          true);
                LOG_WARN(Editor, "Exported with the Debug player {}: build AtelierPlayer in Release for a fast game",
                         player.string());
            }
            else
            {
                SetStatus("Exported! Your game is in " + result.executable.parent_path().string());
            }
        }
        catch (const std::exception& e)
        {
            SetStatus(std::string("Export failed: ") + e.what(), true);
        }
    }

    // The game's controls changed: a playing game restarts, the edit view reloads them
    void SceneEditor::ReloadGame()
    {
        if (playing_)
        {
            StopPlay();
            StartPlay();
        }
        else
        {
            runtime_.reset();
            runtimeProject_.clear();
        }
    }

    // ---------------------------------------------------------------- History, clipboard

    // Undo works with commands: creating and deleting objects and gizmo drags record their own. What is left is
    // recorded here: property edits as a PropertyChangeCommand, anything that also added, removed or reordered objects
    // (placing a prefab, Apply, Unpack, reparenting) as a snapshot of the whole scene
    void SceneEditor::CommitHistory(bool force)
    {
        const bool editing = ImGui::IsAnyItemActive() || ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if (gizmoObject_ != 0 && (force || !editing))
        {
            FinishGizmoDrag();
        }
        if (!force && editing)
        {
            return;
        }
        // What each prefab instance now has that its prefab does not (part of the same undo step as the edit)
        prefabs_.RefreshOverrides(scene_.Description());
        EditorScene::State now = scene_.CaptureState();
        if (auto command = Engine::History::MakeEditCommand(scene_, committed_, now))
        {
            history_.Push(std::move(command));
        }
        committed_ = std::move(now);
    }

    // Clear undo/redo, e.g. after opening a different scene
    void SceneEditor::ResetHistory()
    {
        consoleSessions_.clear();
        history_.Clear();
        gizmoObject_ = 0;
        committed_ = scene_.CaptureState();
    }

    // Called once the new object (and anything under it) is set up so redo brings it back with the same components
    void SceneEditor::RecordCreated(std::uint32_t id)
    {
        if (!scene_.Find(id))
        {
            return;
        }
        // Earlier unfinished edits become their own step first. The new objects are taken out meanwhile, so that step
        // does not include them too
        auto command = std::make_unique<Engine::History::CreateObjectCommand>(scene_, id);
        scene_.Remove(id);
        CommitHistory(true);
        history_.Execute(std::move(command));
        committed_ = scene_.CaptureState();
    }

    // One undo step from where the drag started to where it ended
    void SceneEditor::FinishGizmoDrag()
    {
        const std::uint32_t id = std::exchange(gizmoObject_, 0);
        const GameObject* object = scene_.Find(id);
        if (!object || !object->transform || *object->transform == gizmoBefore_)
        {
            return;
        }
        const std::string tool = gizmoTool_ == GizmoTool::Rotate  ? "Rotate "
                                 : gizmoTool_ == GizmoTool::Scale ? "Scale "
                                                                  : "Move ";
        history_.Push(std::make_unique<Engine::History::TransformCommand>(scene_, id, gizmoBefore_, *object->transform,
                                                                          tool + object->name));
        committed_ = scene_.CaptureState();
    }

    // Not while a drag is still held
    bool SceneEditor::IsDragging() const
    {
        return ImGui::IsMouseDown(ImGuiMouseButton_Left);
    }

    // Ctrl+Z: unfinished edits are recorded first
    void SceneEditor::Undo()
    {
        if (IsDragging())
        {
            return;
        }
        CommitHistory(true);
        const Engine::History::ICommand* command = history_.NextUndo();
        if (!command)
        {
            return;
        }
        const std::string name = command->Name();
        history_.Undo();
        committed_ = scene_.CaptureState();
        SetStatus("Undo " + name);
    }

    // Ctrl+Y / Ctrl+Shift+Z: the opposite of Undo
    void SceneEditor::Redo()
    {
        if (IsDragging())
        {
            return;
        }
        CommitHistory(true);
        const Engine::History::ICommand* command = history_.NextRedo();
        if (!command)
        {
            return;
        }
        const std::string name = command->Name();
        history_.Redo();
        committed_ = scene_.CaptureState();
        SetStatus("Redo " + name);
    }

    // Ctrl+C keeps a copy of the selected object and everything under it; Ctrl+V pastes it (with new ids)
    void SceneEditor::CopySelected()
    {
        if (const GameObject* object = scene_.Find(scene_.Selected()))
        {
            clipboard_ = scene_.CopyOf(object->id);
            SetStatus("Copied " + object->name);
        }
    }

    // Ctrl+V: adds the copied objects again at the top of the scene, where the original was in the world (new ids,
    // "Copy" added to the name)
    void SceneEditor::PasteClipboard()
    {
        if (!clipboard_.empty())
        {
            RecordCreated(scene_.Paste(clipboard_).id);
        }
    }

    // ---------------------------------------------------------------- Objects and prefabs

    // An instance's parts are laid out by its prefab: one deleted or moved here would come back (or go back) the next
    // time the instance is updated, so that is done in the prefab instead
    bool SceneEditor::CanRestructure(std::uint32_t id, const char* action)
    {
        // In Prefab Mode the root is the prefab itself: there is always exactly one
        if (InPrefabMode() && id == Engine::Scene::kPrefabRootId)
        {
            SetStatus(std::format("Cannot {} '{}': it is the prefab's root, which holds everything else in it", action,
                                  scene_.Find(id)->name),
                      true);
            return false;
        }
        const std::uint32_t root = Engine::Scene::InstanceRootOf(scene_.Description(), id);
        if (root != 0 && root != id)
        {
            SetStatus(std::format("Cannot {} '{}': it is part of the prefab instance '{}'. Unpack the instance first, "
                                  "or change the prefab.",
                                  action, scene_.Find(id)->name, scene_.Find(root)->name),
                      true);
            return false;
        }
        return true;
    }

    void SceneEditor::DeleteObject(std::uint32_t id)
    {
        // Removes everything under it too; one undo step brings all of it back
        if (scene_.Find(id) && CanRestructure(id, "delete"))
        {
            CommitHistory(true);
            history_.Execute(std::make_unique<Engine::History::DeleteObjectCommand>(scene_, id));
            committed_ = scene_.CaptureState();
        }
    }

    void SceneEditor::DuplicateObject(std::uint32_t id)
    {
        if (scene_.Find(id) && CanRestructure(id, "duplicate"))
        {
            RecordCreated(scene_.Duplicate(id).id);
        }
    }

    void SceneEditor::ReparentObject(std::uint32_t id, std::uint32_t parent)
    {
        if (!scene_.Find(id) || !CanRestructure(id, "move"))
        {
            return;
        }
        if (InPrefabMode() && parent == 0)
        {
            SetStatus("Everything in a prefab stays under its root: drop it onto the root (or another object) instead",
                      true);
            return;
        }
        if (!scene_.SetParent(id, parent))
        {
            SetStatus("An object cannot go under itself or under one of its own children", true);
            return;
        }
        scene_.Select(id);
    }

    void SceneEditor::SyncLibrary()
    {
        if (projectDir_ == libraryProject_)
        {
            return;
        }
        libraryProject_ = projectDir_;
        library_ = {};
        if (!projectDir_.empty())
        {
            Engine::Logic::LoadLibraryFile(projectDir_ / Engine::Logic::kLibraryFile, library_); // problems are logged
        }
    }

    void SceneEditor::LibraryChanged(std::string_view from, std::string_view to)
    {
        // A renamed library block: the blocks in this scene that run it follow (other scenes do when they are opened
        // and the old name is not found: they report it, and can be pointed at the new name)
        if (!from.empty())
        {
            for (GameObject& object : scene_.Objects())
            {
                if (object.logic)
                {
                    Engine::Logic::RenameCalls(*object.logic, from, to);
                }
            }
        }
        if (!projectDir_.empty())
        {
            try
            {
                Engine::Logic::SaveLibraryFile(projectDir_ / Engine::Logic::kLibraryFile, library_);
            }
            catch (const std::exception& e)
            {
                SetStatus(std::string("Could not save My Blocks: ") + e.what(), true);
            }
        }
        if (runtime_)
        {
            runtime_->SetLibrary(library_);
        }
    }

    void SceneEditor::ImportDropped(const std::vector<std::filesystem::path>& files)
    {
        if (projectDir_.empty())
        {
            SetStatus("Open a game first, then drop files on the editor to import them", true);
            return;
        }
        assetBrowser_.SetProject(projectDir_);
        std::vector<std::string> errors;
        const std::size_t count = assetBrowser_.Import(files, errors);
        for (const std::string& error : errors)
        {
            LOG_WARN(Editor, "Import: {}", error);
        }
        if (count == 0)
        {
            SetStatus("Nothing was imported" + (errors.empty() ? std::string{} : ": " + errors.front()), true);
            return;
        }
        SetStatus(std::format("Imported {} file{} into {}{}", count, count == 1 ? "" : "s", assetBrowser_.Folder(),
                              errors.empty() ? "" : " (some could not be copied: see Console)"));
        // Show where they went
        if (interface_ == InterfaceMode::Simple)
        {
            simpleLeftTab_ = 1;
        }
        else
        {
            showAssets_ = true;
        }
    }

    bool SceneEditor::IsGroup(const GameObject& object) const
    {
        return !object.renderable && !object.audioSource && !object.player && !object.camera && !object.light &&
               !object.logic && !object.prefabInstance;
    }

    void SceneEditor::GroupObject(std::uint32_t id)
    {
        const GameObject* object = scene_.Find(id);
        if (!object || !CanRestructure(id, "group"))
        {
            return;
        }
        // The group goes where the object is, in the same place in the tree, and the object goes into it
        const std::uint32_t parent = object->parent;
        const Engine::Scene::TransformComponent at = object->transform.value_or(Engine::Scene::TransformComponent{});
        GameObject& group = scene_.CreateObject("Group");
        group.transform = Engine::Scene::TransformComponent{.x = at.x, .y = at.y, .z = at.z};
        group.parent = parent;
        const std::uint32_t groupId = group.id;
        ReparentObject(id, groupId);
        scene_.Select(groupId);
        SetStatus("Grouped: drag more objects onto the group, and give it blocks to move them all together");
    }

    void SceneEditor::UngroupObject(std::uint32_t id)
    {
        const GameObject* object = scene_.Find(id);
        if (!object || !CanRestructure(id, "ungroup"))
        {
            return;
        }
        const std::uint32_t parent = object->parent;
        for (const std::uint32_t child : Engine::Scene::ChildrenOf(scene_.Description(), id))
        {
            if (!scene_.SetParent(child, parent))
            {
                SetStatus("Could not take everything out of the group", true);
                return;
            }
        }
        if (IsGroup(*scene_.Find(id)))
        {
            DeleteObject(id); // nothing left in it, and nothing of its own
        }
        SetStatus("Ungrouped");
    }

    // A new instance, named after the prefab and numbered. Without a place to go (at, parent) it goes next to the
    // instances already in the scene.
    void SceneEditor::PlacePrefab(const std::string& path, std::optional<glm::vec3> at, std::uint32_t parent)
    {
        // In Prefab Mode the instance is nested in the open prefab, which must not end up containing itself
        if (prefabMode_ && prefabs_.Contains(path, prefabMode_->path))
        {
            SetStatus(path == prefabMode_->path
                          ? "A prefab cannot be nested in itself"
                          : "Cannot nest " + path + " in " + prefabMode_->path + ": it already contains " +
                                prefabMode_->path + ", so the prefab would contain itself",
                      true);
            return;
        }
        const Engine::Scene::Prefab* prefab = prefabs_.Find(path);
        if (!prefab)
        {
            SetStatus("Could not read the prefab " + path + " (see Console)", true);
            return;
        }
        const auto placed = Engine::Game::PrefabManager::InstancesOf(scene_.Description(), path).size();
        Engine::Scene::TransformComponent placement =
            prefab->objects.front().transform.value_or(Engine::Scene::TransformComponent{});
        if (at)
        {
            // Dropped in the Viewport: where it landed, at the height the prefab's root has
            placement.x = at->x;
            placement.z = at->z;
        }
        else if (const auto world =
                     parent != 0 ? Engine::Scene::WorldTransformOf(scene_.Description(), parent) : std::nullopt)
        {
            // Dropped on an object in the Hierarchy: at that object (it goes under it below)
            placement.x = world->x;
            placement.y = world->y;
            placement.z = world->z;
        }
        else
        {
            placement.x += 2.0f * static_cast<float>(placed);
        }
        const std::string name = placed == 0 ? prefab->name : std::format("{} {}", prefab->name, placed + 1);
        const std::uint32_t root = prefabs_.Instantiate(scene_.Description(), path, scene_.NextId(), placement, name);
        if (root == 0)
        {
            return;
        }
        // Under parent, staying where it was placed in the world. Everything in a prefab is under its root.
        const std::uint32_t under = parent != 0 ? parent : InPrefabMode() ? Engine::Scene::kPrefabRootId : 0;
        if (under != 0 && scene_.Find(under))
        {
            scene_.SetParent(root, under);
        }
        scene_.Select(root);
        view_ = EditorView::Workspace;
        SetStatus(prefabMode_ ? "Nested " + name + " (an instance of " + path + ") in " + prefabMode_->name
                              : "Placed " + name + " (an instance of " + path + ")");
    }

    void SceneEditor::ApplyPrefab(std::uint32_t id)
    {
        try
        {
            prefabs_.ApplyToPrefab(scene_.Description(), id, scene_.NextId());
            SetStatus("Applied to the prefab: every other instance took the changes it had not made itself");
        }
        catch (const std::exception& e)
        {
            SetStatus(std::string("Apply to Prefab failed: ") + e.what(), true);
        }
    }

    // Inspector > an override's Apply: that one property becomes the prefab's, for every instance
    void SceneEditor::ApplyPrefabProperty(std::uint32_t id, const std::string& property)
    {
        try
        {
            if (prefabs_.ApplyProperty(scene_.Description(), id, property, scene_.NextId()))
            {
                SetStatus("Applied " + PropertyLabel(property) +
                          " to the prefab: every instance that had not changed it took it");
            }
        }
        catch (const std::exception& e)
        {
            SetStatus(std::string("Apply failed: ") + e.what(), true);
        }
    }

    // ---------------------------------------------------------------- Prefab Mode

    // The scene (with its selection, undo history and file) is kept aside and the prefab's objects take its place,
    // with the ids they have in the prefab, so saving keeps every instance's link to its parts
    void SceneEditor::OpenPrefab(const std::string& path)
    {
        if (prefabMode_)
        {
            if (prefabMode_->path == path)
            {
                return;
            }
            if (HasUnsavedChanges())
            {
                SetStatus("Save or close the open prefab before opening another", true);
                return;
            }
            LeavePrefabMode();
        }
        const Engine::Scene::Prefab* prefab = prefabs_.Find(path);
        if (!prefab)
        {
            SetStatus("Could not open the prefab " + path + " (see Console)", true);
            return;
        }
        StopPlay();
        CommitHistory(true); // the scene's last edit is an undo step of its own when it comes back

        prefabMode_ = PrefabModeContext{.path = path,
                                        .name = prefab->name,
                                        .scene = scene_.CaptureState(),
                                        .selected = scene_.Selected(),
                                        .history = std::move(history_),
                                        .committed = std::move(committed_),
                                        .saved = std::move(saved_),
                                        .scenePath = scenePath_};
        scene_.SetDescription(Engine::Game::PrefabManager::ToScene(*prefab));
        prefabs_.RefreshOverrides(scene_.Description()); // a variant's, against its base, before history starts
        scene_.Select(Engine::Scene::kPrefabRootId);
        scenePath_ = projectDir_ / path;
        ResetHistory();
        saved_ = scene_.CaptureState();
        view_ = EditorView::Workspace;
        FocusSelected();
        SetStatus("Prefab Mode: " + prefabMode_->name + " - Save (Ctrl+S) updates every instance; < Scene goes back");
    }

    // Writes the prefab file from the objects under the root. Instances in the scene take it when Prefab Mode closes.
    bool SceneEditor::SavePrefabMode()
    {
        if (!prefabMode_)
        {
            return false;
        }
        try
        {
            prefabs_.Save(prefabMode_->path,
                          Engine::Game::PrefabManager::FromScene(scene_.Description(), prefabMode_->name));
            saved_ = scene_.CaptureState();
            SetStatus("Saved prefab " + prefabMode_->path);
            return true;
        }
        catch (const std::exception& e)
        {
            SetStatus(std::string("Could not save the prefab: ") + e.what(), true);
            return false;
        }
    }

    void SceneEditor::ClosePrefab()
    {
        if (!InPrefabMode())
        {
            return;
        }
        if (HasUnsavedChanges())
        {
            showClosePrefabPopup_ = true; // Save / Don't Save / Cancel
            return;
        }
        LeavePrefabMode();
    }

    void SceneEditor::LeavePrefabMode()
    {
        if (!prefabMode_)
        {
            return;
        }
        PrefabModeContext context = std::move(*prefabMode_);
        prefabMode_.reset();

        scene_.RestoreState(context.scene);
        scene_.Select(context.selected);
        history_ = std::move(context.history);
        committed_ = std::move(context.committed);
        saved_ = std::move(context.saved);
        scenePath_ = std::move(context.scenePath);

        // The prefab may have been saved meanwhile: its instances, and those of its variants, take what they did not
        // change themselves (an edit of the scene, so it can be undone and shows as unsaved)
        const bool updated = prefabs_.UpdateInstances(scene_.Description(), scene_.NextId());
        prefabs_.RefreshOverrides(scene_.Description());
        if (!scene_.Find(scene_.Selected()))
        {
            scene_.Select(0);
        }
        SetStatus(updated ? "Back to the scene - its instances of " + context.name + " took the prefab's changes"
                          : "Back to the scene");
    }

    void SceneEditor::KeepPrefabUnderRoot()
    {
        if (!InPrefabMode() || !scene_.Find(Engine::Scene::kPrefabRootId))
        {
            return;
        }
        for (const std::uint32_t id : Engine::Scene::ChildrenOf(scene_.Description(), 0))
        {
            if (id != Engine::Scene::kPrefabRootId)
            {
                scene_.SetParent(id, Engine::Scene::kPrefabRootId); // stays where it is in the world
            }
        }
    }

    // Reads the prefab files again and brings the scene's instances up to date with them
    void SceneEditor::RefreshPrefabs()
    {
        prefabs_.Forget();
        prefabList_ = prefabs_.List();
        if (prefabs_.UpdateInstances(scene_.Description(), scene_.NextId()))
        {
            SetStatus("Prefab instances updated from their prefab files");
        }
    }

    // ---------------------------------------------------------------- Modes (tutorials)

    // Tutorial notes from the Modes page, saved per game in data/tutorials.editor.modes
    void SceneEditor::SaveModes()
    {
        // The lessons shipped with the editor are not this game's notes: only the lesson game keeps a copy, for the
        // learner's progress. They are saved in any other game only once a teacher changes them.
        const bool keep = !modesShipped_ || modesDirty_ || InLessonGame();
        modesDirty_ = false;
        if (projectDir_.empty() || !keep)
        {
            return;
        }
        std::string error;
        if (!TutorialNotes::Save(modes_, projectDir_ / kTutorialNotesFile, error))
        {
            LOG_ERROR(Editor, "{}", error);
        }
    }

    // Reads the game's tutorial notes; falls back to the default modes if there's no file or it's broken
    void SceneEditor::LoadModes()
    {
        modes_ = TutorialNotes::Defaults();
        modesShipped_ = false;
        // The lessons shipped with the editor (Track 1, ...) instead, when there are any: so they are on the Home page
        // from the start, before a game is open and for a game with no lesson notes of its own yet. How far the learner
        // got is kept in the lesson game, whichever game is open.
        {
            std::vector<Engine::Editor::TutorialMode> shipped;
            std::string shippedError;
            if (TutorialNotes::Load(shipped, Engine::Game::GameProject::TemplatesDir() / kTutorialNotesFile,
                                    shippedError) &&
                !shipped.empty())
            {
                std::vector<Engine::Editor::TutorialMode> progress;
                if (TutorialNotes::Load(progress, LessonGameDir() / kTutorialNotesFile, shippedError))
                {
                    for (Engine::Editor::TutorialMode& mode : shipped)
                    {
                        const auto found = std::find_if(progress.begin(), progress.end(),
                                                        [&mode](const auto& each) { return each.name == mode.name; });
                        mode.done = found != progress.end() ? found->done : mode.done;
                    }
                }
                modes_ = std::move(shipped);
                modesShipped_ = true;
            }
        }
        // The first track not finished yet
        const auto unfinished = std::find_if(modes_.begin(), modes_.end(), [](const Engine::Editor::TutorialMode& each)
                                             { return each.done < static_cast<int>(each.tutorials.size()); });
        activeMode_ = unfinished != modes_.end() ? static_cast<int>(unfinished - modes_.begin()) : 0;
        openTutorial_ = -1;
        modesDirty_ = false;
        if (projectDir_.empty() || InLessonGame())
        {
            return;
        }
        const auto path = projectDir_ / kTutorialNotesFile;
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec))
        {
            return; // no notes yet
        }

        std::vector<Engine::Editor::TutorialMode> own;
        std::string error;
        if (!TutorialNotes::Load(own, path, error))
        {
            LOG_WARN(Editor, "{} - using the shipped lessons", error);
            return;
        }
        // Switching games saves the notes, so a game no one wrote any in has a file of the empty modes, or an old copy
        // of the shipped lessons: neither is a reason to hide the lessons
        if (!own.empty() && !TutorialNotes::IsUntouched(own) && !own.front().name.starts_with("Track "))
        {
            modes_ = std::move(own);
            modesShipped_ = false;
        }
    }

    // Shows a message in the status bar and also logs it, so it ends up in the Console and log file
    void SceneEditor::SetStatus(std::string message, bool error)
    {
        status_ = std::move(message);
        statusError_ = error;
        statusTime_ = ImGui::GetCurrentContext() != nullptr ? ImGui::GetTime() : -100.0;
        if (error)
        {
            LOG_ERROR(Editor, "{}", status_);
        }
        else
        {
            LOG_INFO(Editor, "{}", status_);
        }
    }
}
