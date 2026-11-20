#include "engine/assets/Animation.hpp"
#include "engine/assets/loaders/AtelierModel.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using Engine::Assets::AnimationChannel;
using Engine::Assets::AnimationClip;
using Engine::Assets::Compose;
using Engine::Assets::Decompose;
using Engine::Assets::FindClip;
using Engine::Assets::ModelData;
using Engine::Assets::ModelNode;
using Engine::Assets::NodeWorlds;
using Engine::Assets::SampleClip;
using Engine::Assets::Skin;
using Engine::Assets::SkinMatrices;

namespace
{
    void ExpectNear(const glm::vec3& actual, const glm::vec3& expected)
    {
        EXPECT_NEAR(actual.x, expected.x, 1e-4f);
        EXPECT_NEAR(actual.y, expected.y, 1e-4f);
        EXPECT_NEAR(actual.z, expected.z, 1e-4f);
    }

    // Moves node 1 from (0, 0, 0) to (0, 2, 0) over a second, and turns it 90 degrees about Y
    AnimationClip RiseAndTurn()
    {
        AnimationClip clip;
        clip.name = "Rise";
        clip.duration = 1.0f;
        AnimationChannel rise{.node = 1, .path = AnimationChannel::Path::Translation};
        rise.times = {0.0f, 1.0f};
        rise.values = {0, 0, 0, 0, 2, 0};
        AnimationChannel turn{.node = 1, .path = AnimationChannel::Path::Rotation};
        turn.times = {0.0f, 1.0f};
        const glm::quat quarter = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));
        turn.values = {0, 0, 0, 1, quarter.x, quarter.y, quarter.z, quarter.w};
        clip.channels = {rise, turn};
        return clip;
    }
}

TEST(AnimationTest, DecomposeAndComposeGiveTheTransformBack)
{
    const glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(1, 2, 3)) *
                                glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(0, 0, 1)) *
                                glm::scale(glm::mat4(1.0f), glm::vec3(2, 3, 4));
    const auto pose = Decompose(transform);
    ExpectNear(pose.translation, glm::vec3(1, 2, 3));
    ExpectNear(pose.scale, glm::vec3(2, 3, 4));
    const glm::mat4 back = Compose(pose);
    for (int c = 0; c < 4; ++c)
    {
        ExpectNear(glm::vec3(back[c]), glm::vec3(transform[c]));
    }
}

TEST(AnimationTest, SamplesBetweenKeysAndHoldsTheEnds)
{
    const AnimationClip clip = RiseAndTurn();
    const std::vector<glm::mat4> rest(2, glm::mat4(1.0f));

    const auto half = SampleClip(clip, 0.5f, rest);
    ExpectNear(glm::vec3(half[1][3]), glm::vec3(0, 1, 0));
    // Turned 45 degrees: its front (-Z) points between -Z and -X
    const glm::vec3 front = glm::vec3(half[1] * glm::vec4(0, 0, -1, 0));
    ExpectNear(front, glm::vec3(-std::sqrt(0.5f), 0, -std::sqrt(0.5f)));
    EXPECT_EQ(half[0], glm::mat4(1.0f)) << "a node the clip does not move keeps its rest transform";

    ExpectNear(glm::vec3(SampleClip(clip, -1.0f, rest)[1][3]), glm::vec3(0, 0, 0));
    ExpectNear(glm::vec3(SampleClip(clip, 5.0f, rest)[1][3]), glm::vec3(0, 2, 0));
}

TEST(AnimationTest, StepKeysHoldUntilTheNextOne)
{
    AnimationClip clip = RiseAndTurn();
    clip.channels.resize(1);
    clip.channels[0].interpolation = AnimationChannel::Interpolation::Step;
    const std::vector<glm::mat4> rest(2, glm::mat4(1.0f));
    ExpectNear(glm::vec3(SampleClip(clip, 0.9f, rest)[1][3]), glm::vec3(0, 0, 0));
}

TEST(AnimationTest, AChannelKeepsThePartsItDoesNotMove)
{
    AnimationClip clip = RiseAndTurn();
    clip.channels.resize(1); // only the translation
    const std::vector<glm::mat4> rest{glm::mat4(1.0f), glm::scale(glm::mat4(1.0f), glm::vec3(3.0f))};
    const auto pose = Decompose(SampleClip(clip, 1.0f, rest)[1]);
    ExpectNear(pose.translation, glm::vec3(0, 2, 0));
    ExpectNear(pose.scale, glm::vec3(3.0f));
}

TEST(AnimationTest, FindsClipsByNameInAnyCase)
{
    const std::vector<AnimationClip> clips{AnimationClip{.name = "Idle"}, AnimationClip{.name = "Walk"}};
    EXPECT_EQ(FindClip(clips, "walk"), 1);
    EXPECT_EQ(FindClip(clips, "IDLE"), 0);
    EXPECT_EQ(FindClip(clips, "Run"), -1);
}

TEST(AnimationTest, NodesArePlacedDownTheHierarchyAndBonesUndoTheirRest)
{
    // Root at x = 5, its child Bone 1 above it
    std::vector<ModelNode> nodes(2);
    nodes[0].children = {1};
    const std::vector<glm::mat4> locals{glm::translate(glm::mat4(1.0f), glm::vec3(5, 0, 0)),
                                        glm::translate(glm::mat4(1.0f), glm::vec3(0, 1, 0))};
    const auto worlds = NodeWorlds(nodes, {0}, locals);
    ExpectNear(glm::vec3(worlds[1][3]), glm::vec3(5, 1, 0));

    // At rest the bone matrix is the identity: the mesh is drawn as modelled
    Skin skin;
    skin.joints = {1};
    skin.inverseBind = {glm::inverse(worlds[1])};
    const auto bones = SkinMatrices(skin, worlds);
    ASSERT_EQ(bones.size(), 1u);
    ExpectNear(glm::vec3(bones[0] * glm::vec4(1, 2, 3, 1)), glm::vec3(1, 2, 3));

    // The bone moved up by 2: what it carries moves up by 2
    const auto moved = NodeWorlds(nodes, {0}, {locals[0], glm::translate(glm::mat4(1.0f), glm::vec3(0, 3, 0))});
    ExpectNear(glm::vec3(SkinMatrices(skin, moved)[0] * glm::vec4(1, 2, 3, 1)), glm::vec3(1, 4, 3));
}

TEST(AnimationTest, TheModelFileKeepsSkinsAndAnimations)
{
    ModelData model;
    model.name = "rig";
    model.materials.resize(1);
    Engine::Assets::MeshData mesh;
    mesh.positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    mesh.indices = {0, 1, 2};
    mesh.joints = {0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0};
    mesh.weights = {1, 0, 0, 0, 1, 0, 0, 0, 0.25f, 0.75f, 0, 0};
    mesh.skin = 0;
    model.meshes = {mesh};
    model.nodes.resize(3);
    model.nodes[0].meshIndices = {0};
    model.nodes[1].children = {2};
    model.roots = {0, 1};
    model.skins = {Skin{.name = "Armature",
                        .joints = {1, 2},
                        .inverseBind = {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(0, -1, 0))}}};
    model.animations = {RiseAndTurn()};
    model.animations[0].channels[0].interpolation = AnimationChannel::Interpolation::Step;

    const auto read = Engine::Assets::ParseAtelierModel(Engine::Assets::WriteAtelierModel(model));
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(read->meshes[0].skin, 0);
    EXPECT_EQ(read->meshes[0].joints, mesh.joints);
    EXPECT_EQ(read->meshes[0].weights, mesh.weights);
    ASSERT_EQ(read->skins.size(), 1u);
    EXPECT_EQ(read->skins[0].name, "Armature");
    EXPECT_EQ(read->skins[0].joints, model.skins[0].joints);
    EXPECT_EQ(read->skins[0].inverseBind, model.skins[0].inverseBind);
    ASSERT_EQ(read->animations.size(), 1u);
    const AnimationClip& clip = read->animations[0];
    EXPECT_EQ(clip.name, "Rise");
    EXPECT_FLOAT_EQ(clip.duration, 1.0f);
    ASSERT_EQ(clip.channels.size(), 2u);
    EXPECT_EQ(clip.channels[0].interpolation, AnimationChannel::Interpolation::Step);
    EXPECT_EQ(clip.channels[1].path, AnimationChannel::Path::Rotation);
    EXPECT_EQ(clip.channels[1].times, model.animations[0].channels[1].times);
    EXPECT_EQ(clip.channels[1].values, model.animations[0].channels[1].values);
}

TEST(AnimationTest, TheBoxAroundAModelPlacesItsMeshesAndBendsItsSkins)
{
    ModelData model;
    Engine::Assets::MeshData triangle;
    triangle.positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    model.meshes = {triangle, triangle};
    // Mesh 1 is skinned to a bone 5 up whose inverse bind matrix is the identity: at rest it stands 5 up
    model.meshes[1].joints = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    model.meshes[1].weights = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    model.meshes[1].skin = 0;
    model.nodes.resize(3);
    model.nodes[0].localTransform = glm::translate(glm::mat4(1.0f), glm::vec3(-2, 0, 0));
    model.nodes[0].meshIndices = {0};
    model.nodes[1].meshIndices = {1};
    model.nodes[2].localTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0, 5, 0));
    model.skins = {Skin{.joints = {2}, .inverseBind = {glm::mat4(1.0f)}}};

    const auto box = Engine::Assets::RestBounds(model);
    ASSERT_TRUE(box.has_value());
    ExpectNear(box->min, glm::vec3(-2, 0, 0));
    ExpectNear(box->max, glm::vec3(1, 6, 0));

    EXPECT_FALSE(Engine::Assets::RestBounds(ModelData{}).has_value()) << "nothing to measure";
}

// A model's triangles at rest are placed by their nodes, like its box: what a click in the editor is tested against
TEST(AnimationTest, RestTrianglesArePlacedByTheirNodes)
{
    ModelData model;
    Engine::Assets::MeshData quad;
    quad.positions = {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
    quad.indices = {0, 1, 2, 0, 2, 3};
    model.meshes = {quad};
    model.nodes.resize(1);
    model.nodes[0].meshIndices = {0};
    model.nodes[0].localTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -4));
    model.roots = {0};

    const std::vector<glm::vec3> triangles = Engine::Assets::RestTriangles(model);
    ASSERT_EQ(triangles.size(), 6u) << "two triangles, three corners each";
    ExpectNear(triangles[0], glm::vec3(0, 0, -4));
    ExpectNear(triangles[2], glm::vec3(1, 1, -4));
    EXPECT_TRUE(Engine::Assets::RestTriangles(ModelData{}).empty());
}

TEST(AnimationTest, TheModelFileRejectsBonesThatDoNotExist)
{
    ModelData model;
    model.materials.resize(1);
    Engine::Assets::MeshData mesh;
    mesh.positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    mesh.indices = {0, 1, 2};
    mesh.joints = {0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0}; // joint 5 of a skin of one
    mesh.weights = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    mesh.skin = 0;
    model.meshes = {mesh};
    model.nodes.resize(1);
    model.roots = {0};
    model.skins = {Skin{.joints = {0}, .inverseBind = {glm::mat4(1.0f)}}};
    EXPECT_FALSE(Engine::Assets::ParseAtelierModel(Engine::Assets::WriteAtelierModel(model)).has_value());
}
