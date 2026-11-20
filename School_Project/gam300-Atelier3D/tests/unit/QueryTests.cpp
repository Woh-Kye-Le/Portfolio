/* !
@file    	    QueryTests.cpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		30/10/2026
@brief
    Tests for the physics queries (ATE3D-381): ray casts (ATE3D-375) hit every
    shape on its surface, the nearest body first, bodies of every motion type
    and asleep, and follow entities that move; distance checks (ATE3D-376)
    are symmetric, agree with their own points and with ray casts, shrink as
    bodies close in, and find bodies near a point nearest first; on-ground
    detection (ATE3D-377) finds what a body stands on in stacks, corners, on
    slopes and under sideways gravity, and changes once when a body lands or
    jumps.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/physics/PhysicsWorld.hpp"
#include "engine/scene/Scene.hpp"
#include "engine/scene/systems/PhysicsSystem.hpp"

#include <gtest/gtest.h>

#include <glm/gtc/quaternion.hpp>

#include <array>
#include <cmath>
#include <utility>
#include <vector>

using Engine::Physics::BodyId;
using Engine::Physics::Collider;
using Engine::Physics::Material;
using Engine::Physics::MotionType;
using Engine::Physics::PhysicsWorld;
using Engine::Physics::ShapeType;

namespace
{
    constexpr double kStep = PhysicsWorld::FixedStep;
    constexpr float kTolerance = 1e-3f;
    const glm::vec3 kDown(0.0f, -1.0f, 0.0f);

    void Steps(PhysicsWorld& world, int steps)
    {
        for (int i = 0; i < steps; ++i)
        {
            world.Update(kStep);
        }
    }

    // A world with no gravity: bodies stay where they are put
    struct Space
    {
        PhysicsWorld world;
        Space()
        {
            world.SetGravity({0.0f, 0.0f, 0.0f});
            EXPECT_TRUE(world.Init());
        }
    };

    // A world with Earth's gravity and a wide static floor whose top is at y = 0
    struct Ground
    {
        PhysicsWorld world;
        BodyId floor;
        Ground()
        {
            EXPECT_TRUE(world.Init());
            floor = world.AddBody({.motion = MotionType::Static,
                                   .position = {0.0f, -0.5f, 0.0f},
                                   .collider = {.halfExtents = {50.0f, 0.5f, 50.0f}}});
        }
    };

    BodyId AddStatic(PhysicsWorld& world, const glm::vec3& position, const Collider& collider = {})
    {
        return world.AddBody({.motion = MotionType::Static, .position = position, .collider = collider});
    }

    void ExpectNear(const glm::vec3& actual, const glm::vec3& expected, float tolerance = kTolerance)
    {
        EXPECT_NEAR(actual.x, expected.x, tolerance);
        EXPECT_NEAR(actual.y, expected.y, tolerance);
        EXPECT_NEAR(actual.z, expected.z, tolerance);
    }
}

// ---- Ray casts

// Rays meet every shape on its surface, from any side: the point is on the ray, as far along it as the distance
// says, and the normal faces out of the shape there.
TEST(Queries, RaysHitEveryShapeOnItsSurface)
{
    Space space;
    PhysicsWorld& world = space.world;

    // A sphere of radius 1, from eight directions around it
    AddStatic(world, {0.0f, 0.0f, 0.0f}, {.shape = ShapeType::Sphere, .radius = 1.0f});
    for (int i = 0; i < 8; ++i)
    {
        const float angle = glm::radians(45.0f * static_cast<float>(i));
        const glm::vec3 out(std::cos(angle), 0.0f, std::sin(angle));
        const auto hit = world.CastRay(out * 5.0f, -out, 10.0f);
        ASSERT_TRUE(hit.has_value()) << i;
        EXPECT_NEAR(hit->distance, 4.0f, kTolerance) << i;
        ExpectNear(hit->point, out);
        ExpectNear(hit->normal, out);
        ExpectNear(hit->point, out * 5.0f - out * hit->distance);
    }

    // A capsule standing up: its rounded top, and its side
    AddStatic(world, {10.0f, 0.0f, 0.0f}, {.shape = ShapeType::Capsule, .radius = 0.5f, .halfHeight = 1.0f});
    const auto top = world.CastRay({10.0f, 5.0f, 0.0f}, kDown, 10.0f);
    ASSERT_TRUE(top.has_value());
    EXPECT_NEAR(top->distance, 3.5f, kTolerance);
    ExpectNear(top->normal, {0.0f, 1.0f, 0.0f});
    const auto side = world.CastRay({7.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, 10.0f);
    ASSERT_TRUE(side.has_value());
    EXPECT_NEAR(side->distance, 2.5f, kTolerance);
    ExpectNear(side->normal, {-1.0f, 0.0f, 0.0f});

    // A box turned 45 degrees: a face's normal turns with it
    const glm::quat turn = glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    world.AddBody({.motion = MotionType::Static, .position = {20.0f, 0.0f, 0.0f}, .rotation = turn});
    const glm::vec3 face = turn * glm::vec3(1.0f, 0.0f, 0.0f);
    const auto turned = world.CastRay(glm::vec3(20.0f, 0.0f, 0.0f) + face * 5.0f, -face, 10.0f);
    ASSERT_TRUE(turned.has_value());
    EXPECT_NEAR(turned->distance, 4.5f, kTolerance);
    ExpectNear(turned->normal, face);

    // A box whose collider sits 1 above its body
    AddStatic(world, {30.0f, 0.0f, 0.0f}, {.center = {0.0f, 1.0f, 0.0f}});
    const auto raised = world.CastRay({30.0f, 5.0f, 0.0f}, kDown, 10.0f);
    ASSERT_TRUE(raised.has_value());
    EXPECT_NEAR(raised->point.y, 1.5f, kTolerance);
}

// Of bodies in a row the nearest is hit; passing through it, the next one. A ray through the gap between them misses.
TEST(Queries, RaysHitTheNearestOfSeveral)
{
    Space space;
    PhysicsWorld& world = space.world;
    const std::array<BodyId, 3> row{AddStatic(world, {2.0f, 0.0f, 0.0f}), AddStatic(world, {4.0f, 0.0f, 0.0f}),
                                    AddStatic(world, {6.0f, 0.0f, 0.0f})};

    const auto first = world.CastRay({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 20.0f);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->body, row[0]);
    EXPECT_NEAR(first->distance, 1.5f, kTolerance);

    const auto second = world.CastRay({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 20.0f, row[0]);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->body, row[1]);
    EXPECT_NEAR(second->distance, 3.5f, kTolerance);

    const auto back = world.CastRay({10.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, 20.0f);
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(back->body, row[2]) << "from the other end, the other end is nearest";

    EXPECT_FALSE(world.CastRay({3.0f, 0.0f, -10.0f}, {0.0f, 0.0f, 1.0f}, 20.0f).has_value()) << "through the gap";
}

// Static, kinematic and dynamic bodies are all hit, and so is a body that has gone to sleep.
TEST(Queries, RaysHitEveryKindOfBodyAndSleepers)
{
    Ground ground;
    PhysicsWorld& world = ground.world;
    const BodyId fixed = AddStatic(world, {0.0f, 0.5f, 0.0f});
    const BodyId moved = world.AddBody({.motion = MotionType::Kinematic, .position = {3.0f, 0.5f, 0.0f}});
    const BodyId resting = world.AddBody({.position = {6.0f, 0.5f, 0.0f}});

    Steps(world, 180); // the dynamic box settles and sleeps
    EXPECT_FALSE(world.IsAwake(resting));

    for (const auto& [body, x] : {std::pair{fixed, 0.0f}, std::pair{moved, 3.0f}, std::pair{resting, 6.0f}})
    {
        const auto hit = world.CastRay({x, 5.0f, 0.0f}, kDown, 10.0f);
        ASSERT_TRUE(hit.has_value()) << x;
        EXPECT_EQ(hit->body, body) << x;
        EXPECT_NEAR(hit->distance, 4.0f, 0.02f) << x;
    }
}

// An entity moved by game code (here its Transform, as the Inspector or a block would) is hit where it went once
// the next Update has put its body there.
TEST(Queries, RaysFollowMovedEntities)
{
    using Engine::Scene::BodyType;
    using Engine::Scene::Entity;
    using Engine::Scene::RigidBodyComponent;
    using Engine::Scene::TransformComponent;
    using Engine::Systems::PhysicsSystem;

    Engine::Scene::Scene scene;
    Space space;
    const Entity platform = scene.CreateEntity();
    scene.AddComponent(platform, RigidBodyComponent{.type = BodyType::Kinematic});
    PhysicsSystem::Update(scene, space.world, kStep);

    const auto before = PhysicsSystem::Raycast(scene, space.world, {0.0f, 5.0f, 0.0f}, kDown, 10.0f);
    ASSERT_TRUE(before.has_value());
    EXPECT_EQ(before->entity, platform);

    scene.SetTransform(platform, TransformComponent{.x = 8.0f});
    PhysicsSystem::Update(scene, space.world, kStep);
    EXPECT_FALSE(PhysicsSystem::Raycast(scene, space.world, {0.0f, 5.0f, 0.0f}, kDown, 10.0f).has_value());
    const auto after = PhysicsSystem::Raycast(scene, space.world, {8.0f, 5.0f, 0.0f}, kDown, 10.0f);
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->entity, platform);
    EXPECT_NEAR(after->distance, 4.5f, kTolerance);
}

// ---- Distance checks

// The distance from a to b is the distance from b to a, as far as their two nearest points are apart; a ray from a's
// point towards b's meets b that far away. (Near a box's edges and corners the distance check rounds them off by Jolt's
// convex radius, 0.05 at most, where a ray meets the sharp box: the two can differ by a hundredth or so there.)
TEST(Queries, DistancesAgreeWithTheirPointsAndWithRays)
{
    Space space;
    PhysicsWorld& world = space.world;
    constexpr float kRoundedEdges = 0.01f;
    const BodyId ball = AddStatic(world, {0.0f, 0.0f, 0.0f}, {.shape = ShapeType::Sphere, .radius = 0.5f});
    const BodyId box = AddStatic(world, {3.0f, 1.2f, 0.4f});
    const BodyId capsule = AddStatic(world, {-2.0f, 3.0f, 1.0f}, {.shape = ShapeType::Capsule, .radius = 0.4f});
    const BodyId turned =
        world.AddBody({.motion = MotionType::Static,
                       .position = {0.5f, -3.0f, -2.0f},
                       .rotation = glm::angleAxis(glm::radians(30.0f), glm::vec3(1.0f, 0.0f, 1.0f) / std::sqrt(2.0f))});

    const std::array<std::pair<BodyId, BodyId>, 5> pairs{std::pair{ball, box}, std::pair{ball, capsule},
                                                         std::pair{ball, turned}, std::pair{box, capsule},
                                                         std::pair{capsule, turned}};
    for (const auto& [a, b] : pairs)
    {
        const auto there = world.DistanceBetween(a, b);
        const auto back = world.DistanceBetween(b, a);
        ASSERT_TRUE(there.has_value() && back.has_value()) << a.value << " " << b.value;
        EXPECT_GT(there->distance, 0.0f);
        EXPECT_NEAR(there->distance, back->distance, kTolerance);
        EXPECT_NEAR(glm::length(there->pointOnB - there->pointOnA), there->distance, kTolerance);

        const auto ray = world.CastRay(there->pointOnA, there->pointOnB - there->pointOnA, 100.0f, a);
        ASSERT_TRUE(ray.has_value());
        EXPECT_EQ(ray->body, b);
        EXPECT_NEAR(ray->distance, there->distance, kRoundedEdges) << a.value << " " << b.value;
    }
}

// A body moving towards a wall closes the gap by its speed every step, until it touches and the gap is 0.
TEST(Queries, GapsCloseAsBodiesApproach)
{
    Space space;
    PhysicsWorld& world = space.world;
    const BodyId wall = AddStatic(world, {10.0f, 0.0f, 0.0f}, {.halfExtents = {0.5f, 5.0f, 5.0f}});
    const BodyId ball =
        world.AddBody({.position = {0.0f, 0.0f, 0.0f}, .collider = {.shape = ShapeType::Sphere, .radius = 0.5f}});
    world.SetVelocity(ball, {3.0f, 0.0f, 0.0f});
    EXPECT_NEAR(world.DistanceBetween(ball, wall)->distance, 9.0f, kTolerance);

    Steps(world, 60); // one second: 3 units closer
    EXPECT_NEAR(world.DistanceBetween(ball, wall)->distance, 6.0f, kTolerance);
    Steps(world, 60);
    EXPECT_NEAR(world.DistanceBetween(ball, wall)->distance, 3.0f, kTolerance);

    Steps(world, 90); // reached the wall
    EXPECT_NEAR(world.DistanceBetween(ball, wall)->distance, 0.0f, 0.02f);
    EXPECT_TRUE(world.AreTouching(ball, wall));
}

// Bodies within a radius of a point are found whatever moves them, nearest first, each as far as its surface; the
// radius leaves out what is beyond it.
TEST(Queries, BodiesWithinAreFoundNearestFirst)
{
    Space space;
    PhysicsWorld& world = space.world;
    const BodyId closest = AddStatic(world, {2.0f, 0.0f, 0.0f});                                          // 1.5 away
    const BodyId mid = world.AddBody({.motion = MotionType::Kinematic, .position = {0.0f, 0.0f, -4.0f}}); // 3.5
    const BodyId farthest = world.AddBody({.position = {0.0f, 6.0f, 0.0f}});                              // 5.5
    AddStatic(world, {-10.0f, 0.0f, 0.0f});                                                               // 9.5

    const auto found = world.BodiesWithin({0.0f, 0.0f, 0.0f}, 6.0f);
    ASSERT_EQ(found.size(), 3u);
    EXPECT_EQ(found[0].body, closest);
    EXPECT_NEAR(found[0].distance, 1.5f, kTolerance);
    EXPECT_EQ(found[1].body, mid);
    EXPECT_NEAR(found[1].distance, 3.5f, kTolerance);
    EXPECT_EQ(found[2].body, farthest);
    EXPECT_NEAR(found[2].distance, 5.5f, kTolerance);

    EXPECT_EQ(world.BodiesWithin({0.0f, 0.0f, 0.0f}, 3.0f).size(), 1u);
    EXPECT_EQ(world.BodiesWithin({0.0f, 0.0f, 0.0f}, 20.0f).size(), 4u);

    // Every one of them as far as DistanceBetween puts a tiny ball at the point
    const BodyId probe =
        world.AddBody({.motion = MotionType::Static, .collider = {.shape = ShapeType::Sphere, .radius = 0.01f}});
    for (const auto& body : world.BodiesWithin({0.0f, 0.0f, 0.0f}, 6.0f, probe))
    {
        EXPECT_NEAR(world.DistanceBetween(probe, body.body)->distance + 0.01f, body.distance, kTolerance);
    }
}

// Entities that game code moves are measured where they went once the next Update has put their bodies there.
TEST(Queries, DistancesFollowMovedEntities)
{
    using Engine::Scene::BodyType;
    using Engine::Scene::Entity;
    using Engine::Scene::RigidBodyComponent;
    using Engine::Scene::TransformComponent;
    using Engine::Systems::PhysicsSystem;

    Engine::Scene::Scene scene;
    Space space;
    const Entity a = scene.CreateEntity();
    const Entity b = scene.CreateEntity();
    scene.SetTransform(b, TransformComponent{.x = 3.0f});
    scene.AddComponent(a, RigidBodyComponent{.type = BodyType::Kinematic});
    scene.AddComponent(b, RigidBodyComponent{.type = BodyType::Static});
    PhysicsSystem::Update(scene, space.world, kStep);
    EXPECT_NEAR(PhysicsSystem::DistanceBetween(scene, space.world, a, b)->distance, 2.0f, kTolerance);

    scene.SetTransform(a, TransformComponent{.x = -5.0f});
    PhysicsSystem::Update(scene, space.world, kStep);
    EXPECT_NEAR(PhysicsSystem::DistanceBetween(scene, space.world, a, b)->distance, 7.0f, kTolerance);
    const auto nearby = PhysicsSystem::EntitiesWithin(scene, space.world, {-5.0f, 0.0f, 0.0f}, 3.0f);
    ASSERT_EQ(nearby.size(), 1u);
    EXPECT_EQ(nearby[0].entity, a);
}

// ---- On-ground detection

// In a stack each box stands on the one below it, and the bottom one on the floor.
TEST(Queries, StackedBoxesStandOnTheOneBelow)
{
    Ground ground;
    PhysicsWorld& world = ground.world;
    const BodyId bottom = world.AddBody({.position = {0.0f, 0.5f, 0.0f}});
    const BodyId top = world.AddBody({.position = {0.0f, 1.6f, 0.0f}});
    Steps(world, 120);

    EXPECT_EQ(world.GroundUnder(bottom)->body, ground.floor);
    const auto underTop = world.GroundUnder(top);
    ASSERT_TRUE(underTop.has_value());
    EXPECT_EQ(underTop->body, bottom);
    EXPECT_NEAR(underTop->point.y, 1.0f, 0.02f);
}

// A box in a corner touches a wall and the floor: the floor is its ground. Allowing slopes up to 90 degrees makes the
// wall ground too; allowing none still counts flat ground.
TEST(Queries, CornersAndSlopeLimits)
{
    Ground ground;
    PhysicsWorld& world = ground.world;
    const BodyId wall = AddStatic(world, {1.0f, 2.0f, 0.0f}, {.halfExtents = {0.5f, 2.0f, 5.0f}});
    const BodyId box = world.AddBody({.position = {0.0f, 0.5f, 0.0f}});
    Steps(world, 60);
    ASSERT_TRUE(world.AreTouching(box, wall));

    EXPECT_EQ(world.GroundUnder(box)->body, ground.floor) << "the flattest one";
    EXPECT_TRUE(world.IsOnGround(box, 0.0f)) << "flat ground has no slope";

    // Without the floor only the wall is left: ground when any slope is allowed
    world.RemoveBody(ground.floor);
    world.SetGravity({0.0f, 0.0f, 0.0f});
    EXPECT_FALSE(world.IsOnGround(box));
    const auto steep = world.GroundUnder(box, 90.0f);
    ASSERT_TRUE(steep.has_value());
    EXPECT_EQ(steep->body, wall);
}

// With gravity pulling sideways a body falls onto a wall and stands on it; the floor it slides along is not ground.
TEST(Queries, SidewaysGravityMakesAWallTheGround)
{
    Ground ground;
    PhysicsWorld& world = ground.world;
    world.SetGravity({-9.81f, 0.0f, 0.0f});
    const BodyId wall = AddStatic(world, {-5.0f, 3.0f, 0.0f}, {.halfExtents = {0.5f, 3.0f, 5.0f}});
    const BodyId box = world.AddBody({.position = {0.0f, 0.5f, 0.0f}, .material = {.friction = 0.0f}});
    EXPECT_FALSE(world.IsOnGround(box)) << "resting on the floor, which is now a wall";

    Steps(world, 120);
    const auto under = world.GroundUnder(box);
    ASSERT_TRUE(under.has_value());
    EXPECT_EQ(under->body, wall);
    ExpectNear(under->normal, {1.0f, 0.0f, 0.0f});
}

// A box sliding down an icy ramp stays on the ground the whole way.
TEST(Queries, SlidingDownARampStaysOnTheGround)
{
    Ground ground;
    PhysicsWorld& world = ground.world;
    constexpr Material kIce{.friction = 0.0f};
    const glm::quat tilt = glm::angleAxis(glm::radians(20.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    const glm::vec3 rampCentre(0.0f, 5.0f, 0.0f);
    world.AddBody({.motion = MotionType::Static,
                   .position = rampCentre,
                   .rotation = tilt,
                   .collider = {.halfExtents = {6.0f, 0.5f, 3.0f}},
                   .material = kIce});
    const BodyId box = world.AddBody(
        {.position = rampCentre + tilt * glm::vec3(-3.0f, 1.0f, 0.0f), .rotation = tilt, .material = kIce});

    const float startX = world.Pose(box)->position.x;
    for (int step = 0; step < 60; ++step)
    {
        world.Update(kStep);
        ASSERT_TRUE(world.IsOnGround(box)) << "step " << step;
    }
    EXPECT_LT(world.Pose(box)->position.x, startX - 0.5f) << "it slid down";
}

// Played frame by frame, an entity dropped onto the floor is in the air until it lands and on the ground from then
// on: the answer changes once. A jump changes it back until it lands again.
TEST(Queries, TheGroundStateChangesOnceWhenLandingAndJumping)
{
    using Engine::Scene::BodyType;
    using Engine::Scene::Entity;
    using Engine::Scene::RigidBodyComponent;
    using Engine::Scene::TransformComponent;
    using Engine::Systems::PhysicsSystem;

    Engine::Scene::Scene scene;
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    const Entity floor = scene.CreateEntity();
    scene.SetTransform(floor, TransformComponent{.y = -0.5f, .scaleX = 20.0f, .scaleZ = 20.0f});
    scene.AddComponent(floor, RigidBodyComponent{.type = BodyType::Static});
    const Entity player = scene.CreateEntity();
    scene.SetTransform(player, TransformComponent{.y = 2.0f});
    scene.AddComponent(player, RigidBodyComponent{});

    // Frames until the answer changes, and how often it does
    const auto watch = [&](int frames)
    {
        int changes = 0;
        bool last = PhysicsSystem::IsOnGround(scene, world, player);
        for (int frame = 0; frame < frames; ++frame)
        {
            PhysicsSystem::Update(scene, world, kStep);
            const bool now = PhysicsSystem::IsOnGround(scene, world, player);
            changes += now != last ? 1 : 0;
            last = now;
        }
        return changes;
    };

    EXPECT_EQ(watch(120), 1) << "lands once";
    ASSERT_TRUE(PhysicsSystem::IsOnGround(scene, world, player));
    EXPECT_EQ(PhysicsSystem::GroundUnder(scene, world, player)->entity, floor);

    ASSERT_TRUE(PhysicsSystem::AddImpulse(scene, world, player, {0.0f, 5.0f, 0.0f})); // mass 1: 5 units per second
    EXPECT_EQ(watch(150), 2) << "takes off, lands";
    EXPECT_TRUE(PhysicsSystem::IsOnGround(scene, world, player));
}
