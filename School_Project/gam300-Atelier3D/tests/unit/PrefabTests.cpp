#include "engine/editor/EditorScene.hpp"
#include "engine/game/GameProject.hpp"
#include "engine/game/GameRuntime.hpp"
#include "engine/game/PrefabManager.hpp"
#include "engine/scene/Hierarchy.hpp"
#include "engine/scene/Prefab.hpp"
#include "engine/scene/SceneFile.hpp"
#include "engine/scene/systems/HierarchySystem.hpp"
#include "engine/scene/systems/SpinSystem.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Parents and children, prefab files, and Game::PrefabManager: saving a group of objects as a prefab, placing instances
// of it, and changing properties on one instance without touching the others (overrides)

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
using Engine::Scene::Prefab;
using Engine::Scene::SceneDescription;
using Engine::Scene::TransformComponent;

namespace
{
    constexpr float kTolerance = 1e-4f;

    void ExpectNear(const TransformComponent& actual, const TransformComponent& expected)
    {
        EXPECT_NEAR(actual.x, expected.x, kTolerance);
        EXPECT_NEAR(actual.y, expected.y, kTolerance);
        EXPECT_NEAR(actual.z, expected.z, kTolerance);
        EXPECT_NEAR(actual.rotationX, expected.rotationX, kTolerance);
        EXPECT_NEAR(actual.rotation, expected.rotation, kTolerance);
        EXPECT_NEAR(actual.rotationZ, expected.rotationZ, kTolerance);
        EXPECT_NEAR(actual.scale * actual.scaleX, expected.scale * expected.scaleX, kTolerance);
        EXPECT_NEAR(actual.scale * actual.scaleY, expected.scale * expected.scaleY, kTolerance);
        EXPECT_NEAR(actual.scale * actual.scaleZ, expected.scale * expected.scaleZ, kTolerance);
    }

    GameObject Shaped(std::uint32_t id, std::string name, Shape shape, TransformComponent transform,
                      std::uint32_t parent = 0)
    {
        GameObject object{.id = id, .name = std::move(name), .parent = parent};
        object.transform = transform;
        object.renderable = Engine::Scene::RenderableDesc{.shape = shape, .rotationSpeed = 0.0f};
        return object;
    }

    // The demo's player: a body the move keys drive, with 100 health and three child objects
    SceneDescription PlayerScene()
    {
        GameObject player = Shaped(1, "Player", Shape::Cube, {});
        player.player.emplace();
        player.light = PrefabPart(100.0f, 100.0f);
        return SceneDescription{.objects = {player, Shaped(2, "Head", Shape::Sphere, {.y = 1.2f, .scale = 0.4f}, 1),
                                            Shaped(3, "Left Arm", Shape::Cube, {.x = -0.7f, .scaleY = 0.3f}, 1),
                                            Shaped(4, "Right Arm", Shape::Cube, {.x = 0.7f, .scaleY = 0.3f}, 1)}};
    }

    // Every object of the scene whose prefab link says it is a root, in scene order
    std::vector<const GameObject*> InstanceRoots(const SceneDescription& scene, const std::string& prefab)
    {
        std::vector<const GameObject*> roots;
        for (const GameObject& object : scene.objects)
        {
            if (object.prefabInstance && object.prefabInstance->prefab == prefab && object.prefabInstance->IsRoot())
            {
                roots.push_back(&object);
            }
        }
        return roots;
    }

    // A game folder of its own for each test
    class PrefabTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            dir_ = std::filesystem::temp_directory_path() / "atelier_prefab_tests" / info->name();
            std::filesystem::remove_all(dir_);
            std::filesystem::create_directories(dir_ / "configs");
            std::ofstream(dir_ / "configs" / "game.cfg") << "name=prefab_test\n";
            prefabs_.SetProjectDir(dir_);
        }

        void TearDown() override { std::filesystem::remove_all(dir_); }

        // The player scene with the player saved as the Enemy prefab and 5 more enemies placed in a row
        void MakeEnemies()
        {
            editor_.SetDescription(PlayerScene());
            enemyPath_ = prefabs_.CreatePrefab(editor_.Description(), 1, "Enemy");
            for (int i = 1; i <= 5; ++i)
            {
                const std::uint32_t root = prefabs_.Instantiate(
                    editor_.Description(), enemyPath_, editor_.NextId(),
                    TransformComponent{.x = 3.0f * static_cast<float>(i), .z = -5.0f}, "Enemy " + std::to_string(i));
                ASSERT_NE(root, 0u);
                enemies_.push_back(root);
            }
        }

        GameObject& Object(std::uint32_t id) { return *FindObject(editor_.Description(), id); }

        // The instance object that copies prefab object prefabId
        GameObject& Part(std::uint32_t root, std::uint32_t prefabId)
        {
            for (const std::uint32_t id : Engine::Scene::InstanceMembers(editor_.Description(), root))
            {
                if (Object(id).prefabInstance->object == prefabId)
                {
                    return Object(id);
                }
            }
            throw std::out_of_range("no such part");
        }

        std::filesystem::path dir_;
        PrefabManager prefabs_;
        EditorScene editor_;
        std::string enemyPath_;
        std::vector<std::uint32_t> enemies_;
    };
}

// ---------------------------------------------------------------- Parents and children

// A matrix turns back into the transform it was made from (with the size kept as asked)
TEST(Hierarchy, AModelMatrixTurnsBackIntoItsTransform)
{
    const TransformComponent transform{.x = 1.5f,
                                       .y = -2.0f,
                                       .z = 0.25f,
                                       .rotation = 30.0f,
                                       .rotationX = -20.0f,
                                       .rotationZ = 10.0f,
                                       .scale = 2.0f,
                                       .scaleX = 1.5f,
                                       .scaleY = 0.5f,
                                       .scaleZ = 1.0f};
    const TransformComponent back =
        Engine::Scene::TransformFromMatrix(Engine::Scene::ModelMatrixOf(transform), transform.scale);
    ExpectNear(back, transform);
    EXPECT_FLOAT_EQ(back.scale, 2.0f);
}

// A child's Transform is relative to its parent: turned and moved with it, all the way up the tree
TEST(Hierarchy, AChildIsPlacedRelativeToItsParents)
{
    SceneDescription scene{.objects = {Shaped(1, "Base", Shape::Cube, {.x = 10.0f, .rotation = 90.0f, .scale = 2.0f}),
                                       Shaped(2, "Arm", Shape::Cube, {.x = 1.0f}, 1),
                                       Shaped(3, "Hand", Shape::Cube, {.y = 1.0f}, 2)}};

    // Turning 90 degrees around Y takes +X to -Z; the parent's size doubles every offset under it
    ExpectNear(*Engine::Scene::WorldTransformOf(scene, 2), {.x = 10.0f, .z = -2.0f, .rotation = 90.0f, .scale = 2.0f});
    ExpectNear(*Engine::Scene::WorldTransformOf(scene, 3),
               {.x = 10.0f, .y = 2.0f, .z = -2.0f, .rotation = 90.0f, .scale = 2.0f});

    EXPECT_EQ(Engine::Scene::SubtreeOf(scene, 1), (std::vector<std::uint32_t>{1, 2, 3}));
    EXPECT_EQ(Engine::Scene::ChildrenOf(scene, 0), (std::vector<std::uint32_t>{1}));
    EXPECT_TRUE(Engine::Scene::IsAncestorOf(scene, 1, 3));
    EXPECT_FALSE(Engine::Scene::CanSetParent(scene, 1, 3)); // a loop
    EXPECT_TRUE(Engine::Scene::CanSetParent(scene, 3, 0));
}

// Reparenting keeps an object where it is in the world; removing a parent removes its children; a duplicate copies
// the whole group
TEST(Hierarchy, TheEditorMovesRemovesAndCopiesWholeGroups)
{
    EditorScene editor;
    editor.SetDescription(SceneDescription{.objects = {Shaped(1, "Base", Shape::Cube, {.x = 4.0f, .rotation = 90.0f}),
                                                       Shaped(2, "Box", Shape::Cube, {.x = 5.0f, .y = 1.0f})}});

    const TransformComponent before = *Engine::Scene::WorldTransformOf(editor.Description(), 2);
    ASSERT_TRUE(editor.SetParent(2, 1));
    EXPECT_EQ(editor.Find(2)->parent, 1u);
    ExpectNear(*Engine::Scene::WorldTransformOf(editor.Description(), 2), before);
    ExpectNear(*editor.Find(2)->transform, {.y = 1.0f, .z = 1.0f, .rotation = -90.0f}); // now measured from Base
    EXPECT_FALSE(editor.SetParent(1, 2)) << "a parent cannot go under its own child";

    const GameObject& copy = editor.Duplicate(1);
    const std::uint32_t copyId = copy.id;
    ASSERT_EQ(Engine::Scene::ChildrenOf(editor.Description(), copyId).size(), 1u);
    EXPECT_EQ(editor.Objects().size(), 4u);

    ASSERT_TRUE(editor.Remove(1));
    EXPECT_EQ(editor.Objects().size(), 2u) << "Box went with Base";
    EXPECT_EQ(editor.Find(2), nullptr);

    ASSERT_TRUE(editor.SetParent(Engine::Scene::ChildrenOf(editor.Description(), copyId).front(), 0));
    EXPECT_EQ(Engine::Scene::ChildrenOf(editor.Description(), 0).size(), 2u);
}

// A parent that does not exist, or a loop of parents, is dropped when the file is read; parent, health and prefab
// links survive a save and load
TEST(Hierarchy, SceneFilesKeepParentsHealthAndPrefabLinks)
{
    const auto dir = std::filesystem::temp_directory_path() / "atelier_prefab_tests" / "scene_file";
    std::filesystem::remove_all(dir);

    SceneDescription scene = PlayerScene();
    scene.objects[1].prefabInstance =
        Engine::Scene::PrefabInstance{.prefab = "assets/prefabs/Enemy.prefab", .object = 2, .overrides = {"name"}};
    Engine::Scene::SaveSceneFile(dir / "saved.json", scene);
    SceneDescription loaded;
    const auto report = Engine::Scene::LoadSceneFile(dir / "saved.json", loaded);
    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(report.warnings.empty());
    EXPECT_EQ(loaded, scene);

    std::filesystem::create_directories(dir);
    std::ofstream(dir / "bad.json") << R"({ "version": 8, "objects": [
        { "id": 1, "name": "A", "parent": 2 }, { "id": 2, "name": "B", "parent": 1 },
        { "id": 3, "name": "C", "parent": 99 } ] })";
    const auto bad = Engine::Scene::LoadSceneFile(dir / "bad.json", loaded);
    ASSERT_TRUE(bad.ok);
    EXPECT_EQ(bad.warnings.size(), 2u);
    EXPECT_EQ(loaded.objects[0].parent, 0u) << "the loop is broken";
    EXPECT_EQ(loaded.objects[1].parent, 1u);
    EXPECT_EQ(loaded.objects[2].parent, 0u) << "no object 99";

    // A version 7 file (from before parents) still opens
    std::ofstream(dir / "old.json") << R"({ "version": 7, "objects": [ { "id": 1, "name": "Old" } ] })";
    EXPECT_TRUE(Engine::Scene::LoadSceneFile(dir / "old.json", loaded).ok);
    std::filesystem::remove_all(dir);
}

// The runtime keeps children with their parents: placed from the parent, and moving (here, spinning) with it
TEST(Hierarchy, ChildEntitiesFollowTheirParents)
{
    using namespace Engine::Scene;
    Scene ecs;
    const Entity parent = ecs.CreateEntity();
    const Entity child = ecs.CreateEntity();
    ecs.SetTransform(parent, {.x = 2.0f});
    ecs.AddComponent(parent, SpinComponent{.degreesPerSecond = 90.0f});
    ecs.AddComponent(child, HierarchyComponent{.parent = parent.GetId(), .local = {.x = 1.0f}});

    Engine::Systems::HierarchySystem::Update(ecs);
    ExpectNear(*ecs.GetTransform(child), {.x = 3.0f});

    Engine::Systems::SpinSystem::Update(ecs, 1.0); // the parent turns a quarter
    Engine::Systems::HierarchySystem::Update(ecs);
    ExpectNear(*ecs.GetTransform(child), {.x = 2.0f, .z = -1.0f, .rotation = 90.0f});
}

// ---------------------------------------------------------------- Prefab files

// The file holds the prefab's name and objects, root first; reading it back gives the same prefab
TEST_F(PrefabTest, PrefabFilesRoundTrip)
{
    const Prefab prefab = PrefabManager::Capture(PlayerScene(), 1, "Enemy");
    ASSERT_EQ(prefab.objects.size(), 4u);
    EXPECT_EQ(prefab.objects[0].id, Engine::Scene::kPrefabRootId);
    EXPECT_EQ(prefab.objects[0].parent, 0u);

    Engine::Scene::SavePrefabFile(dir_ / "Enemy.prefab", prefab);
    Prefab loaded;
    const auto report = Engine::Scene::LoadPrefabFile(dir_ / "Enemy.prefab", loaded);
    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(report.warnings.empty());
    EXPECT_EQ(loaded, prefab);
}

// A hand-written file with the root out of place, a stray top-level object or a link to part of a prefab (not a
// whole instance of it, which would be a nested prefab) is fixed when read
TEST_F(PrefabTest, BadPrefabLayoutsAreFixedWithWarnings)
{
    std::ofstream(dir_ / "odd.prefab") << R"({ "version": 1, "name": "Odd", "objects": [
        { "id": 5, "name": "Root" },
        { "id": 1, "name": "Part", "parent": 5 },
        { "id": 7, "name": "Stray", "prefab": { "path": "x.prefab", "object": 2 } } ] })";
    Prefab prefab;
    const auto report = Engine::Scene::LoadPrefabFile(dir_ / "odd.prefab", prefab);
    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_EQ(report.warnings.size(), 3u);
    ASSERT_EQ(prefab.objects.size(), 3u);
    EXPECT_EQ(prefab.objects[0].name, "Root");
    EXPECT_EQ(prefab.objects[0].id, Engine::Scene::kPrefabRootId);
    EXPECT_EQ(prefab.Find(5)->name, "Part") << "ids swapped with the root";
    EXPECT_EQ(prefab.Find(5)->parent, Engine::Scene::kPrefabRootId);
    EXPECT_EQ(prefab.Find(7)->parent, Engine::Scene::kPrefabRootId);
    EXPECT_FALSE(prefab.Find(7)->prefabInstance);

    std::ofstream(dir_ / "empty.prefab") << R"({ "version": 1, "objects": [] })";
    EXPECT_FALSE(Engine::Scene::LoadPrefabFile(dir_ / "empty.prefab", prefab).ok);
    EXPECT_FALSE(Engine::Scene::LoadPrefabFile(dir_ / "missing.prefab", prefab).ok);
}

// ---------------------------------------------------------------- The demo

// Save the player and its 3 children as the Enemy prefab, place 5 copies, give one copy different health: only that
// copy changes, it remembers the change as an override, and it all survives saving and opening the scene
TEST_F(PrefabTest, DemoFiveEnemiesOneWithDifferentHealth)
{
    MakeEnemies();

    // The prefab file holds the player and its 3 children
    EXPECT_EQ(enemyPath_, "assets/prefabs/Enemy.prefab");
    EXPECT_TRUE(std::filesystem::is_regular_file(dir_ / enemyPath_));
    EXPECT_EQ(prefabs_.List(), std::vector<std::string>{enemyPath_});
    const Prefab* enemy = prefabs_.Find(enemyPath_);
    ASSERT_NE(enemy, nullptr);
    ASSERT_EQ(enemy->objects.size(), 4u);
    EXPECT_EQ(enemy->objects[0].light, PrefabPart(100.0f, 100.0f));

    // The player itself became the first instance, and 5 copies were placed: 6 instances of 4 objects each
    const SceneDescription& scene = editor_.Description();
    EXPECT_EQ(InstanceRoots(scene, enemyPath_).size(), 6u);
    EXPECT_EQ(scene.objects.size(), 24u);
    EXPECT_TRUE(Object(1).prefabInstance && Object(1).prefabInstance->IsRoot());
    for (const std::uint32_t root : enemies_)
    {
        EXPECT_EQ(Engine::Scene::ChildrenOf(scene, root).size(), 3u);
        EXPECT_EQ(Engine::Scene::InstanceMembers(scene, root).size(), 4u);
    }
    // Each copy's children are placed relative to it: the head sits above wherever its enemy is
    const GameObject& thirdHead = Part(enemies_[2], 2);
    ExpectNear(*Engine::Scene::WorldTransformOf(scene, thirdHead.id),
               {.x = 9.0f, .y = 1.2f, .z = -5.0f, .scale = 0.4f});

    // Change the health of one copy
    Object(enemies_[2]).light->intensity = 25.0f;
    EXPECT_TRUE(prefabs_.RefreshOverrides(editor_.Description()));

    EXPECT_EQ(Object(enemies_[2]).prefabInstance->overrides, std::vector<std::string>{"light/intensity"});
    for (const GameObject& object : editor_.Objects())
    {
        if (object.id != enemies_[2])
        {
            EXPECT_TRUE(object.prefabInstance->overrides.empty()) << object.name;
            if (object.light)
            {
                EXPECT_EQ(object.light->intensity, 100.0f) << object.name;
            }
        }
    }
    // Placing a copy and naming it are the instance's own, not overrides
    EXPECT_NE(Object(enemies_[0]).transform, Object(enemies_[1]).transform);

    // Saved and opened again: the same scene, the same override
    Engine::Game::GameProject(dir_).SaveScene("data/scenes/main.json", editor_.Description());
    const SceneDescription reopened = Engine::Game::GameProject(dir_).LoadScene("data/scenes/main.json");
    EXPECT_EQ(reopened, editor_.Description());
    EXPECT_EQ(FindObject(reopened, enemies_[2])->light->intensity, 25.0f);
}

// The game plays the instances as they are: each enemy is drawn with its own health, its parts around it
TEST_F(PrefabTest, DemoEnemiesPlayWithTheirOwnHealth)
{
    MakeEnemies();
    Object(enemies_[2]).light->intensity = 25.0f;

    Engine::Game::GameRuntime runtime;
    runtime.SetSimulating(false);
    runtime.LoadProject(Engine::Game::GameProject(dir_));
    ASSERT_TRUE(runtime.Start(editor_.Description()));

    std::map<float, int> healths; // health -> how many entities have it
    runtime.Ecs()->ForEach<const LightComponent>([&](auto, const LightComponent& part)
                                                 { ++healths[part.intensity]; });
    EXPECT_EQ(healths, (std::map<float, int>{{25.0f, 1}, {100.0f, 5}}));
    EXPECT_EQ(runtime.Ecs()->Count<Engine::Scene::HierarchyComponent>(), 18u); // 3 children on each of 6

    // Each head is where the editor shows it: above its own enemy
    std::vector<float> headHeights;
    runtime.Ecs()->ForEach<const TransformComponent, const Engine::Scene::MeshRendererComponent>(
        [&](auto, const TransformComponent& world, const Engine::Scene::MeshRendererComponent& mesh)
        {
            if (mesh.shape == Shape::Sphere)
            {
                headHeights.push_back(world.y);
            }
        });
    ASSERT_EQ(headHeights.size(), 6u);
    for (const float height : headHeights)
    {
        EXPECT_NEAR(height, 1.2f, kTolerance);
    }
    runtime.Stop();
}

// The committed Prefabs demo (the sample game's data/scenes/prefabs.json and assets/prefabs/Enemy.prefab): the player
// with its 3 children, and 5 Enemy instances made from it, one with different health. It is up to date with its
// prefab, and a game made from the template brings the prefab along.
TEST(PrefabDemo, TheSampleGameHasFiveEnemiesOneWithDifferentHealth)
{
    const std::filesystem::path game = ATELIER_SAMPLE_GAME_DIR;
    SceneDescription scene = Engine::Game::GameProject(game).LoadScene("data/scenes/prefabs.json");
    PrefabManager prefabs(game);
    const std::string enemy = "assets/prefabs/Enemy.prefab";
    const std::vector<std::string> listed = prefabs.List(); // the genre games' prefabs are there too
    ASSERT_NE(std::find(listed.begin(), listed.end(), enemy), listed.end());
    ASSERT_NE(prefabs.Find(enemy), nullptr);
    EXPECT_EQ(prefabs.Find(enemy)->objects.size(), 4u);

    const auto player = std::find_if(scene.objects.begin(), scene.objects.end(),
                                     [](const GameObject& object) { return object.player.has_value(); });
    ASSERT_NE(player, scene.objects.end());
    EXPECT_EQ(Engine::Scene::ChildrenOf(scene, player->id).size(), 3u);

    const auto roots = InstanceRoots(scene, enemy);
    ASSERT_EQ(roots.size(), 5u);
    std::vector<float> healths;
    for (const GameObject* root : roots)
    {
        EXPECT_EQ(Engine::Scene::InstanceMembers(scene, root->id).size(), 4u) << root->name;
        ASSERT_TRUE(root->logic.has_value());
        healths.push_back(Engine::Logic::FindVariable(*root->logic, "health")->value);
    }
    EXPECT_EQ(healths, (std::vector<float>{100.0f, 100.0f, 25.0f, 100.0f, 100.0f}));
    EXPECT_FALSE(roots[2]->prefabInstance->overrides.empty());

    // Nothing to update and nothing unrecorded: what the editor finds when it opens the demo
    const SceneDescription asSaved = scene;
    std::uint32_t nextId = 1000;
    EXPECT_FALSE(prefabs.UpdateInstances(scene, nextId));
    EXPECT_FALSE(prefabs.RefreshOverrides(scene));
    EXPECT_EQ(scene, asSaved);

    // A new game from the template gets the prefab file too
    const auto root = std::filesystem::temp_directory_path() / "atelier_prefab_tests" / "template";
    std::filesystem::remove_all(root);
    const auto project =
        Engine::Game::GameProject::Create(root, "prefab_game", Engine::Game::GameTemplate::Prefabs, game);
    EXPECT_TRUE(std::filesystem::is_regular_file(project.ProjectDir() / enemy));
    std::filesystem::remove_all(root);
}

// ---------------------------------------------------------------- Changing the prefab

// Apply to Prefab from one copy: every other copy takes the change, except where it has an override of its own
TEST_F(PrefabTest, ApplyingAnInstanceUpdatesTheOthersButKeepsTheirOverrides)
{
    MakeEnemies();
    Object(enemies_[2]).light->intensity = 25.0f;
    prefabs_.RefreshOverrides(editor_.Description());

    // A tougher enemy with a red head, made on copy 0 and applied to the prefab
    Object(enemies_[0]).light = PrefabPart(150.0f, 150.0f);
    Part(enemies_[0], 2).renderable->albedoG = 0.0f;
    Part(enemies_[0], 2).renderable->albedoB = 0.0f;
    prefabs_.ApplyToPrefab(editor_.Description(), Part(enemies_[0], 3).id, editor_.NextId()); // any part will do

    EXPECT_EQ(prefabs_.Find(enemyPath_)->objects[0].light->range, 150.0f);
    Prefab onDisk;
    ASSERT_TRUE(Engine::Scene::LoadPrefabFile(dir_ / enemyPath_, onDisk).ok);
    EXPECT_EQ(onDisk, *prefabs_.Find(enemyPath_));
    EXPECT_EQ(onDisk.objects[0].transform, (TransformComponent{})) << "the prefab keeps its own placement";
    EXPECT_EQ(onDisk.objects[0].name, "Enemy") << "and its own root name";

    for (const std::uint32_t root : enemies_)
    {
        EXPECT_EQ(Object(root).light->range, 150.0f);
        EXPECT_EQ(Part(root, 2).renderable->albedoG, 0.0f) << "every head is red now";
        EXPECT_TRUE(Object(root).name.starts_with("Enemy")) << "names stay";
    }
    EXPECT_EQ(Object(enemies_[1]).light->intensity, 150.0f);
    EXPECT_EQ(Object(enemies_[2]).light->intensity, 25.0f) << "its own health is kept";
    EXPECT_EQ(Object(enemies_[2]).prefabInstance->overrides, std::vector<std::string>{"light/intensity"});
    EXPECT_TRUE(Object(enemies_[0]).prefabInstance->overrides.empty());

    // Revert gives it the prefab's health again
    EXPECT_TRUE(prefabs_.Revert(editor_.Description(), enemies_[2], "light/intensity"));
    EXPECT_EQ(Object(enemies_[2]).light->intensity, 150.0f);
    EXPECT_TRUE(Object(enemies_[2]).prefabInstance->overrides.empty());
    EXPECT_FALSE(prefabs_.Revert(editor_.Description(), enemies_[2])) << "nothing left to revert";
}

// A whole component added to one copy is one override ("light"); a part added under a copy becomes part of the prefab
// when applied, and the other copies gain it
TEST_F(PrefabTest, ComponentOverridesAndPartsAddedToThePrefab)
{
    MakeEnemies();
    Part(enemies_[1], 4).light.emplace();
    prefabs_.RefreshOverrides(editor_.Description());
    EXPECT_EQ(Part(enemies_[1], 4).prefabInstance->overrides, std::vector<std::string>{"light"});

    // A sword in the right hand of copy 3, applied to the prefab
    GameObject& sword = editor_.CreateObject("Sword");
    sword.transform = TransformComponent{.x = 0.2f};
    sword.renderable.emplace();
    const std::uint32_t swordId = sword.id;
    ASSERT_TRUE(editor_.SetParent(swordId, Part(enemies_[3], 4).id));
    prefabs_.ApplyToPrefab(editor_.Description(), enemies_[3], editor_.NextId());

    ASSERT_EQ(prefabs_.Find(enemyPath_)->objects.size(), 5u);
    EXPECT_TRUE(Object(swordId).prefabInstance) << "the sword is part of the instance now";
    for (const std::uint32_t root : enemies_)
    {
        EXPECT_EQ(Engine::Scene::InstanceMembers(editor_.Description(), root).size(), 5u);
        const GameObject& copy = Part(root, Object(swordId).prefabInstance->object);
        EXPECT_EQ(copy.name, "Sword");
        EXPECT_EQ(copy.parent, Part(root, 4).id) << "in that enemy's own right hand";
    }
    EXPECT_TRUE(Part(enemies_[1], 4).light) << "the light override stays";
    EXPECT_FALSE(Part(enemies_[2], 4).light);

    // The prefab is edited elsewhere (here: by hand) to drop the sword again; the next update removes it everywhere
    Prefab edited = *prefabs_.Find(enemyPath_);
    edited.objects.pop_back();
    Engine::Scene::SavePrefabFile(dir_ / enemyPath_, edited);
    prefabs_.Forget();
    EXPECT_TRUE(prefabs_.UpdateInstances(editor_.Description(), editor_.NextId()));
    EXPECT_EQ(editor_.Objects().size(), 24u);
}

// Unpack turns an instance back into plain objects; making a prefab from part of an instance, with a bad name, or over
// an existing one is refused; a missing prefab places nothing
TEST_F(PrefabTest, UnpackAndRefusals)
{
    MakeEnemies();
    EXPECT_THROW(prefabs_.CreatePrefab(editor_.Description(), Part(enemies_[0], 2).id, "Head"), std::runtime_error);
    EXPECT_THROW(prefabs_.CreatePrefab(editor_.Description(), 1, "Enemy"), std::runtime_error) << "already exists";
    EXPECT_THROW(prefabs_.CreatePrefab(editor_.Description(), 1, "bad name!"), std::runtime_error);
    EXPECT_THROW(prefabs_.CreatePrefab(editor_.Description(), 999, "Nothing"), std::invalid_argument);
    EXPECT_EQ(prefabs_.Instantiate(editor_.Description(), "assets/prefabs/Missing.prefab", editor_.NextId()), 0u);
    EXPECT_THROW(prefabs_.ApplyToPrefab(editor_.Description(), 999, editor_.NextId()), std::invalid_argument);

    ASSERT_TRUE(PrefabManager::Unpack(editor_.Description(), Part(enemies_[4], 3).id));
    for (const std::uint32_t id : Engine::Scene::SubtreeOf(editor_.Description(), enemies_[4]))
    {
        EXPECT_FALSE(Object(id).prefabInstance);
    }
    EXPECT_EQ(InstanceRoots(editor_.Description(), enemyPath_).size(), 5u);

    // An unpacked group can become a prefab of its own
    EXPECT_EQ(prefabs_.CreatePrefab(editor_.Description(), enemies_[4], "Boss"), "assets/prefabs/Boss.prefab");
}

// Copying a whole instance makes another instance; copying one of its parts makes a plain object
TEST_F(PrefabTest, CopiesOfInstancesAndOfTheirParts)
{
    MakeEnemies();
    const GameObject& copy = editor_.Duplicate(enemies_[0]);
    const std::uint32_t copyId = copy.id;
    EXPECT_EQ(Engine::Scene::InstanceMembers(editor_.Description(), copyId).size(), 4u);

    const GameObject& arm = editor_.Paste(editor_.CopyOf(Part(enemies_[0], 3).id));
    EXPECT_FALSE(arm.prefabInstance);
    EXPECT_EQ(arm.parent, 0u);
}

// Apply one property from the Inspector's Overrides list: only that one goes into the prefab, and every other
// instance takes it; the instance's other changes stay its own
TEST_F(PrefabTest, ApplyingOnePropertyLeavesTheOtherChangesOnTheInstance)
{
    MakeEnemies();
    Object(enemies_[1]).light->intensity = 40.0f;
    Part(enemies_[1], 2).renderable->shape = Shape::Cube; // a square head
    prefabs_.RefreshOverrides(editor_.Description());

    EXPECT_FALSE(prefabs_.ApplyProperty(editor_.Description(), enemies_[1], "camera", editor_.NextId()))
        << "it did not change that";
    EXPECT_TRUE(prefabs_.ApplyProperty(editor_.Description(), enemies_[1], "light/intensity", editor_.NextId()));

    EXPECT_EQ(prefabs_.Find(enemyPath_)->objects[0].light->intensity, 40.0f);
    EXPECT_EQ(prefabs_.Find(enemyPath_)->Find(2)->renderable->shape, Shape::Sphere) << "not applied";
    Prefab onDisk;
    ASSERT_TRUE(Engine::Scene::LoadPrefabFile(dir_ / enemyPath_, onDisk).ok);
    EXPECT_EQ(onDisk, *prefabs_.Find(enemyPath_));

    EXPECT_EQ(Object(1).light->intensity, 40.0f) << "the first instance takes it too";
    for (const std::uint32_t root : enemies_)
    {
        EXPECT_EQ(Object(root).light->intensity, 40.0f);
    }
    EXPECT_TRUE(Object(enemies_[1]).prefabInstance->overrides.empty());
    EXPECT_EQ(Part(enemies_[1], 2).prefabInstance->overrides, std::vector<std::string>{"renderable/shape"});
    EXPECT_EQ(Part(enemies_[0], 2).renderable->shape, Shape::Sphere);

    EXPECT_THROW(prefabs_.ApplyProperty(editor_.Description(), 999, "health", editor_.NextId()),
                 std::invalid_argument);
}

// InstancesOf finds each instance's root once, in scene order (the Prefabs list's "n in scene")
TEST_F(PrefabTest, InstancesOfListsEachRootOnce)
{
    MakeEnemies();
    std::vector<std::uint32_t> expected{1};
    expected.insert(expected.end(), enemies_.begin(), enemies_.end());
    EXPECT_EQ(PrefabManager::InstancesOf(editor_.Description(), enemyPath_), expected);
    EXPECT_TRUE(PrefabManager::InstancesOf(editor_.Description(), "assets/prefabs/Other.prefab").empty());
}

// ---------------------------------------------------------------- Variants

// A variant file names its base, and its root (and any part copied from the base) is linked to the base object it
// copies; read back, it is the same variant
TEST_F(PrefabTest, VariantFilesKeepTheirBaseAndLinks)
{
    std::filesystem::create_directories(dir_ / "assets" / "prefabs");
    std::ofstream(dir_ / "boss.prefab") << R"({ "version": 1, "name": "Boss", "base": "assets/prefabs/Enemy.prefab",
        "objects": [
        { "id": 1, "name": "Boss", "prefab": { "path": "assets/prefabs/Enemy.prefab", "object": 1 } },
        { "id": 2, "name": "Head", "parent": 1, "prefab": { "path": "assets/prefabs/Enemy.prefab", "object": 2 } },
        { "id": 5, "name": "Crown", "parent": 2 } ] })";
    Prefab boss;
    const auto report = Engine::Scene::LoadPrefabFile(dir_ / "boss.prefab", boss);
    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(report.warnings.empty()) << report.warnings.front();
    EXPECT_TRUE(boss.IsVariant());
    EXPECT_EQ(boss.base, "assets/prefabs/Enemy.prefab");
    ASSERT_EQ(boss.objects.size(), 3u);
    ASSERT_TRUE(boss.Find(1)->prefabInstance);
    EXPECT_TRUE(boss.Find(1)->prefabInstance->IsRoot());
    EXPECT_EQ(boss.Find(2)->prefabInstance->object, 2u);
    EXPECT_FALSE(boss.Find(5)->prefabInstance) << "added by the variant: its own";

    Engine::Scene::SavePrefabFile(dir_ / "again.prefab", boss);
    Prefab again;
    ASSERT_TRUE(Engine::Scene::LoadPrefabFile(dir_ / "again.prefab", again).ok);
    EXPECT_EQ(again, boss);

    // An ordinary prefab writes no base
    Engine::Scene::SavePrefabFile(dir_ / "plain.prefab", PrefabManager::Capture(PlayerScene(), 1, "Plain"));
    std::ifstream plain(dir_ / "plain.prefab");
    const std::string text{std::istreambuf_iterator<char>(plain), std::istreambuf_iterator<char>()};
    EXPECT_EQ(text.find("\"base\""), std::string::npos);
}

// A variant whose root is not linked to its base's root, or whose base is not text, is read as an ordinary prefab
// (links to the base dropped), with a warning
TEST_F(PrefabTest, BrokenVariantsAreReadAsOrdinaryPrefabs)
{
    std::ofstream(dir_ / "unlinked.prefab") << R"({ "version": 1, "name": "Boss", "base": "assets/prefabs/Enemy.prefab",
        "objects": [
        { "id": 1, "name": "Boss" },
        { "id": 2, "name": "Head", "parent": 1, "prefab": { "path": "assets/prefabs/Enemy.prefab", "object": 2 } } ] })";
    Prefab unlinked;
    const auto report = Engine::Scene::LoadPrefabFile(dir_ / "unlinked.prefab", unlinked);
    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_EQ(report.warnings.size(), 2u) << "the root, and the link dropped";
    EXPECT_FALSE(unlinked.IsVariant());
    EXPECT_FALSE(unlinked.Find(2)->prefabInstance);

    std::ofstream(dir_ / "number.prefab") << R"({ "version": 1, "name": "Boss", "base": 5,
        "objects": [ { "id": 1, "name": "Boss" } ] })";
    Prefab number;
    const auto numberReport = Engine::Scene::LoadPrefabFile(dir_ / "number.prefab", number);
    ASSERT_TRUE(numberReport.ok) << numberReport.error;
    EXPECT_EQ(numberReport.warnings.size(), 1u);
    EXPECT_FALSE(number.IsVariant());
}

// Create Variant: a new prefab that copies every object of its base, each linked to the one it copies, nothing
// changed yet, its root named after it
TEST_F(PrefabTest, CreatingAVariant)
{
    MakeEnemies();
    const std::string boss = prefabs_.CreateVariant(enemyPath_, "Boss");
    EXPECT_EQ(boss, "assets/prefabs/Boss.prefab");
    EXPECT_TRUE(std::filesystem::is_regular_file(dir_ / boss));
    EXPECT_EQ(prefabs_.List(), (std::vector<std::string>{boss, enemyPath_}));

    const Prefab* variant = prefabs_.Find(boss);
    ASSERT_NE(variant, nullptr);
    EXPECT_TRUE(variant->IsVariant());
    EXPECT_EQ(variant->base, enemyPath_);
    EXPECT_EQ(variant->name, "Boss");
    ASSERT_EQ(variant->objects.size(), 4u);
    EXPECT_EQ(variant->objects[0].name, "Boss");
    for (const GameObject& object : variant->objects)
    {
        ASSERT_TRUE(object.prefabInstance) << object.name;
        EXPECT_EQ(object.prefabInstance->prefab, enemyPath_);
        EXPECT_EQ(object.prefabInstance->object, object.id) << "a copy of the base object with its id";
        EXPECT_TRUE(object.prefabInstance->overrides.empty()) << object.name << ": the root's name is its own";
    }
    Prefab onDisk;
    ASSERT_TRUE(Engine::Scene::LoadPrefabFile(dir_ / boss, onDisk).ok);
    EXPECT_EQ(onDisk, *variant);

    EXPECT_EQ(prefabs_.VariantsOf(enemyPath_), std::vector<std::string>{boss});
    EXPECT_TRUE(prefabs_.VariantsOf(boss).empty());

    EXPECT_THROW(prefabs_.CreateVariant(enemyPath_, "Boss"), std::runtime_error) << "already exists";
    EXPECT_THROW(prefabs_.CreateVariant(enemyPath_, "bad name!"), std::runtime_error);
    EXPECT_THROW(prefabs_.CreateVariant("assets/prefabs/Missing.prefab", "Ghost"), std::runtime_error);
    EXPECT_FALSE(std::filesystem::exists(dir_ / PrefabManager::PathFor("Ghost")));
}

// A variant edited on its own (Prefab Mode) keeps what it changed when its base changes, and takes the rest; so do
// its instances in the scene
TEST_F(PrefabTest, AVariantKeepsWhatItChangesAndFollowsItsBase)
{
    MakeEnemies();
    const std::string boss = prefabs_.CreateVariant(enemyPath_, "Boss");

    // More health for the boss, saved from Prefab Mode
    SceneDescription editing = PrefabManager::ToScene(*prefabs_.Find(boss));
    FindObject(editing, Engine::Scene::kPrefabRootId)->light->intensity = 500.0f;
    prefabs_.Save(boss, PrefabManager::FromScene(editing, "Boss"));
    EXPECT_EQ(prefabs_.Find(boss)->objects[0].prefabInstance->overrides, std::vector<std::string>{"light/intensity"});

    const std::uint32_t bossInScene = prefabs_.Instantiate(editor_.Description(), boss, editor_.NextId());
    ASSERT_NE(bossInScene, 0u);
    EXPECT_EQ(Object(bossInScene).light->intensity, 500.0f);

    // The Enemy prefab gets more max health and a red head (from one of its instances)
    Object(enemies_[0]).light->range = 150.0f;
    Part(enemies_[0], 2).renderable->albedoG = 0.0f;
    Part(enemies_[0], 2).renderable->albedoB = 0.0f;
    prefabs_.ApplyToPrefab(editor_.Description(), enemies_[0], editor_.NextId());

    const Prefab* variant = prefabs_.Find(boss);
    ASSERT_NE(variant, nullptr);
    EXPECT_EQ(variant->objects[0].light->intensity, 500.0f) << "its own change is kept";
    EXPECT_EQ(variant->objects[0].light->range, 150.0f) << "the rest follows the base";
    EXPECT_EQ(variant->Find(2)->renderable->albedoG, 0.0f);
    EXPECT_EQ(variant->objects[0].name, "Boss");

    EXPECT_EQ(Object(bossInScene).light->intensity, 500.0f) << "and so do the variant's instances";
    EXPECT_EQ(Object(bossInScene).light->range, 150.0f);
    EXPECT_EQ(Part(bossInScene, 2).renderable->albedoG, 0.0f);
    EXPECT_EQ(Object(enemies_[1]).light->intensity, 100.0f) << "the base's instances keep the base's health";
}

// A variant of a variant follows both: a part its base's base gains appears in each, under the same parent
TEST_F(PrefabTest, VariantsOfVariantsFollowTheirBases)
{
    MakeEnemies();
    const std::string boss = prefabs_.CreateVariant(enemyPath_, "Boss");
    const std::string bigBoss = prefabs_.CreateVariant(boss, "BigBoss");
    EXPECT_EQ(prefabs_.Find(bigBoss)->base, boss);
    EXPECT_EQ(prefabs_.VariantsOf(enemyPath_), std::vector<std::string>{boss}) << "only its own variants";
    EXPECT_EQ(prefabs_.VariantsOf(boss), std::vector<std::string>{bigBoss});

    // A sword in the right hand of an Enemy, applied to the Enemy prefab
    GameObject& sword = editor_.CreateObject("Sword");
    sword.transform = TransformComponent{.x = 0.2f};
    sword.renderable.emplace();
    const std::uint32_t swordId = sword.id;
    ASSERT_TRUE(editor_.SetParent(swordId, Part(enemies_[3], 4).id));
    prefabs_.ApplyToPrefab(editor_.Description(), enemies_[3], editor_.NextId());

    for (const std::string& path : {boss, bigBoss})
    {
        const Prefab* prefab = prefabs_.Find(path);
        ASSERT_NE(prefab, nullptr) << path;
        ASSERT_EQ(prefab->objects.size(), 5u) << path;
        EXPECT_EQ(prefab->objects.back().name, "Sword") << path;
        EXPECT_EQ(prefab->objects.back().parent, 4u) << path << ": in the right hand";
    }
}

// A variant whose base is gone, or whose bases lead back to itself, is read as it was saved (logged), not lost
TEST_F(PrefabTest, VariantsWithoutAUsableBaseKeepTheirObjects)
{
    std::filesystem::create_directories(dir_ / "assets" / "prefabs");
    // A variant called name of the prefab called base, with a part of its own
    const auto write = [this](const std::string& name, const std::string& base)
    {
        std::ofstream(dir_ / PrefabManager::PathFor(name)) << std::format(
            R"({{ "version": 1, "name": "{0}", "base": "assets/prefabs/{1}.prefab", "objects": [
                {{ "id": 1, "name": "{0}", "prefab": {{ "path": "assets/prefabs/{1}.prefab", "object": 1 }} }},
                {{ "id": 2, "name": "{0} Part", "parent": 1 }} ] }})",
            name, base);
    };
    const auto hasPart = [](const Prefab& prefab, const std::string& name)
    {
        return std::any_of(prefab.objects.begin(), prefab.objects.end(),
                           [&name](const GameObject& object) { return object.name == name; });
    };
    write("Orphan", "Gone");
    write("Ping", "Pong");
    write("Pong", "Ping");

    const Prefab* orphan = prefabs_.Find(PrefabManager::PathFor("Orphan"));
    ASSERT_NE(orphan, nullptr);
    EXPECT_EQ(orphan->objects.size(), 2u);
    EXPECT_TRUE(hasPart(*orphan, "Orphan Part"));

    for (const std::string name : {"Ping", "Pong"})
    {
        const Prefab* prefab = prefabs_.Find(PrefabManager::PathFor(name));
        ASSERT_NE(prefab, nullptr) << name;
        EXPECT_TRUE(hasPart(*prefab, name + " Part")) << name;
    }
}

// Editing a prefab on its own: the scene Prefab Mode edits gives the same prefab back; objects not under the root,
// and links to any prefab but a variant's base, are left out; an unpacked variant is an ordinary prefab again
TEST_F(PrefabTest, PrefabModeScenesGiveThePrefabBack)
{
    MakeEnemies();
    const std::string boss = prefabs_.CreateVariant(enemyPath_, "Boss");
    const Prefab variant = *prefabs_.Find(boss);

    SceneDescription scene = PrefabManager::ToScene(variant);
    EXPECT_EQ(scene.objects, variant.objects) << "links to the base kept";
    EXPECT_EQ(PrefabManager::FromScene(scene, "Boss"), variant);

    GameObject stray{.id = 20, .name = "Stray"};
    GameObject linked{.id = 21, .name = "Linked", .parent = Engine::Scene::kPrefabRootId};
    // Part of another prefab without its root (a whole instance would be a nested prefab, and keep its link)
    linked.prefabInstance = Engine::Scene::PrefabInstance{.prefab = "assets/prefabs/Other.prefab", .object = 2};
    scene.objects.push_back(stray);
    scene.objects.push_back(linked);
    const Prefab edited = PrefabManager::FromScene(scene, "Boss");
    EXPECT_EQ(edited.Find(20), nullptr) << "not under the root";
    ASSERT_NE(edited.Find(21), nullptr);
    EXPECT_FALSE(edited.Find(21)->prefabInstance);
    EXPECT_TRUE(edited.IsVariant());

    ASSERT_TRUE(PrefabManager::Unpack(scene, Engine::Scene::kPrefabRootId));
    const Prefab unpacked = PrefabManager::FromScene(scene, "Boss");
    EXPECT_FALSE(unpacked.IsVariant());
    for (const GameObject& object : unpacked.objects)
    {
        EXPECT_FALSE(object.prefabInstance) << object.name;
    }

    EXPECT_THROW(PrefabManager::FromScene(SceneDescription{}, "Empty"), std::invalid_argument);
}

// Deleting a prefab unpacks its instances in the scene and turns its variants into ordinary prefabs that keep what
// they have
TEST_F(PrefabTest, DeletingABaseMakesItsVariantsOrdinaryPrefabs)
{
    MakeEnemies();
    const std::string boss = prefabs_.CreateVariant(enemyPath_, "Boss");
    SceneDescription editing = PrefabManager::ToScene(*prefabs_.Find(boss));
    FindObject(editing, Engine::Scene::kPrefabRootId)->light->intensity = 500.0f;
    prefabs_.Save(boss, PrefabManager::FromScene(editing, "Boss"));

    const auto deleted = prefabs_.Delete(editor_.Description(), enemyPath_);
    EXPECT_EQ(deleted.instances, 6u);
    EXPECT_EQ(deleted.variants, 1u);
    EXPECT_FALSE(std::filesystem::exists(dir_ / enemyPath_));
    EXPECT_EQ(prefabs_.List(), std::vector<std::string>{boss});
    EXPECT_EQ(prefabs_.Find(enemyPath_), nullptr);

    EXPECT_EQ(editor_.Objects().size(), 24u) << "the objects stay";
    for (const GameObject& object : editor_.Objects())
    {
        EXPECT_FALSE(object.prefabInstance) << object.name;
    }

    prefabs_.Forget(); // read from its file
    const Prefab* ordinary = prefabs_.Find(boss);
    ASSERT_NE(ordinary, nullptr);
    EXPECT_FALSE(ordinary->IsVariant());
    ASSERT_EQ(ordinary->objects.size(), 4u);
    EXPECT_EQ(ordinary->objects[0].light->intensity, 500.0f);
    for (const GameObject& object : ordinary->objects)
    {
        EXPECT_FALSE(object.prefabInstance) << object.name;
    }
}

// ---------------------------------------------------------------- Hierarchies over time

// A parent moved every frame: its five children keep up, with no error building up over the frames
TEST(Hierarchy, ChildrenFollowAMovingParentOverManyFrames)
{
    using namespace Engine::Scene;
    Scene ecs;
    const Entity parent = ecs.CreateEntity();
    ecs.SetTransform(parent, {});
    std::vector<Entity> children;
    for (int i = 0; i < 5; ++i)
    {
        children.push_back(ecs.CreateEntity());
        ecs.AddComponent(children.back(), HierarchyComponent{.parent = parent.GetId(), .local = {.y = float(i + 1)}});
    }

    for (int frame = 1; frame <= 120; ++frame)
    {
        ecs.SetTransform(parent, {.x = 0.25f * float(frame), .z = -0.5f * float(frame)});
        Engine::Systems::HierarchySystem::Update(ecs);
        for (int i = 0; i < 5; ++i)
        {
            ExpectNear(*ecs.GetTransform(children[static_cast<std::size_t>(i)]),
                       {.x = 0.25f * float(frame), .y = float(i + 1), .z = -0.5f * float(frame)});
        }
    }
}

namespace
{
    // root, then four levels under it, each one unit along X from the one above
    struct FiveLevels
    {
        Engine::Scene::Scene ecs;
        std::vector<Engine::Scene::Entity> levels;

        FiveLevels()
        {
            using namespace Engine::Scene;
            levels.push_back(ecs.CreateEntity());
            ecs.SetTransform(levels[0], {});
            for (int i = 1; i < 5; ++i)
            {
                levels.push_back(ecs.CreateEntity());
                ecs.AddComponent(levels.back(),
                                 HierarchyComponent{.parent = levels[static_cast<std::size_t>(i - 1)].GetId(),
                                                    .local = {.x = 1.0f}});
            }
            Engine::Systems::HierarchySystem::Update(ecs);
        }

        TransformComponent At(std::size_t level) { return *ecs.GetTransform(levels[level]); }
        void SetLocal(std::size_t level, const TransformComponent& local)
        {
            ecs.TryGetComponent<Engine::Scene::HierarchyComponent>(levels[level])->local = local;
            Engine::Systems::HierarchySystem::Update(ecs);
        }
    };
}

TEST(Hierarchy, FiveLevelsAddUp)
{
    FiveLevels tree;
    for (std::size_t level = 0; level < 5; ++level)
    {
        ExpectNear(tree.At(level), {.x = float(level)});
    }
}

TEST(Hierarchy, MovingARootMovesEveryLevel)
{
    FiveLevels tree;
    tree.ecs.SetTransform(tree.levels[0], {.y = 3.0f});
    Engine::Systems::HierarchySystem::Update(tree.ecs);
    ExpectNear(tree.At(4), {.x = 4.0f, .y = 3.0f});
}

TEST(Hierarchy, TurningARootTurnsEveryLevelAroundIt)
{
    FiveLevels tree;
    tree.ecs.SetTransform(tree.levels[0], {.rotation = 90.0f});
    Engine::Systems::HierarchySystem::Update(tree.ecs);
    ExpectNear(tree.At(2), {.z = -2.0f, .rotation = 90.0f});
    ExpectNear(tree.At(4), {.z = -4.0f, .rotation = 90.0f});
}

TEST(Hierarchy, ScalingARootSpreadsAndScalesEveryLevel)
{
    FiveLevels tree;
    tree.ecs.SetTransform(tree.levels[0], {.scale = 2.0f});
    Engine::Systems::HierarchySystem::Update(tree.ecs);
    ExpectNear(tree.At(4), {.x = 8.0f, .scale = 2.0f});
}

// A level in the middle takes only what is under it along
TEST(Hierarchy, ChangingAMiddleLevelLeavesWhatIsAboveIt)
{
    FiveLevels tree;
    tree.SetLocal(2, {.x = 1.0f, .rotation = 90.0f});
    ExpectNear(tree.At(1), {.x = 1.0f});
    ExpectNear(tree.At(2), {.x = 2.0f, .rotation = 90.0f});
    ExpectNear(tree.At(3), {.x = 2.0f, .z = -1.0f, .rotation = 90.0f});
    ExpectNear(tree.At(4), {.x = 2.0f, .z = -2.0f, .rotation = 90.0f});
}

// ---------------------------------------------------------------- Prefab guids

TEST_F(PrefabTest, APrefabKeepsItsGuidWhenSavedAgain)
{
    PrefabManager manager(dir_);
    manager.Save("assets/prefabs/Enemy.prefab", PrefabManager::Capture(PlayerScene(), 1, "Enemy"));
    const std::string guid = manager.Find("assets/prefabs/Enemy.prefab")->guid;
    ASSERT_EQ(guid.size(), 32u);

    manager.Save("assets/prefabs/Enemy.prefab", PrefabManager::Capture(PlayerScene(), 1, "Enemy"));
    manager.Forget();
    EXPECT_EQ(manager.Find("assets/prefabs/Enemy.prefab")->guid, guid) << "read back from the file";

    manager.Save("assets/prefabs/Other.prefab", PrefabManager::Capture(PlayerScene(), 1, "Other"));
    EXPECT_NE(manager.Find("assets/prefabs/Other.prefab")->guid, guid);
}

TEST_F(PrefabTest, LinksFollowAPrefabThatMoved)
{
    PrefabManager manager(dir_);
    manager.Save("assets/prefabs/Enemy.prefab", PrefabManager::Capture(PlayerScene(), 1, "Enemy"));
    const std::string guid = manager.Find("assets/prefabs/Enemy.prefab")->guid;
    Engine::Scene::SceneDescription scene;
    scene.objects.push_back(GameObject{.id = 5, .name = "Enemy"});
    scene.objects[0].prefabInstance =
        Engine::Scene::PrefabInstance{.prefab = "assets/prefabs/Enemy.prefab", .guid = guid, .object = 1};
    EXPECT_EQ(manager.RelinkMoved(scene), 0u) << "the file is where the link says";

    std::filesystem::create_directories(dir_ / "assets/prefabs/foes");
    std::filesystem::rename(dir_ / "assets/prefabs/Enemy.prefab", dir_ / "assets/prefabs/foes/Boss.prefab");
    manager.Forget();

    EXPECT_EQ(manager.RelinkMoved(scene), 1u);
    EXPECT_EQ(scene.objects[0].prefabInstance->prefab, "assets/prefabs/foes/Boss.prefab");
}
