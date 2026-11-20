/* !
@file    	    RecycleBin.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		01/11/2026
@brief
    Declares MoveToRecycleBin: deleting a game or a scene so it can still be
    brought back.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <filesystem>
#include <string>

namespace Editor
{
    // Deletes a file or a folder (with everything in it) the way the desktop does: into the Recycle Bin on Windows.
    // Where there is none (or it refuses), it is moved into fallbackFolder instead (made if needed), under its own
    // name, so nothing is lost; with no fallbackFolder it is left where it is. Returns false, with the reason in
    // error, if it could be neither.
    bool MoveToRecycleBin(const std::filesystem::path& path, const std::filesystem::path& fallbackFolder,
                          std::string& error);
}
