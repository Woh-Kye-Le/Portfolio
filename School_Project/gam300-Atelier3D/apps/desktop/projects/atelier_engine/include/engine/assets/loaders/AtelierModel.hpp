/* !
@file               AtelierModel.hpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Declares the engine's own model file (.amodel): a compact binary copy of a
    ModelData that the asset tool writes after converting OBJ, FBX, glTF and
    other formats, and the only model format the running engine needs to read.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Assets Headers
#include "engine/assets/data/ModelData.hpp"

// Standard Library Headers
#include <cstdint>
#include <optional>
#include <vector>

namespace Engine::Assets
{
    /*!***********************************************************************
    @brief          File extension of the engine's model format, lower case.
    *************************************************************************/
    inline constexpr const char* kAtelierModelExtension = ".amodel";

    /*!***********************************************************************
    @brief          Version written into every .amodel file. A reader refuses a file with a newer version.
    *************************************************************************/
    inline constexpr std::uint32_t kAtelierModelVersion = 2; // 2 added skins and animations

    /*!***********************************************************************
    @brief          Serializes a ModelData into the .amodel binary format: little-endian, no padding, a
                    4-byte magic ("AMDL") and a version, then the name, materials, meshes (positions, UVs,
                    normals, indices, material runs and skinning), nodes and roots, skins, animations, and a
                    checksum of everything before it. A mesh's UVs or normals are written only when there is one per vertex;
                    anything else is left out. The renderer's cache id is not stored.
    @param          model         Model to serialize.
    @return         The bytes of the file.
    *************************************************************************/
    std::vector<std::uint8_t> WriteAtelierModel(const ModelData& model);

    /*!***********************************************************************
    @brief          Reads a .amodel file back into a ModelData. The file is checked as it is read: magic,
                    version, checksum, that no count claims more data than the file holds, and that every
                    index, material run, node link and root points at something that exists.
    @param          bytes         Entire file contents.
    @return         The model, or nullopt when the data is not a valid .amodel file of a version this
                    engine can read.
    *************************************************************************/
    std::optional<ModelData> ParseAtelierModel(const std::vector<std::uint8_t>& bytes);
}
