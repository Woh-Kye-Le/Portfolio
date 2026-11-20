/* !
@file    	    ColliderLinesTests.cpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		28/10/2026
@brief
    Unit tests for collider debug drawing (ATE3D-378): the wireframes
    OutlineOf makes for boxes, spheres and capsules, placed at a pose, and the
    outlines the PhysicsSystem gives for every rigid body in edit and Play mode.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/physics/ColliderLines.hpp"
#include "engine/physics/PhysicsWorld.hpp"
#include "engine/scene/Scene.hpp"
#include "engine/scene/systems/PhysicsSystem.hpp"

#include <gtest/gtest.h>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

using Engine::Physics::BodyPose;
using Engine::Physics::Collider;
using Engine::Physics::Line;
using Engine::Physics::OutlineOf;
using Engine::Physics::ShapeType;

namespace
{
    constexpr float kTolerance = 1e-4f;

    // Every end of every segment
    std::vector<glm::vec3> PointsOf(const std::vector<Line>& lines)
    {
        std::vector<glm::vec3> points;
        for (const Line& line : lines)
        {
            points.push_back(line.from);
            points.push_back(line.to);
        }
        return points;
    }

    // The smallest box around the points
    std::pair<glm::vec3, glm::vec3> BoundsOf(const std::vector<Line>& lines)
    {
        glm::vec3 low(1e9f);
        glm::vec3 high(-1e9f);
        for (const glm::vec3& point : PointsOf(lines))
        {
            low = glm::min(low, point);
            high = glm::max(high, point);
        }
        return {low, high};
    }

    void ExpectNear(const glm::vec3& actual, const glm::vec3& expected)
    {
        EXPECT_NEAR(actual.x, expected.x, kTolerance);
        EXPECT_NEAR(actual.y, expected.y, kTolerance);
        EXPECT_NEAR(actual.z, expected.z, kTolerance);
    }
}

// A box is its 12 edges, each as long as the box is on one axis, between its 8 corners.
TEST(ColliderLines, ABoxIsItsTwelveEdges)
{
    const glm::vec3 half(1.0f, 2.0f, 3.0f);
    const std::vector<Line> lines = OutlineOf({.shape = ShapeType::Box, .halfExtents = half}, {});

    ASSERT_EQ(lines.size(), 12u);
    int alongX = 0;
    int alongY = 0;
    int alongZ = 0;
    for (const Line& line : lines)
    {
        const glm::vec3 edge = glm::abs(line.to - line.from);
        alongX += std::abs(edge.x - 2.0f) < kTolerance && edge.y < kTolerance && edge.z < kTolerance ? 1 : 0;
        alongY += std::abs(edge.y - 4.0f) < kTolerance && edge.x < kTolerance && edge.z < kTolerance ? 1 : 0;
        alongZ += std::abs(edge.z - 6.0f) < kTolerance && edge.x < kTolerance && edge.y < kTolerance ? 1 : 0;
        for (const glm::vec3& end : {line.from, line.to})
        {
            ExpectNear(glm::abs(end), half); // a corner
        }
    }
    EXPECT_EQ(alongX, 4);
    EXPECT_EQ(alongY, 4);
    EXPECT_EQ(alongZ, 4);
}

// A sphere is three circles: every point is one radius from the centre, and they reach it on every axis.
TEST(ColliderLines, ASphereIsThreeCircles)
{
    const std::vector<Line> lines = OutlineOf({.shape = ShapeType::Sphere, .radius = 2.0f}, {}, 16);

    EXPECT_EQ(lines.size(), 3u * 16u);
    for (const glm::vec3& point : PointsOf(lines))
    {
        EXPECT_NEAR(glm::length(point), 2.0f, kTolerance);
    }
    const auto [low, high] = BoundsOf(lines);
    ExpectNear(low, glm::vec3(-2.0f));
    ExpectNear(high, glm::vec3(2.0f));
}

// A capsule stands along Y: half its straight middle plus its radius up and down, its radius around.
TEST(ColliderLines, ACapsuleStandsAlongY)
{
    const std::vector<Line> lines =
        OutlineOf({.shape = ShapeType::Capsule, .radius = 0.5f, .halfHeight = 1.0f}, {}, 16);

    const auto [low, high] = BoundsOf(lines);
    ExpectNear(low, glm::vec3(-0.5f, -1.5f, -0.5f));
    ExpectNear(high, glm::vec3(0.5f, 1.5f, 0.5f));

    // Every point is on its surface: one radius from the nearest point of the middle segment
    for (const glm::vec3& point : PointsOf(lines))
    {
        const glm::vec3 nearest(0.0f, std::clamp(point.y, -1.0f, 1.0f), 0.0f);
        EXPECT_NEAR(glm::length(point - nearest), 0.5f, kTolerance);
    }
}

// A capsule with no straight middle is a sphere, as the physics makes it.
TEST(ColliderLines, AFlatCapsuleIsASphere)
{
    const auto capsule = OutlineOf({.shape = ShapeType::Capsule, .radius = 1.0f, .halfHeight = 0.0f}, {}, 16);
    const auto sphere = OutlineOf({.shape = ShapeType::Sphere, .radius = 1.0f}, {}, 16);
    ASSERT_EQ(capsule.size(), sphere.size());
    for (std::size_t i = 0; i < capsule.size(); ++i)
    {
        ExpectNear(capsule[i].from, sphere[i].from);
        ExpectNear(capsule[i].to, sphere[i].to);
    }
}

// The collider's centre is moved and turned with the body: an offset box on a body turned 90 degrees around Y.
TEST(ColliderLines, TheOutlineFollowsThePoseAndTheCentre)
{
    const BodyPose pose{.position = {10.0f, 0.0f, 0.0f},
                        .rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))};
    const std::vector<Line> lines =
        OutlineOf({.shape = ShapeType::Box, .halfExtents = {1.0f, 0.5f, 0.25f}, .center = {2.0f, 0.0f, 0.0f}}, pose);

    // Turned 90 degrees around Y, the body's +X points along world -Z: the box's centre is at (10, 0, -2) and its
    // long side runs along Z
    const auto [low, high] = BoundsOf(lines);
    ExpectNear((low + high) * 0.5f, glm::vec3(10.0f, 0.0f, -2.0f));
    ExpectNear(high - low, glm::vec3(0.5f, 1.0f, 2.0f));
}

// Circles get at least 8 segments, however few are asked for.
TEST(ColliderLines, CirclesHaveAtLeastEightSegments)
{
    EXPECT_EQ(OutlineOf({.shape = ShapeType::Sphere}, {}, 2).size(), 3u * 8u);
}

// The PhysicsSystem outlines every rigid body: in edit mode (no bodies made) where its Transform puts it, and in Play
// mode where the physics has it, with whether it is asleep or touching something.
TEST(ColliderLines, OutlinesForEveryRigidBody)
{
    using Engine::Physics::PhysicsWorld;
    using Engine::Scene::BodyType;
    using Engine::Scene::Entity;
    using Engine::Scene::MeshRendererComponent;
    using Engine::Scene::RigidBodyComponent;
    using Engine::Scene::TransformComponent;
    using Engine::Systems::ColliderOutline;
    using Engine::Systems::PhysicsSystem;

    Engine::Scene::Scene scene;
    const Entity floor = scene.CreateEntity();
    scene.AddComponent(floor, RigidBodyComponent{.type = BodyType::Static});
    scene.AddComponent(floor, MeshRendererComponent{.shape = Engine::Game::Shape::Plane});
    const Entity cube = scene.CreateEntity();
    scene.SetTransform(cube, TransformComponent{.y = 3.0f});
    scene.AddComponent(cube, RigidBodyComponent{.type = BodyType::Dynamic});
    scene.CreateEntity(); // no RigidBody: not outlined

    const auto find = [](const std::vector<ColliderOutline>& outlines, Entity entity)
    {
        const auto it =
            std::ranges::find_if(outlines, [entity](const ColliderOutline& o) { return o.entity == entity; });
        return it != outlines.end() ? *it : ColliderOutline{};
    };

    // Edit mode: where the Transforms are
    std::vector<ColliderOutline> outlines = PhysicsSystem::Outlines(scene, nullptr);
    ASSERT_EQ(outlines.size(), 2u);
    ColliderOutline box = find(outlines, cube);
    EXPECT_EQ(box.type, BodyType::Dynamic);
    EXPECT_EQ(box.lines.size(), 12u);
    EXPECT_NEAR(BoundsOf(box.lines).first.y, 2.5f, kTolerance);
    EXPECT_FALSE(box.asleep);
    EXPECT_FALSE(box.touching);
    EXPECT_EQ(find(outlines, floor).type, BodyType::Static);

    // Play mode: the cube falls onto the plane, comes to rest and falls asleep there
    PhysicsWorld world;
    ASSERT_TRUE(world.Init());
    for (int frame = 0; frame < 5 * 60; ++frame)
    {
        PhysicsSystem::Update(scene, world, PhysicsWorld::FixedStep);
    }
    outlines = PhysicsSystem::Outlines(scene, &world);
    box = find(outlines, cube);
    EXPECT_NEAR(BoundsOf(box.lines).first.y, 0.0f, 0.03f) << "drawn where it lies, on the plane";
    EXPECT_TRUE(box.asleep);
    EXPECT_TRUE(box.touching);
    EXPECT_FALSE(find(outlines, floor).asleep) << "static bodies are never asleep";
}
