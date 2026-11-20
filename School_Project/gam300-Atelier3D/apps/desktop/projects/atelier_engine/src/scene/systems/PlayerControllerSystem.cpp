/* !
@file    	    PlayerControllerSystem.cpp
@author  	    sung.h@digipen.edu [67%]
         	    c.kojat@digipen.edu [33%]
@date    		01/10/2026
@brief
    Implements PlayerControllerSystem: movement relative to facing, mouse look
    and camera distance, and the follow view for the Main camera or the game
    view.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/scene/systems/PlayerControllerSystem.hpp"

#include "engine/graphics/Camera/Camera3D.hpp"
#include "engine/input/GameInput.hpp"
#include "engine/physics/PhysicsWorld.hpp"
#include "engine/scene/Components.hpp"
#include "engine/scene/Scene.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <optional>

namespace Engine::Systems
{
    namespace
    {
        // Where a camera behind the player sits and which way it looks. yaw and pitch use the camera Transform
        // convention RenderSystem::ApplyPrimaryCamera reads: forward = (cos p sin y, sin p, -cos p cos y).
        struct FollowView
        {
            glm::vec3 position{0.0f};
            glm::vec3 forward{0.0f, 0.0f, -1.0f};
            float yaw = 0.0f;
            float pitch = 0.0f;
        };

        // A Transform rotation of r degrees turns the entity's front (-Z) to (-sin r, 0, -cos r)
        glm::vec3 FacingOf(float rotationDegrees)
        {
            const float r = glm::radians(rotationDegrees);
            return {-std::sin(r), 0.0f, -std::cos(r)};
        }

        std::optional<FollowView> FindFollowView(Scene::Scene& scene, const Physics::PhysicsWorld* world)
        {
            Scene::Entity::Id bestId = 0;
            std::optional<FollowView> best;

            scene.ForEach<const Scene::TransformComponent, const Scene::PlayerControllerComponent>(
                [&](Scene::Entity entity, const Scene::TransformComponent& transform,
                    const Scene::PlayerControllerComponent& player)
                {
                    if (!player.followCamera || (best && entity.GetId() >= bestId))
                    {
                        return;
                    }
                    const float pitch = glm::radians(player.cameraPitch);
                    const glm::vec3 forward =
                        FacingOf(transform.rotation) * std::cos(pitch) + glm::vec3(0.0f, std::sin(pitch), 0.0f);
                    const glm::vec3 target(transform.x, transform.y + player.cameraOffsetY, transform.z);

                    // Back from the player's head to where the camera goes: what is in the way (not the player's own
                    // body) brings it in to just in front of that
                    float distance = player.cameraOffsetZ;
                    if (world != nullptr && distance > 0.0f)
                    {
                        const auto* body = scene.TryGetComponent<Scene::PhysicsBodyComponent>(entity);
                        const auto hit =
                            world->CastRay(target, -forward, distance + PlayerControllerSystem::kCameraClearance,
                                           body ? body->body : Physics::BodyId{});
                        if (hit)
                        {
                            distance = std::clamp(hit->distance - PlayerControllerSystem::kCameraClearance,
                                                  PlayerControllerSystem::kClosestCamera, distance);
                        }
                    }

                    bestId = entity.GetId();
                    best = FollowView{.position = target - forward * distance,
                                      .forward = forward,
                                      .yaw = -transform.rotation,
                                      .pitch = player.cameraPitch};
                });
            return best;
        }
    }

    // Read the input once, then apply it to every player
    void PlayerControllerSystem::Update(Scene::Scene& scene, const Input::GameInput& input, double deltaSeconds)
    {
        namespace Action = Input::Action;

        const auto axis = [&input](std::string_view positive, std::string_view negative)
        { return (input.IsActionDown(positive) ? 1.0f : 0.0f) - (input.IsActionDown(negative) ? 1.0f : 0.0f); };

        float right = axis(Action::MoveRight, Action::MoveLeft);
        float forward = axis(Action::MoveForward, Action::MoveBack);
        const float up = axis(Action::MoveUp, Action::MoveDown);
        if (const float length = std::sqrt(right * right + forward * forward); length > 1.0f)
        {
            right /= length; // diagonals are no faster than straight lines
            forward /= length;
        }
        const bool fast = input.IsActionDown(Action::MoveFast);
        // Holding Look (right mouse) turns with the mouse, and so does the mouse alone while the game has the pointer
        const Input::Vec2 look =
            input.IsActionDown(Action::Look) || input.IsPointerLocked() ? input.GetMouseDelta() : Input::Vec2{};
        const float scroll = input.GetScrollDelta();

        scene.ForEach<Scene::TransformComponent, Scene::PlayerControllerComponent>(
            [&](Scene::Entity, Scene::TransformComponent& transform, Scene::PlayerControllerComponent& player)
            {
                // Mouse right turns right (a smaller rotation around Y); mouse up tilts the camera up
                transform.rotation -= look.x * player.lookSensitivity;
                player.cameraPitch =
                    std::clamp(player.cameraPitch - look.y * player.lookSensitivity, kMinPitch, kMaxPitch);
                // A first-person camera (no distance behind the player) has nothing to zoom
                if (scroll != 0.0f && player.cameraOffsetZ > 0.0f)
                {
                    player.cameraOffsetZ =
                        std::clamp(player.cameraOffsetZ * std::pow(0.9f, scroll), kMinDistance, kMaxDistance);
                }

                const float step =
                    player.speed * (fast ? player.fastMultiplier : 1.0f) * static_cast<float>(deltaSeconds);
                const glm::vec3 front = FacingOf(transform.rotation);
                const glm::vec3 side(-front.z, 0.0f, front.x); // the entity's right
                const glm::vec3 move = (front * forward + side * right) * step;

                transform.x += move.x;
                transform.y += up * step;
                transform.z += move.z;
            });
    }

    // Moves the Main camera's Transform, so RenderSystem then renders from behind the player
    bool PlayerControllerSystem::MoveMainCameraToPlayer(Scene::Scene& scene, const Physics::PhysicsWorld* world)
    {
        const std::optional<FollowView> view = FindFollowView(scene, world);
        if (!view)
        {
            return false;
        }

        bool moved = false;
        Scene::Entity::Id bestId = 0;
        Scene::TransformComponent* mainCamera = nullptr;
        scene.ForEach<Scene::TransformComponent, const Scene::CameraComponent>(
            [&](Scene::Entity entity, Scene::TransformComponent& transform, const Scene::CameraComponent& camera)
            {
                // The same camera RenderSystem::ApplyPrimaryCamera picks: the primary one with the lowest id
                if (camera.primary && (!mainCamera || entity.GetId() < bestId))
                {
                    bestId = entity.GetId();
                    mainCamera = &transform;
                }
            });
        if (mainCamera)
        {
            mainCamera->x = view->position.x;
            mainCamera->y = view->position.y;
            mainCamera->z = view->position.z;
            mainCamera->rotation = view->yaw;
            mainCamera->rotationX = view->pitch;
            mainCamera->rotationZ = 0.0f;
            moved = true;
        }
        return moved;
    }

    bool PlayerControllerSystem::FollowWithCamera(Scene::Scene& scene, Graphics::Camera3D& camera,
                                                  const Physics::PhysicsWorld* world)
    {
        const std::optional<FollowView> view = FindFollowView(scene, world);
        if (!view)
        {
            return false;
        }
        camera.SetPosition(view->position);
        camera.SetOrientation(view->forward);
        return true;
    }
}
