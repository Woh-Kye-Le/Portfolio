/* !
@file    	    SceneEditorSimple.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/10/2026
@brief
    Implements the parts of SceneEditor that follow the Atelier3D UI design:
    the top bar both interfaces share, the Home page (Learn / Create), and the
    Simple interface's one screen (Objects, Viewport, Logic, a short
    Inspector, the open lesson). The Advanced interface is the docked
    workspace in SceneEditor.cpp.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/SceneEditor.hpp"

#include "editor/Theme.hpp"
#include "editor/panels/assets/AssetDragDrop.hpp"

#include "engine/core/Log.hpp"
#include "engine/game/GameProject.hpp"
#include "engine/logic/Blocks.hpp"
#include "engine/scene/Hierarchy.hpp"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <format>
#include <functional>
#include <string>
#include <vector>

namespace Editor
{
    namespace
    {
        using Engine::Editor::GameObject;
        using Engine::Editor::Tutorial;
        using Engine::Game::Shape;

        // The same drag payload as the Advanced Hierarchy's: a game object's id
        constexpr const char* kObjectPayload = "ATELIER_OBJECT";

        ImU32 U32(const ImVec4& color)
        {
            return ImGui::GetColorU32(color);
        }

        // Done lessons and project thumbnails take these in turn
        constexpr std::array kPathColors{Theme::kMovement,   Theme::kEvents,  Theme::kVariables,
                                         Theme::kConditions, Theme::kLoops,   Theme::kObjects,
                                         Theme::kAudio,      Theme::kVisuals, Theme::kPhysics};

        // The colour an object shows in the lists and the Inspector: its own tint when it has one
        ImVec4 ObjectColor(const GameObject& object)
        {
            if (object.player)
            {
                return Theme::kMovement;
            }
            if (object.light)
            {
                return Theme::kEvents;
            }
            if (object.camera)
            {
                return Theme::kTextSoft;
            }
            if (object.renderable)
            {
                const auto& r = *object.renderable;
                const bool white = r.albedoR > 0.95f && r.albedoG > 0.95f && r.albedoB > 0.95f;
                return white ? Theme::kObjects : ImVec4(r.albedoR, r.albedoG, r.albedoB, 1.0f);
            }
            return Theme::kBorderStrong;
        }

        // What an object is, in a learner's words (the Inspector's subtitle)
        const char* ObjectKind(const GameObject& object)
        {
            if (object.screen)
            {
                switch (object.screen->kind)
                {
                case Engine::Scene::ScreenElementKind::Text: return "Text on the screen";
                case Engine::Scene::ScreenElementKind::Picture: return "Picture on the screen";
                case Engine::Scene::ScreenElementKind::Bar: return "Bar on the screen";
                case Engine::Scene::ScreenElementKind::Button: return "Button on the screen";
                }
            }
            if (object.player)
            {
                return "Player character";
            }
            if (object.camera)
            {
                return object.camera->primary ? "Main camera" : "Camera";
            }
            if (object.light)
            {
                return "Light";
            }
            if (object.renderable)
            {
                return "Object";
            }
            return "Empty (holds other objects)";
        }

        // A small picture of the object: a sun for a light, a camera, a ball for round shapes, else a block
        void DrawObjectIcon(ImDrawList* drawList, const GameObject& object, ImVec2 min, float size)
        {
            const ImU32 color = U32(ObjectColor(object));
            const ImVec2 center(min.x + size * 0.5f, min.y + size * 0.5f);
            const float thickness = std::max(1.5f, size * 0.09f);
            if (object.screen)
            {
                // A screen with something on it
                const ImU32 tint =
                    U32(ImVec4(object.screen->colorR, object.screen->colorG, object.screen->colorB, 1.0f));
                drawList->AddRect(ImVec2(min.x, min.y + size * 0.15f), ImVec2(min.x + size, min.y + size * 0.85f),
                                  U32(Theme::kTextSoft), size * 0.12f, ImDrawFlags_None, thickness);
                drawList->AddRectFilled(ImVec2(min.x + size * 0.22f, min.y + size * 0.4f),
                                        ImVec2(min.x + size * 0.78f, min.y + size * 0.6f), tint, size * 0.08f);
                return;
            }
            if (object.light)
            {
                drawList->AddCircle(center, size * 0.2f, color, 0, thickness);
                for (int i = 0; i < 8; ++i)
                {
                    const float angle = static_cast<float>(i) * 0.785398f;
                    const ImVec2 dir(std::cos(angle), std::sin(angle));
                    drawList->AddLine(ImVec2(center.x + dir.x * size * 0.32f, center.y + dir.y * size * 0.32f),
                                      ImVec2(center.x + dir.x * size * 0.46f, center.y + dir.y * size * 0.46f), color,
                                      thickness);
                }
                return;
            }
            if (object.camera)
            {
                drawList->AddRect(ImVec2(min.x + size * 0.1f, min.y + size * 0.3f),
                                  ImVec2(min.x + size * 0.66f, min.y + size * 0.72f), color, size * 0.08f,
                                  ImDrawFlags_None, thickness);
                drawList->AddTriangle(ImVec2(min.x + size * 0.68f, center.y),
                                      ImVec2(min.x + size * 0.9f, min.y + size * 0.32f),
                                      ImVec2(min.x + size * 0.9f, min.y + size * 0.7f), color, thickness);
                return;
            }
            if (object.renderable &&
                (object.renderable->shape == Shape::Sphere || object.renderable->shape == Shape::Circle))
            {
                drawList->AddCircleFilled(center, size * 0.5f, color);
                return;
            }
            if (!object.renderable && !object.player)
            {
                drawList->AddRect(min, ImVec2(min.x + size, min.y + size), color, size * 0.3f, ImDrawFlags_None,
                                  thickness);
                return;
            }
            drawList->AddRectFilled(min, ImVec2(min.x + size, min.y + size), color, size * 0.3f);
        }

        bool StartsWithHat(const Engine::Logic::Script& script)
        {
            return !script.blocks.empty() &&
                   Engine::Logic::InfoOf(script.blocks.front().op).shape == Engine::Logic::BlockShape::Hat;
        }

        // The scripts that run (those that start with a "when" block)
        std::size_t RunningScripts(const GameObject& object)
        {
            if (!object.logic)
            {
                return 0;
            }
            return static_cast<std::size_t>(
                std::count_if(object.logic->scripts.begin(), object.logic->scripts.end(), StartsWithHat));
        }

        // The templates' names in the New game dialog (Describe has the long version, for the tooltip)
        const char* ShortName(Engine::Game::GameTemplate gameTemplate)
        {
            using Engine::Game::GameTemplate;
            switch (gameTemplate)
            {
            case GameTemplate::Empty: return "Empty world";
            case GameTemplate::Rendering: return "Crates & lights";
            case GameTemplate::Resources: return "Missing files";
            case GameTemplate::Ecs: return "Speed test";
            case GameTemplate::Audio: return "Sound & music";
            case GameTemplate::Input: return "Walk around";
            case GameTemplate::Prefabs: return "Enemies";
            case GameTemplate::Rpg: return "Adventure (RPG)";
            case GameTemplate::Fps: return "Arena shooter (FPS)";
            case GameTemplate::Racing: return "Racing";
            }
            return "";
        }

        // A panel of the design: a rounded card on the ground colour, outlined in ink on a hard ink shadow (when it has
        // a border). With height 0 it grows to fit what is in it.
        template <class Draw>
        void Card(const char* id, ImVec2 size, float padding, ImGuiWindowFlags flags, Draw&& draw,
                  const ImVec4& background = Theme::kPanel, bool border = true)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, background);
            ImGui::PushStyleColor(ImGuiCol_Border, Theme::kBorder);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.0f * ImGui::GetStyle().FontScaleDpi);
            ImGuiChildFlags childFlags = ImGuiChildFlags_AlwaysUseWindowPadding;
            if (border)
            {
                childFlags |= ImGuiChildFlags_Borders;
            }
            if (size.y <= 0.0f)
            {
                childFlags |= ImGuiChildFlags_AutoResizeY;
            }
            // The card's background is drawn in the parent's draw list: draw it on a layer above the shadow's
            ImDrawList* parent = ImGui::GetWindowDrawList();
            if (border)
            {
                parent->ChannelsSplit(2);
                parent->ChannelsSetCurrent(1);
            }
            const bool visible = ImGui::BeginChild(id, size, childFlags, flags);
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
            if (visible)
            {
                draw();
            }
            ImGui::EndChild();
            if (border)
            {
                const float depth = 4.0f * ImGui::GetStyle().FontScaleDpi;
                const ImVec2 min = ImGui::GetItemRectMin();
                const ImVec2 max = ImGui::GetItemRectMax();
                parent->ChannelsSetCurrent(0);
                parent->AddRectFilled(ImVec2(min.x + depth, min.y + depth), ImVec2(max.x + depth, max.y + depth),
                                      U32(Theme::kInk), ImGui::GetStyle().ChildRounding);
                parent->ChannelsMerge();
            }
        }

        // The design's dialogs: a centred modal with no title bar (the caller draws a bold heading) and room around it
        bool BeginDialog(const char* id, float scale)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f * scale, 22.0f * scale));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f * scale);
            const bool open =
                ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::PopStyleVar(2);
            return open;
        }

    }

    // ---------------------------------------------------------------- Interface

    void SceneEditor::SetInterface(InterfaceMode mode)
    {
        if (interface_ == mode)
        {
            return;
        }
        interface_ = mode;
        controls_->CancelListening();
        ImGui::MarkIniSettingsDirty();
        LOG_INFO(Editor, "{} interface", mode == InterfaceMode::Simple ? "Simple" : "Advanced");
    }

    // [AtelierEditor][Interface] Advanced=0|1 and Variables=0|1 (the Variable Inspector is open) in imgui.ini
    void SceneEditor::RegisterSettings()
    {
        ImGuiSettingsHandler handler;
        handler.TypeName = "AtelierEditor";
        handler.TypeHash = ImHashStr("AtelierEditor");
        handler.UserData = this;
        handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler* self, const char* name) -> void*
        { return std::strcmp(name, "Interface") == 0 ? self->UserData : nullptr; };
        handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line)
        {
            if (std::strncmp(line, "Advanced=", 9) == 0)
            {
                static_cast<SceneEditor*>(entry)->interface_ =
                    line[9] == '1' ? InterfaceMode::Advanced : InterfaceMode::Simple;
            }
            else if (std::strncmp(line, "Variables=", 10) == 0)
            {
                static_cast<SceneEditor*>(entry)->showVariables_ = line[10] != '0';
            }
            else if (std::strncmp(line, "Toured=", 7) == 0)
            {
                static_cast<SceneEditor*>(entry)->tourDone_ = line[7] != '0';
            }
            else if (std::strncmp(line, "AdvancedToured=", 15) == 0)
            {
                static_cast<SceneEditor*>(entry)->advancedTourDone_ = line[15] != '0';
            }
            else if (std::strncmp(line, "DebugExtras=", 12) == 0 && std::strlen(line) >= 14)
            {
                auto& lines = static_cast<SceneEditor*>(entry)->debugLines_;
                lines.forces = line[12] == '1';
                lines.layerColors = line[13] == '1';
            }
            else if (std::strncmp(line, "DebugLines=", 11) == 0 && std::strlen(line) >= 15)
            {
                auto& lines = static_cast<SceneEditor*>(entry)->debugLines_;
                lines.bounds = line[11] == '1';
                lines.colliders = line[12] == '1';
                lines.rays = line[13] == '1';
                lines.contacts = line[14] == '1';
            }
        };
        handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* self, ImGuiTextBuffer* out)
        {
            const auto* editor = static_cast<const SceneEditor*>(self->UserData);
            const auto& lines = editor->debugLines_;
            out->appendf(
                "[%s][Interface]\nAdvanced=%d\nVariables=%d\nToured=%d\nAdvancedToured=%d\nDebugLines=%d%d%d%d\n"
                "DebugExtras=%d%d\n\n",
                self->TypeName, editor->interface_ == InterfaceMode::Advanced ? 1 : 0, editor->showVariables_ ? 1 : 0,
                editor->tourDone_ ? 1 : 0, editor->advancedTourDone_ ? 1 : 0, lines.bounds ? 1 : 0,
                lines.colliders ? 1 : 0, lines.rays ? 1 : 0, lines.contacts ? 1 : 0, lines.forces ? 1 : 0,
                lines.layerColors ? 1 : 0);
        };
        ImGui::AddSettingsHandler(&handler);
    }

    // ---------------------------------------------------------------- Top bar

    void SceneEditor::DrawTopBar()
    {
        const float s = uiScale_;
        const float menu = interface_ == InterfaceMode::Advanced ? ImGui::GetFrameHeight() : 0.0f;
        const float height = 64.0f * s;
        const float button = 44.0f * s;
        const float buttonY = (height - button) * 0.5f;
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        topBarBottom_ = menu + height;

        ImGui::SetNextWindowPos(ImVec2(0.0f, menu));
        ImGui::SetNextWindowSize(ImVec2(display.x, height));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::kHeader);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * s, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                           ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImGui::Begin("##TopBar", nullptr, flags);
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetWindowPos();
        drawList->AddLine(ImVec2(origin.x, origin.y + height - 1.0f),
                          ImVec2(origin.x + display.x, origin.y + height - 1.0f), U32(Theme::kBorder));
        const bool workspace = view_ == EditorView::Workspace;

        // Left: on Home the name; in the workspace, back to Home, what is open, and undo / redo
        if (!workspace)
        {
            ImGui::SetCursorPos(ImVec2(16.0f * s, (height - 34.0f * s) * 0.5f));
            const ImVec2 logo = ImGui::GetCursorScreenPos();
            Theme::DrawIcon(drawList, Theme::Icon::Logo, ImVec2(logo.x + 17.0f * s, logo.y + 17.0f * s), 34.0f * s,
                            U32(Theme::kEvents));
            ImGui::Dummy(ImVec2(34.0f * s, 34.0f * s));
            ImGui::SameLine(0.0f, 12.0f * s);
            ImGui::PushFont(Theme::DisplayFont(), 22.0f);
            ImGui::SetCursorPosY((height - ImGui::GetTextLineHeight()) * 0.5f);
            ImGui::TextUnformatted("Atelier3D");
            ImGui::PopFont();
        }
        else
        {
            ImGui::SetCursorPos(ImVec2(16.0f * s, buttonY));
            // While a prefab is open, back goes to the scene it was opened from (asking about unsaved changes)
            if (Theme::IconButton("##home", Theme::Icon::Back, button,
                                  InPrefabMode() ? "Back to the scene" : "Home: lessons and games"))
            {
                if (InPrefabMode())
                {
                    ClosePrefab();
                }
                else
                {
                    SaveThumbnail();
                    StopPlay();
                    view_ = EditorView::Home;
                }
            }
            TourMark("home", ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            ImGui::SameLine(0.0f, 12.0f * s);

            const auto& tutorials = modes_[activeMode_].tutorials;
            const bool lesson = openTutorial_ >= 0 && openTutorial_ < static_cast<int>(tutorials.size());
            const char* kind = InPrefabMode() ? "SAVED OBJECT" : lesson ? "LESSON" : "SANDBOX";
            const ImVec4 kindColor = InPrefabMode() ? Theme::kSelect
                                     : lesson       ? Theme::kWarningText
                                                    : Theme::kSuccessText;
            const std::string name = prefabMode_           ? prefabMode_->name
                                     : projectDir_.empty() ? std::string("No game open")
                                                           : projectDir_.filename().string();
            ImGui::SetCursorPosY(height * 0.5f - 20.0f * s);
            ImGui::BeginGroup();
            Theme::Text(kind, 13.0f, kindColor, true);
            Theme::Text(name.c_str(), 17.0f, Theme::kText, true);
            ImGui::SameLine(0.0f, 6.0f * s);
            ImGui::PushFont(nullptr, 13.0f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f * s);
            if (!HasUnsavedChanges())
            {
                ImGui::TextColored(Theme::kTextMuted, "\xC2\xB7 saved");
            }
            else if (ImGui::TextLink("Save"))
            {
                SaveScene();
            }
            else if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("There are changes that are not saved yet (Ctrl+S)");
            }
            ImGui::PopFont();
            ImGui::EndGroup();

            ImGui::SameLine(0.0f, 16.0f * s);
            ImGui::SetCursorPosY(buttonY);
            const bool canUndo = history_.CanUndo() || scene_.CaptureState() != committed_;
            if (Theme::IconButton("##undo", Theme::Icon::Undo, button, "Undo (Ctrl+Z)", false, canUndo))
            {
                Undo();
            }
            ImGui::SameLine(0.0f, 6.0f * s);
            if (Theme::IconButton("##redo", Theme::Icon::Redo, button, "Redo (Ctrl+Y)", false, history_.CanRedo()))
            {
                Redo();
            }
        }
        const float leftEnd = ImGui::GetItemRectMax().x - origin.x;

        // Where the right side starts: the interface switch, then Export
        static constexpr std::array<const char*, 2> kInterfaces{"Simple", "Advanced"};
        const float switchHeight = 40.0f * s;
        const float switchWidth = Theme::SegmentedWidth(kInterfaces.data(), 2, switchHeight);
        const float exportWidth = workspace ? Theme::PillWidth("Export game", button, true) : 0.0f;
        const float right = display.x - 16.0f * s - exportWidth - (workspace ? 10.0f * s : 0.0f) - switchWidth;

        // Centre: Run / Pause / Stop on their own dark strip (moved left of the switch when the window is narrow)
        if (workspace)
        {
            const char* runLabel = !playing_ ? "Run" : paused_ ? "Resume" : "Running";
            const float runWidth = std::max(Theme::PillWidth("Running", button, true), 110.0f * s);
            const float pad = 6.0f * s;
            const float gap = 8.0f * s;
            const float total = pad * 2.0f + runWidth + (gap + button) * 3.0f;
            const float x =
                std::max(std::min((display.x - total) * 0.5f, right - total - 16.0f * s), leftEnd + 16.0f * s);
            const ImVec2 stripMin(origin.x + x, origin.y + buttonY - pad);
            drawList->AddRectFilled(stripMin, ImVec2(stripMin.x + total, stripMin.y + button + pad * 2.0f),
                                    U32(Theme::kGround), 16.0f * s);
            TourMark("run", stripMin, ImVec2(stripMin.x + total, stripMin.y + button + pad * 2.0f));
            ImGui::SetCursorPos(ImVec2(x + pad, buttonY));
            if (Theme::PillButton(runLabel, Theme::kRun, Theme::kInk, button, runWidth, true, Theme::Icon::Play,
                                  !projectDir_.empty()))
            {
                if (!playing_)
                {
                    StartPlay();
                }
                else if (paused_)
                {
                    TogglePause();
                }
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip(projectDir_.empty() ? "Open or make a game first (Home)"
                                  : !playing_         ? "Run the game (F5)"
                                  : paused_           ? "Carry on running the game"
                                                      : "The game is running. Stop (F5) goes back to editing.");
            }
            ImGui::SameLine(0.0f, gap);
            if (Theme::IconButton("##pause", Theme::Icon::Pause, button, "Pause", paused_, playing_, !playing_))
            {
                TogglePause();
            }
            ImGui::SameLine(0.0f, gap);
            if (Theme::IconButton("##step", Theme::Icon::Step, button,
                                  "Step (F8): run the next block of one script, then pause again.\nClick a block to "
                                  "pick the script.",
                                  false, paused_, !paused_))
            {
                Step();
            }
            ImGui::SameLine(0.0f, gap);
            if (Theme::IconButton("##stop", Theme::Icon::Stop, button, "Stop and reset (F5)", false, playing_,
                                  !playing_))
            {
                StopPlay();
            }
        }

        // Right: the interface switch, then Export
        ImGui::SetCursorPos(ImVec2(right, (height - switchHeight) * 0.5f));
        int current = static_cast<int>(interface_);
        if (Theme::Segmented("##Interface", kInterfaces.data(), 2, current, switchWidth, switchHeight))
        {
            SetInterface(static_cast<InterfaceMode>(current));
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("Simple: the screen for learning, with what you need.\nAdvanced: every window and "
                              "menu, like a commercial engine.");
        }
        if (workspace)
        {
            ImGui::SameLine(0.0f, 10.0f * s);
            ImGui::SetCursorPosY(buttonY);
            if (Theme::PillButton("Export game", Theme::kRaised, Theme::kText, button, 0.0f, true, Theme::Icon::Export,
                                  !projectDir_.empty() && !playing_))
            {
                BuildGame();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("Build a copy of the game your friends can play (Ctrl+B)");
            }
        }
        ImGui::End();
    }

    // ---------------------------------------------------------------- Dialogs

    // A name, and the template to start from (the tiles of the Home page's Create card)
    void SceneEditor::DrawNewGamePopup()
    {
        if (showNewGamePopup_)
        {
            ImGui::OpenPopup("##NewGame");
            showNewGamePopup_ = false;
            newGameError_.clear();
        }
        const float s = uiScale_;
        if (!BeginDialog("##NewGame", s))
        {
            return;
        }
        const float width = 470.0f * s; // three template tiles across
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
        Theme::Text("Make a new game", 22.0f, Theme::kText, true);
        ImGui::Dummy(ImVec2(width, 8.0f * s));

        Theme::Label("NAME");
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(width);
        const bool enter = ImGui::InputTextWithHint("##name", "My first game", newGameName_.data(), newGameName_.size(),
                                                    ImGuiInputTextFlags_EnterReturnsTrue);
        Theme::Text("Letters, numbers, spaces, - and _", 13.0f, Theme::kTextMuted);
        ImGui::Dummy(ImVec2(0.0f, 8.0f * s));

        // Start empty, or from one of the demo scenes (with the asset files it uses)
        Theme::Label("START FROM");
        const float gap = 10.0f * s;
        const float tile = (width - gap * 2.0f) / 3.0f;
        const auto& templates = Engine::Game::kGameTemplates;
        for (int i = 0; i < static_cast<int>(templates.size()); ++i)
        {
            if (i % 3 != 0)
            {
                ImGui::SameLine(0.0f, gap);
            }
            const bool chosen = newGameTemplate_ == i;
            ImGui::PushID(i);
            if (Theme::PillButton(ShortName(templates[i]), chosen ? Theme::kRaisedHigh : Theme::kRaised, Theme::kText,
                                  40.0f * s, tile))
            {
                newGameTemplate_ = i;
            }
            ImGui::PopID();
        }
        Theme::TextWrapped(Engine::Game::Describe(templates[newGameTemplate_]), 14.0f, Theme::kTextSoft);
        if (!newGameError_.empty())
        {
            Theme::TextWrapped(newGameError_.c_str(), 14.0f, Theme::kErrorText);
        }
        ImGui::PopTextWrapPos();

        ImGui::Dummy(ImVec2(0.0f, 10.0f * s));
        const float height = 44.0f * s;
        const float make = Theme::PillWidth("Make game", height, true);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + width - make - Theme::PillWidth("Cancel", height) - gap);
        if (Theme::PillButton("Cancel", Theme::kRaised, Theme::kText, height) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0.0f, gap);
        if (Theme::PillButton("Make game", Theme::kObjects, Theme::kInk, height, 0.0f, true, Theme::Icon::Plus) ||
            enter)
        {
            CreateGame();
            if (newGameError_.empty())
            {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    void SceneEditor::AskToSave(std::function<void()> then)
    {
        if (!HasUnsavedChanges())
        {
            then();
            return;
        }
        afterSaving_ = std::move(then);
        showUnsavedPopup_ = true;
    }

    // Save / Don't save / Cancel, then what AskToSave was given (unless cancelled)
    void SceneEditor::DrawUnsavedPopup()
    {
        if (showUnsavedPopup_)
        {
            ImGui::OpenPopup("##Unsaved");
            showUnsavedPopup_ = false;
        }
        const float s = uiScale_;
        if (!BeginDialog("##Unsaved", s))
        {
            return;
        }
        const std::string name = prefabMode_           ? prefabMode_->name
                                 : projectDir_.empty() ? std::string("this scene")
                                                       : projectDir_.filename().string();
        Theme::Text(std::format("Save your changes to {}?", name).c_str(), 20.0f, Theme::kText, true);
        ImGui::Spacing();
        Theme::Text("If you don't save, the changes since your last save are lost.", 15.0f, Theme::kTextSoft);
        ImGui::Dummy(ImVec2(0.0f, 12.0f * s));

        std::function<void()> proceed;
        const float height = 44.0f * s;
        const float gap = 10.0f * s;
        if (Theme::PillButton("Save", Theme::kObjects, Theme::kInk, height))
        {
            SaveScene();
            // Still unsaved: Save As is asking where (a scene never saved before), or saving failed (the status says
            // why). Nothing is lost; the learner can try again.
            if (!HasUnsavedChanges())
            {
                proceed = std::move(afterSaving_);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0.0f, gap);
        if (Theme::PillButton("Don't save", Theme::kRaised, Theme::kText, height))
        {
            saved_ = scene_.CaptureState(); // the learner chose to drop them
            proceed = std::move(afterSaving_);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0.0f, gap);
        if (Theme::PillButton("Cancel", Theme::kGround, Theme::kTextSoft, height) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        if (proceed)
        {
            proceed();
        }
    }

    void SceneEditor::DrawResetLessonsPopup()
    {
        if (showResetLessonsPopup_)
        {
            ImGui::OpenPopup("##ResetLessons");
            showResetLessonsPopup_ = false;
        }
        const float s = uiScale_;
        if (!BeginDialog("##ResetLessons", s))
        {
            return;
        }
        Theme::Text("Reset your lessons?", 20.0f, Theme::kText, true);
        ImGui::Spacing();
        Theme::Text("You start again from the first lesson.", 15.0f, Theme::kTextSoft);
        Theme::Text(modesShipped_ ? "Everything you built in the lessons goes to the Recycle Bin."
                                  : "This game stays as it is.",
                    15.0f, Theme::kTextSoft);
        ImGui::Dummy(ImVec2(0.0f, 12.0f * s));

        const float height = 44.0f * s;
        const bool reset = Theme::PillButton("Reset", Theme::kError, Theme::kInk, height);
        ImGui::SameLine(0.0f, 10.0f * s);
        if (reset || Theme::PillButton("Cancel", Theme::kGround, Theme::kTextSoft, height) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        if (reset)
        {
            ResetLessons();
        }
    }

    // ---------------------------------------------------------------- Home

    void SceneEditor::DrawHome()
    {
        const float s = uiScale_;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::kGround);
        ImGui::BeginChild("##Home", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
        ImGui::PopStyleColor();

        const float avail = ImGui::GetContentRegionAvail().x;
        const float width = std::max(std::min(avail - 80.0f * s, 1200.0f * s), 320.0f * s);
        ImGui::SetCursorPos(ImVec2(std::max((avail - width) * 0.5f, 12.0f * s), 40.0f * s));
        ImGui::BeginGroup();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);

        Theme::Text("What do you want to do today?", 34.0f, Theme::kText, true);
        Theme::Text("Learn one idea at a time, or build anything you like.", 17.0f, Theme::kTextMuted);
        ImGui::Dummy(ImVec2(0.0f, 20.0f * s));

        Engine::Editor::TutorialMode& mode = modes_[activeMode_];
        const int lessons = static_cast<int>(mode.tutorials.size());
        // Done: the lessons finished in order (Next lesson / Finish). Up next: the open one, else the first not done
        const int done = std::clamp(mode.done, 0, lessons);
        const int next =
            openTutorial_ >= 0 && openTutorial_ < lessons ? openTutorial_ : std::min(done, std::max(lessons - 1, 0));
        const bool allDone = lessons > 0 && done == lessons;
        // Not now: opening a lesson can switch games, which replaces modes_ while this page is still drawing from it
        const auto openLesson = [this](int index) { pendingLesson_ = std::pair(activeMode_, index); };
        // The editor's own lessons open in order; a teacher's can be opened in any
        const auto unlocked = [this](int index)
        { return !modesShipped_ || Engine::Editor::TutorialNotes::IsUnlocked(modes_, activeMode_, index); };

        // A card's heading: a coloured tile with its icon, a title and a line under it
        const auto cardHeading = [s](Theme::Icon icon, const ImVec4& color, const char* title, const char* subtitle)
        {
            const ImVec2 tile = ImGui::GetCursorScreenPos();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(tile, ImVec2(tile.x + 52.0f * s, tile.y + 52.0f * s), U32(color), 16.0f * s);
            Theme::DrawIcon(drawList, icon, ImVec2(tile.x + 26.0f * s, tile.y + 26.0f * s), 28.0f * s,
                            U32(Theme::kInk));
            ImGui::Dummy(ImVec2(52.0f * s, 52.0f * s));
            ImGui::SameLine(0.0f, 14.0f * s);
            ImGui::BeginGroup();
            Theme::Text(title, 26.0f, Theme::kText, true);
            Theme::Text(subtitle, 15.0f, Theme::kTextMuted);
            ImGui::EndGroup();
        };

        const bool sideBySide = width > 760.0f * s;
        const float cardWidth = sideBySide ? (width - 24.0f * s) * 0.5f : width;
        // Tall enough for the Create card's tiles and button however many rows they wrap to (measured last frame)
        const float cardHeight = std::max(360.0f * s, homeCardHeight_);
        Card("##Learn", ImVec2(cardWidth, cardHeight), 28.0f * s, ImGuiWindowFlags_NoScrollbar,
             [&]
             {
                 const ImVec2 learnAt = ImGui::GetWindowPos();
                 TourMark("learn", learnAt,
                          ImVec2(learnAt.x + ImGui::GetWindowWidth(), learnAt.y + ImGui::GetWindowHeight()));
                 cardHeading(Theme::Icon::Learn, Theme::kEvents, "Learn", "Teaching Mode \xC2\xB7 guided lessons");
                 ImGui::Spacing();
                 Theme::TextWrapped("Build one game from nothing, a few blocks at a time. Each lesson gives you goals "
                                    "and ticks them off as you reach them.",
                                    16.0f, Theme::kTextSoft);
                 ImGui::Spacing();
                 Card(
                     "##UpNext", ImVec2(0.0f, 0.0f), 16.0f * s, ImGuiWindowFlags_NoScrollbar,
                     [&]
                     {
                         if (lessons == 0)
                         {
                             Theme::Text("No lessons yet", 18.0f, Theme::kText, true);
                             Theme::TextWrapped(interface_ == InterfaceMode::Advanced
                                                    ? "Add one with + on the learning path below."
                                                    : "Your teacher adds lessons here. Until then, try Create.",
                                                14.0f, Theme::kTextMuted);
                             return;
                         }
                         const std::string progress = std::format("{} of {} done", done, lessons);
                         Theme::Text(allDone ? "All done - open any lesson to go over it" : "Up next", 14.0f,
                                     Theme::kTextSoft);
                         ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(progress.c_str()).x);
                         Theme::Text(progress.c_str(), 14.0f, Theme::kTextSoft);
                         Theme::Text(mode.tutorials[next].name.c_str(), 19.0f, Theme::kText, true);
                         const ImVec2 bar = ImGui::GetCursorScreenPos();
                         const float barWidth = ImGui::GetContentRegionAvail().x;
                         ImDrawList* drawList = ImGui::GetWindowDrawList();
                         drawList->AddRectFilled(bar, ImVec2(bar.x + barWidth, bar.y + 10.0f * s), U32(Theme::kGround),
                                                 999.0f);
                         const float fraction = static_cast<float>(done) / static_cast<float>(lessons);
                         if (fraction > 0.0f)
                         {
                             drawList->AddRectFilled(bar, ImVec2(bar.x + barWidth * fraction, bar.y + 10.0f * s),
                                                     U32(Theme::kEvents), 999.0f);
                         }
                         ImGui::Dummy(ImVec2(barWidth, 10.0f * s));
                     },
                     Theme::kRaised, false);
                 ImGui::Spacing();
                 // No lessons: no button that looks like it would do something
                 if (lessons > 0 && Theme::PillButton("Continue lesson", Theme::kEvents, Theme::kInk, 48.0f * s, 0.0f,
                                                      true, Theme::Icon::Arrow, unlocked(next)))
                 {
                     openLesson(next);
                 }
                 ImGui::SameLine(0.0f, 14.0f * s);
                 ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 14.0f * s);
                 if (ImGui::TextLink("Show me around"))
                 {
                     StartTour();
                 }
             });
        if (sideBySide)
        {
            ImGui::SameLine(0.0f, 24.0f * s);
        }
        else
        {
            ImGui::Dummy(ImVec2(0.0f, 8.0f * s));
        }
        Card("##Create", ImVec2(cardWidth, cardHeight), 28.0f * s, ImGuiWindowFlags_NoScrollbar,
             [&]
             {
                 const ImVec2 createAt = ImGui::GetWindowPos();
                 TourMark("create", createAt,
                          ImVec2(createAt.x + ImGui::GetWindowWidth(), createAt.y + ImGui::GetWindowHeight()));
                 cardHeading(Theme::Icon::Create, Theme::kObjects, "Create", "Sandbox Mode \xC2\xB7 no rules");
                 ImGui::Spacing();
                 Theme::TextWrapped("Start from an empty world or a template. Build a scene, give objects logic, test "
                                    "it, and export a game your friends can play.",
                                    16.0f, Theme::kTextSoft);
                 ImGui::Spacing();
                 // One button: its dialog has the templates to pick from
                 if (Theme::PillButton("New game", Theme::kObjects, Theme::kInk, 48.0f * s, 0.0f, true,
                                       Theme::Icon::Plus))
                 {
                     newGameTemplate_ = 0;
                     showNewGamePopup_ = true;
                 }
                 homeCardHeight_ = ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y + 28.0f * s;
             });

        // The learning path: the lessons of the chosen mode, in order
        ImGui::Dummy(ImVec2(0.0f, 28.0f * s));
        const float pathLeft = ImGui::GetCursorPosX();
        Theme::Text("Your learning path", 20.0f, Theme::kText, true);
        // Only with something to reset: lessons finished, or the lesson game with what was built in it so far
        const bool anyDone = std::any_of(modes_.begin(), modes_.end(),
                                         [](const Engine::Editor::TutorialMode& each) { return each.done > 0; });
        const bool lessonGameMade =
            std::any_of(projects_.begin(), projects_.end(),
                        [this](const std::filesystem::path& each) { return IsLessonGame(each); });
        if (anyDone || (modesShipped_ && lessonGameMade))
        {
            const float resetHeight = 36.0f * s;
            ImGui::SameLine();
            ImGui::SetCursorPosX(pathLeft + width - Theme::PillWidth("Reset lessons", resetHeight));
            if (Theme::PillButton("Reset lessons", Theme::kRaised, Theme::kText, resetHeight))
            {
                showResetLessonsPopup_ = true;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("Start the lessons again from the first one");
            }
        }
        // The modes to choose from: all of them in Advanced (where lessons are added); in Simple only those with
        // lessons (and the one open), so a learner never picks an empty path
        const bool advanced = interface_ == InterfaceMode::Advanced;
        std::vector<int> shown;
        for (int i = 0; i < static_cast<int>(modes_.size()); ++i)
        {
            if (advanced || i == activeMode_ || !modes_[static_cast<std::size_t>(i)].tutorials.empty())
            {
                shown.push_back(i);
            }
        }
        if (shown.size() > 1)
        {
            // "Track 2: Physics" reads as Physics here: the tabs are in order already
            std::vector<const char*> names;
            for (const int i : shown)
            {
                const std::string& name = modes_[static_cast<std::size_t>(i)].name;
                const std::size_t colon = name.starts_with("Track ") ? name.find(": ") : std::string::npos;
                names.push_back(name.c_str() + (colon == std::string::npos ? 0 : colon + 2));
            }
            ImGui::Spacing();
            int chosen = static_cast<int>(std::find(shown.begin(), shown.end(), activeMode_) - shown.begin());
            if (Theme::Segmented("##Modes", names.data(), static_cast<int>(names.size()), chosen, 0.0f, 36.0f * s))
            {
                activeMode_ = shown[static_cast<std::size_t>(chosen)];
                openTutorial_ = -1;
            }
        }
        Theme::Text(lessons == 0     ? "No lessons on this path yet."
                    : !modesShipped_ ? "Go in order, or click any lesson to open it."
                    : unlocked(0)    ? "Finish a lesson to unlock the next one."
                                     : "Finish the track before this one to unlock it.",
                    15.0f, Theme::kTextMuted);
        ImGui::Spacing();

        const float node = 56.0f * s;
        const float slot = 120.0f * s;
        const float ringRoom = 10.0f * s; // the current lesson's ring stands 6 out from its node: inside the path
        const int nodes = lessons;
        const float link =
            nodes > 1 ? std::max(24.0f * s, (width - slot * static_cast<float>(nodes)) / static_cast<float>(nodes - 1))
                      : 0.0f;
        const float pathWidth = slot * static_cast<float>(nodes) + link * static_cast<float>(std::max(nodes - 1, 0));
        int removeIndex = -1;
        // Nothing to draw (no lessons): the line above says so, and the path takes no room
        ImGui::BeginChild("##Path", ImVec2(width, nodes > 0 ? node + ringRoom + 64.0f * s : 1.0f), ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar);
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 start = ImGui::GetCursorScreenPos();
            for (int i = 0; i < nodes; ++i)
            {
                const float x = start.x + static_cast<float>(i) * (slot + link);
                const ImVec2 min(x + (slot - node) * 0.5f, start.y + ringRoom);
                const ImVec2 max(min.x + node, min.y + node);
                if (i + 1 < nodes)
                {
                    const ImVec4& lineColor =
                        i < done ? kPathColors[static_cast<std::size_t>(i) % kPathColors.size()] : Theme::kBorderStrong;
                    drawList->AddRectFilled(
                        ImVec2(x + slot - (slot - node) * 0.5f + 4.0f * s, min.y + node * 0.5f - 1.5f * s),
                        ImVec2(x + slot + link + (slot - node) * 0.5f - 4.0f * s, min.y + node * 0.5f + 1.5f * s),
                        U32(lineColor), 2.0f * s);
                }
                ImGui::SetCursorScreenPos(min);
                ImGui::PushID(i);
                const bool clicked = ImGui::InvisibleButton("##node", ImVec2(node, node), ImGuiButtonFlags_EnableNav);
                const bool hovered = ImGui::IsItemHovered();
                const ImVec4& color = kPathColors[static_cast<std::size_t>(i) % kPathColors.size()];
                const bool finished = i < done;
                const bool locked = !unlocked(i);
                const bool current = i == next && !locked;
                drawList->AddRectFilled(min, max, U32(finished || current ? color : Theme::kRaised), 16.0f * s);
                Theme::FocusRing();
                if (current)
                {
                    drawList->AddRect(ImVec2(min.x - 6.0f * s, min.y - 6.0f * s),
                                      ImVec2(max.x + 6.0f * s, max.y + 6.0f * s), U32(Theme::kText), 20.0f * s,
                                      ImDrawFlags_None, 3.0f * s);
                }
                else if (hovered && !locked)
                {
                    drawList->AddRect(min, max, U32(Theme::kTextMuted), 16.0f * s, ImDrawFlags_None, 2.0f * s);
                }
                const ImVec2 center(min.x + node * 0.5f, min.y + node * 0.5f);
                if (finished)
                {
                    Theme::DrawIcon(drawList, Theme::Icon::Check, center, 26.0f * s, U32(Theme::kInk));
                }
                else if (locked)
                {
                    // A padlock: its body, and the shackle over it
                    const float half = 9.0f * s;
                    const ImU32 ink = U32(Theme::kTextMuted);
                    drawList->AddRectFilled(ImVec2(center.x - half, center.y - 2.0f * s),
                                            ImVec2(center.x + half, center.y + 11.0f * s), ink, 3.0f * s);
                    drawList->PathArcTo(ImVec2(center.x, center.y - 3.0f * s), 6.0f * s, 3.14159265f, 6.2831853f, 12);
                    drawList->PathStroke(ink, ImDrawFlags_None, 2.5f * s);
                }
                else
                {
                    ImGui::PushFont(Theme::BoldFont(), 18.0f);
                    const std::string number = std::to_string(i + 1);
                    const ImVec2 size = ImGui::CalcTextSize(number.c_str());
                    drawList->AddText(ImVec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f),
                                      U32(current ? Theme::kInk : Theme::kTextMuted), number.c_str());
                    ImGui::PopFont();
                }
                // Its name under it, cut to the slot
                ImGui::PushFont(current ? Theme::BoldFont() : nullptr, 14.0f);
                const std::string& name = mode.tutorials[static_cast<std::size_t>(i)].name;
                const float nameWidth = std::min(ImGui::CalcTextSize(name.c_str()).x, slot);
                const ImVec2 at(x + (slot - nameWidth) * 0.5f, max.y + 10.0f * s);
                drawList->PushClipRect(ImVec2(x, at.y), ImVec2(x + slot, at.y + ImGui::GetFontSize() * 1.5f), true);
                drawList->AddText(at, U32(finished || current ? Theme::kText : Theme::kTextMuted), name.c_str());
                drawList->PopClipRect();
                ImGui::PopFont();

                if (clicked && !locked)
                {
                    openLesson(i);
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    ImGui::SetTooltip(locked ? "%s\nLocked: finish what comes before it first" : "%s", name.c_str());
                }
                if (advanced && ImGui::BeginPopupContextItem("##LessonMenu"))
                {
                    if (ImGui::MenuItem("Remove"))
                    {
                        removeIndex = i;
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
            ImGui::SetCursorScreenPos(start);
            ImGui::Dummy(ImVec2(pathWidth, node + ringRoom + 40.0f * s));
        }
        ImGui::EndChild();
        if (removeIndex >= 0)
        {
            mode.tutorials.erase(mode.tutorials.begin() + removeIndex);
            if (removeIndex < mode.done)
            {
                --mode.done;
            }
            if (openTutorial_ == removeIndex)
            {
                openTutorial_ = -1;
            }
            modesDirty_ = true;
        }

        // The games made so far, newest choice first
        ImGui::Dummy(ImVec2(0.0f, 20.0f * s));
        Theme::Text("Jump back in", 20.0f, Theme::kText, true);
        ImGui::Spacing();
        // The learner's own games: the lesson game is reached from Learn
        if (FirstOwnGame().empty())
        {
            Theme::Text("No games yet. Make one with New game.", 15.0f, Theme::kTextMuted);
        }
        const float gap = 16.0f * s;
        const int columns = std::max(1, static_cast<int>((width + gap) / (260.0f * s + gap)));
        const float tileWidth = (width - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
        const float thumbnail = 120.0f * s;
        const float tileHeight = thumbnail + 64.0f * s;
        std::filesystem::path open;
        std::size_t tiles = 0;
        for (std::size_t i = 0; i < projects_.size(); ++i)
        {
            const std::filesystem::path& project = projects_[i];
            if (IsLessonGame(project))
            {
                continue;
            }
            if (tiles++ % static_cast<std::size_t>(columns) != 0)
            {
                ImGui::SameLine(0.0f, gap);
            }
            ImGui::PushID(static_cast<int>(i));
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max(min.x + tileWidth, min.y + tileHeight);
            if (ImGui::InvisibleButton("##game", ImVec2(tileWidth, tileHeight), ImGuiButtonFlags_EnableNav))
            {
                open = project;
            }
            if (ImGui::BeginPopupContextItem("##GameMenu"))
            {
                ImGui::TextDisabled("%s", project.filename().string().c_str());
                ImGui::Separator();
                if (ImGui::MenuItem("Open"))
                {
                    open = project;
                }
                if (ImGui::MenuItem("Delete Game..."))
                {
                    deleteGame_ = project;
                    openDeleteGame_ = true;
                }
                ImGui::EndPopup();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("Click to open; right-click to delete");
            }
            const bool hovered = ImGui::IsItemHovered();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float rounding = 18.0f * s;
            drawList->AddRectFilled(min, max, U32(Theme::kPanel), rounding);
            const ImVec2 thumbMax(max.x, min.y + thumbnail);
            if (const Thumbnail& picture = ThumbnailOf(project); picture.texture != 0)
            {
                // The game as it was last seen in the editor, cut to fill the tile (OpenGL rows go bottom up)
                const float tileAspect = tileWidth / thumbnail;
                const float pictureAspect = static_cast<float>(picture.width) / static_cast<float>(picture.height);
                const float keepX = pictureAspect > tileAspect ? tileAspect / pictureAspect : 1.0f;
                const float keepY = pictureAspect > tileAspect ? 1.0f : pictureAspect / tileAspect;
                drawList->AddImageRounded(static_cast<ImTextureID>(picture.texture), min, thumbMax,
                                          ImVec2(0.5f - keepX * 0.5f, 0.5f + keepY * 0.5f),
                                          ImVec2(0.5f + keepX * 0.5f, 0.5f - keepY * 0.5f), IM_COL32_WHITE, rounding,
                                          ImDrawFlags_RoundCornersTop);
            }
            else
            {
                // No picture yet (it is taken when the game is saved): a little scene in the game's own colour
                const ImVec4& accent = kPathColors[i % kPathColors.size()];
                drawList->AddRectFilled(min, thumbMax, U32(Theme::Hex(0xCDEBFF)), rounding,
                                        ImDrawFlags_RoundCornersTop);
                drawList->AddQuadFilled(
                    ImVec2(min.x, thumbMax.y), ImVec2(min.x + tileWidth * 0.27f, min.y + thumbnail * 0.5f),
                    ImVec2(min.x + tileWidth * 0.73f, min.y + thumbnail * 0.5f), thumbMax, U32(Theme::Hex(0xA8E6B5)));
                const float cx = min.x + tileWidth * 0.5f;
                drawList->AddRectFilled(ImVec2(cx - 13.0f * s, min.y + thumbnail * 0.52f),
                                        ImVec2(cx + 13.0f * s, min.y + thumbnail * 0.77f), U32(accent), 7.0f * s);
                drawList->AddCircleFilled(ImVec2(cx + tileWidth * 0.17f, min.y + thumbnail * 0.67f), 8.0f * s,
                                          U32(Theme::kEvents));
                drawList->AddCircleFilled(ImVec2(cx - tileWidth * 0.18f, min.y + thumbnail * 0.73f), 8.0f * s,
                                          U32(Theme::kEvents));
            }
            drawList->AddRect(min, max, U32(hovered ? Theme::kSelect : Theme::kBorder), rounding, ImDrawFlags_None,
                              hovered ? 2.0f * s : 1.0f);
            ImGui::PushFont(Theme::BoldFont(), 17.0f);
            drawList->AddText(ImVec2(min.x + 16.0f * s, thumbMax.y + 12.0f * s), U32(Theme::kText),
                              project.filename().string().c_str());
            ImGui::PopFont();
            ImGui::PushFont(nullptr, 14.0f);
            drawList->AddText(ImVec2(min.x + 16.0f * s, thumbMax.y + 36.0f * s), U32(Theme::kTextMuted),
                              project == projectDir_ ? "Open now \xC2\xB7 click to go back to it" : "Click to open");
            ImGui::PopFont();
            Theme::FocusRing();
            ImGui::PopID();
        }
        if (!open.empty())
        {
            if (open != projectDir_)
            {
                OpenProject(open);
            }
            view_ = EditorView::Workspace;
        }
        ImGui::Dummy(ImVec2(0.0f, 40.0f * s));
        ImGui::PopTextWrapPos();
        ImGui::EndGroup();
        ImGui::EndChild();
    }

    // ---------------------------------------------------------------- Simple interface

    // The Blocks editor on the left, the Viewport over Objects (or Assets, or the lesson) on the right: each a card on
    // the ground, as in the design
    void SceneEditor::DrawSimpleWorkspace()
    {
        const float s = uiScale_;
        const float gap = 10.0f * s;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 area(avail.x, std::max(avail.y - ImGui::GetFrameHeight(), 1.0f));

        // Keep the Advanced interface's docked layout while it is hidden; the Controls window is not shown here
        ImGui::DockSpace(ImHashStr(kWorkspaceDock), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_KeepAliveOnly);
        controls_->CancelListening();
        viewportOwnsMouse_ = false;

        const ImVec2 inner(area.x - gap * 2.0f, area.y - gap * 2.0f);
        const ImVec2 start(origin.x + gap, origin.y + gap);
        constexpr ImGuiWindowFlags kFixed = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (viewportMaximized_)
        {
            ImGui::SetCursorScreenPos(start);
            Card("##SimpleViewport", inner, 6.0f * s, kFixed, [this] { DrawViewport(); });
        }
        else
        {
            // Like Scratch: the Blocks editor down the left; on the right the view (16:9 at the column's width, with
            // its toolbar) over the Objects / Assets list. No Inspector: the selected object is changed in the view
            // (its gizmo, and its bar along the bottom) and from its menu.
            const float rightWidth =
                std::clamp(inner.x * 0.42f, 360.0f * s, std::max(360.0f * s, inner.x - 480.0f * s));
            const float blocksWidth = std::max(inner.x - rightWidth - gap, 200.0f * s);
            const float toolbar = 52.0f * s;
            const float viewportHeight =
                std::clamp(rightWidth * 9.0f / 16.0f + toolbar, 200.0f * s, std::max(200.0f * s, inner.y - 220.0f * s));
            const float rightX = start.x + blocksWidth + gap;

            ImGui::SetCursorScreenPos(start);
            Card("##SimpleLogic", ImVec2(blocksWidth, inner.y), 10.0f * s, kFixed,
                 [this]
                 {
                     // Blocks glow only in Play mode: the edit-mode runtime does not run them
                     blocks_->SetScale(uiScale_);
                     GameObject* selected = scene_.Find(scene_.Selected());
                     blocks_->SetTarget(selected, playing_ && runtime_ ? &runtime_->Blocks() : nullptr);
                     blocks_->SetBreakpoints(BreakpointsOf(scene_.Selected()));
                     blocks_->SetPreviewLook(projectDir_,
                                             selected ? Panels::LookOf(scene_.Description(), *selected) : nullptr,
                                             selected ? Panels::AnimatorOf(scene_.Description(), *selected) : nullptr);
                     blocks_->DrawContents();
                     TourMark("palette", blocks_->PaletteMin(), blocks_->PaletteMax());
                     TourMark("canvas", blocks_->CanvasMin(), blocks_->CanvasMax());
                 });
            ImGui::SetCursorScreenPos(ImVec2(rightX, start.y));
            Card("##SimpleViewport", ImVec2(rightWidth, viewportHeight), 6.0f * s, kFixed, [this] { DrawViewport(); });
            TourMark("viewport", ImVec2(rightX, start.y), ImVec2(rightX + rightWidth, start.y + viewportHeight));
            ImGui::SetCursorScreenPos(ImVec2(rightX, start.y + viewportHeight + gap));
            Card("##SimpleLeft", ImVec2(rightWidth, inner.y - viewportHeight - gap), 12.0f * s, kFixed,
                 [this] { DrawSimpleObjects(); });
            TourMark("objects", ImVec2(rightX, start.y + viewportHeight + gap),
                     ImVec2(rightX + rightWidth, start.y + inner.y));
        }
        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + area.y));
        DrawStatusBar();
    }

    void SceneEditor::DrawSimpleObjects()
    {
        const float s = uiScale_;
        const auto& tutorials = modes_[activeMode_].tutorials;
        const bool lessonOpen = openTutorial_ >= 0 && openTutorial_ < static_cast<int>(tutorials.size());
        if (!lessonOpen && simpleLeftTab_ == 3)
        {
            simpleLeftTab_ = 0;
        }
        // Changing a saved object on its own: the way back to the scene, and Save
        if (InPrefabMode())
        {
            DrawSimpleSavedObjectBar();
        }
        static constexpr std::array<const char*, 4> kTabs{"Objects", "Assets", "Scenes", "Lesson"};
        Theme::Segmented("##LeftTabs", kTabs.data(), lessonOpen ? 4 : 3, simpleLeftTab_,
                         ImGui::GetContentRegionAvail().x, 40.0f * s);
        ImGui::Spacing();
        if (simpleLeftTab_ == 1)
        {
            ImGui::BeginChild("##SimpleAssets");
            DrawSimpleStuff();
            ImGui::EndChild();
            return;
        }
        if (simpleLeftTab_ == 2)
        {
            ImGui::BeginChild("##SimpleScenes");
            DrawScenes();
            ImGui::EndChild();
            return;
        }
        if (simpleLeftTab_ == 3)
        {
            DrawLesson();
            return;
        }

        const float addHeight = 44.0f * s;
        std::function<void()> deferred;
        ImGui::BeginChild("##SimpleObjectList", ImVec2(0.0f, std::max(ImGui::GetContentRegionAvail().y - addHeight -
                                                                          ImGui::GetStyle().ItemSpacing.y,
                                                                      1.0f)));
        const Engine::Scene::SceneDescription& description = scene_.Description();
        // One row per object, those under another indented below it; click to select, double-click to look at it,
        // right-click for Rename / Duplicate / Delete, drop an asset on it to use it
        const auto row = [&](const auto& self, std::uint32_t id, int depth) -> void
        {
            GameObject* object = scene_.Find(id);
            if (!object)
            {
                return;
            }
            ImGui::PushID(static_cast<int>(id));
            const float height = 40.0f * s;
            const float width = ImGui::GetContentRegionAvail().x;
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max(min.x + width, min.y + height);
            const bool selected = scene_.Selected() == id;
            const bool renaming = renamingId_ == id;
            ImGui::SetNextItemAllowOverlap();
            // Enter / Space on the focused row selects it too
            const bool pressed = ImGui::InvisibleButton("##row", ImVec2(width, height), ImGuiButtonFlags_EnableNav);
            const ImVec2 next = ImGui::GetCursorScreenPos();
            const bool hovered = ImGui::IsItemHovered();
            const ImGuiID rowId = ImGui::GetItemID();
            if (pressed)
            {
                scene_.Select(id);
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                scene_.Select(id);
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    FocusSelected();
                }
            }
            ImGui::OpenPopupOnItemClick("##ObjectMenu", ImGuiPopupFlags_MouseButtonRight);
            if (const auto dropped = Panels::AcceptAnyAssetDrop())
            {
                UseAsset(*object, *dropped);
            }
            // Drag a row onto another to put it in that group (it stays where it is in the world)
            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload(kObjectPayload, &id, sizeof(id));
                ImGui::Text("Put %s in a group...", object->name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kObjectPayload))
                {
                    std::uint32_t dragged = 0;
                    std::memcpy(&dragged, payload->Data, sizeof(dragged));
                    if (dragged != id)
                    {
                        deferred = [this, dragged, id] { ReparentObject(dragged, id); };
                    }
                }
                ImGui::EndDragDropTarget();
            }
            const auto children = Engine::Scene::ChildrenOf(description, id);
            const bool collapsed =
                std::find(collapsedGroups_.begin(), collapsedGroups_.end(), id) != collapsedGroups_.end();

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float rounding = 10.0f * s;
            if (selected)
            {
                drawList->AddRectFilled(min, max, U32(Theme::kRaisedHigh), rounding);
                drawList->AddRect(min, max, U32(Theme::kSelect), rounding, ImDrawFlags_None, 2.0f * s);
            }
            else if (hovered)
            {
                drawList->AddRectFilled(min, max, U32(Theme::kRaised), rounding);
            }
            ImGui::RenderNavCursor(ImRect(min, max), rowId);
            const float icon = 22.0f * s;
            const float iconX = min.x + 22.0f * s + static_cast<float>(depth) * 16.0f * s;
            DrawObjectIcon(drawList, *object, ImVec2(iconX, min.y + (height - icon) * 0.5f), icon);
            if (!children.empty())
            {
                // A group: an arrow to fold it (down when open), and a click on the arrow folds or unfolds it
                const float a = 5.0f * s;
                const ImVec2 c(iconX - 11.0f * s, min.y + height * 0.5f);
                if (collapsed)
                {
                    drawList->AddTriangleFilled(ImVec2(c.x - a * 0.6f, c.y - a), ImVec2(c.x - a * 0.6f, c.y + a),
                                                ImVec2(c.x + a * 0.8f, c.y), U32(Theme::kTextSoft));
                }
                else
                {
                    drawList->AddTriangleFilled(ImVec2(c.x - a, c.y - a * 0.6f), ImVec2(c.x + a, c.y - a * 0.6f),
                                                ImVec2(c.x, c.y + a * 0.8f), U32(Theme::kTextSoft));
                }
                if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsDragDropActive() &&
                    std::abs(ImGui::GetIO().MousePos.x - c.x) < 10.0f * s)
                {
                    if (collapsed)
                    {
                        std::erase(collapsedGroups_, id);
                    }
                    else
                    {
                        collapsedGroups_.push_back(id);
                    }
                }
            }
            const float nameX = iconX + icon + 10.0f * s;

            // A badge for the scripts it runs
            float badgeLeft = max.x - 8.0f * s;
            if (const std::size_t scripts = RunningScripts(*object); scripts > 0)
            {
                ImGui::PushFont(Theme::BoldFont(), 11.0f);
                const std::string badge = std::format("{} SCRIPT{}", scripts, scripts == 1 ? "" : "S");
                const ImVec2 size = ImGui::CalcTextSize(badge.c_str());
                const ImVec2 badgeMin(max.x - size.x - 22.0f * s, min.y + (height - size.y - 6.0f * s) * 0.5f);
                drawList->AddRectFilled(badgeMin, ImVec2(max.x - 8.0f * s, badgeMin.y + size.y + 6.0f * s),
                                        U32(Theme::kEvents), 999.0f);
                drawList->AddText(ImVec2(badgeMin.x + 7.0f * s, badgeMin.y + 3.0f * s), U32(Theme::kInk),
                                  badge.c_str());
                ImGui::PopFont();
                badgeLeft = badgeMin.x - 6.0f * s;
            }

            if (renaming)
            {
                // F2 / Rename: typed over the name; Enter or clicking away keeps it, Esc puts the old one back
                ImGui::SetCursorScreenPos(ImVec2(nameX, min.y + (height - ImGui::GetFrameHeight()) * 0.5f));
                ImGui::SetNextItemWidth(std::max(badgeLeft - nameX, 40.0f * s));
                if (focusRename_)
                {
                    ImGui::SetKeyboardFocusHere();
                    focusRename_ = false;
                }
                ImGui::InputText("##rename", renameBuffer_.data(), renameBuffer_.size(),
                                 ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
                if (ImGui::IsItemDeactivated())
                {
                    if (renameBuffer_[0] != '\0')
                    {
                        object->name = renameBuffer_.data();
                    }
                    renamingId_ = 0;
                }
                ImGui::SetCursorScreenPos(next);
                ImGui::Dummy(ImVec2(0.0f, 0.0f));
            }
            else
            {
                ImGui::PushFont(selected ? Theme::BoldFont() : nullptr, 15.0f);
                drawList->PushClipRect(ImVec2(nameX, min.y), ImVec2(badgeLeft, max.y), true);
                drawList->AddText(ImVec2(nameX, min.y + (height - ImGui::GetFontSize()) * 0.5f), U32(Theme::kText),
                                  object->name.c_str());
                drawList->PopClipRect();
                ImGui::PopFont();
            }

            if (ImGui::BeginPopup("##ObjectMenu"))
            {
                scene_.Select(id);
                DrawSimpleObjectMenu(*object, deferred);
                ImGui::EndPopup();
            }

            if (!collapsed)
            {
                for (const std::uint32_t child : children)
                {
                    self(self, child, depth + 1);
                }
            }
            ImGui::PopID();
        };
        for (const std::uint32_t id : Engine::Scene::ChildrenOf(description, 0))
        {
            row(row, id, 0);
        }
        if (scene_.Objects().empty())
        {
            Theme::EmptyState("Nothing here yet. Add an object to start building.", 14.0f);
        }
        // The empty space below the rows: a click lets go of the selection, a row dropped here leaves its group
        const ImVec2 rest(ImGui::GetContentRegionAvail().x,
                          std::max(ImGui::GetContentRegionAvail().y, ImGui::GetFrameHeight()));
        ImGui::InvisibleButton("##rest", ImVec2(std::max(rest.x, 1.0f), rest.y));
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            scene_.Select(0);
        }
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kObjectPayload))
            {
                std::uint32_t dragged = 0;
                std::memcpy(&dragged, payload->Data, sizeof(dragged));
                deferred = [this, dragged] { ReparentObject(dragged, 0); };
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::IsWindowFocused() && !ImGui::GetIO().WantTextInput && scene_.Selected() != 0)
        {
            const std::uint32_t selected = scene_.Selected();
            if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            {
                deferred = [this, selected] { DeleteObject(selected); };
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
            {
                renamingId_ = selected;
                std::fill(renameBuffer_.begin(), renameBuffer_.end(), '\0');
                scene_.Find(selected)->name.copy(renameBuffer_.data(), renameBuffer_.size() - 1);
                focusRename_ = true;
            }
        }
        ImGui::EndChild();
        if (deferred)
        {
            deferred();
        }

        const ImVec2 addAt = ImGui::GetCursorScreenPos();
        TourMark("add", addAt, ImVec2(addAt.x + ImGui::GetContentRegionAvail().x, addAt.y + addHeight));
        if (Theme::PillButton("Add object", Theme::kRaised, Theme::kText, addHeight, ImGui::GetContentRegionAvail().x,
                              true, Theme::Icon::Plus))
        {
            ImGui::OpenPopup("##SimpleCreate");
        }
        if (ImGui::BeginPopup("##SimpleCreate"))
        {
            CreateObjectOf(DrawCreateMenuItems(), 0);
            ImGui::EndPopup();
        }
    }

    // The object menu (right-click it in the Objects list or in the view): what the Inspector did, for one object.
    // What adds or removes objects goes in deferred, run once the list or the view is drawn.
    void SceneEditor::DrawSimpleObjectMenu(GameObject& object, std::function<void()>& deferred)
    {
        const std::uint32_t id = object.id;
        const Engine::Scene::SceneDescription& description = scene_.Description();
        const bool hasChildren = !Engine::Scene::ChildrenOf(description, id).empty();
        ImGui::TextDisabled("%s", ObjectKind(object));
        ImGui::Separator();
        if (ImGui::MenuItem("Look at it", "F"))
        {
            FocusSelected();
        }
        if (ImGui::MenuItem("Rename", "F2"))
        {
            // Typed over its name in the Objects list
            simpleLeftTab_ = 0;
            viewportMaximized_ = false;
            renamingId_ = id;
            std::fill(renameBuffer_.begin(), renameBuffer_.end(), '\0');
            object.name.copy(renameBuffer_.data(), renameBuffer_.size() - 1);
            focusRename_ = true;
        }
        if (ImGui::MenuItem("Change its parts..."))
        {
            simplePartsOpen_ = true;
        }
        if (ImGui::BeginMenu("Add object inside"))
        {
            const CreateKind kind = DrawCreateMenuItems();
            if (kind != CreateKind::None)
            {
                deferred = [this, kind, id] { CreateObjectOf(kind, id); };
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Duplicate"))
        {
            deferred = [this, id] { DuplicateObject(id); };
        }
        if (ImGui::MenuItem("Delete", "Del"))
        {
            deferred = [this, id] { DeleteObject(id); };
        }
        ImGui::Separator();
        // Groups: objects that move, and are controlled, together
        if (ImGui::MenuItem("Group"))
        {
            deferred = [this, id] { GroupObject(id); };
        }
        if (hasChildren && ImGui::MenuItem("Ungroup"))
        {
            deferred = [this, id] { UngroupObject(id); };
        }
        if (object.parent != 0 && ImGui::MenuItem("Take out of group"))
        {
            const GameObject* parent = scene_.Find(object.parent);
            const std::uint32_t outside = parent ? parent->parent : 0;
            deferred = [this, id, outside] { ReparentObject(id, outside); };
        }
        ImGui::Separator();
        if (!object.prefabInstance)
        {
            if (ImGui::MenuItem("Save for reuse...", nullptr, false, !projectDir_.empty() && !InPrefabMode()))
            {
                BeginSavePrefab(id);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip("Keep it in Assets, to add copies of it: change the saved one and every copy "
                                  "changes");
            }
            return;
        }
        const std::uint32_t root = Engine::Scene::InstanceRootOf(description, id);
        const GameObject* rootObject = scene_.Find(root);
        const std::string path = rootObject && rootObject->prefabInstance ? rootObject->prefabInstance->prefab
                                                                          : object.prefabInstance->prefab;
        // A copy of a saved object (a prefab instance)
        ImGui::TextDisabled("A copy of %s", std::filesystem::path(path).stem().string().c_str());
        if (ImGui::MenuItem("Change the saved object"))
        {
            deferred = [this, path] { OpenPrefab(path); };
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("Opens it on its own: what you change there, every copy takes");
        }
        // What this copy changed: into the saved object, for every copy, or back to the saved object's
        const bool changed = !object.prefabInstance->overrides.empty() && prefabs_.PrefabObjectOf(object) != nullptr;
        if (ImGui::MenuItem("Make every copy like this one", nullptr, false, changed))
        {
            deferred = [this, id] { ApplyPrefab(id); };
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip(changed ? "Saves what you changed on this copy into the saved object"
                                      : "This copy is the same as the saved object");
        }
        if (ImGui::MenuItem("Make it like the saved one again", nullptr, false, changed))
        {
            deferred = [this, id] { prefabs_.Revert(scene_.Description(), id, std::string{}); };
        }
        if (root != id && ImGui::MenuItem("Select the whole copy"))
        {
            scene_.Select(root);
        }
        if (ImGui::MenuItem("Make it a normal object"))
        {
            deferred = [this, id]
            {
                Engine::Game::PrefabManager::Unpack(scene_.Description(), id);
                SetStatus("It is a normal object now: it no longer changes with the saved object");
            };
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        {
            ImGui::SetTooltip("Keeps everything it has, but stops being a copy");
        }
    }

    // The selected object's bar, small and centred along the bottom of the view, in place of an Inspector: what it
    // is, its colour (a swatch that opens the colours), and buttons for its parts and its menu. Where it is, how it is
    // turned and how big are the gizmo's (W / E / R, Snap on the toolbar). The bar is a window of its own, so the view
    // does not take its clicks for picking.
    void SceneEditor::DrawSimpleObjectBar(const ImVec2& position, const ImVec2& size)
    {
        GameObject* object = scene_.Find(scene_.Selected());
        const float s = uiScale_;
        const float height = 48.0f * s;
        const float margin = 10.0f * s;
        if (!object || size.x < 240.0f * s || size.y < height + margin * 4.0f)
        {
            simplePartsOpen_ = false;
            return;
        }

        // Its width: what is in it, with a long name cut short
        const float padding = 8.0f * s;
        const float button = height - padding * 2.0f;
        const float icon = 24.0f * s;
        const bool hasColour = object->renderable || object->light || object->screen;
        const char* partsLabel = "Parts";
        ImGui::PushFont(Theme::BoldFont(), 14.0f);
        const float nameWidth = std::min(ImGui::CalcTextSize(object->name.c_str()).x, 140.0f * s);
        ImGui::PopFont();
        const float width = std::min(padding * 2.0f + icon + 8.0f * s + std::max(nameWidth, 60.0f * s) + 12.0f * s +
                                         (hasColour ? button + 6.0f * s : 0.0f) + Theme::PillWidth(partsLabel, button) +
                                         6.0f * s + button,
                                     size.x - margin * 2.0f);

        std::function<void()> deferred;
        ImGui::SetCursorScreenPos(ImVec2(position.x + (size.x - width) * 0.5f, position.y + size.y - height - margin));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f * s);
        Card(
            "##ObjectBar", ImVec2(width, height), padding,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse,
            [&]
            {
                const float top = ImGui::GetCursorPosY();
                const auto centred = [&](float itemHeight)
                { ImGui::SetCursorPosY(top + (button - itemHeight) * 0.5f); };
                ImDrawList* drawList = ImGui::GetWindowDrawList();

                // What it is: its picture, its name and kind (or the group it heads)
                centred(icon);
                DrawObjectIcon(drawList, *object, ImGui::GetCursorScreenPos(), icon);
                ImGui::Dummy(ImVec2(icon, icon));
                ImGui::SameLine(0.0f, 8.0f * s);
                centred(button);
                const ImVec2 textAt = ImGui::GetCursorScreenPos();
                const float textWidth = std::max(nameWidth, 60.0f * s);
                ImGui::Dummy(ImVec2(textWidth, button));
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && nameWidth >= 140.0f * s)
                {
                    ImGui::SetTooltip("%s", object->name.c_str());
                }
                drawList->PushClipRect(textAt, ImVec2(textAt.x + textWidth, textAt.y + button), true);
                ImGui::PushFont(Theme::BoldFont(), 14.0f);
                drawList->AddText(ImVec2(textAt.x, textAt.y + button * 0.5f - ImGui::GetFontSize()), U32(Theme::kText),
                                  object->name.c_str());
                ImGui::PopFont();
                const std::size_t members = Engine::Scene::SubtreeOf(scene_.Description(), object->id).size() - 1;
                const std::string kind =
                    members > 0 ? std::format("A group of {}", members) : std::string(ObjectKind(*object));
                ImGui::PushFont(nullptr, 12.0f);
                drawList->AddText(ImVec2(textAt.x, textAt.y + button * 0.5f + 1.0f * s), U32(Theme::kTextMuted),
                                  kind.c_str());
                ImGui::PopFont();
                drawList->PopClipRect();

                // Its colour: one swatch of the colour it has, which opens the colours to pick from
                if (hasColour)
                {
                    ImGui::SameLine(0.0f, 12.0f * s);
                    centred(button);
                    const ImVec2 min = ImGui::GetCursorScreenPos();
                    if (ImGui::InvisibleButton("##colour", ImVec2(button, button), ImGuiButtonFlags_EnableNav))
                    {
                        ImGui::OpenPopup("##Colours");
                    }
                    const ImVec4 colour =
                        object->renderable ? ImVec4(object->renderable->albedoR, object->renderable->albedoG,
                                                    object->renderable->albedoB, 1.0f)
                        : object->light
                            ? ImVec4(object->light->colorR, object->light->colorG, object->light->colorB, 1.0f)
                            : ImVec4(object->screen->colorR, object->screen->colorG, object->screen->colorB, 1.0f);
                    const float inset = 4.0f * s;
                    drawList->AddRectFilled(ImVec2(min.x + inset, min.y + inset),
                                            ImVec2(min.x + button - inset, min.y + button - inset), U32(colour),
                                            8.0f * s);
                    drawList->AddRect(ImVec2(min.x + inset, min.y + inset),
                                      ImVec2(min.x + button - inset, min.y + button - inset),
                                      U32(ImGui::IsItemHovered() ? Theme::kSelect : Theme::kBorderStrong), 8.0f * s,
                                      ImDrawFlags_None, 1.5f * s);
                    Theme::FocusRing();
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                    {
                        ImGui::SetTooltip(object->light ? "Its colour and brightness" : "Its colour");
                    }
                }

                // On the right: its parts (sound, player controls, camera, ...) and its menu
                ImGui::SameLine(0.0f, 6.0f * s);
                centred(button);
                if (Theme::PillButton(partsLabel, Theme::kRaised, Theme::kText, button) || simplePartsOpen_)
                {
                    simplePartsOpen_ = false;
                    ImGui::OpenPopup("##Parts");
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                {
                    ImGui::SetTooltip("Its looks, sound, player controls, camera, light and health; add or take away "
                                      "parts");
                }
                ImGui::SameLine(0.0f, 6.0f * s);
                centred(button);
                if (Theme::IconButton("##objectMenu", Theme::Icon::More, button, "More"))
                {
                    ImGui::OpenPopup("##BarMenu");
                }
                if (ImGui::BeginPopup("##BarMenu"))
                {
                    DrawSimpleObjectMenu(*object, deferred);
                    ImGui::EndPopup();
                }

                // The popups open over the view, just above the bar
                const ImVec2 barAt = ImGui::GetWindowPos();
                const ImVec2 above(barAt.x + ImGui::GetWindowSize().x * 0.5f, barAt.y - 8.0f * s);
                ImGui::SetNextWindowPos(above, ImGuiCond_Appearing, ImVec2(0.5f, 1.0f));
                if (ImGui::BeginPopup("##Colours"))
                {
                    if (object->renderable)
                    {
                        Engine::Scene::RenderableDesc& looks = *object->renderable;
                        static constexpr std::array<std::array<float, 3>, 7> kSwatches{{{1.0f, 1.0f, 1.0f},
                                                                                        {0.90f, 0.30f, 0.27f},
                                                                                        {0.96f, 0.62f, 0.22f},
                                                                                        {0.98f, 0.85f, 0.30f},
                                                                                        {0.40f, 0.78f, 0.38f},
                                                                                        {0.32f, 0.56f, 0.92f},
                                                                                        {0.62f, 0.42f, 0.86f}}};
                        Theme::Label("COLOUR");
                        const float swatch = 26.0f * s;
                        ImDrawList* popupList = ImGui::GetWindowDrawList();
                        for (std::size_t i = 0; i < kSwatches.size(); ++i)
                        {
                            const auto& [r, g, b] = kSwatches[i];
                            ImGui::PushID(static_cast<int>(i));
                            const ImVec2 min = ImGui::GetCursorScreenPos();
                            if (ImGui::InvisibleButton("##swatch", ImVec2(swatch, swatch), ImGuiButtonFlags_EnableNav))
                            {
                                looks.albedoR = r;
                                looks.albedoG = g;
                                looks.albedoB = b;
                            }
                            const bool current = std::abs(looks.albedoR - r) < 0.01f &&
                                                 std::abs(looks.albedoG - g) < 0.01f &&
                                                 std::abs(looks.albedoB - b) < 0.01f;
                            const ImVec2 max(min.x + swatch, min.y + swatch);
                            popupList->AddRectFilled(min, max, U32(ImVec4(r, g, b, 1.0f)), 6.0f * s);
                            popupList->AddRect(min, max, U32(current ? Theme::kSelect : Theme::kBorder), 6.0f * s,
                                               ImDrawFlags_None, (current ? 2.5f : 1.0f) * s);
                            Theme::FocusRing();
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                            {
                                ImGui::SetTooltip(i == 0 ? "Plain" : "Paint it this colour");
                            }
                            ImGui::PopID();
                            ImGui::SameLine(0.0f, 4.0f * s);
                        }
                        float rgb[3] = {looks.albedoR, looks.albedoG, looks.albedoB};
                        if (ImGui::ColorEdit3("##paint", rgb,
                                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
                        {
                            looks.albedoR = rgb[0];
                            looks.albedoG = rgb[1];
                            looks.albedoB = rgb[2];
                        }
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                        {
                            ImGui::SetTooltip("Any colour");
                        }
                    }
                    // On the screen: its colour, and what it says
                    if (object->screen)
                    {
                        auto& element = *object->screen;
                        Theme::Label("ON THE SCREEN");
                        float rgb[3] = {element.colorR, element.colorG, element.colorB};
                        if (ImGui::ColorEdit3("##screenColour", rgb,
                                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
                        {
                            element.colorR = rgb[0];
                            element.colorG = rgb[1];
                            element.colorB = rgb[2];
                        }
                        if (element.kind != Engine::Scene::ScreenElementKind::Picture)
                        {
                            ImGui::SameLine(0.0f, 6.0f * s);
                            std::array<char, 256> words{};
                            element.text.copy(words.data(), words.size() - 1);
                            ImGui::SetNextItemWidth(200.0f * s);
                            if (ImGui::InputTextWithHint("##screenText", "What it says", words.data(), words.size()))
                            {
                                element.text = words.data();
                            }
                        }
                    }
                    // A light: its colour and how bright it is
                    if (object->light)
                    {
                        auto& light = *object->light;
                        Theme::Label("LIGHT");
                        float rgb[3] = {light.colorR, light.colorG, light.colorB};
                        if (ImGui::ColorEdit3("##light", rgb,
                                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
                        {
                            light.colorR = rgb[0];
                            light.colorG = rgb[1];
                            light.colorB = rgb[2];
                        }
                        ImGui::SameLine(0.0f, 6.0f * s);
                        ImGui::SetNextItemWidth(160.0f * s);
                        ImGui::SliderFloat("##brightness", &light.intensity, 0.0f, 10.0f, "Brightness %.1f",
                                           ImGuiSliderFlags_AlwaysClamp);
                        ImGui::SameLine(0.0f, 6.0f * s);
                        ImGui::SetNextItemWidth(160.0f * s);
                        ImGui::SliderFloat("##flicker", &light.flicker, 0.0f, 1.0f, "Flicker %.2f",
                                           ImGuiSliderFlags_AlwaysClamp);
                    }
                    ImGui::EndPopup();
                }

                // The parts
                ImGui::SetNextWindowPos(above, ImGuiCond_Appearing, ImVec2(0.5f, 1.0f));
                ImGui::SetNextWindowSizeConstraints(ImVec2(320.0f * s, 0.0f),
                                                    ImVec2(420.0f * s, std::max(size.y - height, 200.0f * s)));
                if (ImGui::BeginPopup("##Parts"))
                {
                    Theme::Text(object->name.c_str(), 16.0f, Theme::kText, true);
                    ImGui::Spacing();
                    // A copy of a saved object: which, and the way to change it (the rest is in its menu)
                    if (object->prefabInstance)
                    {
                        const std::string path = object->prefabInstance->prefab;
                        ImGui::TextColored(Theme::kSelect, "A copy of %s",
                                           std::filesystem::path(path).stem().string().c_str());
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Change the saved object"))
                        {
                            deferred = [this, path] { OpenPrefab(path); };
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::Separator();
                    }
                    DrawInspectorComponents(*object, true);
                    ImGui::EndPopup();
                }
            },
            Theme::kPanel);
        ImGui::PopStyleVar();
        if (deferred)
        {
            deferred();
        }
    }

    // The Assets tab: the saved objects first (what a learner makes), then the game's pictures, 3D models and sounds
    void SceneEditor::DrawSimpleStuff()
    {
        DrawSimpleSavedObjects();
        ImGui::Dummy(ImVec2(0.0f, 8.0f * uiScale_));
        Theme::Label("FILES");
        ImGui::Spacing();
        PrepareAssetBrowser();
        assetBrowser_.DrawSimpleContents();
    }

    // One row per saved object: a click on Add puts a copy in the scene (or drag it to where it should go), a
    // double-click opens it to change it, right-click for the rest
    void SceneEditor::DrawSimpleSavedObjects()
    {
        const float s = uiScale_;
        Theme::Label("SAVED OBJECTS");
        ImGui::Spacing();
        if (projectDir_.empty())
        {
            Theme::TextWrapped("Open a game to save objects for reuse.", 14.0f, Theme::kTextSoft);
            return;
        }
        if (prefabList_.empty())
        {
            Theme::TextWrapped("Nothing saved yet. Right-click an object and pick Save for reuse: then add copies of "
                               "it from here. Change the saved one, and every copy changes too.",
                               14.0f, Theme::kTextSoft);
            return;
        }

        // Done once the list is drawn: each changes the scene (or replaces it, to change a saved object)
        std::function<void()> deferred;
        const float height = 40.0f * s;
        for (const std::string& path : prefabList_)
        {
            ImGui::PushID(path.c_str());
            const std::string name = std::filesystem::path(path).stem().string();
            const std::size_t copies = Engine::Game::PrefabManager::InstancesOf(scene_.Description(), path).size();
            const bool isOpen = prefabMode_ && prefabMode_->path == path;
            const char* addLabel = "Add";
            const float addWidth = Theme::PillWidth(addLabel, height - 8.0f * s, true);
            const float width = ImGui::GetContentRegionAvail().x;
            const ImVec2 min = ImGui::GetCursorScreenPos();
            ImGui::SetNextItemAllowOverlap();
            ImGui::InvisibleButton("##saved", ImVec2(std::max(width, 1.0f), height), ImGuiButtonFlags_EnableNav);
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 next = ImGui::GetCursorScreenPos();
            if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !isOpen)
            {
                deferred = [this, path] { OpenPrefab(path); };
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && !ImGui::IsDragDropActive())
            {
                ImGui::SetTooltip("Drag it into the scene, or click Add. Double-click to change it: every copy "
                                  "changes too.");
            }
            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload(kPrefabPayload, path.c_str(), path.size() + 1);
                ImGui::Text("Add a copy of %s", name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginPopupContextItem("##SavedMenu"))
            {
                ImGui::TextDisabled("%s", name.c_str());
                ImGui::Separator();
                if (ImGui::MenuItem("Add a copy to the scene"))
                {
                    deferred = [this, path] { PlacePrefab(path); };
                }
                if (ImGui::MenuItem("Change it", nullptr, false, !isOpen))
                {
                    deferred = [this, path] { OpenPrefab(path); };
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete...", nullptr, false, !InPrefabMode()))
                {
                    deletePrefabPath_ = path;
                    showDeletePrefabPopup_ = true;
                }
                ImGui::EndPopup();
            }

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 max(min.x + width, min.y + height);
            if (hovered || isOpen)
            {
                drawList->AddRectFilled(min, max, U32(isOpen ? Theme::kRaisedHigh : Theme::kRaised), 10.0f * s);
            }
            Theme::FocusRing();
            // A box in the saved objects' blue, then the name and how many copies the scene has
            const float box = 18.0f * s;
            const ImVec2 boxMin(min.x + 10.0f * s, min.y + (height - box) * 0.5f);
            drawList->AddRectFilled(boxMin, ImVec2(boxMin.x + box, boxMin.y + box), U32(Theme::kSelect), 4.0f * s);
            const float textX = boxMin.x + box + 10.0f * s;
            const float textRight = max.x - addWidth - 12.0f * s;
            drawList->PushClipRect(ImVec2(textX, min.y), ImVec2(textRight, max.y), true);
            ImGui::PushFont(Theme::BoldFont(), 14.0f);
            drawList->AddText(ImVec2(textX, min.y + height * 0.5f - ImGui::GetFontSize()), U32(Theme::kText),
                              name.c_str());
            ImGui::PopFont();
            const std::string count =
                copies == 0 ? std::string("not in this scene") : std::format("{} in this scene", copies);
            ImGui::PushFont(nullptr, 12.0f);
            drawList->AddText(ImVec2(textX, min.y + height * 0.5f + 1.0f * s), U32(Theme::kTextMuted), count.c_str());
            ImGui::PopFont();
            drawList->PopClipRect();

            ImGui::SetCursorScreenPos(ImVec2(max.x - addWidth - 4.0f * s, min.y + 4.0f * s));
            if (Theme::PillButton(addLabel, Theme::kObjects, Theme::kInk, height - 8.0f * s, 0.0f, true,
                                  Theme::Icon::Plus))
            {
                deferred = [this, path] { PlacePrefab(path); };
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ImGui::SetTooltip(InPrefabMode() ? "Put a copy inside the one you are changing"
                                                 : "Put a copy in the scene");
            }
            ImGui::SetCursorScreenPos(next);
            ImGui::Dummy(ImVec2(0.0f, 2.0f * s));
            ImGui::PopID();
        }
        if (deferred)
        {
            deferred();
        }
    }

    void SceneEditor::DrawSimpleSavedObjectBar()
    {
        const float s = uiScale_;
        const float height = 36.0f * s;
        Card(
            "##SavedObjectBar", ImVec2(0.0f, 0.0f), 10.0f * s, ImGuiWindowFlags_NoScrollbar,
            [&]
            {
                Theme::Text(std::format("Changing {}{}", prefabMode_->name, HasUnsavedChanges() ? " *" : "").c_str(),
                            15.0f, Theme::kText, true);
                Theme::TextWrapped("Every copy of it changes too. Save, then go back to your scene.", 13.0f,
                                   Theme::kTextSoft);
                ImGui::Spacing();
                bool back = Theme::PillButton("Back to the scene", Theme::kRaised, Theme::kText, height, 0.0f, true,
                                              Theme::Icon::Back);
                ImGui::SameLine(0.0f, 6.0f * s);
                if (Theme::PillButton("Save", Theme::kObjects, Theme::kInk, height, 0.0f, false, Theme::Icon::Plus,
                                      HasUnsavedChanges()))
                {
                    SavePrefabMode();
                }
                if (back)
                {
                    ClosePrefab(); // asks about unsaved changes
                }
            },
            Theme::kRaised, false);
        ImGui::Spacing();
    }

    // The open lesson: its goal and notes, and the way to the next one
    void SceneEditor::DrawLesson()
    {
        const float s = uiScale_;
        Engine::Editor::TutorialMode& mode = modes_[activeMode_];
        std::vector<Tutorial>& tutorials = mode.tutorials;
        if (openTutorial_ < 0 || openTutorial_ >= static_cast<int>(tutorials.size()))
        {
            return;
        }
        const Tutorial& tutorial = tutorials[static_cast<std::size_t>(openTutorial_)];
        const int count = static_cast<int>(tutorials.size());
        Theme::Text(std::format("LESSON {} OF {}", openTutorial_ + 1, count).c_str(), 12.0f, Theme::kWarningText, true);
        ImGui::PushTextWrapPos(0.0f);
        Theme::Text(tutorial.name.c_str(), 20.0f, Theme::kText, true);
        ImGui::PopTextWrapPos();
        ImGui::Spacing();

        const float buttons = 44.0f * s;
        ImGui::BeginChild("##LessonNotes", ImVec2(0.0f, std::max(ImGui::GetContentRegionAvail().y - buttons * 2.0f -
                                                                     ImGui::GetStyle().ItemSpacing.y * 2.0f,
                                                                 1.0f)));
        Theme::TextWrapped(tutorial.code.empty() ? "This lesson has no notes yet." : tutorial.code.c_str(), 15.0f,
                           tutorial.code.empty() ? Theme::kTextMuted : Theme::kTextSoft);
        // The steps tick themselves off, in order: the next one to do stands out
        bool finished = true;
        if (!tutorial.steps.empty())
        {
            ImGui::Spacing();
            Theme::Label("YOUR GOALS");
            ImGui::Spacing();
            bool current = true;
            for (std::size_t i = 0; i < tutorial.steps.size(); ++i)
            {
                const bool met = i < lessonMet_.size() && lessonMet_[i];
                const bool now = !met && current;
                current = current && met;
                finished = finished && met;
                const float line = ImGui::GetTextLineHeight();
                const ImVec2 at = ImGui::GetCursorScreenPos();
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImVec2 centre(at.x + 11.0f * s, at.y + line * 0.5f + 2.0f * s);
                drawList->AddCircleFilled(centre, 10.0f * s,
                                          U32(met   ? Theme::kRun
                                              : now ? Theme::kEvents
                                                    : Theme::kGround));
                if (met)
                {
                    Theme::DrawIcon(drawList, Theme::Icon::Check, centre, 14.0f * s, U32(Theme::kInk));
                }
                ImGui::SetCursorScreenPos(ImVec2(at.x + 30.0f * s, at.y));
                ImGui::BeginGroup();
                ImGui::PushTextWrapPos(0.0f);
                Theme::Text(tutorial.steps[i].text.c_str(), 15.0f,
                            met   ? Theme::kTextMuted
                            : now ? Theme::kText
                                  : Theme::kTextSoft,
                            now);
                ImGui::PopTextWrapPos();
                ImGui::EndGroup();
                ImGui::Dummy(ImVec2(0.0f, 2.0f * s));
            }
            if (finished)
            {
                ImGui::Spacing();
                Theme::Text("Goal reached! Well done.", 16.0f, Theme::kSuccessText, true);
            }
        }
        ImGui::EndChild();

        const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (Theme::PillButton("Previous", Theme::kRaised, Theme::kText, buttons, half, false, Theme::Icon::Back,
                              openTutorial_ > 0))
        {
            --openTutorial_;
            lessonTicked_.clear();
        }
        ImGui::SameLine();
        // Next lesson (Finish on the last one) counts this lesson as done: progress on Home is lessons done in order
        const bool last = openTutorial_ + 1 >= count;
        if (Theme::PillButton(last ? "Finish" : "Next lesson", Theme::kEvents, Theme::kInk, buttons, half, true,
                              last ? Theme::Icon::Check : Theme::Icon::Arrow, finished))
        {
            mode.done = std::max(mode.done, openTutorial_ + 1);
            modesDirty_ = !modesShipped_;
            SaveModes();
            lessonTicked_.clear();
            if (last)
            {
                // On to the next track, as the world carries on from one to the next
                const bool another = activeMode_ + 1 < static_cast<int>(modes_.size()) &&
                                     !modes_[static_cast<std::size_t>(activeMode_) + 1].tutorials.empty();
                SetStatus(std::format("You finished {}. Well done!", mode.name));
                if (another)
                {
                    ++activeMode_;
                    openTutorial_ = 0;
                }
                else
                {
                    openTutorial_ = -1;
                    simpleLeftTab_ = 0;
                }
                return;
            }
            ++openTutorial_;
        }
        // Building is most of the first lessons: adding an object does not need leaving the goals
        if (Theme::PillButton("Add object", Theme::kRaised, Theme::kText, buttons, half, true, Theme::Icon::Plus))
        {
            ImGui::OpenPopup("##LessonCreate");
        }
        if (ImGui::BeginPopup("##LessonCreate"))
        {
            CreateObjectOf(DrawCreateMenuItems(), 0);
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        if (Theme::PillButton("Close lesson", Theme::kGround, Theme::kTextSoft, buttons, half))
        {
            openTutorial_ = -1;
            simpleLeftTab_ = 0;
        }
    }
}
