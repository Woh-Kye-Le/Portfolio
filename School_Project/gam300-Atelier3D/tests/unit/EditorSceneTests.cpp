#include "engine/editor/EditorScene.hpp"
#include "engine/editor/TutorialNotes.hpp"
#include "engine/scene/components/TransformComponent.hpp"

#include <imgui/imgui.h> // before ImGuizmo.h, which uses its types

#include <ImGuizmo/ImGuizmo.h>

#include <glm/gtc/type_ptr.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using Engine::Editor::EditorScene;
using Engine::Editor::GameObject;
using Engine::Editor::TutorialMode;
using Engine::Editor::TutorialNotes;
using Engine::Game::Shape;
using Engine::Scene::SceneDescription;

namespace
{
    // A player, a camera with no visuals and an object with no components
    SceneDescription SmallScene()
    {
        GameObject hero{.id = 3, .name = "Hero"};
        hero.transform = Engine::Scene::TransformComponent{.x = 1.5f, .scale = 2.5f};
        hero.renderable = Engine::Scene::RenderableDesc{.shape = Shape::Sphere, .texture = "assets/textures/crate.png"};
        hero.player.emplace();

        GameObject camera{.id = 5, .name = "Main Camera"};
        camera.transform = Engine::Scene::TransformComponent{.y = 2.0f, .z = 8.0f};
        camera.camera = Engine::Scene::CameraComponent{.fieldOfView = 70.0f, .primary = true};

        return {.objects = {hero, camera, GameObject{.id = 9, .name = "Empty"}}};
    }

    class EditorSceneFileTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            dir_ = std::filesystem::temp_directory_path() / "atelier_editor_scene_tests" / info->name();
            std::filesystem::remove_all(dir_);
            std::filesystem::create_directories(dir_);
        }

        void TearDown() override { std::filesystem::remove_all(dir_); }

        std::filesystem::path dir_;
    };
}

// --- The editor edits the same scene description the file holds and the game plays ---

TEST(EditorScene, KeepsALoadedSceneAsItIs)
{
    EditorScene editor;
    editor.SetDescription(SmallScene());

    EXPECT_EQ(editor.Description(), SmallScene());
    ASSERT_EQ(editor.Objects().size(), 3u);
    EXPECT_TRUE(editor.Objects()[0].player.has_value());
    EXPECT_FALSE(editor.Objects()[1].renderable.has_value()); // a camera with no visuals of its own
    EXPECT_EQ(editor.Selected(), 0u);                         // nothing selected after loading
}

// After loading a scene whose biggest id is 9, a new object gets an id above 9.
TEST(EditorScene, NewObjectsDoNotReuseLoadedIds)
{
    EditorScene editor;
    editor.SetDescription(SmallScene());

    EXPECT_GT(editor.CreateObject("New").id, 9u);
}

// Create Game Object makes an empty object (no components at all) with a name, and selects it.
TEST(EditorScene, NewGameObjectsHaveNoComponents)
{
    EditorScene editor;
    const GameObject& object = editor.CreateObject();

    EXPECT_FALSE(object.transform || object.renderable || object.audioSource || object.player || object.camera ||
                 object.light);
    EXPECT_FALSE(object.name.empty());
    EXPECT_EQ(editor.Selected(), object.id);
}

// Duplicate and Paste copy exactly the components the source has, with a new id; Paste adds " Copy".
TEST(EditorScene, CopiesHaveTheSameComponentsOnly)
{
    EditorScene editor;
    GameObject& source = editor.CreateObject("Box");
    source.renderable.emplace().shape = Shape::Pyramid;
    const std::uint32_t sourceId = source.id;

    const GameObject duplicate = editor.Duplicate(sourceId);
    const GameObject pasted = editor.Paste(*editor.Find(sourceId));

    for (const GameObject& copy : {duplicate, pasted})
    {
        EXPECT_NE(copy.id, sourceId);
        EXPECT_FALSE(copy.transform.has_value());
        ASSERT_TRUE(copy.renderable.has_value());
        EXPECT_EQ(copy.renderable->shape, Shape::Pyramid);
    }
    EXPECT_NE(duplicate.id, pasted.id);
    EXPECT_EQ(pasted.name, "Box Copy");
}

// Deleting the selected object clears the selection; deleting it again returns false.
TEST(EditorScene, RemovingTheSelectedObjectClearsTheSelection)
{
    EditorScene editor;
    const std::uint32_t id = editor.CreateObject("Gone").id;

    EXPECT_TRUE(editor.Remove(id));
    EXPECT_EQ(editor.Selected(), 0u);
    EXPECT_EQ(editor.Find(id), nullptr);
    EXPECT_FALSE(editor.Remove(id));
}

// Undo: restoring a snapshot brings back the scene exactly as it was before the edits.
TEST(EditorScene, RestoringAStateUndoesEdits)
{
    EditorScene editor;
    editor.SetDescription(SmallScene());
    const EditorScene::State before = editor.CaptureState();

    editor.Objects()[0].transform->x = 10.0f;
    editor.CreateObject("Added");
    editor.RestoreState(before);

    EXPECT_EQ(editor.Description(), SmallScene());
    EXPECT_EQ(editor.CaptureState(), before);
}

// --- Tutorial notes (the Modes page) ---

TEST_F(EditorSceneFileTest, TutorialNotesRoundTrip)
{
    std::vector<TutorialMode> saved = TutorialNotes::Defaults();
    saved[0].tutorials.push_back({"Tutorial 1", "line one\nline \"two\" \\ with symbols"});
    saved[0].nextIndex = 2;
    saved[0].done = 1;
    saved.push_back({"My mode", {{"Intro", ""}}, 5});
    const auto path = dir_ / "data" / "tutorials.editor.modes";

    std::string error;
    ASSERT_TRUE(TutorialNotes::Save(saved, path, error)) << error;
    std::vector<TutorialMode> loaded;
    ASSERT_TRUE(TutorialNotes::Load(loaded, path, error)) << error;

    EXPECT_EQ(loaded, saved);
}

// An old text-format notes file fails to load with an error message, and the notes stay as they were.
TEST_F(EditorSceneFileTest, TutorialNotesFailWithoutChangingAnything)
{
    const auto path = dir_ / "tutorials.editor.modes";
    std::ofstream(path) << "# Atelier3D editor tutorials\nmode 1 Mode 1\n"; // the old text format
    std::vector<TutorialMode> modes = TutorialNotes::Defaults();

    std::string error;
    EXPECT_FALSE(TutorialNotes::Load(modes, path, error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(modes, TutorialNotes::Defaults());
}

// --- Object transforms: the editor's gizmos read them back with ImGuizmo ---

TEST(ObjectTransform, MatchesImGuizmosComposition)
{
    const float translation[3] = {1.5f, -2.0f, 3.0f};
    const float rotation[3] = {20.0f, -35.0f, 70.0f};
    const float scale[3] = {2.0f, 0.5f, 1.5f};
    float expected[16];
    ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, expected);

    const Engine::Scene::TransformComponent transform{.x = 1.5f,
                                                      .y = -2.0f,
                                                      .z = 3.0f,
                                                      .rotation = -35.0f,
                                                      .rotationX = 20.0f,
                                                      .rotationZ = 70.0f,
                                                      .scaleX = 2.0f,
                                                      .scaleY = 0.5f,
                                                      .scaleZ = 1.5f};
    const glm::mat4 actual = Engine::Scene::ToGraphicsTransform(transform).ComposeModelMatrix();
    const float* values = glm::value_ptr(actual);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_NEAR(values[i], expected[i], 1e-4f) << "element " << i;
    }
}

// Splitting our model matrix back up with ImGuizmo gives the same position, rotation and scale (scale = size times the
// per-axis scale), so the gizmos edit the right values.
TEST(ObjectTransform, DecomposingGivesTheFieldsBack)
{
    const Engine::Scene::TransformComponent transform{.x = 1.0f,
                                                      .y = 2.0f,
                                                      .z = 3.0f,
                                                      .rotation = 40.0f,
                                                      .rotationX = 10.0f,
                                                      .rotationZ = -25.0f,
                                                      .scale = 2.0f,
                                                      .scaleX = 1.5f,
                                                      .scaleY = 0.5f};

    const glm::mat4 model = Engine::Scene::ToGraphicsTransform(transform).ComposeModelMatrix();
    float translation[3];
    float rotation[3];
    float scale[3];
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), translation, rotation, scale);

    EXPECT_NEAR(translation[0], 1.0f, 1e-4f);
    EXPECT_NEAR(translation[2], 3.0f, 1e-4f);
    EXPECT_NEAR(rotation[0], 10.0f, 1e-3f);
    EXPECT_NEAR(rotation[1], 40.0f, 1e-3f);
    EXPECT_NEAR(rotation[2], -25.0f, 1e-3f);
    EXPECT_NEAR(scale[0], 3.0f, 1e-4f); // scale * scaleX
    EXPECT_NEAR(scale[1], 1.0f, 1e-4f);
    EXPECT_NEAR(scale[2], 2.0f, 1e-4f);
}

// rotation = 90 turns around Y, so +X ends up pointing at -Z.
TEST(ObjectTransform, YawTurnsAroundY)
{
    const Engine::Scene::TransformComponent transform{.rotation = 90.0f};
    const glm::mat4 spun = Engine::Scene::ToGraphicsTransform(transform).ComposeModelMatrix();
    // Rotated 90 degrees around Y: +X ends up at -Z
    const glm::vec4 x = spun * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_NEAR(x.x, 0.0f, 1e-5f);
    EXPECT_NEAR(x.z, -1.0f, 1e-5f);
}
