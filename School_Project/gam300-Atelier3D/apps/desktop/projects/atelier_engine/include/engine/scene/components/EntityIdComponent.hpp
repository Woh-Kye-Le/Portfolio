/* !
@file    	    EntityIdComponent.hpp
@author  	    sung.h@digipen.edu [92%]
         	    c.kojat@digipen.edu [8%]
@date    		01/10/2026
@brief
    EntityIdComponent: stores the engine entity id on each EnTT entity (managed
    by Scene).

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <cstdint>

namespace Engine::Scene
{
    // Lets ECS query ret the corres engine ent
    // Scene will create and manage this component
    // (every entity has one; you can't remove it and ForEach won't let you ask for it directly)
    struct EntityIdComponent
    {

        std::uint32_t value = 0;
    };
}
