/* !
@file               ModelImporterTests.cpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Tests of importing a model into a game by running the asset tool: the
    model gets a folder of its own under assets/art/models with its .amodel and
    its original files, its images go to assets/art/textures/<name>/, the
    tool's warnings and errors
    reach the caller, and without the tool .obj/.fbx files are still copied.
    The tests that run the tool are skipped when it was not built.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/assets/loaders/AtelierModel.hpp"
#include "engine/game/GameProject.hpp"
#include "engine/game/ModelImporter.hpp"
#include "engine/platform/Process.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <vector>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using Engine::Game::GameProject;
using Engine::Game::ImportModel;
using Engine::Game::IsImportableModel;
using Engine::Game::IsModelPackage;

namespace
{
    namespace fs = std::filesystem;

    // The tool the build made next to the tests, or an empty path when it was not built
    fs::path BuiltTool()
    {
#ifdef ATELIER_ASSET_TOOL_PATH
        std::error_code ec;
        const fs::path tool = ATELIER_ASSET_TOOL_PATH;
        return fs::is_regular_file(tool, ec) ? tool : fs::path{};
#else
        return {};
#endif
    }

    class ModelImporterTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            root_ = fs::temp_directory_path() / "atelier_tests" / ("import_" + std::string(info->name()));
            fs::remove_all(root_);
            fs::create_directories(root_ / "game");
            fs::create_directories(root_ / "downloads");
        }

        void TearDown() override
        {
            std::error_code ec;
            fs::remove_all(root_, ec);
        }

        void Write(const fs::path& relative, const std::string& text) const
        {
            const fs::path path = root_ / relative;
            fs::create_directories(path.parent_path());
            std::ofstream(path, std::ios::binary) << text;
        }

        // A textured square, as an artist's folder would hold it
        void WriteSquare(const std::string& name = "square") const
        {
            Write("downloads/" + name + ".obj",
                  "mtllib " + name + ".mtl\nv 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                  "vn 0 0 1\nusemtl paint\nf 1/1/1 2/2/1 3/3/1 4/4/1\n");
            Write("downloads/" + name + ".mtl", "newmtl paint\nKd 1 1 1\nmap_Kd paint.png\n");
            Write("downloads/paint.png", "pixels");
        }

        GameProject Project() const { return GameProject(root_ / "game"); }

        fs::path root_;
    };
}

TEST(ModelImporterFormatTest, RecognisesTheFormatsTheToolConverts)
{
    for (const char* file : {"a.obj", "a.FBX", "dir/a.glb", "a.gltf", "a.dae", "a.3ds", "a.ply", "a.stl"})
    {
        EXPECT_TRUE(IsImportableModel(file)) << file;
    }
    for (const char* file : {"a.amodel", "a.png", "a.mtl", "a", "a.obj.txt"})
    {
        EXPECT_FALSE(IsImportableModel(file)) << file;
    }
}

TEST(ProcessTest, ProgramThatDoesNotExistDoesNotStart)
{
    const auto result = Engine::Platform::RunProcess(fs::temp_directory_path() / "no_such_program_atelier", {});
    EXPECT_FALSE(result.started);
    EXPECT_EQ(result.exitCode, -1);
}

TEST(ProcessTest, ToolReportsItsUsageAndExitCode)
{
    const fs::path tool = BuiltTool();
    if (tool.empty())
    {
        GTEST_SKIP() << "AtelierAssetTool was not built";
    }
    const auto help = Engine::Platform::RunProcess(tool, {"--help"});
    EXPECT_TRUE(help.started);
    EXPECT_EQ(help.exitCode, 0);
    EXPECT_NE(help.output.find("usage"), std::string::npos);

    const auto wrong = Engine::Platform::RunProcess(tool, {"only one argument"});
    EXPECT_TRUE(wrong.started);
    EXPECT_EQ(wrong.exitCode, 1);
}

TEST_F(ModelImporterTest, ConvertsTheModelAndBringsItsTexturesIntoTheGame)
{
    const fs::path tool = BuiltTool();
    if (tool.empty())
    {
        GTEST_SKIP() << "AtelierAssetTool was not built";
    }
    WriteSquare();

    Write("downloads/unrelated.png", "something else in the downloads folder");

    const auto result = ImportModel(Project(), root_ / "downloads" / "square.obj", Engine::Game::kModelsFolder, tool);
    EXPECT_EQ(result.path, "assets/art/models/square/square.amodel");
    EXPECT_TRUE(result.warnings.empty());
    const fs::path folder = root_ / "game" / "assets" / "art" / "models" / "square";
    const fs::path textures = root_ / "game" / "assets" / "art" / "textures" / "square";
    EXPECT_TRUE(fs::is_regular_file(folder / "square.amodel"));
    EXPECT_TRUE(fs::is_regular_file(textures / "paint.png"));
    EXPECT_FALSE(fs::exists(folder / "paint.png"));
    // The originals stay with it, so it can be imported again
    EXPECT_TRUE(fs::is_regular_file(folder / "square.obj"));
    EXPECT_TRUE(fs::is_regular_file(folder / "square.mtl"));
    EXPECT_FALSE(fs::exists(textures / "unrelated.png")); // the neighbours of a lone file are not part of it
    // The .amodel names its texture relative to itself
    std::ifstream file(folder / "square.amodel", std::ios::binary);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const auto model = Engine::Assets::ParseAtelierModel(bytes);
    ASSERT_TRUE(model.has_value());
    EXPECT_EQ(model->materials[0].baseColorTexture, "../../textures/square/paint.png");
}

TEST_F(ModelImporterTest, ImportedModelIsCentredAtTheOriginAndFitsTheStandardSize)
{
    const fs::path tool = BuiltTool();
    if (tool.empty())
    {
        GTEST_SKIP() << "AtelierAssetTool was not built";
    }
    Write("downloads/far.obj", "v 4800 0 200\nv 5200 0 200\nv 5200 100 200\nv 4800 100 200\nf 1 2 3 4\n");

    const auto result = ImportModel(Project(), root_ / "downloads" / "far.obj", Engine::Game::kModelsFolder, tool);
    std::ifstream file(root_ / "game" / result.path, std::ios::binary);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const auto model = Engine::Assets::ParseAtelierModel(bytes);
    ASSERT_TRUE(model.has_value());
    // One mesh on one node whose root places it: the vertices are the file's, the root brings them in
    const glm::mat4& root = model->nodes[0].localTransform;
    const auto& positions = model->meshes[0].positions;
    const glm::vec3 first(root * glm::vec4(positions[0], positions[1], positions[2], 1.0f));
    EXPECT_LE(std::abs(first.x), Engine::Game::kImportedModelSize);
    EXPECT_LE(std::abs(first.z), Engine::Game::kImportedModelSize);
}

TEST_F(ModelImporterTest, ImportingAgainUpdatesTheFolderAndAnotherModelOfTheSameNameGetsItsOwn)
{
    const fs::path tool = BuiltTool();
    if (tool.empty())
    {
        GTEST_SKIP() << "AtelierAssetTool was not built";
    }
    WriteSquare();
    const GameProject project = Project();
    ImportModel(project, root_ / "downloads" / "square.obj", Engine::Game::kModelsFolder, tool);
    const auto again = ImportModel(project, root_ / "downloads" / "square.obj", Engine::Game::kModelsFolder, tool);

    EXPECT_EQ(again.path, "assets/art/models/square/square.amodel");
    std::size_t images = 0;
    for (const auto& entry : fs::directory_iterator(root_ / "game" / "assets" / "art" / "textures" / "square"))
    {
        images += entry.path().extension() == ".png" ? 1 : 0;
    }
    EXPECT_EQ(images, 1u);

    Write("downloads/square.obj", "v 0 0 0\nv 1 0 0\nv 1 1 0\nf 1 2 3\n"); // a different model called square
    const auto other = ImportModel(project, root_ / "downloads" / "square.obj", Engine::Game::kModelsFolder, tool);
    EXPECT_EQ(other.path, "assets/art/models/square_2/square_2.amodel");
}

TEST_F(ModelImporterTest, WarningsFromTheToolReachTheCaller)
{
    const fs::path tool = BuiltTool();
    if (tool.empty())
    {
        GTEST_SKIP() << "AtelierAssetTool was not built";
    }
    WriteSquare();
    fs::remove(root_ / "downloads" / "paint.png"); // the model now names a texture that is not there

    const auto result = ImportModel(Project(), root_ / "downloads" / "square.obj", Engine::Game::kModelsFolder, tool);
    ASSERT_FALSE(result.warnings.empty());
    EXPECT_NE(result.warnings[0].find("paint.png"), std::string::npos);
}

TEST_F(ModelImporterTest, AModelFolderWithSpacesAndAnotherModelsFolderWork)
{
    const fs::path tool = BuiltTool();
    if (tool.empty())
    {
        GTEST_SKIP() << "AtelierAssetTool was not built";
    }
    WriteSquare("my square");
    const auto result =
        ImportModel(Project(), root_ / "downloads" / "my square.obj", "assets/art/models/props", tool);
    EXPECT_EQ(result.path, "assets/art/models/props/my square/my square.amodel");
    EXPECT_TRUE(fs::is_regular_file(root_ / "game" / "assets/art/models/props/my square/my square.amodel"));
}

TEST_F(ModelImporterTest, ABrokenModelThrowsWithTheToolsReason)
{
    const fs::path tool = BuiltTool();
    if (tool.empty())
    {
        GTEST_SKIP() << "AtelierAssetTool was not built";
    }
    Write("downloads/broken.fbx", "this is not a model");

    try
    {
        ImportModel(Project(), root_ / "downloads" / "broken.fbx", Engine::Game::kModelsFolder, tool);
        FAIL() << "expected an exception";
    }
    catch (const std::runtime_error& error)
    {
        EXPECT_NE(std::string(error.what()).find("broken.fbx"), std::string::npos);
    }
    EXPECT_FALSE(fs::exists(root_ / "game" / "assets/art/models/broken/broken.amodel"));
}

TEST_F(ModelImporterTest, WithoutTheToolObjAndFbxAreCopiedAsTheyAreWithAWarning)
{
    WriteSquare();
    const auto result = ImportModel(Project(), root_ / "downloads" / "square.obj", Engine::Game::kModelsFolder,
                                    root_ / "no_tool_here");
    EXPECT_EQ(result.path, "assets/art/models/square/square.obj");
    ASSERT_EQ(result.warnings.size(), 1u);
    EXPECT_NE(result.warnings[0].find("AtelierAssetTool"), std::string::npos);
    EXPECT_TRUE(fs::is_regular_file(root_ / "game" / "assets/art/models/square/square.obj"));
}

TEST_F(ModelImporterTest, WithoutTheToolOtherFormatsCannotBeImported)
{
    Write("downloads/thing.dae", "<COLLADA/>");
    EXPECT_THROW(ImportModel(Project(), root_ / "downloads" / "thing.dae", Engine::Game::kModelsFolder,
                             root_ / "no_tool_here"),
                 std::runtime_error);
}

TEST_F(ModelImporterTest, NotAModelAndMissingFilesAreRefused)
{
    Write("downloads/picture.png", "png");
    EXPECT_THROW(ImportModel(Project(), root_ / "downloads" / "picture.png"), std::runtime_error);
    EXPECT_THROW(ImportModel(Project(), root_ / "downloads" / "missing.obj"), std::runtime_error);
}

TEST_F(ModelImporterTest, AZipOrAFolderWithOneModelIsAModelPack)
{
    Write("downloads/pack.zip", "contents are checked by the tool");
    Write("downloads/one/OBJ/tower.obj", "v 0 0 0");
    Write("downloads/one/FBX/tower.fbx", "same model, another format");
    Write("downloads/one/tower.max", "scene");
    Write("downloads/two/tower.obj", "v 0 0 0");
    Write("downloads/two/crate.obj", "v 0 0 0");
    Write("downloads/none/picture.png", "png");

    EXPECT_TRUE(IsModelPackage(root_ / "downloads" / "pack.zip"));
    EXPECT_TRUE(IsModelPackage(root_ / "downloads" / "one"));
    EXPECT_FALSE(IsModelPackage(root_ / "downloads" / "two"));  // several models: imported file by file
    EXPECT_FALSE(IsModelPackage(root_ / "downloads" / "none")); // no model: copied as it is
    EXPECT_FALSE(IsModelPackage(root_ / "downloads" / "two" / "tower.obj"));
    EXPECT_FALSE(IsModelPackage(root_ / "downloads" / "missing.zip"));
}

TEST_F(ModelImporterTest, AFolderPackIsImportedInOneGoWithoutTheFilesTheModelDoesNotUse)
{
    const fs::path tool = BuiltTool();
    if (tool.empty())
    {
        GTEST_SKIP() << "AtelierAssetTool was not built";
    }
    WriteSquare();
    for (const char* file : {"square.obj", "square.mtl", "paint.png"})
    {
        fs::create_directories(root_ / "downloads" / "Square Pack");
        fs::rename(root_ / "downloads" / file, root_ / "downloads" / "Square Pack" / file);
    }
    Write("downloads/Square Pack/square.max", "3ds max scene");

    const auto result = ImportModel(Project(), root_ / "downloads" / "Square Pack", Engine::Game::kModelsFolder, tool);
    EXPECT_EQ(result.path, "assets/art/models/square/square.amodel"); // named after the model inside, not the folder
    EXPECT_TRUE(result.warnings.empty());
    EXPECT_TRUE(fs::is_regular_file(root_ / "game" / "assets/art/models/square/square.amodel"));
    EXPECT_TRUE(fs::is_regular_file(root_ / "game" / "assets/art/textures/square/paint.png"));
    for (const auto& entry : fs::recursive_directory_iterator(root_ / "game"))
    {
        EXPECT_NE(entry.path().extension(), ".max") << entry.path();
    }
}

TEST_F(ModelImporterTest, WithoutTheToolAModelPackCannotBeImported)
{
    Write("downloads/pack/thing.obj", "v 0 0 0");
    EXPECT_THROW(
        ImportModel(Project(), root_ / "downloads" / "pack", Engine::Game::kModelsFolder, root_ / "no_tool_here"),
        std::runtime_error);
}

TEST_F(ModelImporterTest, AZipWhoseObjHasNoMtlStillBringsItsImages)
{
    const fs::path tool = BuiltTool();
    if (tool.empty())
    {
        GTEST_SKIP() << "AtelierAssetTool was not built";
    }
    // books_and_mugs: an OBJ shipped without its .mtl, its pictures in a folder beside it
    Write("downloads/Books/books.obj", "v 0 0 0\nv 1 0 0\nv 1 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nf 1/1 2/2 3/3\n");
    Write("downloads/Books/textures/cover.jpg", "cover");
    Write("downloads/Books/textures/pages.jpg", "pages");

    const auto result = ImportModel(Project(), root_ / "downloads" / "Books", Engine::Game::kModelsFolder, tool);
    EXPECT_EQ(result.path, "assets/art/models/books/books.amodel");
    const fs::path textures = root_ / "game" / "assets/art/textures/books";
    EXPECT_TRUE(fs::is_regular_file(textures / "cover.jpg"));
    EXPECT_TRUE(fs::is_regular_file(textures / "pages.jpg"));
    EXPECT_TRUE(fs::is_regular_file(root_ / "game" / "assets/art/models/books/books.obj"));
    ASSERT_FALSE(result.warnings.empty()); // says the images are there but not on the model
    EXPECT_NE(result.warnings[0].find("Texture"), std::string::npos) << result.warnings[0];
}

TEST_F(ModelImporterTest, AnAmodelIsCopiedIntoAFolderOfItsOwn)
{
    Engine::Assets::ModelData model;
    Engine::Assets::MaterialData material;
    material.baseColorTexture = "wood.png";
    model.materials.push_back(material);
    Engine::Assets::MeshData mesh;
    mesh.positions = {0, 0, 0, 1, 0, 0, 1, 1, 0};
    mesh.indices = {0, 1, 2};
    mesh.subMeshes.push_back(Engine::Assets::SubMesh{0, 3, 0, 0});
    model.meshes.push_back(mesh);
    model.nodes.push_back(Engine::Assets::ModelNode{});
    model.nodes[0].meshIndices = {0};
    model.roots = {0};
    const auto bytes = Engine::Assets::WriteAtelierModel(model);
    fs::create_directories(root_ / "downloads" / "crate");
    std::ofstream(root_ / "downloads" / "crate" / "crate.amodel", std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Write("downloads/crate/wood.png", "pixels");

    const auto result = ImportModel(Project(), root_ / "downloads" / "crate" / "crate.amodel",
                                    Engine::Game::kModelsFolder, root_ / "no_tool_needed");
    EXPECT_EQ(result.path, "assets/art/models/crate/crate.amodel");
    EXPECT_TRUE(fs::is_regular_file(root_ / "game" / "assets/art/textures/crate/wood.png")); // moved to its place
    std::ifstream file(root_ / "game" / result.path, std::ios::binary);
    const std::vector<std::uint8_t> copied((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const auto imported = Engine::Assets::ParseAtelierModel(copied);
    ASSERT_TRUE(imported.has_value());
    EXPECT_EQ(imported->materials[0].baseColorTexture, "../../textures/crate/wood.png");
}
