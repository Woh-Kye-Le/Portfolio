/* !
@file    	    EditorApp.cpp
@author  	    c.kojat@digipen.edu [80%]
         	    zhihenghenrey.hu@digipen.edu [20%]
@date    		22/09/2026
@brief
    Program entry point of AtelierEditor: opens the editor, or handles the
    command line (--new-game, --build-game, --play), or plays a built game found
    next to the executable. Sets up logging.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/Engine.hpp"
#include "engine/core/LogSinks.hpp"
#include "engine/core/StdCapture.hpp"

#include "editor/SceneEditor.hpp"
#include "editor/panels/console/ConsolePanel.hpp"
#include "editor/runtime/RuntimeApp.hpp"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace Editor
{
    // The editor window: an Application whose frames the docked SceneEditor draws
    class EditorApp final : public Engine::Core::Application
    {
    public:
        EditorApp(std::shared_ptr<Engine::Core::MemorySink> consoleSink, std::filesystem::path logDirectory)
            : Engine::Core::Application(
                  Engine::Core::ApplicationSpec{.window = Engine::Platform::WindowSpec{.width = 1280,
                                                                                       .height = 720,
                                                                                       .title = "Atelier3D",
                                                                                       .closeOnEscape = false,
                                                                                       .maximized = true},
                                                .maxFrames = 0}),
              editor_(std::move(consoleSink), std::move(logDirectory))
        {
        }

    protected:
        void OnInit() override
        {
            editor_.Init(GetWindow(), GetAudio(), GetEvents(), [this] { Close(); });
        }
        void OnUpdate(double deltaSeconds) override { editor_.Update(GetWindow(), deltaSeconds); }
        void OnShutdown() override { editor_.Shutdown(GetWindow()); }

    private:
        SceneEditor editor_;
    };

    // A path to a game project, or the name of one in projectsDir
    std::filesystem::path ResolveGame(const std::string& nameOrPath, const std::filesystem::path& projectsDir)
    {
        std::filesystem::path asPath = nameOrPath;
        if (Engine::Game::GameProject::IsGameProject(asPath))
        {
            return asPath;
        }
        return projectsDir / nameOrPath;
    }

    // "rpg" -> GameTemplate::Rpg etc. for --template (the milestone demos too); nullopt for a name we don't know
    std::optional<Engine::Game::GameTemplate> ParseTemplate(std::string_view name)
    {
        const auto& templates = Engine::Game::kAllGameTemplates;
        const auto found =
            std::find_if(templates.begin(), templates.end(), [name](Engine::Game::GameTemplate gameTemplate)
                         { return name == Engine::Game::ToString(gameTemplate); });
        if (found == templates.end())
        {
            return std::nullopt;
        }
        return *found;
    }

    // AtelierEditor --new-game <name> [--template empty|rpg|fps|racing|rendering|resources|ecs|audio|input|prefabs]
    //               [--projects-dir <dir>]
    // Creates a game project without opening a window; returns the process exit code
    int RunNewGameCommand(int argc, char** argv)
    {
        std::string name;
        auto gameTemplate = Engine::Game::GameTemplate::Empty;
        auto projectsDir = Engine::Game::GameProject::ProjectsRoot();

        for (int i = 1; i < argc; ++i)
        {
            const std::string_view arg = argv[i];
            const bool hasValue = i + 1 < argc;
            if (arg == "--new-game" && hasValue)
            {
                name = argv[++i];
            }
            else if (arg == "--template" && hasValue)
            {
                const auto parsed = ParseTemplate(argv[++i]);
                if (!parsed)
                {
                    std::cerr << "Unknown template '" << argv[i]
                              << "' (expected empty, rpg, fps or racing, or a demo: rendering, resources, ecs, "
                                 "audio, input, prefabs)\n";
                    return 1;
                }
                gameTemplate = *parsed;
            }
            else if (arg == "--projects-dir" && hasValue)
            {
                projectsDir = argv[++i];
            }
            else
            {
                std::cerr << "Usage: AtelierEditor --new-game <name> "
                             "[--template empty|rpg|fps|racing|<demo>] "
                             "[--projects-dir <dir>]\n";
                return 1;
            }
        }

        try
        {
            const auto project = Engine::Game::GameProject::Create(projectsDir, name, gameTemplate);
            std::cout << "Created game project " << project.ProjectDir().string() << "\n";
            return 0;
        }
        catch (const std::exception& e)
        {
            std::cerr << "ERROR: " << e.what() << "\n";
            return 1;
        }
    }

    // Opens the editor window; returns the process exit code
    int RunEditor()
    {
        // Everything logged or printed from here on goes to the terminal, <exe folder>/logs/editor_<date>_<time>.log
        // and the Console panel
        auto& logger = Engine::Core::Logger::Get();
        const auto logDirectory = Engine::Platform::ExecutableDir() / "logs";
        const auto consoleSink = std::make_shared<Engine::Core::MemorySink>(Panels::ConsolePanel::kMaxEntries);
        logger.AddSink(consoleSink);
        const auto fileSink = std::make_shared<Engine::Core::FileSink>(logDirectory, "editor");
        logger.AddSink(fileSink);

        Engine::Core::StdCapture stdCapture(logger);
        if (!stdCapture.Start())
        {
            LOG_WARN(Editor, "printf / std::cout output will not appear in the Console panel");
        }
        if (fileSink->IsOpen())
        {
            LOG_INFO(Editor, "Log file: {}", fileSink->FilePath().string());
        }

        int exitCode = 0;
        try
        {
            EditorApp app(consoleSink, logDirectory);
            app.Run();
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(Editor, "Editor stopped because of an unhandled error: {}", e.what());
            exitCode = 1;
        }

        // After the app is gone, so its last messages (audio shutdown) are included
        stdCapture.Stop();
        logger.Flush();
        logger.RemoveSink(consoleSink);
        logger.RemoveSink(fileSink);
        return exitCode;
    }

    // AtelierEditor --build-game <name|path> [--output <dir>] [--projects-dir <dir>]
    // Builds a standalone copy of a game (default output: builds/ next to the editor); returns the process exit code
    int RunBuildGameCommand(int argc, char** argv)
    {
        std::string game;
        auto outputDir = Engine::Platform::ExecutableDir() / "builds";
        auto projectsDir = Engine::Game::GameProject::ProjectsRoot();

        for (int i = 1; i < argc; ++i)
        {
            const std::string_view arg = argv[i];
            const bool hasValue = i + 1 < argc;
            if (arg == "--build-game" && hasValue)
            {
                game = argv[++i];
            }
            else if (arg == "--output" && hasValue)
            {
                outputDir = argv[++i];
            }
            else if (arg == "--projects-dir" && hasValue)
            {
                projectsDir = argv[++i];
            }
            else
            {
                std::cerr << "Usage: AtelierEditor --build-game <name|path> [--output <dir>] [--projects-dir <dir>]\n";
                return 1;
            }
        }

        const auto gameDir = ResolveGame(game, projectsDir);
        if (!Engine::Game::GameProject::IsGameProject(gameDir))
        {
            std::cerr << "ERROR: no game project '" << game << "' (looked for " << gameDir.string() << ")\n";
            return 1;
        }

        try
        {
            // The player (no editor in it), a Release build's if there is one; the editor itself without one
            std::filesystem::path player = Engine::Game::FindPlayerExecutable(Engine::Platform::ExecutablePath());
            if (player.empty())
            {
                player = Engine::Platform::ExecutablePath();
            }
            const auto result = Engine::Game::BuildGame(Engine::Game::GameProject(gameDir), player, outputDir);
            std::cout << "Built " << result.executable.string() << " (from " << player.string() << ")\n";
            if (Engine::Game::IsDebugBuildOutput(player))
            {
                std::cout << "Note: that is a Debug build. Build AtelierPlayer in Release for a fast game to share.\n";
            }
            return 0;
        }
        catch (const std::exception& e)
        {
            std::cerr << "ERROR: " << e.what() << "\n";
            return 1;
        }
    }

    // What gets printed when the command line arguments don't make sense
    void PrintUsage()
    {
        std::cerr << "Usage:\n"
                     "  AtelierEditor                                    open the editor\n"
                     "  AtelierEditor --new-game <name> "
                     "[--template empty|rpg|fps|racing|<demo>] "
                     "[--projects-dir <dir>]\n"
                     "  AtelierEditor --build-game <name|path> [--output <dir>] [--projects-dir <dir>]\n"
                     "  AtelierEditor --play <name|path> [--scene <file>] play a game (or one of its scenes, e.g.\n"
                     "                                                   data/scenes/audio.json) in its own window\n";
    }
}

int main(int argc, char** argv)
{
    try
    {
        // Any arguments mean a command-line action; no arguments open the editor
        if (argc > 1)
        {
            const std::string_view command = argv[1];
            if (command == "--new-game")
            {
                return Editor::RunNewGameCommand(argc, argv);
            }
            if (command == "--build-game")
            {
                return Editor::RunBuildGameCommand(argc, argv);
            }
            if (command == "--play" && (argc == 3 || (argc == 5 && std::string_view(argv[3]) == "--scene")))
            {
                return Editor::Runtime::RunGame(Editor::ResolveGame(argv[2], Engine::Game::GameProject::ProjectsRoot()),
                                                argc == 5 ? argv[4] : "");
            }
            Editor::PrintUsage();
            return 1;
        }

        // A built game: BuildGame copied this executable next to a game/ folder, so play that game instead of editing
        if (const auto builtGame = Engine::Game::FindBuiltGame(Engine::Platform::ExecutableDir()); !builtGame.empty())
        {
            return Editor::Runtime::RunGame(builtGame);
        }

        return Editor::RunEditor();
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Fatal error: %s\n", e.what());
    }
    catch (...)
    {
        std::fprintf(stderr, "Fatal error\n");
    }
    return 1;
}
