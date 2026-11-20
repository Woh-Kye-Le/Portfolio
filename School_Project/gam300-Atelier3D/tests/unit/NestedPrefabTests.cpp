#include "engine/editor/EditorScene.hpp"
#include "engine/game/PrefabManager.hpp"
#include "engine/scene/Hierarchy.hpp"
#include "engine/scene/Prefab.hpp"
#include "engine/scene/PrefabInstance.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

// Nested prefabs: a prefab with whole instances of other prefabs under its root. A change to the inner prefab reaches
// every instance of the outer one, except what the outer prefab (or the instance) changed, and no prefab may contain
// itself.

using Engine::Editor::EditorScene;
using Engine::Game::PrefabManager;
using Engine::Game::Shape;
using Engine::Scene::FindObject;
using Engine::Scene::GameObject;
using Engine::Scene::LightComponent;
// What the prefabs are told apart by in these tests: a part with two numbers
static LightComponent PrefabPart(float first, float second)
{
    LightComponent part;
    part.intensity = first;
    part.range = second;
    return part;
}
using Engine::Scene::kPrefabRootId;
using Engine::Scene::Prefab;
using Engine::Scene::SceneDescription;
using Engine::Scene::TransformComponent;

namespace
{
    GameObject Shaped(std::uint32_t id, std::string name, Shape shape, std::uint32_t parent = 0)
    {
        GameObject object{.id = id, .name = std::move(name), .parent = parent};
        object.transform.emplace();
        object.renderable = Engine::Scene::RenderableDesc{.shape = shape, .rotationSpeed = 0.0f};
        return object;
    }

    // An enemy (100 health) with a head
    SceneDescription EnemyScene()
    {
        GameObject body = Shaped(1, "Enemy", Shape::Cube);
        body.light = PrefabPart(100.0f, 100.0f);
        return {.objects = {body, Shaped(2, "Head", Shape::Sphere, 1)}};
    }

    std::size_t LinkedTo(const std::vector<GameObject>& objects, const std::string& path)
    {
        return static_cast<std::size_t>(std::count_if(objects.begin(), objects.end(), [&path](const GameObject& object)
                                                      { return object.prefabInstance && object.prefabInstance->prefab == path; }));
    }

    std::vector<const GameObject*> Named(const SceneDescription& scene, const std::string& name)
    {
        std::vector<const GameObject*> found;
        for (const GameObject& object : scene.objects)
        {
            if (object.name == name)
            {
                found.push_back(&object);
            }
        }
        return found;
    }

    // A game folder of its own, with the Enemy prefab and a Squad prefab of two enemies (the second at 50 health)
    class NestedPrefabTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            dir_ = std::filesystem::temp_directory_path() / "atelier_nested_prefab_tests" / info->name();
            std::filesystem::remove_all(dir_);
            std::filesystem::create_directories(dir_ / "configs");
            std::ofstream(dir_ / "configs" / "game.cfg") << "name=nested_prefab_test\n";
            prefabs_.SetProjectDir(dir_);

            editor_.SetDescription(EnemyScene());
            enemyPath_ = prefabs_.CreatePrefab(editor_.Description(), 1, "Enemy");

            // Two enemies under an empty object, saved together as the Squad prefab
            const std::uint32_t squad = editor_.CreateObject("Squad").id;
            for (const char* name : {"Enemy 1", "Enemy 2"})
            {
                const std::uint32_t enemy = prefabs_.Instantiate(editor_.Description(), enemyPath_, editor_.NextId(),
                                                                 std::nullopt, name);
                ASSERT_TRUE(editor_.SetParent(enemy, squad));
            }
            FindObject(editor_.Description(), Named(editor_.Description(), "Enemy 2").front()->id)->light->intensity =
                50.0f;
            prefabs_.RefreshOverrides(editor_.Description());
            squadPath_ = prefabs_.CreatePrefab(editor_.Description(), squad, "Squad");
        }

        void TearDown() override { std::filesystem::remove_all(dir_); }

        Prefab OnDisk(const std::string& path)
        {
            Prefab prefab;
            const auto report = Engine::Scene::LoadPrefabFile(dir_ / path, prefab);
            EXPECT_TRUE(report.ok) << report.error;
            EXPECT_TRUE(report.warnings.empty()) << report.warnings.front();
            return prefab;
        }

        // Changes the Enemy prefab: health 200 and a bigger head
        void GrowEnemies()
        {
            Prefab enemy = *prefabs_.Find(enemyPath_);
            enemy.objects[0].light = PrefabPart(200.0f, 200.0f);
            for (GameObject& object : enemy.objects)
            {
                if (object.name == "Head")
                {
                    object.transform->scale = 0.8f;
                }
            }
            prefabs_.Save(enemyPath_, enemy);
        }

        std::filesystem::path dir_;
        PrefabManager prefabs_;
        EditorScene editor_;
        std::string enemyPath_;
        std::string squadPath_;
    };
}

// Save as Prefab on a group with whole instances in it: they are saved as nested prefabs, with their own overrides
TEST_F(NestedPrefabTest, WholeInstancesAreSavedAsNestedPrefabs)
{
    const Prefab squad = OnDisk(squadPath_);
    ASSERT_EQ(squad.objects.size(), 5u) << "the root and two enemies of two objects each";
    EXPECT_FALSE(squad.objects[0].prefabInstance) << "the Squad is not a variant";
    EXPECT_EQ(LinkedTo(squad.objects, enemyPath_), 4u);

    const SceneDescription layout{.objects = squad.objects};
    for (const GameObject& object : squad.objects)
    {
        if (object.name == "Enemy 2")
        {
            EXPECT_EQ(object.prefabInstance->overrides, (std::vector<std::string>{"light/intensity"}));
        }
        if (object.id != kPrefabRootId)
        {
            EXPECT_NE(Engine::Scene::InstanceRootOf(layout, object.id), 0u) << object.name << " is in a whole instance";
        }
    }

    // In the scene, the group it was made from is now one instance of the Squad
    for (const GameObject* enemy : Named(editor_.Description(), "Enemy 1"))
    {
        EXPECT_EQ(enemy->prefabInstance->prefab, squadPath_);
    }
}

// A change to the Enemy prefab reaches the enemies in every Squad instance, except the health the Squad changed
TEST_F(NestedPrefabTest, ChangesToTheInnerPrefabReachInstancesOfTheOuterOne)
{
    SceneDescription level;
    std::uint32_t nextId = 1;
    ASSERT_NE(prefabs_.Instantiate(level, squadPath_, nextId), 0u);
    EXPECT_EQ(LinkedTo(level.objects, squadPath_), level.objects.size()) << "scene objects link to the Squad only";

    GrowEnemies();
    EXPECT_TRUE(prefabs_.UpdateInstances(level, nextId));

    EXPECT_EQ(Named(level, "Enemy 1").front()->light->intensity, 200.0f);
    EXPECT_EQ(Named(level, "Enemy 2").front()->light->intensity, 50.0f) << "the Squad's override is kept";
    EXPECT_EQ(Named(level, "Enemy 2").front()->light->range, 200.0f) << "only health was overridden";
    for (const GameObject* head : Named(level, "Head"))
    {
        EXPECT_FLOAT_EQ(head->transform->scale, 0.8f);
    }
}

// What an instance of the Squad changed on a nested enemy stays when the Enemy prefab changes that property
TEST_F(NestedPrefabTest, SceneOverridesOnNestedPartsAreKept)
{
    SceneDescription level;
    std::uint32_t nextId = 1;
    ASSERT_NE(prefabs_.Instantiate(level, squadPath_, nextId), 0u);
    const std::uint32_t enemy1 = Named(level, "Enemy 1").front()->id;
    FindObject(level, enemy1)->light->intensity = 75.0f;
    prefabs_.RefreshOverrides(level);

    GrowEnemies();
    prefabs_.UpdateInstances(level, nextId);
    EXPECT_EQ(FindObject(level, enemy1)->light->intensity, 75.0f);
    EXPECT_EQ(FindObject(level, enemy1)->light->range, 200.0f);
}

// The editor's Prefab Mode: an instance placed under the open prefab's root is saved as a nested prefab
TEST_F(NestedPrefabTest, PlacingAPrefabInPrefabModeNestsIt)
{
    SceneDescription crateScene{.objects = {Shaped(1, "Crate", Shape::Cube)}};
    const std::string cratePath = prefabs_.CreatePrefab(crateScene, 1, "Crate");

    EditorScene mode;
    mode.SetDescription(PrefabManager::ToScene(*prefabs_.Find(cratePath)));
    const std::uint32_t guard =
        prefabs_.Instantiate(mode.Description(), enemyPath_, mode.NextId(), TransformComponent{.x = 2.0f}, "Guard");
    ASSERT_TRUE(mode.SetParent(guard, kPrefabRootId));
    FindObject(mode.Description(), guard)->light->intensity = 10.0f;
    prefabs_.RefreshOverrides(mode.Description());
    prefabs_.Save(cratePath, PrefabManager::FromScene(mode.Description(), "Crate"));

    const Prefab crate = OnDisk(cratePath);
    ASSERT_EQ(crate.objects.size(), 3u);
    EXPECT_EQ(LinkedTo(crate.objects, enemyPath_), 2u);
    EXPECT_TRUE(prefabs_.Contains(cratePath, enemyPath_));
    EXPECT_FALSE(prefabs_.Contains(enemyPath_, cratePath));

    SceneDescription level;
    std::uint32_t nextId = 1;
    ASSERT_NE(prefabs_.Instantiate(level, cratePath, nextId), 0u);
    ASSERT_EQ(level.objects.size(), 3u);
    EXPECT_EQ(Named(level, "Guard").front()->light->intensity, 10.0f);
}

// Applying an instance with another prefab's instance added under it nests that one in the prefab
TEST_F(NestedPrefabTest, ApplyingAnInstanceNestsTheInstancesAddedUnderIt)
{
    SceneDescription crateScene{.objects = {Shaped(1, "Crate", Shape::Cube)}};
    const std::string cratePath = prefabs_.CreatePrefab(crateScene, 1, "Crate");

    const std::uint32_t enemy = prefabs_.Instantiate(editor_.Description(), enemyPath_, editor_.NextId());
    const std::uint32_t crate = prefabs_.Instantiate(editor_.Description(), cratePath, editor_.NextId());
    ASSERT_TRUE(editor_.SetParent(crate, enemy));
    prefabs_.ApplyToPrefab(editor_.Description(), enemy, editor_.NextId());

    const Prefab enemyPrefab = OnDisk(enemyPath_);
    ASSERT_EQ(enemyPrefab.objects.size(), 3u);
    EXPECT_EQ(LinkedTo(enemyPrefab.objects, cratePath), 1u);

    // The Squad's enemies took the crate too, and a change to the Crate prefab reaches all of them
    SceneDescription level;
    std::uint32_t nextId = 1;
    prefabs_.Instantiate(level, squadPath_, nextId);
    EXPECT_EQ(Named(level, "Crate").size(), 2u);

    Prefab round = *prefabs_.Find(cratePath);
    round.objects[0].renderable->shape = Shape::Sphere;
    prefabs_.Save(cratePath, round);
    prefabs_.UpdateInstances(level, nextId);
    for (const GameObject* copy : Named(level, "Crate"))
    {
        EXPECT_EQ(copy->renderable->shape, Shape::Sphere);
    }
}

TEST_F(NestedPrefabTest, APrefabCannotContainItself)
{
    EXPECT_TRUE(prefabs_.Contains(squadPath_, enemyPath_));
    EXPECT_FALSE(prefabs_.Contains(enemyPath_, squadPath_));
    EXPECT_TRUE(prefabs_.Contains(enemyPath_, enemyPath_));

    // A Squad under an Enemy, applied to the Enemy: the Enemy would contain a Squad of Enemies
    const std::uint32_t enemy = prefabs_.Instantiate(editor_.Description(), enemyPath_, editor_.NextId());
    const std::uint32_t squad = prefabs_.Instantiate(editor_.Description(), squadPath_, editor_.NextId());
    ASSERT_TRUE(editor_.SetParent(squad, enemy));
    EXPECT_THROW(prefabs_.ApplyToPrefab(editor_.Description(), enemy, editor_.NextId()), std::runtime_error);
    EXPECT_EQ(OnDisk(enemyPath_).objects.size(), 2u) << "the Enemy prefab is unchanged";
}

// Two hand-written prefabs nested in each other still load, without going round in circles
TEST_F(NestedPrefabTest, PrefabsNestedInEachOtherStillLoad)
{
    std::filesystem::create_directories(dir_ / "assets" / "prefabs");
    std::ofstream(dir_ / "assets" / "prefabs" / "A.prefab") << R"({ "version": 1, "name": "A", "objects": [
        { "id": 1, "name": "A" },
        { "id": 2, "name": "B inside A", "parent": 1, "prefab": { "path": "assets/prefabs/B.prefab", "object": 1 } } ] })";
    std::ofstream(dir_ / "assets" / "prefabs" / "B.prefab") << R"({ "version": 1, "name": "B", "objects": [
        { "id": 1, "name": "B" },
        { "id": 2, "name": "A inside B", "parent": 1, "prefab": { "path": "assets/prefabs/A.prefab", "object": 1 } } ] })";

    // A's copy of B takes B's objects once (B's own copy of A cannot be brought up to date: it leads back to A)
    const Prefab* a = prefabs_.Find("assets/prefabs/A.prefab");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->objects.size(), 3u);
    EXPECT_TRUE(prefabs_.Contains("assets/prefabs/A.prefab", "assets/prefabs/B.prefab"));
    EXPECT_TRUE(prefabs_.Contains("assets/prefabs/B.prefab", "assets/prefabs/A.prefab"));
}

// Deleting the inner prefab: the outer one keeps its enemies as ordinary objects
TEST_F(NestedPrefabTest, DeletingTheInnerPrefabKeepsItsObjectsInTheOuterOne)
{
    const auto result = prefabs_.Delete(editor_.Description(), enemyPath_);
    EXPECT_EQ(result.containers, 1u);
    EXPECT_EQ(result.variants, 0u);

    const Prefab squad = OnDisk(squadPath_);
    EXPECT_EQ(squad.objects.size(), 5u);
    EXPECT_EQ(LinkedTo(squad.objects, enemyPath_), 0u);
}
