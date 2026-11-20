/* !
@file               ZipReader.hpp
@author             w.kyele@digipen.edu [100%]
@date               30/10/2026
@brief
    Unpacks a .zip file into a folder, so a downloaded model pack (the model,
    its .mtl and its textures in one archive) can be converted in one go.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Standard Library Headers
#include <filesystem>
#include <string>
#include <vector>

namespace Atelier::Tool
{
    /*!***********************************************************************
    @brief          Extracts every file of a zip archive into a folder, keeping the folders inside the archive.
                    Stored and deflated entries are read (what Windows, macOS and download sites produce); an
                    entry that is encrypted, uses another compression, or would land outside the folder ("../x",
                    "C:/x") is skipped with a warning. ZIP64 archives (over 4 GB) are not read.
    @param          zip           The archive.
    @param          folder        Where the files go (created if needed).
    @param          warnings      Gets one line per entry that was skipped.
    @param          error         Why nothing could be extracted, when the result is false.
    @return         True when the archive could be read (even if some entries were skipped).
    *************************************************************************/
    bool ExtractZip(const std::filesystem::path& zip, const std::filesystem::path& folder,
                    std::vector<std::string>& warnings, std::string& error);
}
