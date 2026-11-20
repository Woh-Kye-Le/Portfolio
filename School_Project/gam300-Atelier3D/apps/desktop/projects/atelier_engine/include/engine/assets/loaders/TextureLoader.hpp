/* !
@file               TextureLoader.hpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Declares the texture file loader and the magenta placeholder texture.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Assets Headers
#include "engine/assets/data/TextureData.hpp"

// Standard Library Headers
#include <filesystem>
#include <optional>

namespace Engine::Assets
{
    /*!***********************************************************************
    @brief          Decodes a .png/.jpg/.jpeg file into RGBA8 pixels.
    @param          path          Image file to read.
    @return         Populated TextureData, or nullopt when the file is missing
                    or cannot be decoded.
    *************************************************************************/
    std::optional<TextureData> LoadTextureFromFile(const std::filesystem::path& path);

    /*!***********************************************************************
    @brief          Builds the 2x2 magenta stand-in used when a texture file is
                    missing, so the mistake is obvious on screen.
    @return         Magenta placeholder TextureData.
    *************************************************************************/
    TextureData PlaceholderTexture();
}
