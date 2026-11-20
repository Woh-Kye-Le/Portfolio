#include "engine/assets/data/ModelData.hpp"
#include "engine/assets/loaders/AtelierModel.hpp"
#include "engine/assets/loaders/MeshLoader.hpp"
#include "engine/assets/loaders/ModelLoader.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using Engine::Assets::FlattenModelToMesh;
using Engine::Assets::LoadMeshFromFile;
using Engine::Assets::LoadModelFromFile;
using Engine::Assets::MaterialData;
using Engine::Assets::MeshData;
using Engine::Assets::ModelData;
using Engine::Assets::ModelNode;
using Engine::Assets::ParseAtelierModel;
using Engine::Assets::SubMesh;
using Engine::Assets::WriteAtelierModel;

namespace
{
    // A small model with everything the format carries: two materials (one textured, one emissive), a mesh with
    // UVs, normals and two material runs, a second mesh with neither, and a Group > Base > Child hierarchy
    ModelData SampleModel()
    {
        ModelData model;
        model.name = "sample";

        MaterialData wall;
        wall.name = "wall";
        wall.baseColorFactor = glm::vec4(1.0f, 0.5f, 0.25f, 0.75f);
        wall.metallic = 0.1f;
        wall.roughness = 0.9f;
        wall.baseColorTexture = "../textures/wall.png";
        wall.normalTexture = "wall_n.png";
        wall.metallicRoughnessTexture = "wall_mr.png";
        MaterialData lamp;
        lamp.name = "lamp";
        lamp.emissiveFactor = glm::vec3(1.0f, 0.9f, 0.5f);
        lamp.emissiveStrength = 1.0f;
        model.materials = {wall, lamp};

        MeshData quad;
        quad.name = "quad";
        quad.positions = {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
        quad.uvs = {0, 0, 1, 0, 1, 1, 0, 1};
        quad.normals = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
        quad.indices = {0, 1, 2, 0, 2, 3};
        quad.subMeshes = {SubMesh{0, 3, 0, 0}, SubMesh{3, 3, 0, 1}};
        MeshData triangle;
        triangle.name = "triangle";
        triangle.positions = {0, 0, 0, 2, 0, 0, 0, 2, 0};
        triangle.indices = {0, 1, 2};
        model.meshes = {quad, triangle};

        ModelNode group;
        group.name = "Group";
        group.localTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 3.0f));
        group.children = {1};
        ModelNode base;
        base.name = "Base";
        base.localTransform = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f));
        base.meshIndices = {0};
        base.children = {2};
        ModelNode child;
        child.name = "Child";
        child.localTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 5.0f, 0.0f));
        child.meshIndices = {1};
        model.nodes = {group, base, child};
        model.roots = {0};
        return model;
    }

    // The checksum is the last four bytes: recompute it after a deliberate edit so the edit is what gets rejected
    void FixChecksum(std::vector<std::uint8_t>& bytes)
    {
        std::uint32_t hash = 2166136261u;
        for (std::size_t i = 0; i + 4 < bytes.size(); ++i)
        {
            hash = (hash ^ bytes[i]) * 16777619u;
        }
        for (int i = 0; i < 4; ++i)
        {
            bytes[bytes.size() - 4 + i] = static_cast<std::uint8_t>(hash >> (8 * i));
        }
    }

    class AtelierModelTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            root_ = std::filesystem::temp_directory_path() / "atelier_amodel_tests" / info->name();
            std::filesystem::remove_all(root_);
            std::filesystem::create_directories(root_);
        }

        void TearDown() override { std::filesystem::remove_all(root_); }

        std::filesystem::path WriteBytes(const std::string& name, const std::vector<std::uint8_t>& bytes) const
        {
            const auto path = root_ / name;
            std::ofstream(path, std::ios::binary)
                .write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            return path;
        }

        std::filesystem::path root_;
    };
}

// Writing and reading a model gives the same model back: every material factor and texture, mesh array, material
// run, node transform and link.
TEST(AtelierModelFormatTest, RoundTripsAWholeModel)
{
    const ModelData original = SampleModel();
    const auto bytes = WriteAtelierModel(original);
    const auto read = ParseAtelierModel(bytes);
    ASSERT_TRUE(read.has_value());

    EXPECT_EQ(read->name, "sample");
    ASSERT_EQ(read->materials.size(), 2u);
    EXPECT_EQ(read->materials[0].name, "wall");
    EXPECT_EQ(read->materials[0].baseColorFactor, glm::vec4(1.0f, 0.5f, 0.25f, 0.75f));
    EXPECT_EQ(read->materials[0].metallic, 0.1f);
    EXPECT_EQ(read->materials[0].roughness, 0.9f);
    EXPECT_EQ(read->materials[0].baseColorTexture, "../textures/wall.png");
    EXPECT_EQ(read->materials[0].normalTexture, "wall_n.png");
    EXPECT_EQ(read->materials[0].metallicRoughnessTexture, "wall_mr.png");
    EXPECT_EQ(read->materials[1].emissiveFactor, glm::vec3(1.0f, 0.9f, 0.5f));
    EXPECT_EQ(read->materials[1].emissiveStrength, 1.0f);

    ASSERT_EQ(read->meshes.size(), 2u);
    EXPECT_EQ(read->meshes[0].name, "quad");
    EXPECT_EQ(read->meshes[0].positions, original.meshes[0].positions);
    EXPECT_EQ(read->meshes[0].uvs, original.meshes[0].uvs);
    EXPECT_EQ(read->meshes[0].normals, original.meshes[0].normals);
    EXPECT_EQ(read->meshes[0].indices, original.meshes[0].indices);
    ASSERT_EQ(read->meshes[0].subMeshes.size(), 2u);
    EXPECT_EQ(read->meshes[0].subMeshes[1].baseIndex, 3u);
    EXPECT_EQ(read->meshes[0].subMeshes[1].indexCount, 3u);
    EXPECT_EQ(read->meshes[0].subMeshes[1].materialIndex, 1u);
    EXPECT_TRUE(read->meshes[1].uvs.empty());
    EXPECT_TRUE(read->meshes[1].normals.empty());
    EXPECT_TRUE(read->meshes[1].subMeshes.empty());

    ASSERT_EQ(read->nodes.size(), 3u);
    EXPECT_EQ(read->nodes[1].name, "Base");
    EXPECT_EQ(read->nodes[1].localTransform, original.nodes[1].localTransform);
    EXPECT_EQ(read->nodes[1].meshIndices, original.nodes[1].meshIndices);
    EXPECT_EQ(read->nodes[0].children, original.nodes[0].children);
    EXPECT_EQ(read->roots, original.roots);
}

TEST(AtelierModelFormatTest, RoundTripsAModelWithoutNodes)
{
    ModelData original = SampleModel();
    original.nodes.clear();
    original.roots.clear();
    const auto read = ParseAtelierModel(WriteAtelierModel(original));
    ASSERT_TRUE(read.has_value());
    EXPECT_TRUE(read->nodes.empty());
    EXPECT_EQ(read->meshes.size(), 2u);
}

// UVs or normals that do not match the vertex count cannot be addressed per vertex, so they are left out rather than
// written wrong.
TEST(AtelierModelFormatTest, LeavesOutUvsAndNormalsThatDoNotMatchTheVertices)
{
    ModelData original = SampleModel();
    original.meshes[0].uvs.pop_back();
    original.meshes[0].normals.resize(5);
    const auto read = ParseAtelierModel(WriteAtelierModel(original));
    ASSERT_TRUE(read.has_value());
    EXPECT_TRUE(read->meshes[0].uvs.empty());
    EXPECT_TRUE(read->meshes[0].normals.empty());
    EXPECT_EQ(read->meshes[0].positions.size(), 12u);
}

TEST(AtelierModelFormatTest, TheBytesStartWithTheMagicAndVersion)
{
    const auto bytes = WriteAtelierModel(SampleModel());
    ASSERT_GT(bytes.size(), 12u);
    EXPECT_EQ(std::string(bytes.begin(), bytes.begin() + 4), "AMDL");
    EXPECT_EQ(bytes[4], Engine::Assets::kAtelierModelVersion);
    EXPECT_EQ(bytes[5], 0);
}

TEST(AtelierModelFormatTest, RejectsDataThatIsNotAModelFile)
{
    EXPECT_FALSE(ParseAtelierModel({}).has_value());
    EXPECT_FALSE(ParseAtelierModel({'A', 'M', 'D', 'L'}).has_value());
    EXPECT_FALSE(ParseAtelierModel(std::vector<std::uint8_t>(64, 0)).has_value());

    auto bytes = WriteAtelierModel(SampleModel());
    bytes[0] = 'X';
    FixChecksum(bytes);
    EXPECT_FALSE(ParseAtelierModel(bytes).has_value());
}

TEST(AtelierModelFormatTest, RejectsATruncatedFile)
{
    const auto bytes = WriteAtelierModel(SampleModel());
    for (const std::size_t keep : {bytes.size() - 1, bytes.size() / 2, std::size_t{13}})
    {
        EXPECT_FALSE(ParseAtelierModel(std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + keep)).has_value())
            << keep;
    }
}

TEST(AtelierModelFormatTest, RejectsAChangedByte)
{
    auto bytes = WriteAtelierModel(SampleModel());
    bytes[bytes.size() / 2] ^= 0x40;
    EXPECT_FALSE(ParseAtelierModel(bytes).has_value());
}

TEST(AtelierModelFormatTest, RejectsAVersionFromTheFuture)
{
    auto bytes = WriteAtelierModel(SampleModel());
    bytes[4] = static_cast<std::uint8_t>(Engine::Assets::kAtelierModelVersion + 1);
    FixChecksum(bytes);
    EXPECT_FALSE(ParseAtelierModel(bytes).has_value());

    bytes[4] = 0; // version 0 never existed
    FixChecksum(bytes);
    EXPECT_FALSE(ParseAtelierModel(bytes).has_value());
}

// A count that promises more data than the file holds is rejected without trying to allocate it.
TEST(AtelierModelFormatTest, RejectsACountLargerThanTheFile)
{
    ModelData tiny;
    tiny.name = "t";
    auto bytes = WriteAtelierModel(tiny);
    // magic(4) version(4) nameLength(4) "t"(1) then materialCount: claim four billion materials
    for (int i = 0; i < 4; ++i)
    {
        bytes[13 + i] = 0xFF;
    }
    FixChecksum(bytes);
    EXPECT_FALSE(ParseAtelierModel(bytes).has_value());
}

// Data that is intact but points at things that do not exist is rejected: a triangle index past the vertices, a
// material run past the indices or naming a missing material, links to missing meshes, nodes or roots.
TEST(AtelierModelFormatTest, RejectsIndicesThatPointNowhere)
{
    const auto rejected = [](ModelData model) { return !ParseAtelierModel(WriteAtelierModel(model)).has_value(); };

    ModelData badIndex = SampleModel();
    badIndex.meshes[0].indices[2] = 4; // the quad has four vertices: 0 to 3
    EXPECT_TRUE(rejected(badIndex));

    ModelData badRun = SampleModel();
    badRun.meshes[0].subMeshes[1].indexCount = 4; // runs past the six indices
    EXPECT_TRUE(rejected(badRun));

    ModelData badMaterial = SampleModel();
    badMaterial.meshes[0].subMeshes[0].materialIndex = 2;
    EXPECT_TRUE(rejected(badMaterial));

    ModelData badMeshLink = SampleModel();
    badMeshLink.nodes[1].meshIndices = {2};
    EXPECT_TRUE(rejected(badMeshLink));

    ModelData badChild = SampleModel();
    badChild.nodes[0].children = {3};
    EXPECT_TRUE(rejected(badChild));

    ModelData badRoot = SampleModel();
    badRoot.roots = {7};
    EXPECT_TRUE(rejected(badRoot));

    ModelData notTriangles = SampleModel();
    notTriangles.meshes[1].indices = {0, 1};
    EXPECT_TRUE(rejected(notTriangles));

    EXPECT_FALSE(rejected(SampleModel()));
}

// The model loader reads .amodel files and names the model after the file.
TEST_F(AtelierModelTest, ModelLoaderLoadsAnAmodelFile)
{
    const auto path = WriteBytes("house.amodel", WriteAtelierModel(SampleModel()));
    const auto model = LoadModelFromFile(path);
    ASSERT_TRUE(model.has_value());
    EXPECT_EQ(model->name, "house");
    EXPECT_EQ(model->meshes.size(), 2u);
    EXPECT_EQ(model->materials.size(), 2u);
    EXPECT_EQ(model->nodes.size(), 3u);
}

TEST_F(AtelierModelTest, ModelLoaderRejectsABrokenAmodelFile)
{
    EXPECT_FALSE(LoadModelFromFile(WriteBytes("broken.amodel", {1, 2, 3, 4, 5})).has_value());
    EXPECT_FALSE(LoadModelFromFile(root_ / "missing.amodel").has_value());
}

// Used where only a mesh is wanted, a model is merged into one mesh, each part placed by its node's world transform:
// Base at (10,0,3) and Child above it at (10,5,3).
TEST_F(AtelierModelTest, AnAmodelFileCanBeUsedAsOneMesh)
{
    const auto path = WriteBytes("house.amodel", WriteAtelierModel(SampleModel()));
    const auto mesh = LoadMeshFromFile(path);
    ASSERT_TRUE(mesh.has_value());
    EXPECT_EQ(mesh->indices.size(), 9u);
    EXPECT_EQ(mesh->positions.size(), 21u);
    EXPECT_TRUE(mesh->subMeshes.empty());

    const auto has = [&mesh](const glm::vec3& p)
    {
        for (std::size_t i = 0; i < mesh->positions.size() / 3; ++i)
        {
            if (glm::vec3(mesh->positions[i * 3], mesh->positions[i * 3 + 1], mesh->positions[i * 3 + 2]) == p)
            {
                return true;
            }
        }
        return false;
    };
    EXPECT_TRUE(has({10, 0, 3})); // Base's first corner
    EXPECT_TRUE(has({11, 1, 3})); // Base's third corner
    EXPECT_TRUE(has({10, 5, 3})); // Child's first corner: Group + Base + Child
    EXPECT_TRUE(has({12, 5, 3}));
    ASSERT_EQ(mesh->normals.size(), mesh->positions.size()); // only the quad had normals: the triangle's are zero
}

TEST(AtelierModelFormatTest, FlatteningAModelWithoutNodesKeepsEachMeshWhereItIs)
{
    ModelData model = SampleModel();
    model.nodes.clear();
    model.roots.clear();
    const auto mesh = FlattenModelToMesh(model);
    ASSERT_TRUE(mesh.has_value());
    EXPECT_EQ(mesh->indices.size(), 9u);
    EXPECT_EQ(mesh->positions[0], 0.0f);
    EXPECT_EQ(mesh->indices[6], 4u); // the triangle's indices continue after the quad's four vertices
}

TEST(AtelierModelFormatTest, FlatteningAModelWithoutTrianglesYieldsNothing)
{
    EXPECT_FALSE(FlattenModelToMesh(ModelData{}).has_value());
}
