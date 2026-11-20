/* !
@file    	    ControlsPanel.cpp
@author  	    beh.s@digipen.edu [80%]
                sanni.go@digipen.edu [20%]
@date    		30/09/2026
@brief
    Implements ControlsPanel: loads the selected game's input actions, lists
    each action with its keys, binds keys by picking or pressing them, and saves
    the file so the new keys work straight away.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/panels/controls/ControlsPanel.hpp"

#include "editor/Theme.hpp"
#include "engine/game/GameProject.hpp"
#include "engine/input/Input.hpp"

#include <imgui/imgui.h>

#include <algorithm>
#include <cctype>
#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace Editor::Panels
{
    namespace
    {
        using namespace Engine::Input;

        const ImVec4 kGreen = Theme::kSuccessText;
        const ImVec4 kRed = Theme::kErrorText;
        const ImVec4 kYellow = Theme::kWarningText;

        // "MoveForward" -> "Move forward": an action's name in words, for the table (the file keeps the name)
        std::string InWords(const std::string& name)
        {
            std::string words;
            for (std::size_t i = 0; i < name.size(); ++i)
            {
                const auto c = static_cast<unsigned char>(name[i]);
                if (i > 0 && std::isupper(c) && std::islower(static_cast<unsigned char>(name[i - 1])))
                {
                    words += ' ';
                    words += static_cast<char>(std::tolower(c));
                }
                else
                {
                    words += name[i];
                }
            }
            return words;
        }
    }

    // selectedGame is a callback rather than a path because the user can switch games while the panel is open
    ControlsPanel::ControlsPanel(Engine::Input::InputSystem& input, std::function<std::filesystem::path()> selectedGame,
                                 std::function<void()> onSaved)
        : Panel("Controls"), input_(input), selectedGame_(std::move(selectedGame)), onGameSaved_(std::move(onSaved))
    {
    }

    std::filesystem::path ControlsPanel::SelectedGame() const
    {
        return selectedGame_ ? selectedGame_() : std::filesystem::path{};
    }

    // (Re)reads the selected game's input_actions.json and throws away unsaved changes
    void ControlsPanel::LoadControls()
    {
        controlsGameDir_ = SelectedGame();
        controlsChanged_ = false;
        controlsFailed_ = false;
        controlsMessage_.clear();
        controlsWarnings_.clear();
        listeningAction_.clear();
        if (controlsGameDir_.empty())
        {
            controls_ = Engine::Input::InputActions::Defaults();
            return;
        }
        // Problems are logged by Load and listed here; the defaults fill in whatever the file lacks
        controls_ = Engine::Input::InputActions::Load(Engine::Game::GameProject(controlsGameDir_).InputActionsPath(),
                                                      &controlsWarnings_);
    }

    // Writes input_actions.json and tells the editor so the running game picks up the new keys
    void ControlsPanel::SaveControls()
    {
        try
        {
            const Engine::Game::GameProject project(controlsGameDir_);
            controls_.Save(project.InputActionsPath());
            controlsChanged_ = false;
            controlsFailed_ = false;
            controlsWarnings_.clear();
            controlsMessage_ = "Saved " + project.InputActionsPath().filename().string() + " - the new keys work now";
            if (onGameSaved_)
            {
                onGameSaved_(); // the game re-reads its controls
            }
        }
        catch (const std::exception& e)
        {
            controlsFailed_ = true;
            controlsMessage_ = std::string("Save failed: ") + e.what();
        }
    }

    // The table of actions and their keys, plus the buttons under it
    void ControlsPanel::DrawContents()
    {
        namespace Input = Engine::Input;
        // another game got selected -> show its controls instead
        if (controlsGameDir_ != SelectedGame())
        {
            LoadControls(); // follow the selected game
        }

        // Waiting for a key: the next key pressed is bound (Escape cancels). The editor keeps keys away from its
        // shortcuts and the game meanwhile (see IsListeningForKey).
        if (!listeningAction_.empty())
        {
            for (const Input::InputBinding& binding : Input::AllBindings())
            {
                if (binding.device != Input::InputBinding::Device::Key || !input_.IsKeyPressed(binding.code))
                {
                    continue;
                }
                if (binding.code != Key_Escape)
                {
                    controls_.AddBinding(listeningAction_, binding);
                    controlsChanged_ = true;
                }
                listeningAction_.clear();
                break;
            }
        }

        if (controlsGameDir_.empty())
        {
            ImGui::TextDisabled("Select a game (project list, top right) or create one (Project > New Game).");
            return;
        }
        ImGui::TextWrapped("Keys for each action%s. Click a key to remove it; + adds one from a list; Press key "
                           "binds the next key you press (Escape cancels).",
                           controlsChanged_ ? "   (unsaved changes)" : "");

        const Input::InputActions defaults = Input::InputActions::Defaults();
        std::string removeAction;
        std::optional<std::pair<std::string, Input::InputBinding>> removeBinding;
        std::optional<std::pair<std::string, Input::InputBinding>> addBinding;

        if (ImGui::BeginTable("controls", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            // The fixed columns fit their contents (at any display scale); Keys takes the rest
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Keys");
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableHeadersRow();

            for (const auto& [action, bindings] : controls_.All())
            {
                ImGui::PushID(action.c_str());
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(InWords(action).c_str());

                ImGui::TableNextColumn();
                if (bindings.empty())
                {
                    ImGui::TextDisabled("(none)");
                    ImGui::SameLine();
                }
                for (const Input::InputBinding& binding : bindings)
                {
                    ImGui::PushID(static_cast<int>(binding.device) * 1000 + binding.code);
                    if (ImGui::Button((Input::BindingName(binding) + " x").c_str()))
                    {
                        removeBinding.emplace(action, binding);
                    }
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
                    {
                        ImGui::SetTooltip("Remove this key");
                    }
                    ImGui::PopID();
                    ImGui::SameLine();
                }

                ImGui::SetNextItemWidth(ImGui::GetFrameHeight()); // square, at any display scale
                if (ImGui::BeginCombo("##add", "+", ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_HeightLarge))
                {
                    for (const Input::InputBinding& binding : Input::AllBindings())
                    {
                        if (std::find(bindings.begin(), bindings.end(), binding) != bindings.end())
                        {
                            continue;
                        }
                        if (ImGui::Selectable(Input::BindingName(binding).c_str()))
                        {
                            addBinding.emplace(action, binding);
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                const bool listening = listeningAction_ == action;
                if (ImGui::Button(listening ? "Press a key... (Esc)" : "Press key"))
                {
                    listeningAction_ = listening ? "" : action;
                }

                ImGui::TableNextColumn();
                const bool builtIn = defaults.Find(action) != nullptr;
                ImGui::BeginDisabled(builtIn);
                if (ImGui::Button("x"))
                {
                    removeAction = action;
                }
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    ImGui::SetTooltip(builtIn
                                          ? "Used by the engine (camera / player) - it can be left with no keys instead"
                                          : "Remove this action");
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        // Changed after drawing, so the table was not edited while it was being walked
        if (removeBinding)
        {
            controls_.RemoveBinding(removeBinding->first, removeBinding->second);
            controlsChanged_ = true;
        }
        if (addBinding)
        {
            controls_.AddBinding(addBinding->first, addBinding->second);
            controlsChanged_ = true;
        }
        if (!removeAction.empty())
        {
            controls_.RemoveAction(removeAction);
            if (listeningAction_ == removeAction)
            {
                listeningAction_.clear();
            }
            controlsChanged_ = true;
        }

        // A new action for game code (read with GameInput::IsActionDown("Jump") etc.)
        ImGui::SetNextItemWidth(-90.0f);
        ImGui::InputTextWithHint("##newaction", "New action, e.g. Jump", newActionName_.data(), newActionName_.size());
        ImGui::SameLine();
        const std::string newAction = newActionName_.data();
        ImGui::BeginDisabled(newAction.empty() || controls_.Find(newAction) != nullptr);
        if (ImGui::Button("Add action"))
        {
            controls_.Bind(newAction, {});
            listeningAction_ = newAction; // straight on to choosing its key
            newActionName_.fill('\0');
            controlsChanged_ = true;
        }
        ImGui::EndDisabled();

        if (ImGui::Button("Save controls"))
        {
            SaveControls();
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert"))
        {
            LoadControls();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset to defaults"))
        {
            controls_ = defaults;
            listeningAction_.clear();
            controlsChanged_ = true;
        }

        if (!controlsMessage_.empty())
        {
            ImGui::TextColored(controlsFailed_ ? kRed : kGreen, "%s", controlsMessage_.c_str());
        }
        for (const auto& warning : controlsWarnings_)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, kYellow);
            ImGui::TextWrapped("%s", warning.c_str());
            ImGui::PopStyleColor();
        }
    }
}
