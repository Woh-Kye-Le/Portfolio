#include "engine/assets/data/ModelData.hpp"
#include "engine/assets/loaders/MeshLoader.hpp"
#include "engine/assets/loaders/ModelLoader.hpp"
#include "engine/graphics/Mesh/Mesh.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using Engine::Assets::LoadModelFromFile;
using Engine::Assets::MeshData;
using Engine::Assets::ModelData;
using Engine::Assets::PlaceholderModel;
using Engine::Assets::SubMesh;

namespace
{
    const std::string kQuadObj = "v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
                                 "vn 0 0 1\n"
                                 "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                                 "f 1/1/1 2/2/1 3/3/1 4/4/1\n";

    // Two triangles, one material each, with the .mtl declaring both slots
    const std::string kTwoMaterialObj = "mtllib quad.mtl\n"
                                        "v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
                                        "usemtl red\n"
                                        "f 1 2 3\n"
                                        "usemtl blue\n"
                                        "f 1 3 4\n";

    const std::string kQuadMtl = "newmtl red\nKd 1 0 0\nmap_Kd red.png\n\nnewmtl blue\nKd 0 0 1\n";

    class ModelLoaderTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            root_ = std::filesystem::temp_directory_path() / "atelier_model_tests" / info->name();
            std::filesystem::remove_all(root_);
            std::filesystem::create_directories(root_);
        }

        void TearDown() override { std::filesystem::remove_all(root_); }

        std::filesystem::path WriteFile(const std::string& name, const std::string& text) const
        {
            const auto path = root_ / name;
            std::filesystem::create_directories(path.parent_path());
            std::ofstream(path) << text;
            return path;
        }

        std::filesystem::path root_;
    };
}

TEST(SubMeshTest, DefaultsToTheStartOfTheMeshWithMaterialZero)
{
    const SubMesh sub;
    EXPECT_EQ(sub.baseIndex, 0u);
    EXPECT_EQ(sub.indexCount, 0u);
    EXPECT_EQ(sub.baseVertex, 0);
    EXPECT_EQ(sub.materialIndex, 0u);
}

TEST(MeshDataTest, StartsWithNoSubMeshesAndNoGpuCopy)
{
    const MeshData mesh;
    EXPECT_TRUE(mesh.subMeshes.empty());
    EXPECT_EQ(mesh.rendererCacheId, 0u);
    EXPECT_TRUE(mesh.positions.empty());
    EXPECT_TRUE(mesh.indices.empty());
}

TEST_F(ModelLoaderTest, LoadsObjIntoASingleMeshModel)
{
    const auto path = WriteFile("quad.obj", kQuadObj);

    const auto model = LoadModelFromFile(path);
    ASSERT_TRUE(model.has_value());
    ASSERT_EQ(model->meshes.size(), 1u);
    EXPECT_EQ(model->name, "quad");
    EXPECT_EQ(model->meshes[0].positions.size(), 12u);
    EXPECT_EQ(model->meshes[0].uvs.size(), 8u);
    EXPECT_EQ(model->meshes[0].normals.size(), 12u);
    EXPECT_EQ(model->meshes[0].indices.size(), 6u);
}

TEST_F(ModelLoaderTest, LoadedObjCarriesOneMaterialSlotAndNoHierarchy)
{
    const auto path = WriteFile("quad.obj", kQuadObj);

    const auto model = LoadModelFromFile(path);
    ASSERT_TRUE(model.has_value());
    ASSERT_EQ(model->materials.size(), 1u);
    EXPECT_EQ(model->materials[0].name, "default"); // no usemtl: the single implicit slot
    EXPECT_EQ(model->materials[0].baseColorFactor, glm::vec4(1.0f));
    EXPECT_EQ(model->materials[0].roughness, 0.5f);
    EXPECT_TRUE(model->materials[0].baseColorTexture.empty());
    EXPECT_TRUE(model->nodes.empty());
    EXPECT_TRUE(model->roots.empty());
}

// The OBJ+MTL path: the mtllib slots land in the model in file order, and every usemtl group becomes its own
// submesh run pointing at the slot whose name it used.
TEST_F(ModelLoaderTest, ObjWithMtlSplitsFacesIntoOneRunPerMaterial)
{
    WriteFile("quad.mtl", kQuadMtl);
    const auto path = WriteFile("quad.obj", kTwoMaterialObj);

    const auto model = LoadModelFromFile(path);
    ASSERT_TRUE(model.has_value());
    ASSERT_EQ(model->materials.size(), 2u);
    EXPECT_EQ(model->materials[0].name, "red");
    EXPECT_EQ(model->materials[0].baseColorFactor, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_EQ(model->materials[0].baseColorTexture, "red.png");
    EXPECT_EQ(model->materials[1].name, "blue");
    EXPECT_EQ(model->materials[1].baseColorFactor, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    EXPECT_TRUE(model->materials[1].baseColorTexture.empty());

    ASSERT_EQ(model->meshes.size(), 1u);
    const MeshData& mesh = model->meshes[0];
    ASSERT_EQ(mesh.subMeshes.size(), 2u);
    EXPECT_EQ(mesh.subMeshes[0].baseIndex, 0u);
    EXPECT_EQ(mesh.subMeshes[0].indexCount, 3u);
    EXPECT_EQ(mesh.subMeshes[0].materialIndex, 0u);
    EXPECT_EQ(mesh.subMeshes[1].baseIndex, 3u);
    EXPECT_EQ(mesh.subMeshes[1].indexCount, 3u);
    EXPECT_EQ(mesh.subMeshes[1].materialIndex, 1u);
    EXPECT_EQ(mesh.indices.size(), 6u);
}

// A missing .mtl only costs the factors it would have declared: the group names still become slots, so the model
// renders as two runs instead of losing half its faces.
TEST_F(ModelLoaderTest, ObjWithAMissingMtlStillKeepsOneSlotPerGroup)
{
    const auto path = WriteFile("quad.obj", kTwoMaterialObj);

    const auto model = LoadModelFromFile(path);
    ASSERT_TRUE(model.has_value());
    ASSERT_EQ(model->materials.size(), 2u);
    EXPECT_EQ(model->materials[0].name, "red");
    EXPECT_EQ(model->materials[1].name, "blue");
    EXPECT_EQ(model->materials[0].baseColorFactor, glm::vec4(1.0f));
    ASSERT_EQ(model->meshes[0].subMeshes.size(), 2u);
    EXPECT_EQ(model->meshes[0].subMeshes[0].materialIndex, 0u);
    EXPECT_EQ(model->meshes[0].subMeshes[1].materialIndex, 1u);
}

// Faces with no usemtl fall back to their g/o group, so a grouped OBJ without any MTL still splits per group.
TEST_F(ModelLoaderTest, ObjWithoutMtlGroupsFacesByTheirGroupName)
{
    const auto path = WriteFile("quad.obj", "v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
                                             "g body\n"
                                             "f 1 2 3\n"
                                             "g lid\n"
                                             "f 1 3 4\n");

    const auto model = LoadModelFromFile(path);
    ASSERT_TRUE(model.has_value());
    ASSERT_EQ(model->materials.size(), 2u);
    EXPECT_EQ(model->materials[0].name, "body");
    EXPECT_EQ(model->materials[1].name, "lid");
    ASSERT_EQ(model->meshes[0].subMeshes.size(), 2u);
    EXPECT_EQ(model->meshes[0].subMeshes[0].indexCount, 3u);
    EXPECT_EQ(model->meshes[0].subMeshes[1].materialIndex, 1u);
}

// Polygons with more than three corners are fanned into triangles: a quad gives two, a pentagon three.
TEST_F(ModelLoaderTest, ObjPolygonsAreFanTriangulated)
{
    const auto path = WriteFile("poly.obj", "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 0.5 2 0\n"
                                             "f 1 2 3 4\n"
                                             "f 1 2 3 4 5\n");

    const auto model = LoadModelFromFile(path);
    ASSERT_TRUE(model.has_value());
    EXPECT_EQ(model->meshes[0].indices.size(), 6u + 9u);
}

// A negative index counts back from the latest vertex, texture coordinate or normal read so far.
TEST_F(ModelLoaderTest, ObjNegativeIndicesCountBackFromTheEnd)
{
    const auto path = WriteFile("neg.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                                            "vt 0 0\nvt 1 0\nvt 0 1\n"
                                            "f -3/-3 -2/-2 -1/-1\n");

    const auto model = LoadModelFromFile(path);
    ASSERT_TRUE(model.has_value());
    const MeshData& mesh = model->meshes[0];
    EXPECT_EQ(mesh.positions[3], 1.0f); // second corner is the second vertex
    EXPECT_EQ(mesh.uvs[2], 1.0f);       // third corner is the third coordinate: (0, 1)
}

// When only some faces name a normal, the mesh still holds exactly one normal per vertex (a zero one where nothing was
// authored, which the uploader derives), or none at all when no face names one.
TEST_F(ModelLoaderTest, ObjKeepsOneNormalPerVertexEvenWhenOnlySomeFacesHaveNormals)
{
    const auto mixed = LoadModelFromFile(WriteFile("mixed.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\n"
                                                                 "f 1 2 3\n"
                                                                 "f 1//1 2//1 3//1\n"));
    ASSERT_TRUE(mixed.has_value());
    const MeshData& mesh = mixed->meshes[0];
    ASSERT_EQ(mesh.normals.size(), mesh.positions.size());
    EXPECT_EQ(mesh.normals[2], 0.0f); // first face: nothing authored
    EXPECT_EQ(mesh.normals[9 + 2], 1.0f); // second face: the authored (0, 0, 1)

    const auto none = LoadModelFromFile(WriteFile("none.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));
    ASSERT_TRUE(none.has_value());
    EXPECT_TRUE(none->meshes[0].normals.empty());
}

// Windows line endings in the OBJ and in the MTL leave no stray characters in names or paths.
TEST_F(ModelLoaderTest, ObjAndMtlWithWindowsLineEndings)
{
    WriteFile("crlf.mtl", "newmtl red\r\nKd 1 0 0\r\nmap_Kd red.png\r\n");
    const auto path = WriteFile("crlf.obj", "mtllib crlf.mtl\r\nv 0 0 0\r\nv 1 0 0\r\nv 0 1 0\r\nusemtl red\r\nf 1 2 3\r\n");

    const auto model = LoadModelFromFile(path);
    ASSERT_TRUE(model.has_value());
    ASSERT_EQ(model->materials.size(), 1u);
    EXPECT_EQ(model->materials[0].name, "red");
    EXPECT_EQ(model->materials[0].baseColorTexture, "red.png");
    EXPECT_EQ(model->materials[0].baseColorFactor, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
}

// Several libraries can be listed on one mtllib line, and a model can come back to an earlier material: that makes a
// third run but not a third slot.
TEST_F(ModelLoaderTest, ObjWithTwoLibrariesAndAMaterialUsedTwice)
{
    WriteFile("a.mtl", "newmtl A\nKd 1 0 0\n");
    WriteFile("b.mtl", "newmtl B\nKd 0 1 0\n");
    const auto path = WriteFile("two.obj", "mtllib a.mtl b.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\n"
                                            "usemtl A\nf 1 2 3\nusemtl B\nf 1 2 3\nusemtl A\nf 1 2 3\n");

    const auto model = LoadModelFromFile(path);
    ASSERT_TRUE(model.has_value());
    EXPECT_EQ(model->materials.size(), 2u);
    ASSERT_EQ(model->meshes[0].subMeshes.size(), 3u);
    EXPECT_EQ(model->meshes[0].subMeshes[0].materialIndex, 0u);
    EXPECT_EQ(model->meshes[0].subMeshes[1].materialIndex, 1u);
    EXPECT_EQ(model->meshes[0].subMeshes[2].materialIndex, 0u);
}

// A map_* statement may carry options before the file name (-s 1 1 1, -bm 0.5, -clamp on) and the name may hold
// spaces; none of that leaks into the path.
TEST_F(ModelLoaderTest, MtlTextureNamesMayHaveOptionsAndSpaces)
{
    WriteFile("opts.mtl", "newmtl A\nmap_Kd -s 1 1 1 -o 0 0 0 my wood.png\nmap_Bump -bm 0.5 nrm.png\n"
                          "newmtl B\nmap_Kd -clamp on tex.png\nnewmtl C\nmap_Kd 2wood.png\n");
    const auto path = WriteFile("opts.obj", "mtllib opts.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\n"
                                             "usemtl A\nf 1 2 3\nusemtl B\nf 1 2 3\nusemtl C\nf 1 2 3\n");

    const auto model = LoadModelFromFile(path);
    ASSERT_TRUE(model.has_value());
    EXPECT_EQ(model->materials[0].baseColorTexture, "my wood.png");
    EXPECT_EQ(model->materials[0].normalTexture, "nrm.png");
    EXPECT_EQ(model->materials[1].baseColorTexture, "tex.png");
    EXPECT_EQ(model->materials[2].baseColorTexture, "2wood.png"); // a name starting with a digit is still a name
}

// d and Tr (its inverse) set the opacity; a non-zero Ke makes the material emissive.
TEST_F(ModelLoaderTest, MtlOpacityAndEmissive)
{
    WriteFile("alpha.mtl", "newmtl A\nKd 0.2 0.4 0.6\nd 0.5\nKe 1 1 1\nnewmtl B\nTr 0.25\nKd 1 1 1\n");
    const auto path = WriteFile("alpha.obj", "mtllib alpha.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\n"
                                              "usemtl A\nf 1 2 3\nusemtl B\nf 1 2 3\n");

    const auto model = LoadModelFromFile(path);
    ASSERT_TRUE(model.has_value());
    EXPECT_FLOAT_EQ(model->materials[0].baseColorFactor.g, 0.4f);
    EXPECT_FLOAT_EQ(model->materials[0].baseColorFactor.a, 0.5f);
    EXPECT_EQ(model->materials[0].emissiveStrength, 1.0f);
    EXPECT_FLOAT_EQ(model->materials[1].baseColorFactor.a, 0.75f);
    EXPECT_EQ(model->materials[1].emissiveStrength, 0.0f);
}

// A statement ending in a backslash continues on the next line.
TEST_F(ModelLoaderTest, ObjLineContinuation)
{
    const auto path = WriteFile("cont.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 \\\n 3\n");

    const auto model = LoadModelFromFile(path);
    ASSERT_TRUE(model.has_value());
    EXPECT_EQ(model->meshes[0].indices.size(), 3u);
}

// A face that names a vertex the file never declared makes the whole file unusable, not half a model.
TEST_F(ModelLoaderTest, ObjFaceWithAnUnknownVertexIsRejected)
{
    EXPECT_FALSE(LoadModelFromFile(WriteFile("bad.obj", "v 0 0 0\nv 1 0 0\nf 1 2 3\n")).has_value());
}

// Normals the file authored reach the uploaded mesh as they are (so a smooth sphere stays smooth); a vertex without
// one still gets a normal derived from its face.
TEST(MeshFromAssetDataTest, UsesAuthoredNormalsAndDerivesTheMissingOnes)
{
    MeshData data;
    data.positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};   // a triangle facing +Z
    data.normals = {0, 1, 0, 0, 0, 0, 0, 0, 0};     // only the first vertex has one, pointing up
    data.indices = {0, 1, 2};

    const auto mesh = Engine::Graphics::Mesh::FromAssetData(data);
    ASSERT_EQ(mesh.GetVertices().size(), 3u);
    EXPECT_EQ(mesh.GetVertices()[0].normal, glm::vec3(0.0f, 1.0f, 0.0f)); // authored, not the face's +Z
    EXPECT_EQ(mesh.GetVertices()[1].normal, glm::vec3(0.0f, 0.0f, 1.0f)); // derived
    EXPECT_EQ(mesh.GetVertices()[2].normal, glm::vec3(0.0f, 0.0f, 1.0f));

    data.normals.clear(); // nothing authored: all derived
    const auto derived = Engine::Graphics::Mesh::FromAssetData(data);
    EXPECT_EQ(derived.GetVertices()[0].normal, glm::vec3(0.0f, 0.0f, 1.0f));
}

TEST_F(ModelLoaderTest, UnknownExtensionYieldsNoModel)
{
    const auto path = WriteFile("thing.xyz", "not a model");
    EXPECT_FALSE(LoadModelFromFile(path).has_value());
}

TEST_F(ModelLoaderTest, MissingFileYieldsNoModel)
{
    EXPECT_FALSE(LoadModelFromFile(root_ / "missing.obj").has_value());
    EXPECT_FALSE(LoadModelFromFile(root_ / "missing.fbx").has_value());
}

TEST_F(ModelLoaderTest, UnsupportedFormatLoadsAsNothingYet)
{
    const auto path = WriteFile("model.gltf", "{}");
    EXPECT_FALSE(LoadModelFromFile(path).has_value());
}

TEST(PlaceholderModelTest, IsOneTexturedCubeUnderOneMaterial)
{
    const ModelData model = PlaceholderModel();

    ASSERT_EQ(model.meshes.size(), 1u);
    ASSERT_EQ(model.materials.size(), 1u);
    EXPECT_EQ(model.meshes[0].name, "placeholder_cube");
    EXPECT_EQ(model.meshes[0].positions.size(), 24u * 3u);
    EXPECT_EQ(model.meshes[0].indices.size(), 36u);
    EXPECT_TRUE(model.meshes[0].subMeshes.empty());
    EXPECT_TRUE(model.nodes.empty());
}

TEST(ModelDataTest, SubMeshesSplitOneMeshIntoMaterialRuns)
{
    ModelData model;
    model.name = "two_materials";
    model.materials.resize(2);
    model.materials[0].name = "red";
    model.materials[1].name = "blue";

    MeshData mesh;
    mesh.positions = {0.0f, 0.0f, 0.0f};
    mesh.indices = {0, 0, 0, 0, 0, 0};
    mesh.subMeshes.push_back(SubMesh{0, 3, 0, 0});
    mesh.subMeshes.push_back(SubMesh{3, 3, 0, 1});
    model.meshes.push_back(mesh);

    ASSERT_EQ(model.meshes[0].subMeshes.size(), 2u);
    EXPECT_EQ(model.meshes[0].subMeshes[0].materialIndex, 0u);
    EXPECT_EQ(model.meshes[0].subMeshes[1].materialIndex, 1u);
    EXPECT_EQ(model.meshes[0].subMeshes[1].baseIndex, 3u);
}
