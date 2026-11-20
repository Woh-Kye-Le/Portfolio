/* !
@file    	    CollisionResponseTests.cpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		28/10/2026
@brief
    Tests for collision response (ATE3D-373): friction holds a body or lets it
    slide, bounciness bounces it back by the right amount, the slipperier and
    the bouncier of two surfaces count, static and kinematic bodies are not
    pushed by what hits them, and a RigidBody's friction and bounciness reach
    its body (changed in Play mode without making the body again).

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/physics/PhysicsWorld.hpp"
#include "engine/scene/Scene.hpp"
#include "engine/scene/systems/PhysicsSystem.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

using Engine::Physics::BodyId;
using Engine::Physics::Material;
using Engine::Physics::MotionType;
using Engine::Physics::PhysicsWorld;
using Engine::Physics::ShapeType;

namespace
{
    constexpr float kEarth = 9.81f;
    constexpr Material kIce{.friction = 0.02f};
    constexpr Material kRubber{.friction = 1.0f};

    // A wide static floor whose top is at y = 0
    BodyId AddFloor(PhysicsWorld& world, const Material& material = {})
    {
        return world.AddBody({.motion = MotionType::Static,
                              .position = {0.0f, -0.5f, 0.0f},
                              .collider = {.halfExtents = {50.0f, 0.5f, 50.0f}},
                              .material = material});
    }

    void RunSeconds(PhysicsWorld& world, double seconds)
    {
        for (double time = 0.0; time < seconds; time += PhysicsWorld::FixedStep)
        {
            world.Update(PhysicsWorld::FixedStep);
        }
    }

    // A unit box resting on the floor, pushed along X by gravity at 3 units per second squared (as on a slope of
    // about 17 degrees): friction above 3 / 9.81 = 0.31 holds it, less lets it slide
    struct Slope
    {
        PhysicsWorld world;
        BodyId box;

        Slope(const Material& floor, const Material& block)
        {
            world.SetGravity({3.0f, -kEarth, 0.0f});
            EXPECT_TRUE(world.Init());
            AddFloor(world, floor);
            box = world.AddBody({.position = {0.0f, 0.5f, 0.0f}, .material = block});
        }

        float SlidAfter(double seconds)
        {
            RunSeconds(world, seconds);
            return world.Pose(box)->position.x;
        }
    };

    // How high a ball of radius 0.5 dropped from 2.5 above the floor goes back up after its first bounce
    float BounceHeight(const Material& floor, const Material& ball)
    {
        PhysicsWorld world;
        EXPECT_TRUE(world.Init());
        AddFloor(world, floor);
        const BodyId body = world.AddBody({.position = {0.0f, 3.0f, 0.0f},
                                           .collider = {.shape = ShapeType::Sphere, .radius = 0.5f},
                                           .material = ball});

        float previous = 3.0f;
        float highest = 0.0f;
        bool rising = false;
        for (int step = 0; step < 4 * 60; ++step)
        {
            world.Update(PhysicsWorld::FixedStep);
            const float y = world.Pose(body)->position.y;
            rising = rising || y > previous;
            if (rising)
            {
                highest = std::max(highest, y);
            }
            previous = y;
        }
        return std::max(highest - 0.5f, 0.0f); // its bottom above the floor
    }
}

// Enough friction holds a box where it is; on ice it slides, as fast as the friction left over lets it.
TEST(CollisionResponse, FrictionHoldsOrLetsSlide)
{
    EXPECT_NEAR(Slope(kRubber, kRubber).SlidAfter(2.0), 0.0f, 0.01f);
    EXPECT_NEAR(Slope({.friction = 0.5f}, {.friction = 0.5f}).SlidAfter(2.0), 0.0f, 0.01f) << "the default holds";
    EXPECT_GT(Slope({.friction = 0.2f}, {.friction = 0.2f}).SlidAfter(2.0), 1.0f);

    // On ice: 3 - 0.02 * 9.81 = 2.8 units per second squared, about 5.6 units in 2 seconds
    EXPECT_NEAR(Slope(kIce, kIce).SlidAfter(2.0), 5.6f, 0.3f);
}

// Of two surfaces the slipperier one counts, whichever body it is on: an icy floor is slippery for a rubber box, and
// an icy box slides on a rubber floor.
TEST(CollisionResponse, TheSlipperierSurfaceCounts)
{
    EXPECT_GT(Slope(kIce, kRubber).SlidAfter(2.0), 5.0f);
    EXPECT_GT(Slope(kRubber, kIce).SlidAfter(2.0), 5.0f);
}

// Bounciness is the share of its speed a ball bounces back with, so it climbs back that share squared of the height:
// none at 0, all of it at 1.
TEST(CollisionResponse, BouncinessBouncesBack)
{
    constexpr float kDrop = 2.5f;
    EXPECT_LT(BounceHeight({}, {.bounciness = 0.0f}), 0.05f);
    EXPECT_NEAR(BounceHeight({}, {.bounciness = 0.5f}), 0.25f * kDrop, 0.15f);
    EXPECT_NEAR(BounceHeight({}, {.bounciness = 0.8f}), 0.64f * kDrop, 0.15f);
    EXPECT_NEAR(BounceHeight({}, {.bounciness = 1.0f}), kDrop, 0.15f);
}

// Of two surfaces the bouncier one counts: a dull ball bounces on a trampoline floor as a bouncy ball does on a dull
// one.
TEST(CollisionResponse, TheBouncierSurfaceCounts)
{
    const float trampoline = BounceHeight({.bounciness = 0.8f}, {});
    EXPECT_NEAR(trampoline, BounceHeight({}, {.bounciness = 0.8f}), 0.05f);
    EXPECT_GT(trampoline, 1.0f);
}

// A moving body that hits another moving body pushes it; hitting a static or kinematic body moves that body not at all.
TEST(CollisionResponse, OnlyMovingBodiesArePushed)
{
    for (const MotionType target : {MotionType::Dynamic, MotionType::Static, MotionType::Kinematic})
    {
        PhysicsWorld world;
        world.SetGravity({4.0f, 0.0f, 0.0f}); // pulls the ball along X into the box
        ASSERT_TRUE(world.Init());
        world.AddBody({.position = {-3.0f, 0.0f, 0.0f}, .collider = {.shape = ShapeType::Sphere, .radius = 0.5f}});
        const BodyId box =
            world.AddBody({.motion = target, .position = {0.0f, 0.0f, 0.0f}, .gravityScale = 0.0f, .material = kIce});

        RunSeconds(world, 2.0);

        const float moved = world.Pose(box)->position.x;
        if (target == MotionType::Dynamic)
        {
            EXPECT_GT(moved, 0.5f) << "pushed along";
        }
        else
        {
            EXPECT_EQ(moved, 0.0f) << (target == MotionType::Static ? "static" : "kinematic");
        }
    }
}

// A body is made with its material, which can be changed while the world runs: a box held by friction starts to
// slide as soon as it is made icy (it is woken if it sleeps). Values out of range are kept in range.
TEST(CollisionResponse, MaterialsCanBeChangedLive)
{
    PhysicsWorld world;
    world.SetGravity({3.0f, -kEarth, 0.0f});
    ASSERT_TRUE(world.Init());
    AddFloor(world, kRubber);
    const BodyId box = world.AddBody({.position = {0.0f, 0.5f, 0.0f}, .material = kRubber});
    EXPECT_EQ(world.MaterialOf(box), kRubber);

    RunSeconds(world, 2.0); // held, and asleep by now
    EXPECT_NEAR(world.Pose(box)->position.x, 0.0f, 0.01f);
    EXPECT_FALSE(world.IsAwake(box));

    world.SetMaterial(box, kIce);
    EXPECT_EQ(world.MaterialOf(box), kIce);
    RunSeconds(world, 1.0);
    EXPECT_GT(world.Pose(box)->position.x, 1.0f);

    world.SetMaterial(box, {.friction = -1.0f, .bounciness = 3.0f});
    EXPECT_EQ(world.MaterialOf(box), (Material{.friction = 0.0f, .bounciness = 1.0f}));
    EXPECT_FALSE(world.MaterialOf({}).has_value());
}

// A RigidBody's friction and bounciness reach its body, and changing them in Play mode changes the body itself (no
// new body), so a moving object keeps its speed.
TEST(CollisionResponse, RigidBodySurfacesReachTheirBodies)
{
    using Engine::Scene::Entity;
    using Engine::Scene::PhysicsBodyComponent;
    using Engine::Scene::RigidBodyComponent;
    using Engine::Scene::TransformComponent;
    using Engine::Systems::PhysicsSystem;

    Engine::Scene::Scene scene;
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const Entity ball = scene.CreateEntity();
    scene.SetTransform(ball, TransformComponent{.y = 100.0f});
    scene.AddComponent(ball, RigidBodyComponent{.friction = 0.3f, .bounciness = 0.6f});

    for (int frame = 0; frame < 30; ++frame)
    {
        PhysicsSystem::Update(scene, world, PhysicsWorld::FixedStep);
    }
    const BodyId body = scene.GetComponent<PhysicsBodyComponent>(ball)->body;
    const auto made = world.MaterialOf(body);
    ASSERT_TRUE(made.has_value());
    EXPECT_FLOAT_EQ(made->friction, 0.3f);
    EXPECT_FLOAT_EQ(made->bounciness, 0.6f);

    // Falling for a while: then made bouncy and slippery
    const float before = scene.GetTransform(ball)->y;
    RigidBodyComponent changed = *scene.GetComponent<RigidBodyComponent>(ball);
    changed.friction = 0.0f;
    changed.bounciness = 1.0f;
    scene.SetComponent(ball, changed);
    PhysicsSystem::Update(scene, world, PhysicsWorld::FixedStep);

    EXPECT_EQ(scene.GetComponent<PhysicsBodyComponent>(ball)->body, body) << "the same body";
    EXPECT_EQ(world.MaterialOf(body), (Material{.friction = 0.0f, .bounciness = 1.0f}));
    // Still falling at the speed 30 steps of gravity gave it (a new body would have started from rest)
    EXPECT_GT(before - scene.GetTransform(ball)->y, kEarth * 30.0f * PhysicsWorld::FixedStep * PhysicsWorld::FixedStep);
}
