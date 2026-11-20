/* !
@file    	    TransformComponent.hpp
@author  	    sung.h@digipen.edu [66%]
         	    c.kojat@digipen.edu [34%]
@date    		30/09/2026
@brief
    TransformComponent: position, rotation and scale of an entity, and its
    conversion to a model matrix.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/graphics/Transform.hpp"

namespace Engine::Scene
{
    // Where an entity is, how it's turned and how big it is. Systems that move things (spin, player)
    // change this, and the renderer, lights, camera and audio all read it.
    struct TransformComponent
    {

        //pos
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;

        //rot in deg arnd Y
        float rotation = 0.f;
        float rotationX = 0.f;
        float rotationZ = 0.f;

        //uniform scl multi by per axis scl
        float scale = 1.f;
        float scaleX = 1.f;
        float scaleY = 1.f;
        float scaleZ = 1.f;

        bool operator==(const TransformComponent&) const = default;
    };

    // The component as a Graphics::Transform (rotations in degrees, scale = size times the per-axis scale); its
    // ComposeModelMatrix() is the object's model matrix, in the order ImGuizmo uses
    inline Graphics::Transform ToGraphicsTransform(const TransformComponent& t)
    {
        return Graphics::Transform{.position = {t.x, t.y, t.z},
                                   .rotationEulerDegrees = {t.rotationX, t.rotation, t.rotationZ},
                                   .scale = t.scale * glm::vec3(t.scaleX, t.scaleY, t.scaleZ)};
    }
}
