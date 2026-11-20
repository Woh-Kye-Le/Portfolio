/* !
@file    	    GravityTests.cpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		28/10/2026
@brief
    Tests for gravity (ATE3D-371): a falling body follows the gravity formula
    step by step at any frame rate, whatever its mass or shape; only dynamic
    bodies fall; a body's Gravity Scale and the game's gravity (any strength,
    any direction, changed at any time) are applied; and entities fall through
    the ECS PhysicsSystem the same way.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/physics/PhysicsWorld.hpp"
#include "engine/scene/Scene.hpp"
#include "engine/scene/systems/PhysicsSystem.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

using Engine::Physics::BodyId;
using Engine::Physics::MotionType;
using Engine::Physics::PhysicsWorld;
using Engine::Physics::ShapeType;

namespace
{
    constexpr float kEarth = 9.81f;
    constexpr float kStart = 100.0f; // high enough that nothing lands
    constexpr double kStep = PhysicsWorld::FixedStep;

    // Jolt adds the step's gravity to the velocity, then moves by the new velocity. After n steps of length dt
    // a body that started still has gone g dt^2 n (n + 1) / 2: a little more than the textbook g t^2 / 2, by less
    // than 2% after a second, and the same at any frame rate.
    double DropAfter(int steps, double gravity)
    {
        return gravity * kStep * kStep * steps * (steps + 1) / 2.0;
    }

    void RunSteps(PhysicsWorld& world, int steps)
    {
        for (int i = 0; i < steps; ++i)
        {
            world.Update(kStep);
        }
    }

    float HeightOf(const PhysicsWorld& world, BodyId body)
    {
        return world.Pose(body)->position.y;
    }

    // Close to expected, allowing for float rounding relative to its size
    void ExpectClose(double actual, double expected)
    {
        EXPECT_NEAR(actual, expected, 1e-4 * std::max(1.0, std::abs(expected)));
    }
}

// A falling body goes the distance the step formula gives, which stays within 2% of g t^2 / 2.
TEST(Gravity, FallsByTheFormula)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId body = world.AddBody({.position = {0.0f, kStart, 0.0f}});

    int done = 0;
    for (const int steps : {30, 60, 120, 180})
    {
        RunSteps(world, steps - done);
        done = steps;
        const double drop = kStart - HeightOf(world, body);
        ExpectClose(drop, DropAfter(steps, kEarth));

        const double seconds = steps * kStep;
        const double textbook = 0.5 * kEarth * seconds * seconds;
        if (seconds >= 1.0)
        {
            EXPECT_NEAR(drop, textbook, 0.02 * textbook) << "after " << seconds << " s";
        }
    }
}

// The fall is the same at any frame rate: physics always steps by FixedStep.
TEST(Gravity, TheFrameRateDoesNotChangeTheFall)
{
    std::vector<float> heights;
    for (const int fps : {30, 45, 60, 144, 240})
    {
        PhysicsWorld world;
        ASSERT_TRUE(world.Init());
        const BodyId body = world.AddBody({.position = {0.0f, kStart, 0.0f}});
        for (int frame = 0; frame < fps; ++frame) // one second
        {
            world.Update(1.0 / fps);
        }
        EXPECT_EQ(world.StepCount(), 60u) << fps << " FPS";
        heights.push_back(HeightOf(world, body));
    }
    for (const float height : heights)
    {
        EXPECT_FLOAT_EQ(height, heights.front());
    }
    ExpectClose(kStart - heights.front(), DropAfter(60, kEarth));
}

// Heavy and light bodies fall together, as Galileo found.
TEST(Gravity, HeavyAndLightBodiesFallTogether)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId feather = world.AddBody({.position = {0.0f, kStart, 0.0f}, .mass = 0.05f});
    const BodyId ball = world.AddBody({.position = {3.0f, kStart, 0.0f}, .mass = 1.0f});
    const BodyId anvil = world.AddBody({.position = {6.0f, kStart, 0.0f}, .mass = 500.0f});

    RunSteps(world, 90);

    ExpectClose(kStart - HeightOf(world, feather), DropAfter(90, kEarth));
    EXPECT_FLOAT_EQ(HeightOf(world, ball), HeightOf(world, feather));
    EXPECT_FLOAT_EQ(HeightOf(world, anvil), HeightOf(world, feather));
}

// The collider's shape and where it sits on the body do not change how far the body falls.
TEST(Gravity, EveryShapeFallsTheSame)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const std::vector<BodyId> bodies = {
        world.AddBody({.position = {0.0f, kStart, 0.0f}, .collider = {.shape = ShapeType::Box}}),
        world.AddBody({.position = {3.0f, kStart, 0.0f}, .collider = {.shape = ShapeType::Sphere, .radius = 1.0f}}),
        world.AddBody({.position = {6.0f, kStart, 0.0f},
                       .collider = {.shape = ShapeType::Capsule, .radius = 0.5f, .halfHeight = 1.0f}}),
        world.AddBody(
            {.position = {9.0f, kStart, 0.0f}, .collider = {.shape = ShapeType::Box, .center = {0.0f, 0.5f, 0.0f}}}),
    };

    RunSteps(world, 60);

    for (const BodyId body : bodies)
    {
        ExpectClose(kStart - HeightOf(world, body), DropAfter(60, kEarth));
    }
}

// Static and kinematic bodies ignore gravity: they stay exactly where they were put.
TEST(Gravity, OnlyDynamicBodiesFall)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId fixed = world.AddBody({.motion = MotionType::Static, .position = {0.0f, kStart, 0.0f}});
    const BodyId platform = world.AddBody({.motion = MotionType::Kinematic, .position = {3.0f, kStart, 0.0f}});
    const BodyId falling = world.AddBody({.motion = MotionType::Dynamic, .position = {6.0f, kStart, 0.0f}});

    RunSteps(world, 60);

    EXPECT_EQ(HeightOf(world, fixed), kStart);
    EXPECT_EQ(HeightOf(world, platform), kStart);
    EXPECT_LT(HeightOf(world, falling), kStart - 4.0f);
}

// A body's Gravity Scale multiplies the gravity it falls with: 0 floats, 0.5 falls half as far, 2 twice as far, and
// below 0 rises.
TEST(Gravity, GravityScaleScalesTheFall)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const std::vector<float> scales = {0.0f, 0.5f, 1.0f, 2.0f, -1.0f};
    std::vector<BodyId> bodies;
    for (std::size_t i = 0; i < scales.size(); ++i)
    {
        bodies.push_back(
            world.AddBody({.position = {3.0f * static_cast<float>(i), kStart, 0.0f}, .gravityScale = scales[i]}));
    }

    RunSteps(world, 60);

    for (std::size_t i = 0; i < scales.size(); ++i)
    {
        ExpectClose(kStart - HeightOf(world, bodies[i]), DropAfter(60, kEarth * scales[i]));
    }
}

// The game's gravity can be any strength and point any way; bodies fall along it by the same formula.
TEST(Gravity, GravityCanBeAnyStrengthAndDirection)
{
    struct Case
    {
        glm::vec3 gravity;
        const char* name;
    };
    for (const Case& test : {Case{{0.0f, -1.62f, 0.0f}, "the Moon"}, Case{{0.0f, -24.79f, 0.0f}, "Jupiter"},
                             Case{{5.0f, 0.0f, 0.0f}, "sideways"}, Case{{0.0f, 3.0f, -4.0f}, "up and back"}})
    {
        PhysicsWorld world;
        world.SetGravity(test.gravity);
        ASSERT_TRUE(world.Init());
        const glm::vec3 start(0.0f, kStart, 0.0f);
        const BodyId body = world.AddBody({.position = start});

        RunSteps(world, 60);

        const glm::vec3 moved = world.Pose(body)->position - start;
        for (int axis = 0; axis < 3; ++axis)
        {
            EXPECT_NEAR(moved[axis], DropAfter(60, test.gravity[axis]), 1e-3) << test.name << ", axis " << axis;
        }
    }
}

// With no gravity a body left alone does not move or turn at all.
TEST(Gravity, NoGravityNothingDrifts)
{
    PhysicsWorld world;
    world.SetGravity({0.0f, 0.0f, 0.0f});
    ASSERT_TRUE(world.Init());
    const BodyId body = world.AddBody({.position = {1.0f, 2.0f, 3.0f}});

    RunSteps(world, 300);

    const auto pose = world.Pose(body);
    EXPECT_EQ(pose->position, glm::vec3(1.0f, 2.0f, 3.0f));
    EXPECT_FLOAT_EQ(std::abs(pose->rotation.w), 1.0f);
}

// Gravity changed in the middle of a fall applies from the next step: switched off, the body keeps the speed it had
// (nothing slows it) and goes the same distance every step.
TEST(Gravity, ChangingGravityTakesEffectNextStep)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId body = world.AddBody({.position = {0.0f, kStart, 0.0f}});
    RunSteps(world, 30);

    world.SetGravity({0.0f, 0.0f, 0.0f});
    const float before = HeightOf(world, body);
    RunSteps(world, 1);
    const float perStep = before - HeightOf(world, body);
    EXPECT_NEAR(perStep, kEarth * 30 * kStep * kStep, 1e-4f) << "the speed 30 steps of gravity gave it";

    const float after = HeightOf(world, body);
    RunSteps(world, 30);
    EXPECT_NEAR(after - HeightOf(world, body), 30.0f * perStep, 1e-3f);
}

// Gravity set on a world is kept when it is stopped and started again (Play pressed a second time).
TEST(Gravity, GravityIsKeptAcrossARestart)
{
    PhysicsWorld world;
    world.SetGravity({0.0f, -1.62f, 0.0f});
    ASSERT_TRUE(world.Init());
    world.Shutdown();
    ASSERT_TRUE(world.Init());
    EXPECT_FLOAT_EQ(world.Gravity().y, -1.62f);

    const BodyId body = world.AddBody({.position = {0.0f, kStart, 0.0f}});
    RunSteps(world, 60);
    ExpectClose(kStart - HeightOf(world, body), DropAfter(60, 1.62));
}

// Entities fall through the PhysicsSystem by the same formula, with the world's gravity and each RigidBody's Gravity
// Scale; static and kinematic entities stay put.
TEST(Gravity, EntitiesFallWithTheGamesGravity)
{
    using Engine::Scene::BodyType;
    using Engine::Scene::Entity;
    using Engine::Scene::RigidBodyComponent;
    using Engine::Scene::TransformComponent;

    Engine::Scene::Scene scene;
    PhysicsWorld world;
    world.SetGravity({0.0f, -1.62f, 0.0f}); // the game's configs/game.cfg says the Moon
    ASSERT_TRUE(world.Init());

    const auto make = [&scene](float x, RigidBodyComponent body)
    {
        const Entity entity = scene.CreateEntity();
        scene.SetTransform(entity, TransformComponent{.x = x, .y = kStart});
        scene.AddComponent(entity, body);
        return entity;
    };
    const Entity normal = make(0.0f, {.type = BodyType::Dynamic});
    const Entity doubled = make(3.0f, {.type = BodyType::Dynamic, .gravityScale = 2.0f});
    const Entity fixed = make(6.0f, {.type = BodyType::Static});
    const Entity platform = make(9.0f, {.type = BodyType::Kinematic});

    // The first frame makes the bodies; the next 60 are a second of falling
    for (int frame = 0; frame <= 60; ++frame)
    {
        Engine::Systems::PhysicsSystem::Update(scene, world, frame == 0 ? 0.0 : kStep);
    }

    ExpectClose(kStart - scene.GetTransform(normal)->y, DropAfter(60, 1.62));
    ExpectClose(kStart - scene.GetTransform(doubled)->y, DropAfter(60, 2.0 * 1.62));
    EXPECT_EQ(scene.GetTransform(fixed)->y, kStart);
    EXPECT_EQ(scene.GetTransform(platform)->y, kStart);
}
