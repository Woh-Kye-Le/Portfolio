/* !
@file    	    FileSystem.cpp
@author  	    w.kyele@digipen.edu [100%]
@date    		27/10/2026
@brief
    Implements the loader file helpers: whole-file byte and text reads plus
    lowercase extension normalisation.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/core/FileSystem.hpp"

// Standard Library Headers
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>

namespace Engine::Core
{
    /*!***********************************************************************
    @brief          Reads a whole file into a byte buffer.
    @param          path          File to read.
    @return         File contents, or nullopt when it cannot be opened.
    *************************************************************************/
    std::optional<std::vector<std::uint8_t>> ReadBytes(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            return std::nullopt;
        }
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    /*!***********************************************************************
    @brief          Reads a whole file into a string.
    @param          path          File to read.
    @return         File contents, or nullopt when it cannot be opened.
    *************************************************************************/
    std::optional<std::string> ReadText(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            return std::nullopt;
        }
        std::ostringstream text;
        text << file.rdbuf();
        return text.str();
    }

    /*!***********************************************************************
    @brief          Whether the path is an existing regular file.
    @param          path          File path to check.
    @return         True when the file exists and is a regular file.
    *************************************************************************/
    bool FileExists(const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::is_regular_file(path, ec);
    }

    /*!***********************************************************************
    @brief          Returns the lowercased extension including its dot.
    @param          path          File path to inspect.
    @return         Extension such as ".obj", or an empty string when absent.
    *************************************************************************/
    std::string Extension(const std::filesystem::path& path)
    {
        auto ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return ext;
    }
}
