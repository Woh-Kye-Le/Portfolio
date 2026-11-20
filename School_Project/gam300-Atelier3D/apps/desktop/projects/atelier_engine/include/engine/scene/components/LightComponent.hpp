/* !
@file    	    LightComponent.hpp
@author  	    sung.h@digipen.edu [80%]
         	    c.kojat@digipen.edu [20%]
@date    		01/10/2026
@brief
    LightComponent: directional, point or spot light colour, intensity, range
    and cone angles.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/graphics/Lighting/Lights.hpp"

namespace Engine::Scene
{
    // A light in the scene. The position (and direction, for directional/spot) comes from the Transform.
    // RenderSystem::CollectLights gathers these every frame and hands them to the renderer.
    struct LightComponent
    {

        Graphics::LightType type = Graphics::LightType::Point;

        float colorR = 1.0f;
        float colorG = 1.0f;
        float colorB = 1.0f;
        float intensity = 1.0f;

        float range = 10.0f;            // point/spot only: how far the light reaches
        float spotInnerConeDeg = 25.0f; // spot only: full brightness inside this angle, fades out to the outer one
        float spotOuterConeDeg = 35.0f;
        bool enabled = true;
        float flicker = 0.0f; // how much it flickers in Play mode, like a flame: 0 steady, 1 dips right down to dark

        bool operator==(const LightComponent&) const = default;
    };

}
