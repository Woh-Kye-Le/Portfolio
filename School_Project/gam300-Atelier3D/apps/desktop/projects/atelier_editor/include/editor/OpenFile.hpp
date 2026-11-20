/* !
@file    	    OpenFile.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		02/11/2026
@brief
    Declares OpenWithDefaultApp: opens a file the way double-clicking it in
    the file manager would (a shader in the code editor, say).

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <filesystem>
#include <string>

namespace Editor
{
    // Opens path with the program the system uses for its kind of file. Returns false, with error saying why, when
    // it could not be opened.
    bool OpenWithDefaultApp(const std::filesystem::path& path, std::string& error);
}
