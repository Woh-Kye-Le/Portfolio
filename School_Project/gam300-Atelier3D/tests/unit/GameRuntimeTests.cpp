#include "engine/game/GameProject.hpp"
#include "engine/game/GameRuntime.hpp"
#include "engine/graphics/DebugLines.hpp"
#include "engine/scene/systems/PhysicsSystem.hpp"

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

// GameRuntime turns a scene description into ECS entities and takes edits while it runs (without graphics: these
// tests never draw a frame)

using Engine::Game::GameRuntime;
using Engine::Game::Shape;
using Engine::Scene::GameObject;
using Engine::Scene::SceneDescription;

namespace
{
    class GameRuntimeTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            dir_ = std::filesystem::temp_directory_path() / "atelier_runtime_tests" / info->name();
            std::filesystem::remove_all(dir_);
            std::filesystem::create_directories(dir_ / "configs");
            std::ofstream(dir_ / "configs" / "game.cfg") << "name=runtime_test\n";
            runtime_.SetSimulating(false);
            runtime_.LoadProject(Engine::Game::GameProject(dir_));
        }

        void TearDown() override
        {
            runtime_.Stop();
            std::filesystem::remove_all(dir_);
        }

        static GameObject Box(std::uint32_t id)
        {
            GameObject box{.id = id, .name = "Box " + std::to_string(id)};
            box.transform.emplace();
            box.renderable = Engine::Scene::RenderableDesc{.shape = Shape::Cube, .rotationSpeed = 45.0f};
            return box;
        }

        std::size_t Drawn()
        {
            return runtime_.Ecs()->Count<Engine::Scene::TransformComponent, Engine::Scene::MeshRendererComponent>();
        }

        std::filesystem::path dir_;
        GameRuntime runtime_;
    };
}

// Every scene object becomes an entity (even an empty one), but only the box gets drawn and nothing is a player.
TEST_F(GameRuntimeTest, EveryObjectBecomesAnEntityWithTheComponentsItHas)
{
    GameObject light{.id = 2, .name = "Lamp"};
    light.transform.emplace();
    light.light.emplace();
    ASSERT_TRUE(runtime_.Start(SceneDescription{.objects = {Box(1), light, GameObject{.id = 3, .name = "Empty"}}}));

    EXPECT_EQ(runtime_.EntityCount(), 3u);
    EXPECT_EQ(Drawn(), 1u);
    EXPECT_FALSE(runtime_.HasPlayer());
}

// Live edits while running: adding a Player and a second box shows up straight away, and removing them again does too.
TEST_F(GameRuntimeTest, EditsAddAndRemoveComponentsOnTheRunningEntities)
{
    const SceneDescription first{.objects = {Box(1)}};
    ASSERT_TRUE(runtime_.Start(first));
    EXPECT_EQ(runtime_.Scene(), first);

    // Add a Player to the box and a new drawn object
    SceneDescription second = first;
    second.objects[0].player.emplace();
    second.objects.push_back(Box(2));
    runtime_.UpdateScene(second);
    EXPECT_EQ(runtime_.Scene(), second);
    EXPECT_TRUE(runtime_.HasPlayer());
    EXPECT_EQ(runtime_.EntityCount(), 2u);
    EXPECT_EQ(Drawn(), 2u);

    // Remove the first box's Renderable and Player, and the second object
    SceneDescription third = first;
    third.objects[0].renderable.reset();
    runtime_.UpdateScene(third);
    EXPECT_FALSE(runtime_.HasPlayer());
    EXPECT_EQ(runtime_.EntityCount(), 1u);
    EXPECT_EQ(Drawn(), 0u);
}

// Changing one box's tint edits its entity in place - the entity ids stay the same, nothing is rebuilt.
TEST_F(GameRuntimeTest, AnObjectThatDidNotChangeKeepsItsEntity)
{
    ASSERT_TRUE(runtime_.Start(SceneDescription{.objects = {Box(1), Box(2)}}));
    const auto ids = [this]
    {
        std::vector<Engine::Scene::Entity::Id> found;
        runtime_.Ecs()->ForEach<Engine::Scene::MeshRendererComponent>(
            [&found](Engine::Scene::Entity entity, Engine::Scene::MeshRendererComponent&)
            { found.push_back(entity.GetId()); });
        std::sort(found.begin(), found.end());
        return found;
    };
    const auto before = ids();

    SceneDescription edited{.objects = {Box(1), Box(2)}};
    edited.objects[1].renderable->albedoG = 0.25f;
    runtime_.UpdateScene(edited);

    EXPECT_EQ(ids(), before); // edited in place, not rebuilt
}

// 1,000 copies of the box get drawn and spin like it; spawning from an object with nothing to draw or a wrong id
// throws; Clear removes exactly the copies.
TEST_F(GameRuntimeTest, SpawnsCopiesOfAnObjectAndClearsThem)
{
    GameObject empty{.id = 2, .name = "Empty"};
    empty.transform.emplace();
    ASSERT_TRUE(runtime_.Start(SceneDescription{.objects = {Box(1), empty}}));

    EXPECT_EQ(runtime_.SpawnEntities(1000, 1), 1000u);
    EXPECT_EQ(runtime_.SpawnedEntityCount(), 1000u);
    EXPECT_EQ(runtime_.EntityCount(), 1002u);
    EXPECT_EQ(Drawn(), 1001u);
    EXPECT_EQ((runtime_.Ecs()->Count<Engine::Scene::TransformComponent, Engine::Scene::SpinComponent>()),
              1001u); // the copies spin like the box

    EXPECT_THROW(runtime_.SpawnEntities(1000, 2), std::invalid_argument);  // nothing to draw
    EXPECT_THROW(runtime_.SpawnEntities(1000, 99), std::invalid_argument); // no such object

    EXPECT_EQ(runtime_.ClearSpawnedEntities(), 1000u);
    EXPECT_EQ(runtime_.EntityCount(), 2u);
}

// LiveObject (the Variable Inspector's values) reads the running game, not the scene it started from: a child's
// Transform is the one relative to its parent, a component the object does not have stays empty, and an unknown
// object has none.
TEST_F(GameRuntimeTest, LiveObjectFollowsTheRunningGame)
{
    GameObject box = Box(1);
    box.transform->x = 2.0f;
    box.light = Engine::Scene::LightComponent{.intensity = 80.0f};
    GameObject child{.id = 2, .name = "Hat", .parent = 1};
    child.transform = Engine::Scene::TransformComponent{.y = 1.0f};
    ASSERT_TRUE(runtime_.Start(SceneDescription{.objects = {box, child}}));

    const auto boxNow = runtime_.LiveObject(1);
    ASSERT_TRUE(boxNow.has_value());
    ASSERT_TRUE(boxNow->transform && boxNow->renderable && boxNow->light);
    EXPECT_EQ(boxNow->name, "Box 1");
    EXPECT_FLOAT_EQ(boxNow->transform->x, 2.0f);
    EXPECT_FLOAT_EQ(boxNow->light->intensity, 80.0f);

    const auto childNow = runtime_.LiveObject(2);
    ASSERT_TRUE(childNow && childNow->transform);
    EXPECT_FLOAT_EQ(childNow->transform->x, 0.0f); // relative to the box, not at x 2 in the world
    EXPECT_FLOAT_EQ(childNow->transform->y, 1.0f);
    EXPECT_FALSE(childNow->renderable.has_value());
    EXPECT_FALSE(childNow->light.has_value());
    EXPECT_FALSE(runtime_.LiveObject(99).has_value());

    // The game changes them: LiveObject shows the change, the scene description does not
    runtime_.Ecs()->ForEach<Engine::Scene::LightComponent>(
        [](Engine::Scene::Entity, Engine::Scene::LightComponent& light) { light.intensity = 40.0f; });
    runtime_.Ecs()->ForEach<Engine::Scene::MeshRendererComponent>(
        [](Engine::Scene::Entity, Engine::Scene::MeshRendererComponent& renderer) { renderer.albedoG = 0.25f; });
    EXPECT_FLOAT_EQ(runtime_.LiveObject(1)->light->intensity, 40.0f);
    EXPECT_FLOAT_EQ(runtime_.LiveObject(1)->renderable->albedoG, 0.25f);
    EXPECT_FLOAT_EQ(runtime_.Scene().objects[0].light->intensity, 80.0f);
    EXPECT_FLOAT_EQ(runtime_.Scene().objects[0].renderable->albedoG, 1.0f);

    runtime_.Stop();
    EXPECT_FALSE(runtime_.LiveObject(1).has_value());
}

// An object with every component that has just started is the object it was saved as. A component the runtime does
// not add, or does not read back, is a difference here: this is what keeps the component list in GameRuntime.cpp
// complete.
TEST_F(GameRuntimeTest, LiveObjectOfAFreshGameIsTheSavedObject)
{
    GameObject full = Box(1);
    full.transform->x = 2.0f;
    full.transform->rotation = 30.0f;
    full.transform->scale = 1.5f;
    full.renderable->albedoR = 0.2f;
    full.renderable->albedoA = 0.5f;
    full.audioSource = Engine::Scene::AudioSourceDesc{.volume = 0.5f};
    full.player = Engine::Scene::PlayerControllerComponent{.speed = 7.0f};
    full.camera = Engine::Scene::CameraComponent{.fieldOfView = 50.0f};
    full.light = Engine::Scene::LightComponent{.intensity = 2.0f};
    full.rigidBody = Engine::Scene::RigidBodyComponent{
        .mass = 3.0f, .gravityScale = 0.5f, .friction = 0.2f, .bounciness = 0.8f, .startVelocityY = 4.0f};
    ASSERT_TRUE(runtime_.Start(SceneDescription{.objects = {full}}));

    const auto live = runtime_.LiveObject(1);
    ASSERT_TRUE(live.has_value());
    EXPECT_EQ(*live, full);
}

// The game's gravity (configs/game.cfg) is the physics world's when the game starts.
TEST_F(GameRuntimeTest, PhysicsUsesTheGamesGravity)
{
    std::ofstream(dir_ / "configs" / "game.cfg") << "name=runtime_test\ngravity_y=-1.62\n";
    runtime_.LoadProject(Engine::Game::GameProject(dir_));
    ASSERT_TRUE(runtime_.Start(SceneDescription{.objects = {Box(1)}}));
    EXPECT_TRUE(runtime_.Physics().IsInitialized());
    EXPECT_FLOAT_EQ(runtime_.Physics().Gravity().y, -1.62f);
}

// The Physics window asks these every editor frame, also while paused when the world never moves on: they only answer,
// or the rays would pile up for the Raycasts debug lines to draw.
TEST_F(GameRuntimeTest, TheQueriesThemselvesLogNoRays)
{
    runtime_.SetSimulating(true);
    ASSERT_TRUE(runtime_.Start(SceneDescription{.objects = {Box(1)}}));
    for (int i = 0; i < 1000; ++i)
    {
        runtime_.Raycast({0.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 5.0f);
        runtime_.GroundUnder(runtime_.EntityOf(1));
    }
    EXPECT_TRUE(runtime_.CastRays().empty());
}

// A ray game code records is kept for the Raycasts debug lines. When the world moves on it becomes the last frame's, which
// is what shows while paused, and the frame after that it is gone. Stopping the game drops them all.
TEST_F(GameRuntimeTest, RecordedRaysLastOneFrameOfTheWorld)
{
    runtime_.SetSimulating(true);
    ASSERT_TRUE(runtime_.Start(SceneDescription{.objects = {Box(1)}}));
    const Engine::Systems::CastRay ray{{1.0f, 2.0f, 3.0f}, {0.0f, 0.0f, -1.0f}, 10.0f, std::nullopt};

    runtime_.RecordRay(ray);
    ASSERT_EQ(runtime_.CastRays().size(), 1u);
    EXPECT_EQ(runtime_.CastRays().front().origin, ray.origin);
    EXPECT_TRUE(runtime_.LastCastRays().empty());

    runtime_.EndRayFrame();
    EXPECT_TRUE(runtime_.CastRays().empty());
    EXPECT_EQ(runtime_.LastCastRays().size(), 1u);

    runtime_.EndRayFrame();
    EXPECT_TRUE(runtime_.LastCastRays().empty());

    runtime_.RecordRay(ray);
    runtime_.EndRayFrame();
    runtime_.RecordRay(ray);
    runtime_.Stop();
    EXPECT_TRUE(runtime_.CastRays().empty());
    EXPECT_TRUE(runtime_.LastCastRays().empty());
}

// However many rays the game casts in a frame, the debug lines keep a bounded number.
TEST_F(GameRuntimeTest, RecordedRaysStopAtTheCap)
{
    runtime_.SetSimulating(true);
    ASSERT_TRUE(runtime_.Start(SceneDescription{.objects = {Box(1)}}));
    for (std::size_t i = 0; i < Engine::Systems::DebugLineSystem::kMaxCastRays + 50; ++i)
    {
        runtime_.RecordRay({{0.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 1.0f, std::nullopt});
    }
    EXPECT_EQ(runtime_.CastRays().size(), Engine::Systems::DebugLineSystem::kMaxCastRays);
}

// The Variable Inspector shows a body's velocity: none before the body exists (it is made on the first frame), then
// falling once the world has stepped, and the same until it steps again (which is what a pause relies on).
TEST_F(GameRuntimeTest, AFallingBodyHasAVelocityToShow)
{
    runtime_.SetSimulating(true);
    GameObject box = Box(1);
    box.transform->y = 10.0f;
    box.rigidBody = Engine::Scene::RigidBodyComponent{};
    ASSERT_TRUE(runtime_.Start(SceneDescription{.objects = {box}}));
    const Engine::Scene::Entity entity = runtime_.EntityOf(1);
    EXPECT_FALSE(runtime_.VelocityOf(entity).has_value());

    Engine::Systems::PhysicsSystem::Update(*runtime_.Ecs(), runtime_.Physics(), 0.5);
    const auto falling = runtime_.VelocityOf(entity);
    ASSERT_TRUE(falling.has_value());
    EXPECT_LT(falling->y, -0.5f); // one update runs at most 4 steps of 1/60 s: about 0.65 units per second down

    EXPECT_EQ(runtime_.VelocityOf(entity), falling); // nothing stepped: nothing changed
}

// Edit mode casts no rays to show.
TEST_F(GameRuntimeTest, NoRaysAreKeptWhileNotPlaying)
{
    ASSERT_TRUE(runtime_.Start(SceneDescription{.objects = {Box(1)}})); // SetUp made it not simulate
    runtime_.RecordRay({{0.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 5.0f, std::nullopt});
    runtime_.CastRay({0.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 5.0f);
    EXPECT_TRUE(runtime_.CastRays().empty());
}

// CastRay answers as Raycast does, and each ray it casts gets its debug line: a hit up to the wall in front (at z -5, its
// near face at -4.5), a miss when too short or told to ignore the wall.
TEST_F(GameRuntimeTest, CastRayAnswersAndKeepsItsLine)
{
    runtime_.SetSimulating(true);
    GameObject wall = Box(1);
    wall.transform->z = -5.0f;
    wall.rigidBody = Engine::Scene::RigidBodyComponent{.type = Engine::Scene::BodyType::Static};
    ASSERT_TRUE(runtime_.Start(SceneDescription{.objects = {wall}}));
    const Engine::Scene::Entity entity = runtime_.EntityOf(1);
    Engine::Systems::PhysicsSystem::Update(*runtime_.Ecs(), runtime_.Physics(), 1.0 / 60.0); // makes the body

    const glm::vec3 origin(0.0f, 0.0f, 0.0f);
    const glm::vec3 forward(0.0f, 0.0f, -1.0f);
    const auto hit = runtime_.CastRay(origin, forward, 10.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->entity, entity);
    EXPECT_NEAR(hit->distance, 4.5f, 0.01f);
    ASSERT_EQ(runtime_.CastRays().size(), 1u);
    EXPECT_EQ(runtime_.CastRays()[0].origin, origin);
    EXPECT_EQ(runtime_.CastRays()[0].direction, forward);
    EXPECT_FLOAT_EQ(runtime_.CastRays()[0].length, 10.0f);
    ASSERT_TRUE(runtime_.CastRays()[0].hit.has_value());
    EXPECT_NEAR(runtime_.CastRays()[0].hit->z, -4.5f, 0.01f);

    EXPECT_FALSE(runtime_.CastRay(origin, forward, 2.0f).has_value());
    ASSERT_EQ(runtime_.CastRays().size(), 2u);
    EXPECT_FALSE(runtime_.CastRays()[1].hit.has_value());

    EXPECT_FALSE(runtime_.CastRay(origin, forward, 10.0f, entity).has_value());
    ASSERT_EQ(runtime_.CastRays().size(), 3u);
    EXPECT_FALSE(runtime_.CastRays()[2].hit.has_value());

    // Any length of direction will do: the line still gets a unit one, as its drawing expects
    const auto longer = runtime_.CastRay(origin, forward * 5.0f, 10.0f);
    ASSERT_TRUE(longer.has_value());
    EXPECT_NEAR(longer->distance, 4.5f, 0.01f);
    ASSERT_EQ(runtime_.CastRays().size(), 4u);
    EXPECT_EQ(runtime_.CastRays()[3].direction, forward);
}

// A ray of no length or no direction looks at nothing, so it has no line either.
TEST_F(GameRuntimeTest, CastRayWithNothingToLookAlongKeepsNoLine)
{
    runtime_.SetSimulating(true);
    ASSERT_TRUE(runtime_.Start(SceneDescription{.objects = {Box(1)}}));
    const glm::vec3 origin(0.0f, 5.0f, 0.0f);
    const glm::vec3 down(0.0f, -1.0f, 0.0f);
    EXPECT_FALSE(runtime_.CastRay(origin, down, 0.0f).has_value());
    EXPECT_FALSE(runtime_.CastRay(origin, down, -3.0f).has_value());
    EXPECT_FALSE(runtime_.CastRay(origin, glm::vec3(0.0f), 5.0f).has_value());
    EXPECT_TRUE(runtime_.CastRays().empty());
}
