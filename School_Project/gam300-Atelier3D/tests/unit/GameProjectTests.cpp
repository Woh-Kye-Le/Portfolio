#include "engine/assets/loaders/AtelierModel.hpp"
#include "engine/game/GameProject.hpp"
#include "engine/input/InputActions.hpp"
#include "engine/platform/Paths.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using Engine::Game::GameProject;
using Engine::Game::GameTemplate;
using Engine::Game::Shape;
using Engine::Scene::SceneDescription;

namespace
{
    // The demo templates, as the build copies them to templates/demos: the sample game in the repo
    const std::filesystem::path kTemplatesDir = ATELIER_SAMPLE_GAME_DIR;

    void SetEnv(const char* name, const char* value)
    {
#ifdef _WIN32
        _putenv_s(name, value);
#else
        setenv(name, value, 1);
#endif
    }

    void UnsetEnv(const char* name)
    {
#ifdef _WIN32
        _putenv_s(name, "");
#else
        unsetenv(name);
#endif
    }

    // Gives each test an empty projects folder containing an empty "mygame" folder
    class GameProjectTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            root_ = std::filesystem::temp_directory_path() / "atelier_tests" / info->name();
            std::filesystem::remove_all(root_);
            projectDir_ = root_ / "mygame";
            std::filesystem::create_directories(projectDir_);
        }

        void TearDown() override { std::filesystem::remove_all(root_); }

        void WriteFile(const std::filesystem::path& relativePath, const std::string& contents) const
        {
            const auto path = projectDir_ / relativePath;
            std::filesystem::create_directories(path.parent_path());
            std::ofstream(path) << contents;
        }

        void WriteConfig(const std::string& contents) const { WriteFile("configs/game.cfg", contents); }
        void WriteScene(const std::string& contents) const { WriteFile("data/scenes/main.json", contents); }

        SceneDescription Load() const { return GameProject(projectDir_).LoadStartScene(); }

        std::filesystem::path root_;
        std::filesystem::path projectDir_;
    };
}

// --- Settings (configs/game.cfg, read with CFGSerializer) ---

TEST_F(GameProjectTest, KeepsProjectDir)
{
    EXPECT_EQ(GameProject(projectDir_).ProjectDir(), projectDir_);
}

// No game.cfg at all: the name falls back to the folder name and everything else to the defaults.
TEST_F(GameProjectTest, MissingConfigUsesFolderNameAndDefaults)
{
    const GameProject project(projectDir_);
    const auto& settings = project.Settings();
    EXPECT_EQ(settings.name, "mygame");
    EXPECT_EQ(settings.windowWidth, 1280);
    EXPECT_EQ(settings.windowHeight, 720);
    EXPECT_EQ(settings.startScene, "data/scenes/main.json");
}

// A full game.cfg with a comment and a [global] section; values are trimmed ("My Game ").
TEST_F(GameProjectTest, ParsesConfig)
{
    WriteConfig("# comment\n[global]\nname = My Game \nwindow_width=800\nwindow_height=600\n"
                "start_scene=data/scenes/intro.json\n");

    const GameProject project(projectDir_);
    const auto& settings = project.Settings();
    EXPECT_EQ(settings.name, "My Game");
    EXPECT_EQ(settings.windowWidth, 800);
    EXPECT_EQ(settings.windowHeight, 600);
    EXPECT_EQ(settings.startScene, "data/scenes/intro.json");
}

// Gravity is Earth's, pulling down Y, unless game.cfg says otherwise; a value that is not a number keeps the default.
TEST_F(GameProjectTest, GravityComesFromTheConfig)
{
    WriteConfig("name=Space\n");
    EXPECT_FLOAT_EQ(GameProject(projectDir_).Settings().gravityY, -9.81f);

    WriteConfig("name=Moon\ngravity_x=0.5\ngravity_y=-1.62\ngravity_z=heavy\n");
    const auto settings = GameProject(projectDir_).Settings();
    EXPECT_FLOAT_EQ(settings.gravityX, 0.5f);
    EXPECT_FLOAT_EQ(settings.gravityY, -1.62f);
    EXPECT_FLOAT_EQ(settings.gravityZ, 0.0f) << "not a number: the default";
}

// SaveSettings writes game.cfg and keeps the gravity exactly (not rounded to one decimal).
TEST_F(GameProjectTest, SavedSettingsReadBackTheSame)
{
    WriteConfig("name=Saved\nwindow_width=800\n");
    GameProject project(projectDir_);
    auto settings = project.Settings();
    settings.gravityX = 0.25f;
    settings.gravityY = -9.81f;
    project.SaveSettings(settings);
    EXPECT_FLOAT_EQ(project.Settings().gravityX, 0.25f);

    const auto reread = GameProject(projectDir_).Settings();
    EXPECT_EQ(reread.name, "Saved");
    EXPECT_EQ(reread.windowWidth, 800);
    EXPECT_EQ(reread.gravityX, 0.25f);
    EXPECT_EQ(reread.gravityY, -9.81f);
    EXPECT_EQ(reread.gravityZ, 0.0f);
}

// The [global] line is optional.
TEST_F(GameProjectTest, ConfigWithoutSectionHeaderIsRead)
{
    WriteConfig("name=No Header\n");

    const GameProject project(projectDir_);
    EXPECT_EQ(project.Settings().name, "No Header");
}

// A window size that is not a number or is negative keeps the default size.
TEST_F(GameProjectTest, InvalidConfigNumbersKeepDefaults)
{
    WriteConfig("window_width=wide\nwindow_height=-5\n");

    const GameProject project(projectDir_);
    const auto& settings = project.Settings();
    EXPECT_EQ(settings.windowWidth, 1280);
    EXPECT_EQ(settings.windowHeight, 720);
}

// LoadStartScene opens whatever start_scene in game.cfg points at, not always main.json.
TEST_F(GameProjectTest, StartSceneFromConfigIsLoaded)
{
    WriteConfig("start_scene=data/scenes/other.json\n");
    WriteFile("data/scenes/other.json",
              R"({ "version": 7, "objects": [ { "id": 1, "name": "P",
                   "renderable": { "shape": "pyramid", "rotation_speed": 60 } } ] })");

    const SceneDescription scene = Load();
    ASSERT_EQ(scene.objects.size(), 1u);
    ASSERT_TRUE(scene.objects[0].renderable);
    EXPECT_EQ(scene.objects[0].renderable->shape, Shape::Pyramid);
}

// --- Scenes (the format itself is tested in SceneFileTests) ---

TEST_F(GameProjectTest, UnusableSceneFilesGiveAnEmptyScene)
{
    EXPECT_TRUE(Load().objects.empty()); // missing

    WriteScene("{ not json");
    EXPECT_TRUE(Load().objects.empty());

    SceneDescription scene;
    const auto report = GameProject(projectDir_).LoadSceneChecked("data/scenes/main.json", scene);
    EXPECT_FALSE(report.ok);
    EXPECT_NE(report.error.find("not valid JSON"), std::string::npos) << report.error;
}

// SaveScene/LoadScene take paths relative to the game folder and give back the same scene.
TEST_F(GameProjectTest, SavedScenesAreRelativeToTheGameFolder)
{
    Engine::Scene::GameObject box{.id = 1, .name = "Box"};
    box.transform = Engine::Scene::TransformComponent{.x = 2.0f};
    box.renderable = Engine::Scene::RenderableDesc{.shape = Shape::Wirebox};
    const SceneDescription scene{.objects = {box}};

    const GameProject project(projectDir_);
    project.SaveScene("data/scenes/level.json", scene);

    EXPECT_TRUE(std::filesystem::is_regular_file(projectDir_ / "data/scenes/level.json"));
    EXPECT_EQ(project.LoadScene("data/scenes/level.json"), scene);
}

// --- Game assets (texture picker and import) ---

TEST_F(GameProjectTest, FindAssetsListsMatchingFilesSorted)
{
    WriteFile("assets/art/sprites/stone.PNG", "x");
    WriteFile("assets/art/sprites/sub/wood.jpg", "x");
    WriteFile("assets/art/sprites/notes.txt", "x");
    WriteFile("assets/art/models/crate/crate.obj", "x");

    const GameProject project(projectDir_);
    EXPECT_EQ(project.FindAssets(Engine::Game::kSpritesFolder, Engine::Game::kTextureExtensions),
              (std::vector<std::string>{"assets/art/sprites/stone.PNG", "assets/art/sprites/sub/wood.jpg"}));
    EXPECT_EQ(project.FindAssets(Engine::Game::kModelsFolder, Engine::Game::kMeshExtensions),
              std::vector<std::string>{"assets/art/models/crate/crate.obj"});
    EXPECT_TRUE(project.FindAssets("assets/nothing_here", Engine::Game::kTextureExtensions).empty());
}

// Import copies the file into the folder asked for and returns the path to put in the scene; the original is not
// moved.
TEST_F(GameProjectTest, ImportAssetCopiesIntoTheGame)
{
    const auto source = root_ / "outside" / "crate.png";
    std::filesystem::create_directories(source.parent_path());
    std::ofstream(source, std::ios::binary) << "image-bytes";

    const GameProject project(projectDir_);
    const std::string imported = project.ImportAsset(source, Engine::Game::kSpritesFolder);

    EXPECT_EQ(imported, "assets/art/sprites/crate.png");
    std::ifstream copy(projectDir_ / imported, std::ios::binary);
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>(copy), {}), "image-bytes");
    EXPECT_TRUE(std::filesystem::exists(source)); // the original is left alone
}

// Importing the same file twice reuses the copy; a different file with the same name becomes crate_2.png instead of
// overwriting.
TEST_F(GameProjectTest, ImportAssetReusesTheSameFileAndNumbersADifferentOne)
{
    const auto source = root_ / "outside" / "crate.png";
    std::filesystem::create_directories(source.parent_path());
    std::ofstream(source, std::ios::binary) << "first";
    const GameProject project(projectDir_);

    EXPECT_EQ(project.ImportAsset(source, Engine::Game::kSpritesFolder), "assets/art/sprites/crate.png");
    EXPECT_EQ(project.ImportAsset(source, Engine::Game::kSpritesFolder), "assets/art/sprites/crate.png"); // same

    std::ofstream(source, std::ios::binary) << "second, different";
    EXPECT_EQ(project.ImportAsset(source, Engine::Game::kSpritesFolder), "assets/art/sprites/crate_2.png");
    std::ifstream kept(projectDir_ / "assets/art/sprites/crate.png", std::ios::binary);
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>(kept), {}), "first"); // not overwritten
}

// Importing a model that is already in the game (same contents) still brings over the materials and textures that sit
// beside it, in case they were missing.
TEST_F(GameProjectTest, ImportingAModelAlreadyInTheGameStillBringsItsMaterials)
{
    const auto outside = root_ / "outside";
    std::filesystem::create_directories(outside / "textures");
    std::ofstream(outside / "crate.obj") << "mtllib crate.mtl\nv 0 0 0\n";
    std::ofstream(outside / "crate.mtl") << "newmtl wood\nmap_Kd textures/crate.png\n";
    std::ofstream(outside / "textures" / "crate.png", std::ios::binary) << "image-bytes";
    WriteFile("assets/art/models/crate.obj", "mtllib crate.mtl\nv 0 0 0\n"); // already imported, without its materials
    ASSERT_FALSE(std::filesystem::exists(projectDir_ / "assets/art/models/crate.mtl"));

    const GameProject project(projectDir_);
    EXPECT_EQ(project.ImportAsset(outside / "crate.obj", Engine::Game::kModelsFolder), "assets/art/models/crate.obj");

    EXPECT_TRUE(std::filesystem::is_regular_file(projectDir_ / "assets/art/models/crate.mtl"));
    EXPECT_TRUE(std::filesystem::is_regular_file(projectDir_ / "assets/art/models/textures/crate.png"));
}

namespace
{
    // A triangle whose one material names the given texture (a path relative to the model, as the asset tool writes)
    void WriteAmodel(const std::filesystem::path& file, const std::string& texture)
    {
        Engine::Assets::ModelData model;
        Engine::Assets::MeshData mesh;
        mesh.positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
        mesh.indices = {0, 1, 2};
        mesh.subMeshes.push_back({0, 3, 0, 0});
        model.meshes.push_back(mesh);
        Engine::Assets::MaterialData material;
        material.baseColorTexture = texture;
        model.materials.push_back(material);
        model.nodes.push_back({});
        model.nodes[0].meshIndices = {0};
        model.roots = {0};
        const auto bytes = Engine::Assets::WriteAtelierModel(model);
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()),
                                                    static_cast<std::streamsize>(bytes.size()));
    }
}

// A .amodel names its textures relative to itself, usually ../textures/x.png: importing it brings those textures to the
// same place beside the copy, and not the other files in the folder it came from.
TEST_F(GameProjectTest, ImportingAnAmodelBringsTheTexturesItNamesAndNothingElse)
{
    const auto outside = root_ / "outside";
    WriteAmodel(outside / "models" / "crate.amodel", "../textures/crate.png");
    std::filesystem::create_directories(outside / "textures");
    std::ofstream(outside / "textures" / "crate.png", std::ios::binary) << "image-bytes";
    std::ofstream(outside / "textures" / "unrelated.png", std::ios::binary) << "other";
    std::ofstream(outside / "models" / "other.amodel") << "another model";

    const GameProject project(projectDir_);
    EXPECT_EQ(project.ImportAsset(outside / "models" / "crate.amodel", Engine::Game::kModelsFolder),
              "assets/art/models/crate.amodel");

    EXPECT_TRUE(std::filesystem::is_regular_file(projectDir_ / "assets/art/textures/crate.png"));
    EXPECT_FALSE(std::filesystem::exists(projectDir_ / "assets/art/textures/unrelated.png"));
    EXPECT_FALSE(std::filesystem::exists(projectDir_ / "assets/art/models/other.amodel"));
}

// A texture path that climbs out of the game folder is not followed.
TEST_F(GameProjectTest, ImportingAnAmodelNeverWritesOutsideTheGame)
{
    // outside/m/crate.amodel names ../../../probe.png, which is a real file next to the test folder; copied beside
    // the model in the game, the same path would land next to the game folder
    const auto probe = root_.parent_path() / "atelier_escape_probe.png";
    std::ofstream(probe, std::ios::binary) << "x";
    WriteAmodel(root_ / "outside" / "m" / "crate.amodel", "../../../atelier_escape_probe.png");

    const GameProject project(projectDir_);
    EXPECT_EQ(project.ImportAsset(root_ / "outside" / "m" / "crate.amodel", Engine::Game::kModelsFolder),
              "assets/art/models/crate.amodel");
    EXPECT_FALSE(std::filesystem::exists(root_ / "atelier_escape_probe.png"));

    std::filesystem::remove(probe);
}

// A texture that is already in the game is kept as it is, so importing a model again changes nothing.
TEST_F(GameProjectTest, ImportingAnAmodelKeepsTexturesAlreadyInTheGame)
{
    const auto outside = root_ / "outside";
    WriteAmodel(outside / "models" / "wall.amodel", "../textures/wall.png");
    std::filesystem::create_directories(outside / "textures");
    std::ofstream(outside / "textures" / "wall.png", std::ios::binary) << "new";
    WriteFile("assets/art/textures/wall.png", "old");

    const GameProject project(projectDir_);
    project.ImportAsset(outside / "models" / "wall.amodel", Engine::Game::kModelsFolder);

    std::ifstream kept(projectDir_ / "assets/art/textures/wall.png");
    std::string text;
    kept >> text;
    EXPECT_EQ(text, "old");
}

// A missing file or a folder cannot be imported.
TEST_F(GameProjectTest, ImportAssetRejectsAMissingFile)
{
    const GameProject project(projectDir_);
    EXPECT_THROW(project.ImportAsset(root_ / "nope.png", Engine::Game::kSpritesFolder), std::runtime_error);
    EXPECT_THROW(project.ImportAsset(root_, Engine::Game::kSpritesFolder), std::runtime_error); // a folder
}

// The model pickers list each model once: the .obj or .fbx kept beside a .amodel is its original, not another model.
TEST_F(GameProjectTest, FindModelsLeavesOutTheOriginalsKeptBesideAnAmodel)
{
    WriteFile("assets/art/models/car/car.amodel", "x");
    WriteFile("assets/art/models/car/car.fbx", "x");
    WriteFile("assets/art/models/crate/crate.obj", "x"); // copied without the tool: the .obj is the model
    WriteFile("assets/art/models/crate/crate.mtl", "x");

    const GameProject project(projectDir_);
    EXPECT_EQ(project.FindModels(),
              (std::vector<std::string>{"assets/art/models/car/car.amodel", "assets/art/models/crate/crate.obj"}));
}

// An image goes where its use says: art/sprites, art/spritesheets, or art/textures for a 3D object.
TEST_F(GameProjectTest, ImportImageChoosesTheFolderByUse)
{
    const auto source = root_ / "outside" / "wood.png";
    std::filesystem::create_directories(source.parent_path());
    std::ofstream(source, std::ios::binary) << "wood";
    const GameProject project(projectDir_);
    using Engine::Game::ImageUse;

    EXPECT_EQ(project.ImportImage(source, ImageUse::Sprite), "assets/art/sprites/wood.png");
    EXPECT_EQ(project.ImportImage(source, ImageUse::SpriteSheet), "assets/art/spritesheets/wood.png");
    EXPECT_EQ(project.ImportImage(source, ImageUse::Texture3D), "assets/art/textures/wood.png");
    EXPECT_EQ(project.ImportImage(source, ImageUse::Texture3D), "assets/art/textures/wood.png"); // the same image

    std::ofstream(source, std::ios::binary) << "another wood";
    EXPECT_EQ(project.ImportImage(source, ImageUse::Texture3D), "assets/art/textures/wood_2.png");
}

// A game made from the Input template has a player with a Transform that the camera follows, and it does not spin on
// its own.
TEST_F(GameProjectTest, TheInputTemplateHasAPlayerTheCameraFollows)
{
    const auto scene = GameProject::Create(root_, "player_game", GameTemplate::Input, kTemplatesDir).LoadStartScene();

    const auto player = std::find_if(scene.objects.begin(), scene.objects.end(),
                                     [](const Engine::Scene::GameObject& object) { return object.player.has_value(); });
    ASSERT_NE(player, scene.objects.end());
    EXPECT_TRUE(player->transform);
    EXPECT_TRUE(player->player->followCamera);
    EXPECT_FLOAT_EQ(player->renderable->rotationSpeed, 0.0f); // it turns with the mouse, not by itself
}

// Shapes are written as these lower-case names.
TEST(GameProjectShapeNames, MatchTheSceneFileNames)
{
    EXPECT_STREQ(Engine::Game::ToString(Shape::Quad), "quad");
    EXPECT_STREQ(Engine::Game::ToString(Shape::Cube), "cube");
    EXPECT_STREQ(Engine::Game::ToString(Shape::Wirebox), "wirebox");
    EXPECT_STREQ(Engine::Game::ToString(Shape::Line), "line");
}

// --- Creating games ---

TEST(GameProjectNameTest, AcceptsValidNames)
{
    EXPECT_TRUE(GameProject::IsValidName("my_game"));
    EXPECT_TRUE(GameProject::IsValidName("Level-2"));
    EXPECT_TRUE(GameProject::IsValidName("a"));
    EXPECT_TRUE(GameProject::IsValidName(std::string(64, 'x')));
}

// Empty, too long, spaces, slashes or "../" are rejected, so a game can never be made outside the projects folder.
TEST(GameProjectNameTest, RejectsInvalidNames)
{
    EXPECT_FALSE(GameProject::IsValidName(""));
    EXPECT_FALSE(GameProject::IsValidName("_hidden"));
    EXPECT_FALSE(GameProject::IsValidName("-flag"));
    EXPECT_FALSE(GameProject::IsValidName("my game"));
    EXPECT_FALSE(GameProject::IsValidName("../escape"));
    EXPECT_FALSE(GameProject::IsValidName("a/b"));
    EXPECT_FALSE(GameProject::IsValidName(std::string(65, 'x')));
}

// A new game gets every standard folder, with .gitkeep files so Git keeps the empty ones.
TEST_F(GameProjectTest, CreateMakesStandardLayout)
{
    const auto project = GameProject::Create(root_, "newgame", GameTemplate::Empty);
    const auto dir = root_ / "newgame";

    EXPECT_EQ(project.ProjectDir(), dir);
    EXPECT_TRUE(GameProject::IsGameProject(dir));
    EXPECT_EQ(project.Settings().name, "newgame");
    for (const char* folder :
         {"assets/art/models", "assets/art/textures", "assets/art/sprites", "assets/art/spritesheets", "assets/audio",
          "assets/prefabs", "assets/shaders", "scripts", "data/scenes"})
    {
        EXPECT_TRUE(std::filesystem::is_directory(dir / folder)) << folder;
    }
    EXPECT_TRUE(std::filesystem::exists(dir / "assets/art/models/.gitkeep"));
    EXPECT_FALSE(std::filesystem::exists(dir / "assets/models"));
    EXPECT_FALSE(std::filesystem::exists(dir / "assets/textures")); // under art/ now
    EXPECT_FALSE(std::filesystem::exists(dir / "assets/materials"));
}

// A new game gets configs/input_actions.json with the default keys, and it loads without problems.
TEST_F(GameProjectTest, CreateWritesTheDefaultInputActions)
{
    const GameProject project = GameProject::Create(root_, "input_game", GameTemplate::Empty);

    EXPECT_EQ(project.InputActionsPath(), root_ / "input_game" / "configs" / "input_actions.json");
    ASSERT_TRUE(std::filesystem::is_regular_file(project.InputActionsPath()));

    std::vector<std::string> problems;
    const auto actions = Engine::Input::InputActions::Load(project.InputActionsPath(), &problems);
    EXPECT_TRUE(problems.empty()) << problems.front();
    EXPECT_EQ(actions.All(), Engine::Input::InputActions::Defaults().All());
}

// Empty gives an empty scene; every other template gives exactly that demo scene, and a genre game starts at its
// main menu (<name>_menu.json in the templates), whose Play button goes to that scene, data/scenes/main.json.
TEST_F(GameProjectTest, NewGamesStartEmptyOrFromADemoScene)
{
    EXPECT_TRUE(GameProject::Create(root_, "empty_game", GameTemplate::Empty).LoadStartScene().objects.empty());

    for (const GameTemplate gameTemplate : Engine::Game::kGameTemplates)
    {
        if (gameTemplate == GameTemplate::Empty)
        {
            continue;
        }
        const std::string name = std::string(Engine::Game::ToString(gameTemplate)) + "_game";
        const GameProject game = GameProject::Create(root_, name, gameTemplate, kTemplatesDir);
        const std::string scenes = std::string("data/scenes/") + Engine::Game::ToString(gameTemplate);
        const SceneDescription demo = GameProject(kTemplatesDir).LoadScene(scenes + ".json");
        EXPECT_FALSE(demo.objects.empty()) << name;
        EXPECT_EQ(game.LoadScene("data/scenes/main.json"), demo) << name;
        if (std::filesystem::exists(kTemplatesDir / (scenes + "_menu.json")))
        {
            EXPECT_EQ(game.Settings().startScene, "data/scenes/menu.json") << name;
            EXPECT_EQ(game.LoadStartScene(), GameProject(kTemplatesDir).LoadScene(scenes + "_menu.json")) << name;
        }
        else
        {
            EXPECT_EQ(game.LoadStartScene(), demo) << name;
        }
        EXPECT_NE(std::string(Engine::Game::Describe(gameTemplate)), "") << name;
    }
}

// The demos are built from shapes alone: a game made from one brings no model, picture or sound files (it still gets
// the built-in shaders; a template copying the files its scene uses is checked with a template of its own, below)
TEST_F(GameProjectTest, ADemoTemplateBringsNoModelPictureOrSoundFiles)
{
    const std::vector<std::string> media{".obj", ".mtl", ".fbx", ".gltf", ".glb", ".bin", ".amodel", ".png",
                                         ".jpg", ".jpeg", ".wav", ".ogg", ".mp3"};
    for (const GameTemplate gameTemplate : {GameTemplate::Audio, GameTemplate::Resources, GameTemplate::Rpg,
                                            GameTemplate::Fps})
    {
        const auto game =
            GameProject::Create(root_, std::format("game_{}", static_cast<int>(gameTemplate)), gameTemplate,
                                kTemplatesDir)
                .ProjectDir();
        for (const auto& entry : std::filesystem::recursive_directory_iterator(game / "assets"))
        {
            const std::string extension = entry.path().extension().string();
            EXPECT_EQ(std::find(media.begin(), media.end(), extension), media.end()) << entry.path();
        }
    }
}

TEST_F(GameProjectTest, ATemplateModelBringsItsMaterialsAndTexturesButNotUnrelatedOnes)
{
    const auto templates = root_ / "templates";
    const auto writeTemplateFile = [&templates](const std::filesystem::path& relativePath, const std::string& text)
    {
        std::filesystem::create_directories((templates / relativePath).parent_path());
        std::ofstream(templates / relativePath) << text;
    };
    writeTemplateFile("assets/models/hero.obj", "mtllib hero.mtl\r\n");
    writeTemplateFile("assets/models/hero.mtl",
                      "newmtl skin\r\nmap_Kd -s 1 1 ../textures/hero.png\r\nmap_Bump -bm 1 ../textures/hero_n.png\r\n");
    writeTemplateFile("assets/textures/hero.png", "albedo");
    writeTemplateFile("assets/textures/hero_n.png", "normal");
    writeTemplateFile("assets/textures/other.png", "not used");
    writeTemplateFile(
        "data/scenes/rendering.json",
        R"({"version":8,"objects":[{"id":1,"name":"Hero","renderable":{"model":"assets/models/hero.obj","shape":"cube"},)"
        R"("transform":{"x":0.0,"y":0.0,"z":0.0,"size":1.0}}]})");

    const auto game = GameProject::Create(root_, "hero_game", GameTemplate::Rendering, templates).ProjectDir();

    EXPECT_TRUE(std::filesystem::is_regular_file(game / "assets/models/hero.obj"));
    EXPECT_TRUE(std::filesystem::is_regular_file(game / "assets/models/hero.mtl"));
    EXPECT_TRUE(std::filesystem::is_regular_file(game / "assets/textures/hero.png"));
    EXPECT_TRUE(std::filesystem::is_regular_file(game / "assets/textures/hero_n.png"));
    EXPECT_FALSE(std::filesystem::exists(game / "assets/textures/other.png")); // nothing names it
}

// If the template folder is missing, Create throws and leaves no half-made game; Empty needs no template so it still
// works.
TEST_F(GameProjectTest, AMissingTemplateIsAnErrorAndCreatesNothing)
{
    EXPECT_THROW(GameProject::Create(root_, "no_template", GameTemplate::Rendering, root_ / "nowhere"),
                 std::runtime_error);
    EXPECT_FALSE(std::filesystem::exists(root_ / "no_template"));
    EXPECT_NO_THROW(GameProject::Create(root_, "still_fine", GameTemplate::Empty, root_ / "nowhere")); // needs none
}

// Everything Create writes reads back: the settings, and a start scene that loads with no warnings.
TEST_F(GameProjectTest, CreatedFilesRoundTrip)
{
    GameProject::Create(root_, "round_trip", GameTemplate::Input, kTemplatesDir);

    const GameProject reloaded(root_ / "round_trip");
    EXPECT_EQ(reloaded.Settings().name, "round_trip");
    EXPECT_EQ(reloaded.Settings().windowWidth, 1280);
    EXPECT_EQ(reloaded.Settings().windowHeight, 720);
    EXPECT_EQ(reloaded.Settings().startScene, "data/scenes/main.json");
    EXPECT_TRUE(std::filesystem::exists(root_ / "round_trip" / "data/scenes/main.json"));

    SceneDescription scene;
    const auto report = reloaded.LoadSceneChecked(reloaded.Settings().startScene, scene);
    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(report.warnings.empty());
    EXPECT_EQ(scene, GameProject(kTemplatesDir).LoadScene("data/scenes/input.json"));
}

// A bad name throws and nothing is created outside the folder.
TEST_F(GameProjectTest, CreateRejectsInvalidName)
{
    EXPECT_THROW(GameProject::Create(root_, "../escape", GameTemplate::Empty), std::runtime_error);
    EXPECT_FALSE(std::filesystem::exists(root_.parent_path() / "escape"));
}

// Never overwrite a folder that is already there.
TEST_F(GameProjectTest, CreateRejectsExistingFolder)
{
    EXPECT_THROW(GameProject::Create(root_, "mygame", GameTemplate::Empty), std::runtime_error);
}

// --- Finding games ---

TEST(GameProjectRootTest, GamesLiveInProjectsFolderNextToTheExecutable)
{
    const auto exeDir = Engine::Platform::ExecutableDir();
    EXPECT_EQ(GameProject::ProjectsRoot(), exeDir / "projects");

    // ExecutableDir() is the folder this test binary is in
    bool foundTestExe = false;
    for (const auto& entry : std::filesystem::directory_iterator(exeDir))
    {
        foundTestExe |= entry.path().stem() == "AtelierTests";
    }
    EXPECT_TRUE(foundTestExe) << exeDir;
}

// The first game also creates the projects folder if it does not exist yet.
TEST_F(GameProjectTest, CreateMakesMissingProjectsFolder)
{
    const auto projectsDir = root_ / "projects"; // does not exist yet
    GameProject::Create(projectsDir, "first_game", GameTemplate::Empty);

    EXPECT_TRUE(GameProject::IsGameProject(projectsDir / "first_game"));
}

// Only folders with configs/game.cfg count as games, and they come back sorted by name.
TEST_F(GameProjectTest, FindAllListsOnlyGameProjectsSorted)
{
    GameProject::Create(root_, "zeta", GameTemplate::Empty);
    GameProject::Create(root_, "alpha", GameTemplate::Empty);
    std::filesystem::create_directories(root_ / "atelier_engine" / "src"); // not a game project
    // "mygame" has no configs/game.cfg, so it is not a game project either

    const auto found = GameProject::FindAll(root_);
    ASSERT_EQ(found.size(), 2u);
    EXPECT_EQ(found[0].filename(), "alpha");
    EXPECT_EQ(found[1].filename(), "zeta");
}

// No projects folder yet = no games (not an error).
TEST_F(GameProjectTest, FindAllOnMissingFolderIsEmpty)
{
    EXPECT_TRUE(GameProject::FindAll(root_ / "does_not_exist").empty());
}

// ATELIER_GAME_PROJECT pointing at a real folder wins over the projects folder.
TEST_F(GameProjectTest, EnvironmentVariableOverridesProjectDir)
{
    SetEnv("ATELIER_GAME_PROJECT", projectDir_.string().c_str());
    const auto found = GameProject::FindDefaultGameProjectDir();
    UnsetEnv("ATELIER_GAME_PROJECT");

    EXPECT_EQ(found, projectDir_);
}

// ATELIER_GAME_PROJECT pointing at a folder that does not exist is ignored.
TEST_F(GameProjectTest, NonexistentEnvironmentPathIsIgnored)
{
    const auto missing = projectDir_ / "does_not_exist";
    SetEnv("ATELIER_GAME_PROJECT", missing.string().c_str());
    const auto found = GameProject::FindDefaultGameProjectDir();
    UnsetEnv("ATELIER_GAME_PROJECT");

    EXPECT_NE(found, missing);
}
