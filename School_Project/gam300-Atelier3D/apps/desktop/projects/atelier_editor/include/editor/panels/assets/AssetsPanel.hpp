/* !
@file    	    AssetsPanel.hpp
@author  	    j.cai@digipen.edu [72%]
                sanni.go@digipen.edu [20%]
         	    c.kojat@digipen.edu [8%]
@date    		27/09/2026
@brief
    AssetsPanel, the Loaded Assets window: a table of every asset the running
    scene uses with its type, reference count, disk reads and whether it is a
    placeholder.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "editor/panels/Panel.hpp"

#include "engine/assets/ResourceManager.hpp"

#include <imgui/imgui.h>

namespace Editor::Panels
{
    // Every loaded asset with its reference count and how often it was read from disk
    class AssetsPanel final : public Panel
    {
    public:
        AssetsPanel() : Panel("Assets") {}

        // The running game's assets, or nullptr when nothing is playing
        void SetSource(const Engine::Assets::ResourceManager* resources) { resources_ = resources; }

        // The table in the current window (the docked editor hosts it as "Loaded Assets")
        void DrawContents() override
        {
            if (!resources_)
            {
                ImGui::TextDisabled("Select a game to see the assets its scene loads.");
                return;
            }

            constexpr ImGuiTableFlags kFlags =
                ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("assets", 5, kFlags))
            {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Refs", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Disk reads", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableHeadersRow();

                // Refs = how many objects use the file, Disk reads should stay at 1 however many objects share it
                for (const auto& asset : resources_->ListAssets())
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(asset.path.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(Engine::Assets::ToString(asset.type));
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", asset.refCount);
                    ImGui::TableNextColumn();
                    ImGui::Text("%u", asset.diskReads);
                    ImGui::TableNextColumn();
                    if (asset.isPlaceholder)
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Placeholder");
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Loaded");
                    }
                }
                ImGui::EndTable();
            }
        }

    private:
        const Engine::Assets::ResourceManager* resources_ = nullptr;
    };
}
