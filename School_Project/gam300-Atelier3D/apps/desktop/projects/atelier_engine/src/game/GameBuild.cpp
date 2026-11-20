/* !
@file    	    GameBuild.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/09/2026
@brief
    Implements BuildGame (copies the runtime executable renamed after the game,
    its DLLs and the game data) and FindBuiltGame.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/game/GameBuild.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace Engine::Game
{
    namespace
    {
        // Marks a folder as made by BuildGame, so a rebuild may replace it (and never an unrelated folder)
        constexpr const char* kBuildMarker = ".atelier-build";

        bool IsInside(const std::filesystem::path& child, const std::filesystem::path& parent)
        {
            const auto relative =
                std::filesystem::weakly_canonical(child).lexically_relative(std::filesystem::weakly_canonical(parent));
            return !relative.empty() && *relative.begin() != "..";
        }

        // Copies the game project, leaving out the .gitkeep placeholders that only exist for Git
        void CopyGameData(const std::filesystem::path& from, const std::filesystem::path& to)
        {
            std::filesystem::create_directories(to);
            for (const auto& entry : std::filesystem::recursive_directory_iterator(from))
            {
                const auto target = to / entry.path().lexically_relative(from);
                if (entry.is_directory())
                {
                    std::filesystem::create_directories(target);
                }
                else if (entry.path().filename() != ".gitkeep")
                {
                    std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::overwrite_existing);
                }
            }
        }

        // True for shared libraries: Windows DLLs and Linux .so files (also versioned ones such as libfmod.so.14)
        bool IsSharedLibrary(const std::filesystem::path& file)
        {
            const std::string name = file.filename().string();
            return file.extension() == ".dll" || file.extension() == ".so" || name.find(".so.") != std::string::npos;
        }

        // Copies every runtime library that lives next to runtimeExecutable into targetDir.
        // These are placed at the editor's output by CMake POST_BUILD (GLEW, FMOD, LUA, ... on Windows; FMOD on Linux)
        // and the built standalone game needs identical siblings next to its renamed copy.
        void CopyRuntimeDlls(const std::filesystem::path& runtimeExecutable, const std::filesystem::path& targetDir)
        {
            const auto runtimeDir = runtimeExecutable.parent_path();
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(runtimeDir, ec))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }
                if (!IsSharedLibrary(entry.path()))
                {
                    continue;
                }
                const auto destination = targetDir / entry.path().filename();
                std::filesystem::copy_file(entry.path(), destination, std::filesystem::copy_options::overwrite_existing,
                                           ec);
            }
        }
    }

    // "Building" a game doesn't compile anything: games are just data, so we copy the player exe (renamed to the
    // game's name) plus the game folder. When that exe starts and finds game/ next to it, it plays the game.
    BuildResult BuildGame(const GameProject& project, const std::filesystem::path& runtimeExecutable,
                          const std::filesystem::path& outputRoot)
    {
        if (!std::filesystem::is_regular_file(runtimeExecutable))
        {
            throw std::runtime_error("Runtime executable not found: " + runtimeExecutable.string());
        }

        const auto gameName = project.ProjectDir().filename();
        BuildResult result;
        result.outputDir = outputRoot / gameName;
        result.executable = result.outputDir / (gameName.string() + runtimeExecutable.extension().string());

        if (IsInside(result.outputDir, project.ProjectDir()) || result.outputDir == project.ProjectDir())
        {
            throw std::runtime_error("The build folder cannot be inside the game project: " +
                                     result.outputDir.string());
        }
        if (IsInside(runtimeExecutable, result.outputDir))
        {
            throw std::runtime_error("The runtime executable is inside the build folder it would replace: " +
                                     runtimeExecutable.string());
        }

        if (std::filesystem::exists(result.outputDir))
        {
            if (!std::filesystem::exists(result.outputDir / kBuildMarker))
            {
                throw std::runtime_error("Not replacing " + result.outputDir.string() +
                                         ": the folder exists and was not made by a game build");
            }
            std::filesystem::remove_all(result.outputDir);
        }

        std::filesystem::create_directories(result.outputDir);
        std::ofstream(result.outputDir / kBuildMarker)
            << "Built by Atelier3D from " << project.ProjectDir().string() << "\n";

        std::filesystem::copy_file(runtimeExecutable, result.executable);
        // Make sure the copy can be run (on Windows this only affects the read-only flag, which is fine)
        std::error_code ec;
        std::filesystem::permissions(result.executable,
                                     std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
                                         std::filesystem::perms::others_exec,
                                     std::filesystem::perm_options::add, ec);

        CopyRuntimeDlls(runtimeExecutable, result.outputDir);
        // The HUD's fonts (without them it falls back to the system's)
        if (const auto fonts = runtimeExecutable.parent_path() / "assets" / "fonts";
            std::filesystem::is_directory(fonts))
        {
            std::filesystem::create_directories(result.outputDir / "assets");
            std::filesystem::copy(fonts, result.outputDir / "assets" / "fonts",
                                  std::filesystem::copy_options::recursive |
                                      std::filesystem::copy_options::overwrite_existing);
        }
        CopyGameData(project.ProjectDir(), result.outputDir / kBuiltGameFolder);
        // A game made with an older editor has older copies of the shaders (without skinning, say): the built game
        // gets this editor's
        CopyBuiltinShaders(result.outputDir / kBuiltGameFolder);
        return result;
    }

    // main() calls this on startup to decide between "open the editor" and "play the built game"
    std::filesystem::path FindBuiltGame(const std::filesystem::path& exeDir)
    {
        const auto game = exeDir / kBuiltGameFolder;
        return GameProject::IsGameProject(game) ? game : std::filesystem::path{};
    }

    std::filesystem::path FindPlayerExecutable(const std::filesystem::path& editorExecutable)
    {
        const auto dir = editorExecutable.parent_path();
        const std::string name = "AtelierPlayer" + editorExecutable.extension().string();
        // Multi-config builds put each configuration in a folder of its own under bin/
        for (const auto& candidate :
             {dir.parent_path() / "Release" / name, dir.parent_path() / "Package" / name, dir / name})
        {
            std::error_code ec;
            if (std::filesystem::is_regular_file(candidate, ec))
            {
                return candidate;
            }
        }
        return {};
    }

    bool IsDebugBuildOutput(const std::filesystem::path& executable)
    {
        return executable.parent_path().filename() == "Debug";
    }
}
