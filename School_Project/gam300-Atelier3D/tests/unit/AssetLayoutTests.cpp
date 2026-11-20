/* !
@file               AssetLayoutTests.cpp
@author             w.kyele@digipen.edu [100%]
@date               30/10/2026
@brief
    Tests of the migration of a game from the earlier asset layouts (every
    image in assets/textures with models loose in assets/models; or a folder
    per model in assets/models) to the current one by type: art/models/<name>/,
    art/textures/<name>/, art/sprites, art/spritesheets, with .amodel, .mtl and
    scene paths updated, and nothing done twice.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/assets/loaders/AtelierModel.hpp"
#include "engine/game/AssetLayout.hpp"

#include <gtest/gtest.h>

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

    class AssetLayoutTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            game_ = fs::temp_directory_path() / "atelier_tests" / ("layout_" + std::string(info->name()));
            fs::remove_all(game_);
            fs::create_directories(game_);
        }

        void TearDown() override
        {
            std::error_code ec;
            fs::remove_all(game_, ec);
        }

        void Write(const fs::path& relative, const std::string& text) const
        {
            fs::create_directories((game_ / relative).parent_path());
            std::ofstream(game_ / relative, std::ios::binary) << text;
        }

        std::string Read(const fs::path& relative) const
        {
            std::ifstream file(game_ / relative, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }

        // A triangle whose material names the two textures, as the old asset tool wrote them
        void WriteAmodel(const fs::path& relative, const std::string& albedo, const std::string& normal) const
        {
            Engine::Assets::ModelData model;
            Engine::Assets::MeshData mesh;
            mesh.positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
            mesh.indices = {0, 1, 2};
            mesh.subMeshes.push_back({0, 3, 0, 0});
            model.meshes.push_back(mesh);
            Engine::Assets::MaterialData material;
            material.baseColorTexture = albedo;
            material.normalTexture = normal;
            model.materials.push_back(material);
            model.nodes.push_back({});
            model.nodes[0].meshIndices = {0};
            model.roots = {0};
            const auto bytes = Engine::Assets::WriteAtelierModel(model);
            Write(relative, std::string(bytes.begin(), bytes.end()));
        }

        std::optional<Engine::Assets::ModelData> ReadAmodel(const fs::path& relative) const
        {
            const std::string text = Read(relative);
            return Engine::Assets::ParseAtelierModel(std::vector<std::uint8_t>(text.begin(), text.end()));
        }

        // A game as the editor made them before models had folders of their own
        void WriteOldGame() const
        {
            Write("configs/game.cfg", "name = old\n");
            Write("assets/models/crate.obj", "mtllib crate.mtl\nv 0 0 0\n");
            Write("assets/models/crate.mtl",
                  "newmtl wood\r\nmap_Kd ../textures/crate.png\r\nmap_Bump -bm 1 ../textures/shared.png\r\n");
            Write("assets/textures/crate.png", "crate pixels");
            Write("assets/textures/shared.png", "used by two models");
            Write("assets/models/tower.fbx", "binary fbx ... Tower_Col.jpg ... more bytes");
            Write("assets/textures/Tower_Col.jpg", "tower pixels");
            WriteAmodel("assets/models/books.amodel", "../textures/cover.jpg", "../textures/shared.png");
            Write("assets/textures/cover.jpg", "cover pixels");
            Write("assets/textures/wood.png", "a texture for the built-in cube");
            Write("assets/materials/.gitkeep", "");
            Write("assets/audio/beep.wav", "sound");
            Write(
                "data/scenes/main.json",
                R"({"objects":[{"renderable":{"mesh":"assets/models/crate.obj","texture":"assets/textures/wood.png"}},)"
                R"({"renderable":{"texture":"assets/textures/missing.png"}},)"
                R"({"audio_source":{"clip":"assets/audio/beep.wav"}}]})");
            Write("assets/prefabs/Enemy.prefab", R"({"renderable":{"texture":"assets/textures/crate.png"}})");
            Write("data/assets.editor.favorites", R"({"version":1,"favorites":["assets/models/tower.fbx"]})");
        }

        fs::path game_;
    };
}

TEST_F(AssetLayoutTest, AnOldGameNeedsMigratingAndANewOneDoesNot)
{
    WriteOldGame();
    EXPECT_TRUE(Engine::Game::NeedsLayoutMigration(game_));

    fs::remove_all(game_);
    Write("configs/game.cfg", "name = new\n");
    Write("assets/art/models/car/car.amodel", "x");
    Write("assets/art/models/car/car.fbx", "x");
    Write("assets/art/textures/car/paint.png", "x");
    Write("assets/art/textures/wood.png", "x");
    Write("assets/art/sprites/hero.png", "x");
    EXPECT_FALSE(Engine::Game::NeedsLayoutMigration(game_));

    Write("assets/art/models/car/stray.png", "an image among the models");
    EXPECT_TRUE(Engine::Game::NeedsLayoutMigration(game_));
}

TEST_F(AssetLayoutTest, EachModelGetsAModelsFolderAndATexturesFolder)
{
    WriteOldGame();
    const Engine::Game::LayoutMigration report = Engine::Game::MigrateAssetLayout(game_);
    EXPECT_TRUE(report.warnings.empty()) << report.warnings.front();
    EXPECT_TRUE(report.Changed());

    for (const char* file : {"assets/art/models/crate/crate.obj", "assets/art/models/crate/crate.mtl",
                             "assets/art/textures/crate/crate.png", "assets/art/textures/crate/shared.png",
                             "assets/art/models/tower/tower.fbx", "assets/art/textures/tower/Tower_Col.jpg",
                             "assets/art/models/books/books.amodel", "assets/art/textures/books/cover.jpg",
                             "assets/art/textures/books/shared.png", // used by two models: a copy in each
                             "assets/art/textures/wood.png",         // used by no model: a 3D texture of its own
                             "assets/audio/beep.wav"})
    {
        EXPECT_TRUE(fs::is_regular_file(game_ / file)) << file;
    }
    for (const char* gone : {"assets/textures", "assets/materials", "assets/models"})
    {
        EXPECT_FALSE(fs::exists(game_ / gone)) << gone;
    }
    EXPECT_TRUE(fs::is_directory(game_ / "assets/art/sprites"));
    EXPECT_TRUE(fs::is_directory(game_ / "assets/art/spritesheets"));
}

TEST_F(AssetLayoutTest, PathsInsideModelsAndMaterialLibrariesLeadToTheTexturesFolder)
{
    WriteOldGame();
    Engine::Game::MigrateAssetLayout(game_);

    EXPECT_EQ(
        Read("assets/art/models/crate/crate.mtl"),
        "newmtl wood\r\nmap_Kd ../../textures/crate/crate.png\r\nmap_Bump -bm 1 ../../textures/crate/shared.png\r\n");
    const auto books = ReadAmodel("assets/art/models/books/books.amodel");
    ASSERT_TRUE(books.has_value()); // rewritten with a valid checksum
    EXPECT_EQ(books->materials[0].baseColorTexture, "../../textures/books/cover.jpg");
    EXPECT_EQ(books->materials[0].normalTexture, "../../textures/books/shared.png");
}

TEST_F(AssetLayoutTest, ScenesPrefabsAndFavoritesFollowTheFilesAndMissingOnesStayMissing)
{
    WriteOldGame();
    Engine::Game::MigrateAssetLayout(game_);

    const std::string scene = Read("data/scenes/main.json");
    EXPECT_NE(scene.find(R"("mesh":"assets/art/models/crate/crate.obj")"), std::string::npos) << scene;
    EXPECT_NE(scene.find(R"("texture":"assets/art/textures/wood.png")"), std::string::npos) << scene;
    EXPECT_NE(scene.find(R"("texture":"assets/textures/missing.png")"), std::string::npos) << scene; // never there
    EXPECT_NE(scene.find(R"("clip":"assets/audio/beep.wav")"), std::string::npos) << scene;
    EXPECT_NE(Read("assets/prefabs/Enemy.prefab").find("assets/art/textures/crate/crate.png"), std::string::npos);
    EXPECT_NE(Read("data/assets.editor.favorites").find("assets/art/models/tower/tower.fbx"), std::string::npos);
}

TEST_F(AssetLayoutTest, AGameWithAFolderPerModelMovesItsImagesToTheTexturesFolder)
{
    // The layout before this one: assets/models/<name>/ with the images inside, sprites at the top of assets/
    Write("configs/game.cfg", "name = middle\n");
    WriteAmodel("assets/models/car/car.amodel", "paint.png", "");
    Write("assets/models/car/car.fbx", "the original");
    Write("assets/models/car/paint.png", "paint");
    Write("assets/models/car/decals.jpg", "a loose image of the pack");
    Write("assets/models/wood/wood.png", "a texture for the built-in cube");
    Write("assets/sprites/hero.png", "hero");
    Write("assets/spritesheets/run.png", "frames");
    Write("data/scenes/main.json", R"({"objects":[{"renderable":{"model":"assets/models/car/car.amodel"}},)"
                                   R"({"renderable":{"texture":"assets/models/wood/wood.png"}}]})");

    const Engine::Game::LayoutMigration report = Engine::Game::MigrateAssetLayout(game_);
    EXPECT_TRUE(report.warnings.empty()) << report.warnings.front();
    for (const char* file :
         {"assets/art/models/car/car.amodel", "assets/art/models/car/car.fbx", "assets/art/textures/car/paint.png",
          "assets/art/textures/car/decals.jpg", "assets/art/textures/wood.png", "assets/art/sprites/hero.png",
          "assets/art/spritesheets/run.png"})
    {
        EXPECT_TRUE(fs::is_regular_file(game_ / file)) << file;
    }
    EXPECT_FALSE(fs::exists(game_ / "assets/models"));
    EXPECT_FALSE(fs::exists(game_ / "assets/sprites"));
    const auto car = ReadAmodel("assets/art/models/car/car.amodel");
    ASSERT_TRUE(car.has_value());
    EXPECT_EQ(car->materials[0].baseColorTexture, "../../textures/car/paint.png");
    const std::string scene = Read("data/scenes/main.json");
    EXPECT_NE(scene.find("assets/art/models/car/car.amodel"), std::string::npos) << scene;
    EXPECT_NE(scene.find("assets/art/textures/wood.png"), std::string::npos) << scene;
    EXPECT_FALSE(Engine::Game::NeedsLayoutMigration(game_));
}

TEST_F(AssetLayoutTest, MigratingTwiceChangesNothingTheSecondTime)
{
    WriteOldGame();
    Engine::Game::MigrateAssetLayout(game_);
    const std::string scene = Read("data/scenes/main.json");
    const std::string library = Read("assets/art/models/crate/crate.mtl");

    EXPECT_FALSE(Engine::Game::NeedsLayoutMigration(game_));
    const Engine::Game::LayoutMigration again = Engine::Game::MigrateAssetLayout(game_);
    EXPECT_FALSE(again.Changed());
    EXPECT_EQ(Read("data/scenes/main.json"), scene);
    EXPECT_EQ(Read("assets/art/models/crate/crate.mtl"), library);
}

TEST_F(AssetLayoutTest, ModelsInASubfolderKeepItAndAFolderWithoutAssetsIsLeftAlone)
{
    Write("assets/models/props/barrel.fbx", "barrel");
    const Engine::Game::LayoutMigration report = Engine::Game::MigrateAssetLayout(game_);
    EXPECT_TRUE(fs::is_regular_file(game_ / "assets/art/models/props/barrel/barrel.fbx"));
    EXPECT_FALSE(fs::exists(game_ / "assets/art/sprites")); // not a game (no configs/game.cfg): no folders added
    EXPECT_EQ(report.moved.size(), 1u);

    EXPECT_FALSE(Engine::Game::MigrateAssetLayout(game_ / "nowhere").Changed());
}
