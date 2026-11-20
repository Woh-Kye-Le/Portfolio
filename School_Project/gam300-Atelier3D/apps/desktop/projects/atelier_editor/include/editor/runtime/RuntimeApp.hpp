/* !
@file    	    RuntimeApp.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/09/2026
@brief
    Declares RunGame, which plays a game project (or one of its scenes) in its
    own window without the editor UI. Used by --play and by built games.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <filesystem>

namespace Editor::Runtime
{
    // Plays a game in its own window, without the editor UI: the window's size and title come from the game's
    // configs/game.cfg. This is what a built game's executable runs. scene (relative to the game folder, e.g.
    // data/scenes/audio.json) plays that scene instead of the start scene. Returns the process exit code.
    int RunGame(const std::filesystem::path& gameDir, const std::filesystem::path& scene = {});
}
