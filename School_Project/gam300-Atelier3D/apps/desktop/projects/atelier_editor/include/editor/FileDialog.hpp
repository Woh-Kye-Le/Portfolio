/* !
@file    	    FileDialog.hpp
@author  	    k.thantwin@digipen.edu [100%]
@date    		06/11/2026
@brief
    Declares PickFiles, the system's Open dialog for choosing files to bring
    into a game (Windows only), and the filters it shows.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Editor
{
    // One choice in the dialog's file type list: its name and the extensions it shows (".wav", ".ogg", ...)
    struct FileFilter
    {
        std::string name;
        std::vector<std::string_view> extensions;
    };

    /*!
    \brief   Whether PickFiles can show a dialog here (Windows); elsewhere callers fall back to typing a path.
    \return  True on Windows.
    */
    bool CanPickFiles();

    /*!
    \brief   Shows the system's Open dialog and waits for the user to choose files or cancel.
    \param   title The dialog's title.
    \param   filters The file type choices, the first one picked to start with; "All files" is added after them.
    \param   many Whether more than one file can be chosen.
    \return  The chosen files; none when cancelled, on failure, or where there is no dialog.
    */
    std::vector<std::filesystem::path> PickFiles(const std::string& title, const std::vector<FileFilter>& filters,
                                                 bool many = true);
}
