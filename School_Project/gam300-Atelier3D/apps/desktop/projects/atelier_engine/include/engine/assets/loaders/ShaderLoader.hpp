/* !
@file               ShaderLoader.hpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Declares the shader file loader and the minimal fallback shader.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Assets Headers
#include "engine/assets/data/ShaderData.hpp"

// Standard Library Headers
#include <filesystem>
#include <optional>

namespace Engine::Assets
{
    /*!***********************************************************************
    @brief          Loads a shader pair. The path names the vertex shader, and a
                    fragment shader with the same base name is expected next to
                    it. Quoted #include directives are expanded in both sources.
    @param          path          Vertex shader file to read.
    @return         Populated ShaderData, or nullopt when either file is missing.
    *************************************************************************/
    std::optional<ShaderData> LoadShaderFromFile(const std::filesystem::path& path);

    /*!***********************************************************************
    @brief          Builds the simplest textured shader, so an object whose
                    shader is missing still shows up on screen.
    @return         ShaderData for the built-in unlit fallback.
    *************************************************************************/
    ShaderData PlaceholderShader();
}
