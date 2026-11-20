/* !
@file    	    PlayerMain.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		02/11/2026
@brief
    Entry point of AtelierPlayer, the executable an exported game runs: it plays the game folder next to it (or the
    one given with --play), with none of the editor.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "editor/runtime/RuntimeApp.hpp"
#include "engine/game/GameBuild.hpp"
#include "engine/platform/Paths.hpp"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <string_view>

int main(int argc, char** argv)
{
    try
    {
        // An exported game: its game/ folder sits next to this executable
        if (argc == 1)
        {
            const auto game = Engine::Game::FindBuiltGame(Engine::Platform::ExecutableDir());
            if (game.empty())
            {
                std::fprintf(stderr, "No game next to this player (expected a game folder beside it)\n");
                return 1;
            }
            return Editor::Runtime::RunGame(game);
        }
        // Trying a game folder by hand: --play <game folder> [--scene <scene file in it>]
        if (std::string_view(argv[1]) == "--play" &&
            (argc == 3 || (argc == 5 && std::string_view(argv[3]) == "--scene")))
        {
            return Editor::Runtime::RunGame(argv[2], argc == 5 ? argv[4] : "");
        }
        std::fprintf(stderr, "Usage: AtelierPlayer [--play <game folder> [--scene <scene file>]]\n");
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
