/* !
@file               TextureLoader.cpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Implements the texture file loader on top of stb_image and the magenta
    placeholder texture.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/assets/loaders/TextureLoader.hpp"

// Engine Core Headers
#include "engine/core/FileSystem.hpp"

// Engine Assets Headers
#include "engine/assets/loaders/GltfLoader.hpp"

// Third-Party Headers
#include <stb/stb_safe.h>

// Standard Library Headers
#include <cstddef>

namespace Engine::Assets
{
    /*!***********************************************************************
    @brief          Decodes a .png/.jpg/.jpeg file into RGBA8 pixels.
    @param          path          Image file to read.
    @return         Populated TextureData, or nullopt when the file is missing
                    or cannot be decoded.
    *************************************************************************/
    std::optional<TextureData> LoadTextureFromFile(const std::filesystem::path& path)
    {
        // "car.glb#0": an image packed inside a glTF model
        const auto bytes = GltfContainerOf(path) != path ? ReadGltfImage(path) : Core::ReadBytes(path);
        if (!bytes || bytes->empty())
        {
            return std::nullopt;
        }

        // OpenGL's first row is the bottom one
        stbi_set_flip_vertically_on_load(1);
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels =
            stbi_load_from_memory(bytes->data(), static_cast<int>(bytes->size()), &width, &height, &channels, 4);
        if (!pixels)
        {
            return std::nullopt;
        }

        TextureData texture{.width = width, .height = height};
        texture.rgba.assign(pixels, pixels + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
        stbi_image_free(pixels);
        return texture;
    }

    /*!***********************************************************************
    @brief          Builds the 2x2 magenta stand-in for a missing texture.
    @return         Magenta placeholder TextureData.
    *************************************************************************/
    TextureData PlaceholderTexture()
    {
        TextureData texture;
        texture.width = 2;
        texture.height = 2;
        constexpr std::size_t kPixels = 4; // 2 x 2
        texture.rgba.reserve(kPixels * 4);
        for (std::size_t i = 0; i < kPixels; ++i)
        {
            texture.rgba.push_back(255); // R
            texture.rgba.push_back(0);   // G
            texture.rgba.push_back(255); // B
            texture.rgba.push_back(255); // A
        }
        return texture;
    }
}
