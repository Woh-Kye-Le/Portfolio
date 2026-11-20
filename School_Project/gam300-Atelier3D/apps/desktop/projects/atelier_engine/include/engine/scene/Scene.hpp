/* !
@file    	    Scene.hpp
@author  	    sung.h@digipen.edu [85%]
         	    c.kojat@digipen.edu [15%]
@date    		22/09/2026
@brief
    Declares Scene, the ECS world built on EnTT: creating and destroying
    entities, adding, removing and getting components, and iterating entities
    with ForEach.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/scene/Components.hpp"
#include "engine/scene/Entity.hpp"

#include <entt/entity/registry.hpp>

#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace Engine::Scene
{
    // Our ECS world. It wraps an EnTT registry so the rest of the engine never touches EnTT directly
    // (if we ever swap the library, only Scene.cpp changes). Only the types listed in Components.hpp
    // can be used as components.
    class Scene
    {
    public:
        Scene();
        ~Scene();

        // not copyable - entities point back at their scene
        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;

        // New entities get a unique id (never reused) and start with a default Transform
        Entity CreateEntity();
        void DestroyEntity(Entity entity);
        bool IsAlive(Entity entity) const;
        Entity FindEntity(Entity::Id id) const;
        std::size_t EntityCount() const;

        const std::vector<Entity>& Entities() const;

        std::optional<TransformComponent> GetTransform(Entity entity) const;
        bool SetTransform(Entity entity, const TransformComponent& transform);

        template <class T> bool HasComponent(Entity entity) const;

        // Tryget-> ptr to stored cmpt, change ecs dir, refetch aft struct change, get returns copy, and set component to save, cpy inde of ecs
        template <class T> T* TryGetComponent(Entity entity);

        template <class T> const T* TryGetComponent(Entity entity) const;

        template <class T> std::optional<T> GetComponent(Entity entity) const;

        // Set = overwrite one the entity already has (false if it doesn't have it)
        template <class T> bool SetComponent(Entity entity, const T& component);

        // Add = give it a new one (false if it already has one, use SetComponent for that)
        template <class T> bool AddComponent(Entity entity, const T& component = T{});

        template <class T> bool RemoveComponent(Entity entity);

        // Calls function(entity, components...) for every entity that has all of Ts. Mark a type const if you
        // only read it. Don't add/remove entities or components inside the callback (it throws).
        template <class... Ts, class Fn> void ForEach(Fn&& function);

        // How many entities have all of Ts, e.g. Count<TransformComponent, MeshRendererComponent>() = how many get drawn
        template <class... Ts> std::size_t Count();

    private:
        entt::registry& Registry();
        const entt::registry& Registry() const;
        entt::entity Handle(Entity entity) const;

        void RequireNotIterating() const;

        int iterating_ = 0;

        //ecs imple details
        struct EcsState;
        std::unique_ptr<EcsState> ecs_;

        std::vector<Entity> entities_;
        Entity::Id nextId_ = 1;
    };

    //def in header to supply own callback types
    template <class... Ts, class Fn> void Scene::ForEach(Fn&& function)
    {
        static_assert(sizeof...(Ts) > 0, "Foreach req at least one component type");

        static_assert((IsEngineComponent<Ts> && ...),
                      "Every component must be listed in EngineComponents in components.hpp");

        static_assert((!std::is_same_v<std::remove_cv_t<Ts>, EntityIdComponent> && ...),
                      "Use the Entity arg instead of EntityIdComponent");

        auto view = Registry().view<const EntityIdComponent, Ts...>();

        ++iterating_;

        // Lowers the counter again even if the callback throws, so the scene doesn't stay "locked"
        struct IterationGuard
        {
            int& depth;

            ~IterationGuard() { --depth; }
        };

        IterationGuard guard{iterating_};

        // Walks the view directly instead of calling view.each(callback): MSVC 14.41 mis-expands the nested parameter
        // pack inside EnTT 4.1's each(callback) and cannot compile it. This visits the same entities with the same
        // component references.
        for (const entt::entity handle : view)
        {
            const EntityIdComponent& id = view.template get<const EntityIdComponent>(handle);
            function(Entity{id.value, this}, view.template get<Ts>(handle)...);
        }
    }

    template <class... Ts> std::size_t Scene::Count()
    {
        std::size_t count = 0;

        ForEach<std::remove_cv_t<Ts>...>([&count](Entity, auto&...) { ++count; });

        return count;
    }
}
