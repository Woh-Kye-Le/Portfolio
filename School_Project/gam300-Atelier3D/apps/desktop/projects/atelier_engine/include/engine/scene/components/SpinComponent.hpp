/* !
@file    	    SpinComponent.hpp
@author  	    sung.h@digipen.edu [70%]
         	    c.kojat@digipen.edu [30%]
@date    		01/10/2026
@brief
    SpinComponent: makes an entity turn around its Y axis at a set speed.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

namespace Engine::Scene
{
    // Makes an entity keep turning around its Y axis (SpinSystem does the turning).
    // Every Renderable gets one, with the object's rotation speed - 0 means it just doesn't move.
    struct SpinComponent
    {

        float degreesPerSecond = 60.0f; // negative spins the other way
    };

}
