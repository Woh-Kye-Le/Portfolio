#include "engine/graphics/Camera/Camera3D.hpp"
#include "engine/input/GameInput.hpp"
#include "engine/physics/PhysicsWorld.hpp"
#include "engine/scene/Scene.hpp"
#include "engine/scene/systems/PlayerControllerSystem.hpp"
#include <gtest/gtest.h>

#include <cmath>

namespace
{
    using namespace Engine::Scene;
    using namespace Engine::Input;
    using Engine::Systems::PlayerControllerSystem;

    // W is held the whole time: the entity only moves while it has a PlayerController, and stops/starts again on the very
    // next update when we remove/add it. The other entity never moves.
    TEST(EcsPlayerControllerSystem, AddingAndRemovingControllerChangesMovementImmediately)
    {
        Scene scene;
        const auto player = scene.CreateEntity();
        const auto scenery = scene.CreateEntity();
        InputSystem input;
        const auto actions = InputActions::Defaults();
        input.SetKey(Key_W, KeyState::Down);
        const GameInput gameInput(input, actions);
        PlayerControllerSystem::Update(scene, gameInput, 0.5);
        EXPECT_FLOAT_EQ(scene.GetTransform(player)->z, 0.0f);
        PlayerControllerComponent controller;
        controller.speed = 8.0f;
        ASSERT_TRUE(scene.AddComponent(player, controller));
        PlayerControllerSystem::Update(scene, gameInput, 0.5);
        EXPECT_FLOAT_EQ(scene.GetTransform(player)->z, -4.0f);
        EXPECT_FLOAT_EQ(scene.GetTransform(scenery)->z, 0.0f);
        ASSERT_TRUE(scene.RemoveComponent<PlayerControllerComponent>(player));
        PlayerControllerSystem::Update(scene, gameInput, 0.5);
        EXPECT_FLOAT_EQ(scene.GetTransform(player)->z, -4.0f);
        ASSERT_TRUE(scene.AddComponent(player, controller));
        PlayerControllerSystem::Update(scene, gameInput, 0.5);
        EXPECT_FLOAT_EQ(scene.GetTransform(player)->z, -8.0f);
    }

    // No Transform = nothing to move (and no crash). While a text box has the keyboard the player does not move either.
    TEST(EcsPlayerControllerSystem, RequiresTransformAndRespectsKeyboardCapture)
    {
        Scene scene;
        const auto player = scene.CreateEntity();
        ASSERT_TRUE(scene.AddComponent<PlayerControllerComponent>(player));
        ASSERT_TRUE(scene.RemoveComponent<TransformComponent>(player));
        InputSystem input;
        const auto actions = InputActions::Defaults();
        input.SetKey(Key_W, KeyState::Down);
        const GameInput gameInput(input, actions);
        EXPECT_NO_THROW(PlayerControllerSystem::Update(scene, gameInput, 1.0));
        EXPECT_FALSE(scene.GetTransform(player).has_value());
        ASSERT_TRUE(scene.AddComponent<TransformComponent>(player));
        input.SetCaptureInput(true);
        PlayerControllerSystem::Update(scene, gameInput, 1.0);
        EXPECT_FLOAT_EQ(scene.GetTransform(player)->z, 0.0f);
        input.SetCaptureInput(false);
        PlayerControllerSystem::Update(scene, gameInput, 1.0);
        EXPECT_FLOAT_EQ(scene.GetTransform(player)->z, -3.0f);
    }

    // Shift + D for 0.25 s at speed 4 x3 should move exactly 3 units to the right.
    TEST(EcsPlayerControllerSystem, FastMovementUsesComponentSpeedAndDeltaTime)
    {
        Scene scene;
        const auto player = scene.CreateEntity();
        PlayerControllerComponent controller;
        controller.speed = 4.0f;
        controller.fastMultiplier = 3.0f;
        ASSERT_TRUE(scene.AddComponent(player, controller));
        InputSystem input;
        const auto actions = InputActions::Defaults();
        input.SetKey(Key_D, KeyState::Down);
        input.SetKey(Key_LeftShift, KeyState::Down);
        PlayerControllerSystem::Update(scene, GameInput(input, actions), 0.25);
        EXPECT_FLOAT_EQ(scene.GetTransform(player)->x, 3.0f);
        EXPECT_FLOAT_EQ(scene.GetTransform(player)->z, 0.0f);
    }
    // Turned 90 degrees, W moves along -X instead of -Z. Pressing W and D together is not faster than pressing one.
    TEST(EcsPlayerControllerSystem, MovesTheWayThePlayerFaces)
    {
        Scene scene;
        const auto player = scene.CreateEntity();
        ASSERT_TRUE(scene.AddComponent(player, PlayerControllerComponent{.speed = 2.0f}));
        TransformComponent turned;
        turned.rotation = 90.0f; // facing -X
        ASSERT_TRUE(scene.SetTransform(player, turned));

        InputSystem input;
        const auto actions = InputActions::Defaults();
        input.SetKey(Key_W, KeyState::Down);
        PlayerControllerSystem::Update(scene, GameInput(input, actions), 1.0);
        EXPECT_NEAR(scene.GetTransform(player)->x, -2.0f, 1e-5f);
        EXPECT_NEAR(scene.GetTransform(player)->z, 0.0f, 1e-5f);

        // Forward and right together: no faster than one direction
        input.SetKey(Key_D, KeyState::Down);
        PlayerControllerSystem::Update(scene, GameInput(input, actions), 1.0);
        const auto moved = *scene.GetTransform(player);
        EXPECT_NEAR(std::hypot(moved.x + 2.0f, moved.z), 2.0f, 1e-4f);
        EXPECT_LT(moved.z, 0.0f); // facing -X, its right is -Z
    }

    // Moving the mouse does nothing until right mouse (Look) is held; then it turns the player and tilts the camera, and
    // the tilt stops at the max pitch.
    TEST(EcsPlayerControllerSystem, MouseTurnsThePlayerOnlyWhileLookIsHeld)
    {
        Scene scene;
        const auto player = scene.CreateEntity();
        ASSERT_TRUE(
            scene.AddComponent(player, PlayerControllerComponent{.lookSensitivity = 0.5f, .cameraPitch = -10.0f}));
        InputSystem input;
        const auto actions = InputActions::Defaults();

        input.SetMousePosition(40.0f, 20.0f); // moved without Look: nothing turns
        PlayerControllerSystem::Update(scene, GameInput(input, actions), 0.1);
        EXPECT_FLOAT_EQ(scene.GetTransform(player)->rotation, 0.0f);
        input.Update();

        input.SetMouseButton(Mouse_Right, KeyState::Down); // Look
        input.SetMousePosition(60.0f, 30.0f);              // 20 right, 10 down
        PlayerControllerSystem::Update(scene, GameInput(input, actions), 0.1);
        EXPECT_FLOAT_EQ(scene.GetTransform(player)->rotation, -10.0f); // turned right
        EXPECT_FLOAT_EQ(scene.GetComponent<PlayerControllerComponent>(player)->cameraPitch, -15.0f);

        input.Update();
        input.SetMousePosition(60.0f, -10000.0f); // far up: the pitch stops at the limit
        PlayerControllerSystem::Update(scene, GameInput(input, actions), 0.1);
        EXPECT_FLOAT_EQ(scene.GetComponent<PlayerControllerComponent>(player)->cameraPitch,
                        PlayerControllerSystem::kMaxPitch);
    }

    // Only the primary camera gets moved behind the player (5 units back, facing the same way). With followCamera off
    // nothing follows.
    TEST(EcsPlayerControllerSystem, TheMainCameraStaysBehindTheFollowingPlayer)
    {
        Scene scene;
        const auto player = scene.CreateEntity();
        ASSERT_TRUE(scene.AddComponent(
            player, PlayerControllerComponent{.cameraOffsetY = 1.0f, .cameraOffsetZ = 5.0f, .cameraPitch = 0.0f}));
        TransformComponent at;
        at.x = 3.0f;
        at.rotation = 90.0f; // facing -X
        ASSERT_TRUE(scene.SetTransform(player, at));

        const auto other = scene.CreateEntity();
        ASSERT_TRUE(scene.AddComponent(other, CameraComponent{.primary = false}));
        const auto main = scene.CreateEntity();
        ASSERT_TRUE(scene.AddComponent(main, CameraComponent{.primary = true}));

        ASSERT_TRUE(PlayerControllerSystem::MoveMainCameraToPlayer(scene));
        const auto camera = *scene.GetTransform(main);
        EXPECT_NEAR(camera.x, 8.0f, 1e-4f); // 5 behind, on +X
        EXPECT_NEAR(camera.y, 1.0f, 1e-4f);
        EXPECT_NEAR(camera.z, 0.0f, 1e-4f);
        EXPECT_FLOAT_EQ(camera.rotation, -90.0f);                    // the camera Transform yaw that looks along -X
        EXPECT_EQ(*scene.GetTransform(other), TransformComponent{}); // only the Main camera moves

        // Without followCamera nothing follows
        ASSERT_TRUE(scene.SetComponent(player, PlayerControllerComponent{.followCamera = false}));
        EXPECT_FALSE(PlayerControllerSystem::MoveMainCameraToPlayer(scene));
    }

    // A wall between the player and where its camera would be: the camera comes in to just in front of the wall instead
    // of going through it, and goes back out once the wall is gone
    TEST(EcsPlayerControllerSystem, TheCameraDoesNotGoThroughWhatIsBehindThePlayer)
    {
        Engine::Physics::PhysicsWorld world;
        ASSERT_TRUE(world.Init());
        Scene scene;
        const auto player = scene.CreateEntity();
        ASSERT_TRUE(scene.AddComponent(
            player, PlayerControllerComponent{.cameraOffsetY = 1.0f, .cameraOffsetZ = 5.0f, .cameraPitch = 0.0f}));
        const auto main = scene.CreateEntity();
        ASSERT_TRUE(scene.AddComponent(main, CameraComponent{.primary = true}));

        // Facing -Z, so the camera goes 5 back on +Z; a wall 2.5 back, 1 thick, from z = 2 to 3
        Engine::Physics::Collider wall;
        wall.shape = Engine::Physics::ShapeType::Box;
        wall.halfExtents = glm::vec3(5.0f, 5.0f, 0.5f);
        const auto body = world.AddBody(
            {.motion = Engine::Physics::MotionType::Static, .position = glm::vec3(0.0f, 0.0f, 2.5f), .collider = wall});
        ASSERT_TRUE(body.IsValid());
        world.Update(1.0 / 60.0);

        ASSERT_TRUE(PlayerControllerSystem::MoveMainCameraToPlayer(scene, &world));
        EXPECT_NEAR(scene.GetTransform(main)->z, 2.0f - PlayerControllerSystem::kCameraClearance, 1e-3f);

        world.RemoveBody(body);
        world.Update(1.0 / 60.0);
        ASSERT_TRUE(PlayerControllerSystem::MoveMainCameraToPlayer(scene, &world));
        EXPECT_NEAR(scene.GetTransform(main)->z, 5.0f, 1e-3f);
        world.Shutdown();
    }

    // No camera entity at all: FollowWithCamera puts the render camera behind and above the player, looking down at it.
    TEST(EcsPlayerControllerSystem, WithoutAMainCameraTheGameViewFollowsThePlayer)
    {
        Scene scene;
        const auto player = scene.CreateEntity();
        ASSERT_TRUE(
            scene.AddComponent(player, PlayerControllerComponent{.cameraOffsetZ = 4.0f, .cameraPitch = -30.0f}));
        EXPECT_FALSE(PlayerControllerSystem::MoveMainCameraToPlayer(scene));

        Engine::Graphics::Camera3D camera;
        ASSERT_TRUE(PlayerControllerSystem::FollowWithCamera(scene, camera));
        EXPECT_GT(camera.GetPosition().z, 0.0f); // behind a player facing -Z
        EXPECT_GT(camera.GetPosition().y, 0.0f); // above it, looking down
        EXPECT_LT(camera.GetForward().y, 0.0f);
        EXPECT_LT(camera.GetForward().z, 0.0f);
    }
} // namespace
