/* !
@file    	    Scene.cpp
@author  	    sung.h@digipen.edu [85%]
         	    c.kojat@digipen.edu [15%]
@date    		22/09/2026
@brief
    Implements Scene: stable entity ids mapped to EnTT handles, component access
    for every engine component type, and the guard against structural changes
    during iteration.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/scene/Scene.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

namespace Engine::Scene
{

    // Kept out of the header so only this file needs EnTT's registry.
    // EnTT reuses its entity values after a destroy, so we hand out our own ids (never reused) and map them
    // to EnTT's handles here. That way an old Entity can't accidentally point at a new one.
    struct Scene::EcsState
    {
        entt::registry registry;
        std::unordered_map<Entity::Id, entt::entity> handles;
    };

    Scene::Scene() : ecs_(std::make_unique<EcsState>()) {}

    Scene::~Scene() = default;

    entt::registry& Scene::Registry()
    {
        return ecs_->registry;
    }

    const entt::registry& Scene::Registry() const
    {
        return ecs_->registry;
    }

    // Our Entity -> EnTT handle. entt::null if it isn't ours or doesn't exist anymore
    entt::entity Scene::Handle(Entity entity) const
    {
        if (entity.owner_ != this)
        {
            return entt::null;
        }

        const auto found = ecs_->handles.find(entity.GetId());

        return found != ecs_->handles.end() ? found->second : entt::null;
    }

    // Adding/removing entities or components while ForEach is walking the view can skip or repeat entities,
    // so we just don't allow it (throws instead of doing something weird)
    void Scene::RequireNotIterating() const
    {
        if (iterating_ != 0)
        {
            throw std::logic_error("Cannot change entity/component membership inside foreach");
        }
    }

    Entity Scene::CreateEntity()
    {
        RequireNotIterating();

        if (nextId_ == std::numeric_limits<Entity::Id>::max())
        {
            throw std::overflow_error("Scene entity IDs exhausted");
        }

        const Entity::Id id = nextId_;
        const auto handle = Registry().create();
        const Entity entity{id, this};

        try
        {
            // the id lives on the entity too, so ForEach can hand back an Entity without a reverse lookup
            Registry().emplace<EntityIdComponent>(handle, EntityIdComponent{.value = id});

            // Every new entity starts with a Transform (callers remove it if the object shouldn't have one)
            Registry().emplace<TransformComponent>(handle);

            ecs_->handles.emplace(id, handle);
            entities_.push_back(entity);
        }
        catch (...)
        {
            // undo the half-made entity so the scene stays consistent
            ecs_->handles.erase(id);
            Registry().destroy(handle);
            throw;
        }

        ++nextId_;
        return entity;
    }

    bool Scene::IsAlive(Entity entity) const
    {
        const auto handle = Handle(entity);

        return handle != entt::null && Registry().valid(handle);
    }

    Entity Scene::FindEntity(Entity::Id id) const
    {
        const Entity entity{id, this};
        return IsAlive(entity) ? entity : Entity{};
    }

    std::size_t Scene::EntityCount() const
    {
        return entities_.size();
    }

    // Removes the entity and all its components
    void Scene::DestroyEntity(Entity entity)
    {
        //rej empty, del or other scene handles
        if (!IsAlive(entity))
        {
            return;
        }

        RequireNotIterating();

        Registry().destroy(Handle(entity));
        ecs_->handles.erase(entity.GetId());

        entities_.erase(std::remove_if(entities_.begin(), entities_.end(),
                                       [entity](const Entity& current) { return current.GetId() == entity.GetId(); }),
                        entities_.end());
    }

    const std::vector<Entity>& Scene::Entities() const
    {
        return entities_;
    }

    std::optional<TransformComponent> Scene::GetTransform(Entity entity) const
    {
        return GetComponent<TransformComponent>(entity);
    }

    bool Scene::SetTransform(Entity entity, const TransformComponent& transform)
    {
        return SetComponent<TransformComponent>(entity, transform);
    }

    template <class T> bool Scene::HasComponent(Entity entity) const
    {
        static_assert(IsEngineComponent<T>, "Component type is not listed in EngineComponents");

        return IsAlive(entity) && Registry().all_of<T>(Handle(entity));
    }

    template <class T> T* Scene::TryGetComponent(Entity entity)
    {
        static_assert(IsEngineComponent<T>, "Component type is not listed in EngineComponents");

        if (!IsAlive(entity))
        {
            return nullptr;
        }

        return Registry().try_get<T>(Handle(entity));
    }

    template <class T> const T* Scene::TryGetComponent(Entity entity) const
    {
        static_assert(IsEngineComponent<T>, "Component type is not listed in EngineComponents");

        if (!IsAlive(entity))
        {
            return nullptr;
        }

        return Registry().try_get<T>(Handle(entity));
    }

    template <class T> std::optional<T> Scene::GetComponent(Entity entity) const
    {
        const T* component = TryGetComponent<T>(entity);

        if (!component)
        {
            return std::nullopt;
        }

        return *component;
    }

    template <class T> bool Scene::SetComponent(Entity entity, const T& component)
    {
        static_assert(IsEngineComponent<T>, "Component type is not listed in EngineComponents");

        T* current = TryGetComponent<T>(entity);

        if (!current)
        {
            return false;
        }

        *current = component;
        return true;
    }

    // Fails (false) if the entity is dead or already has one
    template <class T> bool Scene::AddComponent(Entity entity, const T& component)
    {
        static_assert(IsEngineComponent<T>, "Component type is not listed in EngineComponents");

        if (!IsAlive(entity) || HasComponent<T>(entity))
        {
            return false;
        }

        RequireNotIterating();

        // The input may refer to data already stored in the registry.
        const T snapshot = component;

        Registry().emplace<T>(Handle(entity), snapshot);
        return true;
    }

    // false if it didn't have one. EntityIdComponent can never be removed
    template <class T> bool Scene::RemoveComponent(Entity entity)
    {
        static_assert(IsEngineComponent<T>, "Component type is not listed in EngineComponents");

        if constexpr (std::is_same_v<T, EntityIdComponent>)
        {
            return false;
        }
        else
        {
            if (!IsAlive(entity) || !HasComponent<T>(entity))
            {
                return false;
            }

            RequireNotIterating();

            return Registry().remove<T>(Handle(entity)) != 0;
        }
    }

// Type cannot be put in parentheses here (it is a template argument), so the macro check is off for this macro
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ATELIER_INSTANTIATE_COMPONENT_API(Type)                                                                        \
    template bool Scene::HasComponent<Type>(Entity) const;                                                             \
    template Type* Scene::TryGetComponent<Type>(Entity);                                                               \
    template const Type* Scene::TryGetComponent<Type>(Entity) const;                                                   \
    template std::optional<Type> Scene::GetComponent<Type>(Entity) const;                                              \
    template bool Scene::SetComponent<Type>(Entity, const Type&);                                                      \
    template bool Scene::AddComponent<Type>(Entity, const Type&);                                                      \
    template bool Scene::RemoveComponent<Type>(Entity);
    // NOLINTEND(bugprone-macro-parentheses)

    ATELIER_INSTANTIATE_COMPONENT_API(TransformComponent)
    ATELIER_INSTANTIATE_COMPONENT_API(MeshRendererComponent)
    ATELIER_INSTANTIATE_COMPONENT_API(LightComponent)
    ATELIER_INSTANTIATE_COMPONENT_API(CameraComponent)
    ATELIER_INSTANTIATE_COMPONENT_API(AudioSourceComponent)
    ATELIER_INSTANTIATE_COMPONENT_API(PlayerControllerComponent)
    ATELIER_INSTANTIATE_COMPONENT_API(SpinComponent)
    ATELIER_INSTANTIATE_COMPONENT_API(HierarchyComponent)
    ATELIER_INSTANTIATE_COMPONENT_API(RigidBodyComponent)
    ATELIER_INSTANTIATE_COMPONENT_API(PhysicsBodyComponent)
    ATELIER_INSTANTIATE_COMPONENT_API(ScreenElementComponent)
    ATELIER_INSTANTIATE_COMPONENT_API(AnimatorComponent)
    ATELIER_INSTANTIATE_COMPONENT_API(SkyComponent)

#undef ATELIER_INSTANTIATE_COMPONENT_API
}
