#include "engine/core/Log.hpp"
#include "engine/scene/SceneFile.hpp"
#include "engine/serializer/JsonSerializer.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

// The scene file format (Scene::SceneFile): what the editor saves and the game plays

using Engine::Game::Shape;
using Engine::Graphics::LightType;
using Engine::Scene::GameObject;
using Engine::Scene::kSceneFormatVersion;
using Engine::Scene::LoadSceneFile;
using Engine::Scene::SaveSceneFile;
using Engine::Scene::SceneDescription;

namespace
{
    bool AnyContains(const std::vector<std::string>& lines, const std::string& text)
    {
        return std::any_of(lines.begin(), lines.end(),
                           [&text](const std::string& line) { return line.find(text) != std::string::npos; });
    }

    // Every component, with every field set to something other than its default (including text that needs escaping
    // and floats with no exact binary form)
    SceneDescription FullScene()
    {
        GameObject hero{.id = 4,
                        .name = "Hero \"A\" / \xC3\xBC"
                                "ber"};
        hero.transform = Engine::Scene::TransformComponent{.x = 0.1f,
                                                           .y = -2.75f,
                                                           .z = 1.0f / 3.0f,
                                                           .rotation = 45.0f,
                                                           .rotationX = 15.0f,
                                                           .rotationZ = -30.0f,
                                                           .scale = 2.5f,
                                                           .scaleX = 2.0f,
                                                           .scaleY = 0.5f,
                                                           .scaleZ = 1.25f};
        hero.renderable = Engine::Scene::RenderableDesc{.shape = Shape::Sphere,
                                                        .rotationSpeed = -90.0f,
                                                        .mesh = "assets/models/my crate.obj",
                                                        .model = "assets/models/crate.gltf",
                                                        .texture = "assets/textures/crate.png",
                                                        .shader = "assets/shaders/unlit.vert",
                                                        .material = "assets/materials/Shiny Red.amat",
                                                        .modelTurn = 180.0f,
                                                        .holdBone = "handslot.r",
                                                        .albedoR = 0.5f,
                                                        .albedoG = 0.25f,
                                                        .albedoB = 1.0f,
                                                        .albedoA = 0.75f};
        hero.audioSource = Engine::Scene::AudioSourceDesc{.clip = "assets/audio/beep.wav",
                                                          .volume = 0.4f,
                                                          .loop = false,
                                                          .playOnStart = false,
                                                          .spatial = false,
                                                          .minDistance = 3.0f,
                                                          .maxDistance = 30.0f,
                                                          .bus = "music"};
        hero.player = Engine::Scene::PlayerControllerComponent{.speed = 5.0f,
                                                               .fastMultiplier = 3.0f,
                                                               .cameraOffsetY = 2.0f,
                                                               .cameraOffsetZ = 6.0f,
                                                               .followCamera = false,
                                                               .lookSensitivity = 0.35f,
                                                               .cameraPitch = -12.5f};
        hero.rigidBody = Engine::Scene::RigidBodyComponent{.type = Engine::Scene::BodyType::Kinematic,
                                                           .mass = 2.5f,
                                                           .gravityScale = 0.5f,
                                                           .friction = 0.8f,
                                                           .bounciness = 0.25f,
                                                           .startVelocityX = 1.5f,
                                                           .startVelocityY = 6.0f,
                                                           .startVelocityZ = -2.25f,
                                                           .shape = Engine::Scene::ColliderShape::Capsule,
                                                           .centerX = 0.25f,
                                                           .centerY = -1.0f / 3.0f,
                                                           .centerZ = 2.0f,
                                                           .sizeX = 1.5f,
                                                           .sizeY = 0.75f,
                                                           .sizeZ = 3.0f,
                                                           .radius = 0.4f,
                                                           .height = 1.8f};

        hero.screen = Engine::Scene::ScreenElementComponent{.kind = Engine::Scene::ScreenElementKind::Bar,
                                                            .text = "Health",
                                                            .picture = "heart",
                                                            .barObject = "Hero",
                                                            .barVariable = "lives",
                                                            .barMaximum = 3.0f,
                                                            .anchor = Engine::Scene::ScreenAnchor::BottomRight,
                                                            .x = -12.5f,
                                                            .y = -4.0f,
                                                            .height = 3.5f,
                                                            .width = 20.0f,
                                                            .colorR = 0.25f,
                                                            .colorG = 0.5f,
                                                            .colorB = 0.75f,
                                                            .layer = 2,
                                                            .visible = false};
        hero.animator =
            Engine::Scene::AnimatorComponent{.clip = "Walk", .playing = false, .loop = false, .speed = 1.5f};
        hero.sky = Engine::Scene::SkyComponent{
            .topR = 0.9f, .horizonB = 0.1f, .groundG = 0.5f, .haze = 0.03f, .picture = "sunset", .turn = 90.0f};

        GameObject camera{.id = 7, .name = "Main Camera"};
        camera.transform = Engine::Scene::TransformComponent{.y = 2.0f, .z = 8.0f, .rotationX = -10.0f};
        camera.camera = Engine::Scene::CameraComponent{
            .fieldOfView = 70.0f, .nearPlane = 0.1f, .farPlane = 500.0f, .primary = true};

        GameObject sun{.id = 9, .name = "Sun"};
        sun.transform.emplace();
        sun.light = Engine::Scene::LightComponent{.type = LightType::Spot,
                                                  .colorR = 1.0f,
                                                  .colorG = 0.9f,
                                                  .colorB = 0.8f,
                                                  .intensity = 2.0f,
                                                  .range = 20.0f,
                                                  .spotInnerConeDeg = 20.0f,
                                                  .spotOuterConeDeg = 30.0f,
                                                  .enabled = false,
                                                  .flicker = 0.5f};

        return {.objects = {hero, camera, sun, GameObject{.id = 12, .name = "Empty"}}};
    }

    // Collects what the engine logs, for the lifetime of the object
    class ScopedLogCapture
    {
    public:
        ScopedLogCapture() { Engine::Core::Logger::Get().AddSink(sink_); }
        ~ScopedLogCapture() { Engine::Core::Logger::Get().RemoveSink(sink_); }

        std::vector<std::string> Errors()
        {
            std::vector<std::string> messages;
            for (const auto& entry : sink_->Entries())
            {
                if (entry.level == Engine::Core::LogLevel::Error)
                {
                    messages.push_back(entry.message);
                }
            }
            return messages;
        }

    private:
        class Sink final : public Engine::Core::ILogSink
        {
        public:
            void Write(const Engine::Core::LogEntry& entry) override
            {
                const std::scoped_lock lock(mutex_);
                entries_.push_back(entry);
            }

            std::vector<Engine::Core::LogEntry> Entries()
            {
                const std::scoped_lock lock(mutex_);
                return entries_;
            }

        private:
            std::mutex mutex_;
            std::vector<Engine::Core::LogEntry> entries_;
        };

        std::shared_ptr<Sink> sink_ = std::make_shared<Sink>();
    };

    // Gives each test an empty folder for its scene files
    class SceneFileTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            dir_ = std::filesystem::temp_directory_path() / "atelier_scene_file_tests" / info->name();
            std::filesystem::remove_all(dir_);
            std::filesystem::create_directories(dir_);
        }

        void TearDown() override { std::filesystem::remove_all(dir_); }

        std::filesystem::path Write(const std::string& contents, const char* name = "main.json") const
        {
            std::ofstream(dir_ / name, std::ios::binary) << contents;
            return dir_ / name;
        }

        // A valid file with one object, whose JSON text is objectJson
        std::filesystem::path WriteObject(const std::string& objectJson) const
        {
            return Write(std::format(R"({{ "version": {}, "objects": [ {} ] }})", kSceneFormatVersion, objectJson));
        }

        std::string Read(const char* name = "main.json") const
        {
            std::ifstream file(dir_ / name, std::ios::binary);
            return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        }

        std::filesystem::path dir_;
    };
}

// --- Saving and loading ---

TEST_F(SceneFileTest, SaveThenLoadGivesBackEveryComponentAndField)
{
    SaveSceneFile(dir_ / "main.json", FullScene());

    SceneDescription loaded;
    const auto report = LoadSceneFile(dir_ / "main.json", loaded);

    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(report.warnings.empty()) << report.warnings.front();
    EXPECT_EQ(loaded, FullScene());
}

// Save, load, save again: the two files are identical (the round-trip requirement).
TEST_F(SceneFileTest, SaveLoadSaveGivesTheSameFile)
{
    SaveSceneFile(dir_ / "first.json", FullScene());
    SceneDescription loaded;
    ASSERT_TRUE(LoadSceneFile(dir_ / "first.json", loaded).ok);
    SaveSceneFile(dir_ / "second.json", loaded);

    const std::string first = Read("first.json");
    ASSERT_FALSE(first.empty());
    EXPECT_EQ(first, Read("second.json"));
}

// The file is plain JSON with a version number, and each object only lists the components it has.
TEST_F(SceneFileTest, FilesAreReadableVersionedJsonListingOnlyTheComponentsAnObjectHas)
{
    SaveSceneFile(dir_ / "main.json", FullScene());

    Engine::Serializer::JSONSerializer json;
    json.LoadFile((dir_ / "main.json").string());
    int version = 0;
    EXPECT_TRUE(json.TryReadInt("version", version));
    EXPECT_EQ(version, kSceneFormatVersion);
    ASSERT_TRUE(json.HasArray("objects"));
    json.EnterArray("objects");
    ASSERT_EQ(json.GetCurrentNode().size(), 4u);

    json.EnterArrayElement(0); // every component
    for (const char* component :
         {"transform", "renderable", "audio_source", "player", "rigid_body", "screen", "animator", "sky"})
    {
        EXPECT_TRUE(json.HasObject(component)) << component;
    }
    EXPECT_FALSE(json.HasObject("camera"));
    json.LeaveObject();

    json.EnterArrayElement(3); // none
    for (const char* component : {"transform", "renderable", "audio_source", "player", "camera", "light"})
    {
        EXPECT_FALSE(json.HasObject(component)) << component;
    }
}

// Empty asset paths and a plain white tint are not written, and the object still loads back the same.
TEST_F(SceneFileTest, OptionalFieldsAreLeftOutWhenUnset)
{
    GameObject plain{.id = 1, .name = "Plain"};
    plain.renderable.emplace(); // no asset paths, plain white
    SaveSceneFile(dir_ / "main.json", {.objects = {plain}});

    const std::string text = Read();
    for (const char* field : {"\"mesh\"", "\"model\"", "\"texture\"", "\"shader\"", "\"albedo_r\""})
    {
        EXPECT_EQ(text.find(field), std::string::npos) << field;
    }
    SceneDescription loaded;
    ASSERT_TRUE(LoadSceneFile(dir_ / "main.json", loaded).ok);
    EXPECT_EQ(loaded.objects[0], plain);
}

// Saving into a folder that does not exist yet creates it.
TEST_F(SceneFileTest, SavingCreatesTheFolder)
{
    SaveSceneFile(dir_ / "data" / "scenes" / "level.json", FullScene());
    EXPECT_TRUE(std::filesystem::is_regular_file(dir_ / "data" / "scenes" / "level.json"));
}

// Saving over a folder fails with an exception the editor can show, instead of silently doing nothing.
TEST_F(SceneFileTest, SavingWhereNoFileCanBeWrittenThrows)
{
    std::filesystem::create_directories(dir_ / "taken");
    EXPECT_THROW(SaveSceneFile(dir_ / "taken", FullScene()), std::runtime_error);
}

// --- Files that cannot be used ---

TEST_F(SceneFileTest, UnusableFilesFailAndLeaveTheSceneAlone)
{
    const SceneDescription before = FullScene();
    const std::pair<const char*, const char*> files[] = {
        {"broken.json", R"({ "version": 7, "objects": [ )"},
        {"array.json", R"([ 1, 2, 3 ])"},
        {"layout.json", R"({ "version": 7, "things": [] })"},
        {"list.json", R"({ "version": 7, "objects": 5 })"},
        {"unversioned.json", R"({ "objects": [] })"},
        {"flat.json", R"({ "version": 6, "objects": [ { "shape": "cube", "x": 1 } ] })"},
        {"future.json", R"({ "version": 99, "objects": [] })"},
        {"text.json", R"({ "version": "seven", "objects": [] })"},
    };
    for (const auto& [name, text] : files)
    {
        SceneDescription scene = before;
        const auto report = LoadSceneFile(Write(text, name), scene);
        EXPECT_FALSE(report.ok) << name;
        EXPECT_FALSE(report.error.empty()) << name;
        EXPECT_EQ(scene, before) << name;
    }

    SceneDescription scene = before;
    const auto missing = LoadSceneFile(dir_ / "missing.json", scene);
    EXPECT_FALSE(missing.ok);
    EXPECT_NE(missing.error.find("file not found"), std::string::npos) << missing.error;
    EXPECT_EQ(scene, before);
}

// The error messages point at the problem: the line of broken JSON, an old version number, or a missing version.
TEST_F(SceneFileTest, ErrorsSayWhatIsWrong)
{
    SceneDescription scene;
    const auto corrupt = LoadSceneFile(Write("{\n  \"version\": 7,\n  \"objects\": [ ,\n"), scene);
    EXPECT_NE(corrupt.error.find("not valid JSON"), std::string::npos) << corrupt.error;
    EXPECT_NE(corrupt.error.find("line 3"), std::string::npos) << corrupt.error;

    const auto flat = LoadSceneFile(Write(R"({ "version": 6, "objects": [] })"), scene);
    EXPECT_NE(flat.error.find("unsupported format version 6"), std::string::npos) << flat.error;

    const auto unversioned = LoadSceneFile(Write(R"({ "objects": [] })"), scene);
    EXPECT_NE(unversioned.error.find("missing \"version\""), std::string::npos) << unversioned.error;
}

// --- Forgiving reads: problems become warnings and defaults ---

TEST_F(SceneFileTest, MissingFieldsAreReportedAndKeepTheirDefaults)
{
    SceneDescription scene;
    const auto report = LoadSceneFile(WriteObject(R"({ "id": 1, "name": "Box",
        "transform": { "x": 1 }, "renderable": { "shape": "cube" }, "camera": { "primary": true } })"),
                                      scene);

    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(AnyContains(report.warnings, "object 0 (Box) transform: missing \"y\""));
    EXPECT_TRUE(AnyContains(report.warnings, "object 0 (Box) renderable: missing \"rotation_speed\""));
    EXPECT_TRUE(AnyContains(report.warnings, "object 0 (Box) camera: missing \"field_of_view\""));
    ASSERT_EQ(scene.objects.size(), 1u);
    const GameObject& box = scene.objects[0];
    EXPECT_FLOAT_EQ(box.transform->x, 1.0f);
    EXPECT_FLOAT_EQ(box.transform->y, 0.0f);
    EXPECT_FLOAT_EQ(box.transform->scale, 1.0f);
    EXPECT_FLOAT_EQ(box.renderable->rotationSpeed, 0.0f) << "shapes stay still unless given a spin";
    EXPECT_FLOAT_EQ(box.camera->fieldOfView, 60.0f);
    EXPECT_TRUE(box.camera->primary);
}

// A shape added in the editor (a default Renderable) does not turn by itself in Play mode, and the speed is still
// written to the file, so a scene saved with a spin keeps it
TEST_F(SceneFileTest, ShapesStayStillUnlessGivenASpin)
{
    EXPECT_FLOAT_EQ(Engine::Scene::RenderableDesc{}.rotationSpeed, 0.0f);

    GameObject cube{.id = 1, .name = "Cube"};
    cube.renderable.emplace();
    GameObject spinner{.id = 2, .name = "Spinner"};
    spinner.renderable = Engine::Scene::RenderableDesc{.rotationSpeed = 60.0f};
    SaveSceneFile(dir_ / "main.json", {.objects = {cube, spinner}});
    EXPECT_NE(Read().find("\"rotation_speed\""), std::string::npos);

    SceneDescription loaded;
    ASSERT_TRUE(LoadSceneFile(dir_ / "main.json", loaded).ok);
    ASSERT_EQ(loaded.objects.size(), 2u);
    EXPECT_FLOAT_EQ(loaded.objects[0].renderable->rotationSpeed, 0.0f);
    EXPECT_FLOAT_EQ(loaded.objects[1].renderable->rotationSpeed, 60.0f);
}

// A value of the wrong type (text where a number goes, etc.) is reported and the default is used.
TEST_F(SceneFileTest, MistypedFieldsAreReportedAndKeepTheirDefaults)
{
    SceneDescription scene;
    const auto report = LoadSceneFile(WriteObject(R"({ "id": 1, "name": 7,
        "transform": { "x": "left", "y": 0, "z": 0, "rotation_x": 0, "rotation": 0, "rotation_z": 0, "size": 1,
                       "scale_x": 1, "scale_y": 1, "scale_z": 1 },
        "audio_source": { "clip": 5, "volume": 1, "loop": "yes", "play_on_start": true, "spatial": true,
                          "min_distance": 2, "max_distance": 50, "bus": "sfx" } })"),
                                      scene);

    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(AnyContains(report.warnings, "object 0: \"name\" is not text"));
    EXPECT_TRUE(AnyContains(report.warnings, "transform: \"x\" is not a number - using 0"));
    EXPECT_TRUE(AnyContains(report.warnings, "audio_source: \"clip\" is not text"));
    EXPECT_TRUE(AnyContains(report.warnings, "audio_source: \"loop\" is not true or false - using true"));
    EXPECT_FLOAT_EQ(scene.objects[0].transform->x, 0.0f);
    EXPECT_TRUE(scene.objects[0].audioSource->clip.empty());
    EXPECT_TRUE(scene.objects[0].audioSource->loop);
}

// An unknown shape becomes a cube, an unknown light type a point light, and a size <= 0 becomes 1, each with a warning.
TEST_F(SceneFileTest, UnknownShapesLightTypesAndBadSizesAreReported)
{
    SceneDescription scene;
    const auto report = LoadSceneFile(WriteObject(R"({ "id": 1, "name": "Odd",
        "transform": { "x": 0, "y": 0, "z": 0, "rotation_x": 0, "rotation": 0, "rotation_z": 0, "size": -2,
                       "scale_x": 1, "scale_y": 1, "scale_z": 1 },
        "renderable": { "shape": "teapot", "rotation_speed": 0 },
        "light": { "type": "laser", "enabled": true, "color_r": 1, "color_g": 1, "color_b": 1, "intensity": 1,
                   "range": 10, "spot_inner": 25, "spot_outer": 35 } })"),
                                      scene);

    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(AnyContains(report.warnings, "unknown shape \"teapot\" - using cube"));
    EXPECT_TRUE(AnyContains(report.warnings, "unknown light type \"laser\" - using point"));
    EXPECT_TRUE(AnyContains(report.warnings, "\"size\" must be above 0 - using 1"));
    EXPECT_EQ(scene.objects[0].renderable->shape, Shape::Cube);
    EXPECT_EQ(scene.objects[0].light->type, LightType::Point);
    EXPECT_FLOAT_EQ(scene.objects[0].transform->scale, 1.0f);
}

// Junk entries in the objects list are skipped with a warning; the real object still loads.
TEST_F(SceneFileTest, EntriesThatAreNotObjectsAreSkipped)
{
    SceneDescription scene;
    const auto report =
        LoadSceneFile(Write(R"({ "version": 7, "objects": [ 42, { "id": 1, "name": "Kept" }, "text" ] })"), scene);

    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(AnyContains(report.warnings, "object 0: not a JSON object - skipped"));
    EXPECT_TRUE(AnyContains(report.warnings, "object 2: not a JSON object - skipped"));
    ASSERT_EQ(scene.objects.size(), 1u);
    EXPECT_EQ(scene.objects[0].name, "Kept");
}

// A rigid body saved before collider shapes existed (only type and mass) loads without warnings, with an Auto collider.
TEST_F(SceneFileTest, RigidBodiesWithoutAColliderLoadAsAuto)
{
    SceneDescription scene;
    const auto report = LoadSceneFile(
        WriteObject(R"({ "id": 1, "name": "Crate", "rigid_body": { "type": "static", "mass": 3 } })"), scene);

    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(report.warnings.empty()) << report.warnings.front();
    ASSERT_TRUE(scene.objects[0].rigidBody.has_value());
    EXPECT_EQ(*scene.objects[0].rigidBody,
              (Engine::Scene::RigidBodyComponent{.type = Engine::Scene::BodyType::Static, .mass = 3.0f}));
}

// An unknown collider shape or a size that is not above 0 falls back to the default, with a warning.
TEST_F(SceneFileTest, BadColliderValuesFallBackWithAWarning)
{
    SceneDescription scene;
    const auto report = LoadSceneFile(WriteObject(R"({ "id": 1, "name": "Crate",
        "rigid_body": { "type": "dynamic", "mass": 1, "shape": "donut", "radius": 0, "size_y": -2 } })"),
                                      scene);

    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(AnyContains(report.warnings, "unknown collider shape \"donut\""));
    EXPECT_TRUE(AnyContains(report.warnings, "\"radius\" must be above 0"));
    EXPECT_TRUE(AnyContains(report.warnings, "\"size_y\" must be above 0"));
    EXPECT_EQ(*scene.objects[0].rigidBody, Engine::Scene::RigidBodyComponent{});
}

// Friction below 0 and bounciness outside 0 to 1 are fixed, with a warning.
TEST_F(SceneFileTest, BadSurfaceValuesAreFixedWithAWarning)
{
    SceneDescription scene;
    const auto report = LoadSceneFile(WriteObject(R"({ "id": 1, "name": "Crate",
        "rigid_body": { "type": "dynamic", "mass": 1, "friction": -1, "bounciness": 3 } })"),
                                      scene);

    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(AnyContains(report.warnings, "\"friction\" must be 0 or more - using 0.5"));
    EXPECT_TRUE(AnyContains(report.warnings, "\"bounciness\" must be from 0 to 1 - using 1"));
    EXPECT_FLOAT_EQ(scene.objects[0].rigidBody->friction, 0.5f);
    EXPECT_FLOAT_EQ(scene.objects[0].rigidBody->bounciness, 1.0f);
}

// Components this build does not know about are ignored without warnings, so newer files still open.
TEST_F(SceneFileTest, UnknownComponentsAreSkippedSilently)
{
    // e.g. a component a newer build saved: the rest of the object still loads
    SceneDescription scene;
    const auto report = LoadSceneFile(
        WriteObject(R"({ "id": 1, "name": "Rig", "rigidbody": { "mass": 4 }, "script": { "file": "a.lua" } })"), scene);

    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(report.warnings.empty()) << report.warnings.front();
    EXPECT_EQ(scene.objects[0], (GameObject{.id = 1, .name = "Rig"}));
}

// Missing, invalid or repeated ids get replaced so every object ends up with its own id above 0.
TEST_F(SceneFileTest, MissingOrRepeatedIdsAreReplacedWithUniqueOnes)
{
    SceneDescription scene;
    const auto report = LoadSceneFile(Write(R"({ "version": 7, "objects": [
        { "id": 3, "name": "a" }, { "name": "b" }, { "id": 3, "name": "c" }, { "id": -1, "name": "d" } ] })"),
                                      scene);

    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(AnyContains(report.warnings, "object 1 (b): no valid \"id\""));
    EXPECT_TRUE(AnyContains(report.warnings, "object 2 (c): repeated \"id\""));
    ASSERT_EQ(scene.objects.size(), 4u);
    EXPECT_EQ(scene.objects[0].id, 3u);
    std::vector<std::uint32_t> ids;
    for (const GameObject& object : scene.objects)
    {
        EXPECT_GT(object.id, 0u);
        ids.push_back(object.id);
    }
    std::sort(ids.begin(), ids.end());
    EXPECT_EQ(std::adjacent_find(ids.begin(), ids.end()), ids.end()); // all different
}

// --- Problems are logged as errors ---

TEST_F(SceneFileTest, MissingCorruptAndIncompleteFilesAreLoggedAsErrors)
{
    SceneDescription scene;
    {
        ScopedLogCapture log;
        LoadSceneFile(dir_ / "missing.json", scene);
        EXPECT_TRUE(AnyContains(log.Errors(), "missing.json: file not found"));
    }
    {
        ScopedLogCapture log;
        LoadSceneFile(Write("{ not json", "corrupt.json"), scene);
        EXPECT_TRUE(AnyContains(log.Errors(), "corrupt.json: not valid JSON"));
    }
    {
        ScopedLogCapture log;
        LoadSceneFile(WriteObject(R"({ "id": 1, "name": "Box", "transform": { "x": 1 } })"), scene);
        EXPECT_TRUE(AnyContains(log.Errors(), "main.json: object 0 (Box) transform: missing \"y\""));
    }
}

// A good file must not log any errors.
TEST_F(SceneFileTest, AValidSceneLogsNothing)
{
    SaveSceneFile(dir_ / "main.json", FullScene());
    ScopedLogCapture log;
    SceneDescription scene;
    EXPECT_TRUE(LoadSceneFile(dir_ / "main.json", scene).ok);
    EXPECT_TRUE(log.Errors().empty());
}

// A true / false variable keeps its kind through a save and a load; a number variable writes no "kind", so scenes
// from before true / false variables stay the same file.
TEST_F(SceneFileTest, TrueFalseVariablesKeepTheirKind)
{
    using Engine::Logic::VariableKind;
    GameObject hero{.id = 1, .name = "Hero"};
    hero.logic =
        Engine::Logic::LogicDesc{.variables = {{.name = "Speed", .value = 5.0f},
                                               {.name = "IsAlive", .value = 1.0f, .kind = VariableKind::TrueFalse}}};
    SaveSceneFile(dir_ / "main.json", {.objects = {hero}});

    const std::string text = Read();
    const std::size_t first = text.find("\"kind\"");
    ASSERT_NE(first, std::string::npos);
    EXPECT_EQ(text.find("\"kind\"", first + 1), std::string::npos); // only IsAlive has one
    EXPECT_NE(text.find("\"true_false\""), std::string::npos);

    SceneDescription loaded;
    ASSERT_TRUE(LoadSceneFile(dir_ / "main.json", loaded).ok);
    ASSERT_EQ(loaded.objects.size(), 1u);
    EXPECT_EQ(loaded.objects[0], hero);
}

// A variable without a kind (an older file) is a number; a kind this build does not know is read as a number too,
// with a warning.
TEST_F(SceneFileTest, VariablesWithoutAKnownKindAreNumbers)
{
    SceneDescription scene;
    const auto report = LoadSceneFile(WriteObject(R"({ "id": 1, "name": "Hero", "logic": { "variables": [
        { "name": "Score", "value": 3 }, { "name": "Mood", "value": 1, "kind": "colour" } ], "scripts": [] } })"),
                                      scene);

    ASSERT_TRUE(report.ok) << report.error;
    ASSERT_TRUE(scene.objects[0].logic.has_value());
    const auto& variables = scene.objects[0].logic->variables;
    ASSERT_EQ(variables.size(), 2u);
    EXPECT_EQ(variables[0].kind, Engine::Logic::VariableKind::Number);
    EXPECT_EQ(variables[1].kind, Engine::Logic::VariableKind::Number);
    EXPECT_TRUE(AnyContains(report.warnings, "kind \"colour\" is not known"));
}

// --- Shape names (as scene files write them) ---

TEST(SceneFileShapes, NamesRoundTripIgnoringCase)
{
    for (const Shape shape : {Shape::Quad, Shape::Circle, Shape::Cube, Shape::Pyramid, Shape::Plane, Shape::Wirebox,
                              Shape::Sphere, Shape::Line})
    {
        EXPECT_EQ(Engine::Game::ParseShape(Engine::Game::ToString(shape)), shape);
    }
    EXPECT_STREQ(Engine::Game::ToString(Shape::Wirebox), "wirebox");
    EXPECT_EQ(Engine::Game::ParseShape("SPHERE"), Shape::Sphere);
    EXPECT_FALSE(Engine::Game::ParseShape("teapot").has_value());
}

// Compound colliders and the locks and layers of a rigid body are kept by the scene file
TEST(RigidBodyFile, ColliderPartsLocksAndLayersRoundTrip)
{
    const auto dir = std::filesystem::temp_directory_path() / "atelier_rigid_body_file_tests";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    Engine::Scene::RigidBodyComponent body;
    body.shape = Engine::Scene::ColliderShape::Compound;
    body.parts = {{.shape = Engine::Scene::ColliderShape::Sphere, .centerY = 1.5f, .radius = 0.25f},
                  {.shape = Engine::Scene::ColliderShape::Box, .sizeX = 3.0f}};
    body.lockMoveY = true;
    body.lockTurnZ = true;
    body.layers = 6;
    Engine::Scene::SceneDescription scene;
    scene.objects.push_back(Engine::Scene::GameObject{.id = 1, .name = "Crate"});
    scene.objects[0].rigidBody = body;

    Engine::Scene::SaveSceneFile(dir / "scene.json", scene);
    Engine::Scene::SceneDescription loaded;
    const auto report = Engine::Scene::LoadSceneFile(dir / "scene.json", loaded);

    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_TRUE(report.warnings.empty());
    ASSERT_TRUE(loaded.objects[0].rigidBody.has_value());
    EXPECT_EQ(*loaded.objects[0].rigidBody, body);
    std::filesystem::remove_all(dir);
}

TEST(RigidBodyFile, MeshIsAColliderShapeName)
{
    using Engine::Scene::ColliderShape;
    EXPECT_EQ(Engine::Scene::ParseColliderShape("mesh"), ColliderShape::Mesh);
    EXPECT_EQ(Engine::Scene::ParseColliderShape("compound"), ColliderShape::Compound);
    EXPECT_STREQ(Engine::Scene::ColliderShapeName(ColliderShape::Mesh), "mesh");
}
