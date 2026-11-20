#include "engine/assets/MaterialFile.hpp"
#include "engine/game/GameProject.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

using Engine::Assets::MaterialFile;
using Engine::Assets::ReadMaterialFile;
using Engine::Assets::WriteMaterialFile;

namespace
{
    class MaterialFileTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            root_ = std::filesystem::temp_directory_path() / "atelier_material_tests" / info->name();
            std::filesystem::remove_all(root_);
            std::filesystem::create_directories(root_);
        }

        void TearDown() override { std::filesystem::remove_all(root_); }

        std::filesystem::path root_;
    };
}

// Everything a material holds comes back as it was written
TEST_F(MaterialFileTest, WritesAndReadsEveryField)
{
    MaterialFile material;
    material.shader = "assets/shaders/Toon.vert";
    material.color = glm::vec4(1.0f, 0.25f, 0.5f, 0.75f);
    material.texture = "assets/art/textures/brick.png";
    material.metallic = 0.75f;
    material.roughness = 0.125f;
    material.emissive = glm::vec3(1.0f, 0.5f, 0.0f);
    material.emissiveStrength = 2.0f;
    material.values = {{"u_Brightness", 1.5f}, {"u_Bands", 4.0f}};

    const auto path = root_ / "materials" / "Shiny.amat";
    ASSERT_TRUE(WriteMaterialFile(path, material)) << "its folder is made";
    const auto read = ReadMaterialFile(path);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(read->shader, material.shader);
    EXPECT_EQ(read->color, material.color);
    EXPECT_EQ(read->texture, material.texture);
    EXPECT_FLOAT_EQ(read->metallic, 0.75f);
    EXPECT_FLOAT_EQ(read->roughness, 0.125f);
    EXPECT_EQ(read->emissive, material.emissive);
    EXPECT_FLOAT_EQ(read->emissiveStrength, 2.0f);
    ASSERT_EQ(read->values.size(), 2u);
    // Values come back by name (in name order)
    EXPECT_EQ(read->values[0], (std::pair<std::string, float>{"u_Bands", 4.0f}));
    EXPECT_EQ(read->values[1], (std::pair<std::string, float>{"u_Brightness", 1.5f}));
}

// A hand-written file may leave things out: they keep their defaults
TEST_F(MaterialFileTest, MissingFieldsKeepTheirDefaults)
{
    const auto path = root_ / "Red.amat";
    std::ofstream(path) << R"({"color": [1, 0, 0, 1]})";
    const auto read = ReadMaterialFile(path);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(read->color, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_TRUE(read->shader.empty()) << "the standard shader";
    EXPECT_FLOAT_EQ(read->roughness, MaterialFile{}.roughness);
}

TEST_F(MaterialFileTest, RejectsWhatIsNotAMaterial)
{
    std::string error;
    EXPECT_FALSE(ReadMaterialFile(root_ / "missing.amat", &error).has_value());
    EXPECT_FALSE(error.empty());
    std::ofstream(root_ / "broken.amat") << "{ not json";
    EXPECT_FALSE(ReadMaterialFile(root_ / "broken.amat").has_value());
}

// New Material makes a plain material under assets/art/materials, numbered when the name is taken, with only the characters
// a file name can have
TEST_F(MaterialFileTest, AGameMakesNewMaterialsWithFreeNames)
{
    const Engine::Game::GameProject project(root_);
    EXPECT_EQ(project.CreateMaterial("Shiny Red"), "assets/art/materials/Shiny Red.amat");
    EXPECT_EQ(project.CreateMaterial("Shiny Red"), "assets/art/materials/Shiny Red 2.amat");
    EXPECT_EQ(project.CreateMaterial("a/b:c"), "assets/art/materials/abc.amat");
    EXPECT_EQ(project.CreateMaterial("???"), "assets/art/materials/Material.amat");
    const auto read = ReadMaterialFile(root_ / "assets/art/materials/Shiny Red.amat");
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(*read, MaterialFile{});
}
