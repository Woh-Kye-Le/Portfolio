/* !
@file    	    Tour.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		05/11/2026
@brief
    Declares the first-time tour: its steps, and where a step's words go
    beside what it points at. Header-only so the tests can check it;
    SceneEditorTour.cpp draws it.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <algorithm>
#include <array>

namespace Editor::Tour
{
    // One stop of the tour. target names what is lit up, as SceneEditor::TourMark marks it; nullptr: nothing, the words
    // go in the middle of the screen. workspace: it is on the workspace, not Home.
    struct Step
    {
        const char* target;
        const char* title;
        const char* text;
        bool workspace;
    };

    inline constexpr std::array kSteps{
        Step{nullptr, "Welcome to Atelier3D",
             "You make games here by snapping blocks together, with no typing needed. This quick tour shows you where "
             "everything is.",
             false},
        Step{"learn", "Learn",
             "Guided lessons that teach you to think in blocks. They build one game, step by step, and tick off your "
             "goals as you reach them.",
             false},
        Step{"create", "Create", "When you are ready, start a game of your own here. There are no rules.", false},
        Step{"palette", "The blocks",
             "Every block you can use, in groups by colour. Hover over any block to watch what it does.", true},
        Step{"canvas", "Your logic",
             "Drag blocks here and snap them together. The blue bars show where a block can go. Each object has its "
             "own blocks.",
             true},
        Step{"viewport", "Your game",
             "This is your world. Click an object to choose it, then move it around. The game also plays here.", true},
        Step{"objects", "Your objects", "Everything in your scene. Pick one to see and change its blocks.", true},
        Step{"add", "Add object", "Start here: put a new object into your scene.", true},
        Step{"run", "Run",
             "Plays your game. Press Stop to come back to editing, and everything goes back to how it was.", true},
        Step{"home", "Home", "Back to your lessons and games. That is everything. Have fun!", true},
    };

    // The Advanced interface's tour: each target is a window of that name, brought to the front for its step (or what
    // SceneEditor::TourMark marks). It stays in the workspace, on the open game.
    inline constexpr std::array kAdvancedSteps{
        Step{nullptr, "Welcome to Advanced",
             "This is the full editor: the same game, with every tool in a window of its own. Drag a window by its tab "
             "to put it anywhere. Here is what the main ones do.",
             true},
        Step{"Viewport", "Viewport",
             "Your world. Click an object to select it. W, E and R move, turn and scale it. Hold the right mouse "
             "button and use WASD to fly around.",
             true},
        Step{"Hierarchy", "Hierarchy",
             "Every object in the scene, and your prefabs. Drag one object onto another to put it inside.", true},
        Step{"Inspector", "Inspector",
             "What the selected object is made of. Add Component adds parts, such as a Rigid Body so it falls and "
             "bumps into things.",
             true},
        Step{"Blocks", "Blocks", "The selected object's logic, the same blocks as in Simple.", true},
        Step{"Assets", "Assets", "Your game's models, textures and sounds. Drag one onto an object to use it.", true},
        Step{"Console", "Console", "Messages from the game and the editor. Look here first when something goes wrong.",
             true},
        Step{"run", "Run", "Plays your game right here. Stop puts everything back how it was.", true},
        Step{"window-menu", "Window menu",
             "Open any window you closed, reset the layout, go back to the Simple interface, or see this guide again.",
             true},
    };

    struct Box
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    // Where words of the given size go beside target: to its right if they fit, else its left, then under it, then
    // above it, and over it when it fills the screen. They always stay on the screen.
    inline Box PlaceBeside(const Box& target, float width, float height, float screenWidth, float screenHeight,
                           float gap)
    {
        const float maxX = std::max(screenWidth - width - gap, gap);
        const float maxY = std::max(screenHeight - height - gap, gap);
        const float alongY = std::clamp(target.y, gap, maxY);
        const float alongX = std::clamp(target.x + (target.width - width) * 0.5f, gap, maxX);
        float x = alongX;
        float y = alongY;
        if (target.x + target.width + gap + width + gap <= screenWidth)
        {
            x = target.x + target.width + gap;
        }
        else if (target.x - gap - width - gap >= 0.0f)
        {
            x = target.x - gap - width;
        }
        else if (target.y + target.height + gap + height + gap <= screenHeight)
        {
            y = target.y + target.height + gap;
        }
        else if (target.y - gap - height - gap >= 0.0f)
        {
            y = target.y - gap - height;
        }
        else
        {
            y = maxY;
        }
        return {x, y, width, height};
    }
}
