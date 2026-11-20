/* !
@file    	    LessonGoals.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		05/11/2026
@brief
    Declares the checks behind a lesson's steps: small sentences saying what
    the learner should have built ("number Wall turn_by 0 90") or made happen
    in the running game ("dropped Ball 1"). A step is ticked off by itself when
    its check holds, so a check says the thing is really done: not that a
    button was pressed.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/logic/Blocks.hpp"
#include "engine/scene/SceneDescription.hpp"

#include <array>
#include <functional>
#include <optional>
#include <string_view>

namespace Engine::Editor
{
    // What the running game says, for the checks about it. Unset: the game is not running, and none of them holds.
    struct GoalWorld
    {
        double seconds = 0.0; // how long it has been running
        // Where an object is now (x, y, z), by name
        std::function<std::optional<std::array<float, 3>>(std::string_view)> place;
        // A variable of an object, by name
        std::function<std::optional<double>(std::string_view, std::string_view)> variable;
        // Whether a block of this kind of an object's is running, or just ran
        std::function<bool(std::string_view, Logic::Op)> ran;
        // Whether an object has a script that started and ran to its end
        std::function<bool(std::string_view)> finished;
    };

    // A check is words split by spaces. Names of objects and variables ignore case; blocks go by the names of the
    // block table. A block only counts in a script that runs: one under a "when" block. "if" also means "if / else".
    //
    // What is in the scene:
    //   object Flag                      there is an object called Flag
    //   shape Wall cube                  it is a cube (or sphere, pyramid, plane)
    //   has Ball body                    it has this part: body, light, sky, camera, player, sound, screen, animator
    //   body Ground static               its Rigid Body is of this type: static, dynamic or kinematic
    //   bouncy Ball                      its Rigid Body bounces
    //   moved Wall                       it is not where it was made
    //   sized Ground                     it is not the size it was made
    //   colored Ground                   it (or its light) is not the white it was made
    //   above Ball Ground 3              it is at least this much higher than the other
    //   apart Tree Wall 2                they are at least this far apart along the ground
    //
    // What its blocks are:
    //   variable Player score            it has this variable
    //   script Player a b c              one script of it has blocks a, b and c, in that order
    //   uses Player a                    a script of it has block a
    //   gone Player a                    it has no block a anywhere (the lesson moved on from it)
    //   inside Player a b                block b is inside block a
    //   slot Player a b                  block b is in a condition or number of block a
    //   number Wall turn_by 0 90         number 0 of a block turn_by of it is 90
    //   text Wall point_towards Flag     the text of a block point_towards of it is Flag
    //
    // What happens in the running game:
    //   ran Wall say                     that block has run
    //   finished Wall                    a script of it ran to its end, and the game has run long enough to see it
    //   near Player Flag 2               they are within 2 of each other along the ground
    //   travelled Player 2               it is at least this far along the ground from where it started
    //   dropped Ball 1                   it is at least this far below where it started
    //   landed Ball Ground               it has been falling a while, and is still not under the other
    //   reaches Player score 3           the variable is 3 or more
    //
    // Returns false for a check it cannot read.
    bool CheckGoal(const Scene::SceneDescription& scene, std::string_view check, const GoalWorld& world);

    // Whether a check can be read: a known word, the right number of words, and blocks that exist
    bool IsValidGoal(std::string_view check);
}
