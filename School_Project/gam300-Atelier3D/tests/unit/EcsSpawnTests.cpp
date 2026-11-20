#include "engine/input/GameInput.hpp"
#include "engine/scene/EntitySpawner.hpp"
#include "engine/scene/Scene.hpp"
#include "engine/scene/systems/PlayerControllerSystem.hpp"
#include "engine/scene/systems/SpinSystem.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <set>
#include <tuple>
#include <unordered_set>
#include <utility>

// Spawning entities at runtime (Scene::SpawnCopies, behind the editor's "Spawn 1,000 copies" button, which copies
// the selected object's MeshRenderer and spin) and the 1,000-entity frame budget.

namespace
{
    using namespace Engine::Scene;
    using namespace Engine::Input;
    using Engine::Systems::PlayerControllerSystem;
    using Engine::Systems::SpinSystem;

    constexpr std::size_t kThousand = 1000;

    // What a spinning, textured object's copies get
    SpawnOptions Copies(std::size_t count = kThousand)
    {
        SpawnOptions options;
        options.count = count;
        options.meshRenderer.shape = Engine::Game::Shape::Cube;
        options.meshRenderer.albedoR = 0.5f;
        options.spinDegreesPerSecond = 60.0f;
        return options;
    }

    // Spawning 1,000 copies gives 1,000 live entities with unique ids, each with Transform, MeshRenderer (the template's
    // tint) and Spin, so RenderSystem would draw all of them.
    TEST(EcsSpawn, ThousandEntitiesGetUniqueIdsAndRenderableComponents)
    {
        Scene scene;
        const auto spawned = SpawnCopies(scene, Copies());
        ASSERT_EQ(spawned.size(), kThousand);
        EXPECT_EQ(scene.EntityCount(), kThousand);

        std::unordered_set<Entity::Id> ids;
        for (const Entity entity : spawned)
        {
            EXPECT_TRUE(scene.IsAlive(entity));
            EXPECT_TRUE(ids.insert(entity.GetId()).second) << "duplicate id " << entity.GetId();
            EXPECT_TRUE(scene.HasComponent<TransformComponent>(entity));
            EXPECT_TRUE(scene.HasComponent<MeshRendererComponent>(entity));
            EXPECT_TRUE(scene.HasComponent<SpinComponent>(entity));
            EXPECT_FLOAT_EQ(scene.GetComponent<MeshRendererComponent>(entity)->albedoR, 0.5f); // the template's
        }
        // What RenderSystem::Draw iterates
        EXPECT_EQ((scene.Count<TransformComponent, MeshRendererComponent>()), kThousand);
        EXPECT_EQ((scene.Count<TransformComponent, SpinComponent>()), kThousand);
    }

    // No two copies share a spot; the block is centred on the template in X and starts one spacing above it, and every copy
    // keeps the template's rotation and scale.
    TEST(EcsSpawn, CopiesFillABlockAboveTheTemplateKeepingItsRotationAndScale)
    {
        Scene scene;
        SpawnOptions options = Copies();
        options.spacing = 2.0f;
        options.origin.x = 10.0f;
        options.origin.y = 1.0f;
        options.origin.rotation = 30.0f;
        options.origin.scale = 0.25f;
        const auto spawned = SpawnCopies(scene, options);

        std::set<std::tuple<float, float, float>> cells;
        float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
        for (const Entity entity : spawned)
        {
            const auto transform = scene.GetTransform(entity);
            ASSERT_TRUE(transform.has_value());
            EXPECT_FLOAT_EQ(transform->rotation, 30.0f);
            EXPECT_FLOAT_EQ(transform->scale, 0.25f);
            EXPECT_TRUE(cells.emplace(transform->x, transform->y, transform->z).second) << "two copies share a place";
            minX = std::min(minX, transform->x);
            maxX = std::max(maxX, transform->x);
            minY = std::min(minY, transform->y);
            maxY = std::max(maxY, transform->y);
        }
        EXPECT_EQ(cells.size(), kThousand);
        EXPECT_FLOAT_EQ(minX, 1.0f); // 10 wide, 2 apart, centred on x = 10: 1 .. 19
        EXPECT_FLOAT_EQ(maxX, 19.0f);
        EXPECT_FLOAT_EQ(minY, 3.0f); // 10 layers rising from one spacing above the template: 3 .. 21
        EXPECT_FLOAT_EQ(maxY, 21.0f);
    }

    // Pressing spawn again (firstIndex = 100) puts the new copies on the next layer up instead of on top of the first
    // batch.
    TEST(EcsSpawn, ALaterBatchContinuesTheBlockUpwards)
    {
        Scene scene;
        SpawnOptions options = Copies(100); // one layer
        SpawnCopies(scene, options);
        options.firstIndex = 100;
        const auto second = SpawnCopies(scene, options);
        for (const Entity entity : second)
            EXPECT_FLOAT_EQ(scene.GetTransform(entity)->y, 2.0f * options.spacing);
    }

    // count = 0 spawns nothing; spin speed 0 means no SpinComponent; the shape comes from the template's MeshRenderer.
    TEST(EcsSpawn, OptionsControlCountShapeAndSpin)
    {
        Scene scene;
        EXPECT_TRUE(SpawnCopies(scene, Copies(0)).empty());
        EXPECT_EQ(scene.EntityCount(), 0u);

        SpawnOptions options = Copies(7);
        options.spinDegreesPerSecond = 0.0f;
        options.meshRenderer.shape = Engine::Game::Shape::Sphere;
        const auto spawned = SpawnCopies(scene, options);
        ASSERT_EQ(spawned.size(), 7u);
        for (const Entity entity : spawned)
        {
            EXPECT_FALSE(scene.HasComponent<SpinComponent>(entity));
            ASSERT_TRUE(scene.GetComponent<MeshRendererComponent>(entity).has_value());
            EXPECT_EQ(scene.GetComponent<MeshRendererComponent>(entity)->shape, Engine::Game::Shape::Sphere);
        }
    }

    // Spawn, destroy and spawn again: the entity that was already there is untouched and no id is ever reused. DestroyAll
    // on already-destroyed entities just skips them.
    TEST(EcsSpawn, SpawningKeepsExistingEntitiesAndNeverReusesTheirIds)
    {
        Scene scene;
        const auto existing = scene.CreateEntity();
        TransformComponent placed;
        placed.x = 42.0f;
        ASSERT_TRUE(scene.SetTransform(existing, placed));

        const auto first = SpawnCopies(scene, Copies());
        EXPECT_EQ(DestroyAll(scene, first), kThousand);
        EXPECT_EQ(DestroyAll(scene, first), 0u); // already destroyed: skipped
        EXPECT_EQ(scene.EntityCount(), 1u);

        const auto second = SpawnCopies(scene, Copies());
        std::unordered_set<Entity::Id> ids{existing.GetId()};
        for (const Entity entity : first)
            ids.insert(entity.GetId());
        for (const Entity entity : second)
            EXPECT_TRUE(ids.insert(entity.GetId()).second) << "id reused: " << entity.GetId();

        EXPECT_EQ(scene.EntityCount(), kThousand + 1);
        EXPECT_TRUE(scene.IsAlive(existing));
        EXPECT_EQ(*scene.GetTransform(existing), placed);
        EXPECT_FALSE(scene.HasComponent<MeshRendererComponent>(existing));
    }

    // Spawning from inside a ForEach throws and leaves the scene as it was; spawning afterwards works.
    TEST(EcsSpawn, CannotSpawnWhileASystemIsIterating)
    {
        Scene scene;
        scene.CreateEntity();
        EXPECT_THROW(
            scene.ForEach<TransformComponent>([&](Entity, TransformComponent&) { SpawnCopies(scene, Copies()); }),
            std::logic_error);
        EXPECT_EQ(scene.EntityCount(), 1u);
        EXPECT_EQ(SpawnCopies(scene, Copies(3)).size(), 3u);
    }

    // SpinSystem turns all 1,000 copies; remove Spin from half and only the other half keeps turning, add it back and they
    // all turn again.
    TEST(EcsSpawn, SystemsReactToComponentsAddedAndRemovedOnSpawnedEntities)
    {
        Scene scene;
        const auto spawned = SpawnCopies(scene, Copies());
        SpinSystem::Update(scene, 0.5);
        for (const Entity entity : spawned)
            EXPECT_FLOAT_EQ(scene.GetTransform(entity)->rotation, 30.0f);

        // Remove Spin from every other entity: only the rest keep turning, from the very next update
        for (std::size_t i = 0; i < spawned.size(); i += 2)
            ASSERT_TRUE(scene.RemoveComponent<SpinComponent>(spawned[i]));
        SpinSystem::Update(scene, 0.5);
        for (std::size_t i = 0; i < spawned.size(); ++i)
            EXPECT_FLOAT_EQ(scene.GetTransform(spawned[i])->rotation, i % 2 == 0 ? 30.0f : 60.0f);

        for (std::size_t i = 0; i < spawned.size(); i += 2)
            ASSERT_TRUE(scene.AddComponent<SpinComponent>(spawned[i]));
        SpinSystem::Update(scene, 0.5);
        for (std::size_t i = 0; i < spawned.size(); ++i)
            EXPECT_FLOAT_EQ(scene.GetTransform(spawned[i])->rotation, i % 2 == 0 ? 60.0f : 90.0f);
    }

    // The CPU side of "1,000 entities at 60 FPS": one frame of every non-GPU system over 1,000 entities has to fit in
    // a 60 FPS frame (16.67 ms). It normally takes well under a millisecond, so the budget only fails on a real
    // regression (e.g. a system going quadratic), not on a slow CI machine. Drawing is measured in the editor
    // (Diagnostics > ECS shows the frame rate).
    TEST(EcsSpawn, DISABLED_ThousandEntityFrameFitsThe60FpsBudget)
    {
        Scene scene;
        const auto spawned = SpawnCopies(scene, Copies());
        for (const Entity entity : spawned)
            ASSERT_TRUE(scene.AddComponent<PlayerControllerComponent>(entity));

        InputSystem input;
        const auto actions = InputActions::Defaults();
        input.SetKey(Key_W, KeyState::Down); // so PlayerControllerSystem moves every entity each frame
        const GameInput gameInput(input, actions);

        constexpr int kFrames = 120;
        constexpr double kDelta = 1.0 / 60.0;
        std::size_t visited = 0;
        const auto start = std::chrono::steady_clock::now();
        for (int frame = 0; frame < kFrames; ++frame)
        {
            SpinSystem::Update(scene, kDelta);
            PlayerControllerSystem::Update(scene, gameInput, kDelta);
            // The query RenderSystem::Draw runs before issuing draw calls
            scene.ForEach<const TransformComponent, const MeshRendererComponent>(
                [&visited](Entity, const TransformComponent&, const MeshRendererComponent& mesh)
                { visited += mesh.visible; });
        }
        const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start;
        const double perFrameMs = elapsed.count() / kFrames;

        EXPECT_EQ(visited, kThousand * kFrames);
        EXPECT_NEAR(scene.GetTransform(spawned.front())->rotation, 120.0f, 0.01f); // 2 s at 60 deg/s
        RecordProperty("ms_per_frame", std::to_string(perFrameMs));
        EXPECT_LT(perFrameMs, 1000.0 / 60.0) << "1,000-entity system update took " << perFrameMs << " ms per frame";
    }

    // Spawning itself must not hitch the frame it happens on
    TEST(EcsSpawn, DISABLED_SpawningAThousandEntitiesFitsInOneFrame)
    {
        Scene scene;
        const auto start = std::chrono::steady_clock::now();
        const auto spawned = SpawnCopies(scene, Copies());
        const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - start;
        EXPECT_EQ(spawned.size(), kThousand);
        EXPECT_LT(elapsed.count(), 1000.0 / 60.0) << "spawning took " << elapsed.count() << " ms";
    }
} // namespace
