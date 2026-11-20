/* !
@file    	    PhysicsSystem.hpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		26/10/2026
@brief
    Declares the ECS PhysicsSystem, which gives each entity with a RigidBody a
    body in the PhysicsWorld, steps the simulation, moves the entities of
    dynamic bodies to where the physics put them, and reports which entities
    started or stopped touching. It also casts rays to find which entity is in
    a direction, measures how far apart entities are and finds those near a
    point, tells whether an entity stands on the ground, and gives the
    collider outlines the editor draws.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/assets/ResourceManager.hpp"

#include "engine/assets/Animation.hpp"
#include "engine/game/Shape.hpp"
#include "engine/physics/Body.hpp"
#include "engine/physics/ColliderLines.hpp"
#include "engine/scene/Entity.hpp"
#include "engine/scene/components/MeshRendererComponent.hpp"
#include "engine/scene/components/RigidBodyComponent.hpp"
#include "engine/scene/components/TransformComponent.hpp"

#include <glm/glm.hpp>

#include <optional>
#include <vector>

namespace Engine::Scene
{
    class Scene;
}

namespace Engine::Physics
{
    class PhysicsWorld;
}

namespace Engine::Systems
{
    // Two entities' bodies started or stopped touching
    struct Collision
    {
        Scene::Entity a;
        Scene::Entity b;
        bool began = true; // false: they stopped touching

        // When began only (zero otherwise):
        glm::vec3 point{0.0f};    // where they touch, in world space
        glm::vec3 normal{0.0f};   // unit length, pointing from a to b
        float impactSpeed = 0.0f; // how fast they were closing in, in units per second (how hard they hit)
    };

    // The first entity a ray hits (PhysicsSystem::Raycast)
    struct RaycastHit
    {
        Scene::Entity entity;
        glm::vec3 point{0.0f};  // where the ray meets its collider, in world space
        glm::vec3 normal{0.0f}; // unit length, out of the collider there (back along the ray if it started inside)
        float distance = 0.0f;  // from the ray's start to the point, in units
    };

    // An entity near a point (PhysicsSystem::EntitiesWithin)
    struct NearbyEntity
    {
        Scene::Entity entity;
        float distance = 0.0f; // from the point to the nearest point of its collider; 0 if the point is inside
    };

    // The entity another one stands on (PhysicsSystem::GroundUnder)
    struct GroundHit
    {
        Scene::Entity entity;
        glm::vec3 point{0.0f};  // where they meet, on its collider, in world space
        glm::vec3 normal{0.0f}; // unit length, out of its collider there: up on flat ground
    };

    // An entity's collider, ready to draw
    struct ColliderOutline
    {
        Scene::Entity entity;
        Scene::BodyType type = Scene::BodyType::Dynamic;
        std::uint32_t layers = 1;
        bool asleep = false;   // a moving body that has come to rest (Play mode)
        bool touching = false; // touches another body (Play mode)
        std::vector<Physics::Line> lines;
    };

    // The ECS system for RigidBodyComponents. Bodies are made when the game plays (edit mode makes none), live in
    // the GameRuntime's PhysicsWorld, and are tracked on their entity with a PhysicsBodyComponent.
    class PhysicsSystem
    {
    public:
        /*!
        \brief   One frame in Play mode. Makes a body for every entity with a Transform and a RigidBody (again when
                 its RigidBody, size or shape changed) and removes the body of an entity that lost either one. Puts
                 a body where its entity is when something other than the physics moved the entity (the Inspector,
                 a gizmo, game code moving a kinematic body). Then steps the world by deltaSeconds and moves the
                 entity of each dynamic body to where the physics put it.
        \param   scene The ECS world.
        \param   world The physics world the bodies live in. Nothing happens if it is not running.
        \param   deltaSeconds The time since the last frame, in seconds.
        \return  The entities that started or stopped touching this frame, in the order it happened. Each entity's
                 PhysicsBodyComponent::touching is kept up to date as well. A pair where one entity has been destroyed
                 is left out (but taken off the other's touching list).
        */
        static std::vector<Collision> Update(Scene::Scene& scene, Physics::PhysicsWorld& world, double deltaSeconds,
                                             Assets::ResourceManager* resources = nullptr);

        /*!
        \brief   A Mesh collider made of what the object draws: its model's meshes, each placed by its nodes, or its
                 mesh, turned and scaled as the object draws them. No triangles if it draws neither.
        */
        static Physics::Collider MeshColliderFor(const Scene::TransformComponent& transform,
                                                 const Scene::MeshRendererComponent* renderer,
                                                 Assets::ResourceManager& resources);

        /*!
        \brief   Removes an entity's body, if it has one. Call it before destroying the entity, or the body stays in
                 the world.
        \param   scene The ECS world.
        \param   world The physics world the body lives in.
        \param   entity The entity whose body to remove.
        */
        static void RemoveBody(Scene::Scene& scene, Physics::PhysicsWorld& world, Scene::Entity entity);

        /*!
        \brief   Puts the body of every entity something other than the physics moved (the Inspector, a gizmo, a block)
                 where its entity is now, with no time passing. Update does this itself; a paused game calls it alone.
        \param   scene The ECS world.
        \param   world The physics world the bodies live in.
        */
        static void SyncMoved(Scene::Scene& scene, Physics::PhysicsWorld& world);

        // ---- Pushing entities (game code, blocks). Only dynamic bodies are pushed; each returns false when the
        //      entity has no body (yet: bodies are made by the first Update after its RigidBody is added).

        /*!
        \brief   Kicks an entity's body once through its centre: see PhysicsWorld::AddImpulse.
        \param   scene The ECS world.
        \param   world The physics world its body lives in.
        \param   entity The entity to kick.
        \param   impulse The kick in kilogram units per second.
        \return  True if it has a body.
        */
        static bool AddImpulse(const Scene::Scene& scene, Physics::PhysicsWorld& world, Scene::Entity entity,
                               const glm::vec3& impulse);

        /*!
        \brief   Pushes an entity's body steadily until changed (zero stops it): see PhysicsWorld::SetConstantForce.
                 A body made again (its RigidBody changed) starts without it.
        \param   scene The ECS world.
        \param   world The physics world its body lives in.
        \param   entity The entity to push.
        \param   force The push in newtons.
        \return  True if it has a body.
        */
        static bool SetConstantForce(const Scene::Scene& scene, Physics::PhysicsWorld& world, Scene::Entity entity,
                                     const glm::vec3& force);

        /*!
        \brief   Sets how fast and which way an entity's body moves.
        \param   scene The ECS world.
        \param   world The physics world its body lives in.
        \param   entity The entity to set.
        \param   velocity In units per second.
        \return  True if it has a body.
        */
        static bool SetVelocity(const Scene::Scene& scene, Physics::PhysicsWorld& world, Scene::Entity entity,
                                const glm::vec3& velocity);

        /*!
        \brief   How fast and which way an entity's body moves.
        \param   scene The ECS world.
        \param   world The physics world its body lives in.
        \param   entity The entity to look at.
        \return  Its velocity in units per second, or nullopt if it has no body.
        */
        static std::optional<glm::vec3> VelocityOf(const Scene::Scene& scene, const Physics::PhysicsWorld& world,
                                                   Scene::Entity entity);

        /*!
        \brief   Casts a ray and finds the first entity whose collider it hits, as of the last Update: see
                 PhysicsWorld::CastRay. Only entities with a body (a RigidBody, in Play mode) can be hit.
        \param   scene The ECS world.
        \param   world The physics world the bodies live in.
        \param   origin Where the ray starts, in world space.
        \param   direction Which way it goes, of any length but zero.
        \param   maxDistance How far it reaches, in units.
        \param   ignore An entity the ray passes through, usually the one casting it (which often starts inside its
                 own collider); none by default.
        \return  The nearest hit, or nullopt if nothing is hit (see PhysicsWorld::CastRay), or if what it hits is the
                 body of an entity that no longer exists.
        */
        static std::optional<RaycastHit> Raycast(const Scene::Scene& scene, const Physics::PhysicsWorld& world,
                                                 const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                                                 Scene::Entity ignore = {},
                                                 std::uint32_t layerMask = Physics::kAllLayers);

        /*!
        \brief   How far apart two entities are, collider to collider, as of the last Update: see
                 PhysicsWorld::DistanceBetween.
        \param   scene The ECS world.
        \param   world The physics world the bodies live in.
        \param   a One entity; pointOnA is on its collider.
        \param   b The other; pointOnB is on its collider.
        \param   maxDistance Entities farther apart than this are out of range.
        \return  The gap, or nullopt if they are out of range, either has no body, or they are the same entity.
        */
        static std::optional<Physics::Gap> DistanceBetween(const Scene::Scene& scene,
                                                           const Physics::PhysicsWorld& world, Scene::Entity a,
                                                           Scene::Entity b, float maxDistance = Physics::DefaultMaxGap);

        /*!
        \brief   The entities whose colliders reach within a distance of a point, as of the last Update: see
                 PhysicsWorld::BodiesWithin.
        \param   scene The ECS world.
        \param   world The physics world the bodies live in.
        \param   point The point, in world space.
        \param   radius How far from it, in units.
        \param   ignore An entity to leave out, usually the one asking (its point is often inside its own collider);
                 none by default.
        \return  The live entities in range, nearest first.
        */
        static std::vector<NearbyEntity> EntitiesWithin(const Scene::Scene& scene, const Physics::PhysicsWorld& world,
                                                        const glm::vec3& point, float radius,
                                                        Scene::Entity ignore = {});

        /*!
        \brief   The entity an entity stands on, as of the last Update: see PhysicsWorld::GroundUnder.
        \param   scene The ECS world.
        \param   world The physics world the bodies live in.
        \param   entity The entity to look under.
        \param   maxSlope The steepest surface that counts as ground, in degrees from flat.
        \return  The ground, or nullopt if it stands on nothing, has no body, or what it stands on is the body of an
                 entity that no longer exists.
        */
        static std::optional<GroundHit> GroundUnder(const Scene::Scene& scene, const Physics::PhysicsWorld& world,
                                                    Scene::Entity entity, float maxSlope = Physics::DefaultMaxSlope);

        /*!
        \brief   Whether an entity stands on the ground: see GroundUnder.
        \param   scene The ECS world.
        \param   world The physics world the bodies live in.
        \param   entity The entity to look under.
        \param   maxSlope The steepest surface that counts as ground, in degrees from flat.
        \return  True if it stands on another entity.
        */
        static bool IsOnGround(const Scene::Scene& scene, const Physics::PhysicsWorld& world, Scene::Entity entity,
                               float maxSlope = Physics::DefaultMaxSlope);

        /*!
        \brief   The entities an entity's body touches now (as of the last Update).
        \param   scene The ECS world.
        \param   entity The entity to look at.
        \return  The live entities it touches, oldest contact first; empty if it has no body.
        */
        static std::vector<Scene::Entity> Touching(const Scene::Scene& scene, Scene::Entity entity);

        /*!
        \brief   Whether two entities' bodies touch now (as of the last Update).
        \param   scene The ECS world.
        \param   a One entity.
        \param   b The other; the order does not matter.
        \return  True if they touch.
        */
        static bool AreTouching(const Scene::Scene& scene, Scene::Entity a, Scene::Entity b);

        /*!
        \brief   The collider of every entity with a Transform and a RigidBody, as lines in world space. In Play mode a
                 body's collider is drawn where the physics has it, with whether it is asleep or touching something;
                 otherwise (edit mode, or a body not made yet) where its Transform is, as ColliderFor makes it.
        \param   scene The ECS world (not changed; ForEach needs it non-const).
        \param   world The physics world, or nullptr when there is none.
        \param   segments The lines each full circle is made of.
        \return  One outline per entity.
        */
        static std::vector<ColliderOutline> Outlines(Scene::Scene& scene, const Physics::PhysicsWorld* world,
                                                     int segments = 24);

        /*!
        \brief   The collider an entity's RigidBody gets, scaled with its Transform. Auto fits what the entity draws:
                 a sphere of radius 1 for a sphere, a thin slab with its top at the plane for a 1 x 1 plane, and a
                 1 x 1 x 1 box for a cube, any other shape, a mesh file or nothing. Box, Sphere and Capsule use the
                 RigidBody's own sizes and centre.
        \param   transform Where the entity is and how big it is.
        \param   renderer What the entity draws, or nullptr.
        \param   body The entity's RigidBody.
        \return  The collider, in world units, in the body's frame.
        */
        static Physics::Collider ColliderFor(const Scene::TransformComponent& transform,
                                             const Scene::MeshRendererComponent* renderer,
                                             const Scene::RigidBodyComponent& body);

        /*!
        \brief   Sets a RigidBody's collider centre and sizes to fit a built-in shape, in the object's own units (its
                 Transform still scales them). The editor uses it when a collider shape is picked. The box gets the
                 shape's size and the capsule its height (at least its width). The radius fits the RigidBody's
                 shape: for a capsule the larger of the X and Z half sizes, otherwise the largest half size.
        \param   body The RigidBody to change. Its shape and type are kept, and say which radius to fit.
        \param   drawn The built-in shape the object draws; nullopt when it draws nothing or a model or mesh file.
        \param   drawnBox The box around the model or mesh file it draws (Assets::RestBounds), which is fitted instead
                 of drawn; with neither, a 1 x 1 x 1 cube is fitted.
        */
        static void FitCollider(Scene::RigidBodyComponent& body, std::optional<Game::Shape> drawn,
                                const std::optional<Assets::BoundingBox>& drawnBox = std::nullopt);
    };
}
