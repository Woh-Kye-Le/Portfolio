#include "engine/assets/loaders/GltfLoader.hpp"
#include "engine/assets/loaders/MeshLoader.hpp"
#include "engine/assets/loaders/ModelLoader.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using Engine::Assets::GltfContainerOf;
using Engine::Assets::LoadMeshFromFile;
using Engine::Assets::LoadModelFromFile;
using Engine::Assets::ReadGltfImage;

namespace
{
    // One triangle: positions (36 bytes), uvs (24), indices (6 + 2 padding), then the packed image (7)
    const std::vector<std::uint8_t> kImage = {'P', 'N', 'G', 'D', 'A', 'T', 'A'};

    std::vector<std::uint8_t> TriangleBuffer()
    {
        std::vector<std::uint8_t> bytes;
        const auto add = [&bytes](const void* data, std::size_t size)
        {
            const auto* begin = static_cast<const std::uint8_t*>(data);
            bytes.insert(bytes.end(), begin, begin + size);
        };
        const float positions[] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
        const float uvs[] = {0, 0, 1, 0, 0, 0.25f};
        const std::uint16_t indices[] = {0, 1, 2, 0};
        add(positions, sizeof(positions));
        add(uvs, sizeof(uvs));
        add(indices, sizeof(indices));
        add(kImage.data(), kImage.size());
        return bytes;
    }

    const std::string kTriangleJson = R"({
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "Body", "mesh": 0, "translation": [1, 2, 3]}],
        "meshes": [{"name": "Tri", "primitives": [{"attributes": {"POSITION": 0, "TEXCOORD_0": 1}, "indices": 2,
                    "material": 0}]}],
        "materials": [{"name": "Paint", "pbrMetallicRoughness": {"baseColorFactor": [1, 0.5, 0.25, 1],
                       "metallicFactor": 0.0, "roughnessFactor": 0.75, "baseColorTexture": {"index": 0}}}],
        "textures": [{"source": 0}],
        "images": [{"bufferView": 3, "mimeType": "image/png"}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"},
            {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC2"},
            {"bufferView": 2, "componentType": 5123, "count": 3, "type": "SCALAR"}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": 36},
            {"buffer": 0, "byteOffset": 36, "byteLength": 24},
            {"buffer": 0, "byteOffset": 60, "byteLength": 6},
            {"buffer": 0, "byteOffset": 68, "byteLength": 7}],
        "buffers": [{"byteLength": 75}]})";

    std::vector<std::uint8_t> Glb(std::string json, std::vector<std::uint8_t> bin)
    {
        while (json.size() % 4 != 0)
        {
            json.push_back(' ');
        }
        while (bin.size() % 4 != 0)
        {
            bin.push_back(0);
        }
        std::vector<std::uint8_t> bytes;
        const auto u32 = [&bytes](std::uint32_t value)
        {
            std::uint8_t raw[4];
            std::memcpy(raw, &value, 4);
            bytes.insert(bytes.end(), raw, raw + 4);
        };
        u32(0x46546C67);
        u32(2);
        u32(static_cast<std::uint32_t>(12 + 8 + json.size() + 8 + bin.size()));
        u32(static_cast<std::uint32_t>(json.size()));
        u32(0x4E4F534A);
        bytes.insert(bytes.end(), json.begin(), json.end());
        u32(static_cast<std::uint32_t>(bin.size()));
        u32(0x004E4942);
        bytes.insert(bytes.end(), bin.begin(), bin.end());
        return bytes;
    }

    class GltfLoaderTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            root_ = std::filesystem::temp_directory_path() / "atelier_gltf_tests" / info->name();
            std::filesystem::remove_all(root_);
            std::filesystem::create_directories(root_);
        }

        void TearDown() override { std::filesystem::remove_all(root_); }

        std::filesystem::path Write(const std::string& name, const std::vector<std::uint8_t>& bytes) const
        {
            const auto path = root_ / name;
            std::ofstream(path, std::ios::binary)
                .write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            return path;
        }

        std::filesystem::path root_;
    };
}

TEST_F(GltfLoaderTest, LoadsAGlbsMeshNodeAndMaterial)
{
    const auto model = LoadModelFromFile(Write("tri.glb", Glb(kTriangleJson, TriangleBuffer())));
    ASSERT_TRUE(model.has_value());
    EXPECT_EQ(model->name, "tri");

    ASSERT_EQ(model->meshes.size(), 1u);
    const auto& mesh = model->meshes[0];
    EXPECT_EQ(mesh.name, "Tri");
    EXPECT_EQ(mesh.positions, (std::vector<float>{0, 0, 0, 1, 0, 0, 0, 1, 0}));
    EXPECT_EQ(mesh.indices, (std::vector<std::uint32_t>{0, 1, 2}));
    // glTF's v runs down the image, the engine's up
    EXPECT_EQ(mesh.uvs, (std::vector<float>{0, 1, 1, 1, 0, 0.75f}));
    ASSERT_EQ(mesh.subMeshes.size(), 1u);
    EXPECT_EQ(mesh.subMeshes[0].indexCount, 3u);
    EXPECT_EQ(mesh.subMeshes[0].materialIndex, 0u);

    ASSERT_EQ(model->materials.size(), 1u);
    const auto& material = model->materials[0];
    EXPECT_EQ(material.name, "Paint");
    EXPECT_EQ(material.baseColorFactor, glm::vec4(1.0f, 0.5f, 0.25f, 1.0f));
    EXPECT_FLOAT_EQ(material.metallic, 0.0f);
    EXPECT_FLOAT_EQ(material.roughness, 0.75f);
    EXPECT_EQ(material.baseColorTexture, "tri.glb#0");

    // The file's node, under the node that turns glTF's +Z front to the engine's -Z forward
    ASSERT_EQ(model->nodes.size(), 2u);
    EXPECT_EQ(model->nodes[0].name, "Body");
    EXPECT_EQ(model->nodes[0].meshIndices, (std::vector<std::uint32_t>{0}));
    EXPECT_EQ(model->nodes[0].localTransform[3], glm::vec4(1.0f, 2.0f, 3.0f, 1.0f));
    EXPECT_EQ(model->roots, (std::vector<std::uint32_t>{1}));
    EXPECT_EQ(model->nodes[1].children, (std::vector<std::uint32_t>{0}));
}

TEST_F(GltfLoaderTest, ReadsTheImagePackedInAGlb)
{
    const auto file = Write("tri.glb", Glb(kTriangleJson, TriangleBuffer()));
    EXPECT_EQ(GltfContainerOf(root_ / "tri.glb#0"), file);
    EXPECT_EQ(GltfContainerOf(root_ / "wood.png"), root_ / "wood.png");
    EXPECT_EQ(ReadGltfImage(root_ / "tri.glb#0"), kImage);
    EXPECT_FALSE(ReadGltfImage(root_ / "tri.glb#1").has_value());
    EXPECT_FALSE(ReadGltfImage(root_ / "tri.glb").has_value());
}

TEST_F(GltfLoaderTest, LoadsAGlbAsOneMeshWithItsNodeTransform)
{
    const auto mesh = LoadMeshFromFile(Write("tri.glb", Glb(kTriangleJson, TriangleBuffer())));
    ASSERT_TRUE(mesh.has_value());
    // Moved by its node to (1, 2, 3), then turned round Y to face the engine's forward
    const std::vector<float> expected{-1, 2, -3, -2, 2, -3, -1, 3, -3};
    ASSERT_EQ(mesh->positions.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_NEAR(mesh->positions[i], expected[i], 1e-5f) << i;
    }
}

TEST_F(GltfLoaderTest, LoadsAGltfWithADataUriAndNoMaterial)
{
    // AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA = the floats 0 0 0  1 0 0  0 1 0
    const std::string json = R"({
        "asset": {"version": "2.0"},
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}}]}],
        "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"}],
        "bufferViews": [{"buffer": 0, "byteLength": 36}],
        "buffers": [{"byteLength": 36,
                     "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA"}]})";
    const auto model = LoadModelFromFile(Write("tri.gltf", std::vector<std::uint8_t>(json.begin(), json.end())));
    ASSERT_TRUE(model.has_value());
    ASSERT_EQ(model->meshes.size(), 1u);
    EXPECT_EQ(model->meshes[0].positions, (std::vector<float>{0, 0, 0, 1, 0, 0, 0, 1, 0}));
    EXPECT_EQ(model->meshes[0].indices, (std::vector<std::uint32_t>{0, 1, 2}));
    EXPECT_TRUE(model->meshes[0].uvs.empty());
    // A primitive without a material gets a default one
    ASSERT_EQ(model->materials.size(), 1u);
    EXPECT_EQ(model->meshes[0].subMeshes[0].materialIndex, 0u);
    EXPECT_TRUE(model->nodes.empty());
}

TEST_F(GltfLoaderTest, RejectsBrokenFiles)
{
    EXPECT_FALSE(LoadModelFromFile(root_ / "missing.glb").has_value());
    EXPECT_FALSE(LoadModelFromFile(Write("text.glb", {'n', 'o', 't', ' ', 'g', 'l', 't', 'f'})).has_value());
    // The indices point past the buffer
    std::vector<std::uint8_t> shortBuffer = TriangleBuffer();
    shortBuffer.resize(40);
    EXPECT_FALSE(LoadModelFromFile(Write("short.glb", Glb(kTriangleJson, shortBuffer))).has_value());
}

namespace
{
    // A triangle bent by a two-bone skeleton (Root > Bone) whose Bone rises 2 over a second
    std::vector<std::uint8_t> SkinnedBuffer()
    {
        std::vector<std::uint8_t> bytes;
        const auto add = [&bytes](const void* data, std::size_t size)
        {
            const auto* begin = static_cast<const std::uint8_t*>(data);
            bytes.insert(bytes.end(), begin, begin + size);
        };
        const float positions[] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
        const std::uint8_t joints[] = {0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0};
        const float weights[] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 3, 0, 0};
        const float identity[] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        const float times[] = {0, 1};
        const float rise[] = {0, 0, 0, 0, 2, 0};
        const std::uint16_t indices[] = {0, 1, 2, 0};
        add(positions, sizeof(positions)); // 0
        add(joints, sizeof(joints));       // 36
        add(weights, sizeof(weights));     // 48
        add(identity, sizeof(identity));   // 96
        add(identity, sizeof(identity));   // 160
        add(times, sizeof(times));         // 224
        add(rise, sizeof(rise));           // 232
        add(indices, sizeof(indices));     // 256
        return bytes;
    }

    const std::string kSkinnedJson = R"({
        "asset": {"version": "2.0"},
        "scenes": [{"nodes": [0, 1]}],
        "nodes": [{"name": "Body", "mesh": 0, "skin": 0}, {"name": "Root", "children": [2]}, {"name": "Bone"}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "JOINTS_0": 1, "WEIGHTS_0": 2}, "indices": 6}]}],
        "skins": [{"name": "Armature", "joints": [1, 2], "inverseBindMatrices": 3}],
        "animations": [{"name": "Rise", "samplers": [{"input": 4, "output": 5}],
                        "channels": [{"sampler": 0, "target": {"node": 2, "path": "translation"}}]}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"},
            {"bufferView": 1, "componentType": 5121, "count": 3, "type": "VEC4"},
            {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC4"},
            {"bufferView": 3, "componentType": 5126, "count": 2, "type": "MAT4"},
            {"bufferView": 4, "componentType": 5126, "count": 2, "type": "SCALAR"},
            {"bufferView": 5, "componentType": 5126, "count": 2, "type": "VEC3"},
            {"bufferView": 6, "componentType": 5123, "count": 3, "type": "SCALAR"}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": 36},
            {"buffer": 0, "byteOffset": 36, "byteLength": 12},
            {"buffer": 0, "byteOffset": 48, "byteLength": 48},
            {"buffer": 0, "byteOffset": 96, "byteLength": 128},
            {"buffer": 0, "byteOffset": 224, "byteLength": 8},
            {"buffer": 0, "byteOffset": 232, "byteLength": 24},
            {"buffer": 0, "byteOffset": 256, "byteLength": 6}],
        "buffers": [{"byteLength": 264}]})";
}

TEST_F(GltfLoaderTest, LoadsASkeletonAndItsAnimation)
{
    const auto model = LoadModelFromFile(Write("rig.glb", Glb(kSkinnedJson, SkinnedBuffer())));
    ASSERT_TRUE(model.has_value());
    ASSERT_EQ(model->meshes.size(), 1u);
    const auto& mesh = model->meshes[0];
    EXPECT_EQ(mesh.skin, 0);
    EXPECT_EQ(mesh.joints, (std::vector<std::uint16_t>{0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0}));
    // Weights are made to add up to 1
    EXPECT_EQ(mesh.weights, (std::vector<float>{1, 0, 0, 0, 1, 0, 0, 0, 0.25f, 0.75f, 0, 0}));

    ASSERT_EQ(model->skins.size(), 1u);
    EXPECT_EQ(model->skins[0].name, "Armature");
    EXPECT_EQ(model->skins[0].joints, (std::vector<std::uint32_t>{1, 2}));
    EXPECT_EQ(model->skins[0].inverseBind, (std::vector<glm::mat4>(2, glm::mat4(1.0f))));

    ASSERT_EQ(model->animations.size(), 1u);
    const auto& clip = model->animations[0];
    EXPECT_EQ(clip.name, "Rise");
    EXPECT_FLOAT_EQ(clip.duration, 1.0f);
    ASSERT_EQ(clip.channels.size(), 1u);
    EXPECT_EQ(clip.channels[0].node, 2u);
    EXPECT_EQ(clip.channels[0].path, Engine::Assets::AnimationChannel::Path::Translation);
    EXPECT_EQ(clip.channels[0].times, (std::vector<float>{0, 1}));
    EXPECT_EQ(clip.channels[0].values, (std::vector<float>{0, 0, 0, 0, 2, 0}));
}

TEST_F(GltfLoaderTest, AMeshWithoutASkinDropsItsBoneWeights)
{
    std::string json = kSkinnedJson;
    json.replace(json.find(R"(, "skin": 0)"), std::string(R"(, "skin": 0)").size(), "");
    const auto model = LoadModelFromFile(Write("rig.glb", Glb(json, SkinnedBuffer())));
    ASSERT_TRUE(model.has_value());
    EXPECT_EQ(model->meshes[0].skin, -1);
    EXPECT_TRUE(model->meshes[0].joints.empty());
    EXPECT_TRUE(model->meshes[0].weights.empty());
}
