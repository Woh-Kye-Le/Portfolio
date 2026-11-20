#include "engine/game/GameBuild.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

using Engine::Game::BuildGame;
using Engine::Game::FindBuiltGame;
using Engine::Game::GameProject;
using Engine::Game::GameTemplate;

namespace
{
    // Each test gets: projects/<game>, a fake runtime executable, and an empty output folder
    class GameBuildTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
            root_ = std::filesystem::temp_directory_path() / "atelier_build_tests" / info->name();
            std::filesystem::remove_all(root_);
            std::filesystem::create_directories(root_ / "bin");

            runtime_ = root_ / "bin" / "AtelierEditor.exe";
            std::ofstream(runtime_, std::ios::binary) << "runtime-binary";
            output_ = root_ / "builds";
        }

        void TearDown() override { std::filesystem::remove_all(root_); }

        GameProject MakeGame(const std::string& name) const
        {
            return GameProject::Create(root_ / "projects", name, GameTemplate::Input, ATELIER_SAMPLE_GAME_DIR);
        }

        static std::string ReadAll(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            std::stringstream text;
            text << file.rdbuf();
            return text.str();
        }

        std::filesystem::path root_;
        std::filesystem::path runtime_;
        std::filesystem::path output_;
    };
}

// A build is the runtime exe renamed after the game plus a game/ folder with its data (no .gitkeep files).
TEST_F(GameBuildTest, BuildCopiesRuntimeNamedAfterGameAndGameData)
{
    const auto result = BuildGame(MakeGame("space_game"), runtime_, output_);

    EXPECT_EQ(result.outputDir, output_ / "space_game");
    EXPECT_EQ(result.executable, output_ / "space_game" / "space_game.exe"); // keeps the runtime's extension
    EXPECT_EQ(ReadAll(result.executable), "runtime-binary");

    const auto game = result.outputDir / "game";
    EXPECT_TRUE(GameProject::IsGameProject(game));
    EXPECT_TRUE(std::filesystem::exists(game / "data/scenes/main.json"));
    EXPECT_TRUE(std::filesystem::is_directory(game / "assets/art/models"));
    EXPECT_FALSE(std::filesystem::exists(game / "assets/art/models/.gitkeep")); // Git placeholders are left out
}

// The built copy reads back the same settings and start scene.
TEST_F(GameBuildTest, BuiltGameLoadsTheSameSettingsAndScene)
{
    const auto project = MakeGame("same_game");
    const auto result = BuildGame(project, runtime_, output_);

    const GameProject built(result.outputDir / "game");
    EXPECT_EQ(built.Settings().name, project.Settings().name);
    EXPECT_EQ(built.Settings().windowWidth, project.Settings().windowWidth);
    EXPECT_EQ(built.LoadStartScene(), project.LoadStartScene()); // the Input template scene
}

// FindBuiltGame finds game/ next to a built exe, and nothing next to the editor itself.
TEST_F(GameBuildTest, FindBuiltGameFindsGameNextToExecutable)
{
    const auto result = BuildGame(MakeGame("found_game"), runtime_, output_);

    EXPECT_EQ(FindBuiltGame(result.outputDir), result.outputDir / "game");
    EXPECT_TRUE(FindBuiltGame(root_ / "bin").empty()); // the editor's own folder has no built game
}

// The export uses the player (no editor in it): a Release build's before the one beside a Debug editor, none without
// one; and the fonts beside the player go along
TEST_F(GameBuildTest, ExportPrefersTheReleasePlayer)
{
    const auto bin = root_ / "bin";
    const auto editor = bin / "Debug" / "AtelierEditor.exe";
    std::filesystem::create_directories(editor.parent_path());
    std::ofstream(editor) << "editor";
    EXPECT_TRUE(Engine::Game::FindPlayerExecutable(editor).empty()) << "no player built";

    std::ofstream(bin / "Debug" / "AtelierPlayer.exe") << "debug player";
    EXPECT_EQ(Engine::Game::FindPlayerExecutable(editor), bin / "Debug" / "AtelierPlayer.exe");
    EXPECT_TRUE(Engine::Game::IsDebugBuildOutput(bin / "Debug" / "AtelierPlayer.exe"));

    std::filesystem::create_directories(bin / "Release" / "assets" / "fonts");
    std::ofstream(bin / "Release" / "AtelierPlayer.exe") << "release player";
    std::ofstream(bin / "Release" / "assets" / "fonts" / "Hud.ttf") << "font";
    const auto player = Engine::Game::FindPlayerExecutable(editor);
    EXPECT_EQ(player, bin / "Release" / "AtelierPlayer.exe");
    EXPECT_FALSE(Engine::Game::IsDebugBuildOutput(player));

    const auto result = BuildGame(MakeGame("fast_game"), player, output_);
    EXPECT_EQ(ReadAll(result.executable), "release player");
    EXPECT_TRUE(std::filesystem::is_regular_file(result.outputDir / "assets" / "fonts" / "Hud.ttf"));
}

// Building again replaces the old build completely (a leftover file is gone).
TEST_F(GameBuildTest, RebuildReplacesPreviousBuild)
{
    const auto project = MakeGame("rebuilt_game");
    const auto first = BuildGame(project, runtime_, output_);
    std::ofstream(first.outputDir / "game" / "stale.txt") << "old";

    const auto second = BuildGame(project, runtime_, output_);

    EXPECT_TRUE(std::filesystem::exists(second.executable));
    EXPECT_FALSE(std::filesystem::exists(second.outputDir / "game" / "stale.txt"));
}

// A folder with the same name that we did not make is never deleted - the build throws instead.
TEST_F(GameBuildTest, RefusesToReplaceFolderThatIsNotABuild)
{
    std::filesystem::create_directories(output_ / "my_game");
    std::ofstream(output_ / "my_game" / "important.txt") << "keep me";

    EXPECT_THROW(BuildGame(MakeGame("my_game"), runtime_, output_), std::runtime_error);
    EXPECT_EQ(ReadAll(output_ / "my_game" / "important.txt"), "keep me");
}

// Building into the game's own folder would copy the game into itself, so it is refused.
TEST_F(GameBuildTest, RefusesOutputInsideTheGameProject)
{
    const auto project = MakeGame("self_game");

    EXPECT_THROW(BuildGame(project, runtime_, project.ProjectDir() / "builds"), std::runtime_error);
    EXPECT_FALSE(std::filesystem::exists(project.ProjectDir() / "builds"));
}

// No runtime exe to copy: throws and creates nothing.
TEST_F(GameBuildTest, MissingRuntimeThrows)
{
    EXPECT_THROW(BuildGame(MakeGame("no_runtime"), root_ / "bin" / "missing.exe", output_), std::runtime_error);
    EXPECT_FALSE(std::filesystem::exists(output_ / "no_runtime"));
}

#ifndef _WIN32
// The copied exe keeps its execute permission (needed on Linux).
TEST_F(GameBuildTest, BuiltExecutableIsRunnableOnLinux)
{
    const auto result = BuildGame(MakeGame("linux_game"), runtime_, output_);

    const auto perms = std::filesystem::status(result.executable).permissions();
    EXPECT_NE(perms & std::filesystem::perms::owner_exec, std::filesystem::perms::none);
}
#endif
