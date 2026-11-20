/* !
@file    	    PlayerControllerComponent.hpp
@author  	    sung.h@digipen.edu [67%]
         	    c.kojat@digipen.edu [33%]
@date    		01/10/2026
@brief
    PlayerControllerComponent: movement speed, mouse look and the follow-camera
    settings of the player.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

namespace Engine::Scene
{
    // Moved by the move actions (relative to the way the entity faces) and turned by the mouse while Look is held.
    // With followCamera, the game camera stays behind it: cameraOffsetZ away, looking at a point cameraOffsetY above
    // it, tilted by cameraPitch (mouse look and the scroll wheel change these two while playing).
    struct PlayerControllerComponent
    {

        float speed = 3.0f;
        float fastMultiplier = 2.0f;

        float cameraOffsetY = 1.0f;
        float cameraOffsetZ = 4.0f;
        bool followCamera = true;

        float lookSensitivity = 0.2f; // degrees per pixel of mouse movement
        float cameraPitch = -20.0f;   // degrees; below 0 looks down on the player

        bool operator==(const PlayerControllerComponent&) const = default;
    };

}
