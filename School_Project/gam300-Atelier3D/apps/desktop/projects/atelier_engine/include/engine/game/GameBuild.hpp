/* !
@file    	    GameBuild.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/09/2026
@brief
    Declares BuildGame, which makes a standalone copy of a game, and
    FindBuiltGame, which detects a built game next to the executable.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/game/GameProject.hpp"

#include <filesystem>

namespace Engine::Game
{
    // Folder next to a built game's executable that holds the game's data
    inline constexpr const char* kBuiltGameFolder = "game";

    struct BuildResult
    {
        std::filesystem::path outputDir;  // <outputRoot>/<game>
        std::filesystem::path executable; // <outputRoot>/<game>/<game>[.exe]
    };

    // Builds a standalone copy of a game that runs without the editor:
    //   <outputRoot>/<game>/<game>[.exe]   copy of runtimeExecutable, named after the game's folder
    //   <outputRoot>/<game>/game/          the game's configs, data, assets and scripts
    //   <outputRoot>/<game>/assets/fonts/  the HUD's fonts, when runtimeExecutable has them beside it
    // runtimeExecutable is an Atelier3D executable (AtelierPlayer, see FindPlayerExecutable, or the editor): when it
    // finds game/ next to itself it plays that game (see FindBuiltGame).
    // A previous build in the same folder is replaced. Throws std::runtime_error if the runtime is missing, if the
    // output folder exists but is not an earlier build, or if the output would be inside the game project.
    BuildResult BuildGame(const GameProject& project, const std::filesystem::path& runtimeExecutable,
                          const std::filesystem::path& outputRoot);

    // The game a built executable should play: <exeDir>/game if it is a game project, else an empty path
    std::filesystem::path FindBuiltGame(const std::filesystem::path& exeDir);

    // The player an export copies (AtelierPlayer: the game runtime without the editor). A Release (or Package) build's
    // first, from the build folder beside the editor's (bin/Release next to bin/Debug), so the game runs at full speed;
    // else the one beside the editor. Empty when there is none.
    std::filesystem::path FindPlayerExecutable(const std::filesystem::path& editorExecutable);

    // True for an executable in a Debug build's output folder (bin/Debug): slower, and it needs the debug runtime
    bool IsDebugBuildOutput(const std::filesystem::path& executable);
}
