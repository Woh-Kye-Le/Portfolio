/* !
@file    	    ForceTests.cpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		29/10/2026
@brief
    Tests for forces and impulses (ATE3D-374): a kick changes a body's speed
    by impulse / mass and spins it when off its centre, a steady force
    accelerates it by force / mass at any frame rate (and can hold it up
    against gravity), a set velocity is kept, only dynamic bodies are pushed,
    sleeping bodies wake, and pushes work with friction, contacts and
    momentum the way they should.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/physics/PhysicsWorld.hpp"
#include "engine/scene/Scene.hpp"
#include "engine/scene/systems/PhysicsSystem.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <utility>
#include <vector>

using Engine::Physics::BodyId;
using Engine::Physics::ContactEvent;
using Engine::Physics::Material;
using Engine::Physics::MotionType;
using Engine::Physics::PhysicsWorld;
using Engine::Physics::ShapeType;

namespace
{
    constexpr float kEarth = 9.81f;
    constexpr double kStep = PhysicsWorld::FixedStep;
    constexpr float kTolerance = 1e-4f;

    void Steps(PhysicsWorld& world, int steps)
    {
        for (int i = 0; i < steps; ++i)
        {
            world.Update(kStep);
        }
    }

    // A world with no gravity, so only the pushes being tested move things
    struct Space
    {
        PhysicsWorld world;
        Space()
        {
            world.SetGravity({0.0f, 0.0f, 0.0f});
            EXPECT_TRUE(world.Init());
        }
    };

    // A wide static floor whose top is at y = 0
    BodyId AddFloor(PhysicsWorld& world, const Material& material = {})
    {
        return world.AddBody({.motion = MotionType::Static,
                              .position = {0.0f, -0.5f, 0.0f},
                              .collider = {.halfExtents = {50.0f, 0.5f, 50.0f}},
                              .material = material});
    }

    // How far a body pushed from still by an acceleration goes in n steps: the same step formula gravity follows
    // (see GravityTests.cpp)
    double DistanceAfter(int steps, double acceleration)
    {
        return acceleration * kStep * kStep * steps * (steps + 1) / 2.0;
    }
}

// ---- Impulses

// A kick changes the speed by impulse / mass straight away: the same kick sends a body four times as heavy off at a
// quarter of the speed.
TEST(Forces, AnImpulseChangesSpeedByImpulseOverMass)
{
    Space space;
    const BodyId light = space.world.AddBody({.mass = 1.0f});
    const BodyId heavy = space.world.AddBody({.position = {0.0f, 5.0f, 0.0f}, .mass = 4.0f});

    space.world.AddImpulse(light, {8.0f, 0.0f, 0.0f});
    space.world.AddImpulse(heavy, {8.0f, 0.0f, 0.0f});

    EXPECT_NEAR(space.world.Velocity(light)->x, 8.0f, kTolerance);
    EXPECT_NEAR(space.world.Velocity(heavy)->x, 2.0f, kTolerance);
}

// Kicks add up, in any direction: two opposite kicks cancel, two the same way double the speed.
TEST(Forces, ImpulsesAddUp)
{
    Space space;
    const BodyId body = space.world.AddBody({.mass = 2.0f});

    space.world.AddImpulse(body, {0.0f, 4.0f, 0.0f});
    space.world.AddImpulse(body, {0.0f, 4.0f, 0.0f});
    space.world.AddImpulse(body, {6.0f, 0.0f, -2.0f});
    space.world.AddImpulse(body, {-6.0f, 0.0f, 0.0f});

    const glm::vec3 velocity = *space.world.Velocity(body);
    EXPECT_NEAR(velocity.x, 0.0f, kTolerance);
    EXPECT_NEAR(velocity.y, 4.0f, kTolerance);
    EXPECT_NEAR(velocity.z, -1.0f, kTolerance);
}

// A kick through the centre only moves a body; the same kick at its edge also spins it, by (point - centre) x impulse
// / inertia. A 1 x 1 x 1 box of mass 1 has inertia 1/6 around each axis.
TEST(Forces, AnImpulseOffTheCentreSpinsTheBody)
{
    Space space;
    const BodyId straight = space.world.AddBody({});
    const BodyId spun = space.world.AddBody({.position = {5.0f, 0.0f, 0.0f}});

    space.world.AddImpulse(straight, {0.0f, 0.0f, 1.0f});
    space.world.AddImpulseAt(spun, {0.0f, 0.0f, 1.0f}, {5.5f, 0.0f, 0.0f}); // its +X face

    EXPECT_NEAR(glm::length(*space.world.AngularVelocity(straight)), 0.0f, kTolerance);
    const glm::vec3 spin = *space.world.AngularVelocity(spun);
    EXPECT_NEAR(spin.y, -0.5f * 6.0f, 1e-3f); // (0.5, 0, 0) x (0, 0, 1) = (0, -0.5, 0)
    EXPECT_NEAR(spin.x, 0.0f, kTolerance);
    EXPECT_NEAR(spin.z, 0.0f, kTolerance);
    EXPECT_NEAR(space.world.Velocity(spun)->z, 1.0f, kTolerance) << "it still moves as much";
}

// ---- Steady forces

// A steady force accelerates a body by force / mass, so it goes as far as the step formula says, at any frame rate.
TEST(Forces, AConstantForceAcceleratesByForceOverMass)
{
    for (const int fps : {30, 60, 144})
    {
        Space space;
        const BodyId body = space.world.AddBody({.mass = 2.0f});
        space.world.SetConstantForce(body, {6.0f, 0.0f, 0.0f}); // 3 units per second squared

        for (int frame = 0; frame < fps; ++frame) // one second
        {
            space.world.Update(1.0 / fps);
        }

        EXPECT_NEAR(space.world.Pose(body)->position.x, DistanceAfter(60, 3.0), 1e-3) << fps << " FPS";
        EXPECT_NEAR(space.world.Velocity(body)->x, 3.0f, 1e-3f) << fps << " FPS";
    }
}

// A force equal to the body's weight, pointing up, holds it still in the air; half of it lets it fall at half
// gravity.
TEST(Forces, AForceCanHoldABodyUpAgainstGravity)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const BodyId held = world.AddBody({.position = {0.0f, 10.0f, 0.0f}, .mass = 2.0f});
    const BodyId slowed = world.AddBody({.position = {3.0f, 10.0f, 0.0f}, .mass = 2.0f});
    world.SetConstantForce(held, {0.0f, 2.0f * kEarth, 0.0f});
    world.SetConstantForce(slowed, {0.0f, kEarth, 0.0f});

    Steps(world, 120);

    EXPECT_NEAR(world.Pose(held)->position.y, 10.0f, 1e-3f);
    EXPECT_NEAR(10.0f - world.Pose(slowed)->position.y, DistanceAfter(120, kEarth / 2.0), 1e-3);
}

// Setting the force to zero stops the push; the body keeps the speed it had. The force is forgotten with its body.
TEST(Forces, AConstantForceStopsWhenSetToZero)
{
    Space space;
    const BodyId body = space.world.AddBody({});
    space.world.SetConstantForce(body, {0.0f, 0.0f, 2.0f});
    Steps(space.world, 60);
    const float speed = space.world.Velocity(body)->z;

    space.world.SetConstantForce(body, glm::vec3(0.0f));
    EXPECT_EQ(space.world.ConstantForceOf(body), glm::vec3(0.0f));
    Steps(space.world, 60);
    EXPECT_NEAR(space.world.Velocity(body)->z, speed, kTolerance);

    space.world.SetConstantForce(body, {0.0f, 0.0f, 2.0f});
    space.world.RemoveBody(body);
    EXPECT_EQ(space.world.ConstantForceOf(body), glm::vec3(0.0f));
}

// ---- Velocities

// A body given a velocity keeps it (no gravity, nothing touching, no air drag): one second at 2 units per second is
// 2 units. A spin slows down slowly.
TEST(Forces, ASetVelocityIsKept)
{
    Space space;
    const BodyId body = space.world.AddBody({});
    space.world.SetVelocity(body, {2.0f, 0.0f, -1.0f});
    space.world.SetAngularVelocity(body, {0.0f, 1.5f, 0.0f});

    Steps(space.world, 60);

    const glm::vec3 position = space.world.Pose(body)->position;
    EXPECT_NEAR(position.x, 2.0f, 1e-3f);
    EXPECT_NEAR(position.z, -1.0f, 1e-3f);
    EXPECT_NEAR(space.world.Velocity(body)->x, 2.0f, kTolerance);
    // Spinning slows by 5% a second (Jolt's angular damping, kept so things do not spin forever)
    EXPECT_NEAR(space.world.AngularVelocity(body)->y, 1.5f * std::exp(-0.05f), 5e-3f);
}

// ---- Which bodies are pushed

// Static and kinematic bodies are not pushed by kicks, forces or velocities: they stay exactly where they are.
TEST(Forces, OnlyDynamicBodiesArePushed)
{
    for (const MotionType motion : {MotionType::Static, MotionType::Kinematic})
    {
        Space space;
        const BodyId body = space.world.AddBody({.motion = motion});
        space.world.AddImpulse(body, {5.0f, 0.0f, 0.0f});
        space.world.AddImpulseAt(body, {5.0f, 0.0f, 0.0f}, {0.0f, 0.5f, 0.0f});
        space.world.SetConstantForce(body, {5.0f, 0.0f, 0.0f});
        space.world.SetVelocity(body, {5.0f, 0.0f, 0.0f});
        space.world.SetAngularVelocity(body, {5.0f, 0.0f, 0.0f});

        Steps(space.world, 60);

        EXPECT_EQ(space.world.Pose(body)->position, glm::vec3(0.0f));
        EXPECT_EQ(*space.world.Velocity(body), glm::vec3(0.0f));
        EXPECT_EQ(space.world.ConstantForceOf(body), glm::vec3(0.0f));
    }
}

// A body asleep on the floor is woken by a kick, and by a steady force, and moves.
TEST(Forces, PushingWakesASleepingBody)
{
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    AddFloor(world, {.friction = 0.0f});
    const BodyId kicked = world.AddBody({.position = {0.0f, 0.5f, 0.0f}});
    const BodyId pushed = world.AddBody({.position = {3.0f, 0.5f, 0.0f}});
    Steps(world, 120);
    ASSERT_FALSE(world.IsAwake(kicked));
    ASSERT_FALSE(world.IsAwake(pushed));

    world.AddImpulse(kicked, {0.0f, 0.0f, 1.0f});
    world.SetConstantForce(pushed, {0.0f, 0.0f, 1.0f});
    EXPECT_TRUE(world.IsAwake(kicked));
    EXPECT_TRUE(world.IsAwake(pushed));

    Steps(world, 60);
    EXPECT_GT(world.Pose(kicked)->position.z, 0.5f);
    EXPECT_GT(world.Pose(pushed)->position.z, 0.2f);
}

// ---- With the rest of the physics

// A box kicked along the floor slides until friction stops it, at v^2 / (2 friction g): about 0.4 units on a normal
// floor and 10 on ice.
TEST(Forces, FrictionStopsAKickedBox)
{
    for (const auto& [friction, expected] : {std::pair{0.5f, 0.41f}, std::pair{0.02f, 10.2f}})
    {
        PhysicsWorld world;
        ASSERT_TRUE(world.Init());
        AddFloor(world, {.friction = friction});
        const BodyId box = world.AddBody({.position = {0.0f, 0.5f, 0.0f}, .material = {.friction = friction}});
        Steps(world, 30); // settled on the floor

        world.AddImpulse(box, {2.0f, 0.0f, 0.0f}); // 2 units per second
        Steps(world, 12 * 60);                     // ice takes about 10 seconds to stop it

        EXPECT_NEAR(world.Pose(box)->position.x, expected, 0.05f * expected + 0.03f) << "friction " << friction;
        EXPECT_NEAR(world.Velocity(box)->x, 0.0f, 1e-3f) << "stopped";
    }
}

// A body kicked into a wall hits it, and the hit is reported at the speed it was kicked to.
TEST(Forces, AKickedBodyHitsAWall)
{
    Space space;
    const BodyId ball = space.world.AddBody({.collider = {.shape = ShapeType::Sphere, .radius = 0.5f}});
    const BodyId wall = space.world.AddBody({.motion = MotionType::Static, .position = {3.0f, 0.0f, 0.0f}});

    space.world.AddImpulse(ball, {4.0f, 0.0f, 0.0f});
    Steps(space.world, 60);

    const std::vector<ContactEvent> events = space.world.TakeContactEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].a, ball);
    EXPECT_EQ(events[0].b, wall);
    EXPECT_NEAR(events[0].impactSpeed, 4.0f, 0.05f);
}

// Momentum is kept when bodies hit: a ball rolling into an equal one with full bounciness stops dead and sends the
// other off at its speed (a Newton's cradle); one that sticks (no bounciness) to a body three times as heavy carries
// it along at a quarter of the speed.
TEST(Forces, MomentumIsKeptWhenBodiesHit)
{
    {
        Space space;
        const Material bouncy{.bounciness = 1.0f};
        const BodyId moving = space.world.AddBody({.position = {-3.0f, 0.0f, 0.0f},
                                                   .collider = {.shape = ShapeType::Sphere, .radius = 0.5f},
                                                   .material = bouncy});
        const BodyId still =
            space.world.AddBody({.collider = {.shape = ShapeType::Sphere, .radius = 0.5f}, .material = bouncy});
        space.world.SetVelocity(moving, {2.0f, 0.0f, 0.0f});

        Steps(space.world, 3 * 60);

        EXPECT_NEAR(space.world.Velocity(moving)->x, 0.0f, 0.02f);
        EXPECT_NEAR(space.world.Velocity(still)->x, 2.0f, 0.02f);
    }
    {
        Space space;
        const BodyId light = space.world.AddBody(
            {.position = {-3.0f, 0.0f, 0.0f}, .collider = {.shape = ShapeType::Sphere, .radius = 0.5f}, .mass = 1.0f});
        const BodyId heavy =
            space.world.AddBody({.collider = {.shape = ShapeType::Sphere, .radius = 0.5f}, .mass = 3.0f});
        space.world.SetVelocity(light, {2.0f, 0.0f, 0.0f});

        Steps(space.world, 3 * 60);

        EXPECT_NEAR(space.world.Velocity(light)->x, 0.5f, 0.02f);
        EXPECT_NEAR(space.world.Velocity(heavy)->x, 0.5f, 0.02f);
    }
}

// Entities are pushed the same way through their bodies: a kick, then a steady force against it, through the
// PhysicsSystem.
TEST(Forces, EntitiesArePushedThroughThePhysicsSystem)
{
    using Engine::Scene::Entity;
    using Engine::Scene::RigidBodyComponent;
    using Engine::Scene::TransformComponent;
    using Engine::Systems::PhysicsSystem;

    Engine::Scene::Scene scene;
    Space space;
    const Entity box = scene.CreateEntity();
    scene.AddComponent(box, RigidBodyComponent{.mass = 2.0f});
    PhysicsSystem::Update(scene, space.world, 0.0); // makes its body

    ASSERT_TRUE(PhysicsSystem::AddImpulse(scene, space.world, box, {4.0f, 0.0f, 0.0f}));
    EXPECT_NEAR(PhysicsSystem::VelocityOf(scene, space.world, box)->x, 2.0f, kTolerance);

    // A force of -2 slows it by 1 unit per second squared: stopped after 2 seconds
    ASSERT_TRUE(PhysicsSystem::SetConstantForce(scene, space.world, box, {-2.0f, 0.0f, 0.0f}));
    for (int frame = 0; frame < 2 * 60; ++frame)
    {
        PhysicsSystem::Update(scene, space.world, kStep);
    }
    EXPECT_NEAR(PhysicsSystem::VelocityOf(scene, space.world, box)->x, 0.0f, 1e-3f);
    EXPECT_NEAR(scene.GetTransform(box)->x, 2.0f, 0.05f) << "the entity moved with its body";

    ASSERT_TRUE(PhysicsSystem::SetVelocity(scene, space.world, box, {0.0f, 1.0f, 0.0f}));
    EXPECT_NEAR(PhysicsSystem::VelocityOf(scene, space.world, box)->y, 1.0f, kTolerance);
}

// ---- Start velocity (the RigidBody's, set in the Inspector)

// A dynamic entity with a start velocity is already moving at it when Play starts: thrown up at an angle, it goes as
// far as that speed takes it. Static and kinematic entities ignore it.
TEST(Forces, AStartVelocityLaunchesADynamicEntity)
{
    using Engine::Scene::BodyType;
    using Engine::Scene::Entity;
    using Engine::Scene::RigidBodyComponent;
    using Engine::Scene::TransformComponent;
    using Engine::Systems::PhysicsSystem;

    Engine::Scene::Scene scene;
    Space space;
    const auto make = [&scene](BodyType type, float x)
    {
        const Entity entity = scene.CreateEntity();
        scene.SetTransform(entity, TransformComponent{.x = x}); // apart, so they do not push each other
        scene.AddComponent(
            entity,
            RigidBodyComponent{.type = type, .startVelocityX = 3.0f, .startVelocityY = 4.0f, .startVelocityZ = 0.0f});
        return entity;
    };
    const Entity thrown = make(BodyType::Dynamic, 0.0f);
    const Entity fixed = make(BodyType::Static, 10.0f);
    const Entity platform = make(BodyType::Kinematic, 20.0f);

    PhysicsSystem::Update(scene, space.world, 0.0); // Play starts: the bodies are made
    EXPECT_EQ(*PhysicsSystem::VelocityOf(scene, space.world, thrown), glm::vec3(3.0f, 4.0f, 0.0f));

    for (int frame = 0; frame < 60; ++frame)
    {
        PhysicsSystem::Update(scene, space.world, kStep);
    }
    EXPECT_NEAR(scene.GetTransform(thrown)->x, 3.0f, 1e-3f);
    EXPECT_NEAR(scene.GetTransform(thrown)->y, 4.0f, 1e-3f);
    EXPECT_EQ(scene.GetTransform(fixed)->x, 10.0f) << "a start velocity only throws dynamic bodies";
    EXPECT_EQ(scene.GetTransform(platform)->x, 20.0f);
}

// Changing the start velocity while playing does not throw the entity again, and changing something that makes its
// body again (here its mass) keeps it moving as it was.
TEST(Forces, ChangesWhilePlayingKeepTheEntityMoving)
{
    using Engine::Scene::Entity;
    using Engine::Scene::PhysicsBodyComponent;
    using Engine::Scene::RigidBodyComponent;
    using Engine::Systems::PhysicsSystem;

    Engine::Scene::Scene scene;
    Space space;
    const Entity ball = scene.CreateEntity();
    scene.AddComponent(ball, RigidBodyComponent{.startVelocityX = 2.0f});
    for (int frame = 0; frame < 10; ++frame)
    {
        PhysicsSystem::Update(scene, space.world, kStep);
    }
    const auto first = scene.GetComponent<PhysicsBodyComponent>(ball)->body;

    RigidBodyComponent changed = *scene.GetComponent<RigidBodyComponent>(ball);
    changed.startVelocityX = 0.0f;
    changed.startVelocityY = 10.0f;
    scene.SetComponent(ball, changed);
    PhysicsSystem::Update(scene, space.world, kStep);
    EXPECT_EQ(scene.GetComponent<PhysicsBodyComponent>(ball)->body, first) << "the same body";
    EXPECT_EQ(*PhysicsSystem::VelocityOf(scene, space.world, ball), glm::vec3(2.0f, 0.0f, 0.0f)) << "not thrown again";

    changed.mass = 5.0f;
    scene.SetComponent(ball, changed);
    PhysicsSystem::Update(scene, space.world, kStep);
    EXPECT_FALSE(scene.GetComponent<PhysicsBodyComponent>(ball)->body == first) << "made again";
    EXPECT_EQ(*PhysicsSystem::VelocityOf(scene, space.world, ball), glm::vec3(2.0f, 0.0f, 0.0f)) << "still moving";
}
