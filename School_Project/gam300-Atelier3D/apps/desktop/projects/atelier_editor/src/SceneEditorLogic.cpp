/* !
@file    	    SceneEditorLogic.cpp
@author  	    j.cai@digipen.edu [100%]
@date    		02/11/2026
@brief
    Implements the Viewport's logic visualiser: a badge over every object
    with block logic saying what starts its scripts. While the game runs it
    marks the scripts running now, the variables as they are now and any
    problems. Hovering a badge (or selecting its object) shows all of it;
    clicking one opens the object's blocks.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/SceneEditor.hpp"

#include "editor/Theme.hpp"
#include "engine/game/GameRuntime.hpp"
#include "engine/logic/LogicRunner.hpp"

#include <imgui/imgui.h>

#include <algorithm>
#include <cfloat>
#include <format>
#include <optional>
#include <string>
#include <vector>

namespace Editor
{
    namespace
    {
        // A badge lists at most this many variables, and as many problems; the Logic window has the rest
        constexpr std::size_t kMaxListed = 4;
        // How far above the top of the object the badge sits (world units), as the HUD's health bar over an object;
        // and how far above that, in pixels at 100%, so the bar shows under it
        constexpr float kAbove = 0.3f;
        constexpr float kLift = 12.0f;

        enum class LineKind
        {
            Title,   // the object's name
            Running, // a script running now
            Idle,    // a script that runs when its hat says
            Off,     // a script with no hat: it never runs
            Variable,
            Problem,
            Hint
        };

        struct Rect
        {
            ImVec2 min;
            ImVec2 max;

            bool Contains(ImVec2 point) const
            {
                return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
            }
        };

        struct Line
        {
            LineKind kind;
            std::string text;
        };

        // What one object's badge says and where it goes
        struct Badge
        {
            std::uint32_t objectId = 0;
            ImVec2 anchor;      // the point above the object it stands on
            float depth = 0.0f; // clip w: the nearest is drawn last, on top
            bool running = false;
            std::size_t problems = 0;
            std::string summary;     // the badge folded: what starts its script, or how many scripts
            std::vector<Line> lines; // the badge open: everything
            Rect folded;
            Rect open;
        };

        ImVec4 TextColor(LineKind kind)
        {
            switch (kind)
            {
            case LineKind::Title: return Theme::kText;
            case LineKind::Running: return Theme::kSuccessText;
            case LineKind::Variable: return Theme::kWarningText;
            case LineKind::Problem: return Theme::kErrorText;
            case LineKind::Off:
            case LineKind::Hint: return Theme::kTextMuted;
            case LineKind::Idle: break;
            }
            return Theme::kTextSoft;
        }

        bool HasDot(LineKind kind)
        {
            return kind == LineKind::Running || kind == LineKind::Idle;
        }

        // The object's name in bold, if the bold face loaded
        ImFont* FontOf(LineKind kind)
        {
            return kind == LineKind::Title && Theme::BoldFont() ? Theme::BoldFont() : ImGui::GetFont();
        }

        // A status dot: mint and filled while running, an empty ring when waiting for its hat
        void DrawDot(ImDrawList* drawList, ImVec2 center, float radius, bool running, float scale)
        {
            if (running)
            {
                drawList->AddCircleFilled(center, radius, ImGui::GetColorU32(Theme::kRun));
            }
            drawList->AddCircle(center, radius, ImGui::GetColorU32(Theme::kInk), 0, 1.5f * scale);
        }

        bool StartsWithHat(const Engine::Logic::Script& script)
        {
            return !script.blocks.empty() &&
                   Engine::Logic::InfoOf(script.blocks.front().op).shape == Engine::Logic::BlockShape::Hat;
        }

        // The badge's words, from the object's logic and (while the game runs) what its blocks are doing now
        void Describe(Badge& badge, const Engine::Scene::GameObject& object, const Engine::Logic::LogicRunner* runner,
                      bool clickable)
        {
            if (!object.logic)
            {
                return;
            }
            const Engine::Logic::LogicDesc& logic = *object.logic;
            const std::vector<std::uint32_t> active =
                runner ? runner->ActiveBlocks(object.id) : std::vector<std::uint32_t>{};

            badge.lines.push_back({LineKind::Title, object.name});
            std::size_t runs = 0;
            const Engine::Logic::Block* onlyHat = nullptr;
            for (const Engine::Logic::Script& script : logic.scripts)
            {
                if (!StartsWithHat(script))
                {
                    badge.lines.push_back({LineKind::Off, "(not under a \"when\" block: does not run)"});
                    continue;
                }
                const bool running = Engine::Logic::ContainsAny(script.blocks, active);
                badge.running |= running;
                badge.lines.push_back(
                    {running ? LineKind::Running : LineKind::Idle, Engine::Logic::Describe(script.blocks.front())});
                onlyHat = ++runs == 1 ? &script.blocks.front() : nullptr;
            }
            badge.running |= runner && runner->RunningScripts(object.id) > 0;

            // Its variables: as they are now while the game runs, else what they start at
            const auto live = runner ? runner->Variables(object.id) : std::vector<std::pair<std::string, double>>{};
            const std::size_t variables = runner ? live.size() : logic.variables.size();
            for (std::size_t i = 0; i < std::min(variables, kMaxListed); ++i)
            {
                badge.lines.push_back(
                    {LineKind::Variable,
                     runner ? std::format("{} = {:g}", live[i].first, live[i].second)
                            : std::format("{} starts at {:g}", logic.variables[i].name, logic.variables[i].value)});
            }
            if (variables > kMaxListed)
            {
                badge.lines.push_back(
                    {LineKind::Hint, std::format("+{} more variables in the Logic window", variables - kMaxListed)});
            }

            const auto problems =
                runner ? runner->Problems(object.id) : std::vector<Engine::Logic::LogicRunner::Problem>{};
            badge.problems = problems.size();
            for (std::size_t i = 0; i < std::min(problems.size(), kMaxListed); ++i)
            {
                badge.lines.push_back({LineKind::Problem, "Problem: " + problems[i].message});
            }
            if (problems.size() > kMaxListed)
            {
                badge.lines.push_back({LineKind::Hint, std::format("+{} more problems in the Logic window",
                                                                   problems.size() - kMaxListed)});
            }
            if (clickable)
            {
                badge.lines.push_back({LineKind::Hint, "Click to see its blocks"});
            }

            badge.summary = runs == 0 ? std::string("Logic that does not run")
                            : onlyHat ? Engine::Logic::Describe(*onlyHat)
                                      : std::format("{} scripts", runs);
            if (badge.problems > 0)
            {
                badge.summary += std::format(" \xC2\xB7 {} problem{}", badge.problems, badge.problems == 1 ? "" : "s");
            }
        }

        // Where the badge's box goes: centred over its anchor with its bottom on it, kept inside the view
        Rect Place(const Badge& badge, ImVec2 size, const ImVec2& position, const ImVec2& viewSize)
        {
            const float x = std::clamp(badge.anchor.x - size.x * 0.5f, position.x,
                                       std::max(position.x, position.x + viewSize.x - size.x));
            const float y =
                std::clamp(badge.anchor.y - size.y, position.y, std::max(position.y, position.y + viewSize.y - size.y));
            return {ImVec2(x, y), ImVec2(x + size.x, y + size.y)};
        }
    }

    bool SceneEditor::DrawLogicBadges(const glm::mat4& viewProjection, const ImVec2& position, const ImVec2& size)
    {
        Engine::Scene::Scene* ecs = runtime_ ? runtime_->Ecs() : nullptr;
        if (!showLogicBadges_ || !ecs)
        {
            return false;
        }
        const Engine::Logic::LogicRunner* runner = playing_ ? &runtime_->Blocks() : nullptr;
        const bool clickable = !playing_;

        const float pad = 6.0f * uiScale_;
        const float dotRadius = 4.0f * uiScale_;
        const float dotGap = 6.0f * uiScale_;
        const float lineHeight = ImGui::GetTextLineHeight();
        const float lineGap = 2.0f * uiScale_;
        const auto foldedSize = [&](const Badge& badge)
        {
            return ImVec2(ImGui::CalcTextSize(badge.summary.c_str()).x + dotRadius * 2.0f + dotGap + pad * 2.0f,
                          lineHeight + pad * 2.0f);
        };
        const auto openSize = [&](const Badge& badge)
        {
            float width = 0.0f;
            for (const Line& line : badge.lines)
            {
                width = std::max(
                    width, FontOf(line.kind)->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, line.text.c_str()).x +
                               (HasDot(line.kind) ? dotRadius * 2.0f + dotGap : 0.0f));
            }
            const float count = static_cast<float>(badge.lines.size());
            return ImVec2(width + pad * 2.0f, count * lineHeight + (count - 1.0f) * lineGap + pad * 2.0f);
        };

        // Every object with logic in front of the camera; its Transform in the game's ECS is where it is now
        std::vector<Badge> badges;
        for (const Engine::Scene::GameObject& object : scene_.Description().objects)
        {
            if (!object.logic || object.logic->scripts.empty())
            {
                continue;
            }
            const Engine::Scene::Entity entity = runtime_->EntityOf(object.id);
            const auto where = entity ? ecs->GetTransform(entity) : std::nullopt;
            if (!where)
            {
                continue; // a Screen Element: it has no place in the world (the Logic window lists it)
            }
            const float top = where->scale * std::max({where->scaleX, where->scaleY, where->scaleZ}) + kAbove;
            const glm::vec4 clip = viewProjection * glm::vec4(where->x, where->y + top, where->z, 1.0f);
            if (clip.w <= 0.1f)
            {
                continue;
            }
            Badge badge{.objectId = object.id,
                        .anchor = ImVec2(position.x + (clip.x / clip.w * 0.5f + 0.5f) * size.x,
                                         position.y + (0.5f - clip.y / clip.w * 0.5f) * size.y - kLift * uiScale_),
                        .depth = clip.w};
            if (badge.anchor.x < position.x || badge.anchor.x > position.x + size.x || badge.anchor.y < position.y ||
                badge.anchor.y > position.y + size.y)
            {
                continue;
            }
            Describe(badge, object, runner, clickable);
            badge.folded = Place(badge, foldedSize(badge), position, size);
            badge.open = Place(badge, openSize(badge), position, size);
            badges.push_back(std::move(badge));
        }
        std::sort(badges.begin(), badges.end(), [](const Badge& a, const Badge& b) { return a.depth > b.depth; });

        // The nearest badge under the mouse is the hovered one; it and the selected object's open up, on top. A badge
        // open last frame is hit as it was drawn, open, so the mouse can move onto its lower lines and click them.
        std::uint32_t hovered = 0;
        if (ImGui::IsWindowHovered())
        {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            for (auto each = badges.rbegin(); each != badges.rend(); ++each)
            {
                const bool wasOpen = each->objectId == logicBadgeHovered_ || each->objectId == scene_.Selected();
                if ((wasOpen ? each->open : each->folded).Contains(mouse))
                {
                    hovered = each->objectId;
                    break;
                }
            }
        }
        logicBadgeHovered_ = hovered;
        const auto isOpen = [&](const Badge& badge)
        { return badge.objectId == hovered || badge.objectId == scene_.Selected(); };
        std::stable_partition(badges.begin(), badges.end(), [&](const Badge& badge) { return !isOpen(badge); });

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(position, ImVec2(position.x + size.x, position.y + size.y), true);
        const ImU32 ink = ImGui::GetColorU32(Theme::kInk);
        for (Badge& badge : badges)
        {
            const bool open = isOpen(badge);
            const Rect& box = open ? badge.open : badge.folded;

            // A Toybox card: white on a hard ink shadow, outlined in ink, mint while running, red with a problem
            const float rounding = 6.0f * uiScale_;
            const float shadow = 2.0f * uiScale_;
            const ImU32 edge = badge.problems > 0 ? ImGui::GetColorU32(Theme::kError)
                               : badge.running    ? ImGui::GetColorU32(Theme::kRun)
                                                  : ink;
            drawList->AddRectFilled(ImVec2(box.min.x + shadow, box.min.y + shadow),
                                    ImVec2(box.max.x + shadow, box.max.y + shadow), ink, rounding);
            drawList->AddRectFilled(box.min, box.max, ImGui::GetColorU32(Theme::kPanel), rounding);
            drawList->AddRect(box.min, box.max, edge, rounding, ImDrawFlags_None,
                              (badge.running || badge.problems > 0 ? 2.5f : 1.5f) * uiScale_);

            ImVec2 at(box.min.x + pad, box.min.y + pad);
            if (!open)
            {
                DrawDot(drawList, ImVec2(at.x + dotRadius, at.y + lineHeight * 0.5f), dotRadius, badge.running,
                        uiScale_);
                drawList->AddText(ImVec2(at.x + dotRadius * 2.0f + dotGap, at.y),
                                  ImGui::GetColorU32(badge.problems > 0 ? Theme::kErrorText : Theme::kText),
                                  badge.summary.c_str());
                continue;
            }
            for (const Line& line : badge.lines)
            {
                float x = at.x;
                if (HasDot(line.kind))
                {
                    DrawDot(drawList, ImVec2(x + dotRadius, at.y + lineHeight * 0.5f), dotRadius,
                            line.kind == LineKind::Running, uiScale_);
                    x += dotRadius * 2.0f + dotGap;
                }
                drawList->AddText(FontOf(line.kind), ImGui::GetFontSize(), ImVec2(x, at.y),
                                  ImGui::GetColorU32(TextColor(line.kind)), line.text.c_str());
                at.y += lineHeight + lineGap;
            }
        }
        drawList->PopClipRect();

        if (hovered != 0 && clickable && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            OpenBlock(hovered, 0);
        }
        return hovered != 0;
    }
}
