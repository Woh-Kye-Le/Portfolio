/* !
@file               MaterialFile.hpp
@author             c.kojat@digipen.edu [100%]
@date               02/11/2026
@brief
    Declares the game's material files (.amat): how an object looks - its
    shader (the engine's standard one or a custom .vert/.frag pair), colour,
    texture, metal and roughness, glow, and values for a custom shader - and
    the form a running game loads one into.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Assets Headers
#include "engine/assets/ResourceManager.hpp"

// Third-Party Headers
#include <glm/glm.hpp>

// Standard Library Headers
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Engine::Assets
{
    /*!***********************************************************************
    @brief          File extension of a material, lower case.
    *************************************************************************/
    inline constexpr const char* kMaterialExtension = ".amat";

    /*!***********************************************************************
    @brief          A material file. Paths are relative to the game folder.
    *************************************************************************/
    struct MaterialFile
    {
        std::string shader;            // a custom shader's .vert (its .frag beside it); empty: the standard lit shader
        glm::vec4 color{1.0f};         // multiplies the texture (and the object's own tint)
        std::string texture;           // its picture; empty: the object's own Texture, if any
        float metallic = 0.0f;         // 0 plastic or wood, 1 metal
        float roughness = 0.5f;        // 0 mirror-smooth, 1 chalky
        glm::vec3 emissive{0.0f};      // the colour it glows...
        float emissiveStrength = 0.0f; // ...and how brightly (0: no glow)
        // Values a custom shader reads as float uniforms, by their names in the shader ("u_Speed")
        std::vector<std::pair<std::string, float>> values;

        bool operator==(const MaterialFile&) const = default;
    };

    /*!***********************************************************************
    @brief          Reads a material file. Missing fields keep their defaults.
    @param          path          The .amat file.
    @param          error         Gets why it could not be read (optional).
    @return         The material, or nullopt for a missing or malformed file.
    *************************************************************************/
    std::optional<MaterialFile> ReadMaterialFile(const std::filesystem::path& path, std::string* error = nullptr);

    /*!***********************************************************************
    @brief          Writes a material file (indented JSON, keys in order), making
                    its folder if needed.
    @param          path          The .amat file.
    @param          material      What to write.
    @param          error         Gets why it could not be written (optional).
    @return         Whether it was written.
    *************************************************************************/
    bool WriteMaterialFile(const std::filesystem::path& path, const MaterialFile& material,
                           std::string* error = nullptr);

    /*!***********************************************************************
    @brief          A material as a running game uses it: the file, and the
                    shader and texture it names, loaded through the game's
                    ResourceManager (invalid handles for none).
    *************************************************************************/
    struct LoadedMaterial
    {
        MaterialFile file;
        ShaderHandle shader;
        TextureHandle texture;
    };
}
