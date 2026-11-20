/* !
@file    	    Theme.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/10/2026
@brief
    Implements the editor's look: the ImGui style made from the Atelier3D UI
    design's colours, its bold heading font, and the drawn widgets (icons,
    icon buttons, pill buttons, segmented toggles).

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/Theme.hpp"

#include "engine/platform/Paths.hpp"

#include <imgui/imgui_internal.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <initializer_list>
#include <numbers>
#include <system_error>
#include <utility>

namespace Editor::Theme
{
    namespace
    {
        ImFont* g_boldFont = nullptr;
        ImFont* g_displayFont = nullptr;

        ImVec4 Shade(const ImVec4& color, float factor)
        {
            return ImVec4(std::min(color.x * factor, 1.0f), std::min(color.y * factor, 1.0f),
                          std::min(color.z * factor, 1.0f), color.w);
        }

        ImVec4 WithAlpha(const ImVec4& color, float alpha)
        {
            return ImVec4(color.x, color.y, color.z, color.w * alpha);
        }

        // Draws in the 24 x 24 box the design's SVG icons use, scaled to size and centred on center
        struct Pen
        {
            ImDrawList* drawList;
            ImVec2 center;
            float size;
            ImU32 color;

            ImVec2 P(float x, float y) const
            {
                return ImVec2(center.x + (x - 12.0f) / 24.0f * size, center.y + (y - 12.0f) / 24.0f * size);
            }
            float Thickness() const { return std::max(1.4f, size * 0.095f); }
            void Lines(std::initializer_list<ImVec2> points) const
            {
                for (const ImVec2& point : points)
                {
                    drawList->PathLineTo(P(point.x, point.y));
                }
                drawList->PathStroke(color, ImDrawFlags_None, Thickness());
            }
            void Rect(float x, float y, float w, float h, float rounding, bool filled) const
            {
                const float k = size / 24.0f;
                if (filled)
                {
                    drawList->AddRectFilled(P(x, y), P(x + w, y + h), color, rounding * k);
                }
                else
                {
                    drawList->AddRect(P(x, y), P(x + w, y + h), color, rounding * k, ImDrawFlags_None, Thickness());
                }
            }
        };

        float Scale()
        {
            return ImGui::GetStyle().FontScaleDpi;
        }

        // The design's chunky control: a face outlined in ink on a hard ink shadow depth px down and right. Pressed,
        // the face sinks onto its shadow. Returns how far the face moved, to move what is drawn on it too.
        ImVec2 Chunky(ImDrawList* drawList, ImVec2 min, ImVec2 max, const ImVec4& fill, float rounding, float depth,
                      bool pressed)
        {
            const ImU32 ink = ImGui::GetColorU32(kInk);
            if (depth > 0.0f && !pressed)
            {
                drawList->AddRectFilled(ImVec2(min.x + depth, min.y + depth), ImVec2(max.x + depth, max.y + depth), ink,
                                        rounding);
            }
            const ImVec2 offset = pressed ? ImVec2(depth, depth) : ImVec2(0.0f, 0.0f);
            const ImVec2 faceMin(min.x + offset.x, min.y + offset.y);
            const ImVec2 faceMax(max.x + offset.x, max.y + offset.y);
            drawList->AddRectFilled(faceMin, faceMax, ImGui::GetColorU32(fill), rounding);
            drawList->AddRect(faceMin, faceMax, ink, rounding, ImDrawFlags_None, 2.0f * Scale());
            return offset;
        }
    }

    void Apply(ImGuiStyle& style)
    {
        ImGui::StyleColorsLight(&style);
        style.WindowRounding = 10.0f;
        style.ChildRounding = 16.0f;
        style.FrameRounding = 10.0f;
        style.PopupRounding = 12.0f;
        style.TabRounding = 8.0f;
        style.ScrollbarRounding = 8.0f;
        style.GrabRounding = 8.0f;
        // Windows, fields and buttons get the ink outline (ImGuiCol_Border, kControlEdge)
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.TabBarBorderSize = 1.0f;
        style.WindowPadding = ImVec2(12.0f, 12.0f);
        style.FramePadding = ImVec2(10.0f, 6.0f);
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style.CellPadding = ImVec2(6.0f, 4.0f);
        style.IndentSpacing = 18.0f;
        style.ScrollbarSize = 12.0f;
        style.GrabMinSize = 10.0f;
        // Docked windows sit apart on the ground colour, like the design's panels
        style.DockingSeparatorSize = 10.0f;
        style.WindowMenuButtonPosition = ImGuiDir_None;
        // A narrow dock shrinks its tabs further before it scrolls them, so the open tab stays in view
        style.TabMinWidthShrink = 48.0f;

        ImVec4* c = style.Colors;
        c[ImGuiCol_Text] = kText;
        c[ImGuiCol_TextDisabled] = kTextMuted;
        c[ImGuiCol_WindowBg] = kPanel;
        c[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        c[ImGuiCol_PopupBg] = kPanel;
        c[ImGuiCol_Border] = kControlEdge;
        c[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        c[ImGuiCol_FrameBg] = kGround;
        c[ImGuiCol_FrameBgHovered] = kRaised;
        c[ImGuiCol_FrameBgActive] = kRaisedHigh;
        c[ImGuiCol_TitleBg] = kHeader;
        c[ImGuiCol_TitleBgActive] = kHeader;
        c[ImGuiCol_TitleBgCollapsed] = kHeader;
        c[ImGuiCol_MenuBarBg] = kHeader;
        c[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        c[ImGuiCol_ScrollbarGrab] = kBorderStrong;
        c[ImGuiCol_ScrollbarGrabHovered] = kTextMuted;
        c[ImGuiCol_ScrollbarGrabActive] = kInk;
        c[ImGuiCol_CheckMark] = kInk;
        c[ImGuiCol_SliderGrab] = kSelect;
        c[ImGuiCol_SliderGrabActive] = kLink;
        c[ImGuiCol_Button] = kRaised;
        c[ImGuiCol_ButtonHovered] = kRaisedHigh;
        c[ImGuiCol_ButtonActive] = kEvents;
        c[ImGuiCol_Header] = kRaised;
        c[ImGuiCol_HeaderHovered] = kRaisedHigh;
        c[ImGuiCol_HeaderActive] = kEvents;
        c[ImGuiCol_Separator] = kBorder;
        c[ImGuiCol_SeparatorHovered] = WithAlpha(kSelect, 0.6f);
        c[ImGuiCol_SeparatorActive] = kSelect;
        c[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        c[ImGuiCol_ResizeGripHovered] = WithAlpha(kSelect, 0.5f);
        c[ImGuiCol_ResizeGripActive] = kSelect;
        c[ImGuiCol_Tab] = kHeader;
        c[ImGuiCol_TabHovered] = kRaisedHigh;
        c[ImGuiCol_TabSelected] = kPanel;
        c[ImGuiCol_TabSelectedOverline] = kSelect;
        c[ImGuiCol_TabDimmed] = kHeader;
        c[ImGuiCol_TabDimmedSelected] = kPanel;
        c[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        c[ImGuiCol_DockingPreview] = WithAlpha(kSelect, 0.5f);
        c[ImGuiCol_DockingEmptyBg] = kGround;
        c[ImGuiCol_PlotLines] = kSelect;
        c[ImGuiCol_PlotHistogram] = kEvents;
        c[ImGuiCol_TableHeaderBg] = kRaised;
        c[ImGuiCol_TableBorderStrong] = kBorderStrong;
        c[ImGuiCol_TableBorderLight] = kBorder;
        c[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        c[ImGuiCol_TableRowBgAlt] = WithAlpha(kInk, 0.04f);
        c[ImGuiCol_TextLink] = kLink;
        c[ImGuiCol_TextSelectedBg] = WithAlpha(kSelect, 0.25f);
        c[ImGuiCol_DragDropTarget] = kSelect;
        c[ImGuiCol_NavCursor] = kSelect;
        c[ImGuiCol_ModalWindowDimBg] = WithAlpha(kInk, 0.45f);
    }

    void LoadFonts()
    {
        if (g_boldFont != nullptr)
        {
            return;
        }
        // The first of the candidates that loads (the editor's own fonts, copied next to the exe, then the system's)
        const auto load = [](std::initializer_list<std::filesystem::path> candidates) -> ImFont*
        {
            for (const std::filesystem::path& candidate : candidates)
            {
                std::error_code ec;
                if (std::filesystem::is_regular_file(candidate, ec))
                {
                    if (ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(candidate.string().c_str(), 16.0f))
                    {
                        return font;
                    }
                }
            }
            return nullptr;
        };
        const std::filesystem::path fonts = Engine::Platform::ExecutableDir() / "assets" / "fonts";
        g_boldFont =
            load({fonts / "DMSans-Bold.ttf", "C:/Windows/Fonts/segoeuib.ttf", "C:/Windows/Fonts/tahomabd.ttf"});
        g_displayFont = load({fonts / "BricolageGrotesque-ExtraBold.ttf"});
        if (g_displayFont == nullptr)
        {
            g_displayFont = g_boldFont;
        }
    }

    ImFont* BoldFont()
    {
        return g_boldFont;
    }

    ImFont* DisplayFont()
    {
        return g_displayFont;
    }

    void Text(const char* text, float size, const ImVec4& color, bool bold)
    {
        ImGui::PushFont(!bold ? nullptr : size >= 18.0f ? g_displayFont : g_boldFont, size);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }

    void TextWrapped(const char* text, float size, const ImVec4& color)
    {
        ImGui::PushFont(nullptr, size);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s", text);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }

    void Label(const char* text)
    {
        Text(text, 12.0f, kTextMuted, true);
    }

    void DrawIcon(ImDrawList* drawList, Icon icon, ImVec2 center, float size, ImU32 color)
    {
        const Pen pen{drawList, center, size, color};
        switch (icon)
        {
        case Icon::Back: pen.Lines({{15, 5}, {8, 12}, {15, 19}}); break;
        case Icon::Minus: pen.Lines({{5, 12}, {19, 12}}); break;
        case Icon::Undo:
        case Icon::Redo:
        {
            // A hooked arrow; Redo is Undo mirrored
            const float m = icon == Icon::Redo ? -1.0f : 1.0f;
            const auto X = [m](float x) { return 12.0f + (x - 12.0f) * m; };
            drawList->PathLineTo(pen.P(X(4), 10));
            drawList->PathLineTo(pen.P(X(15), 10));
            drawList->PathBezierCubicCurveTo(pen.P(X(18), 10), pen.P(X(20), 12), pen.P(X(20), 15));
            drawList->PathBezierCubicCurveTo(pen.P(X(20), 18), pen.P(X(18), 20), pen.P(X(15), 20));
            drawList->PathLineTo(pen.P(X(9), 20));
            drawList->PathStroke(color, ImDrawFlags_None, pen.Thickness());
            pen.Lines({{X(8), 6}, {X(4), 10}, {X(8), 14}});
            break;
        }
        case Icon::Play: drawList->AddTriangleFilled(pen.P(7, 4), pen.P(20, 12), pen.P(7, 20), color); break;
        case Icon::Pause:
            pen.Rect(6, 5, 4, 14, 1, true);
            pen.Rect(14, 5, 4, 14, 1, true);
            break;
        case Icon::Stop: pen.Rect(5, 5, 14, 14, 2, true); break;
        case Icon::Step:
            drawList->AddTriangleFilled(pen.P(5, 4), pen.P(15, 12), pen.P(5, 20), color);
            pen.Rect(16, 5, 3, 14, 1, true);
            break;
        case Icon::Export:
            pen.Lines({{12, 3}, {12, 15}});
            pen.Lines({{7, 10}, {12, 15}, {17, 10}});
            pen.Lines({{4, 17}, {4, 20}, {20, 20}, {20, 17}});
            break;
        case Icon::Plus:
            pen.Lines({{12, 5}, {12, 19}});
            pen.Lines({{5, 12}, {19, 12}});
            break;
        case Icon::Move:
            pen.Lines({{12, 3}, {12, 21}});
            pen.Lines({{3, 12}, {21, 12}});
            pen.Lines({{9, 6}, {12, 3}, {15, 6}});
            pen.Lines({{9, 18}, {12, 21}, {15, 18}});
            pen.Lines({{6, 9}, {3, 12}, {6, 15}});
            pen.Lines({{18, 9}, {21, 12}, {18, 15}});
            break;
        case Icon::Rotate:
            // Most of a circle, from the right going down and round to the top right, with its arrowhead
            drawList->PathArcTo(pen.P(12, 12), 8.0f / 24.0f * size, 0.0f, 2.0f * std::numbers::pi_v<float> - 0.89f, 24);
            drawList->PathStroke(color, ImDrawFlags_None, pen.Thickness());
            pen.Lines({{20, 4}, {20, 9}, {15, 9}});
            break;
        case Icon::Scale:
            pen.Rect(4, 10, 10, 10, 1, false);
            pen.Lines({{12, 4}, {20, 4}, {20, 12}});
            pen.Lines({{20, 4}, {13, 11}});
            break;
        case Icon::More:
            for (const float x : {5.0f, 12.0f, 19.0f})
            {
                drawList->AddCircleFilled(pen.P(x, 12), 2.0f / 24.0f * size, color);
            }
            break;
        case Icon::Home:
            pen.Lines({{3, 11}, {12, 4}, {21, 11}});
            pen.Lines({{6, 9}, {6, 20}, {18, 20}, {18, 9}});
            break;
        case Icon::Arrow:
            pen.Lines({{5, 12}, {19, 12}});
            pen.Lines({{13, 6}, {19, 12}, {13, 18}});
            break;
        case Icon::Check: pen.Lines({{5, 12}, {10, 17}, {19, 7}}); break;
        case Icon::Learn:
            pen.Lines({{3, 6}, {12, 2}, {21, 6}, {12, 10}, {3, 6}});
            drawList->PathLineTo(pen.P(7, 8));
            drawList->PathLineTo(pen.P(7, 13));
            drawList->PathBezierCubicCurveTo(pen.P(7, 15), pen.P(17, 15), pen.P(17, 13));
            drawList->PathLineTo(pen.P(17, 8));
            drawList->PathStroke(color, ImDrawFlags_None, pen.Thickness());
            pen.Lines({{21, 6}, {21, 12}});
            break;
        case Icon::Create:
            pen.Lines({{12, 3}, {20, 7.5f}, {20, 16.5f}, {12, 21}, {4, 16.5f}, {4, 7.5f}, {12, 3}});
            pen.Lines({{12, 9}, {12, 15}});
            pen.Lines({{9, 12}, {15, 12}});
            break;
        case Icon::Watch:
            // An eye: two arcs meeting at the corners, and the pupil
            drawList->PathLineTo(pen.P(2, 12));
            drawList->PathBezierCubicCurveTo(pen.P(7, 4.5f), pen.P(17, 4.5f), pen.P(22, 12));
            drawList->PathBezierCubicCurveTo(pen.P(17, 19.5f), pen.P(7, 19.5f), pen.P(2, 12));
            drawList->PathStroke(color, ImDrawFlags_None, pen.Thickness());
            drawList->AddCircleFilled(pen.P(12, 12), 3.0f / 24.0f * size, color);
            break;
        case Icon::Logo:
            pen.Lines({{12, 2}, {21.2f, 7}, {21.2f, 17}, {12, 22}, {2.8f, 17}, {2.8f, 7}, {12, 2}});
            pen.Lines({{2.8f, 7}, {12, 12}, {21.2f, 7}});
            pen.Lines({{12, 12}, {12, 22}});
            break;
        case Icon::Flag:
            pen.Lines({{5, 21}, {5, 3}});
            pen.Lines({{5, 4}, {18, 4}, {15, 8.5f}, {18, 13}, {5, 13}});
            break;
        case Icon::Eye:
            drawList->PathLineTo(pen.P(2, 12));
            drawList->PathBezierCubicCurveTo(pen.P(7, 4.5f), pen.P(17, 4.5f), pen.P(22, 12));
            drawList->PathBezierCubicCurveTo(pen.P(17, 19.5f), pen.P(7, 19.5f), pen.P(2, 12));
            drawList->PathStroke(color, ImDrawFlags_None, pen.Thickness());
            drawList->AddCircleFilled(pen.P(12, 12), 3.0f / 24.0f * size, color);
            break;
        case Icon::Diamond: pen.Lines({{12, 3}, {21, 12}, {12, 21}, {3, 12}, {12, 3}}); break;
        case Icon::Box:
            pen.Rect(3, 5, 18, 14, 3, false);
            pen.Lines({{9, 9}, {15, 15}});
            pen.Lines({{15, 9}, {9, 15}});
            break;
        case Icon::Waves:
            drawList->AddCircleFilled(pen.P(5, 12), 2.2f / 24.0f * size, color);
            for (const float radius : {7.0f, 13.0f})
            {
                drawList->PathArcTo(pen.P(5, 12), radius / 24.0f * size, -0.8f, 0.8f, 12);
                drawList->PathStroke(color, ImDrawFlags_None, pen.Thickness());
            }
            break;
        case Icon::Heart:
            drawList->AddCircleFilled(pen.P(8.2f, 9.5f), 4.6f / 24.0f * size, color);
            drawList->AddCircleFilled(pen.P(15.8f, 9.5f), 4.6f / 24.0f * size, color);
            drawList->AddTriangleFilled(pen.P(3.9f, 11.6f), pen.P(20.1f, 11.6f), pen.P(12, 20.5f), color);
            break;
        }
    }

    void DockedCard()
    {
        const ImGuiWindow* window = ImGui::GetCurrentWindow();
        const ImGuiDockNode* node = window->DockNode;
        if (!window->DockIsActive || node == nullptr)
        {
            return;
        }
        const float rounding = ImGui::GetStyle().ChildRounding;
        const float depth = 4.0f * Scale();
        const ImVec2 min = node->Pos;
        const ImVec2 max(node->Pos.x + node->Size.x, node->Pos.y + node->Size.y);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRectFullScreen();

        // The shadow: the rounded card moved down and right, clipped to the gap beside the card, as anything drawn here
        // is over the card itself
        const ImU32 ink = ImGui::GetColorU32(kInk);
        const auto shadowIn = [&](ImVec2 clipMin, ImVec2 clipMax)
        {
            drawList->PushClipRect(clipMin, clipMax, false);
            drawList->AddRectFilled(ImVec2(min.x + depth, min.y + depth), ImVec2(max.x + depth, max.y + depth), ink,
                                    rounding);
            drawList->PopClipRect();
        };
        shadowIn(ImVec2(max.x, min.y), ImVec2(max.x + depth, max.y + depth));
        shadowIn(ImVec2(min.x, max.y), ImVec2(max.x, max.y + depth));

        // A docked window has square corners: each is filled in with what shows behind a rounded card there (the
        // shadow in the bottom right one, the ground in the others), then the outline goes round. A fan from the
        // corner, as the area between a corner and its arc is not convex.
        const auto corner = [&](ImVec2 at, ImVec2 centre, float fromAngle, const ImVec4& colour)
        {
            constexpr int kSegments = 8;
            const ImU32 fill = ImGui::GetColorU32(colour);
            const auto onArc = [&](int step)
            {
                const float angle = fromAngle + IM_PI * 0.5f * static_cast<float>(step) / kSegments;
                return ImVec2(centre.x + std::cos(angle) * rounding, centre.y + std::sin(angle) * rounding);
            };
            for (int step = 0; step < kSegments; ++step)
            {
                drawList->AddTriangleFilled(at, onArc(step), onArc(step + 1), fill);
            }
        };
        corner(min, ImVec2(min.x + rounding, min.y + rounding), IM_PI, kGround);
        corner(ImVec2(max.x, min.y), ImVec2(max.x - rounding, min.y + rounding), IM_PI * 1.5f, kGround);
        corner(max, ImVec2(max.x - rounding, max.y - rounding), 0.0f, kGround);
        // The shadow in that corner stops at its own curve, which passes just inside the square corner
        const float shadowEdge = depth - rounding * (1.0f - 0.7071f);
        corner(ImVec2(max.x + shadowEdge, max.y + shadowEdge), ImVec2(max.x - rounding, max.y - rounding), 0.0f, kInk);
        corner(ImVec2(min.x, max.y), ImVec2(min.x + rounding, max.y - rounding), IM_PI * 0.5f, kGround);
        drawList->AddRect(min, max, ImGui::GetColorU32(kBorder), rounding, ImDrawFlags_None, 2.0f * Scale());
        drawList->PopClipRect();
    }

    void FocusRing()
    {
        ImGui::RenderNavCursor(ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()), ImGui::GetItemID());
    }

    bool IconButton(const char* id, Icon icon, float size, const char* tooltip, bool active, bool enabled, bool dimmed)
    {
        ImGui::BeginDisabled(!enabled);
        const bool clicked = ImGui::InvisibleButton(id, ImVec2(size, size), ImGuiButtonFlags_EnableNav);
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const bool held = ImGui::IsItemActive();
        const ImVec4 background = active ? kRaisedHigh : hovered ? kRaised : kPanel;
        // Disabled and dimmed buttons (nothing to pause yet) lie flat; held, the shadow shrinks under the pointer
        const float depth = !enabled || dimmed ? 0.0f : held ? 1.0f * Scale() : 3.0f * Scale();
        Chunky(drawList, min, max, background, size * 0.27f, depth, false);
        const float alpha = dimmed ? 0.55f : 1.0f;
        DrawIcon(drawList, icon, ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f), size * 0.45f,
                 ImGui::GetColorU32(WithAlpha(kText, alpha)));
        FocusRing();
        ImGui::EndDisabled();
        if (tooltip != nullptr &&
            ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("%s", tooltip);
        }
        return clicked;
    }

    float PillWidth(const char* label, float height, bool hasIcon)
    {
        ImGui::PushFont(g_boldFont, 15.0f);
        const float text = ImGui::CalcTextSize(label, nullptr, true).x;
        ImGui::PopFont();
        const float s = Scale();
        return text + 36.0f * s + (hasIcon ? height * 0.4f + 8.0f * s : 0.0f);
    }

    bool PillButton(const char* label, const ImVec4& background, const ImVec4& text, float height, float width,
                    bool hasIcon, Icon icon, bool enabled)
    {
        const float s = Scale();
        const float w = width > 0.0f ? width : PillWidth(label, height, hasIcon);
        ImGui::BeginDisabled(!enabled);
        const bool clicked = ImGui::InvisibleButton(label, ImVec2(w, height), ImGuiButtonFlags_EnableNav);
        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        const ImVec2 min = ImGui::GetItemRectMin();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        // Hovered a touch darker; held, it sinks onto its shadow; disabled, it lies flat
        const ImVec4 fill = hovered && !held ? Shade(background, 0.94f) : background;
        const ImVec2 offset =
            Chunky(drawList, min, ImVec2(min.x + w, min.y + height), fill, 12.0f * s, enabled ? 3.0f * s : 0.0f, held);

        ImGui::PushFont(g_boldFont, 15.0f);
        const char* end = ImGui::FindRenderedTextEnd(label);
        const ImVec2 textSize = ImGui::CalcTextSize(label, end);
        const float iconSize = hasIcon ? height * 0.4f : 0.0f;
        const float gap = hasIcon ? 8.0f * s : 0.0f;
        float x = min.x + offset.x + (w - (textSize.x + iconSize + gap)) * 0.5f;
        const float centreY = min.y + offset.y + height * 0.5f;
        const ImU32 ink = ImGui::GetColorU32(text);
        if (hasIcon)
        {
            DrawIcon(drawList, icon, ImVec2(x + iconSize * 0.5f, centreY), iconSize, ink);
            x += iconSize + gap;
        }
        drawList->AddText(ImVec2(x, centreY - textSize.y * 0.5f), ink, label, end);
        ImGui::PopFont();
        FocusRing();
        ImGui::EndDisabled();
        return clicked;
    }

    float SegmentedWidth(const char* const* labels, int count, float height)
    {
        ImGui::PushFont(g_boldFont, 14.0f);
        float widest = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            widest = std::max(widest, ImGui::CalcTextSize(labels[i], nullptr, true).x);
        }
        ImGui::PopFont();
        const float s = Scale();
        const float pad = std::max(4.0f * s, height * 0.1f);
        return pad * 2.0f + static_cast<float>(count) * (widest + 28.0f * s) + static_cast<float>(count - 1) * 4.0f * s;
    }

    bool Segmented(const char* id, const char* const* labels, int count, int& current, float width, float height)
    {
        const float s = Scale();
        const float w = width > 0.0f ? width : SegmentedWidth(labels, count, height);
        const float pad = std::max(4.0f * s, height * 0.1f);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        // A white track outlined in ink; the chosen segment is a sun-yellow tab outlined in ink
        const ImU32 ink = ImGui::GetColorU32(kInk);
        drawList->AddRectFilled(origin, ImVec2(origin.x + w, origin.y + height), ImGui::GetColorU32(kPanel), 12.0f * s);
        drawList->AddRect(origin, ImVec2(origin.x + w, origin.y + height), ink, 12.0f * s, ImDrawFlags_None, 2.0f * s);

        ImGui::PushID(id);
        ImGui::PushFont(g_boldFont, 14.0f);
        const float gap = 4.0f * s;
        const float segment = (w - pad * 2.0f - gap * static_cast<float>(count - 1)) / static_cast<float>(count);
        bool changed = false;
        for (int i = 0; i < count; ++i)
        {
            const ImVec2 min(origin.x + pad + static_cast<float>(i) * (segment + gap), origin.y + pad);
            const ImVec2 max(min.x + segment, origin.y + height - pad);
            ImGui::SetCursorScreenPos(min);
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##segment", ImVec2(segment, max.y - min.y), ImGuiButtonFlags_EnableNav) &&
                current != i)
            {
                current = i;
                changed = true;
            }
            const bool hovered = ImGui::IsItemHovered();
            const ImGuiID segmentId = ImGui::GetItemID();
            ImGui::PopID();
            const bool selected = current == i;
            if (selected || hovered)
            {
                drawList->AddRectFilled(min, max, ImGui::GetColorU32(selected ? kEvents : kRaised), 9.0f * s);
            }
            if (selected)
            {
                drawList->AddRect(min, max, ink, 9.0f * s, ImDrawFlags_None, 1.5f * s);
            }
            const char* end = ImGui::FindRenderedTextEnd(labels[i]);
            const ImVec2 text = ImGui::CalcTextSize(labels[i], end);
            drawList->AddText(ImVec2(min.x + (segment - text.x) * 0.5f, (min.y + max.y - text.y) * 0.5f),
                              ImGui::GetColorU32(selected ? kText : kTextSoft), labels[i], end);
            ImGui::RenderNavCursor(ImRect(min, max), segmentId);
        }
        ImGui::PopFont();
        ImGui::PopID();
        // Leave the cursor below the control, as an ordinary item would
        ImGui::SetCursorScreenPos(origin);
        ImGui::Dummy(ImVec2(w, height));
        return changed;
    }

    void DrawMascot(ImDrawList* drawList, ImVec2 center, float size, bool happy, ImVec4 tint, float alpha)
    {
        // The logo's cube in the 24 x 24 icon box: a lit top, a sunny left face (with the eyes) and a shaded right
        const auto P = [&](float x, float y)
        { return ImVec2(center.x + (x - 12.0f) / 24.0f * size, center.y + (y - 12.0f) / 24.0f * size); };
        // A face's colour tinted towards tint (keeping its light: shade scales the tint), and faded by alpha
        const auto face = [&](ImVec4 own, float shade)
        {
            const float k = tint.w;
            return ImGui::GetColorU32(ImVec4(own.x + (tint.x * shade - own.x) * k, own.y + (tint.y * shade - own.y) * k,
                                             own.z + (tint.z * shade - own.z) * k, alpha));
        };
        const ImU32 ink = ImGui::GetColorU32(ImVec4(kInk.x, kInk.y, kInk.z, alpha));
        const ImVec2 top(P(12, 2)), topRight(P(21.2f, 7)), bottomRight(P(21.2f, 17)), bottom(P(12, 22)),
            bottomLeft(P(2.8f, 17)), topLeft(P(2.8f, 7)), middle(P(12, 12));
        drawList->AddQuadFilled(top, topRight, middle, topLeft, face(Hex(0xFFD866), 1.0f));
        drawList->AddQuadFilled(topLeft, middle, bottom, bottomLeft, face(kEvents, 0.85f));
        drawList->AddQuadFilled(middle, topRight, bottomRight, bottom, face(Hex(0xE0A200), 0.65f));
        const float thickness = std::max(1.5f, size * 0.045f);
        const std::array outline{top, topRight, bottomRight, bottom, bottomLeft, topLeft};
        drawList->AddPolyline(outline.data(), static_cast<int>(outline.size()), ink, ImDrawFlags_Closed, thickness);
        drawList->AddLine(topLeft, middle, ink, thickness);
        drawList->AddLine(middle, topRight, ink, thickness);
        drawList->AddLine(middle, bottom, ink, thickness);
        for (const ImVec2 eye : {P(5.6f, 13.6f), P(9.2f, 15.6f)})
        {
            if (happy)
            {
                // Smiling eyes: little upside-down U's
                drawList->PathArcTo(ImVec2(eye.x, eye.y + size * 0.02f), size * 0.045f, std::numbers::pi_v<float>,
                                    2.0f * std::numbers::pi_v<float>, 8);
                drawList->PathStroke(ink, ImDrawFlags_None, thickness);
            }
            else
            {
                drawList->AddEllipseFilled(eye, ImVec2(size * 0.04f, size * 0.06f), ink);
            }
        }
    }

    bool AnimationsOn()
    {
#ifdef _WIN32
        BOOL on = TRUE;
        return SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &on, 0) == FALSE || on != FALSE;
#else
        return true;
#endif
    }

    void EmptyState(const char* text, float size)
    {
        const float mascot = 44.0f * Scale();
        const ImVec2 at = ImGui::GetCursorScreenPos();
        DrawMascot(ImGui::GetWindowDrawList(), ImVec2(at.x + mascot * 0.5f, at.y + mascot * 0.5f), mascot);
        ImGui::Dummy(ImVec2(mascot, mascot));
        ImGui::SameLine(0.0f, 10.0f * Scale());
        TextWrapped(text, size, kTextMuted);
    }

    namespace
    {
        struct Celebration
        {
            std::string title;
            std::string detail;
            int frame = -1; // the frame it was asked for; below 0: none
            bool started = false;
            std::chrono::steady_clock::time_point start; // when it began showing (wall time: the editor's frame
                                                         // times stretch while it is busy)
        };
        Celebration g_celebration;

        constexpr float kCelebrationSeconds = 3.5f;

        // A repeatable 0..1 value for confetti piece i (no state: the same piece looks the same every frame)
        float Noise(int i, float salt)
        {
            const float value = std::sin(static_cast<float>(i) * 12.9898f + salt * 78.233f) * 43758.5453f;
            return value - std::floor(value);
        }
    }

    void Celebrate(std::string title, std::string detail)
    {
        // Its clock starts next frame: what is celebrated (an export) can hold up this one for seconds
        g_celebration = {std::move(title), std::move(detail), ImGui::GetFrameCount(), false, {}};
    }

    void DrawCelebration()
    {
        if (g_celebration.frame < 0 || ImGui::GetFrameCount() <= g_celebration.frame)
        {
            return;
        }
        if (!g_celebration.started)
        {
            g_celebration.started = true;
            g_celebration.start = std::chrono::steady_clock::now();
        }
        const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - g_celebration.start).count();
        if (t > kCelebrationSeconds)
        {
            g_celebration = {};
            return;
        }
        const bool moving = AnimationsOn();
        const float s = Scale();
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImFont* titleFont = g_displayFont != nullptr ? g_displayFont : ImGui::GetFont();
        ImFont* bodyFont = ImGui::GetIO().Fonts->Fonts[0];
        const float titleSize = 22.0f * s;
        const float bodySize = 15.0f * s;
        const ImVec2 titleText = titleFont->CalcTextSizeA(titleSize, FLT_MAX, 0.0f, g_celebration.title.c_str());
        const ImVec2 bodyText = bodyFont->CalcTextSizeA(bodySize, FLT_MAX, 0.0f, g_celebration.detail.c_str());

        // The sticker: top centre, under the top bar, sliding down into place
        const float mascot = 52.0f * s;
        const float pad = 16.0f * s;
        const ImVec2 size(pad * 3.0f + mascot + std::max(titleText.x, bodyText.x), pad * 2.0f + mascot);
        const float slide = moving ? std::max(0.0f, 1.0f - t / 0.25f) : 0.0f;
        const ImVec2 min(viewport->WorkPos.x + (viewport->WorkSize.x - size.x) * 0.5f,
                         viewport->WorkPos.y + 84.0f * s - slide * slide * 40.0f * s);
        const ImVec2 max(min.x + size.x, min.y + size.y);

        // Confetti in the block colours, thrown up from the sticker and falling (the first 1.8 s)
        if (moving && t < 1.8f)
        {
            constexpr std::array colors{kEvents, kMovement, kVariables, kConditions,
                                        kLoops,  kObjects,  kVisuals,   kAudio};
            for (int i = 0; i < 36; ++i)
            {
                const float x0 = min.x + Noise(i, 1.0f) * size.x;
                const float vx = (Noise(i, 2.0f) - 0.5f) * 220.0f * s;
                const float vy = -(140.0f + Noise(i, 3.0f) * 160.0f) * s;
                const ImVec2 at(x0 + vx * t, max.y + vy * t + 0.5f * 600.0f * s * t * t);
                const float angle = Noise(i, 4.0f) * 6.28f + t * (4.0f + Noise(i, 5.0f) * 6.0f);
                const ImVec2 a(std::cos(angle) * 5.0f * s, std::sin(angle) * 5.0f * s);
                const ImVec2 b(-std::sin(angle) * 2.5f * s, std::cos(angle) * 2.5f * s);
                drawList->AddQuadFilled(
                    ImVec2(at.x - a.x - b.x, at.y - a.y - b.y), ImVec2(at.x + a.x - b.x, at.y + a.y - b.y),
                    ImVec2(at.x + a.x + b.x, at.y + a.y + b.y), ImVec2(at.x - a.x + b.x, at.y - a.y + b.y),
                    ImGui::GetColorU32(colors[static_cast<std::size_t>(i) % colors.size()]));
            }
        }

        Chunky(drawList, min, max, kEvents, 16.0f * s, 4.0f * s, false);
        DrawMascot(drawList, ImVec2(min.x + pad + mascot * 0.5f, min.y + pad + mascot * 0.5f), mascot, true);
        const float textX = min.x + pad * 2.0f + mascot;
        const float textTop = min.y + (size.y - titleText.y - bodyText.y) * 0.5f;
        const ImU32 ink = ImGui::GetColorU32(kInk);
        drawList->AddText(titleFont, titleSize, ImVec2(textX, textTop), ink, g_celebration.title.c_str());
        drawList->AddText(bodyFont, bodySize, ImVec2(textX, textTop + titleText.y), ink, g_celebration.detail.c_str());
    }
}
