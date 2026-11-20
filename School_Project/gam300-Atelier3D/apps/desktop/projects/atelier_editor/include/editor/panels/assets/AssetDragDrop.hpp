/* !
@file    	    AssetDragDrop.hpp
@author  	    j.cai@digipen.edu [100%]
@date    		25/10/2026
@brief
    Dragging assets out of the Assets window: the ImGui payload types (one per
    kind of asset) and the helpers the editor's drop targets use (Inspector
    fields, Hierarchy rows, the Viewport).

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "editor/panels/assets/AssetCatalog.hpp"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <optional>
#include <string>

namespace Editor::Panels
{
    // An asset dropped on a target that takes any kind
    struct DroppedAsset
    {
        AssetKind kind = AssetKind::Model;
        std::string path; // relative to the game folder
    };

    // One payload type per kind, so a target lights up only for the assets it can take. The payload is the asset's
    // path with its terminating '\0'.
    inline const char* AssetPayloadType(AssetKind kind)
    {
        switch (kind)
        {
        case AssetKind::Model: return "ATELIER_ASSET_MODEL";
        case AssetKind::Texture: return "ATELIER_ASSET_TEXTURE";
        case AssetKind::Audio: return "ATELIER_ASSET_AUDIO";
        case AssetKind::Material: return "ATELIER_ASSET_MATERIAL";
        case AssetKind::Shader: return "ATELIER_ASSET_SHADER";
        }
        return "ATELIER_ASSET";
    }

    // Starts dragging asset (call right after the item it is dragged from); the tooltip under the mouse shows its name
    inline void BeginAssetDrag(const AssetEntry& asset)
    {
        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(AssetPayloadType(asset.kind), asset.path.c_str(), asset.path.size() + 1);
            ImGui::Text("%s %s", ToString(asset.kind), asset.name.c_str());
            ImGui::EndDragDropSource();
        }
    }

    namespace Detail
    {
        inline std::string PayloadPath(const ImGuiPayload& payload)
        {
            return payload.DataSize > 0 ? std::string(static_cast<const char*>(payload.Data)) : std::string{};
        }

        // Inside BeginDragDropTarget / EndDragDropTarget: the first payload of any asset kind that was dropped
        inline std::optional<DroppedAsset> AcceptAnyKind()
        {
            for (const AssetKind kind : kAssetKinds)
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetPayloadType(kind)))
                {
                    return DroppedAsset{kind, PayloadPath(*payload)};
                }
            }
            return std::nullopt;
        }
    }

    // Makes the last item a drop target for assets of one kind; the path of the asset dropped on it this frame
    inline std::optional<std::string> AcceptAssetDrop(AssetKind kind)
    {
        std::optional<std::string> path;
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetPayloadType(kind)))
            {
                path = Detail::PayloadPath(*payload);
            }
            ImGui::EndDragDropTarget();
        }
        return path;
    }

    // Makes the last item a drop target for any asset; the asset dropped on it this frame
    inline std::optional<DroppedAsset> AcceptAnyAssetDrop()
    {
        std::optional<DroppedAsset> dropped;
        if (ImGui::BeginDragDropTarget())
        {
            dropped = Detail::AcceptAnyKind();
            ImGui::EndDragDropTarget();
        }
        return dropped;
    }

    // Makes a screen rectangle (not an item, e.g. the Viewport's picture) a drop target for any asset
    inline std::optional<DroppedAsset> AcceptAnyAssetDropInRect(const ImVec2& min, const ImVec2& max, const char* id)
    {
        std::optional<DroppedAsset> dropped;
        if (ImGui::BeginDragDropTargetCustom(ImRect(min, max), ImGui::GetID(id)))
        {
            dropped = Detail::AcceptAnyKind();
            ImGui::EndDragDropTarget();
        }
        return dropped;
    }
}
