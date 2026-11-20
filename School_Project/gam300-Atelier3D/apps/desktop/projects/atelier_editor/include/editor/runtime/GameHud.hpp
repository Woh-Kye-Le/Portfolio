/* !
@file    	    GameHud.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		01/11/2026
@brief
    Declares the games' on-screen display: health, the variables the blocks
    show, speech bubbles, a crosshair and "click to play", drawn over the game
    in Play and in a built game.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <imgui/imgui.h>

#include <cfloat>
#include <cstdint>
#include <utility>
#include <vector>

namespace Engine::Game
{
    class GameRuntime;
}

namespace Engine::Scene
{
    struct ScreenElementComponent;
}

namespace Editor::Runtime
{
    // Where the game is on the screen, and how its HUD is shown
    struct HudView
    {
        ImVec2 min;                       // the game view's top-left, in screen pixels
        ImVec2 size;                      // its size
        float scale = 1.0f;               // the display's scale (DPI): the HUD's sizes are at 100%
        bool pointerLocked = false;       // the game has the mouse: no "click to play"
        ImVec2 mouse{-FLT_MAX, -FLT_MAX}; // where the pointer is (for showing what a click would press)
    };

    // An object's thing on the screen that can be clicked ("when clicked on screen"), where it was drawn
    struct ScreenTarget
    {
        std::uint32_t objectId = 0;
        ImVec2 min;
        ImVec2 max;
    };

    // Where a Screen Element's middle is on the view
    ImVec2 ScreenElementCentre(const HudView& view, const Engine::Scene::ScreenElementComponent& element);
    // Draws one Screen Element (a bar filled to barValue; pictures found through the running game, none without one),
    // faded by alpha; returns where it was drawn. The game's HUD and the editor (which shows them while editing) use it.
    std::pair<ImVec2, ImVec2> DrawScreenElement(ImDrawList* drawList, const HudView& view,
                                                const Engine::Scene::ScreenElementComponent& element, double barValue,
                                                Engine::Game::GameRuntime* pictures, float alpha = 1.0f);

    // The game's HUD, in the editor's fonts (Theme::LoadFonts must have run), over the running game:
    //   - the Screen Elements (texts, pictures, bars and buttons), lowest layer first, and the variables shown ("show
    //     variable")
    //   - top left, the player's Health as a bar (unless "hide the player's health bar")
    //   - a health bar over the objects that show their health above them ("show health above me")
    //   - a speech bubble over each object that said something in the last few seconds ("say"); what is said off
    //     screen, or by the player in first person, is a subtitle along the bottom instead
    //   - a crosshair when the player's camera is right behind them (first person), or as the blocks say
    //   - "Click to play" while the game does not have the mouse (and has nothing on the screen to click)
    // Returns the things that can be clicked, where they were drawn: the caller hands a click on one to
    // GameRuntime::ScreenClicked.
    std::vector<ScreenTarget> DrawGameHud(ImDrawList* drawList, const HudView& view,
                                          Engine::Game::GameRuntime& runtime);
}
