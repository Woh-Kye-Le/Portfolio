/* !
@file    	    SpinSystem.hpp
@author  	    sung.h@digipen.edu [87%]
         	    c.kojat@digipen.edu [13%]
@date    		01/10/2026
@brief
    Declares SpinSystem, which turns entities that have a SpinComponent.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

namespace Engine::Scene
{
    class Scene;
}

namespace Engine::Systems
{
    // Turns everything that has a Transform and a SpinComponent. Only runs in Play mode.
    class SpinSystem
    {
    public:
        static void Update(Scene::Scene& scene, double deltaSeconds);
    };
}
