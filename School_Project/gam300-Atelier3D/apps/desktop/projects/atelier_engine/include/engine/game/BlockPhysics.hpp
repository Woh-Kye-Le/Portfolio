/* !
@file    	    BlockPhysics.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		01/11/2026
@brief
    Declares BlockPhysics, the block language's way into a game's physics:
    Motion blocks walk Dynamic bodies, Physics blocks jump and push them, and
    walking characters stay upright.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/logic/LogicRunner.hpp"
#include "engine/scene/systems/PhysicsSystem.hpp"

#include <glm/vec3.hpp>

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace Engine::Scene
{
    class Scene;
    struct PhysicsBodyComponent;
}

namespace Engine::Physics
{
    class PhysicsWorld;
}

namespace Engine::Systems
{
    struct CastRay;
}

namespace Engine::Game
{
    // Logic::LogicRunner::PhysicsAccess for a game's PhysicsWorld. A Motion block on an object with a Dynamic Rigid
    // Body walks it: the frame's moves become its velocity for one physics step, so walls and other bodies stop it
    // (it cannot pass through them, however fast) and it pushes what it walks into. Jump and push change its velocity.
    //
    // An object that walks, a player, and a body with Keep Upright is a character: it is kept upright (its spin is taken away before each step,
    // and after it its Transform is levelled to the way it faces, so it never tips over) and it stops when it stops
    // walking, rather than sliding on.
    //
    // Each frame in Play: Bind, the blocks run (Walk, Push, Jump, OnGround), BeforeStep, PhysicsSystem::Update,
    // AfterStep.
    class BlockPhysics final : public Logic::LogicRunner::PhysicsAccess
    {
    public:
        // The ECS world and physics world of this frame
        void Bind(Scene::Scene& scene, Physics::PhysicsWorld& world);
        // Turns the frame's walks into velocities, and keeps characters upright
        void BeforeStep(double deltaSeconds);
        // Characters are levelled, and those that walked this frame stop going sideways (falling and jumping carry on)
        void AfterStep();
        // Forgets the walks (a new Play session)
        void Clear();
        // Who casts the rays the blocks ask for (GameRuntime::CastRay); with none set, a block's ray hits nothing
        using RayCaster = std::function<std::optional<Systems::RaycastHit>(
            const glm::vec3& origin, const glm::vec3& direction, float length, Scene::Entity ignore)>;
        void SetRayCaster(RayCaster caster);
        // Who hears about each ground check (GameRuntime::RecordRay, for the debug lines). It asks the physics for the
        // ground rather than casting a ray, so it does not go through the caster.
        void SetRayRecorder(std::function<void(const Systems::CastRay&)> recorder);

        bool Walk(Scene::Entity entity, const glm::vec3& offset) override;
        bool Push(Scene::Entity entity, const glm::vec3& velocityChange) override;
        bool ApplyForce(Scene::Entity entity, const glm::vec3& force, double seconds) override;
        bool Jump(Scene::Entity entity, float height) override;
        std::optional<bool> OnGround(Scene::Entity entity) const override;
        bool Raycast(Scene::Entity entity, const glm::vec3& direction, float length) override;
        std::optional<std::string> SurfaceUnder(Scene::Entity entity) const override;
        std::optional<RayResult> CastRay(Scene::Entity from, Scene::Entity target, float height, float range,
                                         float yawOff, float pitchOff) const override;
        // Where the player's view is this frame and which way it looks: the player's shots go along it
        void SetAim(const glm::vec3& origin, const glm::vec3& direction);

    private:
        // Whether the entity has a Dynamic Rigid Body (its physics body may not be made yet)
        bool IsDynamic(Scene::Entity entity) const;
        // Its physics body, when it is Dynamic and the body has been made (nullptr otherwise)
        const Scene::PhysicsBodyComponent* DynamicBody(Scene::Entity entity) const;
        // The characters this frame: the players, and what walked
        std::vector<Scene::Entity> Characters();
        void StopSpinning(Scene::Entity entity);
        void Level(Scene::Entity entity);
        // Whether the body stands on something, telling the recorder about the check
        bool CheckGround(Scene::Entity entity, const Scene::PhysicsBodyComponent& body) const;

        Scene::Scene* scene_ = nullptr;
        Physics::PhysicsWorld* world_ = nullptr;
        glm::vec3 aimOrigin_{0.0f};
        glm::vec3 aimDirection_{0.0f, 0.0f, -1.0f};
        bool aimed_ = false;
        RayCaster castRay_;
        std::function<void(const Systems::CastRay&)> recordRay_;
        std::map<Scene::Entity::Id, glm::vec3> walks_; // this frame's moves, until BeforeStep
        std::set<Scene::Entity::Id> walked_;           // the entities BeforeStep walked, until AfterStep
        // Retains impulses until the physics body exists.
        std::map<Scene::Entity::Id, glm::vec3> pendingImpulses_;
    };
}
