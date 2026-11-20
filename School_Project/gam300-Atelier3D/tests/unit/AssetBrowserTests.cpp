#include "editor/panels/assets/AssetCatalog.hpp"
#include "editor/panels/assets/AssetFavorites.hpp"

#include "engine/assets/Resources.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

using Editor::Panels::AssetEntry;
using Editor::Panels::AssetFavorites;
using Editor::Panels::AssetKind;
using Editor::Panels::AssetKindFilter;
using Editor::Panels::IsFiltering;
using Editor::Panels::KindOf;
using Editor::Panels::MatchesSearch;
using Editor::Panels::PassesKindFilter;
using Editor::Panels::ScanAssets;

namespace
{
    // The sample game committed in the repo: its demos are built from shapes, so it ships one material and no models,
    // pictures or sounds (only the empty sound bank's sounds.json, which is not an asset the browser lists)
    const std::filesystem::path kSampleGame = ATELIER_SAMPLE_GAME_DIR;
    // A small triangle, as a model file
    constexpr const char* kTriangleObj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";

    AssetKindFilter Only(AssetKind kind)
    {
        AssetKindFilter filter{};
        filter[static_cast<std::size_t>(kind)] = true;
        return filter;
    }

    // What the Assets grid shows: the assets matching the search whose kind passes the type buttons
    std::vector<AssetEntry> Visible(const std::vector<AssetEntry>& assets, std::string_view search,
                                    const AssetKindFilter& filter = {})
    {
        std::vector<AssetEntry> visible;
        std::copy_if(assets.begin(), assets.end(), std::back_inserter(visible), [&](const AssetEntry& asset)
                     { return MatchesSearch(asset, search) && PassesKindFilter(asset.kind, filter); });
        return visible;
    }

    std::vector<std::string> Names(const std::vector<AssetEntry>& assets)
    {
        std::vector<std::string> names;
        for (const AssetEntry& asset : assets)
        {
            names.push_back(asset.name);
        }
        return names;
    }

    AssetEntry Entry(const std::string& path, AssetKind kind)
    {
        return {path, std::filesystem::path(path).filename().string(), kind};
    }

    // Gives each test an empty game folder
    class AssetBrowserTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            gameDir_ = std::filesystem::temp_directory_path() / "atelier_tests" / "asset_browser" / info->name();
            std::filesystem::remove_all(gameDir_);
            std::filesystem::create_directories(gameDir_);
        }

        void TearDown() override { std::filesystem::remove_all(gameDir_); }

        void WriteFile(const std::filesystem::path& relativePath, const std::string& contents = "x") const
        {
            const auto path = gameDir_ / relativePath;
            std::filesystem::create_directories(path.parent_path());
            std::ofstream(path, std::ios::binary) << contents;
        }

        std::filesystem::path gameDir_;
    };
}

// ---------------------------------------------------------------- Catalog

TEST(AssetCatalogTest, SampleGameListsOnlyItsMaterial)
{
    const auto assets = ScanAssets(kSampleGame);

    // The demos draw built-in shapes: the one material is all there is (the sound bank config sits in assets/audio
    // but is not an asset)
    ASSERT_EQ(assets.size(), 1u);
    EXPECT_EQ(assets[0].name, "Car Paint.amat");
    EXPECT_EQ(assets[0].kind, AssetKind::Material);
}

TEST_F(AssetBrowserTest, AssetsAreSortedByNameIgnoringCase)
{
    for (const char* file :
         {"assets/art/textures/FPS/Arena_Floor.png", "assets/art/textures/FPS/Arena_Metal.png",
          "assets/art/textures/FPS/Arena_Wall.png", "assets/art/models/arrow/arrow_crossbow.amodel",
          "assets/audio/bandit_hurt.wav", "assets/art/models/Barbarian/Barbarian.glb", "assets/audio/beep.wav",
          "assets/art/materials/Car Paint.amat", "assets/art/models/car/car.fbx", "assets/audio/piano_bgm.mp3",
          "assets/audio/PianoSFX_01.wav", "assets/audio/sounds.json"})
    {
        WriteFile(file);
    }
    const auto names = Names(ScanAssets(gameDir_));

    ASSERT_GE(names.size(), 9u);
    EXPECT_EQ(names[0], "Arena_Floor.png");
    EXPECT_EQ(names[1], "Arena_Metal.png");
    EXPECT_EQ(names[2], "Arena_Wall.png");
    EXPECT_EQ(names[3], "arrow_crossbow.amodel");
    EXPECT_EQ(names[4], "bandit_hurt.wav");
    EXPECT_EQ(names[5], "Barbarian.glb");
    EXPECT_EQ(names[6], "beep.wav");
    const auto paint = std::find(names.begin(), names.end(), "Car Paint.amat");
    ASSERT_NE(paint, names.end());
    EXPECT_EQ(*(paint + 1), "car.fbx"); // a space comes before a dot
    // "piano_bgm" and "PianoSFX_01" compare as "piano_..." < "pianos...", whatever their case
    const auto bgm = std::find(names.begin(), names.end(), "piano_bgm.mp3");
    const auto sfx = std::find(names.begin(), names.end(), "PianoSFX_01.wav");
    ASSERT_NE(bgm, names.end());
    ASSERT_NE(sfx, names.end());
    EXPECT_LT(bgm, sfx);
}

TEST_F(AssetBrowserTest, PathsAreRelativeToTheGameFolderWithForwardSlashes)
{
    WriteFile("assets/art/models/crate/crate.obj", kTriangleObj);
    WriteFile("assets/art/textures/crate/crate.png");
    WriteFile("assets/audio/beep.wav");
    const auto assets = ScanAssets(gameDir_);

    const auto crate =
        std::find_if(assets.begin(), assets.end(), [](const AssetEntry& asset) { return asset.name == "crate.obj"; });
    ASSERT_NE(crate, assets.end());
    EXPECT_EQ(crate->path, "assets/art/models/crate/crate.obj");
    EXPECT_EQ(assets.size(), 3u);
    for (const AssetEntry& asset : assets)
    {
        EXPECT_EQ(asset.path.find('\\'), std::string::npos) << asset.path;
        EXPECT_EQ(asset.path.rfind("assets/", 0), 0u) << asset.path;
        EXPECT_TRUE(std::filesystem::is_regular_file(gameDir_ / asset.path)) << asset.path;
    }
}

TEST(AssetCatalogTest, KindComesFromTheExtensionInAnyCase)
{
    EXPECT_EQ(KindOf("crate.obj"), AssetKind::Model);
    EXPECT_EQ(KindOf("ship.FBX"), AssetKind::Model);
    EXPECT_EQ(KindOf("crate.Png"), AssetKind::Texture);
    EXPECT_EQ(KindOf("photo.jpeg"), AssetKind::Texture);
    EXPECT_EQ(KindOf("beep.WAV"), AssetKind::Audio);
    EXPECT_EQ(KindOf("click.ogg"), AssetKind::Audio);
    EXPECT_EQ(KindOf("music.mp3"), AssetKind::Audio);
    EXPECT_FALSE(KindOf("sounds.json").has_value());
    EXPECT_EQ(KindOf("Toon.vert"), AssetKind::Shader); // its .frag goes with it, and is not listed
    EXPECT_FALSE(KindOf("Toon.frag").has_value());
    EXPECT_EQ(KindOf("Shiny.amat"), AssetKind::Material);
    EXPECT_FALSE(KindOf("README").has_value());
}

TEST_F(AssetBrowserTest, ScanFindsSubfoldersAndSkipsHiddenAndOtherFiles)
{
    WriteFile("assets/models/props/barrel.fbx");
    WriteFile("assets/textures/Wood.JPG");
    WriteFile("assets/textures/.hidden.png");
    WriteFile("assets/notes.txt");
    WriteFile("data/scenes/main.json");

    const auto assets = ScanAssets(gameDir_);

    EXPECT_EQ(Names(assets), (std::vector<std::string>{"barrel.fbx", "Wood.JPG"}));
    ASSERT_EQ(assets.size(), 2u);
    EXPECT_EQ(assets[0].path, "assets/models/props/barrel.fbx");
    EXPECT_EQ(assets[0].kind, AssetKind::Model);
    EXPECT_EQ(assets[1].kind, AssetKind::Texture);
}

TEST_F(AssetBrowserTest, GameWithoutAnAssetsFolderHasNoAssets)
{
    EXPECT_TRUE(ScanAssets(gameDir_).empty());
    EXPECT_TRUE(ScanAssets(gameDir_ / "does_not_exist").empty());
    EXPECT_TRUE(Editor::Panels::ScanFolders(gameDir_).empty());
}

// The folder tree, as in Unity's Project window: assets and every folder under it (empty ones too), sorted ignoring
// case; hidden folders and what is in them are left out. Each asset knows the folder it is in.
TEST_F(AssetBrowserTest, FoldersAreListedLikeTheProjectWindow)
{
    WriteFile("assets/models/props/barrel.fbx");
    WriteFile("assets/Textures/wood.png");
    WriteFile("assets/.cache/inside/hidden.png");
    std::filesystem::create_directories(gameDir_ / "assets" / "empty");

    EXPECT_EQ(Editor::Panels::ScanFolders(gameDir_),
              (std::vector<std::string>{"assets", "assets/empty", "assets/models", "assets/models/props",
                                        "assets/Textures"}));
    const auto assets = ScanAssets(gameDir_);
    ASSERT_EQ(Names(assets), (std::vector<std::string>{"barrel.fbx", "wood.png"})) << "nothing from the hidden folder";
    EXPECT_EQ(Editor::Panels::FolderOf(assets[0]), "assets/models/props");
    EXPECT_EQ(Editor::Panels::FolderOf(assets[1]), "assets/Textures");
    EXPECT_EQ(Editor::Panels::ParentFolder("assets/models/props"), "assets/models");
    EXPECT_EQ(Editor::Panels::ParentFolder("assets"), "");
}

// ---------------------------------------------------------------- Search

TEST(AssetSearchTest, MatchesAnyPartOfTheFileNameIgnoringCase)
{
    const AssetEntry piano = Entry("assets/audio/PianoSFX_01.wav", AssetKind::Audio);

    EXPECT_TRUE(MatchesSearch(piano, "sfx"));
    EXPECT_TRUE(MatchesSearch(piano, "PIANO"));
    EXPECT_TRUE(MatchesSearch(piano, "01.w"));
    EXPECT_TRUE(MatchesSearch(piano, ".WAV"));
    EXPECT_TRUE(MatchesSearch(piano, "PianoSFX_01.wav"));
    EXPECT_FALSE(MatchesSearch(piano, "drum"));
    EXPECT_FALSE(MatchesSearch(piano, "PianoSFX_01.wav2"));
}

TEST(AssetSearchTest, EmptyOrBlankSearchMatchesEverything)
{
    const AssetEntry crate = Entry("assets/models/crate.obj", AssetKind::Model);

    EXPECT_TRUE(MatchesSearch(crate, ""));
    EXPECT_TRUE(MatchesSearch(crate, "   "));
    EXPECT_TRUE(MatchesSearch(crate, "\t"));
}

TEST(AssetSearchTest, SpacesAroundTheSearchAreIgnoredButNotInside)
{
    const AssetEntry crate = Entry("assets/models/crate.obj", AssetKind::Model);

    EXPECT_TRUE(MatchesSearch(crate, "  crate "));
    EXPECT_FALSE(MatchesSearch(crate, "cr ate"));
}

TEST(AssetSearchTest, SearchLooksAtTheFileNameNotItsFolders)
{
    const AssetEntry crate = Entry("assets/models/crate.obj", AssetKind::Model);

    EXPECT_FALSE(MatchesSearch(crate, "models"));
    EXPECT_FALSE(MatchesSearch(crate, "assets/"));
}

TEST_F(AssetBrowserTest, CrateFindsTheCrateModelsAndTexture)
{
    for (const char* file : {"assets/art/models/crate/crate.obj", "assets/art/textures/crate/crate.png",
                             "assets/art/models/Prop_Crate_1/Prop_Crate_1.obj",
                             "assets/art/models/Prop_Crate_1_Open/Prop_Crate_1_Open.obj", "assets/audio/piano_bgm.mp3",
                             "assets/audio/PianoSFX_01.wav", "assets/audio/PianoSFX_02.wav", "assets/audio/beep.wav"})
    {
        WriteFile(file);
    }
    EXPECT_EQ(Names(Visible(ScanAssets(gameDir_), "crate")),
              (std::vector<std::string>{"crate.obj", "crate.png", "Prop_Crate_1.obj", "Prop_Crate_1_Open.obj"}));
    EXPECT_EQ(Visible(ScanAssets(gameDir_), "piano").size(), 3u); // piano_bgm.mp3, PianoSFX_01 and _02
}

// ---------------------------------------------------------------- Type filter

TEST(AssetFilterTest, NoButtonOnShowsEveryKind)
{
    const AssetKindFilter none{};

    EXPECT_FALSE(IsFiltering(none));
    EXPECT_TRUE(PassesKindFilter(AssetKind::Model, none));
    EXPECT_TRUE(PassesKindFilter(AssetKind::Texture, none));
    EXPECT_TRUE(PassesKindFilter(AssetKind::Audio, none));
}

TEST(AssetFilterTest, OneButtonShowsOnlyThatKind)
{
    const AssetKindFilter models = Only(AssetKind::Model);

    EXPECT_TRUE(IsFiltering(models));
    EXPECT_TRUE(PassesKindFilter(AssetKind::Model, models));
    EXPECT_FALSE(PassesKindFilter(AssetKind::Texture, models));
    EXPECT_FALSE(PassesKindFilter(AssetKind::Audio, models));
}

TEST(AssetFilterTest, ButtonsCombine)
{
    AssetKindFilter filter = Only(AssetKind::Model);
    filter[static_cast<std::size_t>(AssetKind::Audio)] = true;

    EXPECT_TRUE(PassesKindFilter(AssetKind::Model, filter));
    EXPECT_TRUE(PassesKindFilter(AssetKind::Audio, filter));
    EXPECT_FALSE(PassesKindFilter(AssetKind::Texture, filter));
}

TEST_F(AssetBrowserTest, FilterByType)
{
    for (const char* file : {"assets/art/models/crate/crate.obj", "assets/art/models/car/car.fbx",
                             "assets/art/textures/crate/crate.png", "assets/art/textures/skies/night_sky.png",
                             "assets/art/sprites/ui/ui_button.png", "assets/audio/beep.wav",
                             "assets/audio/piano_bgm.mp3", "assets/art/materials/Car Paint.amat"})
    {
        WriteFile(file);
    }
    const auto assets = ScanAssets(gameDir_);

    EXPECT_EQ(Names(Visible(assets, "", Only(AssetKind::Model))), (std::vector<std::string>{"car.fbx", "crate.obj"}));
    EXPECT_EQ(Names(Visible(assets, "", Only(AssetKind::Texture))),
              (std::vector<std::string>{"crate.png", "night_sky.png", "ui_button.png"}));
    EXPECT_EQ(Visible(assets, "", Only(AssetKind::Audio)).size(), 2u);
    EXPECT_EQ(Visible(assets, "", Only(AssetKind::Material)).size(), 1u);
    EXPECT_TRUE(Visible(assets, "crate", Only(AssetKind::Audio)).empty());
}

// ---------------------------------------------------------------- Using what the browser finds

// Search "crate", filter to Models only: crate.obj first, whose path the editor puts in the selected object's
// Renderable. The file must load as a real model (not the placeholder cube a missing path gets).
TEST_F(AssetBrowserTest, SearchCrateAndFilterToModelsGivesALoadableCrateModel)
{
    WriteFile("assets/art/models/crate/crate.obj", kTriangleObj);
    WriteFile("assets/art/models/Prop_Crate_1/Prop_Crate_1.obj", kTriangleObj);
    WriteFile("assets/art/textures/crate/crate.png");
    const auto visible = Visible(ScanAssets(gameDir_), "crate", Only(AssetKind::Model));

    ASSERT_EQ(visible.size(), 2u);
    EXPECT_EQ(visible[0].name, "crate.obj");
    EXPECT_EQ(visible[0].path, "assets/art/models/crate/crate.obj");
    const auto mesh = Engine::Assets::LoadMeshFromFile(gameDir_ / visible[0].path);
    ASSERT_TRUE(mesh.has_value());
    EXPECT_FALSE(mesh->indices.empty());
    EXPECT_EQ(mesh->positions.size() % 3, 0u);
}

TEST(AssetBrowserDemoTest, EveryListedImageAndModelLoadsForItsPreview)
{
    for (const AssetEntry& asset : ScanAssets(kSampleGame))
    {
        const auto file = kSampleGame / asset.path;
        if (asset.kind == AssetKind::Texture)
        {
            const auto image = Engine::Assets::LoadTextureFromFile(file);
            ASSERT_TRUE(image.has_value()) << asset.path;
            EXPECT_GT(image->width, 0) << asset.path;
            EXPECT_EQ(image->rgba.size(), static_cast<std::size_t>(image->width) * image->height * 4) << asset.path;
        }
        else if (asset.kind == AssetKind::Model)
        {
            // A file of animations alone (a reload for the first-person arms) has nothing of its own to show
            const auto model = Engine::Assets::LoadModelFromFile(file);
            const bool animationsOnly = model && model->meshes.empty() && !model->animations.empty();
            EXPECT_TRUE(animationsOnly || Engine::Assets::LoadMeshFromFile(file).has_value()) << asset.path;
        }
    }
}

// ---------------------------------------------------------------- Favorites

TEST(AssetFavoritesTest, ToggleAddsThenRemoves)
{
    AssetFavorites favorites;

    favorites.Toggle("assets/models/crate.obj");
    EXPECT_TRUE(favorites.Contains("assets/models/crate.obj"));
    EXPECT_EQ(favorites.Count(), 1u);

    favorites.Toggle("assets/models/crate.obj");
    EXPECT_FALSE(favorites.Contains("assets/models/crate.obj"));
    EXPECT_EQ(favorites.Count(), 0u);
}

TEST_F(AssetBrowserTest, GameWithoutAFavoritesFileHasNone)
{
    AssetFavorites favorites;
    favorites.Toggle("assets/left/over.png"); // from the game opened before

    std::string error;
    EXPECT_TRUE(favorites.Load(gameDir_, error));
    EXPECT_EQ(favorites.Count(), 0u);
    EXPECT_TRUE(error.empty());
}

TEST_F(AssetBrowserTest, FavoritesAreSavedInTheGameAndReadBack)
{
    AssetFavorites favorites;
    favorites.Toggle("assets/models/crate.obj");
    favorites.Toggle("assets/audio/beep.wav");

    std::string error;
    ASSERT_TRUE(favorites.Save(gameDir_, error)) << error; // also makes the data/ folder
    EXPECT_TRUE(std::filesystem::is_regular_file(gameDir_ / "data" / "assets.editor.favorites"));

    AssetFavorites reopened;
    ASSERT_TRUE(reopened.Load(gameDir_, error)) << error;
    EXPECT_EQ(reopened.Count(), 2u);
    EXPECT_TRUE(reopened.Contains("assets/models/crate.obj"));
    EXPECT_TRUE(reopened.Contains("assets/audio/beep.wav"));
}

TEST_F(AssetBrowserTest, FavoritesFileIsVersionedAndSorted)
{
    AssetFavorites favorites;
    favorites.Toggle("assets/textures/crate.png");
    favorites.Toggle("assets/audio/beep.wav");
    std::string error;
    ASSERT_TRUE(favorites.Save(gameDir_, error)) << error;

    std::ifstream file(gameDir_ / AssetFavorites::kFile);
    const json saved = json::parse(file);
    EXPECT_EQ(saved.at("version"), 1);
    EXPECT_EQ(saved.at("favorites"), json({"assets/audio/beep.wav", "assets/textures/crate.png"}));
}

TEST_F(AssetBrowserTest, UnreadableFavoritesFileIsReportedAndGivesNone)
{
    WriteFile(AssetFavorites::kFile, "{ not json");
    AssetFavorites favorites;
    favorites.Toggle("assets/left/over.png");

    std::string error;
    EXPECT_FALSE(favorites.Load(gameDir_, error));
    EXPECT_NE(error.find("assets.editor.favorites"), std::string::npos) << error;
    EXPECT_EQ(favorites.Count(), 0u);
}

TEST_F(AssetBrowserTest, FavoritesFromANewerEditorAreNotRead)
{
    WriteFile(AssetFavorites::kFile, R"({ "version": 2, "favorites": ["assets/models/crate.obj"] })");

    AssetFavorites favorites;
    std::string error;
    EXPECT_FALSE(favorites.Load(gameDir_, error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(favorites.Count(), 0u);
}

TEST_F(AssetBrowserTest, EntriesThatAreNotPathsAreSkipped)
{
    WriteFile(AssetFavorites::kFile,
              R"({ "version": 1, "favorites": ["assets/models/crate.obj", 3, null, "", {"path": "x"}] })");

    AssetFavorites favorites;
    std::string error;
    ASSERT_TRUE(favorites.Load(gameDir_, error)) << error;
    EXPECT_EQ(favorites.Count(), 1u);
    EXPECT_TRUE(favorites.Contains("assets/models/crate.obj"));
}

// A model's folder keeps the .fbx or .obj it was converted from; the Assets window lists the .amodel only, so each model
// shows once.
TEST(AssetCatalogTest, TheOriginalKeptBesideAnAmodelIsNotListed)
{
    const auto game = std::filesystem::temp_directory_path() / "atelier_tests" / "catalog_originals";
    std::filesystem::remove_all(game);
    for (const char* file : {"assets/art/models/car/car.amodel", "assets/art/models/car/car.fbx",
                             "assets/art/textures/car/paint.png", "assets/art/models/crate/crate.obj"})
    {
        std::filesystem::create_directories((game / file).parent_path());
        std::ofstream(game / file) << "x";
    }
    EXPECT_EQ(Names(ScanAssets(game)), (std::vector<std::string>{"car.amodel", "crate.obj", "paint.png"}));
    std::filesystem::remove_all(game);
}
