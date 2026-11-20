/* !
@file    	    HierarchySystem.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		20/10/2026
@brief
    Implements HierarchySystem::Update.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/scene/systems/HierarchySystem.hpp"

#include "engine/scene/Hierarchy.hpp"
#include "engine/scene/Scene.hpp"

#include <unordered_map>
#include <vector>

namespace Engine::Systems
{
    void HierarchySystem::Update(Scene::Scene& scene)
    {
        // Gathered first: placing an entity sets components, which ForEach does not allow while it walks the view
        std::unordered_map<Scene::Entity::Id, Scene::HierarchyComponent> links;
        std::vector<Scene::Entity> children;
        scene.ForEach<const Scene::HierarchyComponent>(
            [&](Scene::Entity entity, const Scene::HierarchyComponent& link)
            {
                links.emplace(entity.GetId(), link);
                children.push_back(entity);
            });

        // Where each entity is in the world, worked out once; a parent is placed before its children
        std::unordered_map<Scene::Entity::Id, Scene::TransformComponent> placed;
        std::unordered_map<Scene::Entity::Id, bool> visiting; // a loop of parents ends at the entity that closes it
        const auto worldOf = [&](auto& self, Scene::Entity::Id id) -> Scene::TransformComponent
        {
            if (const auto done = placed.find(id); done != placed.end())
            {
                return done->second;
            }
            const auto link = links.find(id);
            if (link == links.end() || visiting[id])
            {
                // Not a child (or a loop): it is where its own Transform says
                return scene.GetTransform(scene.FindEntity(id)).value_or(Scene::TransformComponent{});
            }
            visiting[id] = true;
            const Scene::TransformComponent world = Scene::Compose(self(self, link->second.parent), link->second.local);
            visiting[id] = false;
            placed[id] = world;
            return world;
        };

        for (const Scene::Entity child : children)
        {
            scene.SetTransform(child, worldOf(worldOf, child.GetId()));
        }
    }
}
