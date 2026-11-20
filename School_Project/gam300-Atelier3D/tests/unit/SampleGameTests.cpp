#include "engine/assets/ResourceManager.hpp"
#include "engine/editor/EditorScene.hpp"
#include "engine/game/BlockPhysics.hpp"
#include "engine/logic/Library.hpp"
#include "engine/game/GameProject.hpp"
#include "engine/game/GameRuntime.hpp"
#include "engine/game/PrefabManager.hpp"
#include "engine/input/Input.hpp"
#include "engine/input/InputActions.hpp"
#include "engine/logic/LogicRunner.hpp"
#include "engine/physics/PhysicsWorld.hpp"
#include "engine/scene/Components.hpp"
#include "engine/scene/Scene.hpp"
#include "engine/scene/systems/HierarchySystem.hpp"
#include "engine/scene/systems/PhysicsSystem.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

// The sample game committed in the repo (projects/atelier_editor): the genre games new games start from (rpg, fps,
// racing) and one demo scene per engine feature for the milestones. These tests keep every scene readable as the
// formats change, and check that each still holds what it shows.

using Engine::Game::GameProject;
using Engine::Graphics::LightType;
using Engine::Scene::GameObject;
using Engine::Scene::SceneDescription;

namespace
{
    const char* kSampleGameDir = ATELIER_SAMPLE_GAME_DIR;
    // The demos ship no models, textures or sounds: a crate is a built-in cube the size its model was
    constexpr float kCrateSize = 0.7f;
    // Every scene of the sample game, in the order File > Game Scenes lists them
    const std::vector<std::string> kDemoScenes = {
        "data/scenes/audio.json",     "data/scenes/ecs.json",         "data/scenes/fps.json",
        "data/scenes/fps_menu.json",  "data/scenes/input.json",       "data/scenes/prefabs.json",
        "data/scenes/racing.json",    "data/scenes/racing_menu.json", "data/scenes/rendering.json",
        "data/scenes/resources.json", "data/scenes/rpg.json",         "data/scenes/rpg_menu.json"};
    const std::vector<std::string> kGenreMenus = {"data/scenes/rpg_menu.json", "data/scenes/fps_menu.json",
                                                  "data/scenes/racing_menu.json"};

    bool IsMenu(const std::string& scene)
    {
        return scene.ends_with("_menu.json");
    }
    const std::vector<std::string> kGenreGames = {"data/scenes/rpg.json", "data/scenes/fps.json",
                                                  "data/scenes/racing.json"};

    SceneDescription Load(const std::string& scene)
    {
        SceneDescription loaded;
        const auto report = GameProject(kSampleGameDir).LoadSceneChecked(scene, loaded);
        EXPECT_TRUE(report.ok) << scene << ": " << report.error;
        return loaded;
    }

    std::ptrdiff_t CountIf(const SceneDescription& scene, bool (*predicate)(const GameObject&))
    {
        return std::count_if(scene.objects.begin(), scene.objects.end(), predicate);
    }

    const GameObject* Named(const SceneDescription& scene, const std::string& name)
    {
        const auto it = std::find_if(scene.objects.begin(), scene.objects.end(),
                                     [&name](const GameObject& object) { return object.name == name; });
        return it == scene.objects.end() ? nullptr : &*it;
    }

    bool IsMainCamera(const GameObject& o)
    {
        return o.transform && o.camera && o.camera->primary;
    }

    bool IsSharedCrate(const GameObject& o)
    {
        return o.transform && o.renderable && o.renderable->mesh.empty() && o.renderable->model.empty() &&
               o.renderable->texture.empty() && o.renderable->shader.empty() &&
               o.renderable->shape == Engine::Game::Shape::Cube && o.renderable->shapeSizeX == kCrateSize &&
               o.renderable->shapeSizeY == kCrateSize && o.renderable->shapeSizeZ == kCrateSize;
    }

    std::string ReadFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }
}

// The sample game is a real game project, starts on the rendering demo, and File > Game Scenes would list exactly the
// demo scenes and the genre games.
TEST(SampleGame, IsAGameProjectThatStartsWithTheRenderingDemo)
{
    ASSERT_TRUE(GameProject::IsGameProject(kSampleGameDir));
    const GameProject project(kSampleGameDir);
    EXPECT_EQ(project.Settings().name, "AtelierDemos");
    EXPECT_EQ(project.Settings().startScene, "data/scenes/rendering.json");

    const std::vector<std::string> extensions{".json"};
    std::vector<std::string_view> views(extensions.begin(), extensions.end());
    EXPECT_EQ(project.FindAssets("data/scenes", views), kDemoScenes); // what File > Game Scenes lists
}

// Every demo scene loads with no warnings and has exactly one Main camera.
TEST(SampleGame, EveryDemoSceneLoadsWithoutProblems)
{
    for (const std::string& scene : kDemoScenes)
    {
        SceneDescription loaded;
        const auto report = GameProject(kSampleGameDir).LoadSceneChecked(scene, loaded);
        ASSERT_TRUE(report.ok) << scene << ": " << report.error;
        EXPECT_TRUE(report.warnings.empty()) << scene << ": " << report.warnings.front();
        EXPECT_EQ(CountIf(loaded, IsMainCamera), 1) << scene << " needs one Main camera";
    }
}

// The committed demo files are byte for byte what SaveSceneFile writes, so the save > load > save requirement holds for
// real files too.
TEST(SampleGame, DemoScenesAreExactlyWhatTheEngineSaves)
{
    // Load + save leaves each committed file byte for byte the same: the round trip the serializer promises
    const auto out = std::filesystem::temp_directory_path() / "atelier_sample_round_trip";
    std::filesystem::remove_all(out);
    for (const std::string& scene : kDemoScenes)
    {
        const auto saved = out / std::filesystem::path(scene).filename();
        Engine::Scene::SaveSceneFile(saved, Load(scene));
        EXPECT_EQ(ReadFile(saved), ReadFile(std::filesystem::path(kSampleGameDir) / scene)) << scene;
    }
    std::filesystem::remove_all(out);
}

// Opening each demo in the editor keeps every object and component unchanged.
TEST(SampleGame, EditorKeepsEveryObjectOfEveryDemo)
{
    for (const std::string& scene : kDemoScenes)
    {
        const SceneDescription loaded = Load(scene);
        Engine::Editor::EditorScene editor;
        editor.SetDescription(loaded);
        EXPECT_EQ(editor.Description(), loaded) << scene;
    }
}

// The rendering demo has what the requirement asks for: 100 crates, 1 directional and 4 point lights (and no
// player, so the Main camera stays put).
TEST(SampleGame, RenderingDemoHas100TexturedObjectsAndFiveLights)
{
    const SceneDescription scene = Load("data/scenes/rendering.json");

    EXPECT_EQ(CountIf(scene, IsSharedCrate), 100);
    EXPECT_EQ(CountIf(scene, [](const GameObject& o) { return o.light && o.light->type == LightType::Directional; }),
              1);
    EXPECT_EQ(CountIf(scene, [](const GameObject& o) { return o.light && o.light->type == LightType::Point; }), 4);
    EXPECT_EQ(CountIf(scene, [](const GameObject& o) { return o.player.has_value(); }), 0); // the Main camera stays
}

// The 10 crates are built-in cubes, and nothing in the demo is drawn from a file: it reads none.
TEST(SampleGame, ResourceDemoIsBuiltFromShapesAndReadsNoFiles)
{
    const SceneDescription scene = Load("data/scenes/resources.json");
    EXPECT_EQ(CountIf(scene, IsSharedCrate), 10);

    // What GameRuntime loads for the scene: one reference per object using a file
    Engine::Assets::ResourceManager resources(kSampleGameDir);
    for (const GameObject& object : scene.objects)
    {
        if (object.renderable && !object.renderable->mesh.empty())
            resources.LoadMesh(object.renderable->mesh);
        if (object.renderable && !object.renderable->model.empty())
            resources.LoadModel(object.renderable->model);
        if (object.renderable && !object.renderable->texture.empty())
            resources.LoadTexture(object.renderable->texture);
    }

    EXPECT_TRUE(resources.ListAssets().empty());
}

// The ECS demo has a spawn template, a spinner, a lamp and a player, and spawning 1,000 copies of the template works on
// the real scene.
TEST(SampleGame, EcsDemoSpawnsAThousandCopiesAndHasAnObjectPerSystem)
{
    const SceneDescription scene = Load("data/scenes/ecs.json");
    const GameObject* source = Named(scene, "Spawn Template");
    ASSERT_NE(source, nullptr);
    ASSERT_TRUE(source->transform && source->renderable);

    const GameObject* spinner = Named(scene, "Spinner");
    ASSERT_TRUE(spinner && spinner->renderable && spinner->renderable->rotationSpeed != 0.0f);
    const GameObject* lamp = Named(scene, "Lamp");
    ASSERT_TRUE(lamp && lamp->light && lamp->light->type == LightType::Point);
    const GameObject* player = Named(scene, "Player");
    ASSERT_TRUE(player && player->player && !player->player->followCamera); // the overview camera stays

    Engine::Game::GameRuntime runtime;
    runtime.SetSimulating(false);
    runtime.LoadProject(GameProject(kSampleGameDir));
    ASSERT_TRUE(runtime.Start(scene));
    EXPECT_EQ(runtime.SpawnEntities(1000, source->id), 1000u);
    EXPECT_EQ(runtime.EntityCount(), scene.objects.size() + 1000);
}

// The audio demo has a looping 3D sound source on the radio, non-3D music on the music bus (silent: the demos ship no
// sound files, a clip is chosen in the Inspector), and the camera follows the player around.
TEST(SampleGame, AudioDemoHasALooping3DSourceAndBackgroundMusic)
{
    const SceneDescription scene = Load("data/scenes/audio.json");

    const GameObject* radio = Named(scene, "Radio");
    ASSERT_TRUE(radio && radio->transform && radio->renderable && radio->audioSource);
    EXPECT_TRUE(radio->audioSource->loop);
    EXPECT_TRUE(radio->audioSource->spatial);
    EXPECT_EQ(radio->audioSource->bus, "sfx");

    const GameObject* music = Named(scene, "Background Music");
    ASSERT_TRUE(music && music->transform && music->audioSource);
    EXPECT_TRUE(music->audioSource->loop);
    EXPECT_FALSE(music->audioSource->spatial);
    EXPECT_EQ(music->audioSource->bus, "music");

    // The listener is the camera, which follows the player around the radio
    const GameObject* player = Named(scene, "Player");
    ASSERT_TRUE(player && player->transform && player->player && player->player->followCamera);
}

// The input demo has a player the camera follows, and the game's input_actions.json loads cleanly with every move and
// look action.
// The input demo's waypoints are controlled by blocks: each has a script under a "when" block, the yellow one counts
// its jumps in a variable, and the blue one lights up while the player is near it
TEST(SampleGame, InputDemoWaypointsAreControlledByBlocks)
{
    const SceneDescription scene = Load("data/scenes/input.json");
    for (const char* name :
         {"Waypoint North (red)", "Waypoint East (green)", "Waypoint South (blue)", "Waypoint West (yellow)"})
    {
        const GameObject* waypoint = Named(scene, name);
        ASSERT_TRUE(waypoint && waypoint->logic) << name;
        ASSERT_FALSE(waypoint->logic->scripts.empty()) << name;
        for (const auto& script : waypoint->logic->scripts)
        {
            EXPECT_EQ(Engine::Logic::InfoOf(script.blocks.front().op).shape, Engine::Logic::BlockShape::Hat) << name;
        }
    }
    ASSERT_NE(Engine::Logic::FindVariable(*Named(scene, "Waypoint West (yellow)")->logic, "jumps"), nullptr);

    // Run the blue one's blocks: its own colour while the player is far, white once it comes within 3
    const GameObject& south = *Named(scene, "Waypoint South (blue)");
    Engine::Scene::Scene ecs;
    const Engine::Scene::Entity waypoint = ecs.CreateEntity();
    ecs.SetTransform(waypoint, *south.transform);
    ecs.AddComponent(waypoint, Engine::Scene::MeshRendererComponent{});
    const Engine::Scene::Entity player = ecs.CreateEntity();
    ecs.SetTransform(player, *Named(scene, "Player")->transform);
    Engine::Logic::LogicRunner runner;
    runner.SetObjects({{.name = "Player", .entity = player.GetId()}});
    runner.Set(south.id, south.name, waypoint.GetId(), *south.logic);
    runner.Update(ecs, 1.0 / 60.0, {});
    const auto* mesh = ecs.TryGetComponent<Engine::Scene::MeshRendererComponent>(waypoint);
    EXPECT_FLOAT_EQ(mesh->albedoB, south.renderable->albedoB);
    EXPECT_FLOAT_EQ(mesh->albedoR, south.renderable->albedoR);

    Engine::Scene::TransformComponent near = *south.transform;
    near.z -= 1.0f;
    ecs.SetTransform(player, near);
    runner.Update(ecs, 1.0 / 60.0, {});
    EXPECT_FLOAT_EQ(mesh->albedoR, 1.0f);
    EXPECT_FLOAT_EQ(mesh->albedoG, 1.0f);
    EXPECT_TRUE(runner.Problems(south.id).empty());
}

// Every demo is moved with blocks: the player (or, with no player, the Main camera) has a "speed" variable and a script
// that moves it while W is held, faster with Shift. The Player component no longer moves it (its speed is 0): it only
// keeps the camera behind it.
namespace
{
    Engine::Logic::Library SampleLibrary();
}

TEST(SampleGame, EveryDemoMovesWithBlocks)
{
    for (const std::string& name : kDemoScenes)
    {
        if (IsMenu(name))
        {
            continue; // a main menu has no one to move
        }
        const SceneDescription scene = Load(name);
        const auto mover =
            std::find_if(scene.objects.begin(), scene.objects.end(), [](const GameObject& object)
                         { return object.logic && Engine::Logic::FindVariable(*object.logic, "speed"); });
        ASSERT_NE(mover, scene.objects.end()) << name;
        EXPECT_TRUE(mover->player || IsMainCamera(*mover)) << name;
        if (mover->player)
        {
            EXPECT_FLOAT_EQ(mover->player->speed, 0.0f) << name;
        }

        Engine::Scene::Scene ecs;
        const Engine::Scene::Entity entity = ecs.CreateEntity();
        ecs.SetTransform(entity, *mover->transform);
        if (mover->player)
        {
            ecs.AddComponent(entity, *mover->player); // and raise its camera (the shooter's crouch)
        }
        Engine::Logic::LogicRunner runner;
        runner.SetLibrary(SampleLibrary());
        runner.Set(mover->id, mover->name, entity.GetId(), *mover->logic);
        std::set<int> down{Engine::Input::Key_W};
        const Engine::Logic::LogicRunner::Keys keys{.down = [&down](int code) { return down.contains(code); },
                                                    .pressed = [](int) { return false; }};
        const float start = mover->transform->z;
        runner.Update(ecs, 1.0 / 60.0, keys);
        const float walked = start - ecs.GetTransform(entity)->z;
        EXPECT_GT(walked, 0.0f) << name << ": W moves it forward (-Z)";
        down.insert(Engine::Input::Key_LeftShift);
        runner.Update(ecs, 1.0 / 60.0, keys);
        EXPECT_GT(start - ecs.GetTransform(entity)->z - walked, walked * 1.5f) << name << ": Shift is faster";
        EXPECT_TRUE(runner.Problems(mover->id).empty()) << name;
    }
}

TEST(SampleGame, InputDemoHasAFollowedPlayerAndRemappableControls)
{
    const SceneDescription scene = Load("data/scenes/input.json");
    const GameObject* player = Named(scene, "Player");
    ASSERT_TRUE(player && player->transform && player->renderable && player->player);
    EXPECT_TRUE(player->player->followCamera);

    const GameProject project(kSampleGameDir);
    std::vector<std::string> problems;
    const auto actions = Engine::Input::InputActions::Load(project.InputActionsPath(), &problems);
    EXPECT_TRUE(problems.empty()) << problems.front();
    for (const auto action :
         {Engine::Input::Action::MoveForward, Engine::Input::Action::MoveBack, Engine::Input::Action::MoveLeft,
          Engine::Input::Action::MoveRight, Engine::Input::Action::Look})
    {
        EXPECT_NE(actions.Find(action), nullptr) << action;
    }
}

namespace
{
    // A genre game in an ECS scene and a physics world, run frame by frame the way GameRuntime runs Play: the blocks
    // (walking Dynamic bodies through BlockPhysics), then the physics step
    Engine::Logic::Library SampleLibrary()
    {
        Engine::Logic::Library library;
        Engine::Logic::LoadLibraryFile(std::filesystem::path(kSampleGameDir) / Engine::Logic::kLibraryFile, library);
        return library;
    }

    struct Playing
    {
        Engine::Scene::Scene ecs;
        Engine::Physics::PhysicsWorld world;
        Engine::Game::BlockPhysics physics;
        Engine::Logic::LogicRunner runner;
        std::map<std::string, Engine::Scene::Entity> entities;
        std::map<std::string, std::uint32_t> ids;
        std::set<int> down;
        std::set<int> pressed;
        bool mouse = false; // the left mouse button is held
        bool mouseRight = false;

        explicit Playing(const SceneDescription& scene)
        {
            EXPECT_TRUE(world.Init());
            std::vector<Engine::Logic::LogicRunner::NamedEntity> named;
            for (const GameObject& object : scene.objects)
            {
                const Engine::Scene::Entity entity = ecs.CreateEntity();
                // The parts of a model (a child) are drawn with it; only the objects themselves take part here
                if (object.transform && object.parent == 0)
                {
                    ecs.SetTransform(entity, *object.transform);
                }
                if (object.renderable)
                {
                    // Its built-in shape, which an Auto collider fits (a model is not loaded here)
                    ecs.AddComponent(entity, Engine::Scene::MeshRendererComponent{.shape = object.renderable->shape});
                }
                if (object.rigidBody && object.parent == 0)
                {
                    ecs.AddComponent(entity, *object.rigidBody);
                }
                if (object.player)
                {
                    ecs.AddComponent(entity, *object.player);
                }
                if (object.camera)
                {
                    ecs.AddComponent(entity, *object.camera);
                }
                if (object.screen)
                {
                    ecs.AddComponent(entity, *object.screen);
                }
                entities.emplace(object.name, entity);
                ids.emplace(object.name, object.id);
                named.push_back({.name = object.name, .entity = entity.GetId()});
            }
            runner.SetObjects(std::move(named));
            runner.SetLibrary(SampleLibrary());
            for (const GameObject& object : scene.objects)
            {
                if (object.logic)
                {
                    runner.Set(object.id, object.name, entities.at(object.name).GetId(), *object.logic);
                }
                // A child is placed from its parent every frame, as in the game (the sword's hitbox in front of
                // the player), so the blocks can find where it is
                const auto parent =
                    std::find_if(scene.objects.begin(), scene.objects.end(),
                                 [&object](const GameObject& each) { return each.id == object.parent; });
                if (object.transform && parent != scene.objects.end())
                {
                    const Engine::Scene::Entity entity = entities.at(object.name);
                    ecs.SetTransform(entity, *object.transform);
                    ecs.AddComponent(entity,
                                     Engine::Scene::HierarchyComponent{.parent = entities.at(parent->name).GetId(),
                                                                       .local = *object.transform});
                }
            }
            runner.SetPhysics(&physics);
            Engine::Systems::HierarchySystem::Update(ecs);
        }

        ~Playing() { world.Shutdown(); }

        void Frames(int count)
        {
            const Engine::Logic::LogicRunner::Keys keys{.down = [this](int code) { return down.contains(code); },
                                                        .pressed = [this](int code) { return pressed.contains(code); },
                                                        .mouseButton = [this](int button)
                                                        {
                                                            return button == Engine::Input::Mouse_Right  ? mouseRight
                                                                   : button == Engine::Input::Mouse_Left ? mouse
                                                                                                         : false;
                                                        }};
            constexpr double kFrame = 1.0 / 60.0;
            for (int i = 0; i < count; ++i)
            {
                physics.Bind(ecs, world);
                runner.Update(ecs, kFrame, keys);
                physics.BeforeStep(kFrame);
                Engine::Systems::PhysicsSystem::Update(ecs, world, kFrame);
                physics.AfterStep();
                Engine::Systems::HierarchySystem::Update(ecs);
                pressed.clear(); // a press lasts one frame
            }
        }

        Engine::Scene::TransformComponent Where(const std::string& name) const
        {
            return *ecs.GetTransform(entities.at(name));
        }

        // Puts the player where an object is (at the player's own height)
        void PlayerTo(const std::string& name)
        {
            const auto there = Where(name);
            PlayerAt(there.x, there.z);
        }

        void PlayerAt(float x, float z)
        {
            auto player = Where("Player");
            player.x = x;
            player.z = z;
            ecs.SetTransform(entities.at("Player"), player);
        }

        double Variable(const std::string& object, const std::string& name) const
        {
            for (const auto& [variable, value] : runner.Variables(ids.at(object)))
            {
                if (variable == name)
                {
                    return value;
                }
            }
            ADD_FAILURE() << object << " has no variable " << name;
            return 0.0;
        }

        float Health(const std::string& object) { return static_cast<float>(Variable(object, "health")); }
    };
}

// New Game offers an empty world and the three genre games, each a scene of the sample game
TEST(SampleGame, NewGamesStartFromTheGenreGames)
{
    using Engine::Game::GameTemplate;
    EXPECT_EQ(Engine::Game::kGameTemplates,
              (std::array{GameTemplate::Empty, GameTemplate::Rpg, GameTemplate::Fps, GameTemplate::Racing}));
    for (std::size_t i = 1; i < Engine::Game::kGameTemplates.size(); ++i)
    {
        const std::string scene =
            std::string("data/scenes/") + Engine::Game::ToString(Engine::Game::kGameTemplates[i]) + ".json";
        EXPECT_NE(std::find(kGenreGames.begin(), kGenreGames.end(), scene), kGenreGames.end()) << scene;
    }
}

// Each genre game plays with its physics: a few seconds of going forward, jumping and attacking, and no block runs
// into a problem. The player is a Dynamic body, kept upright.
TEST(SampleGame, EveryGenreGameRunsItsBlocksWithoutProblems)
{
    for (const std::string& name : kGenreGames)
    {
        const SceneDescription scene = Load(name);
        const GameObject* player = Named(scene, "Player");
        ASSERT_NE(player, nullptr) << name;
        EXPECT_TRUE(player->player && player->player->followCamera) << name;
        ASSERT_TRUE(player->rigidBody) << name;
        EXPECT_EQ(player->rigidBody->type, Engine::Scene::BodyType::Dynamic) << name;
        Playing game(scene);
        game.down.insert(Engine::Input::Key_W);
        for (int second = 0; second < 5; ++second)
        {
            game.pressed.insert(Engine::Input::Key_Space);
            game.pressed.insert(Engine::Input::Key_F); // the arena's and the race's attack
            game.mouse = true;                         // the adventure's: a click
            game.Frames(30);
            game.mouse = false;
            game.Frames(30);
        }
        for (const GameObject& object : scene.objects)
        {
            for (const auto& problem : game.runner.Problems(object.id))
            {
                ADD_FAILURE() << name << ": " << object.name << ": " << problem.message;
            }
        }
        const auto where = game.Where("Player");
        EXPECT_NEAR(where.rotationX, 0.0f, 1.0f) << name << ": the player does not tip over (degrees)";
        EXPECT_NEAR(where.rotationZ, 0.0f, 1.0f) << name;
        EXPECT_GT(where.y, -1.0f) << name << ": the player stays on the ground";
    }
}

// The player lands on the floor, a wall stops it however long it walks into it, and a jump goes up and comes back
TEST(SampleGame, PlayersStandOnTheFloorAreStoppedByWallsAndJump)
{
    Playing game(Load("data/scenes/fps.json"));
    game.Frames(60);
    const float standing = game.Where("Player").y;
    EXPECT_NEAR(standing, 0.9f, 0.1f) << "the capsule (1.8 tall) stands on the floor";

    // Backwards (+Z) into the south wall, whose inside is at z 32
    game.down.insert(Engine::Input::Key_S);
    game.Frames(300);
    game.down.clear();
    const float z = game.Where("Player").z;
    EXPECT_GT(z, 31.0f) << "it walked there";
    EXPECT_LT(z, 32.0f) << "and the wall stopped it";

    game.pressed.insert(Engine::Input::Key_Space);
    game.Frames(15);
    EXPECT_GT(game.Where("Player").y, standing + 0.4f) << "Space jumps";
    game.Frames(120);
    EXPECT_NEAR(game.Where("Player").y, standing, 0.1f) << "and it lands again";
}

// Walking into a hay bale pushes it along
TEST(SampleGame, AdventureHeroPushesWhatTheyWalkInto)
{
    Playing game(Load("data/scenes/rpg.json"));
    const float bale = game.Where("Hay Bale 1").z;
    game.PlayerAt(game.Where("Hay Bale 1").x, bale + 2.0f); // facing -Z, towards it
    game.Frames(30);
    game.down.insert(Engine::Input::Key_W);
    game.Frames(90);
    EXPECT_LT(game.Where("Hay Bale 1").z, bale - 1.0f);
}

// The adventure: walking onto a spellbook tells the villager, who counts them
TEST(SampleGame, AdventureSpellbooksAreCountedByTheVillager)
{
    Playing game(Load("data/scenes/rpg.json"));
    game.Frames(2);
    EXPECT_DOUBLE_EQ(game.Variable("Villager", "books found"), 0.0);
    for (const char* book : {"Spellbook 1", "Spellbook 2", "Spellbook 3"})
    {
        game.PlayerTo(book);
        game.Frames(3);
    }
    EXPECT_DOUBLE_EQ(game.Variable("Villager", "books found"), 3.0);
}

// The tactical shooter's helpers: a round under way, and the player's view pointed at an object's middle
namespace
{
    void StartRound(Playing& game)
    {
        game.Frames(200); // the round's countdown
        ASSERT_DOUBLE_EQ(game.Variable("Match", "active"), 1.0);
    }

    void AimAt(Playing& game, const std::string& name)
    {
        const auto player = game.Where("Player");
        const auto target = game.Where(name);
        const glm::vec3 eye(player.x, player.y + 0.7f, player.z);
        game.physics.SetAim(eye, glm::normalize(glm::vec3(target.x, target.y + 0.9f, target.z) - eye));
    }

    // Stands the player a few steps from an enemy with nothing between them, and aims at it
    void StandBefore(Playing& game, const std::string& enemy)
    {
        const auto there = game.Where(enemy);
        game.PlayerAt(there.x, there.z + 4.0f);
        game.Frames(1);
        AimAt(game, enemy);
    }
}

// Holding the mouse fires the rifle at what the view is on: only that enemy is hurt, by whole shots, and the magazine
// goes down with them
TEST(SampleGame, FpsRifleHurtsWhatItIsAimedAtAndUsesAmmo)
{
    Playing game(Load("data/scenes/fps.json"));
    StartRound(game);
    StandBefore(game, "Enemy 1");
    const double before = game.Variable("Arms", "r ammo");
    game.mouse = true;
    game.Frames(14); // two shots, 0.1 s apart
    game.mouse = false;
    game.Frames(2);
    EXPECT_LT(game.Variable("Arms", "r ammo"), before);
    EXPECT_LT(game.Health("Enemy 1"), 100.0f);
    EXPECT_FLOAT_EQ(game.Health("Enemy 3"), 100.0f);
    EXPECT_DOUBLE_EQ(100.0 - game.Health("Enemy 1"), (before - game.Variable("Arms", "r ammo")) * 34.0);
}

// A shot stops at the first solid thing: an enemy behind a wall is safe, and cannot see the player either
TEST(SampleGame, FpsWallsStopShotsAndSight)
{
    Playing game(Load("data/scenes/fps.json"));
    StartRound(game);
    auto enemy = game.Where("Enemy 1");
    enemy.x = 0.0f;
    enemy.z = -8.0f;
    game.ecs.SetTransform(game.entities.at("Enemy 1"), enemy);
    game.PlayerAt(-24.0f, 8.0f); // the far side of the block between the yard and site A
    game.Frames(1);
    AimAt(game, "Enemy 1");
    const Engine::Scene::Entity player = game.entities.at("Player");
    const Engine::Scene::Entity seen = game.entities.at("Enemy 1");
    const auto shot = game.physics.CastRay(player, Engine::Scene::Entity{}, 0.0f, 250.0f, 0.0f, 0.0f);
    ASSERT_TRUE(shot.has_value());
    EXPECT_NE(shot->entity, seen.GetId());
    const auto sight = game.physics.CastRay(seen, player, 1.5f, 250.0f, 0.0f, 0.0f);
    EXPECT_FALSE(sight && sight->entity == player.GetId());
    game.PlayerAt(0.0f, -5.0f); // in the open with it
    game.Frames(1);
    const auto clear = game.physics.CastRay(seen, player, 1.5f, 250.0f, 0.0f, 0.0f);
    ASSERT_TRUE(clear.has_value());
    EXPECT_EQ(clear->entity, player.GetId());
}

// An empty magazine clicks and stays empty; R fills it from the spare ammunition, which goes down by what it took
TEST(SampleGame, FpsReloadFillsTheMagazineFromTheSpares)
{
    Playing game(Load("data/scenes/fps.json"));
    StartRound(game);
    game.physics.SetAim(glm::vec3(0.0f, 1.6f, 27.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // at the sky
    game.mouse = true;
    game.Frames(60 * 4); // much longer than 30 shots take
    game.mouse = false;
    EXPECT_DOUBLE_EQ(game.Variable("Arms", "r ammo"), 0.0);
    game.pressed.insert(Engine::Input::Key_R);
    game.Frames(1);
    EXPECT_DOUBLE_EQ(game.Variable("Arms", "busy"), 1.0) << "its hands are full";
    game.Frames(60 * 4);
    EXPECT_DOUBLE_EQ(game.Variable("Arms", "busy"), 0.0);
    EXPECT_DOUBLE_EQ(game.Variable("Arms", "r ammo"), 30.0);
    EXPECT_DOUBLE_EQ(game.Variable("Arms", "r spare"), 60.0);
    EXPECT_DOUBLE_EQ(game.Variable("Arms", "ammo"), 30.0) << "and the HUD's numbers follow";
}

// 2 takes out the pistol, which fires once a click however long it is held; 1 goes back to the rifle
TEST(SampleGame, FpsSecondWeaponIsSwitchedToAndFiresOnceAClick)
{
    Playing game(Load("data/scenes/fps.json"));
    StartRound(game);
    game.physics.SetAim(glm::vec3(0.0f, 1.6f, 27.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    game.pressed.insert(Engine::Input::Key_2);
    game.Frames(70);
    EXPECT_DOUBLE_EQ(game.Variable("Arms", "weapon"), 2.0);
    game.mouse = true;
    game.Frames(60);
    EXPECT_DOUBLE_EQ(game.Variable("Arms", "p ammo"), 11.0);
    game.mouse = false;
    game.Frames(30);
    game.mouse = true;
    game.Frames(5);
    EXPECT_DOUBLE_EQ(game.Variable("Arms", "p ammo"), 10.0);
    game.mouse = false;
    EXPECT_DOUBLE_EQ(game.Variable("Arms", "r ammo"), 30.0) << "the rifle's magazine is its own";
    game.Frames(30);
    game.pressed.insert(Engine::Input::Key_1);
    game.Frames(70);
    EXPECT_DOUBLE_EQ(game.Variable("Arms", "weapon"), 1.0);
}

// The right mouse button goes up to the sights while it is held
// The debugger on the real game: a breakpoint on the player's Shoot stops the world before the shot lands, and it lands
// when the game goes on
TEST(SampleGame, FpsBreakpointOnShootStopsBeforeTheShot)
{
    const SceneDescription scene = Load("data/scenes/fps.json");
    Playing game(scene);
    StartRound(game);
    StandBefore(game, "Enemy 1");

    const GameObject* player = Named(scene, "Player");
    ASSERT_TRUE(player && player->logic);
    std::uint32_t shoot = 0;
    for (const Engine::Logic::Script& script : player->logic->scripts)
    {
        if (script.blocks.front().op == Engine::Logic::Op::WhenIReceive && script.blocks.front().text == "fire rifle")
        {
            shoot = script.blocks[1].id;
            EXPECT_EQ(script.blocks[1].text, "Shoot");
        }
    }
    ASSERT_NE(shoot, 0u);
    game.runner.ToggleBreakpoint(player->id, shoot);

    game.mouse = true;
    for (int i = 0; i < 30 && !game.runner.IsPaused(); ++i)
    {
        game.Frames(1);
    }
    game.mouse = false;
    ASSERT_TRUE(game.runner.IsPaused());
    EXPECT_EQ(game.runner.Debug().pausedObject, player->id);
    EXPECT_EQ(game.runner.Debug().pausedBlock, shoot);
    EXPECT_FLOAT_EQ(game.Health("Enemy 1"), 100.0f) << "stopped before the shot";

    const double time = game.Variable("Match", "time");
    game.Frames(120);
    EXPECT_DOUBLE_EQ(game.Variable("Match", "time"), time) << "the whole game waits";
    EXPECT_FLOAT_EQ(game.Health("Enemy 1"), 100.0f);

    game.runner.ToggleBreakpoint(player->id, shoot);
    game.runner.Resume();
    game.Frames(3);
    EXPECT_FALSE(game.runner.IsPaused());
    EXPECT_LT(game.Health("Enemy 1"), 100.0f) << "and the shot lands";
}

TEST(SampleGame, FpsRightMouseAimsWhileHeld)
{
    Playing game(Load("data/scenes/fps.json"));
    StartRound(game);
    game.mouseRight = true;
    game.Frames(40);
    EXPECT_DOUBLE_EQ(game.Variable("Arms", "aiming"), 1.0);
    game.mouseRight = false;
    game.Frames(40);
    EXPECT_DOUBLE_EQ(game.Variable("Arms", "aiming"), 0.0);
    EXPECT_DOUBLE_EQ(game.Variable("Arms", "busy"), 0.0);
}

// The stair in site A's corner carries the player up to the raised platform, and that platform is metal underfoot
TEST(SampleGame, FpsStairsLeadUpToTheRaisedPosition)
{
    Playing game(Load("data/scenes/fps.json"));
    game.Frames(30); // the round has set it down at its spawn
    game.PlayerAt(-40.0f, 4.0f); // south of the stair, facing -Z
    game.Frames(30);
    game.down.insert(Engine::Input::Key_W);
    game.Frames(60 * 3);
    game.down.clear();
    const auto top = game.Where("Player");
    EXPECT_GT(top.y, 3.2f) << "on the platform, 2.6 up, at z " << top.z;
    EXPECT_LT(top.z, -4.0f);
    EXPECT_EQ(game.physics.SurfaceUnder(game.entities.at("Player")).value_or(""), "metal");
}

// The garage, the outpost and the containers are open: one walks in, and their far wall still stops one
TEST(SampleGame, FpsOpenBuildingsCanBeWalkedInto)
{
    struct Entry
    {
        const char* name;
        float x, z, heading;
        bool alongX;
        float insideFrom, insideTo;
        float floor; // how far its floor is off the ground
    };
    for (const Entry& entry :
         {Entry{"garage", 23.0f, -3.0f, 0.0f, false, -14.4f, -11.0f, 0.3f},
          Entry{"outpost", -33.0f, 26.0f, 90.0f, true, -39.1f, -36.0f, 0.91f},
          Entry{"container", -8.8f, 2.0f, 180.0f, false, 6.0f, 11.1f, 0.12f}})
    {
        Playing game(Load("data/scenes/fps.json"));
        game.Frames(30);
        game.PlayerAt(entry.x, entry.z);
        auto place = game.Where("Player");
        place.rotation = entry.heading;
        game.ecs.SetTransform(game.entities.at("Player"), place);
        game.Frames(10);
        game.down.insert(Engine::Input::Key_W);
        game.Frames(60 * 4);
        const auto reached = game.Where("Player");
        const float along = entry.alongX ? reached.x : reached.z;
        EXPECT_GT(along, entry.insideFrom) << entry.name << ": the far wall stops it";
        EXPECT_LT(along, entry.insideTo) << entry.name << ": it got inside";
        EXPECT_NEAR(reached.y, 0.9f + entry.floor, 0.12f) << entry.name << ": it stands on the floor, not in it";
    }
}

// Run into, an enemy stays on its feet, standing still (not only while it walks)
TEST(SampleGame, FpsEnemiesDoNotTopple)
{
    Playing game(Load("data/scenes/fps.json"));
    game.Frames(30); // before the round's countdown ends: they stand still
    const auto enemy = game.Where("Enemy 1");
    game.PlayerAt(enemy.x, enemy.z + 3.0f);
    game.down.insert(Engine::Input::Key_W);
    game.down.insert(Engine::Input::Key_LeftShift);
    game.Frames(90);
    const auto after = game.Where("Enemy 1");
    EXPECT_NEAR(after.rotationX, 0.0f, 1.0f);
    EXPECT_NEAR(after.rotationZ, 0.0f, 1.0f);
}

// Nothing in the arena stops an enemy on its way: with the player out of sight they all reach the far side of the
// middle, each along its own route
TEST(SampleGame, FpsEnemiesFollowTheirRoutesThroughTheArena)
{
    Playing game(Load("data/scenes/fps.json"));
    StartRound(game);
    for (int i = 0; i < 60 * 20; ++i)
    {
        game.PlayerAt(44.0f, 31.3f); // the far corner of the player's spawn, behind everything
        game.Frames(1);
    }
    for (const char* enemy : {"Enemy 1", "Enemy 2", "Enemy 3", "Enemy 4"})
    {
        EXPECT_GE(game.Variable(enemy, "leg"), 3.0) << enemy << " stands at " << game.Where(enemy).x << ", "
                                                    << game.Where(enemy).z;
    }
}

// An enemy that sees the player stops and fires, and its shots hurt
TEST(SampleGame, FpsEnemiesShootThePlayerTheySee)
{
    Playing game(Load("data/scenes/fps.json"));
    StartRound(game);
    const auto enemy = game.Where("Enemy 1");
    game.PlayerAt(enemy.x, enemy.z + 6.0f);
    game.Frames(60 * 6);
    EXPECT_LT(game.Health("Player"), 100.0f);
}

// All four down wins the round: it is counted, and the next one starts with everything put back
TEST(SampleGame, FpsClearingTheEnemiesWinsTheRoundAndStartsTheNext)
{
    Playing game(Load("data/scenes/fps.json"));
    StartRound(game);
    for (const char* enemy : {"Enemy 1", "Enemy 2", "Enemy 3", "Enemy 4"})
    {
        game.Frames(6);
        StandBefore(game, enemy);
        game.mouse = true;
        for (int i = 0; i < 120 && game.Health(enemy) > 0.0f; ++i)
        {
            StandBefore(game, enemy);
            game.Frames(1);
        }
        game.mouse = false;
        EXPECT_LE(game.Health(enemy), 0.0f) << enemy;
    }
    game.Frames(10);
    EXPECT_DOUBLE_EQ(game.Variable("Match", "wins"), 1.0);
    EXPECT_DOUBLE_EQ(game.Variable("Match", "left"), 0.0);
    game.Frames(60 * 8);
    EXPECT_DOUBLE_EQ(game.Variable("Match", "round"), 2.0);
    EXPECT_DOUBLE_EQ(game.Variable("Match", "left"), 4.0);
    EXPECT_FLOAT_EQ(game.Health("Enemy 1"), 100.0f) << "it stands up again";
    EXPECT_FLOAT_EQ(game.Health("Player"), 100.0f);
    EXPECT_DOUBLE_EQ(game.Variable("Arms", "r ammo"), 30.0);
    EXPECT_GT(game.Where("Player").z, 20.0f) << "back at its spawn";
}

// Being shot down loses the round, once, and the player comes back for the next
TEST(SampleGame, FpsDyingLosesTheRoundAndRespawns)
{
    Playing game(Load("data/scenes/fps.json"));
    StartRound(game);
    const auto enemy = game.Where("Enemy 2");
    for (int i = 0; i < 60 * 60 && game.Variable("Player", "alive") > 0.0; ++i)
    {
        game.PlayerAt(enemy.x, enemy.z + 5.0f);
        game.Frames(1);
    }
    EXPECT_DOUBLE_EQ(game.Variable("Player", "alive"), 0.0);
    game.Frames(10);
    EXPECT_DOUBLE_EQ(game.Variable("Match", "losses"), 1.0);
    EXPECT_DOUBLE_EQ(game.Variable("Arms", "alive"), 0.0) << "its hands are gone with it";
    game.Frames(60 * 6);
    EXPECT_DOUBLE_EQ(game.Variable("Player", "alive"), 1.0);
    EXPECT_FLOAT_EQ(game.Health("Player"), 100.0f);
    EXPECT_DOUBLE_EQ(game.Variable("Match", "round"), 2.0);
}

// The settings page: its buttons change the settings the game reads, within their limits, in every scene
TEST(SampleGame, FpsSettingsChangeWhatTheGameReads)
{
    Playing menu(Load("data/scenes/fps_menu.json"));
    menu.Frames(3);
    const auto setting = [&menu](const std::string& name) { return menu.runner.Settings().at(name); };
    EXPECT_DOUBLE_EQ(setting("master volume"), 100.0);
    EXPECT_NEAR(setting("sensitivity"), 0.12, 1e-6);
    const auto click = [&menu](const std::string& button)
    {
        menu.runner.ScreenClicked(menu.ids.at(button));
        menu.Frames(3);
    };
    click("Settings Button");
    click("Sensitivity Up");
    EXPECT_NEAR(setting("sensitivity"), 0.14, 1e-6);
    click("Music volume Down");
    EXPECT_DOUBLE_EQ(setting("music volume"), 50.0);
    click("Master volume Up");
    EXPECT_DOUBLE_EQ(setting("master volume"), 100.0) << "already at the top";
    click("Field of view Up");
    EXPECT_DOUBLE_EQ(setting("fov"), 80.0);
    click("Crosshair Choice");
    EXPECT_DOUBLE_EQ(setting("crosshair"), 1.0);
    click("Quality Choice");
    EXPECT_DOUBLE_EQ(setting("quality"), 0.0);
    click("Fullscreen Choice");
    EXPECT_DOUBLE_EQ(setting("fullscreen"), 1.0);
    click("Resolution Choice");
    EXPECT_DOUBLE_EQ(setting("width"), 1600.0);
    EXPECT_DOUBLE_EQ(setting("height"), 900.0);
    const auto* button = menu.ecs.TryGetComponent<Engine::Scene::ScreenElementComponent>(menu.entities.at("Play Button"));
    EXPECT_FALSE(button->visible) << "the main page is put away";
    click("Settings Back");
    EXPECT_TRUE(button->visible);
    EXPECT_EQ(button->kind, Engine::Scene::ScreenElementKind::Button) << "and it is a button again, not a label";
}

// The genre games' copies of prefabs (bandits, raiders, crates, barrels...) are up to date with their prefabs, as the
// editor finds them when it opens the scene: nothing to update, no change of their own
TEST(SampleGame, GenreGamesAreBuiltFromUpToDatePrefabs)
{
    Engine::Game::PrefabManager prefabs(kSampleGameDir);
    std::vector<std::string> scenes = kGenreGames;
    scenes.insert(scenes.end(), kGenreMenus.begin(), kGenreMenus.end());
    for (const std::string& name : scenes)
    {
        SceneDescription scene = Load(name);
        const SceneDescription asSaved = scene;
        if (!IsMenu(name)) // the games are built from them; the menus may have some too
        {
            EXPECT_GT(std::count_if(scene.objects.begin(), scene.objects.end(),
                                    [](const GameObject& object) { return object.prefabInstance.has_value(); }),
                      0)
                << name;
        }
        std::uint32_t nextId = 10000;
        EXPECT_FALSE(prefabs.UpdateInstances(scene, nextId)) << name;
        EXPECT_FALSE(prefabs.RefreshOverrides(scene)) << name;
        EXPECT_EQ(scene, asSaved) << name;
    }
}

// A new game from a genre template brings the prefabs its scene uses and the My Blocks it runs, and only those
TEST(SampleGame, GenreTemplatesBringTheirPrefabsAndBlocks)
{
    const auto root = std::filesystem::temp_directory_path() / "atelier_genre_templates";
    std::filesystem::remove_all(root);
    const auto rpg = GameProject::Create(root, "adventure", Engine::Game::GameTemplate::Rpg, kSampleGameDir);
    EXPECT_TRUE(std::filesystem::is_regular_file(rpg.ProjectDir() / "assets/prefabs/Bandit.prefab"));
    EXPECT_TRUE(std::filesystem::is_regular_file(rpg.ProjectDir() / "assets/prefabs/Crate.prefab"));
    EXPECT_EQ(rpg.Settings().startScene, "data/scenes/menu.json") << "it starts at its main menu";
    EXPECT_TRUE(std::filesystem::is_regular_file(rpg.ProjectDir() / "data/scenes/main.json"));
    const auto fps = GameProject::Create(root, "arena", Engine::Game::GameTemplate::Fps, kSampleGameDir);
    EXPECT_TRUE(std::filesystem::is_regular_file(fps.ProjectDir() / "assets/prefabs/FPS_Barrel.prefab"));
    Engine::Logic::Library library;
    Engine::Logic::LoadLibraryFile(fps.ProjectDir() / Engine::Logic::kLibraryFile, library);
    EXPECT_TRUE(library.Find("Shoot")) << "the My Blocks it uses";
    EXPECT_TRUE(library.Find("Move With Keys"));
    EXPECT_FALSE(library.Find("Bandit Chase")) << "and not the adventure's";
    EXPECT_FALSE(std::filesystem::exists(fps.ProjectDir() / "assets/prefabs/Bandit.prefab")) << "only what it uses";
    std::filesystem::remove_all(root);
}

// However it is pointed at them, the car at full speed (holding Shift, on a boost) stops at the banks: it is not moved
// past them, and it stays on the ground
TEST(SampleGame, RaceCarStaysOnTheMapAtFullSpeed)
{
    for (int heading = 0; heading < 360; heading += 30)
    {
        for (const char* pad : {"Boost Pad 1", "Boost Pad 2", "Boost Pad 3"})
        {
            Playing game(Load("data/scenes/racing.json"));
            game.PlayerTo(pad);
            game.Frames(6);
            auto turned = game.Where("Player");
            turned.rotation = static_cast<float>(heading);
            game.ecs.SetTransform(game.entities.at("Player"), turned);
            game.down.insert(Engine::Input::Key_W);
            game.down.insert(Engine::Input::Key_LeftShift);
            for (int frame = 0; frame < 120; ++frame)
            {
                game.Frames(1);
                const auto at = game.Where("Player");
                ASSERT_LT(std::abs(at.x), 36.0f) << pad << " heading " << heading << " frame " << frame;
                ASSERT_LT(std::abs(at.z), 31.0f) << pad << " heading " << heading << " frame " << frame;
                ASSERT_GT(at.y, -0.5f) << pad << " heading " << heading << " frame " << frame;
            }
        }
    }
}

// The RPG's sword strikes where its blade comes down, in front of the player: a bandit there is hit, one behind is not
TEST(SampleGame, RpgSwordHitsOnlyWhatIsInFrontOfThePlayer)
{
    for (const float facing : {0.0f, 180.0f}) // 0 faces -Z, towards the bandit
    {
        Playing game(Load("data/scenes/rpg.json"));
        game.Frames(1);
        const auto bandit = game.Where("Bandit 1");
        game.PlayerAt(bandit.x, bandit.z + 1.2f);
        auto player = game.Where("Player");
        player.rotation = facing;
        game.ecs.SetTransform(game.entities.at("Player"), player);
        game.Frames(1);
        game.mouse = true; // a click swings the sword
        game.Frames(2);
        game.mouse = false;
        game.Frames(40);
        if (facing == 0.0f)
        {
            EXPECT_FLOAT_EQ(game.Health("Bandit 1"), 20.0f) << "the blade comes down on it";
        }
        else
        {
            EXPECT_FLOAT_EQ(game.Health("Bandit 1"), 30.0f) << "the sword swings away from it";
        }
    }
}

// The RPG's sword swings once a click: holding the button down does not keep hitting
TEST(SampleGame, RpgSwordSwingsOnceAClick)
{
    Playing game(Load("data/scenes/rpg.json"));
    game.Frames(1);
    const auto bandit = game.Where("Bandit 1");
    game.PlayerAt(bandit.x, bandit.z + 1.2f);
    game.Frames(1);
    game.mouse = true;
    game.Frames(150); // held for 2.5 s
    EXPECT_FLOAT_EQ(game.Health("Bandit 1"), 20.0f);
    game.mouse = false;
    game.Frames(2);
    game.mouse = true; // a second click
    game.Frames(40);
    EXPECT_FLOAT_EQ(game.Health("Bandit 1"), 10.0f);
}

// The RPG's ground says what it is made of: the knight's footsteps are stone on the path, the square and the rocks,
// grass on the meadow (its model asks, through the player's body)
TEST(SampleGame, RpgFootstepsKnowWhatIsUnderfoot)
{
    for (const auto& [x, z, surface] :
         {std::tuple{0.0f, 8.0f, "stone"}, std::tuple{3.0f, -4.0f, "stone"}, std::tuple{12.0f, 14.0f, "grass"}})
    {
        Playing game(Load("data/scenes/rpg.json"));
        game.PlayerAt(x, z);
        game.Frames(30); // lands on the ground
        EXPECT_EQ(game.physics.SurfaceUnder(game.entities.at("Player Model")), std::optional<std::string>(surface))
            << x << ", " << z;
    }
}

// The RPG is a night lit by flickering lamps, with a sword in the player's hand
TEST(SampleGame, RpgIsADarkNightAndThePlayerHoldsASword)
{
    const SceneDescription scene = Load("data/scenes/rpg.json");
    const GameObject* sky = Named(scene, "Sky");
    ASSERT_TRUE(sky && sky->sky);
    EXPECT_LT(sky->sky->ambient, 0.2f);

    std::size_t pointLights = 0;
    std::size_t flickering = 0;
    for (const GameObject& object : scene.objects)
    {
        if (object.light && object.light->type == Engine::Graphics::LightType::Point)
        {
            ++pointLights;
            flickering += object.light->flicker > 0.0f ? 1u : 0u;
        }
    }
    EXPECT_LE(pointLights, Engine::Graphics::LightingContext::MaxLightsDefault) << "the renderer drops any more";
    EXPECT_EQ(flickering, pointLights);

    const GameObject* model = Named(scene, "Player Model");
    const GameObject* sword = Named(scene, "Sword");
    ASSERT_TRUE(model && sword && sword->renderable);
    EXPECT_EQ(sword->parent, model->id);
    // A long thin box held out in front (the sword's model is gone, and with it the hand's bone)
    EXPECT_GT(sword->renderable->shapeSizeZ, 5.0f * sword->renderable->shapeSizeX);
    EXPECT_LT(sword->renderable->shapeCenterZ, 0.0f);
}

// The race: gates count only in order, and the finish line ends a lap once all three were driven through
TEST(SampleGame, RaceCountsTheGatesInOrder)
{
    Playing game(Load("data/scenes/racing.json"));
    game.Frames(2);
    game.PlayerTo("Checkpoint 2");
    game.Frames(3);
    EXPECT_DOUBLE_EQ(game.Variable("Race Control", "next"), 1.0) << "gate 2 before gate 1 does not count";
    for (const char* gate : {"Checkpoint 1", "Checkpoint 2", "Checkpoint 3", "Finish Line"})
    {
        game.PlayerTo(gate);
        game.Frames(3);
    }
    EXPECT_DOUBLE_EQ(game.Variable("Race Control", "lap"), 1.0);
    EXPECT_DOUBLE_EQ(game.Variable("Race Control", "next"), 1.0);
}

// The car drives on the track and knocks a barrel flying
TEST(SampleGame, RaceCarKnocksBarrelsOver)
{
    Playing game(Load("data/scenes/racing.json"));
    const auto barrel = game.Where("Barrel 1");
    game.PlayerAt(barrel.x, barrel.z + 8.0f); // facing -Z, down the track at it
    game.Frames(30);
    EXPECT_GT(game.Where("Player").y, 0.2f) << "it rests on the track";
    game.down.insert(Engine::Input::Key_W);
    game.Frames(60);
    const auto after = game.Where("Barrel 1");
    EXPECT_GT(std::abs(after.x - barrel.x) + std::abs(after.z - barrel.z), 1.0f);
}

// A player turned more than 90 degrees keeps facing that way, and W walks it that way: the physics writes such a
// turn back as X and Z turned 180 and Y under 90 (the same rotation), and levelling the player must keep the heading
TEST(SampleGame, PlayersKeepFacingTheWayTheyTurn)
{
    for (const float heading : {135.0f, -150.0f})
    {
        Playing game(Load("data/scenes/fps.json"));
        game.Frames(30); // the round has set it down at its spawn
        auto turned = game.Where("Player");
        turned.rotation = heading;
        game.ecs.SetTransform(game.entities.at("Player"), turned);
        game.Frames(30);
        const auto settled = game.Where("Player");
        EXPECT_NEAR(std::remainder(settled.rotation - heading, 360.0f), 0.0f, 1.0f) << heading;
        EXPECT_NEAR(settled.rotationX, 0.0f, 1.0f) << heading;
        EXPECT_NEAR(settled.rotationZ, 0.0f, 1.0f) << heading;

        game.down.insert(Engine::Input::Key_W);
        game.Frames(20);
        const auto walked = game.Where("Player");
        const float dx = walked.x - settled.x;
        const float dz = walked.z - settled.z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        ASSERT_GT(distance, 0.5f) << heading;
        // A rotation of r degrees turns the front (-Z) to (-sin r, 0, -cos r)
        const float r = heading * 3.14159265f / 180.0f;
        EXPECT_GT((dx * -std::sin(r) + dz * -std::cos(r)) / distance, 0.95f) << heading << ": W goes the way it faces";
    }
}

// Each genre game's main menu: its title and Play button are on the screen, the mouse is free to click, and Play
// takes the mouse and goes to the game (the scene called main)
TEST(SampleGame, MainMenusStartTheGame)
{
    for (const std::string& name : kGenreMenus)
    {
        const SceneDescription scene = Load(name);
        const GameObject* play = Named(scene, "Play Button");
        ASSERT_TRUE(play && play->screen) << name;
        EXPECT_EQ(play->screen->kind, Engine::Scene::ScreenElementKind::Button) << name;
        ASSERT_TRUE(Named(scene, "Title") && Named(scene, "Title")->screen) << name;

        Playing game(scene);
        game.Frames(2);
        EXPECT_EQ(game.runner.TakeMouseRequest(), std::optional<bool>(false)) << name << ": the mouse is free";
        EXPECT_TRUE(game.runner.HasScreenClickScript(play->id)) << name;
        game.runner.ScreenClicked(play->id);
        game.Frames(2);
        EXPECT_EQ(game.runner.TakeSceneRequest(), std::optional<std::string>("main")) << name;
        EXPECT_EQ(game.runner.TakeMouseRequest(), std::optional<bool>(true)) << name << ": the game takes the mouse";
        for (const GameObject& object : scene.objects)
        {
            EXPECT_TRUE(game.runner.Problems(object.id).empty()) << name << ": " << object.name;
        }
    }
}

// The genre games' screens are Screen Elements: a bar tied to a variable that counts, in the scene from the start
// (the shooter's only count is the player's health, shown as a number)
TEST(SampleGame, GenreGamesShowTheirProgressOnScreenElements)
{
    for (const auto& [name, bar, owner, variable] :
         {std::tuple{"data/scenes/rpg.json", "Books Bar", "Villager", "books found"},
          std::tuple{"data/scenes/racing.json", "Lap Bar", "Race Control", "lap"}})
    {
        const SceneDescription scene = Load(name);
        const GameObject* element = Named(scene, bar);
        ASSERT_TRUE(element && element->screen) << name;
        EXPECT_EQ(element->screen->kind, Engine::Scene::ScreenElementKind::Bar) << name;
        EXPECT_EQ(element->screen->barObject, owner) << name;
        EXPECT_EQ(element->screen->barVariable, variable) << name;
        const GameObject* counter = Named(scene, owner);
        ASSERT_TRUE(counter && counter->logic) << name;
        EXPECT_NE(Engine::Logic::FindVariable(*counter->logic, variable), nullptr) << name;
    }
}
