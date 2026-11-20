/* !
@file    	    AnimatorComponent.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		02/11/2026
@brief
    AnimatorComponent: plays one of the animations of an object's model
    ("Walk", "Run"), bending its skeleton.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/assets/ResourceManager.hpp"

#include <string>
#include <vector>

namespace Engine::Scene
{
    // Plays an animation of the object's model (its Mesh Renderer's model). In the editor the model shows the clip's
    // first moment; in Play it moves (AnimationSystem), and the Animation blocks change the clip while the game runs.
    struct AnimatorComponent
    {
        std::string clip;    // the model's animation, by name; empty plays its first one
        bool playing = true; // whether it moves (a clip that does not loop stops at its end)
        bool loop = true;    // starts again at its end
        float speed = 1.0f;  // 1 is as authored, 2 twice as fast
        float time = 0.0f;   // seconds into the clip (while the game plays; not saved)
        // Other model files whose animations it can play too: a character pack's separate animation files, made for
        // the same skeleton (their clips move the model's bones of the same names)
        std::vector<std::string> sources;
        std::vector<Engine::Assets::ModelHandle> sourceModels; // those files, loaded while the game runs (not saved)

        // Two animators are the same when they would be saved the same (the time is the game's, not the scene's)
        bool operator==(const AnimatorComponent& other) const
        {
            return clip == other.clip && playing == other.playing && loop == other.loop && speed == other.speed &&
                   sources == other.sources;
        }
    };
}
