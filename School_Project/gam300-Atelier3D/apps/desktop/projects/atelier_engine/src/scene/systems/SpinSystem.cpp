/* !
@file    	    SpinSystem.cpp
@author  	    sung.h@digipen.edu [87%]
         	    c.kojat@digipen.edu [13%]
@date    		01/10/2026
@brief
    Implements SpinSystem::Update.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/scene/systems/SpinSystem.hpp"
#include "engine/scene/Scene.hpp"

#include <cmath>

namespace Engine::Systems
{
    // rotation += speed * time for every spinning entity
    void SpinSystem::Update(Scene::Scene& scene, double deltaSeconds)
    {
        const float seconds = static_cast<float>(deltaSeconds);

        // wrap at 360 so the angle doesn't keep growing forever (and lose float precision). A child turns relative to
        // its parent: HierarchySystem works its Transform out from the local one.
        scene.ForEach<Scene::TransformComponent, const Scene::SpinComponent>(
            [&scene, seconds](Scene::Entity entity, Scene::TransformComponent& transform,
                              const Scene::SpinComponent& spin)
            {
                auto* link = scene.TryGetComponent<Scene::HierarchyComponent>(entity);
                float& rotation = link ? link->local.rotation : transform.rotation;
                rotation = std::fmod(rotation + seconds * spin.degreesPerSecond, 360.0f);
            });
    }
}
