/* !
@file    	    PhysicsSystemTests.cpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		26/10/2026
@brief
    Unit tests for the ECS PhysicsSystem: bodies made for entities with a
    RigidBody, moved entities, fitted and chosen colliders (box, sphere,
    capsule), bodies removed or made again when the RigidBody changes,
    collisions between entities, pushing entities, rays hitting them,
    distances between entities and from a point, and the ground an entity
    stands on.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/physics/PhysicsWorld.hpp"
#include "engine/scene/Scene.hpp"
#include "engine/scene/systems/HierarchySystem.hpp"
#include "engine/scene/systems/PhysicsSystem.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <utility>
#include <vector>

using Engine::Physics::PhysicsWorld;
using Engine::Scene::BodyType;
using Engine::Scene::Entity;
using Engine::Scene::MeshRendererComponent;
using Engine::Scene::PhysicsBodyComponent;
using Engine::Scene::RigidBodyComponent;
using Engine::Scene::Scene;
using Engine::Scene::TransformComponent;
using Engine::Systems::Collision;
using Engine::Systems::PhysicsSystem;

namespace
{
    Entity MakeBody(Scene& scene, const TransformComponent& transform, BodyType type)
    {
        const Entity entity = scene.CreateEntity(); // comes with a Transform
        scene.SetTransform(entity, transform);
        scene.AddComponent(entity, RigidBodyComponent{.type = type});
        return entity;
    }

    // Frames at 60 FPS, as GameRuntime::Tick runs them in Play mode
    void RunSeconds(Scene& scene, PhysicsWorld& world, double seconds)
    {
        for (double time = 0.0; time < seconds; time += PhysicsWorld::FixedStep)
        {
            PhysicsSystem::Update(scene, world, PhysicsWorld::FixedStep);
            Engine::Systems::HierarchySystem::Update(scene);
        }
    }

    // Frames at 60 FPS, keeping every collision they report
    std::vector<Collision> RunCollecting(Scene& scene, PhysicsWorld& world, double seconds)
    {
        std::vector<Collision> all;
        for (double time = 0.0; time < seconds; time += PhysicsWorld::FixedStep)
        {
            for (const Collision& collision : PhysicsSystem::Update(scene, world, PhysicsWorld::FixedStep))
            {
                all.push_back(collision);
            }
        }
        return all;
    }

    // A static plane at y = 0 and a dynamic cube held above it
    std::pair<Entity, Entity> MakeFloorAndCube(Scene& scene, float height)
    {
        const Entity floor = MakeBody(scene, TransformComponent{}, BodyType::Static);
        scene.AddComponent(floor, MeshRendererComponent{.shape = Engine::Game::Shape::Plane});
        const Entity cube = MakeBody(scene, TransformComponent{.y = height}, BodyType::Dynamic);
        scene.AddComponent(cube, MeshRendererComponent{.shape = Engine::Game::Shape::Cube});
        return {floor, cube};
    }

    float HeightOf(const Scene& scene, Entity entity)
    {
        return scene.GetTransform(entity)->y;
    }

    class PhysicsSystemTest : public ::testing::Test
    {
    protected:
        void SetUp() override { ASSERT_TRUE(world_.Init()); }

        Scene scene_;
        PhysicsWorld world_;
    };
}

// Without a running world (edit mode never starts the system) no body is made.
TEST(PhysicsSystem, NothingHappensWithoutARunningWorld)
{
    Scene scene;
    PhysicsWorld world;
    const Entity box = MakeBody(scene, TransformComponent{.y = 5.0f}, BodyType::Dynamic);
    PhysicsSystem::Update(scene, world, 1.0);
    EXPECT_FALSE(scene.HasComponent<PhysicsBodyComponent>(box));
    EXPECT_FLOAT_EQ(HeightOf(scene, box), 5.0f);
}

// A dynamic entity falls; a static one stays where it is.
TEST_F(PhysicsSystemTest, DynamicEntitiesFallAndStaticOnesStay)
{
    const Entity falling = MakeBody(scene_, TransformComponent{.y = 10.0f}, BodyType::Dynamic);
    const Entity fixed = MakeBody(scene_, TransformComponent{.x = 5.0f, .y = 10.0f}, BodyType::Static);

    RunSeconds(scene_, world_, 1.0);

    EXPECT_TRUE(scene_.HasComponent<PhysicsBodyComponent>(falling));
    EXPECT_EQ(world_.BodyCount(), 2u);
    EXPECT_NEAR(HeightOf(scene_, falling), 10.0f - 4.9f, 0.3f);
    EXPECT_FLOAT_EQ(HeightOf(scene_, fixed), 10.0f);
}

// A cube dropped on a static plane lands on it: the plane's collider has its top where the plane is drawn.
TEST_F(PhysicsSystemTest, ACubeLandsOnAPlane)
{
    const Entity floor = MakeBody(scene_, TransformComponent{}, BodyType::Static);
    scene_.AddComponent(floor, MeshRendererComponent{.shape = Engine::Game::Shape::Plane});
    const Entity cube = MakeBody(scene_, TransformComponent{.y = 3.0f}, BodyType::Dynamic);
    scene_.AddComponent(cube, MeshRendererComponent{.shape = Engine::Game::Shape::Cube});

    RunSeconds(scene_, world_, 3.0);

    EXPECT_NEAR(HeightOf(scene_, cube), 0.5f, 0.05f);
}

// Taking the RigidBody away removes the body; destroying the entity needs RemoveBody first.
TEST_F(PhysicsSystemTest, BodiesAreRemovedWithTheirRigidBody)
{
    const Entity first = MakeBody(scene_, TransformComponent{}, BodyType::Dynamic);
    const Entity second = MakeBody(scene_, TransformComponent{.x = 3.0f}, BodyType::Dynamic);
    RunSeconds(scene_, world_, PhysicsWorld::FixedStep);
    ASSERT_EQ(world_.BodyCount(), 2u);

    scene_.RemoveComponent<RigidBodyComponent>(first);
    RunSeconds(scene_, world_, PhysicsWorld::FixedStep);
    EXPECT_FALSE(scene_.HasComponent<PhysicsBodyComponent>(first));
    EXPECT_EQ(world_.BodyCount(), 1u);

    PhysicsSystem::RemoveBody(scene_, world_, second);
    scene_.DestroyEntity(second);
    EXPECT_EQ(world_.BodyCount(), 0u);
}

// Changing the RigidBody makes the body again: a falling body made static stops where it is.
TEST_F(PhysicsSystemTest, ChangingTheRigidBodyMakesTheBodyAgain)
{
    const Entity box = MakeBody(scene_, TransformComponent{.y = 10.0f}, BodyType::Dynamic);
    RunSeconds(scene_, world_, 0.5);
    const auto before = scene_.GetComponent<PhysicsBodyComponent>(box);
    ASSERT_TRUE(before.has_value());

    scene_.SetComponent(box, RigidBodyComponent{.type = BodyType::Static});
    RunSeconds(scene_, world_, PhysicsWorld::FixedStep);
    const float stoppedAt = HeightOf(scene_, box);
    RunSeconds(scene_, world_, 0.5);

    EXPECT_FALSE(world_.HasBody(before->body));
    EXPECT_EQ(world_.BodyCount(), 1u);
    EXPECT_FLOAT_EQ(HeightOf(scene_, box), stoppedAt);
}

// Moving an entity's Transform (the Inspector, a gizmo, game code) moves its body there.
TEST_F(PhysicsSystemTest, MovingTheTransformMovesTheBody)
{
    const Entity platform = MakeBody(scene_, TransformComponent{}, BodyType::Kinematic);
    RunSeconds(scene_, world_, PhysicsWorld::FixedStep);

    scene_.SetTransform(platform, TransformComponent{.x = 5.0f, .rotation = 90.0f});
    RunSeconds(scene_, world_, PhysicsWorld::FixedStep);

    const auto body = scene_.GetComponent<PhysicsBodyComponent>(platform);
    ASSERT_TRUE(body.has_value());
    EXPECT_NEAR(world_.Pose(body->body)->position.x, 5.0f, 1e-4f);
}

// While paused a block step can still move an object: its body follows, with no time passing in the physics.
TEST_F(PhysicsSystemTest, SyncMovedMovesTheBodyWithNoStep)
{
    const Entity platform = MakeBody(scene_, TransformComponent{}, BodyType::Kinematic);
    RunSeconds(scene_, world_, PhysicsWorld::FixedStep);
    const auto steps = world_.StepCount();

    scene_.SetTransform(platform, TransformComponent{.x = 5.0f});
    PhysicsSystem::SyncMoved(scene_, world_);

    const auto body = scene_.GetComponent<PhysicsBodyComponent>(platform);
    ASSERT_TRUE(body.has_value());
    EXPECT_NEAR(world_.Pose(body->body)->position.x, 5.0f, 1e-4f);
    EXPECT_EQ(world_.StepCount(), steps);
}

// A dynamic child falls away from its parent: its local transform follows, so the hierarchy does not pull it back.
TEST_F(PhysicsSystemTest, ADynamicChildFallsAwayFromItsParent)
{
    const Entity parent = scene_.CreateEntity();
    scene_.SetTransform(parent, TransformComponent{.y = 10.0f});
    const Entity child = MakeBody(scene_, TransformComponent{.y = 10.0f}, BodyType::Dynamic);
    scene_.AddComponent(child, Engine::Scene::HierarchyComponent{.parent = parent.GetId()});

    RunSeconds(scene_, world_, 1.0);

    EXPECT_LT(HeightOf(scene_, child), 6.0f);
    EXPECT_LT(scene_.GetComponent<Engine::Scene::HierarchyComponent>(child)->local.y, -4.0f);
    EXPECT_FLOAT_EQ(HeightOf(scene_, parent), 10.0f);
}

namespace
{
    bool CloseTo(const glm::vec3& actual, const glm::vec3& expected)
    {
        return glm::all(glm::lessThan(glm::abs(actual - expected), glm::vec3(1e-5f)));
    }
}

// Auto fits what the entity draws, scaled with it: a box for nothing or a cube, a sphere for a sphere, and a thin slab
// under a plane whatever its Y scale.
TEST(PhysicsSystem, AutoCollidersFitTheDrawnShape)
{
    using Engine::Physics::ShapeType;
    const TransformComponent doubled{.scale = 2.0f};
    const RigidBodyComponent body; // Auto

    const auto nothing = PhysicsSystem::ColliderFor(doubled, nullptr, body);
    EXPECT_EQ(nothing.shape, ShapeType::Box);
    EXPECT_TRUE(CloseTo(nothing.halfExtents, glm::vec3(1.0f)));

    const MeshRendererComponent cube{.shape = Engine::Game::Shape::Cube};
    EXPECT_TRUE(CloseTo(PhysicsSystem::ColliderFor(doubled, &cube, body).halfExtents, glm::vec3(1.0f)));

    const MeshRendererComponent sphere{.shape = Engine::Game::Shape::Sphere};
    const auto round = PhysicsSystem::ColliderFor(doubled, &sphere, body);
    EXPECT_EQ(round.shape, ShapeType::Sphere);
    EXPECT_FLOAT_EQ(round.radius, 2.0f);

    const MeshRendererComponent plane{.shape = Engine::Game::Shape::Plane};
    const auto slab = PhysicsSystem::ColliderFor(TransformComponent{.scaleX = 2.0f, .scaleY = 7.0f}, &plane, body);
    EXPECT_EQ(slab.shape, ShapeType::Box);
    EXPECT_TRUE(CloseTo(slab.halfExtents, glm::vec3(1.0f, 0.05f, 0.5f)));
    EXPECT_TRUE(CloseTo(slab.center, glm::vec3(0.0f, -0.05f, 0.0f)));
}

// A chosen collider's sizes are in the object's own units: its Transform scales them. A sphere takes the largest
// scale; a capsule's radius the larger of X and Z, and its height Y.
TEST(PhysicsSystem, ChosenCollidersScaleWithTheObject)
{
    using Engine::Physics::ShapeType;
    using Engine::Scene::ColliderShape;
    const TransformComponent scaled{.scale = 2.0f, .scaleX = 0.5f, .scaleY = 1.5f}; // size (1, 3, 2)

    const RigidBodyComponent box{
        .shape = ColliderShape::Box, .centerY = 1.0f, .sizeX = 1.0f, .sizeY = 2.0f, .sizeZ = 3.0f};
    const auto boxCollider = PhysicsSystem::ColliderFor(scaled, nullptr, box);
    EXPECT_EQ(boxCollider.shape, ShapeType::Box);
    EXPECT_TRUE(CloseTo(boxCollider.halfExtents, glm::vec3(0.5f, 3.0f, 3.0f)));
    EXPECT_TRUE(CloseTo(boxCollider.center, glm::vec3(0.0f, 3.0f, 0.0f)));

    const RigidBodyComponent sphere{.shape = ColliderShape::Sphere, .radius = 0.5f};
    const auto sphereCollider = PhysicsSystem::ColliderFor(scaled, nullptr, sphere);
    EXPECT_EQ(sphereCollider.shape, ShapeType::Sphere);
    EXPECT_FLOAT_EQ(sphereCollider.radius, 1.5f);

    const RigidBodyComponent capsule{.shape = ColliderShape::Capsule, .radius = 0.5f, .height = 2.0f};
    const auto capsuleCollider = PhysicsSystem::ColliderFor(scaled, nullptr, capsule);
    EXPECT_EQ(capsuleCollider.shape, ShapeType::Capsule);
    EXPECT_FLOAT_EQ(capsuleCollider.radius, 1.0f);     // 0.5 x the larger of X (1) and Z (2)
    EXPECT_FLOAT_EQ(capsuleCollider.halfHeight, 2.0f); // 6 high in all, less a rounded end of 1 at each end
}

// A model's Auto collider is the box around the model (here a figure 2 tall standing on the object's origin), scaled
// with the object, whatever built-in shape the object had before the model was given to it.
TEST(PhysicsSystem, AutoCollidersFitTheModel)
{
    using Engine::Physics::ShapeType;
    MeshRendererComponent figure{.shape = Engine::Game::Shape::Sphere};
    figure.model = Engine::Assets::ModelHandle{0, 1};
    figure.hasBounds = true;
    figure.boundsMin = glm::vec3(-0.5f, 0.0f, -0.25f);
    figure.boundsMax = glm::vec3(0.5f, 2.0f, 0.25f);
    const auto collider = PhysicsSystem::ColliderFor(TransformComponent{.scale = 2.0f}, &figure, RigidBodyComponent{});
    EXPECT_EQ(collider.shape, ShapeType::Box);
    EXPECT_TRUE(CloseTo(collider.halfExtents, glm::vec3(1.0f, 2.0f, 0.5f)));
    EXPECT_TRUE(CloseTo(collider.center, glm::vec3(0.0f, 2.0f, 0.0f)));
}

// Fit to Shape on an object that draws a model fits the box around the model.
TEST(PhysicsSystem, FitColliderFitsAModel)
{
    RigidBodyComponent body{.shape = Engine::Scene::ColliderShape::Capsule};
    PhysicsSystem::FitCollider(
        body, std::nullopt, Engine::Assets::BoundingBox{glm::vec3(-0.5f, 0.0f, -0.25f), glm::vec3(0.5f, 2.0f, 0.25f)});
    EXPECT_FLOAT_EQ(body.sizeX, 1.0f);
    EXPECT_FLOAT_EQ(body.sizeY, 2.0f);
    EXPECT_FLOAT_EQ(body.centerY, 1.0f);
    EXPECT_FLOAT_EQ(body.radius, 0.5f);
    EXPECT_FLOAT_EQ(body.height, 2.0f);
}

// Picking a shape in the editor fits it to the drawn shape, in the object's own units.
TEST(PhysicsSystem, FitColliderFitsTheDrawnShape)
{
    using Engine::Scene::ColliderShape;

    RigidBodyComponent body{.shape = ColliderShape::Box};
    PhysicsSystem::FitCollider(body, Engine::Game::Shape::Sphere);
    EXPECT_EQ(body.shape, ColliderShape::Box) << "the shape is kept";
    EXPECT_FLOAT_EQ(body.sizeX, 2.0f);
    EXPECT_FLOAT_EQ(body.radius, 1.0f);

    PhysicsSystem::FitCollider(body, Engine::Game::Shape::Plane);
    EXPECT_FLOAT_EQ(body.sizeX, 1.0f);
    EXPECT_FLOAT_EQ(body.sizeY, 0.1f);
    EXPECT_FLOAT_EQ(body.centerY, -0.05f);

    body.shape = ColliderShape::Capsule;
    PhysicsSystem::FitCollider(body, std::nullopt); // nothing drawn: a 1 x 1 x 1 cube
    EXPECT_FLOAT_EQ(body.radius, 0.5f);
    EXPECT_FLOAT_EQ(body.height, 1.0f);
    EXPECT_FLOAT_EQ(body.centerY, 0.0f);
}

// A sphere collider on a cube-drawing object rests at its radius, not at the cube's half height: the collider is
// what lands, and changing it makes the body again.
TEST_F(PhysicsSystemTest, TheChosenColliderIsWhatLands)
{
    const Entity floor = MakeBody(scene_, TransformComponent{}, BodyType::Static);
    scene_.AddComponent(floor, MeshRendererComponent{.shape = Engine::Game::Shape::Plane});
    const Entity ball = MakeBody(scene_, TransformComponent{.y = 3.0f}, BodyType::Dynamic);
    RunSeconds(scene_, world_, PhysicsWorld::FixedStep);
    const auto box = scene_.GetComponent<PhysicsBodyComponent>(ball);
    ASSERT_TRUE(box.has_value());

    scene_.SetComponent(ball, RigidBodyComponent{.shape = Engine::Scene::ColliderShape::Sphere, .radius = 1.5f});
    RunSeconds(scene_, world_, 3.0);

    EXPECT_FALSE(world_.HasBody(box->body)) << "made again with the new collider";
    EXPECT_EQ(scene_.GetComponent<PhysicsBodyComponent>(ball)->collider.shape, Engine::Physics::ShapeType::Sphere);
    EXPECT_NEAR(HeightOf(scene_, ball), 1.5f, 0.05f);
}

// The RigidBody's Gravity Scale reaches its body: a dynamic body with 0 stays where it is.
TEST_F(PhysicsSystemTest, GravityScaleZeroFloats)
{
    const Entity balloon = scene_.CreateEntity();
    scene_.SetTransform(balloon, TransformComponent{.y = 5.0f});
    scene_.AddComponent(balloon, RigidBodyComponent{.type = BodyType::Dynamic, .gravityScale = 0.0f});

    RunSeconds(scene_, world_, 1.0);

    EXPECT_FLOAT_EQ(HeightOf(scene_, balloon), 5.0f);
}

// A cube landing on a plane is reported once, as the two entities, and each lists the other as touching.
TEST_F(PhysicsSystemTest, LandingIsReportedBetweenTheEntities)
{
    const auto [floor, cube] = MakeFloorAndCube(scene_, 2.0f);

    const std::vector<Collision> collisions = RunCollecting(scene_, world_, 2.0);

    ASSERT_EQ(collisions.size(), 1u);
    const Collision& hit = collisions[0];
    EXPECT_TRUE(hit.began);
    EXPECT_EQ(hit.a, cube) << "the one that moved comes first";
    EXPECT_EQ(hit.b, floor);
    EXPECT_GT(hit.impactSpeed, 3.0f);
    EXPECT_NEAR(hit.point.y, 0.0f, 0.05f);

    EXPECT_TRUE(PhysicsSystem::AreTouching(scene_, cube, floor));
    EXPECT_TRUE(PhysicsSystem::AreTouching(scene_, floor, cube));
    EXPECT_EQ(PhysicsSystem::Touching(scene_, cube), std::vector<Entity>{floor});
    EXPECT_EQ(PhysicsSystem::Touching(scene_, floor), std::vector<Entity>{cube});
}

// Moving a resting cube up off the plane (the Inspector, a block) ends the contact on both sides.
TEST_F(PhysicsSystemTest, MovingAwayStopsTouching)
{
    const auto [floor, cube] = MakeFloorAndCube(scene_, 0.6f);
    RunCollecting(scene_, world_, 3.0); // lands and goes to sleep

    scene_.SetTransform(cube, TransformComponent{.y = 5.0f});
    const std::vector<Collision> collisions = RunCollecting(scene_, world_, PhysicsWorld::FixedStep);

    ASSERT_EQ(collisions.size(), 1u);
    EXPECT_FALSE(collisions[0].began);
    EXPECT_FALSE(PhysicsSystem::AreTouching(scene_, cube, floor));
    EXPECT_TRUE(PhysicsSystem::Touching(scene_, floor).empty());
}

// Destroying an entity (RemoveBody first) takes it off the touching list of what it touched. The collision is not
// reported, since one side no longer exists.
TEST_F(PhysicsSystemTest, DestroyedEntitiesStopTouching)
{
    const auto [floor, cube] = MakeFloorAndCube(scene_, 0.6f);
    RunCollecting(scene_, world_, 1.0);
    ASSERT_TRUE(PhysicsSystem::AreTouching(scene_, floor, cube));

    PhysicsSystem::RemoveBody(scene_, world_, cube);
    scene_.DestroyEntity(cube);
    EXPECT_TRUE(PhysicsSystem::Touching(scene_, floor).empty()) << "dead entities are never listed";

    const std::vector<Collision> collisions = RunCollecting(scene_, world_, PhysicsWorld::FixedStep);
    EXPECT_TRUE(collisions.empty());
    EXPECT_TRUE(scene_.GetComponent<PhysicsBodyComponent>(floor)->touching.empty());
}

// Changing a resting cube's RigidBody makes its body again: the contact ends and begins again in the same frame,
// and it is still touching afterwards.
TEST_F(PhysicsSystemTest, RemadeBodiesKeepTouching)
{
    const auto [floor, cube] = MakeFloorAndCube(scene_, 0.6f);
    RunCollecting(scene_, world_, 1.0);

    RigidBodyComponent heavier = *scene_.GetComponent<RigidBodyComponent>(cube);
    heavier.mass = 5.0f;
    scene_.SetComponent(cube, heavier);
    RunCollecting(scene_, world_, 0.5);

    EXPECT_TRUE(PhysicsSystem::AreTouching(scene_, cube, floor));
    EXPECT_TRUE(PhysicsSystem::AreTouching(scene_, floor, cube));
}

// At the editor's frame rate a cube that has landed (and tilted a hair) keeps its exact size, so its collider still
// matches and its body is never made again: one landing, one collision.
TEST_F(PhysicsSystemTest, ALandedBodyKeepsItsSizeAndItsBody)
{
    const auto [floor, cube] = MakeFloorAndCube(scene_, 6.0f);
    std::vector<Collision> collisions;
    std::optional<Engine::Physics::BodyId> body;
    for (int frame = 0; frame < 144 * 6; ++frame)
    {
        for (const Collision& collision : PhysicsSystem::Update(scene_, world_, 1.0 / 144.0))
        {
            collisions.push_back(collision);
        }
        Engine::Systems::HierarchySystem::Update(scene_);
        const auto made = scene_.GetComponent<PhysicsBodyComponent>(cube);
        ASSERT_TRUE(made.has_value());
        if (!body)
        {
            body = made->body;
        }
        ASSERT_EQ(made->body, *body) << "made again at frame " << frame;
    }

    EXPECT_EQ(collisions.size(), 1u);
    const TransformComponent transform = *scene_.GetTransform(cube);
    EXPECT_EQ(transform.scale, 1.0f);
    EXPECT_EQ(transform.scaleX, 1.0f);
    EXPECT_EQ(transform.scaleY, 1.0f);
    EXPECT_EQ(transform.scaleZ, 1.0f);
}

// Entities are pushed through their bodies; one with no body yet is not, and says so.
TEST_F(PhysicsSystemTest, EntitiesArePushedThroughTheirBodies)
{
    world_.SetGravity({0.0f, 0.0f, 0.0f});
    const Entity box = MakeBody(scene_, TransformComponent{}, BodyType::Dynamic);
    EXPECT_FALSE(PhysicsSystem::AddImpulse(scene_, world_, box, {1.0f, 0.0f, 0.0f})) << "no body before Update";
    EXPECT_FALSE(PhysicsSystem::VelocityOf(scene_, world_, box).has_value());

    RunSeconds(scene_, world_, PhysicsWorld::FixedStep);
    EXPECT_TRUE(PhysicsSystem::AddImpulse(scene_, world_, box, {2.0f, 0.0f, 0.0f})); // mass 1: 2 units per second
    RunSeconds(scene_, world_, 1.0);

    EXPECT_NEAR(PhysicsSystem::VelocityOf(scene_, world_, box)->x, 2.0f, 1e-3f);
    EXPECT_NEAR(scene_.GetTransform(box)->x, 2.0f, 0.1f);
}

// Rays find entities by their colliders and can pass through the entity casting them. There is nothing to hit before
// the bodies are made (edit mode), or where a destroyed entity's body was left behind.
TEST_F(PhysicsSystemTest, RaysFindEntities)
{
    const auto [floor, cube] = MakeFloorAndCube(scene_, 3.0f);
    const glm::vec3 down(0.0f, -1.0f, 0.0f);
    EXPECT_FALSE(PhysicsSystem::Raycast(scene_, world_, {0.0f, 10.0f, 0.0f}, down, 20.0f).has_value());

    PhysicsSystem::Update(scene_, world_, 0.0); // Play starts: the bodies are made
    const auto top = PhysicsSystem::Raycast(scene_, world_, {0.0f, 10.0f, 0.0f}, down, 20.0f);
    ASSERT_TRUE(top.has_value());
    EXPECT_EQ(top->entity, cube);
    EXPECT_NEAR(top->distance, 6.5f, 1e-3f);

    // From the cube's centre: its own collider, unless the ray passes through it to the plane
    const auto self = PhysicsSystem::Raycast(scene_, world_, {0.0f, 3.0f, 0.0f}, down, 20.0f);
    ASSERT_TRUE(self.has_value());
    EXPECT_EQ(self->entity, cube);
    EXPECT_EQ(self->distance, 0.0f);
    const auto ground = PhysicsSystem::Raycast(scene_, world_, {0.0f, 3.0f, 0.0f}, down, 20.0f, cube);
    ASSERT_TRUE(ground.has_value());
    EXPECT_EQ(ground->entity, floor);
    EXPECT_NEAR(ground->distance, 3.0f, 1e-3f);
    EXPECT_NEAR(ground->point.y, 0.0f, 1e-3f);
    EXPECT_NEAR(ground->normal.y, 1.0f, 1e-3f);

    scene_.DestroyEntity(cube); // without RemoveBody: its body stays in the world
    EXPECT_FALSE(PhysicsSystem::Raycast(scene_, world_, {0.0f, 10.0f, 0.0f}, down, 20.0f).has_value());
}

// Entities are measured collider to collider and found near a point, the asking entity left out when given. Nothing
// is found before the bodies are made, or for a body a destroyed entity left behind.
TEST_F(PhysicsSystemTest, DistancesBetweenEntities)
{
    const auto [floor, cube] = MakeFloorAndCube(scene_, 3.0f); // the cube's bottom 2.5 above the plane
    EXPECT_FALSE(PhysicsSystem::DistanceBetween(scene_, world_, cube, floor).has_value());
    EXPECT_TRUE(PhysicsSystem::EntitiesWithin(scene_, world_, {0.0f, 3.0f, 0.0f}, 5.0f).empty());

    PhysicsSystem::Update(scene_, world_, 0.0); // Play starts: the bodies are made
    const auto gap = PhysicsSystem::DistanceBetween(scene_, world_, cube, floor);
    ASSERT_TRUE(gap.has_value());
    EXPECT_NEAR(gap->distance, 2.5f, 1e-3f);
    EXPECT_NEAR(gap->pointOnA.y, 2.5f, 1e-3f);
    EXPECT_NEAR(gap->pointOnB.y, 0.0f, 1e-3f);
    EXPECT_FALSE(PhysicsSystem::DistanceBetween(scene_, world_, cube, cube).has_value());

    // From the cube's centre: itself first (the point is inside it), then the plane
    const auto all = PhysicsSystem::EntitiesWithin(scene_, world_, {0.0f, 3.0f, 0.0f}, 5.0f);
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0].entity, cube);
    EXPECT_EQ(all[0].distance, 0.0f);
    EXPECT_EQ(all[1].entity, floor);
    EXPECT_NEAR(all[1].distance, 3.0f, 1e-3f);
    const auto others = PhysicsSystem::EntitiesWithin(scene_, world_, {0.0f, 3.0f, 0.0f}, 5.0f, cube);
    ASSERT_EQ(others.size(), 1u);
    EXPECT_EQ(others[0].entity, floor);

    scene_.DestroyEntity(cube); // without RemoveBody: its body stays in the world
    const auto left = PhysicsSystem::EntitiesWithin(scene_, world_, {0.0f, 3.0f, 0.0f}, 5.0f);
    ASSERT_EQ(left.size(), 1u);
    EXPECT_EQ(left[0].entity, floor);
}

// An entity is on the ground once it lands, on the entity it landed on; not before its body is made, nor while it falls.
TEST_F(PhysicsSystemTest, EntitiesStandOnTheGround)
{
    const auto [floor, cube] = MakeFloorAndCube(scene_, 3.0f);
    EXPECT_FALSE(PhysicsSystem::IsOnGround(scene_, world_, cube)) << "no body yet";

    RunSeconds(scene_, world_, 0.3);
    EXPECT_FALSE(PhysicsSystem::IsOnGround(scene_, world_, cube)) << "falling";

    RunSeconds(scene_, world_, 2.0);
    const auto ground = PhysicsSystem::GroundUnder(scene_, world_, cube);
    ASSERT_TRUE(ground.has_value());
    EXPECT_EQ(ground->entity, floor);
    EXPECT_NEAR(ground->normal.y, 1.0f, 1e-3f);
    EXPECT_FALSE(PhysicsSystem::IsOnGround(scene_, world_, floor));

    scene_.DestroyEntity(floor); // without RemoveBody: its body stays in the world, but no entity stands on it
    EXPECT_FALSE(PhysicsSystem::IsOnGround(scene_, world_, cube));
}

// ---------------------------------------------------------------- Mesh and compound colliders of an entity

TEST(PhysicsColliders, ACompoundIsMadeOfItsParts)
{
    Engine::Scene::RigidBodyComponent body;
    body.shape = Engine::Scene::ColliderShape::Compound;
    body.parts = {{.centerX = -2.0f}, {.shape = Engine::Scene::ColliderShape::Sphere, .centerX = 2.0f, .radius = 0.5f}};
    Engine::Scene::TransformComponent transform;
    transform.scale = 2.0f;

    const auto collider = Engine::Systems::PhysicsSystem::ColliderFor(transform, nullptr, body);

    ASSERT_EQ(collider.shape, Engine::Physics::ShapeType::Compound);
    ASSERT_EQ(collider.parts.size(), 2u);
    EXPECT_EQ(collider.parts[0].shape, Engine::Physics::ShapeType::Box);
    EXPECT_FLOAT_EQ(collider.parts[0].center.x, -4.0f) << "scaled with the object";
    EXPECT_EQ(collider.parts[1].shape, Engine::Physics::ShapeType::Sphere);
    EXPECT_FLOAT_EQ(collider.parts[1].radius, 1.0f);
}

TEST(PhysicsColliders, ACompoundWithNoPartsFitsTheObjectLikeAuto)
{
    Engine::Scene::RigidBodyComponent body;
    body.shape = Engine::Scene::ColliderShape::Compound;
    const auto collider = Engine::Systems::PhysicsSystem::ColliderFor({}, nullptr, body);
    EXPECT_EQ(collider.shape, Engine::Physics::ShapeType::Box);
}

TEST(PhysicsColliders, AMeshColliderWithNothingDrawnHasNoTriangles)
{
    Engine::Assets::ResourceManager resources;
    const auto none = Engine::Systems::PhysicsSystem::MeshColliderFor({}, nullptr, resources);
    EXPECT_TRUE(none.triangles.empty());
    const Engine::Scene::MeshRendererComponent renderer;
    EXPECT_TRUE(Engine::Systems::PhysicsSystem::MeshColliderFor({}, &renderer, resources).triangles.empty());
}
