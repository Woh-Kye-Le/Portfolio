/* !
@file    	    BlockPhysics.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		01/11/2026
@brief
    Implements BlockPhysics: walking, jumping and pushing Dynamic bodies for
    the blocks, and keeping characters upright.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/game/BlockPhysics.hpp"

#include "engine/graphics/DebugLines.hpp"
#include "engine/physics/PhysicsWorld.hpp"
#include "engine/scene/Components.hpp"
#include "engine/scene/Scene.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <utility>
#include <vector>

namespace Engine::Game
{
    namespace
    {
        // How far under its feet a body still stands on what is there, for its footsteps: a stride's bob, not a jump
        constexpr float kSurfaceReach = 0.3f;
        // Where a shot that is not the player's starts, above the shooter's place (its feet, for a character)
    }

    void BlockPhysics::Bind(Scene::Scene& scene, Physics::PhysicsWorld& world)
    {
        scene_ = &scene;
        world_ = &world;
    }

    void BlockPhysics::Clear()
    {
        walks_.clear();
        walked_.clear();
        pendingImpulses_.clear();
    }

    void BlockPhysics::SetRayCaster(RayCaster caster)
    {
        castRay_ = std::move(caster);
    }

    void BlockPhysics::SetRayRecorder(std::function<void(const Systems::CastRay&)> recorder)
    {
        recordRay_ = std::move(recorder);
    }

    bool BlockPhysics::IsDynamic(Scene::Entity entity) const
    {
        const auto* body = scene_ && entity ? scene_->TryGetComponent<Scene::RigidBodyComponent>(entity) : nullptr;
        return body && body->type == Scene::BodyType::Dynamic;
    }

    const Scene::PhysicsBodyComponent* BlockPhysics::DynamicBody(Scene::Entity entity) const
    {
        if (!world_ || !IsDynamic(entity))
        {
            return nullptr;
        }
        const auto* body = scene_->TryGetComponent<Scene::PhysicsBodyComponent>(entity);
        return body && body->settings.type == Scene::BodyType::Dynamic && world_->HasBody(body->body) ? body : nullptr;
    }

    bool BlockPhysics::Walk(Scene::Entity entity, const glm::vec3& offset)
    {
        // Until its body is made (the first physics step of Play) the Transform is moved, as without physics
        if (!DynamicBody(entity))
        {
            return false;
        }
        walks_[entity.GetId()] += offset;
        return true;
    }

    bool BlockPhysics::Push(Scene::Entity entity, const glm::vec3& velocityChange)
    {
        if (!IsDynamic(entity))
        {
            return false;
        }
        if (const auto* body = DynamicBody(entity))
        {
            world_->AddImpulse(body->body, velocityChange * body->settings.mass);
        }
        return true;
    }

    bool BlockPhysics::ApplyForce(Scene::Entity entity, const glm::vec3& force, double seconds)
    {
        if (!IsDynamic(entity))
        {
            return false;
        }

        // A debugger block step advances no physics time.
        if (seconds <= 0.0)
        {
            return true;
        }

        auto [it, inserted] = pendingImpulses_.try_emplace(entity.GetId(), glm::vec3(0.0f));

        it->second += force * static_cast<float>(seconds);
        return true;
    }

    bool BlockPhysics::Jump(Scene::Entity entity, float height)
    {
        if (!IsDynamic(entity))
        {
            return false;
        }
        const auto* body = DynamicBody(entity);
        if (!body || height <= 0.0f || !CheckGround(entity, *body))
        {
            return true; // not standing on anything (yet): no jump, and nothing wrong
        }
        // Up is against the gravity: leaving the ground at sqrt(2 g h) rises h before the gravity stops it
        const glm::vec3 gravity = world_->Gravity() * body->settings.gravityScale;
        const float pull = glm::length(gravity);
        if (pull <= 0.0f)
        {
            return true;
        }
        const glm::vec3 up = -gravity / pull;
        glm::vec3 velocity = world_->Velocity(body->body).value_or(glm::vec3(0.0f));
        velocity += up * (std::sqrt(2.0f * pull * height) - glm::dot(velocity, up));
        world_->SetVelocity(body->body, velocity);
        return true;
    }

    std::optional<bool> BlockPhysics::OnGround(Scene::Entity entity) const
    {
        if (!scene_ || !entity || !scene_->TryGetComponent<Scene::RigidBodyComponent>(entity))
        {
            return std::nullopt;
        }
        const auto* body = scene_->TryGetComponent<Scene::PhysicsBodyComponent>(entity);
        return world_ && body && world_->HasBody(body->body) && CheckGround(entity, *body);
    }

    bool BlockPhysics::CheckGround(Scene::Entity entity, const Scene::PhysicsBodyComponent& body) const
    {
        const std::optional<Physics::Ground> ground = world_->GroundUnder(body.body);
        const auto from = scene_->GetTransform(entity);
        if (recordRay_ && from)
        {
            recordRay_(Systems::DebugLineSystem::GroundRay(
                glm::vec3(from->x, from->y, from->z), world_->Gravity() * body.settings.gravityScale,
                ground ? std::optional<glm::vec3>(ground->point) : std::nullopt));
        }
        return ground.has_value();
    }

    bool BlockPhysics::Raycast(Scene::Entity entity, const glm::vec3& direction, float length)
    {
        const auto from = scene_ && entity ? scene_->GetTransform(entity) : std::nullopt;
        if (!from || !castRay_)
        {
            return false;
        }
        return castRay_(glm::vec3(from->x, from->y, from->z), direction, length, entity).has_value();
    }

    std::optional<std::string> BlockPhysics::SurfaceUnder(Scene::Entity entity) const
    {
        if (!scene_ || !world_)
        {
            return std::nullopt;
        }
        // The body that carries it: its own, or a parent's (a model drawn inside a player)
        const Scene::PhysicsBodyComponent* body = nullptr;
        for (Scene::Entity carrier = entity; carrier;)
        {
            body = scene_->TryGetComponent<Scene::PhysicsBodyComponent>(carrier);
            if (body)
            {
                break;
            }
            const auto* link = scene_->TryGetComponent<Scene::HierarchyComponent>(carrier);
            carrier = link ? scene_->FindEntity(link->parent) : Scene::Entity{};
        }
        const auto pose = body ? world_->Pose(body->body) : std::nullopt;
        if (!pose)
        {
            return std::nullopt;
        }
        // Straight down from its collider's middle, past its lowest point to a little below its feet
        const Physics::Collider& collider = body->collider;
        const float below = collider.shape == Physics::ShapeType::Box       ? collider.halfExtents.y
                            : collider.shape == Physics::ShapeType::Capsule ? collider.halfHeight + collider.radius
                                                                            : collider.radius;
        const auto hit = world_->CastRay(pose->position + collider.center, glm::vec3(0.0f, -1.0f, 0.0f),
                                         below + kSurfaceReach, body->body);
        if (!hit)
        {
            return std::nullopt;
        }
        const Scene::Entity ground = scene_->FindEntity(static_cast<Scene::Entity::Id>(hit->userData));
        const auto* floor = ground ? scene_->TryGetComponent<Scene::RigidBodyComponent>(ground) : nullptr;
        return floor ? floor->surface : std::string{};
    }

    void BlockPhysics::SetAim(const glm::vec3& origin, const glm::vec3& direction)
    {
        aimOrigin_ = origin;
        aimDirection_ = glm::length(direction) > 0.0f ? glm::normalize(direction) : glm::vec3(0.0f, 0.0f, -1.0f);
        aimed_ = true;
    }

    std::optional<Logic::LogicRunner::PhysicsAccess::RayResult> BlockPhysics::CastRay(
        Scene::Entity from, Scene::Entity target, float height, float range, float yawOff, float pitchOff) const
    {
        if (!scene_ || !world_ || !from)
        {
            return std::nullopt;
        }
        // Up from it (its model, its gun) to the object that carries it: its body is not hit, and a player looks along
        // its view
        Physics::BodyId own;
        Scene::Entity carrier;
        bool player = false;
        for (Scene::Entity each = from; each;)
        {
            if (const auto* body = scene_->TryGetComponent<Scene::PhysicsBodyComponent>(each); body && !own.IsValid())
            {
                own = body->body;
                carrier = each;
            }
            player = player || scene_->HasComponent<Scene::PlayerControllerComponent>(each);
            const auto* link = scene_->TryGetComponent<Scene::HierarchyComponent>(each);
            each = link ? scene_->FindEntity(link->parent) : Scene::Entity{};
        }
        const auto place = scene_->GetTransform(from);
        if (!place)
        {
            return std::nullopt;
        }
        glm::vec3 origin(place->x, place->y + height, place->z);
        glm::vec3 direction;
        if (target)
        {
            const auto to = scene_->GetTransform(target);
            if (!to)
            {
                return std::nullopt;
            }
            // Its middle, when its body says where that is
            const auto* body = scene_->TryGetComponent<Scene::RigidBodyComponent>(target);
            direction = glm::vec3(to->x, to->y + (body ? body->centerY : 0.0f), to->z) - origin;
        }
        else if (player && aimed_)
        {
            origin = aimOrigin_;
            direction = aimDirection_;
        }
        else
        {
            const float yaw = glm::radians(place->rotation);
            const float tilt = glm::radians(place->rotationX);
            direction = glm::vec3(-std::sin(yaw) * std::cos(tilt), std::sin(tilt), -std::cos(yaw) * std::cos(tilt));
        }
        const float along = glm::length(glm::vec2(direction.x, direction.z));
        const float yaw = std::atan2(direction.x, direction.z) + glm::radians(yawOff);
        const float tilt = std::atan2(direction.y, along) + glm::radians(pitchOff);
        const glm::vec3 line(std::sin(yaw) * std::cos(tilt), std::sin(tilt), std::cos(yaw) * std::cos(tilt));
        // Through the game's entry point (its debug line shows); with none set, straight to the physics
        if (castRay_)
        {
            const auto cast = castRay_(origin, line, range, carrier);
            if (!cast)
            {
                return std::nullopt;
            }
            return RayResult{.entity = cast->entity.GetId(), .distance = cast->distance};
        }
        const auto hit = world_->CastRay(origin, line, range, own);
        if (!hit)
        {
            return std::nullopt;
        }
        return RayResult{.entity = static_cast<Scene::Entity::Id>(hit->userData), .distance = hit->distance};
    }

    std::vector<Scene::Entity> BlockPhysics::Characters()
    {
        std::vector<Scene::Entity> characters;
        scene_->ForEach<Scene::PlayerControllerComponent, Scene::PhysicsBodyComponent>(
            [&characters](Scene::Entity entity, const Scene::PlayerControllerComponent&,
                          const Scene::PhysicsBodyComponent&) { characters.push_back(entity); });
        for (const Scene::Entity::Id id : walked_)
        {
            characters.push_back(scene_->FindEntity(id));
        }
        scene_->ForEach<Scene::RigidBodyComponent, Scene::PhysicsBodyComponent>(
            [&characters](Scene::Entity entity, const Scene::RigidBodyComponent& body,
                          const Scene::PhysicsBodyComponent&)
            {
                if (body.keepUpright)
                {
                    characters.push_back(entity);
                }
            });
        return characters;
    }

    void BlockPhysics::StopSpinning(Scene::Entity entity)
    {
        if (const auto* body = DynamicBody(entity))
        {
            world_->SetAngularVelocity(body->body, glm::vec3(0.0f));
        }
    }

    // Only the way it faces stays. That is not simply its Y rotation: the physics writes a turn of more than 90 degrees
    // back as X and Z turned 180 and Y less than 90 (the same rotation), so the heading is read from where its front
    // (-Z) points. The body is put back level on the next step (PhysicsSystem sees its Transform moved).
    void BlockPhysics::Level(Scene::Entity entity)
    {
        auto* transform = DynamicBody(entity) ? scene_->TryGetComponent<Scene::TransformComponent>(entity) : nullptr;
        if (!transform || (transform->rotationX == 0.0f && transform->rotationZ == 0.0f))
        {
            return;
        }
        const glm::quat rotation = glm::angleAxis(glm::radians(transform->rotationZ), glm::vec3(0.0f, 0.0f, 1.0f)) *
                                   glm::angleAxis(glm::radians(transform->rotation), glm::vec3(0.0f, 1.0f, 0.0f)) *
                                   glm::angleAxis(glm::radians(transform->rotationX), glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::vec3 front = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        if (front.x * front.x + front.z * front.z > 1e-8f) // facing straight up or down: keep the heading it had
        {
            // A rotation of r degrees turns the front to (-sin r, 0, -cos r)
            transform->rotation = glm::degrees(std::atan2(-front.x, -front.z));
        }
        transform->rotationX = 0.0f;
        transform->rotationZ = 0.0f;
    }

    void BlockPhysics::BeforeStep(double deltaSeconds)
    {
        if (!scene_ || !world_ || deltaSeconds <= 0.0)
        {
            walks_.clear();
            return;
        }
        const auto dt = static_cast<float>(deltaSeconds);
        for (const auto& [id, offset] : walks_)
        {
            const Scene::Entity entity = scene_->FindEntity(id);
            const auto* body = DynamicBody(entity);
            if (!body)
            {
                continue;
            }
            // Sideways it goes as far as it walked, up to a wall and along it; up and down it keeps falling (or
            // jumping) unless it walked that way too
            const glm::vec3 now = world_->Velocity(body->body).value_or(glm::vec3(0.0f));
            const glm::vec3 clear = world_->ClipWalk(body->body, offset);
            world_->SetVelocity(body->body,
                                glm::vec3(clear.x / dt, offset.y != 0.0f ? offset.y / dt : now.y, clear.z / dt));
            walked_.insert(id);
        }
        walks_.clear();

        for (const Scene::Entity entity : Characters())
        {
            StopSpinning(entity);
            Level(entity); // in case something else tilted it since the last step
        }

        for (auto it = pendingImpulses_.begin(); it != pendingImpulses_.end();)
        {
            const auto entity = scene_->FindEntity(it->first);

            if (!entity || !IsDynamic(entity))
            {
                it = pendingImpulses_.erase(it);
            }
            else if (const auto* body = DynamicBody(entity))
            {
                world_->AddImpulse(body->body, it->second);
                it = pendingImpulses_.erase(it);
            }
            else
            {
                // The following physics update may create the body.
                ++it;
            }
        }
    }

    void BlockPhysics::AfterStep()
    {
        if (scene_ && world_)
        {
            // Level before the blocks and the mouse turn it on the next frame: they turn its Y rotation
            for (const Scene::Entity entity : Characters())
            {
                Level(entity);
            }
            for (const Scene::Entity::Id id : walked_)
            {
                if (const auto* body = DynamicBody(scene_->FindEntity(id)))
                {
                    const glm::vec3 now = world_->Velocity(body->body).value_or(glm::vec3(0.0f));
                    world_->SetVelocity(body->body, glm::vec3(0.0f, now.y, 0.0f));
                }
            }
        }
        walked_.clear();
    }
}
