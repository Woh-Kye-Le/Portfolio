/* !
@file    	    VariableInspectorPanel.cpp
@date    		29/10/2026
@brief
    Implements the Variable Inspector: the selected object's values by
    section and the list of blocks that set each value.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/panels/blocks/VariableInspectorPanel.hpp"

#include "engine/game/GameRuntime.hpp"

#include "editor/Theme.hpp"

#include "engine/scene/GameObjectValues.hpp"
#include "engine/scene/SceneDescription.hpp"

#include <imgui/imgui.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <string_view>
#include <utility>

namespace Editor::Panels
{
    namespace
    {
        using Engine::Logic::FormatNumber;
        using Engine::Logic::Property;

        // Every digit of the numbers ("2.0000002, 0, -3"), or as shown ("2, 0, -3")
        std::string Joined(const std::vector<double>& numbers, bool exact)
        {
            std::string text;
            for (const double value : numbers)
            {
                text += (text.empty() ? "" : ", ") + (exact ? std::format("{}", value) : FormatNumber(value));
            }
            return text;
        }

        std::string YesNo(bool value)
        {
            return value ? "true" : "false";
        }

        // A file's name without its folders, or "(none)"
        std::string FileName(const std::string& path)
        {
            return path.empty() ? "(none)" : std::filesystem::path(path).filename().string();
        }
    }

    VariableInspectorPanel::VariableInspectorPanel() : Panel("Variable Inspector") {}

    void VariableInspectorPanel::SetTarget(const Engine::Scene::SceneDescription* scene, std::uint32_t selected,
                                           const Engine::Game::GameRuntime* runtime)
    {
        scene_ = scene;
        selected_ = selected;
        runtime_ = runtime;
    }

    void VariableInspectorPanel::SetOnOpen(std::function<void(std::uint32_t, std::uint32_t)> onOpen)
    {
        onOpen_ = std::move(onOpen);
    }

    void VariableInspectorPanel::SetDrawFields(
        std::function<void(Engine::Scene::GameObject&, std::vector<FieldBox>&)> drawFields)
    {
        drawFields_ = std::move(drawFields);
    }

    const Engine::Scene::GameObject* VariableInspectorPanel::Find(std::uint32_t objectId) const
    {
        if (!scene_ || objectId == 0)
        {
            return nullptr;
        }
        const auto found =
            std::find_if(scene_->objects.begin(), scene_->objects.end(),
                         [objectId](const Engine::Scene::GameObject& each) { return each.id == objectId; });
        return found != scene_->objects.end() ? &*found : nullptr;
    }

    const Engine::Logic::LogicRunner* VariableInspectorPanel::Runner() const
    {
        return runtime_ ? &runtime_->Blocks() : nullptr;
    }

    // The block variables, as the game has them now while it runs (their start values otherwise)
    std::optional<VariableInspectorPanel::Section> VariableInspectorPanel::VariablesOf(
        const Engine::Scene::GameObject& object) const
    {
        if (!object.logic || object.logic->variables.empty())
        {
            return std::nullopt;
        }
        const Engine::Logic::LogicRunner* runner = Runner();
        const auto now = runner ? runner->Variables(object.id) : std::vector<std::pair<std::string, double>>{};
        std::vector<Row> rows;
        for (const Engine::Logic::Variable& variable : object.logic->variables)
        {
            double value = variable.value;
            if (const auto found = std::find_if(now.begin(), now.end(),
                                                [&variable](const auto& each) { return each.first == variable.name; });
                found != now.end())
            {
                value = found->second;
            }
            const bool trueFalse = variable.kind == Engine::Logic::VariableKind::TrueFalse;
            rows.push_back({.key = "variable/" + variable.name,
                            .label = variable.name,
                            .text = Engine::Logic::ValueText(variable.kind, value),
                            .numbers = {value},
                            .variable = variable.name,
                            .trueFalse = trueFalse,
                            .detail = trueFalse ? "true/false, this object" : "number, this object"});
        }
        return Section{.name = "Variables", .rows = std::move(rows)};
    }

    // The velocity the physics gives the object's body now. Only while the game runs and the object has a body; it
    // holds still while the game is paused, because the physics does
    std::optional<VariableInspectorPanel::Section> VariableInspectorPanel::PhysicsOf(
        const Engine::Scene::GameObject& object) const
    {
        if (!runtime_ || !object.rigidBody)
        {
            return std::nullopt;
        }
        const auto velocity = runtime_->VelocityOf(runtime_->EntityOf(object.id));
        if (!velocity)
        {
            return std::nullopt; // no body yet: it is made on the game's first frame
        }
        const std::vector<double> numbers{velocity->x, velocity->y, velocity->z};
        Row row{.key = "physics/Velocity", .label = "Velocity", .text = Joined(numbers, false), .numbers = numbers};
        row.measured = true;
        return Section{.name = "Physics", .rows = {std::move(row)}};
    }

    // A copy of the object as the game has it now, for the Inspector's own fields to draw: the runtime knows every
    // component it holds, so this does not
    Engine::Scene::GameObject VariableInspectorPanel::LiveObject(const Engine::Scene::GameObject& object) const
    {
        std::optional<Engine::Scene::GameObject> live;
        if (runtime_)
        {
            live = runtime_->LiveObject(object.id);
        }
        Engine::Scene::GameObject copy = live ? std::move(*live) : object;
        copy.prefabInstance.reset(); // the prefab marks are for editing
        return copy;
    }

    // On top of the Inspector's fields the live view has just drawn: a field some block writes shows which blocks on
    // hover and opens one on click. The fields and what they hold come from the Inspector's own drawing and the scene
    // file's layout, so a new component or field is covered as soon as the Inspector has it.
    void VariableInspectorPanel::MarkFields(const std::vector<FieldBox>& boxes, const Engine::Scene::GameObject& live)
    {
        const Engine::Scene::ObjectValues values = Engine::Scene::ValuesOf(live);
        const bool windowHovered = ImGui::IsWindowHovered();
        for (const FieldBox& box : boxes)
        {
            // What the fields hold, as one value
            Row row{.key = "field/" + box.component + "/" + box.label, .label = box.label};
            const auto component = values.find(box.component);
            for (const std::string& field : box.fields)
            {
                const Engine::Scene::FieldValue* value = nullptr;
                if (component != values.end())
                {
                    if (const auto found = component->second.find(field); found != component->second.end())
                    {
                        value = &found->second;
                    }
                }
                std::string part = "default"; // the file leaves a field out while it has its default
                if (value)
                {
                    part = value->type == Engine::Scene::FieldValue::Type::Text   ? FileName(value->text)
                           : value->type == Engine::Scene::FieldValue::Type::Flag ? YesNo(value->number != 0.0)
                                                                                  : FormatNumber(value->number);
                    row.numbers.push_back(value->number);
                }
                row.text += (row.text.empty() ? "" : ", ") + part;
            }
            row.property = Engine::Logic::PropertyOfField(box.component, box.fields.front());

            const ImVec2 min(box.x0, box.y0);
            const ImVec2 max(box.x1, box.y1);
            if (!row.property)
            {
                continue; // no block writes it: nothing to say about who sets it
            }
            ImGui::PushID(row.key.c_str());
            const bool over = windowHovered && ImGui::IsMouseHoveringRect(min, max);
            if (over && !ImGui::IsPopupOpen("##setters"))
            {
                ImGui::BeginTooltip();
                DrawSetters(row, live, false);
                ImGui::EndTooltip();
            }
            if (over && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                const std::vector<Engine::Logic::BlockAt> writers = WritersOf(row, live);
                if (writers.size() == 1 && onOpen_)
                {
                    onOpen_(live.id, writers.front().block->id);
                }
                else if (writers.size() > 1)
                {
                    ImGui::OpenPopup("##setters");
                }
            }
            if (ImGui::BeginPopup("##setters"))
            {
                DrawSetters(row, live, true);
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
    }

    std::vector<Engine::Logic::BlockAt> VariableInspectorPanel::WritersOf(const Row& row,
                                                                          const Engine::Scene::GameObject& object) const
    {
        if (!object.logic)
        {
            return {};
        }
        if (!row.variable.empty())
        {
            return Engine::Logic::WritersOf(*object.logic, row.variable);
        }
        return row.property ? Engine::Logic::WritersOf(*object.logic, *row.property)
                            : std::vector<Engine::Logic::BlockAt>{};
    }

    std::optional<Engine::Logic::LogicRunner::Write> VariableInspectorPanel::LastWrite(const Row& row,
                                                                                       std::uint32_t objectId) const
    {
        const Engine::Logic::LogicRunner* runner = Runner();
        if (!runner)
        {
            return std::nullopt;
        }
        if (!row.variable.empty())
        {
            return runner->LastWrite(objectId, row.variable);
        }
        return row.property ? runner->LastWrite(objectId, *row.property) : std::nullopt;
    }

    void VariableInspectorPanel::DrawSetters(const Row& row, const Engine::Scene::GameObject& object, bool interactive)
    {
        const std::vector<Engine::Logic::BlockAt> writers = WritersOf(row, object);
        const auto last = LastWrite(row, object.id);
        if (!interactive)
        {
            ImGui::Text("%s: %s", row.label.c_str(), row.text.c_str());
            const std::string exact = Joined(row.numbers, true);
            if (row.trueFalse)
            {
                ImGui::TextDisabled("Kept as the number %s: 0 is false, anything else true", exact.c_str());
            }
            else if (exact != Joined(row.numbers, false))
            {
                ImGui::TextDisabled("Exactly %s", exact.c_str()); // the shown value is rounded
            }
            ImGui::Separator();
        }
        if (row.measured)
        {
            ImGui::TextDisabled("The physics sets this as the body falls, slides and bounces.");
            return;
        }
        if (row.variable.empty() && !row.property)
        {
            ImGui::TextDisabled("No block changes this. Change it in the Inspector.");
            return;
        }
        if (writers.empty())
        {
            ImGui::TextDisabled(row.variable.empty() ? "No block of this object changes this yet."
                                                     : "No block sets this variable yet.");
            return;
        }
        ImGui::TextUnformatted("Set by:");
        for (const Engine::Logic::BlockAt& writer : writers)
        {
            const bool ranLast = last && last->block == writer.block->id;
            const std::string where = writer.runs && object.logic
                                          ? Engine::Logic::Describe(object.logic->scripts[writer.script].blocks.front())
                                          : std::string("not under a \"when\" block: never runs");
            ImGui::PushID(static_cast<int>(writer.block->id));
            ImGui::TextColored(ranLast ? Theme::kRun : Theme::kTextMuted, "%s", ranLast ? ">" : " ");
            ImGui::SameLine();
            const std::string text = Engine::Logic::Describe(*writer.block);
            if (interactive)
            {
                if (ImGui::Selectable(text.c_str()) && onOpen_)
                {
                    onOpen_(object.id, writer.block->id);
                }
            }
            else
            {
                ImGui::TextUnformatted(text.c_str());
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", where.c_str());
            if (ranLast)
            {
                ImGui::SameLine();
                ImGui::TextColored(Theme::kRun, "ran last");
            }
            ImGui::PopID();
        }
        if (!interactive)
        {
            ImGui::TextDisabled(writers.size() == 1 ? "Click to show the block in the Blocks window"
                                                    : "Click to pick a block to show in the Blocks window");
        }
    }

    void VariableInspectorPanel::DrawRow(const Row& row, const Engine::Scene::GameObject& object)
    {
        ImGui::PushID(row.key.c_str());
        ImGui::TableNextRow();

        // Laid out like an Inspector row: the label, then the value in a box the height of a field. The box is
        // locked (it only shows).
        const ImGuiStyle& style = ImGui::GetStyle();
        const float height = ImGui::GetFrameHeight();

        ImGui::TableNextColumn();
        const ImVec2 labelMin = ImGui::GetCursorScreenPos();
        const float labelWidth = ImGui::GetContentRegionAvail().x;
        const bool clicked =
            ImGui::Selectable("##row", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
                              ImVec2(0.0f, height));
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(labelMin, ImVec2(labelMin.x + labelWidth - style.ItemSpacing.x, labelMin.y + height),
                               true);
        drawList->AddText(ImVec2(labelMin.x, labelMin.y + style.FramePadding.y), ImGui::GetColorU32(ImGuiCol_Text),
                          row.label.c_str());
        if (!row.detail.empty())
        {
            const float labelEnd = ImGui::CalcTextSize(row.label.c_str()).x + style.ItemSpacing.x;
            drawList->AddText(ImVec2(labelMin.x + labelEnd, labelMin.y + style.FramePadding.y),
                              ImGui::GetColorU32(ImGuiCol_TextDisabled), row.detail.c_str());
        }
        drawList->PopClipRect();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_DelayNone) &&
            !ImGui::IsPopupOpen("##setters"))
        {
            ImGui::BeginTooltip();
            DrawSetters(row, object, false);
            ImGui::EndTooltip();
        }
        if (clicked)
        {
            const std::vector<Engine::Logic::BlockAt> writers = WritersOf(row, object);
            if (writers.size() == 1 && onOpen_)
            {
                onOpen_(object.id, writers.front().block->id);
            }
            else if (writers.size() > 1)
            {
                ImGui::OpenPopup("##setters");
            }
        }
        if (ImGui::BeginPopup("##setters"))
        {
            DrawSetters(row, object, true);
            ImGui::EndPopup();
        }

        ImGui::TableNextColumn();
        const ImVec2 boxMin = ImGui::GetCursorScreenPos();
        const ImVec2 boxMax(boxMin.x + std::max(ImGui::GetContentRegionAvail().x - style.ItemSpacing.x, 1.0f),
                            boxMin.y + height);
        drawList->AddRectFilled(boxMin, boxMax, ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);
        drawList->PushClipRect(boxMin, boxMax, true);
        drawList->AddText(ImVec2(boxMin.x + style.FramePadding.x, boxMin.y + style.FramePadding.y),
                          ImGui::GetColorU32(ImGuiCol_Text), row.text.c_str());
        drawList->PopClipRect();
        ImGui::Dummy(ImVec2(boxMax.x - boxMin.x, height));
        ImGui::PopID();
    }

    void VariableInspectorPanel::DrawSection(const Section& section, const Engine::Scene::GameObject& object)
    {
        if (!ImGui::CollapsingHeader(section.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }
        constexpr ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
        if (ImGui::BeginTable(section.name.c_str(), 2, flags))
        {
            // The label's share matches the Inspector's rows (38%)
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.38f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.62f);
            for (const Row& row : section.rows)
            {
                DrawRow(row, object);
            }
            ImGui::EndTable();
        }
    }

    void VariableInspectorPanel::DrawContents()
    {
        const Engine::Scene::GameObject* object = Find(selected_);
        // Run, Pause and Step are the top bar's; the block running or next glows in the Blocks window (F02)
        if (!runtime_)
        {
            ImGui::TextColored(Theme::kTextMuted, "Start values. Run the game to watch them change.");
        }
        if (!object)
        {
            ImGui::TextWrapped("Select an object to see its values.");
            return;
        }

        const auto drawSection = [this, object](const std::optional<Section>& section)
        {
            if (section)
            {
                ImGui::PushID(section->name.c_str());
                DrawSection(*section, *object);
                ImGui::PopID();
            }
        };
        drawSection(VariablesOf(*object));
        drawSection(PhysicsOf(*object));
        if (drawFields_)
        {
            // The Inspector's own fields, showing what the game has now
            Engine::Scene::GameObject live = LiveObject(*object);
            std::vector<FieldBox> boxes;
            drawFields_(live, boxes);
            MarkFields(boxes, live);
        }
        if (runtime_)
        {
            ImGui::Spacing();
            ImGui::TextWrapped("Stop the game to change values.");
        }
    }
}
