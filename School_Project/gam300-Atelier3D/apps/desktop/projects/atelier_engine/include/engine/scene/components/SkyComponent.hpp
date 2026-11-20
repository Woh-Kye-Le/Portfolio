/* !
@file    	    SkyComponent.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		02/11/2026
@brief
    SkyComponent: the sky a scene is drawn in - its colours, its haze, or a
    picture of a whole sky wrapped all round.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <string>

namespace Engine::Scene
{
    // The scene's sky (one is enough: with several, the first in the scene is used). Without one a scene has the
    // default blue sky. The Sky blocks (in Looks) change it while the game runs.
    struct SkyComponent
    {
        static constexpr float kDefaultAmbient = 0.55f; // as the renderer's default lighting

        // Straight up, at the horizon (also the colour far things fade into) and below the horizon; 0 to 1 each
        float topR = 0.20f;
        float topG = 0.38f;
        float topB = 0.68f;
        float horizonR = 0.64f;
        float horizonG = 0.74f;
        float horizonB = 0.86f;
        float groundR = 0.22f;
        float groundG = 0.23f;
        float groundB = 0.25f;
        float haze = 0.012f; // how quickly far things fade into the horizon colour; 0: no haze
        // A picture of the whole sky (a panorama twice as wide as it is high, the horizon across its middle), by its
        // name ("sunset") or path; drawn instead of the colours. Empty: the colours.
        std::string picture;
        float turn = 0.0f;               // degrees the picture is turned round, to put its sun where the scene's is
        float ambient = kDefaultAmbient; // light from all around, reaching what no light shines on; low for a night

        bool operator==(const SkyComponent&) const = default;
    };
}
