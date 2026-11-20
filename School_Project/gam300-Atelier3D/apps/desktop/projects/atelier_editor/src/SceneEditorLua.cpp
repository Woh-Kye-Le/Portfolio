#include "editor/SceneEditor.hpp"
#include "editor/Theme.hpp"

#include <algorithm>
#include <cstdio>
#include <format>
#include <sstream>

namespace Editor
{
    void SceneEditor::DrawLuaPanel()
    {
        auto* object = scene_.Find(scene_.Selected());

        if (!object)
        {
            ImGui::TextWrapped("Select an object in the Hierarchy "
                               "to write its Lua script.");

            return;
        }

        ImGui::Text("Object: %s [%u]", object->name.c_str(), object->id);

        ImGui::TextWrapped("move(x, y, z) moves this object's Transform. "
                           "update(dt) runs each fixed simulation step. "
                           "Scripts execute during Play; Stop restores the scene. "
                           "Use a Transform without a Dynamic Rigid Body for this demo.");

        // Refresh the drafts when selection or applied source changes.
        if (luaEditingObject_ != object->id || luaAppliedSource_ != object->luaSource ||
            luaAppliedFile_ != object->luaFile)
        {
            luaEditingObject_ = object->id;
            luaAppliedSource_ = object->luaSource;
            luaAppliedFile_ = object->luaFile;

            std::snprintf(luaFileDraft_.data(), luaFileDraft_.size(), "%s", object->luaFile.c_str());

            std::snprintf(luaDraft_.data(), luaDraft_.size(), "%s", object->luaSource.c_str());
        }

        const bool tooLarge = object->luaSource.size() >= luaDraft_.size();

        const bool fileTooLarge = object->luaFile.size() >= luaFileDraft_.size();

        if (tooLarge)
        {
            ImGui::TextWrapped("This script exceeds the 65535-byte editor limit. "
                               "Edit lua_source in the scene file; "
                               "the runtime can still execute it.");
        }

        if (fileTooLarge)
        {
            ImGui::TextWrapped("The script file path exceeds the editor's input limit.");
        }

        // Apply source changes between Play sessions.
        ImGui::BeginDisabled(playing_ || tooLarge || fileTooLarge);

        ImGui::InputText("Script file", luaFileDraft_.data(), luaFileDraft_.size());

        ImGui::TextWrapped("Optional path relative to the game folder, "
                           "e.g. behaviors/attack.script. "
                           "A file path runs that file instead of the inline code below. "
                           "Clear the path to use inline code.");

        if (ImGui::Button("Insert movement demo"))
        {
            // Ensure the inline demo is used.
            luaFileDraft_[0] = '\0';

            std::snprintf(luaDraft_.data(), luaDraft_.size(), "%s",
                          "speed = 2\n"
                          "distance = 0\n"
                          "\n"
                          "function update(dt)\n"
                          "    local step = speed * dt\n"
                          "    move(step, 0, 0)\n"
                          "    distance = distance + step\n"
                          "end\n");
        }

        ImGui::InputTextMultiline("##LuaSource", luaDraft_.data(), luaDraft_.size(), ImVec2(-1, 175),
                                  ImGuiInputTextFlags_AllowTabInput);

        if (ImGui::Button("Apply script"))
        {
            object->luaSource = luaDraft_.data();
            object->luaFile = luaFileDraft_.data();

            luaAppliedFile_ = object->luaFile;
            luaAppliedSource_ = object->luaSource;

            CommitHistory(true);
        }

        ImGui::SameLine();

        if (ImGui::Button("Remove script"))
        {
            object->luaSource.clear();
            object->luaFile.clear();

            luaAppliedFile_.clear();
            luaFileDraft_[0] = '\0';

            luaAppliedSource_.clear();
            luaDraft_[0] = '\0';

            CommitHistory(true);
        }

        ImGui::EndDisabled();

        if (!playing_ || !runtime_)
        {
            ImGui::TextDisabled("Apply, save the scene, then press Play to execute.");

            return;
        }

        ImGui::Separator();

        ImGui::TextUnformatted(paused_ ? "Simulation paused" : "Simulation running");

        ImGui::TextUnformatted("Compiled scripts (select to inspect):");

        for (const auto& [id, script] : runtime_->LuaScripts())
        {
            ImGui::PushID(static_cast<int>(id));

            if (ImGui::Selectable(script->Name().c_str(), id == object->id))
            {
                scene_.Select(id);
            }

            ImGui::PopID();
        }

        const auto found = runtime_->LuaScripts().find(object->id);

        if (found == runtime_->LuaScripts().end())
        {
            ImGui::TextDisabled("No script on this object at Play start.");

            return;
        }

        Engine::Scripting::LuaScript& script = *found->second;

        if (script.Paused())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Stopped before line %d", script.PausedLine());
            ImGui::SameLine();
            if (ImGui::SmallButton("Step Line"))
            {
                script.StepLine();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Continue"))
            {
                script.Continue();
                if (!script.Paused())
                {
                    runtime_->Debugger().Resume();
                }
            }
        }
        else if (ImGui::SmallButton("Stop at Next Line"))
        {
            script.PauseNextLine();
        }

        ImGui::Text("Script: %s | update calls: %llu", script.Name().c_str(),
                    static_cast<unsigned long long>(script.Calls()));

        if (!script.Error().empty())
        {
            ImGui::TextWrapped("ERROR: %s", script.Error().c_str());
        }
        else
        {
            ImGui::TextUnformatted(script.Initialized() ? "Executing update(dt), or completed one-shot script"
                                                        : "Compiled; waiting for simulation tick");
        }

        const auto& trace = script.Trace();

        const int activeLine = trace.empty() ? 0 : trace.back().line;

        ImGui::Text("Last observed line: %d", activeLine);

        ImGui::TextWrapped("Yellow marks the last observed line; "
                           "> marks lines visited in the latest call. "
                           "Click a line to stop there (*). "
                           "Pause the simulation to inspect a stable trace. "
                           "Local values are captured before each line runs.");

        if (ImGui::BeginChild("LuaExecution", ImVec2(0, 140), true))
        {
            std::istringstream input(script.Source());
            std::string line;
            int number = 0;

            while (std::getline(input, line))
            {
                ++number;

                const bool visited = std::any_of(trace.begin(), trace.end(),
                                                 [number](const auto& event) { return event.line == number; });

                if (number == activeLine)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
                }

                const bool stopsHere = script.HasLineBreakpoint(number);
                const std::string row =
                    std::format("{} {} {:3}  {}", stopsHere ? '*' : ' ', visited ? '>' : ' ', number, line);
                ImGui::PushID(number);
                if (ImGui::Selectable(row.c_str()))
                {
                    script.ToggleLineBreakpoint(number); // * marks a line it stops at
                }
                ImGui::PopID();

                if (number == activeLine)
                {
                    ImGui::PopStyleColor();
                }
            }
        }

        ImGui::EndChild();

        if (ImGui::CollapsingHeader("Global variables", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (const auto& [key, value] : script.Variables())
            {
                ImGui::Text("%s (%s) = %s", key.c_str(), value.type.c_str(), value.text.c_str());
            }
        }

        if (!trace.empty() && ImGui::CollapsingHeader("Call stack", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (const std::string& frame : trace.back().stack)
            {
                ImGui::TextUnformatted(frame.c_str());
            }
        }

        if (ImGui::CollapsingHeader("Execution trace / locals "
                                    "(up to 128 line events per tick)"))
        {
            for (std::size_t i = 0; i < trace.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));

                if (ImGui::TreeNode("event", "Event %zu: line %d", i + 1, trace[i].line))
                {
                    for (const auto& [key, value] : trace[i].variables)
                    {
                        ImGui::Text("%s = %s", key.c_str(), value.c_str());
                    }

                    ImGui::TreePop();
                }

                ImGui::PopID();
            }
        }
    }
}
