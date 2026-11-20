/* !
@file    	    HierarchyComponent.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		20/10/2026
@brief
    HierarchyComponent: the parent of a child entity and its transform relative
    to that parent.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/scene/Entity.hpp"
#include "engine/scene/components/TransformComponent.hpp"

namespace Engine::Scene
{
    // Only child entities have one. Systems::HierarchySystem sets the entity's TransformComponent (where it is in the
    // world) to its parent's composed with local every frame, so a child follows its parent around. Anything that
    // moves a child itself (SpinSystem) changes local, not the TransformComponent.
    struct HierarchyComponent
    {

        Entity::Id parent = 0;
        TransformComponent local;

        bool operator==(const HierarchyComponent&) const = default;
    };

}
