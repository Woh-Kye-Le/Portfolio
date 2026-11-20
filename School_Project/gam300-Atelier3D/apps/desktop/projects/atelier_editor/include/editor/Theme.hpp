/* !
@file    	    Theme.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/10/2026
@brief
    Declares the editor's look (the Atelier3D UI design): its colours, the
    ImGui style built from them, and the small drawn widgets both interfaces
    share (icon buttons, pill buttons, segmented toggles, section labels).

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <imgui/imgui.h>

#include <cstdint>
#include <string>

namespace Editor::Theme
{
    // 0xRRGGBB as an ImGui colour
    constexpr ImVec4 Hex(std::uint32_t rgb, float alpha = 1.0f)
    {
        return ImVec4(static_cast<float>((rgb >> 16) & 0xFFu) / 255.0f, static_cast<float>((rgb >> 8) & 0xFFu) / 255.0f,
                      static_cast<float>(rgb & 0xFFu) / 255.0f, alpha);
    }

    // "Toybox": warm paper, white cards, ink outlines with hard shadows, flat bright colours. Every text colour is
    // 4.5:1 on the ground, a panel and a raised control; edges are 3:1 or more (WCAG AA).

    // ---- Surfaces: ground (behind everything), panels on it, raised controls on panels
    constexpr ImVec4 kGround = Hex(0xFFF5E1);
    constexpr ImVec4 kHeader = Hex(0xFFFDF7); // the top bar, and the block canvas
    constexpr ImVec4 kPanel = Hex(0xFFFFFF);
    constexpr ImVec4 kRaised = Hex(0xFFF0D2);
    constexpr ImVec4 kRaisedHigh = Hex(0xFFD966);   // the selected row or segment, and a hovered raised control
    constexpr ImVec4 kBorder = Hex(0x1B1B2F);       // the ink outline round cards, panels, fields and separators
    constexpr ImVec4 kBorderStrong = Hex(0xD2C29C); // the quiet edge: canvas dots, locked steps, pressed fills
    // The edge of a field, checkbox or button: 3:1 on a panel or the ground, so it can be told apart (WCAG 1.4.11)
    constexpr ImVec4 kControlEdge = kBorder;

    // ---- Text: body, secondary (labels, hints), muted (still 4.5:1 on a raised control)
    constexpr ImVec4 kText = Hex(0x1B1B2F);
    constexpr ImVec4 kTextSoft = Hex(0x3E3D55);
    constexpr ImVec4 kTextMuted = Hex(0x5C5A72);
    constexpr ImVec4 kInk = Hex(0x1B1B2F); // dark text on the bright colours, the outlines and the hard shadows

    // ---- Signals: mint means "running now", red an error, cobalt the selection, focus and links. The *Text
    // versions are the same signals dark enough to be read as text.
    constexpr ImVec4 kRun = Hex(0x2BD98F);
    constexpr ImVec4 kError = Hex(0xE5484D);
    constexpr ImVec4 kErrorText = Hex(0xB4231B);
    constexpr ImVec4 kSuccessText = Hex(0x11734A);
    constexpr ImVec4 kWarningText = Hex(0x8A5A00);
    constexpr ImVec4 kSelect = Hex(0x2456E8);
    constexpr ImVec4 kLink = Hex(0x2456E8);

    // ---- Block categories (also used to tell the modes and objects apart); fills, with kInk on them. Those named
    // after a block category are that category's colour (Engine::Logic::CategoryColor): keep them the same.
    constexpr ImVec4 kEvents = Hex(0xFFC21A);
    constexpr ImVec4 kMovement = Hex(0x4C97FF);
    constexpr ImVec4 kPhysics = Hex(0x2BD9C0);
    constexpr ImVec4 kVariables = Hex(0xFF5F57);
    constexpr ImVec4 kConditions = Hex(0xE466D6);
    constexpr ImVec4 kLoops = Hex(0xFF8A1F);
    constexpr ImVec4 kObjects = Hex(0x2BD98F);
    constexpr ImVec4 kAudio = Hex(0xFF8FB8);
    constexpr ImVec4 kVisuals = Hex(0x38C6E8);

    // ---- Axes (X / Y / Z fields)
    constexpr ImVec4 kAxisX = Hex(0xC2362F);
    constexpr ImVec4 kAxisY = Hex(0x17773A);
    constexpr ImVec4 kAxisZ = Hex(0x2D62C9);

    // The design's ImGui style: light surfaces, ink-outlined panels and fields. Sizes are at 100% scale (the editor
    // scales them with the display).
    void Apply(ImGuiStyle& style);

    // A bold face of the UI font (DM Sans Bold) for labels and buttons, and the display face (Bricolage Grotesque
    // ExtraBold) for headings; nullptr: not found, the regular face is used (DisplayFont falls back to BoldFont).
    // Loaded once by LoadFonts, after ImGui has started.
    void LoadFonts();
    ImFont* BoldFont();
    ImFont* DisplayFont();

    // Text at a size (in unscaled pixels, like the design) and colour, optionally bold (18 px and up: the display face)
    void Text(const char* text, float size, const ImVec4& color, bool bold = false);
    // TextWrapped at a size and colour
    void TextWrapped(const char* text, float size, const ImVec4& color);
    // A small upper-case label above a group ("POSITION", "WHAT CONTROLS ROBO")
    void Label(const char* text);

    // The drawn icons (the design's stroke icons; ImGui has no icon font here)
    enum class Icon : std::uint8_t
    {
        Back,
        Minus,
        Undo,
        Redo,
        Play,
        Pause,
        Stop,
        Step, // play and a bar: run one block
        Export,
        Plus,
        Move,
        Rotate,
        Scale,
        More,
        Home,
        Arrow,  // pointing right ("Continue")
        Check,  // a lesson done
        Learn,  // a mortarboard
        Create, // a hexagon with a plus
        Watch,  // an eye: the Variable Inspector
        Logo,   // Atelier3D's cube
        // The block categories' marks (with Arrow for Motion, Rotate for Loops and Plus for Math), so a block's group
        // shows by more than its colour
        Flag,    // Events
        Eye,     // Looks
        Diamond, // Conditions: a flowchart's decision
        Box,     // Variables: a box with an x in it
        Waves,   // Sensing
        Heart    // Health
    };
    void DrawIcon(ImDrawList* drawList, Icon icon, ImVec2 center, float size, ImU32 color);

    // Atti, the logo's cube with a face: in empty states, celebrations and the block previews. happy: smiling eyes;
    // tint: a colour its faces are tinted towards, by tint.w (0: its own colours); alpha fades all of it.
    void DrawMascot(ImDrawList* drawList, ImVec2 center, float size, bool happy = false,
                    ImVec4 tint = ImVec4(0, 0, 0, 0), float alpha = 1.0f);
    // Windows' "Show animations" setting (Accessibility > Visual effects): off means nothing should move
    bool AnimationsOn();
    // An empty place's message, with Atti beside it (text size unscaled, as for Text)
    void EmptyState(const char* text, float size);

    // A win worth marking (a game exported, a lesson done): a sticker with Atti, the title and a line, and a burst of
    // the block colours (no movement when Windows' "Show animations" is off). DrawCelebration, called once a frame,
    // shows it over everything for a few seconds.
    void Celebrate(std::string title, std::string detail);
    void DrawCelebration();

    // Makes the docked window being drawn look like one of the design's cards (rounded, ink outlined, on a hard
    // shadow). Call it after drawing the window's contents; a window that is not docked keeps its own frame.
    void DockedCard();

    // The keyboard focus ring (Tab / arrow keys) around the last item. Drawn widgets call it after drawing, as their
    // InvisibleButton (made with ImGuiButtonFlags_EnableNav, so Tab reaches it) drew ImGui's ring under the drawing.
    void FocusRing();

    // The widgets' sizes are in screen pixels (already scaled with the display, like the rest of the editor's
    // layout); text sizes are unscaled font sizes, as PushFont takes them.

    // A square icon button on a raised background (the design's 44 px targets). active keeps it highlighted;
    // dimmed draws it at half strength (the transport's Pause / Stop while nothing runs). Returns true when clicked.
    bool IconButton(const char* id, Icon icon, float size, const char* tooltip, bool active = false,
                    bool enabled = true, bool dimmed = false);
    // A rounded button with its own colours and an optional icon before the label. width 0: fits the label.
    bool PillButton(const char* label, const ImVec4& background, const ImVec4& text, float height, float width = 0.0f,
                    bool hasIcon = false, Icon icon = Icon::Plus, bool enabled = true);
    // Its width, to centre or right-align a row of them
    float PillWidth(const char* label, float height, bool hasIcon = false);
    // A segmented toggle (Objects | Assets, Simple | Advanced). Returns true when current changed.
    bool Segmented(const char* id, const char* const* labels, int count, int& current, float width, float height);
    float SegmentedWidth(const char* const* labels, int count, float height);
}
