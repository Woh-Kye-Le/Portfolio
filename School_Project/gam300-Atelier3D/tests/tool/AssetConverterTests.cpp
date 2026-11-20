/* !
@file               AssetConverterTests.cpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Tests of the asset tool's converter: models written in OBJ, glTF and FBX
    become .amodel files that the engine's own reader accepts, with their
    materials, hierarchy and textures, and bad input fails with a reason
    instead of writing a broken file. Model packs (a folder or .zip with the
    model, its .mtl and its textures) are converted in one go.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include <gtest/gtest.h>

#include "engine/assets/loaders/AtelierModel.hpp"
#include "tool/AssetConverter.hpp"
#include "tool/ZipReader.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using Atelier::Tool::ConvertModelFile;
    using Atelier::Tool::ConvertOptions;
    using Atelier::Tool::ConvertResult;
    using Engine::Assets::ModelData;

    // Each test works in its own folder under the system temp directory, removed afterwards
    class AssetConverterTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            static std::atomic<int> counter{0};
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            root_ = fs::temp_directory_path() /
                    ("atelier_tool_" + std::string(info->name()) + "_" + std::to_string(++counter));
            fs::remove_all(root_);
            fs::create_directories(root_ / "in");
        }

        void TearDown() override
        {
            std::error_code ec;
            fs::remove_all(root_, ec);
        }

        void Write(const fs::path& relative, const std::string& text)
        {
            const fs::path path = root_ / relative;
            fs::create_directories(path.parent_path());
            std::ofstream(path, std::ios::binary) << text;
        }

        ConvertOptions Options(const std::string& input, bool separateTextures = true) const
        {
            ConvertOptions options;
            options.input = root_ / "in" / input;
            options.output = root_ / "out" / "models" / (fs::path(input).stem().string() + ".amodel");
            if (separateTextures)
            {
                options.texturesDir = root_ / "out" / "textures";
            }
            return options;
        }

        static std::optional<ModelData> Read(const fs::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                                  std::istreambuf_iterator<char>());
            return Engine::Assets::ParseAtelierModel(bytes);
        }

        static bool HasWarning(const ConvertResult& result, const std::string& text)
        {
            return std::any_of(result.warnings.begin(), result.warnings.end(),
                               [&](const std::string& w) { return w.find(text) != std::string::npos; });
        }

        fs::path root_;
    };

    // A quad (two triangles once triangulated) with a material, and a triangle with another
    const char* kTwoMaterialObj = "mtllib two.mtl\n"
                                  "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 2 0 0\nv 3 0 0\nv 2 1 0\n"
                                  "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                                  "vn 0 0 1\n"
                                  "o panel\nusemtl wood\nf 1/1/1 2/2/1 3/3/1 4/4/1\n"
                                  "o flag\nusemtl red\nf 5/1/1 6/2/1 7/3/1\n";
    const char* kTwoMaterialMtl = "newmtl wood\nKd 1 1 1\nmap_Kd wood.png\n"
                                  "newmtl red\nKd 0.8 0.1 0.1\nd 0.5\n";

    const char* kRedQuadFbx = R"(; FBX 7.4.0 project file
FBXHeaderExtension:  {
	FBXHeaderVersion: 1003
	FBXVersion: 7400
	Creator: "atelier tool test"
}
GlobalSettings:  {
	Version: 1000
	Properties70:  {
		P: "UpAxis", "int", "Integer", "",1
		P: "UpAxisSign", "int", "Integer", "",1
		P: "FrontAxis", "int", "Integer", "",2
		P: "FrontAxisSign", "int", "Integer", "",1
		P: "CoordAxis", "int", "Integer", "",0
		P: "CoordAxisSign", "int", "Integer", "",1
		P: "UnitScaleFactor", "double", "Number", "",1
	}
}
Objects:  {
	Geometry: 100, "Geometry::Quad", "Mesh" {
		Vertices: *12 {
			a: 0,0,0,1,0,0,1,1,0,0,1,0
		}
		PolygonVertexIndex: *4 {
			a: 0,1,2,-4
		}
		GeometryVersion: 124
		LayerElementUV: 0 {
			Version: 101
			Name: "UVMap"
			MappingInformationType: "ByPolygonVertex"
			ReferenceInformationType: "IndexToDirect"
			UV: *8 {
				a: 0,0,1,0,1,1,0,1
			}
			UVIndex: *4 {
				a: 0,1,2,3
			}
		}
		LayerElementMaterial: 0 {
			Version: 101
			Name: ""
			MappingInformationType: "AllSame"
			ReferenceInformationType: "IndexToDirect"
			Materials: *1 {
				a: 0
			}
		}
		Layer: 0 {
			Version: 100
			LayerElement:  {
				Type: "LayerElementUV"
				TypedIndex: 0
			}
			LayerElement:  {
				Type: "LayerElementMaterial"
				TypedIndex: 0
			}
		}
	}
	Model: 200, "Model::Quad", "Mesh" {
		Version: 232
		Properties70:  {
			P: "Lcl Translation", "Lcl Translation", "", "A",2,3,4
		}
	}
	Material: 300, "Material::Red", "" {
		Version: 102
		ShadingModel: "phong"
		Properties70:  {
			P: "DiffuseColor", "Color", "", "A",1,0,0
		}
	}
	Texture: 400, "Texture::skin", "" {
		Type: "TextureVideoClip"
		FileName: "skin.png"
		RelativeFilename: "skin.png"
	}
}
Connections:  {
	C: "OO",200,0
	C: "OO",100,200
	C: "OO",300,200
	C: "OP",400,300,"DiffuseColor"
}
)";

    std::string Base64(const std::vector<std::uint8_t>& bytes)
    {
        static const char* kDigits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        for (std::size_t i = 0; i < bytes.size(); i += 3)
        {
            const std::uint32_t chunk = (bytes[i] << 16) | (i + 1 < bytes.size() ? bytes[i + 1] << 8 : 0) |
                                        (i + 2 < bytes.size() ? bytes[i + 2] : 0);
            out += kDigits[(chunk >> 18) & 63];
            out += kDigits[(chunk >> 12) & 63];
            out += i + 1 < bytes.size() ? kDigits[(chunk >> 6) & 63] : '=';
            out += i + 2 < bytes.size() ? kDigits[chunk & 63] : '=';
        }
        return out;
    }

    // The triangle's vertex and index bytes
    std::vector<std::uint8_t> TriangleBuffer()
    {
        std::vector<std::uint8_t> buffer;
        const float positions[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
        for (float f : positions)
        {
            const auto* p = reinterpret_cast<const std::uint8_t*>(&f);
            buffer.insert(buffer.end(), p, p + sizeof f);
        }
        buffer.insert(buffer.end(), {0, 0, 1, 0, 2, 0, 0, 0}); // indices 0 1 2 as uint16, padded to 4 bytes
        return buffer;
    }

    // A glTF with one triangle, one material (colour, metallic, roughness) and its colour image embedded as a data URI.
    // For a .glb the buffer has no URI: it is the file's binary chunk.
    std::string TriangleGltf(const std::string& imageUri, bool glb = false)
    {
        return R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],)"
               R"("nodes":[{"name":"tri","mesh":0,"translation":[1,2,3]}],)"
               R"("meshes":[{"primitives":[{"attributes":{"POSITION":1},"indices":0,"material":0}]}],)"
               R"("materials":[{"name":"metal","pbrMetallicRoughness":{"baseColorFactor":[0.2,0.4,0.6,1],)"
               R"("metallicFactor":0.75,"roughnessFactor":0.25,"baseColorTexture":{"index":0}}}],)"
               R"("textures":[{"source":0}],"images":[{"uri":")" +
               imageUri + R"("}],)" +
               (glb ? std::string(R"("buffers":[{"byteLength":44}],)")
                    : R"("buffers":[{"byteLength":44,"uri":"data:application/octet-stream;base64,)" +
                          Base64(TriangleBuffer()) + R"("}],)") +
               R"("bufferViews":[{"buffer":0,"byteOffset":36,"byteLength":6,"target":34963},)"
               R"({"buffer":0,"byteOffset":0,"byteLength":36,"target":34962}],)"
               R"("accessors":[{"bufferView":0,"componentType":5123,"count":3,"type":"SCALAR"},)"
               R"({"bufferView":1,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]}]})";
    }

    // A binary glTF: the header, the JSON chunk and the BIN chunk (each padded to 4 bytes)
    std::string Glb(std::string json, const std::vector<std::uint8_t>& bytes)
    {
        while (json.size() % 4 != 0)
        {
            json += ' ';
        }
        std::string bin(bytes.begin(), bytes.end());
        while (bin.size() % 4 != 0)
        {
            bin += '\0';
        }
        const auto u32 = [](std::uint32_t v)
        {
            return std::string{static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF),
                               static_cast<char>((v >> 16) & 0xFF), static_cast<char>(v >> 24)};
        };
        const auto total = static_cast<std::uint32_t>(12 + 8 + json.size() + 8 + bin.size());
        return "glTF" + u32(2) + u32(total) + u32(static_cast<std::uint32_t>(json.size())) + "JSON" + json +
               u32(static_cast<std::uint32_t>(bin.size())) + std::string("BIN\0", 4) + bin;
    }

    // Translation of the node that holds the mesh, summed along the path from the root (axes are not rotated here)
    glm::vec3 MeshWorldTranslation(const ModelData& model)
    {
        glm::vec3 sum(0.0f);
        std::vector<std::uint32_t> path;
        std::vector<std::uint32_t> stack{model.roots.at(0)};
        std::vector<std::vector<std::uint32_t>> paths{{model.roots.at(0)}};
        while (!stack.empty())
        {
            const std::uint32_t node = stack.back();
            stack.pop_back();
            const auto here = paths.back();
            paths.pop_back();
            if (!model.nodes[node].meshIndices.empty())
            {
                path = here;
                break;
            }
            for (std::uint32_t child : model.nodes[node].children)
            {
                stack.push_back(child);
                auto next = here;
                next.push_back(child);
                paths.push_back(next);
            }
        }
        for (std::uint32_t node : path)
        {
            sum += glm::vec3(model.nodes[node].localTransform[3]);
        }
        return sum;
    }
}

TEST_F(AssetConverterTest, ObjBecomesOneMeshPerMaterialAndKeepsItsTextureBesideTheModels)
{
    Write("in/two.obj", kTwoMaterialObj);
    Write("in/two.mtl", kTwoMaterialMtl);
    Write("in/wood.png", "not really a png, only copied");

    const ConvertResult result = ConvertModelFile(Options("two.obj"));
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(result.warnings.empty());
    EXPECT_EQ(result.meshes, 2u);
    EXPECT_EQ(result.materials, 2u); // assimp's extra "DefaultMaterial" is not kept
    ASSERT_EQ(result.textures.size(), 1u);

    const auto model = Read(root_ / "out" / "models" / "two.amodel");
    ASSERT_TRUE(model.has_value());
    EXPECT_EQ(model->name, "two");
    ASSERT_EQ(model->meshes.size(), 2u);
    ASSERT_EQ(model->materials.size(), 2u);

    std::size_t wood = model->materials[0].name == "wood" ? 0 : 1;
    const auto& woodMaterial = model->materials[wood];
    const auto& redMaterial = model->materials[1 - wood];
    EXPECT_EQ(woodMaterial.name, "wood");
    EXPECT_EQ(woodMaterial.baseColorTexture, "../textures/wood.png");
    EXPECT_TRUE(fs::exists(root_ / "out" / "textures" / "wood.png"));
    EXPECT_EQ(redMaterial.name, "red");
    EXPECT_TRUE(redMaterial.baseColorTexture.empty());
    EXPECT_FLOAT_EQ(redMaterial.baseColorFactor.r, 0.8f);
    EXPECT_FLOAT_EQ(redMaterial.baseColorFactor.a, 0.5f);

    // The quad was triangulated, the flag is one triangle; every vertex has UVs and normals
    std::size_t triangles = 0;
    for (const auto& mesh : model->meshes)
    {
        triangles += mesh.indices.size() / 3;
        EXPECT_EQ(mesh.uvs.size() / 2, mesh.positions.size() / 3);
        EXPECT_EQ(mesh.normals.size() / 3, mesh.positions.size() / 3);
    }
    EXPECT_EQ(triangles, 3u);
}

TEST_F(AssetConverterTest, TexturesGoBesideTheModelWhenNoTexturesFolderIsGiven)
{
    Write("in/two.obj", kTwoMaterialObj);
    Write("in/two.mtl", kTwoMaterialMtl);
    Write("in/wood.png", "png");

    const ConvertResult result = ConvertModelFile(Options("two.obj", false));
    ASSERT_TRUE(result.ok) << result.error;
    const auto model = Read(root_ / "out" / "models" / "two.amodel");
    ASSERT_TRUE(model.has_value());
    const bool found = std::any_of(model->materials.begin(), model->materials.end(),
                                   [](const auto& m) { return m.baseColorTexture == "wood.png"; });
    EXPECT_TRUE(found);
    EXPECT_TRUE(fs::exists(root_ / "out" / "models" / "wood.png"));
}

TEST_F(AssetConverterTest, TextureFoundInATexturesFolderBesideTheModelByItsBareName)
{
    Write("in/two.obj", kTwoMaterialObj);
    Write("in/two.mtl", "newmtl wood\nmap_Kd C:\\Users\\someone\\Desktop\\art\\wood.png\nnewmtl red\nKd 1 0 0\n");
    Write("in/textures/wood.png", "png");

    const ConvertResult result = ConvertModelFile(Options("two.obj"));
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(result.warnings.empty());
    EXPECT_TRUE(fs::exists(root_ / "out" / "textures" / "wood.png"));
}

TEST_F(AssetConverterTest, MissingTextureIsAWarningAndTheModelStillConverts)
{
    Write("in/two.obj", kTwoMaterialObj);
    Write("in/two.mtl", kTwoMaterialMtl); // wood.png is not there

    const ConvertResult result = ConvertModelFile(Options("two.obj"));
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(HasWarning(result, "wood.png"));
    EXPECT_TRUE(result.textures.empty());
}

TEST_F(AssetConverterTest, TwoDifferentTexturesWithTheSameNameBothSurvive)
{
    Write("in/two.obj", kTwoMaterialObj);
    Write("in/two.mtl", "newmtl wood\nmap_Kd a/skin.png\nnewmtl red\nmap_Kd b/skin.png\n");
    Write("in/a/skin.png", "first image");
    Write("in/b/skin.png", "second image");

    const ConvertResult result = ConvertModelFile(Options("two.obj"));
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.textures.size(), 2u);
    const auto model = Read(root_ / "out" / "models" / "two.amodel");
    ASSERT_TRUE(model.has_value());
    ASSERT_EQ(model->materials.size(), 2u);
    EXPECT_NE(model->materials[0].baseColorTexture, model->materials[1].baseColorTexture);
}

TEST_F(AssetConverterTest, ConvertingTwiceReusesTheIdenticalTextureInsteadOfCopyingItAgain)
{
    Write("in/two.obj", kTwoMaterialObj);
    Write("in/two.mtl", kTwoMaterialMtl);
    Write("in/wood.png", "png");

    ASSERT_TRUE(ConvertModelFile(Options("two.obj")).ok);
    const ConvertResult second = ConvertModelFile(Options("two.obj"));
    ASSERT_TRUE(second.ok) << second.error;
    EXPECT_TRUE(second.textures.empty()); // already there, identical
    std::size_t files = 0;
    for (const auto& entry : fs::directory_iterator(root_ / "out" / "textures"))
    {
        files += entry.is_regular_file() ? 1 : 0;
    }
    EXPECT_EQ(files, 1u);
}

TEST_F(AssetConverterTest, ScaleMultipliesTheWholeModel)
{
    Write("in/two.obj", kTwoMaterialObj);
    Write("in/two.mtl", kTwoMaterialMtl);
    ConvertOptions options = Options("two.obj");
    options.scale = 0.01f;

    ASSERT_TRUE(ConvertModelFile(options).ok);
    const auto model = Read(options.output);
    ASSERT_TRUE(model.has_value());
    const glm::mat4& root = model->nodes[model->roots.at(0)].localTransform;
    EXPECT_FLOAT_EQ(root[0][0], 0.01f);
    EXPECT_FLOAT_EQ(root[1][1], 0.01f);
    EXPECT_FLOAT_EQ(root[2][2], 0.01f);
}

// Bounds of a converted model as its nodes place it
static void WorldBounds(const ModelData& model, glm::vec3& low, glm::vec3& high)
{
    low = glm::vec3(1e30f);
    high = glm::vec3(-1e30f);
    std::vector<glm::mat4> world(model.nodes.size(), glm::mat4(1.0f));
    std::vector<int> parent(model.nodes.size(), -1);
    for (std::size_t n = 0; n < model.nodes.size(); ++n)
    {
        for (const std::uint32_t child : model.nodes[n].children)
        {
            parent[child] = static_cast<int>(n);
        }
    }
    for (std::size_t n = 0; n < model.nodes.size(); ++n)
    {
        world[n] = (parent[n] < 0 ? glm::mat4(1.0f) : world[static_cast<std::size_t>(parent[n])]) *
                   model.nodes[n].localTransform;
        for (const std::uint32_t meshIndex : model.nodes[n].meshIndices)
        {
            const auto& positions = model.meshes[meshIndex].positions;
            for (std::size_t i = 0; i + 2 < positions.size(); i += 3)
            {
                const glm::vec3 p(world[n] * glm::vec4(positions[i], positions[i + 1], positions[i + 2], 1.0f));
                low = glm::min(low, p);
                high = glm::max(high, p);
            }
        }
    }
}

TEST_F(AssetConverterTest, FitAndCenterBringAFarAwayHugeModelToTheOriginAtTheRequestedSize)
{
    // A 400 x 100 x 0 slab, 5000 units from the origin
    Write("in/slab.obj", "v 4800 100 200\nv 5200 100 200\nv 5200 200 200\nv 4800 200 200\nf 1 2 3 4\n");
    ConvertOptions options = Options("slab.obj");
    options.center = true;
    options.fitSize = 2.0f;

    ASSERT_TRUE(ConvertModelFile(options).ok);
    const auto model = Read(options.output);
    ASSERT_TRUE(model.has_value());
    glm::vec3 low;
    glm::vec3 high;
    WorldBounds(*model, low, high);
    EXPECT_NEAR(high.x - low.x, 2.0f, 1e-4f); // the longest side
    EXPECT_NEAR(high.y - low.y, 0.5f, 1e-4f); // the others keep their proportion
    EXPECT_NEAR((low.x + high.x) / 2.0f, 0.0f, 1e-4f);
    EXPECT_NEAR((low.y + high.y) / 2.0f, 0.0f, 1e-4f);
    EXPECT_NEAR((low.z + high.z) / 2.0f, 0.0f, 1e-4f);
}

TEST_F(AssetConverterTest, WithoutFitOrCenterTheModelKeepsItsOwnSizeAndPlace)
{
    Write("in/slab.obj", "v 4800 100 200\nv 5200 100 200\nv 5200 200 200\nv 4800 200 200\nf 1 2 3 4\n");
    ConvertOptions options = Options("slab.obj");

    ASSERT_TRUE(ConvertModelFile(options).ok);
    const auto model = Read(options.output);
    ASSERT_TRUE(model.has_value());
    glm::vec3 low;
    glm::vec3 high;
    WorldBounds(*model, low, high);
    EXPECT_NEAR(low.x, 4800.0f, 1e-3f);
    EXPECT_NEAR(high.x, 5200.0f, 1e-3f);
}

TEST_F(AssetConverterTest, GltfKeepsFactorsHierarchyAndWritesItsEmbeddedImage)
{
    // A 1x1 PNG, as the data URI an exporter would embed
    const std::vector<std::uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0,    0,    0,
                                           0x0D, 'I', 'H', 'D', 'R',  0,    0,    0,    1,    0,    0,
                                           0,    1,   8,   6,   0,    0,    0,    0x1F, 0x15, 0xC4, 0x89};
    Write("in/tri.gltf", TriangleGltf("data:image/png;base64," + Base64(png)));

    const ConvertResult result = ConvertModelFile(Options("tri.gltf"));
    ASSERT_TRUE(result.ok) << result.error;
    ASSERT_EQ(result.meshes, 1u);
    ASSERT_EQ(result.materials, 1u);
    ASSERT_EQ(result.textures.size(), 1u);
    EXPECT_EQ(result.textures[0].extension().string(), ".png");

    const auto model = Read(root_ / "out" / "models" / "tri.amodel");
    ASSERT_TRUE(model.has_value());
    const auto& material = model->materials[0];
    EXPECT_EQ(material.name, "metal");
    EXPECT_FLOAT_EQ(material.baseColorFactor.g, 0.4f);
    EXPECT_FLOAT_EQ(material.metallic, 0.75f);
    EXPECT_FLOAT_EQ(material.roughness, 0.25f);
    EXPECT_EQ(material.baseColorTexture, "../textures/" + result.textures[0].filename().string());
    EXPECT_TRUE(fs::exists(result.textures[0]));
    EXPECT_EQ(fs::file_size(result.textures[0]), png.size());

    const glm::vec3 at = MeshWorldTranslation(*model);
    EXPECT_FLOAT_EQ(at.x, -1.0f); // glTF models are turned round to face the engine forward
    EXPECT_FLOAT_EQ(at.y, 2.0f);
    EXPECT_FLOAT_EQ(at.z, -3.0f);
    ASSERT_EQ(model->meshes[0].indices.size(), 3u);
    EXPECT_EQ(model->meshes[0].normals.size(), 9u); // glTF without normals: generated
}

TEST_F(AssetConverterTest, AsciiFbxKeepsMaterialTextureAndNodePlacement)
{
    Write("in/quad.fbx", kRedQuadFbx);
    Write("in/skin.png", "png");

    const ConvertResult result = ConvertModelFile(Options("quad.fbx"));
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.meshes, 1u);

    const auto model = Read(root_ / "out" / "models" / "quad.amodel");
    ASSERT_TRUE(model.has_value());
    ASSERT_EQ(model->materials.size(), 1u);
    EXPECT_EQ(model->materials[0].name, "Red");
    EXPECT_FLOAT_EQ(model->materials[0].baseColorFactor.r, 1.0f);
    EXPECT_FLOAT_EQ(model->materials[0].baseColorFactor.g, 0.0f);
    EXPECT_EQ(model->materials[0].baseColorTexture, "../textures/skin.png");
    EXPECT_TRUE(fs::exists(root_ / "out" / "textures" / "skin.png"));

    EXPECT_EQ(model->meshes[0].indices.size(), 6u); // the quad became two triangles
    EXPECT_EQ(model->meshes[0].uvs.size() / 2, model->meshes[0].positions.size() / 3);
    const glm::vec3 at = MeshWorldTranslation(*model);
    EXPECT_FLOAT_EQ(at.x, 2.0f);
    EXPECT_FLOAT_EQ(at.y, 3.0f);
    EXPECT_FLOAT_EQ(at.z, 4.0f);
}

TEST_F(AssetConverterTest, DamagedTextureCoordinatesAreResetAndReported)
{
    // One corner of the quad has a coordinate in the billions, as some exporters leave behind
    Write("in/bad.obj", "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 34693107712\n"
                        "f 1/1 2/2 3/3 4/4\n");

    const ConvertResult result = ConvertModelFile(Options("bad.obj"));
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(HasWarning(result, "damaged texture coordinates"));
    const auto model = Read(root_ / "out" / "models" / "bad.amodel");
    ASSERT_TRUE(model.has_value());
    for (const float uv : model->meshes[0].uvs)
    {
        EXPECT_LE(std::abs(uv), 1.0f);
    }
}

TEST_F(AssetConverterTest, MissingInputFailsWithAReason)
{
    const ConvertResult result = ConvertModelFile(Options("nothing.obj"));
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("not found"), std::string::npos);
    EXPECT_FALSE(fs::exists(root_ / "out"));
}

TEST_F(AssetConverterTest, OutputThatIsNotAnAmodelIsRefused)
{
    Write("in/two.obj", kTwoMaterialObj);
    ConvertOptions options = Options("two.obj");
    options.output = root_ / "out" / "two.obj";

    const ConvertResult result = ConvertModelFile(options);
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(fs::exists(options.output));
}

TEST_F(AssetConverterTest, GarbageAndEmptyModelsFailWithoutWritingAnything)
{
    Write("in/garbage.fbx", "this is not a model at all");
    Write("in/empty.obj", "# nothing here\nv 0 0 0\nv 1 0 0\n");

    const ConvertResult garbage = ConvertModelFile(Options("garbage.fbx"));
    EXPECT_FALSE(garbage.ok);
    EXPECT_FALSE(garbage.error.empty());
    EXPECT_FALSE(fs::exists(root_ / "out" / "models" / "garbage.amodel"));

    const ConvertResult empty = ConvertModelFile(Options("empty.obj"));
    EXPECT_FALSE(empty.ok);
    EXPECT_FALSE(empty.error.empty());
    EXPECT_FALSE(fs::exists(root_ / "out" / "models" / "empty.amodel"));
}

namespace
{
    // A model pack made by Python's zipfile (deflated, as download sites and Windows make them): "Crate Pack/" holds
    // crate.obj, crate.mtl, textures/crate.png and a crate.max scene
    const char kDeflatedPack[] =
        "\x50\x4b\x03\x04\x14\x00\x00\x00\x08\x00\x87\x0b\x5e\x5d\xb1\xae\x51\xa1\x49\x00\x00\x00\x6b\x00"
        "\x00\x00\x14\x00\x00\x00\x43\x72\x61\x74\x65\x20\x50\x61\x63\x6b\x2f\x63\x72\x61\x74\x65\x2e\x6f"
        "\x62\x6a\xcb\x2d\xc9\xc9\xc9\x4c\x52\x48\x2e\x4a\x2c\x49\xd5\xcb\x2d\xc9\xe1\x2a\x53\x30\x00\x41"
        "\x20\x6d\x08\xa7\x0d\xc1\xb4\x01\x84\x2e\x81\x08\x97\xc0\x78\x40\x69\x88\xa0\x21\x57\x69\x71\x2a"
        "\xd0\x08\x85\xf2\xfc\xfc\x14\xae\x34\x05\x43\x7d\x43\x05\x23\x7d\x23\x05\x63\x7d\x63\x05\x13\x7d"
        "\x13\x2e\x00\x50\x4b\x03\x04\x14\x00\x00\x00\x08\x00\x87\x0b\x5e\x5d\x41\x74\x78\x98\x2d\x00\x00"
        "\x00\x2f\x00\x00\x00\x14\x00\x00\x00\x43\x72\x61\x74\x65\x20\x50\x61\x63\x6b\x2f\x63\x72\x61\x74"
        "\x65\x2e\x6d\x74\x6c\xcb\x4b\x2d\xcf\x2d\xc9\x51\x28\xcf\xcf\x4f\xe1\xf2\x4e\x51\x30\x04\x41\xae"
        "\xdc\xc4\x82\x78\x20\xa7\x24\xb5\xa2\xa4\xb4\x28\xb5\x58\x3f\xb9\x28\xb1\x24\x55\xaf\x20\x2f\x9d"
        "\x0b\x00\x50\x4b\x03\x04\x14\x00\x00\x00\x08\x00\x87\x0b\x5e\x5d\x1c\xc5\x7d\xc8\x22\x00\x00\x00"
        "\x78\x00\x00\x00\x1d\x00\x00\x00\x43\x72\x61\x74\x65\x20\x50\x61\x63\x6b\x2f\x74\x65\x78\x74\x75"
        "\x72\x65\x73\x2f\x63\x72\x61\x74\x65\x2e\x70\x6e\x67\xcb\xcb\x2f\x51\x28\x4a\x4d\xcc\xc9\xa9\x54"
        "\x48\x54\x28\xc8\x4b\xd7\x51\xc8\xcf\x03\xb2\x93\xf3\x0b\x32\x53\x53\x14\xf2\x68\x26\x0b\x00\x50"
        "\x4b\x03\x04\x14\x00\x00\x00\x08\x00\x87\x0b\x5e\x5d\xdf\xb3\xad\x90\x1b\x00\x00\x00\x19\x00\x00"
        "\x00\x14\x00\x00\x00\x43\x72\x61\x74\x65\x20\x50\x61\x63\x6b\x2f\x63\x72\x61\x74\x65\x2e\x6d\x61"
        "\x78\x33\x4e\x29\x56\xc8\x4d\xac\x50\x28\x4e\x4e\xcd\x4b\xd5\x51\xc8\xcb\x2f\x51\xc8\x4b\x4d\x4d"
        "\x49\x4d\x01\x00\x50\x4b\x01\x02\x14\x03\x14\x00\x00\x00\x08\x00\x87\x0b\x5e\x5d\xb1\xae\x51\xa1"
        "\x49\x00\x00\x00\x6b\x00\x00\x00\x14\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x80\x01\x00\x00"
        "\x00\x00\x43\x72\x61\x74\x65\x20\x50\x61\x63\x6b\x2f\x63\x72\x61\x74\x65\x2e\x6f\x62\x6a\x50\x4b"
        "\x01\x02\x14\x03\x14\x00\x00\x00\x08\x00\x87\x0b\x5e\x5d\x41\x74\x78\x98\x2d\x00\x00\x00\x2f\x00"
        "\x00\x00\x14\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x80\x01\x7b\x00\x00\x00\x43\x72\x61\x74"
        "\x65\x20\x50\x61\x63\x6b\x2f\x63\x72\x61\x74\x65\x2e\x6d\x74\x6c\x50\x4b\x01\x02\x14\x03\x14\x00"
        "\x00\x00\x08\x00\x87\x0b\x5e\x5d\x1c\xc5\x7d\xc8\x22\x00\x00\x00\x78\x00\x00\x00\x1d\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x80\x01\xda\x00\x00\x00\x43\x72\x61\x74\x65\x20\x50\x61\x63\x6b"
        "\x2f\x74\x65\x78\x74\x75\x72\x65\x73\x2f\x63\x72\x61\x74\x65\x2e\x70\x6e\x67\x50\x4b\x01\x02\x14"
        "\x03\x14\x00\x00\x00\x08\x00\x87\x0b\x5e\x5d\xdf\xb3\xad\x90\x1b\x00\x00\x00\x19\x00\x00\x00\x14"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x80\x01\x37\x01\x00\x00\x43\x72\x61\x74\x65\x20\x50"
        "\x61\x63\x6b\x2f\x63\x72\x61\x74\x65\x2e\x6d\x61\x78\x50\x4b\x05\x06\x00\x00\x00\x00\x04\x00\x04"
        "\x00\x11\x01\x00\x00\x84\x01\x00\x00\x00\x00";

    std::uint32_t Crc32(const std::string& data)
    {
        std::uint32_t crc = 0xFFFFFFFFu;
        for (const char c : data)
        {
            crc ^= static_cast<unsigned char>(c);
            for (int bit = 0; bit < 8; ++bit)
            {
                crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
            }
        }
        return ~crc;
    }

    // A zip with its entries stored (not compressed), the other method zips use
    std::string StoredZip(const std::vector<std::pair<std::string, std::string>>& entries)
    {
        const auto u16 = [](std::string& out, std::uint32_t v)
        {
            out += static_cast<char>(v & 0xFF);
            out += static_cast<char>((v >> 8) & 0xFF);
        };
        const auto u32 = [&](std::string& out, std::uint32_t v)
        {
            u16(out, v & 0xFFFF);
            u16(out, v >> 16);
        };
        std::string zip;
        std::string directory;
        for (const auto& [name, data] : entries)
        {
            const auto offset = static_cast<std::uint32_t>(zip.size());
            const auto size = static_cast<std::uint32_t>(data.size());
            const auto nameSize = static_cast<std::uint32_t>(name.size());
            u32(zip, 0x04034b50);
            u16(zip, 20);
            u16(zip, 0);
            u16(zip, 0); // stored
            u32(zip, 0);
            u32(zip, Crc32(data));
            u32(zip, size);
            u32(zip, size);
            u16(zip, nameSize);
            u16(zip, 0);
            zip += name + data;

            u32(directory, 0x02014b50);
            u16(directory, 20);
            u16(directory, 20);
            u16(directory, 0);
            u16(directory, 0);
            u32(directory, 0);
            u32(directory, Crc32(data));
            u32(directory, size);
            u32(directory, size);
            u16(directory, nameSize);
            u16(directory, 0);
            u16(directory, 0);
            u16(directory, 0);
            u16(directory, 0);
            u32(directory, 0);
            u32(directory, offset);
            directory += name;
        }
        const auto directoryOffset = static_cast<std::uint32_t>(zip.size());
        zip += directory;
        u32(zip, 0x06054b50);
        u16(zip, 0);
        u16(zip, 0);
        u16(zip, static_cast<std::uint32_t>(entries.size()));
        u16(zip, static_cast<std::uint32_t>(entries.size()));
        u32(zip, static_cast<std::uint32_t>(directory.size()));
        u32(zip, directoryOffset);
        u16(zip, 0);
        return zip;
    }

    const char* kCrateObj = "mtllib crate.mtl\nv 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                            "usemtl wood\nf 1/1 2/2 3/3 4/4\n";
    const char* kCrateMtl = "newmtl wood\nKd 1 1 1\nmap_Kd textures/crate.png\n";
}

TEST_F(AssetConverterTest, FolderPackConvertsItsModelWithItsTexturesAndLeavesTheRestOut)
{
    Write("in/Tower Pack/tower.obj", kCrateObj);
    Write("in/Tower Pack/crate.mtl", kCrateMtl);
    Write("in/Tower Pack/textures/crate.png", "not really a png, only copied");
    Write("in/Tower Pack/tower.max", "3ds max scene");
    Write("in/Tower Pack/readme.txt", "thanks for downloading");

    ConvertOptions options = Options("Tower Pack");
    options.output = root_ / "out" / "models"; // a folder: the .amodel is named after the model inside
    const ConvertResult result = ConvertModelFile(options);
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(result.warnings.empty());
    EXPECT_EQ(result.source, "tower.obj");
    EXPECT_EQ(result.output, root_ / "out" / "models" / "tower.amodel");

    const auto model = Read(result.output);
    ASSERT_TRUE(model.has_value());
    ASSERT_EQ(model->materials.size(), 1u);
    EXPECT_EQ(model->materials[0].baseColorTexture, "../textures/crate.png");
    EXPECT_TRUE(fs::exists(root_ / "out" / "textures" / "crate.png"));
    // Only the model and the texture it uses came out: no .max, no readme
    std::size_t files = 0;
    for (const auto& entry : fs::recursive_directory_iterator(root_ / "out"))
    {
        files += entry.is_regular_file() ? 1 : 0;
    }
    EXPECT_EQ(files, 2u);
}

TEST_F(AssetConverterTest, FolderPackWithOneModelInSeveralFormatsPicksTheBestFormat)
{
    Write("in/pack/OBJ/quad.obj", kCrateObj);
    Write("in/pack/FBX/quad.fbx", kRedQuadFbx);

    ConvertOptions options = Options("pack");
    options.output = root_ / "out" / "models";
    const ConvertResult result = ConvertModelFile(options);
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.source, "FBX/quad.fbx");
    EXPECT_TRUE(fs::exists(root_ / "out" / "models" / "quad.amodel"));
}

TEST_F(AssetConverterTest, FolderPackWithSeveralModelsOrNoneIsRefusedWithAReason)
{
    Write("in/two/tower.obj", kCrateObj);
    Write("in/two/crate.obj", kCrateObj);
    Write("in/none/readme.txt", "no model here");

    ConvertOptions options = Options("two");
    options.output = root_ / "out" / "models";
    const ConvertResult two = ConvertModelFile(options);
    EXPECT_FALSE(two.ok);
    EXPECT_NE(two.error.find("more than one model"), std::string::npos) << two.error;

    options.input = root_ / "in" / "none";
    const ConvertResult none = ConvertModelFile(options);
    EXPECT_FALSE(none.ok);
    EXPECT_NE(none.error.find("no model file"), std::string::npos) << none.error;
    EXPECT_FALSE(fs::exists(root_ / "out" / "models"));
}

TEST_F(AssetConverterTest, DeflatedZipPackIsUnpackedConvertedAndCleanedUp)
{
    Write("in/crate_pack.zip", std::string(kDeflatedPack, sizeof(kDeflatedPack) - 1));
    const auto unpackedFolders = []
    {
        std::size_t count = 0;
        for (const auto& entry : fs::directory_iterator(fs::temp_directory_path()))
        {
            count += entry.path().filename().string().rfind("atelier_import_", 0) == 0 ? 1 : 0;
        }
        return count;
    };
    const std::size_t before = unpackedFolders();

    ConvertOptions options = Options("crate_pack.zip");
    options.output = root_ / "out" / "models";
    const ConvertResult result = ConvertModelFile(options);
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(result.warnings.empty());
    EXPECT_EQ(result.source, "Crate Pack/crate.obj");
    EXPECT_EQ(result.output, root_ / "out" / "models" / "crate.amodel");
    const auto model = Read(result.output);
    ASSERT_TRUE(model.has_value());
    EXPECT_EQ(model->materials[0].baseColorTexture, "../textures/crate.png");
    std::ifstream texture(root_ / "out" / "textures" / "crate.png", std::ios::binary);
    const std::string pixels((std::istreambuf_iterator<char>(texture)), std::istreambuf_iterator<char>());
    EXPECT_EQ(pixels.size(), std::string("not really a png, only copied ").size() * 4);
    EXPECT_EQ(unpackedFolders(), before); // the temporary copy is gone
}

TEST_F(AssetConverterTest, StoredZipKeepsItsFoldersAndSkipsEntriesThatWouldEscape)
{
    Write("in/pack.zip", StoredZip({{"pack/crate.obj", kCrateObj},
                                    {"pack/crate.mtl", kCrateMtl},
                                    {"pack/textures/crate.png", "pixels"},
                                    {"../escaped.txt", "outside"},
                                    {"__MACOSX/pack/._crate.obj", "resource fork"}}));
    std::vector<std::string> warnings;
    std::string error;
    ASSERT_TRUE(Atelier::Tool::ExtractZip(root_ / "in" / "pack.zip", root_ / "unpacked", warnings, error)) << error;
    EXPECT_TRUE(fs::exists(root_ / "unpacked" / "pack" / "textures" / "crate.png"));
    EXPECT_FALSE(fs::exists(root_ / "escaped.txt"));
    EXPECT_FALSE(fs::exists(root_ / "unpacked" / "__MACOSX"));
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].find("outside"), std::string::npos);
}

TEST_F(AssetConverterTest, FileThatIsNotAZipFailsWithAReason)
{
    Write("in/broken.zip", "this is not a zip");
    ConvertOptions options = Options("broken.zip");
    options.output = root_ / "out" / "models";
    const ConvertResult result = ConvertModelFile(options);
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("could not unpack"), std::string::npos) << result.error;
}

TEST_F(AssetConverterTest, ModelWithoutMaterialsWarnsAboutTheImagesBesideIt)
{
    // An OBJ that came without its .mtl: nothing says which image goes where
    Write("in/bare.obj", "v 0 0 0\nv 1 0 0\nv 1 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nf 1/1 2/2 3/3\n");
    Write("in/textures/1.jpg", "cover");
    Write("in/textures/papers.jpg", "pages");

    const ConvertResult result = ConvertModelFile(Options("bare.obj"));
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(HasWarning(result, "2 image(s) beside it (1.jpg, papers.jpg)")) << result.warnings.size();
    EXPECT_TRUE(HasWarning(result, ".mtl"));
    EXPECT_TRUE(result.textures.empty());
}

// --model-folder: the editor's import. The model gets assets/models/<name>/ with its .amodel, the images it uses (named
// by bare file name), the files it was made from, and, from a pack, the pack's other images.
TEST_F(AssetConverterTest, ModelFolderHoldsTheModelItsTexturesAndItsOriginals)
{
    Write("in/Tower Pack/tower.obj", kCrateObj);
    Write("in/Tower Pack/crate.mtl", kCrateMtl);
    Write("in/Tower Pack/textures/crate.png", "not really a png, only copied");
    Write("in/Tower Pack/tower.max", "3ds max scene");
    Write("in/Tower Pack/readme.txt", "thanks for downloading");

    ConvertOptions options = Options("Tower Pack", false);
    options.output = root_ / "out" / "models";
    options.modelFolder = true;
    const ConvertResult result = ConvertModelFile(options);
    ASSERT_TRUE(result.ok) << result.error;
    const fs::path folder = root_ / "out" / "models" / "tower";
    EXPECT_EQ(result.output, folder / "tower.amodel");
    const auto model = Read(result.output);
    ASSERT_TRUE(model.has_value());
    EXPECT_EQ(model->materials[0].baseColorTexture, "crate.png"); // beside the .amodel
    EXPECT_TRUE(fs::is_regular_file(folder / "crate.png"));
    EXPECT_TRUE(fs::is_regular_file(folder / "tower.obj")); // the originals, to import it again
    EXPECT_TRUE(fs::is_regular_file(folder / "crate.mtl"));
    EXPECT_FALSE(fs::exists(folder / "tower.max"));
    EXPECT_FALSE(fs::exists(folder / "readme.txt"));
    EXPECT_FALSE(fs::exists(root_ / "out" / "textures"));
}

TEST_F(AssetConverterTest, ZipWithoutMtlKeepsItsImagesWithTheModel)
{
    // books_and_mugs: an OBJ shipped without its .mtl, with its pictures beside it
    Write("in/books.zip", StoredZip({{"books/books.obj", "v 0 0 0\nv 1 0 0\nv 1 1 0\nvt 0 0\nvt 1 0\nvt 1 1\n"
                                                         "f 1/1 2/2 3/3\n"},
                                     {"books/1.jpg", "cover"},
                                     {"books/textures/papers.jpg", "pages"},
                                     {"books/preview.jpg", "a render of the scene"},
                                     {"books/books.max", "3ds max scene"},
                                     {"__MACOSX/books/._1.jpg", "resource fork"}}));
    ConvertOptions options = Options("books.zip", false);
    options.output = root_ / "out" / "models";
    options.modelFolder = true;
    const ConvertResult result = ConvertModelFile(options);
    ASSERT_TRUE(result.ok) << result.error;
    const fs::path folder = root_ / "out" / "models" / "books";
    EXPECT_EQ(result.output, folder / "books.amodel");
    EXPECT_TRUE(fs::is_regular_file(folder / "books.obj"));
    EXPECT_TRUE(fs::is_regular_file(folder / "1.jpg"));
    EXPECT_TRUE(fs::is_regular_file(folder / "papers.jpg"));
    EXPECT_FALSE(fs::exists(folder / "preview.jpg"));
    EXPECT_FALSE(fs::exists(folder / "books.max"));
    EXPECT_FALSE(fs::exists(folder / "._1.jpg"));
    EXPECT_TRUE(HasWarning(result, "stored with it")) << (result.warnings.empty() ? "" : result.warnings[0]);
}

TEST_F(AssetConverterTest, ModelFolderIsUpdatedByTheSameModelAndNumberedForAnother)
{
    Write("in/crate.obj", kCrateObj);
    Write("in/crate.mtl", kCrateMtl);
    Write("in/textures/crate.png", "pixels");
    Write("in/unrelated.png", "a lone file's neighbours are not part of it");

    ConvertOptions options = Options("crate.obj", false);
    options.output = root_ / "out" / "models";
    options.modelFolder = true;
    const ConvertResult first = ConvertModelFile(options);
    ASSERT_TRUE(first.ok) << first.error;
    EXPECT_EQ(first.output, root_ / "out" / "models" / "crate" / "crate.amodel");
    EXPECT_FALSE(fs::exists(root_ / "out" / "models" / "crate" / "unrelated.png"));

    const ConvertResult again = ConvertModelFile(options);
    ASSERT_TRUE(again.ok) << again.error;
    EXPECT_EQ(again.output, first.output); // an update
    EXPECT_FALSE(fs::exists(root_ / "out" / "models" / "crate" / "crate_2.png"));

    Write("in/crate.obj", std::string(kCrateObj) + "v 5 5 5\n"); // another model of the same name
    const ConvertResult other = ConvertModelFile(options);
    ASSERT_TRUE(other.ok) << other.error;
    EXPECT_EQ(other.output, root_ / "out" / "models" / "crate_2" / "crate_2.amodel");
    EXPECT_TRUE(fs::is_regular_file(root_ / "out" / "models" / "crate_2" / "crate.obj"));
}

TEST_F(AssetConverterTest, ModelFolderNeedsAFolderAsOutput)
{
    Write("in/crate.obj", kCrateObj);
    ConvertOptions options = Options("crate.obj", false);
    options.modelFolder = true; // output is still crate.amodel
    const ConvertResult result = ConvertModelFile(options);
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("models folder"), std::string::npos) << result.error;
}

// The rule by format: a .glb carries everything it uses, so its folder holds the .glb and the images written out of it,
// not the other images of its pack; an OBJ's folder keeps them (see ModelFolderHoldsTheModelItsTexturesAndItsOriginals)
TEST_F(AssetConverterTest, GlbFolderHoldsTheGlbAndItsOwnImagesOnly)
{
    const std::vector<std::uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0,    0,    0,
                                           0x0D, 'I', 'H', 'D', 'R',  0,    0,    0,    1,    0,    0,
                                           0,    1,   8,   6,   0,    0,    0,    0x1F, 0x15, 0xC4, 0x89};
    Write("in/pack/tri.glb", Glb(TriangleGltf("data:image/png;base64," + Base64(png), true), TriangleBuffer()));
    Write("in/pack/render.png", "a picture the site added");

    ConvertOptions options = Options("pack", false);
    options.output = root_ / "out" / "models";
    options.modelFolder = true;
    const ConvertResult result = ConvertModelFile(options);
    ASSERT_TRUE(result.ok) << result.error;
    const fs::path folder = root_ / "out" / "models" / "tri";
    EXPECT_EQ(result.output, folder / "tri.amodel");
    EXPECT_TRUE(fs::is_regular_file(folder / "tri.glb"));
    ASSERT_EQ(result.textures.size(), 1u); // the image inside the .glb, written out beside the .amodel
    EXPECT_EQ(result.textures[0].parent_path(), folder);
    EXPECT_FALSE(fs::exists(folder / "render.png"));

    std::size_t files = 0;
    for (const auto& entry : fs::directory_iterator(folder))
    {
        files += entry.is_regular_file() ? 1 : 0;
    }
    EXPECT_EQ(files, 3u); // tri.amodel, tri.glb, the image
}

// The editor's import: the model's folder under art/models gets the .amodel and the originals, and its images go to
// art/textures/<name>/, named in the .amodel by a path relative to it
TEST_F(AssetConverterTest, ModelFolderWithATexturesFolderSplitsTheModelFromItsImages)
{
    Write("in/Tower Pack/tower.obj", kCrateObj);
    Write("in/Tower Pack/crate.mtl", kCrateMtl);
    Write("in/Tower Pack/textures/crate.png", "pixels");
    Write("in/Tower Pack/extra.jpg", "an image no material names");

    ConvertOptions options = Options("Tower Pack", false);
    options.output = root_ / "out" / "art" / "models";
    options.texturesDir = root_ / "out" / "art" / "textures";
    options.modelFolder = true;
    const ConvertResult result = ConvertModelFile(options);
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.output, root_ / "out" / "art" / "models" / "tower" / "tower.amodel");
    const auto model = Read(result.output);
    ASSERT_TRUE(model.has_value());
    EXPECT_EQ(model->materials[0].baseColorTexture, "../../textures/tower/crate.png");
    const fs::path textures = root_ / "out" / "art" / "textures" / "tower";
    EXPECT_TRUE(fs::is_regular_file(textures / "crate.png"));
    EXPECT_TRUE(fs::is_regular_file(textures / "extra.jpg"));
    EXPECT_TRUE(fs::is_regular_file(root_ / "out" / "art" / "models" / "tower" / "tower.obj"));
    EXPECT_TRUE(fs::is_regular_file(root_ / "out" / "art" / "models" / "tower" / "crate.mtl"));
    EXPECT_FALSE(fs::exists(root_ / "out" / "art" / "models" / "tower" / "crate.png"));

    // Imported again from the game's own folder: the original .mtl names crate.png by the pack's folders, and the
    // image is found in art/textures/tower/
    ConvertOptions again = options;
    again.input = root_ / "out" / "art" / "models" / "tower" / "tower.obj";
    const ConvertResult reimported = ConvertModelFile(again);
    ASSERT_TRUE(reimported.ok) << reimported.error;
    EXPECT_EQ(reimported.output, result.output);
    EXPECT_FALSE(HasWarning(reimported, "texture not found"));
}
