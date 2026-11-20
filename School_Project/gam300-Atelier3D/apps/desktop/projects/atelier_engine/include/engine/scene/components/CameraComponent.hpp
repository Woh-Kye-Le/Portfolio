/* !
@file    	    CameraComponent.hpp
@author  	    sung.h@digipen.edu [69%]
         	    c.kojat@digipen.edu [23%]
         	    sanni.go@digipen.edu [8%]
@date    		01/10/2026
@brief
    CameraComponent: field of view, near/far planes and whether it is the Main
    camera used in Play mode.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

namespace Engine::Scene
{
    // A camera the game can look through. Where it is and where it points come from the entity's Transform.
    struct CameraComponent
    {

        float fieldOfView = 60.0f; // vertical, in degrees
        float nearPlane = 0.05f;
        float farPlane = 1000.0f;
        bool primary = false; // the "Main Camera" tick box - Play mode renders from this one

        bool operator==(const CameraComponent&) const = default;
    };

}
