/* !
@file    	    EntitySpawner.hpp
@author  	    sung.h@digipen.edu [73%]
         	    c.kojat@digipen.edu [27%]
@date    		02/10/2026
@brief
    Declares SpawnCopies and DestroyAll, used by the ECS demo to create and
    remove many entities at runtime.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/scene/Entity.hpp"
#include "engine/scene/components/MeshRendererComponent.hpp"
#include "engine/scene/components/TransformComponent.hpp"

#include <cstddef>
#include <vector>

namespace Engine::Scene
{
    class Scene;

    // Entities created at runtime, straight into the ECS (they are not in the scene file or the editor's Hierarchy).
    // Used by the ECS demo ("spawn 1,000 copies" of a scene object) and its unit tests.
    struct SpawnOptions
    {
        std::size_t count = 1000;
        std::size_t side = 10;              // copies per row and per column: a layer holds side * side
        std::size_t firstIndex = 0;         // block position of the first copy, so a later batch continues the block
        float spacing = 1.5f;               // distance between neighbours
        TransformComponent origin;          // the block is centred on it in X / Z and rises from one spacing above it;
                                            // copies keep its rotation and scale
        MeshRendererComponent meshRenderer; // what every copy draws
        float spinDegreesPerSecond = 0.0f;  // 0: no SpinComponent is added
    };

    // Creates options.count entities, each with a Transform, the MeshRenderer (so RenderSystem draws it) and, when
    // spinDegreesPerSecond is not 0, a SpinComponent. Block position n is column n % side, row (n / side) % side,
    // layer n / (side * side). Returns the new entities in creation order; on failure none are left behind. Must not
    // be called from inside Scene::ForEach.
    std::vector<Entity> SpawnCopies(Scene& scene, const SpawnOptions& options);

    // Destroys the given entities (ones already destroyed are skipped). Returns how many were destroyed.
    std::size_t DestroyAll(Scene& scene, const std::vector<Entity>& entities);
}
