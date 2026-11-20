/* !
@file               MaterialData.hpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Declares the CPU-side material description - a material slot - that every
    loader writes into. A ModelData owns a list of these and its submeshes point
    into that list by index.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Third-Party Headers
#include <glm/glm.hpp>

// Standard Library Headers
#include <string>

namespace Engine::Assets
{
    /*!***********************************************************************
    @brief          One material slot: the PBR factors a file declared plus the
                    texture files that belong to it. Texture paths are relative
                    to the folder the model was loaded from; an empty path means
                    the slot has no such map.
    *************************************************************************/
    struct MaterialData
    {
        std::string name;
        glm::vec4 baseColorFactor = glm::vec4(1.0f);
        float metallic = 0.0f;
        float roughness = 0.5f;
        glm::vec3 emissiveFactor = glm::vec3(0.0f);
        float emissiveStrength = 0.0f;
        std::string baseColorTexture;         // albedo / diffuse map
        std::string normalTexture;            // tangent-space normal map
        std::string metallicRoughnessTexture; // G = roughness, B = metallic
    };
}
