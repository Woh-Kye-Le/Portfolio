/* !
@file    	    EntitySpawner.cpp
@author  	    sung.h@digipen.edu [73%]
         	    c.kojat@digipen.edu [27%]
@date    		02/10/2026
@brief
    Implements SpawnCopies (a block of copies with Transform, MeshRenderer and
    Spin, all or nothing) and DestroyAll.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/scene/EntitySpawner.hpp"

#include "engine/scene/Scene.hpp"

#include <algorithm>
#include <stdexcept>

namespace Engine::Scene
{
    // Used by the ECS demo, see the header for how the block is laid out
    std::vector<Entity> SpawnCopies(Scene& scene, const SpawnOptions& options)
    {
        std::vector<Entity> spawned;
        if (options.count == 0)
        {
            return spawned;
        }

        spawned.reserve(options.count);

        // half the width of a row, so the block ends up centred on the origin
        const std::size_t side = std::max<std::size_t>(options.side, 1);
        const float half = static_cast<float>(side - 1) * options.spacing * 0.5f;

        try
        {
            for (std::size_t i = 0; i < options.count; ++i)
            {
                const std::size_t n = options.firstIndex + i;
                const Entity entity = scene.CreateEntity();
                spawned.push_back(entity);

                // n -> column, row, layer. Layers start one spacing above the origin so the first one
                // doesn't sit inside the object we're copying
                TransformComponent transform = options.origin;
                transform.x = options.origin.x + static_cast<float>(n % side) * options.spacing - half;
                transform.z = options.origin.z + static_cast<float>((n / side) % side) * options.spacing - half;
                const std::size_t layer = n / (side * side); // whole layers below this one (integer division)
                transform.y = options.origin.y + static_cast<float>(layer + 1) * options.spacing;

                bool ok = scene.SetTransform(entity, transform) && scene.AddComponent(entity, options.meshRenderer);

                if (ok && options.spinDegreesPerSecond != 0.0f)
                {
                    ok = scene.AddComponent(entity, SpinComponent{.degreesPerSecond = options.spinDegreesPerSecond});
                }

                if (!ok)
                {
                    throw std::runtime_error("Could not initialize a spawned ECS entity");
                }
            }
        }
        catch (...)
        {
            // all or nothing: don't leave half a block behind if something failed
            DestroyAll(scene, spawned);
            throw;
        }

        return spawned;
    }

    // Skips ones that are already gone, so it's safe to call twice
    std::size_t DestroyAll(Scene& scene, const std::vector<Entity>& entities)
    {
        std::size_t destroyed = 0;
        for (const Entity entity : entities)
        {
            if (scene.IsAlive(entity))
            {
                scene.DestroyEntity(entity);
                ++destroyed;
            }
        }
        return destroyed;
    }
}
