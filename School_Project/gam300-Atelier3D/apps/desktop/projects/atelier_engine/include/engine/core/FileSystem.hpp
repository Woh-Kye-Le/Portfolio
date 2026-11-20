/* !
@file    	    FileSystem.hpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/10/2026
@brief
    Small filesystem helpers shared by the asset loaders: reading a whole file
    as bytes or text and normalising an extension for loader dispatch.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Standard Library Headers
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Engine::Core
{
    /*!***********************************************************************
    @brief          Reads a whole file into a byte buffer.
    @param          path          File to read.
    @return         File contents, or nullopt when it cannot be opened.
    *************************************************************************/
    std::optional<std::vector<std::uint8_t>> ReadBytes(const std::filesystem::path& path);

    /*!***********************************************************************
    @brief          Reads a whole file into a string.
    @param          path          File to read.
    @return         File contents, or nullopt when it cannot be opened.
    *************************************************************************/
    std::optional<std::string> ReadText(const std::filesystem::path& path);

    /*!***********************************************************************
    @brief          Whether the path is an existing regular file.
    @param          path          File path to check.
    @return         True when the file exists and is a regular file.
    *************************************************************************/
    bool FileExists(const std::filesystem::path& path);

    /*!***********************************************************************
    @brief          Returns the lowercased extension including its dot.
    @param          path          File path to inspect.
    @return         Extension such as ".obj", or an empty string when absent.
    *************************************************************************/
    std::string Extension(const std::filesystem::path& path);
}
