/* !
@file    	    SceneEditorColliders.cpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		28/10/2026
@brief
    Implements the Viewport's physics debug view: every rigid body's collider
    drawn as a wireframe over the scene (in edit mode where its Transform puts
    it, in Play mode where the physics has it), coloured by body type (green
    static, pink dynamic, blue kinematic), faded when asleep and thicker while
    touching something; and the Physics window's test ray with what it hits,
    its distance check: the gap between two colliders and the range around
    the selected object; and its ground check: where the selected object
    stands.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/SceneEditor.hpp"

#include "engine/scene/systems/PhysicsSystem.hpp"

#include <imgui/imgui.h>

#include <optional>
#include <utility>

namespace Editor
{
    namespace
    {
        // Closer to the camera than this (in clip space w) a point cannot be projected
        constexpr float kNearW = 1e-3f;

        // A point in clip space as a point in the view
        ImVec2 ToView(const glm::vec4& clip, const ImVec2& position, const ImVec2& size)
        {
            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            return ImVec2(position.x + (ndc.x * 0.5f + 0.5f) * size.x, position.y + (0.5f - ndc.y * 0.5f) * size.y);
        }

        // A segment as it appears in the view: the part behind the camera is cut off, so an edge that runs past the
        // camera still shows the part in front. nullopt if it is all behind.
        std::optional<std::pair<ImVec2, ImVec2>> Project(const glm::mat4& viewProjection,
                                                         const Engine::Physics::Line& line, const ImVec2& position,
                                                         const ImVec2& size)
        {
            glm::vec4 from = viewProjection * glm::vec4(line.from, 1.0f);
            glm::vec4 to = viewProjection * glm::vec4(line.to, 1.0f);
            if (from.w < kNearW && to.w < kNearW)
            {
                return std::nullopt;
            }
            if (from.w < kNearW)
            {
                from = glm::mix(from, to, (kNearW - from.w) / (to.w - from.w));
            }
            else if (to.w < kNearW)
            {
                to = glm::mix(to, from, (kNearW - to.w) / (from.w - to.w));
            }
            return std::pair{ToView(from, position, size), ToView(to, position, size)};
        }

    }

    // The Physics window's test ray, distance and ground check while it runs (the colliders themselves are debug
    // lines, from the Debug menu)
    void SceneEditor::DrawPhysicsTests(const glm::mat4& viewProjection, const ImVec2& position, const ImVec2& size)
    {
        // Cast in this frame or the last one (the Physics window may be drawn after the Viewport)
        const int lastFrame = ImGui::GetFrameCount() - 1;
        const bool drawRay = playing_ && rayTest_.on && rayTest_.frame >= lastFrame;
        const bool drawDistance = playing_ && distanceTest_.on && distanceTest_.frame >= lastFrame;
        const bool drawGround = playing_ && groundTest_.on && groundTest_.frame >= lastFrame;
        if (!runtime_ || (!drawRay && !drawDistance && !drawGround))
        {
            return;
        }

        // Only inside the view: a collider partly off screen must not draw over the toolbar or the panels around it
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(position, ImVec2(position.x + size.x, position.y + size.y), true);
        if (drawRay)
        {
            DrawTestRay(*drawList, viewProjection, position, size);
        }
        if (drawDistance)
        {
            DrawTestDistance(*drawList, viewProjection, position, size);
        }
        if (drawGround)
        {
            DrawTestGround(*drawList, viewProjection, position, size);
        }
        drawList->PopClipRect();
    }

    // The test ray in yellow up to where it stops; where it hits, a dot and a short line out of the surface hit
    void SceneEditor::DrawTestRay(ImDrawList& drawList, const glm::mat4& viewProjection, const ImVec2& position,
                                  const ImVec2& size) const
    {
        constexpr ImU32 kRayColor = IM_COL32(255, 220, 60, 255);
        constexpr ImU32 kNormalColor = IM_COL32(255, 255, 255, 255);
        constexpr float kNormalLength = 0.5f; // in world units

        if (const auto segment = Project(viewProjection, {rayTest_.from, rayTest_.to}, position, size))
        {
            drawList.AddLine(segment->first, segment->second, kRayColor, 2.0f * uiScale_);
        }
        if (!rayTest_.hit)
        {
            return;
        }
        const glm::vec3 tip = rayTest_.hit->point + rayTest_.hit->normal * kNormalLength;
        if (const auto segment = Project(viewProjection, {rayTest_.hit->point, tip}, position, size))
        {
            drawList.AddLine(segment->first, segment->second, kNormalColor, 1.5f * uiScale_);
            drawList.AddCircleFilled(segment->first, 4.0f * uiScale_, kRayColor);
        }
    }

    // The distance check in cyan: the range around the selected object as a sphere, and the gap to the object measured
    // to as a line between the nearest points of the two colliders
    void SceneEditor::DrawTestDistance(ImDrawList& drawList, const glm::mat4& viewProjection, const ImVec2& position,
                                       const ImVec2& size) const
    {
        constexpr ImU32 kRangeColor = IM_COL32(80, 220, 230, 120);
        constexpr ImU32 kGapColor = IM_COL32(80, 220, 230, 255);
        constexpr int kSegments = 48;

        const Engine::Physics::Collider range{.shape = Engine::Physics::ShapeType::Sphere,
                                              .radius = distanceTest_.range};
        for (const Engine::Physics::Line& line :
             Engine::Physics::OutlineOf(range, {.position = distanceTest_.from}, kSegments))
        {
            if (const auto segment = Project(viewProjection, line, position, size))
            {
                drawList.AddLine(segment->first, segment->second, kRangeColor, 1.0f * uiScale_);
            }
        }

        if (const auto& gap = distanceTest_.gap)
        {
            if (const auto segment = Project(viewProjection, {gap->pointOnA, gap->pointOnB}, position, size))
            {
                drawList.AddLine(segment->first, segment->second, kGapColor, 2.0f * uiScale_);
                drawList.AddCircleFilled(segment->first, 3.5f * uiScale_, kGapColor);
                drawList.AddCircleFilled(segment->second, 3.5f * uiScale_, kGapColor);
            }
        }
    }

    // The ground check in green: a dot where the selected object stands and a short line out of the ground there
    void SceneEditor::DrawTestGround(ImDrawList& drawList, const glm::mat4& viewProjection, const ImVec2& position,
                                     const ImVec2& size) const
    {
        constexpr ImU32 kGroundColor = IM_COL32(120, 235, 110, 255);
        constexpr float kNormalLength = 0.75f; // in world units

        const auto& ground = groundTest_.ground;
        if (!ground)
        {
            return;
        }
        const glm::vec3 tip = ground->point + ground->normal * kNormalLength;
        if (const auto segment = Project(viewProjection, {ground->point, tip}, position, size))
        {
            drawList.AddLine(segment->first, segment->second, kGroundColor, 2.0f * uiScale_);
            drawList.AddCircleFilled(segment->first, 4.5f * uiScale_, kGroundColor);
        }
    }
}
