/* !
@file    	    LogicPanel.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/10/2026
@brief
    Implements the Logic window: objects with block logic, their scripts as
    outlines, running blocks and problems.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/panels/blocks/LogicPanel.hpp"

#include "editor/Theme.hpp"
#include "engine/logic/LogicRunner.hpp"
#include "engine/scene/SceneDescription.hpp"

#include <imgui/imgui.h>

#include <algorithm>
#include <string>

namespace Editor::Panels
{
    namespace
    {
        ImVec4 ColourOf(Engine::Logic::Category category)
        {
            const std::uint32_t rgb = Engine::Logic::CategoryColor(category);
            return ImVec4(static_cast<float>((rgb >> 16) & 0xFFu) / 255.0f,
                          static_cast<float>((rgb >> 8) & 0xFFu) / 255.0f, static_cast<float>(rgb & 0xFFu) / 255.0f,
                          1.0f);
        }

        bool StartsWithHat(const Engine::Logic::Script& script)
        {
            return !script.blocks.empty() &&
                   Engine::Logic::InfoOf(script.blocks.front().op).shape == Engine::Logic::BlockShape::Hat;
        }
    }

    LogicPanel::LogicPanel() : Panel("Logic") {}

    void LogicPanel::SetScene(const Engine::Scene::SceneDescription* scene, std::uint32_t selected,
                              const Engine::Logic::LogicRunner* runner)
    {
        scene_ = scene;
        selected_ = selected;
        runner_ = runner;
    }

    void LogicPanel::SetOnOpen(std::function<void(std::uint32_t, std::uint32_t)> onOpen)
    {
        onOpen_ = std::move(onOpen);
    }

    // One line per block, indented inside C blocks, with a dot of its category's colour; running blocks in green
    void LogicPanel::DrawBlocks(const std::vector<Engine::Logic::Block>& blocks, std::uint32_t objectId, int depth,
                                const std::vector<std::uint32_t>& active)
    {
        for (const Engine::Logic::Block& block : blocks)
        {
            const Engine::Logic::BlockInfo& info = Engine::Logic::InfoOf(block.op);
            ImGui::PushID(static_cast<int>(block.id));
            ImGui::Indent(static_cast<float>(depth) * ImGui::GetStyle().IndentSpacing);
            const bool running = std::binary_search(active.begin(), active.end(), block.id);
            ImGui::TextColored(ColourOf(info.category), "%s", running ? ">" : "-");
            ImGui::SameLine();
            if (running)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, Theme::kSuccessText);
            }
            if (ImGui::Selectable(Engine::Logic::Describe(block).c_str()) && onOpen_)
            {
                onOpen_(objectId, block.id);
            }
            if (running)
            {
                ImGui::PopStyleColor();
            }
            ImGui::Unindent(static_cast<float>(depth) * ImGui::GetStyle().IndentSpacing);
            if (info.hasBody)
            {
                DrawBlocks(block.body, objectId, depth + 1, active);
            }
            if (info.hasElse)
            {
                ImGui::Indent(static_cast<float>(depth) * ImGui::GetStyle().IndentSpacing);
                ImGui::TextColored(ColourOf(info.category), "  else");
                ImGui::Unindent(static_cast<float>(depth) * ImGui::GetStyle().IndentSpacing);
                DrawBlocks(block.elseBody, objectId, depth + 1, active);
            }
            ImGui::PopID();
        }
    }

    void LogicPanel::DrawContents()
    {
        if (!scene_)
        {
            ImGui::TextDisabled("Open a game to see its logic.");
            return;
        }
        bool any = false;
        for (const Engine::Scene::GameObject& object : scene_->objects)
        {
            if (!object.logic || object.logic->scripts.empty())
            {
                continue;
            }
            any = true;
            ImGui::PushID(static_cast<int>(object.id));
            const Engine::Logic::LogicDesc& logic = *object.logic;
            const std::size_t running = runner_ ? runner_->RunningScripts(object.id) : 0;
            const auto problems =
                runner_ ? runner_->Problems(object.id) : std::vector<Engine::Logic::LogicRunner::Problem>{};
            const std::vector<std::uint32_t> active =
                runner_ ? runner_->ActiveBlocks(object.id) : std::vector<std::uint32_t>{};

            // The object: click to select it and show its blocks
            const std::string title = object.name + "##object";
            const bool selected = object.id == selected_;
            if (selected)
            {
                ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            }
            const bool open = ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            if (selected)
            {
                ImGui::PopStyleColor();
            }
            if (ImGui::IsItemClicked() && onOpen_)
            {
                onOpen_(object.id, 0);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%zu script%s", logic.scripts.size(), logic.scripts.size() == 1 ? "" : "s");
            if (running > 0)
            {
                ImGui::SameLine();
                ImGui::TextColored(Theme::kSuccessText, "%zu running", running);
            }
            if (!problems.empty())
            {
                ImGui::SameLine();
                ImGui::TextColored(Theme::kErrorText, "%zu problem%s", problems.size(),
                                   problems.size() == 1 ? "" : "s");
            }
            if (open)
            {
                for (const auto& problem : problems)
                {
                    ImGui::PushID(static_cast<int>(problem.block));
                    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kErrorText);
                    if (ImGui::Selectable(("Problem: " + problem.message).c_str()) && onOpen_)
                    {
                        onOpen_(object.id, problem.block);
                    }
                    ImGui::PopStyleColor();
                    ImGui::PopID();
                }
                // Its variables: what they start at, or what they are now while the game runs
                if (!logic.variables.empty())
                {
                    const auto live =
                        runner_ ? runner_->Variables(object.id) : std::vector<std::pair<std::string, double>>{};
                    ImGui::Indent();
                    for (const Engine::Logic::Variable& variable : logic.variables)
                    {
                        const auto now = std::find_if(live.begin(), live.end(), [&variable](const auto& each)
                                                      { return each.first == variable.name; });
                        if (now != live.end())
                        {
                            ImGui::TextColored(Theme::kWarningText, "%s = %s", variable.name.c_str(),
                                               Engine::Logic::ValueText(variable.kind, now->second).c_str());
                        }
                        else
                        {
                            ImGui::TextColored(Theme::kWarningText, "%s starts at %s", variable.name.c_str(),
                                               Engine::Logic::ValueText(variable.kind, variable.value).c_str());
                        }
                    }
                    ImGui::Unindent();
                }
                for (std::size_t i = 0; i < logic.scripts.size(); ++i)
                {
                    const Engine::Logic::Script& script = logic.scripts[i];
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::Indent();
                    if (!StartsWithHat(script))
                    {
                        ImGui::TextDisabled("Not under a \"when\" block, so it does not run:");
                    }
                    DrawBlocks(script.blocks, object.id, 0, active);
                    ImGui::Unindent();
                    ImGui::Spacing();
                    ImGui::PopID();
                }
                if (runner_)
                {
                    const std::string said = runner_->LastSaid(object.id);
                    if (!said.empty())
                    {
                        ImGui::Indent();
                        ImGui::TextDisabled("Last said: \"%s\"", said.c_str());
                        ImGui::Unindent();
                    }
                }
            }
            ImGui::PopID();
        }
        if (!any)
        {
            ImGui::TextWrapped("No object has blocks yet. Select an object, then drag blocks onto the canvas in the "
                               "Blocks window.");
        }
    }
}
