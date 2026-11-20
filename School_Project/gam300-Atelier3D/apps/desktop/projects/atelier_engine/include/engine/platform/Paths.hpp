/* !
@file    	    Paths.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		26/09/2026
@brief
    Declares ExecutablePath and ExecutableDir, used to find games, templates and
    shaders next to the running executable.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Engine::Platform
{
    // Full path of the running executable, or an empty path if the OS cannot report it
    std::filesystem::path ExecutablePath();

    // Folder containing the running executable, whatever folder it was launched from
    // (falls back to the working directory if the OS cannot report it)
    std::filesystem::path ExecutableDir();

    // The folders a built-in shader is looked up in, in order: the source tree (using GAM300_ROOT_DIR, skipped
    // when it is empty), then the loose assets/shaders/Builtin and game/assets/shaders/Builtin folders next to
    // the executable.
    std::vector<std::filesystem::path> BuiltinShaderDirs();

    // Full path of a built-in shader (e.g. "Standard3D.vert"), or an empty string if it cannot be found. The
    // folders are tried in order, so running from the build folder picks up source edits first.
    std::string BuiltinShaderPath(const std::string& name,
                                  const std::vector<std::filesystem::path>& folders = BuiltinShaderDirs());
}
