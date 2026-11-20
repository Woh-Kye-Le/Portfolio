/* !
@file    	    PhysicsBodyComponent.hpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		26/10/2026
@brief
    PhysicsBodyComponent: the physics body made for an entity's RigidBody, what
    it was made from, and the entities it touches. Runtime only, never saved.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/physics/Body.hpp"
#include "engine/scene/Entity.hpp"
#include "engine/scene/components/RigidBodyComponent.hpp"
#include "engine/scene/components/TransformComponent.hpp"

#include <vector>

namespace Engine::Scene
{
    // Added by Systems::PhysicsSystem to an entity it made a body for. It keeps what the body was made from, so the
    // system can tell when the RigidBody or the collider changed and the body has to be made again, and where the
    // physics last put the entity, so it can tell when something else moved it.
    struct PhysicsBodyComponent
    {
        Physics::BodyId body;
        RigidBodyComponent settings;      // the RigidBody the body was made from
        Physics::Collider collider;       // the collider it was made with (scaled, so a resize makes it again)
        TransformComponent lastTransform; // the entity's Transform when the physics last placed it (world)
        std::vector<Entity> touching;     // the entities whose bodies touch this one, oldest contact first

        bool operator==(const PhysicsBodyComponent&) const = default;
    };
}
